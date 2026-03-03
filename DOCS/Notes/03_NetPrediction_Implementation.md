# 3단계 구현서: NetPrediction (현재 코드 기준)

> 기준 문서: `03_NetPrediction.md`
> 목표: **지금 바로 따라 구현 가능한 순서** 제공
> 범위: Hit Validation + Lag Compensation + Reconciliation + 투사체 지원
> 비범위: 헤드샷/약점 배율 **구현하지 않음** (Chapter 4 BoneHitbox 단계)

---

## 0. 구현 철학 (이번 단계)

1. 서버 권한 판정은 유지한다.
2. 클라 체감은 예측으로 보완한다.
3. 판정 시점 불일치는 리와인드로 보정한다.
4. 히트스캔/투사체를 `EEPBallisticType`으로 분리해 스킬 재사용 구조를 미리 잡는다.
5. 데미지 모델(헤드샷/재질 태그/GAS)은 다음 단계로 미룬다.

즉, 이번 단계의 핵심은 "얼마나 아픈가"가 아니라 **"맞았는가 + 어떤 방식으로"** 다.

---

## 1. 수정/추가 대상 파일

**수정:**
- `Public/Types/EPTypes.h`
- `Public/Data/EPWeaponDefinition.h`
- `Public/Core/EPCharacter.h`
- `Private/Core/EPCharacter.cpp`
- `Public/Combat/EPCombatComponent.h`
- `Private/Combat/EPCombatComponent.cpp`

**신규:**
- `Public/Combat/EPProjectile.h`
- `Private/Combat/EPProjectile.cpp`

---

## 2. Step-by-Step

---

## Step 1) EPTypes.h — 스냅샷 타입 + 탄도 enum 추가

파일: `Public/Types/EPTypes.h`

기존 enum 블록 아래에 추가한다.

```cpp
// 탄도 방식 — EEPFireMode(트리거: Single/Burst/Auto)와 별개
UENUM(BlueprintType)
enum class EEPBallisticType : uint8
{
	Hitscan,    // 즉발 LineTrace (라이플, SMG)
	Projectile, // 탄속 Actor   (저격총, 유탄)
};

// 히트박스 히스토리 스냅샷 (캡슐 기준, 본 단위 확장은 03_BoneHitbox 참고)
USTRUCT()
struct FEPHitboxSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	float ServerTime = 0.f;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;
};
```

---

## Step 2) EPWeaponDefinition.h — 탄도 필드 추가

파일: `Public/Data/EPWeaponDefinition.h`

```cpp
// include 추가 (EEPBallisticType 사용)
#include "Types/EPTypes.h"

// forward declare (헤더 상단)
class AEPProjectile;
```

기존 멤버 아래 `"Weapon|Ballistics"` 카테고리 추가:

```cpp
UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ballistics")
EEPBallisticType BallisticType = EEPBallisticType::Hitscan;

// BallisticType == Projectile일 때만 사용
// 탄속은 여기에 두지 않는다 — ProjectileClass Blueprint의
// UProjectileMovementComponent::InitialSpeed에서 직접 설정한다.
UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ballistics",
	meta = (EditCondition = "BallisticType == EEPBallisticType::Projectile"))
TSubclassOf<AEPProjectile> ProjectileClass;
```

---

## Step 3) EPCharacter.h — 히스토리 버퍼 선언

파일: `Public/Core/EPCharacter.h`

### 3-1. include

```cpp
#include "Types/EPTypes.h"
```

### 3-2. 멤버/함수 추가

```cpp
protected:
	virtual void Tick(float DeltaTime) override;

private:
	static constexpr int32 MaxHitboxHistory = 20; // 100ms × 20 = 약 2초

	UPROPERTY()
	TArray<FEPHitboxSnapshot> HitboxHistory;

	int32 HistoryIndex = 0;
	float HistoryTimer = 0.f;

	void SaveHitboxSnapshot();

public:
	FEPHitboxSnapshot GetSnapshotAtTime(float TargetTime) const;
```

주의:
- `UPROPERTY()`는 GC 안정성과 디버깅 가독성 때문에 권장한다.
- 히스토리는 서버 전용(`HasAuthority()`)으로만 기록한다.

---

## Step 4) EPCharacter.cpp — 기록/보간 구현

파일: `Private/Core/EPCharacter.cpp`

### 4-1. include 추가

```cpp
#include "GameFramework/GameStateBase.h"
```

### 4-2. 생성자에서 Tick 보장

```cpp
PrimaryActorTick.bCanEverTick = true;
```

### 4-3. Tick 구현

```cpp
void AEPCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority()) return; // 서버만 기록

	HistoryTimer += DeltaTime;
	if (HistoryTimer >= 0.1f)   // 100ms 간격
	{
		HistoryTimer = 0.f;
		SaveHitboxSnapshot();
	}
}
```

### 4-4. SaveHitboxSnapshot 구현

```cpp
void AEPCharacter::SaveHitboxSnapshot()
{
	FEPHitboxSnapshot Snapshot;

	// GetServerWorldTimeSeconds(): GameState가 복제하는 서버 기준 시간.
	// ClientFireTime과 동일 기준을 써야 리와인드 시각이 정확하다.
	const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
	Snapshot.ServerTime = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	Snapshot.Location   = GetActorLocation();
	Snapshot.Rotation   = GetActorRotation();

	if (HitboxHistory.Num() < MaxHitboxHistory)
	{
		HitboxHistory.Add(Snapshot);
	}
	else
	{
		HitboxHistory[HistoryIndex] = Snapshot;
		HistoryIndex = (HistoryIndex + 1) % MaxHitboxHistory;
	}
}
```

### 4-5. GetSnapshotAtTime 구현 (보간)

```cpp
FEPHitboxSnapshot AEPCharacter::GetSnapshotAtTime(float TargetTime) const
{
	const FEPHitboxSnapshot* Before = nullptr;
	const FEPHitboxSnapshot* After  = nullptr;

	for (const FEPHitboxSnapshot& Snap : HitboxHistory)
	{
		if (Snap.ServerTime <= TargetTime)
		{
			if (!Before || Snap.ServerTime > Before->ServerTime)
				Before = &Snap;
		}
		if (Snap.ServerTime >= TargetTime)
		{
			if (!After || Snap.ServerTime < After->ServerTime)
				After = &Snap;
		}
	}

	if (!Before && !After)
	{
		// 히스토리 없으면 현재 위치 반환
		FEPHitboxSnapshot Current;
		Current.ServerTime = TargetTime;
		Current.Location   = GetActorLocation();
		Current.Rotation   = GetActorRotation();
		return Current;
	}
	if (!Before) return *After;
	if (!After)  return *Before;
	if (Before == After) return *Before;

	const float Range = After->ServerTime - Before->ServerTime;
	// KINDA_SMALL_NUMBER guard: 두 스냅샷 시각이 같으면 나눗셈 생략
	const float Alpha = Range > KINDA_SMALL_NUMBER
		? FMath::Clamp((TargetTime - Before->ServerTime) / Range, 0.f, 1.f)
		: 0.f;

	FEPHitboxSnapshot Result;
	Result.ServerTime = TargetTime;
	Result.Location   = FMath::Lerp(Before->Location, After->Location, Alpha);
	// FMath::Lerp는 ±179° 랩어라운드 문제 → FQuat::Slerp 사용
	Result.Rotation = FQuat::Slerp(
		Before->Rotation.Quaternion(),
		After->Rotation.Quaternion(),
		Alpha).Rotator();

	return Result;
}
```

---

## Step 5) EPCombatComponent.h — 시그니처 + 핸들러 선언

파일: `Public/Combat/EPCombatComponent.h`

### 5-1. forward declare 추가

```cpp
class AEPProjectile;
```

### 5-2. RPC 시그니처 변경

기존:
```cpp
UFUNCTION(Server, Reliable)
void Server_Fire(const FVector& Origin, const FVector& Direction);
```

변경:
```cpp
UFUNCTION(Server, Reliable)
void Server_Fire(const FVector_NetQuantize& Origin,
                 const FVector_NetQuantizeNormal& Direction,
                 float ClientFireTime);
```

### 5-3. 핸들러 함수 선언 추가 (private)

```cpp
private:
	// 히트스캔: 리와인드 + 레이캐스트 판정
	void HandleHitscanFire(AEPCharacter* Owner,
	                       const FVector& Origin,
	                       const FVector& Direction,
	                       float ClientFireTime);

	// 투사체: 서버에서 Actor 스폰
	void HandleProjectileFire(AEPCharacter* Owner,
	                          const FVector& Origin,
	                          const FVector& Direction);
```

주의:
- 현재는 `private`으로 선언한다 (Chapter 3 범위에서는 CombatComponent 내부에서만 호출).
- Chapter 4에서 GAS 어빌리티가 히트스캔 판정이 필요해지면 `protected`로 변경하거나,
  별도 public 래퍼(`RequestHitscanTrace`)를 추가한다.

---

## Step 6) RequestFire — ClientFireTime 전달

파일: `Private/Combat/EPCombatComponent.cpp`

### 6-1. include 추가

```cpp
#include "GameFramework/GameStateBase.h"
```

### 6-2. RequestFire 수정

```cpp
void UEPCombatComponent::RequestFire(const FVector& Origin, const FVector& Direction)
{
	if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;
	if (EquippedWeapon->CurrentAmmo <= 0) return;

	AEPCharacter* Owner = GetOwnerCharacter();

	const float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
	const float CurrentTime  = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LocalLastFireTime < FireInterval) return;
	LocalLastFireTime = CurrentTime;

	// 서버/클라 모두 GetServerWorldTimeSeconds()로 동일 기준 확보
	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGameStateBase>() : nullptr;
	const float ClientFireTime = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();

	Server_Fire(Origin, Direction.GetSafeNormal(), ClientFireTime);

	if (Owner && Owner->IsLocallyControlled())
	{
		Owner->AddControllerPitchInput(-EquippedWeapon->GetRecoilPitch());
		Owner->AddControllerYawInput(FMath::RandRange(
			-EquippedWeapon->GetRecoilYaw(),
			 EquippedWeapon->GetRecoilYaw()));
	}
}
```

---

## Step 7) Server_Fire — 탄도 방식 분기

파일: `Private/Combat/EPCombatComponent.cpp`

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
	const FVector_NetQuantize& Origin,
	const FVector_NetQuantizeNormal& Direction,
	float ClientFireTime)
{
	if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;
	if (!EquippedWeapon->WeaponDef) return; // WeaponDef 없으면 탄도 방식 판별 불가

	AEPCharacter* Owner = GetOwnerCharacter();
	if (!Owner) return;

	// Spread 적용 + 탄약 차감 (AEPWeapon 내부 처리)
	FVector SpreadDir = Direction;
	EquippedWeapon->Fire(SpreadDir);

	if (EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::Hitscan)
		HandleHitscanFire(Owner, Origin, SpreadDir, ClientFireTime);
	else
		HandleProjectileFire(Owner, Origin, SpreadDir);
}
```

---

## Step 8) HandleHitscanFire — 리와인드 판정

파일: `Private/Combat/EPCombatComponent.cpp`

흐름: 윈도우 클램프 → PreTrace → 리와인드 → ReTrace → 복구 → 데미지 → 이펙트

```cpp
void UEPCombatComponent::HandleHitscanFire(
	AEPCharacter* Owner,
	const FVector& Origin,
	const FVector& Direction,
	float ClientFireTime)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HitscanFire), false);
	Params.AddIgnoredActor(Owner);
	Params.AddIgnoredActor(EquippedWeapon);

	const FVector End = Origin + Direction * 10000.f;

	// ── [Rewind Block] ────────────────────────────────────────────────────────
	// 무기/스킬 공통 인프라 — Chapter 4 GAS 어빌리티도 이 블록을 동일하게 사용한다.

	// 0. 리와인드 윈도우 클램프 (200ms 초과 = 조작으로 간주)
	const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	if (ServerNow - ClientFireTime > 0.2f)
		ClientFireTime = ServerNow;

	// 1. PreTrace — 후보 캐릭터 1명 특정 (O(N) 전체 순회 방지)
	FHitResult PreHit;
	GetWorld()->LineTraceSingleByChannel(PreHit, Origin, End, ECC_GameTraceChannel1, Params);

	AEPCharacter* HitChar = Cast<AEPCharacter>(PreHit.GetActor());
	bool bConfirmedHit = false;
	FHitResult FinalHit = PreHit;

	if (HitChar)
	{
		// 2. 해당 캐릭터만 리와인드
		const FVector   OrigLoc = HitChar->GetActorLocation();
		const FRotator  OrigRot = HitChar->GetActorRotation();
		const FEPHitboxSnapshot Snap = HitChar->GetSnapshotAtTime(ClientFireTime);

		// TeleportPhysics: 물리/콜리전 이벤트 없이 즉시 이동
		HitChar->SetActorLocationAndRotation(
			Snap.Location, Snap.Rotation, false, nullptr, ETeleportType::TeleportPhysics);

		// 3. ReTrace — 리와인드 위치에서 재확인
		FHitResult RewindHit;
		bConfirmedHit = GetWorld()->LineTraceSingleByChannel(
			RewindHit, Origin, End, ECC_GameTraceChannel1, Params)
			&& RewindHit.GetActor() == HitChar;

		if (bConfirmedHit) FinalHit = RewindHit;

		// 4. 복구 (반드시 리와인드 직후, 순서 절대 변경 금지)
		HitChar->SetActorLocationAndRotation(
			OrigLoc, OrigRot, false, nullptr, ETeleportType::TeleportPhysics);
	}
	// 주의: else 브랜치로 비캐릭터(벽 등) 히트 처리를 하지 않는다.
	// ECC_GameTraceChannel1은 캐릭터 히트박스 전용 채널이며,
	// 환경 피격은 별도 채널/로직으로 처리한다.

	// ── [Damage Block] ────────────────────────────────────────────────────────
	// Chapter 4 GAS 전환 시 이 블록을 GameplayEffectSpec + SetByCaller로 교체한다.
	if (bConfirmedHit && FinalHit.GetActor())
	{
		UGameplayStatics::ApplyPointDamage(
			FinalHit.GetActor(),
			EquippedWeapon->GetDamage(),
			Direction,
			FinalHit,                    // BoneName → 부위 배율 (03_BoneHitbox 단계)
			Owner->GetController(),
			Owner,
			UDamageType::StaticClass());
	}

	// ── [Effect Block] ────────────────────────────────────────────────────────
	const FVector MuzzleLoc =
		EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
		? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
		: EquippedWeapon->GetActorLocation();

	Multicast_PlayMuzzleEffect(MuzzleLoc);
	if (bConfirmedHit)
		Multicast_PlayImpactEffect(FinalHit.ImpactPoint, FinalHit.ImpactNormal);
}
```

---

## Step 9) HandleProjectileFire — 투사체 스폰

파일: `Private/Combat/EPCombatComponent.cpp`

### 9-1. include 추가

```cpp
#include "Combat/EPProjectile.h"
```

### 9-2. 구현

```cpp
void UEPCombatComponent::HandleProjectileFire(
	AEPCharacter* Owner,
	const FVector& Origin,
	const FVector& Direction)
{
	// WeaponDef null 체크 필수 (ProjectileClass는 EditCondition으로 숨겨져 있어도 null 가능)
	if (!EquippedWeapon->WeaponDef || !EquippedWeapon->WeaponDef->ProjectileClass) return;

	const FVector MuzzleLoc =
		EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
		? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
		: Origin;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner      = Owner;
	SpawnParams.Instigator = Owner;

	AEPProjectile* Proj = GetWorld()->SpawnActor<AEPProjectile>(
		EquippedWeapon->WeaponDef->ProjectileClass,
		MuzzleLoc,
		Direction.GetSafeNormal().Rotation(), // Fire() 후 정규화 보장 안 되므로 명시적 정규화
		SpawnParams);

	if (!Proj) return; // 스폰 실패 시 이펙트도 재생하지 않는다

	// 탄속은 Proj Blueprint의 ProjectileMovementComponent에서 설정됨
	Proj->Initialize(EquippedWeapon->GetDamage(), Direction);

	// 투사체 탄착 이펙트는 AEPProjectile::OnProjectileHit에서 처리
	Multicast_PlayMuzzleEffect(MuzzleLoc);
}
```

---

## Step 10) AEPProjectile — 신규 파일 추가

### 10-1. EPProjectile.h

파일: `Public/Combat/EPProjectile.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EPProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class EMPLOYMENTPROJ_API AEPProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEPProjectile();

	// Owner에서 호출 — 데미지와 발사 방향을 주입
	void Initialize(float InDamage, const FVector& InDirection);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> MovementComp;

private:
	float BaseDamage    = 0.f;
	FVector LaunchDir   = FVector::ForwardVector; // OnProjectileHit 방향 기준

	UFUNCTION()
	void OnProjectileHit(
		UPrimitiveComponent* HitComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);
};
```

### 10-2. EPProjectile.cpp

파일: `Private/Combat/EPProjectile.cpp`

```cpp
#include "Combat/EPProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AEPProjectile::AEPProjectile()
{
	bReplicates = true; // 클라이언트도 투사체를 시각적으로 본다

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(5.f);
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	// OnComponentHit 발동 조건: bSimulationGeneratesHitEvents = true 필수.
	// "Projectile" 프로파일이 이를 포함하지 않는다면 아래 줄을 명시적으로 추가한다.
	CollisionComp->SetNotifyRigidBodyCollision(true);
	CollisionComp->OnComponentHit.AddDynamic(this, &AEPProjectile::OnProjectileHit);
	SetRootComponent(CollisionComp);

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	MovementComp->bRotationFollowsVelocity = true;
	// InitialSpeed/MaxSpeed는 각 Blueprint에서 설정한다 (무기별 탄속 분리)
}

void AEPProjectile::Initialize(float InDamage, const FVector& InDirection)
{
	BaseDamage = InDamage;
	LaunchDir  = InDirection.GetSafeNormal();
}

void AEPProjectile::OnProjectileHit(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	FVector,
	const FHitResult& Hit)
{
	if (!HasAuthority()) return; // 서버만 데미지 처리

	// 리와인드 불필요 — 투사체 이동 시간이 지연을 흡수한다
	// [Chapter 4 GAS 전환 포인트] ApplyPointDamage → GameplayEffectSpec
	if (OtherActor)
	{
		UGameplayStatics::ApplyPointDamage(
			OtherActor,
			BaseDamage,
			LaunchDir,            // Initialize에서 저장한 발사 방향 사용
			Hit,
			GetInstigatorController(),
			GetInstigator(),
			UDamageType::StaticClass());
	}

	Destroy();
}
```

---

## Step 11) Reconciliation — 히트 확인 피드백

현재 구조에서 발사 VFX/SFX는 Multicast로만 전달된다.
클라 체감 개선을 위해 아래 원칙을 유지한다.

- 로컬 플레이어: 클릭 즉시 반동/로컬 발사감 (이미 구현됨, RequestFire에서 처리)
- 서버 확정 히트: 히트마커/데미지 숫자는 서버 확정 신호로만 표시

최소 구현:
- 기존 `Client_PlayHitConfirmSound`(또는 `Client_OnHitConfirmed`)를
  `HandleHitscanFire`의 `bConfirmedHit == true` 분기에서만 호출한다.
- 투사체는 `OnProjectileHit` 서버 처리 후 동일 방식으로 클라에 통보한다.

---

## 3. 테스트 절차 (반드시 멀티로)

### PIE 설정
- Dedicated Server ON
- Client 2~3명

### 히트스캔 테스트 케이스
| 케이스 | 기대 결과 |
| --- | --- |
| 낮은 핑 | 기존과 동일하게 맞음 |
| 높은 핑 (네트워크 에뮬) | 조준 시점 기준 히트 확인 |
| `MaxLagCompWindow` 초과 | 과도한 과거 판정 차단 |

### 투사체 테스트 케이스
| 케이스 | 기대 결과 |
| --- | --- |
| 서버에서 스폰 확인 | bReplicates로 클라에도 보임 |
| 충돌 시 데미지 | HasAuthority() 조건 충족 시만 |
| 스폰 실패 | 이펙트 미재생 확인 |

### 확인 로그 (UE_LOG 추가 권장)
```cpp
UE_LOG(LogTemp, Log, TEXT("[HitscanFire] ServerNow=%.3f ClientFireTime=%.3f Delta=%.3f"),
	ServerNow, ClientFireTime, ServerNow - ClientFireTime);
UE_LOG(LogTemp, Log, TEXT("[HitscanFire] PreHit=%s RewindHit=%s Confirmed=%d"),
	*GetNameSafe(PreHit.GetActor()),
	bConfirmedHit ? *GetNameSafe(FinalHit.GetActor()) : TEXT("none"),
	bConfirmedHit);
```

---

## 4. 트러블슈팅

**1. 캐릭터가 안 맞는 경우**
- Physics Asset에서 각 히트 바디가 `ECC_GameTraceChannel1 = Block`인지 확인
- `Params.AddIgnoredActor`에 대상이 잘못 들어가 있는지 확인

**2. 리와인드 후 위치가 틀어지는 경우**
- 복구 코드 누락 확인 (SetActorLocationAndRotation 두 번 호출 모두 있는지)
- `ETeleportType::TeleportPhysics` 사용 여부 확인

**3. 투사체가 클라에서 안 보이는 경우**
- `bReplicates = true` 생성자에 있는지 확인
- `SpawnActor`가 서버에서만 호출되는지 확인 (`Server_Fire_Implementation`)

**4. 히트는 되는데 체감이 이상한 경우**
- `FinalHit` vs `PreHit` 혼용 여부 확인 (임팩트 이펙트 기준을 FinalHit으로 통일)
- `ClientFireTime`이 올바른 기준(GetServerWorldTimeSeconds)으로 전송되는지 확인

**5. 같은 시각 스냅샷 두 개로 Alpha 계산이 틀어지는 경우**
- `KINDA_SMALL_NUMBER` guard가 `GetSnapshotAtTime`에 있는지 확인

---

## 5. 다음 단계 연결

이 단계 완료 후 Chapter 4(GAS)에서 교체할 부분:

| 현재 | Chapter 4 이후 |
| --- | --- |
| `ApplyPointDamage` (HandleHitscanFire Damage Block) | `GameplayEffectSpec` + `SetByCaller` |
| `ApplyPointDamage` (AEPProjectile::OnProjectileHit) | 동일 |
| `UDamageType::StaticClass()` | 커스텀 `UEPDamageType` |

지금 만든 Rewind Block과 투사체 스폰 흐름은 **그대로 유지**된다.
데미지 계산기만 교체하면 된다.
