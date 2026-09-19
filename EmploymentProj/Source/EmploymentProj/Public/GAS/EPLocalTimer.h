#pragma once

#include "CoreMinimal.h"

struct FEPLocalTimer
{
	void Start(float Now, float Duration)
	{
		PrevRemaining = Remaining;
		PrevLastUpdate = LastUpdate;
		bPrevStarted = bStarted;
		
		Remaining = FMath::Max(0.f, Duration);
		LastUpdate = Now;
		bStarted = true;
		++Generation;
	}
	
	void Bank(float Now, float RateSoFar)
	{
		if (!bStarted) return;
		Remaining = GetRemaining(Now, RateSoFar);
		LastUpdate = Now;
	}
	
	float GetRemaining(float Now, float Rate) const {
		if (!bStarted) return 0.f;
		return FMath::Max(0.f, Remaining - (Now - LastUpdate) * FMath::Max(0.f, Rate));
	}
	
	bool IsElapsed(float Now, float Rate, float Tolerance) const
	{
		return GetRemaining(Now, Rate) <= Tolerance;
	}
	
	void Revert(uint32 StartedGeneration)
	{
		if (StartedGeneration != Generation) return;
		Remaining = PrevRemaining;
		LastUpdate = PrevLastUpdate;
		bStarted = bPrevStarted;
	}
	
	uint32 GetGeneration() const { return Generation; }
	
private:
	float Remaining = 0.f;
	float LastUpdate = 0.f;
	bool bStarted = false;
	
	float PrevRemaining = 0.f;
	float PrevLastUpdate = 0.f;
	bool bPrevStarted = false;
	
	uint32 Generation = 0;
};