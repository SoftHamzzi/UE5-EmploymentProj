# 3단계: Client Prediction / Reconciliation / Lag Compensation

## 1. 왜 필요한가

멀티플레이어 FPS에서 모든 판정을 서버에서 하면, 클라이언트는 RTT(왕복 지연) 만큼 늦게 결과를 본다.
- 이동: 키를 눌렀는데 RTT 후에야 캐릭터가 움직임 → 조작감 파괴
- 사격: 화면에서 적에게 조준하고 쐈는데, 서버에서는 적이 이미 이동 → 빗나감

이를 해결하는 세 가지 기법:
1. **Client Prediction**: 클라이언트가 서버 응답 전에 미리 결과를 예측
2. **Server Reconciliation**: 서버 결과와 예측이 다를 때 보정
3. **Lag Compensation**: 서버가 클라이언트의 시점으로 되돌아가서 판정

---

## 2. Client Prediction (클라이언트 예측)

### 이동 예측 - CMC가 자동 처리

CharacterMovementComponent는 이동 예측을 내장하고 있다:

```
[클라이언트]
1. 매 프레임 이동 입력 → 로컬에서 즉시 이동 시뮬레이션
2. 이동 입력을 FSavedMove에 저장 (타임스탬프, 가속도, 회전, 이동모드 등)
3. 저장된 이동을 서버에 전송 (매 프레임이 아닌, 여러 프레임 합쳐서)

[서버]
4. 클라이언트 이동 입력을 받아 서버에서 동일하게 시뮬레이션
5. 권한적 위치/속도/상태 확정
6. 결과를 클라이언트에 전송

[클라이언트]
7. 서버 결과 수신 → 해당 타임스탬프의 예측값과 비교
8. 오차가 허용 범위 내면 → 유지
9. 오차가 허용 범위 초과 → 보정 (다음 섹션)
```

### 커스텀 예측 (이동 외)

이동이 아닌 것(스킬 발동, 상태 변경 등)도 예측할 수 있다:
- GAS의 `LocalPredicted` 정책: 클라가 먼저 어빌리티 실행, 서버가 확인/거부
- `FPredictionKey`: 예측 식별자. 서버가 확인하면 유지, 거부하면 롤백

```
예측 가능: 어빌리티 활성화, GameplayEffect 적용, 어트리뷰트 변경, 몽타주 재생
예측 불가: GameplayEffect 제거 (서버만), 서버 전용 데이터에 의존하는 판정
```

---

## 3. Server Reconciliation (서버 보정)

### 이동 보정 - CMC 자동 처리

```
[클라이언트가 서버 결과 수신]
1. 서버의 권한적 위치/상태 + 해당 타임스탬프 수신
2. 해당 타임스탬프의 클라이언트 예측 위치와 비교
3. 오차 > 허용치 (기본 ~1 unit) 이면:
   a. 서버 위치로 스냅
   b. 해당 타임스탬프 이후의 미확인 입력을 모두 재실행 (리플레이)
   c. 리플레이 결과가 새 예측 위치가 됨
```

이 "스냅 + 리플레이" 과정이 **서버 보정(Server Reconciliation)**이다.

### 사격/전투 보정

사격에서의 보정은 이동보다 단순하다:

```
[클라이언트]
1. 발사 → 로컬에서 총구 이펙트/사운드 즉시 재생 (예측)
2. Server RPC로 발사 정보 전송

[서버]
3. 히트 판정 수행
4. 결과를 클라이언트에 전송 (Client RPC 또는 Multicast)

[클라이언트]
5. 서버 결과에 맞게 후처리
   - 히트 확인: 히트마커, 데미지 숫자 표시
   - 미스: 예측 이펙트는 이미 재생됨 (문제없음, cosmetic)
```

포트폴리오에서는 이 수준이면 충분하다:
- 서버 판정 결과를 Client RPC로 전달
- 클라이언트는 결과에 맞게 VFX/사운드 정리

---

## 4. Hit Validation (서버 권한 히트 판정)

### 원칙

**클라이언트는 "맞았다"고 주장할 수 없다.**
클라이언트는 "이 시점에, 이 위치에서, 이 방향으로 쐈다"만 전송.
서버가 레이캐스트로 판정한다.

### RPC 설계

`Server_Fire`는 `AEPCharacter`가 아닌 `UEPCombatComponent`에 위치한다.
전투 관련 로직을 Character에서 분리한 설계 의도를 유지.

```cpp
// EPCombatComponent.h
// 현재: const FVector& 사용
// 권장: FVector_NetQuantize / FVector_NetQuantizeNormal (대역폭 절감)
//       Lag Compensation 구현 시 float ClientFireTime 추가
UFUNCTION(Server, Reliable)
void Server_Fire(const FVector& Origin, const FVector& Direction);

// EPCombatComponent.cpp
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector& Origin, const FVector& Direction)
{
    if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;

    // Spread 적용 + 탄약 차감 (AEPWeapon 내부 처리)
    FVector SpreadDir = Direction;
    EquippedWeapon->Fire(SpreadDir);

    AEPCharacter* Owner = GetOwnerCharacter();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);
    Params.AddIgnoredActor(EquippedWeapon);  // 자기 무기 무시

    const FVector End = Origin + SpreadDir * 10000.f;

    // ECC_GameTraceChannel1: 히트박스 전용 채널 (환경과 분리)
    FHitResult Hit;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, Origin, End, ECC_GameTraceChannel1, Params);

    if (bHit && Hit.GetActor())
    {
        // ApplyPointDamage 권장 (BoneName → 부위별 데미지)
        // 현재는 ApplyDamage 사용 — 03_BoneHitbox_Implementation.md에서 교체
        UGameplayStatics::ApplyDamage(
            Hit.GetActor(),
            EquippedWeapon->GetDamage(),
            Owner->GetController(),
            Owner,
            nullptr
        );
    }

    // 총구 이펙트와 탄착 이펙트를 별도 Multicast로 분리
    // (발생 위치와 조건이 다르기 때문)
    const FVector MuzzleLocation =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();

    Multicast_PlayMuzzleEffect(MuzzleLocation);
    if (bHit)
        Multicast_PlayImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal);
}
```

**3단계에서 변경할 내용:**
- `const FVector&` → `FVector_NetQuantize` / `FVector_NetQuantizeNormal` (대역폭 절감)
- `ClientFireTime` 파라미터 추가 → Lag Compensation 활성화
- `EEPBallisticType` 분기 → `HandleHitscanFire` / `HandleProjectileFire` 추출
- `ApplyDamage` → `ApplyPointDamage` (부위별 데미지, 03_BoneHitbox 단계)

### FVector_NetQuantize / FVector_NetQuantizeNormal

| 타입 | 용도 | 정밀도 |
|------|------|--------|
| `FVector_NetQuantize` | 위치 | 소수점 1자리 (0.1 단위, 게임에서 충분) |
| `FVector_NetQuantizeNormal` | 방향 (정규화 벡터) | 16비트 각도 (65536 방향, 0.005도 정밀도) |

일반 FVector 대비 대역폭을 크게 절감한다.

### 탄도 방식 분리 (EEPBallisticType)

무기뿐 아니라 **스킬(어빌리티)도 히트스캔·투사체 방식을 쓴다.**
예) 섬광탄 스킬 = 즉발 범위 판정(히트스캔), 화염구 스킬 = 투사체.
따라서 탄도 방식 분기를 `Server_Fire` 내부에 묻지 않고 **재사용 가능한 함수로 분리**한다.

`EEPFireMode`(Single/Burst/Auto)는 트리거 방식이다. 탄도 방식은 **별개 enum**:

```cpp
// EPTypes.h
UENUM(BlueprintType)
enum class EEPBallisticType : uint8
{
    Hitscan,         // 즉발 LineTrace (라이플, SMG, 히트스캔 스킬)
    ProjectileFast,  // 고속 투사체 — 서버 시뮬, 클라 코스메틱만 (소총탄, 권총탄)
    ProjectileSlow,  // 저속 투사체 — Actor 복제 (수류탄, 로켓)
};
```

**고속/저속을 분리하는 이유:**
- 870m/s 소총탄을 네트워크 틱(30Hz)으로 복제하면 위치가 뚝뚝 끊김. 복제가 의미없다.
- 수류탄/로켓은 느려서 복제 지연이 문제되지 않고, 플레이어가 궤적을 보고 피해야 하므로 정확한 위치 동기화가 필요하다.

**WeaponDefinition 확장:**

```cpp
UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ballistics")
EEPBallisticType BallisticType = EEPBallisticType::Hitscan;

// Hitscan이 아닌 모든 탄도 방식(ProjectileFast/Slow)에서 ProjectileClass가 필요하다
UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ballistics",
    meta = (EditCondition = "BallisticType != EEPBallisticType::Hitscan"))
TSubclassOf<AEPProjectile> ProjectileClass;

// ProjectileSpeed는 여기 두지 않는다.
// 투사체마다 중력·반사·속도 특성이 다르므로 각 ProjectileClass Blueprint에서
// UProjectileMovementComponent::InitialSpeed 등을 직접 설정한다.
```

**Server_Fire 분기:**

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector_NetQuantize& Origin,
    const FVector_NetQuantizeNormal& Direction,
    float ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;

    AEPCharacter* Owner = GetOwnerCharacter();
    if (!Owner) return;

    // ── 서버 사이드 검증 ────────────────────────────────────────────────────

    // 1. 발사 속도 검증 (FireRate): 클라이언트가 RPC를 스팸해도 서버가 독립적으로 차단.
    //    LastServerFireTime은 서버에서만 기록 — 클라이언트가 조작 불가.
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    const float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
    if (ServerNow - LastServerFireTime < FireInterval)
        return; // FireRate 초과 RPC 즉시 거부
    LastServerFireTime = ServerNow;

    // 2. 탄약 확인 + 무기 상태 검증
    if (!EquippedWeapon->CanFire()) return;

    // 3. Origin 위치 검증: 클라가 보낸 발사 Origin이 서버의 캐릭터 위치와 너무 멀면 거부.
    //    조작 클라이언트가 벽 너머 좌표를 Origin으로 보내는 것을 방지한다.
    //    200cm: 이동 예측 오차 + 무기 Offset(팔 길이 등) 포함 여유치
    constexpr float MaxOriginDrift = 200.f;
    if (FVector::DistSquared(Origin, Owner->GetActorLocation()) > FMath::Square(MaxOriginDrift))
        return;

    // ── 탄도 처리 ────────────────────────────────────────────────────────────

    const EEPBallisticType BallisticType = EquippedWeapon->WeaponDef->BallisticType;

    if (BallisticType == EEPBallisticType::Hitscan)
    {
        // 서버 스프레드 재생성 (결정론적 RNG):
        // 클라에서 방향을 수신하면 조작 가능(핵이 정확한 방향만 전송).
        // 서버가 동일 시드로 독립 계산 → 클라이언트 제어 불가.
        // 탄약 차감도 Fire() 내부에서 처리한다.
        TArray<FVector> PelletDirs;
        EquippedWeapon->Fire(Direction, PelletDirs); // 탄약 차감 + 스프레드 배열 반환
        // 단일 펠릿(라이플 등): PelletDirs.Num() == 1
        // 산탄총: PelletDirs.Num() == PelletCount (WeaponDef에서 설정)
        HandleHitscanFire(Owner, Origin, PelletDirs, ClientFireTime);
    }
    else // ProjectileFast / ProjectileSlow 모두 HandleProjectileFire에서 처리
    {
        FVector SpreadDir = Direction;
        EquippedWeapon->Fire(SpreadDir); // 탄약 차감 + 스프레드 (단일 방향)
        HandleProjectileFire(Owner, Origin, SpreadDir);
    }
}
```

> **산탄총 펠릿 생성 — 결정론적 RNG 방식:**
> 서버는 `ClientFireTime`을 시드로 삼아 스프레드 배열을 재생성한다.
> 클라이언트도 동일 시드로 N개 펠릿 방향을 생성하여 로컬 이펙트를 표시한다.
> RPC로 클라이언트의 펠릿 방향을 그대로 수신하는 방식은
> **핵 클라이언트가 모든 펠릿을 조준점으로 보내는 것을 막을 수 없어** 사용하지 않는다.
>
> **시드 변환:** `ClientFireTime`은 float(초 단위)이므로 `FRandomStream` 시드(int32)로 변환한다.
> ```cpp
> // 밀리초 단위 정수로 변환 — 클라/서버 동일 연산으로 동일 시드 보장
> const int32 Seed = FMath::FloorToInt(ClientFireTime * 1000.f);
> FRandomStream RandStream(Seed);
> // 128Hz 틱: 연속 두 틱 차이 = 7.8ms → 시드 7 이상 차이 보장 (같은 발사에서 충돌 없음)
> ```
>
> **구현 포인트:** `AEPWeapon::Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets)`
> — `WeaponDef->PelletCount`, `WeaponDef->BaseSpread`, `FRandomStream(Seed)`로 N개 방향 생성.
> 단일 펠릿 무기는 `PelletCount = 1`로 설정하면 동일 함수를 공유한다.

**RequestFire (클라이언트 측):**

```cpp
void UEPCombatComponent::RequestFire(const FVector& Origin, const FVector& Direction)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;
    if (EquippedWeapon->CurrentAmmo <= 0) return;

    AEPCharacter* Owner = GetOwnerCharacter();

    // 클라이언트 측 연사속도 체크 (서버도 독립적으로 검증 — 이중 방어)
    const float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
    const float CurrentTime  = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LocalLastFireTime < FireInterval) return;
    LocalLastFireTime = CurrentTime;

    // 서버 기준 시간 획득 — HitboxHistory 기록과 동일 기준이어야 리와인드가 정확하다.
    // GameState null 체크: 초기화 타이밍에 따라 null일 수 있다.
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ClientFireTime = GS
        ? GS->GetServerWorldTimeSeconds()
        : GetWorld()->GetTimeSeconds();

    Server_Fire(Origin, Direction.GetSafeNormal(), ClientFireTime);

    if (Owner && Owner->IsLocallyControlled())
    {
        // 반동 예측 — 서버 확인 전에 즉시 적용 (체감 우선)
        Owner->AddControllerPitchInput(-EquippedWeapon->GetRecoilPitch());
        Owner->AddControllerYawInput(FMath::RandRange(
            -EquippedWeapon->GetRecoilYaw(),
             EquippedWeapon->GetRecoilYaw()));

        // ProjectileFast: 발사자는 Multicast 왕복을 기다리지 않고 즉시 코스메틱 스폰.
        // Multicast_SpawnCosmeticProjectile의 IsLocallyControlled() 체크와 짝을 이룬다.
        if (EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast
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

        // 산탄총(PelletCount > 1): 서버와 동일 시드로 N개 펠릿 방향 생성 → 로컬 이펙트 표시
        // const int32 Seed = FMath::FloorToInt(ClientFireTime * 1000.f);
        // FRandomStream RandStream(Seed);
        // EquippedWeapon->GenerateClientPellets(Direction, RandStream, PelletDirs);
        // → 각 방향으로 트레이서/이펙트 재생 (판정 없음, 순수 코스메틱)
    }
}
```

`HandleHitscanFire`를 독립 함수로 추출하는 이유:
**GAS 어빌리티의 히트스캔 스킬도 동일 함수를 호출**할 수 있어야 하기 때문이다.
랙 보상 인프라(`GetSnapshotAtTime`, 링버퍼)는 무기·스킬 구분 없이 공유된다.

---

## 5. Lag Compensation (랙 보상 / 서버 리와인드)

### 문제

서버에서 레이캐스트하면, 클라이언트가 쐈을 때와 서버가 판정할 때 사이에 시간차가 있다.
그 사이에 적이 움직이면 → 클라이언트 화면에서는 맞았는데 서버에서는 빗나감.

```
[시간축]
T=0: 클라이언트가 적을 보고 발사 (클라 화면에서 적은 위치 A에 있음)
T=50ms: 서버가 RPC 수신 (서버에서 적은 이미 위치 B로 이동)
→ 서버 레이캐스트가 위치 B를 기준으로 하면 빗나감
→ 클라이언트 체감: "분명 맞았는데?"
```

### 해결: 서버 리와인드

서버가 클라이언트의 발사 시각으로 **히트박스를 되돌려서** 판정하는 기법.

### 히트스캔 vs 투사체 — 랙 보상 필요 여부

| | 히트스캔 | 투사체 |
|---|---|---|
| 판정 시점 | 발사 즉시 | 투사체가 충돌한 순간 |
| 랙 보상 | **필요** | **불필요** |
| 이유 | 클라 시점 ≠ 서버 시점 | 투사체 이동 시간이 지연을 흡수 |
| 리와인드 시각 | `ClientFireTime` | — |

투사체는 서버에서도 실시간으로 날아간다.
클라가 발사한 시점과 서버 투사체가 충돌하는 시점이 이미 같은 서버 시각이므로,
히트스캔처럼 과거로 되돌릴 필요가 없다.

### 구현 구조

#### 1) 히트박스 히스토리 링버퍼

```cpp
// EPTypes.h에 정의 (본 단위 확장은 03_BoneHitbox.md 참고)
USTRUCT()
struct FEPHitboxSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    float ServerTime = 0.f;                    // 기록 시점의 서버 시간

    UPROPERTY()
    FVector Location = FVector::ZeroVector;    // 캐릭터 위치

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator; // 회전
};

// AEPCharacter에 링버퍼 저장 (서버에서만, private)
// 60Hz 서버: 16.7ms × 64 ≈ 1067ms
// 30Hz 서버: 33ms  × 64 ≈ 2133ms
// MaxLagCompWindow(200ms) 대비 충분한 마진 — 매 틱 기록이므로 틱레이트가 간격을 결정
static constexpr int32 MaxHitboxHistory = 64;

UPROPERTY()
TArray<FEPHitboxSnapshot> HitboxHistory;
int32 HistoryIndex = 0;
// HistoryTimer 없음 — 고정 간격 대신 매 틱 기록 (서버 틱레이트가 샘플링 간격을 결정)
```

#### 2) 히스토리 기록 (서버, 주기적)

```cpp
// AEPCharacter::Tick (EPCharacter.cpp)
void AEPCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!HasAuthority()) return;

    // 매 틱마다 기록 — 서버 틱레이트가 샘플링 간격을 결정한다.
    // 고정 100ms 간격은 ClientFireTime이 두 스냅샷 중간에 오면
    // 최대 50ms치 이동 오차(~150cm@30km/h)를 유발한다.
    SaveHitboxSnapshot();
}

void AEPCharacter::SaveHitboxSnapshot()
{
    FEPHitboxSnapshot Snapshot;
    // GetServerWorldTimeSeconds(): GameState가 복제하는 서버 기준 시간.
    // ClientFireTime과 동일 기준을 써야 리와인드 시각이 정확하게 맞는다.
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    Snapshot.ServerTime = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    Snapshot.Location   = GetActorLocation();
    Snapshot.Rotation   = GetActorRotation();

    if (HitboxHistory.Num() < MaxHitboxHistory)
        HitboxHistory.Add(Snapshot);
    else
    {
        HitboxHistory[HistoryIndex] = Snapshot;
        HistoryIndex = (HistoryIndex + 1) % MaxHitboxHistory;
    }
}
```

> **서버 틱레이트와 샘플링 간격:**
> 매 틱 기록으로 서버 틱레이트(30/60/128Hz)가 자동으로 샘플링 간격이 된다.
> MaxHitboxHistory=64로 30Hz에서 ~2초, 60Hz에서 ~1초 분량을 보관한다.
> MaxLagCompWindow(200ms) 대비 충분하다.

#### 3) 서버 시간 동기화

클라이언트의 `ClientFireTime`을 서버 시간으로 변환해야 한다:

```cpp
// GameState null 체크 필수 — 복제 타이밍에 따라 초기에 null일 수 있다
const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
float ServerTime = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();

// 클라이언트 → 서버 시간 변환
// 방법 1 (채택): 클라이언트도 GetServerWorldTimeSeconds() 사용
//   GameState가 서버 시간을 복제하므로 클라/서버 동일 기준 유지
//   → RequestFire에서 이 값을 ClientFireTime으로 전송

// 방법 2: RTT 기반 보정 (방법 1이 불가능할 때)
//   서버 시간 ≈ ClientFireTime + (RTT / 2)
//   RTT는 PlayerState->ExactPing * 4 / 1000 (ExactPing 단위: 0.25ms)
```

#### 4) Broad Phase → Multi-Rewind → Narrow Phase → 복구

`HandleHitscanFire`로 추출한다. `Server_Fire`뿐 아니라 GAS 어빌리티에서도 호출할 수 있다.

**단계 요약:**

```
1. Broad Phase  — GetHitscanCandidates()로 총알 경로 근방 캐릭터 선정
                   현재 O(N), 향후 Spatial Hash로 O(K) 교체 가능
2. Multi-Rewind — 후보 전체를 ClientFireTime 위치로 이동 + 현재 위치 저장
3. Narrow Phase — 모든 후보가 과거 위치에 있는 상태에서 정밀 LineTrace
4. 일괄 복구    — 후보 전체를 원래 위치로 복원
```

**Broad Phase — GetHitscanCandidates:**

```cpp
// 현재: O(N) AllPlayers 순회. 향후 Spatial Hash 교체 시 이 함수만 바꾼다.
// Directions 배열 지원: 산탄총 다중 방향 중 하나라도 후보 범위 내이면 포함
TArray<AEPCharacter*> UEPCombatComponent::GetHitscanCandidates(
    AEPCharacter* Owner,
    const FVector& Origin,
    const TArray<FVector>& Directions, // 단일 펠릿: {SpreadDir}, 산탄총: 다수
    float ClientFireTime) const
{
    TArray<AEPCharacter*> Candidates;

    for (TActorIterator<AEPCharacter> It(GetWorld()); It; ++It)
    {
        AEPCharacter* Char = *It;
        if (!Char || Char == Owner) continue;
        if (Char->GetHP() <= 0) continue; // 이미 사망한 캐릭터 — 리와인드 대상 제외

        // 과거 위치 기준으로 Broad Phase — 현재 위치 기준이면 이동한 대상을 놓친다
        const FEPHitboxSnapshot Snap = Char->GetSnapshotAtTime(ClientFireTime);
        const float CapsuleRadius    = Char->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float BroadPhaseRadius = CapsuleRadius + 50.f;

        // 하나라도 방향이 후보 범위 내이면 포함 (산탄총 다중 방향 지원)
        for (const FVector& Dir : Directions)
        {
            const FVector End = Origin + Dir * 10000.f;
            if (FMath::PointDistToSegment(Snap.Location, Origin, End) <= BroadPhaseRadius)
            {
                Candidates.Add(Char);
                break; // 이미 후보 — 나머지 방향 체크 불필요
            }
        }
    }
    return Candidates;
}
```

**FEPRewindEntry (EPCombatComponent.cpp 파일 스코프):**

```cpp
// AEPCharacter*에 의존하므로 EPTypes.h에 넣지 않는다.
// EPTypes.h → AEPCharacter 순환 의존을 막기 위해 .cpp 내부에 정의한다.
// 복제 불필요 — 리와인드 중 임시 사용 후 즉시 복구된다.
struct FEPRewindEntry
{
    AEPCharacter* Character     = nullptr;
    FVector       SavedLocation = FVector::ZeroVector;
    FRotator      SavedRotation = FRotator::ZeroRotator;
};
```

**HandleHitscanFire:**

```cpp
// 단일 펠릿(라이플·SMG): Server_Fire에서 { SpreadDir } 전달
// 산탄총(샷건): Server_Fire에서 N개 펠릿 방향 배열 전달
// → 리와인드 1회, LineTrace N회, 복구 1회로 처리 (리와인드를 N번 반복하지 않는다)
void UEPCombatComponent::HandleHitscanFire(
    AEPCharacter* Owner,
    const FVector& Origin,
    const TArray<FVector>& Directions, // 단일 펠릿: {SpreadDir}, 산탄총: 다수
    float ClientFireTime)
{
    // ── [Rewind Block] ──────────────────────────────────────────────────────
    // 무기/스킬 공통 인프라 — Chapter 4 GAS 어빌리티도 이 블록을 동일하게 사용한다.

    // 0. 리와인드 윈도우 클램프 (200ms 초과 = 조작으로 간주)
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    if (ServerNow - ClientFireTime > 0.2f)
        ClientFireTime = ServerNow;

    // 1. Broad Phase — 후보 선정 (교체 가능한 추상화 경계)
    const TArray<AEPCharacter*> Candidates =
        GetHitscanCandidates(Owner, Origin, Directions, ClientFireTime);

    // 2. 후보 전체 리와인드 + 현재 위치 저장
    TArray<FEPRewindEntry> RewindEntries;
    RewindEntries.Reserve(Candidates.Num());

    for (AEPCharacter* Char : Candidates)
    {
        FEPRewindEntry& Entry = RewindEntries.AddDefaulted_GetRef();
        Entry.Character     = Char;
        Entry.SavedLocation = Char->GetActorLocation();
        Entry.SavedRotation = Char->GetActorRotation();

        const FEPHitboxSnapshot Snap = Char->GetSnapshotAtTime(ClientFireTime);
        Char->SetActorLocationAndRotation(
            Snap.Location, Snap.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    }

    // 3. Narrow Phase — 모든 후보가 과거 위치에 있는 상태에서 N방향 레이캐스트
    // 산탄총: N개 펠릿 각각 LineTrace. 단일 펠릿: Directions.Num() == 1
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HitscanFire), false);
    Params.AddIgnoredActor(Owner);
    Params.AddIgnoredActor(EquippedWeapon);

    TArray<FHitResult> ConfirmedHits;
    for (const FVector& Dir : Directions)
    {
        const FVector End = Origin + Dir * 10000.f;
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_GameTraceChannel1, Params)
            && Cast<AEPCharacter>(Hit.GetActor()) != nullptr)
        {
            ConfirmedHits.Add(Hit);
        }
    }

    // 4. 후보 전체 일괄 복구 (반드시 Narrow Phase 직후, 순서 변경 금지)
    for (const FEPRewindEntry& Entry : RewindEntries)
    {
        Entry.Character->SetActorLocationAndRotation(
            Entry.SavedLocation, Entry.SavedRotation,
            false, nullptr, ETeleportType::TeleportPhysics);
    }

    // ── [Damage Block] ──────────────────────────────────────────────────────
    // Chapter 4 GAS 전환 시 이 블록을 GameplayEffectSpec + SetByCaller로 교체한다.
    // 산탄총: 맞은 펠릿 수만큼 각각 ApplyPointDamage (동일 대상 중복 적용 의도적 허용)
    for (const FHitResult& Hit : ConfirmedHits)
    {
        if (!Hit.GetActor()) continue;
        UGameplayStatics::ApplyPointDamage(
            Hit.GetActor(),
            EquippedWeapon->GetDamage(),
            (Hit.ImpactPoint - Origin).GetSafeNormal(), // 펠릿별 입사 방향
            Hit,                                         // BoneName → 부위 배율 (03_BoneHitbox)
            Owner->GetController(),
            Owner,
            UDamageType::StaticClass());
    }

    // ── [Effect Block] ───────────────────────────────────────────────────────
    // EquippedWeapon은 Damage Block 이후에도 유효해야 한다.
    // (무기 해제 엣지케이스 방어) 재검사 후 진입
    if (!EquippedWeapon || !EquippedWeapon->WeaponMesh) return;

    const FVector MuzzleLoc =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();

    // 발사자 클라이언트는 RequestFire에서 이미 총구 이펙트를 재생했다.
    // Multicast는 SimulatedProxy(다른 클라이언트)에게만 의미 있다.
    Multicast_PlayMuzzleEffect(MuzzleLoc);
    // 펠릿별 탄착 이펙트 — 산탄총은 N개, 단일 펠릿은 최대 1개
    for (const FHitResult& Hit : ConfirmedHits)
        Multicast_PlayImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal);
}
```

> **GAS 전환 포인트:**
> Rewind Block은 Chapter 4에서도 그대로 재사용된다.
> Damage Block의 `ApplyPointDamage`만 `GameplayEffectSpec` 적용으로 교체하면 된다.
> Effect Block은 변경 없음.

> **🥇 확장 경로 (50인 이상):**
> `GetHitscanCandidates`를 Spatial Hash/Grid 구현으로 교체하면 된다.
> 함수 시그니처는 동일하므로 `HandleHitscanFire` 본체는 수정하지 않는다.

**왜 200ms인가:**
- 일반적인 게임에서 허용하는 최대 핑 = 150~200ms
- 200ms를 초과하는 ClientFireTime은 지연이 아닌 조작으로 간주
- 이 값은 게임 특성에 따라 조정 (배틀로얄은 더 관대, 경쟁전은 더 엄격)

**클램프 vs 거부:**
- 현재: 초과 시 `ClientFireTime = ServerNow` → 리와인드 없이 현재 위치 기준 판정
- 실무(Valorant, CS2): 초과 시 **판정 자체를 거부(early return)**
- 포트폴리오에서는 클램프 방식으로 구현해도 무방하나, 거부 방식이 더 보수적으로 안전하다

#### 5) 스냅샷 보간

정확한 시점의 스냅샷이 없을 수 있으므로 두 스냅샷 사이를 보간.

> **O(N) 탐색 한계:**
> 링버퍼(64개)를 선형 탐색한다. 버퍼를 시간 순으로 정렬된 상태로 유지하면
> 이진 탐색 O(log N)으로 개선 가능하다. 64개 규모에서는 무시해도 되지만
> 본 단위 히스토리(03_BoneHitbox)로 확장 시 버퍼 크기가 커지면 고려해야 한다.

```cpp
// AEPCharacter::GetSnapshotAtTime (EPCharacter.cpp)
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

    // 히스토리 없으면 현재 위치 반환
    if (!Before && !After)
    {
        FEPHitboxSnapshot Current;
        Current.ServerTime = TargetTime;
        Current.Location   = GetActorLocation();
        Current.Rotation   = GetActorRotation();
        return Current;
    }
    if (!Before) return *After;
    if (!After)  return *Before;
    if (Before == After) return *Before;

    // 두 스냅샷 사이 보간
    const float Range = After->ServerTime - Before->ServerTime;
    // KINDA_SMALL_NUMBER guard: Range == 0 (같은 시각 스냅샷 2개) 이면 나눗셈 생략
    const float Alpha = Range > KINDA_SMALL_NUMBER
        ? FMath::Clamp((TargetTime - Before->ServerTime) / Range, 0.f, 1.f)
        : 0.f;

    FEPHitboxSnapshot Result;
    Result.ServerTime = TargetTime;
    Result.Location   = FMath::Lerp(Before->Location, After->Location, Alpha);
    // FMath::Lerp는 각도 랩어라운드(-179° ↔ 181°) 문제 발생
    // Slerp(구면 선형 보간)으로 올바른 회전 보간
    Result.Rotation = FQuat::Slerp(
        Before->Rotation.Quaternion(),
        After->Rotation.Quaternion(),
        Alpha).Rotator();
    return Result;
}
```

#### 6) 투사체 스폰 및 히트 처리

투사체는 속도에 따라 클라이언트 처리 방식이 다르다.

| | ProjectileFast (소총탄, 권총탄) | ProjectileSlow (수류탄, 로켓) |
|---|---|---|
| 서버 Actor | bReplicates = **false** | bReplicates = **true** |
| 클라 렌더링 | Multicast RPC → 로컬 코스메틱 스폰 | 복제된 Actor 위치 추적 |
| 이유 | 870m/s → 복제가 의미없음 | 느려서 복제 지연 무시 가능, 궤적 회피 필요 |

**데이터 흐름:**

```
[ProjectileFast]
서버: SpawnActor(bReplicates=false) → 시뮬레이션 → 히트 → 데미지
                                                        ↓
                                          Multicast_PlayImpactEffect
클라: Multicast_SpawnCosmeticProjectile → 로컬 코스메틱 스폰 → 궤적 렌더링
                                                                    ↑ 이펙트 수신

[ProjectileSlow]
서버: SpawnActor(bReplicates=true) → 자동 복제 → 히트 → 데미지
클라:                  복제 Actor 수신 → 위치 추적 → 렌더링
```

**HandleProjectileFire:**

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
    // ProjectileFast Blueprint → bReplicates = false
    // ProjectileSlow Blueprint → bReplicates = true (자동 복제)
    AEPProjectile* Proj = GetWorld()->SpawnActor<AEPProjectile>(
        EquippedWeapon->WeaponDef->ProjectileClass,
        MuzzleLoc,
        Direction.GetSafeNormal().Rotation(),
        SpawnParams);

    if (!Proj) return;

    Proj->Initialize(EquippedWeapon->GetDamage(), Direction);

    // ProjectileFast: 발사자 클라는 RequestFire에서 이미 로컬 스폰 완료.
    //                 Multicast는 다른 클라이언트(SimulatedProxy)만을 위한 것.
    // ProjectileSlow: 복제 Actor가 클라 렌더링을 담당하므로 불필요.
    if (EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
        Multicast_SpawnCosmeticProjectile(MuzzleLoc, Direction.GetSafeNormal());

    // 총구 이펙트도 발사자 클라는 RequestFire에서 이미 재생.
    // Multicast_PlayMuzzleEffect는 다른 클라이언트만을 위한 것.
    Multicast_PlayMuzzleEffect(MuzzleLoc);
}
```

**Multicast_SpawnCosmeticProjectile:**

```cpp
// 클라이언트가 받는 것: 발사 시점의 위치 + 방향뿐
// 이후 동일한 물리 공식으로 로컬에서 궤적을 독립 시뮬레이션한다
void UEPCombatComponent::Multicast_SpawnCosmeticProjectile_Implementation(
    const FVector_NetQuantize& MuzzleLoc,
    const FVector_NetQuantizeNormal& Direction)
{
    // 서버는 실제 Actor 보유. 발사자(AutonomousProxy)는 RequestFire에서 이미 스폰.
    // 두 경우 모두 여기서 다시 스폰하면 중복 → early return
    if (HasAuthority()) return;
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar && OwnerChar->IsLocallyControlled()) return;

    // EquippedWeapon은 클라이언트에서 아직 복제 안 됐을 수 있다.
    // Multicast가 무기 복제보다 먼저 도착하면 크래시 → 반드시 null 체크
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef
        || !EquippedWeapon->WeaponDef->ProjectileClass) return;

    // 충돌/데미지 없이 궤적 렌더링만 수행
    // 실무에서는 Niagara Ribbon/Beam으로 대체 가능 (Actor 스폰 비용 없음)
    AEPProjectile* Cosmetic = GetWorld()->SpawnActor<AEPProjectile>(
        EquippedWeapon->WeaponDef->ProjectileClass,
        MuzzleLoc, Direction.Rotation());

    if (Cosmetic)
        Cosmetic->SetCosmeticOnly(); // 충돌/데미지 비활성화, 궤적 렌더링만
}

// AEPProjectile::SetCosmeticOnly() — 구현 개요
// 1. CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision)
// 2. MovementComp->ProjectileGravityScale = 해당 무기 설정 유지 (궤적은 동일해야 함)
// 3. bIsCosmeticOnly = true 플래그 → OnProjectileHit에서 HasAuthority() 이전에 early return
// 실무에서는 이 클래스를 상속해 AEPCosmeticProjectile로 분리하거나,
// Niagara Beam으로 완전히 대체한다.
```

**AEPProjectile::OnProjectileHit:**

투사체는 랙 보상 없이 서버 현재 시각 기준으로 즉시 판정한다.

```cpp
void AEPProjectile::OnProjectileHit(
    UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, FVector, const FHitResult& Hit)
{
    if (!HasAuthority()) return; // 서버만 데미지 처리 (코스메틱 인스턴스는 여기 진입 안 함)

    // 리와인드 불필요 — 투사체 이동 시간이 지연을 흡수
    // [Chapter 4 GAS 전환 포인트] ApplyPointDamage → GameplayEffectSpec
    if (OtherActor)
    {
        UGameplayStatics::ApplyPointDamage(
            OtherActor, BaseDamage,
            LaunchDir, // Initialize(float, FVector)로 주입받은 발사 방향
            Hit, GetInstigatorController(), GetInstigator(),
            UDamageType::StaticClass());
    }

    Destroy();
}
```

> **ProjectileSlow 클라이언트 예측 gap (Chapter 3 범위 밖):**
> 서버 스폰 Actor가 클라이언트에 처음 복제될 때, 스폰 이후 경과 시간만큼
> 궤적이 앞으로 점프한 것처럼 보인다 (망령 문제).
> 실무에서는 클라이언트도 로컬 예측 스폰 후 서버 복제 Actor와 조율하지만,
> 이 처리는 Chapter 3 범위 밖이며 언급만 한다.

---

## 6. UE5 내장 지원 vs 직접 구현

| 기능 | UE5 내장 | 비고 |
| --- | --- | --- |
| 이동 예측/보정 | O (CMC) | 자동 처리, 확장만 하면 됨 |
| 히트스캔 랙 보상 | X | 직접 구현 (HandleHitscanFire) |
| 고속 투사체 (ProjectileFast) | X | 서버 시뮬 + 클라 코스메틱 분리 |
| 저속 투사체 (ProjectileSlow) | O (Actor 복제) | bReplicates=true로 자동 처리 |
| GAS 어빌리티 예측 | O (FPredictionKey) | LocalPredicted — Chapter 4에서 구현 |
| GAS 히트스캔/투사체 스킬 | X | Rewind Block 재사용 — Chapter 4에서 연결 |

Lag Compensation은 UE5가 범용으로 제공하지 않는다.
이를 직접 구현하고 무기/스킬 양쪽에서 재사용하는 구조가 포트폴리오 차별화 포인트다.

---

## 7. EmploymentProj 3단계 구현 체크리스트

- [ ] 탄도 방식 분리
  - [ ] `EEPBallisticType` enum 추가 (Hitscan / ProjectileFast / ProjectileSlow)
  - [ ] `WeaponDefinition`에 `BallisticType` + `ProjectileClass` 추가
  - [ ] `Server_Fire`에서 `HandleHitscanFire` / `HandleProjectileFire` 분기
- [ ] Hit Validation (히트스캔)
  - [ ] `Server_Fire` RPC 시그니처 확장 (FVector_NetQuantize + ClientFireTime)
  - [ ] `Server_Fire` 서버 검증 3단계: FireRate → CanFire → Origin drift
  - [ ] `LastServerFireTime` 멤버 추가 (서버 전용, 발사 속도 검증용)
  - [ ] `AEPWeapon::Fire` 오버로드: 단일 방향 반환 vs 산탄총 배열 반환 (결정론적 RNG)
  - [ ] `HandleHitscanFire` 구현 (Multi-Rewind → N방향 Narrow Phase → 복구)
  - [ ] `GetHitscanCandidates()` 구현 (Broad Phase, 사망 캐릭터 필터, O(N) → O(K) 확장 경계)
  - [ ] 입력 검증은 구현부 내부에서 처리 (UE5는 `_Validate` 대신 서버 내부 조건문 사용)
- [ ] Lag Compensation
  - [ ] `FEPHitboxSnapshot` 구조체 정의 (EPTypes.h)
  - [ ] `FEPRewindEntry` 구조체 정의 (EPCombatComponent.cpp 파일 스코프)
  - [ ] 서버에서 매 틱 히스토리 기록 (링버퍼 MaxHitboxHistory=64, AEPCharacter::Tick)
  - [ ] `GetSnapshotAtTime()` 보간 함수 (KINDA_SMALL_NUMBER 가드 포함)
  - [ ] 리와인드 → N방향 레이캐스트 → 복구 흐름 (HandleHitscanFire 내부)
  - [ ] `GetHitscanCandidates()` 다중 방향(Directions 배열) 지원 — 산탄총/단일 공용
  - [ ] `HandleHitscanFire()` 다중 펠릿 지원 — 1회 리와인드, N회 LineTrace, 1회 복구
- [ ] 투사체 지원
  - [ ] `AEPProjectile` 클래스 추가 (`UProjectileMovementComponent`, `SetCosmeticOnly()`)
  - [ ] `HandleProjectileFire` 구현 (ProjectileFast/Slow 분기)
  - [ ] `Multicast_SpawnCosmeticProjectile` RPC (ProjectileFast 클라 렌더링)
  - [ ] `OnProjectileHit`에서 서버 권한 데미지 처리 (`HasAuthority()` 보장)
- [ ] Reconciliation (사격 결과 피드백)
  - [ ] `Client_OnHitConfirmed` RPC로 히트 결과 전달
  - [ ] 히트마커/데미지 숫자 UI 표시
- [ ] Multicast 이펙트
  - [ ] `Multicast_PlayMuzzleEffect` (총구 이펙트 + 사운드)
  - [ ] `Multicast_PlayImpactEffect` (탄착 이펙트)

---

## 8. 참고 자료

### 공식 문서
- Networking Overview: `dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine`
- Character Movement Component: `dev.epicgames.com/documentation/en-us/unreal-engine/character-movement-component-in-unreal-engine`

### UE5 소스 코드
- `Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h` - FSavedMove, 예측/보정 로직
- `Engine/Source/Runtime/Engine/Private/CharacterMovementComponent.cpp` - 구현 상세

### 검색 키워드
- "UE5 server rewind lag compensation"
- "UE5 hitscan server authoritative"
- "UE5 hitbox history ring buffer"
- "UE5 FVector_NetQuantize"
- "UE5 client prediction reconciliation explained"
- "Overwatch networking GDC" (원리 이해에 좋은 외부 자료)
- "Valorant netcode 128 tick" (히트스캔 랙 보상 사례)

### 참고할 만한 외부 자료
- Gabriel Gambetta의 "Fast-Paced Multiplayer" 시리즈 (언어 무관 원리 설명)
- GDC Vault의 "Overwatch Gameplay Architecture and Netcode" (서버 리와인드 실전 사례)
- Valve의 "Source Multiplayer Networking" 문서 (Lag Compensation 원조 설명)
