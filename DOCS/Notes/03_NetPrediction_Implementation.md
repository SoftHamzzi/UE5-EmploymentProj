# 3단계 구현서: NetPrediction

> 기준 문서: `03_NetPrediction.md`
> 목표: **지금 바로 따라 구현 가능한 순서** 제공
> 범위: Hit Validation + Lag Compensation + Reconciliation + 투사체 지원
> 비범위: 헤드샷/약점 배율은 BoneHitbox에서 이미 구현됨
>
> **BoneHitbox 완료 후 업데이트 기준**
> - ✅ = BoneHitbox에서 이미 구현 완료 — 건너뜀
> - 나머지 = 이번 단계에서 구현

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
- `Public/Types/EPTypes.h` — `EEPBallisticType` 추가
- `Public/Data/EPWeaponDefinition.h` — `BallisticType`, `ProjectileClass`, `PelletCount` 추가
- `Public/Combat/EPCombatComponent.h` — `LastServerFireTime`, `HandleProjectileFire`, `Multicast_SpawnCosmeticProjectile` 추가
- `Private/Combat/EPCombatComponent.cpp` — `Server_Fire` switch 구조 + ProjectileFire 구현
- `Public/Combat/EPWeapon.h` / `Private/Combat/EPWeapon.cpp` — `Fire` 오버로드 추가

**신규:**
- `Public/Combat/EPProjectile.h`
- `Private/Combat/EPProjectile.cpp`

**이미 완료 (건너뜀):**
- `EP_TraceChannel_Weapon` 상수 (EPTypes.h) ✅
- `FEPBoneSnapshot` / `FEPHitboxSnapshot` (EPTypes.h) ✅
- `UEPServerSideRewindComponent` — 히스토리 기록, 보간, Broad Phase, ConfirmHitscan ✅
- `HandleHitscanFire` — SSR 위임 패턴 ✅
- `GetBoneMultiplier` / `GetMaterialMultiplier` ✅
- `RequestFire(ClientFireTime)` / `Server_Fire(ClientFireTime)` ✅

---

## Step 0) ✅ 구현 완료 — WeaponTrace 채널 설정

BoneHitbox Step 0에서 완료. `DefaultEngine.ini`에 `DefaultResponse=ECR_Ignore` 설정됨.

---

## Step 1) EPTypes.h — EEPBallisticType 추가

파일: `Public/Types/EPTypes.h`

`FEPHitboxSnapshot` / `FEPBoneSnapshot` / `EP_TraceChannel_Weapon`은 ✅ 이미 존재.
현재 구조 참고:

```cpp
// ✅ 이미 존재
USTRUCT()
struct FEPBoneSnapshot
{
    GENERATED_BODY()
    UPROPERTY() FName      BoneName;
    UPROPERTY() FTransform WorldTransform;
};

USTRUCT()
struct FEPHitboxSnapshot
{
    GENERATED_BODY()
    UPROPERTY() float   ServerTime = 0.f;
    UPROPERTY() FVector Location   = FVector::ZeroVector; // Broad Phase용
    UPROPERTY() TArray<FEPBoneSnapshot> Bones;            // Narrow Phase 리와인드용
};

static constexpr ECollisionChannel EP_TraceChannel_Weapon = ECC_GameTraceChannel1;
```

> **NetPrediction 문서의 `FEPHitboxSnapshot`(Location + Rotation, 캡슐 기반)은 무시.**
> 현재 구현은 본 단위 배열(`Bones`) 방식이며 이것이 최종 구조다.

**추가 필요 — EEPBallisticType:**

기존 enum 블록 아래에 추가:

```cpp
// 탄도 방식 — EEPFireMode(Single/Burst/Auto 트리거)와 별개
UENUM(BlueprintType)
enum class EEPBallisticType : uint8
{
    Hitscan,        // 즉발 LineTrace (라이플, SMG)
    ProjectileFast, // 고속 투사체 — 서버 시뮬, 클라 코스메틱만
    ProjectileSlow, // 저속 투사체 — Actor 복제 (수류탄, 로켓)
};
```

---

## Step 2) EPWeaponDefinition.h — 탄도 필드 추가

파일: `Public/Data/EPWeaponDefinition.h`

**추가 필요:**

헤더 상단:
```cpp
#include "Types/EPTypes.h" // EEPBallisticType

class AEPProjectile; // forward declare
```

기존 멤버 아래 추가:

```cpp
// --- 탄도 ---
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistics")
EEPBallisticType BallisticType = EEPBallisticType::Hitscan;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistics",
    meta = (EditCondition = "BallisticType != EEPBallisticType::Hitscan"))
TSubclassOf<AEPProjectile> ProjectileClass;

// 산탄총용 펠릿 수 — 1이면 단일(라이플/SMG), 2 이상이면 다중(샷건)
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistics",
    meta = (EditCondition = "BallisticType == EEPBallisticType::Hitscan", ClampMin = 1))
int32 PelletCount = 1;
```

> `BaseSpread`는 이미 WeaponDefinition에 존재 (`Category = "Weapon|Spread"`).
> 산탄총 펠릿 반각(도) 계산에 기존 `BaseSpread` 값을 재사용한다.
> 탄도 타입에 따라 의미가 달라지므로 에디터 카테고리만 다를 뿐 별도 추가 불필요.

---

## Steps 3~4) ✅ 구현 완료 — 히트박스 히스토리 기록 + 보간

BoneHitbox에서 `UEPServerSideRewindComponent`에 구현됨.

| NetPrediction 문서 항목 | 실제 위치 |
|------------------------|----------|
| `HitboxHistory` TArray | `UEPServerSideRewindComponent` (protected) |
| `SaveHitboxSnapshot()` | `UEPServerSideRewindComponent::TickComponent(TG_PostPhysics)` — CMC `OnServerMoveProcessed` 델리게이트로 pending 저장 후 커밋 |
| `GetSnapshotAtTime()` | `UEPServerSideRewindComponent` (per-bone BlendWith) |
| `AEPCharacter` 히스토리 멤버 | **없음** — 모두 SSR 컴포넌트에 있음 |

> **NetPrediction 문서의 Step 3(EPCharacter 멤버 추가)과 Step 4(AEPCharacter 구현)는 무시.**
> `SetActorLocationAndRotation` 기반 리와인드도 무시 — `FBodyInstance::SetBodyTransform` 방식 유지.

---

## Step 5) EPCombatComponent.h — 미완료 항목 추가

파일: `Public/Combat/EPCombatComponent.h`

**✅ 이미 존재:**
- `LocalLastFireTime` (protected)
- `RequestFire(Origin, Direction, ClientFireTime)`
- `Server_Fire(Origin, Direction, ClientFireTime)`
- `HandleHitscanFire(Owner, Origin, Directions, ClientFireTime)` (private)
- `GetBoneMultiplier` / `GetMaterialMultiplier` (private)

**추가 필요 (private 섹션):**

```cpp
// forward declare 추가 (헤더 상단)
class AEPProjectile;

private:
    // 서버 전용 — 클라이언트가 조작 불가. FireRate 초과 RPC 차단
    float LastServerFireTime = -999.f;

    // 투사체: ProjectileFast/Slow 분기 처리
    void HandleProjectileFire(
        AEPCharacter* Owner,
        const FVector& Origin,
        const FVector& Direction);
```

**RPC 추가 (protected 섹션):**

```cpp
// ProjectileFast — 발사자 외 다른 클라이언트에게 코스메틱 투사체 스폰 요청
UFUNCTION(NetMulticast, Unreliable)
void Multicast_SpawnCosmeticProjectile(
    const FVector_NetQuantize&       MuzzleLoc,
    const FVector_NetQuantizeNormal& Direction);
```

> `GetHitscanCandidates`는 SSR로 이전됨 — CombatComponent에 불필요.

---

## Step 6) RequestFire — ProjectileFast 코스메틱 스폰 추가

파일: `Private/Combat/EPCombatComponent.cpp`

**현재 구현 (✅ 완료 부분):**

```cpp
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
```

**✅ 이미 존재 — Multicast_PlayMuzzleEffect_Implementation:**

발사자 로컬 클라이언트에서 중복 재생을 막는 패턴:

```cpp
void UEPCombatComponent::Multicast_PlayMuzzleEffect_Implementation(const FVector_NetQuantize& MuzzleLocation)
{
    AEPCharacter* OwnerChar = GetOwnerCharacter();
    if (OwnerChar && OwnerChar->IsLocallyControlled()) return; // RequestFire에서 이미 재생

    PlayLocalMuzzleEffect(MuzzleLocation);
}
```

Muzzle 이펙트 전체 흐름:
1. 발사자 클라이언트: `RequestFire` → `PlayLocalMuzzleEffect` **즉시** 재생 (RTT 없음)
2. 서버: `Server_Fire_Implementation` → `Multicast_PlayMuzzleEffect` 브로드캐스트
3. 다른 클라이언트(SimulatedProxy): `Multicast_PlayMuzzleEffect_Implementation` → `PlayLocalMuzzleEffect`
4. 발사자: `IsLocallyControlled()` 체크로 **중복 차단**

ImpactEffect 흐름:
- `HandleHitscanFire` 내부 `ConfirmedHits` 루프 → `Multicast_PlayImpactEffect` (Step 9 참고)
- Impact는 서버 판정 후 확정된 위치만 브로드캐스트 → 클라이언트 예측 없음 (탄착 위치 불확실)

---

**추가 필요 — ProjectileFast 코스메틱 즉시 스폰:**

`Server_Fire` 호출 이후, 반동 적용 이후에 추가:

```cpp
    // ProjectileFast: Multicast 왕복 없이 발사자 측 코스메틱 즉시 스폰.
    // Multicast_SpawnCosmeticProjectile의 IsLocallyControlled() 체크와 짝을 이룬다.
    if (Owner && Owner->IsLocallyControlled()
        && EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast
        && EquippedWeapon->WeaponDef->ProjectileClass)
    {
        const FVector MuzzleLoc =
            EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
            ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
            : Origin;
        AEPProjectile* Cosmetic = GetWorld()->SpawnActor<AEPProjectile>(
            EquippedWeapon->WeaponDef->ProjectileClass,
            MuzzleLoc, Direction.GetSafeNormal().Rotation());
        if (Cosmetic)
            Cosmetic->SetCosmeticOnly();
    }
```

---

## Step 7) Server_Fire_Implementation — 검증 강화 + switch 구조

파일: `Private/Combat/EPCombatComponent.cpp`

**현재 구현 (일부 미완):**

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector_NetQuantize& Origin,
    const FVector_NetQuantizeNormal& Direction,
    float ClientFireTime)
{
    // 연사 속도, 탄약 검증
    if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;

    FVector SpreadDir = Direction;
    EquippedWeapon->Fire(SpreadDir); // 탄약 차감

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
    // ImpactEffect는 HandleHitscanFire 내부 ConfirmedHits 루프에서 Multicast_PlayImpactEffect로 호출 (Step 9)
}
```

**교체 후:**

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector_NetQuantize&       Origin,
    const FVector_NetQuantizeNormal& Direction,
    float                            ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;

    AEPCharacter* Owner = GetOwnerCharacter();
    if (!Owner) return;

    // ── 서버 사이드 검증 ────────────────────────────────────────────────────────

    // 1. FireRate: 클라 RPC 스팸 차단 (LastServerFireTime은 서버 전용 — 조작 불가)
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ServerNow = GS
        ? GS->GetServerWorldTimeSeconds()
        : GetWorld()->GetTimeSeconds();
    const float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
    if (ServerNow - LastServerFireTime < FireInterval) return;
    LastServerFireTime = ServerNow;

    // 2. 탄약 + 무기 상태
    if (!EquippedWeapon->CanFire()) return;

    // 3. Origin 위치 검증: 벽 너머 조작 방지 (200cm 여유: 이동 예측 오차 + 팔 길이)
    constexpr float MaxOriginDrift = 200.f;
    if (FVector::DistSquared(Origin, Owner->GetActorLocation()) > FMath::Square(MaxOriginDrift))
        return;

    // ── 탄도 분기 ─────────────────────────────────────────────────────────────

    // NetPrediction 단계에서 ProjectileFast / ProjectileSlow 케이스를 추가한다.
    switch (EquippedWeapon->WeaponDef->BallisticType)
    {
        case EEPBallisticType::Hitscan:
        default:
        {
            // 서버가 결정론적 RNG로 펠릿 방향 생성 (산탄총/단일 공용)
            TArray<FVector> PelletDirs;
            EquippedWeapon->Fire(Direction, ClientFireTime, PelletDirs);
            HandleHitscanFire(Owner, Origin, PelletDirs, ClientFireTime);
            break;
        }
        case EEPBallisticType::ProjectileFast:
        case EEPBallisticType::ProjectileSlow:
        {
            FVector SpreadDir = Direction;
            EquippedWeapon->Fire(SpreadDir); // 탄약 차감 (단일 방향)
            HandleProjectileFire(Owner, Origin, SpreadDir);
            break;
        }
    }

    const FVector MuzzleLocation =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();

    Multicast_PlayMuzzleEffect(MuzzleLocation);
}
```

---

## Step 8) ✅ 구현 완료 — GetHitscanCandidates (Broad Phase)

`UEPServerSideRewindComponent::GetHitscanCandidates`에 구현됨.
`UEPCombatComponent`에는 없음 — CombatComponent가 직접 호출할 필요 없음.

---

## Step 9) ✅ 구현 완료 — HandleHitscanFire

SSR 위임 패턴으로 구현됨. 현재 코드:

```cpp
void UEPCombatComponent::HandleHitscanFire(
    AEPCharacter* Owner, const FVector& Origin,
    const TArray<FVector>& Directions, float ClientFireTime)
{
    if (!Owner || !Owner->GetServerSideRewindComponent()) return;

    // [Rewind Block] → SSR에 위임 (Broad Phase + 리와인드 + Narrow Trace + 복구 + 디버그)
    TArray<FHitResult> ConfirmedHits;
    Owner->GetServerSideRewindComponent()->ConfirmHitscan(
        Owner, EquippedWeapon, Origin, Directions, ClientFireTime, ConfirmedHits);

    // [Damage Block] — GAS 전환 시 GameplayEffectSpec으로 교체
    for (const FHitResult& Hit : ConfirmedHits)
    {
        if (!Hit.GetActor()) continue;

        const float BaseDamage         = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
        const float BoneMultiplier     = GetBoneMultiplier(Hit.BoneName);
        const float MaterialMultiplier = GetMaterialMultiplier(Hit.PhysMaterial.Get());
        const float FinalDamage        = BaseDamage * BoneMultiplier * MaterialMultiplier;

        UE_LOG(LogTemp, Log,
            TEXT("[BoneHitbox] Bone=%s PM=%s Base=%.1f Bone*=%.2f Mat*=%.2f Final=%.1f"),
            *Hit.BoneName.ToString(),
            Hit.PhysMaterial.IsValid() ? *Hit.PhysMaterial->GetName() : TEXT("None"),
            BaseDamage, BoneMultiplier, MaterialMultiplier, FinalDamage);

        UGameplayStatics::ApplyPointDamage(
            Hit.GetActor(), FinalDamage,
            (Hit.ImpactPoint - Origin).GetSafeNormal(), Hit,
            Owner->GetController(), Owner, UDamageType::StaticClass());

        Multicast_PlayImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal);
    }
}
```

> **NetPrediction 문서의 `SetActorLocationAndRotation` 기반 리와인드 / `FEPRewindEntry(SavedLocation, SavedRotation)` / `GetHitscanCandidates` 직접 구현은 모두 무시.**
> `FBodyInstance::SetBodyTransform` 방식 + SSR 위임 패턴이 최종 구조다.
> PelletCount 대응: `Directions` 배열 크기가 `PelletCount`가 되므로 자동 대응됨.

---

## Step 10) HandleProjectileFire — Fast/Slow 분기

파일: `Private/Combat/EPCombatComponent.cpp`

include 추가:
```cpp
#include "Combat/EPProjectile.h"
```

```cpp
void UEPCombatComponent::HandleProjectileFire(
    AEPCharacter* Owner,
    const FVector& Origin,
    const FVector& Direction)
{
    if (!EquippedWeapon->WeaponDef || !EquippedWeapon->WeaponDef->ProjectileClass) return;

    const FVector MuzzleLoc =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : Origin;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner      = Owner;
    SpawnParams.Instigator = Owner;

    // bReplicates는 각 ProjectileClass Blueprint에서 설정한다.
    //   ProjectileFast Blueprint → bReplicates = false (서버 시뮬만, 복제 비용 0)
    //   ProjectileSlow Blueprint → bReplicates = true  (수류탄/로켓 자동 복제)
    AEPProjectile* Proj = GetWorld()->SpawnActor<AEPProjectile>(
        EquippedWeapon->WeaponDef->ProjectileClass,
        MuzzleLoc, Direction.GetSafeNormal().Rotation(), SpawnParams);

    if (!Proj) return;

    Proj->Initialize(EquippedWeapon->GetDamage(), Direction);

    // ProjectileFast: 발사자는 RequestFire에서 이미 로컬 스폰 완료.
    //                 Multicast는 SimulatedProxy(다른 클라)만을 위한 것.
    // ProjectileSlow: 복제 Actor가 클라 렌더링 담당 → Multicast 불필요.
    if (EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
        Multicast_SpawnCosmeticProjectile(MuzzleLoc, Direction.GetSafeNormal());
}
```

---

## Step 11) Multicast_SpawnCosmeticProjectile

파일: `Private/Combat/EPCombatComponent.cpp`

```cpp
void UEPCombatComponent::Multicast_SpawnCosmeticProjectile_Implementation(
    const FVector_NetQuantize&       MuzzleLoc,
    const FVector_NetQuantizeNormal& Direction)
{
    // 서버는 실제 Actor 보유 — 여기서 다시 스폰하면 중복
    if (HasAuthority()) return;

    // 발사자(AutonomousProxy)는 RequestFire에서 이미 스폰 완료 — 중복 방지
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar && OwnerChar->IsLocallyControlled()) return;

    // EquippedWeapon이 이 Multicast보다 늦게 복제될 수 있다 — 반드시 null 체크
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef
        || !EquippedWeapon->WeaponDef->ProjectileClass) return;

    AEPProjectile* Cosmetic = GetWorld()->SpawnActor<AEPProjectile>(
        EquippedWeapon->WeaponDef->ProjectileClass,
        MuzzleLoc, Direction.Rotation());

    if (Cosmetic)
        Cosmetic->SetCosmeticOnly(); // 충돌/데미지 비활성화, 궤적 렌더링만
}
```

---

## Step 12) AEPProjectile — 신규 클래스

### 12-1. EPProjectile.h

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

    // HandleProjectileFire / RequestFire에서 호출 — 데미지와 발사 방향 주입
    void Initialize(float InDamage, const FVector& InDirection);

    // 충돌/데미지 비활성화 → 궤적 렌더링 전용 인스턴스로 변환
    // ProjectileFast 코스메틱 스폰 시 호출
    void SetCosmeticOnly();

protected:
    UPROPERTY(VisibleAnywhere, Category = "Projectile")
    TObjectPtr<USphereComponent> CollisionComp;

    UPROPERTY(VisibleAnywhere, Category = "Projectile")
    TObjectPtr<UProjectileMovementComponent> MovementComp;

private:
    float   BaseDamage      = 0.f;
    FVector LaunchDir       = FVector::ForwardVector;
    bool    bIsCosmeticOnly = false; // true이면 OnProjectileHit 조기 반환

    UFUNCTION()
    void OnProjectileHit(
        UPrimitiveComponent* HitComp,
        AActor*              OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector              NormalImpulse,
        const FHitResult&    Hit);
};
```

### 12-2. EPProjectile.cpp

파일: `Private/Combat/EPProjectile.cpp`

```cpp
#include "Combat/EPProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AEPProjectile::AEPProjectile()
{
    // bReplicates는 각 Blueprint에서 설정한다
    // ProjectileFast BP → false / ProjectileSlow BP → true

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(5.f);
    CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
    CollisionComp->SetNotifyRigidBodyCollision(true);
    CollisionComp->OnComponentHit.AddDynamic(this, &AEPProjectile::OnProjectileHit);
    SetRootComponent(CollisionComp);

    MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
    MovementComp->bRotationFollowsVelocity = true;
    // InitialSpeed / MaxSpeed / ProjectileGravityScale 은 각 Blueprint에서 설정한다
}

void AEPProjectile::Initialize(float InDamage, const FVector& InDirection)
{
    BaseDamage = InDamage;
    LaunchDir  = InDirection.GetSafeNormal();
}

void AEPProjectile::SetCosmeticOnly()
{
    bIsCosmeticOnly = true;
    // 충돌 비활성화 — 다른 액터를 관통하며 궤적만 그린다
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // 중력 스케일은 유지 — 클라/서버 궤적이 동일해야 설득력이 있다
}

void AEPProjectile::OnProjectileHit(
    UPrimitiveComponent*,
    AActor* OtherActor,
    UPrimitiveComponent*,
    FVector,
    const FHitResult& Hit)
{
    if (bIsCosmeticOnly) return;

    // 서버만 데미지 처리
    if (!HasAuthority()) return;

    // 투사체는 랙 보상 불필요 — 이동 시간이 지연을 흡수한다
    // [GAS 전환 포인트] ApplyPointDamage → GameplayEffectSpec
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

## Step 13) AEPWeapon::Fire 오버로드 — 결정론적 RNG 펠릿 생성

파일: `Public/Combat/EPWeapon.h` + `Private/Combat/EPWeapon.cpp`

### 13-1. 헤더 선언 추가

```cpp
// 히트스캔 전용 오버로드 — 탄약 차감 + 결정론적 RNG로 N개 펠릿 방향 생성
// 단일 펠릿(라이플/SMG): PelletCount == 1 → OutPellets.Num() == 1
// 산탄총: PelletCount > 1 → OutPellets.Num() == PelletCount
void Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets);
```

### 13-2. 구현

```cpp
void AEPWeapon::Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets)
{
    if (!WeaponDef) return;

    CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);

    // 결정론적 시드: 클라이언트와 동일 연산 → 동일 펠릿 방향 보장
    // FloorToInt(* 1000): 밀리초 단위 int32 — 128Hz 틱에서도 충돌 없음
    const int32 Seed = FMath::FloorToInt(ClientFireTime * 1000.f);
    FRandomStream RandStream(Seed);

    const int32 Count = FMath::Max(1, WeaponDef->PelletCount);
    OutPellets.Reserve(Count);

    for (int32 i = 0; i < Count; ++i)
    {
        // BaseSpread 반각(도) 범위 내 랜덤 방향 — 원뿔 균등 분포
        const float Angle = FMath::DegreesToRadians(WeaponDef->BaseSpread);
        OutPellets.Add(FMath::VRandCone(AimDir, Angle, RandStream));
    }
}
```

---

## 3. 테스트 절차 (반드시 멀티로)

### PIE 설정
- Dedicated Server ON
- Client 2~3명

### 히트스캔 테스트 케이스

| 케이스 | 기대 결과 |
|--------|-----------|
| 낮은 핑 | 조준 시점 기준 히트 판정 |
| 높은 핑 (네트워크 에뮬) | 클라 발사 시점 기준 히트 보장 |
| MaxRewindSeconds 초과 핑 | 현재 위치 기준 판정 (리와인드 없음) |
| FireRate 초과 RPC | `LastServerFireTime` 체크로 서버에서 조용히 거부 |
| Origin 200cm 초과 | 서버에서 거부 |
| 사망한 적 조준 발사 | 데미지 없음 |

### 투사체 테스트 케이스

| 케이스 | 기대 결과 |
|--------|-----------|
| ProjectileFast 발사자 | `RequestFire`에서 즉시 코스메틱 궤적 |
| ProjectileFast 다른 클라 | `Multicast`로 코스메틱 수신 → 궤적 렌더링 |
| ProjectileSlow 발사 | 서버 Actor 복제 → 클라 자동 동기화 |
| 코스메틱 투사체 충돌 | 데미지 없음 (`SetCosmeticOnly` 확인) |

---

## 4. 트러블슈팅

| 증상 | 원인 | 해결 |
|------|------|------|
| 캐릭터가 안 맞는 경우 | Physics Asset WeaponTrace Block 미설정 | BoneHitbox Step 1 확인 |
| FireRate 초과 RPC가 통과됨 | `LastServerFireTime` 누락 | Step 5 추가 확인 |
| Origin 검증이 항상 실패 | `MaxOriginDrift` 너무 작거나 GetActorLocation 오프셋 | 값 조정 |
| ProjectileFast 발사자에게 두 번 보임 | `Multicast_SpawnCosmeticProjectile`의 `IsLocallyControlled()` 체크 누락 | Step 11 확인 |
| 산탄총 클라/서버 방향 불일치 | 시드 변환 불일치 | 양측 모두 `FMath::FloorToInt(ClientFireTime * 1000.f)` 확인 |
| 코스메틱 투사체가 데미지를 줌 | `SetCosmeticOnly()` 호출 타이밍 오류 | SpawnActor 직후 호출 확인 |

---

## 5. 다음 단계 연결 (GAS)

이 단계 완료 후 GAS에서 교체할 부분:

| 현재 | GAS 이후 |
|------|---------|
| `ApplyPointDamage` (HandleHitscanFire Damage Block) | `GameplayEffectSpec` + `SetByCaller` |
| `ApplyPointDamage` (AEPProjectile::OnProjectileHit) | 동일 |
| `UDamageType::StaticClass()` | 커스텀 `UEPDamageType` |
| `LastServerFireTime` 수동 관리 | GAS Cooldown GameplayEffect |

Rewind Block (`UEPServerSideRewindComponent::ConfirmHitscan`)과 ProjectileFast/Slow 분기는
**GAS 단계에서도 그대로 유지된다.** Damage Block만 교체하면 된다.
