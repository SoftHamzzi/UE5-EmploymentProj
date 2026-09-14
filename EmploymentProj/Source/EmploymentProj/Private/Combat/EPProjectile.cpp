// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/EPProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AEPProjectile::AEPProjectile()
{
 	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(5.f);
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComp->SetNotifyRigidBodyCollision(true);
	CollisionComp->OnComponentHit.AddDynamic(this, &AEPProjectile::OnProjectileHit);
	SetRootComponent(CollisionComp);
	
	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	MovementComp->bRotationFollowsVelocity = true;

}

void AEPProjectile::Initialize(float InDamage, const FVector& InDirection)
{
	BaseDamage = InDamage;
	LaunchDir = InDirection.GetSafeNormal();
	if (AActor* MyInstigator = GetInstigator())
		CollisionComp->IgnoreActorWhenMoving(MyInstigator, true);
}

void AEPProjectile::SetCosmeticOnly()
{
	bIsCosmeticOnly = true;
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEPProjectile::OnProjectileHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (bIsCosmeticOnly) return;
	
	if (!HasAuthority()) return;
	if (OtherActor)
	{
		UGameplayStatics::ApplyPointDamage(
			OtherActor, BaseDamage, LaunchDir, Hit,
			GetInstigatorController(), GetInstigator(),
			UDamageType::StaticClass());
	}
	
	Destroy();
}







