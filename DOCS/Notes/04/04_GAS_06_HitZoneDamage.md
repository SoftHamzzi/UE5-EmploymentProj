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

---

## 3. 변경 대상 파일

| 파일 | 작업 |
|------|------|
| `EPPhysicalMaterial.h` | bIsWeakSpot/WeakSpotMultiplier 제거, MaterialTags 주석 해제 + UPROPERTY |
| `EPWeaponDefinition.h` | BoneDamageMultiplierMap 제거, TagDamageMultiplierMap 추가 + UPROPERTY |
| `EPCombatComponent.h/cpp` | GetBoneMultiplier/GetMaterialMultiplier 제거, 태그 기반 배율 조회 추가 |

---

## 4. 구현 순서

### Step 1 — UEPPhysicalMaterial 수정

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
    // 이 PhysicalMaterial이 나타내는 부위 태그
    // 예: HitZone.Head, HitZone.Chest, HitZone.Limbs
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
// SetByCaller는 GameplayTag 버전 사용 권장 (FName은 오타 위험)
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

```cpp
// EPCombatComponent.h — 제거
float GetBoneMultiplier(const FName& BoneName) const;
static float GetMaterialMultiplier(const UPhysicalMaterial* PM);

// EPCombatComponent.h — 추가
static float GetTagDamageMultiplier(
    const UEPPhysicalMaterial* PM,
    const UEPWeaponDefinition* WeaponDef);
```

```cpp
// EPCombatComponent.cpp
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
    return 1.f; // 태그 없는 부위 폴백
}
```

### Step 4 — HandleHitscanFire 대미지 계산 교체

```cpp
// EPCombatComponent.cpp — HandleHitscanFire 내부

// 기존
const float BoneMult     = GetBoneMultiplier(HitResult.BoneName);
const float MaterialMult = GetMaterialMultiplier(HitResult.PhysMaterial.Get());
const float FinalDamage  = BaseDamage * BoneMult * MaterialMult;
UGameplayStatics::ApplyPointDamage(...);

// 변경 후
const UEPPhysicalMaterial* PM =
    Cast<UEPPhysicalMaterial>(HitResult.PhysMaterial.Get());
const float Multiplier  = GetTagDamageMultiplier(PM, Weapon->WeaponDef);
const float FinalDamage = BaseDamage * Multiplier;
ApplyGEDamage(HitChar, OwnerChar, GE_DamageClass, FinalDamage);
```

> **PhysicalMaterial 반환 조건**: 트레이스 쿼리 파라미터에 `bReturnPhysicalMaterial = true` 필수.
> SSR `ConfirmHitscan`의 `FCollisionQueryParams` 확인 필요.

### Step 5 — SetByCaller 태그 통일

`Data.Damage` 태그는 DamagePipeline 문서에서 이미 정의됨.
SetByCaller는 항상 `GameplayTag` 버전 사용:

```cpp
// 권장 (오타 방지)
Spec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Damage, FinalDamage);

// 비권장
Spec.Data->SetSetByCallerMagnitude(FName("Data.Damage"), FinalDamage);
```

---

## 5. 에셋 설정 가이드

### PhysicalMaterial 에셋 → MaterialTags

캐릭터 PhysAsset에서 각 본(Bone)에 PM 에셋 할당:
- `head` 본 → `PM_Head` (MaterialTags: HitZone.Head)
- `spine_03` 본 → `PM_Chest` (MaterialTags: HitZone.Chest)
- `lowerarm_l`, `thigh_l` 등 → `PM_Limbs` (MaterialTags: HitZone.Limbs)

### WeaponDefinition 에셋 → TagDamageMultiplierMap

무기별로 다른 배율 설정 가능:
- AK74: Head 2.5x, Limb 0.75x
- 저격총: Head 4.0x, Limb 0.6x
- 산탄총: Head 1.5x, Limb 0.9x

---

## 6. 완료 체크리스트

- [ ] `UEPPhysicalMaterial::bIsWeakSpot` / `WeakSpotMultiplier` 제거 후 컴파일
- [ ] `MaterialTags` UPROPERTY 추가 및 에디터 노출 확인
- [ ] `BoneDamageMultiplierMap` 제거 후 컴파일
- [ ] `TagDamageMultiplierMap` 에디터에서 GameplayTag 키 입력 가능 확인
- [ ] `GetBoneMultiplier` / `GetMaterialMultiplier` 제거
- [ ] `FCollisionQueryParams::bReturnPhysicalMaterial = true` 확인
- [ ] `PM_Head`에 `HitZone.Head` 태그 → 피격 시 2.5배 적용 확인
- [ ] 태그 없는 부위 → 1.0x 폴백 확인
- [ ] SetByCaller `GameplayTag` 버전 사용 확인

---

## 7. 함정 & 주의사항

| 상황 | 원인 | 해결 |
|------|------|------|
| PhysicalMaterial 항상 null | bReturnPhysicalMaterial 미설정 | FCollisionQueryParams에 true 설정 |
| 태그 키 에디터에서 안 보임 | GameplayTags 모듈 미로드 | Build.cs GameplayTags 모듈 및 태그 등록 확인 |
| SetByCaller 런타임 오류 | GE_Damage에 Data.Damage SetByCaller Modifier 미정의 | GE Blueprint의 Modifier 설정 확인 |
| 복수 태그 매칭 (Head + Armor) | 첫 번째 매칭만 반환 | 현재는 단일 태그만 부여하므로 문제 없음. 추후 설계 시 Max/곱 방식 선택 |
