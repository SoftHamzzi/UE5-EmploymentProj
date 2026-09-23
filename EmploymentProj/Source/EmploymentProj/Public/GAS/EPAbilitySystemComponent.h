// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "EPAbilitySystemComponent.generated.h"


UCLASS()
class EMPLOYMENTPROJ_API UEPAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }
};
