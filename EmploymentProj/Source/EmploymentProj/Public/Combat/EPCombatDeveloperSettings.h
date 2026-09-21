// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EPCombatDeveloperSettings.generated.h"

UCLASS(Config=Game, DefaultConfig)
class EMPLOYMENTPROJ_API UEPCombatDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, Category="LagComp")
	float MaxRewindSeconds = 0.5f;
	
	UPROPERTY(Config, EditAnywhere, Category="LagComp")
	float PredictionFudgeSeconds = 0.02f;
	
	UPROPERTY(Config, EditAnywhere, Category="GAS|FireRate", meta=(ClampMin="1", ClampMax="4"))
	float FireRateBurstAllowance = 2.f;
	
	UPROPERTY(Config, EditAnywhere, Category="LagComp", meta=(ClampMin="16", ClampMax="256"))
	int32 ShotOriginHistoryCount = 64;
	
	UPROPERTY(Config, EditAnywhere, Category="Trace")
	float BroadPhasePaddingCm = 50.f;
	
	UPROPERTY(Config, EditAnywhere, Category="Trace")
	float DefaultTraceDistanceCm = 10000.f;

	UPROPERTY(Config, EditAnywhere, Category="Debug|SSR")
	bool bEnableSSRDebugDraw = false;

	UPROPERTY(Config, EditAnywhere, Category="Debug|SSR", meta=(ClampMin="0.01"))
	float SSRDebugDrawDuration = 2.0f;

	UPROPERTY(Config, EditAnywhere, Category="Debug|SSR", meta=(ClampMin="0.1"))
	float SSRDebugLineThickness = 1.5f;

	UPROPERTY(Config, EditAnywhere, Category="Debug|SSR")
	bool bEnableSSRDebugLog = false;
	
	UPROPERTY(Config, EditAnywhere, Category="GAS|Cooldown", meta=(ClampMin="0.0", ClampMax="0.5"))
	float ServerCooldownToleranceSeconds = 0.1f;
};
