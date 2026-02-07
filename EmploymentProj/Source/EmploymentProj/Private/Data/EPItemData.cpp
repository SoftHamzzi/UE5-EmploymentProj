// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/EPItemData.h"

FPrimaryAssetId UEPItemData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Item"), GetFName());
}