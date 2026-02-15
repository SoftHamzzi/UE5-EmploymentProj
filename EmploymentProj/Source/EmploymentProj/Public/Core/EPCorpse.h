// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EPCorpse.generated.h"

class AEPCharacter;

UCLASS()
class EMPLOYMENTPROJ_API AEPCorpse : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEPCorpse();
	
	// 사망한 캐릭터로부터 초기화 (서버에서 호출)
	void InitializeFromCharacter(AEPCharacter* DeadCharacter);
	
	// 상호작용 (루팅)
	void Interact(AEPCharacter* Looter);

protected:
	// Body 메시
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> CorpseMesh;
	
	// Face 메시
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> FaceMesh;
	
	// Outfit 메시
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> OutfitMesh;
	
	UPROPERTY(ReplicatedUsing = OnRep_CorpseMeshAsset)
	TObjectPtr<USkeletalMesh> CorpseMeshAsset;
	
	UPROPERTY(ReplicatedUsing = OnRep_CorpseFaceAsset)
	TObjectPtr<USkeletalMesh> CorpseFaceAsset;
	
	UPROPERTY(ReplicatedUsing = OnRep_CorpseOutfitAsset)
	TObjectPtr<USkeletalMesh> CorpseOutfitAsset;
	
	// 플레이어 이름
	UPROPERTY(Replicated)
	FString PlayerName;
	
	UFUNCTION()
	void OnRep_CorpseMeshAsset();
	UFUNCTION()
	void OnRep_CorpseFaceAsset();
	UFUNCTION()
	void OnRep_CorpseOutfitAsset();
	
	void ApplyCorpseMesh();
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};