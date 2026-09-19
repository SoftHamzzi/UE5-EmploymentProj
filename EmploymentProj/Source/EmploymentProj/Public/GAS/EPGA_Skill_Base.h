// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GAS/EPLocalTimer.h"
#include "EPGA_Skill_Base.generated.h"

class AEPCharacter;

UCLASS(Abstract)
class EMPLOYMENTPROJ_API UEPGA_Skill_Base : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UEPGA_Skill_Base();
	
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	
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
	
	void BankCooldown(float Now, float RateSoFar) { CooldownTimer.Bank(Now, RateSoFar); }
	
protected:
	// === 변수 ===
	// --- Cast ---
	UPROPERTY(EditDefaultsOnly, Category = "Cast")
	float CastTime = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Cast")
	bool bInterruptibleOnDamage = false;

	UPROPERTY(EditDefaultsOnly, Category = "Cast", meta = (Categories = "State"))
	FGameplayTag CastChannelTag;

	// --- Cooldown ---
	UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
	float Cooldown = 0.f;

	// 쿨다운 Broadcast 채널
	FGameplayTag CooldownChannelTag;

	// --- Active ---
	FGameplayTag ActiveChannelTag;

	// === 함수 ===
	// --- Cast ---
	virtual void OnCastStarted() {}
	virtual void OnCastComplete() PURE_VIRTUAL(UEPGA_Skill_Base::OnCastComplete, );
	virtual void OnCastInterrupted() {}
	virtual float GetCastMoveSpeedMultiplier() const { return 1.f; }

	// --- Cooldown ---
	float GetEffectiveCooldown() const;

	// --- Active ---
	void BroadcastActiveDuration(float Duration);

private:
	// === 변수 ===
	FEPLocalTimer CooldownTimer;

	// === 함수 ===
	void CompleteCast();
	
	UFUNCTION()
	void OnCastTimerComplete();
	
	UFUNCTION()
	void OnCastSynced();

	UFUNCTION()
	void OnDamageDuringCast(FGameplayEventData Payload);

	void OnActivationRejected(uint32 StartedGeneration);
	
	AEPCharacter* GetEPCharacter() const;
	void BroadcastDurationMessage(FGameplayTag Channel, float Duration);
};
