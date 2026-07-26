// Fill out your copyright notice in the Description page of Project Settings.


#include "HUD/EPSkillSlotWidget.h"

#include "AbilitySystemComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UEPSkillSlotWidget::InitWithASC(UAbilitySystemComponent* InASC)
{
	if (ASC.IsValid())
	{
		if (CooldownTag.IsValid() && CooldownHandle.IsValid())
			ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(CooldownHandle);
		
		int32 ActiveIdx = 0;
		for (const FGameplayTag& Tag : ActiveTags)
		{
			if (ActiveHandles.IsValidIndex(ActiveIdx) && ActiveHandles[ActiveIdx].IsValid())
				ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).Remove(ActiveHandles[ActiveIdx]);
			++ActiveIdx;
		}
		
		int32 LockIdx = 0;
		for (const FGameplayTag& LockTag : LockTags)
		{
			if (LockHandles.IsValidIndex(LockIdx) && LockHandles[LockIdx].IsValid())
				ASC->RegisterGameplayTagEvent(LockTag, EGameplayTagEventType::NewOrRemoved).Remove(LockHandles[LockIdx]);
			++LockIdx;
		}
	}
	ActiveHandles.Reset();
	LockHandles.Reset();
		
	ASC = InASC;
	if (!ASC.IsValid()) return;
	
	if (CooldownTag.IsValid())
		CooldownHandle = ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEPSkillSlotWidget::OnCooldownTagChanged);
	
	for (const FGameplayTag& Tag : ActiveTags)
		ActiveHandles.Add(ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEPSkillSlotWidget::OnActiveTagChanged));
	
	for (const FGameplayTag& Tag : LockTags)
		LockHandles.Add(ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UEPSkillSlotWidget::OnLockTagChanged));
	
	bCoolingDown = CooldownTag.IsValid() && ASC->HasMatchingGameplayTag(CooldownTag);
	bActive = ASC->HasAnyMatchingGameplayTags(ActiveTags);
	bLocked = ASC->HasAnyMatchingGameplayTags(LockTags);
	RecomputeState();
}

void UEPSkillSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	
	const bool bTickActive = CurrentState == EEPSkillSlotState::Active;
	const bool bTickCooldown = CurrentState == EEPSkillSlotState::Cooldown;
	if ((!bTickActive && !bTickCooldown) || !ASC.IsValid()) return;
	
	const FGameplayEffectQuery Query =
		FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
			bTickActive ? ActiveTags : FGameplayTagContainer(CooldownTag)
		);
	
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
	
	if (Duration <= 0.f) return;
	if (LastShownRemaining >= 0.f)
		Remaining = FMath::Min(Remaining, LastShownRemaining);
	LastShownRemaining = Remaining;
	
	if (bTickCooldown && CooldownBar)
	{
		const float Progress = Duration > 0.f ? FMath::Clamp(1.f - Remaining / Duration, 0.f, 1.f) : 0.f;
		CooldownBar->SetPercent(Progress);
	}
	if (CooldownText)
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
}

void UEPSkillSlotWidget::NativeDestruct()
{
	if (ASC.IsValid())
	{
		if (CooldownTag.IsValid() && CooldownHandle.IsValid())
			ASC->RegisterGameplayTagEvent(CooldownTag, EGameplayTagEventType::NewOrRemoved).Remove(CooldownHandle);
		
		int32 ActiveIdx = 0;
		for (const FGameplayTag& Tag : ActiveTags)
		{
			if (ActiveHandles.IsValidIndex(ActiveIdx) && ActiveHandles[ActiveIdx].IsValid())
				ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).Remove(ActiveHandles[ActiveIdx]);
			++ActiveIdx;
		}
		
		int32 LockIdx = 0;
		for (const FGameplayTag& LockTag : LockTags)
		{
			if (LockHandles.IsValidIndex(LockIdx) && LockHandles[LockIdx].IsValid())
				ASC->RegisterGameplayTagEvent(LockTag, EGameplayTagEventType::NewOrRemoved).Remove(LockHandles[LockIdx]);
			++LockIdx;
		}
	}
	
	Super::NativeDestruct();
}

void UEPSkillSlotWidget::OnCooldownTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bCoolingDown = NewCount > 0;
	RecomputeState();
}

void UEPSkillSlotWidget::OnActiveTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bActive = ASC.IsValid() && ASC->HasAnyMatchingGameplayTags(ActiveTags);
	RecomputeState();
}

void UEPSkillSlotWidget::OnLockTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	bLocked = ASC.IsValid() && ASC->HasAnyMatchingGameplayTags(LockTags);
	RecomputeState();
}

void UEPSkillSlotWidget::RecomputeState()
{
	if (bActive) ApplyState(EEPSkillSlotState::Active);
	else if (bLocked) ApplyState(EEPSkillSlotState::Locked);
	else if (bCoolingDown) ApplyState(EEPSkillSlotState::Cooldown);
	else ApplyState(EEPSkillSlotState::Ready);
}

void UEPSkillSlotWidget::ApplyState(EEPSkillSlotState NewState)
{
	CurrentState = NewState;
	LastShownRemaining = -1.f;
	
	const bool bShowBar = (NewState == EEPSkillSlotState::Cooldown || NewState == EEPSkillSlotState::Active);
	if (CooldownBar)
		CooldownBar->SetVisibility(bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (CooldownText)
		CooldownText->SetVisibility(
			bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	
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
