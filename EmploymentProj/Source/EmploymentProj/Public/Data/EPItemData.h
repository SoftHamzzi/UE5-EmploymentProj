// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/EPTypes.h"
#include "EPItemData.generated.h"

UCLASS(BlueprintType)
class EMPLOYMENTPROJ_API UEPItemData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// 아이템 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemName;
	
	// 아이템 설명
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText Description;
	
	// 등급
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EEPItemRarity Rarity = EEPItemRarity::Common;
	
	// 판매 가격
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 SellPrice = 100;
	
	// 퀘스트 아이템 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bIsQuestItem = false;
	
	// 인벤토리 슬롯 차지 수 (기본 1)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 SlotSize = 1;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	
};
