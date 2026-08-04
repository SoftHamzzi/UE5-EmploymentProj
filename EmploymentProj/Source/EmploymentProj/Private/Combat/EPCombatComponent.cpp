// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/EPCombatComponent.h"

// System
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"

// Inheritance
#include "Combat/EPPhysicalMaterial.h"
#include "Combat/EPServerSideRewindComponent.h"
#include "Combat/EPWeapon.h"
#include "Core/EPCharacter.h"
#include "Combat/EPProjectile.h"
#include "Data/EPWeaponDefinition.h"
#include "Core/EPPlayerState.h"

// GAS
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GAS/EPAttributeSet.h"
#include "GAS/EPGA_Item_PrimaryUse.h"
#include "GameplayTagContainer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/EPNativeGameplayTags.h"

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
	
	if (EquippedWeapon)
		EquippedWeapon->BP_PlayImpactEffect(ImpactPoint, ImpactNormal, 0);
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
	AEPPlayerState* PS = Owner? Owner->GetPlayerState<AEPPlayerState>() : nullptr;
	if (PS)
	{
		if (UEPAttributeSet* AS = PS->GetAttributeSet())
		{
			AS->InitAmmo(static_cast<float>(NewWeapon->WeaponDef->MaxAmmo));
			AS->InitMaxAmmo(static_cast<float>(NewWeapon->WeaponDef->MaxAmmo));
		}
	}
	
	NewWeapon->AttachToComponent(
		Owner->GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		TEXT("WeaponSocket"));
	
	if (NewWeapon->WeaponDef && NewWeapon->WeaponDef->WeaponAnimLayer)
	{
		Owner->GetMesh()->LinkAnimClassLayers(NewWeapon->WeaponDef->WeaponAnimLayer);
	}
	
	if (GetOwner()->HasAuthority() && Owner && NewWeapon->WeaponDef)
	{
		if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
		{
			for (const FGameplayAbilitySpecHandle& Handle : GrantedWeaponAbilityHandles)
				if (Handle.IsValid())
					ASC->ClearAbility(Handle);
			GrantedWeaponAbilityHandles.Reset();
			
			for (const TSubclassOf<UGameplayAbility>& AbilityClass : NewWeapon->WeaponDef->WeaponAbilities)
			{
				if (!AbilityClass) continue;
				FGameplayAbilitySpec Spec(AbilityClass, 1);
				GrantedWeaponAbilityHandles.Add(ASC->GiveAbility(Spec));
			}
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
			for (const FGameplayAbilitySpecHandle& Handle : GrantedWeaponAbilityHandles)
				if (Handle.IsValid())
					ASC->ClearAbility(Handle);
	}
	GrantedWeaponAbilityHandles.Reset();
}

void UEPCombatComponent::Multicast_PlayMuzzleEffect_Implementation(const FVector_NetQuantize& MuzzleLocation)
{
	AEPCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar && OwnerChar->IsLocallyControlled()) return;
	
	PlayLocalMuzzleEffect(MuzzleLocation);
}

void UEPCombatComponent::Multicast_PlayImpactEffect_Implementation(const TArray<FVector_NetQuantize>& ImpactPoints, const TArray<FVector_NetQuantize>& ImpactNormals)
{
	UE_LOG(LogTemp, Log, TEXT("Multicast_ImpactEffect_Impl"));
	for (int32 i = 0; i < ImpactPoints.Num(); ++i)                                                                                                                                                                                                                                                                
	{                                                                                                                                                                                                                                                                                                             
		PlayLocalImpactEffect(ImpactPoints[i], ImpactNormals[i]);                                                                                                                                                                                                                                                 
	} 
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
	
	TArray<FVector_NetQuantize> ImpactPoints;
	TArray<FVector_NetQuantize> ImpactNormals;
	
	// Damage - GAS 전환 시 GameplayEffectSpec + SetByCaller로 교체
	for (const FHitResult& Hit : ConfirmedHits)
	{
		if (AEPCharacter* HitChar = Cast<AEPCharacter>(Hit.GetActor()))
		{
			const float BaseDamage = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
			const UEPPhysicalMaterial* PM = Cast<UEPPhysicalMaterial>(Hit.PhysMaterial.Get());
			const float Multiplier = GetTagDamageMultiplier(PM, EquippedWeapon->WeaponDef);
			const float FinalDamage = BaseDamage * Multiplier;
            
			UE_LOG(LogTemp, Log,
				TEXT("[BoneHitbox] Base=%.1f PM_Name=%s PM=%.1f Final=%.1f"),
				BaseDamage,
				Hit.PhysMaterial.IsValid() ? *Hit.PhysMaterial->GetName() : TEXT("None"),
				Multiplier,
				FinalDamage);
            
			ApplyGEDamage(Hit.GetActor(), Owner, GE_DamageClass, FinalDamage);
			
		}
		
		ImpactPoints.Add(Hit.ImpactPoint);
		ImpactNormals.Add(Hit.ImpactNormal);
	}
	
	Multicast_PlayImpactEffect(ImpactPoints, ImpactNormals);
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

float UEPCombatComponent::GetTagDamageMultiplier(const UEPPhysicalMaterial* PM, const UEPWeaponDefinition* WeaponDef)
{
	if (!PM || !WeaponDef) return 1.f;
	
	for (const FGameplayTag& Tag : PM->MaterialTags)
	{
		if (const float* Multiplier = WeaponDef->TagDamageMultiplierMap.Find(Tag))
			return *Multiplier;
	}
	return 1.f;
}
