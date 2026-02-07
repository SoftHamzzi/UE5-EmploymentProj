// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Types/EPTypes.h"
#include "EPGameState.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API AEPGameState : public AGameState
{
	GENERATED_BODY()
	
public:
	AEPGameState();
	
	// --- OnRep 콜백 ---
	UFUNCTION()
	void OnRep_RemainingTime();
	
	UFUNCTION()
	void OnRep_MatchPhase();
	
	// --- 서버 전용 함수 ---
	// GameMode에서 호출. 남은 시간 설정
	FORCEINLINE float GetRemainingTime() const { return RemainingTime; }
	void SetRemainingTime(float NewTime);
	
	// GameMode에서 호출. 매치 단계 변경
	FORCEINLINE EEPMatchPhase GetMatchPhase() const { return MatchPhase; }
	void SetMatchPhase(EEPMatchPhase NewPhase);
	
protected:
	// Replication 등록
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 매치 남은 시간 (초). 서버에서 매 초 갱신
	UPROPERTY(ReplicatedUsing = OnRep_RemainingTime, BlueprintReadOnly, Category="Match")
	float RemainingTime;
	
	// 현재 매치 단계 (UI 표시 용)
	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, BlueprintReadOnly, Category="Match")
	EEPMatchPhase MatchPhase;
};
