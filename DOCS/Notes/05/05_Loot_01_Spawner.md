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
- [ ] `WorldMesh`가 없는 아이템(`Ammo_762` 등)도 플레이스홀더로 보인다
- [ ] 멀리 있는 픽업이 컬링되고, 가까이 가면 나타난다

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

    UPROPERTY(EditAnywhere, meta = (ClampMin = "1", EditCondition = "SubTable == nullptr"))
    int32 MinQuantity = 1;

    UPROPERTY(EditAnywhere, meta = (ClampMin = "1", EditCondition = "SubTable == nullptr"))
    int32 MaxQuantity = 1;
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

```cpp
// 반환: 뽑힌 아이템. ItemId가 None이면 "빈 결과"
struct FEPLootRollResult { FName ItemId; int32 Quantity = 0; };

bool RollLootTable(const UEPLootTable* Table, FEPLootRollResult& Out, int32 Depth = 0)
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
        return true;                       // 빈 결과 (Out.ItemId == None)

    for (const FEPLootEntry& E : Table->Entries)
    {
        if ((Pick -= E.Weight) >= 0.f) continue;

        if (E.SubTable) return RollLootTable(E.SubTable, Out, Depth + 1);

        Out.ItemId   = E.ItemId;
        Out.Quantity = FMath::RandRange(E.MinQuantity, E.MaxQuantity);
        return true;
    }
    return false;
}
```

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
├─ Ammo_762      Weight 1, Qty 20~60
├─ Bandage       Weight 1, Qty 1~2
└─ Scrap_Paper   Weight 1, Qty 1~3
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

> 실사용만 보면 스포너가 하드 포인터로 들고 있어 불필요해 보이지만, **`EP.Loot.RollTable <이름>`이 이름으로 찾는다.** 아직 어떤 스포너도 참조하지 않는 새 테이블 — 정확히 검증하고 싶은 그 테이블 — 이 메모리에 없어서 커맨드가 못 찾는다.

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

    void InitFromSpawn(FName InItemId, int32 InQuantity);         // 서버, 스포너가 호출
    void InitFromDrop(FName InItemId, int32 InQuantity, int32 InHandle);  // 서버, Step 03

protected:
    UPROPERTY(ReplicatedUsing = OnRep_ItemId)
    FName ItemId;

    UPROPERTY(Replicated)
    int32 Quantity = 0;

    // ★ 서버 전용. 복제하지 않는다. 포인터가 아니라 핸들 (§4-1)
    int32 InstanceHandle = INDEX_NONE;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;

    UFUNCTION()
    void OnRep_ItemId();

    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    bool bClaimed = false;      // 서버 전용, 복제 안 함 (Step 02)
    TSharedPtr<FStreamableHandle> MeshHandle;
};
```

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
스폰            → DORM_Initial (초기 1회 복제 후 휴면)
Quantity 변경   → FlushNetDormancy()   ← 부분 획득 시 필수 (Step 03)
획득 완료       → Destroy()            ← 파괴는 휴면과 무관하게 전달된다
```

> `FlushNetDormancy()`를 빠뜨리면 **클라이언트 화면의 개수만 옛날 값으로 남는다.** 서버는 정상이라 재현이 까다로운 종류다. 이번 단계에는 `Quantity` 변경 경로가 없지만, Step 03에서 반드시 넣는다.

### 메시 적용 — 플레이스홀더가 필수다

```cpp
void AEPPickup::OnRep_ItemId()
{
    if (IsNetMode(NM_DedicatedServer)) return;      // 서버는 시각 에셋을 로드하지 않는다

    // 플레이스홀더는 설정에서 온다. 이것도 TSoftObjectPtr이므로 최초 1회 동기 로드해 캐시한다
    const UEPLootDeveloperSettings* Settings = GetDefault<UEPLootDeveloperSettings>();
    if (UStaticMesh* Placeholder = Settings->PlaceholderPickupMesh.LoadSynchronous())
        Mesh->SetStaticMesh(Placeholder);           // 먼저 플레이스홀더

    UEPItemDefinition* Def = /* DefinitionSubsystem->FindDefinition(ItemId) */;
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

**현재 `WorldMesh`가 있는 아이템은 하나도 없다.** 무기조차 `WeaponMesh`(스켈레탈)만 있고 `WorldMesh`는 비어 있다. 플레이스홀더 박스는 선택이 아니라 **이번 단계의 유일한 표시 수단**이다.

> `CreateWeakLambda`를 쓴다. 로드가 도착하기 전에 픽업이 파괴되면(다른 플레이어가 먼저 주움) 일반 람다는 죽은 객체를 건드린다.

### `EndPlay` — 인스턴스 정리

```cpp
void AEPPickup::EndPlay(const EEndPlayReason::Type Reason)
{
    if (HasAuthority() && InstanceHandle != INDEX_NONE)
    {
        /* InstanceSubsystem->Destroy(InstanceHandle); */
        InstanceHandle = INDEX_NONE;
    }
    Super::EndPlay(Reason);
}
```

> **★ 이관 프로토콜을 지켜야 한다** (§4-1). 획득 시 `InstanceHandle`을 `INDEX_NONE`으로 **먼저** 비운 뒤 `Destroy()`한다. 순서를 뒤집으면 `EndPlay`가 방금 인벤토리로 넘긴 인스턴스를 지운다 — **획득 직후 무기 잔탄이 사라지는** 형태로 나타난다. 이번 단계에는 획득 경로가 없지만 Step 02/03에서 바로 걸린다.

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

    UPROPERTY(Config, EditAnywhere, Category = "Debug")
    TSoftObjectPtr<UStaticMesh> PlaceholderPickupMesh;
};
```

| 커맨드 | 용도 |
|---|---|
| `EP.Loot.RollTable <이름> <횟수>` | N회 굴려 아이템별·등급별 집계 출력. 이름 해석은 `UAssetManager::GetPrimaryAssetObject` |
| `EP.Loot.Respawn` | 모든 스포너 `ClearLoot()` 후 `SpawnLoot()` (서버 전용) |

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

---

## 이 단계에서 하지 않는 것

- 줍기 / `bClaimed` 사용 → **Step 02** (필드는 미리 선언만)
- `IEPInteractable` 구현 → **Step 02**
- `Quantity` 변경 + `FlushNetDormancy()` → **Step 03** (부분 획득이 생길 때)
- `InitFromDrop()` 실제 호출 → **Step 03**
