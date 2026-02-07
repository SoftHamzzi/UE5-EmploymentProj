// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/EPPlayerState.h"
#include "Net/UnrealNetwork.h"

AEPPlayerState::AEPPlayerState()
{
	
}

// --- 서버 전용 함수
void AEPPlayerState::AddKill()
{
	KillCount++;
}

void AEPPlayerState::SetExtracted(bool bExtracted)
{
	bIsExtracted = bExtracted;
}

// --- OnRep 콜백 ---
void AEPPlayerState::OnRep_KillCount()
{
	
}

void AEPPlayerState::OnRep_IsExtracted()
{
	
}

void AEPPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// 본인에게만 복제
	DOREPLIFETIME_CONDITION(AEPPlayerState, KillCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AEPPlayerState, bIsExtracted, COND_OwnerOnly);
	
}