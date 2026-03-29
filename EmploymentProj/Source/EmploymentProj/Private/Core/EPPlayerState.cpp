// Fill out your copyright notice in the Description page of Project Settings.

// Core
#include "Core/EPPlayerState.h"
#include "Net/UnrealNetwork.h"

// GAS
#include "AbilitySystemComponent.h"
#include "GAS/EPAttributeSet.h"
#include "Math/UnitConversion.h"

AEPPlayerState::AEPPlayerState()
{
	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	ASC->SetIsReplicated(true);
	
	ASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	SetNetUpdateFrequency(100.f);
	
	AttributeSet = CreateDefaultSubobject<UEPAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AEPPlayerState::GetAbilitySystemComponent() const
{
	return ASC;
}

// --- 서버 전용 함수
void AEPPlayerState::AddKill()
{
	if (!HasAuthority()) return;
	KillCount++;
}

void AEPPlayerState::SetExtracted(bool bExtracted)
{
	if (!HasAuthority()) return;
	bIsExtracted = bExtracted;
}

void AEPPlayerState::BeginPlay()
{
	Super::BeginPlay();
	
	if (ASC && AttributeSet)
	{
		HealthChangedDelegateHandle =
			ASC->GetGameplayAttributeValueChangeDelegate(
				AttributeSet->GetHealthAttribute())
			.AddUObject(this, &AEPPlayerState::HealthChanged);
		
		// Tag 변경 콜백 등록
		// ASC->RegisterGameplayTagEvent(TAG_State_Dead, ...)
	}
}

void AEPPlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	ASC->GetGameplayAttributeValueChangeDelegate(
		AttributeSet->GetHealthAttribute())
	.Remove(HealthChangedDelegateHandle);
}

void AEPPlayerState::HealthChanged(const FOnAttributeChangeData& Data)
{
	// UI 갱신 및 사망 처리(클라+서버)
	// 서버는 나중에 GA를 통해 처리
}

// --- OnRep 콜백 ---
void AEPPlayerState::OnRep_KillCount()
{
	UE_LOG(LogTemp, Log, TEXT("KillCount: %d"), KillCount);
}

void AEPPlayerState::OnRep_IsExtracted()
{
	UE_LOG(LogTemp, Log, TEXT("IsExtracted: %d"), bIsExtracted);
}

void AEPPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	// 본인에게만 복제
	DOREPLIFETIME_CONDITION(AEPPlayerState, KillCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AEPPlayerState, bIsExtracted, COND_OwnerOnly);
	
}