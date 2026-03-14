# Post 3-5 작성 가이드 — 투사체 시스템: ProjectileFast vs ProjectileSlow

> **예상 제목**: `[UE5] 추출 슈터 3-5. 투사체 시스템: 고속/저속 분리와 클라이언트 코스메틱 스폰`
> **참고 문서**: `DOCS/Notes/03_NetPrediction.md` Section 5.6, `DOCS/Notes/03_NetPrediction_Implementation.md` Step 10~12
>
> ⚠️ **작성 전 구현 필요 항목:**
> - `AEPProjectile` (EPProjectile.h + cpp)
> - `HandleProjectileFire` (EPCombatComponent)
> - `Multicast_SpawnCosmeticProjectile` RPC
> - `RequestFire`에 ProjectileFast 코스메틱 스폰 추가

---

## 개요

**이 포스팅에서 다루는 것:**
- 고속/저속 투사체를 왜 다르게 처리해야 하는가
- `AEPProjectile` 클래스 설계 (`SetCosmeticOnly` 패턴)
- `HandleProjectileFire` — ProjectileFast/Slow 분기
- 발사자/서버/다른 클라이언트 3자에 대한 코스메틱 스폰 중복 방지
- `RequestFire`에 ProjectileFast 코스메틱 즉시 스폰

**왜 이렇게 구현했는가 (설계 의도):**
- 히트스캔과 투사체의 가장 큰 차이: 투사체는 **이동 시간**이 클라/서버 시차를 흡수 → 랙 보상 불필요
- ProjectileFast는 서버 Actor 복제보다 Multicast 코스메틱이 훨씬 효율적
- 서버/발사자/다른 클라 3자 모두 각자의 방식으로 렌더링 → 중복 방지 패턴이 핵심

---

## 구현 전 상태 (Before)

3-4 포스팅(NetPrediction_Validation) 구현 완료 직후:
- `EEPBallisticType::ProjectileFast/ProjectileSlow` switch case가 있지만 `HandleProjectileFire`가 없어 컴파일 실패
- `AEPProjectile` 클래스가 없어 `ProjectileClass` 에셋 설정 불가

---

## 구현 내용

### 1. ProjectileFast vs ProjectileSlow 처리 비교

```
[ProjectileFast — 소총탄 수준]
서버:  SpawnActor(bReplicates=false) → 물리 시뮬 → 충돌 → 데미지 → Destroy
클라(발사자):  RequestFire에서 즉시 코스메틱 스폰 (SetCosmeticOnly)
클라(다른):   Multicast_SpawnCosmeticProjectile 수신 → 코스메틱 스폰

[ProjectileSlow — 수류탄/로켓]
서버:  SpawnActor(bReplicates=true) → 자동 복제 → 충돌 → 데미지 → Destroy
클라:  복제된 Actor 자동 수신 → 위치 추적 → 렌더링
       (Multicast 불필요 — 복제가 렌더링 담당)
```

**고속 투사체를 Actor 복제로 처리하면 안 되는 이유:**
```
서버 틱레이트 30Hz → 틱 간격 33ms
소총탄 속도 870m/s → 33ms간 이동거리 = 28.7m
→ 복제 수신 시 투사체가 갑자기 28m 앞에 나타남 → 끊김/점프
→ 복제가 사실상 의미 없음
```

### 2. AEPProjectile 클래스

```cpp
// Public/Combat/EPProjectile.h
UCLASS()
class EMPLOYMENTPROJ_API AEPProjectile : public AActor
{
    GENERATED_BODY()
public:
    AEPProjectile();

    // HandleProjectileFire / RequestFire에서 호출
    void Initialize(float InDamage, const FVector& InDirection);

    // 충돌/데미지 비활성화 → 궤적 렌더링 전용 인스턴스
    void SetCosmeticOnly();

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> CollisionComp;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProjectileMovementComponent> MovementComp;

private:
    float   BaseDamage      = 0.f;
    FVector LaunchDir       = FVector::ForwardVector;
    bool    bIsCosmeticOnly = false;

    UFUNCTION()
    void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
```

```cpp
// Private/Combat/EPProjectile.cpp
AEPProjectile::AEPProjectile()
{
    // bReplicates는 각 Blueprint에서 설정
    // ProjectileFast BP → false / ProjectileSlow BP → true

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(5.f);
    CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
    CollisionComp->SetNotifyRigidBodyCollision(true);
    CollisionComp->OnComponentHit.AddDynamic(this, &AEPProjectile::OnProjectileHit);
    SetRootComponent(CollisionComp);

    MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
    MovementComp->bRotationFollowsVelocity = true;
    // InitialSpeed / MaxSpeed / ProjectileGravityScale 은 각 Blueprint에서 설정
}

void AEPProjectile::SetCosmeticOnly()
{
    bIsCosmeticOnly = true;
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // 중력 스케일은 유지 — 클라/서버 궤적이 동일해야 설득력 있음
}

void AEPProjectile::OnProjectileHit(
    UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
    // 코스메틱 인스턴스는 충돌 없음 → 진입 불가
    // 클라이언트는 데미지 처리 안 함
    if (!HasAuthority()) return;

    // 투사체는 랙 보상 불필요 — 이동 시간이 지연을 흡수
    if (OtherActor)
        UGameplayStatics::ApplyPointDamage(
            OtherActor, BaseDamage, LaunchDir, Hit,
            GetInstigatorController(), GetInstigator(), UDamageType::StaticClass());

    Destroy();
}
```

> **에디터 설정**: BP_ProjectileFast 생성 → bReplicates = false, InitialSpeed = 87000 (870m/s)
> BP_ProjectileSlow 생성 → bReplicates = true, InitialSpeed = 3000 (30m/s), ProjectileGravityScale = 1.5

### 3. HandleProjectileFire

```cpp
void UEPCombatComponent::HandleProjectileFire(
    AEPCharacter* Owner, const FVector& Origin, const FVector& Direction)
{
    if (!EquippedWeapon->WeaponDef || !EquippedWeapon->WeaponDef->ProjectileClass) return;

    const FVector MuzzleLoc =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : Origin;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner      = Owner;
    SpawnParams.Instigator = Owner;

    // bReplicates는 각 ProjectileClass Blueprint에서 설정
    // → ProjectileFast: false / ProjectileSlow: true (자동 복제)
    AEPProjectile* Proj = GetWorld()->SpawnActor<AEPProjectile>(
        EquippedWeapon->WeaponDef->ProjectileClass,
        MuzzleLoc, Direction.GetSafeNormal().Rotation(), SpawnParams);

    if (!Proj) return;
    Proj->Initialize(EquippedWeapon->GetDamage(), Direction);

    // ProjectileFast: 발사자는 RequestFire에서 이미 로컬 스폰 완료
    //                 Multicast는 SimulatedProxy(다른 클라)만을 위한 것
    // ProjectileSlow: 복제 Actor가 클라 렌더링 담당 → Multicast 불필요
    if (EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
        Multicast_SpawnCosmeticProjectile(MuzzleLoc, Direction.GetSafeNormal());

    // Multicast_PlayMuzzleEffect는 Server_Fire_Implementation에서 한 번만 호출
    // 여기서 중복 호출하면 ProjectileFast/Slow에서 두 번 재생됨 → 하지 않음
}
```

### 4. Multicast_SpawnCosmeticProjectile — 3자 중복 방지

```cpp
void UEPCombatComponent::Multicast_SpawnCosmeticProjectile_Implementation(
    const FVector_NetQuantize& MuzzleLoc,
    const FVector_NetQuantizeNormal& Direction)
{
    // 서버: 실제 Actor를 이미 보유 → 중복 스폰 금지
    if (HasAuthority()) return;

    // 발사자(AutonomousProxy): RequestFire에서 이미 스폰 → 중복 금지
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar && OwnerChar->IsLocallyControlled()) return;

    // 다른 클라이언트(SimulatedProxy): 여기서 스폰
    // EquippedWeapon이 아직 복제 안됐을 수 있음 → null 체크 필수
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef
        || !EquippedWeapon->WeaponDef->ProjectileClass) return;

    AEPProjectile* Cosmetic = GetWorld()->SpawnActor<AEPProjectile>(
        EquippedWeapon->WeaponDef->ProjectileClass,
        MuzzleLoc, Direction.Rotation());

    if (Cosmetic) Cosmetic->SetCosmeticOnly();
}
```

**3자 처리 흐름:**

```
발사자 클라이언트 → RequestFire → 즉시 코스메틱 스폰 (SetCosmeticOnly)
                                → Server_Fire RPC
                                      ↓ (서버)
                                   HandleProjectileFire → SpawnActor(bReplicates=false)
                                                       → Multicast_SpawnCosmeticProjectile
발사자      ← IsLocallyControlled() → return (중복 방지)
서버        ← HasAuthority() → return (중복 방지)
다른 클라   ← SpawnActor(ProjectileClass) + SetCosmeticOnly() ← 여기서만 스폰
```

### 5. RequestFire에 ProjectileFast 코스메틱 추가

```cpp
// Server_Fire 호출 + 반동 적용 이후에 추가
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

### 6. 왜 투사체에는 랙 보상이 필요 없는가

```
히트스캔:
T=0   클라이언트 발사 (적이 위치 A에 있음)
T=50ms 서버 RPC 수신 (적은 이미 위치 B)
→ 서버 현재 위치(B)로 판정 → 빗나감 → 랙 보상 필요

ProjectileSlow:
T=0   클라이언트 발사
T=50ms 서버 투사체 생성
T=2000ms 서버 투사체가 적과 충돌 (이 시점에서 적 위치로 판정)
→ 충돌 시점이 이미 동일한 서버 시각 → 랙 보상 불필요
```

---

## 결과

**확인 항목 (PIE Dedicated Server + Client 2명):**
- ProjectileFast 무기 발사 시 클라이언트에서 즉시 궤적 렌더링 (RTT 없음)
- 다른 클라이언트에서 Multicast로 코스메틱 투사체 수신 → 궤적 일치 확인
- 코스메틱 투사체가 데미지를 주지 않는지 확인 (HasAuthority 로그)
- ProjectileSlow 수류탄 발사 → 복제 Actor 자동 동기화 확인
- 두 클라이언트 화면에서 수류탄 궤적이 동일하게 보이는지 확인

**한계 및 향후 개선:**
- `ProjectileSlow` 복제 지연: 서버 스폰 후 클라이언트에 첫 복제까지 1~2 틱 지연으로 궤적이 앞으로 점프
  → 실무: 클라이언트도 로컬 예측 스폰 후 서버 복제 Actor와 조율 (이 단계 범위 밖)
- 코스메틱을 Actor 대신 Niagara Ribbon/Beam으로 대체 시 스폰 비용 제거 가능

---

## 참고

- `DOCS/Notes/03_NetPrediction.md` Section 5.6 (투사체 스폰 및 히트 처리)
- `DOCS/Notes/03_NetPrediction_Implementation.md` Step 10~12
