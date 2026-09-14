# 3단계 구현서: NetPrediction

> 기준 문서: `03_NetPrediction.md`
> 목표: **지금 바로 따라 구현 가능한 순서** 제공
> 범위: Hit Validation + Lag Compensation + Reconciliation + 투사체 지원
> 비범위: 헤드샷/약점 배율은 BoneHitbox에서 이미 구현됨
>
> **현재 코드 상태 기준 (모든 Step 완료)**
> - ✅ = 구현 완료

---

## 0. 구현 철학

1. 서버 권한 판정 — 클라이언트는 방향만 보내고 서버가 직접 LineTrace + 리와인드 (Valorant 방식)
2. 클라 체감 예측 — 반동·총구 이펙트는 즉시 로컬 재생, 서버 판정 결과로 히트마커 표시
3. 판정 시점 보정 — HitboxHistory 리와인드로 클라 발사 시점 기준 판정 보장
4. 탄도 분기 — `EEPBallisticType`으로 Hitscan / ProjectileFast / ProjectileSlow 분리
5. GAS 연결 예약 — Damage Block은 `ApplyPointDamage` 래퍼로 고립, GAS 단계에서 GE로 교체

이번 단계 핵심: **"얼마나 아픈가"가 아닌 "맞았는가 + 어떤 방식으로"**

---

## 1. 수정/추가 대상 파일

**수정:**
- `Public/Types/EPTypes.h` — `EEPBallisticType` 추가 ✅
- `Public/Data/EPWeaponDefinition.h` — `BallisticType`, `ProjectileClass`, `PelletCount`, `SpreadDistributionCurve` ✅
- `Public/Combat/EPCombatComponent.h` — `HandleProjectileFire`, `SpawnLocalCosmeticProjectile` ✅
- `Private/Combat/EPCombatComponent.cpp` — `Server_Fire` 검증 3단계 + switch + ProjectileFire ✅
- `Public/Combat/EPWeapon.h` / `Private/Combat/EPWeapon.cpp` — `Fire` 오버로드 ✅

**신규:**
- `Public/Combat/EPProjectile.h` ✅
- `Private/Combat/EPProjectile.cpp` ✅

**이미 완료 (건너뜀):**
- `EP_TraceChannel_Weapon` 상수 (EPTypes.h) ✅
- `FEPBoneSnapshot` / `FEPHitboxSnapshot` (EPTypes.h) ✅
- `UEPServerSideRewindComponent` — 히스토리 기록, 보간, Broad Phase, ConfirmHitscan ✅
- `HandleHitscanFire` — SSR 위임 패턴 ✅
- `GetBoneMultiplier` / `GetMaterialMultiplier` ✅
- `RequestFire(ClientFireTime)` / `Server_Fire(ClientFireTime)` ✅

---

## Step 0) ✅ WeaponTrace 채널 설정

BoneHitbox Step 0에서 완료. `DefaultEngine.ini`에 `DefaultResponse=ECR_Ignore` 설정됨.

---

## Step 1) ✅ EPTypes.h — EEPBallisticType

파일: `Public/Types/EPTypes.h`

```cpp
UENUM(BlueprintType)
enum class EEPBallisticType : uint8
{
    Hitscan,
    ProjectileFast,
    ProjectileSlow
};
```

> **NetPrediction 문서의 `FEPHitboxSnapshot`(Location + Rotation, 캡슐 기반)은 무시.**
> 현재 구현은 본 단위 배열(`Bones`) 방식이며 이것이 최종 구조다.

---

## Step 2) ✅ EPWeaponDefinition.h — 탄도 필드

파일: `Public/Data/EPWeaponDefinition.h`

```cpp
class AEPProjectile;
class UCurveFloat;

// 탄도
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistic")
EEPBallisticType BallisticType = EEPBallisticType::Hitscan;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistic",
    meta=(EditCondition = "BallisticType != EEPBallisticType::Hitscan"))
TSubclassOf<AEPProjectile> ProjectileClass;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistic",
    meta=(EditCondition="BallisticType == EEPBallisticType::Hitscan", ClampMin = 1))
int32 PelletCount = 1;

// 퍼짐 분포 커브 (Inverse Transform Sampling)
// - 오목 커브: 중앙 집중 (현실적인 샷건 패턴)
// - 직선(y=x): 균등 분포 (VRandCone과 동일)
// - 볼록 커브: 외곽 집중
// null이면 균등 분포로 폴백
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread",
    meta=(EditCondition="BallisticType == EEPBallisticType::Hitscan"))
TObjectPtr<UCurveFloat> SpreadDistributionCurve;

// 트레이스 거리
float TraceDistanceCm = 10000.f;

// 부위별 대미지 (GAS 이후 태그 기반으로 수정)
TMap<FName, float> BoneDamageMultiplierMap;
```

---

## Steps 3~4) ✅ 히트박스 히스토리 기록 + 보간

BoneHitbox에서 `UEPServerSideRewindComponent`에 구현됨.

| NetPrediction 문서 항목 | 실제 위치 |
|---|---|
| `HitboxHistory` TArray | `UEPServerSideRewindComponent` (protected) |
| `SaveHitboxSnapshot()` | CMC `OnMovementUpdated` → pending 플래그 → TG_PostPhysics에서 커밋 |
| `GetSnapshotAtTime()` | `UEPServerSideRewindComponent` (per-bone BlendWith) |

> **NetPrediction 문서의 Step 3(EPCharacter 멤버 추가)과 Step 4(AEPCharacter 구현)는 무시.**
> `SetActorLocationAndRotation` 기반 리와인드도 무시 — `FBodyInstance::SetBodyTransform` 방식 유지.

---

## Step 5) ✅ EPCombatComponent.h

파일: `Public/Combat/EPCombatComponent.h`

**존재하는 항목:**
- `LocalLastFireTime` (protected)
- `RequestFire(Origin, Direction, ClientFireTime)`
- `Server_Fire(Origin, Direction, ClientFireTime)`
- `HandleHitscanFire(Owner, Origin, Directions, ClientFireTime)` (private)
- `HandleProjectileFire(Owner, Origin, Direction)` (private)
- `SpawnLocalCosmeticProjectile(MuzzleLocation, Direction)` (protected)
- `GetBoneMultiplier` / `GetMaterialMultiplier` (private)
- `LastServerFireTime = -999.f` (private)
- `Multicast_SpawnCosmeticProjectile` (protected)

---

## Step 6) ✅ AEPWeapon::Fire — 펠릿 방향 생성

파일: `Public/Combat/EPWeapon.h` + `Private/Combat/EPWeapon.cpp`

**헤더:**
```cpp
void Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets);
```

**구현:**
```cpp
void AEPWeapon::Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets)
{
    if (!HasAuthority()) return;
    if (!WeaponDef) return;

    CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);
    LastFireTime = GetWorld()->GetTimeSeconds();

    CurrentSpread = FMath::Min(CurrentSpread + WeaponDef->SpreadPerShot, WeaponDef->MaxSpread);
    ConsecutiveShots++;
    WeaponState = EEPWeaponState::Firing;

    const int32 Count = FMath::Max(1, WeaponDef->PelletCount);
    OutPellets.Reserve(Count);
    const float HalfAngle = FMath::DegreesToRadians(CalculateSpread() * 0.5f);

    // FindBestAxisVectors: AimDir에 수직인 두 축을 안정적으로 계산
    FVector Up, Right;
    AimDir.FindBestAxisVectors(Up, Right);

    for (int32 i = 0; i < Count; ++i)
    {
        // SpreadDistributionCurve: Inverse Transform Sampling
        // 커브 X(0~1) 입력 → Y(0~1) 편향 출력
        const float R = WeaponDef->SpreadDistributionCurve
            ? WeaponDef->SpreadDistributionCurve->GetFloatValue(FMath::FRand())
            : FMath::FRand();

        // 구면좌표 변환: Theta(반경각), Phi(방위각)
        const float Theta = R * HalfAngle;
        const float Phi   = FMath::FRand() * TWO_PI;
        OutPellets.Add(
            AimDir * FMath::Cos(Theta)
          + Up     * FMath::Sin(Theta) * FMath::Cos(Phi)
          + Right  * FMath::Sin(Theta) * FMath::Sin(Phi)
        );
    }

    if (CurrentAmmo <= 0) StartReload();
}
```

---

## Step 7) ✅ RequestFire

파일: `Private/Combat/EPCombatComponent.cpp`

```cpp
void UEPCombatComponent::RequestFire(const FVector& Origin, const FVector& Direction, float ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;

    if (EquippedWeapon->CurrentAmmo <= 0) return;

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
        float Yaw = FMath::RandRange(-EquippedWeapon->GetRecoilYaw(), EquippedWeapon->GetRecoilYaw());
        Owner->AddControllerPitchInput(-Pitch);
        Owner->AddControllerYawInput(Yaw);
    }

    // ProjectileFast: Multicast 왕복 없이 발사자 측 코스메틱 즉시 스폰
    // Multicast_SpawnCosmeticProjectile의 IsLocallyControlled() 체크와 짝을 이룬다
    if (Owner && Owner->IsLocallyControlled()
        && EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast
        && EquippedWeapon->WeaponDef->ProjectileClass)
    {
        const FVector MuzzleLoc =
            (EquippedWeapon->WeaponMesh && EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket")))
            ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
            : Origin;
        SpawnLocalCosmeticProjectile(MuzzleLoc, Direction);
    }
}
```

**Muzzle 이펙트 흐름:**
1. 발사자 클라이언트: `RequestFire` → `PlayLocalMuzzleEffect` 즉시 재생
2. 서버: `Server_Fire_Implementation` → `Multicast_PlayMuzzleEffect`
3. 다른 클라이언트: `Multicast_PlayMuzzleEffect_Implementation` → `PlayLocalMuzzleEffect`
4. 발사자: `IsLocallyControlled()` 체크로 중복 차단

---

## Step 8) ✅ Server_Fire_Implementation — 검증 3단계 + switch

파일: `Private/Combat/EPCombatComponent.cpp`

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector_NetQuantize&       Origin,
    const FVector_NetQuantizeNormal& Direction,
    float                            ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;

    AEPCharacter* Owner = GetOwnerCharacter();
    if (!Owner) return;

    // 1. FireRate: 클라 RPC 스팸 차단
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ServerNow = GS
        ? GS->GetServerWorldTimeSeconds()
        : GetWorld()->GetTimeSeconds();
    const float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
    if (ServerNow - LastServerFireTime < FireInterval) return;
    LastServerFireTime = ServerNow;

    // 2. 탄약 + 무기 상태
    if (!EquippedWeapon->CanFire()) return;

    // 3. Origin 위치 검증: 벽 너머 조작 방지
    constexpr float MaxOriginDrift = 200.f;
    if (FVector::DistSquared(Origin, Owner->GetActorLocation()) > FMath::Square(MaxOriginDrift))
        return;

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

    const FVector MuzzleLocation =
        (EquippedWeapon->WeaponMesh && EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket")))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();

    Multicast_PlayMuzzleEffect(MuzzleLocation);
}
```

---

## Step 9) ✅ GetHitscanCandidates (Broad Phase)

`UEPServerSideRewindComponent::GetHitscanCandidates`에 구현됨.

---

## Step 10) ✅ HandleHitscanFire

```cpp
void UEPCombatComponent::HandleHitscanFire(
    AEPCharacter* Owner, const FVector& Origin,
    const TArray<FVector>& Directions, float ClientFireTime)
{
    if (!Owner || !Owner->GetServerSideRewindComponent()) return;

    TArray<FHitResult> ConfirmedHits;
    Owner->GetServerSideRewindComponent()->ConfirmHitscan(
        Owner, EquippedWeapon, Origin, Directions, ClientFireTime, ConfirmedHits);

    for (const FHitResult& Hit : ConfirmedHits)
    {
        if (!Hit.GetActor()) continue;

        const float BaseDamage         = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
        const float BoneMultiplier     = GetBoneMultiplier(Hit.BoneName);
        const float MaterialMultiplier = GetMaterialMultiplier(Hit.PhysMaterial.Get());
        const float FinalDamage        = BaseDamage * BoneMultiplier * MaterialMultiplier;

        UGameplayStatics::ApplyPointDamage(
            Hit.GetActor(), FinalDamage,
            (Hit.ImpactPoint - Origin).GetSafeNormal(), Hit,
            Owner->GetController(), Owner, UDamageType::StaticClass());

        Multicast_PlayImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal);
    }
}
```

---

## Step 11) ✅ HandleProjectileFire

```cpp
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
    SpawnParams.Owner      = Owner;
    SpawnParams.Instigator = Owner;

    AEPProjectile* Proj = GetWorld()->SpawnActor<AEPProjectile>(
        EquippedWeapon->WeaponDef->ProjectileClass,
        MuzzleLoc, Direction.GetSafeNormal().Rotation(), SpawnParams);

    if (!Proj) return;

    Proj->Initialize(EquippedWeapon->GetDamage(), Direction);

    // ProjectileFast: 발사자는 RequestFire에서 이미 로컬 스폰 완료.
    //                 Multicast는 SimulatedProxy(다른 클라)만을 위한 것.
    // ProjectileSlow: bReplicates로 복제 처리 → Multicast 불필요.
    if (EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
        Multicast_SpawnCosmeticProjectile(MuzzleLoc, Direction.GetSafeNormal());
}
```

---

## Step 12) ✅ Multicast_SpawnCosmeticProjectile + SpawnLocalCosmeticProjectile

```cpp
void UEPCombatComponent::SpawnLocalCosmeticProjectile(const FVector& MuzzleLocation, const FVector& Direction)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef || !EquippedWeapon->WeaponDef->ProjectileClass) return;

    AEPProjectile* Cosmetic = GetWorld()->SpawnActor<AEPProjectile>(
        EquippedWeapon->WeaponDef->ProjectileClass,
        MuzzleLocation, Direction.GetSafeNormal().Rotation());

    if (Cosmetic)
        Cosmetic->SetCosmeticOnly();
}

void UEPCombatComponent::Multicast_SpawnCosmeticProjectile_Implementation(
    const FVector_NetQuantize&       MuzzleLocation,
    const FVector_NetQuantizeNormal& Direction)
{
    if (GetOwner()->HasAuthority()) return;

    // 발사한 본인은 RequestFire에서 이미 스폰했으므로 스킵
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar && OwnerChar->IsLocallyControlled()) return;

    SpawnLocalCosmeticProjectile(MuzzleLocation, Direction);
}
```

**투사체 타입별 코스메틱 처리 요약:**

| 타입 | 발사 클라이언트 | 다른 클라이언트 | 서버 |
|---|---|---|---|
| ProjectileFast | `RequestFire`에서 즉시 코스메틱 스폰 | `Multicast`로 코스메틱 스폰 | 실제 투사체 (충돌/데미지) |
| ProjectileSlow | 없음 (복제 대기) | `bReplicates`로 자동 복제 | 실제 투사체 (충돌/데미지) |

---

## Step 13) ✅ AEPProjectile

### 13-1. EPProjectile.h

```cpp
UCLASS()
class EMPLOYMENTPROJ_API AEPProjectile : public AActor
{
    GENERATED_BODY()

public:
    AEPProjectile();

    void Initialize(float InDamage, const FVector& InDirection);
    void SetCosmeticOnly();

protected:
    UPROPERTY(VisibleAnywhere, Category = "Projectile")
    TObjectPtr<USphereComponent> CollisionComp;

    UPROPERTY(VisibleAnywhere, Category = "Projectile")
    TObjectPtr<UProjectileMovementComponent> MovementComp;

private:
    float BaseDamage = 0.f;
    FVector LaunchDir = FVector::ForwardVector;
    bool bIsCosmeticOnly = false;

    UFUNCTION()
    void OnProjectileHit(
        UPrimitiveComponent* HitComp,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector NormalImpulse,
        const FHitResult& Hit);
};
```

### 13-2. EPProjectile.cpp

```cpp
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

    // 발사자 본인은 충돌 무시
    if (AActor* MyInstigator = GetInstigator())
        CollisionComp->IgnoreActorWhenMoving(MyInstigator, true);
}

void AEPProjectile::SetCosmeticOnly()
{
    bIsCosmeticOnly = true;
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEPProjectile::OnProjectileHit(...)
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
```

---

## 에디터 작업

| 항목 | 내용 |
|---|---|
| Collision Object Channel | `Projectile` 추가 (Project Settings → Collision) |
| Collision Preset | `Projectile` 프리셋 생성: WorldStatic/WorldDynamic/Pawn → Block, 나머지 → Ignore |
| Collision Trace Channel | `WeaponTrace` 추가 |
| `BP_Projectile_Fast` | CollisionComp 루트 + Niagara 자식 + MovementComp. `bReplicates = false` |
| `BP_Projectile_Slow` | 동일 구조. `bReplicates = true`, `Gravity Scale` 조정 |
| `DA_AK74_FastProj` | `BallisticType = ProjectileFast`, `ProjectileClass = BP_Projectile_Fast` |
| `DA_AK74_SlowProj` | `BallisticType = ProjectileSlow`, `ProjectileClass = BP_Projectile_Slow` |
| `DA_AK74_HitScan` | `BallisticType = Hitscan`, `PelletCount`, `SpreadDistributionCurve` 설정 |

---

## 투사체 Hit Reg 개선 (GAS 이후)

현재는 투사체가 서버에서 물리 충돌로 데미지를 판정한다.
래그 보상 없이 서버 현재 시간 기준으로만 동작한다.

GAS 단계에서 `Predicted Projectile Path` 방식으로 개선 예정:
1. `ClientFireTime` 기반으로 리와인드 구간 LineTrace
2. 이후 정방향 시뮬레이션
3. CMC 업데이트 시점마다 서버 현재 상태 기준 충돌 체크

자세한 내용: `DOCS/Mine/Proj.md`
