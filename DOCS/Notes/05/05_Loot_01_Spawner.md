# Step 01 — Spawner (루트 테이블 + 스포너 + 픽업)

> 마스터 기획: `05_Loot_DOCS.md` (§4-2, §4-3, §4-4)
> 선행: `05_Loot_00_ItemCore.md` — `ItemId → Definition` 조회가 동작해야 한다

---

## 목표

맵에 스포너를 배치하면 매치 시작 시 확률대로 아이템이 바닥에 스폰되고, 서버·클라 양쪽에서 같게 보인다. **줍는 건 Step 02다.**

**완료 조건**

- [ ] 스포너를 맵에 놓고 PIE 2인 → 서버·클라에서 같은 아이템이 같은 위치에 보인다
- [ ] `EP.Loot.RollTable LT_Floor_Common 1000` → 등급 비율이 기획표(50/30/15/5)와 오차 범위 내
- [ ] 어떤 스포너도 참조하지 않는 테이블도 `RollTable`이 이름으로 찾는다
- [ ] `EP.Loot.Respawn` → 기존 픽업이 정리되고 새로 굴려진다 (`ClearLoot`이 자기 것만 지우는지는 버리기가 생기는 **Step 03에서 재확인**)
- [ ] `WorldMesh`가 없는 아이템(`AmmoBox_545` 등)도 플레이스홀더로 보인다
- [ ] 멀리 있는 픽업이 컬링되고, 가까이 가면 나타난다
- [ ] **클라이언트 패킷에 픽업의 `Charges`가 나가지 않는다** (`State`는 서버 전용 — 01-4). 검증: 클라 창에서 `EP.Loot.List` → `Charges`가 전부 `0`

---

## 01-1. `UEPLootTable` — 가중치 + 중첩

```cpp
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

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    { return FPrimaryAssetId(TEXT("LootTable"), GetFName()); }
};
```

`SubTable`은 **하드 참조(`TObjectPtr`)** 로 둔다 — 루트 테이블이 로드되면 등급 테이블이 따라 로드된다.

### 롤 함수 — 깊이 상한과 `EmptyWeight` 규칙

**수량 필드가 없다.** 스택이 없으므로 롤 결과는 **아이템 하나**다. "탄약 20~60발"은 수량이 아니라 **탄약상자 하나의 `Charges`** 이고, 그 초기값은 `Definition->InitState()`가 정한다 (Step 00). 루트 테이블이 아이템 타입별 상태를 알게 하지 않는다.

```cpp
// 반환: 뽑힌 ItemId. None이면 "빈 결과"
bool RollLootTable(const UEPLootTable* Table, FName& OutItemId, int32 Depth = 0)
{
    static constexpr int32 MaxDepth = 8;
    if (!Table || Depth > MaxDepth)
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
        return true;                       // 빈 결과 (OutItemId == None)

    for (const FEPLootEntry& E : Table->Entries)
    {
        if ((Pick -= E.Weight) >= 0.f) continue;

        if (E.SubTable) return RollLootTable(E.SubTable, OutItemId, Depth + 1);

        OutItemId = E.ItemId;
        return true;
    }
    return false;
}
```

**어디에 두는가:** `UEPLootTable`의 멤버로 두지 않는다 — `Depth`를 노출하는 공개 API가 되어 버린다. `EPLootTable.h`에 **정적 자유 함수**로 선언하고 스포너와 `EP.Loot.RollTable`이 함께 쓴다.

**반환 규약이 두 겹이다.** `false` = 테이블이 잘못됐다(널·순환·가중치 0) → 호출부가 에러 로그. `true` + `OutItemId == NAME_None` = **정상적인 빈 결과** → 조용히 스폰 생략. 스포너가 이 둘을 구분하지 않으면 데이터 오류가 "그냥 안 나온 것"에 묻힌다.

> `FMath::FRand()`는 **`[0, 1]` 닫힌 구간**이다(`GenericPlatformMath.h:635` — *"Returns a random float between 0 and 1, **inclusive**"*). `Pick == TotalWeight`가 나오면 루프가 끝까지 음수가 안 되어 마지막 `return false`에 도달한다. 확률은 2⁻²⁴ 수준이라 무시해도 되지만, **`false`가 "테이블이 잘못됐다"는 뜻이므로 그때 엉뚱한 에러 로그가 뜬다.** 마지막 줄을 `return false;` 대신 마지막 엔트리를 집어 주는 편이 안전하다.

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
└─ Scrap_Paper   Weight 1
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
| Has Blueprint Classes | `false` |
| bApplyRecursively | `true` |

> **★ `Is Editor Only`를 반드시 끈다.** 기존 `DefaultGame.ini`의 `Map` / `PrimaryAssetLabel` 두 항목이 **둘 다 `bIsEditorOnly=True`** 라, 그 줄을 보고 따라 하면 **에디터에서는 되는데 패키지 빌드에서만** 루트 테이블 리스트가 빈다. Step 00의 `ItemDef` 등록도 같은 함정이다.

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
    // 자기가 뿌린 것만 정리하기 위한 약참조
    TArray<TWeakObjectPtr<AEPPickup>> SpawnedPickups;
};
```

- 생성자에서 `bReplicates = false`, `PrimaryActorTick.bCanEverTick = false`. **스포너는 복제하지 않는다** — 픽업 액터가 복제되므로 클라 동기화는 자동으로 따라온다
- 런타임에 보이지 않는다. 에디터 빌보드(`UBillboardComponent`)만 둔다
- `bAlignToGround`면 스폰 지점에서 아래로 짧은 라인 트레이스를 쏴 접지시킨다. 레벨 디자이너가 높이를 정밀하게 안 맞춰도 되게 하기 위함

**★ 약참조로 들고 있는 이유:** `ClearLoot()`이 자기가 뿌린 것만 지워야 한다. 월드의 모든 `AEPPickup`을 순회해 지우면 **플레이어가 버린 아이템까지 사라진다**(Step 03). 그리고 이미 주워진 픽업은 파괴됐으므로 강참조로 들면 GC를 막는다.

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

private:
    bool bClaimed = false;      // 서버 전용, 복제 안 함 (Step 02)
    TSharedPtr<FStreamableHandle> MeshHandle;
};
```

**스포너가 뿌린 것도 플레이어가 버린 것도 같은 진입점을 쓴다.**

```cpp
// 스포너 (01-2)
FEPItemState NewState;
if (Defs->MakeItemState(RolledItemId, NewState))
    Pickup->InitPickup(RolledItemId, NewState);

// 버리기 (Step 03)
Pickup->InitPickup(Entry.ItemId, Entry.State);      // ← 값 복사. 잔탄이 여기서 보존된다
```

핸들 유무로 갈리던 두 경로가 하나가 된다. **버린 무기의 잔탄이 보존되는 이유는 규칙을 지켜서가 아니라 값을 복사했기 때문**이므로, 이관 프로토콜도 `EndPlay` 정리도 필요 없다.

### ★ `State`를 복제하지 않는 이유는 비용이 아니라 정보 은폐다

`FEPItemState`는 8바이트라 대역폭은 이유가 못 된다. 문제는 **바닥 무기의 잔탄이 복제되면 치트 클라이언트가 릴러번시 범위 내 모든 픽업을 읽어 "어디서 얼마 전에 교전이 있었는지"를 추론한다**는 것이다. `12/30`짜리 라이플이 바닥에 있다 = **여기서 누가 죽었다.** GAME.md가 두 번 명시한 정보 은폐 기둥을 사고로 뒤집는다.

`UPROPERTY()`를 붙이되 `GetLifetimeReplicatedProps`에 등록하지 않는다 — 직렬화·GC 대상이면서 복제는 안 되는 상태다.

> **★ Step 03에서 이 필드가 `TArray<FEPInventoryEntry> Payload`로 교체된다** (추가가 아니라 **교체**). 배낭을 버리면 안의 아이템이 같이 나가야 하고(GAME.md), 나중에 부착물 달린 총도 마찬가지다(§7-3). `InitPickup`의 시그니처도 같이 바뀌고, 스포너 경로는 **원소 1개짜리 배열**을 넘기게 된다. **`AEPPickup`에서 모양이 바뀌는 유일한 곳**이다.
>
> **지금 배열로 만들지 않는다.** 스포너가 뿌리는 것은 언제나 아이템 하나이고, 원소가 항상 1개인 배열은 읽는 쪽에 군더더기만 남긴다. 컨테이너를 버리는 경로가 실제로 생기는 Step 03에서 확장한다 — 그때 이 절을 함께 고친다.

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
}
```

> **★ 5.5부터 필드 직접 대입은 deprecated다.** `NetCullDistanceSquared`(`Actor.h:869`)와 `NetUpdateFrequency`(`Actor.h:874`)에 `UE_DEPRECATED(5.5, ...)`가 붙어 있어 `NetCullDistanceSquared = ...`로 쓰면 5.7에서 경고가 난다. **세터를 쓴다.** 반면 `bReplicates`(`Actor.h:556`) / `bAlwaysRelevant`(`Actor.h:300`) / `NetDormancy`(`Actor.h:832`)는 deprecated가 아니므로 생성자 대입 그대로 둔다.

> **★ 단위 함정:** `NetCullDistanceSquared`의 기본값은 `225000000`(= 15000cm의 제곱)이다. 여기에 `5000`을 그대로 넣으면 컬링 거리가 √5000 ≈ **70cm**가 되어 픽업이 코앞에서만 보인다. "왜 아이템이 안 보이지"로 한참 헤매는 대표적인 실수다.

### Dormancy 규칙

```
스폰       → DORM_Initial (초기 1회 복제 후 휴면)
획득 완료  → Destroy()    ← 파괴는 휴면과 무관하게 전달된다
```

**복제되는 값이 `ItemId` 하나뿐이고 그것은 스폰 시점에 정해져 바뀌지 않는다.** 스택이 있던 설계에서는 부분 획득 시 `Quantity`를 낮추고 `FlushNetDormancy()`를 불러야 했고, 빠뜨리면 "서버는 정상인데 클라 화면의 개수만 옛날 값"이라는 재현 까다로운 버그가 났다. **그 호출도 그 함정도 이제 없다.**

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

### `EndPlay` 오버라이드가 필요 없다

이전 설계는 여기서 `InstanceSubsystem->Destroy(Handle)`을 불러야 했고, **획득 시 핸들을 비우는 순서를 지키지 않으면 방금 인벤토리로 넘긴 인스턴스를 지워 잔탄이 사라졌다.** `State`가 값이므로 픽업이 파괴되면 그냥 같이 사라진다 — 지울 대상도, 지키는 순서도 없다.

---

## 01-5. 디버그 도구

```cpp
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "EP Loot"))
class EMPLOYMENTPROJ_API UEPLootDeveloperSettings : public UDeveloperSettings   // Step 00에서 생성, 여기서 확장
{
    UPROPERTY(Config, EditAnywhere, Category = "Data")
    TSoftObjectPtr<UDataTable> ItemDataTable;

    UPROPERTY(Config, EditAnywhere, Category = "Debug")
    bool bEnableLootDebugLog = false;

    UPROPERTY(Config, EditAnywhere, Category = "Debug")
    bool bEnableSpawnerDebugDraw = false;

    // ★ Debug가 아니다. WorldMesh가 하나도 없는 지금은 유일한 표시 수단이다 (01-4)
    UPROPERTY(Config, EditAnywhere, Category = "Visual")
    TSoftObjectPtr<UStaticMesh> PlaceholderPickupMesh;
};
```

> 엔진 기본 에셋 `/Engine/BasicShapes/Cube.Cube`를 그대로 넣으면 된다. 별도로 만들 필요 없다.

| 커맨드 | 용도 |
|---|---|
| `EP.Loot.RollTable <이름> <횟수>` | N회 굴려 아이템별·등급별 집계 출력 |
| `EP.Loot.Respawn` | 모든 스포너 `ClearLoot()` 후 `SpawnLoot()` (서버 전용) |
| `EP.Loot.List` | 월드의 모든 `AEPPickup` (서버·클라 양쪽) |

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
  Idx  ItemId          Location            Charges  Claimed  Cooldown  Payload
  0    Bandage         (1200, 340, 92)     1        false    -         1
  1    AK74_HitScan    (880, -20, 90)      12       false    0.31      1      ← 방금 버린 것

> EP.Loot.List                                    (클라 창)
  Idx  ItemId          Location            Charges  Claimed  Cooldown  Payload
  0    Bandage         (1200, 340, 92)     0        false    -         0
  1    AK74_HitScan    (880, -20, 90)      0        false    -         0     ← 전부 0이어야 한다
```

**★ 클라에서도 돌게 만드는 것이 완료 조건 7번의 검증 수단이다.** "`Charges`가 안 나간다"는 눈으로 확인할 방법이 달리 없다. 클라 창에서 잔탄 12짜리 라이플이 `0`으로 찍히면 그게 증명이다 — **거꾸로 `12`가 찍히면 `DOREPLIFETIME`에 `State`를 넣은 것이다**(함정 #10).

> `Claimed` / `Cooldown` / `Payload`도 서버 전용이라 클라에서는 같이 기본값으로 찍힌다. **이상한 게 아니라 같은 이유의 같은 결과다.** 클라 출력은 "복제되는 것은 `ItemId`와 위치뿐"임을 통째로 보여준다.

**`EP.Loot.List`가 Step 03의 검증 수단이기도 하다.** 이 문서의 완료 조건 3번이 *"`ClearLoot`이 자기 것만 지우는지는 **Step 03에서 재확인**"* 이라고 적었는데, 확인할 수단이 없으면 그 줄이 공수표다.

| 열 | 증명하는 것 |
|---|---|
| `Charges` (클라) | **잔탄이 복제되지 않는가** — 완료 조건 7 |
| `Cooldown` | Step 03의 "버린 직후 0.5초 회색" |
| `Payload` | **배낭 안의 것이 같이 나갔는가** (Step 03 서브트리) |
| `Idx` 목록 | `ClearLoot`이 플레이어가 버린 것을 안 지웠는가 |

> 픽업 도구를 Step 03에 두지 않는 이유는 **두 문서로 갈리지 않게** 하기 위함이다. 인벤토리는 `EP.Inv.*`, 월드 픽업은 `EP.Loot.*`로 나눈다.

**`RollTable`이 이 단계의 핵심 검증 수단이다.** 확률은 눈으로 못 믿는다 — 1000회 굴려 등급 비율이 50/30/15/5에 수렴하는지 확인해야 중첩 롤과 `EmptyWeight` 규칙이 맞게 구현됐음을 안다.

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
| 14 | 클래스 컴파일 전에 `LT_*` 에셋 생성 | 옛 `PrimaryAssetType`이 `.uasset`에 구워져 영구 배제. 에디터에서만 되살아나 재현이 들쭉날쭉 | 컴파일 → 등록 → 재시작 → 에셋 생성 순서 (01-1, Step 00 함정 #1) |

---

## 이 단계에서 하지 않는 것

- 줍기 / `bClaimed` 사용 → **Step 02** (필드는 미리 선언만)
- `IEPInteractable` 구현 → **Step 02**
- 버리기 경로에서 `InitPickup()` 호출 → **Step 03** (이번엔 스포너 경로만)
