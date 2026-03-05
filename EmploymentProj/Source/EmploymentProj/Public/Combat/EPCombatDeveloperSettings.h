// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EPCombatDeveloperSettings.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPCombatDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, Category="LagComp")
	float MaxRewindSeconds = 0.2f;
	
	UPROPERTY(Config, EditAnywhere, Category="LagComp")
	float SnapshotIntervalSeconds = 0.05f;
	
	UPROPERTY(Config, EditAnywhere, Category="Trace")
	float BroadPhasePaddingCm = 50.f;
	
	UPROPERTY(Config, EditAnywhere, Category="Trace")
	float DefaultTraceDistanceCm = 10000.f;
};
