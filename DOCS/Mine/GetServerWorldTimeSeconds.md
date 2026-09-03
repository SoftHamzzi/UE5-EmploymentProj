# `GetServerWorldTimeSeconds()` — 발사 시각 검증에 적용

**핵심 유도(S0/U/D/RTT 표기, `GameStateBase.cpp` 인용)는 여기 없다.**
`DOCS/Mine/CooldownPrediction.md` §1~2에 이미 있다 — GAS 쿨다운 위젯 문제를 파다가
먼저 정리된 것이고, 결론(클라의 `GetServerWorldTimeSeconds()`는 `Sv(A) - D`)이 여기서도
그대로 쓰인다. 이 문서는 그 결론을 **`LagCompensationFix.md`의 발사 시각 검증**에
적용한 부분만 적는다. 두 곳에 같은 유도를 베끼지 않는다.

---

## 적용: `ClaimedDelay`가 왜 `RTT` 전체로 나오는가

`ComputeRewindTime`에서:

```cpp
const float ClaimedDelay = ServerNow - ClientFireTime;
```

`ClientFireTime`은 클라이언트가 발사 순간 `GetServerWorldTimeSeconds()`를 불러 만든 값이다.
`CooldownPrediction.md`의 결론(`Cl(A) + Delta = Sv(A) - D`)을 그대로 대입하면:

```
ClientFireTime ≈ Sv(발사 순간) - D          (D = 다운링크, 서버→클라 편도)
```

이 값이 RPC로 서버까지 가는 데 업링크 `U`가 또 걸린다. 도착 시점의 서버 시각을
`ServerNow ≈ Sv(발사 순간) + U`라 하면:

```
ClaimedDelay = ServerNow - ClientFireTime
             ≈ [Sv(발사) + U] - [Sv(발사) - D]
             = U + D
             = RTT 전체
```

`ExpectedDelay`(`= GetPingInMilliseconds() * 0.5`)는 **의도적으로 RTT/2**를 낸 값이니,
**`ClaimedDelay ≈ RTT`, `ExpectedDelay ≈ RTT/2`로 구조적으로 2배 차이 나는 게 정상**이다.
실측 로그(`LagCompensationFix.md` §8)에서 계속 2~2.3배 근처로 나온 게 이 때문이다 — 잡음이
아니라 `GetServerWorldTimeSeconds()`가 편도(`D`)만 보정하고 왕복은 안 하기 때문에 생기는
결정론적 결과다.

**SSR과의 대비** (`CooldownPrediction.md` §관련 문서가 이미 짚은 것): SSR의 스냅샷 시각은
**서버가 서버에서** `GetServerWorldTimeSeconds()`로 찍는다 — `ServerWorldTimeSecondsDelta`는
서버 자신에서는 갱신되지 않으므로(클라 전용 보정 메커니즘) 편향이 0이다. 반면 발사 시각
검증은 **클라가 클라에서** 찍은 값을 서버가 받아 쓰는 구조라 이 `D`(→`ClaimedDelay`에서는
`U+D`) 편향이 그대로 들어온다. 같은 함수라도 어느 쪽에서 부르느냐가 정확도를 가른다는
`CooldownPrediction.md`의 결론이 여기도 똑같이 적용된다.

---

## 실무 함의 (2026-09-01 정정)

처음엔 `Tolerance`(`FireTimeToleranceSeconds`)로 이 구조적 절반(`RTT/2`)을 흡수하려 했다 —
`ExpectedDelay`를 `RTT/2`로 두고 `Tolerance`를 넉넉히 잡아서 `ClaimedDelay(≈RTT)`가 항상
그 안에 들어오게 하는 식. **틀린 접근이었다.** 이러면 `Tolerance`가 "진짜 지터를 감안한
여유폭"이 아니라 "매번 걸리는 구조적 편향을 가리는 땜질"이 되고, `ValidatedDelay`가 정직한
클라이언트의 값도 상시로 절반 근처까지 깎아버린다(`LagCompensationFix.md` §8 — `Tolerance=0.1`이
"잘 맞았던" 것도 실은 이 편향의 극히 일부만 우연히 커버했기 때문이었다).

**올바른 수정:** `ExpectedDelay` 자체를 `ClaimedDelay`와 같은 스케일로 계산한다 — `*0.5`를
빼고 `GetPingInMilliseconds() * 0.001`(RTT 전체)만 쓴다.

```
ExpectedDelay = GetPingInMilliseconds() * 0.001          (RTT 전체, *0.5 없음)
ClaimedDelay  = ServerNow - ClientFireTime  ≈ RTT 전체
```

이러면 `ExpectedDelay ≈ ClaimedDelay`가 정직한 클라이언트에서 거의 일치하고(실측 잔차
`ClaimedDelay=0.435` vs `ExpectedDelay(RTT)=0.432`, 차이 `0.003` — `LagCompensationFix.md`
§8 참고), `Tolerance`는 원래 의도대로 **진짜 지터 폭만** 감당하면 된다. `RTT/2`(진짜 편도
지연)는 이 검증식에는 필요 없는 값이다 — `ClaimedDelay`가 편도가 아니라 왕복 전체를
가리키는 값이라는 걸 §본문에서 이미 확인했으니, 비교 기준도 그에 맞춰야 했다.

---

## 관련 문서

- `DOCS/Mine/CooldownPrediction.md` §1~2 — S0/U/D/RTT 표기법과 전체 유도, 소스 인용
  (`GameStateBase.cpp:144, 155, 164, 36`)
- `DOCS/Mine/LagCompensationFix.md` §4, §8 — `ComputeRewindTime`의 `ExpectedDelay`/`Tolerance`
  설계와 이번 실측 결과
