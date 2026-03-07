// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/EPTypes.h"
#include "EPServerSideRewindComponent.generated.h"

class AEPCharacter;
class AEPWeapon;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class EMPLOYMENTPROJ_API UEPServerSideRewindComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UEPServerSideRewindComponent();
	
	bool ConfirmHitscan(
		AEPCharacter* Shooter,
		AEPWeapon* EquippedWeapon,
		const FVector& Origin,
		const TArray<FVector>& Directions,
		float ClientFireTime,
		TArray<FHitResult>& OutConfirmedHits);
	
	// Lag Compensation: 서버에서 호출한다.
	FEPHitboxSnapshot GetSnapshotAtTime(float TargetTime) const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
	// --- Net Prediction ---
	static const TArray<FName> HitBones; // 기록할 본 목록
	float SnapshotAccumulator = 0.f;
	int32 MaxHistoryCount = 0;
	
	// 시간 오름차순으로 유지 - [0] 오래됨, [Last] 최신
	// 링버퍼 대신 단순 배열을 통해 GetSnapshotAtTime의 탐색 순서 보장
	UPROPERTY()
	TArray<FEPHitboxSnapshot> HitboxHistory;
	
	// 서버 Tick에서 SnapshotInterval마다 호출
	void SaveHitboxSnapshot();
	
	TArray<AEPCharacter*> GetHitscanCandidates(
		AEPCharacter* Shooter,
		AEPWeapon* EquippedWeapon,
		const FVector& Origin,
		const TArray<FVector>& Directions,
		float ClientFireTime) const;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
