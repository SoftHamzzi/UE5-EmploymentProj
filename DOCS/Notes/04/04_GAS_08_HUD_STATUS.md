# 04_GAS_08_HUD — 구현 상태

**전체 상태: 기본 HUD(체력/탄약/장전/쿨타임 슬롯/킬 사운드) 구현 완료. 4-상태 스킬 슬롯 + 중앙 시전 게이지 재설계는 문서 작성 완료·구현 미착수 (2026-07-21 코드 확인)**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷 (2026-07-21 코드 검증 기준).

---

## 구현 완료

- `EPHUDWidget.h/.cpp`: Health/MaxHealth/Ammo/MaxAmmo/Reloading 어트리뷰트·태그 구독, `RefreshHealth`/`RefreshAmmo`, `InitWithASC`/`UnbindAll` (리스폰 재바인딩 안전) — 문서 §2/§5 기존 설계와 일치
- `EPSkillSlotWidget.h/.cpp`: **구(舊) 2-상태 버전** — `CooldownTag` 하나만 감시, `CooldownBar`/`CooldownText`만 존재. `SlotFace`/`SkillIcon`/`CastingTag`/`LockTags`/`EEPSkillSlotState` 없음
- `EPPlayerController`: `InitHUD`, `Client_OnKill_Implementation`에서 `KillConfirmSound` 재생, `Client_PlayHitConfirmSound_Implementation`에서 `HitConfirmSound` 재생 — **킬 피드 UI(GameState Multicast)는 애초에 구현된 적 없음**, 킬 사운드 단독 방식이 이미 문서의 최신 결정(§2 킬 피드백)과 정확히 일치
- WBP 에셋: `WBP_HUD.uasset`, `WBP_SkillSlot.uasset` 존재 (내부 레이아웃이 구 2-상태 기준인지, 신규 4-상태 대비 여부는 코드가 아직 2-상태라 미반영 상태로 추정 — 에디터에서 직접 확인 필요)

## 미착수 (문서에 설계되어 있으나 코드 없음)

- `EEPSkillSlotState`(Ready/Cooldown/Casting/Locked) 및 `EPSkillSlotWidget`의 `SlotFace`/`SkillIcon`/`CastingTag`/`LockTags`/`RecomputeState`/`ApplyState` — 미착수
- `EPCastGaugeWidget.h/.cpp` (화면 중앙 원형 시전 게이지, 신규 클래스) — 파일 자체 없음
- `EPHUDWidget`의 `CastGauge` 멤버 및 `InitWithASC` 배선 — 미착수
- `WBP_CastGauge` 신규 에셋, 방사형 마스크 머티리얼 — 미착수 (아트 작업)
- `WBP_HUD`의 슬롯별 `CastingTag`/`LockTags` 설정, `WBP_SkillSlot`의 `SlotFace`/`SkillIcon` 분리 + `CooldownBar` Fill Type=Bottom to Top — 미착수. `LockTags`는 2026-07-21 재설계로 세 슬롯 전부 동일하게 `{State.Casting}`(공용 태그) — 더 이상 스킬별로 다른 값 아님, `04_GAS_07_Skills.md` Step 8-4 참고

이동속도 감소(`MoveSpeedMultiplier`)는 HUD와 무관 — GAS 쪽 구현 상태는 `04_GAS_07_Skills_STATUS.md` Step 8 참조.

---

## 검증 (미실시 — 위 미착수 항목 구현 후)

- [ ] Dash/Shield 사용 → 오버레이 아래→위 차오름(회복 진행도) → 완료 시 사라짐
- [ ] Heal 시전 → HealSlot 고정 주황 덮임 + 중앙 링 게이지(1→0) + Dash/ShieldSlot 빨강 잠금
- [ ] Heal 시전 중 Dash/Shield 입력 무반응 (활성화 자체 차단)
- [ ] Heal 채널링 중 피격 취소 → 모든 시각 요소 즉시 원상복귀
- [ ] 킬 발생 → 킬러 화면에서만 사운드 재생

구현 순서는 `04_GAS_07_Skills.md` Step 8을 먼저 마친 뒤 이 문서의 §4(Step 1) 순서를 따를 것 — HUD는 GAS 태그를 구독만 하므로 GAS 쪽이 없으면 태그가 안 켜짐.
