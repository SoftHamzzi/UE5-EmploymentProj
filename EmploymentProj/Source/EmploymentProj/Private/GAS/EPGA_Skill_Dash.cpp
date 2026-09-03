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
	
	SetCooldownTag(EmpGameplayTags::TAG_Cooldown_Skill_Dash);
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
	
	ApplyCooldownGE();
}