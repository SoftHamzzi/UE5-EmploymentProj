// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Skill_ShieldOn.h"

#include "GAS/EPNativeGameplayTags.h"

UEPGA_Skill_ShieldOn::UEPGA_Skill_ShieldOn()
{
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Shield);
	SetAssetTags(Tags);
	
	SetCooldownTag(EmpGameplayTags::TAG_Cooldown_Skill_Shield);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Shielded);
	ActiveChannelTag = EmpGameplayTags::TAG_State_Shielded;
}

void UEPGA_Skill_ShieldOn::OnCastComplete()
{
	if (GE_ShieldOnClass)
	{
		FGameplayEffectSpecHandle ShieldSpec = MakeOutgoingGameplayEffectSpec(GE_ShieldOnClass);
		ShieldSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Duration, ShieldDuration);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, ShieldSpec);
		BroadcastActiveDuration(ShieldDuration);
	}
	
	ApplyCooldownGE();
}
