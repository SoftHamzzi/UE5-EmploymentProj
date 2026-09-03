# GAS 쿨다운 예측과 시각 어긋남

`UEPSkillSlotWidget`에서 쿨타임 숫자가 `5 → 4.5 → 5`로 되돌아간 현상의 원인 분석.
결론만 먼저 적으면 **위젯 버그가 아니라 GAS의 구조적 한계**다.

---

## 0. 한 줄 요약

> 쿨다운 GE는 **클라에도 하나 생기고 서버에서도 하나 와서 두 벌이 된다.**
> 두 벌의 시작 시각이 **왕복 지연(RTT)만큼 어긋나 있고**, 엔진은 이 어긋남을 보정하지 않는다.

---

## 1. 왜 기호가 필요한가

이 문제는 **"지금 몇 초인가"에 대한 답이 기계마다 다르기 때문에** 생긴다.
그래서 "시각"이라는 말을 그냥 쓰면 논의가 안 된다. 어느 시계로 잰 몇 초인지를 구분해야 한다.

### 시계가 세 개다

```
[절대 시계]   현실에 존재하지 않는 이상적 관찰자의 시계. 논의를 위한 기준일 뿐이다
     │
     ├─ [서버 시계]   World->GetTimeSeconds() on 서버.  서버 맵이 로드된 순간부터 0
     └─ [클라 시계]   World->GetTimeSeconds() on 클라.  클라 맵이 로드된 순간부터 0
```

**두 시계는 시작점이 다르다.** 클라가 3초 늦게 접속했으면 클라 시계는 서버 시계보다 3 작다.
그래서 서버가 "12.4초에 이 GE를 켰다"고 보내도 클라의 `GetTimeSeconds()`와 직접 비교할 수 없다.
이 간극을 메우려고 존재하는 게 `AGameStateBase::ServerWorldTimeSecondsDelta`다.

### 기호 정의

| 기호 | 읽는 법 | 뜻 |
|---|---|---|
| **A** | absolute | 절대 시각. 기준선으로만 쓰고 코드에는 없다 |
| **S0** | server, time zero | **서버 시계로 잰**, 플레이어가 스킬을 누른 순간의 시각 |
| **U** | up | **업링크 지연.** 클라 → 서버 편도 시간. `Server_TryActivateAbility` RPC가 가는 시간 |
| **D** | down | **다운링크 지연.** 서버 → 클라 편도 시간. 복제 패킷이 오는 시간 |
| **RTT** | round trip time | 왕복 지연. **RTT = U + D** |

**U와 D를 나눠 쓰는 이유가 있다.** 결론이 `U + D`로 나오는데, 이건 "핑의 절반"이 아니라
**핑 전체**다. 비대칭 회선(업로드가 느린 가정용 회선)에서 U와 D가 다를 수 있어서 굳이 나눴다.

### 숫자 예시 고정

이 문서 전체에서 다음 값을 쓴다.

```
쿨다운 Duration = 5.0초
U = 0.03초   (업링크 30ms)
D = 0.07초   (다운링크 70ms)
RTT = 0.10초 (핑 100ms)
```

---

## 2. 클라가 아는 서버 시각은 D만큼 뒤처져 있다

### 코드

```cpp
// Engine/Source/Runtime/Engine/Private/GameStateBase.cpp:144
double AGameStateBase::GetServerWorldTimeSeconds() const
{
    UWorld* World = GetWorld();
    if (World)
    {
        return World->GetTimeSeconds() + ServerWorldTimeSecondsDelta;
    }
    return 0.;
}
```

`ServerWorldTimeSecondsDelta`가 어떻게 채워지는가.

```cpp
// GameStateBase.cpp:155  서버가 0.1초마다 자기 시각을 찍어 복제한다
void AGameStateBase::UpdateServerTimeSeconds()
{
    ReplicatedWorldTimeSecondsDouble = World->GetTimeSeconds();
}

// GameStateBase.cpp:164  클라가 받아서 차이를 계산한다
void AGameStateBase::OnRep_ReplicatedWorldTimeSecondsDouble()
{
    const double ServerWorldTimeDelta = ReplicatedWorldTimeSecondsDouble - World->GetTimeSeconds();
    // ... 250개까지 누적 평균 + 0.5 계수 스무딩 ...
}
```

### 유도

`ReplicatedWorldTimeSecondsDouble`은 **서버가 보낼 때** 찍힌 값이고,
`World->GetTimeSeconds()`는 **클라가 받을 때** 읽는 값이다.
그 사이에 D가 흘렀는데 **더해주는 항이 없다.**

절대 시각 `A_r`에 패킷이 도착했다고 하자.

```
보낸 시점의 절대 시각      = A_r - D
그때의 서버 시계 값        = Sv(A_r - D)              ← 이게 ReplicatedWorldTimeSecondsDouble
받은 시점의 클라 시계 값   = Cl(A_r)

Delta = Sv(A_r - D) - Cl(A_r)
      = Sv(A_r) - D - Cl(A_r)                        ← 시계는 같은 속도로 흐르므로
```

그러면 클라가 계산하는 "서버 시각"은

```
Cl(A) + Delta = Cl(A) + Sv(A_r) - D - Cl(A_r)
              = Sv(A) - D
```

> **클라의 `GetServerWorldTimeSeconds()`는 진짜 서버 시각보다 항상 D만큼 작다.**

평균과 스무딩은 **지터를 줄일 뿐 이 편향을 없애지 못한다.** 편향은 모든 샘플에 공통으로 들어 있다.
`ExactPing/2` 같은 보정 항이 코드에 아예 없다.

> 참고: `ServerWorldTimeSecondsUpdateFrequency` 기본값은 `0.1f`다(`GameStateBase.cpp:36`).
> 주석이 "Default to every 100 ms"라고 명시한다. **0.1회/초가 아니라 0.1초 주기**다.

---

## 3. 클라와 서버가 각자 GE를 하나씩 만든다

쿨다운 GE는 `CommitAbility` 안, 즉 **활성화 예측 윈도우 안**에서 적용된다.
그래서 클라도 자기 것을 만든다. 엔진 주석이 예측 대상을 직접 나열한다.

```
// GameplayPrediction.h:35
What do we currently predict?
 -Initial GameplayAbility activation (and chained activation with caveats)
 -Triggered Events
 -GameplayEffect application:
     -Attribute modification (EXCEPTIONS: Executions do not currently predict)
     -GameplayTag modification
 -Gameplay Cue events
 -Montages
 -Movement

Some things we don't predict:
 -GameplayEffect removal
 -GameplayEffect periodic effects (dots ticking)
```

**"쿨다운은 예측이 안 된다"를 "클라에 GE가 안 생긴다"로 이해하면 틀린다.** 생긴다.
못 하는 건 적용이 아니라 **시각 정합**이다.

### 생성 코드는 클라와 서버가 완전히 같다

```cpp
// GameplayEffect.cpp:4295
AppliedActiveGE = new(GameplayEffects_Internal)
    FActiveGameplayEffect(NewHandle, Spec, GetWorldTime(), GetServerWorldTime(), InPredictionKey);
//                                        ↑ StartWorldTime  ↑ StartServerWorldTime
```

```cpp
// GameplayEffect.cpp:2669
FActiveGameplayEffect::FActiveGameplayEffect(..., float InCurrentWorldTime, float InStartServerWorldTime, ...)
    : StartServerWorldTime(InStartServerWorldTime)
    , CachedStartServerWorldTime(InStartServerWorldTime)
    , StartWorldTime(InCurrentWorldTime)
```

### 두 벌의 `StartServerWorldTime`

| | 실행 시점 (절대) | `GetServerWorldTime()` 반환값 | 예시 값 |
|---|---|---|---|
| **예측본** (클라) | 입력 순간 `A0` | `Sv(A0) - D` = **S0 - D** | 12.33 |
| **서버본** (서버) | RPC 도착 `A0 + U` | `Sv(A0+U)` = **S0 + U** | 12.43 |

```
차이 = (S0 + U) - (S0 - D) = U + D = RTT = 0.10초
```

**두 개의 오차가 같은 방향으로 더해진다.** 클라는 D만큼 과거로 찍고, 서버는 U만큼 미래에 찍는다.

---

## 4. `RecomputeStartWorldTime`은 어긋남을 고치지 않는다

여기서 헷갈리기 쉬운 게 **복제되는 필드와 안 되는 필드가 나뉘어 있다**는 점이다.

```cpp
// GameplayEffect.h:1416
/** Server time this started */
UPROPERTY()
float StartServerWorldTime = 0.0f;      // ← 복제된다

UPROPERTY(NotReplicated)
float CachedStartServerWorldTime = 0.0f;

UPROPERTY(NotReplicated)
float StartWorldTime = 0.0f;            // ← 복제 안 된다. 로컬에서 다시 만든다
```

**설계 의도는 옳다.** 로컬 시계 값을 그대로 복제하면 시계 원점이 달라 무의미하니,
서버 시계 값만 보내고 받는 쪽이 자기 시계로 환산한다.

```cpp
// GameplayEffect.cpp:2800  서버본이 도착하면
void FActiveGameplayEffect::PostReplicatedAdd(...)
{
    if (InArray.IsServerWorldTimeAvailable())
    {
        RecomputeStartWorldTime(InArray.GetWorldTime(), InArray.GetServerWorldTime());
        CachedStartServerWorldTime = StartServerWorldTime;
    }
}

// GameplayEffect.cpp:2948
void FActiveGameplayEffect::RecomputeStartWorldTime(const float WorldTime, const float ServerWorldTime)
{
    StartWorldTime = WorldTime - (ServerWorldTime - StartServerWorldTime);
    //                            └──────── 이 GE가 지금까지 살아온 시간 ────────┘
}
```

### 상쇄가 일어난다

서버본이 클라에 도착하는 절대 시각은 `A0 + U + D = A0 + RTT`다.
그 순간 이 GE는 **서버에서 이미 D만큼 살아 있었다.** (서버는 `A0+U`에 켰고 지금은 `A0+U+D`)

그런데 괄호 안을 클라가 계산하면

```
ServerWorldTime - StartServerWorldTime
= (Sv(A0 + RTT) - D)  -  (S0 + U)          ← 앞항에 §2의 편향 -D 가 붙어 있다
= (S0 + RTT - D)      -  (S0 + U)
= RTT - D - U
= 0                                         ← RTT = U + D 이므로
```

> **클라의 D 과소평가가 실제 경과 시간 D를 정확히 지운다.**
> 서버본은 "방금 막 시작한 것"으로 취급된다.

따라서

```cpp
StartWorldTime = WorldTime - 0 = 도착한 바로 그 순간의 클라 시계
```

```cpp
// GameplayEffect.h:1351
float GetTimeRemaining(float WorldTime) const
{
    return Duration - (WorldTime - StartWorldTime);   // = 5.0 - 0 = 5.0
}
```

**도착 순간 남은 시간이 전체 Duration 그대로다.**

---

## 5. 그래서 숫자가 정확히 원래 값으로 돌아온다

```
클라 화면 (Duration 5.0, RTT 0.10)

t=0.00   예측본 적용                      표시 5.00
t=0.05   예측본 남은 4.95                 표시 4.95
t=0.10   서버본 도착. 남은 시간 5.00      표시 5.00   ← 튄다
t=0.11   서버본 남은 4.99                 표시 4.99
...
t=5.10   서버본 소멸. 쿨다운 태그 해제
```

**내려간 폭이 RTT, 튀어 오르는 지점이 항상 전체 Duration이다.**
이게 진단의 결정적 단서였다. 4.7이나 4.9 같은 어중간한 값이 아니라 **매번 딱 처음 값**으로 돌아왔다.

### 위젯이 큰 쪽을 고르는 것도 겹친다

```cpp
// EPSkillSlotWidget.cpp:75
for (const TPair<float, float>& Pair : Results)
{
    if (Pair.Key > Remaining)   // 겹치는 동안 둘 중 큰 쪽 = 서버본
    { Remaining = Pair.Key; Duration = Pair.Value; }
}
```

**작은 쪽을 골라도 해결되지 않는다.** 예측 키가 catch-up되면 예측본이 제거되고
(`FActiveGameplayEffectsContainer::ApplyGameplayEffectSpec`이 `RemoveActiveGameplayEffect_NoReturn`을 등록해둔다)
서버본만 남으므로 튐이 그 시점으로 미뤄질 뿐이다.

### 스택 GE가 아니라 별개 두 개인 이유

`FindStackableActiveGameplayEffect`는 GE가 스택 설정을 가질 때만 기존 것을 찾는다.
쿨다운 GE는 보통 스택을 안 쓰므로 **별개의 `FActiveGameplayEffect` 두 개**가 배열에 들어간다.
`GetActiveEffectsTimeRemainingAndDuration`이 겹침 구간에 원소를 두 개 반환하는 게 이 때문이다.

---

## 6. 현재 대응: 단조 감소 강제

```cpp
// EPSkillSlotWidget.cpp:85
if (LastShownRemaining >= 0.f)
    Remaining = FMath::Min(Remaining, LastShownRemaining);
LastShownRemaining = Remaining;
```

실제 동작:

```
t=0.10   min(5.00, 4.95) = 4.95    역행하지 않는다
t=0.15   min(4.95, 4.95) = 4.95    대신 멈춘다
t=0.20   min(4.90, 4.95) = 4.90    서버본이 따라잡고 나면 정직하게 따라간다
```

**역행이 RTT짜리 정지로 바뀐다.** 그리고 끝나는 시각은 서버본과 정확히 일치한다.
어빌리티 재사용 게이트는 어차피 서버본의 태그가 사라져야 열리므로 **표시와 실제가 어긋나지 않는다.**

### 알려진 구멍

```cpp
// EPSkillSlotWidget.cpp:143
void UEPSkillSlotWidget::RecomputeState()
{
    if (bActive)          ApplyState(Active);
    else if (bLocked)     ApplyState(Locked);
    else if (bCoolingDown) ApplyState(Cooldown);
    else                  ApplyState(Ready);
}

void UEPSkillSlotWidget::ApplyState(EEPSkillSlotState NewState)
{
    CurrentState = NewState;
    LastShownRemaining = -1.f;      // ← 무조건 리셋
```

**상태가 안 바뀌어도 리셋된다.** 쿨다운 도중에 관계없는 `ActiveTag`나 `LockTag`가
한 번 들락거리면 클램프가 풀려 그 프레임에 튐이 그대로 보인다.

```cpp
// 수정안
void UEPSkillSlotWidget::ApplyState(EEPSkillSlotState NewState)
{
    if (NewState == CurrentState) return;
    ...
}
```

---

## 7. 표시보다 심각한 것: 실질 연사

표시를 고쳐도 **게임플레이 차이는 남는다.**

```cpp
// EPGA_Item_PrimaryUse.cpp:112   4-3단계에서 LastServerFireTime을 대체한 그 GE
const float Duration = Weapon ? (1.f / Weapon->WeaponDef->FireRate) : 0.2f;
```

연사속도 검증이 쿨다운 GE이므로 **같은 문제에 걸린다.**
서버본이 `S0 + U`에 시작하고, 클라가 다음 발사를 예측해도 서버가 거부한다.

```
FireRate 5 → 쿨다운 0.2초

핑 0ms    실질 발사 간격 0.20초    초당 5.0발
핑 100ms  실질 발사 간격 0.30초    초당 3.3발    ← 33% 손해
```

> **핑이 높을수록 DPS가 낮다.** 지연 보상의 정반대 방향이다.

GASDocumentation도 이걸 GAS의 현재 이슈로 첫 줄에 적어놨다.

> `GameplayEffect` latency reconciliation (can't predict ability cooldowns resulting in
> **players with higher latencies having lower rate of fire** for low cooldown abilities). (§1)

> Fortnite avoids this by their weapons having **custom bookkeeping that do not use
> cooldown GameplayEffects.** (§4.5.15.3)

**에픽 자신도 포트나이트에서는 쿨다운 GE를 안 쓴다.**

---

## 8. 대응 선택지

| | 방법 | 대가 |
|---|---|---|
| **A** | 서버가 GE 적용 시 `ExactPing/2`만큼 Duration을 깎는다 | **핑 위조로 쿨다운 단축 가능. 상한 필수** |
| **B** | 예측 시엔 아이콘만 회색, 숫자 타이머는 서버본 도착 후 시작 | 짧은 쿨다운에서 바가 늦게 뜬다. GAS 샘플 방식 |
| **C** | 표시만 단조 감소로 클램프 (현재 방식) | 실질 연사 차이는 안 고쳐진다 |
| **D** | 쿨다운 GE를 안 쓰고 자체 기록 | GAS 밖으로 나간다. 포트나이트 방식 |

### A를 하려면

`FActiveGameplayEffect`의 `StartServerWorldTime` / `CachedStartServerWorldTime` / `StartWorldTime`을
전부 갱신하고 `CheckDuration()`을 다시 돌려야 한다.
GASDocumentation §4.5.16에 `const_cast`를 쓰는 구현이 있다. 엔진이 의도한 경로는 아니다.

**상한 없이 하면 안 된다.** 클라가 보고하는 핑을 그대로 믿는 구조이므로
3단계 `UEPCombatDeveloperSettings::MaxRewindSeconds`와 같은 성격의 상한이 필요하다.
같은 이유로 같은 자리에 두는 게 맞다.

---

## 9. 소스 위치 정리

| 위치 | 무엇 |
|---|---|
| `GameStateBase.cpp:144` | `GetServerWorldTimeSeconds()`. 델타를 더할 뿐 |
| `GameStateBase.cpp:164` | `OnRep_...Double()`. **지연 보정 항이 없다** |
| `GameStateBase.cpp:36` | 갱신 주기 기본값 `0.1f`. 주석이 "every 100 ms" |
| `GameplayPrediction.h:35` | 예측하는 것과 안 하는 것 목록 |
| `GameplayPrediction.h:78` | "we do not predict over multiple frames". 예측 윈도우 범위 |
| `GameplayEffect.cpp:4295` | GE 생성. 클라와 서버가 같은 줄을 다른 시각에 실행 |
| `GameplayEffect.cpp:2800` | `PostReplicatedAdd`. 서버본 도착 처리 |
| `GameplayEffect.cpp:2948` | `RecomputeStartWorldTime`. **상쇄가 일어나는 곳** |
| `GameplayEffect.h:1416` | `StartServerWorldTime`만 복제, `StartWorldTime`은 `NotReplicated` |
| `GameplayEffect.h:1351` | `GetTimeRemaining`. `StartWorldTime` 기준 |
| `EPSkillSlotWidget.cpp:85` | 현재 대응(단조 클램프) |
| `EPSkillSlotWidget.cpp:154` | 클램프가 풀리는 지점 |
| `EPGA_Item_PrimaryUse.cpp:112` | `1/FireRate`. 실질 연사 문제의 실제 자리 |

---

## 10. 재현과 확인

```
1. PIE 2인. Net Emulation을 Bad 또는 Custom으로 설정해 RTT를 100ms 이상으로 올린다
2. EPSkillSlotWidget.cpp:85-87 클램프를 임시로 주석 처리한다
3. 스킬을 쓰고 숫자를 본다
   → 내려가다가 정확히 처음 값으로 되돌아가면 이 문서의 현상이다
   → 되돌아가는 값이 처음 값이 아니면 다른 원인이다 (스택 정책, 중복 적용 등)
4. 되돌아가기까지 걸린 시간이 대략 RTT와 일치하는지 확인한다
```

`ShowDebug AbilitySystem`으로 활성 GE 목록을 보면 겹침 구간에 **같은 GE가 두 줄** 뜬다.
예측본에는 PredictionKey가 붙어 있고 서버본은 0이다.

---

## 관련 문서

- `DOCS/Notes/03/03_NetPrediction.md`, `03_BoneHitbox.md`. 같은 성격의 문제를 반대 방향에서 다룬다.
  리와인드는 **서버가 클라 시각으로 되감고**, 여기는 **클라가 서버 시각을 추정한다.**
  양쪽 다 `GetServerWorldTimeSeconds()`를 쓰는데, SSR은 서버에서만 읽어서 편향이 0이고
  (`ServerWorldTimeSecondsDelta`가 서버에서는 갱신되지 않는다) 여기는 클라에서 읽어서 D만큼 틀린다.
  **같은 함수를 어느 쪽에서 부르느냐가 정확도를 가른다**
- `DOCS/Mine/GetServerWorldTimeSeconds.md` — 위 문단이 예고한 대로, SSR의 발사 시각 검증
  (`ClientFireTime`)에 이 D 편향이 실제로 적용된 사례. `ClaimedDelay ≈ U+D = RTT 전체`로
  나오는 이유가 §2의 이 유도 그대로다. 398-400행 "3단계 MaxRewindSeconds와 같은 성격의
  상한이 필요하다"는 실제로 `DOCS/Mine/LagCompensationFix.md`에서 구현됐다
- `DOCS/Notes/04/04_GAS_08_HUD.md`. 위젯 구현 전체
- `DOCS/Notes/04/04_GAS_07_Skills.md`. `UEPGA_Skill_Base`의 CastTime과 GE_Casting
- `C:\Github\GASDocumentation\README.md` §1, §4.5.15.3, §4.5.16
