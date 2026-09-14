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

**현재 코드의 한계 (3단계에서 개선):**
- `const FVector&` → `FVector_NetQuantize`로 교체 시 대역폭 절감
- `ClientFireTime` 파라미터 추가로 Lag Compensation 활성화
- `ApplyDamage` → `ApplyPointDamage`로 교체 시 부위별 데미지 가능

### FVector_NetQuantize / FVector_NetQuantizeNormal

| 타입 | 용도 | 정밀도 |
|------|------|--------|
| `FVector_NetQuantize` | 위치 | 소수점 1자리 (0.1 단위, 게임에서 충분) |
| `FVector_NetQuantizeNormal` | 방향 (정규화 벡터) | 16비트 각도 (65536 방향, 0.005도 정밀도) |

일반 FVector 대비 대역폭을 크게 절감한다.

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

### 구현 구조

#### 1) 히트박스 히스토리 링버퍼

```cpp
// EPTypes.h에 정의 (본 단위 확장은 03_BoneHitbox.md 참고)
// 캡슐 기준 기본 구조 (개념 설명용)
USTRUCT()
struct FEPHitboxSnapshot
{
    GENERATED_BODY()

    float ServerTime = 0.f;    // 기록 시점의 서버 시간
    FVector Location;          // 캐릭터 위치
    FRotator Rotation;         // 회전
};

// AEPCharacter에 링버퍼 저장 (서버에서만, private)
static constexpr int32 MaxHitboxHistory = 20;  // ~2초치 (100ms 간격)

TArray<FEPHitboxSnapshot> HitboxHistory;
int32 HistoryIndex = 0;
float HistoryTimer = 0.f;
```

#### 2) 히스토리 기록 (서버, 주기적)

```cpp
// AEPCharacter::Tick (EPCharacter.cpp)
void AEPCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority())
    {
        HistoryTimer += DeltaTime;
        if (HistoryTimer >= 0.1f)  // 100ms 간격
        {
            HistoryTimer = 0.f;
            SaveHitboxSnapshot();
        }
    }
}

void AEPCharacter::SaveHitboxSnapshot()
{
    FEPHitboxSnapshot Snapshot;
    Snapshot.ServerTime = GetWorld()->GetTimeSeconds();
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

> **기록 간격을 100ms로 설정한 이유:**
> 200ms 윈도우 / 100ms 간격 = 최소 2개 스냅샷 사이를 보간하게 됨.
> 빠르게 움직이는 캐릭터에서 오차가 커질 수 있으므로, 실무에서는 33~50ms 간격을 사용.
> 포트폴리오 수준에서는 100ms로 개념 확인 후 필요 시 간격을 줄여 정밀도 향상.

#### 3) 서버 시간 동기화

클라이언트의 `ClientFireTime`을 서버 시간으로 변환해야 한다:

```cpp
// 서버 시간은 GameState에서 가져올 수 있음
float ServerTime = GetWorld()->GetGameState<AGameStateBase>()->GetServerWorldTimeSeconds();

// 클라이언트 → 서버 시간 변환
// 방법 1: 클라이언트가 서버 시간을 기준으로 보냄
//   클라이언트에서도 GetServerWorldTimeSeconds()를 사용하면 동기화된 시간을 얻을 수 있음
//   (GameState가 서버 시간을 복제하므로)

// 방법 2: RTT 기반 보정
// 서버 시간 = ClientFireTime + (RTT / 2)
// RTT는 PlayerState->ExactPing * 2 / 1000 으로 근사
```

#### 4) 리와인드 → 레이캐스트 → 복구

```cpp
// UEPCombatComponent::Server_Fire_Implementation (EPCombatComponent.cpp)
// Lag Compensation 적용 버전 (ClientFireTime 파라미터 추가 후)
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector& Origin, const FVector& Direction, float ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;

    FVector SpreadDir = Direction;
    EquippedWeapon->Fire(SpreadDir);

    AEPCharacter* Owner = GetOwnerCharacter();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);
    Params.AddIgnoredActor(EquippedWeapon);

    const FVector End = Origin + SpreadDir * 10000.f;

    // 0. 리와인드 윈도우 제한
    const float MaxLagCompWindow = 0.2f;
    const float ServerNow = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
    if (ServerNow - ClientFireTime > MaxLagCompWindow)
        ClientFireTime = ServerNow;

    // 1. 선행 레이캐스트 — 히트 캐릭터 특정 (O(N) 전체 순회 방지)
    FHitResult PreHit;
    GetWorld()->LineTraceSingleByChannel(PreHit, Origin, End, ECC_GameTraceChannel1, Params);

    AEPCharacter* HitChar = Cast<AEPCharacter>(PreHit.GetActor());
    if (HitChar)
    {
        // 2. 해당 캐릭터 하나만 리와인드
        FTransform OriginalTransform = HitChar->GetActorTransform();
        FEPHitboxSnapshot Snapshot   = HitChar->GetSnapshotAtTime(ClientFireTime);

        // ETeleportType::TeleportPhysics: 물리/콜리전 이벤트 없이 이동
        HitChar->SetActorLocation(Snapshot.Location, false, nullptr, ETeleportType::TeleportPhysics);
        HitChar->SetActorRotation(Snapshot.Rotation, ETeleportType::TeleportPhysics);

        // 3. 리와인드 상태에서 재확인 레이캐스트
        FHitResult RewindHit;
        const bool bHit = GetWorld()->LineTraceSingleByChannel(
            RewindHit, Origin, End, ECC_GameTraceChannel1, Params);

        // 4. 복구
        HitChar->SetActorTransform(OriginalTransform, false, nullptr, ETeleportType::TeleportPhysics);

        // 5. 히트 처리
        if (bHit && RewindHit.GetActor() == HitChar)
        {
            UGameplayStatics::ApplyPointDamage(
                HitChar,
                EquippedWeapon->GetDamage(),
                SpreadDir,
                RewindHit,                      // BoneName → TakeDamage 부위 배율
                Owner->GetController(),
                Owner,
                UDamageType::StaticClass()
            );
        }
    }

    // 이펙트 (히트 여부 무관)
    const FVector MuzzleLocation =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();

    Multicast_PlayMuzzleEffect(MuzzleLocation);
    if (PreHit.bBlockingHit)
        Multicast_PlayImpactEffect(PreHit.ImpactPoint, PreHit.ImpactNormal);
}
```

**왜 200ms인가:**
- 일반적인 게임에서 허용하는 최대 핑 = 150~200ms
- 200ms를 초과하는 ClientFireTime은 지연이 아닌 조작으로 간주
- 이 값은 게임 특성에 따라 조정 (배틀로얄은 더 관대, 경쟁전은 더 엄격)

#### 5) 스냅샷 보간

정확한 시점의 스냅샷이 없을 수 있으므로 두 스냅샷 사이를 보간:

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
    const float Alpha = FMath::Clamp((TargetTime - Before->ServerTime) / Range, 0.f, 1.f);

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

---

## 6. UE5 내장 지원 vs 직접 구현

| 기능 | UE5 내장 | 비고 |
|------|---------|------|
| 이동 예측/보정 | O (CMC) | 자동 처리, 확장만 하면 됨 |
| 히트스캔 랙 보상 | X | 직접 구현 필요 |
| GAS 어빌리티 예측 | O (FPredictionKey) | LocalPredicted 정책 사용 |

Lag Compensation은 UE5가 범용으로 제공하지 않는다.
이를 직접 구현하는 것이 포트폴리오에서 차별화 포인트가 되는 이유.

---

## 7. EmploymentProj 3단계 구현 체크리스트

- [ ] Hit Validation
  - [ ] `Server_Fire` RPC 구현 (Origin + Direction + ClientFireTime)
  - [ ] 서버에서 레이캐스트로 히트 판정
  - [ ] `WithValidation`으로 기본 치트 방지
- [ ] Lag Compensation
  - [ ] FHitboxSnapshot 구조체 정의
  - [ ] 서버에서 100ms 간격 히스토리 기록 (링버퍼)
  - [ ] GetSnapshotAtTime() 보간 함수
  - [ ] 리와인드 → 레이캐스트 → 복구 흐름
  - [ ] 서버 시간 동기화 (`GetServerWorldTimeSeconds`)
- [ ] Reconciliation (사격)
  - [ ] `Client_OnHitConfirmed` RPC로 히트 결과 전달
  - [ ] 히트마커/데미지 숫자 UI 표시
- [ ] Multicast 이펙트
  - [ ] `Multicast_PlayFireEffect` (총구 이펙트, 사운드)
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
