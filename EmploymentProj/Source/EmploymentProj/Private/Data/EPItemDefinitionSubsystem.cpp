// Fill out your copyright notice in the Description page of Project Settings.

#include "Data/EPItemDefinitionSubsystem.h"
#include "Data/EPItemDefinition.h"
#include "Data/EPLootDeveloperSettings.h"
#include "Data/EPItemData.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Types/EPTypes.h"

void UEPItemDefinitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	BuildDataCache();
	LoadAllDefinitions();
}

void UEPItemDefinitionSubsystem::Deinitialize()
{
	DefinitionCache.Reset();
	DataCache.Reset();
	DefinitionHandle.Reset();
	Super::Deinitialize();
}

const FEPItemData* UEPItemDefinitionSubsystem::FindData(FName ItemId) const
{
	return DataCache.Find(ItemId);
}

UEPItemDefinition* UEPItemDefinitionSubsystem::FindDefinition(FName ItemId) const
{
	const TObjectPtr<UEPItemDefinition>* Found = DefinitionCache.Find(ItemId);
	return Found ? Found->Get() : nullptr;
}

bool UEPItemDefinitionSubsystem::MakeItemState(FName ItemId, FEPItemState& OutState) const
{
	const FEPItemData* Row = FindData(ItemId);
	const UEPItemDefinition* Def = FindDefinition(ItemId);
	if (!Row || !Def)
	{
		UE_LOG(LogTemp, Error, TEXT("[ItemCore] '%s' - Row=%s Definition=%s"),
			*ItemId.ToString(), Row ? TEXT("OK") : TEXT("없음"),
			Def ? TEXT("OK") : TEXT("없음"));
		return false;
	}
	Def->InitState(*Row, OutState);
	return true;
}

void UEPItemDefinitionSubsystem::BuildDataCache()
{
	const UEPLootDeveloperSettings* Settings = GetDefault<UEPLootDeveloperSettings>();
	UDataTable* Table = Settings->ItemDataTable.LoadSynchronous();
	if (!Table)
	{
		UE_LOG(LogTemp, Error, TEXT("ItemDataTable이 설정되지 않았습니다."));
		return;
	}
	
	DataCache.Reset();
	Table->ForeachRow<FEPItemData>(TEXT("BuildDataCache"),
		[this](const FName& RowName, const FEPItemData& Row)
		{
			if (Row.ItemId != RowName)
				UE_LOG(LogTemp, Warning,
					TEXT("[ItemRegistry] RowName(%s) != ItemId(%s)"),
					*RowName.ToString(), *Row.ItemId.ToString());
			
			DataCache.Add(RowName, Row);
		});
}

void UEPItemDefinitionSubsystem::LoadAllDefinitions()
{
	UAssetManager& Manager = UAssetManager::Get();
	
	TArray<FPrimaryAssetId> Ids;
	Manager.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("ItemDef")), Ids);
	
	if (Ids.Num() == 0)
	{
		UE_LOG(LogTemp, Error,
            TEXT("[ItemRegistry] ItemDef 프라이머리 에셋이 하나도 없습니다. "
            "Project Settings > Asset Manager 등록을 확인하십시오."));
        return;
	}
	
	DefinitionHandle = Manager.LoadPrimaryAssets(Ids);
	
	if (DefinitionHandle.IsValid())
	{
		DefinitionHandle->WaitUntilComplete();
	}
	
	BuildDefinitionCache();
}

void UEPItemDefinitionSubsystem::BuildDefinitionCache()
{
	DefinitionCache.Reset();
	
	TArray<UObject*> Loaded;
	UAssetManager::Get().GetPrimaryAssetObjectList(FPrimaryAssetType(TEXT("ItemDef")), Loaded);
	
	for (UObject* Obj : Loaded)
	{
		UEPItemDefinition* Def = Cast<UEPItemDefinition>(Obj);
		if (!Def) { continue; }
		
		if (Def->ItemId.IsNone())
		{
			UE_LOG(LogTemp, Warning, TEXT("[ItemRegistry] %s: ItemId가 비어 있습니다."),
				*GetNameSafe(Def));
			continue;
		}
		
		if (const TObjectPtr<UEPItemDefinition>* Existing = DefinitionCache.Find(Def->ItemId))
		{
			UE_LOG(LogTemp, Error,
				TEXT("[ItemRegistry] ItemId '%s' 중복 - %s와 %s, 후자를 버립니다."),
				*Def->ItemId.ToString(), *GetNameSafe(*Existing), *GetNameSafe(Def));
			continue;
		}
		
		if (!DataCache.Contains(Def->ItemId))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ItemRegistry] %s: ItemId '%s'에 해당하는 DT 행이 없습니다."),
				*GetNameSafe(Def), *Def->ItemId.ToString());
		}
		
		DefinitionCache.Add(Def->ItemId, Def);
	}
	
	for (const TPair<FName, FEPItemData>& Pair : DataCache)
	{
		if (!DefinitionCache.Contains(Pair.Key))
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[ItemRegistry] DT 행 '%s'에 대응하는 Definition 에셋이 없습니다."),
				   *Pair.Key.ToString());
		}
	}
	
	if (DefinitionCache.IsEmpty())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[ItemRegistry] Definition이 0개입니다. "
				 "Project Settings > Asset Manager의 ItemDef 등록을 확인하십시오."));
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static UEPItemDefinitionSubsystem* GetItemSubsystem(UWorld* World)
{
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UEPItemDefinitionSubsystem>() : nullptr;
}

static FAutoConsoleCommandWithWorldAndArgs CmdItemState(
	TEXT("EP.Item.State"),
	TEXT("EP.Item.State <ItemId> - MakeItemState() 결과 출력"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
		{
			const UEPItemDefinitionSubsystem* Sub = GetItemSubsystem(World);
			if (!Sub)
			{
				UE_LOG(LogTemp, Error, TEXT("[ItemCore] 서브시스템이 없습니다. PIE 실행 중에 치십시오."));
				return;
			}
			if (Args.Num() < 1)
			{
				UE_LOG(LogTemp, Error, TEXT("[ItemCore] 사용법: EP.Item.State <ItemId>"));
				return;
			}
			
			const FName ItemId(*Args[0]);
			FEPItemState State;
			if (!Sub->MakeItemState(ItemId, State)) { return; }
			
			const FEPItemData* Row = Sub->FindData(ItemId);
			const UEPItemDefinition* Def = Sub->FindDefinition(ItemId);
			
			UE_LOG(LogTemp, Log, TEXT("[ItemCore] %s, Charges = %d, Durability = %.1f, SlotSize = %d (%s)"),
				*ItemId.ToString(), State.Charges, State.Durability,
				Row->SlotSize, *GetNameSafe(Def->GetClass()));
		}
	), ECVF_Cheat);

static FAutoConsoleCommandWithWorldAndArgs CmdItemDump(
	TEXT("EP.Item.Dump"),
	TEXT("DataCache 행 수 / DefinitionCache 상주 수"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
		{
			const UEPItemDefinitionSubsystem* Sub = GetItemSubsystem(World);
			if (!Sub)
			{
				UE_LOG(LogTemp, Error, TEXT("[ItemCore] 서브시스템이 없습니다. PIE 실행 중에 치십시오."));
				return;
			}
			
			UE_LOG(LogTemp, Log, TEXT("[ItemCore] DataCache = %d, Definitions = %d"),
				Sub->GetDataCacheNum(), Sub->GetDefinitionCacheNum());
		}
	), ECVF_Cheat);
#endif