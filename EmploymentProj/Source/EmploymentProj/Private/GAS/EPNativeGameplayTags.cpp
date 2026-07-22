// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/EPNativeGameplayTags.h"

namespace EmpGameplayTags
{
	// State
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dead,                 "State.Dead")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Reloading,            "State.Reloading")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_UsingItem,            "State.UsingItem")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_FireCooldown,         "State.FireCooldown")
	
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Shielded,        "State.Shielded")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Healing,         "State.Healing")
	UE_DEFINE_GAMEPLAY_TAG(TAG_State_Casting,         "State.Casting")

	// Event
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Death,                "Event.Death")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Damaged,         "Event.Damaged")
	
	// Ability
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Item_PrimaryUse,    "Ability.Item.PrimaryUse")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Item_Reload,        "Ability.Item.Reload")
	
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Skill_Dash,    "Ability.Skill.Dash")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Skill_Heal,    "Ability.Skill.Heal")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Skill_Shield,  "Ability.Skill.Shield")

	// Cooldown
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Weapon_PrimaryUse, "Cooldown.Weapon.PrimaryUse")
	
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Skill_Dash,   "Cooldown.Skill.Dash")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Skill_Heal,   "Cooldown.Skill.Heal")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Skill_Shield, "Cooldown.Skill.Shield")

	// Data
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage,                "Data.Damage")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Cooldown,              "Data.Cooldown")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Duration,               "Data.Duration")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_ReloadDuration,        "Data.ReloadDuration")

	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_HealAmount,       "Data.HealAmount")
	UE_DEFINE_GAMEPLAY_TAG(TAG_Data_MoveSpeedMultiplier, "Data.MoveSpeedMultiplier")
	
	// Hitzone
	UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Head,               "HitZone.Head")
	UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Chest,              "HitZone.Chest")
	UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Limbs,               "HitZone.Limbs")
}