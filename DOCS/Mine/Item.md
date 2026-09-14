# Item 시스템 설계 (Row + Definition + Instance)

> 기준: [Epic 학습 경로 Coder-05](https://dev.epicgames.com/documentation/ko-kr/unreal-engine/coder-05-manage-item-and-data-in-an-unreal-engine-game)
> `FTableRowBase` + `UItemDefinition(UPrimaryDataAsset)` + `UItemInstance(UObject)`

---

## 1. 아키텍처 원칙

1. `DataTable Row`는 운영/밸런스 수치의 소스다.
2. `ItemDefinition(DataAsset)`은 에셋/클래스 참조의 소스다.
3. `ItemInstance`는 플레이 중 변하는 상태의 소스다.
4. DB는 플레이어 상태만 저장한다. 게임 규칙 데이터는 저장하지 않는다.

---

## 2. 데이터 3계층

### 2-1. FEPItemData (DataTable Row — 운영 데이터)

```
Public/Data/EPItemData.h   (기존 파일 교체)
Private/Data/EPItemData.cpp (삭제 — 구조체이므로 cpp 불필요)
```

```cpp
// EPItemData.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Types/EPTypes.h"
#include "EPItemData.generated.h"

class UEPItemDefinition;

USTRUCT(BlueprintType)
struct FEPItemData : public FTableRowBase
{
    GENERATED_BODY()

    // 아이템 고유 ID (Row Name과 동일하게 유지)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FName ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    EEPItemType ItemType = EEPItemType::Misc;

    // 표시 정보
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    EEPItemRarity Rarity = EEPItemRarity::Common;

    // 인벤토리
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    int32 MaxStack = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    int32 SlotSize = 1;

    // 경제
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    int32 SellPrice = 0;

    // 플래그
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    bool bQuestItem = false;

    // 이 Row에 대응하는 Definition 에셋 참조
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    TSoftObjectPtr<UEPItemDefinition> ItemDefinition;
};
```

용도:
- 가격, 스택, 슬롯, 등급, 퀘스트 플래그 등 밸런스/운영 수치
- CSV/JSON 임포트/익스포트 가능 (밸런스 패치에 유리)

### 2-2. UEPItemDefinition (DataAsset — 정적 정의)

```
Public/Data/EPItemDefinition.h   (신규)
Private/Data/EPItemDefinition.cpp (신규)
```

```cpp
// EPItemDefinition.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Types/EPTypes.h"
#include "EPItemDefinition.generated.h"

UCLASS(BlueprintType, Blueprintable)
class EMPLOYMENTPROJ_API UEPItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Row와 매칭되는 ID
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    FName ItemId;

    // DataTable Row 핸들 (역참조용)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    FDataTableRowHandle ItemDataRow;

    // 월드에 떨어졌을 때 메시
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visual")
    TSoftObjectPtr<UStaticMesh> WorldMesh;

    // UI 아이콘
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visual")
    TSoftObjectPtr<UTexture2D> Icon;

    // PrimaryDataAsset ID
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
```

```cpp
// EPItemDefinition.cpp
#include "Data/EPItemDefinition.h"

FPrimaryAssetId UEPItemDefinition::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("ItemDef"), GetFName());
}
```

### 2-3. UEPWeaponDefinition (무기 전용 Definition)

```
Public/Data/EPWeaponDefinition.h   (신규 — 기존 EPWeaponData 대체)
Private/Data/EPWeaponDefinition.cpp (신규)
```

```cpp
// EPWeaponDefinition.h
#pragma once

#include "CoreMinimal.h"
#include "Data/EPItemDefinition.h"
#include "EPWeaponDefinition.generated.h"

UCLASS(BlueprintType, Blueprintable)
class EMPLOYMENTPROJ_API UEPWeaponDefinition : public UEPItemDefinition
{
    GENERATED_BODY()

public:
    // --- 기본 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FName WeaponName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    EEPFireMode FireMode = EEPFireMode::Auto;

    // --- 전투 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
    float Damage = 20.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Combat")
    float FireRate = 5.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
    uint8 MaxAmmo = 30;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
    float ReloadTime = 2.0f;

    // --- 탄 퍼짐 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
    float BaseSpread = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
    float SpreadPerShot = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
    float MaxSpread = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
    float SpreadRecoveryRate = 3.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
    float ADSSpreadMultiplier = 0.5f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
    float MovingSpreadMultiplier = 1.5f;

    // --- 반동 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
    float RecoilPitch = 0.3f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
    float RecoilYaw = 0.1f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
    float RecoilRecoveryRate = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
    TArray<FVector2D> RecoilPattern;

    // --- 애니메이션 (Lyra Linked Anim Layer) ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
    TSubclassOf<UAnimInstance> WeaponAnimLayer;

    // --- 비주얼 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visual")
    TSoftObjectPtr<USkeletalMesh> WeaponMesh;

    // PrimaryAssetId 오버라이드
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
```

```cpp
// EPWeaponDefinition.cpp
#include "Data/EPWeaponDefinition.h"

FPrimaryAssetId UEPWeaponDefinition::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("WeaponDef"), GetFName());
}
```

### 2-4. UEPItemInstance (런타임 상태 — 베이스)

```
Public/Data/EPItemInstance.h   (신규)
Private/Data/EPItemInstance.cpp (신규)
```

```cpp
// EPItemInstance.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "EPItemInstance.generated.h"

class UEPItemDefinition;

UCLASS(BlueprintType)
class EMPLOYMENTPROJ_API UEPItemInstance : public UObject
{
    GENERATED_BODY()

public:
    // 인스턴스 고유 ID
    UPROPERTY(BlueprintReadOnly, Category = "Item")
    FGuid InstanceId;

    // Row/Definition 매칭 키
    UPROPERTY(BlueprintReadOnly, Category = "Item")
    FName ItemId;

    // 수량 (스택)
    UPROPERTY(BlueprintReadWrite, Category = "Item")
    int32 Quantity = 1;

    // 직렬화 버전 (DB 마이그레이션용)
    UPROPERTY()
    int32 SchemaVersion = 1;

    // 캐시된 Definition 참조 (런타임에 Resolve)
    UPROPERTY(Transient)
    TObjectPtr<UEPItemDefinition> CachedDefinition;

    // 팩토리 함수
    static UEPItemInstance* CreateInstance(UObject* Outer, FName InItemId, UEPItemDefinition* InDefinition = nullptr);

    // 서브오브젝트 복제 지원 (인벤토리 복제 시 필요)
    virtual bool IsSupportedForNetworking() const override { return true; }
};
```

```cpp
// EPItemInstance.cpp
#include "Data/EPItemInstance.h"

UEPItemInstance* UEPItemInstance::CreateInstance(UObject* Outer, FName InItemId, UEPItemDefinition* InDefinition)
{
    UEPItemInstance* Instance = NewObject<UEPItemInstance>(Outer);
    Instance->InstanceId = FGuid::NewGuid();
    Instance->ItemId = InItemId;
    Instance->CachedDefinition = InDefinition;
    return Instance;
}
```

### 2-5. UEPWeaponInstance (무기 런타임 상태)

```
Public/Data/EPWeaponInstance.h   (신규)
Private/Data/EPWeaponInstance.cpp (신규)
```

```cpp
// EPWeaponInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Data/EPItemInstance.h"
#include "EPWeaponInstance.generated.h"

UCLASS(BlueprintType)
class EMPLOYMENTPROJ_API UEPWeaponInstance : public UEPItemInstance
{
    GENERATED_BODY()

public:
    // 현재 탄약
    UPROPERTY(BlueprintReadWrite, Category = "Weapon")
    int32 CurrentAmmo = 0;

    // 내구도
    UPROPERTY(BlueprintReadWrite, Category = "Weapon")
    float Durability = 100.f;

    // 팩토리 함수
    static UEPWeaponInstance* CreateWeaponInstance(UObject* Outer, FName InItemId, int32 InMaxAmmo, UEPItemDefinition* InDefinition = nullptr);
};
```

```cpp
// EPWeaponInstance.cpp
#include "Data/EPWeaponInstance.h"

UEPWeaponInstance* UEPWeaponInstance::CreateWeaponInstance(UObject* Outer, FName InItemId, int32 InMaxAmmo, UEPItemDefinition* InDefinition)
{
    UEPWeaponInstance* Instance = NewObject<UEPWeaponInstance>(Outer);
    Instance->InstanceId = FGuid::NewGuid();
    Instance->ItemId = InItemId;
    Instance->CurrentAmmo = InMaxAmmo;
    Instance->CachedDefinition = InDefinition;
    return Instance;
}
```

---

## 3. Enum 추가 (EPTypes.h)

기존 `FItemData` 구조체 제거, `EEPItemType` 추가:

```cpp
// EPTypes.h에 추가
UENUM(BlueprintType)
enum class EEPItemType : uint8
{
    Weapon,
    Ammo,
    Consumable,
    QuestItem,
    Misc
};
```

기존 `FItemData` 구조체 (EPTypes.h 내) → 삭제.

---

## 4. 데이터 흐름

```
FEPItemData (DataTable Row)
    ├─ ItemId: "Weapon_AK74"
    ├─ ItemType: Weapon
    ├─ DisplayName/Description/Rarity
    ├─ MaxStack: 1, SlotSize: 2, SellPrice: 3000
    └─ ItemDefinition: → DA_AK74 (TSoftObjectPtr)
         ↓
UEPWeaponDefinition (DataAsset, UEPItemDefinition 자식)
    ├─ ItemId: "Weapon_AK74"
    ├─ ItemDataRow: → DT_Items.Weapon_AK74 (역참조)
    ├─ WorldMesh: SM_AK74
    ├─ Icon: T_AK74_Icon
    ├─ Damage: 20, FireRate: 10, MaxAmmo: 30 ...
    ├─ WeaponAnimLayer: ABP_Rifle
    └─ WeaponMesh: SK_AK74
         ↓
UEPItemInstance (런타임, 플레이어별)
    ├─ InstanceId: FGuid
    ├─ ItemId: "Weapon_AK74"
    ├─ Quantity: 1
    ├─ CurrentAmmo: 17
    ├─ Durability: 92
    └─ CachedDefinition: → DA_AK74 (Transient)
```

---

## 5. 참조 규칙

1. `ItemId`는 Row / Definition / Instance / DB를 연결하는 공통 키다.
2. Row → Definition: `TSoftObjectPtr<UEPItemDefinition>` (필요시 로드)
3. Definition → Row: `FDataTableRowHandle` (역참조)
4. Instance는 `ItemId + 상태`만 가진다. Definition은 `Transient` 캐시.
5. 월드 Actor(예: AEPWeapon)는 표현체이며 상태 원본이 아니다.

---

## 6. 기존 코드 마이그레이션

### 6-1. EPTypes.h

| 작업 | 내용 |
|------|------|
| 삭제 | `FItemData` 구조체 |
| 추가 | `EEPItemType` 열거형 |

### 6-2. EPItemData.h/.cpp (기존 파일 교체)

| 작업 | 내용 |
|------|------|
| 삭제 | `UEPItemData : UPrimaryDataAsset` (클래스 전체) |
| 교체 | `FEPItemData : FTableRowBase` (구조체) |
| 삭제 | `EPItemData.cpp` (구조체에는 cpp 불필요) |

### 6-3. EPWeaponData.h/.cpp (삭제)

| 작업 | 내용 |
|------|------|
| 삭제 | `EPWeaponData.h`, `EPWeaponData.cpp` 전체 |
| 대체 | `EPWeaponDefinition.h/.cpp`가 모든 필드를 흡수 |

### 6-4. EPWeapon.h/.cpp

| 작업 | 내용 |
|------|------|
| 변경 | `#include "Data/EPWeaponData.h"` → `#include "Data/EPWeaponDefinition.h"` |
| 변경 | `TObjectPtr<UEPWeaponData> WeaponData` → `TObjectPtr<UEPWeaponDefinition> WeaponDef` |
| 변경 | 모든 `WeaponData->` 참조를 `WeaponDef->` 로 변경 |

### 6-5. EPCombatComponent.cpp

| 작업 | 내용 |
|------|------|
| 변경 | `EquippedWeapon->WeaponData->` → `EquippedWeapon->WeaponDef->` |

### 6-6. EPCorpse.h/.cpp

| 작업 | 내용 |
|------|------|
| 삭제 | `struct FItemData;` forward declaration |
| 삭제 | `TArray<FItemData> Inventory;` 멤버 |
| (향후) | 인벤토리 시스템 구현 시 `TArray<UEPItemInstance*>`로 교체 |

### 6-7. 신규 파일

| 파일 | 내용 |
|------|------|
| `Public/Data/EPItemDefinition.h` | 베이스 Definition |
| `Private/Data/EPItemDefinition.cpp` | GetPrimaryAssetId |
| `Public/Data/EPWeaponDefinition.h` | 무기 Definition (EPWeaponData 대체) |
| `Private/Data/EPWeaponDefinition.cpp` | GetPrimaryAssetId |
| `Public/Data/EPItemInstance.h` | 베이스 Instance |
| `Private/Data/EPItemInstance.cpp` | CreateInstance 팩토리 |
| `Public/Data/EPWeaponInstance.h` | 무기 Instance (탄약, 내구도) |
| `Private/Data/EPWeaponInstance.cpp` | CreateWeaponInstance 팩토리 |

---

## 7. 에디터 작업

### 7-1. DataTable 생성

1. Content Browser → `Content/Data/` 폴더
2. Add → Miscellaneous → Data Table
3. Row Structure → `EPItemData` 선택
4. 이름: `DT_Items`
5. 행 추가 예시:

| Row Name | ItemId | ItemType | DisplayName | MaxStack | SlotSize | SellPrice | ItemDefinition |
|----------|--------|----------|-------------|----------|----------|-----------|----------------|
| Weapon_AK74 | Weapon_AK74 | Weapon | AK-74 | 1 | 2 | 3000 | DA_AK74 |
| Ammo_545 | Ammo_545 | Ammo | 5.45x39mm | 60 | 1 | 50 | DA_Ammo_545 |

### 7-2. WeaponDefinition DataAsset 생성

1. Content Browser → `Content/Data/Weapons/`
2. Add → Miscellaneous → Data Asset
3. Class → `EPWeaponDefinition` 선택
4. 이름: `DA_AK74`
5. 기존 `DA_Rifle`(EPWeaponData)의 값들을 옮겨 입력
6. ItemDataRow → `DT_Items.Weapon_AK74` 지정

### 7-3. 기존 DA_Rifle 교체

1. 기존 `DA_Rifle` (UEPWeaponData 기반) 삭제
2. `DA_AK74` (UEPWeaponDefinition 기반)으로 교체
3. BP_EPWeapon의 WeaponDef에 `DA_AK74` 할당

---

## 8. 런타임 책임 분리

| 대상 | 책임 |
|------|------|
| Character | 입력 수집, 이동/시점, 컴포넌트 위임 |
| CombatComponent | 장착/발사/재장전 RPC, 전투 이펙트 |
| InventoryComponent | 슬롯/스택/장착 관리 (향후) |
| ItemInstance | 상태 보관 |
| GAS Ability | PrimaryUse/SecondaryUse/Reload/Interact 실행 (향후) |

---

## 9. DB 통합 원칙

1. 저장 대상:
- 인벤토리 Instance
- 스태시
- 재화/진행도/거래 기록

2. 저장 금지:
- 아이템 정의(메시/FX/클래스 참조)
- 밸런스/확률/가격표

3. 저장 포맷:
- `ItemId + InstanceState + SchemaVersion`

예시:
```json
{
  "instance_id": "6f5d...-...",
  "item_id": "Weapon_AK74",
  "quantity": 1,
  "current_ammo": 17,
  "durability": 92,
  "schema_version": 3
}
```

---

## 10. 구현 순서

1. **EPTypes.h 수정** — `FItemData` 삭제, `EEPItemType` 추가
2. **EPItemData.h 교체** — `UEPItemData` 삭제 → `FEPItemData` (FTableRowBase) 작성
3. **EPItemData.cpp 삭제** — 구조체이므로 불필요
4. **EPItemDefinition.h/.cpp 생성** — 베이스 Definition
5. **EPWeaponDefinition.h/.cpp 생성** — EPWeaponData 필드 전부 흡수
6. **EPItemInstance.h/.cpp 생성** — 베이스 런타임 상태
7. **EPWeaponInstance.h/.cpp 생성** — 무기 전용 런타임 상태 (탄약, 내구도)
8. **EPWeapon.h/.cpp 수정** — `WeaponData` → `WeaponDef` (타입 + 참조 전부)
9. **EPCombatComponent.cpp 수정** — `WeaponData->` → `WeaponDef->`
10. **EPWeaponData.h/.cpp 삭제**
11. **EPCorpse.h/.cpp 수정** — `FItemData` 참조 제거
12. **에디터: DT_Items** DataTable 생성
13. **에디터: DA_AK74** WeaponDefinition DataAsset 생성, 기존 DA_Rifle 값 이전
14. **에디터: BP_EPWeapon** WeaponDef에 DA_AK74 할당
15. **빌드 + 테스트** — 기존 사격/리로드 동작 확인

---

## 11. 향후 확장

- `UEPConsumableDefinition : UEPItemDefinition` — 소모품
- `UEPAmmoDefinition : UEPItemDefinition` — 탄약
- `UEPInventoryComponent` — 인벤토리 관리
- GAS 연동: Definition에 `TSubclassOf<UGameplayAbility>` 추가

---

## 12. 완료 기준

- [ ] Row/Definition/Instance 3계층 C++ 클래스 존재 (Instance는 베이스 + WeaponInstance)
- [ ] EPWeaponData 삭제, EPWeaponDefinition으로 완전 대체
- [ ] EPWeapon이 WeaponDef 참조로 정상 동작
- [ ] DT_Items DataTable 에디터에서 생성/편집 가능
- [ ] 기존 사격/리로드/스프레드/리코일 기능 정상 동작
- [ ] FItemData (EPTypes.h) 레거시 제거
