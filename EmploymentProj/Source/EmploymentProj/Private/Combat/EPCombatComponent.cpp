// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/EPCombatComponent.h"

// System
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

// SFX/VFX
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"

// Components
#include "AbilitySystemComponent.h"
#include "Combat/EPPhysicalMaterial.h"
#include "Combat/EPServerSideRewindComponent.h"
#include "Combat/EPWeapon.h"
#include "Core/EPCharacter.h"
#include "GameFramework/PlayerState.h"
#include "Combat/EPProjectile.h"
#include "GameFramework/GameStateBase.h"
#include "GAS/EPNativeGameplayTags.h"

// GAS
#include "GameplayEffect.h"
#include "GAS/EPGA_Item_PrimaryUse.h"

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

void UEPCombatComponent::HandleServerFire(const FVector& Origin, const FVector& Direction, float ClientFireTime)
{
	// 연사 속도, 탄약 검증
	if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;
	
	AEPCharacter* Owner = GetOwnerCharacter();
	if (!Owner) return;
	
	constexpr float MaxOriginDrift = 200.f;
	if (FVector::DistSquared(Origin, Owner->GetActorLocation()) > FMath::Square(MaxOriginDrift))
		return;
	
	if (!EquippedWeapon->CanFire()) return;
	
	// --- 탄도 분기 ---
	switch (EquippedWeapon->WeaponDef->BallisticType)
	{
	case EEPBallisticType::Hitscan:
	default:
		{
			TArray<FVector> PelletDirs;
			EquippedWeapon->Fire(Direction, ClientFireTime, PelletDirs);
			HandleHitscanFire(Owner, Origin, PelletDirs, ClientFireTime);
			break;
		}
	case EEPBallisticType::ProjectileFast:
	case EEPBallisticType::ProjectileSlow:
		{
			FVector SpreadDir = Direction;
			TArray<FVector> DiscardedPellets;
			EquippedWeapon->Fire(SpreadDir, ClientFireTime, DiscardedPellets);
			HandleProjectileFire(Owner, Origin, SpreadDir);
			break;
		}
	}
	
	// 발사 이펙트 (항상 먼저 재생)
	const FVector MuzzleLocation =
		EquippedWeapon && EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
		? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
		: EquippedWeapon->GetActorLocation();
	
	Multicast_PlayMuzzleEffect(MuzzleLocation);
}

void UEPCombatComponent::SpawnLocalCosmeticProjectile(const FVector& MuzzleLocation, const FVector& Direction)
{
	if (!EquippedWeapon || !EquippedWeapon->WeaponDef || !EquippedWeapon->WeaponDef->ProjectileClass) return;

	AEPProjectile* Cosmetic = GetWorld()->SpawnActor<AEPProjectile>(
		EquippedWeapon->WeaponDef->ProjectileClass,
		MuzzleLocation, Direction.GetSafeNormal().Rotation());

	if (Cosmetic)
		Cosmetic->SetCosmeticOnly();
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
	
	if (GetOwner()->HasAuthority() && Owner && NewWeapon->WeaponDef
		&& NewWeapon->WeaponDef->PrimaryUseAbilityClass)
	{
		if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
		{
			if (GrantedPrimaryUseHandle.IsValid())
				ASC->ClearAbility(GrantedPrimaryUseHandle);
			
			FGameplayAbilitySpec Spec(NewWeapon->WeaponDef->PrimaryUseAbilityClass, 1);
			Spec.GetDynamicSpecSourceTags().AddTag(EmpGameplayTags::TAG_Ability_Item_PrimaryUse);
			GrantedPrimaryUseHandle = ASC->GiveAbility(Spec);
		}
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
	
	if (GetOwner()->HasAuthority() && Owner)
	{
		if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
			ASC->ClearAbility(GrantedPrimaryUseHandle);
	}
	GrantedPrimaryUseHandle = FGameplayAbilitySpecHandle();
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

void UEPCombatComponent::Multicast_SpawnCosmeticProjectile_Implementation(const FVector_NetQuantize& MuzzleLocation,
	const FVector_NetQuantizeNormal& Direction)
{
	if (GetOwner()->HasAuthority()) return;

	// 발사한 본인은 RequestFire에서 이미 스폰했으므로 스킵
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (OwnerChar && OwnerChar->IsLocallyControlled()) return;

	SpawnLocalCosmeticProjectile(MuzzleLocation, Direction);
}

void UEPCombatComponent::ApplyGEDamage(AActor* Target, AActor* Instigator, TSubclassOf<UGameplayEffect> GEClass,
	float FinalDamage)
{
	if (!Target || !GEClass) return;
	
	IAbilitySystemInterface* TargetIF = Cast<IAbilitySystemInterface>(Target);
	UAbilitySystemComponent* TargetASC = TargetIF ? TargetIF->GetAbilitySystemComponent() : nullptr;
	
	IAbilitySystemInterface* InstigatorIF = Cast<IAbilitySystemInterface>(Instigator);
	UAbilitySystemComponent* InstigatorASC = InstigatorIF ? InstigatorIF->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC || !InstigatorASC) return;
	
	FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
	Context.AddInstigator(Instigator, Instigator);
	
	FGameplayEffectSpecHandle Spec = InstigatorASC->MakeOutgoingSpec(GEClass, 1.f, Context);
	if (!Spec.IsValid()) return;
	
	Spec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Damage, FinalDamage);
	
	InstigatorASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
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
		
		ApplyGEDamage(Hit.GetActor(), Owner, GE_DamageClass, FinalDamage);
		
		// UGameplayStatics::ApplyPointDamage(
		// 	Hit.GetActor(),
		// 	FinalDamage,
		// 	(Hit.ImpactPoint - Origin).GetSafeNormal(),
		// 	Hit,
		// 	Owner->GetController(),
		// 	Owner,
		// 	UDamageType::StaticClass()
		// );
		
		Multicast_PlayImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal);
	}
}

void UEPCombatComponent::HandleProjectileFire(
	AEPCharacter* Owner,
	const FVector& Origin,
	const FVector& Direction)
{
	if (!EquippedWeapon->WeaponDef || !EquippedWeapon->WeaponDef->ProjectileClass) return;
	
	const FVector MuzzleLoc =
		(EquippedWeapon->WeaponMesh && EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket")))
		? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
		: Origin;
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Owner;
	
	AEPProjectile* Proj = GetWorld()->SpawnActor<AEPProjectile>(
		EquippedWeapon->WeaponDef->ProjectileClass,
		MuzzleLoc, Direction.GetSafeNormal().Rotation(), SpawnParams);
	
	if (!Proj) return;
	
	Proj->Initialize(EquippedWeapon->GetDamage(), Direction, GE_DamageClass);
	
	// if (EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
		// Multicast_SpawnCosmeticProjectile(MuzzleLoc, Direction.GetSafeNormal());
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
