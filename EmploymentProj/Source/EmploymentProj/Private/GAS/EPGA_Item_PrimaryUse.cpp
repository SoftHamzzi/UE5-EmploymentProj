// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Item_PrimaryUse.h"

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
	
	bServerRespectsRemoteAbilityCancellation = false;

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
	
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	
	const FVector Origin = Char->GetCameraComponent()->GetComponentLocation();
	const AGameStateBase* GS = Char->GetWorld()->GetGameState<AGameStateBase>();
	const float ClientTime = GS ? GS->GetServerWorldTimeSeconds()
		: Char->GetWorld()->GetTimeSeconds();
	
	if (ActorInfo->IsNetAuthority())
	{
		UEPCombatComponent* Combat = Char->GetCombatComponent();
		Combat->HandleServerFire(Origin, Char->GetControlRotation().Vector(), ClientTime);
	}
	
	if (!ActorInfo->IsNetAuthority())
	{
		UEPCombatComponent* Combat = Char->GetCombatComponent();
		if (Combat)
		{
			Combat->PlayLocalMuzzleEffect(Origin);
			
			if (Weapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
				Combat->SpawnLocalCosmeticProjectile(Origin, Char->GetControlRotation().Vector());
		}
	}
	
	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
	
}

bool UEPGA_Item_PrimaryUse::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                               const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
                                               const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
		return false;
	
	const AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
	const AEPWeapon* Weapon = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
	
	return Char && Weapon && Weapon->CanFire();
}

const FGameplayTagContainer* UEPGA_Item_PrimaryUse::GetCooldownTags() const
{
	FGameplayTagContainer* MutableTags = const_cast<FGameplayTagContainer*>(&TempCooldownTags);
	MutableTags->Reset();
	if (const FGameplayTagContainer* ParentTags = Super::GetCooldownTags())
		MutableTags->AppendTags(*ParentTags);
	MutableTags->AppendTags(CooldownTags);
	return MutableTags;
}

void UEPGA_Item_PrimaryUse::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE) return;
	
	FGameplayEffectSpecHandle SpecHandle =
		MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());
	
	SpecHandle.Data->DynamicGrantedTags.AppendTags(CooldownTags);
	
	const AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
	const AEPWeapon* Weapon = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
	const float Duration = Weapon ? (1.f/ Weapon->WeaponDef->FireRate) : 0.2f;
	
	SpecHandle.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, Duration);
	ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
