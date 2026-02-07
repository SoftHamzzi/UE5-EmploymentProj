// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "EPGameMode.generated.h"

class APlayerStart;
class AEPGameState;

UCLASS()
class EMPLOYMENTPROJ_API AEPGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AEPGameMode();

	// --- Exec 테스트 ---
	// 서버 콘솔에서 호출: 특정 플레이어의 킬 카운트 증가
	UFUNCTION(Exec)
	void AddKillToPlayer(int32 PlayerIndex);
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float MatchDuration = 300.f; // 이후 30분으로 수정.
	
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	int32 MinPlayersToStart = 1;
	
	FTimerHandle MatchTimerHandle;

	UPROPERTY()
	TObjectPtr<AEPGameState> EPGameState;
	
	// 생존 플레이어 수 (서버 전용, 복제 안 함)
	int32 AlivePlayerCount = 0;
	
	UPROPERTY()
	TArray<AActor*> PlayerStarts;
	
	// --- AGameMode 오버라이드 ---
	virtual void BeginPlay() override;
	
	// 플레이어 로그인 완료 시
	virtual void PostLogin(APlayerController* NewPlayer) override;
	
	// 플레이어 로그아웃 시
	virtual void Logout(AController* Exiting) override;
	
	// 스폰 위치 결정(랜덤 배정)
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	
	// MatchState 변경 시 호출
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual void HandleMatchIsWaitingToStart() override;
	
	// 매치 시작 가능 여부
	virtual bool ReadyToStartMatch_Implementation() override;
	
	TSet<TWeakObjectPtr<AActor>> UsedPlayerStarts;
	
	
	// --- 매치 로직 ---
	// 매치 타이머 틱 (1초마다)
	void TickMatchTimer();
	
	// 매치 종료 처리 (시간 초과)
	void EndmatchByTimeout();
	
	// 생존 플레이어 확인 -> 전원 탈출/사망 시 매치 종료
	void CheckMatchEndConditions();
	
};
