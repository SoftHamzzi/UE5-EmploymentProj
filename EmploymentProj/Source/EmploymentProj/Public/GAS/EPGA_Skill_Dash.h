// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EPGA_Skill_Base.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Skill_Dash.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Skill_Dash : public UEPGA_Skill_Base
{
	GENERATED_BODY()
	
public:
	UEPGA_Skill_Dash();

protected:
	// === 변수 ===
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashImpulse = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashZBoost = 250.f;

	// === 함수 ===
	virtual void OnCastComplete() override;
};