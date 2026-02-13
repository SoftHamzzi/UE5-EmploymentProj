// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/EPCombatComponent.h"

// System
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

// SFX/VFX
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"

// Components
#include "Combat/EPWeapon.h"
#include "Core/EPCharacter.h"

UEPCombatComponent::UEPCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UEPCombatComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UEPCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

AEPCharacter* UEPCombatComponent::GetOwnerCharacter() const
{
	return Cast<AEPCharacter>(GetOwner());
}

AEPWeapon* UEPCombatComponent::GetEquippedWeapon() const
{
	return EquippedWeapon;
}

void UEPCombatComponent::RequestFire(const FVector& Origin, const FVector& Direction)
{
	if (!EquippedWeapon || !EquippedWeapon->WeaponData) return;
	
	// --- 클라이언트 사전 검증 ---
	if (EquippedWeapon->CurrentAmmo <= 0) return;

	AEPCharacter* Owner = GetOwnerCharacter();
	
	// 연사속도 체크
	float FireInterval = 1.f / EquippedWeapon->WeaponData->FireRate;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LocalLastFireTime < FireInterval) return;
	LocalLastFireTime = CurrentTime;
	
	Server_Fire(Origin, Direction);
	
	if (Owner->IsLocallyControlled())
	{
		float Pitch = EquippedWeapon->GetRecoilPitch();
		float Yaw = FMath::RandRange(
			-EquippedWeapon->GetRecoilYaw(),
			EquippedWeapon->GetRecoilYaw());
		Owner->AddControllerPitchInput(-Pitch);
		Owner->AddControllerYawInput(Yaw);
	}
}

void UEPCombatComponent::OnRep_EquippedWeapon()
{
	if (!EquippedWeapon) return;
	
	EquippedWeapon->AttachToComponent(
		GetOwnerCharacter()->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		TEXT("WeaponSocket")
	);
}

void UEPCombatComponent::SetEquippedWeapon(AEPWeapon* Weapon) { EquippedWeapon = Weapon; }

void UEPCombatComponent::Server_Fire_Implementation(const FVector& Origin, const FVector& Direction)
{
	// 연사 속도, 탄약 검증
	// 서버 레이캐스트
	// 히트 시 ApplyDamage
	// Multicast_PlayFireEffect
	if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;
	
	FVector SpreadDir = Direction;
	EquippedWeapon->Fire(SpreadDir);
	
	AEPCharacter* Owner = GetOwnerCharacter();
	
	// 서버 레이캐스트
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner);
	Params.AddIgnoredActor(EquippedWeapon);
	
	const FVector End = Origin + SpreadDir * 10000.f;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_GameTraceChannel1, Params);
	UE_LOG(LogTemp, Log, TEXT("히트 %d"), bHit);
	if (bHit && Hit.GetActor())
	{
		UGameplayStatics::ApplyDamage(
			Hit.GetActor(),
			
			EquippedWeapon->GetDamage(),
			Owner->GetController(),
			Owner,
			nullptr
		);
	}
	const FVector MuzzleLocation =
		
		EquippedWeapon && EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
		? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
		: EquippedWeapon->GetActorLocation();
	
	Multicast_PlayMuzzleEffect(MuzzleLocation);
	if (bHit)
		Multicast_PlayImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal);
}

void UEPCombatComponent::Server_Reload_Implementation()
{
	if (!EquippedWeapon) return;
	EquippedWeapon->StartReload();
}

void UEPCombatComponent::Multicast_PlayMuzzleEffect_Implementation(const FVector_NetQuantize& MuzzleLocation)
{
	if (MuzzleFX && EquippedWeapon && EquippedWeapon->WeaponMesh)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleFX,
			EquippedWeapon->WeaponMesh,
			TEXT("MuzzleSocket"),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true
		);
	}
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), FireSFX, MuzzleLocation);
}

void UEPCombatComponent::Multicast_PlayImpactEffect_Implementation(const FVector_NetQuantize& ImpactPoint, const FVector_NetQuantize& ImpactNormal)
{
	const FRotator ImpactRot = ImpactNormal.Rotation();
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactFX, ImpactPoint, ImpactRot);
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSFX, ImpactPoint);
}

void UEPCombatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UEPCombatComponent, EquippedWeapon);
}