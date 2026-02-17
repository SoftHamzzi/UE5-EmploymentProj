// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EPWeaponAnimInstance.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API UEPWeaponAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	
	// 메인 AnimBP에서 복사해올 변수 (레이어 내에서 사용)
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	float Speed = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	float Direction = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsSprinting = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsFalling = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	bool bIsCrouching = false;
	
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bIsAiming = false;
};
