// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/EPServerSideRewindComponent.h"

#include "Combat/EPCombatDeveloperSettings.h"
#include "Combat/EPWeapon.h"
#include "Components/CapsuleComponent.h"
#include "Core/EPCharacter.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Movement/EPCharacterMovement.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"

struct FEPRewindEntry
{
	AEPCharacter* Character = nullptr;
	TArray<FEPBoneSnapshot> SavedBones;
};

static void DrawBodyAggGeomPrimitives(
	UWorld* World,
	const FBodyInstance* Body,
	const FColor& Color,
	const float Duration,
	const float Thickness)
{
	if (!World || !Body) return;

	const UBodySetup* BodySetup = Body->GetBodySetup();
	if (!BodySetup) return;

	const FTransform BodyWorld = Body->GetUnrealWorldTransform();
	const FVector Scale3D = BodyWorld.GetScale3D().GetAbs();

	for (const FKSphereElem& Sphere : BodySetup->AggGeom.SphereElems)
	{
		const FVector CenterWS = BodyWorld.TransformPosition(Sphere.Center);
		const float RadiusWS = Sphere.Radius * Scale3D.GetMax();
		DrawDebugSphere(World, CenterWS, RadiusWS, 16, Color, false, Duration, 0, Thickness);
	}

	for (const FKSphylElem& Sphyl : BodySetup->AggGeom.SphylElems)
	{
		const FVector CenterWS = BodyWorld.TransformPosition(Sphyl.Center);
		const FQuat RotWS = BodyWorld.GetRotation() * FQuat(Sphyl.Rotation);

		const float RadiusWS = Sphyl.Radius * FMath::Max(Scale3D.X, Scale3D.Y);
		const float HalfHeightWS = (Sphyl.Length * 0.5f + Sphyl.Radius) * Scale3D.Z;

		DrawDebugCapsule(World, CenterWS, HalfHeightWS, RadiusWS, RotWS, Color, false, Duration, 0, Thickness);
	}

	for (const FKBoxElem& Box : BodySetup->AggGeom.BoxElems)
	{
		const FVector CenterWS = BodyWorld.TransformPosition(Box.Center);
		const FQuat RotWS = BodyWorld.GetRotation() * FQuat(Box.Rotation);
		const FVector ExtentWS = FVector(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f) * Scale3D;

		DrawDebugBox(World, CenterWS, ExtentWS, RotWS, Color, false, Duration, 0, Thickness);
	}
}

static void DrawHitBonesPrimitivesForCharacter(
	UWorld* World,
	const AEPCharacter* Char,
	const TArray<FName>& Bones,
	const FColor& Color,
	const float Duration,
	const float Thickness)
{
	if (!World || !Char || !Char->GetMesh()) return;

	for (const FName& BoneName : Bones)
	{
		const FBodyInstance* Body = Char->GetMesh()->GetBodyInstance(BoneName);
		if (!Body) continue;
		DrawBodyAggGeomPrimitives(World, Body, Color, Duration, Thickness);
	}
}

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
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
}

void UEPServerSideRewindComponent::BeginPlay()
{
	Super::BeginPlay();

	const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();
	const float RewindWindow = FMath::Max(0.05f, CombatSettings->MaxRewindSeconds);
	// ClientNetSendMoveDeltaTime: DefaultGame.ini [/Script/Engine.GameNetworkManager] 에서 설정
	float MoveDeltaTime = 1.f / 60.f;
	GConfig->GetFloat(
		TEXT("/Script/Engine.GameNetworkManager"),
		TEXT("ClientNetSendMoveDeltaTime"),
		MoveDeltaTime,
		GGameIni);
	if (MoveDeltaTime <= 0.f) MoveDeltaTime = 1.f / 60.f;
	MaxHistoryCount = FMath::CeilToInt(RewindWindow / MoveDeltaTime) + 4;

	// CMC의 OnServerMoveProcessed 델리게이트 구독
	if (AEPCharacter* OwnerChar = Cast<AEPCharacter>(GetOwner()))
	{
		if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(OwnerChar->GetCharacterMovement()))
		{
			CMC->OnServerMoveProcessed.AddUObject(this, &UEPServerSideRewindComponent::OnServerMoveProcessed);
		}
	}
}

void UEPServerSideRewindComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AEPCharacter* OwnerChar = Cast<AEPCharacter>(GetOwner());
	if (!OwnerChar) return;

	if (!OwnerChar->HasAuthority()) return;

	// PostPhysics — 본 Transform 갱신 완료. pending이 있으면 스냅샷 커밋.
	if (bHasPendingSnapshot)
	{
		SaveHitboxSnapshot(PendingSnapshotTime, PendingSnapshotLocation);
		bHasPendingSnapshot = false;
	}
}

void UEPServerSideRewindComponent::OnServerMoveProcessed(float Time, FVector Location)
{
	// TickDispatch 시점 — 본 Transform이 아직 갱신되지 않았으므로 값만 보관.
	// 실제 스냅샷 저장은 PostPhysics Tick(본 갱신 완료 후)에서 수행.
	bHasPendingSnapshot = true;
	PendingSnapshotTime = Time;
	PendingSnapshotLocation = Location;
}

void UEPServerSideRewindComponent::SaveHitboxSnapshot(float Time, const FVector& Location)
{
	AEPCharacter* OwnerChar = Cast<AEPCharacter>(GetOwner());
	if (!OwnerChar) return;

	FEPHitboxSnapshot Snapshot;
	Snapshot.ServerTime = Time;
	Snapshot.Location = Location;

	// const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
	// const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	// UE_LOG(LogTemp, Warning, TEXT("[SHS] Now=%.4f Param=%.4f Diff=%.4f"), ServerNow, Time, ServerNow - Time);

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
		UE_LOG(LogTemp, Log, TEXT("Target Time <= Snapshot"));
		return HitboxHistory[0];
	}
	if (TargetTime >= HitboxHistory.Last().ServerTime)
	{
		UE_LOG(LogTemp, Log, TEXT("Target Time >= Snapshot"));
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
		UE_LOG(LogTemp, Log, TEXT("!Before || !After"));
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

	bool bDebugDraw = CombatSettings->bEnableSSRDebugDraw;
	bool bDebugLog = CombatSettings->bEnableSSRDebugLog;
	const float DebugDuration = CombatSettings->SSRDebugDrawDuration;
	const float DebugThickness = CombatSettings->SSRDebugLineThickness;

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bDebugDraw = false;
	bDebugLog = false;
#endif
	
	if (ServerNow - ClientFireTime > CombatSettings->MaxRewindSeconds)
	{
		UE_LOG(LogTemp, Log, TEXT("Exceed MaxRewindSeconds"));
		ClientFireTime = ServerNow;
	}

	const TArray<AEPCharacter*> Candidates =
		GetHitscanCandidates(Shooter, EquippedWeapon, Origin, Directions, ClientFireTime);

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[SSR] ServerNow=%.3f ClientFireTime=%.3f Delta=%.3f Candidates=%d"),
			ServerNow, ClientFireTime, ServerNow - ClientFireTime, Candidates.Num());
	}

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

		if (bDebugLog && Snap.Bones.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[SERVER_REWIND_POS] ClientFireTime=%.3f Actor=%s ServerPos=%s RewindPos=%s"),
				ClientFireTime,
				*Char->GetName(),
				*Char->GetActorLocation().ToString(),
				*Snap.Location.ToString());
		}

		if (bDebugDraw)
		{
			DrawHitBonesPrimitivesForCharacter(GetWorld(), Char, HitBones, FColor::Blue, DebugDuration, DebugThickness);
		}

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

		if (bDebugDraw)
		{
			DrawHitBonesPrimitivesForCharacter(GetWorld(), Char, HitBones, FColor::Red, DebugDuration, DebugThickness);
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

		if (bDebugDraw)
		{
			DrawDebugLine(GetWorld(), Origin, End, FColor::White, false, DebugDuration, 0, DebugThickness);
		}

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, EP_TraceChannel_Weapon, Params))
		{
			AEPCharacter* HitChar = Cast<AEPCharacter>(Hit.GetActor());
			if (HitChar && CandidateSet.Contains(HitChar))
			{
				OutConfirmedHits.Add(Hit);

				// if (bDebugDraw)
				// {
					DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 12, FColor::Yellow, false, DebugDuration, 0, DebugThickness);
				// }

				if (bDebugLog)
				{
					UE_LOG(LogTemp, Log, TEXT("[SSR] Hit Actor=%s Bone=%s PM=%s"),
						*GetNameSafe(Hit.GetActor()),
						*Hit.BoneName.ToString(),
						Hit.PhysMaterial.IsValid() ? *Hit.PhysMaterial->GetName() : TEXT("None"));
				}
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

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[SSR] ConfirmedHits=%d"), OutConfirmedHits.Num());
	}

	return OutConfirmedHits.Num() > 0;
}
