# Step 01 — Spawner (루트 테이블 + 스포너 + 픽업)

> 마스터 기획: `05_Loot_DOCS.md` (§4-2, §4-3, §4-4)
> 선행: `05_Loot_00_ItemCore.md` — `ItemId → Definition` 조회가 동작해야 한다

---

## 목표

맵에 스포너를 배치하면 매치 시작 시 확률대로 아이템이 바닥에 스폰되고, 서버·클라 양쪽에서 같게 보인다. **줍는 건 Step 02다.**

**완료 조건**

- [ ] 스포너를 맵에 놓고 PIE 2인 → 서버·클라에서 같은 아이템이 같은 위치에 보인다
- [ ] `EP.Loot.RollTable LT_Floor_Common 1000` → 등급 비율이 기획표(50/30/15/5)와 오차 범위 내. **빈 결과를 뺀 나머지에 대한 비율이다** (아래)
- [ ] 어떤 스포너도 참조하지 않는 테이블도 `RollTable`이 이름으로 찾는다
- [ ] `EP.Loot.Respawn` → 기존 픽업이 정리되고 새로 굴려진다 (`ClearLoot`이 자기 것만 지우는지는 버리기가 생기는 **Step 03에서 재확인**)
- [ ] `WorldMesh`가 없는 아이템(`AmmoBox_545` 등)도 플레이스홀더로 보인다
- [ ] **픽업을 향해 쏴도 총알이 막히지 않고, 픽업 위를 걸어도 걸리지 않는다** (01-4 콜리전)
- [ ] **아직 못 본 픽업만 컬링된다.** 스폰 지점 5000cm 밖에서 클라 `EP.Loot.List` → 목록에 없다. 가까이 가면 나타난다
- [ ] **한 번 본 픽업은 멀어져도 목록에 남는다 — 이게 정상이다.** 채널이 **휴면 사유**로 닫히면 클라가 액터를 파괴하지 않고 유지한다(`ClientSetActorDormant`). 릴러번시 사유로 닫혔다면 파괴됐을 것이다 (01-4)
- [ ] **클라이언트 패킷에 픽업의 `Charges`가 나가지 않는다** (`State`는 서버 전용 — 01-4)
  - 검증 절차: 서버 창 `EP.Loot.List`에서 **`Charges > 0`인 픽업의 `Idx`를 먼저 확인**한다. 같은 `Idx`가 클라 창에서 `0`이면 통과. 서버에 `Charges > 0`인 픽업이 하나도 없으면 이 조건은 **검증되지 않은 것**이다 (`AmmoBox_545`/`Cash_10000`이 `InitialCharges`를 가지므로 반드시 나온다)

---

## 01-1. `UEPLootTable` — 가중치 + 중첩

```cpp
class UEPLootTable;      // ★ FEPLootEntry가 먼저 선언되므로 전방 선언이 필요하다

USTRUCT(BlueprintType)
struct FEPLootEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
    float Weight = 1.f;

    // ItemId와 SubTable 중 하나만 채운다. SubTable이 유효하면 재귀 롤
    UPROPERTY(EditAnywhere)
    FName ItemId;

    UPROPERTY(EditAnywhere)
    TObjectPtr<UEPLootTable> SubTable;
};

UCLASS()
class EMPLOYMENTPROJ_API UEPLootTable : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "Loot")
    TArray<FEPLootEntry> Entries;

    // 아무것도 안 나올 가중치. ★ 루트 테이블에서만 유효
    UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0.0"))
    float EmptyWeight = 0.f;

    // ★ final — 하위 클래스가 다시 오버라이드하면 타입이 갈린다 (아래)
    virtual FPrimaryAssetId GetPrimaryAssetId() const override final
    { return FPrimaryAssetId(TEXT("LootTable"), GetFName()); }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

// ★ 공개 진입점에는 Depth가 없다. 재귀는 .cpp 안에서만 일어난다 (아래)
EMPLOYMENTPROJ_API bool RollLootTable(const UEPLootTable* Table, FName& OutItemId);
```

`SubTable`은 **하드 참조(`TObjectPtr`)** 로 둔다 — 루트 테이블이 로드되면 등급 테이블이 따라 로드된다.

### `final`을 붙이는 이유

`UEPLootTable`은 지금 하위 클래스가 없고 만들 계획도 없다. `final`은 **없는 문제를 막는 게 아니라, 생기면 진단이 오래 걸리는 문제를 애초에 못 쓰게 하는 것**이다.

Step 00에서 `UEPWeaponDefinition`이 `GetPrimaryAssetId()`를 오버라이드해 `"WeaponDef"`를 반환한 적이 있었다. **그 오버라이드는 Step 00에서 제거됐다** — 지금 코드에는 없다. 문제는 그 사이에 저장된 무기 DA들이 `WeaponDef` 태그를 달고 있어서, **코드를 고친 뒤에도 계속 스캔에서 빠졌다**는 것이다(재저장으로 해결). `final`은 그 오버라이드가 되돌아올 수 없게 만든다.

> **아래 3단계 순서와 `final`은 서로 다른 실패를 막는다. 어느 쪽도 다른 쪽을 대체하지 않는다.**
>
> | | 막는 것 |
> |---|---|
> | `final` | 하위 클래스가 **다른 타입 문자열**을 반환하는 것 |
> | 3단계 순서 | 타입이 등록되기 **전에 저장된** 에셋에 엉뚱한 태그가 구워지는 것 |
>
> `final`을 붙여도 컴파일 전에 `LT_` 에셋을 만들면 그대로 당한다.

> Step 00의 `UEPItemDefinition::GetPrimaryAssetId()`에도 같은 한 단어를 붙일 수 있다 — 두 클래스가 갈리지 않게. 완료된 단계라 판단은 사용자 몫이다.

### 롤 함수 — 깊이 상한과 `EmptyWeight` 규칙

**수량 필드가 없다.** 스택이 없으므로 롤 결과는 **아이템 하나**다. "탄약 20~60발"은 수량이 아니라 **탄약상자 하나의 `Charges`** 이고, 그 초기값은 `Definition->InitState()`가 정한다 (Step 00). 루트 테이블이 아이템 타입별 상태를 알게 하지 않는다.

```cpp
// EPLootTable.cpp — 재귀 본체. static이라 이 .cpp 밖에서는 부를 수 없다
static bool RollInternal(const UEPLootTable* Table, FName& OutItemId, int32 Depth)
{
    OutItemId = NAME_None;              // ★ 불변식을 코드로 — 호출자의 초기화를 믿지 않는다

    if (!Table)
    {
        // ★ 깊이 초과와 합치지 않는다 — 둘은 다른 원인이고 다른 곳을 봐야 한다
        UE_LOG(LogTemp, Error, TEXT("[Loot] 롤 대상 테이블이 null이다 (Depth=%d)"), Depth);
        return false;
    }

    static constexpr int32 MaxDepth = 8;
    if (Depth > MaxDepth)
    {
        UE_LOG(LogTemp, Error, TEXT("[Loot] 롤 깊이 초과 — 순환 참조 의심: %s"),
               *GetNameSafe(Table));
        return false;
    }

    // ★ EmptyWeight는 루트에서만
    float TotalWeight = (Depth == 0) ? Table->EmptyWeight : 0.f;
    if (Depth > 0 && Table->EmptyWeight > 0.f)
        UE_LOG(LogTemp, Warning,
               TEXT("[Loot] %s: 하위 테이블의 EmptyWeight(%.2f)는 무시된다"),
               *GetNameSafe(Table), Table->EmptyWeight);

    for (const FEPLootEntry& E : Table->Entries) TotalWeight += E.Weight;
    if (TotalWeight <= 0.f) return false;

    float Pick = FMath::FRandRange(0.f, TotalWeight);

    if (Depth == 0 && (Pick -= Table->EmptyWeight) < 0.f)
        return true;                    // 빈 결과 (OutItemId == None)

    // 뽑힌 엔트리를 가리킨다. 루프가 끝까지 가도 "마지막 유효 엔트리"가 담겨 있다
    const FEPLootEntry* Chosen = nullptr;

    for (const FEPLootEntry& E : Table->Entries)
    {
        if (E.Weight <= 0.f) continue;          // 가중치 0은 뽑히지 않는다 — 명시적으로
        Chosen = &E;
        if ((Pick -= E.Weight) < 0.f) break;    // 여기서 확정
    }

    if (!Chosen) return false;                  // 유효 엔트리가 하나도 없다 = 데이터 오류

    if (Chosen->SubTable) return RollInternal(Chosen->SubTable, OutItemId, Depth + 1);

    if (Chosen->ItemId.IsNone())
    {
        // ★ ItemId도 SubTable도 없는 엔트리. true + None으로 내보내면
        //    스포너가 "정상적인 빈 결과"로 읽어 데이터 오류가 조용히 묻힌다
        UE_LOG(LogTemp, Error, TEXT("[Loot] %s: ItemId도 SubTable도 없는 엔트리"),
               *GetNameSafe(Table));
        return false;
    }

    OutItemId = Chosen->ItemId;
    return true;
}

// 유일한 공개 진입점. 여기서만 Depth = 0으로 시작한다
bool RollLootTable(const UEPLootTable* Table, FName& OutItemId)
{
    return RollInternal(Table, OutItemId, 0);
}
```

### ★ 왜 둘로 가르는가 — `Depth`는 인자가 아니라 구현 세부다

한 함수에 `int32 Depth = 0` 기본 인자로 두면, **자유 함수로 옮기든 멤버로 두든 `Depth`는 똑같이 공개된다.** 호출자가 `RollLootTable(Table, Out, 3)`을 쓸 수 있고, 그러면 `EmptyWeight`가 조용히 무시된다 — **컴파일되고, 크래시도 안 나고, 확률만 틀린다.** 제일 찾기 어려운 종류다.

| | |
|---|---|
| `RollLootTable(Table, Out)` | 헤더에 선언. **`Depth`가 없어서 잘못 부를 방법이 없다** |
| `RollInternal(..., Depth)` | `.cpp`의 `static`. 번역 단위 밖에서는 이름 자체가 안 보인다 |

`static`은 **`.cpp`에** 쓴다. 헤더에 `static` 자유 함수를 두면 TU마다 사본이 생기고, 안 쓰는 TU에서 `-Wunused-function`이 난다.

**멤버 함수로 두지 않는 이유는 따로 있다.** `UEPLootTable::Roll()`이면 "테이블이 스스로를 굴린다"가 되는데, 실제로는 **여러 테이블을 가로지르며 굴린다**(루트 → 등급). 주체가 테이블이 아니라 롤 자체다.

### ★ 반환 규약이 두 겹이고, 그 규약이 코드 안에 있다

| 반환 | 뜻 | 호출부 |
|---|---|---|
| `false` | **이 테이블에서는 아무것도 뽑을 수 없다** — 널·깊이 초과·`TotalWeight <= 0`·유효 엔트리 0개·엔트리 미기입 | 에러 로그 |
| `true` + `OutItemId == NAME_None` | **루트 `EmptyWeight`가 뽑혔다.** 이 경로가 유일하다 | 조용히 스폰 생략 |
| `true` + 유효한 `ItemId` | 정상 | 스폰 |

**규약을 호출자 쪽 문장으로만 두면 아무도 안 걸린다.** 빈 `ItemId`를 `true + NAME_None`으로 내보내면 스포너가 그것을 "정상적인 빈 결과"로 읽고, 증상은 *"가끔 아이템이 안 나온다"* 가 되어 `EmptyWeight`를 의심하며 엉뚱한 데를 판다. 그래서 **함수가 직접 걸러 `false`를 반환한다.**

### ★ fall-through를 없앤 이유 — 2⁻²⁴이 아니다

`FMath::FRand()`는 **`[0, 1]` 닫힌 구간**이다 (`GenericPlatformMath.h:635` — *"Returns a random float between 0 and 1, **inclusive**"*). `Pick == TotalWeight`면 루프가 끝까지 음수가 안 된다.

**그런데 그건 두 원인 중 하나일 뿐이다.** `TotalWeight`는 **더하면서** 만들고 `Pick`은 **빼면서** 소모한다. float에서 `((T - w₁) - w₂) - … - wₙ`은 `T = Σwᵢ`였어도 정확히 0이 되지 않고 양의 잔차가 남을 수 있다 — `Pick`이 `TotalWeight`와 **정확히 같지 않아도** fall-through한다. 상대 오차(float ≈ 1e-7)에 비례하므로 **2⁻²⁴(≈6e-8)보다 오히려 흔하다.**

빈도가 문제가 아니다. 도달하면 **데이터가 정상인데 "테이블이 잘못됐다"는 Error 로그가 뜬다.** 그 로그는 위 규약이 걸고 있는 신뢰의 근거이고, **거짓 양성이 한 번 나오면 그 다음부터 아무도 그 로그를 안 믿는다.**

`Chosen` 포인터 하나로 셋이 동시에 닫힌다.

| | |
|---|---|
| 닫힌 구간 + 부동소수 잔차 | 마지막 **유효** 엔트리를 이미 들고 있어 fall-through가 없다 |
| `Weight <= 0` 엔트리 | 명시적으로 건너뛴다 (원래 코드는 `Pick -= 0 >= 0`으로 **우연히** 건너뛰었다) |
| 빈 `ItemId` | 규약 위반을 `false`로 정직하게 반환 |

### `IsDataValid()` — 런타임 Error를 에디터로 앞당긴다

위 `Error` 로그는 **그 테이블이 실제로 뽑힐 때까지** 안 뜬다. 등급 테이블은 5%짜리도 있어서 며칠 뒤에 뜰 수 있다. `UEPItemDefinition`이 Step 00에서 이미 쓰는 패턴이므로(`EPItemDefinition.h:47`) 형태도 같다.

```cpp
#if WITH_EDITOR
EDataValidationResult UEPLootTable::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    if (Entries.IsEmpty() && EmptyWeight <= 0.f)
    {
        Context.AddError(NSLOCTEXT("EP", "LootTableEmpty",
            "Entries가 비었고 EmptyWeight도 0입니다. 이 테이블은 아무것도 뽑을 수 없습니다."));
        Result = EDataValidationResult::Invalid;
    }

    for (int32 i = 0; i < Entries.Num(); ++i)
    {
        const FEPLootEntry& E = Entries[i];
        const bool bHasId  = !E.ItemId.IsNone();
        const bool bHasSub = (E.SubTable != nullptr);

        if (bHasId == bHasSub)      // 둘 다 비었거나 둘 다 채워졌다
        {
            Context.AddError(FText::Format(
                NSLOCTEXT("EP", "LootEntryAmbiguous",
                    "Entries[{0}]: ItemId와 SubTable 중 정확히 하나만 채워야 합니다."),
                FText::AsNumber(i)));
            Result = EDataValidationResult::Invalid;
        }

        if (E.SubTable == this)     // 깊이 1 자기 참조
        {
            Context.AddError(FText::Format(
                NSLOCTEXT("EP", "LootEntrySelfRef",
                    "Entries[{0}]: SubTable이 자기 자신입니다."),
                FText::AsNumber(i)));
            Result = EDataValidationResult::Invalid;
        }
    }
    return Result;
}
#endif
```

**`Result = EDataValidationResult::Invalid;`를 빼먹으면 안 된다.** `AddError`는 메시지를 담을 뿐이고, **검증 통과/실패를 정하는 것은 반환값**이다. 빠뜨리면 에러 메시지는 뜨는데 검증은 통과한다 — `UEPItemDefinition::IsDataValid()`(`EPItemDefinition.cpp:10-38`)도 검사마다 두 줄이 짝으로 붙어 있다.

**`ItemId`가 DT에 있는지는 검사하지 않는다** — `IsDataValid` 시점에 DT 로드를 강제하게 되고, 그건 `MakeItemState` 실패(함정 #11)가 이미 잡는다. 깊이 상한 8은 자기 참조를 **못 막는 게 아니라 뒤늦게** 막는다. 저장하는 순간 잡는 편이 낫다.

**★ 순환 참조 방어는 선택이 아니다.** 에디터에서 `LT_A → LT_B → LT_A`로 엮는 실수는 반드시 나오고, 방어가 없으면 스택 오버플로로 에디터가 통째로 죽는다.

**★ `EmptyWeight`를 루트에서만 적용하는 이유:** 하위 등급 테이블에도 먹히면 "일반 등급 50%"를 뽑고 그 안에서 또 빈 결과가 나와 **실제 일반 확률이 50% 미만**이 된다. 기획표가 조용히 침식되고 원인을 찾기 어렵다. 등급별로 빈 확률을 다르게 주고 싶으면 하위 테이블에 "아무것도 아님" 엔트리를 명시적으로 넣는다 — 의도가 데이터에 드러난다.

### 만들 에셋

```
LT_Floor_Common          (루트 — 바닥 스포너용, EmptyWeight 있음)
├─ SubTable: LT_Rarity_Common      Weight 50
├─ SubTable: LT_Rarity_Uncommon    Weight 30
├─ SubTable: LT_Rarity_Rare        Weight 15
└─ SubTable: LT_Rarity_Legendary   Weight  5

LT_Rarity_Common         (등급 — 자판기·컨테이너가 공유, EmptyWeight = 0)
├─ AmmoBox_545   Weight 1
├─ Bandage       Weight 1
└─ Scrap         Weight 1
...
```

등급 테이블을 별도 에셋으로 빼는 게 핵심이다. 컨테이너(§7-1)와 자판기(§7-2)가 **루트만 새로 만들고 등급은 재사용**한다.

### AssetManager 등록

| 필드 | 값 |
|---|---|
| Primary Asset Type | `LootTable` |
| Asset Base Class | `/Script/EmploymentProj.EPLootTable` |
| Directories | `/Game/Data/Loot` |
| **Is Editor Only** | **`false`** ★ |
| **Cook Rule** | **`AlwaysCook`** ★ |
| Has Blueprint Classes | `false` |
| bApplyRecursively | `true` |

### ★ `Is Editor Only`와 `Cook Rule`은 같은 `if` 한 줄에 걸려 있다

```cpp
// AssetManager.cpp:4731-4739  (UAssetManager::ModifyCook)
bool bAlwaysCook = CookRule == EPrimaryAssetCookRule::AlwaysCook || (개발용 변종들…);

if (bAlwaysCook && bCanCook && !TypeInfo.bIsEditorOnly)
{
    PackagesToCook.Add(PackageName);      // ← 여기 들어가야 쿡된다
}
```

**둘 중 하나만 틀려도 결과가 같다** — 에디터에서는 되는데 패키지 빌드에서만 리스트가 빈다. 기존 `DefaultGame.ini`의 `Map` / `PrimaryAssetLabel`이 `bIsEditorOnly=True`, `CookRule=Unknown`이라 그 줄을 보고 따라 하면 정확히 이 함정이다.

**`Unknown`은 "아무도 안 가리키면 안 나간다"는 뜻이다.**

```cpp
// AssetManagerTypes.h:30
/** Nothing is known about this asset specifically.
    It will cook in both Development and Production if something else depends on it. */
Unknown,
```

루트 테이블은 맵에 배치된 스포너가 하드 참조하므로(`Map → Spawner → LT_Floor → LT_Rarity_*`) `Unknown`으로도 나가긴 한다. **문제는 아무 스포너도 참조하지 않는 테이블** — 정확히 `EP.Loot.RollTable`로 검증하려는 그 대상이다. 쿡되지 않으면 **쿡된 레지스트리에 ID조차 없어** `LoadPrimaryAsset`이 실패한다.

> **Step 00의 `ItemDef`는 `AlwaysCook`이 선택이 아니라 필수였다.** Definition DA는 **아무도 하드 참조하지 않는다** — 서브시스템이 런타임에 타입으로 긁어오므로 레지스트리 관점에서 고아다. `Unknown`이면 패키지 빌드에서 `Definitions = 0`이 된다.
>
> `DT_Items`는 각 DA의 `ItemDataRow`(`FDataTableRowHandle::DataTable`은 **하드 참조** — `DataTable.h:408`)를 타고 따라온다. `.ini`의 `ItemDataTable=` 경로는 문자열이라 레지스트리 의존이 아니다.

> **무거운 애셋이면 판단이 달라진다.** `AlwaysCook`은 안 쓰는 것까지 전부 내보낸다. 루트 테이블과 Definition은 무거운 것을 전부 소프트 포인터(`WorldMesh`/`Icon`/`WeaponMesh`)로 들고 있어 몇 KB짜리 데이터만 나간다 — Step 00에서 에셋 참조를 소프트로 둔 값어치가 여기서 돌아온다.

> 실사용만 보면 스포너가 하드 포인터로 들고 있어 불필요해 보이지만, **`EP.Loot.RollTable <이름>`이 ID로 찾는다.** 등록이 없으면 그 `FPrimaryAssetId` 자체가 성립하지 않는다.
>
> 다만 **등록만으로는 부족하다.** 아직 어떤 스포너도 참조하지 않는 테이블은 메모리에 없고, `GetPrimaryAssetObject`는 그 경우 nullptr을 준다 — 커맨드가 직접 로드해야 한다. 01-5를 본다.

### ★ 순서 — 코드를 먼저 컴파일하고 에셋을 나중에 만든다

`GetPrimaryAssetId()`의 반환값은 **에셋을 저장하는 시점에 애셋 레지스트리 태그로 구워진다**(`AssetData.cpp:692`). `UEPLootTable`이 컴파일되기 전에 LT_ 에셋을 만들면 그때의 추론된 타입이 박히고, 나중에 코드를 고쳐도 **그 에셋들은 계속 `LootTable` 스캔에서 배제된다.**

Step 00의 `WeaponDef` 사건이 정확히 이것이었다 — 상세와 진단법은 **`05_Loot_00_ItemCore.md` 함정 #1**에 있다. 이번 단계에서는 순서만 지키면 겪지 않는다.

1. `UEPLootTable` 클래스 작성 → **컴파일**
2. `DefaultGame.ini`에 `LootTable` 등록 → **에디터 재시작**
3. 그 다음에 `LT_*` 에셋 생성



---

## 01-2. `AEPItemSpawner`

```cpp
UCLASS()
class EMPLOYMENTPROJ_API AEPItemSpawner : public AActor
{
    GENERATED_BODY()
public:
    AEPItemSpawner();

    void SpawnLoot();     // 서버 전용
    void ClearLoot();     // 서버 전용

protected:
    UPROPERTY(EditAnywhere, Category = "Loot")
    TObjectPtr<UEPLootTable> LootTable;

    UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "1"))
    int32 RollCount = 1;

    UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0.0"))
    float SpawnRadius = 0.f;

    UPROPERTY(EditAnywhere, Category = "Loot")
    bool bAlignToGround = true;

private:
    FVector GetSpawnPoint() const;

    // 자기가 뿌린 것만 정리하기 위한 약참조
    TArray<TWeakObjectPtr<AEPPickup>> SpawnedPickups;
};
```

- 생성자에서 `bReplicates = false`, `PrimaryActorTick.bCanEverTick = false`. **스포너는 복제하지 않는다** — 픽업 액터가 복제되므로 클라 동기화는 자동으로 따라온다
- 런타임에 보이지 않는다. 에디터 빌보드(`UBillboardComponent`)를 붙이되 **루트로 삼지 않는다** (아래)
- `bAlignToGround`면 스폰 지점에서 아래로 짧은 라인 트레이스를 쏴 접지시킨다. 레벨 디자이너가 높이를 정밀하게 안 맞춰도 되게 하기 위함

### 생성자

```cpp
AEPItemSpawner::AEPItemSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;                // 기본값이지만 명시 — 픽업만 복제된다

    // ★ 루트는 언제나 있어야 한다. 빌보드가 아니다 (아래)
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
    Billboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
    if (Billboard)
    {
        Billboard->SetupAttachment(RootComponent);
        Billboard->bIsScreenSizeScaled = true;
    }
#endif

    SetHidden(true);
    SetCanBeDamaged(false);
}
```

헤더에도 같은 가드가 필요하다.

```cpp
#if WITH_EDITORONLY_DATA
    UPROPERTY()
    TObjectPtr<UBillboardComponent> Billboard;
#endif
```

### ★ 빌보드를 루트로 삼으면 **패키지 빌드에서만** 루트가 원점에 쌓인다

```cpp
// UObjectGlobals.cpp:6036-6050
UObject* FObjectInitializer::CreateEditorOnlyDefaultSubobject(...) const
{
#if WITH_EDITOR
    if (GIsEditor) { ... return EditorSubobject; }
#endif
    return nullptr;                 // ★
}
```

**`WITH_EDITOR`가 아니라 `GIsEditor`까지 본다.** 그것을 `RootComponent`에 넣으면 루트가 null이 되고,

```cpp
// Actor.h:4461  AActor::TemplateGetActorLocation
return (RootComponent != nullptr) ? RootComponent->GetComponentLocation() : FVector::ZeroVector;
```

**`GetSpawnPoint()`가 조용히 `FVector::ZeroVector`를 반환한다.** 전 맵의 루트가 월드 원점 한 곳에 쌓이는데, 완료 조건 8개 중 어디에도 안 걸린다.

**그리고 `GIsEditor` 조건 때문에 쿡한 빌드만의 문제가 아니다.**

| 실행 방식 | `GIsEditor` | 빌보드 |
|---|---|---|
| PIE | `true` | 있다 — **버그가 안 보인다** |
| **Standalone Game** (에디터 툴바, 별도 `-game` 프로세스) | **`false`** | **없다 → 루트 null** |
| 쿡한 패키지 | `false` | 없다 → 루트 null |

레벨 배치 액터가 에디터에서 이동조차 안 된다는 부작용도 같이 온다.

엔진의 같은 종류 액터가 정확히 이래서 두 겹이다.

```cpp
// TargetPoint.cpp:15-21, 50 — 안 보이는 레벨 배치 마커의 표준형
USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
RootComponent = SceneComponent;                          // ← 항상 있다
#if WITH_EDITORONLY_DATA
    SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
    SpriteComponent->SetupAttachment(SceneComponent);    // ← 붙일 뿐, 루트가 아니다
#endif
```

`SetHidden(true)` / `SetCanBeDamaged(false)`도 같은 자리(`:73-74`)에서 가져온다 — 스포너는 렌더링 대상도 데미지 대상도 아니다.

> **`USceneComponent` 루트에는 콜리전이 없다.** `GetSpawnPoint()`의 접지 트레이스가 `Params.AddIgnoredActor(this)`를 하고는 있지만, 그건 나중에 스포너에 무언가 붙었을 때를 위한 보험이다.

**★ 약참조로 들고 있는 이유:** `ClearLoot()`이 자기가 뿌린 것만 지워야 한다. 월드의 모든 `AEPPickup`을 순회해 지우면 **플레이어가 버린 아이템까지 사라진다**(Step 03). 그리고 이미 주워진 픽업은 파괴됐으므로 강참조로 들면 GC를 막는다.

### 본문 — Step 01에서 새로 생기는 유일한 게임플레이 로직이다

```cpp
void AEPItemSpawner::SpawnLoot()
{
    if (!HasAuthority()) return;

    if (!LootTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[Loot] %s: LootTable 미지정"), *GetName());
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UEPItemDefinitionSubsystem* Defs =
        GI ? GI->GetSubsystem<UEPItemDefinitionSubsystem>() : nullptr;
    if (!Defs) return;

    // ★ 폴백이 여기 있다. 세 상태를 가른다 — 근거는 01-5
    const UEPLootDeveloperSettings* Settings = GetDefault<UEPLootDeveloperSettings>();
    UClass* PickupCls = Settings->PickupClass.IsNull()
        ? AEPPickup::StaticClass()
        : Settings->PickupClass.LoadSynchronous();

    if (!PickupCls)
    {
        UE_LOG(LogTemp, Error, TEXT("[Loot] PickupClass 로드 실패: %s"),
               *Settings->PickupClass.ToString());
        return;
    }

    for (int32 i = 0; i < RollCount; ++i)
    {
        FName RolledId;

        if (!RollLootTable(LootTable, RolledId))
        {
            // false = 테이블이 잘못됐다. 조용히 넘기면 안 된다 (01-1 반환 규약)
            UE_LOG(LogTemp, Error, TEXT("[Loot] %s: 롤 실패 — %s 데이터 오류"),
                   *GetName(), *GetNameSafe(LootTable));
            continue;
        }
        if (RolledId.IsNone()) { continue; }      // true + None = 정상적인 빈 결과

        FEPItemState NewState;
        if (!Defs->MakeItemState(RolledId, NewState))
        {
            // 함정 #11 — 기본값으로 깔면 Step 04에서 아이콘 없이 나타나 원인이 멀어진다
            UE_LOG(LogTemp, Error, TEXT("[Loot] %s: Definition 없음 — 스폰 생략"),
                   *RolledId.ToString());
            continue;
        }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;   // 기본값이지만 명시

        AEPPickup* Pickup = GetWorld()->SpawnActor<AEPPickup>(
            PickupCls, GetSpawnPoint(), FRotator::ZeroRotator, Params);
        if (!Pickup) { continue; }

        // ★ SpawnActor와 같은 프레임에 부른다 (아래 계약)
        Pickup->InitPickup(RolledId, NewState);
        SpawnedPickups.Add(Pickup);
    }
}

FVector AEPItemSpawner::GetSpawnPoint() const
{
    FVector Point = GetActorLocation();

    if (SpawnRadius > 0.f)
    {
        const FVector2D Offset = FMath::RandPointInCircle(SpawnRadius);   // UnrealMathUtility.h:348
        Point += FVector(Offset.X, Offset.Y, 0.f);
    }

    if (bAlignToGround)
    {
        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);

        // ★ ECC_Visibility로 충분한 이유는 01-4에 있다 — 픽업이 전 채널 Ignore이므로
        //    먼저 뿌린 픽업 위에 다음 픽업이 얹히지 않는다. 그 줄을 지우면 여기가 깨진다
        if (GetWorld()->LineTraceSingleByChannel(
                Hit, Point + FVector(0, 0, 200.f), Point - FVector(0, 0, 500.f),
                ECC_Visibility, Params))
        {
            Point = Hit.ImpactPoint + FVector(0, 0, 5.f);
        }
    }
    return Point;
}

void AEPItemSpawner::ClearLoot()
{
    if (!HasAuthority()) return;

    for (const TWeakObjectPtr<AEPPickup>& Weak : SpawnedPickups)
        if (AEPPickup* P = Weak.Get())
            P->Destroy();

    SpawnedPickups.Reset();       // 이미 주워져 죽은 약참조도 여기서 비워진다
}
```

**`HasAuthority()` 가드는 실제로 동작한다.** 스포너는 `bReplicates = false`라 `RemoteRole = ROLE_None`인데(`Actor.cpp:286`), 레벨 배치 액터는 클라에서 `ExchangeNetRoles(true)`(`Level.cpp:3683`)를 거쳐 `Role`과 `RemoteRole`이 **무조건 swap**된다(`Actor.cpp:4664` — 엔진 주석은 `RemoteRole != ROLE_None` 조건을 적어 놓았지만 코드는 검사하지 않는다). 결과적으로 클라에서 `Role == ROLE_None`이라 `HasAuthority()`는 false다. `AActor` 파생이므로 `GetOwner()->`를 붙이지 않는다(`CLAUDE.md` 관례 — 그건 `UActorComponent` 규칙이다).

### ★ `InitPickup()`은 `SpawnActor()`와 같은 프레임에서 부른다

**이것이 이 단계에서 지켜야 할 유일한 순서 계약이다.**

프로퍼티 복제는 "값이 바뀔 때 이벤트를 보내는" 것이 아니라 **채널을 채우는 시점의 현재 값을 보내는** 것이고, 그 시점은 액터 틱이 전부 끝난 뒤다(`LevelTick.cpp:1900` `BroadcastTickFlush` → `ServerReplicateActors`). 그래서 `SpawnActor`와 `InitPickup` 사이에 **복제가 끼어들 지점이 물리적으로 없다.** 우연히 되는 것이 아니라 복제 모델이 그렇게 정의돼 있다.

프레임을 넘기면(타이머·다음 틱·비동기 콜백) 클라가 `ItemId = NAME_None`을 먼저 받고, 픽업은 휴면에 들어가 **영원히 안 고쳐진다.**

> **`SpawnActorDeferred`는 필요 없다.** 지켜야 할 것이 "같은 프레임"뿐이므로, `InitPickup`의 시그니처가 Step 03에서 바뀌어도(01-4 각주) **호출 지점은 안 움직인다.**

> **`PickupClass`는 설정에서 온다** — 스포너 단위 필드가 아니다. Step 03의 버리기가 두 번째 스폰 지점이 되는데 그쪽에는 물어볼 스포너가 없다. 근거는 01-5에 있다.

---

## 01-3. 스폰 시점 — GameMode가 지시한다

현재 코드는 이렇다.

```cpp
// EPGameMode.cpp:137
void AEPGameMode::HandleMatchHasStarted()
{
    Super::HandleMatchHasStarted();       // ← 여기서 대기 중인 플레이어가 리스타트된다
    ...
}
```

여기에 **`Super::` 앞에서** 루트 스폰을 넣는다.

```cpp
#include "EngineUtils.h"        // TActorIterator

void AEPGameMode::HandleMatchHasStarted()
{
    for (TActorIterator<AEPItemSpawner> It(GetWorld()); It; ++It)
        It->SpawnLoot();

    Super::HandleMatchHasStarted();
    ...
}
```

**★ 순서가 중요한 이유:** `AGameMode::HandleMatchHasStarted()`는 같은 전이에서 대기 중인 플레이어들을 리스타트시킨다(→ `HandleStartingNewPlayer`). 루트를 나중에 뿌리면 **플레이어가 이미 서 있는 자리에 아이템이 스폰되어 겹칠 수 있고**, "시작 직후 잠깐 맵이 비어 있는" 창이 생긴다.

**스포너 `BeginPlay`에서 굴리지 않는 이유:** 매치 시작 전(Waiting)에 이미 아이템이 깔려 있게 되고, 라운드 재시작 시 재스폰 경로가 없다. 스포너는 `SpawnLoot()`/`ClearLoot()` 두 함수만 노출하고 호출 시점은 GameMode가 정한다 — 테스트용 재굴림(`EP.Loot.Respawn`)이 공짜로 따라온다.

---

## 01-4. `AEPPickup`

```cpp
UCLASS()
class EMPLOYMENTPROJ_API AEPPickup : public AActor
{
    GENERATED_BODY()
public:
    AEPPickup();

    // 서버 전용. 두 경로 모두 유효한 State를 들고 시작한다
    void InitPickup(FName InItemId, const FEPItemState& InState);

    const FEPItemState& GetState() const { return State; }      // 서버에서만 의미 있다

    // ★ 이게 없으면 ItemId가 복제되지 않는다 (아래)
    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UPROPERTY(ReplicatedUsing = OnRep_ItemId)
    FName ItemId;

    // ★ 서버 전용. 복제하지 않는다 (아래 참조)
    UPROPERTY()
    FEPItemState State;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;

    UFUNCTION()
    void OnRep_ItemId();

    void ApplyVisual();         // 두 경로가 공유한다 (아래)

private:
    bool bClaimed = false;      // 서버 전용, 복제 안 함 (Step 02)
    TSharedPtr<FStreamableHandle> MeshHandle;
};
```

**스포너가 뿌린 것도 플레이어가 버린 것도 같은 진입점을 쓴다.** 스포너는 롤 결과로 `MakeItemState()`를 만들어 넘기고(01-2), 버리기는 인벤토리 엔트리의 `State`를 **값으로 복사해** 넘긴다(Step 03).

```cpp
// 버리기 (Step 03)
Pickup->InitPickup(Entry.ItemId, Entry.State);      // ← 값 복사. 잔탄이 여기서 보존된다
```

핸들 유무로 갈리던 두 경로가 하나가 된다. **버린 무기의 잔탄이 보존되는 이유는 규칙을 지켜서가 아니라 값을 복사했기 때문**이므로, 이관 프로토콜도 `EndPlay` 정리도 필요 없다.

### ★ `State`를 복제하지 않는 이유는 정보 은폐다

**바닥 무기의 잔탄이 복제되면 치트 클라이언트가 릴러번시 범위 내 모든 픽업을 읽어 "어디서 얼마 전에 교전이 있었는지"를 추론한다.** `12/30`짜리 라이플이 바닥에 있다 = **여기서 누가 죽었다.** GAME.md가 두 번 명시한 정보 은폐 기둥을 사고로 뒤집는다.

> 근거를 대역폭에 두지 않는다. Step 03에서 이 필드가 `TArray<FEPInventoryEntry> Payload`로 바뀌면 "작으니까 상관없다"는 논리가 그대로 뒤집히지만, **정보 은폐는 배열이 되어도 그대로다.**

`UPROPERTY()`를 붙이되 `GetLifetimeReplicatedProps`에 등록하지 않는다 — 직렬화·GC 대상이면서 복제는 안 되는 상태다.

### ★ 그런데 `ItemId`는 **반드시 등록해야 한다**

```cpp
// EPPickup.cpp — #include "Net/UnrealNetwork.h"
void AEPPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AEPPickup, ItemId);
    // ★ State는 여기 없다. 없는 것이 의도다 (위)
}
```

**`ReplicatedUsing = OnRep_ItemId`만 붙이는 것으로는 복제되지 않는다.** 그 지정자는 *"복제되면 이 함수를 불러라"* 를 정할 뿐이고, **복제 여부를 정하는 것은 이 등록**이다. 빠뜨리면 `OnRep_ItemId()`가 영원히 안 불린다.

> **증상이 함정 #12와 정반대라 더 나쁘다.** 리슨서버 창에서는 `InitPickup → ApplyVisual()` 경로로 **정상적으로 보인다.** 클라 창에서만 빈 픽업이 뜬다. 문서가 #12로 *"클라엔 보이는데 서버엔 안 보인다"* 를 크게 적어놨으므로, 증상이 뒤집혔다는 것을 못 알아채면 `ApplyVisual` 분리 쪽을 먼저 뒤지게 된다.
>
> **두 증상을 가르는 질문은 하나다** — *"서버 창에는 보이는가?"* 보이면 등록 누락, 안 보이면 `ApplyVisual` 누락이다.

`AEPWeapon`(`EPWeapon.h:54`) / `AEPCorpse`(`EPCorpse.h:61`)와 같은 형태다.

> **★ Step 03에서 이 필드가 `TArray<FEPInventoryEntry> Payload`로 교체된다** (추가가 아니라 **교체**). 배낭을 버리면 안의 아이템이 같이 나가야 하고(GAME.md), 나중에 부착물 달린 총도 마찬가지다(§7-3).
>
> **지금 배열로 만들지 않는다.** 스포너가 뿌리는 것은 언제나 아이템 하나이고, 원소가 항상 1개인 배열은 읽는 쪽에 군더더기만 남긴다. 컨테이너를 버리는 경로가 실제로 생기는 Step 03에서 확장한다.
>
> **그때 고칠 곳은 여기 두 줄이 전부다** — "절반 다시 쓰는" 것이 아니다.
>
> | Step 03에서 바뀌는 것 | 바뀌지 않는 것 |
> |---|---|
> | `InitPickup`의 둘째 인자 (`const FEPItemState&` → `TArray<FEPInventoryEntry>&&`) | **호출 지점** (`SpawnActor` 직후, 같은 프레임) |
> | `State` 필드 → `Payload` 필드, `GetState()` → `GetPayload()` | 생성자 / 콜리전 / Dormancy / `ApplyVisual` / `OnRep_ItemId` / `EndPlay` 없음 / 복제 등록 없음 |

> **★ `IEPInteractable`은 아직 상속하지 않는다.** 그 인터페이스는 Step 02에서 만들어지므로, Step 01의 클래스 선언에 `public IEPInteractable`을 미리 써두면 **이 단계만으로는 컴파일되지 않는다.** Step 02에서 상속과 4함수 구현을 함께 추가한다. `bClaimed`는 필드만 미리 두어도 무해하므로 여기 남긴다.

### 생성자 — 네트워크 예산

```cpp
AEPPickup::AEPPickup()
{
    PrimaryActorTick.bCanEverTick = false;      // 픽업은 스스로 할 일이 없다

    bReplicates = true;
    bAlwaysRelevant = false;                    // 기본값이지만 명시
    SetReplicateMovement(false);                // 정적으로 놓인 아이템
    NetDormancy = DORM_Initial;                 // ★ 초기 1회 복제 후 휴면

    SetNetCullDistanceSquared(25000000.f);      // 5000cm의 제곱
    SetNetUpdateFrequency(1.f);

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;

    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionObjectType(ECC_WorldDynamic);        // 런타임 스폰물 — 기본값은 WorldStatic이다
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);   // ★ 아무것도 막지 않는다
}
```

### ★ 콜리전을 지금 정하지 않으면 Step 01 안에서 바로 보인다

`SetCollisionEnabled(QueryOnly)` **한 줄만으로는 아무것도 안 풀린다.** 그건 물리 시뮬레이션만 끄고 **쿼리는 그대로 막는다** — 캐릭터 이동 스윕이 바로 그 쿼리다.

C++로 만든 `UPrimitiveComponent`의 기본 프로파일은 **`BlockAll`** 이다.

```cpp
// PrimitiveComponent.cpp:356 (생성자)
SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

// BaseEngine.ini:3111
+Profiles=(Name="BlockAll", ObjectTypeName="WorldStatic", CustomResponses=, ...
           HelpMessage="WorldStatic object that blocks all actors by default.
                        All new custom channels will use its own default response.")
```

`CustomResponses`가 비어 있으므로 **기본 응답 컨테이너를 그대로 쓴다 — 전 채널 `Block`**(`CollisionProfile.cpp:373` `SetAllChannels(ECR_Block)`).

| 채널 | 손대지 않은 픽업 | 결과 |
|---|---|---|
| `Pawn` | **Block** | **플레이어가 플레이스홀더 Cube에 걸리고 올라탄다.** 바닥 아이템 5개가 장애물이 된다 |
| `Visibility` | **Block** | `bAlignToGround` 라인트레이스가 **먼저 뿌린 픽업**에 걸려 다음 픽업이 그 위에 얹힌다 |
| `WeaponTrace` (`ECC_GameTraceChannel1`) | Ignore | 총알은 안 막힌다 — `DefaultEngine.ini:306`이 `DefaultResponse=ECR_Ignore`이고 `BlockAll`이 그걸 덮지 않기 때문 |

**"맞는다"는 Step 02에 가야 검증되지만 "걸린다"는 PIE에서 즉시 보인다.** 프로파일 미정의 비용은 Step 02로 미뤄지지 않는다.

### 이음매 — Step 01은 전부 닫고, Step 02가 정확히 하나를 연다

Step 02가 `EP_TraceChannel_Interact`를 만들면 **한 줄만 추가한다.**

```cpp
    Mesh->SetCollisionResponseToChannel(EP_TraceChannel_Interact, ECR_Block);
```

Step 01이 **존재하지 않는 채널을 참조하지 않으므로 채널 생성을 앞당길 필요가 없다.** 그리고 이 단계가 검증 가능한 완료 조건을 하나 얻는다(*"쏴도 안 막히고 밟아도 안 걸린다"*).

> **프로파일 에셋(`.ini`의 `+Profiles=`)을 지금 만들지 않는 이유.** 사용자가 픽업 하나뿐이고, 프로파일은 값이 에디터에서 안 보이는 곳(`DefaultEngine.ini`)에 흩어진다. 반응이 세 줄이면 코드가 읽기 쉽다.
>
> **옮길 시점은 상호작용 대상이 셋이 될 때다** — 픽업 / 컨테이너(§7-1) / 자판기(§7-2). 그때 `"EPInteractable"` 프로파일로 묶는다. **소비자가 둘일 때는 그 자리에 두고 셋이 될 때 뺀다**는 이 프로젝트의 일반 기준과 같다.

> **★ 5.5부터 필드 직접 대입은 deprecated다.** `NetCullDistanceSquared`(`Actor.h:869`)와 `NetUpdateFrequency`(`Actor.h:874`)에 `UE_DEPRECATED(5.5, ...)`가 붙어 있어 `NetCullDistanceSquared = ...`로 쓰면 5.7에서 경고가 난다. **세터를 쓴다.** 반면 `bReplicates`(`Actor.h:556`) / `bAlwaysRelevant`(`Actor.h:300`) / `NetDormancy`(`Actor.h:832`)는 deprecated가 아니므로 생성자 대입 그대로 둔다.

> **★ `SetReplicateMovement(false)`인데 위치가 맞는 이유.** 완료 조건 1이 *"같은 위치에 보인다"* 인데 이동 복제를 껐으니 모순처럼 읽힌다. **스폰 위치는 프로퍼티 복제가 아니라 액터 채널의 최초 번들에 실려 간다.**
>
> ```cpp
> // PackageMapClient.cpp:612  UPackageMapClient::SerializeNewActor
> Location = FRepMovement::RebaseOntoZeroOrigin(Actor->GetActorLocation(), Actor);
> // :722 — bReplicateMovement를 보지 않는다
> ConditionallySerializeQuantizedVector(Location, FVector::ZeroVector, GbQuantizeActorLocationOnSpawn, bSerializeLocation);
> ```
>
> `SetReplicateMovement(false)`가 끄는 것은 **이후의 `ReplicatedMovement` 갱신**이다. 픽업은 스폰 뒤 안 움직이므로 정확히 필요 없는 것만 꺼진다.
>
> 다만 **위치는 0.1cm 단위로 양자화된다** — `net.QuantizeActorLocationOnSpawn`의 기본값이 `true`이고(`:210`) `FVector_NetQuantize10`으로 나간다(`:707`). 두 창의 `EP.Loot.List` 좌표가 소수점에서 갈리는 것은 정상이다. **완료 조건 1은 "cm 단위로 같은가"로 본다.**

> **★ 단위 함정:** `NetCullDistanceSquared`의 기본값은 `225000000`(= 15000cm의 제곱)이다. 여기에 `5000`을 그대로 넣으면 컬링 거리가 √5000 ≈ **70cm**가 되어 픽업이 코앞에서만 보인다. "왜 아이템이 안 보이지"로 한참 헤매는 대표적인 실수다.

### Dormancy 규칙

```
스폰       → 한 번 복제된 뒤 휴면
획득 완료  → Destroy()    ← 휴면 중인 연결에도 전달된다 (채널이 아니라 destruction info로)
```

> **메커니즘 전체는 `DOCS/Mine/Concepts/Dormancy.md`에 있다.** 여기서는 픽업에 필요한 결론만 다룬다 — 왜 `DORM_Initial`인지(§6), 왜 `FlushNetDormancy()`를 부를 자리가 없는지(§7), 왜 멀어져도 안 사라지는지(§4).

**복제되는 값이 `ItemId` 하나뿐이고 그것은 스폰 시점에 정해져 바뀌지 않는다.** 스택이 있던 설계에서는 부분 획득 시 `Quantity`를 낮추고 `FlushNetDormancy()`를 불러야 했고, 빠뜨리면 "서버는 정상인데 클라 화면의 개수만 옛날 값"이라는 재현 까다로운 버그가 났다. **그 호출도 그 함정도 이제 없다.**

### ★ 동적 스폰 액터에서 `DORM_Initial`은 `DORM_DormantAll`과 같다

`DORM_Initial`의 "처음부터 휴면" 특별 취급은 **맵에 배치된 액터에만** 적용된다.

```cpp
// NetDriver.cpp:8347
bool UNetDriver::IsDormInitialStartupActor(AActor* Actor)
{
    return Actor && Actor->IsNetStartupActor() && (Actor->NetDormancy == DORM_Initial);
}
// EngineTypes.h:3370 — "This actor is initially dormant for all connection if it was placed in map."
```

스폰된 픽업은 `IsNetStartupActor()`가 false라 여기 걸리지 않는다 — 정상 네트워크 액터 리스트에 들어가고(`NetDriver.cpp:928`), `ShouldActorGoDormant`가 **`!Channel`이면 false를 반환하므로**(`:5316`) **채널이 열려 한 번 복제된 뒤에야** `StartBecomingDormant()`가 불린다(`:5435-5440`).

**즉 문서의 "초기 1회 복제 후 휴면"은 결과적으로 맞지만, 맞는 이유가 다르다.** `DORM_Initial`이라서가 아니라 **동적 스폰이라 `DORM_Initial`이 적용되지 않기 때문**이다. 그래서 여기서는 `DORM_DormantAll`을 써도 동작이 같다.

### ★ 한 번 본 픽업은 멀어져도 클라에서 사라지지 않는다 — 이게 정상이다

**결정하는 것은 "채널이 닫혔는가"가 아니라 "왜 닫혔는가"다.** 클라의 `UActorChannel::CleanUp`이 닫힌 이유를 보고 액터를 살릴지 죽일지 가른다.

```cpp
// DataChannel.cpp:2691-2698  UActorChannel::CleanUp
else if (Dormant && (CloseReason == EChannelCloseReason::Dormancy) && !Actor->GetTearOff())
{
    Connection->Driver->ClientSetActorDormant(Actor);   // ★ 액터를 살려둔다
    Connection->Driver->NotifyActorFullyDormantForConnection(Actor, Connection);
    bWasDormant = true;
}
else if (...)
{
    // Destroy the actor                                 ← 그 외의 이유면 클라에서 파괴
```

| 닫힌 이유 | 클라 결과 |
|---|---|
| `EChannelCloseReason::Relevancy` (멀어져서) | **액터 파괴.** 다시 가까이 가면 채널 재생성 + 초기 번들 재전송 |
| `EChannelCloseReason::Dormancy` (다 보내서) | **액터 유지.** 채널만 닫히고 화면엔 그대로 |

픽업은 **릴러번시로 닫히기 전에 휴면으로 먼저 닫힌다.** `NetDormancy = DORM_Initial`이 없으면 이 액터는 릴러번시 경로로 가고, 멀어질 때마다 사라졌다 나타나며 그때마다 초기 번들을 다시 받는다. **팝핑과 대역폭 둘 다 여기서 갈린다.**

서버 쪽 진행은 이렇다.

| 패스 | 일어나는 일 | 근거 |
|---|---|---|
| N | 채널 없음 → 릴러번시 검사 통과 → **채널 생성, `ItemId` 복제** | `NetDriver.cpp:5386-5399` — 릴러번시 검사는 `if (!Channel)` **안쪽**에 있다 |
| N+1 | 채널 있으므로 릴러번시를 건너뛰고 `ShouldActorGoDormant` → `StartBecomingDormant()` | `:5434-5437` |
| N+k | 남은 프로퍼티를 다 보낸 뒤 `Close(EChannelCloseReason::Dormancy)` | `DataChannel.cpp:4577-4578` |
| — | 클라는 위 분기를 타고 **액터를 유지** | `DataChannel.cpp:2691` |
| 이후 | `DormantConnections`에 등록 → 그 연결에 대해 `continue` | `:5429`, `NetworkObjectList.cpp:219-223` |
| 전원 휴면 시 | 활성 목록에서 **아예 빠진다** — 루프에 안 들어온다 | `NetworkObjectList.cpp:263-265` |

- **아직 못 본 픽업** → `SetNetCullDistanceSquared(25000000.f)`가 작동한다. 채널이 없으므로 릴러번시 검사를 받고, 5000cm 밖이면 채널이 안 열려 클라에 존재하지 않는다. **가까이 가면 나타난다**
- **한 번 본 픽업** → 채널은 닫혔지만 액터는 살아 있고 **대역폭은 0이다**

> **주의 — 휴면 상태에서도 릴러번시 검사에는 도달한다.** 휴면으로 채널이 닫히면 `FindActorChannelRef`가 null을 돌려주므로 다음 패스에서 `if (!Channel)` 분기에 들어가고, 멀리 있으면 거기서 `continue`된다. **클라에서 안 사라지는 이유는 서버가 검사를 건너뛰어서가 아니라 클라가 이미 `ClientSetActorDormant`로 유지 판정을 받았기 때문이다.** 서버 루프 위치로 설명하려 들면 틀린다.

### 파괴는 별도 경로로 나간다

`UNetDriver::NotifyActorDestroyed`(`NetDriver.cpp:4336-4356`)가 연결마다 갈린다. 채널이 있으면 `Channel->Close()`, 없으면 — 엔진 주석 그대로:

> *"Make a new destruction info if necessary. It is necessary if the actor is dormant or recently dormant because **even though the client knew about the actor at some point, it doesn't have a channel to handle destruction.**"*

**무관해서 통과하는 게 아니라 엔진이 그 경우를 따로 처리한다.** 채널이 **한 번도 열린 적 없는** 클라(멀어서 릴러번시 밖)에는 `bDormantOrRecentlyDormant`도 false, 동적 액터라 `bShouldCreateDestructionInfo`도 false라 **아무것도 안 보낸다** — 클라가 애초에 모르므로 무해하다.

### 메시 적용 — 플레이스홀더가 필수다

**★ `OnRep_ItemId()`에 메시 코드를 그냥 넣으면 완료 조건 1번이 실패한다.** `OnRep_`은 **복제를 받는 쪽에서만** 불린다 — 서버에서는 값을 직접 대입하므로 호출되지 않는다. PIE 기본 넷 모드가 *Play As Listen Server*이므로 **서버 창에서만 픽업이 안 보이고**, "클라에서는 보이는데 서버에서는 안 보인다"는 엉뚱한 방향으로 원인을 찾게 된다.

적용 함수를 따로 빼고 **두 경로에서 부른다.**

```cpp
// 헤더에 추가: void ApplyVisual();

void AEPPickup::InitPickup(FName InItemId, const FEPItemState& InState)
{
    ItemId = InItemId;
    State  = InState;

    ApplyVisual();      // ★ 서버 경로 — 리슨서버/스탠드얼론의 로컬 화면
}

void AEPPickup::OnRep_ItemId()
{
    ApplyVisual();      // 클라이언트 경로
}

void AEPPickup::ApplyVisual()
{
    if (IsNetMode(NM_DedicatedServer)) return;      // 데디서버는 시각 에셋을 로드하지 않는다

    // 플레이스홀더는 설정에서 온다. 이것도 TSoftObjectPtr이므로 최초 1회 동기 로드해 캐시한다
    const UEPLootDeveloperSettings* Settings = GetDefault<UEPLootDeveloperSettings>();
    if (UStaticMesh* Placeholder = Settings->PlaceholderPickupMesh.LoadSynchronous())
        Mesh->SetStaticMesh(Placeholder);           // 먼저 플레이스홀더

    UGameInstance* GI = GetGameInstance();
    UEPItemDefinitionSubsystem* Defs =
        GI ? GI->GetSubsystem<UEPItemDefinitionSubsystem>() : nullptr;
    UEPItemDefinition* Def = Defs ? Defs->FindDefinition(ItemId) : nullptr;

    if (!Def || Def->WorldMesh.IsNull()) return;

    MeshHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        Def->WorldMesh.ToSoftObjectPath(),
        FStreamableDelegate::CreateWeakLambda(this, [this, Def]()
        {
            if (UStaticMesh* Loaded = Def->WorldMesh.Get())
                Mesh->SetStaticMesh(Loaded);
        }));
}
```

`IsNetMode(NM_DedicatedServer)` 가드는 그대로 필요하다 — 데디서버에서는 `InitPickup` 경로로 들어와도 시각 에셋을 로드하지 않아야 한다. **가드는 "서버냐"가 아니라 "화면이 있느냐"를 판정한다.**

**현재 `WorldMesh`가 있는 아이템은 하나도 없다.** 무기조차 `WeaponMesh`(스켈레탈)만 있고 `WorldMesh`는 비어 있다. 플레이스홀더 박스는 선택이 아니라 **이번 단계의 유일한 표시 수단**이다.

> `CreateWeakLambda`를 쓴다. 로드가 도착하기 전에 픽업이 파괴되면(다른 플레이어가 먼저 주움) 일반 람다는 죽은 객체를 건드린다.

> `FindDefinition`은 **게임 인스턴스 서브시스템**에서 온다(Step 00). 클라이언트에도 같은 서브시스템이 있고 `Initialize()`에서 이미 전부 로드해 뒀으므로, 여기서 Definition을 못 찾으면 **네트워크 문제가 아니라 Step 00 등록 문제**다 — `EP.Item.Dump`로 먼저 확인한다.

> **★ `MeshHandle`에 대입되기 전에 람다가 실행될 수 있다.** 메시가 이미 메모리에 있으면 완료 델리게이트가 `RequestAsyncLoad` **안에서 동기 호출**된다 — 엔진 주석이 그렇게 적혀 있다(`StreamableManager.cpp:2009` *"this may call the callback right away"*). 지금 람다는 `MeshHandle`을 건드리지 않아 무해하지만, **람다 안에서 `MeshHandle`을 만지는 수정이 들어오면 그 순간 null이다.**

> **여기서는 핸들 null을 판정에 써도 된다.** Step 00에서 배운 "핸들로 판정하지 말라"는 `LoadPrimaryAssets` 이야기이고, `RequestAsyncLoad`는 다른 함수다 — null을 반환하는 경로가 **짧은 패키지 이름**(`StreamableManager.cpp:1852`, Error 로그)과 **요청이 비었거나 전부 null**(`:1862`, Display 로그) 둘뿐이고 **둘 다 진짜 오류**다. "이미 로드됨"은 null이 아니라 유효한 핸들이 나온다. 습관적으로 안 쓰면 정작 잡아야 할 `WorldMesh` 경로 오타를 놓친다.

### `EndPlay` 오버라이드가 필요 없다

이전 설계는 여기서 `InstanceSubsystem->Destroy(Handle)`을 불러야 했고, **획득 시 핸들을 비우는 순서를 지키지 않으면 방금 인벤토리로 넘긴 인스턴스를 지워 잔탄이 사라졌다.** `State`가 값이므로 픽업이 파괴되면 그냥 같이 사라진다 — 지울 대상도, 지키는 순서도 없다.

---

## 01-5. 디버그 도구

```cpp
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "EP Loot"))
class EMPLOYMENTPROJ_API UEPLootDeveloperSettings : public UDeveloperSettings   // Step 00에서 생성, 여기서 확장
{
    // ★ 피커에 FEPItemData 행을 쓰는 DT만 뜬다 (아래)
    UPROPERTY(Config, EditAnywhere, Category = "Data",
              meta = (RequiredAssetDataTags = "RowStructure=/Script/EmploymentProj.EPItemData"))
    TSoftObjectPtr<UDataTable> ItemDataTable;

    UPROPERTY(Config, EditAnywhere, Category = "Debug")
    bool bEnableLootDebugLog = false;

    UPROPERTY(Config, EditAnywhere, Category = "Debug")
    bool bEnableSpawnerDebugDraw = false;

    /** WorldMesh가 없는 아이템의 폴백 메시. 개발용이며 프로덕션에서도 남는다 (01-4) */
    UPROPERTY(Config, EditAnywhere, Category = "Loot")
    TSoftObjectPtr<UStaticMesh> PlaceholderPickupMesh;

    /** 비우면 AEPPickup을 그대로 쓴다. BP로 확장할 때만 지정 (아래) */
    UPROPERTY(Config, EditAnywhere, Category = "Loot")
    TSoftClassPtr<AEPPickup> PickupClass;
};
```

### 왜 `ItemDataTable`에만 `meta =`가 붙어 있는가

**푸는 문제:** Project Settings 화면에서 이 필드들은 **에셋 드롭다운(피커)**으로 뜬다. 거기 잘못된 에셋이 후보로 뜨면 언젠가 누가 고른다. 후보를 좁히는 방법이 둘이고, **둘 중 하나면 충분하다.**

**① 타입이 먼저 거른다 — 대개 이걸로 끝이다**

```cpp
TSoftObjectPtr<UStaticMesh> PlaceholderPickupMesh;
//              ^^^^^^^^^^^ 이 한 단어가 곧 필터다
```

피커에 `UStaticMesh`만 뜬다. 텍스처도 사운드도 안 뜬다. **아무것도 안 붙여도** 그렇다. 그래서 `PlaceholderPickupMesh`와 `PickupClass`에는 메타가 없다 — `AllowedClasses` / `MetaClass`를 붙여봐야 타입이 이미 한 일을 한 번 더 하는 것이다.

> Lyra가 `AllowedClasses`를 쓰는 자리(`LyraAudioSettings.h:224`)는 `FSoftObjectPath`다. **타입이 "그냥 경로"라 아무거나 담기므로** 손으로 좁혀야 한다. 우리 필드들은 그 상황이 아니다.

**② 타입으로 안 되는 게 하나 있다 — `UDataTable`**

`TSoftObjectPtr<UDataTable>`이 좁혀주는 범위는 **"아무 DataTable"**까지다. **행 구조체가 무엇인지는 C++ 타입에 안 담긴다** — `UDataTable`은 런타임에 아무 구조체나 담는 컨테이너이기 때문이다.

| 에셋 | 행 구조체 | 타입 필터만 | 원하는 것 |
|---|---|---|---|
| `DT_Items` | `FEPItemData` | 뜬다 | ✓ |
| `DT_Rarity` (나중) | `FEPRarityRow` | **뜬다** | ✗ |
| `DT_Price` (나중) | `FEPPriceRow` | **뜬다** | ✗ |

**③ 그래서 여기만 에셋 레지스트리 태그로 거른다**

DataTable이 자기 행 구조체를 태그로 광고하고, 피커가 그 태그를 읽는다.

```cpp
// DataTable.cpp:361-365 — UDataTable::GetAssetRegistryTags
// Add the row structure tag
static const FName RowStructureTag = "RowStructure";
Context.AddTag(FAssetRegistryTag(RowStructureTag, GetRowStructPathName().ToString(), ...));

// SPropertyEditorAsset.cpp:258 — 프로퍼티 에디터가 메타에서 요구 태그를 읽고
const FString RequiredAssetDataTagsFilterString = MetadataProperty->GetMetaData("RequiredAssetDataTags");

// SPropertyEditorAsset.cpp:296 — IsAssetFiltered: 태그가 안 맞으면 목록에서 뺀다
if (!InAssetData.TagsAndValues.ContainsKeyValue(RequiredTagAndValue.Key, RequiredTagAndValue.Value))
```

두 번째 DT(등급표·가격표 등)가 생겨도 **피커에 뜨지 않으므로 잘못 고를 수가 없다.**

> **판단 기준: `TSoftObjectPtr<T>`는 `T`까지 걸러준다. `T`보다 더 좁혀야 할 때만 메타를 쓴다.**
> 이 설정에서 그런 필드는 `ItemDataTable` 하나뿐이다.

> 플레이스홀더는 엔진 기본 에셋 `/Engine/BasicShapes/Cube.Cube`를 그대로 넣으면 된다. 별도로 만들 필요 없다.

### `PickupClass`를 설정에 두는 이유

**픽업을 스폰하는 경로가 둘이 된다.**

```
스포너 SpawnLoot()      (Step 01)  ─┐
플레이어 버리기           (Step 03)  ─┴─→ 같은 클래스를 써야 한다
```

버리기 경로에는 **물어볼 스포너가 없다.** 스포너 단위 `UPROPERTY`로 두면 두 경로가 갈리고, 그러면 "바닥에 깔린 픽업과 내가 버린 픽업이 다르게 보인다"가 된다.

### ★ `BP_Pickup`에 넣을 것과 넣지 말아야 할 것

**액터를 BP로 한 겹 감싸는 것은 UE 관례다.** 다만 **아이템마다 달라지는 것을 여기 넣으면 안 된다.**

| | 어디에 |
|---|---|
| 둥둥 떠서 회전하는 연출 (타임라인) | **`BP_Pickup`** — 액터 수준 |
| 상호작용 프롬프트 위젯 컴포넌트 | **`BP_Pickup`** |
| 오디오 컴포넌트 / 포인트 라이트 부착 | **`BP_Pickup`** |
| 월드 메시 | ❌ `Definition->WorldMesh` |
| **획득 사운드 / VFX** | ❌ **`UEPItemDefinition`.** 총 줍는 소리와 붕대 줍는 소리가 같으면 안 된다 |
| 등급별 이펙트 | ❌ DT의 `Rarity`로 분기 |

> **Lyra가 이 경계를 명확히 보여준다.** `ULyraPickupDefinition`(`Equipment/LyraPickupDefinition.h`)이 `DisplayMesh` / `PickedUpSound` / `PickedUpEffect` / `RespawnedEffect`를 **DataAsset에** 들고 있고, **액터 클래스 필드는 없다.** 표현 데이터는 아이템 쪽, 액터는 하나다.
>
> 이 표가 없으면 나중에 누가 `BP_Pickup`에 획득 사운드를 넣고, 그러면 **아이템마다 BP를 만들게 된다.** 그게 이 필드로 인한 진짜 실수다.

### ★ 타입은 `TSoftClassPtr`다. `TSubclassOf`가 아니다

`UPROPERTY(Config) TSubclassOf<X>`에 `.ini`로 **BP 경로**를 주면 이렇게 된다.

```cpp
// UObjectGlobals.cpp:4379-4381  — CDO가 만들어지는 순간
if (bIsCDO || Class->HasAnyClassFlags(CLASS_PerObjectConfig))
{
    Obj->LoadConfig(NULL, NULL, bIsCDO ? UE::LCPF_ReadParentSections : UE::LCPF_None);
}

// PropertyBaseObject.cpp:593-596  — 그 안에서 FClassProperty가 타는 경로
const uint32 LoadFlags = LOAD_NoWarn | LOAD_FindIfFail;
Result = StaticLoadObject(ObjectClass, nullptr, Text, nullptr, LoadFlags, nullptr, true);
```

**지연 로드를 없앤 게 아니라 엔진 초기화 중으로 옮긴 것이다.** BP를 로드하면 그 CDO가 만들어지고, CDO가 하드 참조하는 메시·머티리얼·나이아가라가 전부 딸려온다.

**그리고 실패가 조용하다.** `LOAD_NoWarn | LOAD_FindIfFail`이라 경고가 안 뜨고 값은 null이 된다 — *"초기화가 너무 이름"* / *"경로 오타"* / *"`.ini`에 안 적음"* 이 **전부 같은 증상으로 수렴**하고, 증상은 첫 `SpawnLoot()`에서야 나타난다.

`TSoftClassPtr`는 그렇지 않다.

```cpp
// PropertySoftObjectPtr.cpp:150-165  FSoftObjectProperty::ImportText_Internal
FSoftObjectPath SoftObjectPath;
bImportTextSuccess = SoftObjectPath.ImportTextItem(...);    // ★ 경로만 만든다. StaticLoadObject 없음
```

**엔진·Lyra 관례도 소프트다.** `LyraUIMessaging.h:32-43`이 정확히 이 자리에서 **쌍**을 쓴다.

```cpp
UPROPERTY()                                        // 런타임 해석본 — config 아님
TSubclassOf<UCommonGameDialog> ConfirmationDialogClassPtr;

UPROPERTY(config)                                  // config에 노출되는 쪽은 소프트
TSoftClassPtr<UCommonGameDialog> ConfirmationDialogClass;
```

엔진에서 `UPROPERTY(config)` + `TSubclassOf`는 `WorldSettings::DefaultBookmarkClass`(`:933-934`)처럼 **네이티브 클래스**일 때만 쓴다 — BP가 아니라서 로드가 없다.

### 소비 지점에서 세 상태를 가른다

```cpp
const UEPLootDeveloperSettings* Settings = GetDefault<UEPLootDeveloperSettings>();

// 첫 스폰에서 한 번만 로드된다. 이후에는 이미 메모리에 있어 즉시 반환
UClass* PickupCls = Settings->PickupClass.IsNull()
    ? AEPPickup::StaticClass()                 // .ini 미설정 = 정상. C++ 기본으로 간다
    : Settings->PickupClass.LoadSynchronous();

if (!PickupCls)                                // 설정은 됐는데 로드 실패 = 진짜 오류
{
    UE_LOG(LogTemp, Error, TEXT("[Loot] PickupClass 로드 실패: %s"),
           *Settings->PickupClass.ToString());
    return;
}
```

| 상태 | 의미 | 처리 |
|---|---|---|
| `IsNull()` | `.ini` 미설정 — **정상** | `AEPPickup::StaticClass()` |
| `!IsNull()` + 로드 실패 | 경로 오타·에셋 삭제 — **오류** | Error 로그 후 중단 |
| 로드 성공 | 정상 | 사용 |

`RollLootTable`의 **반환 규약 두 겹**(01-1)과 같은 형태다 — 조용히 수렴하던 상태들을 갈라 놓는다.

> **`TSoftClassPtr`로 바꾸면 헤더의 `= AEPPickup::StaticClass()` 기본값이 없어진다.** 소프트 포인터의 기본값은 빈 경로이고, `.ini`의 빈 값도 값이라 C++ 기본값이 이기지 못한다. **그래서 폴백을 소비 지점으로 옮긴다.**

> 소프트 참조는 **당장 필요 없는 것**에도 쓴다 — `WorldMesh` / `Icon` / `WeaponMesh`가 그쪽이다. 여기서 소프트를 쓰는 이유는 지연 로드가 아니라 **로드 시점을 우리가 고르기 위해서**다.

| 커맨드 | 용도 | 실행 가능 |
|---|---|---|
| `EP.Loot.RollTable <이름> <횟수>` | N회 굴려 아이템별·등급별 집계 출력 | 어디서나 (순수 조회) |
| `EP.Loot.Respawn` | 모든 스포너 `ClearLoot()` 후 `SpawnLoot()` | **서버만** |
| `EP.Loot.List` | 월드의 모든 `AEPPickup` | **서버·클라 양쪽** — 대조가 검증이다 |

> **`Respawn`은 클라에서 조용히 실패한다.** 스포너의 `HasAuthority()`가 클라에서 false이므로(01-2) 아무 일도 안 일어나고 로그도 없다. 커맨드 쪽에서 막고 이유를 찍는다.
>
> ```cpp
> if (World->GetNetMode() == NM_Client)
> {
>     UE_LOG(LogTemp, Warning, TEXT("[Loot] Respawn은 서버 전용입니다."));
>     return;
> }
> ```
>
> 순수 조회는 클라에서 허용한다 — Step 00이 `EP.Item.Dump`에 세운 기준과 같다.

### 어디에 만드는가 — `Private/Loot/EPLootDebugCommands.cpp` (새 파일, 헤더 없음)

**Step 00과 위치 기준이 다르다.** `EP.Item.*`는 소비 대상이 `UEPItemDefinitionSubsystem` 하나뿐이라 그 `.cpp` 끝에 붙였다(`EPItemDefinitionSubsystem.cpp:158-213`). `EP.Loot.*` 셋은 **`UEPLootTable` · `AEPItemSpawner` · `AEPPickup` 셋을 가로지른다.** 어느 한 클래스의 `.cpp`에 넣으면 나머지 둘을 그 파일이 알아야 한다.

| 커맨드 | 건드리는 것 |
|---|---|
| `RollTable` | `UEPLootTable` + `UAssetManager` + Definition 서브시스템 |
| `Respawn` | `AEPItemSpawner` |
| `List` | `AEPPickup` |

> **Step 03의 `EP.Inv.*`는 이 파일에 넣지 않는다.** 그쪽은 주인이 인벤토리 컴포넌트 하나로 명확하므로 Step 00 기준(소비 대상의 `.cpp`)이 그대로 맞다. **주인이 없을 때만 전용 파일을 만든다.**

### 골격

Step 00과 같은 관례를 그대로 쓴다 — 셋 다 `FAutoConsoleCommandWithWorldAndArgs` + `ECVF_Cheat` + 쉬핑 제외.

```cpp
// EPLootDebugCommands.cpp — 이 파일 전체가 가드 안에 들어간다
#include "Loot/EPLootTable.h"
#include "Loot/EPItemSpawner.h"
#include "Loot/EPPickup.h"
#include "Data/EPItemDefinitionSubsystem.h"
#include "Data/EPItemData.h"
#include "Engine/AssetManager.h"
#include "EngineUtils.h"          // TActorRange
#include "Engine/World.h"
#include "Engine/GameInstance.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

// ... 세 개의 static FAutoConsoleCommandWithWorldAndArgs ...

#endif
```

**`static` 전역 객체의 생성자가 등록을 한다.** 아무도 이 파일을 `#include` 하지 않아도, 어디서도 함수를 부르지 않아도 모듈이 로드되면 커맨드가 생긴다 — 그래서 헤더가 필요 없다. Step 00에서 이미 그렇게 동작하고 있다.

> **이 파일이 링커에서 통째로 버려지는 일은 없다.** 언리얼 모듈은 정적 라이브러리가 아니라 DLL로 링크되므로 `.obj`가 무조건 포함된다. 스태틱 라이브러리였다면 "아무도 참조하지 않는 TU는 버려진다" 문제가 생겼을 자리다.

### ★ Step 00의 `GetItemSubsystem`은 여기서 못 부른다

```cpp
// EPItemDefinitionSubsystem.cpp:159 — 파일 스코프 static = 내부 링키지
static UEPItemDefinitionSubsystem* GetItemSubsystem(UWorld* World)
```

**함수에 붙은 `static`은 "이 `.cpp` 밖에 이 심볼은 존재하지 않는다"는 뜻이다.** 클래스 멤버의 `static`(인스턴스 없이 호출)과 철자만 같고 의미가 다르다. 헤더에 선언도 없으므로 `extern`으로 억지 선언해도 링커가 못 찾는다.

**서브시스템에 정적 접근자를 붙인다.** 같은 모양이 이미 세 곳에 있고(`EPPickup.cpp:66-68`, `EPItemSpawner.cpp:46-48`, 위 헬퍼) 이 파일이 네 번째다.

```cpp
// EPItemDefinitionSubsystem.h — public
static UEPItemDefinitionSubsystem* Get(const UObject* WorldContextObject);
```

```cpp
// EPItemDefinitionSubsystem.cpp — #include "Engine/Engine.h"
UEPItemDefinitionSubsystem* UEPItemDefinitionSubsystem::Get(const UObject* WorldContextObject)
{
    const UWorld* World = GEngine
        ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
        : nullptr;
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    return GI ? GI->GetSubsystem<UEPItemDefinitionSubsystem>() : nullptr;
}
```

**`const UObject*`를 받으면 네 호출 지점의 모양 차이가 사라진다.** 액터는 `Get(this)`, 커맨드는 `Get(World)` — `UWorld`도 `UObject`다.

이건 Epic의 관례 그대로다.

```cpp
// GameplayMessageSubsystem.cpp:42-49 (Lyra 번들 플러그인) — 같은 골격
UGameplayMessageSubsystem& UGameplayMessageSubsystem::Get(const UObject* WorldContextObject)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::Assert);
```

> **반환 형태만 Lyra와 다르게 간다.** Lyra는 참조 + `Assert`로 없으면 크래시시키고, 없을 수 있는 경우를 위해 `HasInstance()`를 따로 뒀다(`:51-56`). **우리 호출자는 콘솔 커맨드다** — PIE 밖에서 치는 건 버그가 아니라 정상 상황이므로 크래시가 아니라 안내 로그가 맞다. 그래서 포인터 + `ReturnNull`이다.

> **기존 세 곳은 지금 고치지 않는다.** 각자 가드가 있고 동작한다. Step 03의 인벤토리가 다섯 번째 호출자가 될 때 한 번에 옮긴다 — 지금 손대면 이번 작업과 무관한 diff가 섞인다(§3).

### `EP.Loot.RollTable` — 순서가 중요하다

**`Table`은 ③에서 생긴다.** 이름 해석(아래 절)이 먼저고 집계 루프가 나중이다.

```cpp
[](const TArray<FString>& Args, UWorld* World)
{
    // ① 인자
    if (Args.Num() < 1)
    {
        UE_LOG(LogTemp, Error, TEXT("[Loot] 사용법: EP.Loot.RollTable <TableName> [Count]"));
        return;
    }
    const int32 Count = (Args.Num() >= 2) ? FMath::Max(1, FCString::Atoi(*Args[1])) : 1000;

    // ② 서브시스템 (등급 역산에 필요)
    const UEPItemDefinitionSubsystem* Sub = UEPItemDefinitionSubsystem::Get(World);
    if (!Sub)
    {
        UE_LOG(LogTemp, Error, TEXT("[Loot] 서브시스템이 없습니다. PIE 실행 중에 치십시오."));
        return;
    }

    // ③ ★ Table이 여기서 생긴다 — 상세는 아래 §이름 해석
    const UEPLootTable* Table = /* GetPrimaryAssetObject<UEPLootTable>(Id) */;
    if (!Table) { /* 에러 로그 */ return; }

    // ④ 집계
}
```

**집계 루프:**

```cpp
int32 Empty = 0, Failed = 0;
TMap<FName, int32> PerItem;
TArray<int32> PerRarity;
PerRarity.SetNumZeroed(StaticEnum<EEPItemRarity>()->NumEnums() - 1);   // ★ -1 (아래)

for (int32 i = 0; i < Count; ++i)
{
    FName Rolled;
    if (!RollLootTable(Table, Rolled)) { ++Failed;  continue; }   // 데이터 오류
    if (Rolled.IsNone())               { ++Empty;   continue; }   // EmptyWeight — 정상

    ++PerItem.FindOrAdd(Rolled);

    // 등급은 롤 함수가 모른다. ItemId → DT 행에서 역산한다
    if (const FEPItemData* Row = Sub->FindData(Rolled))
    {
        ++PerRarity[(int32)Row->Rarity];
    }
}
```

**세 갈래(`Failed` / `Empty` / 아이템)가 `RollLootTable`의 반환 규약 두 겹(01-1)과 1:1로 대응한다.** 이게 이 커맨드가 존재하는 이유다 — 세 상태가 한 숫자로 합쳐지면 확률이 틀렸는지 데이터가 깨졌는지 구분이 안 된다.

> **★ `NumEnums() - 1`을 잊지 마라.** UHT가 모든 `UENUM`에 숨은 `EEPItemRarity_MAX`를 덧붙이고 `NumEnums()`는 그것까지 센다. 빼지 않으면 항상 0인 유령 행이 출력에 하나 더 붙는다. 이름은 `StaticEnum<EEPItemRarity>()->GetNameStringByIndex(i)`로 얻는다 — **`Common`/`Uncommon` 문자열을 손으로 배열에 적지 않는다.** 등급이 늘면 출력이 조용히 어긋난다.

출력의 `목표` 열만 기획표에서 온다.

```cpp
// ★ 기획표가 C++에 들어오는 유일한 지점이다. 등급 가중치를 바꾸면 여기도 바꾼다
static constexpr float TargetPct[] = { 50.f, 30.f, 15.f, 5.f };
```

분모는 `Count`가 아니라 `Count - Empty - Failed`다 (아래 §빈 결과).

### `EP.Loot.Respawn` — 액터 순회

```cpp
if (World->GetNetMode() == NM_Client) { /* 위의 가드 */ return; }

int32 N = 0;
for (AEPItemSpawner* Spawner : TActorRange<AEPItemSpawner>(World))
{
    Spawner->ClearLoot();
    Spawner->SpawnLoot();
    ++N;
}
UE_LOG(LogTemp, Log, TEXT("[Loot] 스포너 %d개 재굴림"), N);
```

`SpawnLoot()` / `ClearLoot()`가 `public`이므로(`EPItemSpawner.h:22-23`) 추가로 열 것이 없다. **`TActorRange`에는 `EngineUtils.h`가 필요하다** — `CoreMinimal.h`에 없다.

### `EP.Loot.List` — 접근자 두 개가 필요하다

지금 `AEPPickup`은 `GetState()`만 공개한다(`EPPickup.h:23`). `ItemId`는 `protected`, `bClaimed`는 `private`이라 **표의 5열 중 2열을 못 찍는다.**

```cpp
// EPPickup.h — public에 두 줄 추가
FName GetItemId() const { return ItemId; }
bool  IsClaimed() const { return bClaimed; }
```

> §2 기준 통과: 두 값 모두 **이 문서의 완료 조건 3·7번이 이름으로 요구**한다(`:20`, `:23`). 상상한 확장점이 아니다.

```cpp
const bool bAuthority = World->GetNetMode() != NM_Client;   // 서버 창인가

UE_LOG(LogTemp, Log, TEXT("  Idx  ItemId          Location            Charges%s  Claimed"),
       bAuthority ? TEXT("") : TEXT("[server-only]"));

int32 Idx = 0;
for (AEPPickup* P : TActorRange<AEPPickup>(World))
{
    const FVector L = P->GetActorLocation();
    if (bAuthority)
    {
        UE_LOG(LogTemp, Log, TEXT("  %-4d %-15s (%.0f, %.0f, %.0f)  %-7d  %s"),
               Idx, *P->GetItemId().ToString(), L.X, L.Y, L.Z,
               P->GetState().Charges, P->IsClaimed() ? TEXT("true") : TEXT("false"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("  %-4d %-15s (%.0f, %.0f, %.0f)  %-7d  -"),
               Idx, *P->GetItemId().ToString(), L.X, L.Y, L.Z, P->GetState().Charges);
    }
    ++Idx;
}
```

**`Idx`는 순회 순서일 뿐 서버·클라가 같다는 보장이 없다.** `TActorRange`는 레벨의 액터 배열 순서를 따르고 클라의 스폰 순서는 채널이 열린 순서다. **대조는 `Idx`가 아니라 `ItemId` + 좌표로 한다** — 완료 조건 7번의 절차(`:23`)가 "`Idx`를 먼저 확인"이라고 적은 것은 *같은 창 안에서* 어느 행을 볼지 고르라는 뜻이다.

> 좌표는 0.1cm로 양자화되므로(`:785`) `%.0f`로 찍는다. 소수점을 찍으면 정상인 차이가 불일치로 보인다.

### ★ 이름 해석 — `GetPrimaryAssetObject`만으로는 못 찾는다

```cpp
// AssetManager.h:218 — "returning nullptr if it's not in memory"
// AssetManager.cpp:1910-1920 — NameData->GetAssetPtr().Get()
```

**메모리에 없으면 nullptr이다.** 그런데 이 커맨드로 검증하고 싶은 대상은 정확히 *아직 아무도 참조하지 않는 새 테이블*이다. **직접 로드해야 한다.**

```cpp
UAssetManager& Manager = UAssetManager::Get();
const FPrimaryAssetId Id(TEXT("LootTable"), FName(*Args[0]));

TSharedPtr<FStreamableHandle> Handle = Manager.LoadPrimaryAsset(Id);
if (Handle.IsValid()) { Handle->WaitUntilComplete(); }

// ★ 핸들이 아니라 결과로 판정한다 (Step 00 함정 — 핸들 null은 "이미 로드됨"일 수도 있다)
const UEPLootTable* Table = Manager.GetPrimaryAssetObject<UEPLootTable>(Id);
if (!Table)
{
    UE_LOG(LogTemp, Error,
        TEXT("[Loot] LootTable '%s'를 찾을 수 없습니다. "
             "에셋 이름 오타이거나 AssetManager에 등록되지 않았습니다 (01-1)."),
        *Args[0]);
    return;
}
```

> **핸들이 null이어도 실패가 아니다.** null은 "새로 로드할 게 없다"(이미 상주) 와 "그런 ID가 없다" 를 **둘 다** 의미한다 — Step 00에서 `Definitions = 0`을 만들었던 바로 그 모호함이다. 그래서 `Table` 포인터로 판정한다.
>
> 로컬 핸들이 스코프를 벗어나면 참조가 풀리지만, 커맨드가 끝난 뒤라 상관없다. 커맨드 실행 중에는 살아 있다.

```
> EP.Loot.List                                    (서버 창)
  Idx  ItemId          Location            Charges  Claimed
  0    Bandage         (1200, 340, 92)     1        false
  1    AmmoBox_545     (880, -20, 90)      45       false     ← 대조 대상

> EP.Loot.List                                    (클라 창 — State는 [server-only])
  Idx  ItemId          Location            Charges  Claimed
  0    Bandage         (1200, 340, 92)     0        -
  1    AmmoBox_545     (880, -20, 90)      0        -         ← 서버의 45가 0이면 통과
```

**★ 두 창 대조가 검증이고, 클라 출력 단독은 아니다.** 클라에서 `Charges = 0`인 것만으로는 "복제 안 됨"과 "진짜 0"을 구분할 수 없다. 그래서 **서버에서 `Charges > 0`인 `Idx`를 먼저 잡고** 같은 행을 클라에서 본다. 거기에 `45`가 찍히면 `DOREPLIFETIME`에 `State`를 넣은 것이다(함정 #10).

> 서버 전용 열은 클라에서 `-`로 찍고 헤더에 `[server-only]`를 표시한다. **관측 대상이 아닌 값을 숫자로 찍으면 읽는 사람은 그것이 검증에 쓰인다고 읽는다.**

**`EP.Loot.List`가 Step 03의 검증 수단이기도 하다.** 이 문서의 완료 조건 3번이 *"`ClearLoot`이 자기 것만 지우는지는 **Step 03에서 재확인**"* 이라고 적었는데, 확인할 수단이 없으면 그 줄이 공수표다.

| 열 | 증명하는 것 | 언제 |
|---|---|---|
| `Charges` (두 창 대조) | **잔탄이 복제되지 않는가** — 완료 조건 7 | Step 01 |
| `Idx` 목록 | `ClearLoot`이 플레이어가 버린 것을 안 지웠는가 | Step 03 |
| `Cooldown` / `Payload` 열 | **Step 03에서 추가한다.** 지금은 필드 자체가 없다 (`DropCooldown`도 `Payload`도 Step 03) | Step 03 |

> 픽업 도구를 Step 03에 두지 않는 이유는 **두 문서로 갈리지 않게** 하기 위함이다. 인벤토리는 `EP.Inv.*`, 월드 픽업은 `EP.Loot.*`로 나눈다.

**`RollTable`이 이 단계의 핵심 검증 수단이다.** 확률은 눈으로 못 믿는다 — 1000회 굴려 등급 비율이 50/30/15/5에 수렴하는지 확인해야 중첩 롤과 `EmptyWeight` 규칙이 맞게 구현됐음을 안다.

### ★ 빈 결과를 분모에서 빼고 찍는다

`LT_Floor_Common`은 `EmptyWeight`를 가진다. 1000회 중 일부는 아무것도 안 나오므로 **전체 대비 비율은 절대 50/30/15/5가 안 된다.** 그 숫자를 그대로 기획표와 비교하면 *"확률이 다 틀렸다"* 로 읽히고, 정상 동작을 버그로 몰게 된다.

```
> EP.Loot.RollTable LT_Floor_Common 1000
  Empty        312 / 1000  (31.2%)          ← EmptyWeight. 분모에서 뺀다
  Failed         0 / 1000                   ← RollLootTable이 false를 반환한 횟수 (데이터 오류)
  --- 아이템이 나온 688회 기준 ---
  Common       349  (50.7%)   목표 50%
  Uncommon     203  (29.5%)   목표 30%
  Rare         103  (15.0%)   목표 15%
  Legendary     33   (4.8%)   목표  5%
```

**`Failed` 행이 0이 아니면 비율을 보기 전에 그것부터 고친다.** 반환 규약(01-1)에서 `false`는 전부 데이터 오류이고, 실패한 롤이 섞이면 비율도 같이 흔들린다.

> 등급 집계는 `ItemId → Definition → DT 행의 `Rarity``로 역산한다. 롤 함수는 등급을 모른다 — **하위 테이블 이름으로 세지 않는다.** 그러면 `LT_Rarity_*` 이름 규칙에 검증이 묶이고, 컨테이너·자판기가 다른 루트를 쓰기 시작하면 깨진다.

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | `NetCullDistanceSquared`에 비제곱값 | 픽업이 코앞에서만 보임 | 세터 + 제곱값 |
| 2 | 5.5 deprecated 필드 직접 대입 | 컴파일 경고 | `SetNetCullDistanceSquared()` / `SetNetUpdateFrequency()` |
| 3 | 순환 참조 방어 누락 | 에디터가 스택 오버플로로 죽음 | 깊이 상한 8 |
| 4 | 하위 테이블 `EmptyWeight` 적용 | 등급 비율이 기획표보다 낮게 나옴. 원인 파악 어려움 | `Depth > 0`이면 무시 + 경고 |
| 5 | `Super::` **뒤에서** `SpawnLoot()` | 플레이어와 아이템이 겹침, 시작 직후 빈 맵 | `Super::` 앞으로 |
| 6 | `ClearLoot()`이 월드 전체 순회 | 플레이어가 버린 아이템까지 삭제 | 자기 `SpawnedPickups` 약참조만 |
| 7 | `LootTable` AssetManager 미등록 | `RollTable`이 새 테이블을 못 찾음 | 01-1 등록 |
| 8 | 비동기 메시 로드에 일반 람다 | 로드 도착 전 픽업이 파괴되면 크래시 | `CreateWeakLambda` |
| 9 | 서버에서 시각 에셋 로드 | 데디서버 메모리 낭비 | `IsNetMode(NM_DedicatedServer)` 가드 |
| 10 | `State`를 `DOREPLIFETIME`에 등록 | 바닥 무기 잔탄이 전 클라에 노출 → 교전 흔적 추론 | 01-4. `UPROPERTY()`만, 복제 등록 없음 |
| 11 | `MakeItemState` 실패를 무시하고 픽업 스폰 | Definition 없는 아이템이 기본값으로 깔림. Step 04에서 아이콘 없이 나타나 원인이 멀어짐 | 실패 시 스폰 생략 + 에러 로그 |
| 12 | 메시 적용을 `OnRep_ItemId()`에만 둠 | **클라 창에는 보이는데 리슨서버 창에만 안 보인다.** 완료 조건 1 실패 | `ApplyVisual()`로 빼서 `InitPickup()`에서도 호출 (01-4) |
| 13 | `GetPrimaryAssetObject`만으로 테이블 해석 | 등록은 했는데 `RollTable`이 새 테이블을 못 찾음 (메모리에 없으므로) | `LoadPrimaryAsset` 먼저, **결과 포인터로** 판정 (01-5) |
| 14 | 클래스 컴파일 전에 `LT_*` 에셋 생성 | 옛 `PrimaryAssetType`이 `.uasset`에 구워져 영구 배제. 에디터에서만 되살아나 재현이 들쭉날쭉 | 컴파일 → 등록 → 재시작 → 에셋 생성 순서 + `final` (01-1, Step 00 함정 #1) |
| 15 | 픽업 콜리전을 손대지 않음 | **플레이어가 바닥 아이템에 걸려 올라탄다.** 접지 트레이스가 먼저 뿌린 픽업에 걸린다 | 생성자에서 `SetCollisionResponseToAllChannels(ECR_Ignore)` (01-4) |
| 16 | `InitPickup()`을 다음 프레임에 호출 | 클라가 `ItemId = NAME_None`을 받고 휴면 → **영원히 안 고쳐진다** | `SpawnActor`와 **같은 프레임** (01-2) |
| 17 | 빈 `ItemId` 엔트리를 `true + NAME_None`으로 반환 | "가끔 아이템이 안 나온다". `EmptyWeight`를 의심하며 엉뚱한 데를 판다 | `RollLootTable`이 직접 `false`로 거른다 (01-1) |
| 18 | `UPROPERTY(Config) TSubclassOf<AEPPickup>` | `.ini`에 BP 경로를 주면 **엔진 초기화 중 동기 로드**(`UObjectGlobals.cpp:4379` → `PropertyBaseObject.cpp:596`). 실패해도 `LOAD_NoWarn`이라 **조용히 null** — 미설정·오타·초기화 시점 문제가 전부 같은 증상 | `TSoftClassPtr` + 소비 지점에서 세 상태 구분 (01-5) |
| **19** | **`GetLifetimeReplicatedProps`에 `ItemId`를 등록하지 않음** | `ReplicatedUsing`만으로는 복제되지 않는다. **리슨서버 창에는 정상으로 보이고 클라 창만 빈 픽업** — 함정 #12와 증상이 정반대라 엉뚱한 곳을 판다 | `DOREPLIFETIME(AEPPickup, ItemId)` + `Net/UnrealNetwork.h` (01-4). 가르는 질문: *"서버 창에는 보이는가"* |
| 20 | `RollTable` 비율을 **전체 시행 수**로 나눠서 봄 | 루트 `EmptyWeight` 때문에 절대 50/30/15/5가 안 나온다 → 정상 동작을 확률 버그로 오인 | 빈 결과를 분모에서 빼고 찍는다. `Failed` 행을 따로 둔다 (01-5) |
| **21** | **에디터 전용 빌보드를 스포너의 `RootComponent`로 삼음** | `CreateEditorOnlyDefaultSubobject`가 `GIsEditor == false`면 nullptr(`UObjectGlobals.cpp:6039`) → `GetActorLocation()`이 `FVector::ZeroVector`(`Actor.h:4461`) → **모든 루트가 월드 원점에.** PIE에서는 정상이고 **Standalone/패키지에서만** 터진다 | `USceneComponent`를 루트로, 빌보드는 `SetupAttachment` (01-2, `TargetPoint.cpp:15-21`) |

---

## 착수 직전 — 헤더 정리

| 파일 | 필요한 것 |
|---|---|
| `EPLootTable.h` | `#include "Engine/DataAsset.h"`(`UPrimaryDataAsset`). 상단에 `class UEPLootTable;` **전방 선언** — `FEPLootEntry`가 먼저 선언되는데 `SubTable`이 그 타입을 쓴다 |
| `EPLootTable.cpp` | `#if WITH_EDITOR` 안에 `#include "Misc/DataValidation.h"` — `EPItemDefinition.cpp:8-9`와 같은 자리다 |
| `EPPickup.h` | `#include "Types/EPTypes.h"` — `FEPItemState`가 **값 멤버**라 완전 타입이 필요하다(전방 선언 불가). `struct FStreamableHandle;` / `class UStaticMeshComponent;` 전방 선언. `FLifetimeProperty`는 인라인 `class` 키워드로 쓰므로 include 불필요 (`EPWeapon.h:54` 관례) |
| `EPItemSpawner.h` | `class AEPPickup;`(`TWeakObjectPtr`에 필요) / `class UEPLootTable;` / `class UBillboardComponent;` — 빌보드 멤버는 `#if WITH_EDITORONLY_DATA`로 감싼다 |
| `EPPickup.cpp` | **`Net/UnrealNetwork.h`(`DOREPLIFETIME`)** / `Engine/AssetManager.h` / `Engine/StreamableManager.h` / `Data/EPLootDeveloperSettings.h` / `Data/EPItemDefinitionSubsystem.h` / `Data/EPItemDefinition.h` |
| `EPItemSpawner.cpp` | `EPPickup.h` / `EPLootTable.h` / `Data/EPItemDefinitionSubsystem.h` / `Data/EPLootDeveloperSettings.h` / `Engine/World.h` / `Components/BillboardComponent.h`(`WITH_EDITORONLY_DATA` 안) |
| `EPGameMode.cpp` | `EngineUtils.h`(`TActorIterator`) / `EPItemSpawner.h` |

`FEPItemState`가 값 멤버라 완전 타입이 필요한 것은 **Step 00에서 `TMap<FName, FEPItemData> DataCache`가 겪은 것과 같은 이유**다.

---

## 이 단계에서 하지 않는 것

- 줍기 / `bClaimed` 사용 → **Step 02** (필드는 미리 선언만)
- `IEPInteractable` 구현 → **Step 02**
- `EP_TraceChannel_Interact` 채널 생성 → **Step 02.** Step 01은 전 채널을 닫고, Step 02가 정확히 하나를 연다 (01-4)
- 버리기 경로에서 `InitPickup()` 호출 → **Step 03** (이번엔 스포너 경로만)
- `Cooldown` / `Payload` 열 → **Step 03.** 지금은 필드 자체가 없다
- **스폰 지점 겹침 방지 / 분산 품질 → 레벨 디자인.** 픽업이 서로의 콜리전을 막지 않으므로(01-4) 겹쳐도 **기능은 정상이다** — 겹쳐 보일 뿐이다. "완료 조건에 없으니 무시"가 아니라 "고장나지 않으므로 미룬다"
