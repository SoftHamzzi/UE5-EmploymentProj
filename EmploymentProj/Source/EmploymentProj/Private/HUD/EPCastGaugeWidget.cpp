// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystemComponent.h"
#include "HUD/EPGaugeVisual.h"
#include "HUD/EPCastGaugeWidget.h"

void UEPCastGaugeWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	if (ASC.IsValid() && ChannelTag.IsValid() && ChannelHandle.IsValid())
		ASC->RegisterGameplayTagEvent(ChannelTag, EGameplayTagEventType::NewOrRemoved)
			.Remove(ChannelHandle);
	
	ASC = InASC;
	if (!ASC.IsValid() || !ChannelTag.IsValid()) return;
	
	ChannelHandle = ASC->RegisterGameplayTagEvent(ChannelTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UEPCastGaugeWidget::OnChannelTagChanged);
	
	OnChannelTagChanged(ChannelTag, ASC->HasMatchingGameplayTag(ChannelTag) ? 1 : 0);
}

void UEPCastGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	if (GaugeVisual && !GaugeVisual->Implements<UEPGaugeVisual>())
		UE_LOG(LogTemp, Warning, TEXT("EPCastGaugeWidget: GaugeVisual(%s)가 IEPGaugeVisual을 구현하지 않음."),
			*GaugeVisual->GetName());
	
	SetVisibility(ESlateVisibility::Collapsed);
}

void UEPCastGaugeWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (!bActive || !ASC.IsValid() || !GaugeVisual || !GaugeVisual->Implements<UEPGaugeVisual>()) return;
	
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(ChannelTag));
	TArray<TPair<float, float>> Results = ASC->GetActiveEffectsTimeRemainingAndDuration(Query);
	
	float Remaining = 0.f;
	float Duration = 0.f;
	for (const TPair<float, float>& Pair : Results)
	{
		if (Pair.Key > Remaining)
		{
			Remaining = Pair.Key;
			Duration = Pair.Value;
		}
	}
	
	if (Duration <= 0.f) return;
	if (LastShownRemaining >= 0.f)
		Remaining = FMath::Min(Remaining, LastShownRemaining);
	LastShownRemaining = Remaining;
	
	const float Progress = Duration > 0.f ? FMath::Clamp(Remaining / Duration, 0.f, 1.f) : 0.f;
	IEPGaugeVisual::Execute_SetGaugeProgress(GaugeVisual, Progress);
}

void UEPCastGaugeWidget::NativeDestruct()
{
	if (ASC.IsValid() && ChannelTag.IsValid() && ChannelHandle.IsValid())
		ASC->RegisterGameplayTagEvent(ChannelTag, EGameplayTagEventType::NewOrRemoved)
			.Remove(ChannelHandle);
	
	Super::NativeDestruct();
}

void UEPCastGaugeWidget::OnChannelTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bActive = NewCount > 0;
	LastShownRemaining = -1.f;
	SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	
	if (GaugeVisual && GaugeVisual->Implements<UEPGaugeVisual>())
		IEPGaugeVisual::Execute_SetGaugeVisible(GaugeVisual, bActive);
}
