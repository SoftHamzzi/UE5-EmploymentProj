// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/EPGA_Skill_Base.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Abilities/Tasks/AbilityTask_NetworkSyncPoint.h"
#include "Combat/EPCombatDeveloperSettings.h"
#include "Core/EPCharacter.h"
#include "Engine/World.h"
#include "GAS/EPAttributeSet.h"
#include "GAS/EPDurationMessage.h"
#include "GAS/EPNativeGameplayTags.h"
#include "GameplayPrediction.h"

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
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
		return false;
	
	if (UAbilitySystemGlobals::Get().ShouldIgnoreCooldowns()) return true;
	
	const AEPCharacter* Char = ActorInfo ? Cast<AEPCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWorld* World = Char ? Char->GetWorld() : nullptr;
	if (!Char || !World) return false;
	
	const float Now = World->GetTimeSeconds();
	const float Rate = Char->GetLocalModifiers().Product(EmpGameplayTags::TAG_Modifier_CooldownRate);
	const float Tolerance = ActorInfo->IsNetAuthority()
		? GetDefault<UEPCombatDeveloperSettings>()->ServerCooldownToleranceSeconds
		: 0.f;
	
	if (CooldownTimer.IsElapsed(Now, Rate, Tolerance)) return true;
	
	const FGameplayTag& FailTag = UAbilitySystemGlobals::Get().ActivateFailCooldownTag;
	if (OptionalRelevantTags && FailTag.IsValid()) OptionalRelevantTags->AddTag(FailTag);
	
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
	const FGameplayAbilityActivationInfo activationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (AEPCharacter* Char = ActorInfo ? Cast<AEPCharacter>(ActorInfo->AvatarActor.Get()) : nullptr)
		Char->GetLocalModifiers().Clear(EmpGameplayTags::TAG_Modifier_MoveSpeed_Casting);
	
	Super::EndAbility(Handle, ActorInfo, activationInfo, bReplicateEndAbility, bWasCancelled);
}

float UEPGA_Skill_Base::GetEffectiveCooldown() const
{
	float Flat = 0.f, Pct = 0.f;
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (const UEPAttributeSet* AS = Cast<const UEPAttributeSet>(ASC->GetAttributeSet(UEPAttributeSet::StaticClass())))
		{
			Flat = AS->GetCooldownFlatReduction();
			Pct = AS->GetCooldownPctReduction();
		}
	}
	
	return FMath::Max(0.f, (Cooldown - Flat) * (1.f - FMath::Clamp(Pct, 0.f, 1.f)));
}

void UEPGA_Skill_Base::CompleteCast()
{
	OnCastComplete();
	
	const float Duration = GetEffectiveCooldown();
	const float Now = GetWorld()->GetTimeSeconds();
	CooldownTimer.Start(Now, Duration);
	
	if (IsPredictingClient())
	{
		FPredictionKey Key = CurrentActivationInfo.GetActivationPredictionKey();
		Key.NewRejectedDelegate()
			.BindUObject(this, &UEPGA_Skill_Base::OnActivationRejected, CooldownTimer.GetGeneration());
	}
	
	BroadcastDurationMessage(CooldownChannelTag, Duration);
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
