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
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UEPGA_Item_PrimaryUse::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                            const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
	AEPWeapon* Weapon = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
	if (!Char || !Weapon)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	FireOnce();
	
	if (Weapon->WeaponDef->FireMode == EEPFireMode::Auto)
	{
		const float Interval = 1 / Weapon->GetBaseFireRate();
		GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &UEPGA_Item_PrimaryUse::FireOnce, Interval, true);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
	}
}

void UEPGA_Item_PrimaryUse::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimerHandle);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UEPGA_Item_PrimaryUse::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
}

void UEPGA_Item_PrimaryUse::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
}

void UEPGA_Item_PrimaryUse::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE) return;
	
	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());
	
	const AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
	const AEPWeapon* Weapon = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
	const float Duration = GetFireInterval(Weapon);
	
	SpecHandle.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, Duration);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

bool UEPGA_Item_PrimaryUse::ServerConfirmOneShot(const FVector& Origin, const FVector& Direction)
{
	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
		return false;
	
	if (AEPCharacter* Char = Cast<AEPCharacter>(CurrentActorInfo->AvatarActor.Get()))
		if (UEPCombatComponent* Combat = Char->GetCombatComponent())
			Combat->HandleServerFire(Origin, Direction);
	
	return true;
}

void UEPGA_Item_PrimaryUse::FireOnce()
{
	AEPCharacter* Char = Cast<AEPCharacter>(CurrentActorInfo->AvatarActor.Get());
	AEPWeapon* Weapon = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
	if (!Char || !Weapon || !Weapon->CanFire())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
		return;
	}
	
	CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	
	if (!CurrentActorInfo->IsLocallyControlled()) return;
	
	const FVector Origin = Char->GetCameraComponent()->GetComponentLocation();
	const FVector Direction = Char->GetControlRotation().Vector();
	UEPCombatComponent* Combat = Char->GetCombatComponent();
	
	if (!Combat) return;
	
	if (CurrentActorInfo->IsNetAuthority())
	{
		if (!ServerConfirmOneShot(Origin, Direction))
		{
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, true);
		}
	}
	else
	{
		Combat->PlayLocalMuzzleEffect(Origin);
		if (Weapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
			Combat->SpawnLocalCosmeticProjectile(Origin, Char->GetControlRotation().Vector());
		Combat->Server_ConfirmFire(Origin, Char->GetControlRotation().Vector(), CurrentSpecHandle);
	}
}

void UEPGA_Item_PrimaryUse::ArmNextShot()
{
}

void UEPGA_Item_PrimaryUse::OnFireTimerTick()
{
}

void UEPGA_Item_PrimaryUse::SendFireTargetData(const FVector& Direction, float ClientMoveTimeStamp)
{
}

void UEPGA_Item_PrimaryUse::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
}

bool UEPGA_Item_PrimaryUse::ServerConfirmOneShot(const FVector& Direction, float ClientMoveTimeStamp)
{
	AEPCharacter* Char = GetCharacter();
	UEPCombatComponent* Combat = Char->GetCombatComponent();
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