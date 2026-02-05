// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/EPCharacter.h"
#include "Camera/CameraComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

AEPCharacter::AEPCharacter()
{
	// PrimaryActorTick.bCanEverTick = true;

}

void AEPCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Enhanced Input 바인딩
void AEPCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

// --- 입력 핸들러 ---
// 이동 (WASD)
void AEPCharacter::Input_Move(const FInputActionValue& Value)
{
	
}
	
// 시점 (마우스)
void AEPCharacter::Input_Look(const FInputActionValue& Value)
{
	
}
	
// 점프
void AEPCharacter::Input_Jump(const FInputActionValue& Value)
{
	
}
void AEPCharacter::Input_StopJumping(const FInputActionValue& Value)
{
	
}