// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Skill_Dash.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Skill_Dash : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UEPGA_Skill_Dash();
	
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_DashCooldownClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashImpulse = 1200.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashCooldown = 10.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashZBoost = 250.f;
};