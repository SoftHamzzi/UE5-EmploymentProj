// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EPAnimInstance.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API UEPAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	// --- Locomotion ---
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Speed = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsSprinting = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsFalling = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsCrouching = false;
	
	// --- Combat ---
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsAiming = false;
	
	// --- Aim Offset ---
	UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
	float AimPitch = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
	float AimYaw = 0.f;
	
	// --- IK ---
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FTransform RightHandIKTransform;
	
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FTransform LeftHandIKTransform;
	
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FVector RightElbowWorldPos;
	
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FVector LeftElbowWorldPos;
	
protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	
private:
	TWeakObjectPtr<class AEPCharacter> CachedCharacter;
	
};
