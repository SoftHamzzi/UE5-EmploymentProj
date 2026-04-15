// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Death.h"

#include "Combat/EPCombatComponent.h"
#include "Combat/EPWeapon.h"
#include "Core/EPCharacter.h"
#include "Core/EPGameMode.h"
#include "GAS/EPNativeGameplayTags.h"

UEPGA_Death::UEPGA_Death()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	bServerRespectsRemoteAbilityCancellation = false;
	
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = EmpGameplayTags::TAG_Event_Death;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
}

void UEPGA_Death::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	if (GE_StateDeadClass)
	{
		FActiveGameplayEffectHandle DeadHandle =
			ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
				GE_StateDeadClass.GetDefaultObject(), 1.f);
	}
	
	if (AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get()))
	{
		if (UEPCombatComponent* CC = Char->GetCombatComponent())
		{
			if (AEPWeapon* Weapon = CC->GetEquippedWeapon())
			{
				Weapon->SetActorHiddenInGame(true);
				Weapon->SetActorEnableCollision(false);
			}
		}
		
		const AActor* Killer = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
		AController* KillerController = Killer ? Killer->GetInstigatorController() : nullptr;
		if (AEPGameMode* GM = Char->GetWorld()->GetAuthGameMode<AEPGameMode>())
			GM->OnPlayerKilled(KillerController, Char->GetController());
		
		// 래그돌
		Char->Multicast_Die();
		
		if (AController* Controller = Char->GetController())
			Controller->UnPossess();
	}
	
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
