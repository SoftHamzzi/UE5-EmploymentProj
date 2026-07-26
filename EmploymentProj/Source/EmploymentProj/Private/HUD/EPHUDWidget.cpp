// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/EPHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Core/EPGameState.h"
#include "GAS/EPAttributeSet.h"
#include "GAS/EPNativeGameplayTags.h"
#include "HUD/EPSkillSlotWidget.h"
#include "HUD/EPCastGaugeWidget.h"

void UEPHUDWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	UnbindAll();
	
	ASC = InASC;
	if (!ASC.IsValid()) return;
	
	HealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UEPHUDWidget::OnHealthChanged);
	MaxHealthHandle = ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UEPHUDWidget::OnHealthChanged);
	AmmoHandle = ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetAmmoAttribute())
		.AddUObject(this, &UEPHUDWidget::OnAmmoChanged);
	MaxAmmoHandle = ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMaxAmmoAttribute())
		.AddUObject(this, &UEPHUDWidget::OnAmmoChanged);

	ReloadingHandle = ASC->RegisterGameplayTagEvent(
		EmpGameplayTags::TAG_State_Reloading, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UEPHUDWidget::OnReloadingTagChanged);
	
	if (DashSlot) DashSlot->InitWithASC(InASC);
	if (HealSlot) HealSlot->InitWithASC(InASC);
	if (ShieldSlot) ShieldSlot->InitWithASC(InASC);
	if (CastGauge) CastGauge->InitWithASC(InASC);
	if (ShieldGauge) ShieldGauge->InitWithASC(InASC);
	
	RefreshHealth();
	RefreshAmmo();
	OnReloadingTagChanged(EmpGameplayTags::TAG_State_Reloading,
		ASC->HasMatchingGameplayTag(EmpGameplayTags::TAG_State_Reloading) ? 1 : 0);
}

void UEPHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	if (!GameState.IsValid())
		GameState = GetWorld()->GetGameState<AEPGameState>();
	
	if (GameState.IsValid() && TimerText)
	{
		const int32 Total = FMath::Max(0, FMath::FloorToInt(GameState->GetRemainingTime()));
		TimerText->SetText(FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60)));
	}
}

void UEPHUDWidget::NativeDestruct()
{
	UnbindAll();
	Super::NativeDestruct();
}

void UEPHUDWidget::UnbindAll()
{
	if (!ASC.IsValid()) return;
	
	ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
	ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
	ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetAmmoAttribute()).Remove(AmmoHandle);
	ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMaxAmmoAttribute()).Remove(MaxAmmoHandle);
	ASC->RegisterGameplayTagEvent(
		EmpGameplayTags::TAG_State_Reloading, EGameplayTagEventType::NewOrRemoved)
		.Remove(ReloadingHandle);
}

void UEPHUDWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshHealth();
}

void UEPHUDWidget::OnAmmoChanged(const FOnAttributeChangeData& Data)
{
	RefreshAmmo();
}

void UEPHUDWidget::OnReloadingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (ReloadingText)
		ReloadingText->SetVisibility(NewCount > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UEPHUDWidget::RefreshHealth()
{
	if (!ASC.IsValid()) return;
	
	const float Health = ASC->GetNumericAttribute(UEPAttributeSet::GetHealthAttribute());
	const float MaxHealth = ASC->GetNumericAttribute(UEPAttributeSet::GetMaxHealthAttribute());
	
	if (HealthBar) HealthBar->SetPercent(MaxHealth > 0.f ? Health / MaxHealth : 0.f);
	if (HealthText) HealthText->SetText(FText::FromString(
		FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(Health) , FMath::RoundToInt(MaxHealth))));
}

void UEPHUDWidget::RefreshAmmo()
{
	if (!ASC.IsValid()) return;
	
	const float Ammo = ASC->GetNumericAttribute(UEPAttributeSet::GetAmmoAttribute());
	const float MaxAmmo = ASC->GetNumericAttribute(UEPAttributeSet::GetMaxAmmoAttribute());
	
	if (AmmoText) AmmoText->SetText(FText::FromString(
		FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(Ammo), FMath::RoundToInt(MaxAmmo))));
}
