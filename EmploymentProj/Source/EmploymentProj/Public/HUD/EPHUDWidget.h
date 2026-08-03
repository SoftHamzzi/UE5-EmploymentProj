// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "EPHUDWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
class UEPSkillSlotWidget;
class UEPCastGaugeWidget;
class AEPGameState;
struct FOnAttributeChangeData;

UCLASS()
class EMPLOYMENTPROJ_API UEPHUDWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitWithASC(UAbilitySystemComponent* InASC);
	void SetInteractPrompt(const FText& Text, bool bEnabled);
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> AmmoText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ReloadingText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TimerText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPSkillSlotWidget> DashSlot;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPSkillSlotWidget> HealSlot;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPSkillSlotWidget> ShieldSlot;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEPCastGaugeWidget> CastGauge;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEPCastGaugeWidget> ShieldGauge;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InteractPrompt;
	
private:
	void UnbindAll();
	
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnAmmoChanged(const FOnAttributeChangeData& Data);
	void OnReloadingTagChanged(const FGameplayTag Tag, int32 NewCount);

	void RefreshHealth();
	void RefreshAmmo();
	
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	TWeakObjectPtr<AEPGameState> GameState;
	
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	FDelegateHandle AmmoHandle;
	FDelegateHandle MaxAmmoHandle;
	FDelegateHandle ReloadingHandle;
};
