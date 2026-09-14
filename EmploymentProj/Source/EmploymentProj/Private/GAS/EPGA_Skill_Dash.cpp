// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Skill_Dash.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/EPNativeGameplayTags.h"

UEPGA_Skill_Dash::UEPGA_Skill_Dash()
{
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Dash);
	SetAssetTags(Tags);
	
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_Cooldown_Skill_Dash);
}

void UEPGA_Skill_Dash::OnCastComplete()
{
	ACharacter* Char = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get());
	if (Char)
	{
		UCharacterMovementComponent* CMC = Char->GetCharacterMovement();
		FVector DashDir = CMC ? CMC->GetCurrentAcceleration().GetSafeNormal2D() : FVector::ZeroVector;
		if (DashDir.IsNearlyZero())
			DashDir = Char->GetActorForwardVector().GetSafeNormal2D();
		
		FVector LaunchVel = DashDir.GetSafeNormal() * DashImpulse;
		LaunchVel.Z = DashZBoost;
		Char->LaunchCharacter(LaunchVel, true, true);
	}
	
	if (GE_DashCooldownClass)
	{
		FGameplayEffectSpecHandle CDSpec = MakeOutgoingGameplayEffectSpec(GE_DashCooldownClass);
		CDSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, DashCooldown);
		ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, CDSpec);
	}
}