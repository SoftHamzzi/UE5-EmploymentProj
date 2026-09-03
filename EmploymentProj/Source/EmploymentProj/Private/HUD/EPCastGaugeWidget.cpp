// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/EPCastGaugeWidget.h"

#include "AbilitySystemComponent.h"
#include "GAS/EPDurationMessage.h"
#include "HUD/EPGaugeVisual.h"

void UEPCastGaugeWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	if (ASC.IsValid())
	{
		if (CastChannelTag.IsValid() && ChannelHandle.IsValid())
			ASC->RegisterGameplayTagEvent(CastChannelTag, EGameplayTagEventType::NewOrRemoved)
				.Remove(ChannelHandle);

		if (DurationMessageHandle.IsValid())
			DurationMessageHandle.Unregister();
	}

	ASC = InASC;
	if (!ASC.IsValid() || !CastChannelTag.IsValid()) return;

	ChannelHandle = ASC->RegisterGameplayTagEvent(CastChannelTag, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UEPCastGaugeWidget::OnChannelTagChanged);

	DurationMessageHandle = UGameplayMessageSubsystem::Get(this)
		.RegisterListener<FEPDurationMessage>(CastChannelTag, this, &UEPCastGaugeWidget::OnCastDurationMessage);

	OnChannelTagChanged(CastChannelTag, ASC->HasMatchingGameplayTag(CastChannelTag) ? 1 : 0);
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

	if (!bActive || !GaugeVisual || !GaugeVisual->Implements<UEPGaugeVisual>()) return;
	if (GaugeDuration <= 0.f) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const float Remaining = FMath::Clamp(GaugeStartTime + GaugeDuration - World->GetTimeSeconds(), 0.f, GaugeDuration);
	const float Progress = Remaining / GaugeDuration;
	IEPGaugeVisual::Execute_SetGaugeProgress(GaugeVisual, Progress);
}

void UEPCastGaugeWidget::NativeDestruct()
{
	if (ASC.IsValid() && CastChannelTag.IsValid() && ChannelHandle.IsValid())
		ASC->RegisterGameplayTagEvent(CastChannelTag, EGameplayTagEventType::NewOrRemoved)
			.Remove(ChannelHandle);

	DurationMessageHandle.Unregister();

	Super::NativeDestruct();
}

void UEPCastGaugeWidget::OnChannelTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bActive = NewCount > 0;
	GaugeDuration = -1.f;
	SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (GaugeVisual && GaugeVisual->Implements<UEPGaugeVisual>())
		IEPGaugeVisual::Execute_SetGaugeVisible(GaugeVisual, bActive);
}

void UEPCastGaugeWidget::OnCastDurationMessage(FGameplayTag Channel, const FEPDurationMessage& Message)
{
	if (!ASC.IsValid() || !Message.Instigator || Message.Instigator != ASC->GetAvatarActor())
		return;

	UWorld* World = GetWorld();
	if (!World) return;

	GaugeStartTime = World->GetTimeSeconds();
	GaugeDuration = Message.Duration;
}
