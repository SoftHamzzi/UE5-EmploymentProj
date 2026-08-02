// Fill out your copyright notice in the Description page of Project Settings.


#include "Loot/EPPickup.h"

#include "Components/StaticMeshComponent.h"
#include "Data/EPItemDefinition.h"
#include "Data/EPItemDefinitionSubsystem.h"
#include "Data/EPLootDeveloperSettings.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "Engine/StreamableManager.h"

// Sets default values
AEPPickup::AEPPickup()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	bReplicates = true;
	bAlwaysRelevant = false;
	SetReplicateMovement(false);
	NetDormancy = DORM_Initial;
	
	SetNetCullDistanceSquared(25000000.f);
	SetNetUpdateFrequency(1.f);
	
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
}

void AEPPickup::InitPickup(FName InItemId, const FEPItemState& InState)
{
	ItemId = InItemId;
	State = InState;
	
	ApplyVisual();
}

void AEPPickup::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AEPPickup, ItemId);
}

void AEPPickup::OnRep_ItemId()
{
	ApplyVisual();
}

void AEPPickup::ApplyVisual()
{
	if (IsNetMode(NM_DedicatedServer)) return;
	
	const UEPLootDeveloperSettings* Settings = GetDefault<UEPLootDeveloperSettings>();
	if (UStaticMesh* Placeholder = Settings->PlaceholderPickupMesh.LoadSynchronous())
		Mesh->SetStaticMesh(Placeholder);
	
	UGameInstance* GI = GetGameInstance();
	UEPItemDefinitionSubsystem* Defs =
		GI ? GI->GetSubsystem<UEPItemDefinitionSubsystem>() : nullptr;
	UEPItemDefinition* Def = Defs ? Defs->FindDefinition(ItemId) : nullptr;
	
	if (!Def || Def->WorldMesh.IsNull()) return;
	
	MeshHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Def->WorldMesh.ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this, Def]()
		{
			if (UStaticMesh* Loaded = Def->WorldMesh.Get())
				Mesh->SetStaticMesh(Loaded);
		}));
}

