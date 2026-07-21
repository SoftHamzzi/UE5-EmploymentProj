// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Skill_Heal.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GAS/EPNativeGameplayTags.h"

UEPGA_Skill_Heal::UEPGA_Skill_Heal() {
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Heal);
	SetAssetTags(Tags);
	
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Healing);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_Cooldown_Skill_Heal);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
}

void UEPGA_Skill_Heal::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (GE_HealingClass)
	{
		FGameplayEffectSpecHandle HealingSpec = MakeOutgoingGameplayEffectSpec(GE_HealingClass);
		HealingSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Duration, HealDuration);
		HealingEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, HealingSpec);
	}
	
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, HealDuration);
	WaitTask->OnFinish.AddDynamic(this, &UEPGA_Skill_Heal::OnHealComplete);
	WaitTask->ReadyForActivation();
	
	UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, EmpGameplayTags::TAG_Event_Damaged, nullptr, false, true);
	EventTask->EventReceived.AddDynamic(this, &UEPGA_Skill_Heal::OnDamageTaken);
	EventTask->ReadyForActivation();
}

void UEPGA_Skill_Heal::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		if (ActorInfo->IsNetAuthority() && HealingEffectHandle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(HealingEffectHandle);
			HealingEffectHandle.Invalidate();
		}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEPGA_Skill_Heal::OnHealComplete()
{
	if (GE_HealClass)
	{
		FGameplayEffectSpecHandle HealSpec = MakeOutgoingGameplayEffectSpec(GE_HealClass);
		HealSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_HealAmount, HealAmount);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HealSpec);
	}
	if (GE_HealCooldownClass)
	{
		FGameplayEffectSpecHandle CDSpec = MakeOutgoingGameplayEffectSpec(GE_HealCooldownClass);
		CDSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, HealCooldown);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, CDSpec);
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEPGA_Skill_Heal::OnDamageTaken(FGameplayEventData Payload)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
