// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/EPItemDefinition.h"
#include "EPWeaponDefinition.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPWeaponDefinition : public UEPItemDefinition
{
	GENERATED_BODY()
	
public:
	// --- 기본 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName WeaponName;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EEPFireMode FireMode = EEPFireMode::Auto;
	
	// --- 전투 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
	float Damage = 20.f;
	
	// 연사 속도 (초당 발수)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Combat")
	float FireRate = 5.f;
	
	// 최대 탄약
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
	uint8 MaxAmmo = 30;
	
	// 장전 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
	float ReloadTime = 2.0f;
	
	// --- 탄 퍼짐 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float BaseSpread = 0.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float SpreadPerShot = 0.1f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float MaxSpread = 5.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float SpreadRecoveryRate = 3.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float ADSSpreadMultiplier = 0.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float MovingSpreadMultiplier = 1.5f;
	
	// --- 반동 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float RecoilPitch = 0.3f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float RecoilYaw = 0.1f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float RecoilRecoveryRate = 5.0f;
	
	// --- 애니메이션 (링크드 애님 레이어) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TSubclassOf<UAnimInstance> WeaponAnimLayer;
	
	// --- 비주얼 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visual")
	TSoftObjectPtr<USkeletalMesh> WeaponMesh;
	
	// PrimaryDataAsset Id 오버라이드
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
