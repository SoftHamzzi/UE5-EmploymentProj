// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Types/EPTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "EPInventoryTypes.generated.h"

USTRUCT()
struct FEPInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()
	
	UPROPERTY() int32 EntryId = INDEX_NONE;
	UPROPERTY() int32 ParentEntryId = INDEX_NONE;
	UPROPERTY() FName SlotId;
	UPROPERTY() FName ItemId;
	UPROPERTY() FEPItemState State;
	
	UPROPERTY() int32 SortKey = 0;
};