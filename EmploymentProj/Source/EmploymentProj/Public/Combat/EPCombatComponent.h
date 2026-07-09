// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "Components/ActorComponent.h"
#include "EPCombatComponent.generated.h"

class UNiagaraSystem;
class USoundBase;
class AEPCharacter;
class AEPWeapon;
class UEPPhysicalMaterial;
class AEPProjectile;
class UGameplayEffect;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class EMPLOYMENTPROJ_API UEPCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// === 변수 ===
	
	// === 함수 ===
	UEPCombatComponent();
	// --- Getter/Setter ---
	void EquipWeapon(AEPWeapon* NewWeapon);
	void UnequipWeapon();
	
	AEPCharacter* GetOwnerCharacter() const;
	AEPWeapon* GetEquippedWeapon() const;
	
	void HandleServerFire(const FVector& Origin, const FVector& Direction, float ClientFireTime);
	
	static void ApplyGEDamage(
		AActor* Target,
		AActor* Instigator,
		TSubclassOf<UGameplayEffect> GEClass,
		float FinalDamage);
	
	UFUNCTION()
	void PlayLocalMuzzleEffect(const FVector& MuzzleLocation);
	
	UFUNCTION()
	void PlayLocalImpactEffect(const FVector& ImpactPoint, const FVector& ImpactNormal);

	void SpawnLocalCosmeticProjectile(const FVector& MuzzleLocation, const FVector& Direction);
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// === 변수 ===
	
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TSubclassOf<UGameplayEffect> GE_DamageClass;
	
	UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AEPWeapon> EquippedWeapon;
	
	// --- 임시 변수 (리팩토링 필요)---
	UPROPERTY(EditDefaultsOnly, Category = "VFX|Fire")
	TObjectPtr<UNiagaraSystem> MuzzleFX = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "VFX|Fire")
	TObjectPtr<UNiagaraSystem> ImpactFX = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "SFX|Fire")
	TObjectPtr<USoundBase> FireSFX = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "SFX|Fire")
	TObjectPtr<USoundBase> ImpactSFX = nullptr;
	
	// === 함수 ===
	// --- 오버라이드 ---
	// Called when the game starts
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	// --- 동기화 ---
	UFUNCTION()
	void OnRep_EquippedWeapon();
	
	// --- RPC ---
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayMuzzleEffect(const FVector_NetQuantize& MuzzleLocation);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayImpactEffect(const FVector_NetQuantize& ImpactPoint, const FVector_NetQuantize& ImpactNormal);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SpawnCosmeticProjectile(
		const FVector_NetQuantize& MuzzleLocation,
		const FVector_NetQuantizeNormal& Direction);

private:
	// === 변수 ===
	TArray<FGameplayAbilitySpecHandle> GrantedWeaponAbilityHandles;
	
	// === 함수 ===
	void HandleHitscanFire(
		AEPCharacter*	Owner,
		const FVector&	Origin,
		const TArray<FVector>&	Directions,
		float	ClientFireTime
	);
	
	void HandleProjectileFire(
		AEPCharacter* Owner,
		const FVector& Origin,
		const FVector& Direction
	);
	
	float GetBoneMultiplier(const FName& BoneName) const;
	static float GetMaterialMultiplier(const UPhysicalMaterial* PM);
};
