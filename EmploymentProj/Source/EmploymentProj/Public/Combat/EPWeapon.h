// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/EPWeaponData.h"
#include "EPWeapon.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API AEPWeapon : public AActor
{
	GENERATED_BODY()
	
public:
	AEPWeapon();
	
	// --- 스펙 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UEPWeaponData> WeaponData;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;
	
	// --- 복제 ---
	UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo, BlueprintReadOnly, Category = "Weapon")
	uint8 CurrentAmmo = 0;
	
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon")
	uint8 MaxAmmo = 30;
	
	// --- 인터페이스 ---
	bool CanFire() const;
	void Fire(FVector& OutDirection);
	FVector ApplySpread(const FVector& Direction) const;
	
	void StartReload();
	void FinishReload();
	
	float GetDamage() const;
	FORCEINLINE float GetRecoilPitch() const { return WeaponData->RecoilPitch; }
	FORCEINLINE float GetRecoilYaw() const { return WeaponData->RecoilYaw; }
	FORCEINLINE float GetCurrentSpread() const { return CurrentSpread; }
	
protected:
	// --- 서버 런타임 상태 (복제 X) ---
	EEPWeaponState WeaponState = EEPWeaponState::Idle;
	float LastFireTime = 0.f;
	float CurrentSpread = 0.f; // 현재 퍼짐 (연사 시 누적)
	uint8 ConsecutiveShots = 0; // 연속 발사 수
	
	FTimerHandle ReloadTimerHandle;
	
	void UpdateSpread(float DeltaTime);
	float CalculateSpread() const;
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION()
	void OnRep_CurrentAmmo() const;
};
