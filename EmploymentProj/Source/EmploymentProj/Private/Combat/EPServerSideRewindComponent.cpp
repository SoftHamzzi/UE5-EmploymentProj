// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/EPServerSideRewindComponent.h"

#include "Combat/EPCombatDeveloperSettings.h"
#include "Combat/EPWeapon.h"
#include "Components/CapsuleComponent.h"
#include "Core/EPCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "PhysicsEngine/BodyInstance.h"

struct FEPRewindEntry
{
	AEPCharacter* Character = nullptr;
	TArray<FEPBoneSnapshot> SavedBones;
};

const TArray<FName> UEPServerSideRewindComponent::HitBones =
{
	TEXT("head"),
	TEXT("neck_01"),
	TEXT("pelvis"),
	TEXT("spine_04"), TEXT("spine_02"),
	TEXT("clavicle_l"), TEXT("clavicle_r"),
	TEXT("upperarm_l"), TEXT("upperarm_r"),
	TEXT("lowerarm_l"), TEXT("lowerarm_r"),
	TEXT("hand_l"), TEXT("hand_r"),
	TEXT("thigh_l"), TEXT("thigh_r"),
	TEXT("calf_l"), TEXT("calf_r"),
	TEXT("foot_l"), TEXT("foot_r")
};

UEPServerSideRewindComponent::UEPServerSideRewindComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);
}

void UEPServerSideRewindComponent::BeginPlay()
{
	Super::BeginPlay();

	const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();
	const float Interval = FMath::Max(0.01f, CombatSettings->SnapshotIntervalSeconds);
	const float RewindWindow = FMath::Max(0.05f, CombatSettings->MaxRewindSeconds);
	MaxHistoryCount = FMath::CeilToInt(RewindWindow / Interval) + 2;
}

void UEPServerSideRewindComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AEPCharacter* OwnerChar = Cast<AEPCharacter>(GetOwner());
	if (!OwnerChar || !OwnerChar->HasAuthority()) return;

	const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();
	SnapshotAccumulator += DeltaTime;
	if (SnapshotAccumulator < CombatSettings->SnapshotIntervalSeconds) return;

	SnapshotAccumulator = 0.f;
	SaveHitboxSnapshot();
}

void UEPServerSideRewindComponent::SaveHitboxSnapshot()
{
	AEPCharacter* OwnerChar = Cast<AEPCharacter>(GetOwner());
	if (!OwnerChar) return;

	const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();

	FEPHitboxSnapshot Snapshot;
	Snapshot.ServerTime = ServerNow;
	Snapshot.Location = OwnerChar->GetActorLocation();

	for (const FName& BoneName : HitBones)
	{
		const int32 BoneIndex = OwnerChar->GetMesh()->GetBoneIndex(BoneName);
		if (BoneIndex == INDEX_NONE) continue;

		FEPBoneSnapshot Bone;
		Bone.BoneName = BoneName;
		Bone.WorldTransform = OwnerChar->GetMesh()->GetBoneTransform(BoneIndex);
		Snapshot.Bones.Add(Bone);
	}

	if (HitboxHistory.Num() >= MaxHistoryCount)
	{
		HitboxHistory.RemoveAt(0);
	}
	HitboxHistory.Add(Snapshot);
}

FEPHitboxSnapshot UEPServerSideRewindComponent::GetSnapshotAtTime(float TargetTime) const
{
	if (HitboxHistory.IsEmpty())
	{
		return FEPHitboxSnapshot{};
	}

	if (TargetTime <= HitboxHistory[0].ServerTime)
	{
		return HitboxHistory[0];
	}
	if (TargetTime >= HitboxHistory.Last().ServerTime)
	{
		return HitboxHistory.Last();
	}

	const FEPHitboxSnapshot* Before = nullptr;
	const FEPHitboxSnapshot* After = nullptr;

	for (int32 i = 0; i < HitboxHistory.Num() - 1; ++i)
	{
		if (HitboxHistory[i].ServerTime <= TargetTime &&
			HitboxHistory[i + 1].ServerTime >= TargetTime)
		{
			Before = &HitboxHistory[i];
			After = &HitboxHistory[i + 1];
			break;
		}
	}

	if (!Before || !After)
	{
		return HitboxHistory.Last();
	}

	const float Denom = After->ServerTime - Before->ServerTime;
	const float Alpha = (Denom > KINDA_SMALL_NUMBER)
		? FMath::Clamp((TargetTime - Before->ServerTime) / Denom, 0.f, 1.f)
		: 0.f;

	FEPHitboxSnapshot Result;
	Result.ServerTime = TargetTime;
	Result.Location = FMath::Lerp(Before->Location, After->Location, Alpha);

	const int32 BoneCount = FMath::Min(Before->Bones.Num(), After->Bones.Num());
	Result.Bones.Reserve(BoneCount);
	for (int32 i = 0; i < BoneCount; ++i)
	{
		FEPBoneSnapshot BoneResult;
		BoneResult.BoneName = Before->Bones[i].BoneName;
		BoneResult.WorldTransform = Before->Bones[i].WorldTransform;
		BoneResult.WorldTransform.BlendWith(After->Bones[i].WorldTransform, Alpha);
		Result.Bones.Add(BoneResult);
	}

	return Result;
}

TArray<AEPCharacter*> UEPServerSideRewindComponent::GetHitscanCandidates(
	AEPCharacter* Shooter,
	AEPWeapon* EquippedWeapon,
	const FVector& Origin,
	const TArray<FVector>& Directions,
	float ClientFireTime) const
{
	TArray<AEPCharacter*> Candidates;
	const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();

	for (TActorIterator<AEPCharacter> It(GetWorld()); It; ++It)
	{
		AEPCharacter* Char = *It;
		if (!Char || Char == Shooter) continue;
		if (Char->IsDead()) continue;

		const UEPServerSideRewindComponent* TargetSSR = Char->GetServerSideRewindComponent();
		if (!TargetSSR) continue;

		const FEPHitboxSnapshot Snap = TargetSSR->GetSnapshotAtTime(ClientFireTime);
		const float CapsuleRadius = Char->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float BroadRadius = CapsuleRadius + CombatSettings->BroadPhasePaddingCm;

		for (const FVector& Dir : Directions)
		{
			const float TraceDistanceCm = EquippedWeapon && EquippedWeapon->WeaponDef
				? EquippedWeapon->WeaponDef->TraceDistanceCm
				: CombatSettings->DefaultTraceDistanceCm;
			const FVector End = Origin + Dir * TraceDistanceCm;

			if (FMath::PointDistToSegment(Snap.Location, Origin, End) <= BroadRadius)
			{
				Candidates.Add(Char);
				break;
			}
		}
	}

	return Candidates;
}

bool UEPServerSideRewindComponent::ConfirmHitscan(
	AEPCharacter* Shooter,
	AEPWeapon* EquippedWeapon,
	const FVector& Origin,
	const TArray<FVector>& Directions,
	float ClientFireTime,
	TArray<FHitResult>& OutConfirmedHits)
{
	OutConfirmedHits.Reset();

	if (!Shooter || !EquippedWeapon) return false;
	if (!GetWorld()) return false;

	const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
	const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();
	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();

	if (ServerNow - ClientFireTime > CombatSettings->MaxRewindSeconds)
	{
		ClientFireTime = ServerNow;
	}

	const TArray<AEPCharacter*> Candidates =
		GetHitscanCandidates(Shooter, EquippedWeapon, Origin, Directions, ClientFireTime);

	TSet<TObjectPtr<AEPCharacter>> CandidateSet;
	CandidateSet.Reserve(Candidates.Num());
	for (AEPCharacter* Char : Candidates)
	{
		CandidateSet.Add(Char);
	}

	TArray<FEPRewindEntry> RewindEntries;
	RewindEntries.Reserve(Candidates.Num());

	for (AEPCharacter* Char : Candidates)
	{
		UEPServerSideRewindComponent* TargetSSR = Char->GetServerSideRewindComponent();
		if (!TargetSSR) continue;

		FEPRewindEntry& Entry = RewindEntries.AddDefaulted_GetRef();
		Entry.Character = Char;
		const FEPHitboxSnapshot Snap = TargetSSR->GetSnapshotAtTime(ClientFireTime);

		for (const FEPBoneSnapshot& Bone : Snap.Bones)
		{
			FBodyInstance* Body = Char->GetMesh()->GetBodyInstance(Bone.BoneName);
			if (!Body) continue;

			FEPBoneSnapshot Saved;
			Saved.BoneName = Bone.BoneName;
			Saved.WorldTransform = Body->GetUnrealWorldTransform();
			Entry.SavedBones.Add(Saved);

			Body->SetBodyTransform(Bone.WorldTransform, ETeleportType::TeleportPhysics);
		}
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(HitscanFire), false);
	Params.AddIgnoredActor(Shooter);
	Params.AddIgnoredActor(EquippedWeapon);
	Params.bReturnPhysicalMaterial = true;

	for (const FVector& Dir : Directions)
	{
		const float TraceDistanceCm = EquippedWeapon->WeaponDef
			? EquippedWeapon->WeaponDef->TraceDistanceCm
			: CombatSettings->DefaultTraceDistanceCm;
		const FVector End = Origin + Dir * TraceDistanceCm;

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, EP_TraceChannel_Weapon, Params))
		{
			AEPCharacter* HitChar = Cast<AEPCharacter>(Hit.GetActor());
			if (HitChar && CandidateSet.Contains(HitChar))
			{
				OutConfirmedHits.Add(Hit);
			}
		}
	}

	for (const FEPRewindEntry& Entry : RewindEntries)
	{
		if (!Entry.Character) continue;
		for (const FEPBoneSnapshot& Saved : Entry.SavedBones)
		{
			FBodyInstance* Body = Entry.Character->GetMesh()->GetBodyInstance(Saved.BoneName);
			if (Body)
			{
				Body->SetBodyTransform(Saved.WorldTransform, ETeleportType::TeleportPhysics);
			}
		}
	}

	return OutConfirmedHits.Num() > 0;
}
