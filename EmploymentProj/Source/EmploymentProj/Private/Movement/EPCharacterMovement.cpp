// Fill out your copyright notice in the Description page of Project Settings.

#include "Movement/EPCharacterMovement.h"

#include "GameFramework/Character.h"

// --- CMC 오버라이드 ---
float UEPCharacterMovement:: GetMaxSpeed() const {
	if (bWantsToSprint && IsMovingOnGround()) return SprintSpeed;
	if (bWantsToAim) return AimSpeed;
	return Super::GetMaxSpeed();
}

void UEPCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToAim = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

// FSavedMove_EPCharacter 클래스

class FSavedMove_EPCharacter : public FSavedMove_Character
{
public:
	uint8 bSavedWantsToSprint : 1;
	uint8 bSavedWantsToAim : 1;
	
	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	virtual void SetMoveFor(
		ACharacter* C, float InDeltaTime, FVector const& NewAccel,
		FNetworkPredictionData_Client_Character& ClientData
	) override;
	virtual void PrepMoveFor(ACharacter* C) override;
};

void FSavedMove_EPCharacter::Clear()
{
	FSavedMove_Character::Clear();
	
	bSavedWantsToSprint = false;
	bSavedWantsToAim = false;
}

uint8 FSavedMove_EPCharacter::GetCompressedFlags() const
{
	uint8 Result = FSavedMove_Character::GetCompressedFlags();
	if (bSavedWantsToSprint)
		Result |= FSavedMove_Character::FLAG_Custom_0; // 왜 FSavedMove_EPCharacter로 안하지?
	if (bSavedWantsToAim)
		Result |= FSavedMove_Character::FLAG_Custom_1;
	
	return Result;
}

// SetMoveFor - 현재 CMC 상태 SavedMove에 저장
void FSavedMove_EPCharacter::SetMoveFor(
	ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData
)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(C->GetCharacterMovement());
	bSavedWantsToSprint = CMC->bWantsToSprint;
	bSavedWantsToAim = CMC->bWantsToAim;
}

// PrepMoveFor - SavedMove에서 CMC로 복원
void FSavedMove_EPCharacter::PrepMoveFor(ACharacter* C)
{
	FSavedMove_Character::PrepMoveFor(C);
	
	UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(C->GetCharacterMovement());
	CMC->bWantsToSprint = CMC->bWantsToSprint;
	CMC->bWantsToAim = CMC->bWantsToAim;
}

// CanCombineWith - Sprint/ADS 상태가 다르면 합치지 않음
bool FSavedMove_EPCharacter::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	FSavedMove_EPCharacter* Other = static_cast<FSavedMove_EPCharacter*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint) return false;
	if (bSavedWantsToAim != Other->bSavedWantsToAim) return false;
	return FSavedMove_Character::CanCombineWith(NewMove, Character, MaxDelta);
}

class FNetworkPredictionData_Client_EPCharacter : public FNetworkPredictionData_Client_Character
{
public:
	explicit FNetworkPredictionData_Client_EPCharacter(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Client_Character(ClientMovement) {}
	
	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_EPCharacter());
		// return MakeShared<FSavedMove_EPCharacter>(FSavedMove_EPCharacter());
	}
};

// CMC에서
class FNetworkPredictionData_Client* UEPCharacterMovement::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UEPCharacterMovement* MutableThis = const_cast<UEPCharacterMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_EPCharacter(*this);
	}
	
	return ClientPredictionData;
}