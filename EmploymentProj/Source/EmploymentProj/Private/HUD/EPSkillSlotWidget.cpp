// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/EPSkillSlotWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UEPSkillSlotWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	if (ASC.IsValid() && TagChangedHandle.IsValid())
		ASC->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::NewOrRemoved
		).Remove(TagChangedHandle);
	
	ASC = InASC;
	if (!ASC.IsValid() || !CooldownTag.IsValid()) return;
	
	TagChangedHandle = ASC->RegisterGameplayTagEvent(
		CooldownTag,
		EGameplayTagEventType::NewOrRemoved
	).AddUObject(this, &UEPSkillSlotWidget::OnCooldownTagChanged);
	
	OnCooldownTagChanged(CooldownTag, ASC->HasMatchingGameplayTag(CooldownTag) ? 1 : 0);
}

void UEPSkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (!bCoolingDown || !ASC.IsValid()) return;
	
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(CooldownTag));

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
	
	if (CooldownBar)
		CooldownBar->SetPercent(Duration > 0.f ? Remaining / Duration : 0.f);
	if (CooldownText)
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
}

void UEPSkillSlotWidget::NativeDestruct()
{
	if (ASC.IsValid() && TagChangedHandle.IsValid())
		ASC->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::NewOrRemoved
		).Remove(TagChangedHandle);
	Super::NativeDestruct();
}

void UEPSkillSlotWidget::OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bCoolingDown = NewCount > 0;
	SetCooldownVisible(bCoolingDown);
}

void UEPSkillSlotWidget::SetCooldownVisible(bool bVisible)
{
	const ESlateVisibility NewVis = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (CooldownBar) CooldownBar->SetVisibility(NewVis);
	if (CooldownText) CooldownText->SetVisibility(NewVis);

}
