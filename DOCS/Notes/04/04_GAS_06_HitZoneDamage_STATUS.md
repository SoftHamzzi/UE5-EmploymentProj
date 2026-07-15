# 04_GAS_06_HitZoneDamage — 구현 상태

**전체 상태: 완료 (PIE 검수 완료)**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷.
> `bIsWeakSpot`/`BoneDamageMultiplierMap` → `MaterialTags`/`TagDamageMultiplierMap` 전환 완료.

---

## Step 0 — NativeGameplayTags Limbs 통일 (완료)

`EPNativeGameplayTags.cpp`:
```cpp
UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Head,  "HitZone.Head")
UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Chest, "HitZone.Chest")
UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Limbs, "HitZone.Limbs")
```
`EPNativeGameplayTags.h` 선언도 `TAG_HitZone_Limbs` (복수)로 통일됨.

---

## Step 1 — UEPPhysicalMaterial 수정 (완료)

`EPPhysicalMaterial.h`:
- `bIsWeakSpot`, `WeakSpotMultiplier` 제거
- `MaterialTags` (FGameplayTagContainer) UPROPERTY 추가
- `GameplayTagContainer.h` include 추가

```cpp
UPROPERTY(EditDefaultsOnly, Category="Damage")
FGameplayTagContainer MaterialTags;
```

---

## Step 2 — UEPWeaponDefinition 수정 (완료)

`EPWeaponDefinition.h`:
- `BoneDamageMultiplierMap` (TMap<FName, float>, UPROPERTY 없음) 제거
- `TagDamageMultiplierMap` (TMap<FGameplayTag, float>) UPROPERTY 추가
- `GameplayTagContainer.h` include 추가

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
TMap<FGameplayTag, float> TagDamageMultiplierMap;
```

---

## Step 3 — CombatComponent 배율 조회 로직 교체 (완료)

`EPCombatComponent.h`:
- `GetBoneMultiplier`, `GetMaterialMultiplier` 제거
- `GetTagDamageMultiplier` 추가
- `class UEPWeaponDefinition;` 전방 선언 추가

```cpp
static float GetTagDamageMultiplier(const UEPPhysicalMaterial* PM, const UEPWeaponDefinition* WeaponDef);
```

`EPCombatComponent.cpp` 구현:
```cpp
float UEPCombatComponent::GetTagDamageMultiplier(
    const UEPPhysicalMaterial* PM, const UEPWeaponDefinition* WeaponDef)
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

---

## Step 4 — HandleHitscanFire 대미지 계산 교체 (완료)

```cpp
if (AEPCharacter* HitChar = Cast<AEPCharacter>(Hit.GetActor()))
{
    const float BaseDamage = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
    const UEPPhysicalMaterial* PM = Cast<UEPPhysicalMaterial>(Hit.PhysMaterial.Get());
    const float Multiplier = GetTagDamageMultiplier(PM, EquippedWeapon->WeaponDef);
    const float FinalDamage = BaseDamage * Multiplier;

    ApplyGEDamage(Hit.GetActor(), Owner, GE_DamageClass, FinalDamage);
}
```

---

## 에디터 작업 (완료)

- `PM_Head` → MaterialTags: `HitZone.Head`
- `PM_Chest` → MaterialTags: `HitZone.Chest`
- `PM_Limbs` → MaterialTags: `HitZone.Limbs`
- PhysAsset 각 본에 PM 할당:
  - `head`, `neck_01` → PM_Head
  - `pelvis`, `spine_02`, `spine_04` → PM_Chest
  - `clavicle_l/r` → PM_Chest
  - `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r` → PM_Limbs
  - `thigh_l/r`, `calf_l/r`, `foot_l/r` → PM_Limbs
- `DA_AK74` → TagDamageMultiplierMap: `HitZone.Head`=2.5, `HitZone.Limbs`=0.75

---

## 주의사항

- PhysicalMaterial은 PhysicsAsset 바디(콜리전 셰이프)별 독립 할당. 본 계층 상속 없음 — 각 본마다 개별 지정 필수.
- `GetTagDamageMultiplier`는 첫 번째 매칭 태그만 반환. 현재 본당 태그 1개 구조라 문제 없음.
- `bReturnPhysicalMaterial = true`: `EPServerSideRewindComponent.cpp` `ConfirmHitscan`에 이미 설정됨.
