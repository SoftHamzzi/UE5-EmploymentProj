// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/EPPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Core/EPPlayerState.h"
#include "Core/EPCharacter.h"
#include "HUD/EPCrosshairWidget.h"
#include "Kismet/GameplayStatics.h"

AEPPlayerController::AEPPlayerController()
{
	
}

void AEPPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
	
	if (IsLocalController() && CrosshairWidgetClass)
	{
		CrosshairWidget = CreateWidget<UEPCrosshairWidget>(this, CrosshairWidgetClass);
		if (CrosshairWidget)
			CrosshairWidget->AddToViewport();
	}
}

void AEPPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void AEPPlayerController::Client_OnKill_Implementation(const FString& VictimName)
{
	UE_LOG(LogTemp, Log, TEXT("You Kill %s"), *VictimName);
}

void AEPPlayerController::Client_PlayHitConfirmSound_Implementation()
{
	if (HitConfirmSound)
		UGameplayStatics::PlaySound2D(this, HitConfirmSound);
}
