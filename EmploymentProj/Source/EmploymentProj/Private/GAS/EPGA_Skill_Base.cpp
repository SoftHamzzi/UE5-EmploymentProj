// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/EPGA_Skill_Base.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GAS/EPDurationMessage.h"
#include "GAS/EPNativeGameplayTags.h"

UEPGA_Skill_Base::UEPGA_Skill_Base()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	bServerRespectsRemoteAbilityCancellation = false;
	
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Casting);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
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
		OnCastComplete();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}
	
	if (GE_CastingClass)
	{
		FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(GE_CastingClass);
		Spec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Duration, CastTime);
		ConfigureCastingSpec(Spec);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
	
	BroadcastDurationMessage(EmpGameplayTags::TAG_State_Casting, CastTime);
	
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
	if (ActorInfo->IsNetAuthority())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
				FGameplayTagContainer(EmpGameplayTags::TAG_State_Casting));
			ASC->RemoveActiveEffects(Query);
		}
	}
	
	Super::EndAbility(Handle, ActorInfo, activationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEPGA_Skill_Base::SetCooldownTag(FGameplayTag Tag)
{
	ActivationBlockedTags.AddTag(Tag);
	CooldownChannelTag = Tag;
}

void UEPGA_Skill_Base::ApplyCooldownGE()
{
	if (!GE_CooldownClass) return;
	
	FGameplayEffectSpecHandle CDSpec = MakeOutgoingGameplayEffectSpec(GE_CooldownClass);
	CDSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, Cooldown);
	ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, CDSpec);
	
	BroadcastDurationMessage(CooldownChannelTag, Cooldown);
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
	FScopedPredictionWindow ScopedPrediction(
		GetAbilitySystemComponentFromActorInfo(),
		CurrentActivationInfo.GetActivationPredictionKey());
	
	OnCastComplete();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEPGA_Skill_Base::OnDamageDuringCast(FGameplayEventData Payload)
{
	OnCastInterrupted();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
