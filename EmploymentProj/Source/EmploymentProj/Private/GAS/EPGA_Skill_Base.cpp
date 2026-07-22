// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/EPGA_Skill_Base.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GAS/EPNativeGameplayTags.h"

UEPGA_Skill_Base::UEPGA_Skill_Base()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Casting);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
}

void UEPGA_Skill_Base::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggereventData)
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
		CastingEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
	
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
			if (CastingEffectHandle.IsValid())
			{
				ASC->RemoveActiveGameplayEffect(CastingEffectHandle);
				CastingEffectHandle.Invalidate();
			}
		}
	}
	
	Super::EndAbility(Handle, ActorInfo, activationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEPGA_Skill_Base::OnCastTimerComplete()
{
	OnCastComplete();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEPGA_Skill_Base::OnDamageDuringCast(FGameplayEventData Payload)
{
	OnCastInterrupted();
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
