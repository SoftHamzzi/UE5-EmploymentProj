// Fill out your copyright notice in the Description page of Project Settings.

#include "HUD/EPBarGaugeWidget.h"
#include "Components/ProgressBar.h"

void UEPBarGaugeWidget::SetGaugeProgress_Implementation(float Progress01)
{
	if (GaugeBar)
		GaugeBar->SetPercent(bInvertProgress ? 1 - Progress01 : Progress01);
}

void UEPBarGaugeWidget::SetGaugeVisible_Implementation(bool bVisible)
{
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
