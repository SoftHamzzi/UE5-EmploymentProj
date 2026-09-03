# Polish — 스킬 표시 (Cast / Cooldown / Active)

**상태:** 안 B(`NetworkSyncPoint`, §2)만 미적용. 나머지(메시지 버스 기반
표시 구조, 세 스킬 배선, 위젯 재설계)는 전부 코드에 적용돼 있다.

관련 소스: `EPGA_Skill_Base.h/.cpp`, `EPGA_Skill_Heal/Dash/ShieldOn.h/.cpp`,
`EPSkillSlotWidget.h/.cpp`, `EPCastGaugeWidget.h/.cpp`, `EPDurationMessage.h`.

---

## 1. 현재 아키텍처

**메시지 구조체** (`EPDurationMessage.h`):
```cpp
USTRUCT(BlueprintType)
struct FEPDurationMessage
{
    TObjectPtr<AActor> Instigator = nullptr;
    float Duration = 0.f;
};
```
`GameplayMessageRouter` 플러그인(`UGameplayMessageSubsystem`)으로 방송한다 —
프로세스 로컬 pub/sub이지 네트워크 리플리케이션이 아니다. 클라 실행 시
클라 로컬로, 서버 실행 시 서버 로컬로 각자 독립적으로 방송된다.

**`UEPGA_Skill_Base`가 세 채널의 공용 배선을 갖는다:**
- `SetCooldownTag(Tag)` — `ActivationBlockedTags`에 태그를 걸면서 동시에
  `CooldownChannelTag`(방송 채널)로도 저장. 두 값이 어긋날 수 없게 한 함수로 묶었다.
- `ApplyCooldownGE()` — 베이스의 `GE_CooldownClass`/`Cooldown` 필드를 읽어
  GE 적용 + `CooldownChannelTag`로 방송을 한 번에 한다(매개변수 없음).
- `BroadcastActiveDuration(Duration)` — `ActiveChannelTag`로 방송하는 얇은
  래퍼. 캐스팅 완료 후 지속되는 효과가 있는 스킬(현재 `ShieldOn` 하나)만
  생성자에서 `ActiveChannelTag`를 설정한다 — 없으면(`Invalid`) 조용히 아무것도
  안 한다.
- `ActivateAbility()`의 동기 구간(예측 창 안)에서 `GE_CastingClass` 적용
  직후 `BroadcastDurationMessage(TAG_State_Casting, CastTime)`을 호출 —
  캐스팅 슬로우 태그도, 캐스트 게이지 표시도 여기서 즉시(RTT 없이) 나간다.
- `EndAbility()`는 핸들이 아니라 태그 쿼리(`FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TAG_State_Casting)` + `RemoveActiveEffects`)로 캐스팅 GE를 지운다 —
  예측→확정 GE 스왑으로 핸들이 stale해져도 항상 지금 활성 상태인 캐스팅
  GE를 놓치지 않는다.

**세 스킬의 `OnCastComplete()`** (`OnCastTimerComplete()`에서 호출):
- `Heal` — 힐 GE 적용 → `ApplyCooldownGE()`
- `Dash` — 임펄스 적용 → `ApplyCooldownGE()`
- `ShieldOn` — 실드 GE 적용 + `BroadcastActiveDuration(ShieldDuration)` →
  `ApplyCooldownGE()`

**위젯은 GAS 태그를 직접 안 보고 메시지만 구독한다 — Cooldown/Active는
상태 전환과 값 둘 다 메시지가 결정한다(완전 분리):**
- `EPSkillSlotWidget`이 `CooldownChannelTag`/`ActiveChannelTag`로
  `RegisterListener<FEPDurationMessage>`, 받은 시점의 로컬 시계로
  `CooldownStartTime`/`ActiveStartTime` + 로컬 `FTimerHandle`로 종료 시점을 잡는다.
  `Locked` 상태만 예외 — 지속시간이 없어 여전히 GAS 태그(`LockTags`)를 직접 본다.
- `EPCastGaugeWidget`은 다르다 — **상태 전환(켜짐/꺼짐)은 태그로** 그대로
  판정한다(`CastChannelTag` 태그 이벤트, 이미 예측 창 안이라 지연이 없어서
  분리할 이유가 없다). **값 계산만** 메시지(`OnCastDurationMessage`)로 받은
  `GaugeStartTime`/`GaugeDuration`을 로컬 시계로 카운트다운 — 매 틱 ASC에서
  GE를 직접 조회하던 예전 방식(예측/확정 스왑과 맞물려 끊김 발생)을 대체했다.
- 모든 메시지 핸들러는 `Message.Instigator != ASC->GetAvatarActor()`로
  필터링한다 — `GameplayMessageSubsystem`이 게임 인스턴스 전역 pub/sub이라
  다른 플레이어의 방송도 같은 채널로 들어오기 때문에 필수.

**`EPSkillSlotWidget`은 Active 상태에서 남은 숫자를 표시하지 않는다**
(사용자 결정) — `ApplyState()`의 `bShowText`가 `Cooldown`일 때만 `true`,
`bShowBar`는 `Cooldown`/`Active` 둘 다 `true`. `Active` 진입 시
`CooldownBar->SetPercent(1.f)`로 고정해두고 그 뒤로 갱신하지 않는다.

**받아들이는 대가(의도적 트레이드오프):** 위젯이 이미 쿨다운/실드 중인
ASC에 나중에 붙는 경우(HUD 재생성, 재접속 등) 그 방송은 이미 지나갔으므로
`Ready`로 잘못 보인다. GAS가 쿨다운을 진짜로 예측하지 못한다는 근본 한계
(`GASDocumentation` §4.5.15.3 — 서버 확정 GE가 로컬 예측 GE를 지우고
교체하며, 그 시점은 지연시간에 따라 달라진다) 자체도 여전히 있다 — 이
설계는 "정확한 숫자"가 아니라 "매끄러움"을 목표로 한 것이라 이 한계를
없애지 않는다.

---

## 2. 남은 것 — 안 B (`UAbilityTask_NetworkSyncPoint`) 미적용

**증상:** `OnCastTimerComplete()`가 여전히 아래 코드 그대로다 —

```cpp
void UEPGA_Skill_Base::OnCastTimerComplete()
{
    FScopedPredictionWindow ScopedPrediction(
        GetAbilitySystemComponentFromActorInfo(),
        CurrentActivationInfo.GetActivationPredictionKey());

    OnCastComplete();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
```

이 2인자 생성자는 `IsNetSimulating() == false`일 때만 동작하는데, 소유
클라이언트도 `ROLE_AutonomousProxy`라 이 조건을 만족 못 한다 — **사실상
서버 전용이라 클라에서는 no-op이다.** 그 결과 `OnCastComplete()` 안에서
`ApplyGameplayEffectSpecToOwner`로 적용하는 GE(`Heal`의 힐 GE, `ShieldOn`의
실드 GE, 세 스킬의 쿨다운 GE)가 **클라에서 실제로는 하나도 예측 적용되지
않는다** — 서버가 확정해서 리플리케이트해줄 때까지(최소 RTT) 효과 자체가
클라 화면에 반영 안 된다.

**§1의 표시 구조는 이 문제와 무관하게 매끄럽다** — 위젯은 메시지만 보므로
숫자/바는 즉시 움직인다. 그러나 **실제 게임플레이 효과**(힐량 반영,
실드의 데미지 경감, 재발동 차단 태그)는 여전히 RTT만큼 늦게 적용된다 —
표시와 실제 상태가 그 사이 어긋난다.

**수정 — `UAbilityTask_NetworkSyncPoint`로 교체:**

```cpp
// EPGA_Skill_Base.h — private 섹션
UFUNCTION()
void OnCastSynced();
```

```cpp
// EPGA_Skill_Base.cpp
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"

void UEPGA_Skill_Base::OnCastTimerComplete()
{
    UAbilityTask_NetworkSyncPoint* Sync =
        UAbilityTask_NetworkSyncPoint::WaitNetSync(this, EAbilityTaskNetSyncType::OnlyServerWait);
    Sync->OnSync.AddDynamic(this, &UEPGA_Skill_Base::OnCastSynced);
    Sync->ReadyForActivation();
}

void UEPGA_Skill_Base::OnCastSynced()
{
    OnCastComplete();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
```

**왜 되는가(UE5.7 엔진 소스로 확인됨):** `UAbilityTask_NetworkSyncPoint::Activate()`가
1인자 `FScopedPredictionWindow(ASC, IsPredictingClient())`를 연다 — 이건
클라에서도 실제로 동작하는 생성자다(새 종속 예측 키 생성). `OnlyServerWait`이면
클라는 서버 신호를 기다리지 않고 그 자리에서 바로 `SyncFinished()`가 불려
`OnCastSynced()`가 **방금 연 예측 창 안에서 동기적으로** 실행된다 — 대기가
없으므로 표시가 RTT만큼 늦어지지도 않는다. 서버는 클라의 신호 RPC가
도착하면 2인자 생성자로 창을 열고 같은 함수를 돈다.

`#include "GameplayPrediction.h"`는 이 파일에서 `FScopedPredictionWindow`를
더 이상 직접 안 쓰게 되면 지운다.

**대가(수용):** 서버의 캐스팅 완료가 클라의 신호 RPC에 묶인다 — 조작된
클라가 신호를 안 보내면 서버가 그 스킬을 영원히 안 끝낸다. 다만 이건 그
클라 자신의 `ActivationBlockedTags.State.Casting`만 영원히 걸려서 **자기
자신만 손해 보는 자기 손해**일 뿐 다른 플레이어나 서버 상태에 영향을 주는
익스플로잇이 아니다 — 지금은 타임아웃 방어 코드를 만들지 않는다(`CLAUDE.md`
§2, 도달해도 득이 없는 분기).

**검증:**
1. 원격 클라에서 `Heal` 사용 — 캐스팅 완료 즉시(RTT 기다리지 않고) 힐량이
   반영되는지, 재발동 태그가 즉시 막히는지.
2. `ShieldOn` — 실드 완료 즉시 피해 경감(`EPAttributeSet.cpp`의
   `TAG_State_Shielded` 체크)이 적용되는지.
3. 호스트에서는 원래도 예측/확정 구분이 없어 회귀만 확인.

---

## 3. 그 외 미결정 — 신호 있을 때만

**재발동 자체의 핑 공정성.** GAS는 쿨다운을 진짜로 예측 못한다는 한계(§1)
때문에, 핑이 높은 플레이어는 로컬 표시가 끝나도 서버가 아직 쿨다운 중이라
재발동이 거절될 수 있다 — 이건 표시 문제가 아니라 실제 타이밍의 공정성
문제라 §2로도 안 풀린다. Epic 엔지니어도 GAS 쿨다운 GE 자체를 버리고
자체 북키핑(`Fortnite`가 실제로 쓰는 방식)을 권장했다는 근거가 있다
(`GASDocumentation` README.md:3352-3358) — 무기 발사 쪽(`04_Polish_WeaponFireRate.md`)은
이미 이 방향으로 갔다. 스킬 쪽은 실제로 체감 문제로 확인되기 전엔 손대지
않는다 — 지금 만들면 상상 속 확장점이다(`CLAUDE.md` §2).

방향만 남겨둔다: `GE_CooldownClass`를 버리고 `ApplyPingAdjustedCooldown(Tag, BaseCooldown)`
같은 헬퍼로 서버가 직접 잰 핑(`PlayerState::GetPingInMilliseconds()`)만큼
Duration을 깎아 `AddLooseGameplayTag`로 태그를 걸고 타이머로 해제하는 방식.
미결정: 보정 공식(`Ping/2`가 맞는지 실측 필요), 최소 쿨타임 하한, 핑이
쿨타임을 넘어설 때 힐 효과 자체를 취소할지.

**`GameplayMessageRouter` 플러그인 재사용.** 지금은 스킬 표시 하나를 위해
들여왔다 — 나중에 다른 시스템(팀원 상태 알림, 인터랙션 프롬프트 등)에서도
쓸 계획이 생기면 이 도입 비용이 더 정당화된다. 지금 당장 결정할 것 없음.
