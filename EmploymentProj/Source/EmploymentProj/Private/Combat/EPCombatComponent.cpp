// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/EPCombatComponent.h"

// System
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

// SFX/VFX
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"

// Components
#include "Combat/EPPhysicalMaterial.h"
#include "Combat/EPServerSideRewindComponent.h"
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

void UEPCombatComponent::RequestFire(const FVector& Origin, const FVector& Direction, float ClientFireTime)
{
	if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;
	
	// --- 클라이언트 사전 검증 ---
	if (EquippedWeapon->CurrentAmmo <= 0) return;
	
	// 연사속도 체크
	float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
	float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LocalLastFireTime < FireInterval) return;
	LocalLastFireTime = CurrentTime;
	
	AEPCharacter* Owner = GetOwnerCharacter();
	if (Owner && Owner->IsLocallyControlled())
	{
		const FVector MuzzleLocation =
			(EquippedWeapon->WeaponMesh && EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket")))
			? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
			: EquippedWeapon->GetActorLocation();
		
		PlayLocalMuzzleEffect(MuzzleLocation);
	}
	
	Server_Fire(Origin, Direction, ClientFireTime);
	
	if (Owner && Owner->IsLocallyControlled())
	{
		float Pitch = EquippedWeapon->GetRecoilPitch();
		float Yaw = FMath::RandRange(
			-EquippedWeapon->GetRecoilYaw(),
			EquippedWeapon->GetRecoilYaw());
		Owner->AddControllerPitchInput(-Pitch);
		Owner->AddControllerYawInput(Yaw);
	}
}

void UEPCombatComponent::PlayLocalMuzzleEffect(const FVector& MuzzleLocation)
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
	
	if (FireSFX)
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), FireSFX, MuzzleLocation);
}

void UEPCombatComponent::PlayLocalImpactEffect(const FVector& ImpactPoint, const FVector& ImpactNormal)
{
	const FRotator ImpactRot = ImpactNormal.Rotation();
	
	if (ImpactFX)
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactFX, ImpactPoint, ImpactRot);
	if (ImpactSFX)
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSFX, ImpactPoint);
}

void UEPCombatComponent::OnRep_EquippedWeapon()
{
	AEPCharacter* Owner = GetOwnerCharacter();
	if (!Owner || !EquippedWeapon) return;
	
	EquippedWeapon->AttachToComponent(
		Owner->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		TEXT("WeaponSocket")
	);
	
	if (EquippedWeapon->WeaponDef && EquippedWeapon->WeaponDef->WeaponAnimLayer)
	{
		Owner->GetMesh()->LinkAnimClassLayers(EquippedWeapon->WeaponDef->WeaponAnimLayer);
	}
}

// 서버 전용
void UEPCombatComponent::EquipWeapon(AEPWeapon* NewWeapon)
{
	if (!GetOwner()->HasAuthority() || !NewWeapon) return;
	
	if (EquippedWeapon)
		UnequipWeapon();
	
	EquippedWeapon = NewWeapon;
	
	AEPCharacter* Owner = GetOwnerCharacter();
	NewWeapon->AttachToComponent(
		Owner->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		TEXT("WeaponSocket"));
	
	if (NewWeapon->WeaponDef && NewWeapon->WeaponDef->WeaponAnimLayer)
	{
		Owner->GetMesh()->LinkAnimClassLayers(NewWeapon->WeaponDef->WeaponAnimLayer);
	}
}

// 서버 전용
void UEPCombatComponent::UnequipWeapon()
{
	if (!GetOwner()->HasAuthority() || !EquippedWeapon) return;
	
	AEPCharacter* Owner = GetOwnerCharacter();
	if (EquippedWeapon->WeaponDef && EquippedWeapon->WeaponDef->WeaponAnimLayer)
		Owner->GetMesh()->UnlinkAnimClassLayers(EquippedWeapon->WeaponDef->WeaponAnimLayer);
	
	EquippedWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	EquippedWeapon = nullptr;
}

void UEPCombatComponent::Server_Fire_Implementation(
	const FVector_NetQuantize& Origin, const FVector_NetQuantizeNormal& Direction, float ClientFireTime)
{
	// 연사 속도, 탄약 검증
	if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;
	
	FVector SpreadDir = Direction;
	EquippedWeapon->Fire(SpreadDir);
	
	AEPCharacter* Owner = GetOwnerCharacter();
	if (!Owner || !Owner->GetServerSideRewindComponent()) return;
	
	// 히트스캔: 방향 배열로 감싸 HandleHitscanFire에 전달. 산탄총 확장 대비
	const TArray<FVector> Directions = { SpreadDir };
	HandleHitscanFire(Owner, Origin, Directions, ClientFireTime);
	
	// 발사 이펙트 (항상 먼저 재생)
	const FVector MuzzleLocation =
		EquippedWeapon && EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
		? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
		: EquippedWeapon->GetActorLocation();
	
	Multicast_PlayMuzzleEffect(MuzzleLocation);
	
	// if (bHit) Multicast_PlayImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal);
}

void UEPCombatComponent::Server_Reload_Implementation()
{
	if (!EquippedWeapon) return;
	EquippedWeapon->StartReload();
}

void UEPCombatComponent::Multicast_PlayMuzzleEffect_Implementation(const FVector_NetQuantize& MuzzleLocation)
{
	AEPCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar && OwnerChar->IsLocallyControlled()) return;
	
	PlayLocalMuzzleEffect(MuzzleLocation);
}

void UEPCombatComponent::Multicast_PlayImpactEffect_Implementation(const FVector_NetQuantize& ImpactPoint, const FVector_NetQuantize& ImpactNormal)
{
	PlayLocalImpactEffect(ImpactPoint, ImpactNormal);
}

void UEPCombatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UEPCombatComponent, EquippedWeapon);
}

void UEPCombatComponent::HandleHitscanFire(
	AEPCharacter* Owner,
	const FVector& Origin,
	const TArray<FVector>& Directions,
	float ClientFireTime)
{
	if (!Owner || !Owner->GetServerSideRewindComponent()) return;
	
	TArray<FHitResult> ConfirmedHits;
	Owner->GetServerSideRewindComponent()->ConfirmHitscan(Owner, EquippedWeapon, Origin, Directions, ClientFireTime, ConfirmedHits);
	
	// Damage - GAS 전환 시 GameplayEffectSpec + SetByCaller로 교체
	for (const FHitResult& Hit : ConfirmedHits)
	{
		if (!Hit.GetActor()) continue;
		
		const float BaseDamage = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
		const float BoneMultiplier = GetBoneMultiplier(Hit.BoneName);
		const float MaterialMultiplier = GetMaterialMultiplier(Hit.PhysMaterial.Get());
		const float FinalDamage = BaseDamage * BoneMultiplier * MaterialMultiplier;
		
		UE_LOG(LogTemp, Log,
			TEXT("[BoneHitbox] Bone=%s PM=%s Base=%.1f Bone*=%.2f Mat*=%.2f Final=%.1f"),
			*Hit.BoneName.ToString(),
			Hit.PhysMaterial.IsValid() ? *Hit.PhysMaterial->GetName() : TEXT("None"),
			BaseDamage, BoneMultiplier, MaterialMultiplier, FinalDamage);
		
		UGameplayStatics::ApplyPointDamage(
			Hit.GetActor(),
			FinalDamage,
			(Hit.ImpactPoint - Origin).GetSafeNormal(),
			Hit,
			Owner->GetController(),
			Owner,
			UDamageType::StaticClass()
		);
		
		Multicast_PlayImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal);
	}
}

float UEPCombatComponent::GetBoneMultiplier(const FName& BoneName) const
{
	if (EquippedWeapon && EquippedWeapon->WeaponDef)
		if (const float* Found = EquippedWeapon->WeaponDef->BoneDamageMultiplierMap.Find(BoneName))
			return *Found;
	
	// 누락 본은 기본 배율 1.0 + 경고 로그
	UE_LOG(LogTemp, Verbose, TEXT("[BoneHitbox] Bone multiplier fallback: %s"), *BoneName.ToString());
	return 1.0f;
}

float UEPCombatComponent::GetMaterialMultiplier(const UPhysicalMaterial* PM)
{
	if (const UEPPhysicalMaterial* EPM = Cast<UEPPhysicalMaterial>(PM))
	{
		// 현재는 bool/배율 기반
		if (EPM->bIsWeakSpot) return EPM->WeakSpotMultiplier;
		
		// GAS 들어가면 PhysicalMaterial의 GameplayTagContainer 기반 판정
		// TAG_Gameplay_Zone_Weakspot 태그가 있는가?
	}
	return 1.f;
}
