#include "Inventory/EPInventoryComponent.h"

#include "AudioMixerBlueprintLibrary.h"
#include "Combat/EPCombatComponent.h"
#include "Core/EPCharacter.h"
#include "Data/EPItemData.h"
#include "Data/EPItemDefinitionSubsystem.h"
#include "Data/EPLootDeveloperSettings.h"
#include "Net/UnrealNetwork.h"

struct FScopedInventoryNotify
{
	explicit FScopedInventoryNotify(UEPInventoryComponent* In) : C(In)
	{
		++C->NotifyDepth;
	}
	
	~FScopedInventoryNotify()
	{
		if (--C->NotifyDepth == 0) C->OnInventoryChanged.Broadcast();
	}
	
	UEPInventoryComponent* C;
};

bool FEPInventoryList::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
{
	return FFastArraySerializer::FastArrayDeltaSerialize<FEPInventoryEntry, FEPInventoryList>(
		Items, DeltaParms, *this
	);
}

void FEPInventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters&)
{
	if (Owner) Owner->OnInventoryChanged.Broadcast();
}

UEPInventoryComponent::UEPInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	Entries.Owner = this;
}

int32 UEPInventoryComponent::AddItem(int32 Container, FName ItemId, const FEPItemState& InState)
{
	if (!GetOwner()->HasAuthority()) return INDEX_NONE;
	
	const FEPItemData* Data = Defs() ? Defs()->FindData(ItemId) : nullptr;
	
	if (Data->bFungible)
	{
		const int32 Id = FindFungibleEntryId(Container, ItemId);
		if (Id != INDEX_NONE)
		{
			AddEntryCharges(Id, InState.Charges);
			return Id;
		}
	}
	
	if (!CanFit(Container, ItemId)) return INDEX_NONE;
	
	return InsertEntry(Container, ItemId, InState, NAME_None);
}

int32 UEPInventoryComponent::AddSubtree(int32 Parent, FName SlotId, const TArray<FEPInventoryEntry>& In)
{
	return 0;
}

bool UEPInventoryComponent::FindEntry(int32 EntryId, FEPInventoryEntry& Out) const
{
	for (const FEPInventoryEntry& E : Entries.Items)
	{
		if (E.EntryId != EntryId) continue;
		Out = E;
		return true;
	}
	return false;
}

int32 UEPInventoryComponent::FindFungibleEntryId(int32 Container, FName ItemId) const
{
	for (const FEPInventoryEntry& E : Entries.Items)
		if (E.ParentEntryId == Container && E.ItemId == ItemId)
			return E.EntryId;
	return INDEX_NONE;
}

int32 UEPInventoryComponent::GetUsedSlots(int32 Container) const
{
	const UEPItemDefinitionSubsystem* D = Defs();
	if (!D) return 0;
	
	int32 Sum = 0;
	for (const FEPInventoryEntry& E : Entries.Items)
	{
		if (E.ParentEntryId != Container) continue;
		if (!E.SlotId.IsNone()) continue;
		
		const FEPItemData* Row = D->FindData(E.ItemId);
		if (!ensureMsgf(Row, TEXT("[Inventory] DT에 없는 ItemId: %s"), *E.ItemId.ToString()))
			continue;
		Sum += Row->SlotSize;
	}
	
	return Sum;
}

int32 UEPInventoryComponent::GetCapacity(int32 Container) const
{
	if (Container == INDEX_NONE) return MaxSlots;
	
	FEPInventoryEntry E;
	if (!FindEntry(Container, E)) return 0;
	
	const UEPItemDefinitionSubsystem* D = Defs();
	const FEPItemData* Row = D ? D->FindData(E.ItemId) : nullptr;
	return Row ? Row->ContainerCapacity : 0;
}

bool UEPInventoryComponent::CanFit(int32 Container, FName ItemId) const
{
	const FEPItemData* Data = Defs() ? Defs()->FindData(ItemId) : nullptr;
	if (!Data) return false;
	return GetUsedSlots(Container) + Data->SlotSize <= GetCapacity(Container);
}

bool UEPInventoryComponent::IsFungible(FName ItemId) const
{
	return true;
}

bool UEPInventoryComponent::CanPlaceInSlot(int32 Parent, FName SlotId, FName ItemId) const
{
	if (SlotId.IsNone()) return true;
	
	const FEPItemData* Data = Defs() ? Defs()->FindData(ItemId) : nullptr;
	if (!Data || !Data->SlotPriority.Contains(SlotId)) return false;
	
	
	if (!GetDefault<UEPLootDeveloperSettings>()->BodySlots.Contains(SlotId)) return false;
	if (Parent != INDEX_NONE) return false;
	
	return GetEntryInSlot(Parent, SlotId) == INDEX_NONE;
}

int32 UEPInventoryComponent::GetEntryInSlot(int32 Parent, FName SlotId) const
{
	if (SlotId.IsNone()) return INDEX_NONE;
	
	for (const FEPInventoryEntry& E : Entries.Items)
	{
		if (E.ParentEntryId == Parent && E.SlotId == SlotId)
			return E.EntryId;
	}
	return INDEX_NONE;
}

int32 UEPInventoryComponent::GetEquippedEntryId() const
{
	if (ActiveHotbarIndex < 0 || 3 < ActiveHotbarIndex) return INDEX_NONE;
	
	const FName SlotId(*FString::Printf(TEXT("Hotbar%d"), ActiveHotbarIndex + 1));
	return GetEntryInSlot(INDEX_NONE, SlotId);
}

TArray<int32> UEPInventoryComponent::GetSortedContents(int32 Container) const
{
	TArray<const FEPInventoryEntry*> Out;
	for (const FEPInventoryEntry& E : Entries.Items)
		if (E.ParentEntryId == Container && E.SlotId.IsNone())
			Out.Add(&E);
	
	Out.Sort([](const FEPInventoryEntry& A, const FEPInventoryEntry& B)
	{
		return A.SortKey != B.SortKey ? A.SortKey < B.SortKey
			: A.EntryId < B.EntryId;
	});
	
	TArray<int32> Ids;
	Ids.Reserve(Out.Num());
	for (const FEPInventoryEntry* E : Out) Ids.Add(E->EntryId);
	return Ids;
}

void UEPInventoryComponent::SetEntryCharges(int32 EntryId, int32 NewCharges)
{
	if (!GetOwner()->HasAuthority()) return;
	
	for (FEPInventoryEntry& E : Entries.Items)
	{
		if (E.EntryId == EntryId)
		{
			FScopedInventoryNotify Guard(this);
			E.State.Charges = FMath::Max(0, NewCharges);
			Entries.MarkItemDirty(E);
			return;
		}
	}
}

void UEPInventoryComponent::AddEntryCharges(int32 EntryId, int32 Delta)
{
	FEPInventoryEntry E;
	if (FindEntry(EntryId, E))
		SetEntryCharges(EntryId, E.State.Charges + Delta);
}

bool UEPInventoryComponent::MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId)
{
	if (!GetOwner()->HasAuthority()) return false;

	if (!ContainsEntry(EntryId)) return false;

	FEPInventoryEntry Cur;
	if (!FindEntry(EntryId, Cur)) return false;

	if (Cur.ParentEntryId == NewParent && Cur.SlotId == NewSlotId) return false;

	if (!CanPlaceInSlot(NewParent, NewSlotId, Cur.ItemId)) return false;

	if (NewSlotId.IsNone() && !CanFit(NewParent, Cur.ItemId)) return false;

	for (int32 P = NewParent; P != INDEX_NONE; )
	{
		if (P == EntryId) return false;
		FEPInventoryEntry Up;
		P = FindEntry(P, Up) ? Up.ParentEntryId : INDEX_NONE;
	}

	const bool  bReparent = (NewParent != Cur.ParentEntryId);
	const int32 NewKey    = bReparent ? KeySpace_NextAtEnd(NewParent) : 0;

	FScopedInventoryNotify Guard(this);

	for (FEPInventoryEntry& E : Entries.Items)
	{
		if (E.EntryId != EntryId) continue;

		E.ParentEntryId = NewParent;
		E.SlotId        = NewSlotId;
		Entries.MarkItemDirty(E);

		if (bReparent) AssignSortKey(EntryId, NewKey);
		return true;
	}
	return false;
}

bool UEPInventoryComponent::RemoveEntry(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved)
{
	return RemoveEntryInternal(EntryId, OutRemoved, true);
}

int32 UEPInventoryComponent::TryAutoEquip(const TArray<FEPInventoryEntry>& In)
{
	return 0;
}

TArray<int32> UEPInventoryComponent::GetInsertionOrder() const
{
	return {};
}

bool UEPInventoryComponent::CanMutateInventory() const
{
	return true;
}

void UEPInventoryComponent::Server_DropItem_Implementation(int32 EntryId)
{
	return;
}

void UEPInventoryComponent::ReorderEntry(int32 EntryId, int32 PrevEntryId)
{
	ReorderEntryInternal(EntryId, PrevEntryId, false);
}

void UEPInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UEPInventoryComponent, Entries, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UEPInventoryComponent, MaxSlots, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UEPInventoryComponent, ActiveHotbarIndex, COND_OwnerOnly);
}

bool UEPInventoryComponent::RemoveEntryInternal(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved, bool bIsRoot)
{
	// 서버 확인
	if (!GetOwner()->HasAuthority()) return false;
	if (!ContainsEntry(EntryId)) return false;
	
	FScopedInventoryNotify Guard(this);
	
	// 1. write-back
	if (EntryId == GetEquippedEntryId())
	{
		if (AEPCharacter* Ch = GetOwner<AEPCharacter>())
			if (UEPCombatComponent* C = Ch->GetCombatComponent())
				C->UnequipWeapon();
	}
	
	// 2. 스냅샷
	if (OutRemoved)
	{
		FEPInventoryEntry Snapshot;
		if (FindEntry(EntryId, Snapshot))
		{
			if (bIsRoot) { Snapshot.ParentEntryId = INDEX_NONE; Snapshot.SortKey = 0; Snapshot.SlotId = NAME_None; }
			OutRemoved->Add(Snapshot);
		}
	}
	
	// 3. 자신 제거
	RemoveSelf(EntryId);
	
	// 4. 자식 재귀 제거
	RemoveChildrenRecursive(EntryId, OutRemoved);
	return true;
}

void UEPInventoryComponent::RemoveChildrenRecursive(int32 ParentId, TArray<FEPInventoryEntry>* OutRemoved)
{
	TArray<int32> Children;
	for (const FEPInventoryEntry& E : Entries.Items)
		if (E.ParentEntryId == ParentId)
			Children.Add(E.EntryId);
	
	for (int32 Id : Children)
		RemoveEntryInternal(Id, OutRemoved, false);
}

void UEPInventoryComponent::RemoveSelf(int32 EntryId)
{
	FScopedInventoryNotify Guard(this);
	
	for (int32 i=0; i < Entries.Items.Num(); ++i)
	{
		if (Entries.Items[i].EntryId != EntryId) continue;
		
		Entries.Items.RemoveAtSwap(i);
		Entries.MarkArrayDirty();
		return;
	}
}

bool UEPInventoryComponent::ContainsEntry(int32 EntryId) const
{
	for (const FEPInventoryEntry& E : Entries.Items)
	{
		if (E.EntryId == EntryId) return true;
	}
	return false;
}

int32 UEPInventoryComponent::InsertEntry(int32 Parent, FName ItemId, const FEPItemState& State, FName SlotId)
{
	FScopedInventoryNotify Guard(this);
	
	const int32 NewId = NextEntryId++;
	
	const int32 NewKey = KeySpace_NextAtEnd(Parent);
	
	FEPInventoryEntry& E = Entries.Items.AddDefaulted_GetRef();
	E.EntryId = NewId;
	E.ParentEntryId = Parent;
	E.SlotId = SlotId;
	E.ItemId = ItemId;
	E.State = State;
	E.SortKey = NewKey;
	
	Entries.MarkItemDirty(E);
	return NewId;
}

void UEPInventoryComponent::AssignSortKey(int32 EntryId, int32 NewKey)
{
	FScopedInventoryNotify Guard(this);
	
	for (FEPInventoryEntry& E : Entries.Items)
	{
		if (E.EntryId != EntryId) continue;
		if (E.SortKey == NewKey) return;
		
		E.SortKey = NewKey;
		Entries.MarkItemDirty(E);
		return;
	}
}

void UEPInventoryComponent::RenormalizeSortKeys(int32 Container)
{
	TArray<const FEPInventoryEntry*> All;
	
	for (const FEPInventoryEntry& E : Entries.Items)
		if (E.ParentEntryId == Container) All.Add(&E);
	
	All.Sort([](const FEPInventoryEntry& A, const FEPInventoryEntry& B)
	{
		return A.SortKey != B.SortKey ? A.SortKey < B.SortKey
			: A.EntryId < B.EntryId;
	});
	
	int32 K = 0;
	for (const FEPInventoryEntry* E : All)
	{
		AssignSortKey(E->EntryId, K);
		K += SortKeyStep;
	}
}

int32 UEPInventoryComponent::KeySpace_NextAtEnd(int32 Container)
{
	int32 Max = 0; bool bAny = false;
	for (const FEPInventoryEntry& E : Entries.Items)
		if (E.ParentEntryId == Container)
		{
			Max = bAny ? FMath::Max(Max, E.SortKey) : E.SortKey;
			bAny = true;
		}
	
	if (!bAny) return 0;
	
	if (Max > MAX_int32 - SortKeyGuard)
	{
		RenormalizeSortKeys(Container);
		return KeySpace_NextAtEnd(Container);
	}
	
	return Max + SortKeyStep;
}

int32 UEPInventoryComponent::KeySpace_Min(int32 Container) const
{
	int32 Min = 0; bool bAny = false;
	for (const FEPInventoryEntry& E : Entries.Items)
	{
		if (E.ParentEntryId != Container) continue;
		
		Min = bAny ? FMath::Min(Min, E.SortKey) : E.SortKey;
		bAny = true;
	}
	
	return bAny ? Min : 0;
}

bool UEPInventoryComponent::KeySpace_NextAbove(int32 Container, int32 Key, int32 Exclude, int32& OutKey) const
{
	bool bAny = false;
	for (const FEPInventoryEntry& E : Entries.Items)
	{
		if (E.ParentEntryId != Container) continue;
		if (E.EntryId == Exclude) continue;
		if (E.SortKey <= Key) continue;
		
		OutKey = bAny ? FMath::Min(OutKey, E.SortKey) : E.SortKey;
		bAny = true;
	}
	
	return bAny;
}

int32 UEPInventoryComponent::KeyOf(int32 EntryId, int32& OutKey) const
{
	for (const FEPInventoryEntry& E : Entries.Items)
	{
		if (E.EntryId != EntryId) continue;
		OutKey = E.EntryId;
		return true;
	}
	return false;
}

void UEPInventoryComponent::ReorderEntryInternal(int32 EntryId, int32 PrevEntryId, bool bRetry)
{
	if (!GetOwner()->HasAuthority()) return;
	
	FEPInventoryEntry E;
	if (!FindEntry(EntryId, E)) return;
	if (!E.SlotId.IsNone()) return;
	
	const int32 Container = E.ParentEntryId;
	
	if (PrevEntryId != INDEX_NONE)
	{
		FEPInventoryEntry P;
		if (!FindEntry(PrevEntryId, P)) return;
		if (P.ParentEntryId != Container || !P.SlotId.IsNone()) return;
		if (PrevEntryId == EntryId) return;
	}
	
	int32 PrevKey = KeySpace_Min(Container) - SortKeyStep;
	if (PrevEntryId != INDEX_NONE && !KeyOf(PrevEntryId, PrevKey)) return;
	
	int32 NextKey = 0;
	const bool bTail = !KeySpace_NextAbove(Container, PrevKey, EntryId, NextKey);
	
	int32 NewKey;
	if (PrevEntryId == INDEX_NONE) NewKey = PrevKey;
	else if (bTail) NewKey = PrevKey + SortKeyStep;
	else NewKey = PrevKey + (NextKey - PrevKey) / 2;
	
	const bool bOutOfRange = (NewKey <= MIN_int32 + SortKeyGuard)
		|| (NewKey >= MAX_int32 - SortKeyGuard);
	
	const bool bNoGap = (PrevEntryId != INDEX_NONE) && !bTail && (NewKey <= PrevKey);
	
	if (bOutOfRange || bNoGap)
	{
		if (!ensureMsgf(!bRetry, TEXT("[Inventory] ReorderEntry: 재정규화 후에도 자리가 없다.")))
			return;
		
		RenormalizeSortKeys(Container);
		ReorderEntryInternal(EntryId, PrevEntryId, true);
		return;
	}
	
	{
		const TArray<int32> Cur = GetSortedContents(Container);
		const int32 MyIdx = Cur.Find(EntryId);
		const int32 CurPrev = (MyIdx <= 0) ? INDEX_NONE : Cur[MyIdx - 1];
		if (CurPrev == PrevEntryId) return;
	}
	
	AssignSortKey(EntryId, NewKey);
}

AEPPickup* UEPInventoryComponent::SpawnPickupInFront() const
{
	return nullptr;
}

const UEPItemDefinitionSubsystem* UEPInventoryComponent::Defs() const
{
	return UEPItemDefinitionSubsystem::Get(this);
}
