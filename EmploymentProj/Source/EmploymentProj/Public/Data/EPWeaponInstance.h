// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/EPItemInstance.h"
#include "EPWeaponInstance.generated.h"

UCLASS(BlueprintType)
class EMPLOYMENTPROJ_API UEPWeaponInstance : public UEPItemInstance
{
	GENERATED_BODY()
	
public:
	// 현재 탄약
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	int32 CurrentAmmo = 0;
	
	// 내구도
	UPROPERTY(BlueprintReadWrite, Category = "Weapon")
	float Durability = 100.f;
	
	// 팩토리 함수
	static UEPWeaponInstance* CreateWeaponInstance(
		UObject* Outer, FName InItemId,
		int32 InMaxAmmo, UEPItemDefinition* InDefinition = nullptr);
};
