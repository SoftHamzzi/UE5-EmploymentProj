// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Skill_Heal.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "GAS/EPNativeGameplayTags.h"

UEPGA_Skill_Heal::UEPGA_Skill_Heal() {
	CastTime = 3.f;
	bInterruptibleOnDamage = true;
	
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Heal);
	SetAssetTags(Tags);
	
	SetCooldownTag(EmpGameplayTags::TAG_Cooldown_Skill_Heal);
}

void UEPGA_Skill_Heal::OnCastComplete()
{
	if (GE_HealClass)
	{
		FGameplayEffectSpecHandle HealSpec = MakeOutgoingGameplayEffectSpec(GE_HealClass);
		HealSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_HealAmount, HealAmount);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HealSpec);
	}
	
	ApplyCooldownGE();
}

void UEPGA_Skill_Heal::ConfigureCastingSpec(FGameplayEffectSpecHandle& SpecHandle)
{
	SpecHandle.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_MoveSpeedMultiplier, HealMoveSpeedMultiplier);
}
