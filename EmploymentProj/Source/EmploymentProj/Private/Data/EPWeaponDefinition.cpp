// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/EPWeaponDefinition.h"
#include "Combat/EPProjectile.h"
#include "Curves/CurveFloat.h"

FPrimaryAssetId UEPWeaponDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WeaponDef"), GetFName());
}