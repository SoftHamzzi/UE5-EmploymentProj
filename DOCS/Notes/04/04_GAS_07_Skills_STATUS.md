# 04_GAS_07_Skills — 구현 상태

**전체 상태: Step 0~8 전부 구현 완료. `UEPGA_Skill_Base` 공용 베이스클래스 마이그레이션 완료. PIE 2인 멀티 검수 완료 (2026-07-26 코드 검증)**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷 (2026-07-26 코드 검증 기준).
> 스태미나 시스템 폐기 완료 — Source 전체에 Stamina/SprintDrain/StaminaRegen/State.Dashing 참조 없음.

---

## 최종 구조 — `UEPGA_Skill_Base` 템플릿 메서드

모든 스킬은 `UEPGA_Skill_Base`를 상속한다. "모든 스킬에는 시전시간이 있고, 0초일 수도 있다"는 전제를 코드로 표현한 것.

```
UEPGA_Skill_Base (LocalPredicted / InstancedPerActor)
├─ CastTime (float, 기본 0)
├─ bInterruptibleOnDamage (bool)
├─ GE_CastingClass (TSubclassOf<UGameplayEffect>)
├─ ActivationBlockedTags += { State.Casting, State.Dead }   ← 생성자, 스킬 간 상호 잠금의 유일한 근거
│
├─ ActivateAbility()          템플릿 메서드 — 하위 클래스는 오버라이드하지 않음
│   ├─ CommitAbility 실패 → EndAbility(cancelled)
│   ├─ CastTime <= 0  → OnCastComplete() 즉시 호출 후 EndAbility
│   └─ CastTime >  0  → GE_Casting 적용(SetByCaller Data.Duration=CastTime)
│                       + WaitDelay(CastTime)
│                       + bInterruptibleOnDamage면 WaitGameplayEvent(Event.Damaged) 병렬
├─ EndAbility()              CastingEffectHandle 제거 (IsNetAuthority 가드)
├─ OnCastComplete()          PURE_VIRTUAL — 하위 클래스가 실제 효과를 구현
├─ OnCastInterrupted()       기본 빈 구현
└─ ConfigureCastingSpec()    시전 GE 스펙에 스킬별 SetByCaller 주입 훅
```

**핵심 설계 결정:**

- **`State.Casting` 단일 태그로 상호 잠금.** 시전 GE(`GE_Healing`)가 `State.Casting`을 부여하고, 베이스 생성자가 그 태그를 `ActivationBlockedTags`에 넣으므로 스킬을 추가해도 잠금 배선이 필요 없다. 스킬별로 `State.Healing`을 서로 하드코딩하던 최초안을 폐기한 결과.
- **`bServerRespectsRemoteAbilityCancellation = false`** — 클라 취소가 서버 채널링을 끊지 못하게 함.
- **`State.Shielded`는 잠금 태그가 아니다.** 방벽은 시전시간 0의 즉발 스킬이고 `State.Shielded`는 발동 이후의 버프 지속시간이다. 따라서 방벽이 도는 중에도 다른 스킬을 쓸 수 있다. (HUD에서 중앙 게이지가 2개인 이유 — `04_GAS_08_HUD_STATUS.md` 참조)

---

## Step 0 — NativeGameplayTags (완료)

`EPNativeGameplayTags.h/.cpp` 현재 보유 태그:

| 분류 | 태그 |
|---|---|
| State | Dead, Reloading, UsingItem, FireCooldown, **Healing**, **Shielded**, **Casting** |
| Event | Death, Damaged |
| Ability | Skill.Dash / Skill.Heal / Skill.Shield, Item.PrimaryUse / Item.Reload |
| Cooldown | Weapon.PrimaryUse, Skill.Dash / Skill.Heal / Skill.Shield |
| Data | Damage, Cooldown, **Duration**, ReloadDuration, HealAmount, **MoveSpeedMultiplier** |
| HitZone | Head, Chest, Limbs |

`TAG_State_Dashing` 제거 완료.

## Step 1 — 스태미나 잔재 제거 (완료)

- `EPAttributeSet`: Stamina/MaxStamina 전부 제거. Health 상한 클램프 유지.
- `EPCharacter.cpp` PossessedBy: `InitHealth(100)` / `InitMaxHealth(100)` / `InitMoveSpeedMultiplier(1)` (123~126행)
- 스프린트/점프 경로 무변경

## Step 2 — EPGA_Skill_Dash (완료)

`EPGA_Skill_Dash` — `UEPGA_Skill_Base` 상속, `CastTime = 0`(기본값 유지)

- 생성자: `SetAssetTags(Ability.Skill.Dash)`, `ActivationBlockedTags += Cooldown.Skill.Dash` (State.Casting/Dead는 베이스가 추가)
- `OnCastComplete()`: `CMC->GetCurrentAcceleration().GetSafeNormal2D()`로 방향 결정(saved move로 복제되어 서버/클라 동일), Zero면 ForwardVector 폴백 → `LaunchVel.Z = DashZBoost` → `LaunchCharacter(LaunchVel, true, true)` → `GE_Dash_Cooldown` SetByCaller(`Data.Cooldown`)
- 튜닝: DashImpulse=1200, DashCooldown=10, DashZBoost=250

## Step 3 — GE 에셋 (완료, `Content/Blueprints/GE/Skill/`)

GE_Dash_Cooldown / GE_Healing / GE_Heal / GE_Heal_Cooldown / GE_ShieldOn / GE_Shield_Cooldown

| GE | GrantedTags | SetByCaller |
|---|---|---|
| GE_Healing (시전) | `State.Casting`, `State.Healing` | `Data.Duration`, `Data.MoveSpeedMultiplier` |
| GE_ShieldOn (버프) | `State.Shielded` | `Data.Duration` |
| GE_Heal (즉발) | — | `Data.HealAmount` |
| GE_*_Cooldown | `Cooldown.Skill.*` | `Data.Cooldown` |

> GrantedTags는 UE5.3+ `Target Tags Gameplay Effect Component`(Grant Tags to Target Actor)로 설정.
> **`GE_MoveSpeed_Modifier`는 폐기됨** — 이동속도 감소가 `GE_Healing`의 Multiply 모디파이어로 통합되어, 힐 취소 시 시전 GE 하나만 제거하면 속도가 자동 복구된다.

## Step 4 — EPGA_Skill_Heal (완료)

`UEPGA_Skill_Base` 상속. `ActivateAbility` 오버라이드 없음.

- 생성자: `CastTime = 3.f`, `bInterruptibleOnDamage = true`, `ActivationBlockedTags += Cooldown.Skill.Heal`
- `ConfigureCastingSpec()`: `Data.MoveSpeedMultiplier = HealMoveSpeedMultiplier(0.2)` 주입
- `OnCastComplete()`: `GE_Heal`(`Data.HealAmount`=HealAmount) + `GE_Heal_Cooldown`(`Data.Cooldown`=20)
- 중단 시: `OnCastInterrupted()` 미구현(빈 기본 구현) → 쿨타임 미적용, 베이스 `EndAbility`가 `GE_Healing` 제거

## Step 5 — EPGA_Skill_ShieldOn (완료)

- 생성자: `ActivationBlockedTags += { Cooldown.Skill.Shield, State.Shielded }`, CastTime 0
- `OnCastComplete()`: `GE_ShieldOn`(`Data.Duration`=5) + `GE_Shield_Cooldown`(`Data.Cooldown`)
- **해결된 버그 2건**: Commit 실패 시 `return` 누락 → 베이스 `ActivateAbility`로 이관되며 구조적으로 해소 / 쿨타임 SetByCaller 태그 오기(`TAG_Cooldown_Skill_Shield` → `TAG_Data_Cooldown`) 수정 완료

## Step 6 — 입력 3종 (완료)

- `EPCharacter`: Input_Dash/Heal/Shield → `TryActivateAbilitiesByTag`
- `EPPlayerController`: DashAction/HealAction/ShieldAction UPROPERTY + FORCEINLINE 게터

## Step 7 — 에셋 (완료)

- `IA_Dash/IA_Heal/IA_Shield` — `Content/Characters/InputActions/Skill/`
- `BP_GA_Skill_Dash/Heal/ShieldOn` — `Content/Blueprints/GA/Skill/`
- IMC 매핑 / BP_EPPlayerController 슬롯 / DefaultAbilities 등록 완료

## Step 8 — 이동속도 감소 + 스킬 상호 잠금 (완료)

`MoveSpeedMultiplier` 어트리뷰트 기반. 전 경로 배선 완료:

| 위치 | 내용 |
|---|---|
| `EPAttributeSet.h:44-46` | `MoveSpeedMultiplier` (ReplicatedUsing = OnRep_MoveSpeedMultiplier) |
| `EPAttributeSet.cpp:23-24` | `PreAttributeChange` 클램프 `[0.05, 3.0]` |
| `EPAttributeSet.cpp:97` | `DOREPLIFETIME_CONDITION_NOTIFY(..., REPNOTIFY_Always)` |
| `EPAttributeSet.cpp:120-123` | `OnRep_MoveSpeedMultiplier` → `GAMEPLAYATTRIBUTE_REPNOTIFY` |
| `EPCharacter.cpp:499-507` | `InitASC`에서 재바인딩(기존 핸들 Remove 후 Add) + **구독 직후 `GetNumericAttribute`로 현재값 1회 동기화** |
| `EPCharacter.cpp:514-518` | `OnMoveSpeedMultiplierChanged` → `CMC->MoveSpeedMultiplier = Data.NewValue` |
| `EPCharacterMovement.cpp:36-39` | `GetMaxSpeed()`에서 Sprint/Aim 분기 **이후** 곱연산 |

> `PossessedBy`의 Init* 블록(123~126행)이 `InitASC()` 호출보다 앞에 있어야 한다 — Init* 계열은 델리게이트를 쏘지 않으므로, 구독 직후의 `GetNumericAttribute` 동기화가 0이 아닌 1을 읽으려면 순서가 중요하다. 현재 코드는 올바른 순서.

---

## 남은 이슈

| # | 위치 | 내용 | 심각도 |
|---|---|---|---|
| 1 | `EPGA_Skill_Base.cpp:85` | `OnDamageDuringCast`에서 `EndAbility(..., bWasCancelled = false)`. 피격 중단인데 정상 완료로 보고된다. 현재는 아무도 구독하지 않아 증상이 없지만, 향후 `OnAbilityCancelled`/`OnGameplayAbilityEnded` 구독 시 중단을 완료로 오인한다. → `true`로 | 낮음(잠재) |
| 2 | `EPGA_Skill_ShieldOn.h:31` | `ShieldCooldown = 50.f` vs GAME.md 스펙 30초 | 스펙 결정 필요 |

---

## PIE 2인 멀티 검수 (완료 — 2026-07-26)

- [x] 스태미나 제거 후 스프린트/점프 기존 동작 유지, 데미지 파이프라인 정상
- [x] Dash: 입력 방향 대시, 지상에서도 정상 거리, 10초 쿨타임 태그·GE 복제
- [x] Heal: 3초 후 HP +30 (MaxHealth 초과 없음), 20초 쿨타임
- [x] Heal 취소: 채널링 중 피격 → `State.Casting`/`State.Healing` 즉시 해제, 이동속도 복구, 쿨타임 미적용
- [x] Shield: `State.Shielded` 복제, 피격 데미지 절반, 5초 자동 해제, 쿨타임 동작
- [x] 스킬 상호 잠금: 힐 시전 중 Dash/Shield 활성화 차단
- [x] `showdebug abilitysystem`으로 태그/GE/쿨타임 확인

---

## 주의사항

- 트러블슈팅 이력: Dash 쿨다운 태그 미부착(GE 에셋 GrantedTags 컴포넌트 설정), 지상 대시 감속(DashZBoost), 서버 전방 대시(GetCurrentAcceleration) — 세부는 `04_GAS_07_Skills.md` 함정 표 참조
- `LaunchCharacter` 러버밴딩 한계는 의도적 수용 (정석은 Root Motion Source) — 문서 Step 2 참조
- 향후 확장(스킬 슬롯 배정 + 키 리바인딩)은 `04_GAS_07_Skills.md` 섹션 7
