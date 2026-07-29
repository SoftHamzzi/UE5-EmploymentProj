// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/EPItemData.h"
#include "EPItemDefinitionSubsystem.generated.h"

class UEPItemDefinition;
struct FStreamableHandle;

UCLASS()
class EMPLOYMENTPROJ_API UEPItemDefinitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	const FEPItemData* FindData(FName ItemId) const;
	UEPItemDefinition* FindDefinition(FName ItemId) const;
	
	bool MakeItemState(FName ItemId, FEPItemState& OutState) const;
	
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	int32 GetDataCacheNum() const { return DataCache.Num(); }
	int32 GetDefinitionCacheNum() const { return DefinitionCache.Num(); }
#endif
	
private:
	void BuildDataCache();
	void LoadAllDefinitions();
	void BuildDefinitionCache();
	
	TMap<FName, FEPItemData> DataCache;
	
	UPROPERTY()
	TMap<FName, TObjectPtr<UEPItemDefinition>> DefinitionCache;
	
	TSharedPtr<FStreamableHandle> DefinitionHandle;
};
