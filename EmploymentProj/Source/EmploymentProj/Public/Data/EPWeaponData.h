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
	// 무기 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FName WeaponName;
	
	// 탄당 대미지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Damage = 20.f;
	
	// 연사 속도 (초당 발수)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
	float FireRate = 5.f;
	
	// 반동 크기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Recoil = 1.f;
	
	// 탄퍼짐 (기본값, 이동/점프 시 증가)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Spread = 0.5f;
	
	// 최대 탄약
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EEPFireMode FireMode = EEPFireMode::Auto;
	
	// PrimaryDataAsset ID
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
