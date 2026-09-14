// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/EPItemDefinition.h"

FPrimaryAssetId UEPItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ItemDef"), GetFName());
}
