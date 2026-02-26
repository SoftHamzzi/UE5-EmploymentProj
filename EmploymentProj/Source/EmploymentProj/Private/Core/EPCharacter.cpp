// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/EPCharacter.h"
#include "Camera/CameraComponent.h"
#include "Core/EPPlayerController.h"
#include "Movement/EPCharacterMovement.h"

#include "InputAction.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "Combat/EPWeapon.h"
#include "Combat/EPCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/EPCorpse.h"
#include "Core/EPGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AEPCharacter::AEPCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UEPCharacterMovement>(
		ACharacter::CharacterMovementComponentName))
{
	// --- Body Mesh 설정 ---
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	
	// 메타휴먼
	FaceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));
	FaceMesh->SetupAttachment(GetMesh());
	FaceMesh->SetLeaderPoseComponent(GetMesh());
	FaceMesh->bOwnerNoSee = true;
	
	OutfitMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Outfit"));
	OutfitMesh->SetupAttachment(GetMesh());
	OutfitMesh->SetLeaderPoseComponent(GetMesh());
	//OutfitMesh->bOwnerNoSee = true;
	
	// --- Camera ---
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>("Camera");
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetupAttachment(GetMesh(), FName("head"));
	FirstPersonCamera->SetRelativeLocationAndRotation(FirstPersonCameraOffset, FRotator(0.0f, 90.0f, -90.0f));
	bUseControllerRotationYaw = true;
	
	// --- Combat ---
	CombatComponent = CreateDefaultSubobject<UEPCombatComponent>(TEXT("CombatComponent"));
	
	// --- Movement ---
	UEPCharacterMovement* Movement = Cast<UEPCharacterMovement>(GetCharacterMovement());
	Movement->JumpZVelocity = 420.f;
	Movement->AirControl = 0.5f;
	Movement->BrakingDecelerationFalling = 700.f;
	Movement->NavAgentProps.bCanCrouch = true;
	Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
	
}

void AEPCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (IsLocallyControlled())
	{
		// GetMesh() 제외한 모든 스켈레탈 메시 컴포넌트 숨김
		TArray<USkeletalMeshComponent*> MeshComponents;
		GetComponents<USkeletalMeshComponent>(MeshComponents);

		for (USkeletalMeshComponent* Comp : MeshComponents)
		{
			if (Comp != GetMesh())
				Comp->bOwnerNoSee = true;
		}
	}
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
	
	EnhancedInput->BindAction(
		PC->GetADSAction(),
		ETriggerEvent::Started, this,
		&AEPCharacter::Input_StartADS
	);
	EnhancedInput->BindAction(
		PC->GetADSAction(),
		ETriggerEvent::Completed, this,
		&AEPCharacter::Input_StopADS
	);

	EnhancedInput->BindAction(
		PC->GetCrouchAction(),
		ETriggerEvent::Started, this,
		&AEPCharacter::Input_Crouch
	);
	EnhancedInput->BindAction(
		PC->GetCrouchAction(),
		ETriggerEvent::Completed, this,
		&AEPCharacter::Input_UnCrouch
	);
	
	EnhancedInput->BindAction(
		PC->GetFireAction(),
		ETriggerEvent::Triggered, this,
		&AEPCharacter::Input_Fire
	);
}

UCameraComponent* AEPCharacter::GetCameraComponent() const { return FirstPersonCamera; }

UEPCombatComponent* AEPCharacter::GetCombatComponent() const
{
	return CombatComponent;
}

float AEPCharacter::TakeDamage(
	float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCause)
{
	if(!HasAuthority()) return 0.f;
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent,
		EventInstigator, DamageCause);
	
	HP = FMath::Clamp(HP - ActualDamage, 0.f, MaxHP);
	if (HP <= 0.f) Die(EventInstigator);
	
	Multicast_PlayHitReact();
	Multicast_PlayPainSound();
	
	if (AEPPlayerController* InstigatorPC = Cast<AEPPlayerController>(EventInstigator))
		InstigatorPC->Client_PlayHitConfirmSound();
	
	return ActualDamage;
}

void AEPCharacter::Die(AController* Killer)
{
	if (!HasAuthority()) return;
	
	AController* VictimController = GetController();
	
	// 무기 처리                                                                                                                                                                                                                                                                                                  
	if (CombatComponent && CombatComponent->GetEquippedWeapon())
	{
		CombatComponent->GetEquippedWeapon()->SetActorHiddenInGame(true);
		CombatComponent->GetEquippedWeapon()->SetActorEnableCollision(false);
	}
	
	if (AEPGameMode* GM = GetWorld()->GetAuthGameMode<AEPGameMode>())
		GM->OnPlayerKilled(Killer, GetController());
	
	Multicast_Die();
	
	if (VictimController)
		VictimController->UnPossess();
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
	if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
	{
		if (CMC->bWantsToAim) return;
		CMC->bWantsToSprint = true;
	}
}

void AEPCharacter::Input_StopSprint(const FInputActionValue& Value)
{
	if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
	{
		CMC->bWantsToSprint = false;
	}
}

// Getter
bool AEPCharacter::GetIsSprinting() const
{
	if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
	{
		return CMC->bWantsToSprint;
	}
	return false;
}

bool AEPCharacter::GetIsAiming() const
{
	if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
	{
		return CMC->bWantsToAim;
	}
	return false;
}

void AEPCharacter::Input_StartADS(const FInputActionValue& Value)
{
	if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
	{
		CMC->bWantsToAim = true;
		CMC->bWantsToSprint = false;
	}
	
	// FOV 변경 (로컬만)
	if (IsLocallyControlled() && FirstPersonCamera)
		FirstPersonCamera->SetFieldOfView(60.f);
}

void AEPCharacter::Input_StopADS(const FInputActionValue& Value)
{
	if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
	{
		CMC->bWantsToAim = false;
	}
	// FOV 변경 (로컬만)
	if (IsLocallyControlled() && FirstPersonCamera)
		FirstPersonCamera->SetFieldOfView(90.f);
}

void AEPCharacter::Input_Crouch(const FInputActionValue& Value)
{
	Crouch();
}

void AEPCharacter::Input_UnCrouch(const FInputActionValue& Value)
{
	UnCrouch();
}

void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
	if (!CombatComponent) return;
	CombatComponent->RequestFire(
		FirstPersonCamera->GetComponentLocation(),
		FirstPersonCamera->GetForwardVector()
	);
}

void AEPCharacter::OnRep_HP()
{
	UE_LOG(LogTemp, Warning, TEXT("Current HP: %d"), HP);
}

void AEPCharacter::Multicast_Die_Implementation()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);
}

void AEPCharacter::Multicast_PlayHitReact_Implementation()
{
	if (HitReactMontage)
		PlayAnimMontage(HitReactMontage);
}

void AEPCharacter::Multicast_PlayPainSound_Implementation()
{
	if (PainSound)
		UGameplayStatics::PlaySoundAtLocation(this, PainSound, GetActorLocation());
}

void AEPCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION(AEPCharacter, HP, COND_OwnerOnly);
}
