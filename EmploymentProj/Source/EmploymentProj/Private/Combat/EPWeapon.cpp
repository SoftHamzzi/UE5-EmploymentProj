// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/EPWeapon.h"

#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/EPCharacter.h"
#include "Engine/World.h"
#include "GAS/EPAttributeSet.h"
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
	BuildSpreadCDFTable();
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
	
}

bool AEPWeapon::CanFire() const
{
	if (!WeaponDef) return false;
	if (AEPCharacter* EPOwner = Cast<AEPCharacter>(GetOwner()))
	{
		if (UAbilitySystemComponent* ASC = EPOwner->GetAbilitySystemComponent())
		{
			if (const UEPAttributeSet* AS = Cast<const UEPAttributeSet>(ASC->GetAttributeSet(UEPAttributeSet::StaticClass())))
			{
				if (AS->GetAmmo() <= 0.f) return false;
			}
		}
	}
	return true;
}

float AEPWeapon::GetDamage() const
{
	return WeaponDef ? WeaponDef->Damage : 0.f;
}

void AEPWeapon::Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets)
{
	if (!HasAuthority()) return;
	if (!WeaponDef) return;
	
	LastFireTime = GetWorld()->GetTimeSeconds();
	
	// 퍼짐 누적
	CurrentSpread = FMath::Min(
		CurrentSpread + WeaponDef-> SpreadPerShot,
		WeaponDef->MaxSpread
	);
	ConsecutiveShots++;
	
	const int32 Count = FMath::Max(1, WeaponDef->PelletCount);
	OutPellets.Reserve(Count);
	const float HalfAngle = FMath::DegreesToRadians(CalculateSpread() * 0.5f);
	
	FVector Up, Right;
	AimDir.FindBestAxisVectors(Up, Right);
	
	const float SectorSize = TWO_PI / Count;
	
	for (int32 i=0; i<Count; i++)
	{
		const float R = SampleSpread();
		const float Theta = R * HalfAngle;
		
		const float Phi = (i * SectorSize) + FMath::FRand() * SectorSize;
		OutPellets.Add(
			AimDir	* FMath::Cos(Theta)
			+ Up		* FMath::Sin(Theta) * FMath::Cos(Phi)
			+ Right		* FMath::Sin(Theta) * FMath::Sin(Phi)
		);
	}
	
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

void AEPWeapon::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AEPWeapon::BuildSpreadCDFTable()
{
	SpreadCDFTable.SetNumUninitialized(CDFTableSize);
	
	if (!WeaponDef || !WeaponDef->SpreadDistributionCurve)
	{
		for (int32 i=0; i<CDFTableSize; i++)
			SpreadCDFTable[i] = static_cast<float>(i+1) / CDFTableSize;
		return;
	}
	
	double Cumulative = 0.0;
	TArray<double> RawCDF;
	RawCDF.SetNumUninitialized(CDFTableSize);
	
	for (int32 i=0; i<CDFTableSize; i++)
	{
		const float XMid = (i + 0.5f) /CDFTableSize;
		const float PDFVal = FMath::Max(0.f, WeaponDef->SpreadDistributionCurve->GetFloatValue(XMid));
		Cumulative += PDFVal;
		RawCDF[i] = Cumulative;
	}
	
	if (Cumulative > KINDA_SMALL_NUMBER)
	{
		for (int32 i=0; i<CDFTableSize; i++)
			SpreadCDFTable[i] = static_cast<float>(RawCDF[i] / Cumulative);
	} else
	{
		for (int32 i=0; i<CDFTableSize; i++)
			SpreadCDFTable[i] = static_cast<float>(i+1) / CDFTableSize;
	}
}

float AEPWeapon::SampleSpread() const
{
	if (SpreadCDFTable.IsEmpty())
		return FMath::FRand();
	
	const float U = FMath::FRand();
	
	int32 Lo = 0, Hi = CDFTableSize - 1;
	while (Lo < Hi)
	{
		const int32 Mid = (Lo + Hi) / 2;
		if (SpreadCDFTable[Mid] < U)
			Lo = Mid + 1;
		else
			Hi = Mid;
	}
	
	return static_cast<float>(Lo) / CDFTableSize;
}
