// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "EPPlayerState.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API AEPPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	AEPPlayerState();
	
	// --- Getter ---
	int32 GetKillCount() const { return KillCount; }
	
	// --- 서버 전용 함수
	void AddKill();
	void SetExtracted(bool bExtracted);
	
protected:
	// --- 복제 변수 ---
	// COND_OwnerOnly: 본인에게만 복제 (타인은 모름)
	
	// 킬 수 (본인만 앎)
	UPROPERTY(ReplicatedUsing = OnRep_KillCount)
	int32 KillCount = 0;
	
	// 탈출 성공 여부 (본인만 앎)
	UPROPERTY(ReplicatedUsing = OnRep_IsExtracted)
	bool bIsExtracted = false;
	
	// --- OnRep 콜백 ---
	UFUNCTION()
	void OnRep_KillCount();
	
	UFUNCTION()
	void OnRep_IsExtracted();
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
