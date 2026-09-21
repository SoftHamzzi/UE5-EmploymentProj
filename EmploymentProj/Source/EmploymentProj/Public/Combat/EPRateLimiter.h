// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

struct FEPRateLimiter
{
	void Reset(double Now, float InMaxTokens)
	{
		MaxTokens = FMath::Max(1.f, InMaxTokens);
		Tokens = InMaxTokens;
		LastUpdate = Now;
	}
	
	bool TryTake(double Now, float RefillPerSecond)
	{
		const float Elapsed = static_cast<float>(Now - LastUpdate);
		Tokens = FMath::Min(MaxTokens, Tokens + Elapsed * FMath::Max(0.f, RefillPerSecond));
		LastUpdate = Now;
		if (Tokens < 1.f) return false;
		Tokens -= 1.f;
		return true;
	}
	
private:
	float Tokens = 0.f;
	float MaxTokens = 1.f;
	double LastUpdate = 0.f;
};