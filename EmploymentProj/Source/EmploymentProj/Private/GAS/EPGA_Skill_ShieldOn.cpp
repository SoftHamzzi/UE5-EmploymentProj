// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Skill_ShieldOn.h"

#include "GAS/EPNativeGameplayTags.h"

UEPGA_Skill_ShieldOn::UEPGA_Skill_ShieldOn()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Shield);
	SetAssetTags(Tags);
	
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_Cooldown_Skill_Shield);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Shielded);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
}

void UEPGA_Skill_ShieldOn::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (GE_ShieldOnClass)
	{
		FGameplayEffectSpecHandle ShieldSpec = MakeOutgoingGameplayEffectSpec(GE_ShieldOnClass);
		ShieldSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Duration, ShieldDuration);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, ShieldSpec); 
	}
	
	if (GE_ShieldCooldownClass)
	{
		FGameplayEffectSpecHandle CDSpec = MakeOutgoingGameplayEffectSpec(GE_ShieldCooldownClass);
		CDSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, ShieldCooldown);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CDSpec);
	}
	
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}