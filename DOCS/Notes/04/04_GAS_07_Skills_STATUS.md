# 04_GAS_07_Skills — 구현 상태

**전체 상태: 구현 완료 — ShieldOn 버그 2건 수정 완료 (2026-07-19 코드 확인), PIE 2인 멀티 검수 대기**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷 (2026-07-19 코드 검증 기준).
> 스태미나 시스템 폐기 완료 — Source 전체에 Stamina/SprintDrain/StaminaRegen/State.Dashing 참조 없음 (grep 확인).

---

## 해결된 버그 (EPGA_Skill_ShieldOn.cpp — 2026-07-19 수정 확인)

1. **Commit 실패 시 `return` 누락** → 수정 완료 (29행에 `return` 추가, Dash/Heal 패턴과 통일)
2. **쿨타임 SetByCaller 태그 오기** — `TAG_Cooldown_Skill_Shield`에 값을 넣어 `GE_Shield_Cooldown`(`Data.Cooldown` 조회)이 magnitude를 못 찾던 문제 → `TAG_Data_Cooldown`으로 수정 완료 (42행)

### 참고 — 스펙 불일치 (미해결)

`EPGA_Skill_ShieldOn.h:35` `ShieldCooldown = 50.f` — GAME.md 스펙은 30초. 의도한 튜닝이 아니면 30으로.

---

## Step 0 — NativeGameplayTags (완료)

`EPNativeGameplayTags.h/.cpp`:
- `TAG_Data_Duration` ("Data.Duration") 추가됨
- `TAG_State_Dashing` 제거됨 (h/cpp + Dash 생성자 참조 모두 없음)
- 기존 Skill/Cooldown/Event.Damaged/Data.HealAmount 태그 유지

## Step 1 — 스태미나 잔재 제거 (완료)

- `EPAttributeSet.h/.cpp`: Stamina/MaxStamina UPROPERTY·접근자·OnRep·DOREPLIFETIME·클램프 전부 제거. **Health 상한 클램프는 유지됨** (PreAttributeChange 15~16행)
- `EPCharacter.cpp` PossessedBy: InitHealth/InitMaxHealth만 남음 (InitStamina 제거)
- `EPCharacter.h`: GE_SprintDrainClass/GE_StaminaRegenClass/SprintDrainHandle/ActiveGameplayEffectHandle.h include 제거
- 스프린트(`Input_StartSprint`의 `bWantsToAim` 가드 포함)/점프 경로 무변경 확인

## Step 2 — EPGA_Skill_Dash (완료)

`EPGA_Skill_Dash.h/.cpp`:
- LocalPredicted / InstancedPerActor
- `SetAssetTags(TAG_Ability_Skill_Dash)` 반영
- ActivationBlockedTags: `Cooldown.Skill.Dash`, `State.Dead`
- 방향: `CMC->GetCurrentAcceleration().GetSafeNormal2D()` (saved move로 복제 — 서버/클라 동일 방향), Zero면 ForwardVector 폴백
- `LaunchVel.Z = DashZBoost(250)` + `LaunchCharacter(LaunchVel, true, true)` — 지상 마찰 대책
- 쿨타임: `GE_DashCooldownClass` SetByCaller(`Data.Cooldown`, DashCooldown=10) → 즉시 EndAbility
- 튜닝 UPROPERTY: DashImpulse=1200, DashCooldown=10, DashZBoost=250

## Step 3 — GE 에셋 6종 (생성 완료, `Content/Blueprints/GE/Skill/`)

GE_Dash_Cooldown / GE_Healing / GE_Heal / GE_Heal_Cooldown / GE_ShieldOn / GE_Shield_Cooldown

> GrantedTags는 UE5.3+ `Target Tags Gameplay Effect Component`(Grant Tags to Target Actor)로 설정. 에셋 내부 설정은 PIE 검수에서 최종 확인.

## Step 4 — EPGA_Skill_Heal (완료)

`EPGA_Skill_Heal.h/.cpp` — 문서 코드와 일치:
- SetAssetTags(TAG_Ability_Skill_Heal), Blocked: State.Healing/Cooldown.Skill.Heal/State.Dead
- GE_Healing SetByCaller(`Data.Duration`, 3s) → HealingEffectHandle 보관
- WaitDelay(3s) ∥ WaitGameplayEvent(Event.Damaged) 병렬
- OnHealComplete: GE_Heal(+30, `Data.HealAmount`) + GE_Heal_Cooldown(20s) → EndAbility(정상)
- OnDamageTaken: 쿨타임 없이 EndAbility(취소)
- EndAbility override: HealingEffectHandle 제거 + Invalidate
- 콜백은 `CurrentSpecHandle`/`CurrentActorInfo`/`CurrentActivationInfo` 사용

## Step 5 — EPGA_Skill_ShieldOn (완료 — 버그 2건 수정됨, 상단 참조)

구조는 문서와 일치: SetAssetTags(TAG_Ability_Skill_Shield), Blocked 3종, GE_ShieldOn SetByCaller(`Data.Duration`, 5s), 즉시 EndAbility.

## Step 6 — 입력 3종 (완료)

- `EPCharacter.h/.cpp`: Input_Dash/Heal/Shield → `TryActivateAbilitiesByTag`
- SetupPlayerInputComponent: Dash/Heal/Shield 3종 null 가드 후 Triggered 바인딩
- `EPPlayerController.h`: DashAction/HealAction/ShieldAction UPROPERTY + FORCEINLINE 게터

## Step 7 — 에디터 (에셋 생성 완료)

- `IA_Dash/IA_Heal/IA_Shield` — `Content/Characters/InputActions/Skill/`
- `BP_GA_Skill_Dash/Heal/ShieldOn` — `Content/Blueprints/GA/Skill/`
- IMC 매핑·BP_PlayerController 슬롯·DefaultAbilities 등록 — 에셋 diff 존재 (IMC_DefaultMappingContext, BP_EPPlayerController, BP_EPCharacter 수정됨)

---

## 검증 체크리스트 (PIE 2인 멀티 — 미실시)

- [ ] 스태미나 제거 후: 스프린트/점프 기존 동작 그대로, 데미지 파이프라인 이상 없음 (빌드는 통과)
- [ ] Dash: 이동 방향(키보드 방향) 대시, 지상에서도 정상 거리, 10초 쿨타임 태그·GE 복제
- [ ] Heal: 3초 후 HP +30 (MaxHealth 초과 안 함), 20초 쿨타임
- [ ] Heal 취소: 채널링 중 피격 → State.Healing 즉시 해제, 쿨타임 미적용
- [ ] Shield: State.Shielded 복제, 피격 데미지 절반, 5초 자동 해제, 쿨타임 동작 (현재 값 50초 — 스펙 30초와 불일치 주의)
- [ ] `showdebug abilitysystem`으로 태그/GE/쿨타임 확인

---

## 주의사항

- 트러블슈팅 이력: Dash 쿨다운 태그 미부착(GE 에셋 GrantedTags 컴포넌트 설정), 지상 대시 감속(DashZBoost), 서버 전방 대시(GetCurrentAcceleration) — 세부는 `04_GAS_07_Skills.md` 함정 표 참조
- `LaunchCharacter` 러버밴딩 한계는 의도적 수용 (정석은 Root Motion Source) — 문서 Step 2 참조
- 향후 확장(스킬 슬롯 배정 + 키 리바인딩)은 `04_GAS_07_Skills.md` 섹션 7
