#pragma once

#include "CoreMinimal.h"

struct FEPLocalTimer
{
	void Start(double Now, float Duration)
	{
		PrevRemaining = Remaining;
		PrevLastUpdate = LastUpdate;
		bPrevStarted = bStarted;
		
		Remaining = FMath::Max(0.f, Duration);
		LastUpdate = Now;
		bStarted = true;
		++Generation;
	}
	
	void Bank(double Now, float RateSoFar)
	{
		if (!bStarted) return;
		Remaining = GetRemaining(Now, RateSoFar);
		LastUpdate = Now;
	}
	
	float GetRemaining(double Now, float Rate) const {
		if (!bStarted) return 0.f;
		return FMath::Max(0.f, Remaining - static_cast<float>(Now - LastUpdate) * FMath::Max(0.f, Rate));
	}
	
	bool IsElapsed(double Now, float Rate, float Tolerance) const
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
	double LastUpdate = 0.0;
	bool bStarted = false;
	
	float PrevRemaining = 0.f;
	double PrevLastUpdate = 0.0;
	bool bPrevStarted = false;
	
	uint32 Generation = 0;
};