// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EPGA_Skill_Base.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Skill_ShieldOn.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Skill_ShieldOn : public UEPGA_Skill_Base
{
	GENERATED_BODY()
	
public:
	UEPGA_Skill_ShieldOn();
	
protected:
	virtual void OnCastComplete();
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_ShieldOnClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_ShieldCooldownClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Shield")
	float ShieldDuration = 5.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Shield")
	float ShieldCooldown = 50.f;
};
