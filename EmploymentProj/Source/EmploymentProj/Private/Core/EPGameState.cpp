// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/EPGameState.h"
#include "Net/UnrealNetwork.h"

AEPGameState::AEPGameState()
{
	
}

// --- OnRep 콜백 ---
void AEPGameState::OnRep_RemainingTime()
{
	
}

void AEPGameState::OnRep_MatchPhase()
{
	
}
	
// --- 서버 전용 함수 ---
// GameMode에서 호출. 남은 시간 설정
void AEPGameState::SetRemainingTime(float NewTime)
{
	
}
	
// GameMode에서 호출. 매치 단계 변경
void AEPGameState::SetMatchPhase(EEPMatchPhase NewPhase)
{
	
}
	
// Replication 등록
void AEPGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEPGameState, MatchPhase);
	DOREPLIFETIME(AEPGameState, RemainingTime);
}