# Step 00 — ItemCore (아이템 계층 정비)

> 마스터 기획: `05_Loot_DOCS.md` (§4-1, §4-9)
> 진행 상황: `LOOT_STATUS.md`

---

## 목표

`ItemId → Definition → FEPItemState` 경로를 **처음으로 실제로 돌게 만든다.** 게임플레이 기능은 하나도 추가하지 않는다. 눈에 보이는 결과가 없는 대신 콘솔 커맨드 하나로 독립 검증된다.

**완료 조건**

- [ ] `EP.Item.State Weapon_AK74_HitScan` → `Charges=30 Durability=100` (무기 오버라이드 경로)
- [ ] `EP.Item.State AmmoBox_545` → `Charges=100` (기본 클래스 + `InitialCharges` 경로)
- [ ] `EP.Item.State Resume` → `Charges=0` (기본값 그대로)
- [ ] `EP.Item.Dump` → `DataCache=9  Definitions=9`
- [ ] 에디터에서 DT ↔ Definition 참조를 일부러 어긋나게 하면 `IsDataValid()`가 잡아냄
- [ ] **데디케이티드 서버로 실행해도 `Definitions=9`** — Definition 상주가 넷모드와 무관함을 확인
- [ ] `UEPItemInstance` / `UEPWeaponInstance` 파일이 프로젝트에서 사라졌고 빌드가 통과한다

> **★ 개체 상태가 `UObject`가 아니라 `USTRUCT`다.** 이 결정의 근거와 검증 기록은 `05_Loot_DOCS.md` §4-1 / `05_Loot_REVIEW_StructMigration.md`. **아이템은 스택되지 않으며**, 인벤토리 용량은 칸 수 합산이다 (§4-6).

---

## 00-0. ★ 선행 발견 — PrimaryAssetType이 두 개로 갈려 있다

```cpp
// EPItemDefinition.cpp:8
return FPrimaryAssetId(TEXT("ItemDef"),   GetFName());
// EPWeaponDefinition.cpp:10
return FPrimaryAssetId(TEXT("WeaponDef"), GetFName());   // ← 서브클래스가 다른 타입
```

§4-1의 "AssetManager로 일괄 로드"는 **타입이 하나일 때만 성립한다.** 지금 상태로 `ItemDef`만 등록하면 무기 Definition이 로드되지 않고, 둘 다 등록하면 조회할 때마다 "이 아이템은 어느 타입이지"를 따져야 한다 — §4-9가 없애려던 타입 분기가 로딩 계층에서 부활한다.

**조치: `UEPWeaponDefinition::GetPrimaryAssetId()` 오버라이드를 제거한다.** 상속으로 `ItemDef`를 그대로 쓴다. 방어구·소모품 Definition을 추가해도 같은 규칙이 적용된다.

> 마스터 문서가 타입명을 `EPItemDefinition`으로 적은 곳이 있으나, 실제 코드값은 `ItemDef`다. **`ItemDef`로 통일한다** — 이미 코드에 있는 쪽을 남기는 게 싸다.

---

## 00-1. `FEPItemState` — 개체 상태는 값 타입이다

```cpp
// EPTypes.h (또는 EPItemData.h 옆)
USTRUCT(BlueprintType)
struct FEPItemState
{
    GENERATED_BODY()

    // 이 개체가 담고 있는 소모 단위
    //   무기      : 장전된 발수        탄약상자 : 남은 발수
    //   현금뭉치  : 금액               소모품   : 남은 사용 횟수
    UPROPERTY(BlueprintReadWrite, Category = "Item")
    int32 Charges = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Item")
    float Durability = 100.f;
};
```

`Outer`도, 핸들도, 소유 서브시스템도, 수명 관리도 없다. 인벤토리 엔트리(Step 03)와 픽업(Step 01)에 **값으로 내장**되고, 이관은 대입 한 줄이다.

> **필드 이름이 `Ammo`가 아닌 이유:** 같은 값을 탄약상자·현금뭉치·소모품이 공유한다. 특히 **스택이 없으므로 돈을 인벤토리 아이템으로 두려면 현금뭉치 하나가 금액을 들고 있어야** 하고(안 그러면 10,000원이 엔트리 10,000개가 된다), 그 필드가 `Ammo`면 안 된다.

---

## 00-2. `UEPItemDefinition` — 상태 초기화와 검증

```cpp
// EPItemDefinition.h
UCLASS()
class EMPLOYMENTPROJ_API UEPItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    FName ItemId;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    FDataTableRowHandle ItemDataRow;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visual")
    TSoftObjectPtr<UStaticMesh> WorldMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visual")
    TSoftObjectPtr<UTexture2D> Icon;

    // 사용 시 발동할 어빌리티 (소모품용 — 이번 단계에서는 채우지 않는다)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|GAS")
    TSubclassOf<UGameplayAbility> GrantedAbility;

    // ★ 서브클래스가 오버라이드하는 유일한 지점
    virtual void InitState(const FEPItemData& Data, FEPItemState& State) const;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
```

```cpp
// EPItemDefinition.cpp
void UEPItemDefinition::InitState(const FEPItemData& Data, FEPItemState& State) const
{
    State.Charges = Data.InitialCharges;      // ★ DT에서 온다
}
```

**호출부는 아이템 타입을 전혀 모른다.**

```cpp
FEPItemState NewState;
Def->InitState(*Row, NewState);        // ← 이 한 줄이 전부
```

`NewObject`도, `Outer` 결정도, 핸들 발급도, 실패 경로도 없다. 새 아이템 종류를 추가하는 비용은 **Definition 서브클래스 하나**이고 기존 코드 수정은 0이다.

### ★ 필드를 DT에 둘지 DA에 둘지 — 원칙 두 줄

> **① 여러 아이템을 표로 나란히 놓고 조정하는 값은 `FEPItemData`(DataTable).**
> **② 그 아이템 한 종류에만 의미 있는 것 — 에셋 참조, `virtual` 동작, 타입 전용 필드 — 은 `UEPItemDefinition`(DataAsset).**

판정선은 **"모든 아이템이 값을 갖는가"** 다.

| 필드 | 위치 | 근거 |
|---|---|---|
| `SlotSize` / `SellPrice` / `Rarity` / `MaxStack` / `bFungible` | **DT** | 전형적인 밸런싱 열 |
| **`InitialCharges`** | **DT** | 탄약상자 100 / 현금뭉치 10000 / 붕대 1 — 표로 조정한다 |
| **`ContainerCapacity`** | **DT** | 소형 12 / 중형 20 / 대형 30 — 동상 |
| `WorldMesh` / `Icon` / `GrantedAbility` | DA | 에셋 참조 |
| `InitState()` | DA | `virtual` |
| `MaxAmmo` | DA (Weapon) | **무기 전용.** DT에 넣으면 나머지 행이 전부 빈칸이 된다 |

`InitialCharges`/`ContainerCapacity`는 **전부 값을 갖는다**(대부분 0). `MaxAmmo`는 무기만 갖는다. 그게 판정선이고, 이 배치면 원칙에 예외가 없다.

> **비용은 인자 하나뿐이다.** `InitState` 호출부는 `MakeItemState` 하나이고(00-6) 거기서 Row와 Definition을 이미 둘 다 들고 있다.

> **두 계층을 유지하는 이유:** 합치면 양방향 참조 동기화·`IsDataValid` 오버라이드·캐시 2개·`FindData` null 무증상 버그가 전부 사라진다. 그럼에도 유지하는 것은 **아이템이 수십 종이 되면 밸런싱 표(CSV·일괄 수정·diff)가 확실히 낫기 때문**이다. DataAsset은 그중 아무것도 안 된다.

### `IsDataValid` — 양방향 참조 검증

`FEPItemData::ItemDefinition`(소프트 참조)과 `UEPItemDefinition::ItemDataRow`가 서로를 가리키므로 손으로 동기화해야 한다. 어긋나도 컴파일·로드는 통과한다.

```cpp
// EPItemDefinition.cpp
#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UEPItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    if (ItemId.IsNone())
    {
        Context.AddError(NSLOCTEXT("EP", "ItemIdEmpty", "ItemId가 비어 있습니다."));
        Result = EDataValidationResult::Invalid;
    }

    if (ItemDataRow.RowName != ItemId)
    {
        Context.AddError(FText::Format(
            NSLOCTEXT("EP", "RowNameMismatch", "ItemDataRow.RowName({0}) != ItemId({1})"),
            FText::FromName(ItemDataRow.RowName), FText::FromName(ItemId)));
        Result = EDataValidationResult::Invalid;
    }

    if (const FEPItemData* Row = ItemDataRow.GetRow<FEPItemData>(TEXT("IsDataValid")))
    {
        if (Row->ItemDefinition.ToSoftObjectPath() != FSoftObjectPath(this))
        {
            Context.AddError(NSLOCTEXT("EP", "BackRefMismatch",
                "DataTable Row의 ItemDefinition이 이 에셋을 가리키지 않습니다."));
            Result = EDataValidationResult::Invalid;
        }
    }
    return Result;
}
#endif
```

> UE 5.3부터 `IsDataValid(TArray<FText>&)`와 non-const 버전은 deprecated다 (`Object.h:1100,1110`). **`IsDataValid(FDataValidationContext&) const`** 를 쓴다. `#include "Misc/DataValidation.h"` 필요.

---

## 00-3. `UEPWeaponDefinition` — 3가지 변경

```cpp
// 1) GetPrimaryAssetId() 오버라이드 제거 (선언·정의 둘 다)      ← 00-0

// 2) MaxAmmo 타입 변경
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
int32 MaxAmmo = 30;        // was: uint8

// 3) 상태 초기화 오버라이드
virtual void InitState(const FEPItemData& Data, FEPItemState& State) const override
{
    State.Charges = MaxAmmo;       // 새로 만든 무기는 만탄. Data.InitialCharges를 안 쓴다
}
```

`InitialCharges`가 아니라 `MaxAmmo`를 쓴다 — 무기는 이미 그 값을 갖고 있으므로 두 곳에 적게 하지 않는다.

**`MaxAmmo`를 `int32`로 바꾸는 이유:** 지금 `uint8`(Definition) → `int32`(`Charges`) → `float`(GAS 어트리뷰트) 3중 캐스팅이 낀다. Step 05의 주입/write-back 경로가 이 위를 왕복하므로 최소한 정수 쪽은 통일한다. 폭 확대라 기존 DataAsset 값은 손실 없이 유지된다.

> `EPCombatComponent.cpp:177-178`의 `static_cast<float>(NewWeapon->WeaponDef->MaxAmmo)`는 `int32`로 바뀌어도 그대로 컴파일된다. 이 줄의 **제거는 Step 05**다 — 지금 건드리지 않는다.

---

## 00-4. ★ `UEPItemInstance` / `UEPWeaponInstance` — 파일째 삭제한다

```
Public/Data/EPItemInstance.h      Private/Data/EPItemInstance.cpp
Public/Data/EPWeaponInstance.h    Private/Data/EPWeaponInstance.cpp
```

**호출처가 0이므로 삭제 비용이 없다.** 이 클래스들이 담고 있던 것과 그 행선지:

| 기존 | 행선지 |
|---|---|
| `UEPWeaponInstance::CurrentAmmo` | `FEPItemState::Charges` |
| `UEPWeaponInstance::Durability` | `FEPItemState::Durability` |
| `UEPItemInstance::ItemId` | `FEPInventoryEntry::ItemId` / `AEPPickup::ItemId` |
| `UEPItemInstance::CachedDefinition` | `UEPItemDefinitionSubsystem::FindDefinition(ItemId)` — O(1) 조회라 캐시할 이유가 없다 |
| `UEPItemInstance::Quantity` | **없어진다.** 스택이 없다 (§4-1) |
| `UEPItemInstance::InstanceId` (FGuid) | **없어진다.** 읽는 코드가 없고, DB 영구 식별자는 저장 시점에 발급한다 |
| `UEPItemInstance::SchemaVersion` | **없어진다.** 아이템이 아니라 **세이브 포맷의 속성**이다 — `USaveGame`/DB 행 봉투에 하나만 둔다 |
| `UEPItemInstance::IsSupportedForNetworking()` | **없어진다.** 복제할 UObject 자체가 없다 |
| `static CreateInstance()` / `CreateWeaponInstance()` | Definition의 `InitState()` (00-2) |

> **`CachedDefinition`이 사라지는 것이 이 전환의 축소판이다.** 그 필드는 "인스턴스가 UObject라서 뭔가를 들고 있어야 한다"는 사실 때문에 존재했지, 성능이나 정합성 때문이 아니었다. 조회를 서브시스템 한 곳으로 모으면 각 개체가 참조를 들 이유가 없어진다.

> **`UEPItemInstanceSubsystem`은 만들지 않는다.** 이전 설계에서 이 서브시스템의 유일한 존재 이유는 인스턴스의 `Outer` 문제를 푸는 것이었다 — 소유할 UObject가 없으면 소유자도 필요 없다.

---

## 00-5. `UEPItemDefinitionSubsystem` (GameInstance)

```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPItemDefinitionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // 반환 포인터는 다음 BuildDataCache()까지만 유효하다 — 장기 보관 금지 (아래 참조)
    const FEPItemData* FindData(FName ItemId) const;
    UEPItemDefinition* FindDefinition(FName ItemId) const;

private:
    void BuildDataCache();
    void LoadAllDefinitions();          // 블로킹 — Initialize() 안에서 완료된다
    void BuildDefinitionCache();

    TMap<FName, FEPItemData> DataCache;                        // ★ 포인터가 아니라 값

    UPROPERTY()
    TMap<FName, TObjectPtr<UEPItemDefinition>> DefinitionCache;

    TSharedPtr<FStreamableHandle> DefinitionHandle;            // 상주 유지용 (놓으면 언로드된다)
};
```

### ★ 행 포인터를 캐시하지 않는다

`UDataTable::FindRow()`가 돌려주는 `FEPItemData*`는 DataTable 내부 `RowMap`의 메모리를 가리킨다. 에디터에서 DT를 리임포트하거나 핫리로드하면 `RowMap`이 비워지고 재할당되어 **캐시한 포인터가 댕글링**한다. 패키지 빌드에서는 재현되지 않고 에디터에서만 나는 종류라 원인 추적이 오래 걸린다.

`FEPItemData`는 작은 POD이므로 **값으로 복사해 담는다.**

```cpp
void UEPItemDefinitionSubsystem::BuildDataCache()
{
    const UEPLootDeveloperSettings* Settings = GetDefault<UEPLootDeveloperSettings>();
    UDataTable* Table = Settings->ItemDataTable.LoadSynchronous();
    if (!Table) { UE_LOG(LogTemp, Error, TEXT("ItemDataTable이 설정되지 않았습니다.")); return; }

    DataCache.Reset();
    Table->ForeachRow<FEPItemData>(TEXT("BuildDataCache"),
        [this](const FName& RowName, const FEPItemData& Row)
        {
            if (Row.ItemId != RowName)
                UE_LOG(LogTemp, Warning,
                    TEXT("[ItemRegistry] RowName(%s) != ItemId(%s)"),
                    *RowName.ToString(), *Row.ItemId.ToString());

            DataCache.Add(RowName, Row);       // 값 복사
        });
}
```

> `RowName != ItemId` 경고는 `IsDataValid`와 중복처럼 보이지만 목적이 다르다. `IsDataValid`는 **에디터에서 저장할 때** 잡고, 이쪽은 **패키지 런타임에서** 잡는다. 후자가 없으면 쿠킹된 빌드에서 조용히 null이 된다.

> **★ `FindData()`가 돌려주는 포인터도 영구적이지 않다.** `DataCache`의 값 포인터는 `TMap`에 원소가 추가되면 무효화된다. 빌드가 끝난 뒤에는 추가가 없어 실사용상 안정적이지만, **`BuildDataCache()`가 다시 돌면(DT 핫리로드) 앞서 나눠준 포인터는 전부 댕글링**한다 — 행 포인터 캐시를 금지한 것과 정확히 같은 실패다. 호출부는 **그 자리에서 읽고 버린다.** 멤버에 담지 않는다.

### Definition은 전량 상주시킨다

```cpp
void UEPItemDefinitionSubsystem::LoadAllDefinitions()
{
    UAssetManager& Manager = UAssetManager::Get();

    // GetPrimaryAssetIdList + LoadPrimaryAssets 2단계가 필요 없다 (AssetManager.h:322)
    DefinitionHandle = Manager.LoadPrimaryAssetsWithType(FPrimaryAssetType(TEXT("ItemDef")));

    if (!DefinitionHandle.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("[ItemRegistry] ItemDef 프라이머리 애셋이 하나도 없습니다. "
                 "Project Settings > Asset Manager 등록을 확인하십시오. (00-7)"));
        return;
    }

    DefinitionHandle->WaitUntilComplete();   // ★ 여기서 막는다
    BuildDefinitionCache();
}
```

### ★ 로드는 **블로킹**이다

`LoadPrimaryAssets` 계열은 기본이 비동기라, 델리게이트만 걸어두면 `Initialize()`는 즉시 반환하고 **완료 전까지 `FindDefinition()`이 계속 nullptr**을 돌려준다. 그 상태로는 아래 "동기 경로" 논거가 코드로 보장되지 않는다 — "언제부터 안전한가"라는 지점이 문서에도 코드에도 없게 된다.

`Initialize()`는 `UGameInstance::Init()` 안, 즉 **로딩 화면 시간**이다. 여기서 막는 비용은 게임플레이에 드러나지 않는다.

- `WaitUntilComplete()`가 끝난 시점 = **불변식이 성립하는 시점**이다. 이후 어떤 경로도 Definition 부재를 걱정하지 않는다
- `DefinitionHandle`을 멤버로 유지하는 이유는 **상주 보장**이다. 핸들을 놓으면 참조가 사라져 언로드 대상이 된다. `Deinitialize()`에서 `Reset()`한다
- 비동기를 유지하려면 "로드 완료 전에는 매치를 시작하지 않는다"는 게이트를 어딘가에 만들어야 하는데, Step 00 범위에서는 블로킹이 훨씬 싸다

`BuildDefinitionCache()`는 로드된 에셋을 `ItemId → Definition`으로 담는다. 로드된 에셋의 `ItemId` 필드를 키로 쓰되, `DataCache`에 없는 `ItemId`는 경고를 남긴다. **`DefinitionCache`가 비었으면 에러 로그를 남긴다** — 조용히 0개로 진행하는 것이 함정 #2의 증상이다.

**왜 소프트 참조로 지연 로드하지 않는가:** 픽업 획득은 RPC 응답 안에서 성패가 결정돼야 하는 **동기 경로**인데, `Definition->InitState()`도 `SlotSize` 조회를 통한 칸 여유 판정(§4-6)도 Definition이 메모리에 있어야 한다. 로드를 기다리는 사이 "줍기 성공/실패"를 유보할 수 없다. 반면 `WorldMesh`/`Icon`은 지연이 허용되므로 소프트로 남긴다 (§4-1).

Definition은 수치·참조만 담은 메타데이터라 개당 수 KB다. 수백 개여도 상주 비용이 문제되지 않는다.

> **데디케이티드 서버:** Definition은 로드하되 `WorldMesh`/`Icon`/`WeaponMesh`는 건드리지 않는다. 이 분리가 성립하는 이유가 위 정책이다. 서버에서 시각 에셋을 로드하는 코드는 `AEPPickup`(Step 01)에만 들어가고 거기서 `IsNetMode(NM_DedicatedServer)` 가드를 건다.

---

## 00-6. 아이템 생성 헬퍼 — 서브시스템이 아니라 함수 하나

새 아이템을 만드는 경로(스포너, 기본 지급, 자판기)가 공유할 것은 **함수 하나**다.

```cpp
// UEPItemDefinitionSubsystem
// 반환: 성공 여부. 실패 시 State는 건드리지 않는다
bool MakeItemState(FName ItemId, FEPItemState& OutState) const
{
    const FEPItemData*       Row = FindData(ItemId);
    const UEPItemDefinition* Def = FindDefinition(ItemId);
    if (!Row || !Def)
    {
        UE_LOG(LogTemp, Error, TEXT("[ItemCore] '%s' — Row=%s Definition=%s"),
               *ItemId.ToString(), Row ? TEXT("OK") : TEXT("없음"),
               Def ? TEXT("OK") : TEXT("없음"));
        return false;
    }
    Def->InitState(*Row, OutState);      // ★ Row와 Definition을 둘 다 여기서 들고 있다
    return true;
}
```

> **`InitState`가 `FEPItemData`를 받는 비용이 여기서 0인 이유** — 이 함수가 유일한 호출부이고 조회를 이미 둘 다 한다. 그래서 `InitialCharges`를 DT로 옮기는 데 드는 것이 **인자 하나**뿐이다 (00-2).

- **넷모드 가드가 필요 없다.** 값을 채우는 순수 함수라 클라에서 불러도 아무 부작용이 없다. 권한 검사는 이 값을 **어디에 쓰는가**(서버 인벤토리 삽입 / 픽업 스폰)에서 이미 하고 있다
- 이전 설계의 `checkf(NetMode != NM_Client)`가 필요했던 이유는 **서버·클라의 인스턴스 그래프가 갈라지는 것**을 막기 위해서였다. 갈라질 그래프가 없어졌다

### ★ 모든 아이템이 Definition을 가진다 — 예외 없음

스택이 없어지면서 "상태 없는 아이템"이라는 범주가 사라졌다. 게다가 세 가지가 전부 Definition에만 있다.

| 필요한 것 | 있는 곳 | 쓰는 곳 |
|---|---|---|
| `WorldMesh` | Definition | 바닥 픽업 표시 (Step 01) |
| `Icon` | Definition | 인벤토리 UI (Step 04) |
| `InitState()` | Definition | 생성 전 경로 |

따라서 `Definition`이 없는 `ItemId`는 **정상 흐름이 아니라 에셋 누락**이고, 위 함수는 그것을 에러 로그로 드러낸다. 조용히 기본값으로 넘어가면 나중 Step에서 "메시가 안 보인다"로 나타나 원인 추적이 길어진다.

---

## 00-7. 설정 — DeveloperSettings + AssetManager

```cpp
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "EP Loot"))
class EMPLOYMENTPROJ_API UEPLootDeveloperSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category = "Data")
    TSoftObjectPtr<UDataTable> ItemDataTable;
};
```

> **`FName` 경로가 아니라 `TSoftObjectPtr<UDataTable>`을 쓴다.** 문자열 경로는 에셋을 옮기거나 이름을 바꿔도 컴파일·저장이 통과하고 런타임에 조용히 null이 된다. 소프트 포인터는 에디터가 참조를 추적해 리다이렉터를 따라간다.

**AssetManager 등록** — Project Settings → Asset Manager → Primary Asset Types to Scan에 추가:

| 필드 | 값 | 비고 |
|---|---|---|
| Primary Asset Type | `ItemDef` | 00-0에서 통일한 값 |
| Asset Base Class | `/Script/EmploymentProj.EPItemDefinition` | 서브클래스(`EPWeaponDefinition`)도 함께 잡힌다 |
| Directories | `/Game/Data` | DA 3종은 `/Game/Data/Weapons`에 있다 — 재귀로 커버 |
| bApplyRecursively | true | |
| **Is Editor Only** | **false** | ★ 아래 |
| Has Blueprint Classes | false | Definition은 C++ 에셋이다 |

> **★ `Is Editor Only`를 반드시 꺼둔다.** 현재 `DefaultGame.ini`의 기존 두 항목(`Map`, `PrimaryAssetLabel`)은 **둘 다 `bIsEditorOnly=True`** 다. 그 줄을 보고 따라 하면 쿠킹된 빌드에서 프라이머리 애셋 목록이 비어 **에디터에서만 되고 패키지에서는 안 되는** 상태가 된다.

> 이 항목 자체가 없으면 `LoadPrimaryAssetsWithType`이 **유효하지 않은 핸들을 돌려주고**, Definition이 하나도 로드되지 않은 채 조용히 진행된다. 그래서 00-5가 그 지점에 에러 로그를 둔다. 00-0의 타입 통일과 이 등록은 **세트**다.

---

## 00-8. `DT_Items`에 무기 이외 행 추가 + **각각 Definition 에셋**

현재 3행이 **전부 무기**다(`Weapon_AK74_HitScan` / `FastProj` / `SlowProj`, 모두 `EEPItemType::Weapon`, `Rarity::Rare`, `SlotSize` 미검증). 이 상태로는 기본 `InitState()` 경로도, 칸 수 합산도 **검증할 대상이 없다.**

| ItemId | ItemType | Rarity | `SlotSize` | `InitialCharges` | `bFungible` | `ContainerCapacity` | `SellPrice` | Definition |
|---|---|---|---|---|---|---|---|---|
| `AmmoBox_545` | Ammo | Common | 1 | **100** | **✅** | 0 | 500 | `DA_AmmoBox_545` |
| `Bandage` | Consumable | Common | 1 | 1 | ❌ | 0 | 200 | `DA_Bandage` |
| `Scrap_Paper` | Misc | Common | 1 | 0 | ❌ | 0 | 50 | `DA_Scrap_Paper` |
| `Resume` | QuestItem | Rare | 1 | 0 | ❌ | 0 | 0 | `DA_Resume` |
| `Cash_10000` | Misc | Common | 1 | **10000** | **✅** | 0 | **0** ★ | `DA_Cash_10000` |
| `Backpack_Small` | Misc | Uncommon | **2** | 0 | ❌ | **12** | 1500 | `DA_Backpack_Small` |

> **★ `SellPrice` 열을 반드시 채운다.** `FEPItemData::SellPrice`의 기본값이 **`100`** 이라(`EPItemData.h:43`) 비워두면 `Cash_10000`을 **10,000원짜리를 100원에 파는** 상태가 된다.
>
> **`bFungible` 아이템은 판매가가 `SellPrice`가 아니라 `Charges` 자체다.** 현금뭉치는 판매 대상이 아니라 그 자체가 돈이므로 `SellPrice = 0`으로 두고, 경제 시스템이 `bFungible`을 예외로 다룬다. 이 규칙을 지금 적어두지 않으면 판매 기능이 붙는 순간 조용히 틀린다.

각 행의 검증 목적:

| 행 | 검증하는 것 |
|---|---|
| `AmmoBox_545` | 기본 `InitState()` + `InitialCharges` 경로 |
| `Bandage` | **3개 주우면 엔트리 3개**인지 (스택 안 됨) |
| `Cash_10000` | **둘 주우면 엔트리 1개, `Charges=20000`** 인지 (`bFungible`) |
| `Backpack_Small` | **본체 10칸과 별개로 12칸이 열리는지.** 버리면 안의 아이템이 같이 나가는지 |
| `Resume` | 비무기 Definition이 AssetManager에 잡히는지 |

그리고 **기존 무기 3행의 `SlotSize`를 4~5로 올린다.** 전부 1이면 "칸 수 합산"과 "엔트리 개수 세기"가 구분되지 않아 Step 03에서 합산 로직의 버그가 무증상으로 지나간다.

> **본체가 10칸이므로**(GAME.md) 무기 5칸 + 배낭 2칸이면 벌써 7칸이다. Step 03의 "칸이 모자라 못 줍는" 경로를 손쉽게 재현할 수 있다 — 의도한 것이다.

### ★ 모든 행이 Definition 에셋을 가진다

이전 설계에서는 "스택 아이템은 인스턴스를 안 만드니 Definition이 불필요"였다. **스택이 없어지면서 그 예외가 사라졌고**, 게다가 바닥 픽업의 `WorldMesh`와 UI의 `Icon`이 전부 Definition에만 있으므로 애초에 예외가 성립하지 않았다 (00-6).

| 항목 | 값 |
|---|---|
| 위치 | `/Game/Data/Items/DA_*` — 클래스는 **`UEPItemDefinition`**(무기가 아니다) |
| `ItemId` | 대응 Row Name과 동일 |
| `ItemDataRow` | `DT_Items` / 해당 Row |
| `InitialCharges` | 위 표대로 |
| `WorldMesh` / `Icon` | 비워둔다 — 픽업 표시는 Step 01, 아이콘은 Step 04에서 플레이스홀더 |
| DT 쪽 `ItemDefinition` | 각 DA를 가리키게 한다 (`IsDataValid` 역참조 검사 대상) |

부수 효과로 **AssetManager 등록(00-7)이 무기 이외 Definition도 잡는지**가 검증된다 — 00-0의 타입 통일이 실제로 먹혔는지 확인하는 경로다. `/Game/Data/Items`는 `/Game/Data` 재귀 스캔에 포함된다.

---

## 00-9. 검증 커맨드

```cpp
// #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 가드 — SSR 디버그와 동일

EP.Item.State <ItemId>   // MakeItemState() 결과를 출력 (Charges / Durability / SlotSize)
EP.Item.Dump             // DataCache 행 수 / DefinitionCache 상주 수
```

`FAutoConsoleCommandWithWorldAndArgs`로 등록한다. 순수 조회라 **클라이언트에서 실행해도 된다** — 이전 설계의 `EP.Item.Make`가 서버 전용이었던 것과 다르다.

기대 출력:

```
> EP.Item.State Weapon_AK74_HitScan
[ItemCore] Weapon_AK74_HitScan  Charges=30  Durability=100.0  SlotSize=5   (WeaponDefinition)

> EP.Item.State AmmoBox_545
[ItemCore] AmmoBox_545          Charges=100 Durability=100.0  SlotSize=1   (ItemDefinition)

> EP.Item.State Resume
[ItemCore] Resume               Charges=0   Durability=100.0  SlotSize=1   (ItemDefinition)

> EP.Item.Dump
[ItemCore] DataCache=9  Definitions=9
```

> **`Definitions=9`이 핵심 수치다.** `3`이 나오면 새 DA를 안 만들었거나 AssetManager 스캔 경로에서 빠진 것이고, `4`가 나오면 일부만 만든 것이다. `EP.Item.State`가 해당 아이템에서 에러 로그를 남기므로 어느 것이 빠졌는지 바로 나온다.
>
> **살아있는 인스턴스 수(`LiveInstances`)를 세지 않는다.** 셀 인스턴스가 없다 — 상태는 엔트리와 픽업 안에 값으로 들어 있고, 소유자가 사라지면 같이 사라진다. 이전 설계에서 누수 감시용으로 필요했던 수치다.

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | `WeaponDef` 타입이 남아 있음 | 무기 Definition만 로드 안 됨. `EP.Item.State Weapon_*`이 에러 | 00-0 오버라이드 제거 |
| 2 | AssetManager 미등록 | `Definitions=0`. 00-5의 에러 로그가 없으면 무증상 | 00-7 등록 + `LoadPrimaryAssetsWithType` 실패 시 에러 로그 |
| 2b | `Is Editor Only` 체크됨 | 에디터에서는 되는데 **패키지 빌드에서만** `Definitions=0` | 00-7 표 참조. 기존 `Map`/`PrimaryAssetLabel` 항목을 따라 하면 걸린다 |
| 2c | Definition 로드를 비동기로 둠 | 초반 몇 프레임 동안 `FindDefinition`이 null. 재현이 타이밍 의존적 | `WaitUntilComplete()` (00-5) |
| 2d | 신규 DA 4종 중 일부 미생성 | `Definitions`가 7 미만. 해당 아이템이 다음 Step에서 메시 없이 나타난다 | 00-8 |
| 3 | `FEPItemData*` 캐시 | 에디터에서 DT 리임포트 후 크래시. 패키지에선 재현 안 됨 | 값 복사 (00-5) |
| 4 | `IsDataValid` 구버전 시그니처 | 5.7에서 deprecated 경고, 검증이 안 불림 | `FDataValidationContext&` const 버전 |
| 5 | `FEPItemState`를 `UPROPERTY` 없이 선언 | 복제·직렬화가 조용히 빠짐. Step 03에서 잔탄이 클라에 안 감 | 필드마다 `UPROPERTY()` |
| 6 | 모든 행의 `SlotSize`가 1 | 칸 수 합산과 엔트리 개수 세기가 구분 안 됨 → Step 03 합산 버그가 무증상 | 무기를 4~5로 (00-8) |
| 7 | 인스턴스 클래스를 "혹시 몰라" 남겨둠 | 두 표현이 공존해 다음 Step에서 어느 쪽이 진실인지 갈린다 | 00-4에서 **파일째 삭제** |

---

## 이 단계에서 하지 않는 것

- `EPCombatComponent.cpp:177`의 만탄 리셋 제거 → **Step 05**
- `EPGameMode::HandleStartingNewPlayer`의 `DefaultLoadout` 이관 → **Step 05**
- `UEPLootTable` / `EPLootTable` PrimaryAssetType 등록 → **Step 01**
- `GrantedAbility` 실제 사용(어빌리티 부여/발동) → 소모품 구현 시점
- `WorldMesh` 비동기 로드 → **Step 01** (`AEPPickup`)
- `FEPItemState`를 실제로 보관하는 곳(엔트리·픽업) → **Step 01·03**. 이번 단계는 **타입 선언과 초기화 경로까지**다
