// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Types/EPTypes.h"
#include "EPItemData.generated.h"

class UEPItemDefinition;

USTRUCT(BlueprintType)
struct FEPItemData : public FTableRowBase
{
	GENERATED_BODY()
	
	// 아이템 고유 ID (Row Name과 동일하게 유지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemId;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EEPItemType ItemType = EEPItemType::Misc;
	
	// 표시 정보
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText Description;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	EEPItemRarity Rarity = EEPItemRarity::Common;
	
	// 인벤토리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 MaxStack = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 SlotSize = 1;
	
	// 경제
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 SellPrice = 100;
	
	// 플래그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	bool bIsQuestItem = false;
	
	// 이 Row에 대응하는 Definition 에셋 참조
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UEPItemDefinition> ItemDefinition;
	
};
