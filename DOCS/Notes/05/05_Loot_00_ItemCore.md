# Step 00 — ItemCore (아이템 계층 정비)

> 마스터 기획: `05_Loot_DOCS.md` (§4-1, §4-9)
> 진행 상황: `LOOT_STATUS.md`

---

## 목표

`ItemId → Definition → Instance` 경로를 **처음으로 실제로 돌게 만든다.** 게임플레이 기능은 하나도 추가하지 않는다. 눈에 보이는 결과가 없는 대신 콘솔 커맨드 하나로 독립 검증된다.

**완료 조건**

- [ ] `EP.Item.Make Weapon_AK74_HitScan` → `UEPWeaponInstance`가 생성되고 `CurrentAmmo == 30`으로 초기화됨
- [ ] `EP.Item.Make Resume` → **`UEPItemInstance`**(기본 클래스)가 생성됨 — `DA_Resume` 필요 (00-8)
- [ ] `EP.Item.Make Ammo_762` → 스택 아이템이므로 **인스턴스를 만들지 않고** `INDEX_NONE` 반환
- [ ] `EP.Item.Dump` → `DataCache=7  Definitions=4  LiveInstances=0`
- [ ] 에디터에서 DT ↔ Definition 참조를 일부러 어긋나게 하면 `IsDataValid()`가 잡아냄
- [ ] **데디케이티드 서버로 실행해도 `EP.Item.Dump`가 `Definitions=4`** — Definition 상주가 넷모드와 무관함을 확인

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

## 00-1. `UEPItemDefinition` — 팩토리와 검증

### 핵심 설계: `CreateInstance`는 non-virtual, `GetInstanceClass`만 virtual

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

    // 이 Definition이 만들 인스턴스 클래스 — 서브클래스는 이것만 오버라이드한다
    virtual TSubclassOf<UEPItemInstance> GetInstanceClass() const;

    // 실제 생성. 오버라이드하지 않는다 (const가 아니다 — 아래 참조)
    UEPItemInstance* CreateInstance(UObject* Outer);

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
```

```cpp
// EPItemDefinition.cpp
TSubclassOf<UEPItemInstance> UEPItemDefinition::GetInstanceClass() const
{
    return UEPItemInstance::StaticClass();
}

UEPItemInstance* UEPItemDefinition::CreateInstance(UObject* Outer)
{
    const TSubclassOf<UEPItemInstance> Class = GetInstanceClass();
    if (!Class) return nullptr;

    UEPItemInstance* Instance = NewObject<UEPItemInstance>(Outer, Class);
    Instance->InitFromDefinition(this);
    return Instance;
}
```

**왜 `CreateInstance`를 virtual로 두지 않는가:** virtual로 두면 서브클래스마다 `NewObject` + `InitFromDefinition` 호출을 복붙하게 되고, 나중에 생성 경로에 공통 처리(로그, 통계, GUID 발급)를 넣을 때 전부 고쳐야 한다. **생성 절차는 한 곳, 타입 결정만 분기**가 맞다.

> **왜 `const`를 붙이지 않는가:** `const`로 두면 `InitFromDefinition(this)`가 `const UEPItemDefinition*`를 넘기게 되고, 인스턴스가 `CachedDefinition`(비const)에 담으려면 `const_cast`가 필요해진다. 팩토리가 논리적으로 const일 이유도 없으니 애초에 붙이지 않는다. `GetInstanceClass()`만 `const`로 남긴다.

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

## 00-2. `UEPWeaponDefinition` — 3가지 변경

```cpp
// 1) GetPrimaryAssetId() 오버라이드 제거 (선언·정의 둘 다)      ← 00-0

// 2) MaxAmmo 타입 변경
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
int32 MaxAmmo = 30;        // was: uint8

// 3) 인스턴스 클래스만 선언
virtual TSubclassOf<UEPItemInstance> GetInstanceClass() const override
{
    return UEPWeaponInstance::StaticClass();
}
```

**`MaxAmmo`를 `int32`로 바꾸는 이유:** 지금 `uint8`(Definition) → `int32`(Instance) → `float`(GAS 어트리뷰트) 3중 캐스팅이 낀다. Step 05의 주입/write-back 경로가 이 위를 왕복하므로 최소한 정수 쪽은 통일한다. 폭 확대라 기존 DataAsset 값은 손실 없이 유지된다.

> `EPCombatComponent.cpp:177-178`의 `static_cast<float>(NewWeapon->WeaponDef->MaxAmmo)`는 `int32`로 바뀌어도 그대로 컴파일된다. 이 줄의 **제거는 Step 05**다 — 지금 건드리지 않는다.

---

## 00-3. `UEPItemInstance` — 3개 제거, 1개 추가

```cpp
// EPItemInstance.h
UCLASS(BlueprintType)
class EMPLOYMENTPROJ_API UEPItemInstance : public UObject
{
    GENERATED_BODY()

public:
    // DB 영속용. 복제·RPC에는 UEPItemInstanceSubsystem의 int32 핸들을 쓴다 (§4-1)
    UPROPERTY(BlueprintReadOnly, Category = "Item")
    FGuid InstanceId;

    UPROPERTY(BlueprintReadOnly, Category = "Item")
    FName ItemId;

    UPROPERTY()
    int32 SchemaVersion = 1;

    UPROPERTY(Transient)
    TObjectPtr<UEPItemDefinition> CachedDefinition;

    virtual void InitFromDefinition(UEPItemDefinition* Definition);
};
```

| 제거 | 이유 |
|---|---|
| `Quantity` | 수량의 진실은 `FEPInventoryEntry::Quantity`(인벤토리) 또는 `AEPPickup::Quantity`(월드) **하나뿐**이다. 여기 남겨두면 §4-8의 "탄약의 진실이 두 곳" 문제가 스택에서 그대로 재발한다 |
| `IsSupportedForNetworking()` | 인스턴스는 복제하지 않기로 확정했다(§4-6). `return true`가 남아 있으면 "복제되는 줄 알았다"는 오해의 씨앗이 된다 |
| `static CreateInstance()` | Definition의 팩토리로 대체 (00-1) |

```cpp
// EPItemInstance.cpp
void UEPItemInstance::InitFromDefinition(UEPItemDefinition* Definition)
{
    if (!Definition) return;
    InstanceId       = FGuid::NewGuid();
    ItemId           = Definition->ItemId;
    CachedDefinition = Definition;
}
```

---

## 00-4. `UEPWeaponInstance`

```cpp
// EPWeaponInstance.h — static CreateWeaponInstance() 제거
UPROPERTY(BlueprintReadWrite, Category = "Weapon")
int32 CurrentAmmo = 0;

UPROPERTY(BlueprintReadWrite, Category = "Weapon")
float Durability = 100.f;

virtual void InitFromDefinition(UEPItemDefinition* Definition) override;
```

```cpp
// EPWeaponInstance.cpp
void UEPWeaponInstance::InitFromDefinition(UEPItemDefinition* Definition)
{
    Super::InitFromDefinition(Definition);
    if (const UEPWeaponDefinition* WeaponDef = Cast<UEPWeaponDefinition>(Definition))
        CurrentAmmo = WeaponDef->MaxAmmo;      // 새로 만든 무기는 만탄
}
```

> `Cast<>`가 여기 한 번 나오지만 문제없다. **자기 Definition 타입을 아는 건 인스턴스 자신뿐**이고, 이 캐스트는 인벤토리·픽업·자판기로 번지지 않는다. §4-9가 막으려던 건 "호출부의 타입 분기"이지 "구현부의 다운캐스트"가 아니다.

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

**왜 소프트 참조로 지연 로드하지 않는가:** 픽업 획득은 RPC 응답 안에서 성패가 결정돼야 하는 **동기 경로**인데, `Definition->CreateInstance()`는 Definition이 메모리에 있어야 호출된다. 로드를 기다리는 사이 "줍기 성공/실패"를 유보할 수 없다. 반면 `WorldMesh`/`Icon`은 지연이 허용되므로 소프트로 남긴다 (§4-1).

Definition은 수치·참조만 담은 메타데이터라 개당 수 KB다. 수백 개여도 상주 비용이 문제되지 않는다.

> **데디케이티드 서버:** Definition은 로드하되 `WorldMesh`/`Icon`/`WeaponMesh`는 건드리지 않는다. 이 분리가 성립하는 이유가 위 정책이다. 서버에서 시각 에셋을 로드하는 코드는 `AEPPickup`(Step 01)에만 들어가고 거기서 `IsNetMode(NM_DedicatedServer)` 가드를 건다.

---

## 00-6. `UEPItemInstanceSubsystem` (World, 서버 전용)

```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPItemInstanceSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // 반환: 유효 핸들, 또는 INDEX_NONE(스택 아이템이라 인스턴스를 만들지 않음 / 실패)
    int32 CreateInstance(FName ItemId);

    UEPItemInstance* Find(int32 Handle) const;
    void Destroy(int32 Handle);

private:
    UPROPERTY()
    TMap<int32, TObjectPtr<UEPItemInstance>> Instances;   // ★ 유일한 강참조

    int32 NextHandle = 1;
};
```

### ★ 상태 없는 아이템은 인스턴스를 만들지 않는다

```cpp
int32 UEPItemInstanceSubsystem::CreateInstance(FName ItemId)
{
    checkf(GetWorld()->GetNetMode() != NM_Client,
           TEXT("아이템 인스턴스는 서버에서만 생성된다."));

    const UEPItemDefinitionSubsystem* Defs = /* GameInstance에서 획득 */;
    const FEPItemData* Data = Defs->FindData(ItemId);
    if (!Data) return INDEX_NONE;

    // 스택 아이템(탄약·붕대·잡템)은 개체 상태가 없다 → 인스턴스 자체를 만들지 않는다
    if (Data->MaxStack > 1) return INDEX_NONE;

    UEPItemDefinition* Def = Defs->FindDefinition(ItemId);
    if (!Def)
    {
        // 비스택 아이템인데 Definition이 없다 = 에셋 누락. 조용히 넘기면 안 된다
        UE_LOG(LogTemp, Error, TEXT("[ItemCore] '%s'는 MaxStack==1인데 Definition 에셋이 없습니다."),
               *ItemId.ToString());
        return INDEX_NONE;
    }

    UEPItemInstance* Instance = Def->CreateInstance(this);   // Outer = 서브시스템
    if (!Instance) return INDEX_NONE;

    const int32 Handle = NextHandle++;
    Instances.Add(Handle, Instance);
    return Handle;
}
```

| 조건 | 인스턴스 | 핸들 |
|---|---|---|
| `MaxStack > 1` (탄약·붕대·잡템) | 만들지 않는다 | `INDEX_NONE` |
| `MaxStack == 1` (무기·방어구) | 만든다 | 유효 |

**이 규칙이 사는 이유:** 스택 아이템의 인벤토리 엔트리는 순수 `(ItemId, Quantity)`가 되어 **병합은 정수 덧셈, 분할은 정수 뺄셈**으로 끝난다. 인스턴스를 지우거나 새로 만들 일이 없다. 비스택 아이템은 병합 자체가 불가능하므로 분할 문제가 생기지 않고, 잔탄·내구도 보존이 필요한 것도 이 경우뿐이다.

> `INDEX_NONE`이 "실패"와 "인스턴스 불필요" 둘 다를 의미한다. 호출부(Step 03 `AddItem`)는 **`FindData()`로 이미 유효성을 확인한 뒤** 부르므로 혼동이 없다. 구분이 필요해지면 그때 `EEPCreateResult`를 도입한다 — 지금 넣으면 쓰지 않는 분기만 생긴다.
>
> 다만 **"비스택인데 Definition이 없다"는 데이터 오류**이지 정상 흐름이 아니므로, 반환값으로 구분하는 대신 **에러 로그로 드러낸다.** 이게 없으면 `DA_Resume` 누락(함정 2d)이 무증상으로 지나간다.

**Outer는 끝까지 서브시스템이다.** 인벤토리 → 픽업 → 인벤토리를 오가도 바뀌는 것은 "누가 그 핸들을 들고 있는가"뿐이고 `Rename()`은 일어나지 않는다 (§4-1).

> **서버 전용 강제:** `UWorldSubsystem::ShouldCreateSubsystem`에서 넷모드로 거르는 방법은 **쓰지 않는다** — 서브시스템 생성 시점에 월드의 넷모드가 확정되지 않은 경우가 있다. 대신 위처럼 **API 진입점에서 `checkf`로 막는다.** 클라이언트에 객체가 존재하되 절대 쓰이지 않는 상태가 되고, 잘못 부르면 개발 빌드에서 즉시 터진다.

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

## 00-8. `DT_Items`에 스택 아이템 행 추가

현재 3행이 **전부 무기**다(`Weapon_AK74_HitScan` / `FastProj` / `SlowProj`, 모두 `EEPItemType::Weapon`, `Rarity::Rare`). 이 상태로는 00-6의 `MaxStack > 1` 분기를 **검증할 대상이 없다.**

최소 이만큼 추가한다.

| ItemId | ItemType | Rarity | MaxStack | Definition 에셋 | 용도 |
|---|---|---|---|---|---|
| `Ammo_762` | Ammo | Common | 60 | **불필요** | 스택 병합·부분 획득 검증의 주력 |
| `Bandage` | Consumable | Common | 5 | **불필요** | 소모품 자리 확인 |
| `Scrap_Paper` | Misc | Common | 10 | **불필요** | 잡템 — 등급 테이블의 "일반" 채우기 |
| `Resume` | QuestItem | Rare | **1** | **★ `DA_Resume` 필요** | **비스택 비무기** — `GetInstanceClass()` 기본 분기 검증 |

**스택 아이템 3종은 Definition 에셋이 필요 없다** — `MaxStack > 1`이라 00-6의 `CreateInstance`가 `FindDefinition` 앞에서 `INDEX_NONE`으로 빠진다.

### ★ `Resume`은 예외다 — `DA_Resume`을 반드시 만든다

`Resume`은 `MaxStack == 1`이므로 **`CreateInstance` 경로를 그대로 탄다.** Definition이 없으면 00-6에서 이렇게 끝난다.

```cpp
if (Data->MaxStack > 1) return INDEX_NONE;      // Resume은 통과한다
UEPItemDefinition* Def = Defs->FindDefinition(ItemId);
if (!Def) return INDEX_NONE;                    // ← DA_Resume이 없으면 여기서 탈락
```

`Resume`을 넣은 목적이 정확히 이 경로(기본 `UEPItemInstance` 생성)를 한 번이라도 실행시키는 것이므로, 에셋이 없으면 항목 자체가 무의미해진다.

| 항목 | 값 |
|---|---|
| 에셋 | `/Game/Data/Items/DA_Resume` — 클래스는 **`UEPItemDefinition`**(무기가 아니다) |
| `ItemId` | `Resume` |
| `ItemDataRow` | `DT_Items` / `Resume` |
| `WorldMesh` / `Icon` | 비워둔다 |
| DT 쪽 `ItemDefinition` | `DA_Resume`을 가리키게 한다 (`IsDataValid` 역참조 검사 대상) |

부수 효과로 **AssetManager 등록(00-7)이 무기 이외 Definition도 잡는지**가 여기서 검증된다 — 00-0의 타입 통일이 실제로 먹혔는지 확인하는 유일한 케이스다. `/Game/Data/Items`는 `/Game/Data` 재귀 스캔에 포함된다.

> `Resume`이 중요한 이유: 이게 없으면 "비스택 = 무기"라는 우연한 등식이 성립해버려, `UEPItemInstance`(기본 클래스) 생성 경로가 한 번도 실행되지 않는다.

> 스택 아이템 3종의 `WorldMesh`/`Icon`은 비워둔다. 픽업 표시는 Step 01이고 거기서 플레이스홀더를 쓴다.

---

## 00-9. 검증 커맨드

```cpp
// #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 가드 — SSR 디버그와 동일

EP.Item.Make <ItemId>    // CreateInstance 후 핸들·클래스명·CurrentAmmo를 출력하고 즉시 Destroy
EP.Item.Dump             // DataCache 행 수 / DefinitionCache 상주 수 / 살아있는 인스턴스 수
```

`FAutoConsoleCommandWithWorldAndArgs`로 등록한다. `EP.Item.Make`는 서브시스템이 서버 전용이므로 **리슨서버 호스트나 PIE 단독**에서 실행한다.

기대 출력:

```
> EP.Item.Make Weapon_AK74_HitScan
[ItemCore] Handle=1  Class=UEPWeaponInstance  ItemId=Weapon_AK74_HitScan  CurrentAmmo=30

> EP.Item.Make Resume
[ItemCore] Handle=2  Class=UEPItemInstance    ItemId=Resume

> EP.Item.Make Ammo_762
[ItemCore] Handle=INDEX_NONE (MaxStack=60 > 1 — 인스턴스 불필요)

> EP.Item.Dump
[ItemCore] DataCache=7  Definitions=4  LiveInstances=0
```

> `Definitions=4` = `DA_AK74_HitScan` / `FastProj` / `SlowProj` + **`DA_Resume`**(00-8). `3`이 나오면 `DA_Resume`이 없거나 AssetManager 스캔 경로에서 빠진 것이고, 그 경우 `EP.Item.Make Resume`이 `INDEX_NONE`을 돌려준다.

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | `WeaponDef` 타입이 남아 있음 | 무기 Definition만 로드 안 됨. `EP.Item.Make Weapon_*`이 null | 00-0 오버라이드 제거 |
| 2 | AssetManager 미등록 | `Definitions=0`. 00-5의 에러 로그가 없으면 무증상 | 00-7 등록 + `LoadPrimaryAssetsWithType` 실패 시 에러 로그 |
| 2b | `Is Editor Only` 체크됨 | 에디터에서는 되는데 **패키지 빌드에서만** `Definitions=0` | 00-7 표 참조. 기존 `Map`/`PrimaryAssetLabel` 항목을 따라 하면 걸린다 |
| 2c | Definition 로드를 비동기로 둠 | 초반 몇 프레임 동안 `FindDefinition`이 null. 재현이 타이밍 의존적 | `WaitUntilComplete()` (00-5) |
| 2d | `DA_Resume` 미생성 | `EP.Item.Make Resume`이 `INDEX_NONE`. 기본 인스턴스 경로가 한 번도 안 돌아 다음 Step에서 처음 터진다 | 00-8 |
| 3 | `FEPItemData*` 캐시 | 에디터에서 DT 리임포트 후 크래시. 패키지에선 재현 안 됨 | 값 복사 (00-5) |
| 4 | `IsDataValid` 구버전 시그니처 | 5.7에서 deprecated 경고, 검증이 안 불림 | `FDataValidationContext&` const 버전 |
| 5 | `Quantity`를 인스턴스에 남겨둠 | Step 03에서 수량이 두 곳에 생겨 스택 병합이 어긋남 | 00-3에서 제거 |
| 6 | 클라이언트에서 `CreateInstance` 호출 | 서버·클라 인스턴스가 갈라짐 | `checkf` 가드 (00-6) |
| 7 | `Instances` 맵을 `UPROPERTY` 없이 선언 | GC가 인스턴스를 수거해 핸들이 죽은 객체를 가리킴 | `UPROPERTY()` 필수 |

---

## 이 단계에서 하지 않는 것

- `EPCombatComponent.cpp:177`의 만탄 리셋 제거 → **Step 05**
- `EPGameMode::HandleStartingNewPlayer`의 `DefaultLoadout` 이관 → **Step 05**
- `UEPLootTable` / `EPLootTable` PrimaryAssetType 등록 → **Step 01**
- `GrantedAbility` 실제 사용(어빌리티 부여/발동) → 소모품 구현 시점
- `WorldMesh` 비동기 로드 → **Step 01** (`AEPPickup`)
