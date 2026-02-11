// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EPTypes.generated.h"

USTRUCT(BlueprintType)
struct FItemData
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	FName ItemName;
	
	UPROPERTY(EditAnywhere)
	int32 Value;
};

UENUM(BlueprintType)
enum class EEPMatchPhase : uint8
{
	Waiting	UMETA(DisplayName = "Waiting"),
	Playing	UMETA(DisplayName = "Playing"),
	Ended	UMETA(DisplayName = "Ended")
};

UENUM(BlueprintType)
enum class EEPItemRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Legendary
};

UENUM(BlueprintType)
enum class EEPFireMode : uint8
{
	Single,
	Burst,
	Auto
};

UENUM(BlueprintType)
enum class EEPWeaponState : uint8
{
	Idle,
	Reloading,
	Firing
};