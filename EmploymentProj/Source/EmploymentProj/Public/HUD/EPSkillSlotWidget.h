// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "EPSkillSlotWidget.generated.h"

class UAbilitySystemComponent;
class UProgressBar;
class UTextBlock;
class UImage;
struct FEPDurationMessage;

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

	// === 변수 ===
	// --- 메시지 채널 (Skill 어빌리티의 CooldownChannelTag/ActiveChannelTag와 값이 같아야 한다) ---
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTag CooldownChannelTag;

	// Active 지속시간이 있는 스킬 슬롯에서만 설정. 없으면(Invalid) Active로 절대 안 바뀜
	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTag ActiveChannelTag;

	UPROPERTY(EditAnywhere, Category = "Skill")
	FGameplayTagContainer LockTags;

	// --- 바인딩 위젯 ---
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

	// --- 스타일 ---
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
	// === 변수 ===
	// --- Cooldown ---
	FGameplayMessageListenerHandle CooldownMessageHandle;
	FTimerHandle CooldownTimerHandle;
	float CooldownStartTime = -1.f;
	float CooldownDuration = -1.f;

	// --- Active ---
	FGameplayMessageListenerHandle ActiveMessageHandle;
	FTimerHandle ActiveTimerHandle;

	// --- Lock (지속시간 없음 — GAS 태그를 그대로 구독) ---
	TArray<FDelegateHandle> LockHandles;

	// --- 상태 ---
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	bool bCoolingDown = false;
	bool bActive = false;
	bool bLocked = false;
	EEPSkillSlotState CurrentState = EEPSkillSlotState::Ready;

	// === 함수 ===
	// --- Cooldown ---
	void OnCooldownDurationMessage(FGameplayTag Channel, const FEPDurationMessage& Message);
	void OnCooldownFinished();

	// --- Active ---
	void OnActiveDurationMessage(FGameplayTag Channel, const FEPDurationMessage& Message);
	void OnActiveFinished();

	// --- Lock ---
	void OnLockTagChanged(const FGameplayTag Tag, int32 NewCount);

	// --- 상태 머신 ---
	EEPSkillSlotState ComputeState() const;
	void RecomputeState();
	void ApplyState(EEPSkillSlotState NewState);
};
