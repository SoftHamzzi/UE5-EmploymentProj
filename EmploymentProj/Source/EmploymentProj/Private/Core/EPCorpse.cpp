// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/EPCorpse.h"

#include "AI/NavigationModifier.h"
#include "Net/UnrealNetwork.h"
#include "Core/EPCharacter.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Data/EPItemData.h"

// Sets default values
AEPCorpse::AEPCorpse()
{
	bReplicates = true;
	// bAlwaysRelevant = false; (기본 값)
	CorpseMesh = CreateDefaultSubobject<USkeletalMeshComponent>("CorpseMesh");
	RootComponent = CorpseMesh;
	
	FaceMesh = CreateDefaultSubobject<USkeletalMeshComponent>("FaceMesh");
	FaceMesh->SetupAttachment(CorpseMesh);
	FaceMesh->SetLeaderPoseComponent(CorpseMesh);
	
	OutfitMesh = CreateDefaultSubobject<USkeletalMeshComponent>("OutfitMesh");
	OutfitMesh->SetupAttachment(CorpseMesh);
	OutfitMesh->SetLeaderPoseComponent(CorpseMesh);
}

// 사망한 캐릭터로부터 초기화 (서버에서 호출)
void AEPCorpse::InitializeFromCharacter(AEPCharacter* DeadCharacter)
{
	if (!HasAuthority() || !DeadCharacter) return;

	// Body
	USkeletalMeshComponent* SrcBody = DeadCharacter->GetMesh();
	if (SrcBody && SrcBody->GetSkeletalMeshAsset())
		CorpseMeshAsset = SrcBody->GetSkeletalMeshAsset();

	// Face
	USkeletalMeshComponent* SrcFace = DeadCharacter->GetFaceMesh();
	if (SrcFace && SrcFace->GetSkeletalMeshAsset())
		CorpseFaceAsset = SrcFace->GetSkeletalMeshAsset();

	// Outfit
	USkeletalMeshComponent* SrcOutfit = DeadCharacter->GetOutfitMesh();
	if (SrcOutfit && SrcOutfit->GetSkeletalMeshAsset())
		CorpseOutfitAsset = SrcOutfit->GetSkeletalMeshAsset();

	ApplyCorpseMesh();
}

void AEPCorpse::OnRep_CorpseMeshAsset() { ApplyCorpseMesh(); }

void AEPCorpse::OnRep_CorpseFaceAsset() { ApplyCorpseMesh(); }

void AEPCorpse::OnRep_CorpseOutfitAsset() { ApplyCorpseMesh(); }

void AEPCorpse::ApplyCorpseMesh()
{
	if (CorpseMeshAsset)
	{
		CorpseMesh->SetSkeletalMeshAsset(CorpseMeshAsset);
		CorpseMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		
		GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
		{
			CorpseMesh->SetSimulatePhysics(true);
		});
	}
	
	if (CorpseFaceAsset)
	{
		FaceMesh->SetSkeletalMeshAsset(CorpseFaceAsset);
	}
	
	if (CorpseOutfitAsset)
	{
		OutfitMesh->SetSkeletalMeshAsset(CorpseOutfitAsset);
	}
	
}
	
// 상호작용 (루팅)
void AEPCorpse::Interact(AEPCharacter* Looter)
{
	
}

void AEPCorpse::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AEPCorpse, PlayerName);
	
	// 메시
	DOREPLIFETIME(AEPCorpse, CorpseMeshAsset);
	DOREPLIFETIME(AEPCorpse, CorpseFaceAsset);
	DOREPLIFETIME(AEPCorpse, CorpseOutfitAsset);
}