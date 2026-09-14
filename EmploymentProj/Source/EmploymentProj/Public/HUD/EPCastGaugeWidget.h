// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "EPCastGaugeWidget.generated.h"

class UAbilitySystemComponent;
class UWidget;

UCLASS()
class EMPLOYMENTPROJ_API UEPCastGaugeWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void InitWithASC(UAbilitySystemComponent* InASC);
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;
	
	UPROPERTY(EditAnywhere, Category = "Cast")
	FGameplayTag ChannelTag;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GaugeVisual;
	
private:
	void OnChannelTagChanged(const FGameplayTag Tag, int32 NewCount);
	
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle ChannelHandle;
	bool bActive = false;
	float LastShownRemaining = -1.f;
};
