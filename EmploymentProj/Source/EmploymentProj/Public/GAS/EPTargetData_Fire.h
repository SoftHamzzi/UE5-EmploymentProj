// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "EPTargetData_Fire.generated.h"

USTRUCT()
struct EMPLOYMENTPROJ_API FEPTargetData_Fire : public FGameplayAbilityTargetData
{
	GENERATED_BODY()
	
	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;
	
	UPROPERTY()
	float ClientMoveTimeStamp = -1.f;
	
	virtual UScriptStruct* GetScriptStruct() const override { return FEPTargetData_Fire::StaticStruct(); }
	virtual FString ToString() const override { return TEXT("FEPTargetData_Fire"); }
	
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		Direction.NetSerialize(Ar, Map, bOutSuccess);
		Ar << ClientMoveTimeStamp;
		
		bOutSuccess = true;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FEPTargetData_Fire> : public TStructOpsTypeTraitsBase2<FEPTargetData_Fire>
{
	enum
	{
		WithNetSerializer = true
	};
};

