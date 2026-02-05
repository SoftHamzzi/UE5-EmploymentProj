// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/EPPlayerState.h"
#include "Net/UnrealNetwork.h"

AEPPlayerState::AEPPlayerState()
{
	
}

// --- OnRep 콜백 ---
	
// 킬 카운트 증가
void AEPPlayerState::AddKill()
{
	
}
	
// 탈출 처리
void AEPPlayerState::SetExtracted(bool bExtracted)
{
	
}
	
// 사망 처리
void AEPPlayerState::SetDead(bool bDead)
{
	
}
	
void AEPPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AEPPlayerState, KillCount);
	DOREPLIFETIME(AEPPlayerState, bIsExtracted);
	DOREPLIFETIME(AEPPlayerState, bIsDead);
	
}