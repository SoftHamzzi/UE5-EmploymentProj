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
1. 발사 → 로컬에서 총구 이펙트/사운드 즉시 재생 (예측, RTT 없음)
2. Server RPC로 발사 정보 전송

[서버]
3. 히트 판정 수행 (Lag Compensation으로 클라 발사 시점 기준)
4. 결과를 클라이언트에 전송 (Client RPC 또는 Multicast)

[클라이언트]
5. 서버 결과에 맞게 후처리
   - 히트 확인: 히트마커, 데미지 숫자 표시
   - 미스: 예측 이펙트는 이미 재생됨 (문제없음, cosmetic)
```

**현재 구현 수준 (BoneHitbox 완료):**
- ✅ 총구 이펙트/사운드: `RequestFire`에서 즉시 로컬 재생 → RTT 없음
- ✅ 다른 클라이언트: `Multicast_PlayMuzzleEffect`에서 `IsLocallyControlled()` 체크로 중복 방지
- ✅ 탄착 이펙트: 서버 판정 후 `Multicast_PlayImpactEffect` (탄착 위치 확정 후 브로드캐스트)
- ✅ 피격자 히트마커: `Client_PlayHitConfirmSound`로 공격자에게 전달
- 미완: 히트마커 UI / 데미지 숫자 표시 (GAS 단계에서 구현)

---

## 4. Hit Validation (서버 권한 히트 판정)

### 원칙

**클라이언트는 "맞았다"고 주장할 수 없다.**
클라이언트는 "이 시점에, 이 위치에서, 이 방향으로 쐈다"만 전송.
서버가 레이캐스트로 판정한다.

### RPC 설계

`Server_Fire`는 `AEPCharacter`가 아닌 `UEPCombatComponent`에 위치한다.
전투 관련 로직을 Character에서 분리한 설계 의도를 유지.

✅ **현재 구현 (완료):**

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector_NetQuantize&       Origin,
    const FVector_NetQuantizeNormal& Direction,
    float                            ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;

    AEPCharacter* Owner = GetOwnerCharacter();
    if (!Owner) return;

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

    // 3. Origin 위치 검증: 벽 너머 조작 방지 (200cm 여유)
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

- ✅ `FVector_NetQuantize` / `FVector_NetQuantizeNormal`
- ✅ `ClientFireTime` 파라미터 + SSR `ConfirmHitscan` 위임
- ✅ `HandleHitscanFire` 분리 + `ApplyPointDamage` + 부위 배율
- ✅ `LastServerFireTime` 서버 검증 3단계
- ✅ Origin 위치 검증
- ✅ `EEPBallisticType` switch + `HandleProjectileFire` 분기

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

// 산탄총용 펠릿 수 — 1이면 단일(라이플/SMG), 2 이상이면 다중(샷건)
// 서버가 동일 시드로 이 수만큼 독립 계산 → 클라이언트가 방향 조작 불가
UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ballistics",
    meta = (EditCondition = "BallisticType == EEPBallisticType::Hitscan", ClampMin = 1))
int32 PelletCount = 1;

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
        EquippedWeapon->Fire(Direction, ClientFireTime, PelletDirs); // 탄약 차감 + 결정론적 RNG 펠릿 생성
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

> **산탄총 펠릿 생성 — 서버 독립 생성 방식:**
> RPC로 클라이언트의 펠릿 방향을 그대로 수신하는 방식은
> **핵 클라이언트가 모든 펠릿을 조준점으로 보내는 것을 막을 수 없어** 사용하지 않는다.
> 서버가 독립적으로 펠릿 방향을 생성한다.
>
> **스프레드 방식:** `FMath::VRandCone(AimDir, HalfAngleRad)`
> — 원뿔 내부 균등 분포. `HalfAngleRad`는 반각(전체 스프레드의 절반).
> 서버 단독 판정이므로 클라와 방향이 다소 달라도 비주얼 차이에 그친다.
>
> **구현 포인트:** `AEPWeapon::Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets)`
> — `WeaponDef->PelletCount`, `WeaponDef->BaseSpread`로 N개 방향 생성.
> 단일 펠릿 무기는 `PelletCount = 1`로 설정하면 동일 함수를 공유한다.

**RequestFire (클라이언트 측):**

✅ **현재 구현 (BoneHitbox 완료):**

```cpp
void UEPCombatComponent::RequestFire(const FVector& Origin, const FVector& Direction, float ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;

    // --- 클라이언트 사전 검증 ---
    if (EquippedWeapon->CurrentAmmo <= 0) return;

    // 클라이언트 측 연사속도 체크 (서버도 독립 검증 — 이중 방어)
    const float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
    const float CurrentTime  = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LocalLastFireTime < FireInterval) return;
    LocalLastFireTime = CurrentTime;

    AEPCharacter* Owner = GetOwnerCharacter();
    if (Owner && Owner->IsLocallyControlled())
    {
        // Muzzle: RTT 없이 즉시 재생. Multicast_PlayMuzzleEffect의 IsLocallyControlled() 체크와 짝을 이룬다.
        const FVector MuzzleLocation =
            (EquippedWeapon->WeaponMesh && EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket")))
            ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
            : EquippedWeapon->GetActorLocation();
        PlayLocalMuzzleEffect(MuzzleLocation);
    }

    Server_Fire(Origin, Direction, ClientFireTime);

    if (Owner && Owner->IsLocallyControlled())
    {
        // 반동 예측 — 서버 확인 전에 즉시 적용 (체감 우선)
        float Pitch = EquippedWeapon->GetRecoilPitch();
        float Yaw = FMath::RandRange(-EquippedWeapon->GetRecoilYaw(), EquippedWeapon->GetRecoilYaw());
        Owner->AddControllerPitchInput(-Pitch);
        Owner->AddControllerYawInput(Yaw);
    }
}
```

> `ClientFireTime`은 `AEPCharacter::Input_Fire`에서 `GS->GetServerWorldTimeSeconds()`로 획득해 전달한다.
> HitboxHistory 기록도 동일 기준(`GetServerWorldTimeSeconds()`)을 쓰므로 리와인드 시각이 정확하다.

✅ **ProjectileFast 코스메틱 즉시 스폰 (완료):**

```cpp
    // ProjectileFast: 발사자는 Multicast 왕복을 기다리지 않고 즉시 코스메틱 스폰.
    // Multicast_SpawnCosmeticProjectile의 IsLocallyControlled() 체크와 짝을 이룬다.
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
```

`SpawnLocalCosmeticProjectile`은 코스메틱 스폰 로직을 분리한 함수.
`Multicast_SpawnCosmeticProjectile`도 내부적으로 이 함수를 호출한다.

`HandleHitscanFire`를 독립 함수로 추출하는 이유:
**GAS 어빌리티의 히트스캔 스킬도 동일 함수를 호출**할 수 있어야 하기 때문이다.
랙 보상 인프라(`GetSnapshotAtTime`, 히스토리 배열)는 무기·스킬 구분 없이 공유된다.

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

#### 1) ✅ 히트박스 히스토리 (BoneHitbox 완료)

실제 구현은 `AEPCharacter`가 아닌 `UEPServerSideRewindComponent`에 있다.

```cpp
// EPTypes.h (현재 — 본 단위 BoneHitbox 구조)
USTRUCT()
struct FEPBoneSnapshot
{
    GENERATED_BODY()
    UPROPERTY() FName      BoneName;
    UPROPERTY() FTransform WorldTransform; // 서버 시뮬 기준 본 월드 Transform
};

USTRUCT()
struct FEPHitboxSnapshot
{
    GENERATED_BODY()
    UPROPERTY() float   ServerTime = 0.f;
    UPROPERTY() FVector Location   = FVector::ZeroVector; // Broad Phase용 루트 위치
    UPROPERTY() TArray<FEPBoneSnapshot> Bones;            // Narrow Phase 리와인드용
};
```

> **캡슐 기반(Location + Rotation)이 아닌 본 단위 배열 방식이 최종 구조다.**
> `FBodyInstance::SetBodyTransform`으로 물리 바디를 직접 이동시켜 리와인드한다.

#### 2) ✅ 히스토리 기록 (BoneHitbox 완료)

`AEPCharacter::Tick`이 아닌 `UEPServerSideRewindComponent::TickComponent`에서 처리한다.

```cpp
// CMC::OnMovementUpdated(TickDispatch) → OnServerMoveProcessed 델리게이트 → SSR pending 저장
// UEPServerSideRewindComponent::TickComponent(TG_PostPhysics) — pending이 있으면 SaveHitboxSnapshot() 커밋
// SaveHitboxSnapshot() — GetMesh()->GetBoneTransform(BoneIndex)으로 본 Transform 기록 (물리 반영 완료 시점)
// HitboxHistory — 시간 오름차순 TArray. [0] 오래됨, [Last] 최신
// GetSnapshotAtTime() — per-bone FTransform::BlendWith 보간
```

> `VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones`가
> `AEPCharacter` 생성자에 설정되어 있어야 서버에서 본 Transform이 갱신된다.

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

#### 4) ✅ Broad Phase → Multi-Rewind → Narrow Phase → 복구 (BoneHitbox 완료)

`HandleHitscanFire`는 `UEPCombatComponent`에, 실제 리와인드 로직은 `UEPServerSideRewindComponent`에 분리되어 있다.

**단계 요약:**

```
1. Broad Phase  — SSR::GetHitscanCandidates()로 총알 경로 근방 캐릭터 선정 (O(N), 향후 Spatial Hash 교체 가능)
2. Multi-Rewind — SSR::ConfirmHitscan 내부에서 FBodyInstance::SetBodyTransform으로 물리 바디 이동 + 저장
3. Narrow Phase — 모든 후보가 과거 위치에 있는 상태에서 정밀 LineTrace
4. 일괄 복구    — FBodyInstance::SetBodyTransform으로 원래 본 Transform 복원
```

**GetHitscanCandidates — `UEPServerSideRewindComponent`에 위치 (CombatComponent 아님):**

```cpp
// SSR 컴포넌트 내부. 향후 Spatial Hash로 교체 시 이 함수만 바꾼다.
// Directions 배열 지원: 산탄총 다중 방향 중 하나라도 후보 범위 내이면 포함
TArray<AEPCharacter*> UEPServerSideRewindComponent::GetHitscanCandidates(...) const;
```

**FEPRewindEntry — SSR::ConfirmHitscan 내부 (EPServerSideRewindComponent.cpp 파일 스코프):**

```cpp
// 캡슐 기반(Location+Rotation)이 아닌 본 단위 Transform 저장
struct FEPRewindEntry
{
    AEPCharacter*          Character  = nullptr;
    TArray<FEPBoneSnapshot> SavedBones; // 리와인드 전 각 본의 WorldTransform
};
```

**HandleHitscanFire — ✅ 현재 구현 (SSR 위임 패턴):**

```cpp
// 단일 펠릿(라이플·SMG): { SpreadDir } 배열 전달
// 산탄총: N개 펠릿 방향 배열 전달 → 리와인드 1회, LineTrace N회, 복구 1회
void UEPCombatComponent::HandleHitscanFire(
    AEPCharacter* Owner, const FVector& Origin,
    const TArray<FVector>& Directions, float ClientFireTime)
{
    if (!Owner || !Owner->GetServerSideRewindComponent()) return;

    // [Rewind Block] — SSR에 완전 위임 (Broad Phase + 리와인드 + Narrow Trace + 복구 + 디버그)
    TArray<FHitResult> ConfirmedHits;
    Owner->GetServerSideRewindComponent()->ConfirmHitscan(
        Owner, EquippedWeapon, Origin, Directions, ClientFireTime, ConfirmedHits);

    // [Damage Block] — GAS 전환 시 GameplayEffectSpec + SetByCaller로 교체
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

> **GAS 전환 포인트:** Rewind Block(SSR::ConfirmHitscan)은 Chapter 4에서도 그대로 재사용된다.
> Damage Block의 `ApplyPointDamage`만 `GameplayEffectSpec`으로 교체하면 된다.

> **확장 경로 (50인 이상):** `SSR::GetHitscanCandidates`를 Spatial Hash/Grid로 교체하면 된다.
> `HandleHitscanFire` 본체는 수정하지 않는다 — SSR 내부만 교체.

**왜 200ms인가:**
- 일반적인 게임에서 허용하는 최대 핑 = 150~200ms
- 200ms를 초과하는 ClientFireTime은 지연이 아닌 조작으로 간주
- 이 값은 게임 특성에 따라 조정 (배틀로얄은 더 관대, 경쟁전은 더 엄격)

**클램프 vs 거부:**
- 현재: 초과 시 `ClientFireTime = ServerNow` → 리와인드 없이 현재 위치 기준 판정
- 실무(Valorant, CS2): 초과 시 **판정 자체를 거부(early return)**
- 포트폴리오에서는 클램프 방식으로 구현해도 무방하나, 거부 방식이 더 보수적으로 안전하다

#### 5) ✅ 스냅샷 보간 (BoneHitbox 완료)

`AEPCharacter::GetSnapshotAtTime`이 아닌 `UEPServerSideRewindComponent::GetSnapshotAtTime`에 구현되어 있다.

> **O(N) 탐색:** 시간 오름차순 배열(링버퍼 아님)을 선형 탐색한다.
> 배열 순서가 보장되어 있어 GetSnapshotAtTime 탐색 방향이 예측 가능하다.

```cpp
// UEPServerSideRewindComponent::GetSnapshotAtTime (EPServerSideRewindComponent.cpp)
// — Before/After 스냅샷 탐색 후 per-bone FTransform::BlendWith 보간
// — Location은 FMath::Lerp, Bones는 FTransform::BlendWith (회전 포함)
FEPHitboxSnapshot UEPServerSideRewindComponent::GetSnapshotAtTime(float TargetTime) const;
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
        (EquippedWeapon->WeaponMesh && EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket")))
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

    // Multicast_PlayMuzzleEffect는 Server_Fire_Implementation에서 switch 블록 이후
    // 한 번만 호출한다. 여기서 중복 호출하면 ProjectileFast/Slow에서 두 번 재생된다.
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
    // UActorComponent는 HasAuthority() 없음 → GetOwner()를 통해 접근
    if (GetOwner()->HasAuthority()) return;
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
    if (bIsCosmeticOnly) return; // 코스메틱 인스턴스는 즉시 차단
    if (!HasAuthority()) return; // 서버만 데미지 처리

    // [Chapter 4 GAS 전환 포인트] ApplyPointDamage → GameplayEffectSpec
    if (OtherActor)
    {
        UGameplayStatics::ApplyPointDamage(
            OtherActor, BaseDamage,
            LaunchDir,
            Hit, GetInstigatorController(), GetInstigator(),
            UDamageType::StaticClass());
    }

    Destroy();
}
```

`Initialize()`에서 발사자 본인을 충돌 무시 목록에 추가한다:
```cpp
void AEPProjectile::Initialize(float InDamage, const FVector& InDirection)
{
    BaseDamage = InDamage;
    LaunchDir  = InDirection.GetSafeNormal();

    // 발사자 본인 충돌 무시
    if (AActor* MyInstigator = GetInstigator())
        CollisionComp->IgnoreActorWhenMoving(MyInstigator, true);
}
```

> **투사체 Hit Reg 개선 (GAS 이후):**
> 현재는 서버 현재 시각 기준으로 물리 충돌 판정한다.
> GAS 단계에서 `Predicted Projectile Path` 방식으로 개선 예정:
> ClientFireTime 기반 리와인드 구간 LineTrace + 이후 정방향 시뮬레이션.
> 자세한 내용: `DOCS/Mine/Proj.md`

> **ProjectileSlow 클라이언트 예측 gap (GAS 이후):**
> 서버 스폰 Actor가 클라이언트에 처음 복제될 때, 스폰 이후 경과 시간만큼
> 궤적이 앞으로 점프한 것처럼 보인다 (망령 문제).
> GAS 단계에서 Predicted Projectile Path와 함께 처리 예정.

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

- [x] 탄도 방식 분리
  - [x] `EEPBallisticType` enum 추가 (Hitscan / ProjectileFast / ProjectileSlow)
  - [x] `WeaponDefinition`에 `BallisticType` + `ProjectileClass` + `PelletCount` + `SpreadDistributionCurve` 추가
  - [x] `Server_Fire`에서 `HandleHitscanFire` / `HandleProjectileFire` 분기 (switch 구조)
- [x] Hit Validation (히트스캔)
  - [x] `Server_Fire` RPC 시그니처: `FVector_NetQuantize` + `FVector_NetQuantizeNormal` + `ClientFireTime`
  - [x] `Server_Fire` 서버 검증 3단계: FireRate(`LastServerFireTime`) → CanFire → Origin drift
  - [x] `LastServerFireTime` 멤버 추가 (서버 전용, 발사 속도 검증용)
  - [x] `AEPWeapon::Fire` 오버로드: `Fire(AimDir, ClientFireTime, OutPellets)` — UCurveFloat + FindBestAxisVectors + 구면좌표
  - [x] `HandleHitscanFire` 구현 (SSR::ConfirmHitscan 위임 + Damage Block)
  - [x] `GetHitscanCandidates()` 구현 (SSR 내부, Broad Phase, 사망 필터)
- [x] Lag Compensation
  - [x] `FEPHitboxSnapshot` / `FEPBoneSnapshot` 구조체 (EPTypes.h)
  - [x] `FEPRewindEntry` (SSR::ConfirmHitscan 내부, SavedBones 배열)
  - [x] CMC 델리게이트 + PostPhysics pending 패턴으로 히스토리 기록 (SSR::TickComponent)
  - [x] `GetSnapshotAtTime()` per-bone 보간 (SSR 내부, FTransform::BlendWith)
  - [x] 리와인드(FBodyInstance::SetBodyTransform) → Narrow Trace → 복구 흐름
  - [x] `GetHitscanCandidates()` Directions 배열 지원 — 산탄총/단일 공용
  - [x] `HandleHitscanFire()` 다중 펠릿 지원 — 1회 리와인드, N회 LineTrace, 1회 복구
- [x] 투사체 지원
  - [x] `AEPProjectile` 클래스 추가 (`UProjectileMovementComponent`, `SetCosmeticOnly()`, `IgnoreActorWhenMoving`)
  - [x] `HandleProjectileFire` 구현 (ProjectileFast/Slow 분기)
  - [x] `SpawnLocalCosmeticProjectile` 함수 (코스메틱 스폰 로직 분리)
  - [x] `Multicast_SpawnCosmeticProjectile` RPC (ProjectileFast 다른 클라이언트 렌더링)
  - [x] `OnProjectileHit`에서 서버 권한 데미지 처리 (`bIsCosmeticOnly` + `HasAuthority()`)
  - [x] `RequestFire`에 ProjectileFast 코스메틱 즉시 스폰
- [ ] Reconciliation (사격 결과 피드백)
  - [ ] `Client_OnHitConfirmed` RPC로 히트 결과 전달 (GAS 단계)
  - [ ] 히트마커/데미지 숫자 UI 표시 (GAS 단계)
- [ ] 투사체 Hit Reg 개선 (GAS 이후)
  - [ ] `ClientFireTime` 기반 리와인드 구간 LineTrace
  - [ ] 이후 정방향 시뮬레이션 + CMC 업데이트 시 충돌 체크
  - [ ] 자세한 내용: `DOCS/Mine/Proj.md`
- [x] Multicast 이펙트
  - [x] `Multicast_PlayMuzzleEffect` (발사자 즉시 예측 + IsLocallyControlled 중복 방지)
  - [x] `Multicast_PlayImpactEffect` (HandleHitscanFire 내부 확정 히트마다 호출)

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
