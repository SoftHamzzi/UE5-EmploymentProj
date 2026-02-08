// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EPCharacterMovement.generated.h"

/**
 * 
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
public:
	// --- 이동 상태 플래그 (복제X, CompressedFlags로 전송) ---
	uint8 bWantsToSprint : 1;
	uint8 bWantsToAim : 1;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeed = 650.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float AimSpeed = 200.f;
	
	// --- CMC 오버라이드 ---
	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	
};
