// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EPGaugeVisual.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UEPGaugeVisual : public UInterface
{
	GENERATED_BODY()
};

class EMPLOYMENTPROJ_API IEPGaugeVisual
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintNativeEvent, Category = "Gauge")
	void SetGaugeProgress(float Progress01);
	
	UFUNCTION(BlueprintNativeEvent, Category = "Gauge")
	void SetGaugeVisible(bool bVisible);
};
