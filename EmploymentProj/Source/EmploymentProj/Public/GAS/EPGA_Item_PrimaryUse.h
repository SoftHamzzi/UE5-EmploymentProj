// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Item_PrimaryUse.generated.h"

class AEPCharacter;
class AEPWeapon;

UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Item_PrimaryUse : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UEPGA_Item_PrimaryUse();
	
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	
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
	
	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;
	
protected:
	
private:
	// === 변수 ===
	FTimerHandle FireTimerHandle;
	bool bPendingShot = false;
	FDelegateHandle TargetDataDelegateHandle;
	
	// === 함수 ===
	void FireOnce();
	void ArmNextShot();
	void OnFireTimerTick();
	void SendFireTargetData(const FVector& Direction, float ClientMoveTimeStamp);
	
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	bool ServerConfirmOneShot(const FVector& Direction, float ClientMoveTimeStamp);
	
	AEPCharacter* GetCharacter() const;
	AEPWeapon* GetWeapon() const;
	float GetFireRateMultiplier() const;
	float GetClientMoveTimeStamp() const;
	static bool IsAutoFire(const AEPWeapon* Weapon);
};
