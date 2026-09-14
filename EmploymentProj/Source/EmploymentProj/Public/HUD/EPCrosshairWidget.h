// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EPCrosshairWidget.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, Category = "Crosshair")
	float CrosshairSpread = 0.f;
};
