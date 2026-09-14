// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/EPWeaponDefinition.h"
#include "EPWeapon.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API AEPWeapon : public AActor
{
	GENERATED_BODY()

public:
	
	// === 변수 ===
	// --- 스펙 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UEPWeaponDefinition> WeaponDef;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;
	
	// === 함수 ===
	AEPWeapon();
	
	UFUNCTION(BlueprintImplementableEvent)
	void BP_PlayImpactEffect(const FVector& ImpactPoint, const FVector& ImpactNormal, uint8 SurfaceType);
	
	// --- 인터페이스 ---
    bool CanFire() const;
    void Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets);
    FVector ApplySpread(const FVector& Direction) const;
    
    float GetDamage() const;
    FORCEINLINE float GetRecoilPitch() const { return WeaponDef->RecoilPitch; }
    FORCEINLINE float GetRecoilYaw() const { return WeaponDef->RecoilYaw; }
    FORCEINLINE float GetCurrentSpread() const { return CurrentSpread; }
	
protected:
	// === 변수 ===
	// --- 서버 런타임 상태 (복제 X) ---
	float LastFireTime = 0.f;
	float CurrentSpread = 0.f; // 현재 퍼짐 (연사 시 누적)
	uint8 ConsecutiveShots = 0; // 연속 발사 수
	
	// === 함수 ===
	void UpdateSpread(float DeltaTime);
	float CalculateSpread() const;
	
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
private:
	static constexpr int32 CDFTableSize = 256;
	TArray<float> SpreadCDFTable;
	
	void BuildSpreadCDFTable();
	
	float SampleSpread() const;
};
