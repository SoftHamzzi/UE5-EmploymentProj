// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "Core/EPCharacter.h"
#include "GAS/EPNativeGameplayTags.h"
#include "Net/UnrealNetwork.h"

void UEPAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	if (Attribute == GetHealthAttribute())
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	if (Attribute == GetAmmoAttribute())
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxAmmo());
	if (Attribute == GetMaxAmmoAttribute())
		NewValue = FMath::Max(NewValue, 1.f);
	if (Attribute == GetMaxHealthAttribute())
		NewValue = FMath::Max(NewValue, 1.f);
	if (Attribute == GetMoveSpeedMultiplierAttribute())
		NewValue = FMath::Clamp(NewValue, 0.05f, 3.f);
}

void UEPAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
	if (Data.EvaluatedData.Attribute == GetAmmoAttribute())
		SetAmmo(FMath::Clamp(GetAmmo(), 0.f, GetMaxAmmo()));
	
	if (Data.EvaluatedData.Attribute == GetMaxAmmoAttribute())
		SetAmmo(FMath::Clamp(GetAmmo(), 0.f, GetMaxAmmo()));
	
	// 타겟 캐릭터
	AEPCharacter* TargetCharacter = Cast<AEPCharacter>(GetOwningActor());
	
	// 소스 액터
	UAbilitySystemComponent* SourceASC = Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent();
	AActor* SourceActor = nullptr;
	if (SourceASC && SourceASC->AbilityActorInfo.IsValid() && SourceASC->AbilityActorInfo->AvatarActor.IsValid())
	{
		SourceActor = SourceASC->AbilityActorInfo->AvatarActor.Get();
	}
	
	// GameplayAttributeData가 IncomingDamage인 경우
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.f); // 누적 방지
		
		if (Damage > 0.f)
		{
			// 이미 죽은 대상에게 대미지 중복 적용 방지
			const bool bWasAlive = GetHealth() > 0.f;
			UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
			
			float RemainingDamage = Damage;
			if (TargetASC && TargetASC->HasMatchingGameplayTag(EmpGameplayTags::TAG_State_Shielded))
				RemainingDamage *= 0.5f;
			
			const float NewHealth = FMath::Max(GetHealth() - RemainingDamage, 0.f);
			SetHealth(NewHealth);
			
			// 힐 채널링 취소 이벤트 발송
			if (TargetASC)
			{
				FGameplayEventData DmgPayload;
				TargetASC->HandleGameplayEvent(EmpGameplayTags::TAG_Event_Damaged, &DmgPayload);
			}
			
			// 사망 이벤트 발송
			if (bWasAlive && NewHealth <= 0.f)
			{
				if (TargetASC && !TargetASC->HasMatchingGameplayTag(EmpGameplayTags::TAG_State_Dead))
				{
					FGameplayEventData Payload;
					Payload.Instigator = SourceActor;
					TargetASC->HandleGameplayEvent(EmpGameplayTags::TAG_Event_Death, &Payload);
				}
			}
		}
	}
	
}

void UEPAttributeSet::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, Ammo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, MaxAmmo, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
}

void UEPAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, Health, OldValue);
}

void UEPAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, MaxHealth, OldValue);
}

void UEPAttributeSet::OnRep_Ammo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, Ammo, OldValue);
}

void UEPAttributeSet::OnRep_MaxAmmo(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, MaxAmmo, OldValue);
}

void UEPAttributeSet::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
}
