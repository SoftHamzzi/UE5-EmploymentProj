// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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
	UPROPERTY(EditDefaultsOnly, Category="Damage")
	FGameplayTagContainer MaterialTags;
};
