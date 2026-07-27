// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/EPWeaponDefinition.h"
#include "Combat/EPProjectile.h"
#include "Curves/CurveFloat.h"
#include "Abilities/GameplayAbility.h"

void UEPWeaponDefinition::InitState(const FEPItemData& Data, FEPItemState& State) const
{
	State.Charges = MaxAmmo;
}
