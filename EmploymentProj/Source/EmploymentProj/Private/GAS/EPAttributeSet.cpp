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
	else if (Attribute == GetMaxHealthAttribute())
		NewValue = FMath::Max(NewValue, 1.f);
}

void UEPAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
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
			// 이미 죽은 대상에게 대미지가 중복 적용되는 것을 방지한다.
			const bool bWasAlive = GetHealth() > 0.f;
			
			const float NewHealth = FMath::Max(GetHealth() - Damage, 0.f);
			SetHealth(NewHealth);
			
			if (bWasAlive && NewHealth <= 0.f)
			{
				UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
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
}

void UEPAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, Health, OldValue);
}

void UEPAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, MaxHealth, OldValue);
}
