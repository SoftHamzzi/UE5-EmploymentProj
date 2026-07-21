// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Skill_Heal.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Skill_Heal : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UEPGA_Skill_Heal();
	
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_HealingClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_HealClass;

	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_HealCooldownClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_MoveSpeedModifierClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	float HealAmount = 30.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	float HealDuration = 3.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	float HealCooldown = 20.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Heal")
	float HealMoveSpeedMultiplier = 0.2f;
	
private:
	FActiveGameplayEffectHandle HealingEffectHandle;
	FActiveGameplayEffectHandle MoveSpeedEffectHandle;
	
	UFUNCTION()
	void OnHealComplete();
	
	UFUNCTION()
	void OnDamageTaken(FGameplayEventData Payload);
};
