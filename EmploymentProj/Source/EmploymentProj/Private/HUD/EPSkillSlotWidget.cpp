// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/EPSkillSlotWidget.h"

#include "AbilitySystemComponent.h"
#include "GAS/EPDurationMessage.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UEPSkillSlotWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	if (ASC.IsValid())
	{
		if (CooldownMessageHandle.IsValid())
			CooldownMessageHandle.Unregister();
		
		if (ActiveMessageHandle.IsValid())
			ActiveMessageHandle.Unregister();
		
		int32 LockIdx = 0;
		for (const FGameplayTag& LockTag : LockTags)
		{
			if (LockHandles.IsValidIndex(LockIdx) && LockHandles[LockIdx].IsValid())
				ASC->RegisterGameplayTagEvent(LockTag, EGameplayTagEventType::NewOrRemoved).Remove(LockHandles[LockIdx]);
			++LockIdx;
		}
	}
	LockHandles.Reset();
		
	ASC = InASC;
	if (!ASC.IsValid()) return;
	
	if (CooldownChannelTag.IsValid())
		CooldownMessageHandle = UGameplayMessageSubsystem::Get(this)
			.RegisterListener<FEPDurationMessage>(CooldownChannelTag, this, &UEPSkillSlotWidget::OnCooldownDurationMessage);

	if (ActiveChannelTag.IsValid())
		ActiveMessageHandle = UGameplayMessageSubsystem::Get(this)
			.RegisterListener<FEPDurationMessage>(ActiveChannelTag, this, &UEPSkillSlotWidget::OnActiveDurationMessage);
	
	for (const FGameplayTag& Tag : LockTags)
		LockHandles.Add(ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEPSkillSlotWidget::OnLockTagChanged));
	
	bLocked = ASC->HasAnyMatchingGameplayTags(LockTags);
	ApplyState(ComputeState());
}

void UEPSkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Active는 더 이상 틱에서 할 일이 없다 — 바는 ApplyState가 이미 1.0으로 고정해뒀고,
	// 남은 숫자는 활성화 동안 안 보여주기로 했으므로(사용자 결정) 갱신할 것도 없다.
	if (CurrentState != EEPSkillSlotState::Cooldown) return;

	UWorld* World = GetWorld();
	if (!World) return;
	const float Now = World->GetTimeSeconds();

	if (CooldownDuration <= 0.f) return;
	const float Remaining = FMath::Clamp(
		CooldownStartTime + CooldownDuration - Now, 0.f, CooldownDuration);

	if (CooldownText)
	{
		CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
	}
	if (CooldownBar)
		CooldownBar->SetPercent(1.f - Remaining / CooldownDuration);
}

void UEPSkillSlotWidget::NativeDestruct()
{
	if (!ASC.IsValid()) return;
	
	int32 LockIdx = 0;
	for (const FGameplayTag& LockTag : LockTags)
	{
		if (LockHandles.IsValidIndex(LockIdx) && LockHandles[LockIdx].IsValid())
			ASC->RegisterGameplayTagEvent(LockTag, EGameplayTagEventType::NewOrRemoved).Remove(LockHandles[LockIdx]);
		++LockIdx;
	}
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
		World->GetTimerManager().ClearTimer(ActiveTimerHandle);
	}
	CooldownMessageHandle.Unregister();
	ActiveMessageHandle.Unregister();
	
	Super::NativeDestruct();
}

void UEPSkillSlotWidget::OnLockTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bLocked = ASC.IsValid() && ASC->HasAnyMatchingGameplayTags(LockTags);
	RecomputeState();
}

void UEPSkillSlotWidget::OnCooldownDurationMessage(FGameplayTag Channel, const FEPDurationMessage& Message)
{
	if (!ASC.IsValid() || !Message.Instigator || Message.Instigator != ASC->GetAvatarActor())
		return;

	UWorld* World = GetWorld();
	if (!World) return;

	CooldownStartTime = World->GetTimeSeconds();
	CooldownDuration = Message.Duration;
	bCoolingDown = true;
	RecomputeState();
	
	World->GetTimerManager().SetTimer(
		CooldownTimerHandle, this, &UEPSkillSlotWidget::OnCooldownFinished, Message.Duration, false);
}

void UEPSkillSlotWidget::OnActiveDurationMessage(FGameplayTag Channel, const FEPDurationMessage& Message)
{
	if (!ASC.IsValid() || !Message.Instigator || Message.Instigator != ASC->GetAvatarActor())
		return;

	UWorld* World = GetWorld();
	if (!World) return;

	bActive = true;
	RecomputeState();

	World->GetTimerManager().SetTimer(
		ActiveTimerHandle, this, &UEPSkillSlotWidget::OnActiveFinished, Message.Duration, false);
}

void UEPSkillSlotWidget::OnActiveFinished()
{
	bActive = false;
	RecomputeState();
}

void UEPSkillSlotWidget::OnCooldownFinished()
{
	bCoolingDown = false;
	RecomputeState();
}

EEPSkillSlotState UEPSkillSlotWidget::ComputeState() const
{
	if (bActive) return EEPSkillSlotState::Active;
	if (bLocked) return EEPSkillSlotState::Locked;
	if (bCoolingDown) return EEPSkillSlotState::Cooldown;
	return EEPSkillSlotState::Ready;
}

void UEPSkillSlotWidget::RecomputeState()
{
	const EEPSkillSlotState NewState = ComputeState();
	if (NewState == CurrentState) return;
	ApplyState(NewState);
}

void UEPSkillSlotWidget::ApplyState(EEPSkillSlotState NewState)
{
	CurrentState = NewState;

	const bool bShowBar = (NewState == EEPSkillSlotState::Cooldown || NewState == EEPSkillSlotState::Active);
	const bool bShowText = (NewState == EEPSkillSlotState::Cooldown);   // Active 동안은 남은 숫자 안 보여줌(사용자 결정)
	if (CooldownBar)
		CooldownBar->SetVisibility(bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (CooldownText)
		CooldownText->SetVisibility(
			bShowText ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (bShowBar && CooldownBar)
	{
		CooldownBar->SetFillColorAndOpacity(CooldownFillColor);
		if (NewState == EEPSkillSlotState::Active)
			CooldownBar->SetPercent(1.f);
	}
	
	const bool bLockedLook = (NewState == EEPSkillSlotState::Locked);
	if (SlotBorder) SlotBorder->SetColorAndOpacity(bLockedLook ? LockedBorderColor : ReadyBorderColor);
	if (SlotCenter) SlotCenter->SetColorAndOpacity(bLockedLook ? LockedCenterColor : ReadyCenterColor);
	if (SkillIcon) SkillIcon->SetColorAndOpacity(bLockedLook ? LockedIconColor : ReadyIconColor);
	if (LockSlash)
	{
		LockSlash->SetColorAndOpacity(LockedIconColor);
		LockSlash->SetVisibility(bLockedLook ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
