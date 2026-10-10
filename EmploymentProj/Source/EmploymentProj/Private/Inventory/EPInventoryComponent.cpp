#include "Inventory/EPInventoryComponent.h"

#include "Combat/EPCombatComponent.h"
#include "Core/EPCharacter.h"
#include "Data/EPItemData.h"
#include "Data/EPItemDefinitionSubsystem.h"
#include "Data/EPLootDeveloperSettings.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/Misc/NetConditionGroupManager.h"
#include "Net/Subsystems/NetworkSubsystem.h"

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
	MarkIndexDirty();
	if (Owner) Owner->OnInventoryChanged.Broadcast();
}

const FEPInventoryIndex& FEPInventoryList::EnsureIndex() const
{
	if (!bIndexDirty) return Index;

	Index.ChildrenOf.Reset();
	Index.IdByUid.Reset();
	Index.IndexById.Reset();
	Index.UsedSlotsOf.Reset();

	const UEPItemDefinitionSubsystem* IDS = UEPItemDefinitionSubsystem::Get(Owner);

	for (int32 i = 0; i < Items.Num(); i++)
	{
		const FEPInventoryEntry& E = Items[i];

		Index.IndexById.FindOrAdd(E.EntryId, i);
		Index.ChildrenOf.FindOrAdd(E.ParentEntryId).Add(E.EntryId);
		if (E.State.ItemUid.IsValid())
			Index.IdByUid.FindOrAdd(E.State.ItemUid, E.EntryId);

		if (!E.SlotId.IsNone()) continue;

		const FEPItemData* Row = IDS ? IDS->FindData(E.ItemId) : nullptr;
		if (!ensureMsgf(Row, TEXT("[Inventory] DT에 없는 ItemId: %s"), *E.ItemId.ToString()))
			continue;

		Index.UsedSlotsOf.FindOrAdd(E.ParentEntryId) += Row->SlotSize;
	}

	bIndexDirty = false;
	return Index;
}

const FEPInventoryEntry* FEPInventoryList::FindEntryPtr(int32 EntryId) const
{
	const FEPInventoryIndex& CurIndex = EnsureIndex();

	const int32* Idx = CurIndex.IndexById.Find(EntryId);
	return Idx ? &Items[*Idx] : nullptr;
}

FEPInventoryEntry* FEPInventoryList::FindEntryMutable(int32 EntryId)
{
	const FEPInventoryIndex& CurIndex = EnsureIndex();
	const int32* Idx = CurIndex.IndexById.Find(EntryId);
	return Idx ? &Items[*Idx] : nullptr;
}

int32 FEPInventoryList::Append(int32 Parent, FName ItemId, const FEPItemState& State, FName SlotId, int64 SortKey)
{
	const int32 NewId = NextEntryId++;

	FEPInventoryEntry& E = Items.AddDefaulted_GetRef();
	E.EntryId = NewId;
	E.ParentEntryId = Parent;
	E.SlotId = SlotId;
	E.ItemId = ItemId;
	E.State = State;
	E.SortKey = SortKey;

	MarkItemDirty(E);
	MarkIndexDirty();
	return NewId;
}

void FEPInventoryList::RemoveAll(const TSet<int32>& Ids)
{
	const FEPInventoryIndex& CurIndex = EnsureIndex();

	TArray<int32> Positions;
	for (const int32 Id : Ids)
	{
		if (const int32* Idx = CurIndex.IndexById.Find(Id)) Positions.Add(*Idx);
	}
	if (Positions.IsEmpty()) return;

	Positions.Sort(TGreater<int32>());
	for (const int32 Pos : Positions) Items.RemoveAtSwap(Pos);

	MarkArrayDirty();
	MarkIndexDirty();
}

UEPInventoryComponent::UEPInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
	Entries.Owner = this;
}

void UEPInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority()) return;

	UNetworkSubsystem* NetworkSubsystem = GetWorld()->GetSubsystem<UNetworkSubsystem>();
	if (!NetworkSubsystem) return;
	UE::Net::FNetConditionGroupManager& NetConditionGroupManager = NetworkSubsystem->GetNetConditionGroupManager();
	NetConditionGroupManager.RegisterSubObjectInGroup(this, UE::Net::NetGroupOwner);
}

void UEPInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (!GetOwner()->HasAuthority()) return;
	
	UNetworkSubsystem* NetworkSubsystem = GetWorld()->GetSubsystem<UNetworkSubsystem>();
	if (!NetworkSubsystem) return;
	UE::Net::FNetConditionGroupManager& NetConditionGroupManager = NetworkSubsystem->GetNetConditionGroupManager();
	NetConditionGroupManager.UnregisterSubObjectFromAllGroups(this);
}

ELifetimeCondition UEPInventoryComponent::GetReplicationCondition() const
{
	return COND_NetGroup;
}

int32 UEPInventoryComponent::AddItem(int32 Container, FName ItemId, const FEPItemState& InState)
{
	if (!GetOwner()->HasAuthority()) return INDEX_NONE;
	
	const UEPItemDefinitionSubsystem* D = Defs();
	if (!D) return INDEX_NONE;
	
	if (!CheckIdentity(InState)) return INDEX_NONE;
	
	const FEPItemData* Data = D->FindData(ItemId);
	if (!ensureMsgf(Data, TEXT("[Inventory] DT에 없는 ItemId: %s"), *ItemId.ToString()))
		return INDEX_NONE;
	
	if (Data->bFungible)
	{
		if (!InState.Stats.IsEmpty()) return INDEX_NONE;
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
	const FEPInventoryEntry* E = Entries.FindEntryPtr(EntryId);
	if (!E) return false;
	Out = *E;
	return true;
}

int32 UEPInventoryComponent::FindFungibleEntryId(int32 Container, FName ItemId) const
{
	const FIndex& CurIndex = Entries.EnsureIndex();
	const TArray<int32>* ChildIds = CurIndex.ChildrenOf.Find(Container);
	if (!ChildIds) return INDEX_NONE;
	
	for (const int32 ChildId : *ChildIds)
	{
		const FEPInventoryEntry* E = Entries.FindEntryPtr(ChildId);
		if (E && E->ItemId == ItemId)
			return E->EntryId;
	}
	return INDEX_NONE;
}

int32 UEPInventoryComponent::GetUsedSlots(int32 Container) const
{
	const FIndex& CurIndex = Entries.EnsureIndex();
	return CurIndex.UsedSlotsOf.FindRef(Container);
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
	
	const FIndex& CurIndex = Entries.EnsureIndex();
	const TArray<int32>* ChildIds = CurIndex.ChildrenOf.Find(Parent);
	if (!ChildIds) return INDEX_NONE;
	
	for (int32 ChildId : *ChildIds)
	{
		const FEPInventoryEntry* E = Entries.FindEntryPtr(ChildId);
		if (E && E->SlotId == SlotId) return E->EntryId;
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
	const FIndex& CurIndex = Entries.EnsureIndex();
	const TArray<int32>* ChildIds = CurIndex.ChildrenOf.Find(Container);
	if (!ChildIds) return {};
	
	TArray<const FEPInventoryEntry*> Out;
	for (int32 ChildId : *ChildIds)
	{
		const FEPInventoryEntry* E = Entries.FindEntryPtr(ChildId);
		if (E && E->SlotId.IsNone())
			Out.Add(E);
	}
	
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
	
	FEPInventoryEntry* E = Entries.FindEntryMutable(EntryId);
	if (!E) return;
	
	const int32 Clamped = FMath::Max(0, NewCharges);
	if (E->State.Charges == Clamped) return;
	
	FScopedInventoryNotify Guard(this);
	E->State.Charges = Clamped;
	Entries.MarkItemDirty(*E);
}

void UEPInventoryComponent::AddEntryCharges(int32 EntryId, int32 Delta)
{
	FEPInventoryEntry E;
	if (FindEntry(EntryId, E))
		SetEntryCharges(EntryId, E.State.Charges + Delta);
}

bool UEPInventoryComponent::GetEntryStat(int32 EntryId, FGameplayTag Tag, int32& OutValue) const
{
	if (!Tag.IsValid()) return false;
	const FEPInventoryEntry* E = Entries.FindEntryPtr(EntryId);
	if (!E) return false;
	
	return E->State.FindStat(Tag, OutValue);
}

void UEPInventoryComponent::SetEntryStat(int32 EntryId, FGameplayTag Tag, int32 Value)
{
	if (!GetOwner()->HasAuthority() || !Tag.IsValid()) return;
	FEPInventoryEntry* E = Entries.FindEntryMutable(EntryId);
	if (!E) return;
	
	int32 Cur;
	if (E->State.FindStat(Tag, Cur) && Cur == Value) return;

	FScopedInventoryNotify Guard(this);
	E->State.SetStat(Tag, Value);
	Entries.MarkItemDirty(*E);
}

bool UEPInventoryComponent::MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId)
{
	if (!GetOwner()->HasAuthority()) return false;

	FEPInventoryEntry Cur;
	if (!FindEntry(EntryId, Cur)) return false;

	if (Cur.ParentEntryId == NewParent && Cur.SlotId == NewSlotId) return false;

	if (!CanPlaceInSlot(NewParent, NewSlotId, Cur.ItemId)) return false;

	if (NewSlotId.IsNone() && !CanFit(NewParent, Cur.ItemId)) return false;

	for (int32 P = NewParent; P != INDEX_NONE; )
	{
		if (P == EntryId) return false;
		const FEPInventoryEntry* Up = Entries.FindEntryPtr(P);
		P = Up ? Up->ParentEntryId : INDEX_NONE;
	}

	const bool  bReparent = (NewParent != Cur.ParentEntryId);
	const int64 NewKey    = bReparent ? KeySpace_NextAtEnd(NewParent) : 0;

	FScopedInventoryNotify Guard(this);

	FEPInventoryEntry* E = Entries.FindEntryMutable(EntryId);
	if (!E) return false;
	
	E->ParentEntryId = NewParent;
	E->SlotId = NewSlotId;
	Entries.MarkItemDirty(*E);
	Entries.MarkIndexDirty();

	if (bReparent) AssignSortKey(EntryId, NewKey);
	return true;
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
	DOREPLIFETIME_CONDITION(UEPInventoryComponent, Entries, COND_None);
	DOREPLIFETIME_CONDITION(UEPInventoryComponent, MaxSlots, COND_None);
	DOREPLIFETIME_CONDITION(UEPInventoryComponent, ActiveHotbarIndex, COND_None);
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
	TArray<int32> Children = Entries.EnsureIndex().ChildrenOf.FindRef(ParentId);
	
	for (int32 Id : Children)
		RemoveEntryInternal(Id, OutRemoved, false);
}

void UEPInventoryComponent::RemoveSelf(int32 EntryId)
{
	FScopedInventoryNotify Guard(this);
	
	Entries.RemoveAll({EntryId});
}

bool UEPInventoryComponent::ContainsEntry(int32 EntryId) const
{
	const FIndex& CurIndex = Entries.EnsureIndex();
	return CurIndex.IndexById.Contains(EntryId);
}

int32 UEPInventoryComponent::InsertEntry(int32 Parent, FName ItemId, const FEPItemState& State, FName SlotId)
{
	if (!CheckIdentity(State)) return INDEX_NONE;
	const int64 NewKey = KeySpace_NextAtEnd(Parent);
	
	return AppendEntry(Parent, ItemId, State, SlotId, NewKey);
}

int32 UEPInventoryComponent::AppendEntry(int32 Parent, FName ItemId, const FEPItemState& State, FName SlotId, int64 SortKey)
{
	FScopedInventoryNotify Guard(this);
	
	return Entries.Append(Parent, ItemId, State, SlotId, SortKey);
}

void UEPInventoryComponent::AssignSortKey(int32 EntryId, int64 NewKey)
{
	FEPInventoryEntry* E = Entries.FindEntryMutable(EntryId);
	if (!E) return;
	if (E->SortKey == NewKey) return;
	
	FScopedInventoryNotify Guard(this);
	E->SortKey = NewKey;
	Entries.MarkItemDirty(*E);
}

void UEPInventoryComponent::RenormalizeSortKeys(int32 Container)
{
	TArray<const FEPInventoryEntry*> All;
	
	const FIndex& CurIndex = Entries.EnsureIndex();
	const TArray<int32>* ChildIds = CurIndex.ChildrenOf.Find(Container);
	if (!ChildIds) return;
	
	for (int32 ChildId : *ChildIds)
	{
		const FEPInventoryEntry* E = Entries.FindEntryPtr(ChildId);
		if (!E) continue;
		
		All.Add(E);
	}
	
	All.Sort([](const FEPInventoryEntry& A, const FEPInventoryEntry& B)
	{
		return A.SortKey != B.SortKey ? A.SortKey < B.SortKey
			: A.EntryId < B.EntryId;
	});
	
	int64 K = 0;
	for (const FEPInventoryEntry* E : All)
	{
		AssignSortKey(E->EntryId, K);
		K += SortKeyStep;
	}
}

int64 UEPInventoryComponent::KeySpace_NextAtEnd(int32 Container)
{
	int64 Max = 0; bool bAny = false;
	
	const FIndex& CurIndex = Entries.EnsureIndex();
	const TArray<int32>* ChildIds = CurIndex.ChildrenOf.Find(Container);
	if (!ChildIds) return 0;
	
	for (int32 ChildId : *ChildIds)
	{
		const FEPInventoryEntry* E = Entries.FindEntryPtr(ChildId);
		if (!E) continue;
		
		Max = bAny ? FMath::Max(Max, E->SortKey) : E->SortKey;
		bAny = true;
	}
	
	if (!bAny) return 0;
	
	if (Max > MAX_int64 - SortKeyGuard)
	{
		RenormalizeSortKeys(Container);
		return KeySpace_NextAtEnd(Container);
	}
	
	return Max + SortKeyStep;
}

int64 UEPInventoryComponent::KeySpace_Min(int32 Container) const
{
	int64 Min = 0; bool bAny = false;
	
	const FIndex& CurIndex = Entries.EnsureIndex();
	const TArray<int32>* ChildIds = CurIndex.ChildrenOf.Find(Container);
	if (!ChildIds) return 0;
	
	for (int32 ChildId : *ChildIds)
	{
		const FEPInventoryEntry* E = Entries.FindEntryPtr(ChildId);
		if (!E) continue;
		
		Min = bAny ? FMath::Min(Min, E->SortKey) : E->SortKey;
		bAny = true;
	}
	
	return bAny ? Min : 0;
}

bool UEPInventoryComponent::KeySpace_NextAbove(int32 Container, int64 Key, int32 Exclude, int64& OutKey) const
{
	bool bAny = false;
	
	const FIndex& CurIndex = Entries.EnsureIndex();
	const TArray<int32>* ChildIds = CurIndex.ChildrenOf.Find(Container);
	if (!ChildIds) return false;
	
	for (int32 ChildId : *ChildIds)
	{
		const FEPInventoryEntry* E = Entries.FindEntryPtr(ChildId);
		if (!E) continue;
		
		if (E->EntryId == Exclude || E->SortKey <= Key) continue;
		
		OutKey = bAny ? FMath::Min(OutKey, E->SortKey) : E->SortKey;
		bAny = true;
	}
	
	return bAny;
}

bool UEPInventoryComponent::KeyOf(int32 EntryId, int64& OutKey) const
{
	const FEPInventoryEntry* E = Entries.FindEntryPtr(EntryId);
	if (!E) return false;
	
	OutKey = E->SortKey;
	
	return true;
}

void UEPInventoryComponent::ReorderEntryInternal(int32 EntryId, int32 PrevEntryId, bool bRetry)
{
	if (!GetOwner()->HasAuthority()) return;
	
	FEPInventoryEntry E;
	if (!FindEntry(EntryId, E)) return;
	if (!E.SlotId.IsNone()) return;
	
	const int32 Container = E.ParentEntryId;
	
	// 다른 컨테이너나 슬롯에 넣으려는 경우 return
	if (PrevEntryId != INDEX_NONE)
	{
		FEPInventoryEntry P;
		if (!FindEntry(PrevEntryId, P)) return;
		if (P.ParentEntryId != Container || !P.SlotId.IsNone()) return;
		if (PrevEntryId == EntryId) return;
	}
	
	// 같은 자리에 끼워넣을 시 return
	{
		const TArray<int32> Cur = GetSortedContents(Container);
		const int32 MyIdx = Cur.Find(EntryId);
		const int32 CurPrev = (MyIdx <= 0) ? INDEX_NONE : Cur[MyIdx - 1];
		if (CurPrev == PrevEntryId) return;
	}
	
	int64 PrevKey = KeySpace_Min(Container) - SortKeyStep;
	if (PrevEntryId != INDEX_NONE && !KeyOf(PrevEntryId, PrevKey)) return;
	
	int64 NextKey = 0;
	const bool bTail = !KeySpace_NextAbove(Container, PrevKey, EntryId, NextKey);
	
	int64 NewKey;
	if (PrevEntryId == INDEX_NONE) NewKey = PrevKey;
	else if (bTail) NewKey = PrevKey + SortKeyStep;
	else NewKey = PrevKey + (int64)(((uint64)NextKey - (uint64)PrevKey) / 2);
	
	const bool bOutOfRange = (NewKey <= MIN_int64 + SortKeyGuard)
		|| (NewKey >= MAX_int64 - SortKeyGuard);
	
	const bool bNoGap = (PrevEntryId != INDEX_NONE) && !bTail && (NewKey <= PrevKey);
	
	if (bOutOfRange || bNoGap)
	{
		if (!ensureMsgf(!bRetry, TEXT("[Inventory] ReorderEntry: 재정규화 후에도 자리가 없다.")))
			return;
		
		RenormalizeSortKeys(Container);
		ReorderEntryInternal(EntryId, PrevEntryId, true);
		return;
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

bool UEPInventoryComponent::CheckIdentity(const FEPItemState& State) const
{
	if (!ensureMsgf(State.ItemUid.IsValid(), TEXT("[Inventory] 유효하지 않은 Uid")))
		return false;
	
	const FIndex& CurIndex = Entries.EnsureIndex();
	
	if (!ensureMsgf(!CurIndex.IdByUid.Contains(State.ItemUid),
		TEXT("[Inventory] 중복된 Uid : %s"), *State.ItemUid.ToString()))
		return false;
	return true;
}
