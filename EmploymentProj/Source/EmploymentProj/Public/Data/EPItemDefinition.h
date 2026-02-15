// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Types/EPTypes.h"
#include "EPItemDefinition.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API UEPItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// Row와 매칭되는 ID
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FName ItemId;
	
	// DataTable Row 핸들 (역참조용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FDataTableRowHandle ItemDataRow;
	
	// 월드에 떨어졌을 때 메시
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visual")
	TSoftObjectPtr<UStaticMesh> WorldMesh;
	
	// UI 아이콘
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Visual")
	TSoftObjectPtr<UTexture2D> Icon;
	
	// PrimaryDataAsset ID
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
