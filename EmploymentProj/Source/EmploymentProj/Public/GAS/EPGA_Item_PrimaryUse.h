// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Item_PrimaryUse.generated.h"

class AEPWeapon;

UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Item_PrimaryUse : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UEPGA_Item_PrimaryUse();
	
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	
	virtual void ApplyCooldown(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;
	
	bool ServerConfirmOneShot(const FVector& Origin, const FVector& Direction);
	
protected:
	
private:
	// === 변수 ===
	FTimerHandle FireTimerHandle;
	
	// === 함수 ===
	void FireOnce();
	
	static float GetFireInterval(const AEPWeapon* Weapon);
};
