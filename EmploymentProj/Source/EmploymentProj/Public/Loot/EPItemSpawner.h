// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EPItemSpawner.generated.h"

class AEPPickup;
class UEPLootTable;
class UBillboardComponent;

UCLASS()
class EMPLOYMENTPROJ_API AEPItemSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEPItemSpawner();

	void SpawnLoot();
	void ClearLoot();
	
protected:
	UPROPERTY(EditAnywhere, Category = "Loot")
	TObjectPtr<UEPLootTable> LootTable;
	
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "1"))
	int32 RollCount = 1;
	
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0.0"))
	float SpawnRadius = 0.f;
	
	UPROPERTY(EditAnywhere, Category = "Loot")
	bool bAlignToGround = true;

private:
	FVector GetSpawnPoint() const;
	
	TArray<TWeakObjectPtr<AEPPickup>> SpawnedPickups;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Billboard;
#endif
	
};
