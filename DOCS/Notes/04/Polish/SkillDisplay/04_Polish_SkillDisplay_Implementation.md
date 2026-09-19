# 04 Polish 구현서: 스킬 로컬 타이머 (쿨다운 · 캐스팅 잠금 · 쿨다운 감소)

> 기준 문서: `../04_Polish_SkillDisplay.md` (§3 설계, §3-0 한 줄 규칙)
> 전제 조건: `feature-gas-polish` 브랜치, 현재 코드 빌드 통과 상태
> 목표: **현재 코드에서 바로 따라 구현 가능한 순서** 제공. 코드는 사용자가 직접 작성한다
> 문서 우선순위: 구현 시 충돌하면 이 문서를 기준으로 한다. 설계 이유가 필요하면 기준 문서로

---

## 0. 구현 철학

1. **내가 시작한 시간 상태는 어빌리티가 든다** — 쿨다운·캐스팅은 GE가 아니라 어빌리티 인스턴스의 로컬 타이머와 `ActivationOwnedTags`
2. **핑 추정을 판정에 넣지 않는다** — 클라는 클라 값끼리, 서버는 서버 값끼리. 서버만 상수 허용 오차
3. **시계는 `World->GetTimeSeconds()`** — 값이 기계를 건너가지 않으므로 로컬 시계. `GetServerWorldTimeSeconds()`는 SSR 전용
4. **소비자 코드는 안 건드린다** — 위젯·`ActivationBlockedTags`·`EPAttributeSet`은 태그 카운터 맵과 메시지만 보므로 변경 없음
5. **롤백은 GAS 훅으로** — 서버 거절 시 `FPredictionKey::NewRejectedDelegate`, `EndAbility(bWasCancelled)`가 아님

### 하드코딩 금지

| 값 | 위치 |
|---|---|
| 서버 허용 오차 | `UEPCombatDeveloperSettings::ServerCooldownToleranceSeconds` (ini) |
| 쿨다운 기준값 | `UEPGA_Skill_Base::Cooldown` (BP 편집, 기존) |
| 캐스팅 이동 배율 | `UEPGA_Skill_Heal::HealMoveSpeedMultiplier` (기존) → `GetCastMoveSpeedMultiplier()` |
| 쿨다운 감소 | 속성 `CooldownFlatReduction` / `CooldownPctReduction` (GE로 변경) |
| 배율 저장소 키 | `EPNativeGameplayTags` `Modifier.*` |

---

## 1. 수정/추가 대상 파일

**신규:**
- `Public/GAS/EPLocalTimer.h` (헤더 전용)
- `Public/Core/EPLocalModifiers.h` (헤더 전용)

**수정:**
- `Public/GAS/EPNativeGameplayTags.h`, `Private/GAS/EPNativeGameplayTags.cpp`
- `Public/GAS/EPAttributeSet.h`, `Private/GAS/EPAttributeSet.cpp`
- `Public/Combat/EPCombatDeveloperSettings.h`
- `Public/Core/EPCharacter.h`
- `Public/GAS/EPGA_Skill_Base.h`, `Private/GAS/EPGA_Skill_Base.cpp` ← 핵심
- `Public/GAS/EPGA_Skill_Heal.h`, `Private/GAS/EPGA_Skill_Heal.cpp`
- `Private/GAS/EPGA_Skill_Dash.cpp`
- `Private/GAS/EPGA_Skill_ShieldOn.cpp`
- `Private/Movement/EPCharacterMovement.cpp`

**에디터:**
- `BP_GA_Skill_Heal/Dash/ShieldOn` — 사라진 필드 정리, 재저장
- `GE_Heal_Cooldown`, `GE_Dash_Cooldown`, `GE_Shield_Cooldown`, `GE_Healing` — 참조 끊고 삭제
- `Config/DefaultGame.ini` — `ServerCooldownToleranceSeconds` (선택)

**변경 없음 (확인용):** `EPSkillSlotWidget`, `EPCastGaugeWidget`, `EPHUDWidget`, `EPDurationMessage.h`, `EPAttributeSet::PostGameplayEffectExecute`, `GE_Heal`, `GE_ShieldOn`

---

## Step 1) `FEPLocalTimer` — `Public/GAS/EPLocalTimer.h`

```cpp
#pragma once

#include "CoreMinimal.h"

/**
 * 로컬 시계 기반 타이머. 복제하지 않는다 — 클라는 클라 값끼리, 서버는 서버 값끼리만 비교한다.
 * 남은 시간(Remaining) 모델: 조회 시점에 경과를 계산하므로 틱이 없고, Rate(배율)를 조회자가 넘긴다.
 */
struct FEPLocalTimer
{
	/** 새 타이머 시작. 이전 상태를 보관해 서버 거절 시 Revert()로 되돌린다. */
	void Start(float Now, float Duration)
	{
		PrevRemaining  = Remaining;
		PrevLastUpdate = LastUpdate;
		bPrevStarted   = bStarted;

		Remaining  = FMath::Max(0.f, Duration);
		LastUpdate = Now;
		bStarted   = true;
		++Generation;
	}

	/** 배율이 바뀌기 직전에 호출 — 지금까지 경과를 옛 배율로 적립하고 기준 시각을 Now로 옮긴다. */
	void Bank(float Now, float RateSoFar)
	{
		if (!bStarted) return;
		Remaining  = GetRemaining(Now, RateSoFar);
		LastUpdate = Now;
	}

	/** 한 번도 Start 안 했으면 0. */
	float GetRemaining(float Now, float Rate) const
	{
		if (!bStarted) return 0.f;
		return FMath::Max(0.f, Remaining - (Now - LastUpdate) * FMath::Max(0.f, Rate));
	}

	bool IsElapsed(float Now, float Rate, float Tolerance) const
	{
		return GetRemaining(Now, Rate) <= Tolerance;
	}

	/** 서버 거절 롤백. Start 때 받은 Generation과 다르면(이미 다음 활성화가 찍었으면) 아무것도 안 한다. */
	void Revert(uint32 StartedGeneration)
	{
		if (StartedGeneration != Generation) return;
		Remaining  = PrevRemaining;
		LastUpdate = PrevLastUpdate;
		bStarted   = bPrevStarted;
	}

	uint32 GetGeneration() const { return Generation; }

private:
	float  Remaining  = 0.f;
	float  LastUpdate = 0.f;
	bool   bStarted   = false;

	float  PrevRemaining  = 0.f;
	float  PrevLastUpdate = 0.f;
	bool   bPrevStarted   = false;

	uint32 Generation = 0;
};
```

**왜 Generation인가.** 서버 거절 델리게이트는 활성화 시점의 예측 키에 묶인다. 거절이 아주
늦게(다음 활성화 뒤에) 도착하면 다음 활성화의 스탬프를 되돌리게 된다. `Start`마다 세대를
올리고 델리게이트에 그 세대를 실어 보내면 옛 거절은 무시된다.

**확인:** 헤더만 추가한 상태로 컴파일 통과 (아직 아무도 안 씀).

---

## Step 2) `FEPLocalModifiers` — `Public/Core/EPLocalModifiers.h` + `AEPCharacter` 멤버

### 2-1. 헤더

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * 여러 어빌리티가 함께 보는 로컬 배율 저장소. 복제하지 않는다.
 * 키 = Modifier.<카테고리>.<소스>. 카테고리가 소비자(CMC, 쿨다운), 소스가 쓰는 쪽(Casting, Haste …).
 * 양쪽 어빌리티 코드가 각자 Set/Clear 한다 — 클라는 예측 시점에, 서버는 자기 실행 시점에.
 */
struct FEPLocalModifiers
{
	void Set(FGameplayTag Source, float Value)
	{
		Values.FindOrAdd(Source) = Value;
	}

	void Clear(FGameplayTag Source)
	{
		Values.Remove(Source);
	}

	/** Category 아래 모든 소스의 곱. 없으면 1. 음수는 0으로 클램프. */
	float Product(FGameplayTag Category) const
	{
		float P = 1.f;
		for (const TPair<FGameplayTag, float>& It : Values)
		{
			if (It.Key.MatchesTag(Category))
				P *= FMath::Max(0.f, It.Value);
		}
		return P;
	}

private:
	TMap<FGameplayTag, float> Values;
};
```

### 2-2. `Public/Core/EPCharacter.h`

`#include "Core/EPLocalModifiers.h"` 추가 (값 멤버라 전방 선언으로는 안 됨 — 헤더 전용
구조체 하나라 include 비용이 없다). `public:` 함수 블록에:

```cpp
	FEPLocalModifiers& GetLocalModifiers() { return LocalModifiers; }
	const FEPLocalModifiers& GetLocalModifiers() const { return LocalModifiers; }
```

`private:` 변수 블록에:

```cpp
	// 로컬 배율 저장소 (복제X). 어빌리티가 Set/Clear, CMC·스킬 베이스가 Product로 읽는다.
	FEPLocalModifiers LocalModifiers;
```

`UPROPERTY` 아님 — UObject 참조가 없는 값 타입이라 GC와 무관.

**확인:** 컴파일 통과.

---

## Step 3) 태그 — `EPNativeGameplayTags.h/.cpp`

`.h` (`namespace EmpGameplayTags` 안, State 블록 뒤):

```cpp
	// Modifier — FEPLocalModifiers 키. 카테고리 태그는 Product()의 인자, 리프 태그는 Set/Clear의 키
	EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Modifier_MoveSpeed)
	EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Modifier_MoveSpeed_Casting)
	EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Modifier_CooldownRate)
```

`.cpp`:

```cpp
	// Modifier
	UE_DEFINE_GAMEPLAY_TAG(TAG_Modifier_MoveSpeed,          "Modifier.MoveSpeed")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Modifier_MoveSpeed_Casting,  "Modifier.MoveSpeed.Casting")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Modifier_CooldownRate,       "Modifier.CooldownRate")
```

`Modifier.CooldownRate` 아래 소스 태그는 지금 없다 — 가속 버프가 생길 때 그 어빌리티가
`Modifier.CooldownRate.<이름>`을 정의한다.

**그대로 두는 태그:** `Cooldown.Skill.*`(방송 채널로 계속 씀), `State.Casting`, `Data.Cooldown`,
`Data.Duration`(`GE_ShieldOn`이 씀), `Data.MoveSpeedMultiplier`(이번 변경 후 미사용 — 지우지
말고 그대로. 무기 폴리시에서 같이 정리).

**확인:** 컴파일 통과. 에디터 Gameplay Tags 목록에 `Modifier.*` 3개 보임.

---

## Step 4) 속성 2개 — `EPAttributeSet.h/.cpp`

`.h` `MoveSpeedMultiplier` 블록 뒤:

```cpp
	// --- Cooldown ---
	// 시전 시점에 한 번 읽어 쿨다운 길이를 정한다. 영구 성장·장비용 — Instant GE로만 바꾼다 (Duration GE 금지, 기준 문서 §3-2)
	UPROPERTY(BlueprintReadOnly, Category = "Attribute|Cooldown", ReplicatedUsing = OnRep_CooldownFlatReduction)
	FGameplayAttributeData CooldownFlatReduction;
	ATTRIBUTE_ACCESSORS(UEPAttributeSet, CooldownFlatReduction);

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|Cooldown", ReplicatedUsing = OnRep_CooldownPctReduction)
	FGameplayAttributeData CooldownPctReduction;
	ATTRIBUTE_ACCESSORS(UEPAttributeSet, CooldownPctReduction);
```

`protected:` OnRep 선언 2개 추가 (기존 패턴 그대로):

```cpp
	UFUNCTION()
	void OnRep_CooldownFlatReduction(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_CooldownPctReduction(const FGameplayAttributeData& OldValue);
```

`.cpp`:

```cpp
// PreAttributeChange 안
	if (Attribute == GetCooldownFlatReductionAttribute())
		NewValue = FMath::Max(NewValue, 0.f);
	if (Attribute == GetCooldownPctReductionAttribute())
		NewValue = FMath::Clamp(NewValue, 0.f, 1.f);

// GetLifetimeReplicatedProps 안 — 기존 5줄과 같은 조건
	DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, CooldownFlatReduction, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, CooldownPctReduction,  COND_OwnerOnly, REPNOTIFY_Always);

// OnRep 2개 — 기존 패턴 그대로
void UEPAttributeSet::OnRep_CooldownFlatReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, CooldownFlatReduction, OldValue);
}
void UEPAttributeSet::OnRep_CooldownPctReduction(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, CooldownPctReduction, OldValue);
}
```

초기값은 `FGameplayAttributeData` 기본 0 — 감소 없음이 중립값이라 초기화 GE가 필요 없다.

**확인:** 컴파일 통과. PIE에서 `showdebug abilitysystem`에 두 속성이 0으로 보임.

---

## Step 5) 서버 허용 오차 — `EPCombatDeveloperSettings.h`

```cpp
	// 서버가 스킬 쿨다운을 재검증할 때 이만큼 일찍 온 활성화를 허용한다 (초).
	// 캐스트 완료 신호 RPC와 다음 활성화 RPC의 상행 지터 차이를 흡수한다. 클라는 0 고정.
	UPROPERTY(Config, EditAnywhere, Category="GAS|Cooldown", meta=(ClampMin="0.0", ClampMax="0.5"))
	float ServerCooldownToleranceSeconds = 0.1f;
```

`DefaultGame.ini`에 `[/Script/EmploymentProj.EPCombatDeveloperSettings]` 섹션이 이미 있으면
그 아래 `ServerCooldownToleranceSeconds=0.1` (선택 — 기본값과 같으면 생략).

**확인:** Project Settings → Game → EP Combat Developer Settings에 항목 보임.

---

## Step 6) `EPGA_Skill_Base.h`

전체 교체. 바뀐 줄엔 `// ←` 표시.

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/EPLocalTimer.h"                                        // ←
#include "EPGA_Skill_Base.generated.h"

class AEPCharacter;                                                   // ←

UCLASS(Abstract)
class EMPLOYMENTPROJ_API UEPGA_Skill_Base : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEPGA_Skill_Base();

	virtual bool CanActivateAbility(                                  // ←
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override final;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	/** 쿨다운 가속 배율이 바뀌기 직전에 부른다 — 옛 배율로 경과를 적립. (가속 버프 어빌리티용, 지금 호출자 없음) */
	void BankCooldown(float Now, float RateSoFar) { CooldownTimer.Bank(Now, RateSoFar); }   // ←

protected:
	// === 변수 ===
	// --- Cast ---
	UPROPERTY(EditDefaultsOnly, Category = "Cast")
	float CastTime = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Cast")
	bool bInterruptibleOnDamage = false;

	// 캐스트 게이지 방송 채널. 기본 State.Casting. 시간제 버프는 자기 Active 채널로 바꾼다 (기준 문서 §3-0)
	UPROPERTY(EditDefaultsOnly, Category = "Cast", meta = (Categories = "State"))   // ←
	FGameplayTag CastChannelTag;                                      // ←

	// --- Cooldown ---
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
	float Cooldown = 0.f;

	// 쿨다운 방송 채널 (= 예전 SetCooldownTag의 태그). 서브클래스 생성자에서 대입. 더 이상 ActivationBlockedTags에 안 들어간다
	FGameplayTag CooldownChannelTag;                                  // ← protected로 이동

	// --- Active ---
	FGameplayTag ActiveChannelTag;

	// === 함수 ===
	// --- Cast ---
	virtual void OnCastStarted() {}                                   // ← 시간제 버프용. CastTime > 0 분기에서 태그·배율 직후
	virtual void OnCastComplete() PURE_VIRTUAL(UEPGA_Skill_Base::OnCastComplete, );
	virtual void OnCastInterrupted() {}
	/** 캐스트 중 이동 배율. 1이면 변화 없음. Heal만 오버라이드 */
	virtual float GetCastMoveSpeedMultiplier() const { return 1.f; } // ← ConfigureCastingSpec 대체

	// --- Cooldown ---
	/** 시전 시점에 한 번 — Cooldown에 속성 Flat/Pct 감소를 적용한 값 */
	float GetEffectiveCooldown() const;                               // ←

	// --- Active ---
	void BroadcastActiveDuration(float Duration);

private:
	// === 변수 ===
	FEPLocalTimer CooldownTimer;                                      // ←

	// === 함수 ===
	/** OnCastComplete → 쿨다운 스탬프 → 방송 → EndAbility. CastTime<=0 분기와 OnCastSynced 둘 다 여기로 */
	void CompleteCast();                                              // ←

	UFUNCTION()
	void OnCastTimerComplete();

	UFUNCTION()
	void OnCastSynced();

	UFUNCTION()
	void OnDamageDuringCast(FGameplayEventData Payload);

	/** 서버가 활성화를 거절했을 때 (예측 키 Rejected 델리게이트) */
	void OnActivationRejected(uint32 StartedGeneration);              // ←

	AEPCharacter* GetEPCharacter() const;                             // ←
	void BroadcastDurationMessage(FGameplayTag Channel, float Duration);
};
```

**지운 것:** `GE_CastingClass`, `GE_CooldownClass`, `SetCooldownTag()`, `ApplyCooldownGE()`,
`ConfigureCastingSpec()`. `CooldownChannelTag`는 private → protected(서브클래스가 직접 대입).

**`CastChannelTag`의 `meta = (Categories = "State")`** — 에디터 태그 피커가 `State.*`만 보여준다.

---

## Step 7) `EPGA_Skill_Base.cpp`

전체 교체.

```cpp
#include "GAS/EPGA_Skill_Base.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "Combat/EPCombatDeveloperSettings.h"
#include "Core/EPCharacter.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayPrediction.h"
#include "GAS/EPAttributeSet.h"
#include "GAS/EPDurationMessage.h"
#include "GAS/EPNativeGameplayTags.h"

UEPGA_Skill_Base::UEPGA_Skill_Base()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	bServerRespectsRemoteAbilityCancellation = false;

	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Casting);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);

	CastChannelTag = EmpGameplayTags::TAG_State_Casting;
}

bool UEPGA_Skill_Base::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, OUT FGameplayTagContainer* OptionalRelevantTags) const
{
	// Casting(ActivationOwnedTags) / Dead / Shielded 태그 차단은 엔진 검사 그대로
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
		return false;

	// 콘솔 치트 AbilitySystem.IgnoreCooldowns 존중 — 엔진 CanActivateAbility와 같은 순서
	if (UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns())
		return true;

	const AEPCharacter* Char = ActorInfo ? Cast<AEPCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWorld* World = Char ? Char->GetWorld() : nullptr;
	if (!Char || !World)
		return false;

	const float Now       = World->GetTimeSeconds();
	const float Rate      = Char->GetLocalModifiers().Product(EmpGameplayTags::TAG_Modifier_CooldownRate);
	const float Tolerance = ActorInfo->IsNetAuthority()
		? GetDefault<UEPCombatDeveloperSettings>()->ServerCooldownToleranceSeconds
		: 0.f;

	if (CooldownTimer.IsElapsed(Now, Rate, Tolerance))
		return true;

	// 엔진 CheckCooldown과 같은 실패 사유 태그. ini의 ActivateFailCooldownName이 비어 있으면 Invalid → 아무 일도 안 함
	const FGameplayTag& FailTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;
	if (OptionalRelevantTags && FailTag.IsValid())
		OptionalRelevantTags->AddTag(FailTag);
	return false;
}

void UEPGA_Skill_Base::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (CastTime <= 0.f)
	{
		CompleteCast();
		return;
	}

	// State.Casting 태그는 엔진이 PreActivate에서 ActivationOwnedTags로 이미 붙였다 (Heal 생성자)
	if (AEPCharacter* Char = GetEPCharacter())
		Char->GetLocalModifiers().Set(EmpGameplayTags::TAG_Modifier_MoveSpeed_Casting, GetCastMoveSpeedMultiplier());

	OnCastStarted();
	BroadcastDurationMessage(CastChannelTag, CastTime);

	UAbilityTask_WaitDelay* WaitDelay = UAbilityTask_WaitDelay::WaitDelay(this, CastTime);
	WaitDelay->OnFinish.AddDynamic(this, &UEPGA_Skill_Base::OnCastTimerComplete);
	WaitDelay->ReadyForActivation();

	if (bInterruptibleOnDamage)
	{
		UAbilityTask_WaitGameplayEvent* WaitDamage = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this, EmpGameplayTags::TAG_Event_Damaged, nullptr, false, true);
		WaitDamage->EventReceived.AddDynamic(this, &UEPGA_Skill_Base::OnDamageDuringCast);
		WaitDamage->ReadyForActivation();
	}
}

void UEPGA_Skill_Base::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 모든 종료 경로(완료·피격 취소·서버 거절)에서, 양쪽 다. IsNetAuthority 가드 없음 — 클라도 자기 것을 지운다
	if (AEPCharacter* Char = ActorInfo ? Cast<AEPCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
		Char->GetLocalModifiers().Clear(EmpGameplayTags::TAG_Modifier_MoveSpeed_Casting);

	// State.Casting 태그 제거는 Super가 한다 (ActivationOwnedTags)
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

float UEPGA_Skill_Base::GetEffectiveCooldown() const
{
	float Flat = 0.f, Pct = 0.f;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (const UEPAttributeSet* AS = Cast<const UEPAttributeSet>(ASC->GetAttributeSet(UEPAttributeSet::StaticClass())))
		{
			Flat = AS->GetCooldownFlatReduction();
			Pct  = AS->GetCooldownPctReduction();
		}
	}
	return FMath::Max(0.f, (Cooldown - Flat) * (1.f - FMath::Clamp(Pct, 0.f, 1.f)));
}

void UEPGA_Skill_Base::CompleteCast()
{
	OnCastComplete();                                                    // 서브클래스: 효과만

	const float Duration = GetEffectiveCooldown();
	const float Now = GetWorld()->GetTimeSeconds();
	CooldownTimer.Start(Now, Duration);

	// 서버 거절 시 롤백. K2_EndAbility(bWasCancelled=false)로 오고 즉시 발동형은 이미 끝나 있어 EndAbility로는 못 받는다 —
	// GAS가 예측 GE를 되돌릴 때 쓰는 훅과 같은 것 (GameplayPrediction.h:88)
	if (IsPredictingClient())
	{
		// GetActivationPredictionKey()는 const&를 돌려주고 NewRejectedDelegate()는 비-const → 키를 값으로 복사해서 부른다.
		// 내부는 Current 값으로만 등록하므로 복사본이어도 같다 (GameplayPrediction.cpp:236-238).
		// static FPredictionKeyDelegates::NewRejectedDelegate는 UE_API가 없어 게임 모듈에서 링크 불가 (LNK2019).
		FPredictionKey Key = CurrentActivationInfo.GetActivationPredictionKey();
		Key.NewRejectedDelegate()
			.BindUObject(this, &UEPGA_Skill_Base::OnActivationRejected, CooldownTimer.GetGeneration());
	}

	BroadcastDurationMessage(CooldownChannelTag, Duration);              // 표시. 감소분 반영된 값
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEPGA_Skill_Base::OnActivationRejected(uint32 StartedGeneration)
{
	CooldownTimer.Revert(StartedGeneration);
}

void UEPGA_Skill_Base::BroadcastActiveDuration(float Duration)
{
	BroadcastDurationMessage(ActiveChannelTag, Duration);
}

void UEPGA_Skill_Base::BroadcastDurationMessage(FGameplayTag Channel, float Duration)
{
	if (!Channel.IsValid() || !CurrentActorInfo) return;

	FEPDurationMessage Message;
	Message.Instigator = CurrentActorInfo->AvatarActor.Get();
	Message.Duration = Duration;

	UGameplayMessageSubsystem::Get(CurrentActorInfo->AvatarActor.Get())
		.BroadcastMessage(Channel, Message);
}

void UEPGA_Skill_Base::OnCastTimerComplete()
{
	UAbilityTask_NetworkSyncPoint* Sync =
		UAbilityTask_NetworkSyncPoint::WaitNetSync(this, EAbilityTaskNetSyncType::OnlyServerWait);
	Sync->OnSync.AddDynamic(this, &UEPGA_Skill_Base::OnCastSynced);
	Sync->ReadyForActivation();
}

void UEPGA_Skill_Base::OnCastSynced()
{
	CompleteCast();
}

void UEPGA_Skill_Base::OnDamageDuringCast(FGameplayEventData Payload)
{
	OnCastInterrupted();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

AEPCharacter* UEPGA_Skill_Base::GetEPCharacter() const
{
	return CurrentActorInfo ? Cast<AEPCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}
```

**읽을 때 주의할 곳 세 군데:**
- `CanActivateAbility`는 `const`라 타이머를 **읽기만** 한다 — `Bank`는 여기서 안 부른다.
- `CompleteCast`의 `IsPredictingClient()` — 캐스트형(Heal 3초)은 완료 시점에 이미 서버 확인이
  와서 `false`(거절 불가능, 바인드 불필요). 즉시 발동형은 아직 `Predicting`이라 `true`.
- `EndAbility`의 `Clear`는 멱등 — 스코프락으로 `Super`가 지연돼 두 번 와도 문제없다.

**확인:** 이 시점엔 세 스킬이 `SetCooldownTag`/`ApplyCooldownGE`/`ConfigureCastingSpec`을 아직
불러서 컴파일 실패 — Step 8로.

---

## Step 8) 세 스킬 정리

### 8-1. `EPGA_Skill_Heal.h`

```cpp
	// === 함수 ===
	virtual void OnCastComplete() override;
	virtual float GetCastMoveSpeedMultiplier() const override { return HealMoveSpeedMultiplier; }   // ← ConfigureCastingSpec 대체
```

`ConfigureCastingSpec` 선언 삭제.

### 8-2. `EPGA_Skill_Heal.cpp`

```cpp
UEPGA_Skill_Heal::UEPGA_Skill_Heal()
{
	CastTime = 3.f;
	bInterruptibleOnDamage = true;

	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Heal);
	SetAssetTags(Tags);

	CooldownChannelTag = EmpGameplayTags::TAG_Cooldown_Skill_Heal;              // ← SetCooldownTag 대체
	ActivationOwnedTags.AddTag(EmpGameplayTags::TAG_State_Casting);            // ← 캐스트 중 오너에 붙는 태그. CastTime > 0 스킬만
}

void UEPGA_Skill_Heal::OnCastComplete()
{
	if (GE_HealClass)
	{
		FGameplayEffectSpecHandle HealSpec = MakeOutgoingGameplayEffectSpec(GE_HealClass);
		HealSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_HealAmount, HealAmount);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HealSpec);
	}
	// ApplyCooldownGE() 삭제 — 베이스 CompleteCast가 찍는다
}
```

`ConfigureCastingSpec` 정의 삭제. 이제 안 쓰는 include(`AbilityTask_WaitDelay.h`,
`AbilityTask_WaitGameplayEvent.h`)는 이전부터 미사용이었다 — 그대로 둔다(CLAUDE.md §3).

### 8-3. `EPGA_Skill_Dash.cpp`

```cpp
UEPGA_Skill_Dash::UEPGA_Skill_Dash()
{
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Dash);
	SetAssetTags(Tags);

	CooldownChannelTag = EmpGameplayTags::TAG_Cooldown_Skill_Dash;              // ←
}
```

`OnCastComplete()` 마지막 줄 `ApplyCooldownGE();` 삭제. `ActivationOwnedTags`는 **넣지 않는다**
— 즉시 발동형에 넣으면 같은 프레임에 붙었다 떨어져 게이지가 한 프레임 깜빡인다.

### 8-4. `EPGA_Skill_ShieldOn.cpp`

```cpp
UEPGA_Skill_ShieldOn::UEPGA_Skill_ShieldOn()
{
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Shield);
	SetAssetTags(Tags);

	CooldownChannelTag = EmpGameplayTags::TAG_Cooldown_Skill_Shield;            // ←
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Shielded);         // 유지 (기준 문서 §5)
	ActiveChannelTag = EmpGameplayTags::TAG_State_Shielded;
}
```

`OnCastComplete()` 마지막 줄 `ApplyCooldownGE();` 삭제. `GE_ShieldOnClass`·`BroadcastActiveDuration`은
그대로 — 실드 자체를 옮기는 건 이번 범위 밖.

**확인:** 컴파일 통과 (Step 9 전에도 통과해야 한다).

---

## Step 9) `EPCharacterMovement::GetMaxSpeed()`

```cpp
float UEPCharacterMovement::GetMaxSpeed() const {
	float Base = Super::GetMaxSpeed();
	if (bWantsToSprint && IsMovingOnGround() && !IsCrouching()) Base = SprintSpeed;
	else if (bWantsToAim) Base = AimSpeed;

	float Multiplier = 1.f;
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
		Multiplier = ASC->GetNumericAttribute(UEPAttributeSet::GetMoveSpeedMultiplierAttribute());

	// 시간제 배율(캐스팅 슬로우 등)은 속성이 아니라 로컬 저장소 — 양쪽이 각자 걸었다 떼서 제거 지연이 없다
	if (const AEPCharacter* EPChar = Cast<AEPCharacter>(GetOwner()))                          // ←
		Multiplier *= EPChar->GetLocalModifiers().Product(EmpGameplayTags::TAG_Modifier_MoveSpeed);   // ←

	return Base * Multiplier;
}
```

include: `Core/EPCharacter.h`, `GAS/EPNativeGameplayTags.h` (없으면 추가).

속성 `MoveSpeedMultiplier`는 남는다 — 영구 성장·장비용. 단 `GE_Healing`이 사라지면 이
속성을 바꾸는 GE가 지금은 없다(값 1.0 유지).

**확인:** 컴파일 통과.

---

## Step 10) 에셋 정리 (에디터)

순서 중요 — 참조를 먼저 끊고 삭제한다.

1. `BP_GA_Skill_Heal` / `Dash` / `ShieldOn` 열기 → 컴파일. 사라진 C++ 필드(`GE_CastingClass`,
   `GE_CooldownClass`)는 자동으로 떨어진다. `Cooldown` 값·`CastTime`이 BP에서 덮여 있으면
   그대로. **`CastChannelTag`가 `State.Casting`인지 확인**(C++ 생성자 기본값). 저장.
2. `EPCastGaugeWidget` BP의 `CastChannelTag`가 `State.Casting`인지 확인 — 변경 없음.
3. 콘텐츠 브라우저에서 아래 4개 **Reference Viewer**로 참조 0 확인 후 삭제:
   `GE_Heal_Cooldown`, `GE_Dash_Cooldown`, `GE_Shield_Cooldown`, `GE_Healing`
   (`GE_Heal`은 Instant 힐 — **삭제 금지**. `GE_ShieldOn`도 유지.)
4. Project Settings → Gameplay Tags에서 `Cooldown.Skill.*`·`State.Casting`은 그대로(네이티브).

**확인:** 에디터 재시작 후 로그에 "Failed to load" 없음.

---

## Step 11) 빌드 확인

```
UnrealBuildTool.exe EmploymentProj Win64 Development -project="EmploymentProj/EmploymentProj.uproject"
```

경고로 나올 수 있는 것: `ActivationOwnedTags` 관련 없음. `ConfigureCastingSpec`을 BP에서
오버라이드한 적이 있으면 BP 컴파일 에러 — 해당 노드 삭제.

---

## Step 12) PIE 검증

기준 문서 §4 그대로. 검증용 임시 로그를 `CompleteCast()`에 넣고 끝나면 지운다:

```cpp
	UE_LOG(LogTemp, Log, TEXT("[SkillCD] %s %s Start now=%.3f dur=%.2f gen=%u"),
		*GetName(), CurrentActorInfo->IsNetAuthority() ? TEXT("SV") : TEXT("CL"), Now, Duration, CooldownTimer.GetGeneration());
```

`CanActivateAbility` 실패 지점에도 같은 형식으로 `Remaining` 출력.

설정: PIE Net Mode = Play As Client, 클라 2, 콘솔 `NetEmulation.PktLag 100` (원격 클라 창에서).

| # | 확인 | 통과 기준 |
|---|---|---|
| 1 | 재발동 타이밍 — 카운트다운 0 순간 재발동 ×5 | 서버 로그에 `ClientActivateAbilityFailed` 0회. 1회라도 나오면 `Tolerance` 올리기 전에 로그의 `Remaining` 값 기록 |
| 2 | 되감기 — 5→4.5→5 | 없음 |
| 3 | 연속 발동 간격 — 서버 `[SkillCD] SV Start` 두 줄의 `now` 차 | `Cooldown ± Tolerance` |
| 4 | 캐스팅 잠금 — Heal 종료 직후 Dash | 즉시 발동. 게이지가 종료와 동시에 꺼짐 |
| 5 | 캐스팅 이동 — Heal 종료 시 `p.NetShowCorrections 1` | 정정 ≤ 1회 (전엔 2회, 가설). 남은 1회는 서버 U — 범위 밖 |
| 6 | 쿨다운 감소 — 콘솔 치트 GE 또는 `showdebug abilitysystem`으로 `CooldownPctReduction=0.5` 넣기 | 표시·게이트 둘 다 절반 |
| 7 | 회귀 — Heal 완료 즉시 힐량, ShieldOn 완료 즉시 경감 | 이전과 동일 |
| 8 | 호스트 — 리슨 서버에서 세 스킬 | 정상 |

6번의 Rate(2배 속도)는 호출자가 없어 지금 검증 불가 — 가속 버프가 생길 때 `BankCooldown`
계약과 함께 검증.

---

## 함정표 (구현 수준)

| 함정 | 대응 |
|---|---|
| `CanActivateAbility`에서 `Char->GetWorld()`가 null (스펙 부여 직후 첫 프레임) | `false` 반환. 로컬 시계로 폴백하지 않는다 — 시계가 하나뿐이라 섞일 건 없지만, World 없이 활성화할 이유도 없다 |
| `FPredictionKeyDelegates::NewRejectedDelegate(Key)` static을 쓰면 **LNK2019** — `UE_API` 미익스포트 | 멤버 `FPredictionKey::NewRejectedDelegate()`를 키 **복사본**에서 부른다 (Step 7 코드) |
| `NewRejectedDelegate`에 payload로 `uint32` 넘기기 | `DECLARE_DELEGATE(FPredictionKeyEvent)`는 `TDelegate<void()>` — `BindUObject(Obj, Func, Payload)` 형식이 지원된다. 컴파일 안 되면 `BindWeakLambda(this, [this, Gen]{ OnActivationRejected(Gen); })` |
| 거절이 다음 활성화 뒤에 도착 | `Generation` 불일치 → `Revert` no-op. 로그로 `gen` 확인 |
| 즉시 발동형(Dash)에 `ActivationOwnedTags` | 넣지 않는다 (Step 8-3) |
| `EndAbility`가 `ActorInfo == nullptr`로 올 수 있나 | 엔진은 항상 유효한 포인터를 넘기지만 방어적으로 null 체크 — 위 코드 그대로 |
| BP에서 `Cooldown`을 덮었는데 값이 안 바뀌는 것 같다 | `GetEffectiveCooldown()`은 `Cooldown` 필드를 읽으므로 BP 값이 적용된다. 속성 두 개가 0인지 `showdebug abilitysystem`으로 확인 |
| `GE_Healing` 삭제 후 `MoveSpeedMultiplier` 속성 초기값 | 초기화 GE가 따로 있었는지 확인 — `GE_Healing`이 초기화까지 겸했다면 이동 속도가 0.05로 클램프될 수 있다. 값이 1.0인지 `showdebug abilitysystem` |
| 리슨 서버 호스트 | `IsNetAuthority()` 참 → `Tolerance` 적용. 데디 서버 목표라 무시 |

---

## 완료 후

- `../../Status/04_Polish_STATUS.md`에 항목 추가 — **§4 표 8개 전부 통과한 뒤에만** "완료".
  통과 전엔 "구현 완료, PIE 미검증"으로.
- 기준 문서 `04_Polish_SkillDisplay.md` 상태 줄 갱신, §1 "현재 구조"를 새 코드 기준으로 다시 씀
  (`GE_CastingClass`·`ApplyCooldownGE` 언급 제거).
- `Issue/SkillCooldown_GECooldownPrediction.md`에 "해결됨 — `Polish/SkillDisplay/`" 한 줄.
- 임시 `[SkillCD]` 로그 제거.
