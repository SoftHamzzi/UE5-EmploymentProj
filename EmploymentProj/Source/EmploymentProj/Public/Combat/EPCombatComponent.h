// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EPCombatComponent.generated.h"

class UNiagaraSystem;
class USoundBase;
class AEPCharacter;
class AEPWeapon;
class UEPPhysicalMaterial;
class AEPProjectile;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class EMPLOYMENTPROJ_API UEPCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	
	UEPCombatComponent();
	
	// === 함수 ===
	// --- Getter/Setter ---
	void EquipWeapon(AEPWeapon* NewWeapon);
	void UnequipWeapon();
	
	AEPCharacter* GetOwnerCharacter() const;
	AEPWeapon* GetEquippedWeapon() const;
	
	// Request 이관 함수
	void RequestFire(const FVector& Origin, const FVector& Direction, float ClientFireTime);
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// === 변수 ===
	float LocalLastFireTime = 0.f;
	
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
	
	// --- 선언 ---
	UFUNCTION()
	void PlayLocalMuzzleEffect(const FVector& MuzzleLocation);

	UFUNCTION()
	void PlayLocalImpactEffect(const FVector& ImpactPoint, const FVector& ImpactNormal);

	void SpawnLocalCosmeticProjectile(const FVector& MuzzleLocation, const FVector& Direction);
	
	// --- 동기화 ---
	UFUNCTION()
	void OnRep_EquippedWeapon();
	
	// --- RPC ---
	UFUNCTION(Server, Reliable)
	void Server_Fire(const FVector_NetQuantize& Origin, const FVector_NetQuantizeNormal& Direction, float ClientFireTime);
	UFUNCTION(Server, Reliable)
	void Server_Reload();
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayMuzzleEffect(const FVector_NetQuantize& MuzzleLocation);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayImpactEffect(const FVector_NetQuantize& ImpactPoint, const FVector_NetQuantize& ImpactNormal);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_SpawnCosmeticProjectile(
		const FVector_NetQuantize& MuzzleLocation,
		const FVector_NetQuantizeNormal& Direction);

private:
	float LastServerFireTime = -999.f;
	
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
