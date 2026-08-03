// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Interact.generated.h"

class AEPCharacter;

UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Interact : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UEPGA_Interact();
	
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
private:
	bool TryInteract(AEPCharacter* Char, const FGameplayEventData* Data, FText& OutReason) const;
};
