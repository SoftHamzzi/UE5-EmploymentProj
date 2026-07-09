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
	
	if (GE_ReloadingClass)
	{
		FGameplayEffectSpecHandle SpecHandle =
			MakeOutgoingGameplayEffectSpec(GE_ReloadingClass, GetAbilityLevel());
		SpecHandle.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_ReloadDuration, ReloadTime);
		ReloadingEffectHandle =
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	}
	
	UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReloadTime);
	WaitTask->OnFinish.AddDynamic(this, &UEPGA_Item_Reload::OnReloadComplete_Task);
	WaitTask->ReadyForActivation();
}

void UEPGA_Item_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ReloadingEffectHandle.IsValid())
	{
		ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(ReloadingEffectHandle);
		ReloadingEffectHandle.Invalidate();
	}
	
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
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
