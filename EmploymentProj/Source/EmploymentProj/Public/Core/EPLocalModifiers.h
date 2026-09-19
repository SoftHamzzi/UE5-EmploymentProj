#pragma once

#include "CoreMinimal.h"
#include "AnimationEditorTypes.h"
#include "GameplayTagContainer.h"

struct FEPLocalModifiers
{
	void Set(FGameplayTag Source, float Value)
	{
		Values.FindOrAdd(Source) = Value;
	}
	
	void Clear(FGameplayTag Source)
	{
		Values.Remove(Source);
	}
	
	float Product(FGameplayTag Category) const
	{
		float P = 1.f;
		for (const TPair<FGameplayTag, float>& It : Values)
		{
			if (It.Key.MatchesTag(Category))
				P *= FMath::Max(0.f, It.Value);
		}
		return P;
	}
	
private:
	TMap<FGameplayTag, float> Values;
};