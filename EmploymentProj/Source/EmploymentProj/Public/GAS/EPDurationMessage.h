// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EPDurationMessage.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct FEPDurationMessage
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<AActor> Instigator = nullptr;
	
	UPROPERTY(BlueprintReadWrite)
	float Duration = 0.f;
};