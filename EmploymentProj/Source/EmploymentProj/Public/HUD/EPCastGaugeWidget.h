// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "EPCastGaugeWidget.generated.h"

class UAbilitySystemComponent;
class UWidget;
struct FEPDurationMessage;

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

	// === 변수 ===
	UPROPERTY(EditAnywhere, Category = "Cast")
	FGameplayTag CastChannelTag;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> GaugeVisual;

private:
	// === 변수 ===
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle ChannelHandle;
	FGameplayMessageListenerHandle DurationMessageHandle;

	bool bActive = false;
	float GaugeStartTime = -1.f;
	float GaugeDuration = -1.f;

	// === 함수 ===
	void OnChannelTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnCastDurationMessage(FGameplayTag Channel, const FEPDurationMessage& Message);
};
