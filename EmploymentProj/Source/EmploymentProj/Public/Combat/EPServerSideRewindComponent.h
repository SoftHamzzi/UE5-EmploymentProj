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
	UEPServerSideRewindComponent();

	bool ConfirmHitscan(
		AEPCharacter* Shooter,
		AEPWeapon* EquippedWeapon,
		const FVector& Origin,
		const TArray<FVector>& Directions,
		float ClientFireTime,
		TArray<FHitResult>& OutConfirmedHits);

	FEPHitboxSnapshot GetSnapshotAtTime(float TargetTime) const;


protected:
	virtual void BeginPlay() override;

	static const TArray<FName> HitBones;
	int32 MaxHistoryCount = 0;

	// 시간 오름차순으로 유지 - [0] 오래됨, [Last] 최신
	// 링버퍼 대신 단순 배열을 통해 GetSnapshotAtTime의 탐색 순서 보장
	UPROPERTY()
	TArray<FEPHitboxSnapshot> HitboxHistory;

	void SaveHitboxSnapshot(float Time, const FVector& Location);
	void OnServerMoveProcessed(float Time, FVector Location);

	// CMC OnMovementUpdated(TickDispatch)에서 받은 값을 임시 보관.
	// PostPhysics Tick에서 본 Transform이 갱신된 뒤 실제 스냅샷으로 커밋.
	bool bHasPendingSnapshot = false;
	float PendingSnapshotTime = 0.f;
	FVector PendingSnapshotLocation = FVector::ZeroVector;

	TArray<AEPCharacter*> GetHitscanCandidates(
		AEPCharacter* Shooter,
		AEPWeapon* EquippedWeapon,
		const FVector& Origin,
		const TArray<FVector>& Directions,
		float ClientFireTime) const;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
