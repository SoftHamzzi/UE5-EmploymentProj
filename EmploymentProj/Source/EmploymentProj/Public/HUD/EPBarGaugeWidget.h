// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HUD/EPGaugeVisual.h"
#include "EPBarGaugeWidget.generated.h"

class UProgressBar;

UCLASS()
class EMPLOYMENTPROJ_API UEPBarGaugeWidget : public UUserWidget, public IEPGaugeVisual
{
	GENERATED_BODY()
	
public:
	virtual void SetGaugeProgress_Implementation(float Progress01) override;
	virtual void SetGaugeVisible_Implementation(bool bVisible) override;
	
protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> GaugeBar;
	
	UPROPERTY(EditAnywhere, Category = "Gauge")
	bool bInvertProgress = false;
};
