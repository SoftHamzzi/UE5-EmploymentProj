// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPGA_Item_Reload.h"

#include "AbilitySystemComponent.h"
#include "Combat/EPCombatComponent.h"
#include "Combat/EPWeapon.h"
#include "Core/EPCharacter.h"
#include "GAS/EPNativeGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

UEPGA_Item_Reload::UEPGA_Item_Reload()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	bServerRespectsRemoteAbilityCancellation = false;
	
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Item_Reload);
	SetAssetTags(Tags);
	
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_UsingItem);
	
	ActivationOwnedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);
}

void UEPGA_Item_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                        const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                        const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
	AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
	AEPWeapon* Weapon = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
	
	if (!Char || !Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}
	
	const float ReloadTime = Weapon->WeaponDef->ReloadTime;
	
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReloadTime);
	WaitTask->OnFinish.AddDynamic(this, &UEPGA_Item_Reload::OnReloadComplete_Task);
	WaitTask->ReadyForActivation();
}

void UEPGA_Item_Reload::OnReloadComplete_Task()
{
	const FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const FGameplayAbilityActivationInfo ActivInfo = GetCurrentActivationInfo();
	
	if (ActorInfo->IsNetAuthority() && GE_ReloadAmmoClass)
	{
		ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivInfo, GE_ReloadAmmoClass.GetDefaultObject(), 1.f);
	}
	
	EndAbility(Handle, ActorInfo, ActivInfo, true, false);
}
