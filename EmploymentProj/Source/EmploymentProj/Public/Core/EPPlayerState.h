// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "EPPlayerState.generated.h"

class UAbilitySystemComponent;
class UEPAttributeSet;
struct FOnAttributeChangeData;

UCLASS()
class EMPLOYMENTPROJ_API AEPPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	// === 변수 ===
	// === 함수 ===
	AEPPlayerState();
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UEPAttributeSet* GetAttributeSet() const { return AttributeSet; }
	
	int32 GetKillCount() const { return KillCount; }
	
	// --- 서버 전용 함수
	void AddKill();
	void SetExtracted(bool bExtracted);
	
protected:
	// === 변수 ===
	// 킬 수 (본인만 앎)
	UPROPERTY(ReplicatedUsing = OnRep_KillCount)
	int32 KillCount = 0;
	
	// 탈출 성공 여부 (본인만 앎)
	UPROPERTY(ReplicatedUsing = OnRep_IsExtracted)
	bool bIsExtracted = false;
	
	// === 함수 ===
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	FDelegateHandle HealthChangedDelegateHandle;
	
	void HealthChanged(const FOnAttributeChangeData& Data);
	
	UFUNCTION()
	void OnRep_KillCount();
	
	UFUNCTION()
	void OnRep_IsExtracted();
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
private:
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> ASC;
	
	UPROPERTY()
	TObjectPtr<UEPAttributeSet> AttributeSet;
};
