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
	// --- 복제 변수 ---
	
	// 킬 수
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	int32 KillCount;
	
	// 탈출 성공 여부
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	bool bIsExtracted;
	
	// 사망 여부
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
	bool bIsDead;
	
	// --- OnRep 콜백 ---
	
	// 킬 카운트 증가
	void AddKill();
	
	// 탈출 처리
	void SetExtracted(bool bExtracted);
	
	// 사망 처리
	void SetDead(bool bDead);
	
protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
