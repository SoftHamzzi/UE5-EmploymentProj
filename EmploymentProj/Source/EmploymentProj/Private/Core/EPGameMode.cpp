// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/EPGameMode.h"

#include "Core/EPCharacter.h"
#include "Core/EPPlayerController.h"
#include "Core/EPGameState.h"
#include "Core/EPPlayerState.h"

// FTimerHandle MatchTimerHandle 변수 있음

AEPGameMode::AEPGameMode()
{
	DefaultPawnClass = AEPCharacter::StaticClass();
	PlayerControllerClass = AEPPlayerController::StaticClass();
	PlayerStateClass = AEPPlayerState::StaticClass();
	GameStateClass = AEPGameState::StaticClass();
}

// --- AGameMode 오버라이드 ---
// 플레이어 로그인 완료 시
void AEPGameMode::PostLogin(APlayerController* NewPlayer) {
	Super::PostLogin(NewPlayer);
}
	
// 플레이어 로그아웃 시
void AEPGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
}
	
// 스폰 위치 결정(랜덤 배정)
AActor* AEPGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	
	return nullptr;
}
	
// MatchState 변경 시 호출
void AEPGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
}
void AEPGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
}
	
// 매치 시작 가능 여부
bool AEPGameMode::ReadyToStartMatch_Implementation()
{
	Super::ReadyToStartMatch();
	return true;
}

// 매치 타이머 틱 (1초마다)
void AEPGameMode::TickMatchTimer()
{
	
}

// 매치 종료 처리 (시간 초과)
void AEPGameMode::EndmatchByTimeout()
{
	
}
	
// 생존 플레이어 확인 -> 전원 탈출/사망 시 매치 종료
void AEPGameMode::CheckMatchEndConditions()
{
	
}