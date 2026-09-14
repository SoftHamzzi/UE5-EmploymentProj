# 3단계 외전: 본 단위 히트박스 & 부위별 데미지

> **이 단계에서 구현하는 것**:
> 1. 서버 히트박스 스냅샷 히스토리 + Lag Compensation(리와인드)
> 2. Physics Asset 본 단위 판정으로 헤드샷/부위별 데미지 배율 적용
>
> **전제 조건**: 기본 `Server_Fire` RPC가 존재하고 빌드가 통과되는 상태

---

## 1. 캡슐 방식의 한계

```
현재 구조:
  LineTrace → ECC_GameTraceChannel1 → 캡슐 하나에 충돌 → ApplyDamage(고정 데미지)

문제:
  - 머리를 맞춰도 발을 맞춰도 동일 데미지
  - 헤드샷 시스템 구현 불가
  - Lag Compensation 없음 — 지연 환경에서 판정 불일치
```

---

## 2. 구조 개요

### 운영 상수는 설정값으로 분리

아래 값은 문서 예시 숫자를 그대로 하드코딩하지 말고 설정화한다.
- `MaxRewindSeconds`
- `SnapshotIntervalSeconds`
- `BroadPhasePaddingCm`
- `TraceDistanceCm`
- `BoneDamageMultiplierMap`

권장 위치:
- 공통: `UDeveloperSettings`
- 무기별: `UEPWeaponDefinition`

### Physics Asset 콜리전 바디

스켈레탈 메시의 각 본에 Physics Asset 콜리전 바디를 붙이고,
`WeaponTrace` 채널로 LineTrace해서 본 이름을 얻는다.

```
[Physics Asset: PHA_EPCharacter]
├── head          → 구체  (헤드샷 배율 2.0x)
├── spine_03      → 캡슐  (상체  배율 1.0x)
├── spine_01      → 캡슐  (하체  배율 1.0x)
├── upperarm_l/r  → 캡슐  (팔   배율 0.75x)
├── lowerarm_l/r  → 캡슐  (팔뚝 배율 0.75x)
├── thigh_l/r     → 캡슐  (허벅지 배율 0.75x)
└── calf_l/r      → 캡슐  (종아리 배율 0.75x)
```

### 트레이스 채널

`WeaponTrace` 채널은 이미 `DefaultEngine.ini`에 등록되어 있다.

| 채널 | DefaultResponse | 설정 방향 |
|------|----------------|----------|
| `WeaponTrace` (ECC_GameTraceChannel1) | Ignore | 환경 메시는 무시, Physics Asset 바디에만 명시적 Block |

Default를 Ignore로 두면 리와인드된 캐릭터가 벽 안쪽으로 이동해도
LineTrace가 벽에 막히지 않고 캐릭터 바디만 감지한다.

### 전체 흐름

```
[클라이언트]
  Input_Fire
    → ClientFireTime = GS->GetServerWorldTimeSeconds()  ← 서버 동기화 시계 사용
    → RequestFire(Origin, Direction, ClientFireTime)
    → Server_Fire RPC

[서버] Server_Fire_Implementation
    → HandleHitscanFire(Owner, Origin, Directions, ClientFireTime)
         ├─ [Rewind Block] → Owner->GetServerSideRewindComponent()->ConfirmHitscan(...)
         │    ├─ 0. ClientFireTime 클램프 (MaxRewindSeconds 초과 = 현재 시점)
         │    ├─ 1. GetHitscanCandidates — Broad Phase (Location 기반, O(N))
         │    ├─ 2. 후보 FBodyInstance 리와인드 (과거 본 Transform으로 이동)
         │    ├─ 3. LineTrace × N (WeaponTrace, bReturnPhysicalMaterial=true)
         │    └─ 4. FBodyInstance 복구 (현재 Transform 복원)
         └─ [Damage Block] (CombatComponent에 남음)
              ├─ GetBoneMultiplier(Hit.BoneName)
              ├─ Cast<UEPPhysicalMaterial>(Hit.PhysMaterial)
              ├─ FinalDamage = Base × Bone× × Mat×
              └─ ApplyPointDamage → TakeDamage (HP·사망만)
```

**책임 분리**:
- `UEPServerSideRewindComponent` — 히스토리 기록, 보간, 후보 선정, 리와인드/복구, Narrow Trace, 디버그
- `UEPCombatComponent` — 입력 검증, Server_Fire 진입, SSR 호출, 데미지 계산/적용, 이펙트

---

## 3. 스냅샷 구조

### FEPBoneSnapshot (신규)

```cpp
// EPTypes.h
USTRUCT()
struct FEPBoneSnapshot
{
    GENERATED_BODY()
    UPROPERTY() FName      BoneName;
    UPROPERTY() FTransform WorldTransform; // 서버 시뮬레이션 기준 본 월드 Transform
};
```

### FEPHitboxSnapshot (신규)

```cpp
USTRUCT()
struct FEPHitboxSnapshot
{
    GENERATED_BODY()
    UPROPERTY() float   ServerTime = 0.f;
    UPROPERTY() FVector Location   = FVector::ZeroVector;  // Broad Phase용 (캐릭터 루트)
    UPROPERTY() TArray<FEPBoneSnapshot> Bones;             // Narrow Phase 리와인드용
};
```

`Location`을 별도로 저장하는 이유: Broad Phase(`GetHitscanCandidates`)는 루트 위치만으로 충분하다.
본 단위 `GetBodyInstance` 호출은 Narrow Phase 후보에만 적용해 비용을 줄인다.

### 기록할 본 목록

```cpp
// EPServerSideRewindComponent.cpp (static const, 클래스 정의 전)
const TArray<FName> UEPServerSideRewindComponent::HitBones =
{
    TEXT("head"),
    TEXT("neck_01"),
    TEXT("pelvis"),
    TEXT("spine_04"), TEXT("spine_02"),
    TEXT("clavicle_l"), TEXT("clavicle_r"),
    TEXT("upperarm_l"), TEXT("upperarm_r"),
    TEXT("lowerarm_l"), TEXT("lowerarm_r"),
    TEXT("hand_l"),     TEXT("hand_r"),
    TEXT("thigh_l"),    TEXT("thigh_r"),
    TEXT("calf_l"),     TEXT("calf_r"),
    TEXT("foot_l"),     TEXT("foot_r")
};
```

전체 본 대신 판정에 필요한 본만 선택해 스냅샷 크기를 제한한다.
`HitBones`는 SSR 컴포넌트가 소유하며 `EPCharacter`에는 없다.

---

## 4. 히스토리 기록 (SaveHitboxSnapshot)

SSR 컴포넌트의 TickComponent에서 `SnapshotIntervalSeconds`마다 호출.
`GetServerWorldTimeSeconds()`로 서버 기준 시간을 사용한다.
권한 체크(`HasAuthority`)는 TickComponent에서 이미 처리한다.

```cpp
void UEPServerSideRewindComponent::SaveHitboxSnapshot()
{
    AEPCharacter* OwnerChar = Cast<AEPCharacter>(GetOwner());
    if (!OwnerChar) return;

    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ServerNow = GS ? GS->GetServerWorldTimeSeconds()
                               : GetWorld()->GetTimeSeconds();

    FEPHitboxSnapshot Snapshot;
    Snapshot.ServerTime = ServerNow;
    Snapshot.Location   = OwnerChar->GetActorLocation(); // Broad Phase용

    for (const FName& BoneName : HitBones)
    {
        const int32 BoneIndex = OwnerChar->GetMesh()->GetBoneIndex(BoneName);
        if (BoneIndex == INDEX_NONE) continue;

        FEPBoneSnapshot Bone;
        Bone.BoneName       = BoneName;
        Bone.WorldTransform = OwnerChar->GetMesh()->GetBoneTransform(BoneIndex);
        Snapshot.Bones.Add(Bone);
    }

    // 시간 오름차순 배열: 오래된 것을 앞에서 제거하고 뒤에 추가
    // → GetSnapshotAtTime의 선형 탐색이 항상 시간 순서를 보장한다.
    // (링버퍼 방식은 wrap 후 탐색 순서가 깨져 잘못된 스냅샷을 반환한다)
    if (HitboxHistory.Num() >= MaxHistoryCount)
        HitboxHistory.RemoveAt(0); // O(N), N=20이므로 무시
    HitboxHistory.Add(Snapshot);
}
```

---

## 5. GetSnapshotAtTime — per-bone 보간

클라이언트 발사 시점(ClientFireTime)에 해당하는 스냅샷을 Before/After 사이에서 보간해 반환한다.

```cpp
FEPHitboxSnapshot UEPServerSideRewindComponent::GetSnapshotAtTime(float TargetTime) const
{
    if (HitboxHistory.IsEmpty())
        return FEPHitboxSnapshot{};

    if (TargetTime <= HitboxHistory[0].ServerTime)
        return HitboxHistory[0];
    if (TargetTime >= HitboxHistory.Last().ServerTime)
        return HitboxHistory.Last();

    const FEPHitboxSnapshot* Before = nullptr;
    const FEPHitboxSnapshot* After  = nullptr;

    for (int32 i = 0; i < HitboxHistory.Num() - 1; ++i)
    {
        if (HitboxHistory[i].ServerTime <= TargetTime &&
            HitboxHistory[i + 1].ServerTime >= TargetTime)
        {
            Before = &HitboxHistory[i];
            After  = &HitboxHistory[i + 1];
            break;
        }
    }

    if (!Before || !After)
        return HitboxHistory.Last();

    const float Denom = After->ServerTime - Before->ServerTime;
    const float Alpha = (Denom > KINDA_SMALL_NUMBER)
                      ? (TargetTime - Before->ServerTime) / Denom
                      : 0.f;

    FEPHitboxSnapshot Result;
    Result.ServerTime = TargetTime;
    Result.Location   = FMath::Lerp(Before->Location, After->Location, Alpha);

    // per-bone 보간 — FTransform::BlendWith로 위치/회전/스케일 동시 보간
    const int32 BoneCount = FMath::Min(Before->Bones.Num(), After->Bones.Num());
    Result.Bones.Reserve(BoneCount);
    for (int32 i = 0; i < BoneCount; ++i)
    {
        FEPBoneSnapshot BoneResult;
        BoneResult.BoneName       = Before->Bones[i].BoneName;
        BoneResult.WorldTransform = Before->Bones[i].WorldTransform;
        BoneResult.WorldTransform.BlendWith(After->Bones[i].WorldTransform, Alpha);
        Result.Bones.Add(BoneResult);
    }

    return Result;
}
```

---

## 6. 리와인드 → 레이캐스트 → 복구

### 올바른 리와인드 API

```
잘못된 접근: SetBoneTransformByName → 애니메이션 포즈 변경 (물리 바디 이동 안 됨)
올바른 접근: FBodyInstance::SetBodyTransform → 물리 바디 직접 이동
```

```cpp
// 현재 Transform 저장
FBodyInstance* Body = Char->GetMesh()->GetBodyInstance(BoneName);
FTransform Saved = Body->GetUnrealWorldTransform();

// 리와인드
Body->SetBodyTransform(SnapshotTransform, ETeleportType::TeleportPhysics);

// 복구
Body->SetBodyTransform(Saved, ETeleportType::TeleportPhysics);
```

### FEPRewindEntry (EPServerSideRewindComponent.cpp 파일 스코프)

```cpp
// AEPCharacter*에 의존하므로 EPTypes.h에 넣지 않는다.
// 복제 불필요 — 리와인드 중 임시 사용 후 즉시 복구된다.
struct FEPRewindEntry
{
    AEPCharacter*           Character  = nullptr;
    TArray<FEPBoneSnapshot> SavedBones; // 리와인드 전 현재(서버) 물리 바디 Transform
};
```

### HandleHitscanFire — SSR 위임 패턴

`UEPCombatComponent::HandleHitscanFire`는 Rewind Block을 직접 구현하지 않고
`UEPServerSideRewindComponent::ConfirmHitscan`에 위임한다.
Damage Block만 CombatComponent에 남는다.

```cpp
void UEPCombatComponent::HandleHitscanFire(
    AEPCharacter* Owner, const FVector& Origin,
    const TArray<FVector>& Directions, float ClientFireTime)
{
    if (!Owner || !Owner->GetServerSideRewindComponent()) return;

    // ── [Rewind Block] → SSR 컴포넌트에 위임 ──────────────────────────────
    // 후보 선정, 리와인드, Narrow Trace, 복구, 디버그 모두 SSR 내부에서 처리
    TArray<FHitResult> ConfirmedHits;
    Owner->GetServerSideRewindComponent()->ConfirmHitscan(
        Owner, EquippedWeapon, Origin, Directions, ClientFireTime, ConfirmedHits);

    // ── [Damage Block] ─────────────────────────────────────────────────────
    for (const FHitResult& Hit : ConfirmedHits)
    {
        if (!Hit.GetActor()) continue;

        const float BaseDamage       = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
        const float BoneMultiplier   = GetBoneMultiplier(Hit.BoneName);
        const float MaterialMultiplier = GetMaterialMultiplier(Hit.PhysMaterial.Get());
        const float FinalDamage      = BaseDamage * BoneMultiplier * MaterialMultiplier;

        UGameplayStatics::ApplyPointDamage(Hit.GetActor(), FinalDamage,
            (Hit.ImpactPoint - Origin).GetSafeNormal(), Hit,
            Owner->GetController(), Owner, UDamageType::StaticClass());
    }
}
```

`UEPServerSideRewindComponent::ConfirmHitscan` 내부 흐름:
```
0. ClientFireTime 클램프 (MaxRewindSeconds 초과 = 현재 시점)
1. GetHitscanCandidates — Broad Phase (Location 기반, O(N))
2. 후보 FBodyInstance 리와인드 (과거 본 Transform으로 이동)
3. LineTrace × N (WeaponTrace, bReturnPhysicalMaterial=true)
4. FBodyInstance 복구 (현재 Transform 복원) — 무조건 실행
5. 디버그 draw (Blue/Red/White/Yellow)
```

---

## 7. bReturnPhysicalMaterial — Hit.BoneName 확보 조건

| 필드 | 확보 조건 |
|------|----------|
| `Hit.BoneName` | Physics Asset 바디 이름 = 본 이름이어야 함 (UE5 기본 동작) |
| `Hit.PhysMaterial` | `bReturnPhysicalMaterial = true` **필수** + Physics Asset 바디에 PM 할당 |

`bReturnPhysicalMaterial = true` 없이는 `Hit.PhysMaterial`이 항상 nullptr이다.

---

## 8. 부위별 데미지: PhysicalMaterial + BoneName 이중 계층

```
FinalDamage = BaseDamage × BoneMultiplier × MaterialMultiplier
```

**판정 책임 분리**:
- `EPCombatComponent` — 배율 계산 + 최종 데미지 결정
- `EPCharacter::TakeDamage` — HP 반영 + 사망 처리만 (배율 재계산 없음)

### UEPPhysicalMaterial (신규, GAS 태그 확장 대응)

```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPPhysicalMaterial : public UPhysicalMaterial
{
    GENERATED_BODY()
public:
    // GAS 확장용 (현재 주석 처리 — GAS 단계에서 활성화)
    // UPROPERTY(EditDefaultsOnly, Category = "Damage")
    // FGameplayTagContainer MaterialTags; // 예: Gameplay.Zone.WeakSpot

    UPROPERTY(EditDefaultsOnly, Category = "Damage")
    bool bIsWeakSpot = false;

    UPROPERTY(EditDefaultsOnly, Category = "Damage",
        meta = (EditCondition = "bIsWeakSpot"))
    float WeakSpotMultiplier = 2.0f;
};
```

`MaterialTags`는 GAS 단계에서 `ApplyPointDamage` → `GameplayEffectSpec`으로 전환할 때 활성화한다.

Physics Asset 에디터에서 `head` 바디 → `PM_WeakSpot` (bIsWeakSpot=true) 할당.

### GetBoneMultiplier

```cpp
float UEPCombatComponent::GetBoneMultiplier(const FName& BoneName)
{
    if (EquippedWeapon && EquippedWeapon->WeaponDef)
        if (const float* Found = EquippedWeapon->WeaponDef->BoneDamageMultiplierMap.Find(BoneName))
            return *Found;
    return 1.0f;
}
```

---

## 9. TakeDamage — HP 반영·사망 처리만

배율 계산이 `HandleHitscanFire`로 이전되었으므로 단순화된다.

```cpp
float AEPCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority()) return 0.f;

    HP = FMath::Clamp(HP - DamageAmount, 0.f, static_cast<float>(MaxHP));
    Multicast_PlayHitReact();
    Multicast_PlayPainSound();

    if (AEPPlayerController* PC = Cast<AEPPlayerController>(EventInstigator))
        PC->Client_PlayHitConfirmSound();

    if (HP <= 0.f) Die(EventInstigator);
    ForceNetUpdate();
    return DamageAmount;
}
```

> **GAS 전환 시**: `ApplyPointDamage` → `ApplyGameplayEffectSpecToTarget`으로 교체.
> 트레이스·배율 계산 코드는 그대로 GAS Execution의 입력값으로 재사용된다.

---

## 10. 구현 체크리스트

**에디터**
- [ ] Physics Asset 본 콜리전 바디 설정 (head 구체, spine/사지 캡슐)
- [ ] Physics Asset 모든 바디 → `WeaponTrace` Block 설정
- [ ] head 바디 → `PM_WeakSpot` (`UEPPhysicalMaterial`, bIsWeakSpot=true) 할당

**EPTypes.h**
- [ ] `FEPBoneSnapshot` / `FEPHitboxSnapshot`
- [ ] `EP_TraceChannel_Weapon` 상수 정의

**UEPCombatDeveloperSettings**
- [ ] `MaxRewindSeconds`, `SnapshotIntervalSeconds`, `BroadPhasePaddingCm`, `DefaultTraceDistanceCm`
- [ ] 디버그: `bEnableSSRDebugDraw`, `SSRDebugDrawDuration`, `SSRDebugLineThickness`, `bEnableSSRDebugLog`

**UEPServerSideRewindComponent (신규)**
- [ ] `HitBones` (static const), `HitboxHistory`, `SnapshotAccumulator`, `MaxHistoryCount`
- [ ] `SaveHitboxSnapshot()` — TickComponent에서 Interval마다 호출
- [ ] `GetSnapshotAtTime()` — per-bone BlendWith 보간
- [ ] `GetHitscanCandidates()` — Broad Phase (TActorIterator)
- [ ] `ConfirmHitscan()` — FEPRewindEntry + FBodyInstance 리와인드/복구 + Narrow Trace + 디버그
- [ ] `FEPRewindEntry` (파일 스코프)

**AEPCharacter**
- [ ] `RewindComponent` (`UEPServerSideRewindComponent`) — CreateDefaultSubobject
- [ ] `GetServerSideRewindComponent()` getter
- [ ] `IsDead()` inline
- [ ] `VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones` (생성자)

**UEPCombatComponent**
- [ ] `Server_Fire` 시그니처에 `float ClientFireTime` 추가
- [ ] `HandleHitscanFire` — SSR `ConfirmHitscan` 위임 + Damage Block
- [ ] `GetBoneMultiplier` (WeaponDef->BoneDamageMultiplierMap 기반)
- [ ] `GetMaterialMultiplier` (UEPPhysicalMaterial 캐스트)

**EPCharacter.cpp**
- [ ] `Input_Fire` — `GS->GetServerWorldTimeSeconds()` 사용

**UEPPhysicalMaterial**
- [ ] `bIsWeakSpot`, `WeakSpotMultiplier`

**AEPCharacter::TakeDamage**
- [ ] HP 반영 + 사망 처리만 (배율 계산 없음)

---

## 11. NetPrediction 단계와의 연결

### BoneHitbox가 확립하는 것 (NetPrediction이 재사용)

| 구성 요소 | 역할 | NetPrediction에서 |
|----------|------|-----------------|
| `FEPHitboxSnapshot` (Location + Bones) | 시간별 포즈 저장 | 그대로 사용 |
| `SaveHitboxSnapshot` (링버퍼, GetServerWorldTimeSeconds) | 히스토리 누적 | 그대로 사용 |
| `GetSnapshotAtTime` (per-bone BlendWith) | 시점 보간 | 그대로 사용 |
| `FBodyInstance::SetBodyTransform` 리와인드 | 물리 바디 이동 | 그대로 사용 |
| `GetHitscanCandidates` (Broad Phase) | 후보 선정 | 그대로 사용 |
| `HandleHitscanFire` | Hitscan 전용 처리 | 그대로 유지, Projectile 핸들러 추가됨 |

### NetPrediction이 추가하는 것

```
BoneHitbox 완료 후 Server_Fire 구조:

  Server_Fire → HandleHitscanFire(...)  [Hitscan만]

NetPrediction 완료 후:

  Server_Fire → switch(BallisticType)
                  ├─ Hitscan:         HandleHitscanFire(...)     [이미 완성]
                  ├─ ProjectileFast:  HandleProjectileFastFire(...) [신규]
                  └─ ProjectileSlow:  HandleProjectileSlowFire(...) [신규]
```

NetPrediction에서 추가되는 것:
- `EEPBallisticType` enum (Hitscan / ProjectileFast / ProjectileSlow)
- `EPWeaponDefinition`에 `BallisticType`, `ProjectileClass` 필드
- `Server_Fire`의 switch 분기
- `EPProjectile` 클래스 (서버 권한 투사체)
- ProjectileFast: 클라이언트 스폰 + 서버 검증
- ProjectileSlow: 순수 서버 투사체

### 03_NetPrediction_Implementation.md에서 건너뛸 내용

BoneHitbox 완료 후 NetPrediction 문서를 따라갈 때, 다음 항목은 **이미 구현됨**으로 건너뛴다:

| NetPrediction 구현서 항목 | 이유 |
|--------------------------|------|
| `FEPHitboxSnapshot` 정의 | BoneHitbox에서 Bones 포함 버전으로 완성 |
| `SaveHitboxSnapshot` 구현 | BoneHitbox에서 완성 |
| `GetSnapshotAtTime` 구현 | BoneHitbox에서 per-bone 보간 포함 완성 |
| `FEPRewindEntry` (SavedLocation/Rotation) | **무시** — SavedBones 버전으로 교체됨 |
| `HandleHitscanFire`의 `SetActorLocationAndRotation` | **무시** — FBodyInstance 방식 유지 |
| `GetHitscanCandidates` | BoneHitbox에서 완성 |

### GAS 전환 준비 상태

BoneHitbox 완료 시점에서 GAS 전환을 위한 구조적 준비가 완료된다:

```
[유지]  트레이스 로직 (GetHitscanCandidates + LineTrace)
[유지]  FBodyInstance 리와인드/복구
[유지]  GetBoneMultiplier (Hit.BoneName → 배율)
[교체]  UEPPhysicalMaterial.MaterialTags + WeaponDefinition(MaterialDamageMultiplier)
[교체]  ApplyPointDamage(FinalDamage) → ApplyGameplayEffectSpecToTarget(GE, SetByCaller)
```
