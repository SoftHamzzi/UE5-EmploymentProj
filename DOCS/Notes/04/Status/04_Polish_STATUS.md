# GAS 이후 Polish — 진행 상황

> 세션 시작 시 이 파일을 먼저 읽을 것. 특정 Step에 안 묶이는 잡버그를 모아둔다.

---

## 인수인계

| 항목 | 내용 |
|---|---|
| 갱신 | 2026-10-02, developer 세션(역할별 세션 도입 작업, `DOCS/SETUP_SessionRoles.md`) |
| 지금 단계 | 무기 발사 2차 PIE 확인 완료(2026-09-24). 남은 것은 사용자 코드 정리 3건(아래 "남음"과 `Polish/WeaponFireRate/04_Polish_WeaponFireRate_STATUS.md` §4) |
| **다음 행동** | `/session-start 리뷰 Polish WeaponFireRate` — 미응답인 `04_Polish_WeaponFireRate_PrimaryUse_Guide.md` §3-B를 채점 방식으로 처리한다(역할별 세션 시험 운용, SETUP §4-9) |
| 하지 말 것 | 이 영역은 작업 문서마다 설계가 있다(영역 Design 없음). 작업 문서를 고칠 일이 생기면 개발·리뷰 세션에서 고치지 말고 `제안`으로 남긴다 |
| 근거 | `a8285db`(디버그 HUD), `Polish/WeaponFireRate/04_Polish_WeaponFireRate_STATUS.md` §4 |
| 막힌 점 / 사용자 확인 필요 | 사용자 코드 정리: 태그 2개 삭제, `EPLocalModifiers.h`의 `AnimationEditorTypes.h` include 삭제, `UEPGA_Skill_Base::EndAbility` 첫 줄 `IsEndAbilityValid` 가드 |

---

## 완료

- [x] 이동(CMC) 4건 — 크라우치 중 Sprint 차단, 공중 크라우치 차단,
  `MoveSpeedMultiplier` 라이브 리드(Lyra 패턴), 캐스팅 GE 태그 기반 제거.
  `Polish/04_Polish_Movement.md`
- [x] 무기 발사 속도/탄약 동기화 1차 — 쿨다운 태그 수정, RPC Unreliable화,
  완전자동 GAS 재설계, 발사 확정 지점 단일화. `Polish/04_Polish_WeaponFireRate.md`
  (아래 2차가 이 구조를 대체했다)
- [x] **무기 발사 2차 — 쿨다운 GE 제거 + 로컬 타이머 + TargetData 발사** (2026-09-24 PIE 확인).
  연사 한 번 = 활성화 한 번(Auto/Single 한 수명 경로 + 예약 슬롯), 서버 토큰 버킷,
  GAS TargetData 전송 + RPC 배칭(`UEPAbilitySystemComponent`), UT식 원점 동기(무브 타임스탬프 →
  SSR 히스토리), 탄약 예측, 역할 분기 없는 단일 처리 경로(`OnTargetDataReady`).
  잔여 정리는 `Polish/WeaponFireRate/04_Polish_WeaponFireRate_STATUS.md` §4.
  개발기록 `DOCS/Blog/Submit/2026-09-24-EP_GAS-*.md` 4편 (블로그 `_posts/game_dev/devlog/`에 게시).
- [x] **스킬 쿨다운·캐스팅을 GE에서 로컬 타이머로 이관** (`ea08cfc`, 2026-09-22).
  `FEPLocalTimer` + `FEPLocalModifiers`, `GE_Cooldown`/`GE_Casting` 제거,
  `CanActivateAbility`가 타이머를 직접 게이트(서버만 `ServerCooldownToleranceSeconds` 여유),
  `CompleteCast`에서 `NewRejectedDelegate` + `Revert(Generation)`로 거절 롤백,
  캐스팅 잠금은 `ActivationOwnedTags`, 이동속도는 `LocalModifiers`.
  `Polish/04_Polish_SkillDisplay.md` §3 (완료 조건 §3-8 전항 체크).
  §4 PIE 확인 완료 (2026-09-24).
- [x] 스킬 Cast/Cooldown/Active 표시 — 메시지 버스 기반 완전 분리,
  세 스킬(`Heal`/`Dash`/`ShieldOn`) 배선, 위젯 재설계(Active 중 숫자 숨김
  포함). `Polish/04_Polish_SkillDisplay.md` §1

## 남음

- [ ] (신호 있으면) 재발동 자체의 핑 공정성 — GAS가 쿨다운을 진짜로
  예측 못 하는 근본 한계. 방향만 남겨둠. `Polish/04_Polish_SkillDisplay.md` §3
- [ ] `Entry.State.Charges` 이관 — Step 05(`05_Loot_05_Equipment.md`) 차례,
  Step 03/04 완성 후. `Polish/04_Polish_WeaponFireRate.md` §3
- [ ] `FireMode::Burst` 미구현. `Polish/04_Polish_WeaponFireRate.md` §3
- [ ] **스킬 `EndAbility`가 가드 앞에서 부수 효과를 낸다** — `EPGA_Skill_Base.cpp:96-97`의
  `LocalModifiers.Clear(Modifier.MoveSpeed.Casting)`이 `Super`(=`IsEndAbilityValid` 가드,
  `GameplayAbility.cpp:804`)보다 **앞**에 있다. `InstancedPerActor`라 인스턴스가 하나이므로,
  늦게 도착한 `EndAbility`(서버는 자기 `CompleteCast` + 클라 `ServerEndAbility`로 두 번 받는다)가
  그 사이 시작된 새 캐스팅의 모디파이어를 지운다. 수정: 함수 첫 줄에 `IsEndAbilityValid` 가드.
  한 줄. 무기 2차 잔여 정리(태그 삭제, `AnimationEditorTypes.h` include 제거)와 같이 하면 된다. (2026-09-24 확인)
- [ ] **Dash 방향이 클라·서버에서 갈린다** — `EPGA_Skill_Dash.cpp:25`가 `CMC->GetCurrentAcceleration()`을
  양쪽에서 각자 읽는다. 클라는 발동 순간의 가속도, 서버는 마지막으로 처리한 `ServerMove`의 가속도라
  RTT 동안 방향을 틀면 서로 다른 방향으로 대시하고 CMC 보정으로 끌려온다. 총기의 원점·방향 문제와
  같은 구조 — 해법도 같다(발동 시점 방향을 TargetData로 실어 보낸다,
  `Polish/WeaponFireRate/04_Polish_WeaponFireRate_Implementation.md` Step 2·9 참고).
  방향 전환 중 대시할 때만 드러나므로 v1 범위 밖일 수 있다. (2026-09-24 확인)

---

## 세션 시작 템플릿

`/session-start <역할> Polish <작업>` — 위 인수인계 칸의 "다음 행동"을 따른다. 끝낼 때는 `/handoff`.
