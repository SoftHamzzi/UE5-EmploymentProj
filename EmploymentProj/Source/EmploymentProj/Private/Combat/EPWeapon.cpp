// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/EPWeapon.h"

#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/EPCharacter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

// Sets default values
AEPWeapon::AEPWeapon()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	bReplicates = true;
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Weapon Mesh"));
	RootComponent = WeaponMesh;
	
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEPWeapon::BeginPlay()
{
	Super::BeginPlay();
	
	if (HasAuthority() && WeaponDef)
	{
		MaxAmmo = WeaponDef->MaxAmmo;
		CurrentAmmo = MaxAmmo;
	}
}

void AEPWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (!HasAuthority()) return;
	
	if (CurrentSpread > 0.f)
	{
		CurrentSpread = FMath::Max(
			0.f,
			CurrentSpread - WeaponDef->SpreadRecoveryRate * DeltaTime
		);
	}
	
	float FireInterval = 1.f / WeaponDef->FireRate;
	if (WeaponState == EEPWeaponState::Firing &&
		GetWorld()->GetTimeSeconds() - LastFireTime > FireInterval * 2.f)
	{
		WeaponState = EEPWeaponState::Idle;
		ConsecutiveShots = 0;
	}
}

bool AEPWeapon::CanFire() const
{
	if (WeaponState != EEPWeaponState::Idle &&

	WeaponState != EEPWeaponState::Firing) return false;
	if (CurrentAmmo <= 0) return false;
	if (!WeaponDef) return false;
	
	// 연사 속도 체크
	float FireInterval = 1.f / WeaponDef->FireRate;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastFireTime < FireInterval) return false;
	
	return true;
}

float AEPWeapon::GetDamage() const
{
	return WeaponDef ? WeaponDef->Damage : 0.f;
}

void AEPWeapon::Fire(FVector& OutDirection)
{
	if (!HasAuthority()) return;
	
	CurrentAmmo--;
	LastFireTime = GetWorld()->GetTimeSeconds();
	
	// 퍼짐 누적
	CurrentSpread = FMath::Min(
		CurrentSpread + WeaponDef-> SpreadPerShot,
		WeaponDef->MaxSpread
	);
	ConsecutiveShots++;
	
	// 서버 퍼짐 적용
	OutDirection = ApplySpread(OutDirection);
	
	// 상태 전환
	WeaponState = EEPWeaponState::Firing;
	
	// 탄약 0이면 자동 재장전
	if (CurrentAmmo <= 0) StartReload();
}

FVector AEPWeapon::ApplySpread(const FVector& Direction) const
{
	float FinalSpread = CalculateSpread();
	float HalfAngle = FMath::DegreesToRadians(FinalSpread * 0.5f);
	return FMath::VRandCone(Direction, HalfAngle);
}

float AEPWeapon::CalculateSpread() const
{
	float Spread = WeaponDef->BaseSpread + CurrentSpread;
	
	if (AEPCharacter* EPOwner = Cast<AEPCharacter>(GetOwner()))
	{
		if (EPOwner->GetIsAiming())
			Spread *= WeaponDef->ADSSpreadMultiplier;
		if (EPOwner->GetVelocity().Size2D() > 10.f)
			Spread *= WeaponDef->MovingSpreadMultiplier;
	}
	
	return FMath::Clamp(Spread, 0.f, WeaponDef->MaxSpread);
}

void AEPWeapon::StartReload()
{
	if (!HasAuthority()) return;
	if (WeaponState == EEPWeaponState::Reloading) return;
	
	if (CurrentAmmo >= MaxAmmo) return;
	
	WeaponState = EEPWeaponState::Reloading;
	
	GetWorldTimerManager().SetTimer(
		ReloadTimerHandle,
		this, &AEPWeapon::FinishReload,
		WeaponDef->ReloadTime,
		false
	);
}

void AEPWeapon::FinishReload()
{
	if (!HasAuthority()) return;
	
	CurrentAmmo = MaxAmmo;
	WeaponState = EEPWeaponState::Idle;
	ConsecutiveShots = 0;
	CurrentSpread = 0.f;
}

void AEPWeapon::OnRep_CurrentAmmo() const
{
	UE_LOG(LogTemp, Warning, TEXT("Remaining Ammo: %d"), CurrentAmmo);
}

void AEPWeapon::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION(AEPWeapon, CurrentAmmo, COND_OwnerOnly);
	
}