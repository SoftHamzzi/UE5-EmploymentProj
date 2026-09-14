// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EPCombatComponent.generated.h"

class UNiagaraSystem;
class USoundBase;
class AEPCharacter;
class AEPWeapon;

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
	void RequestFire(const FVector& Origin, const FVector& Direction);
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

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
	// --- 동기화 ---
	UFUNCTION()
	void OnRep_EquippedWeapon();
	
	// --- RPC ---
	UFUNCTION(Server, Reliable)
	void Server_Fire(const FVector& Origin, const FVector& Direction);
	UFUNCTION(Server, Reliable)
	void Server_Reload();
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayMuzzleEffect(const FVector_NetQuantize& MuzzleLocation);
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayImpactEffect(const FVector_NetQuantize& ImpactPoint, const FVector_NetQuantize& ImpactNormal);
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
