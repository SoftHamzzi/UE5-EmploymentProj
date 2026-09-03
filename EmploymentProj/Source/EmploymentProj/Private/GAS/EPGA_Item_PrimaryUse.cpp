// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Item_PrimaryUse.h"

#include "TimerManager.h"
#include "Camera/CameraComponent.h"
#include "Combat/EPCombatComponent.h"
#include "Combat/EPWeapon.h"
#include "Core/EPCharacter.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
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
		const float Interval = GetFireInterval(Weapon);
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

float UEPGA_Item_PrimaryUse::GetFireInterval(const AEPWeapon* Weapon)
{
	return Weapon ? (1.f / Weapon->WeaponDef->FireRate) : 0.2f;
}
