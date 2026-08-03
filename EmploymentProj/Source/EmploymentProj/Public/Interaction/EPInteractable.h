// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EPInteractable.generated.h"

class AEPCharacter;

UINTERFACE(MinimalAPI)
class UEPInteractable : public UInterface
{
	GENERATED_BODY()
};

class EMPLOYMENTPROJ_API IEPInteractable
{
	GENERATED_BODY()

public:
	virtual FText GetInteractText() const = 0;
	virtual bool CanInteract(AEPCharacter* Interactor, FText& OutReason) const = 0;
	
	virtual float GetInteractDuration() const { return 0.f; }
	
	virtual bool OnInteract(AEPCharacter* Interactor, FText& OutReason) = 0;
};
