// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/EPItemDefinition.h"
#include "Abilities/GameplayAbility.h"
#include "Data/EPItemData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
EDataValidationResult UEPItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	
	if (ItemId.IsNone())
	{
		Context.AddError(NSLOCTEXT("EP", "ItemIdEmpty", "ItemId가 비어 있습니다."));
		Result = EDataValidationResult::Invalid;
	}
	
	if (ItemDataRow.RowName != ItemId)
	{
		Context.AddError(FText::Format(
			NSLOCTEXT("EP", "RowNameMismatch", "ItemDataRow.RowName({0}) != ItemId({1})"),
			FText::FromName(ItemDataRow.RowName), FText::FromName(ItemId)));
		Result = EDataValidationResult::Invalid;
	}
	
	if (const FEPItemData* Row = ItemDataRow.GetRow<FEPItemData>(TEXT("IsDataValid")))
	{
		if (Row->ItemDefinition.ToSoftObjectPath() != FSoftObjectPath(this))
		{
			Context.AddError(NSLOCTEXT("EP", "BackRefMismatch",
				"DataTable Row의 ItemDefinition이 이 에셋을 가리키지 않습니다."));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

FPrimaryAssetId UEPItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("ItemDef"), GetFName());
}



void UEPItemDefinition::InitState(const FEPItemData& Data, FEPItemState& State) const
{
	State.Charges = Data.InitialCharges;
}
