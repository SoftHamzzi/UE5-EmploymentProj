// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EPTypes.generated.h"

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

UENUM(BlueprintType)
enum class EEPItemType : uint8
{
	Weapon,
	Ammo,
	Consumable,
	QuestItem,
	Misc
};

USTRUCT()
struct FEPBoneSnapshot
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName BoneName;
	UPROPERTY()
	FTransform WorldTransform;
};

USTRUCT()
struct FEPHitboxSnapshot
{
	GENERATED_BODY()
	
	UPROPERTY()
	float ServerTime = 0.f;
	UPROPERTY()
	FVector Location = FVector::ZeroVector;
	UPROPERTY()
	TArray<FEPBoneSnapshot> Bones;
};

static constexpr ECollisionChannel EP_TraceChannel_Weapon = ECC_GameTraceChannel1;