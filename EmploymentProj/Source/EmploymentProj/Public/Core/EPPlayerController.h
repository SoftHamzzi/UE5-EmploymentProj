// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EPPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class EMPLOYMENTPROJ_API AEPPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AEPPlayerController();
	
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
	
	// --- 오버라이드 ---
	virtual void BeginPlay() override;
	
	// Input Mapping Context 등록
	virtual void OnPossess(APawn* InPawn) override;
};
