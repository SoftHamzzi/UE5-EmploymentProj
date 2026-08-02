// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPtr.h"
#include "EPLootDeveloperSettings.generated.h"

class UDataTable;
class UStaticMesh;
class AEPPickup;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "EP Loot"))
class EMPLOYMENTPROJ_API UEPLootDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config, EditAnywhere, Category = "Data",
		meta = (RequiredAssetDataTags = "RowStructure=/Script/EmploymentProj.EPItemData"))
	TSoftObjectPtr<UDataTable> ItemDataTable;
	
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bEnableLootDebugLog = false;
	
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bEnableSpawnerDebugDraw = false;
	
	UPROPERTY(Config, EditAnywhere, Category = "Loot")
	TSoftObjectPtr<UStaticMesh> PlaceholderPickupMesh;
	
	UPROPERTY(Config, EditAnywhere, Category = "Loot")
	TSoftClassPtr<AEPPickup> PickupClass;
};
