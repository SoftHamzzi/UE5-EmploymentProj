// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/EPTypes.h"
#include "EPWeaponData.generated.h"

UCLASS(BlueprintType)
class EMPLOYMENTPROJ_API UEPWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// -- 기본 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName WeaponName;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    EEPFireMode FireMode = EEPFireMode::Auto;
	
	// --- 전투 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float Damage = 20.f;
	
	// 연사 속도 (초당 발수)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat")
	float FireRate = 5.f;
	
	// 최대 탄약
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	uint8 MaxAmmo = 30;
	
	// 장전 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float ReloadTime = 2.0f;
	
	// --- 탄 퍼짐 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
	float BaseSpread = 0.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
	float SpreadPerShot = 0.1f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
	float MaxSpread = 5.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
	float SpreadRecoveryRate = 3.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
	float ADSSpreadMultiplier = 0.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spread")
	float MovingSpreadMultiplier = 1.5f;
	
	// --- 반동 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float RecoilPitch = 0.3f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float RecoilYaw = 0.1f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	float RecoilRecoveryRate = 5.0f;
	
	// --- 반동 테스트 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recoil")
	TArray<FVector2D> RecoilPattern;
	
	// PrimaryDataAsset ID
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
