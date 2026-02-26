// Fill out your copyright notice in the Description page of Project Settings.


#include "Animation/EPAnimInstance.h"
#include "Core/EPCharacter.h"
#include "Movement/EPCharacterMovement.h"
#include "KismetAnimationLibrary.h"
#include "Combat/EPCombatComponent.h"
#include "Combat/EPWeapon.h"

void UEPAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	CachedCharacter = Cast<AEPCharacter>(TryGetPawnOwner());
}

void UEPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	
	AEPCharacter* Character = CachedCharacter.Get();
	if (!Character) return;
	
	// Locomotion
	FVector Velocity = Character->GetVelocity();
	Speed = Velocity.Size2D();
	
	FRotator AimRotation = Character->GetBaseAimRotation();
	Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, Character->GetActorRotation());
	
	bIsSprinting = Character->GetIsSprinting();
	bIsAiming = Character->GetIsAiming();
	bIsFalling = Character->GetCharacterMovement()->IsFalling();
	bIsCrouching = Character->bIsCrouched;
	
	// Aim Offset
	AimPitch = FMath::ClampAngle(AimRotation.Pitch, -90.f, 90.f);
	AimYaw = FMath::ClampAngle(
		FRotator::NormalizeAxis(AimRotation.Yaw-Character->GetActorRotation().Yaw),
		-90.f, 90.f);
	
	// IK
	if (UEPCombatComponent* Combat = Character->GetCombatComponent())
	{
		if (AEPWeapon* Weapon = Combat->GetEquippedWeapon())
		{
			UMeshComponent* WeaponMesh = Weapon->WeaponMesh;
			if (WeaponMesh)
			{
				FTransform WorldLeftHandIK = WeaponMesh->GetSocketTransform(FName("LeftHandIK"));
				FTransform HandR_World = Character->GetMesh()->GetBoneTransform(FName("hand_r"));
				
				LeftHandIKTransform = WorldLeftHandIK.GetRelativeTransform(HandR_World);
			}
		}
	}
	
}