// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EPLootTable.generated.h"

class UEPLootTable;

USTRUCT(BlueprintType)
struct FEPLootEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float Weight = 1.f;
	
	UPROPERTY(EditAnywhere)
	FName ItemId;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UEPLootTable> SubTable;
};

UCLASS()
class EMPLOYMENTPROJ_API UEPLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Loot")
	TArray<FEPLootEntry> Entries;
	
	UPROPERTY(EditAnywhere, Category = "Loot", meta = (ClampMin = "0.0"))
	float EmptyWeight = 0.f;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override final
	{
		return FPrimaryAssetId(TEXT("LootTable"), GetFName());
	}
	
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

EMPLOYMENTPROJ_API bool RollLootTable(const UEPLootTable* Table, FName& OutItemId);