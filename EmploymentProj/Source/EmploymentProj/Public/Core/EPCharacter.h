// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "GameFramework/Character.h"
#include "EPCharacter.generated.h"

// --- 카메라 ---
class UCameraComponent;
// --- 전투 ---
class UEPCombatComponent;
// --- 입력 ---
class UInputAction;
struct FInputActionValue;

// --- 메타 휴먼 ---
class UGroomComponent;

UCLASS()
class EMPLOYMENTPROJ_API AEPCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// 기본 CMC 대신 커스텀 CMC 사용
	AEPCharacter(const FObjectInitializer& ObjectInitializer);
	
	// --- Getter/Setter ---
	bool GetIsSprinting() const;
	bool GetIsAiming() const;
	UCameraComponent* GetCameraComponent() const;
	UEPCombatComponent* GetCombatComponent() const;
	FORCEINLINE USkeletalMeshComponent* GetFaceMesh() const { return FaceMesh; }
	FORCEINLINE USkeletalMeshComponent* GetOutfitMesh() const { return OutfitMesh; }

protected:
	// === 변수 ===
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FirstPersonCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UEPCombatComponent* CombatComponent;
	
	// // --- 메타 휴먼 ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MetaHuman")
	TObjectPtr<USkeletalMeshComponent> FaceMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MetaHuman")
	TObjectPtr<USkeletalMeshComponent> OutfitMesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FVector FirstPersonCameraOffset = FVector(2.8f, 5.9f, 0.0f);
	UPROPERTY(ReplicatedUsing = OnRep_HP, BlueprintReadOnly, Category = "Stat")
	int32 HP = 100;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	int32 MaxHP = 100;
	
	// === 함수 ===
	// --- 오버라이드 ---
	virtual void BeginPlay() override;
	
	// Enhanced Input 바인딩
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	// 피격
	virtual float TakeDamage(
		float DamageAmount, struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator, AActor* DamageCause) override;
	
	// --- 선언 ---
	void Die(AController* Killer);
	
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
	
	// 발사
	void Input_Fire(const FInputActionValue& Value);
	
	// OnRep
	UFUNCTION()
	void OnRep_HP();
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_Die();
	
	// 동기화
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
