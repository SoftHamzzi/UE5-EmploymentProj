// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/EPCorpse.h"
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
}

// 사망한 캐릭터로부터 초기화 (서버에서 호출)
void AEPCorpse::InitializeFromCharacter(AEPCharacter* DeadCharacter)
{
	if (!HasAuthority() || !DeadCharacter) return;
	
	USkeletalMeshComponent* SrcMesh = DeadCharacter->GetMesh();
	if (!SrcMesh || !SrcMesh->GetSkeletalMeshAsset()) return;
	
	CorpseMeshAsset = SrcMesh->GetSkeletalMeshAsset();
	ApplyCorpseMesh();
}

void AEPCorpse::OnRep_CorpseMeshAsset()
{
	ApplyCorpseMesh();
}

void AEPCorpse::ApplyCorpseMesh()
{
	if (!CorpseMeshAsset) return;
	CorpseMesh->SetSkeletalMeshAsset(CorpseMeshAsset);
	CorpseMesh->SetCollisionProfileName(TEXT("Ragdoll"));
	CorpseMesh->SetSimulatePhysics(true);
}
	
// 상호작용 (루팅)
void AEPCorpse::Interact(AEPCharacter* Looter)
{
	
}

void AEPCorpse::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AEPCorpse, Inventory);
	DOREPLIFETIME(AEPCorpse, PlayerName);
	DOREPLIFETIME(AEPCorpse, CorpseMeshAsset);
}