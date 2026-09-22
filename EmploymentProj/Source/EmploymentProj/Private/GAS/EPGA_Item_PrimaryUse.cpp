// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Item_PrimaryUse.h"

#include "TimerManager.h"
#include "Camera/CameraComponent.h"
#include "Combat/EPCombatComponent.h"
#include "Combat/EPWeapon.h"
#include "Core/EPCharacter.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/EPTargetData_Fire.h"
#include "GAS/EPNativeGameplayTags.h"

UEPGA_Item_PrimaryUse::UEPGA_Item_PrimaryUse()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	bServerRespectsRemoteAbilityCancellation = true;

	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Item_PrimaryUse);
	SetAssetTags(Tags);
	
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);
}

bool UEPGA_Item_PrimaryUse::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
		return false;
	
	if (!ActorInfo->IsLocallyControlled())
		return true;
	
	const AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
	const AEPWeapon* Weapon = (Char && Char->GetCombatComponent()) ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
	const UWorld* World  = Weapon ? Weapon->GetWorld() : nullptr;
	if (!Weapon || !World) return false;
	
	const float Rate = Char->GetLocalModifiers().Product(EmpGameplayTags::TAG_Modifier_FireRate);
	const float Tolerance = 0.01f / Weapon->GetBaseFireRate();
	return Weapon->GetFireTimer().IsElapsed(World->GetTimeSeconds(), Rate, Tolerance);
}

void UEPGA_Item_PrimaryUse::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	bPendingShot = false;
	
	AEPCharacter* Char = GetCharacter();
	AEPWeapon* Weapon = GetWeapon();
	if (!Char || !Weapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	if (!ActorInfo->IsLocallyControlled())
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
			.AddUObject(this, &UEPGA_Item_PrimaryUse::OnTargetDataReady);
		return;
	}
	
	FireOnce();
	ArmNextShot();
}

void UEPGA_Item_PrimaryUse::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	/*
	1. `IsEndAbilityValid(Handle, ActorInfo)` 아니면 return
2. 타이머 정리(지금 코드), `bPendingShot = false`
3. `ActorInfo`가 있고 로컬 컨트롤이 아니면: 델리게이트 `.Remove(TargetDataDelegateHandle)` + `ConsumeClientReplicatedTargetData` (Lyra `EndAbility`와 동일)
4. `Super`
	 */
	if (!IsEndAbilityValid(Handle, ActorInfo)) return;
	
	if (!GetWorld()) return;
	GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
	bPendingShot = false;
	
	if (ActorInfo || ActorInfo->IsLocallyControlled())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FPredictionKey Key = ActivationInfo.GetActivationPredictionKey();
			ASC->AbilityTargetDataSetDelegate(Handle, Key).Remove(TargetDataDelegateHandle);
			ASC->ConsumeClientReplicatedTargetData(Handle, Key);
		}
	}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEPGA_Item_PrimaryUse::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!IsAutoFire(GetWeapon()))
		bPendingShot = true;
}

void UEPGA_Item_PrimaryUse::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (IsAutoFire(GetWeapon()))
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void UEPGA_Item_PrimaryUse::FireOnce()
{
	AEPCharacter* Char = Cast<AEPCharacter>(CurrentActorInfo->AvatarActor.Get());
	UEPCombatComponent* Combat = Char ? Char->GetCombatComponent() : nullptr;
	AEPWeapon* Weapon = GetWeapon();
	if (!Char || !Combat || !Weapon || !Weapon->CanFire())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	
	Weapon->GetFireTimer().Start(GetWorld()->GetTimeSeconds(), 1.f / Weapon->GetBaseFireRate());
	
	const FVector Direction = Char->GetControlRotation().Vector();
	const float ClientMoveTimeStamp = GetClientMoveTimeStamp();
	
	if (CurrentActorInfo->IsNetAuthority())
	{
		if (!ServerConfirmOneShot(Direction, ClientMoveTimeStamp))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
			return;
		}
	}
	
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	bool bCanGenerateNewKey = !ASC->ScopedPredictionKey.IsValidForMorePrediction();
    FScopedPredictionWindow ScopedPrediction(ASC, bCanGenerateNewKey);
	
	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	
	const FVector CamLoc = Char->GetCameraComponent()->GetComponentLocation();
	Combat->PlayLocalMuzzleEffect(CamLoc);
	if (Weapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
		Combat->SpawnLocalCosmeticProjectile(CamLoc, Direction);
	
	SendFireTargetData(Direction, ClientMoveTimeStamp);
}

void UEPGA_Item_PrimaryUse::ArmNextShot()
{
	if (!IsActive()) return;
	
	AEPWeapon* Weapon = GetWeapon();
	if (!Weapon) return;
	
	const float Remaining = Weapon->GetFireTimer().GetRemaining(GetWorld()->GetTimeSeconds(), GetFireRateMultiplier());
	GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &UEPGA_Item_PrimaryUse::OnFireTimerTick,
		FMath::Max(Remaining, KINDA_SMALL_NUMBER), false);
	
}

void UEPGA_Item_PrimaryUse::OnFireTimerTick()
{
	AEPWeapon* Weapon = GetWeapon();
	if (!Weapon) return;
	
	if (IsAutoFire(Weapon) || bPendingShot)
	{
		bPendingShot = false;
		FireOnce();
		ArmNextShot();
		return;
	}
	
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEPGA_Item_PrimaryUse::SendFireTargetData(const FVector& Direction, float ClientMoveTimeStamp)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;
	
	FEPTargetData_Fire* Data = new FEPTargetData_Fire();
	Data->Direction = Direction;
	Data->ClientMoveTimeStamp = ClientMoveTimeStamp;
	const FGameplayAbilityTargetDataHandle Handle(Data);
	
	const FPredictionKey AbilityKey = CurrentActivationInfo.GetActivationPredictionKey();
	ASC->CallServerSetReplicatedTargetData(
		CurrentSpecHandle,
		AbilityKey,
		Handle,
		FGameplayTag(),
		ASC->ScopedPredictionKey
	);
	
}

void UEPGA_Item_PrimaryUse::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	const FPredictionKey Key = CurrentActivationInfo.GetActivationPredictionKey();
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, Key);
	
	const FEPTargetData_Fire* Fire = Data.IsValid(0) ? static_cast<const FEPTargetData_Fire*>(Data.Get(0)) : nullptr;
	if (!Fire) return;
	
	if (!ServerConfirmOneShot(Fire->Direction, Fire->ClientMoveTimeStamp))
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

bool UEPGA_Item_PrimaryUse::ServerConfirmOneShot(const FVector& Direction, float ClientMoveTimeStamp)
{
	AEPCharacter* Char = GetCharacter();
	UEPCombatComponent* Combat = Char ? Char->GetCombatComponent() : nullptr;
	AEPWeapon* Weapon = GetWeapon();
	if (!Char || !Combat || !Weapon) return false;
	
	const float RefillPerSecond = Weapon->GetBaseFireRate() * GetFireRateMultiplier();
	if (!Weapon->GetFireLimiter().TryTake(GetWorld()->GetTimeSeconds(), RefillPerSecond))
		return true;
	
	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
		return false;
	
	Combat->HandleServerFire(Direction, ClientMoveTimeStamp);
	return true;
}

AEPCharacter* UEPGA_Item_PrimaryUse::GetCharacter() const
{
	return CurrentActorInfo ? Cast<AEPCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

AEPWeapon* UEPGA_Item_PrimaryUse::GetWeapon() const
{
	AEPCharacter* Char = GetCharacter();
	if (!Char) return nullptr;
	
	UEPCombatComponent* Combat = Char->GetCombatComponent();
	if (!Combat) return nullptr;
	
	AEPWeapon* Weapon = Combat->GetEquippedWeapon();
	if (!Weapon) return nullptr;
	
	return Weapon;
}

float UEPGA_Item_PrimaryUse::GetFireRateMultiplier() const
{
	AEPCharacter* Char = GetCharacter();
	if (!Char) return 1.0f;
	
	return Char->GetLocalModifiers().Product(EmpGameplayTags::TAG_Modifier_FireRate);
}

float UEPGA_Item_PrimaryUse::GetClientMoveTimeStamp() const
{
	if (!CurrentActorInfo || CurrentActorInfo->IsNetAuthority()) return -1.f;
	AEPCharacter* Char = GetCharacter();
	return Char->GetCharacterMovement()->GetPredictionData_Client_Character()->CurrentTimeStamp;
}

bool UEPGA_Item_PrimaryUse::IsAutoFire(const AEPWeapon* Weapon)
{
	return Weapon && Weapon->WeaponDef && Weapon->WeaponDef->FireMode == EEPFireMode::Auto;
}