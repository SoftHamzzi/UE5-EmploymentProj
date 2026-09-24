# Polish — 스킬 표시 (Cast / Cooldown / Active)

**상태(2026-09-14):** §1 구조는 코드에 적용돼 있다. §3 로컬 타이머 설계(쿨다운 +
캐스팅 잠금 + 쿨다운 감소 3종)는 **미구현** — 오늘 구현 예정. 무기 연사 쿨다운(`04_Polish_WeaponFireRate.md`)은 이
다음 차례.

관련 소스: `EPGA_Skill_Base.h/.cpp`, `EPGA_Skill_Heal/Dash/ShieldOn.cpp`,
`HUD/EPSkillSlotWidget.h/.cpp`, `HUD/EPCastGaugeWidget.h/.cpp`, `EPDurationMessage.h`.
문제의 원인 분석은 `DOCS/Notes/04/Issue/SkillCooldown_GECooldownPrediction.md`에 있다 —
여기서 다시 유도하지 않는다.

---

## 1. 현재 구조 (코드 기준)

**실행 흐름** (`EPGA_Skill_Base.cpp`):

```
ActivateAbility
 ├ CommitAbility 실패 → EndAbility(cancel)
 ├ CastTime <= 0     → OnCastComplete() → EndAbility            … 즉시 발동형
 └ CastTime > 0
    ├ GE_CastingClass 적용 (SetByCaller Duration=CastTime, ConfigureCastingSpec 훅)
    ├ BroadcastDurationMessage(TAG_State_Casting, CastTime)      … 게이지 표시
    ├ WaitDelay(CastTime) → OnCastTimerComplete
    │    └ NetworkSyncPoint(OnlyServerWait) → OnCastSynced
    │         └ OnCastComplete() → EndAbility
    └ bInterruptibleOnDamage → WaitGameplayEvent(TAG_Event_Damaged) → OnCastInterrupted → EndAbility(cancel)

EndAbility: 서버만(IsNetAuthority) 태그 쿼리로 캐스팅 GE 제거 → Super
```

`NetworkSyncPoint(OnlyServerWait)`는 클라에서 대기 없이 그 자리에서 새 예측 창을
열고 `OnCastSynced()`를 돌린다 — `OnCastComplete()` 안의 GE 적용(힐/실드/쿨다운)이
클라에서 실제로 예측 적용되게 하는 장치다. 서버는 클라의 신호 RPC가 도착하면
같은 함수를 돈다. (예전 2인자 `FScopedPredictionWindow`는 클라에서 no-op이었다 —
교체 완료, `OnCastTimerComplete()`에 주석으로 남은 옛 코드는 지워도 된다.)

**세 스킬의 `OnCastComplete()`:**
- `Heal` — 힐 GE 적용 → `ApplyCooldownGE()`
- `Dash` — `LaunchCharacter` → `ApplyCooldownGE()`
- `ShieldOn` — 실드 GE 적용 + `BroadcastActiveDuration(ShieldDuration)` → `ApplyCooldownGE()`

**쿨다운 게이트(현재):** `SetCooldownTag(Tag)`가 `ActivationBlockedTags`에 태그를
넣고 같은 값을 `CooldownChannelTag`로 저장한다. `ApplyCooldownGE()`가
`GE_CooldownClass`(그 태그를 부여하는 Duration GE)를 적용하고 같은 채널로 방송한다.
엔진의 `CheckCooldown()`은 관여하지 않는다 — `CooldownGameplayEffectClass`를 안 쓰므로
`GetCooldownTags()`가 비어 항상 `true`(`GameplayAbility.cpp:1057`).

**표시 — 위젯은 GAS 태그를 직접 안 보고 메시지만 구독한다:**
- `FEPDurationMessage{Instigator, Duration}`을 `UGameplayMessageSubsystem`으로 방송.
  프로세스 로컬 pub/sub이라 네트워크를 안 탄다 — 클라는 클라 예측 시점에, 서버는
  서버 시점에 각자 방송한다.
- `EPSkillSlotWidget`: `CooldownChannelTag`/`ActiveChannelTag` 구독, 받은 시점의
  로컬 시계로 카운트다운. `Locked`만 예외로 GAS 태그(`LockTags`)를 직접 본다.
  Active 중엔 바만 1.0 고정, 숫자 없음(사용자 결정).
- `EPCastGaugeWidget`: 켜짐/꺼짐은 `TAG_State_Casting` 태그 이벤트, 값은 메시지.
- 모든 핸들러가 `Message.Instigator != ASC->GetAvatarActor()`로 남의 방송을 거른다
  (리슨 서버 호스트는 모든 플레이어의 방송을 같은 채널로 받는다).

**수용한 대가:** 이미 쿨다운/실드 중인 ASC에 위젯이 나중에 붙으면(HUD 재생성,
재접속) 방송이 지나갔으므로 `Ready`로 보인다. 지금은 안 고친다.

---

## 2. 남은 문제 — 쿨다운 GE의 *제거*는 예측되지 않는다

표시(§1)는 메시지라 매끄럽다. 문제는 **실제 게이트**다. 클라가 예측 적용한 쿨다운
GE는 서버 확정본이 도착하면 교체되고, 그 확정본의 만료는 서버 시계 기준이라 클라
쪽 실질 대기시간이 `Cooldown + RTT`로 늘어난다. 증상 두 가지:

1. 표시가 끝났는데 재발동이 거절된다(핑 높을수록 자주).
2. 5 → 4.5 → 5 로 되감기는 표시(예측본→확정본 교체 순간).

GE 하나만 골라 오너에게 숨기는 방법은 없다(`EGameplayEffectReplicationMode`는
ASC 단위, 오너는 항상 전체 정보를 받는다). 핑을 추정해 Duration을 깎는 방식은
버렸다 — 서버 판정에 추정치가 들어가면 과소/과대 어느 쪽이든 불공정 또는
익스플로잇이다(`SkillCooldown_GECooldownPrediction.md`). **결론: 내가 시작한 시간 상태는
GE로 표현하지 않는다**(§3-0 한 줄 규칙). 이번 구현 대상은 쿨다운과 `GE_Casting`, 실드는 §5.
힐(Instant)은 그대로.

---

## 3. 설계 — 로컬 타이머 (**구현 완료** 2026-09-22 `ea08cfc`, §4 PIE 미검증)

핑 추정을 아예 안 쓴다. 클라와 서버가 **각자 자기 값끼리만** 비교한다.

### 3-0. 원칙 — 한 줄

> **내가 시작한 시간 상태는 어빌리티가 들고, 남이 나에게 건 상태는 GE가 든다.**

| | 누가 시작 | 무엇으로 | 태그 | 숫자 | 배율 |
|---|---|---|---|---|---|
| 캐스팅·쿨다운·자기 버프(실드, 체력 2배, 가속) | **나** | 어빌리티가 그 시간만큼 살아 있음 + `FEPLocalTimer` | `ActivationOwnedTags` | Instant `+Δ`/`−Δ` | `FEPLocalModifiers` |
| 적이 건 슬로우·독·화상 | **남(서버)** | Duration GE 그대로 | GE 부여 태그 | GE 모디파이어 | GE 모디파이어 |

**Duration GE는 "남이 나에게" 쪽에만 남는다.** 내가 시작하는 것에는 안 쓴다 — 어떤 GE가
어느 쪽인지 표를 보고 판단할 일이 없어진다. 누가 시작했느냐만 본다.

**왜 이 선인가 (근거, 접어 둠).** GAS는 GE *적용*은 예측하지만 *제거*는 예측하지 않는다
(`GameplayPrediction.h`, README §4.5.15.3). 클라는 서버 제거가 리플리케이트될 때까지(~RTT)
그 GE를 계속 본다. 그래서 GE로 표현한 시간 상태는 아래 어느 용도든 RTT만큼 늦게 꺼진다:

| 클라가 GE를 보고… | 늦게 꺼지면 |
|---|---|
| 표시 | 아이콘이 RTT 더 떠 있음 — 플레이어가 본다 |
| 활성화 게이트 | 클라가 RTT 더 막힘 → 입력 거절 |
| 이동 예측 | CMC 오예측 → 정정 → 버벅 |
| 다른 타이머의 속도 | 클라 타이머가 RTT 더 빨리 돎 → 서버 거절 |
| 서버가 계산하는 값의 재료 | 클라 값은 판정에 안 들어감 — 유일하게 무해 |

"내가 시작한" 상태는 클라가 시작·종료 시점을 스스로 아는 것이라 로컬로 들 수 있고, 들면
위 문제가 전부 사라진다. "남이 건" 상태는 클라가 시작 자체를 서버에게 들어야 하는 것이라
(피격과 같은 종류) 어차피 늦게 알고, 로컬로 들어 봐야 얻는 게 없다 — 그래서 GE 그대로.

**대가:**
- 자기 버프는 반드시 어빌리티다. 데이터만 다른 버프("속성 X를 N초간 +Δ")는 제네릭
  `UEPGA_Skill_TimedBuff` 하나(Instant GE + `Delta` + 지속시간 + 태그, 전부 BP 편집)로 끝나
  **BP 자식 하나 + 값 입력**이지 C++ 서브클래스가 아니다. 서브클래스는 고유 로직이 있을 때만.
- 그래도 "Duration GE 에셋 하나 만들어 아무 데서나 `ApplyGameplayEffectToSelf`"는 못 한다 —
  에셋이 둘(Instant GE + BP 어빌리티)이 되고, 부여(grant)된 어빌리티를 발동하는 경로여야 한다.
  지금 기획에서 걸리는 경우는 없다: 아이템 사용은 이미 어빌리티, 구역 버프는 "남이 건" 쪽.
- 표준이 아니라 이 문서가 필요하다.

**지금 걸리는 것:** 쿨다운 GE, `GE_Casting` — 이번 구현. `GE_ShieldOn` — 같은 규칙에 걸리지만
§5로 미룸(훅이 생기면 서브클래스 하나).

**자기 버프의 숫자 — 시간제 버프 어빌리티 (예정 패턴, 이번 구현이 자리를 남긴다).**
"체력 2배" 같은 속성 변경을 Duration GE 모디파이어로 주면 적용은 예측돼도 **제거가 예측되지
않아** RTT까지 2배로 남는다(속성이라서가 아니라 같은 뿌리). 위 규칙대로 Duration GE를
**Instant 두 개 + 로컬 타이머**로 쪼갠다. 제거가 아니라 *역적용*이라 예측이 된다:

```
ActivateAbility : OnCastStarted()  → Instant GE "+Δ" 예측 적용
WaitDelay(Dur)  → NetworkSyncPoint(OnlyServerWait)          … 캐스트 스킬과 같은 뼈대
OnSynced        : OnCastComplete() → Instant GE "−Δ" 예측 적용 (새 예측 창 안)
EndAbility
```

캐스트 스킬과 **뼈대가 같다** — "시작에 뭔가 걸고, N초 뒤 되돌린다". 다른 건 시작 훅이
있느냐, 게이지 채널이 무엇이냐뿐이다. 그래서 이번 구현에서 다음 두 자리를 열어 둔다
(사용자 명시, 2026-09-17):
- **`OnCastStarted()`** virtual 훅 — `ActivateAbility`의 `CastTime > 0` 분기에서 태그·배율 직후
  호출, 기본 빈 함수. 캐스트 스킬은 안 쓰고, 버프는 여기서 적용한다.
- **`CastChannelTag`** 필드 — 지금 `BroadcastDurationMessage(TAG_State_Casting, CastTime)`에
  박혀 있는 채널을 필드로. 기본 `State.Casting`, 버프는 자기 Active 채널(예: `State.Shielded`)로
  바꿔 게이지 대신 슬롯의 Active 바를 돌린다.

첫 소비자는 **`ShieldOn`**이다 — `ShieldDuration` 동안 `Shielded` 태그(`ActivationOwnedTags`)
+ 서버 경감 계산. 지금은 `GE_ShieldOn`(Duration)이라 §5에 두고, 이 훅이 생기면 옮긴다.
주의: 역적용은 곱이 아니라 **덧셈**으로(`Δ`를 시작 시 기억) — 사이에 다른 Instant GE가
기준값을 바꿔도 어긋나지 않는다. 쿨다운을 시작에 찍을지 끝에 찍을지는 그때 결정
(`FEPLocalTimer.Start` 호출 위치만 다르다).

**태그 — Duration만이 아니라 "서버가 나중에 지우는 GE" 전부 같다.** Infinite GE를 서버가
`RemoveActiveGameplayEffect`로 끝내도 클라는 리플리케이션이 올 때까지 태그를 들고 있다.
위 한 줄 규칙을 태그에 적용하면:

> **내가 시작한** 상태의 태그는 **`ActivationOwnedTags`**(엔진이 `PreActivate`에서 붙이고
> `EndAbility`에서 떼는 loose 태그, 양쪽 각자) — 캐스팅·재장전·아이템 사용·자기 버프.
> 어빌리티보다 오래 살아야 하면 어빌리티를 그만큼 살려 둔다. GE 부여 태그는 **남이 건**
> 상태(디버프)에만.

**프로젝트의 상태 태그 전부 — 규칙 적용 결과** (2026-09-14 grep, `TAG_State_*`·`TAG_Cooldown_*`).
열: 누가 붙이고 떼나 / 클라에서 늦게 꺼지나 / 누가 읽나(게이트면 실제 문제) / 이 문서의 처리.

| 태그 | 부여 → 제거 | 클라 stale | 소비자 | 판정 |
|---|---|---|---|---|
| `Cooldown.Skill.*` | `GE_Cooldown` → 만료 | 예 | `ActivationBlockedTags` | §3-5 제거 |
| `State.Casting` | `GE_Casting` → 서버만 쿼리 제거 | 예 | `ActivationBlockedTags`, 게이지 | §3-4 `ActivationOwnedTags` |
| `State.Reloading` | `GE_Reloading`(`EPGA_Item_Reload.cpp:48-54`) → **서버만** 핸들 제거(`:65-68`) | **예** | `PrimaryUse`/`Reload` `ActivationBlockedTags`, `EPHUDWidget:32-44` | **`Casting`과 같은 구조** — 재장전 끝나고 ~D 동안 발사 불가 + HUD 늦게 꺼짐. §5 무기로 인계 |
| `State.Shielded` | `GE_ShieldOn` → 만료 | 예 | `ShieldOn` 게이트, `EPAttributeSet:61`(서버) | 게이트만 문제, §5 |
| `State.UsingItem` | 부여처 없음(BP 또는 미구현) | ? | `Reload` 게이트 | 부여처 확인 |
| `State.Dead` | 서버 판정 → `SetTagMapCount(…,0)` | 예, 무관 | 게이트, HUD | 서버 권한이 맞음 |

### 3-1. `FEPLocalTimer` — 타이머 하나, 용도 셋

쿨다운·캐스팅 잠금·(나중에) 시간제 버프가 전부 "지금부터 N초, 속도 R로"라서 값 타입
하나로 통일한다. **타임스탬프가 아니라 남은 시간(`Remaining`) 모델** — 속도 변경을
받으려면 경과를 적립해야 하기 때문이다.

```cpp
// GAS/EPLocalTimer.h — UObject 아님, 복제 안 함, 상속 없음
struct FEPLocalTimer
{
    void  Start(float Now, float Duration);            // Remaining = Duration, Previous 보관, 플래그 세움
    void  Bank(float Now, float RateSoFar);            // 지금까지 경과를 RateSoFar로 Remaining에 적립, LastUpdate = Now. 배율이 바뀌기 직전에 부른다
    float GetRemaining(float Now, float Rate) const;   // Remaining − (Now − LastUpdate) × Rate, 0 이하로는 안 감
    bool  IsElapsed(float Now, float Rate, float Tolerance) const;   // 한 번도 Start 안 했으면 true
    void  Revert();                                    // 서버 거절 롤백. 이번 활성화에서 Start 안 했으면 no-op
private:
    float Remaining = 0.f, LastUpdate = 0.f;
    float PrevRemaining = 0.f, PrevLastUpdate = 0.f;
    bool  bStartedThisActivation = false;
};
```

틱이 없다 — 조회 시점에 적립한다. `Rate`는 조회자가 넘긴다(§3-3 저장소에서 읽어서) —
타이머가 저장소를 몰라도 되게. `Rate == 1`이면 타임스탬프 방식과 계산이 같다.

시계는 **`World->GetTimeSeconds()` — 로컬 시계.** 값이 기계를 건너가지 않는다 —
클라는 클라 값끼리, 서버는 서버 값끼리만 뺀다. Lyra의 `ULyraWeaponInstance::TimeLastFired`가
같은 이유로 로컬 시계를 쓴다(`LyraWeaponInstance.cpp:58`). `GetServerWorldTimeSeconds()`는
SSR처럼 타임스탬프가 RPC로 기계를 건너갈 때만 필요하다.

### 3-2. 쿨다운 감소 3종 — 어디서 들어오는가

| 종류 | 성질 | 들어오는 자리 | 출처 |
|---|---|---|---|
| **N초 감소** | 시전 시점에 확정 | `Start(Now, EffectiveCooldown)`의 인자 | 속성 `CooldownFlatReduction` |
| **N% 감소** | 시전 시점에 확정 | 같은 인자 | 속성 `CooldownPctReduction` |
| **N배 속도** | 진행 중 실시간 | `IsElapsed`/`GetRemaining`의 `Rate` 인자 | 로컬 배율 저장소(§3-3) `Modifier.CooldownRate.*` |

```cpp
float UEPGA_Skill_Base::GetEffectiveCooldown() const   // 시전 시점에 한 번
{
    const UEPAttributeSet* AS = ...;
    const float Flat = AS ? AS->GetCooldownFlatReduction() : 0.f;
    const float Pct  = AS ? AS->GetCooldownPctReduction()  : 0.f;
    return FMath::Max(0.f, (Cooldown - Flat) * (1.f - FMath::Clamp(Pct, 0.f, 1.f)));
}
```

**왜 Flat/Pct는 속성이고 Rate는 로컬인가.** Flat/Pct는 장비·영구 성장처럼 **타이밍이
안 중요한 값**이다 — Instant GE로 바뀌고, 클라가 잠깐 옛 값을 봐도 `Tolerance`(§3-6)가
흡수한다. Rate는 본질이 **내가 시작한 시간제 버프**("10초간 2배")라 §3-0 규칙에 걸린다 —
Duration GE로 주면 클라 타이머가 RTT만큼 더 빨리 돌아 서버가 거절한다. 그래서
로컬 저장소에서 온다. **Flat/Pct를 시간제로 주고 싶어지면 그것도 로컬 저장소로 옮긴다** —
규칙은 값의 종류가 아니라 "시간제인가"다.

시전 시점 확정(Dota식)이지 진행 중 재계산(LoL식)이 아니다 — 진행 중 Flat/Pct가 바뀌어도
돌고 있는 쿨다운은 안 바뀐다. 기획이 후자를 원하면 `SetRemaining()` 하나 추가.

### 3-3. 로컬 배율 저장소 `FEPLocalModifiers` — 어빌리티끼리 공유하는 숫자

쿨다운 타이머는 어빌리티 인스턴스 안에 살면 되지만, **여러 어빌리티가 같이 보는 숫자**
(이동 배율, 쿨다운 속도)는 공유 자리가 필요하다. 예전엔 그 자리가 GE→속성이었다.

```cpp
// Core/EPLocalModifiers.h — AEPCharacter의 일반 멤버. 복제 안 함
struct FEPLocalModifiers
{
    void  Set(FGameplayTag Source, float Value);   // 같은 Source면 덮어씀
    void  Clear(FGameplayTag Source);
    float Product(FGameplayTag Category) const;    // Category 아래 전부 곱, 없으면 1
private:
    TMap<FGameplayTag, float> Values;              // 키 = Modifier.<Category>.<Source>
};
```

- 태그 계층 `Modifier.MoveSpeed.Casting`, `Modifier.CooldownRate.Haste` 식 — **카테고리가
  소비자, 소스가 쓰는 쪽**. 새 종류 = 새 카테고리 태그 하나.
- **쓰는 쪽은 어빌리티 코드, 양쪽에서 각자.** `ActivateAbility`에서 `Set`, `EndAbility`에서
  `Clear`. 클라는 예측 시점에, 서버는 자기 실행 시점에 — 리플리케이션 없음.
- **읽는 쪽:** `EPCharacterMovement::GetMaxSpeed()`가 `속성 MoveSpeedMultiplier × Product(Modifier.MoveSpeed)`,
  `UEPGA_Skill_Base`가 `Product(Modifier.CooldownRate)`를 `Rate` 인자로 넘긴다.
- 속성 `MoveSpeedMultiplier`는 남는다 — 영구 성장/장비처럼 타이밍이 안 중요한 배율용.
  시간제 배율만 여기로 온다(§3-2와 같은 규칙).

### 3-4. 캐스팅 잠금 — `GE_Casting` 제거

`GE_Casting`이 하던 일 셋과 대체:

| 하던 일 | 소비자 | 대체 |
|---|---|---|
| `TAG_State_Casting` 부여 | `ActivationBlockedTags`(전 스킬), `EPCastGaugeWidget` 켜짐/꺼짐 | **`ActivationOwnedTags`** — 엔진이 `PreActivate`에서 loose 태그로 붙이고(`GameplayAbility.cpp:983`) `EndAbility`에서 뗀다(`:870`), 양쪽 각자. 소비자 코드는 **변경 없음** |
| `MoveSpeedMultiplier` 모디파이어 | `EPCharacterMovement::GetMaxSpeed()` | `LocalModifiers.Set(Modifier.MoveSpeed.Casting, GetCastMoveSpeedMultiplier())` (§3-3) |
| `CastTime` 뒤 자동 만료 | — | 이미 `WaitDelay` 태스크가 그 시점을 안다. `EndAbility`에서 태그 제거 + `Clear` |

```cpp
// CastTime > 0 인 스킬의 생성자 (지금은 Heal) — 즉시 발동형에 넣으면 한 프레임 깜빡인다
ActivationOwnedTags.AddTag(EmpGameplayTags::TAG_State_Casting);

// ActivateAbility (CastTime > 0 분기) — 태그는 엔진이 이미 붙였다
Char->LocalModifiers.Set(TAG_Modifier_MoveSpeed_Casting, GetCastMoveSpeedMultiplier());   // 서브클래스 훅, 기본 1
OnCastStarted();                                          // 시간제 버프용 훅, 기본 빈 함수 (§3-0)
BroadcastDurationMessage(CastChannelTag, CastTime);       // 필드, 기본 State.Casting (§3-0)

// EndAbility (모든 경로: 완료·피격취소·서버거절) — IsNetAuthority 가드 없음, 양쪽 다
Char->LocalModifiers.Clear(TAG_Modifier_MoveSpeed_Casting);
// 태그 제거는 Super::EndAbility가 한다
```

**왜 손으로 `AddLooseGameplayTag`하지 않고 `ActivationOwnedTags`인가.** 그게 정확히 이 용도로
있는 엔진 기능이다 — "어빌리티가 활성인 동안 오너에게 붙는 태그". 붙이고 떼는 코드를
우리가 안 쓰니 빠뜨릴 수 없고, 서버 거절(`K2_EndAbility`)·피격 취소·정상 종료 어느 경로든
엔진 `EndAbility`가 뗀다. 기본 설정 `ReplicateActivationOwnedTags = true`
(`GameplayAbilitiesDeveloperSettings.h:76`, 프로젝트 미변경)면 `CountToOwner`로 복제되어
**시뮬레이티드 프록시도 `Casting`을 본다** — 손 loose 태그로는 못 얻는 것. 오너 클라는
자기 로컬 카운트에 서버 카운트가 `SetTagMapCount`로 **덮어써지는** 식이라(`GameplayEffectTypes.cpp:1743`)
로컬 제거가 먼저 반영되고 서버 제거는 같은 값을 다시 쓸 뿐 — 예측이 깨지지 않는다.
단 `CastTime < RTT`면 서버의 "추가" 복제가 클라의 로컬 제거 **뒤에** 도착해 한 번 깜빡일
수 있다(§3-9). Heal 3초엔 해당 없음.

`ConfigureCastingSpec()` 훅은 `GetCastMoveSpeedMultiplier()`로 바뀐다 — Heal만 오버라이드.
`GE_CastingClass` 필드와 에셋은 제거. `EndAbility`의 `IsNetAuthority()` 가드(현재 캐스팅 GE
제거용)도 같이 사라진다.

**남는 이동 오차 하나 — 서버 쪽 U.** 위로 클라의 늦은 제거는 사라진다. 그러나 서버의
캐스트 종료는 클라 신호 RPC 도착(U 뒤)이라, 그 U 동안 클라는 정상 속도로 움직인 이동을
서버는 느린 배율로 재현한다 → 정정 한 번. 이건 Sprint와 같은 종류의 문제고 같은
해법이다 — **캐스팅 비트를 `FSavedMove`의 `FLAG_Custom_2`에 실어** 서버가 클라 이동
타임라인대로 배율을 바꾸게 한다(`04_Polish_Movement.md`, 별도 단계). 이번 단계에선
안 한다 — U는 RTT의 절반이고 정정 한 번은 지금의 "두 번 튐"보다 낫다.

### 3-5. 언제 찍는가 — 베이스가 찍는다

**캐스트 완료 시점**(캐스트 3초짜리 힐의 쿨다운은 캐스트가 끝난 뒤부터 센다는
현재 규칙 유지). 지금은 세 스킬이 각자 `OnCastComplete()` 끝에서 `ApplyCooldownGE()`를
부르는데, 빠뜨리면 쿨다운 없는 스킬이 조용히 생긴다. 베이스가 순서를 고정한다:

```cpp
// CastTime <= 0 분기와 OnCastSynced() 둘 다 이 함수 하나로 접는다
void UEPGA_Skill_Base::CompleteCast()
{
    OnCastComplete();                                            // 서브클래스: 효과만
    const float Duration = GetEffectiveCooldown();               // §3-2
    CooldownTimer.Start(GetWorld()->GetTimeSeconds(), Duration);
    if (IsPredictingClient())                                    // 서버 거절 시 롤백 — GAS의 GE 예측 롤백과 같은 훅
        CurrentActivationInfo.GetActivationPredictionKey().NewRejectedDelegate()
            .BindUObject(this, &UEPGA_Skill_Base::OnActivationRejected);   // → CooldownTimer.Revert()
    BroadcastDurationMessage(CooldownChannelTag, Duration);      // 표시는 그대로. 감소분 반영된 값
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
```

**롤백은 `EndAbility(bWasCancelled)`가 아니라 예측 키 델리게이트로.** 서버 거절 경로
`ClientActivateAbilityFailed_Implementation`은 `SetActivationRejected()` 뒤 `K2_EndAbility()`를
부르는데 그건 `bWasCancelled = false` 고정이다(`GameplayAbility.cpp:1449-1456`). 게다가
즉시 발동형은 클라 어빌리티가 이미 끝나 있어 `EndAbility`가 `IsEndAbilityValid`(`bIsActive == false`,
`:775-778`)에서 조기 반환 — 우리 코드에 도달조차 안 한다. `FPredictionKey::NewRejectedDelegate`는
GAS가 예측 GE를 되돌릴 때 쓰는 바로 그 훅이다(`GameplayPrediction.h:88`) — 어빌리티가 끝나
있어도 델리게이트는 살아 있다.

### 3-6. `CanActivateAbility` 오버라이드와 서버 허용 오차

```cpp
bool UEPGA_Skill_Base::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(...)) return false;        // Casting(ActivationOwnedTags)/Dead/Shielded 태그 차단은 그대로
    if (UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns()) return true;   // AbilitySystem.IgnoreCooldowns 치트 존중

    const float Now  = ActorInfo->OwnerActor->GetWorld()->GetTimeSeconds();
    const float Rate = Char->LocalModifiers.Product(TAG_Modifier_CooldownRate);
    const float Tolerance = ActorInfo->IsNetAuthority() ? ServerCooldownTolerance : 0.f;
    if (CooldownTimer.IsElapsed(Now, Rate, Tolerance)) return true;

    const FGameplayTag& FailTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;
    if (OptionalRelevantTags && FailTag.IsValid())             // 엔진 CheckCooldown과 같은 실패 사유 태그
        OptionalRelevantTags->AddTag(FailTag);
    return false;
}
```

`const` 함수라 적립(`Bank`)을 안 한다 — `Rate`를 인자로 넘겨 계산만 한다. `Bank`는
배율이 바뀌기 직전(§3-7 마지막 행) 그 어빌리티가 옛 배율로 한 번 부른다 — 그래야 옛
구간은 옛 배율로, 새 구간은 새 배율로 계산된다.

엔진 `CanActivateAbility`도 같은 순서다 — `ShouldIgnoreCooldowns()` 먼저, 실패 시
`ActivateFailCooldownTag`(`GameplayAbility.cpp:1075`, `IsValid()` 가드 포함). Lyra의
오버라이드가 정확히 이 모양이다 — `Super` → 자체 검사 → `AddTag(ActivateFail_*)` → `false`
(`LyraGameplayAbility.cpp:154`). 태그는 `DefaultGame.ini`
`[/Script/GameplayAbilities.AbilitySystemGlobals]` `ActivateFailCooldownName`으로 지정해야
유효해진다 — 지금은 미설정이라 가드 때문에 아무 일도 안 일어난다. 실패 사유 소비자
(Lyra의 `NotifyAbilityFailed` → `FailureTagToUserFacingMessages`)는 사용자가 확장으로
명시한 자리(2026-09-14)라 태그를 넣어 두고, 소비자는 필요할 때 만든다.

GAS 표준 흐름이 재검증 왕복을 해준다: 클라 `TryActivateAbility` → 자기 값으로 통과 →
로컬 예측 실행 + `ServerTryActivateAbility` → 서버가 **자기** 값으로 다시 검사 → 실패면
`ClientActivateAbilityFailed`로 롤백 → 예측 키의 **Rejected 델리게이트**(§3-5)가
`CooldownTimer.Revert()`를 부른다.

**왜 간격이 `Cooldown`으로 맞아떨어지는가.** 서버의 스탬프는 클라의 것보다 상행 지연
`U`만큼 늦다(캐스트 완료 신호 RPC 도착 시점). 클라가 `Cooldown` 뒤 재발동하면 그 RPC도
`U` 뒤 도착 → 서버 입장 경과 시간은 `(T + Cooldown + U) − (T + U) = Cooldown`. **클라의
앞선 출발이 상행 지연을 정확히 상쇄한다.** 남는 건 절대 지연 하나뿐이다.

**왜 서버가 재검증하는가 — Lyra·GASShooter는 안 한다.** 둘 다 무기 발사 간격을 클라
루프에만 맡긴다. 탄약(Cost)이 스팸의 상한이기 때문이다. 우리 스킬은 Cost가 없다 —
서버 게이트가 없으면 조작 클라가 힐/실드를 매 프레임 쓴다. `Tolerance`와 `Revert()`는
전부 이 재검증 때문에 생긴다.

**`ServerCooldownTolerance`.** 위 상쇄는 두 RPC의 상행 지연이 같을 때 정확하다. 지터로
두 번째가 더 빨리 오면 서버는 `Cooldown − 지터`를 보고 거절한다. 그래서 서버만
`Cooldown − Tolerance`로 검사한다. `UEPCombatDeveloperSettings`, 초기값 **0.1s**, ini로 덮음.
클라는 0. **상수**라 조작·추정 오류의 영향이 없고 상한이 명시적이다.

### 3-7. 남는 것 / 빠지는 것

| 항목 | 지금 | 변경 후 |
|---|---|---|
| `SetCooldownTag(Tag)` | `ActivationBlockedTags` 추가 + 채널 저장 | **제거.** `CooldownChannelTag`를 `ActiveChannelTag`처럼 protected 필드로, 생성자에서 대입 |
| `TAG_Cooldown_Skill_*` | 차단 태그 겸 방송 채널 | **방송 채널만** |
| `GE_CooldownClass` / `GE_Cooldown_*` 에셋 | 게이트의 실체 | **제거** |
| `GE_CastingClass` / `GE_Casting` 에셋 | 잠금 + 이동 배율 | **제거** → `ActivationOwnedTags` + `LocalModifiers`(§3-4) |
| `ConfigureCastingSpec(Spec)` 훅 | Heal이 SetByCaller 배율 | `GetCastMoveSpeedMultiplier()` 훅, 기본 1 |
| `ApplyCooldownGE()` | GE 적용 + 방송, 서브클래스 호출 | **제거.** `CompleteCast()`가 스탬프 + 방송 |
| 세 스킬 `OnCastComplete()` 마지막 줄 | `ApplyCooldownGE();` | **줄 삭제** |
| `Cooldown` 필드 | GE SetByCaller 값 | `GetEffectiveCooldown()`의 기준값 |
| `EndAbility` | 서버만 캐스팅 GE 제거 | 양쪽: `LocalModifiers.Clear` (태그는 `Super`가 뗀다. `Revert()`는 여기가 아니라 예측 키 델리게이트 — §3-5) |
| `ActivationBlockedTags`의 `Casting`/`Dead`/`Shielded` | 유지 | 유지. `Casting`은 이제 `ActivationOwnedTags`가 채운다 |
| `EPCharacterMovement::GetMaxSpeed()` | 속성만 | 속성 × `Product(Modifier.MoveSpeed)` |
| 위젯 | 메시지 구독 + `Casting` 태그 이벤트 | **변경 없음** (loose 태그도 같은 태그 이벤트를 쏜다) |
| 신규 | — | `FEPLocalTimer`, `FEPLocalModifiers`(`AEPCharacter` 멤버), 속성 `CooldownFlatReduction`/`CooldownPctReduction`, 태그 `Modifier.MoveSpeed.Casting`/`Modifier.CooldownRate`, `ServerCooldownTolerance`, `CompleteCast()`, `GetEffectiveCooldown()`, **`OnCastStarted()` 훅 + `CastChannelTag` 필드**(시간제 버프 자리, §3-0) |
| Rate 변경 시 표시 | — | 배율을 `Set`/`Clear`하는 어빌리티가 **먼저** 각 스킬의 `CooldownTimer.Bank(Now, 옛 Rate)`를 부르고, 그 다음 `BroadcastDurationMessage(채널, GetRemaining/새 Rate)`를 다시 쏜다. 위젯은 새 메시지로 다시 셀 뿐. 스킬 목록 순회는 `ASC->GetActivatableAbilities()`에서 `UEPGA_Skill_Base` 캐스트 |

### 3-8. 완료 조건

> 2026-09-24 코드 대조 결과. `ServerCooldownToleranceSeconds`는 ini 항목 없이
> C++ 기본값 0.1s를 쓴다(`EPCombatDeveloperSettings.h:46`).

- [x] `GAS/EPLocalTimer.h` — §3-1 API, UObject 의존 없음
- [x] `Core/EPLocalModifiers.h` — `Set/Clear/Product`, `AEPCharacter` 멤버
- [x] `EPGA_Skill_Base` — `CooldownTimer`, `CompleteCast()`, `GetEffectiveCooldown()`, `CanActivateAbility` 오버라이드, 모디파이어 Set/Clear, `OnActivationRejected()` + `NewRejectedDelegate` 바인딩
- [x] `EPGA_Skill_Heal` 생성자 — `ActivationOwnedTags.AddTag(TAG_State_Casting)`
- [x] `OnCastStarted()` 빈 virtual + `CastChannelTag` 필드(기본 `State.Casting`) — 시간제 버프 자리
- [x] `EPCharacterMovement::GetMaxSpeed()` 로컬 배율 곱
- [x] 속성 2개 추가(`COND_None` 기존 규칙대로), 태그 2계층 등록
- [x] `SetCooldownTag`/`ApplyCooldownGE`/`GE_CooldownClass`/`GE_CastingClass`/`ConfigureCastingSpec` 제거, 에셋 참조 정리, Dash/Heal/ShieldOn 컴파일
- [x] `ServerCooldownTolerance` DeveloperSettings + ini
- [ ] §4 PIE 체크리스트 통과 ← **유일한 미완**

### 3-9. 함정표

| 함정 | 대응 |
|---|---|
| 서버가 거절한 활성화(롤백)인데 클라는 이미 찍었다 — 즉시 발동형은 `ActivateAbility` 안에서 바로 `CompleteCast()` | 예측 키 Rejected 델리게이트 → `Revert()`(§3-5). `EndAbility`에 걸면 즉시 발동형에선 **안 불린다**. 캐스트 중 피격 취소는 아직 `Start()` 전이라 무관. 위젯은 쿨다운 방송을 받은 채 남는다 — 리셋 메시지는 **이번엔 안 만든다**, 거절이 `Tolerance`로 드물어진다. 검증에서 빈도 확인 |
| 롤백인데 모디파이어가 남는다 | 캐스트형은 거절 시점에 아직 활성이라 `K2_EndAbility()` → 우리 `EndAbility`가 지난다. `IsNetAuthority()` 가드를 남기면 클라에서 안 지워진다 — **가드를 빼는 게 이 변경의 일부**. 태그는 엔진이 뗀다 |
| **짧은 쿨다운 + 패킷 로스** — 캐스트 완료 신호와 다음 활성화가 둘 다 reliable RPC라 같은 채널에서 순서 보장. 신호가 유실·재전송되는 사이에 `Cooldown`이 지나면 활성화 RPC가 신호 **뒤에 줄을 서서 같이 도착** → 서버 경과 0 → 거절 | `Cooldown < 재전송 지연(~RTT)`일 때만. 스킬(초 단위)엔 해당 없음. **무기 연사(0.1s대)로 인계** — `Tolerance`로는 못 막고, 어떤 방식이든 같은 조건에서 같은 일이 난다 |
| `ActivationOwnedTags` 복제가 오너의 로컬 카운트를 **덮어쓴다**(`SetTagMapCount`) | 같은 태그를 다른 소스(다른 어빌리티, 손 loose 태그)가 동시에 붙이면 서버 값으로 덮여 카운트가 틀어진다. `Casting`은 `ActivationBlockedTags`라 동시 캐스팅 불가 — 성립. **같은 태그를 두 경로로 붙이지 말 것** |
| `CastTime < RTT`면 오너에서 한 번 깜빡인다 — 서버의 "추가" 복제가 클라의 로컬 제거 뒤에 도착해 카운트 1을 다시 쓰고, 서버 제거 복제가 D 뒤에 0으로 되돌린다 | Heal 3초엔 무관. 0.3초 미만 캐스트가 생기면 그 스킬만 `ReplicateActivationOwnedTags`를 끄거나(전역 설정이라 전부 꺼진다) 손 loose 태그(`None`)로 |
| 즉시 발동형에 `ActivationOwnedTags`를 넣으면 같은 프레임에 붙었다 떨어져 게이지가 한 프레임 깜빡인다 | `CastTime > 0`인 스킬 생성자에만 넣는다(§3-4). 베이스 생성자엔 넣지 않는다 |
| 클라가 `LocalModifiers`/타이머를 조작 | 서버는 서버 값만 본다. 이동 배율 조작은 CMC 서버 재현에서 정정된다(지금 Sprint와 같은 신뢰 수준) |
| 어빌리티 인스턴스가 재생성되면 타이머 초기화 | 재부여는 장비 교체 등 명시적 이벤트뿐. 쿨다운 리셋이 맞는지는 **기획 결정** — 아니면 `FEPLocalTimer`를 `AEPCharacter`로 올린다(값 타입이라 옮기기 쉽다) |
| `Cooldown == 0` | `IsElapsed`가 `Remaining <= 0`이라 항상 통과 |
| 리슨 서버 호스트 | 클라 인스턴스 == 서버 인스턴스, `IsNetAuthority()` 참이라 `Tolerance` 적용. 데디 서버가 목표라 무시 |
| `Rate == 0` (쿨다운 정지 버프) | `GetRemaining`이 안 줄어든다 — 의도한 동작. 음수 Rate는 `Product`에서 0으로 클램프 |
| 속성 `CooldownFlatReduction`을 내가 시작한 Duration GE로 바꾼다 | §3-0 위반. 시간제 감소는 `LocalModifiers`로 — 카테고리 하나 추가(`Modifier.CooldownFlat`)하고 `GetEffectiveCooldown`이 거기서도 읽게 한다 |

---

## 4. PIE 검증 체크리스트

`NetEmulation.PktLag 100` 이상에서, 원격 클라로:

1. **재발동 타이밍** — 표시 카운트다운이 0이 되는 순간 재발동. 거절(롤백) 없이
   두 번째 캐스트가 시작되는가. 5회 이상 반복, 거절 횟수 기록.
2. **되감기 표시 소멸** — 5 → 4.5 → 5 현상이 없어졌는가(GE가 없으니 없어야 한다).
3. **연속 발동 간격** — 서버 로그에 두 `CooldownTimer.Start()` 시각 차가 `Cooldown ± Tolerance`인가.
4. **캐스팅 잠금** — Heal 캐스트 종료 직후 Dash가 **즉시** 나가는가(전엔 ~RTT 막혔다).
   `EPCastGaugeWidget`이 캐스트 종료와 동시에 꺼지는가.
5. **캐스팅 이동** — Heal 캐스트 종료 시 정정이 **한 번**인가(전엔 두 번 튐, 가설).
   `p.NetShowCorrections 1`로 횟수 확인. 남은 한 번은 §3-4 서버 U — 이번 범위 밖.
6. **쿨다운 감소** — 속성 `CooldownPctReduction=0.5`를 치트 GE로 넣고 표시와 게이트가
   둘 다 절반이 되는가. `LocalModifiers.Set(Modifier.CooldownRate.Test, 2.0)`로 진행 중
   쿨다운이 2배 속도로 줄고 표시가 갱신되는가.
7. **기존 예측(§1) 회귀** — `Heal` 캐스트 완료 즉시 힐량 반영, `ShieldOn` 완료 즉시 경감.
8. **호스트 회귀** — 리슨 서버 호스트에서 세 스킬 정상.

전부 통과 전엔 STATUS에 "완료"로 쓰지 않는다.

---

## 5. 범위 밖 — 같은 원인, 별도 처리

- **캐스팅 이동의 서버 쪽 U** — §3-4. 캐스팅 비트를 `FSavedMove` `FLAG_Custom_2`에 싣는다.
  `04_Polish_Movement.md`에서 Sprint/Aim과 같은 패턴으로.
- **`TAG_State_Shielded` 차단** — 실드 GE 제거도 비예측이라 `ShieldOn` 재발동이 RTT 더
  막힌다. `Cooldown > ShieldDuration + RTT`인 한 실질 영향 없음. **§3-0 시간제 버프 어빌리티의
  첫 소비자** — `CastTime = ShieldDuration`, `ActivationOwnedTags`에 `Shielded`, `CastChannelTag =
  State.Shielded`, `GetCastMoveSpeedMultiplier() = 1`, `bInterruptibleOnDamage = false`. 실드는
  속성 변경이 없어 `OnCastStarted`/`OnCastComplete`가 비고 태그만 남는다. `GE_ShieldOn` 제거.
  이번 구현에서 훅 두 개가 생기면 바로 할 수 있다 — 이번 범위엔 안 넣는다.
- **피격 취소는 이미 서버 판정이다 — 손댈 것 없음.** `TAG_Event_Damaged`는 서버의
  `PostGameplayEffectExecute`에서만 발송된다(`EPAttributeSet.cpp:76`, 피해 GE는 예측 안 됨).
  서버 `WaitGameplayEvent` → `EndAbility(bReplicateEndAbility=true, cancel)` →
  `ReplicateEndOrCancelAbility` → `ClientCancelAbility` → 클라 종료. 클라의 `WaitGameplayEvent`는
  울릴 일이 없다. `bServerRespectsRemoteAbilityCancellation`(클라→서버 취소 RPC 수용 여부)은
  이 경로와 무관. 양쪽 다 쿨다운을 안 찍으므로 로컬 타이머와도 불일치 없음. 클라가 취소를
  ~D 늦게 보는 건 서버 판정의 본질적 지연이라 수용. (2026-09-14, 앞선 "클라만 취소한다"는
  추정은 틀렸음 — 정정.)
- **Heal 캐스팅 CMC 버벅거림** — §4-5에서 같이 본다. 남으면 `Issue/CastMoveSpeedStutter_GECooldownPrediction.md` 갱신.
- **`State.Reloading` — `ActivationOwnedTags`로** (`04_Polish_WeaponFireRate.md` 첫 항목).
  `GE_Reloading`을 없애고 `EPGA_Item_Reload` 생성자에 `ActivationOwnedTags.AddTag(TAG_State_Reloading)`
  — 재장전 어빌리티가 재장전 내내 활성이라 정확히 맞는다. §3-4 `Casting`과 동일. `IsNetAuthority()`
  가드(`:65`)와 핸들 제거 코드도 같이 사라진다. `EPHUDWidget`은 태그 이벤트를 그대로 받으므로
  변경 없음.
- **무기 연사 쿨다운** — `04_Polish_WeaponFireRate.md`. `FEPLocalTimer`를 그대로 가져다
  쓴다(`UEPGA_Item_PrimaryUse`는 `UGameplayAbility` 직계라 상속으로는 못 받는다 — 값
  타입으로 뺀 이유). **단 위치는 어빌리티가 아니라 무기 인스턴스**(`AEPWeapon` 또는
  `UEPItemInstance`) — Lyra가 `TimeLastFired`를 `ULyraWeaponInstance`에 두는 것과 같다.
  무기 어빌리티는 InstancedPerActor라 무기를 바꿔도 인스턴스가 하나이므로, 어빌리티에
  두면 교체 직후 발사 간격이 이전 무기에 묶인다. 서버 재검증 여부는 탄약 Cost가 있어
  스킬과 다르게 판단할 수 있다(§3-6).
