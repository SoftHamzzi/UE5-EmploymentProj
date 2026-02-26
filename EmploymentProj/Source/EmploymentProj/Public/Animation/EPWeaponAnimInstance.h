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
	virtual void NativeInitializeAnimation() override;
	
	UPROPERTY(Transient)
	TObjectPtr<UEPAnimInstance> CachedMainAnimBP;
	
public:
	UFUNCTION(BlueprintPure, meta=(BlueprintThreadSafe), Category = "Animation")
	UEPAnimInstance* GetMainBPThreadSafe() const { return CachedMainAnimBP; }
};
