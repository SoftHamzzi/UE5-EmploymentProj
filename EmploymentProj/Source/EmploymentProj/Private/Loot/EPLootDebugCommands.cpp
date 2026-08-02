// Fill out your copyright notice in the Description page of Project Settings.

#include "EngineUtils.h"
#include "Data/EPItemDefinitionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Loot/EPLootTable.h"
#include "Engine/AssetManager.h"
#include "Loot/EPItemSpawner.h"
#include "Loot/EPPickup.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static FAutoConsoleCommandWithWorldAndArgs CmdLootRollTable(
	TEXT("EP.Loot.RollTable"),
	TEXT("EP.Loot.RollTable <TableName> [Count]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogTemp, Error, TEXT("[Loot] 사용법: EP.Loot.RollTable <TableName> [Count]"));
				return;
			}
			
			const int32 Count = (Args.Num() >= 2) ? FMath::Max(1, FCString::Atoi(*Args[1])) : 1000;
				
			const UEPItemDefinitionSubsystem* Sub = UEPItemDefinitionSubsystem::Get(World);
			if (!Sub)
			{
				UE_LOG(LogTemp, Error, TEXT("[Loot] 서브시스템이 없습니다. PIE 실행 중에 치십시오."));
				return;
			}
			
			UAssetManager& Manager = UAssetManager::Get();
			const FPrimaryAssetId Id(TEXT("LootTable"), FName(*Args[0]));
			
			TSharedPtr<FStreamableHandle> Handle = Manager.LoadPrimaryAsset(Id);
			if (Handle.IsValid()) { Handle->WaitUntilComplete(); }
			
			const UEPLootTable* Table = Manager.GetPrimaryAssetObject<UEPLootTable>(Id);
			if (!Table)
			{
				UE_LOG(LogTemp, Error,
					TEXT("[Loot] LootTable '%s'를 찾을 수 없습니다. "
					"에셋 이름 오타이거나 AssetManager에 등록되지 않았습니다."),
					*Args[0]);
				return;
			}
			
			int32 Empty = 0, Failed = 0;
			TMap<FName, int32> PerItem;
			TArray<int32> PerRarity;
			PerRarity.SetNumZeroed(StaticEnum<EEPItemRarity>()->NumEnums() - 1);
			
			for (int32 i=0; i<Count; i++)
			{
				FName Rolled;
				if (!RollLootTable(Table, Rolled)) { ++Failed; continue; }
				if (Rolled.IsNone()) { Empty++; continue; }
				
				++PerItem.FindOrAdd(Rolled);
				
				if (const FEPItemData* Row = Sub->FindData(Rolled))
				{
					++PerRarity[(int32)Row->Rarity];
				}
			}
			
			const int32 Rolled = Count - Empty - Failed;
			UE_LOG(LogTemp, Log, TEXT("Empty %6d / %d (%.1f%%)"), Empty, Count, 100.f*Empty/Count);
			UE_LOG(LogTemp, Log, TEXT("Failed %5d / %d"), Failed, Count);
			UE_LOG(LogTemp, Log, TEXT("--- 아이템이 나온 %d회 기준 ---"), Rolled);
			
			UEnum* E = StaticEnum<EEPItemRarity>();
			for (int32 r=0; r<PerRarity.Num(); r++)
			{
				UE_LOG(LogTemp, Log, TEXT("%-12s %4d (%.1f%%)"),
					*E->GetNameStringByIndex(r), PerRarity[r],
					Rolled > 0 ? 100.f * PerRarity[r] / Rolled : 0.f);
			}
			
			PerItem.ValueSort([](int32 A, int32 B) { return A > B; });
			for (const TPair<FName, int32>& P : PerItem)
			{
				UE_LOG(LogTemp, Log, TEXT("   %-18s %4d"), *P.Key.ToString(), P.Value);
			}
		}
	), ECVF_Cheat);

static FAutoConsoleCommandWithWorldAndArgs CmdLootRespawn(
	TEXT("EP.Loot.Respawn"),
	TEXT("EP.Loot.Respawn - 스포너 리롤(서버 전용)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
		{
			if (World->GetNetMode() == NM_Client)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Loot] Respawn은 서버 전용입니다."));
				return;
			}
			
			int32 N = 0;
			for (AEPItemSpawner* Spawner : TActorRange<AEPItemSpawner>(World))
			{
				Spawner->ClearLoot();
				Spawner->SpawnLoot();
				++N;
			}
			UE_LOG(LogTemp, Log, TEXT("[Loot] 스포너 %d개 리롤"), N);
		}
	), ECVF_Cheat);

static FAutoConsoleCommandWithWorldAndArgs CmdLootList(
	TEXT("EP.Loot.List"),
	TEXT("EP.Loot.List - "),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args, UWorld* World)
		{
			const bool bAuthority = World->GetNetMode() != NM_Client;
			
			UE_LOG(LogTemp, Log, TEXT("Idx, ItemId, Location, Charges%s, Claimed"),
				bAuthority ? TEXT(""): TEXT("[server-only]"));
			
			int32 Idx = 0;
			for (AEPPickup* P : TActorRange<AEPPickup>(World))
			{
				const FVector L = P->GetActorLocation();
				if (bAuthority)
				{
					UE_LOG(LogTemp, Log, TEXT("%-4d %-15s (%.0f, %.0f, %0.f) %-7d %s"),
						Idx, *P->GetItemId().ToString(), L.X, L.Y, L.Z,
						P->GetState().Charges, P->IsClaimed() ? TEXT("true") : TEXT("false"));
				} else
				{
					UE_LOG(LogTemp, Log, TEXT("%-4d %-15s (%.0f, %0.f, %0.f) %-7d -"),
						Idx, *P->GetItemId().ToString(), L.X, L.Y, L.Z, P->GetState().Charges);
				}
				++Idx;
			}
		}
	), ECVF_Cheat);
#endif