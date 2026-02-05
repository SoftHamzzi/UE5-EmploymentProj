// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EPCharacter.generated.h"

class UCameraComponent;
class UInputAction;
struct FInputActionValue;

UCLASS()
class EMPLOYMENTPROJ_API AEPCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEPCharacter();

protected:
	// --- 컴포넌트 ---
	
	// 1인칭 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float WalkSpeed = 350.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeed = 650.f;
	
	// --- 오버라이드 ---
	virtual void BeginPlay() override;
	
	// Enhanced Input 바인딩
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// --- 입력 핸들러 ---
	// 이동 (WASD)
	void Input_Move(const FInputActionValue& Value);
	
	// 시점 (마우스)
	void Input_Look(const FInputActionValue& Value);
	
	// 점프
	void Input_Jump(const FInputActionValue& Value);
	void Input_StopJumping(const FInputActionValue& Value);
	
	// 질주
	void Input_StartSprint(const FInputActionValue& Value);
	void Input_StopSprint(const FInputActionValue& Value);
};
