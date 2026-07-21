// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "EPSkillSlotWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;

UCLASS()
class EMPLOYMENTPROJ_API UEPSkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitWithASC(UAbilitySystemComponent* InASC);
	
protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTag CooldownTag;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CooldownBar;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CooldownText;
	
private:
	void OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);
	void SetCooldownVisible(bool bVisible);
	
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle TagChangedHandle;
	bool bCoolingDown = false;
};
