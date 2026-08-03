// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/EPInteractionComponent.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "GAS/EPNativeGameplayTags.h"
#include "Core/EPCharacter.h"
#include "Core/EPPlayerController.h"
#include "Engine/World.h"
#include "Interaction/EPInteractable.h"
#include "Types/EPTypes.h"

// Sets default values for this component's properties
UEPInteractionComponent::UEPInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	
	SetIsReplicatedByDefault(false);
}

void UEPInteractionComponent::Input_Interact()
{
	if (!IsValid(FocusedActor)) return;
	
	AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner());
	UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	if (!ASC) return;
	
	FGameplayEventData Payload;
	Payload.EventTag = EmpGameplayTags::TAG_Ability_Interact;
	Payload.Instigator = Owner;
	Payload.Target = FocusedActor;
	
	ASC->HandleGameplayEvent(EmpGameplayTags::TAG_Ability_Interact, &Payload);
}

void UEPInteractionComponent::RefreshTickEnabled()
{
	const AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner());
	SetComponentTickEnabled(Owner && Owner->IsLocallyControlled());
}

// Called every frame
void UEPInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateFocus();
}

void UEPInteractionComponent::UpdateFocus()
{
	AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner());
	if (!Owner || !Owner->IsLocallyControlled()) return;
	
	const UCameraComponent* Cam = Owner->GetCameraComponent();
	if (!Cam) return;
	
	const FVector Start = Cam->GetComponentLocation();
	const FVector Dir = Owner->GetControlRotation().Vector();
	const FVector End = Start + Dir * InteractRange;
	
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);
	
	AActor* NewFocus = nullptr;
	if (GetWorld()->LineTraceSingleByChannel(
		Hit, Start, End, EP_TraceChannel_Interact, Params))
	{
		if (Hit.GetActor() && Hit.GetActor()->Implements<UEPInteractable>())
			NewFocus = Hit.GetActor();
	}
	
	if (NewFocus == FocusedActor) return;
	FocusedActor = NewFocus;
	
	AEPPlayerController* PC = Cast<AEPPlayerController>(Owner->GetController());
	if (!PC) return;
	
	if (!FocusedActor)
	{
		PC->SetInteractPrompt(FText::GetEmpty(), false);
		return;
	}
	
	IEPInteractable* Interactable = Cast<IEPInteractable>(FocusedActor);
	if (!Interactable) return;
	
	FText Reason;
	const bool bCan = Interactable->CanInteract(Owner, Reason);
	PC->SetInteractPrompt(bCan ? Interactable->GetInteractText() : Reason, bCan);
}

