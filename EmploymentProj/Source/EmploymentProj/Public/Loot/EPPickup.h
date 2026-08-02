// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/EPTypes.h"
#include "EPPickup.generated.h"

struct FStreamableHandle;

UCLASS()
class EMPLOYMENTPROJ_API AEPPickup : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEPPickup();
	
	void InitPickup(FName InItemId, const FEPItemState& InState);
	FName GetItemId() const { return ItemId; }
	bool IsClaimed() const { return bClaimed; }
	
	const FEPItemState& GetState() const { return State; }
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_ItemId)
	FName ItemId;
	
	UPROPERTY()
	FEPItemState State;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
	
	UFUNCTION()
	void OnRep_ItemId();
	
	void ApplyVisual();
	
private:
	bool bClaimed = false;
	TSharedPtr<FStreamableHandle> MeshHandle;
	
};
