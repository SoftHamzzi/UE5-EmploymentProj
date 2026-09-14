// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "EPGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPGameInstance : public UGameInstance
{
	GENERATED_BODY()
	
public:
	// === 변수 ===
	
	// === 함수 ===
	virtual void Init() override;
};
