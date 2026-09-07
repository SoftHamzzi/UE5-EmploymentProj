# 이슈 — 단발(Single) 연사가 `1/FireRate`보다 훨씬 느리게 나감

**상태:** 원인 규명 완료, 수정안(안 A) 확정, **코드 미적용**. 전체 설계는
`DOCS/Notes/04/Polish/04_Polish_WeaponFireRate.md` §4 참고 — 이 문서는
그 문제의 배경과 다른 이슈와의 관계만 정리한 요약이다.

---

## 증상

마우스를 빠르게 연타해 단발로 쏘려 하면 발사가 안 되거나 `1/FireRate`
간격보다 크게 늦게 나간다. 완전자동(`Auto`)은 정상이다.

## 배경 — 왜 `GE_FireCooldown`을 쓰게 됐는가

원래는 총알 한 발마다 `PrimaryUse` 어빌리티를 새로 활성화하고
`GE_FireCooldown`(블루프린트 GE)으로 재발동을 막는 구조였다. 이건
`DOCS/Notes/04/Issue/SkillCooldown_GECooldownPrediction.md`(스킬 쿨타임
5→4.5→5 이슈)와 정확히 같은 종류의 문제를 그대로 물려받는다 — `GASDocumentation`
README.md:1567-1568(§4.5.15.3 Predicting Cooldowns)이 말하는 그 한계다:
GE 기반 쿨다운은 예측이 안 되고, 핑이 높을수록 서버가 쿨다운을 확정하는
시점이 늦어져 재발동 가능 시점도 그만큼 늦어진다.

**1차 대응 — Ability Batching(서버 타이머 루프).** `Auto` 모드는 총알마다
어빌리티를 새로 활성화하지 않는다. `Input_Fire`가 어빌리티를 **한 번만**
활성화하면, 그 안에서 서버 권위 `FTimerHandle`이 `FireInterval`마다
`FireOnce()`를 직접 반복 호출한다(`04_Polish_WeaponFireRate.md` §1-2). 이
루프는 `CheckCooldown()`을 아예 거치지 않으므로 GE 쿨다운의 예측 한계와
무관해지고, GAS의 `ServerTryActivateAbility`/`ServerEndAbility` Reliable
RPC도 연사 한 번당 한 쌍만 나가 핑 의존성이 사라진다. **완전자동의
핑 의존적 발사속도 문제는 이걸로 해결됐다.**

## 남은 증상 — `Single`은 이 루프를 안 탄다

`Single`은 클릭마다 매번 새 활성화다. 즉 클릭마다 `CheckCooldown()`을
다시 거치고, 그 안에서 보는 쿨다운 GE는 여전히 **로컬 예측본과 서버
확정본이 따로 존재하는** 그 GE다. 서버본이 리플리케이트돼서 예측본을
대체하는 순간 `GameplayEffect.cpp:2948`의 `StartWorldTime` 재계산이 경과
시간을 0으로 되돌리고, 클라의 `CheckCooldown()`은 그 되돌아간 값을 본다
— 스킬 쿨타임이 5→4.5→5로 튀던 것과 **원인이 같다.**

**실질 차단 시간 ≈ `1/FireRate` + RTT.** `FireRate`가 낮은 무기일수록 체감이 크다.

## 설계 오류의 본질

발사 간격 하나가 서로 다른 두 가지 역할을 겸하고 있고, 둘 다 같은 쿨다운
GE에 얹혀 있다:

| | 무엇이 필요한가 | 지금 |
|---|---|---|
| `Auto` | 루프 페이싱 | `SetTimer`가 이미 담당 — **GE 불필요** |
| `Single` | 클릭 연타 제한 | 검사는 필요하지만 **복제도 예측도 필요 없는 로컬 제한** |

쿨다운 GE는 복제·예측되는 물건이라 애초에 이 역할(로컬 레이트리밋) 어느
쪽에도 맞지 않는다. `04_GAS_DOCS.md`/README Q7(Fortnite가 무기 발사에
GE를 안 쓰고 자체 북키핑한다는 그 답변)이 가리키는 결론과 같은 방향이다.

## 채택한 방향 (안 A — 상세 설계는 `04_Polish_WeaponFireRate.md` §4)

`GE_FireCooldown`을 발사 간격 용도로는 완전히 걷어내고, `AEPWeapon`에
복제 안 되는 `LastFireTime`을 두어 `GetServerWorldTimeSeconds()` 기준
간격 검사로 대체한다 — 클라 예측용 체크 + `ServerConfirmOneShot`에서
서버가 한 번 더 검증(치트 방어). `Auto`가 이미 검증한 것과 같은 결론:
**발사 간격은 GE가 아니라 로컬 시계 비교로 처리해야 핑과 무관해진다.**

## 참고

- `DOCS/Notes/04/Polish/04_Polish_WeaponFireRate.md` §4 — 전체 진단, 함정(부동소수점 오차로 한 발 건너뛰는 문제), 대가, 기각안(안 B)
- `DOCS/Notes/04/Issue/SkillCooldown_GECooldownPrediction.md` — 같은 근본 원인의 자매 이슈
- `GASDocumentation/README.md:1566-1568` — §4.5.15.3 Predicting Cooldowns
- `GASDocumentation/README.md:3352-3358` — Lead Engineer Q&A Q7 (Fortnite는 GE 없이 자체 북키핑)
