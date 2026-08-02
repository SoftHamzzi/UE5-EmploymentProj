// Fill out your copyright notice in the Description page of Project Settings.


#include "Loot/EPLootTable.h"

#include "Misc/DataValidation.h"

static bool RollInternal(const UEPLootTable* Table, FName& OutItemId, int32 Depth)
{
	OutItemId = NAME_None;
    	
    static constexpr int32 MaxDepth = 8;
    if (!Table || Depth > MaxDepth)
    {
    	UE_LOG(LogTemp, Error, TEXT("[Loot] 롤 깊이 초과 - 순환 참조 의심: %s"),
    		*GetNameSafe(Table));
    	return false;
    }
	
	float TotalWeight = (Depth == 0) ? Table->EmptyWeight : 0.f;
	if (Depth > 0 && Table->EmptyWeight > 0.f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Loot] %s: 하위 테이블의 EmptyWeight(%.2f)는 무시된다."),
			*GetNameSafe(Table), Table->EmptyWeight);
	}
	
	for (const FEPLootEntry& E : Table->Entries) TotalWeight += E.Weight;
	if (TotalWeight <= 0.f) return false;
	
	float Pick = FMath::RandRange(0.f, TotalWeight);
	
	if (Depth == 0 && (Pick -= Table->EmptyWeight) < 0.f)
		return true;
	
	const FEPLootEntry* Chosen = nullptr;
	
	for (const FEPLootEntry& E : Table->Entries)
	{
		if (E.Weight <= 0.f) continue;
		Chosen = &E;
		if ((Pick -= E.Weight) < 0.f) break;
	}
	
	if (!Chosen) return false;
	if (Chosen->SubTable) return RollInternal(Chosen->SubTable, OutItemId, Depth + 1);
	if (Chosen->ItemId.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("[Loot] %s: ItemId도 SubTable도 없는 엔트리"),
			*GetNameSafe(Table));
		return false;
	}
	OutItemId = Chosen->ItemId;
	return true;
}

#if WITH_EDITOR
EDataValidationResult UEPLootTable::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	
	if (Entries.IsEmpty() && EmptyWeight <= 0.f)
	{
		Context.AddError(NSLOCTEXT("EP", "LootTableEmpty",
			"Entries가 비었고, EmptyWeight도 0이다."));
		Result = EDataValidationResult::Invalid;
	}
	
	for (int32 i=0; i<Entries.Num(); i++)
	{
		const FEPLootEntry& E = Entries[i];
		const bool bHasId = !E.ItemId.IsNone();
		const bool bHasSub = (E.SubTable != nullptr);
		
		if (bHasId == bHasSub)
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("EP", "LootEntryAmbigious",
						"Entries[{0}]: ItemId와 SubTable 중 정확히 하나만 채워야 합니다."),
					FText::AsNumber(i)));
			Result = EDataValidationResult::Invalid;
		}
		
		if(E.SubTable == this)
		{
			Context.AddError(FText::Format(
				NSLOCTEXT("EP", "LootEntrySelfRef",
					"Entries[{0}]: SubTable이 자기 자신입니다."),
				FText::AsNumber(i)));
			Result = EDataValidationResult::Invalid;
		}
	}
	return Result;
}
#endif

bool RollLootTable(const UEPLootTable* Table, FName& OutItemId)
{
	return RollInternal(Table, OutItemId, 0);
}
