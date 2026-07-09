# 04_GAS_04_Reload — 구현 상태

**전체 상태: 완료 (PIE 검수 완료)**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷. 문서(04_GAS_04_Reload.md)의 예정 코드와 혼동 금지.

---

## Step 1 — AttributeSet Ammo/MaxAmmo

**상태: 완료**

### 완료된 것
- `EPAttributeSet.h`: `Ammo`, `MaxAmmo` Attribute + `ATTRIBUTE_ACCESSORS` 추가
- `EPAttributeSet.h`: `OnRep_Ammo`, `OnRep_MaxAmmo` 선언
- `EPAttributeSet.cpp`: `OnRep_Ammo`, `OnRep_MaxAmmo` 구현
- `EPAttributeSet.cpp`: `PreAttributeChange` — Ammo `[0, MaxAmmo]` 클램핑, MaxAmmo `FMath::Max(1.f)` 클램핑
- `EPAttributeSet.cpp`: `PostGameplayEffectExecute` — Ammo/MaxAmmo 변경 시 재클램핑 로직 추가
- `EPAttributeSet.cpp`: `GetLifetimeReplicatedProps` — `MaxAmmo` DOREPLIFETIME 정상 등록 (MaxHealth 중복 버그 수정됨)
- `EPAttributeSet.cpp` 30번 줄: `GetAmmo()` 사용 정상 확인 (이전 버그 `GetMaxAmmo()` → 수정됨)
- `UEPCombatComponent::EquipWeapon`: 서버 측 `InitAmmo` / `InitMaxAmmo` 호출 완료

---

## Step 2 — GE_Reloading Blueprint 에셋

**상태: 완료**

### 완료된 것
- `Content/Blueprints/GE/GE_Reloading.uasset` 생성 확인
- PIE에서 장전 동작 확인 → GE 설정 정상

---

## Step 3 — GE_Reload_Ammo Blueprint 에셋

**상태: 완료**

### 완료된 것
- `Content/Blueprints/GE/GE_Reload_Ammo.uasset` 생성 확인
- PIE에서 장전 후 탄약 회복 확인 → GE 설정 정상

---

## Step 4 — GA_Item_Reload C++ 구현

**상태: 완료**

### 완료된 것
- `EPGA_Item_Reload.h`: 헤더 완성 (`GE_ReloadingClass`, `GE_ReloadAmmoClass`, `ReloadingEffectHandle`, `OnReloadComplete_Task`)
- `EPGA_Item_Reload.cpp`: 생성자 — `NetExecutionPolicy`, `InstancingPolicy`, `ActivationBlockedTags` 설정
- `EPGA_Item_Reload.cpp`: `ActivateAbility` — CommitAbility, `GE_Reloading` 적용, `UAbilityTask_WaitDelay` 생성/바인딩/`ReadyForActivation` 호출
- `EPGA_Item_Reload.cpp`: `EndAbility` — `ReloadingEffectHandle` 유효 시 강제 제거 후 Super 호출
- `EPGA_Item_Reload.cpp`: `OnReloadComplete_Task` — 서버 권한 확인 후 `GE_ReloadAmmoClass` 적용, `EndAbility` 호출

### 완료 추가
- GA Blueprint에서 `GE_ReloadingClass` / `GE_ReloadAmmoClass` 슬롯 연결 확인 (PIE 장전 동작으로 검증)

### 발견된 버그 (수정 완료)
- `BP_GA_Item_Reload`의 `CooldownGameplayEffectClass`에 `GE_FireCooldown`이 잘못 연결되어 있었음
  → 장전 시 `Cooldown.Weapon.PrimaryUse` 태그가 부여되는 부작용 발생 → None으로 수정

---

## Step 5 — EquipWeapon 배열 패턴 + AEPWeapon 기존 코드 제거

**상태: 완료**

### 완료된 것
- `EPWeapon.h`: `StartReload`, `FinishReload`, `ReloadTimerHandle`, `WeaponState`, `CurrentAmmo` (UPROPERTY), `MaxAmmo` (UPROPERTY) 제거 확인
- `EPWeapon.cpp`: `#include "TimerManager.h"` 제거 확인
- `EPWeapon.cpp`: BeginPlay MaxAmmo/CurrentAmmo 초기화 블록 제거 확인
- `EPWeapon.cpp`: Tick FireInterval/WeaponState 블록 제거 확인
- `EPWeapon.cpp`: CanFire WeaponState/CurrentAmmo 체크 제거 확인 (`if (!WeaponDef) return false;`만 존재)
- `EPCombatComponent.cpp` `HandleServerFire`: `if (!EquippedWeapon->CanFire()) return;` 제거
  → GAS `CommitAbility`가 코스트(Ammo -1)를 먼저 소모하므로, 그 후 `CanFire()` 재체크 시 마지막 탄환이 항상 차단되는 버그였음
- `EPWeapon.cpp`: Fire CurrentAmmo 차감/WeaponState=Firing/StartReload 호출 제거 확인
- `EPWeapon.cpp`: `GetLifetimeReplicatedProps` `DOREPLIFETIME CurrentAmmo` 제거 확인
- `EPCombatComponent.h`: `Server_Reload` RPC 선언 제거 확인
- `EPCombatComponent.cpp`: `Server_Reload_Implementation` 구현부 제거 확인
- `EPCombatComponent.cpp`: `EquipWeapon` — `NewWeapon->WeaponDef->WeaponAbilities` 올바른 경로 사용 확인

---

## Step 6 — 입력 연동

**상태: 완료**

### 완료된 것
- `Content/Characters/InputActions/IA_Reload.uasset` 생성 확인
- `IMC_DefaultMappingContext.uasset` 수정 확인 (R 키 매핑)
- `EPCharacter.cpp` `Input_Reload`: `ASC->TryActivateAbilitiesByTag(TAG_Ability_Item_Reload)` 호출 확인
- `EPCharacter.cpp` `SetupPlayerInputComponent`: `PC->GetReloadAction()` 바인딩 확인
