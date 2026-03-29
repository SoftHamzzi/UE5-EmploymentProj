// Fill out your copyright notice in the Description page of Project Settings.


#include "Core/EPGameInstance.h"
#include "AbilitySystemGlobals.h"

void UEPGameInstance::Init()
{
	Super::Init();
	UAbilitySystemGlobals::Get().InitGlobalData();
}
