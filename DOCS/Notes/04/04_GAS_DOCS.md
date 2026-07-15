# GAS 총괄 기획서 (EmploymentProj)

> `feature-gas` 브랜치의 GAS 도입 전체 그림을 담은 마스터 문서.
> GAS 개념 참조: `04_GAS_00_Reference.md` / 세부 구현: `04_GAS_01_` ~ `04_GAS_06_`

---

## 1. 도입 배경

### 현재 구조의 문제점

| 문제 | 현재 코드 | 영향 |
|------|-----------|------|
| 상태 분산 | `WeaponState` enum, `HP` UPROPERTY, `LastServerFireTime` 각자 관리 | 상태 충돌, 중복 검증 |
| 복제 한계 | `State.Reloading` 없음 → 시뮬레이티드 프록시에서 재장전 상태 쿼리 불가 | 다른 클라이언트에서 발사 차단 미적용 |
| 확장 비용 | 새 무기/스킬 추가 시 Character/CombatComponent 직접 수정 필요 | 코드 집중, 사이드이펙트 위험 |
| 수치 관리 | 탄약, HP, 쿨타임 각각 UPROPERTY로 직접 복제 | 일관성 없는 복제 전략 |

### 이관 완료 시 달라지는 것

| 현재 | 이후 |
|------|------|
| `Server_Fire` RPC | `GA_Item_PrimaryUse` (LocalPredicted) |
| `TakeDamage()` + `HP` UPROPERTY | `GE_Damage` + `UEPAttributeSet::Health` |
| `StartReload/FinishReload` + Timer | `GA_Item_Reload` + `GE_Reloading` Duration GE |
| `WeaponState` enum | GameplayTag (`State.Reloading`) |
| `LastServerFireTime` | `GE_FireCooldown` Duration GE |
| `bIsWeakSpot` / `WeakSpotMultiplier` | `HitZone.*` 태그 + `TagDamageMultiplierMap` |

---

## 2. 아키텍처 결정사항

### ASC 배치: PlayerState

Character가 아닌 **PlayerState**에 ASC를 둔다.

**이유:** 사망/리스폰 시 ASC 상태(Ability Grant, GE, Attribute) 보존. PlayerState에 이미 `Kills`, `Money` 등 영속 데이터가 있어 자연스러운 확장 위치.

**Replication Mode: Mixed** — 소유 클라이언트에는 GE 전체 복제, 타 클라이언트에는 Tag/Cue만 복제. 멀티플레이어 슈터 표준.

> Mixed 모드 요구사항: PlayerState의 Owner가 Controller여야 함. UE 4.24+에서 `PossessedBy` 시 자동 처리.

### Character ASC 캐싱

매 호출마다 PlayerState를 거치지 않도록 ASC를 Character에 `TObjectPtr`로 캐싱한다.

```
PossessedBy (서버)
    → AbilitySystemComponent = PS->GetAbilitySystemComponent()
    → InitAbilityActorInfo(PS, this)
    → Attribute 초기화 (InitHealth, InitMaxHealth)
    → SetTagMapCount(EmpGameplayTags::TAG_State_Dead, 0)  ← 리스폰 시 잔류 State.Dead 클리어

OnRep_PlayerState (클라)
    → AbilitySystemComponent = PS->GetAbilitySystemComponent()
    → InitAbilityActorInfo(PS, this)
```

> `InitAbilityActorInfo`는 서버/클라 양쪽에서 호출해야 한다. 클라 누락 시 LocalPredicted 어빌리티 활성화 실패.

### ActivationOwnedTags는 복제되지 않는다

`ActivationOwnedTags`는 소유 클라이언트 + 서버에서만 보인다. 다른 클라이언트에서 상태를 쿼리해야 한다면 반드시 GE `GrantedTags`로 부여해야 한다.

| 상태 | 방법 | 이유 |
|------|------|------|
| `State.Reloading` | `GE_Reloading` (HasDuration GE, GrantedTags) | 다른 클라에서 GA_PrimaryUse 차단에 필요 |
| `State.Dead` | `GE_State_Dead` (Infinite GE, GrantedTags) | 다른 클라에서 IsDead 쿼리에 필요 |

### bServerRespectsRemoteAbilityCancellation = false

기본값(`true`)이면 클라이언트가 먼저 `EndAbility`를 호출할 때 서버 GA도 강제 종료된다. `LocalPredicted` 어빌리티에서 클라가 먼저 완료해도 서버 로직이 계속 실행되어야 하므로 모든 `InstancedPerActor` GA에서 `false`로 설정한다.

### 입력 추상화

입력 핸들러에서 CombatComponent를 직접 호출하지 않는다.

```cpp
// Before
void AEPCharacter::Input_Fire(...) { CombatComponent->RequestFire(...); }

// After
void AEPCharacter::Input_Fire(...)
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Item_PrimaryUse));
}
```

키 바인딩이 바뀌어도 Character 코드 수정 없음. 무기 종류가 달라도 같은 입력 파이프라인으로 동작.

---

## 3. 어트리뷰트 설계

### 현재 구현 범위 (Foundation ~ Reload)

| Attribute | 복제 | 추가 단계 | 설명 |
|-----------|------|-----------|------|
| `Health` | ✓ | 01 | 현재 HP |
| `MaxHealth` | ✓ | 01 | 최대 HP (이 프로젝트는 MaxHealth 버프 없음 — 단순 클램핑만) |
| `IncomingDamage` | ✗ | 01 | 메타 Attribute. GE_Damage가 Add → PostGEExecute에서 Health로 변환 후 0 초기화 |
| `Ammo` | ✓ | 04 | 현재 탄약 |
| `MaxAmmo` | ✓ | 04 | 최대 탄약 |

### 향후 확장 (스킬 단계에서 추가)

| Attribute | 설명 | 단계 |
|-----------|------|------|
| `Stamina` / `MaxStamina` | Dash 스킬 코스트 GE에서 차감 | 07 Skills |
| `Shield` / `MaxShield` | ShieldOn 스킬이 부여. 데미지 선차감 후 Health 차감 | 07 Skills |
| `ArmorHead` / `ArmorChest` / `ArmorLimbs` | 부위별 방어력 (타르코프식) — 후순위 | 미정 |

> **Shield 처리 방향:** `PostGameplayEffectExecute`에서 `IncomingDamage` 수신 시 Shield 잔량 먼저 차감, 초과분만 Health 차감.
> **부위별 방어력 설계 방향:** GE Context에 HitZone 태그를 실어 전달 → Armor 어트리뷰트 조회 → `IncomingDamage` 감산. `TagDamageMultiplierMap`(무기 배율)과 Armor(방어력)는 독립 계층 적용.

---

## 4. 태그 체계

모든 태그는 `Public/GAS/EPNativeGameplayTags.h`의 `namespace EmpGameplayTags {}` 안에 선언.

```cpp
// 사용 방식 (코드 내)
EmpGameplayTags::TAG_State_Dead
```

### 태그 목록

**구현 완료 (01~06)**

| 태그 | 상수명 | 타입 | 용도 |
|------|--------|------|------|
| `State.Dead` | `TAG_State_Dead` | State | 사망 상태. GE_State_Dead (Infinite)로 복제 |
| `State.Reloading` | `TAG_State_Reloading` | State | 재장전 중. GE_Reloading (Duration)으로 복제 |
| `State.UsingItem` | `TAG_State_UsingItem` | State | 아이템 사용 중 |
| `Event.Death` | `TAG_Event_Death` | Event | 사망 트리거. GA_Death AbilityTrigger가 수신. 순간 신호 |
| `Ability.Item.PrimaryUse` | `TAG_Ability_Item_PrimaryUse` | Ability | 발사 GA 식별 |
| `Ability.Item.Reload` | `TAG_Ability_Item_Reload` | Ability | 재장전 GA 식별 |
| `Cooldown.Weapon.PrimaryUse` | `TAG_Cooldown_Weapon_PrimaryUse` | Cooldown | GE_FireCooldown GrantedTags |
| `Data.Damage` | `TAG_Data_Damage` | Data | SetByCaller 데미지 수치 |
| `Data.Cooldown` | `TAG_Data_Cooldown` | Data | SetByCaller 쿨타임 Duration |
| `Data.ReloadDuration` | `TAG_Data_ReloadDuration` | Data | SetByCaller 재장전 Duration |
| `HitZone.Head` | `TAG_HitZone_Head` | HitZone | 머리 부위. PM_Head PhysicalMaterial |
| `HitZone.Chest` | `TAG_HitZone_Chest` | HitZone | 몸통 부위 |
| `HitZone.Limbs` | `TAG_HitZone_Limbs` | HitZone | 팔/다리 부위 |

**추가 예정 (07 Skills)**

| 태그 | 상수명 | 타입 | 용도 |
|------|--------|------|------|
| `Ability.Skill.Dash` | `TAG_Ability_Skill_Dash` | Ability | 대시 GA 식별 |
| `Ability.Skill.Heal` | `TAG_Ability_Skill_Heal` | Ability | 자가 힐 GA 식별 |
| `Ability.Skill.Shield` | `TAG_Ability_Skill_Shield` | Ability | 실드 GA 식별 |
| `State.Dashing` | `TAG_State_Dashing` | State | 대시 중 (ActivationBlockedTags 용도) |
| `State.Shielded` | `TAG_State_Shielded` | State | 실드 활성 중. GE_ShieldOn GrantedTags. PostGEExecute에서 피해 50% 감소 체크 |
| `State.Healing` | `TAG_State_Healing` | State | 힐 채널링 중. GE_Healing GrantedTags |
| `Cooldown.Skill.Dash` | `TAG_Cooldown_Skill_Dash` | Cooldown | 대시 쿨타임 |
| `Cooldown.Skill.Heal` | `TAG_Cooldown_Skill_Heal` | Cooldown | 힐 쿨타임 (성공 시만 부여) |
| `Cooldown.Skill.Shield` | `TAG_Cooldown_Skill_Shield` | Cooldown | 실드 쿨타임 |
| `Data.HealAmount` | `TAG_Data_HealAmount` | Data | SetByCaller 힐 수치 (HP 30) |
| `Event.Damaged` | `TAG_Event_Damaged` | Event | 피해 수신 시 발송. 힐 채널링 취소 트리거 |

> **Data 태그**: GE Modifier에 DataTag로 등록하고, Spec에 `SetSetByCallerMagnitude`로 값 주입. 태그가 GE에 등록되지 않으면 런타임 오류.
> **Event 태그**: `ASC->HandleGameplayEvent(Tag, &Payload)` 직접 호출. `UAbilitySystemBlueprintLibrary::SendGameplayEventToActor` Blueprint wrapper 사용 금지.

---

## 5. Blueprint GE 에셋 목록

GA는 C++ 클래스. GE는 Blueprint 에셋으로 생성. `Content/Data/GAS/`에 배치.

**구현 완료 (01~06)**

| 에셋명 | Duration | 주요 설정 |
|--------|----------|-----------|
| `GE_Damage` | Instant | Modifier: `EPAttributeSet.IncomingDamage`, Add, SetByCaller(`Data.Damage`) |
| `GE_State_Dead` | Infinite | GrantedTags: `State.Dead` |
| `GE_FireCooldown` | HasDuration | DurationMagnitude: SetByCaller(`Data.Cooldown`), GrantedTags: `Cooldown.Weapon.PrimaryUse` |
| `GE_Reloading` | HasDuration | DurationMagnitude: SetByCaller(`Data.ReloadDuration`), GrantedTags: `State.Reloading` |
| `GE_Reload_Ammo` | Instant | Modifier: `EPAttributeSet.Ammo`, Override, AttributeBased(MaxAmmo, Source, bSnapshot=false) |

**추가 예정 (07 Skills)**

| 에셋명 | Duration | 주요 설정 |
|--------|----------|-----------|
| `GE_Dash_Cost` | Instant | Modifier: `EPAttributeSet.Stamina`, Add, `-30` |
| `GE_Dash_Cooldown` | HasDuration | GrantedTags: `Cooldown.Skill.Dash` (10s) |
| `GE_StaminaRegen` | Infinite | Period: 0.5s, Modifier: `EPAttributeSet.Stamina`, Add, `+5` |
| `GE_Healing` | HasDuration | GrantedTags: `State.Healing` (3s, 채널링 상태) |
| `GE_Heal` | Instant | Modifier: `EPAttributeSet.Health`, Add, SetByCaller(`Data.HealAmount`) |
| `GE_Heal_Cooldown` | HasDuration | GrantedTags: `Cooldown.Skill.Heal` (20s, 성공 시만) |
| `GE_ShieldOn` | HasDuration | GrantedTags: `State.Shielded` (5s). **Modifier 없음** — 피해 감소는 PostGEExecute 태그 체크로 처리 |
| `GE_Shield_Cooldown` | HasDuration | GrantedTags: `Cooldown.Skill.Shield` (30s) |

---

## 6. 구현 로드맵

```
[01 Foundation] ✅
      ↓
[02 DamagePipeline] ✅
      ↓
[03 PrimaryUse] ✅ ──→ [05 Spread] ⬜
      ↓
[04 Reload] ✅
      ↓
[05 WeaponDecals] ✅
      ↓
[06 HitZoneDamage] ✅
      ↓
[07 Skills: Dash / Heal / ShieldOn] ⬜
      ↓
[08 Overwatch HUD] ⬜
```

| # | 파일 | 상태 | 완료 기준 요약 |
|---|------|------|----------------|
| 01 | `04_GAS_01_Foundation.md` | ✅ | PIE 2인: 양쪽 ASC null 없음, `Health = 100` 복제 확인 |
| 02 | `04_GAS_02_DamagePipeline.md` | ✅ | 피격 → Health 감소 → 0 도달 → `State.Dead` 복제 → 래그돌 |
| 03 | `04_GAS_03_PrimaryUse.md` | ✅ | 발사 GA 활성화 → FireRate 쿨타임 GE 동작 → 히트 판정 정상 |
| 04 | `04_GAS_04_Reload.md` | ✅ | 재장전 GA → `State.Reloading` 복제 → 발사 차단 → 탄약 보충 |
| 05 | `04_GAS_05_WeaponDecals.md` | ✅ | 벽 사격 → 탄흔 데칼 생성. 샷건 다중 탄흔 확인 |
| 05 | `04_GAS_05_Spread.md` | ✅ | 중심 집중 커브로 산탄총 PelletCount=5 발사 시 중심 밀집 확인 |
| 06 | `04_GAS_06_HitZoneDamage.md` | ✅ | `HitZone.Head` 피격 → 2.5배. 태그 없는 부위 → 1.0x 폴백 |
| 07 | `04_GAS_07_Skills.md` (예정) | ⬜ | Dash/Heal/ShieldOn 각 GA 활성화, 쿨타임 GE 동작, Stamina/Shield 어트리뷰트 복제 |
| 08 | `04_GAS_08_HUD.md` (예정) | ⬜ | 체력바/탄약/스킬 쿨타임 UI가 GAS Tag/Attribute 변화에 실시간 반응 |

---

## 7. 이관 후 제거 대상

> ✅ 제거 완료 / ⬜ 미착수

| 제거 대상 | 단계 | 상태 | 대체 |
|-----------|------|------|------|
| `AEPCharacter::HP` / `MaxHP` UPROPERTY | 02 | ✅ | `UEPAttributeSet::Health` |
| `AEPCharacter::TakeDamage()` | 02 | ✅ | `PostGameplayEffectExecute` |
| `AEPCharacter::Die()` 직접 호출 경로 | 02 | ✅ | `GA_Death` 내부에서 `Multicast_Die()` 호출 |
| `AEPCharacter::OnRep_HP()` | 02 | ✅ | Attribute 변경 델리게이트 |
| `UEPCombatComponent::Server_Fire` RPC | 03 | ✅ | `GA_Item_PrimaryUse` |
| `UEPCombatComponent::LastServerFireTime` | 03 | ✅ | `GE_FireCooldown` Duration GE |
| `UEPCombatComponent::Server_Reload` RPC | 04 | ✅ | `GA_Item_Reload` |
| `AEPWeapon::StartReload` / `FinishReload` / `ReloadTimerHandle` | 04 | ✅ | `GA_Item_Reload` + `WaitDelay` AbilityTask |
| `AEPWeapon::CurrentAmmo` / `MaxAmmo` / `OnRep_CurrentAmmo` | 04 | ✅ | `AttributeSet::Ammo` / `MaxAmmo` |
| `AEPWeapon::WeaponState` (EEPWeaponState enum) | 04 | ✅ | `State.Reloading` 등 GameplayTag |
| `AEPWeapon::CanFire()` | 03 | ✅ | `ActivationBlockedTags` + Cooldown GE |
| `UEPPhysicalMaterial::bIsWeakSpot` / `WeakSpotMultiplier` | 06 | ✅ | `MaterialTags` (FGameplayTagContainer) |
| `UEPWeaponDefinition::BoneDamageMultiplierMap` | 06 | ✅ | `TagDamageMultiplierMap` (TMap\<FGameplayTag, float\>) |
| `UEPCombatComponent::GetBoneMultiplier` / `GetMaterialMultiplier` | 06 | ✅ | `GetTagDamageMultiplier` |

---

## 8. 유지할 것

| 유지 대상 | 이유 |
|-----------|------|
| `UEPServerSideRewindComponent` | 호출 위치만 CombatComponent → GA로 이동. 구조 변경 없음 |
| `UEPCombatComponent` | `EquipWeapon/UnequipWeapon`, 코스메틱 헬퍼 함수들 유지 |
| `UEPCombatComponent::HandleHitscanFire` / `HandleProjectileFire` | GA 내부에서 호출하는 서버 헬퍼로 유지 |
| `EEPBallisticType` switch 구조 | `GA_Item_PrimaryUse` 내부로 이동 |
| `AEPWeapon::Fire()` | 스프레드 계산 (05에서 CDF 방식으로 개선) |

---

## 9. 테스트 기준 (PIE 멀티플레이어 기준)

| 시나리오 | 확인 항목 |
|----------|-----------|
| 연사 | `GE_FireCooldown`으로 FireRate 제한 동작. `showdebug abilitysystem`에서 쿨타임 GE 확인 |
| 재장전 중 발사 시도 | `State.Reloading` 태그가 시뮬레이티드 프록시에서도 보임 → `GA_PrimaryUse` 활성화 차단 |
| 피격 → 사망 | `Health` Attribute 0 도달 → `State.Dead` 복제 → `Multicast_Die` 실행 → 래그돌 |
| 연속 피격 중 사망 | 이미 `State.Dead`인 대상에게 `GA_Death` 중복 활성화 안 됨 |
| 무기 교체 | `ClearAbility` 후 이전 무기 GA 잔류 없음. 교체 후 정상 발사/재장전 |
| 서버 권한 검증 | 클라이언트 비정상 연사 → 서버 Cooldown GE로 차단 |
| 예측 롤백 | `LocalPredicted` GA 서버 거부 시 클라이언트 롤백. 탄약/UI 불일치 없음 |
| 헤드샷 | `HitZone.Head` 태그 → 기본 대비 2.5배 데미지 |
| 태그 없는 부위 | `TagDamageMultiplierMap` 미매칭 → 1.0x 폴백 |
| 스프레드 분포 | 중심 집중 커브 → 펠릿이 중심부에 밀집 (DebugDraw 확인) |

---

## 10. 설계 원칙

**Do:**
- 입력은 `ASC->TryActivateAbilitiesByTag()` 단일 경로
- 복제 필요 상태(`State.Reloading`, `State.Dead`)는 반드시 GE `GrantedTags`로 부여
- SetByCaller는 **GameplayTag 버전** (`FName` 버전 금지 — 런타임 오타 위험, 에디터 자동완성 없음)
- GA 내부 이벤트 발송: `GetOwningAbilitySystemComponent()->HandleGameplayEvent()`
- GA 외부에서 ASC 획득: `Cast<IAbilitySystemInterface>(Actor)->GetAbilitySystemComponent()`
- 모든 `ActivateAbility` 분기에서 반드시 `EndAbility` 또는 `CancelAbility` 호출

**Don't:**
- `ActivationOwnedTags`에 다른 클라이언트에서 쿼리할 상태 넣기
- Character에 무기 세부 로직 재집중
- GA Grant 후 `UnequipWeapon`에서 `ClearAbility` 누락
- `UAbilitySystemBlueprintLibrary::SendGameplayEventToActor` 사용 — `ASC->HandleGameplayEvent` 직접 호출
- `UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)` 사용 — `IAbilitySystemInterface` 캐스팅
