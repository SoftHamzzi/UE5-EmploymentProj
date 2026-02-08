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
	// 기본 CMC 대신 커스텀 CMC 사용
	AEPCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	// --- 컴포넌트 ---
	
	// 1인칭 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FirstPersonCamera;
	
	// Offset for the first-person camera
	UPROPERTY(EditAnywhere, Category = "Camera")
	FVector FirstPersonCameraOffset = FVector(2.8f, 5.9f, 0.0f);
	
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
	
	// ADS
	void Input_StartADS(const FInputActionValue& Value);
	void Input_StopADS(const FInputActionValue& Value);
	
	// 앉기
	void Input_Crouch(const FInputActionValue& Value);
	void Input_UnCrouch(const FInputActionValue& Value);
	
	// --- Getter (CMC에서 읽기) ---
	bool GetIsSprinting() const;
	bool GetIsAiming() const;
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
