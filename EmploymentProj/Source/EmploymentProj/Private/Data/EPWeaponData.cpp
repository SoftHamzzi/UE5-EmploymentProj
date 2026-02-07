// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/EPWeaponData.h"

FPrimaryAssetId UEPWeaponData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Weapon"), GetFName());
}