// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Types/EPTypes.h"
#include "EPItemDefinition.generated.h"

class UGameplayAbility;
struct FEPItemData;

UCLASS()
class EMPLOYMENTPROJ_API UEPItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	// === 변수 ===
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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|GAS")
	TSubclassOf<UGameplayAbility> GrantedAbility;
	
	// === 함수 ===
	virtual void InitState(const FEPItemData& Data, FEPItemState& State) const;
	
	// PrimaryDataAsset ID
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
