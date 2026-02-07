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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FirstPersonCamera;
	
	// Offset for the first-person camera
	UPROPERTY(EditAnywhere, Category = Camera)
	FVector FirstPersonCameraOffset = FVector(2.8f, 5.9f, 0.0f);
	
	UPROPERTY(ReplicatedUsing=OnRep_IsSprinting)
	bool bIsSprinting;
	
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
	
	UFUNCTION()
	void OnRep_IsSprinting();
	
	UFUNCTION(Server, Reliable)
	void Server_SetSprinting(bool bNewSprinting);
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
