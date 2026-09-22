// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Item_Reload.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Item_Reload : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	// === 변수 ===
	// === 함수 ===
	UEPGA_Item_Reload();
	
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
protected:
	// === 변수 ===
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_ReloadAmmoClass;
	
private:
	// === 함수 ===
	UFUNCTION()
	void OnReloadComplete_Task();
};
