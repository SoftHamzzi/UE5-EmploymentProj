// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EPProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UGameplayEffect;

UCLASS()
class EMPLOYMENTPROJ_API AEPProjectile : public AActor
{
	GENERATED_BODY()
	
public:
	// === 변수 ===
	
	// === 함수 ===
	AEPProjectile();
	
	void Initialize(float InDamage, const FVector& InDirection, TSubclassOf<UGameplayEffect> InGEClass);
	void SetCosmeticOnly();
	
protected:
	// === 변수 ===
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComp;
	
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> MovementComp;
	// === 함수 ===

private:
	// === 변수 ===
	float BaseDamage = 0.f;
	FVector LaunchDir = FVector::ForwardVector;
	bool bIsCosmeticOnly = false;
	
	UPROPERTY()
	TSubclassOf<UGameplayEffect> GE_DamageClass;
	
	// === 함수 ===
	UFUNCTION()
	void OnProjectileHit(
		UPrimitiveComponent* HitComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);
};
