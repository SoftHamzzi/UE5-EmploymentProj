// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Skill_Base.generated.h"

UCLASS(Abstract)
class EMPLOYMENTPROJ_API UEPGA_Skill_Base : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UEPGA_Skill_Base();
	
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override final;
	
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Cast")
	float CastTime = 0.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Cast")
	bool bInterruptibleOnDamage = false;

	UPROPERTY(EditDefaultsOnly, Category = "Cast")
	TSubclassOf<UGameplayEffect> GE_CastingClass;
	
	virtual void OnCastComplete() PURE_VIRTUAL(UEPGA_Skill_Base::OnCastComplete, );

	virtual void OnCastInterrupted() {}
	
	virtual void ConfigureCastingSpec(FGameplayEffectSpecHandle& SpecHandle) {}
	
private:
	UFUNCTION()
	void OnCastTimerComplete();
	
	UFUNCTION()
	void OnDamageDuringCast(FGameplayEventData Payload);
	
	FActiveGameplayEffectHandle CastingEffectHandle;
};
