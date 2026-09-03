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
	// === 변수 ===
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_ShieldOnClass;

	UPROPERTY(EditDefaultsOnly, Category = "Shield")
	float ShieldDuration = 5.f;

	// === 함수 ===
	virtual void OnCastComplete() override;
};
