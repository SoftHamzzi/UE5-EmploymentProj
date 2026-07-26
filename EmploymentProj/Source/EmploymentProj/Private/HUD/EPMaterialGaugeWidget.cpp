// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/EPMaterialGaugeWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void UEPMaterialGaugeWidget::SetGaugeProgress_Implementation(float Progress01)
{
	if (GaugeMID)
		GaugeMID->SetScalarParameterValue(ProgressParamName, bInvertProgress ? 1.f - Progress01 : Progress01);
}

void UEPMaterialGaugeWidget::SetGaugeVisible_Implementation(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UEPMaterialGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (GaugeImage)
		GaugeMID = GaugeImage->GetDynamicMaterial();
}