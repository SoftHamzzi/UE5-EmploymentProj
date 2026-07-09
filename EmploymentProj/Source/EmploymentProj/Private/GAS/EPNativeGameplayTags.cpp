// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPNativeGameplayTags.h"

namespace EmpGameplayTags
{
	// State
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dead,                 "State.Dead")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Reloading,            "State.Reloading")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_UsingItem,            "State.UsingItem")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_FireCooldown,         "State.FireCooldown")

	// Event
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Death,                "Event.Death")

	// Ability
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Item_PrimaryUse,    "Ability.Item.PrimaryUse")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Item_Reload,        "Ability.Item.Reload")

	// Cooldown
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Weapon_PrimaryUse, "Cooldown.Weapon.PrimaryUse")

	// Data
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage,                "Data.Damage")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Cooldown,              "Data.Cooldown")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_ReloadDuration,        "Data.ReloadDuration")

	// Hitzone
	UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Head,               "HitZone.Head")
	UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Chest,              "HitZone.Chest")
	UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Limb,               "HitZone.Limb")
}