# GAS 예측 키 (FPredictionKey)

> **검증:** 2026-09-23, UE 5.7 소스 직독
> (`C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Runtime\GameplayAbilities`).
> 이 문서의 모든 주장은 §인용 색인에 파일:줄로 대응된다.
> 엔진 자체 설계 문서가 `GameplayPrediction.h:22-240`의 대형 주석에 있다 — 이 문서는 그것을
> 읽은 결과에 **소스 확인과 이 프로젝트의 사용 맥락**을 붙인 것이다.

## 0. 한 줄

**예측 키는 "클라가 서버 허락 없이 미리 한 일"에 붙는 꼬리표다.** 나중에 서버 응답이 오면
그 꼬리표가 달린 부수 효과를 통째로 되돌리거나(거절), 서버 값으로 갈아끼운다(정산).

GAS가 예측으로 푸는 문제는 네 가지다 (`GameplayPrediction.h:52-58`):

| 문제 | 뜻 | 예측 키가 하는 일 |
|---|---|---|
| Undo | 예측이 틀렸을 때 부수 효과 되돌리기 | 키별 거절 델리게이트 |
| Redo | 예측한 것이 서버에서 또 복제돼 두 번 적용되는 것 막기 | 복제된 GE의 키와 로컬 예측 GE의 키를 대조 |
| Completeness | 부수 효과를 빠짐없이 묶기 | 한 스코프 안의 모든 GE·큐·태그가 같은 키를 받음 |
| Dependencies | 예측이 예측을 부를 때 | Base 키 체인 |

---

## 1. 구조

```cpp
// GameplayPrediction.h:300-313, :413
typedef int16 KeyType;

UPROPERTY()                int16 Current = 0;   // 이 키의 번호
UPROPERTY(NotReplicated)   int16 Base    = 0;   // 갈라져 나온 키. 0이면 독립
UPROPERTY()                bool  bIsServerInitiated = false;   // 서버가 만든 키 (예측에 못 씀)
FObjectKey PredictiveConnectionObjectKey;                      // 이 키를 보낸 커넥션
```

**`Base`는 `NotReplicated`다.** 종속 관계가 서버에 전달되지 않는다는 뜻이고,
§6의 "종속은 클라 쪽에만 있다"가 필드 수준에서 그렇게 돼 있다.

번호는 클라의 **전역 카운터** 하나에서 나온다 (`GameplayPrediction.cpp:170-178`):
```cpp
void FPredictionKey::GenerateNewPredictionKey()
{
    static KeyType GKey = 1;
    Current = GKey++;
    if (GKey <= 0) GKey = 1;      // int16 오버플로 → 1로 되감김
}
```

판정 함수 셋 (`GameplayPrediction.h:336-357`):

| 함수 | 정의 | 뜻 |
|---|---|---|
| `IsValidKey()` | `Current > 0` | 유효한 키인가 |
| `IsLocalClientKey()` | `Current > 0 && !bIsServerInitiated` | 이 클라가 만든 키인가 |
| `IsValidForMorePrediction()` | `IsLocalClientKey()`와 **같다** | 이름과 달리 "서버에 보냈는지"는 보지 않는다 |

> `IsValidForMorePrediction`의 주석은 *"has it already been sent off to the server?"*라고
> 묻지만 구현은 `IsLocalClientKey()` 한 줄이다. 실제로 이 함수가 대답하는 것은
> **"지금 유효한 로컬 키를 들고 있는가"**뿐이다.

**키는 그것을 보낸 클라에게만 되돌아온다.** `NetSerialize`(`:113-129`)가 커넥션을 비교해,
남의 키는 0(무효)으로 직렬화한다. 그래서 다른 플레이어의 예측 키를 볼 일이 없다.

로그 표기는 `[Current/Base]`다 (`GameplayPrediction.h:381-383`).
`[12/5]`면 5번에서 갈라진 종속 키, `[12/0]`이면 독립 키, `[Srv: 12]`면 서버 키다.

---

## 2. 수명은 어빌리티가 아니라 블록이다

키를 붙잡고 있는 것은 `ASC->ScopedPredictionKey`라는 **필드 하나**이고,
그 필드를 세웠다 되돌리는 것이 `FScopedPredictionWindow`다. RAII — 생성자에서 설정,
소멸자에서 복원. 범위는 **선언한 줄부터 그 지역 변수를 감싼 `{ }`가 닫힐 때까지**다.

```cpp
// 클라 (GameplayPrediction.cpp:389-418)
FScopedPredictionWindow(ASC, bCanGenerateNewKey)
    RestoreKey = ASC->ScopedPredictionKey;              // 원래 키 보관
    ASC->ScopedPredictionKey.GenerateDependentPredictionKey();   // 새 키로 교체

// 서버 (GameplayPrediction.cpp:368-386)
FScopedPredictionWindow(ASC, InPredictionKey)
    ASC->ScopedPredictionKey = InPredictionKey;         // 클라가 보낸 키를 그대로 채택

// 소멸자 (GameplayPrediction.cpp:465-521) — 공통
    if (서버) ReplicatedPredictionKeyMap.ReplicatePredictionKey(ScopedPredictionKey);  // ack 전송
    ASC->ScopedPredictionKey = RestoreKey;                                             // 복원
```

두 생성자는 상대편에서 **아무것도 하지 않는다.** 2-인자(클라용)는 `IsNetSimulating() == false`면
즉시 return하고, 3-인자(서버용)는 `IsNetSimulating() == true`면 아무것도 세우지 않는다.
같은 코드를 양쪽에서 돌려도 되게 만든 장치다.

### 프레임을 넘는 예측은 없다

엔진 주석이 못을 박는다 (`GameplayPrediction.h:77-78`):

> *"You can think of this prediction window as being the initial callstack of ActivateAbility.
> Once ActivateAbility ends, your prediction window (and therefore your prediction key) is no
> longer valid. ... **we do not predict over multiple frames.**"*

그래서 **타이머·래턴트 노드·어빌리티 태스크 콜백은 전부 창 밖**이다.
어빌리티가 살아 있다는 것과 창이 열려 있다는 것은 완전히 별개다.
창 밖에서 또 예측하려면 `FScopedPredictionWindow`를 **새로** 열어야 하고,
그때 만들어지는 키는 새 번호다.

---

## 3. 한 사이클

```
[클라]  FScopedPredictionWindow 열림 → 키 5 생성
        │ 예측 GE 적용 (키 5 기록)
        │ RPC로 키 5 전송
        └ 창 닫힘 → ScopedPredictionKey 무효로 복원 (키 5 자체는 살아 있다)
                                    ↓ RPC
[서버]  같은 키 5로 창을 열고 같은 로직 실행
        │ 권위 GE 적용 (키 5 기록 → 복제될 때 같이 내려감)
        └ 창 닫힘 → ReplicatePredictionKey(5)   ← ack
                                    ↓ 프로퍼티 복제 (RPC 아님)
[클라]  FReplicatedPredictionKeyItem::OnRep → CatchUpTo(5) → 키 5의 예측 GE 제거
```

ack은 RPC가 아니라 **프로퍼티 복제**다. `ReplicatedPredictionKeyMap`은 크기 32의 링버퍼이고
(`GameplayPrediction.cpp:667`), 인덱스가 `Key.Current % 32`다(`:683-688`).
그래서 **한 번에 32개 이상의 키가 미정산 상태면 앞엣것이 덮인다** — `OnRep`에 그 경우를 위한
stale 키 자동 승인 로직이 있다(`:597-610`, CVar `AbilitySystem.PredictionKey.MaxStaleKeysBeforeAck`).

---

## 4. 예측된 GE는 Instant도 Infinite가 된다

즉발 효과(탄약 −1, 마나 −10)를 예측하면 되돌릴 방법이 없다. 그래서 GAS는 **클라에서만**
Instant를 Infinite로 바꿔 적용한다 (`AbilitySystemComponent.cpp:988`):

```cpp
bool bTreatAsInfiniteDuration =
    GetOwnerRole() != ROLE_Authority
    && PredictionKey.IsLocalClientKey()
    && Spec.Def->DurationPolicy == EGameplayEffectDurationType::Instant;
```

즉 클라는 "탄약이 29발이다"를 예측하는 게 아니라 **"서버 값에서 −1"**을 예측한다.
서버가 30 → 29를 복제해 와도 로컬은 `29 + (−1) = 28`이 되지 않는다 —
ack이 도착해 예측 GE(−1)가 제거되면서 29로 수렴한다.

이 구조가 성립하려면 어트리뷰트가 **`REPNOTIFY_Always`**여야 한다
(`GameplayPrediction.h:127`). 값이 같아도 OnRep이 불려야 재합산이 돌기 때문이다.

제거/거절 델리게이트는 GE를 적용할 때 등록된다 (`GameplayEffect.cpp:4449-4450`):
```cpp
InPredictionKey.NewCaughtUpDelegate().BindUObject(Owner, &UAbilitySystemComponent::OnCaughtUpActiveGameplayEffect, ...);
InPredictionKey.NewRejectedDelegate().BindUObject(Owner, &UAbilitySystemComponent::OnRejectedActiveGameplayEffect, ...);
```

**예측되지 않는 것** (`GameplayPrediction.h:46-49`): GE **제거**, 주기 효과(도트 틱),
Execution(어트리뷰트 모디파이어만 예측된다), 메타 어트리뷰트(Damage/Healing).

---

## 5. 거절과 정산은 결과가 같고 원인이 다르다

| | 언제 | 경로 | 클라에서 일어나는 일 |
|---|---|---|---|
| **거절** | 서버가 활성화를 막았을 때 | `ClientActivateAbilityFailed` → `BroadcastRejectedDelegate` (`ASC_Abilities.cpp:2245-2251`) | 예측 GE 제거. 서버 값도 안 옴 → 원상복귀 |
| **정산(ack)** | 서버가 실행했을 때 | 창 소멸자 `ReplicatePredictionKey` → `OnRep` → `CatchUpTo` | 예측 GE 제거 + 서버 값 도착 → 같은 값 |

둘 다 예측 GE를 지운다. 차이는 **서버의 권위 값이 뒤따라오느냐**뿐이다.

중요한 함의: **서버가 "이 키의 행동을 안 했다"고 따로 알리는 통로는 없다.**
거절은 `ClientActivateAbilityFailed` 한 곳뿐이다(`GameplayPrediction.h:89` — *"ClientAbilityFailed is
really the only case where we 'reject' prediction keys"*). 서버가 창을 열고 로직을 돌다가
자체 판단으로 아무것도 안 했더라도 창 소멸자는 **ack을 보낸다.** 그러면 클라의 예측 GE가
제거되면서 값이 되돌아간다 — 이것이 "조용한 롤백"의 정상 경로다.

---

## 6. 종속 키

`GenerateDependentPredictionKey()`(`:180-202`)는 **현재 들고 있던 키**를 보고 결정한다:

```cpp
KeyType Previous = Current;
if (Base == 0) Base = Current;
GenerateNewPredictionKey();
ensureAlwaysMsgf((Base == 0) || (Current - Base < 20), TEXT("Deep PredictionKey Chain Detected."));
if (Previous > 0)
    FPredictionKeyDelegates::AddDependency(Current, Previous);   // ← 창 안에서 창을 열었을 때만
```

**창이 이미 열려 있었으면 종속, 아니면 독립.** 그게 전부다. 어빌리티가 살아 있는지,
같은 어빌리티인지는 보지 않는다.

`AddDependency(ThisKey, DependsOn)`가 거는 델리게이트 (`:338-358`):

| 전파 | 조건 | 줄 |
|---|---|---|
| 베이스 **거절** → 종속도 거절 | 항상 | `:341` |
| 종속 ack → 베이스도 ack | CVar `& 1` (기본 켜짐) | `:346-348` |
| 베이스 ack → 종속도 ack | CVar `& 2 == 0` (기본 켜짐) | `:355-357` |

CVar는 `AbilitySystem.PredictionKey.DepChainBehavior`, **기본값 1**이다(`:31-33`).
주석이 밝히는 의도: 논리적으로 옳은 값은 3이고, 1은 거기로 가는 중간값이다.
세 번째 줄은 **레거시 동작**이며 주석이 그 존재 이유를 명시한다 —
*"This is the case with `FScopedServerAbilityRPCBatcher`. It sends only the BaseKey but needs to
notify dependents."*

용도는 체인 활성화다 (`GameplayPrediction.h:175-185`): X가 Y를, Y가 Z를 부르는데
서버는 X의 키 하나만 받는다. Y·Z의 키는 **클라에만 존재**하므로, 서버가 X를 거절했을 때
Y·Z를 되돌릴 방법이 이 델리게이트 체인뿐이다.

> **주의:** 종속 관계는 클라 쪽에만 있다. 서버는 Y가 X에 딸린 것인지 모르므로,
> X를 거절해도 별도로 도착한 Y의 활성화 요청은 그냥 허용할 수 있다.
> 엔진 주석의 권고는 **태그로 막으라**는 것이다 — X가 주는 태그를 Y의 활성화 조건에 넣는다
> (`GameplayPrediction.h:187-189`).

---

## 7. 어빌리티 RPC 배칭과의 충돌

`FScopedServerAbilityRPCBatcher`는 활성화·TargetData·종료를 RPC 하나로 묶는다.
문제는 **배치가 키를 하나만 싣는다**는 것이다.

`CallServerSetReplicatedTargetData`(`ASC_Abilities.cpp:4211`)의 배치 분기는
`CurrentPredictionKey` 인자를 **버리고** TargetData만 저장한다(`:4234`):
```cpp
ExistingBatchData->TargetData = ReplicatedTargetDataHandle;   // 키는 저장되지 않는다
```
서버는 배치를 풀면서 두 호출 모두에 **활성화 키**를 넣는다(`:4130-4131`):
```cpp
ServerTryActivateAbility_Implementation(..., BatchInfo.PredictionKey);
ServerSetReplicatedTargetData_Implementation(..., BatchInfo.PredictionKey, ..., BatchInfo.PredictionKey);
```

그래서 배치 구간 안에서 **새 키를 만들면 그 키는 서버에 도달하지 않는다.**
기본 CVar(1)에서는 §6 세 번째 줄이 베이스 ack을 종속 키까지 전파해 주므로 정리는 되지만,
그것은 주석이 "논리적으로 옳지 않다"고 적어 둔 레거시 동작이다.
**활성화 키를 그대로 재사용하는 쪽이 CVar 값과 무관하게 안전하다.**

---

## 8. 이 프로젝트에서의 적용

`UEPGA_Item_PrimaryUse`(연사 무기)가 이 규칙들을 전부 밟는다.

```cpp
// FireOnce() — 발마다 호출된다
UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
FScopedPredictionWindow ScopedPrediction(ASC, !ASC->ScopedPredictionKey.IsValidForMorePrediction());
CommitAbilityCost(...);                                  // 예측 GE: 탄약 −1
SendFireTargetData(Direction, ClientMoveTimeStamp);      // ASC->ScopedPredictionKey를 실어 보냄
```

| 발 | 창 상태 | `bCanGenerateNewKey` | 키 |
|---|---|---|---|
| 첫 발 (`ActivateAbility` 안) | 엔진 활성화 창이 열려 있음 (`ASC_Abilities.cpp:1916`) | **false** | 활성화 키 재사용 |
| 타이머 발 (`OnFireTimerTick`) | 창 없음 (다른 콜스택) | **true** | 새 독립 키 (`Base == 0`) |

- **첫 발이 활성화 키를 재사용하는 이유**는 §7이다. 배치 RPC가 활성화 키만 싣는다.
- **타이머 발이 독립 키인 이유**는 §6이다. 창이 닫혀 있어 `Previous == 0` → `AddDependency` 미호출.
  덕분에 연사 도중 한 발이 서버에서 버려져도 그 발의 탄약만 되돌아온다. 앞뒤 발은 무관하다.
- **서버 쪽은 창을 직접 열지 않는다.** `ServerSetReplicatedTargetData_Implementation`이
  이미 클라 키로 창을 열고(`ASC_Abilities.cpp:3941`) 델리게이트를 부르므로,
  `OnTargetDataReady` 안의 `CommitAbilityCost`는 자동으로 같은 키를 쓴다.
- **호스트(리슨 서버)는 예측 자체가 없다.** 2-인자 생성자가 서버에서 no-op이고,
  권위라 되돌릴 것도 없다.

설계 배경은 `DOCS/Notes/04/Polish/WeaponFireRate/04_Polish_WeaponFireRate_STATUS.md`,
쿨다운 쪽의 시각 어긋남은 `DOCS/Mine/CooldownPrediction.md`에 있다.

---

## 9. 디버깅

| 증상 | 확인할 것 |
|---|---|
| 예측한 값이 영영 안 돌아온다 (HUD가 −1 고정) | 그 키가 서버에 도달했는가. 창 소멸자의 경고 로그 — *"No key based off PredictionKey %d was communicated to the server during ScopedPredictionWindow"* (`GameplayPrediction.cpp:479`, `:482`) |
| 예측이 아예 안 된다 | `ScopedPredictionKey`가 무효인 상태로 GE를 적용했는가. 창 **밖**에서 `CommitAbilityCost`를 불렀을 때 생긴다 |
| 값이 두 번 적용된다 | 어트리뷰트에 `REPNOTIFY_Always`가 빠졌거나, 예측 GE와 복제 GE의 키가 다르다 |
| `ensureAlways` "Deep PredictionKey Chain Detected" | 창을 20단 이상 중첩했다 (`:196`). 보통 순환 호출 |
| 키가 종속인지 알고 싶다 | `ASC->ScopedPredictionKey.ToString()` → `[Current/Base]`. Base가 0이면 독립 |
| 전체 흐름 | `LogPredictionKey`를 Verbose로. 생성·전송·ack이 전부 찍힌다 |

---

## 10. 인용 색인 (UE 5.7, `Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/`)

| 파일 | 줄 | 내용 |
|---|---|---|
| `Public/GameplayPrediction.h` | 22-240 | 엔진 설계 주석 전문 |
| | 300-313 / 413 | `FPredictionKey` 필드 (`Base`는 `NotReplicated`) |
| | 52-58 | 푸는 문제 6가지 (Undo/Redo/Completeness/Dependencies/…) |
| | 77-78 | "prediction window = ActivateAbility의 초기 콜스택", 프레임 넘김 불가 |
| | 89 | `ClientAbilityFailed`가 유일한 거절 경로 |
| | 127 | `REPNOTIFY_Always` 요구 |
| | 175-185 | 종속 체인 X→Y→Z의 Base 키
| | 187-189 | 종속은 클라에만 있다 → 태그로 막으라는 권고 |
| | 336 / 341 / 354 | `IsValidKey` / `IsLocalClientKey` / `IsValidForMorePrediction` |
| | 381-383 | `ToString()` = `[Current/Base]` |
| | 610 | `KeyRingBufferSize` 선언 |
| `Private/GameplayPrediction.cpp` | 20-21 | CVar `MaxStaleKeysBeforeAck` |
| | 31-33 | CVar `DepChainBehavior` (기본 1, 목표 3) |
| | 113-129 | `NetSerialize` — 보낸 커넥션에만 되돌려준다 |
| | 170-178 | `GenerateNewPredictionKey` (전역 `GKey`) |
| | 180-202 | `GenerateDependentPredictionKey`, 20단 체인 ensure |
| | 304-319 | `FPredictionKeyDelegates::Reject` |
| | 321-335 | `CatchUpTo` — **정확히 그 키만** |
| | 338-358 | `AddDependency` 델리게이트 3줄 |
| | 368-386 | 서버용 생성자 (키 채택) |
| | 389-418 | 클라용 생성자 (`bCanGenerateNewKey`) |
| | 465-521 | 소멸자 — ack 전송 + 키 복원 |
| | 479 / 482 | "키가 서버로 전달되지 않았다" 경고 |
| | 585-600 | `FReplicatedPredictionKeyItem::OnRep` → `CatchUpTo` |
| | 597-610 | stale 키 자동 승인 |
| | 667 | `KeyRingBufferSize = 32` |
| | 683-688 | `ReplicatePredictionKey` (인덱스 = `Current % 32`) |
| `Private/AbilitySystemComponent.cpp` | 988 | `bTreatAsInfiniteDuration` (Instant → Infinite) |
| | 1036 / 1071 | 무한 지속으로 적용하는 분기 |
| `Private/GameplayEffect.cpp` | 4449-4450 | 예측 GE의 CaughtUp·Rejected 델리게이트 등록 |
| `Private/AbilitySystemComponent_Abilities.cpp` | 1916 / 1927 / 1942 | 활성화 창 → `ServerTryActivateAbility` → `CallActivateAbility` |
| | 2245-2251 | `ClientActivateAbilityFailed` → `BroadcastRejectedDelegate` |
| | 3941 | `ServerSetReplicatedTargetData_Implementation`이 여는 창 |
| | 4125-4140 | `ServerAbilityRPCBatch_Internal` — 배치 키 하나로 전부 실행 |
| | 4211-4241 | `CallServerSetReplicatedTargetData` — 배치 분기는 키를 버린다 |
| `Private/Abilities/Tasks/AbilityTask_WaitTargetData.cpp` | 287-288 | 창 + TargetData 전송의 표준 사용례 |
