// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/EPWeaponDefinition.h"

FPrimaryAssetId UEPWeaponDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("WeaponDef"), GetFName());
}