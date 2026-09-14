# Post 2-3 작성 가이드 — 아이템 데이터 아키텍처 (3계층 구조)

> **예상 제목**: `[UE5] 추출 슈터 2-3. 타르코프 스타일 아이템 시스템 설계 (DataTable + DataAsset + Instance)`
> **참고 문서**: DOCS/Mine/Item.md, Epic Coder-05 학습 경로

---

## 개요

**이 포스팅에서 다루는 것:**
- 단순 WeaponData UObject에서 3계층 구조(Row + Definition + Instance)로 리팩터링한 이유와 방법
- 각 계층의 역할 분리와 `ItemId`로 연결하는 방식

**왜 이렇게 구현했는가 (설계 의도):**
- 타르코프처럼 동일 아이템이 각자 다른 상태(탄약 수, 내구도)를 가져야 함
- 운영 수치(밸런스)는 CSV로 관리하고 싶음 → DataTable Row
- 에셋 참조는 별도 DataAsset으로 분리해 메모리 관리
- Epic 공식 Coder-05 학습 경로의 패턴을 참조

---

## 구현 전 상태 (Before)

실제로 존재했던 코드:

```cpp
// UEPItemData — 아이템 공통 데이터 (UPrimaryDataAsset)
class UEPItemData : public UPrimaryDataAsset
{
    FName ItemName;
    FText Description;
    EEPItemRarity Rarity;
    int32 SellPrice = 100;
    bool bIsQuestItem = false;
    int32 SlotSize = 1;
    // 에셋 참조(메시, 아이콘) 없음
};

// UEPWeaponData — 무기 스탯 (UPrimaryDataAsset, UEPItemData와 무관)
class UEPWeaponData : public UPrimaryDataAsset  // UEPItemData 상속 아님
{
    FName WeaponName;
    EEPFireMode FireMode;
    float Damage = 20.f;
    float FireRate = 5.f;
    uint8 MaxAmmo = 30;
    float ReloadTime = 2.0f;
    float BaseSpread, SpreadPerShot, MaxSpread ...
    float RecoilPitch, RecoilYaw ...
    TArray<FVector2D> RecoilPattern;
    // 메시/아이콘 에셋 참조 없음
};

// FItemData — 미연결 구조체 잔재
struct FItemData
{
    FName ItemName;
    int32 Value;
    // UEPItemData와 아무 연결 없음
};

// AEPWeapon — 런타임 상태를 Actor에 직접 보유
class AEPWeapon : public AActor
{
    TObjectPtr<UEPWeaponData> WeaponData;  // 스탯 참조
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;  // 표현체

    UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo)
    uint8 CurrentAmmo = 0;  // 런타임 상태가 Actor에 직접

    UPROPERTY(Replicated)
    uint8 MaxAmmo = 30;
};
```

**문제점 4가지:**

1. **`UEPWeaponData`와 `UEPItemData`가 완전히 단절** — 무기는 아이템인데 상속 관계가 없어서 "아이템 슬롯에 무기 넣기" 같은 공통 처리가 불가능.

2. **`FItemData` 구조체가 미사용 잔재** — `UEPItemData`와 아무 연결 없이 떠있음. 어디서 쓰는지도 불분명.

3. **에셋 참조(메시, 아이콘)가 어디에도 없음** — `UEPItemData`와 `UEPWeaponData` 어디에도 WorldMesh, Icon 같은 에셋 참조 필드가 없음. UI에 아이템 아이콘을 표시하거나, 맵에 드랍된 아이템 메시를 보여줄 방법이 없음.

4. **런타임 상태(`CurrentAmmo`)가 Actor에 직접 존재** — `AEPWeapon`이 상태 원본. 인벤토리에 "탄약 17발짜리 AK74"와 "탄약 30발짜리 AK74"를 동시에 가지려면 Actor를 두 개 스폰해야 함. 타르코프처럼 줍고/버리는 과정에서 상태를 유지하는 게 구조적으로 불가능.

---

## 구현 내용

### 1. 3계층 아키텍처 설명

다이어그램으로 보여줄 것:

```
[운영/밸런스]        [정적 에셋]              [런타임 상태]
FEPItemData     →  UEPItemDefinition   →   UEPItemInstance
(DataTable Row)    (UPrimaryDataAsset)      (UObject)

ItemId로 연결  →   ItemId로 연결      →   ItemId 보유
```

**각 계층의 책임:**

| 계층 | 클래스 | 담당 | 변경 주체 |
|------|--------|------|-----------|
| Row | `FEPItemData` | 가격/스택/슬롯/등급 | 기획자 (CSV 패치) |
| Definition | `UEPItemDefinition` | 메시/아이콘/FX/애니레이어 | 개발자 |
| Instance | `UEPItemInstance` | 탄약/내구도/수량 | 게임 런타임 |

### 2. FEPItemData (DataTable Row)

```cpp
USTRUCT(BlueprintType)
struct FEPItemData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FName ItemId;  // Row Name과 동일하게 유지

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    int32 MaxStack = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    int32 SellPrice = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
    TSoftObjectPtr<UEPItemDefinition> ItemDefinition;  // Definition 참조
};
```

**DataTable의 장점**: 에디터에서 표 형태로 편집, CSV 익스포트 → 기획 패치 용이

> **스크린샷 위치**: 에디터에서 DT_Items DataTable 편집 화면 (Weapon_AK74 행)

### 3. UEPItemDefinition + UEPWeaponDefinition

```cpp
// 베이스 — 모든 아이템 공통
UCLASS(BlueprintType, Blueprintable)
class UEPItemDefinition : public UPrimaryDataAsset
{
    FName ItemId;
    TSoftObjectPtr<UStaticMesh> WorldMesh;  // 맵에 떨어진 아이템 모습
    TSoftObjectPtr<UTexture2D> Icon;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

// 무기 전용 — Definition 상속
UCLASS(BlueprintType, Blueprintable)
class UEPWeaponDefinition : public UEPItemDefinition
{
    float Damage = 20.f;
    float FireRate = 5.f;
    uint8 MaxAmmo = 30;
    TSubclassOf<UAnimInstance> WeaponAnimLayer;  // Linked Anim Layer
    TSoftObjectPtr<USkeletalMesh> WeaponMesh;
};
```

**UPrimaryDataAsset를 쓰는 이유**: Asset Manager의 비동기 로드, 메모리 관리 지원

> **스크린샷 위치**: DA_AK74 에셋 에디터 (Damage, FireRate 등 수치 입력 화면)

### 4. UEPItemInstance + UEPWeaponInstance

```cpp
// 런타임 상태 — 플레이어별, 아이템별 고유
UCLASS(BlueprintType)
class UEPItemInstance : public UObject
{
    FGuid InstanceId;      // 이 아이템 고유 ID (DB 저장용)
    FName ItemId;          // Definition 조회용 키
    int32 Quantity = 1;
    TObjectPtr<UEPItemDefinition> CachedDefinition;  // Transient, 런타임 캐시

    static UEPItemInstance* CreateInstance(UObject* Outer, FName InItemId, ...);
    virtual bool IsSupportedForNetworking() const override { return true; }
};

// 무기 전용 런타임 상태
UCLASS(BlueprintType)
class UEPWeaponInstance : public UEPItemInstance
{
    int32 CurrentAmmo = 0;
    float Durability = 100.f;
};
```

**핵심 설명**: 동일한 `ItemId("Weapon_AK74")`지만 각 Instance는 다른 `CurrentAmmo`를 가질 수 있음 → 타르코프 스타일 인벤토리 가능

### 5. 연결 구조 — 데이터 흐름

```
DT_Items["Weapon_AK74"]       (DataTable)
    └─ ItemDefinition → DA_AK74 (WeaponDefinition DataAsset)
                              │
                              ├─ Damage: 20
                              ├─ WeaponAnimLayer: ABP_RifleAnimLayers
                              └─ WeaponMesh: SK_KA47
                                       ↓
                    UEPWeaponInstance (런타임, 플레이어 인벤토리)
                              ├─ InstanceId: FGuid("6f5d...")
                              ├─ ItemId: "Weapon_AK74"
                              ├─ CurrentAmmo: 17
                              └─ Durability: 92
```

### 6. 기존 코드 마이그레이션

**변경 전 → 후 핵심 포인트:**

| 파일 | 변경 내용 |
|------|-----------|
| `EPWeapon.h` | `UEPWeaponData* WeaponData` → `UEPWeaponDefinition* WeaponDef` |
| `EPCombatComponent.cpp` | `WeaponData->Damage` → `WeaponDef->Damage` |
| `EPWeaponData.h/.cpp` | 삭제 (WeaponDefinition이 흡수) |

> **스크린샷 위치**: VS 에서 WeaponDef 포인터를 통해 Damage 등 접근하는 코드

---

## 결과

**확인 항목:**
- DT_Items DataTable 에디터에서 행 추가/수정 가능
- DA_AK74 에셋에서 Damage, FireRate 등 수치 입력 가능
- 빌드 후 기존 사격/탄약 동작 이상 없음

**한계 및 향후 개선:**
- 현재는 WeaponInstance가 Weapon Actor에 완전히 연동되지 않음 (Pre-GAS 과도기)
- GAS 4단계에서 InventoryComponent + `GA_Item_PrimaryUse` 경로로 완전 이관 예정
- DB 저장 설계: `ItemId + InstanceId + CurrentAmmo + SchemaVersion` 포맷

---

## 참고

- `DOCS/Mine/Item.md` — 3계층 설계 전체
- Epic 학습 경로 Coder-05: Manage Item and Data
- `UPrimaryDataAsset`, `FTableRowBase` 공식 문서
