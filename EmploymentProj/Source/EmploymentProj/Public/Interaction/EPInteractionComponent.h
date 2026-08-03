// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EPInteractionComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class EMPLOYMENTPROJ_API UEPInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UEPInteractionComponent();
	
	void Input_Interact();
	
	void RefreshTickEnabled();
	
	FORCEINLINE float GetInteractRange() const { return InteractRange; }
	FORCEINLINE float GetServerRangeTolerance() const { return ServerRangeTolerance; }

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractRange = 250.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float ServerRangeTolerance = 100.f;
	
private:
	void UpdateFocus();
	
	UPROPERTY()
	TObjectPtr<AActor> FocusedActor;
		
};
