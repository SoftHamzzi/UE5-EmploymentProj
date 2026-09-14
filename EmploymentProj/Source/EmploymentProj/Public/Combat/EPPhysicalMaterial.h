// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "EPPhysicalMaterial.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPPhysicalMaterial : public UPhysicalMaterial
{
	GENERATED_BODY()
	
public:
	// GAS 확장용
	// UPROPERTY(EditDefaultsOnly, Category="Damage")
	// FGameplayTagContainer MaterialTags;
	
	UPROPERTY(EditDefaultsOnly, Category="Damage")
	bool bIsWeakSpot = false;
	
	UPROPERTY(EditDefaultsOnly, Category="Damage",
		meta = (EditCondition = "bIsWeakSpot"))
	float WeakSpotMultiplier = 2.0f;
	
};
