// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/EPWeaponInstance.h"

UEPWeaponInstance* UEPWeaponInstance::CreateWeaponInstance(
		UObject* Outer, FName InItemId,
		int32 InMaxAmmo, UEPItemDefinition* InDefinition)
{
	UEPWeaponInstance* Instance = NewObject<UEPWeaponInstance>(Outer);
	Instance->InstanceId = FGuid::NewGuid();
	Instance->ItemId = InItemId;
	Instance->CurrentAmmo = InMaxAmmo;
	Instance->CachedDefinition = InDefinition;
	return Instance;
}