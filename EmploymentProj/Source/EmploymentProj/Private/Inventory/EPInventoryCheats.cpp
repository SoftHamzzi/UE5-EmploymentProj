// Fill out your copyright notice in the Description page of Project Settings.


#include "Inventory/EPInventoryCheats.h"

#include "EngineUtils.h"
#include "Core/EPCharacter.h"
#include "Data/EPItemDefinitionSubsystem.h"
#include "Inventory/EPInventoryComponent.h"

UEPInventoryCheats::UEPInventoryCheats()
{
#if UE_WITH_CHEAT_MANAGER
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda(
			[](UCheatManager* CM)
			{
				CM->AddCheatManagerExtension(NewObject<ThisClass>(CM));
			}
		));
	}
#endif
}

void UEPInventoryCheats::EPInvDump()
{
	const UEPInventoryComponent* Inv = GetInv();
	if (!Inv) return;
	
	TArray<FEPInventoryEntry> Rows = Inv->GetEntries();
	Rows.StableSort([](const FEPInventoryEntry& A, const FEPInventoryEntry& B)
	{
		if(A.ParentEntryId != B.ParentEntryId) return A.ParentEntryId < B.ParentEntryId;
		const bool bASlot = !A.SlotId.IsNone(), bBSlot = !B.SlotId.IsNone();
		if (bASlot != bBSlot) return bASlot;
		return A.SortKey < B.SortKey;
	});
	
	UE_LOG(LogTemp, Log, TEXT("EntryId Parent SlotId ItemId Charges SortKey"));
	for (const FEPInventoryEntry& E : Rows)
	{
		UE_LOG(LogTemp, Log, TEXT("%d %d %s %s %d %d"), E.EntryId, E.ParentEntryId, *E.SlotId.ToString(), *E.ItemId.ToString(), E.State.Charges, E.SortKey);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Body : %d / %d"),
		Inv->GetUsedSlots(INDEX_NONE), Inv->GetCapacity(INDEX_NONE));
	for (const FEPInventoryEntry& E : Rows)
	{
		const int32 Cap = Inv->GetCapacity(E.EntryId);
		if (Cap <= 0) continue;
		UE_LOG(LogTemp, Log, TEXT("%s(%d) : %d / %d"),
			*E.ItemId.ToString(), E.EntryId, Inv->GetUsedSlots(E.EntryId), Cap);
	}
}

void UEPInventoryCheats::EPInvDumpAll()
{
	UWorld* World = GetWorld();
	if (!World) return;
	
	for (TActorIterator<AEPCharacter> It(World); It; ++It)
	{
		const UEPInventoryComponent* Inv = It->GetInventoryComponent();
		UE_LOG(LogTemp, Log, TEXT("%s Entries=%d"),
			*It->GetName(), Inv ? Inv->GetEntries().Num() : -1);
	}
}

UEPInventoryComponent* UEPInventoryCheats::GetInv() const
{
	const APlayerController* PC = GetPlayerController();
	const AEPCharacter* C =  PC ? Cast<AEPCharacter>(PC->GetPawn()) : nullptr;
	return C ? C->GetInventoryComponent() : nullptr;
}

void UEPInventoryCheats::EPInvAdd(const FString& ItemId, int32 Container)
{
	UEPInventoryComponent* Inv = GetInv();
	if (!Inv) return;
	
	const UEPItemDefinitionSubsystem* Defs = UEPItemDefinitionSubsystem::Get(Inv);
	FEPItemState State;
	if (!Defs || !Defs->MakeItemState(FName(*ItemId), State)) return;
	
	const int32 NewId = Inv->AddItem(Container, FName(*ItemId), State);
	UE_LOG(LogTemp, Log, TEXT("[Inv] Add(%s -> %d) = %d"),
		*ItemId, Container, NewId);
}

void UEPInventoryCheats::EPInvDrop(int32 EntryId)
{
	UEPInventoryComponent* Inv = GetInv();
	if (!Inv) return;
	
	Inv->Server_DropItem(EntryId);
	UE_LOG(LogTemp, Log, TEXT("[Inv] Drop(%d) 요청. 결과는 EPInvDump / EP.Loot.List"), EntryId);
}

void UEPInventoryCheats::EPInvReorder(int32 EntryId, int32 PrevEntryId)
{
	UEPInventoryComponent* Inv = GetInv();
	if (!Inv) return;

	Inv->ReorderEntry(EntryId, PrevEntryId);
	UE_LOG(LogTemp, Log, TEXT("[Inv] Reorder(%d, prev=%d). SortKey는 EPInvDump"),
		EntryId, PrevEntryId);
}

void UEPInventoryCheats::EPInvMove(int32 EntryId, int32 NewParent, const FString& SlotId)
{
	UEPInventoryComponent* Inv = GetInv();
	
	if (!Inv) return;
	const FName Slot = (SlotId == TEXT("-")) ? NAME_None : FName(*SlotId);
	const bool bOk = Inv->MoveEntry(EntryId, NewParent, Slot);
	UE_LOG(LogTemp, Log, TEXT("[Inv] Move(%d -> %d/%s) = %s"),
		EntryId, NewParent, *SlotId, bOk ? TEXT("OK") : TEXT("거절"));
}
