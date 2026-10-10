// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "EPInventoryCheats.generated.h"

class UEPInventoryComponent;

UCLASS()
class EMPLOYMENTPROJ_API UEPInventoryCheats : public UCheatManagerExtension
{
	GENERATED_BODY()
public:
	UEPInventoryCheats();
	
	UFUNCTION(EXEC) void EPInvDump();
	UFUNCTION(EXEC) void EPInvDumpAll();
	
	// 서버에서 도는 덤프. BlueprintAuthorityOnly라 클라 창에서 쳐도 엔진이 ServerExec으로 서버에 보낸다(CheatManager.cpp:95-128)
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void EPInvDumpServer();
	
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void EPInvAdd(const FString& ItemId, int32 Container = -1);
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void EPInvDrop(int32 EntryId);
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void EPInvReorder(int32 EntryId, int32 PrevEntryId);
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void EPInvMove(int32 EntryId, int32 NewParent, const FString& SlotId);
	
private:
	UEPInventoryComponent* GetInv() const;
	
};
