// Fill out your copyright notice in the Description page of Project Settings.


#include "Loot/EPItemSpawner.h"

#include "Components/BillboardComponent.h"
#include "Engine/GameInstance.h"
#include "Data/EPItemDefinitionSubsystem.h"
#include "Data/EPLootDeveloperSettings.h"
#include "Engine/World.h"
#include "Loot/EPLootTable.h"
#include "Loot/EPPickup.h"

// Sets default values
AEPItemSpawner::AEPItemSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	Billboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	if (Billboard)
	{
		Billboard->SetupAttachment(RootComponent);
		Billboard->bIsScreenSizeScaled = true;
	}
#endif

	SetHidden(true);
	SetCanBeDamaged(false);
}

void AEPItemSpawner::SpawnLoot()
{
	if (!HasAuthority()) return;
	
	if (!LootTable)
	{
		UE_LOG(LogTemp, Error, TEXT("[Loot] %s: LootTable 미지정"), *GetName());
		return;
	}
	
	UGameInstance* GI = GetGameInstance();
	UEPItemDefinitionSubsystem* Defs =
		GI ? GI->GetSubsystem<UEPItemDefinitionSubsystem>() : nullptr;
	if (!Defs) return;
	
	const UEPLootDeveloperSettings* Settings = GetDefault<UEPLootDeveloperSettings>();
	UClass* PickupClass = Settings->PickupClass.IsNull()
		? AEPPickup::StaticClass()
		: Settings->PickupClass.Get();
	
	if (!PickupClass)
	{
		PickupClass = AEPPickup::StaticClass();
		UE_LOG(LogTemp, Error, TEXT("[Loot] PickupClass 로드 실패: %s"),
			*Settings->PickupClass.ToString());
		return;
	}
	
	for (int32 i=0; i<RollCount; i++)
	{
		FName RolledId;
		
		if (!RollLootTable(LootTable, RolledId))
		{
			UE_LOG(LogTemp, Error, TEXT("[Loot] %s: 롤 실패 - %s 데이터 오류"),
				*GetName(), *GetNameSafe(LootTable));
			continue;
		}
		if (RolledId.IsNone()) { continue; }
		
		FEPItemState NewState;
		if (!Defs->MakeItemState(RolledId, NewState))
		{
			UE_LOG(LogTemp, Error, TEXT("[Loot] %s: Definition 없음 - 스폰 생략"),
				*RolledId.ToString());
			continue;
		}
		
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		
		AEPPickup* Pickup = GetWorld()->SpawnActor<AEPPickup>(
			PickupClass, GetSpawnPoint(), FRotator::ZeroRotator, Params);
		if (!Pickup) { continue; }
		
		Pickup->InitPickup(RolledId, NewState);
		SpawnedPickups.Add(Pickup);
	}
	
}

void AEPItemSpawner::ClearLoot()
{
	if (!HasAuthority()) return;
	
	for (const TWeakObjectPtr<AEPPickup>& Weak : SpawnedPickups)
		if (AEPPickup* P = Weak.Get())
			P->Destroy();
	SpawnedPickups.Reset();
}

FVector AEPItemSpawner::GetSpawnPoint() const
{
	FVector Point = GetActorLocation();
	
	if (SpawnRadius > 0.f)
	{
		const FVector2D Offset = FMath::RandPointInCircle(SpawnRadius);
		Point += FVector(Offset.X, Offset.Y, 0.f);
	}
	
	if (bAlignToGround)
	{
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		
		if (GetWorld()->LineTraceSingleByChannel(
			Hit, Point + FVector(0, 0, 200.f), Point - FVector(0, 0, 500.f),
			ECC_Visibility, Params)) {
			Point = Hit.ImpactPoint + FVector(0, 0, 5.f);
		}
	}
	return Point;
}
