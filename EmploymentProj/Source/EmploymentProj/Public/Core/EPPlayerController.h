// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EPPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class AEPCharacter;

UCLASS()
class EMPLOYMENTPROJ_API AEPPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AEPPlayerController();
	
	FORCEINLINE UInputAction* GetMoveAction() const { return MoveAction; }
	FORCEINLINE UInputAction* GetLookAction() const { return LookAction; }
	FORCEINLINE UInputAction* GetJumpAction() const { return JumpAction; }
	FORCEINLINE UInputAction* GetSprintAction() const { return SprintAction; }
	FORCEINLINE UInputAction* GetADSAction() const { return ADSAction; }
	FORCEINLINE UInputAction* GetCrouchAction() const { return CrouchAction; }
	FORCEINLINE UInputAction* GetFireAction() const { return FireAction; }
	
protected:
	// --- Enhanced Input ---
	
	// 기본 Input Mapping Context
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ADSAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;
	
	// --- 오버라이드 ---
	virtual void BeginPlay() override;
	
	// Input Mapping Context 등록
	virtual void OnPossess(APawn* InPawn) override;
	
public:
	// --- Client RPC ---
	// 킬 피드백 (서버 -> 킬러 클라)
	UFUNCTION(Client, Reliable)
	void Client_OnKill(AEPCharacter* Victim);
};
