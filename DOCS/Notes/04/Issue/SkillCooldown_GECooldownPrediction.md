# 이슈 — 스킬 쿨타임 표시가 5 → 4.5 → 5로 되돌아감

**상태:** 근본 원인 자체는 GAS의 구조적 한계라 못 고친다. 표시(UI) 문제로
우회 해결함 — `DOCS/Notes/04/Polish/04_Polish_SkillDisplay.md` §1.

---

## 증상

스킬 사용 직후 쿨타임 게이지가 정상적으로 줄어들다가(5초 → 4.5초 근처),
어느 시점에 다시 5초로 튀어 올랐다가 처음부터 다시 줄어드는 현상.

## 원인

GAS의 쿨다운은 `GE_CooldownClass`를 적용하는 방식인데, **쿨다운을 진짜로
예측하지 못한다** — 로컬(클라이언트)에서 스킬을 실행하는 순간 예측용
Cooldown GE를 하나 만들어 걸지만, 이건 서버가 확정한 진짜 Cooldown GE가
리플리케이트되어 도착하면 **교체(swap)**된다. 이 교체가 일어나는 시점은
그 클라이언트의 레이턴시에 따라 달라지고, 교체된 순간 UI가 보고 있던
"남은 시간"의 기준이 로컬 예측 GE에서 서버 확정 GE로 바뀌면서 카운트가
갑자기 리셋되는 것처럼 보인다. 이게 5 → 4.5 → 5의 정체다.

**더 정확히 말하면, 문제는 "쿨다운을 예측 못 한다"가 아니라 "GE의
제거(removal)를 예측 못 한다"는 것이다.** GAS는 GE **적용**(어트리뷰트
수정, 태그 부착, GameplayCue)은 정식으로 predict를 지원한다 — 반면
**제거**는 지원하지 않는다(`GameplayPrediction.h`, README.md:2557-2571
"What is predicted"/"What is not predicted" 목록). "쿨다운이 끝났다"는
사건 자체가 정의상 Cooldown GE의 **제거**이기 때문에, 쿨다운은 이
제거-불가 한계에 걸리는 여러 사례 중 하나일 뿐이다 — 다른 Duration
GE(버프, 디버프, 실드 지속시간 등)도 잠재적으로 같은 문제를 가진다.

공식 문서(`GASDocumentation` README.md:1567-1568, §4.5.15.3 Predicting
Cooldowns)도 이 한계를 명시한다:

> Cooldowns cannot really be predicted currently. We can start UI cooldown
> timer's when the locally predicted Cooldown GE is applied but the
> GameplayAbility's actual cooldown is tied to the server's cooldown's time
> remaining. Depending on the player's latency, the locally predicted
> cooldown could expire but the GameplayAbility would still be on cooldown
> on the server and this would prevent the GameplayAbility's immediate
> re-activation until the server's cooldown expires.

## Epic 공식 입장 (GASDocumentation README.md:3352-3358, Q&A)

Epic 엔지니어(Paragon/Fortnite 개발자)에게 "서버가 로컬 예측 어빌리티의
쿨다운을 덮어쓰지 않게 할 방법이 있는가"를 직접 물어본 질문에 대한 답변:

> The short answer there is not a way to prevent this and Paragon
> definitely had the problem. Higher latency connections would have a
> lower ROF with basic attacks.
>
> I attempted to fix this by adding "GE reconciliation" where latency was
> taken into account when calculating GE duration... However I never got
> this working in a state that could ship and the project moved fast and
> we just never fully addressed it.
>
> **Fortnite does its own bookkeeping for weapon firing rates: it does not
> use GEs for cooldowns on weapons.** I would recommend this if this is a
> critical problem for your game.

즉 Epic도 GE 기반 쿨다운 예측을 시도했다가 실패했고, Fortnite는 결국
GE를 아예 버리고 **별도 변수로 직접 북키핑**하는 방식으로 우회했다.

## 이 프로젝트의 대응

**무기 발사 쪽(`04_Polish_WeaponFireRate.md`)은 이미 GE를 안 쓰는 방향으로
갔다** — Fortnite와 같은 선택.

**스킬 쿨다운은 GE를 그대로 두고, 표시(UI)만 GAS 상태에서 분리했다**
(`04_Polish_SkillDisplay.md` §1):
- `GameplayMessageSubsystem`(프로세스 로컬 pub/sub, 리플리케이션 아님)으로
  `FEPDurationMessage{Instigator, Duration}`를 로컬 예측 시점에 즉시 방송.
- 위젯(`EPSkillSlotWidget`)은 GAS 태그/GE를 직접 안 보고 이 메시지만
  구독 — 받은 시점의 **로컬 시계**로 카운트다운을 시작해서 서버 GE
  스왑과 무관하게 매끄럽게 줄어든다.
- **트레이드오프(의도적으로 수용):** "정확한 숫자"가 아니라 "매끄러움"이
  목표라, 근본 한계(교체 자체)는 없어지지 않는다. 위젯이 이미 쿨다운/실드
  중인 ASC에 늦게 붙는 경우(HUD 재생성, 재접속) 그 방송을 놓쳐서 `Ready`로
  잘못 보이는 문제가 있다는 것도 알고 그대로 둔 것.
- **재발동 자체의 핑 공정성**(표시가 아니라 실제 타이밍 문제 — 고핑
  플레이어는 로컬 쿨다운이 끝나도 서버가 아직 쿨다운 중이라 거절될 수
  있음)은 이걸로 안 풀린다. 처음엔 "서버가 잰 핑만큼 Duration을 깎아서
  GE에 반영"하는 방향을 생각했었는데, 이건 정확히 위 Q&A에서 Epic이
  시도했다가 "shipping 가능한 상태로 못 만들었다"고 밝힌 "GE reconciliation"과
  같은 것이었다. **정정(2026-09-06):** 대신 무기 발사 쪽과 완전히 같은
  패턴 — GE 자체를 버리고, 클라·서버가 각자 복제 안 되는
  `LastActivationTime`을 들고 `GetServerWorldTimeSeconds()`로 독립적으로
  비교 — 으로 방향을 바꿨다. 핑 추정치가 아예 필요 없어서 이 실패 사례를
  원천적으로 피한다(다만 핑 자체로 인한 격차까지 없애는 건 아니다 — GE가
  덧붙이던 여분의 페널티만 없앤다). `04_Polish_SkillDisplay.md` §3에
  정리돼 있고, 아직 체감 문제로 확인되기 전이라 구현하지 않았다.

## 참고

- `DOCS/Notes/04/Polish/04_Polish_SkillDisplay.md` — 실제 구현 (§1 아키텍처, §3 핑 공정성 방향)
- `DOCS/Notes/04/Issue/FireRate_GECooldownPrediction.md` — 같은 근본 원인의 자매 이슈(무기 발사 간격)
- `GASDocumentation/README.md:2557-2571` — §4.10 Prediction, "What is predicted"/"What is not predicted" (적용은 되고 제거는 안 됨)
- `GASDocumentation/README.md:1566-1568` — §4.5.15.3 Predicting Cooldowns
- `GASDocumentation/README.md:3352-3358` — Lead Engineer Q&A Q7 (GE reconciliation 시도 및 실패)
