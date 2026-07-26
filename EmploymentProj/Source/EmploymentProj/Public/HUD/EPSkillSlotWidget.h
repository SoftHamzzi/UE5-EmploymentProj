// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "EPSkillSlotWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
class UImage;

enum class EEPSkillSlotState : uint8
{
	Ready,
	Cooldown,
	Active,
	Locked,
};

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
	
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTagContainer ActiveTags;
	
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTagContainer LockTags;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SlotBorder;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SlotCenter;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> SkillIcon;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LockSlash;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CooldownBar;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CooldownText;
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor ReadyBorderColor = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor ReadyCenterColor = FLinearColor::White;
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor ReadyIconColor = FLinearColor::Black;
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor CooldownFillColor = FLinearColor(1.f, 0.5f, 0.f, 1.f);
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor LockedBorderColor = FLinearColor(0.8f, 0.05f, 0.05f, 1.f);
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor LockedCenterColor = FLinearColor(0.8f, 0.05f, 0.05f, 0.45f);
	
	UPROPERTY(EditAnywhere, Category = "Style")
	FLinearColor LockedIconColor = FLinearColor(0.8f, 0.05f, 0.05f, 1.f);
	
private:
	void OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnActiveTagChanged(const FGameplayTag Tag, int32 NewCount);
	void OnLockTagChanged(const FGameplayTag Tag, int32 NewCount);
	void RecomputeState();
	void ApplyState(EEPSkillSlotState NewState);
	
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle CooldownHandle;
	TArray<FDelegateHandle> ActiveHandles;
	TArray<FDelegateHandle> LockHandles;
	
	bool bCoolingDown = false;
	bool bActive = false;
	bool bLocked = false;
	float LastShownRemaining = -1.f;
	EEPSkillSlotState CurrentState = EEPSkillSlotState::Ready;
};
