// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Interact.h"

#include "Core/EPCharacter.h"
#include "Core/EPPlayerController.h"
#include "GAS/EPNativeGameplayTags.h"
#include "Interaction/EPInteractable.h"
#include "Interaction/EPInteractionComponent.h"

UEPGA_Interact::UEPGA_Interact()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	bServerRespectsRemoteAbilityCancellation = false;
	
	FAbilityTriggerData Trigger;
	Trigger.TriggerTag = EmpGameplayTags::TAG_Ability_Interact;
	Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
	AbilityTriggers.Add(Trigger);
	
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
}

void UEPGA_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                     const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                     const FGameplayEventData* TriggerEventData)
{
	if (!ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
		return;
	}
	
	AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
	AEPPlayerController* PC = Char ? Cast<AEPPlayerController>(Char->GetController()) : nullptr;
	
	FText Reason;
	if (TryInteract(Char, TriggerEventData, Reason) && PC)
	{
		PC->Client_OnInteractFailed(Reason);
	}
	
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

bool UEPGA_Interact::TryInteract(AEPCharacter* Char, const FGameplayEventData* Data, FText& OutReason) const
{
	if (!Char) return false;
	
	AActor* Target = Data ? const_cast<AActor*>(Data->Target.Get()) : nullptr;
	if (!IsValid(Target) || Target->IsActorBeingDestroyed())
	{
		OutReason = NSLOCTEXT("EP", "InteractNoTarget", "대상이 없습니다.");
		return false;
	}
	
	IEPInteractable* Interactable = Cast<IEPInteractable>(Target);
	if (!Interactable)
	{
		OutReason = NSLOCTEXT("EP", "InteractNotTarget", "상호작용할 수 없습니다.");
		return false;
	}
	
	const UEPInteractionComponent* IC = Char->GetInteractionComponent();
	if (!IC) return false;
	
	const float MaxDist = IC->GetInteractRange() + IC->GetServerRangeTolerance();
	if (FVector::DistSquared(Char->GetActorLocation(), Target->GetActorLocation())
		> FMath::Square(MaxDist))
	{
		OutReason = NSLOCTEXT("EP", "InteractTooFar", "너무 멉니다.");
		return false;
	}
	
	if (!Interactable->CanInteract(Char, OutReason)) return false;
	
	return Interactable->OnInteract(Char, OutReason);
}
