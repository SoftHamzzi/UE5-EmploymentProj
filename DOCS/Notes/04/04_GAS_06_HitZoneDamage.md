# 기획서: 부위별 대미지 — 태그 기반 전환

> 우선순위 6 — 데미지 파이프라인(GE_Damage) 완료 후 진행.
> `bIsWeakSpot`/`BoneDamageMultiplierMap` → `MaterialTags`/`TagDamageMultiplierMap`으로 전환.

---

## 1. 목표

- `UEPPhysicalMaterial::bIsWeakSpot` / `WeakSpotMultiplier` 제거
- `UEPPhysicalMaterial::MaterialTags` (FGameplayTagContainer) 활성화
- `UEPWeaponDefinition::BoneDamageMultiplierMap` (TMap<FName, float>) 제거
- `UEPWeaponDefinition::TagDamageMultiplierMap` (TMap<FGameplayTag, float>) 추가 + UPROPERTY
- 피격 시 PhysicalMaterial 태그 → 무기 TagDamageMultiplierMap 조회 → `SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Damage, FinalDamage)`로 GE_Damage 전달
- `GetBoneMultiplier` / `GetMaterialMultiplier` 함수 제거

완료 기준: 머리(HitZone.Head) 피격 시 기본 대비 2.5배, 팔다리(HitZone.Limbs) 피격 시 0.75배 적용. 태그 없는 부위는 1.0x 폴백.

---

## 2. 현재 코드 상태

```cpp
// UEPPhysicalMaterial
bool bIsWeakSpot = false;
float WeakSpotMultiplier = 2.0f;
// FGameplayTagContainer MaterialTags; ← 주석 처리됨

// UEPWeaponDefinition
TMap<FName, float> BoneDamageMultiplierMap; // UPROPERTY 없음

// UEPCombatComponent
float GetBoneMultiplier(const FName& BoneName) const;
static float GetMaterialMultiplier(const UPhysicalMaterial* PM);
```

### 구현 전 확인 사항 (이미 완료)

- **NativeGameplayTags**: `TAG_HitZone_Head/Chest/Limbs` 이미 `.h`/`.cpp` 모두 선언/정의됨.
  단, `.cpp`에서 `TAG_HitZone_Limb` / `"HitZone.Limb"` (단수) 로 되어 있어 `.h`의 `TAG_HitZone_Limbs` (복수)와 불일치 — **Step 0에서 먼저 수정 필요**.
- **`bReturnPhysicalMaterial = true`**: `EPServerSideRewindComponent.cpp` `ConfirmHitscan` 내 `FCollisionQueryParams`에 이미 설정됨.
- **`ApplyGEDamage` + `TAG_Data_Damage`**: `HandleHitscanFire`에서 이미 사용 중.

---

## 3. 변경 대상 파일

| 파일 | 작업 |
|------|------|
| `EPNativeGameplayTags.cpp` | `TAG_HitZone_Limb` → `TAG_HitZone_Limbs`, `"HitZone.Limb"` → `"HitZone.Limbs"` |
| `EPPhysicalMaterial.h` | bIsWeakSpot/WeakSpotMultiplier 제거, MaterialTags 주석 해제 + UPROPERTY |
| `EPWeaponDefinition.h` | BoneDamageMultiplierMap 제거, TagDamageMultiplierMap 추가 + UPROPERTY |
| `EPCombatComponent.h/cpp` | GetBoneMultiplier/GetMaterialMultiplier 제거, 태그 기반 배율 조회 추가 |

---

## 4. 구현 순서

### Step 0 — NativeGameplayTags Limbs 통일

`EPNativeGameplayTags.cpp`:

```cpp
// 기존
UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Limb, "HitZone.Limb")

// 변경 후
UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Limbs, "HitZone.Limbs")
```

### Step 1 — UEPPhysicalMaterial 수정

> ⚠️ `bIsWeakSpot`/`WeakSpotMultiplier` 제거 시 `GetMaterialMultiplier`에서 참조 오류 발생.
> **Step 3과 동시에 컴파일해야 함** — 따로 빌드하면 중간에 컴파일 불가.

```cpp
// EPPhysicalMaterial.h
#pragma once
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "GameplayTagContainer.h"
#include "EPPhysicalMaterial.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API UEPPhysicalMaterial : public UPhysicalMaterial
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category = "Damage")
    FGameplayTagContainer MaterialTags;
};
```

PhysicalMaterial 에셋 설정:

| 에셋 | MaterialTags |
|------|--------------|
| `PM_Head` | `HitZone.Head` |
| `PM_Chest` | `HitZone.Chest` |
| `PM_Limbs` | `HitZone.Limbs` |
| `PM_Default` | (비어있음 → 1.0x 폴백) |

### Step 2 — UEPWeaponDefinition 수정

```cpp
// EPWeaponDefinition.h

// 제거
TMap<FName, float> BoneDamageMultiplierMap;

// 추가
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
TMap<FGameplayTag, float> TagDamageMultiplierMap;
```

`DA_AK74` 에셋 설정:

| 태그 | 배율 |
|------|------|
| `HitZone.Head` | 2.5 |
| `HitZone.Chest` | 1.0 |
| `HitZone.Limbs` | 0.75 |

### Step 3 — CombatComponent 배율 조회 로직 교체

> ⚠️ Step 1과 동시 작업. `bIsWeakSpot` 제거 + `GetMaterialMultiplier` 제거를 한 번에 빌드.

`EPCombatComponent.h` — 변경:
- 제거: `float GetBoneMultiplier(const FName& BoneName) const;`
- 제거: `static float GetMaterialMultiplier(const UPhysicalMaterial* PM);`
- 추가: `static float GetTagDamageMultiplier(const UEPPhysicalMaterial* PM, const UEPWeaponDefinition* WeaponDef);`
- 전방 선언 추가: `class UEPWeaponDefinition;`

`EPCombatComponent.cpp`:

```cpp
float UEPCombatComponent::GetTagDamageMultiplier(
    const UEPPhysicalMaterial* PM,
    const UEPWeaponDefinition* WeaponDef)
{
    if (!PM || !WeaponDef) return 1.f;

    for (const FGameplayTag& Tag : PM->MaterialTags)
    {
        if (const float* Multiplier = WeaponDef->TagDamageMultiplierMap.Find(Tag))
            return *Multiplier;
    }
    return 1.f;
}
```

include 추가 (.cpp):
```cpp
#include "Data/EPWeaponDefinition.h"
#include "GameplayTagContainer.h"
```

기존 `GetBoneMultiplier`, `GetMaterialMultiplier` 구현부 제거.

### Step 4 — HandleHitscanFire 대미지 계산 교체

```cpp
// EPCombatComponent.cpp — HandleHitscanFire 내부 HitChar 블록

// 기존
const float BaseDamage = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
const float BoneMultiplier = GetBoneMultiplier(Hit.BoneName);
const float MaterialMultiplier = GetMaterialMultiplier(Hit.PhysMaterial.Get());
const float FinalDamage = BaseDamage * BoneMultiplier * MaterialMultiplier;

UE_LOG(LogTemp, Log, TEXT("[BoneHitbox] ..."));

ApplyGEDamage(Hit.GetActor(), Owner, GE_DamageClass, FinalDamage);

// 변경 후
const float BaseDamage = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
const UEPPhysicalMaterial* PM = Cast<UEPPhysicalMaterial>(Hit.PhysMaterial.Get());
const float FinalDamage = BaseDamage * GetTagDamageMultiplier(PM, EquippedWeapon->WeaponDef);

ApplyGEDamage(Hit.GetActor(), Owner, GE_DamageClass, FinalDamage);
```

UE_LOG는 필요 시 `PM->MaterialTags.ToStringSimple()` 등으로 대체하거나 제거.

---

## 5. 에셋 설정 가이드

### PhysicalMaterial 에셋 → MaterialTags

캐릭터 PhysAsset에서 각 본(Bone)에 PM 에셋 할당:
- `head` 본 → `PM_Head` (MaterialTags: HitZone.Head)
- `spine_04` 또는 `spine_02` 본 → `PM_Chest` (MaterialTags: HitZone.Chest)
  (`spine_03`은 HitBones 배열에 없으므로 `spine_04`/`spine_02` 중 선택)
- `lowerarm_l/r`, `thigh_l/r` 등 → `PM_Limbs` (MaterialTags: HitZone.Limbs)

### WeaponDefinition 에셋 → TagDamageMultiplierMap

무기별로 다른 배율 설정 가능:
- AK74: Head 2.5x, Limbs 0.75x
- 저격총: Head 4.0x, Limbs 0.6x
- 산탄총: Head 1.5x, Limbs 0.9x

---

## 6. 완료 체크리스트

### 코드 수정
- [ ] `EPNativeGameplayTags.cpp`: `TAG_HitZone_Limb` → `TAG_HitZone_Limbs`, `"HitZone.Limb"` → `"HitZone.Limbs"` (Step 0)
- [ ] `UEPPhysicalMaterial`: `bIsWeakSpot`/`WeakSpotMultiplier` 제거, `MaterialTags` UPROPERTY 추가
- [ ] `UEPWeaponDefinition`: `BoneDamageMultiplierMap` 제거, `TagDamageMultiplierMap` UPROPERTY 추가
- [ ] `EPCombatComponent.h`: `GetBoneMultiplier`/`GetMaterialMultiplier` 제거, `GetTagDamageMultiplier` 추가, `UEPWeaponDefinition` 전방 선언
- [ ] `EPCombatComponent.cpp`: 함수 구현 교체 (Step 1+3+4 동시 빌드)
- [x] `FCollisionQueryParams::bReturnPhysicalMaterial = true` — 이미 설정됨
- [x] SetByCaller `TAG_Data_Damage` 사용 — 이미 완료
- [x] NativeGameplayTags `TAG_HitZone_*` 선언/정의 — 이미 완료 (Step 0에서 Limbs 오타만 수정)

### 에디터
- [ ] `MaterialTags` UPROPERTY 에디터 노출 확인 (PhysicalMaterial 에셋 열어서 확인)
- [ ] `TagDamageMultiplierMap` GameplayTag 키 입력 가능 확인
- [ ] PM_Head/Chest/Limbs 에셋에 태그 설정
- [ ] PhysAsset 본에 PM 에셋 할당

### 검증
- [ ] `PM_Head`에 `HitZone.Head` → 피격 시 2.5배 적용 확인
- [ ] 태그 없는 부위 → 1.0x 폴백 확인

---

## 7. 함정 & 주의사항

| 상황 | 원인 | 해결 |
|------|------|------|
| Step 1 후 컴파일 오류 | `bIsWeakSpot` 제거 → `GetMaterialMultiplier` 참조 실패 | Step 1 + Step 3 동시 작업 |
| PhysicalMaterial 항상 null | PhysAsset 본에 PM 에셋 미할당 | PhysAsset에서 각 본 → Simple Collision → Physical Material 확인 |
| 태그 키 에디터에서 안 보임 | GameplayTags 모듈 미로드 | Build.cs `GameplayTags` 모듈 및 태그 등록 확인 |
| SetByCaller 런타임 오류 | GE_Damage에 Data.Damage SetByCaller Modifier 미정의 | GE Blueprint의 Modifier 설정 확인 |
| 복수 태그 매칭 (Head + Armor) | 첫 번째 매칭만 반환 | 현재는 단일 태그만 부여하므로 문제 없음 |
