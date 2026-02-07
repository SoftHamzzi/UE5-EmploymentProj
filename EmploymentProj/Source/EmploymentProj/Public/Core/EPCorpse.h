// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EPCorpse.generated.h"

class AEPCharacter;
struct FItemData;

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
	// 시체 메시
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> CorpseMesh;
	
	// 인벤토리 (복제됨 - Relevancy 범위 내)
	UPROPERTY(Replicated)
	TArray<FItemData> Inventory;
	
	// 플레이어 이름
	UPROPERTY(Replicated)
	FString PlayerName;
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};