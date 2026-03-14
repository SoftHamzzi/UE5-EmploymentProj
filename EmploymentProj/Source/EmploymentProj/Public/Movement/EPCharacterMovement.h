// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EPCharacterMovement.generated.h"

// 서버가 NewMove 처리를 완료할 때 발행 — 구독자가 이동 시각과 위치를 수신
DECLARE_MULTICAST_DELEGATE_TwoParams(FEPOnServerMoveProcessed, float /*Time*/, FVector /*Location*/);

UCLASS()
class EMPLOYMENTPROJ_API UEPCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
public:
	UEPCharacterMovement();
	
	// --- 이동 상태 플래그 (복제X, CompressedFlags로 전송) ---
	uint8 bWantsToSprint : 1;
	uint8 bWantsToAim : 1;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeed = 650.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float AimSpeed = 200.f;
	
	// 서버 NewMove 완료 델리게이트 — SSR 등이 구독
	FEPOnServerMoveProcessed OnServerMoveProcessed;

	// --- CMC 오버라이드 ---
	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
};
