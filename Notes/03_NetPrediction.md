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

```cpp
// 클라이언트 → 서버
UFUNCTION(Server, Reliable, WithValidation)
void Server_Fire(
    FVector_NetQuantize Origin,           // 발사 위치 (양자화로 대역폭 절감)
    FVector_NetQuantizeNormal Direction,  // 발사 방향 (정규화 벡터 양자화)
    float ClientFireTime                  // 클라이언트 발사 시각 (Lag Compensation용)
);

bool AMyCharacter::Server_Fire_Validate(
    FVector_NetQuantize Origin,
    FVector_NetQuantizeNormal Direction,
    float ClientFireTime)
{
    // 기본 검증: 비정상 값 체크
    // 필요 시 연사 속도 검증, 탄약 검증 등 추가
    return !Direction.IsNearlyZero();
}

void AMyCharacter::Server_Fire_Implementation(
    FVector_NetQuantize Origin,
    FVector_NetQuantizeNormal Direction,
    float ClientFireTime)
{
    // 서버에서 레이캐스트
    FHitResult HitResult;
    FVector End = Origin + Direction * WeaponRange;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->LineTraceSingleByChannel(HitResult, Origin, End, ECC_Visibility, Params))
    {
        // 히트 판정 성공 → 데미지 적용
        if (AMyCharacter* HitChar = Cast<AMyCharacter>(HitResult.GetActor()))
        {
            UGameplayStatics::ApplyDamage(HitChar, WeaponDamage, GetController(), this, nullptr);
        }
    }

    // 모든 클라이언트에 이펙트 전파
    Multicast_PlayFireEffect(GetActorLocation(), GetActorRotation());
}
```

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
// 과거 위치/회전 기록
USTRUCT()
struct FHitboxSnapshot
{
    GENERATED_BODY()

    float ServerTime;          // 기록 시점의 서버 시간
    FVector Location;          // 캡슐/히트박스 위치
    FRotator Rotation;         // 회전
    float CapsuleHalfHeight;   // 캡슐 크기
    float CapsuleRadius;
};

// 캐릭터에 링버퍼 저장 (서버에서만)
static const int32 MAX_HISTORY = 20;  // ~2초치 (100ms 간격)

TArray<FHitboxSnapshot> HitboxHistory;  // 링버퍼
int32 HistoryIndex = 0;
```

#### 2) 히스토리 기록 (서버, 주기적)

```cpp
// 서버 Tick에서 100ms마다 기록
void AMyCharacter::Tick(float DeltaTime)
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

void AMyCharacter::SaveHitboxSnapshot()
{
    FHitboxSnapshot Snapshot;
    Snapshot.ServerTime = GetWorld()->GetTimeSeconds();
    Snapshot.Location = GetActorLocation();
    Snapshot.Rotation = GetActorRotation();
    Snapshot.CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Snapshot.CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();

    if (HitboxHistory.Num() < MAX_HISTORY)
        HitboxHistory.Add(Snapshot);
    else
    {
        HitboxHistory[HistoryIndex] = Snapshot;
        HistoryIndex = (HistoryIndex + 1) % MAX_HISTORY;
    }
}
```

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
void AMyCharacter::Server_Fire_Implementation(
    FVector_NetQuantize Origin,
    FVector_NetQuantizeNormal Direction,
    float ClientFireTime)
{
    // 1. 모든 다른 캐릭터의 히트박스를 ClientFireTime 시점으로 리와인드
    TArray<TPair<AMyCharacter*, FTransform>> OriginalTransforms;

    for (AMyCharacter* OtherChar : GetAllOtherCharacters())
    {
        // 현재 위치 저장
        OriginalTransforms.Add({OtherChar, OtherChar->GetActorTransform()});

        // ClientFireTime에 해당하는 스냅샷 찾기 (보간)
        FHitboxSnapshot Snapshot = OtherChar->GetSnapshotAtTime(ClientFireTime);

        // 리와인드 (임시로 위치 이동)
        OtherChar->SetActorLocation(Snapshot.Location);
        OtherChar->SetActorRotation(Snapshot.Rotation);
    }

    // 2. 리와인드된 상태에서 레이캐스트
    FHitResult HitResult;
    FVector End = Origin + Direction * WeaponRange;
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult, Origin, End, ECC_Visibility);

    // 3. 원래 위치로 복구
    for (auto& Pair : OriginalTransforms)
    {
        Pair.Key->SetActorTransform(Pair.Value);
    }

    // 4. 히트 처리
    if (bHit)
    {
        if (AMyCharacter* HitChar = Cast<AMyCharacter>(HitResult.GetActor()))
        {
            UGameplayStatics::ApplyDamage(HitChar, WeaponDamage, GetController(), this, nullptr);
        }
    }
}
```

#### 5) 스냅샷 보간

정확한 시점의 스냅샷이 없을 수 있으므로 두 스냅샷 사이를 보간:

```cpp
FHitboxSnapshot AMyCharacter::GetSnapshotAtTime(float TargetTime) const
{
    // 가장 가까운 두 스냅샷 찾기
    const FHitboxSnapshot* Before = nullptr;
    const FHitboxSnapshot* After = nullptr;

    for (const FHitboxSnapshot& Snap : HitboxHistory)
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

    // 둘 다 없으면 현재 위치 반환
    if (!Before && !After)
    {
        FHitboxSnapshot Current;
        Current.Location = GetActorLocation();
        Current.Rotation = GetActorRotation();
        return Current;
    }
    if (!Before) return *After;
    if (!After) return *Before;
    if (Before == After) return *Before;

    // 보간
    float Alpha = (TargetTime - Before->ServerTime) / (After->ServerTime - Before->ServerTime);
    Alpha = FMath::Clamp(Alpha, 0.f, 1.f);

    FHitboxSnapshot Result;
    Result.ServerTime = TargetTime;
    Result.Location = FMath::Lerp(Before->Location, After->Location, Alpha);
    Result.Rotation = FMath::Lerp(Before->Rotation, After->Rotation, Alpha);
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
