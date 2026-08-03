// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EPPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class AEPCharacter;
class UEPHUDWidget;
class UAbilitySystemComponent;
class UEPCrosshairWidget;

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
	FORCEINLINE UInputAction* GetReloadAction() const { return ReloadAction; }
	FORCEINLINE UInputAction* GetDashAction() const { return DashAction; }
	FORCEINLINE UInputAction* GetHealAction() const { return HealAction; }
	FORCEINLINE UInputAction* GetShieldAction() const { return ShieldAction; }
	FORCEINLINE UInputAction* GetInteractAction() const { return InteractAction; }
	
	void InitHUD(UAbilitySystemComponent* InASC);
	
	void SetInteractPrompt(const FText& Text, bool bEnabled);
	
	// --- Client RPC ---
	// 킬 피드백 (서버 -> 킬러 클라)
	UFUNCTION(Client, Reliable)
	void Client_OnKill(const FString& VictimName);
	
	UFUNCTION(Client, Reliable)
	void Client_OnInteractFailed(const FText& Reason);
	
	UFUNCTION(Client, Unreliable)
	void Client_PlayHitConfirmSound();
	
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
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DashAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> HealAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ShieldAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;
	
	// --- 오버라이드 ---
	virtual void BeginPlay() override;
	
	// Input Mapping Context 등록
	virtual void OnPossess(APawn* InPawn) override;
	
private:
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UEPCrosshairWidget> CrosshairWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UEPCrosshairWidget> CrosshairWidget;
	
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TObjectPtr<USoundBase> HitConfirmSound;
	
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TObjectPtr<USoundBase> KillConfirmSound;
	
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UEPHUDWidget> HUDWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UEPHUDWidget> HUDWidget;
};
