// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/EPItemInstance.h"
#include "Data/EPItemDefinition.h"

UEPItemInstance* UEPItemInstance::CreateInstance(UObject* Outer, FName InItemId, UEPItemDefinition* InDefinition)
{
	UEPItemInstance* Instance = NewObject<UEPItemInstance>(Outer);
	Instance->InstanceId = FGuid::NewGuid();
	Instance->ItemId = InItemId;
	Instance->CachedDefinition = InDefinition;
	return Instance;
}
