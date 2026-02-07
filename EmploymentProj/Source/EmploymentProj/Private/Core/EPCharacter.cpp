// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/EPCharacter.h"
#include "Camera/CameraComponent.h"
#include "Core/EPPlayerController.h"
#include "Net/UnrealNetwork.h"

#include "InputAction.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AEPCharacter::AEPCharacter()
{
	// PrimaryActorTick.bCanEverTick = true;
	GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;
	
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>("Camera");
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetupAttachment(GetMesh(), FName("head"));
	FirstPersonCamera->SetRelativeLocationAndRotation(FirstPersonCameraOffset, FRotator(0.0f, 90.0f, -90.0f));
	bUseControllerRotationYaw = true;
	
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->JumpZVelocity = 420.f;
	Movement->AirControl = 0.5f;
	
	Movement->BrakingDecelerationFalling = 700.f;
	// Movement->FallingLateralFriction = 0f; // 공중 마찰력
	
	Movement->MaxWalkSpeed = WalkSpeed;
}

void AEPCharacter::BeginPlay()
{
	Super::BeginPlay();
}

// Enhanced Input 바인딩
void AEPCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	AEPPlayerController* PC = GetController<AEPPlayerController>();
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	
	if (!PC || !EnhancedInput) return;
	
	EnhancedInput->BindAction(
		PC->GetMoveAction(),
		ETriggerEvent::Triggered, this,
		&AEPCharacter::Input_Move
	);
	
	EnhancedInput->BindAction(
		PC->GetLookAction(),
		ETriggerEvent::Triggered, this,
		&AEPCharacter::Input_Look
	);
	
	EnhancedInput->BindAction(
		PC->GetJumpAction(),
		ETriggerEvent::Triggered, this,
		&AEPCharacter::Input_Jump
	);
	EnhancedInput->BindAction(
		PC->GetJumpAction(),
		ETriggerEvent::Completed, this,
		&AEPCharacter::Input_StopJumping
	);
	
	EnhancedInput->BindAction(
		PC->GetSprintAction(),
		ETriggerEvent::Triggered, this,
		&AEPCharacter::Input_StartSprint
	);
	EnhancedInput->BindAction(
		PC->GetSprintAction(),
		ETriggerEvent::Completed, this,
		&AEPCharacter::Input_StopSprint
	);

}

// --- 입력 핸들러 ---
// 이동 (WASD)
void AEPCharacter::Input_Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	
	if (Controller == nullptr) return;
	
	const FRotator YawRotation(0.0, Controller->GetControlRotation().Yaw, 0.0);
	const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	
	AddMovementInput(ForwardDir, Input.X);
	AddMovementInput(RightDir, Input.Y);
}
	
// 시점 (마우스)
void AEPCharacter::Input_Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	const float Sensitivity = 0.5f;
	
	AddControllerYawInput(Input.X * Sensitivity);
	AddControllerPitchInput(Input.Y * Sensitivity);
}
	
// 점프
void AEPCharacter::Input_Jump(const FInputActionValue& Value)
{
	Jump();
}
void AEPCharacter::Input_StopJumping(const FInputActionValue& Value)
{
	StopJumping();
}

void AEPCharacter::Input_StartSprint(const FInputActionValue& Value)
{
	Server_SetSprinting(true);
}

void AEPCharacter::Input_StopSprint(const FInputActionValue& Value)
{
	Server_SetSprinting(false);
}

void AEPCharacter::OnRep_IsSprinting()
{
	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
}

void AEPCharacter::Server_SetSprinting_Implementation(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	
	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
}

void AEPCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AEPCharacter, bIsSprinting);
}
