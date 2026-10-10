// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "EPTypes.generated.h"

UENUM(BlueprintType)
enum class EEPMatchPhase : uint8
{
	Waiting	UMETA(DisplayName = "Waiting"),
	Playing	UMETA(DisplayName = "Playing"),
	Ended	UMETA(DisplayName = "Ended")
};

UENUM(BlueprintType)
enum class EEPItemRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Legendary
};

UENUM(BlueprintType)
enum class EEPFireMode : uint8
{
	Single,
	Burst,
	Auto
};

UENUM(BlueprintType)
enum class EEPWeaponState : uint8
{
	Idle,
	Reloading,
	Firing
};

UENUM(BlueprintType)
enum class EEPItemType : uint8
{
	Weapon,
	Ammo,
	Consumable,
	QuestItem,
	Misc
};

USTRUCT()
struct FEPBoneSnapshot
{
	GENERATED_BODY()
	
	UPROPERTY()
	FName BoneName;
	UPROPERTY()
	FTransform WorldTransform;
};

USTRUCT()
struct FEPHitboxSnapshot
{
	GENERATED_BODY()
	
	UPROPERTY()
	float ServerTime = 0.f;
	UPROPERTY()
	FVector Location = FVector::ZeroVector;
	UPROPERTY()
	TArray<FEPBoneSnapshot> Bones;
};

USTRUCT(BlueprintType)
struct FEPItemStat {
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	FGameplayTag Tag;
	
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 Value = 0;
};

USTRUCT(BlueprintType)
struct FEPItemState
{
	GENERATED_BODY()
	
	UPROPERTY(NotReplicated, BlueprintReadOnly, Category = "Item")
	FGuid ItemUid;
	
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	int32 Charges = 0;
	
	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TArray<FEPItemStat> Stats;

	// 이 태그의 수치를 찾는다.
	//   Tag      : 찾을 수치 태그
	//   OutValue : 찾았을 때만 채워진다
	// 반환 : 찾았는가 (값 0과 '없음'을 구분하려고 bool+out이다)
	bool FindStat(FGameplayTag Tag, int32& OutValue) const
	{
		const FEPItemStat* Stat = Stats.FindByPredicate(
			[&](const FEPItemStat& S){ return Tag == S.Tag; });
		if (!Stat) return false;
		OutValue = Stat->Value;
		return true;
	}

	// 이 태그의 수치를 쓴다. 없으면 새로 넣는다. 권한 · 알림 · dirty는 호출자(컴포넌트) 몫이다.
	// 반환 : 값이 실제로 바뀌었는가. 같은 값이면 false
	bool SetStat(FGameplayTag Tag, int32 Value)
	{
		FEPItemStat* Stat = Stats.FindByPredicate(
			[&](const FEPItemStat& S){ return Tag == S.Tag; });
		if (Stat)
		{
			if (Stat->Value == Value) return false;
			Stat->Value = Value;
			return true;
		}
		Stats.Add({Tag, Value});
		return true;
	}
};



UENUM(BlueprintType)
enum class EEPBallisticType : uint8
{
	Hitscan,
	ProjectileFast,
	ProjectileSlow
};

static constexpr ECollisionChannel EP_TraceChannel_Weapon = ECC_GameTraceChannel1;
static constexpr ECollisionChannel EP_TraceChannel_Interact = ECC_GameTraceChannel3;