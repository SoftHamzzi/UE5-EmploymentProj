// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EPGA_Skill_Base.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Skill_Heal.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Skill_Heal : public UEPGA_Skill_Base
{
	GENERATED_BODY()
	
public:
	UEPGA_Skill_Heal();
	
	virtual void OnCastComplete() override;
	virtual void ConfigureCastingSpec(FGameplayEffectSpecHandle& SpecHandle) override;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_HealClass;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_HealCooldownClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	float HealAmount = 30.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	float HealCooldown = 20.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	float HealMoveSpeedMultiplier = 0.2f;
	
private:
};
