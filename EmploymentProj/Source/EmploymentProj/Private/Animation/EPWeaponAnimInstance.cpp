// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/EPWeaponAnimInstance.h"
#include "Animation/EPAnimInstance.h"

void UEPWeaponAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	
	if (USkeletalMeshComponent* MeshComp = GetOwningComponent())
	{
		CachedMainAnimBP = Cast<UEPAnimInstance>(MeshComp->GetAnimInstance());
	}
}
