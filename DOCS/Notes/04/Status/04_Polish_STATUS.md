# GAS 이후 Polish — 진행 상황

> 세션 시작 시 이 파일을 먼저 읽을 것. 특정 Step에 안 묶이는 잡버그를 모아둔다.

---

## 완료

- [x] 이동(CMC) 4건 — 크라우치 중 Sprint 차단, 공중 크라우치 차단,
  `MoveSpeedMultiplier` 라이브 리드(Lyra 패턴), 캐스팅 GE 태그 기반 제거.
  `Polish/04_Polish_Movement.md`
- [x] 무기 발사 속도/탄약 동기화 — 쿨다운 태그 수정, RPC Unreliable화,
  완전자동 GAS 재설계, `ServerConfirmOneShot` 통일. `Polish/04_Polish_WeaponFireRate.md`
- [x] 스킬 Cast/Cooldown/Active 표시 — 메시지 버스 기반 완전 분리,
  세 스킬(`Heal`/`Dash`/`ShieldOn`) 배선, 위젯 재설계(Active 중 숫자 숨김
  포함). `Polish/04_Polish_SkillDisplay.md` §1

## 남음

- [ ] **안 B(`UAbilityTask_NetworkSyncPoint`)** — 클라에서 힐/실드/쿨다운
  GE가 실제로는 예측 적용이 안 되고 서버 리플리케이션(RTT)을 기다리는
  근본 문제. 표시(위 항목)는 매끄럽지만 실제 효과 반영이 그만큼 늦다.
  설계 확정, 코드 미적용. `Polish/04_Polish_SkillDisplay.md` §2
- [ ] (신호 있으면) 재발동 자체의 핑 공정성 — GAS가 쿨다운을 진짜로
  예측 못 하는 근본 한계. 방향만 남겨둠. `Polish/04_Polish_SkillDisplay.md` §3
- [ ] `Entry.State.Charges` 이관 — Step 05(`05_Loot_05_Equipment.md`) 차례,
  Step 03/04 완성 후. `Polish/04_Polish_WeaponFireRate.md` §3
- [ ] **`FireMode::Single`이 쿨다운 GE에 막혀 발사가 안 된다** — `ForceCooldown=true`는
  적용만 강제할 뿐 다음 활성화의 `CheckCooldown()`은 못 건너뛴다. Auto는 타이머가
  `FireOnce`를 직접 불러 검사를 안 거치지만 Single은 발마다 새 활성화라 매번 걸린다.
  `StartWorldTime` 재계산까지 얹혀 실질 차단이 `1/FireRate + RTT`. 원인 규명 완료,
  코드 미적용. `Polish/04_Polish_WeaponFireRate.md` §4
- [ ] `FireMode::Burst` 미구현. `Polish/04_Polish_WeaponFireRate.md` §3
- [ ] **스킬 `EndAbility`가 가드 앞에서 부수 효과를 낸다** — `EPGA_Skill_Base.cpp:96-97`의
  `LocalModifiers.Clear(Modifier.MoveSpeed.Casting)`이 `Super`(=`IsEndAbilityValid` 가드,
  `GameplayAbility.cpp:804`)보다 **앞**에 있다. `InstancedPerActor`라 인스턴스가 하나이므로,
  늦게 도착한 `EndAbility`(서버는 자기 `CompleteCast` + 클라 `ServerEndAbility`로 두 번 받는다)가
  그 사이 시작된 새 캐스팅의 모디파이어를 지운다. 수정: 함수 첫 줄에 `IsEndAbilityValid` 가드.
  한 줄 — 총기 작업 마무리할 때 같이. (2026-09-24 확인)
- [ ] **Dash 방향이 클라·서버에서 갈린다** — `EPGA_Skill_Dash.cpp:25`가 `CMC->GetCurrentAcceleration()`을
  양쪽에서 각자 읽는다. 클라는 발동 순간의 가속도, 서버는 마지막으로 처리한 `ServerMove`의 가속도라
  RTT 동안 방향을 틀면 서로 다른 방향으로 대시하고 CMC 보정으로 끌려온다. 총기의 원점·방향 문제와
  같은 구조 — 해법도 같다(발동 시점 방향을 TargetData로 실어 보낸다,
  `Polish/WeaponFireRate/04_Polish_WeaponFireRate_Implementation.md` Step 2·9 참고).
  방향 전환 중 대시할 때만 드러나므로 v1 범위 밖일 수 있다. (2026-09-24 확인)

---

## 세션 시작 템플릿

```
@GAS_STATUS.md @04_Polish_STATUS.md @Polish/04_Polish_XXX.md
이어서 진행.
```
