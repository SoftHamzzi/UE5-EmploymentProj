// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HUD/EPGaugeVisual.h"
#include "EPMaterialGaugeWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;

UCLASS()
class EMPLOYMENTPROJ_API UEPMaterialGaugeWidget : public UUserWidget, public IEPGaugeVisual
{
	GENERATED_BODY()

public:
	virtual void SetGaugeProgress_Implementation(float Progress01) override;
	virtual void SetGaugeVisible_Implementation(bool bVisible) override;
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> GaugeImage;
	
	UPROPERTY(EditAnywhere, Category = "Gauge")
	FName ProgressParamName = TEXT("Progress");
	
	UPROPERTY(EditAnywhere, Category = "Gauge")
	bool bInvertProgress = false;
	
private:
	TObjectPtr<UMaterialInstanceDynamic> GaugeMID;
	
};
