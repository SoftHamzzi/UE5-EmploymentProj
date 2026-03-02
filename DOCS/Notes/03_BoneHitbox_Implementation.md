# 3단계 외전: 본 단위 히트박스 구현 가이드

> **전제**: 03_BoneHitbox.md (개념), 03_NetPrediction.md (Lag Compensation 구조)
> **수정 대상 파일**:
> - `Types/EPTypes.h` — 구조체 추가
> - `Core/EPCharacter.h / .cpp` — 히스토리 버퍼, TakeDamage
> - `Combat/EPCombatComponent.h / .cpp` — Server_Fire 파라미터 + 리와인드 로직

---

## 현재 코드 상태 확인

```cpp
// EPCombatComponent.cpp — Server_Fire_Implementation (현재)
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector& Origin, const FVector& Direction)  // ClientFireTime 없음
{
    // ...
    // ECC_GameTraceChannel1 이미 사용 중 ← 히트박스 채널 이미 설정됨
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, Origin, End, ECC_GameTraceChannel1, Params);

    // ApplyDamage 사용 중 ← ApplyPointDamage로 교체 필요
    UGameplayStatics::ApplyDamage(Hit.GetActor(), ...);
}

// EPCharacter.cpp — TakeDamage (현재)
float AEPCharacter::TakeDamage(...)
{
    // FDamageEvent 그대로 사용 — 부위 정보 없음
    HP = FMath::Clamp(HP - ActualDamage, 0.f, MaxHP);
}
```

**이미 된 것:** `ECC_GameTraceChannel1` 채널 분리
**추가 필요:** `ClientFireTime`, 히스토리 버퍼, 리와인드, `ApplyPointDamage`

---

## Step 1. EPTypes.h — 구조체 추가

```cpp
// Public/Types/EPTypes.h

// --- 기존 ENUM 아래에 추가 ---

// 본 단위 히트박스: 단일 본의 월드 Transform 스냅샷
USTRUCT()
struct FEPBoneSnapshot
{
    GENERATED_BODY()

    FName BoneName;
    FTransform WorldTransform;
};

// 본 단위 히트박스: 특정 서버 시각의 전체 스냅샷
USTRUCT()
struct FEPHitboxSnapshot
{
    GENERATED_BODY()

    float ServerTime = 0.f;
    TArray<FEPBoneSnapshot> Bones;
};

// 히트 판정에 사용할 본 목록 (전역 상수)
// 전체 본 대신 판정에 필요한 본만 기록해 스냅샷 크기 최소화
inline const TArray<FName> GEPHitBones =
{
    "head",
    "spine_03", "spine_01",
    "upperarm_l", "upperarm_r",
    "lowerarm_l", "lowerarm_r",
    "thigh_l",   "thigh_r",
    "calf_l",    "calf_r"
};
```

---

## Step 2. EPCharacter.h — 히스토리 버퍼 선언

```cpp
// Public/Core/EPCharacter.h

// 전방 선언 추가
struct FEPHitboxSnapshot;

// private 섹션에 추가
private:
    // --- Lag Compensation 히스토리 버퍼 (서버 전용) ---
    static constexpr int32 MaxHitboxHistory = 20;  // 100ms × 20 = 2초치

    TArray<FEPHitboxSnapshot> HitboxHistory;
    int32 HistoryIndex = 0;
    float HistoryTimer = 0.f;

    void SaveHitboxSnapshot();

public:
    // CombatComponent에서 호출
    FEPHitboxSnapshot GetSnapshotAtTime(float TargetTime) const;
```

---

## Step 3. EPCharacter.cpp — 히스토리 기록

### include 추가

```cpp
// Private/Core/EPCharacter.cpp 상단
#include "Types/EPTypes.h"
#include "Components/CapsuleComponent.h"
```

### Tick — 주기적 기록

```cpp
void AEPCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 서버에서만 히스토리 기록
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
```

### SaveHitboxSnapshot

```cpp
void AEPCharacter::SaveHitboxSnapshot()
{
    FEPHitboxSnapshot Snapshot;
    Snapshot.ServerTime = GetWorld()->GetTimeSeconds();

    for (const FName& BoneName : GEPHitBones)
    {
        const int32 BoneIndex = GetMesh()->GetBoneIndex(BoneName);
        if (BoneIndex == INDEX_NONE) continue;

        FEPBoneSnapshot Bone;
        Bone.BoneName       = BoneName;
        Bone.WorldTransform = GetMesh()->GetBoneTransform(BoneIndex);
        Snapshot.Bones.Add(Bone);
    }

    if (HitboxHistory.Num() < MaxHitboxHistory)
        HitboxHistory.Add(Snapshot);
    else
    {
        HitboxHistory[HistoryIndex] = Snapshot;
        HistoryIndex = (HistoryIndex + 1) % MaxHitboxHistory;
    }
}
```

### GetSnapshotAtTime — 보간

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

    // 엣지 케이스
    if (!Before && !After)
    {
        // 히스토리 없음 — 현재 본 위치로 스냅샷 생성
        FEPHitboxSnapshot Current;
        Current.ServerTime = TargetTime;
        for (const FName& BoneName : GEPHitBones)
        {
            const int32 BoneIndex = GetMesh()->GetBoneIndex(BoneName);
            if (BoneIndex == INDEX_NONE) continue;
            FEPBoneSnapshot Bone;
            Bone.BoneName       = BoneName;
            Bone.WorldTransform = GetMesh()->GetBoneTransform(BoneIndex);
            Current.Bones.Add(Bone);
        }
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

    for (int32 i = 0; i < Before->Bones.Num(); ++i)
    {
        if (!After->Bones.IsValidIndex(i)) break;

        FEPBoneSnapshot InterpBone;
        InterpBone.BoneName = Before->Bones[i].BoneName;
        InterpBone.WorldTransform = FTransform(
            // 회전: Slerp (각도 랩어라운드 방지)
            FQuat::Slerp(
                Before->Bones[i].WorldTransform.GetRotation(),
                After->Bones[i].WorldTransform.GetRotation(),
                Alpha),
            // 위치: Lerp
            FMath::Lerp(
                Before->Bones[i].WorldTransform.GetLocation(),
                After->Bones[i].WorldTransform.GetLocation(),
                Alpha),
            // 스케일: Lerp
            FMath::Lerp(
                Before->Bones[i].WorldTransform.GetScale3D(),
                After->Bones[i].WorldTransform.GetScale3D(),
                Alpha)
        );
        Result.Bones.Add(InterpBone);
    }

    return Result;
}
```

### TakeDamage — 부위별 배율 추가

```cpp
float AEPCharacter::TakeDamage(
    float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCause)
{
    if (!HasAuthority()) return 0.f;

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent,
        EventInstigator, DamageCause);

    // ApplyPointDamage로 넘어온 경우 부위별 배율 적용
    if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
    {
        const FPointDamageEvent* PointDmg =
            static_cast<const FPointDamageEvent*>(&DamageEvent);

        const FName HitBone = PointDmg->HitInfo.BoneName;

        if (HitBone == "head")
            ActualDamage *= 2.0f;
        else if (HitBone == "upperarm_l" || HitBone == "upperarm_r" ||
                 HitBone == "lowerarm_l" || HitBone == "lowerarm_r" ||
                 HitBone == "thigh_l"    || HitBone == "thigh_r"    ||
                 HitBone == "calf_l"     || HitBone == "calf_r")
            ActualDamage *= 0.75f;
        // spine: 기본 1.0x — 변경 없음
    }

    HP = FMath::Clamp(HP - ActualDamage, 0.f, (float)MaxHP);
    if (HP <= 0.f) Die(EventInstigator);

    Multicast_PlayHitReact();
    Multicast_PlayPainSound();

    if (AEPPlayerController* InstigatorPC = Cast<AEPPlayerController>(EventInstigator))
        InstigatorPC->Client_PlayHitConfirmSound();

    ForceNetUpdate();
    return ActualDamage;
}
```

> **include 추가 필요** (EPCharacter.cpp):
> ```cpp
> #include "GameFramework/DamageType.h"
> ```

---

## Step 4. EPCombatComponent.h — Server_Fire 파라미터 변경

```cpp
// Public/Combat/EPCombatComponent.h

// 기존
UFUNCTION(Server, Reliable)
void Server_Fire(const FVector& Origin, const FVector& Direction);

// 변경: ClientFireTime 추가
UFUNCTION(Server, Reliable)
void Server_Fire(const FVector& Origin, const FVector& Direction, float ClientFireTime);
```

---

## Step 5. EPCombatComponent.cpp — Server_Fire + 리와인드 로직

### RequestFire — ClientFireTime 전달

```cpp
void UEPCombatComponent::RequestFire(const FVector& Origin, const FVector& Direction)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;
    if (EquippedWeapon->CurrentAmmo <= 0) return;

    AEPCharacter* Owner = GetOwnerCharacter();

    float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LocalLastFireTime < FireInterval) return;
    LocalLastFireTime = CurrentTime;

    // 클라이언트 발사 시각 — GetServerWorldTimeSeconds()로 서버 기준 시간 전달
    const float ClientFireTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
    Server_Fire(Origin, Direction, ClientFireTime);

    if (Owner->IsLocallyControlled())
    {
        float Pitch = EquippedWeapon->GetRecoilPitch();
        float Yaw   = FMath::RandRange(
            -EquippedWeapon->GetRecoilYaw(),
             EquippedWeapon->GetRecoilYaw());
        Owner->AddControllerPitchInput(-Pitch);
        Owner->AddControllerYawInput(Yaw);
    }
}
```

### Server_Fire_Implementation — 리와인드 삽입

```cpp
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

    // --- 리와인드 윈도우 제한 ---
    const float MaxLagCompWindow = 0.2f;
    const float ServerNow = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
    if (ServerNow - ClientFireTime > MaxLagCompWindow)
        ClientFireTime = ServerNow;  // 오래된 요청 → 현재 시점 기준

    // --- 선행 레이캐스트: 히트 캐릭터 특정 ---
    FHitResult PreHit;
    GetWorld()->LineTraceSingleByChannel(PreHit, Origin, End, ECC_GameTraceChannel1, Params);

    AEPCharacter* HitChar = Cast<AEPCharacter>(PreHit.GetActor());

    if (HitChar)
    {
        // --- 리와인드: 해당 캐릭터 하나만 ---
        FTransform OriginalTransform = HitChar->GetActorTransform();
        FEPHitboxSnapshot Snapshot   = HitChar->GetSnapshotAtTime(ClientFireTime);

        for (const FEPBoneSnapshot& Bone : Snapshot.Bones)
        {
            HitChar->GetMesh()->SetBoneTransformByName(
                Bone.BoneName, Bone.WorldTransform, EBoneSpaces::WorldSpace);
        }

        // --- 리와인드 상태에서 재확인 레이캐스트 ---
        FHitResult RewindHit;
        const bool bHit = GetWorld()->LineTraceSingleByChannel(
            RewindHit, Origin, End, ECC_GameTraceChannel1, Params);

        // --- 복구 ---
        for (const FEPBoneSnapshot& Bone : Snapshot.Bones)
        {
            HitChar->GetMesh()->SetBoneTransformByName(
                Bone.BoneName,
                HitChar->GetMesh()->GetBoneTransform(
                    HitChar->GetMesh()->GetBoneIndex(Bone.BoneName)),
                EBoneSpaces::WorldSpace);
        }

        // --- 히트 처리 ---
        if (bHit && RewindHit.GetActor() == HitChar)
        {
            UGameplayStatics::ApplyPointDamage(
                HitChar,
                EquippedWeapon->GetDamage(),
                SpreadDir,
                RewindHit,   // BoneName 포함 → TakeDamage에서 부위 배율 적용
                Owner->GetController(),
                Owner,
                UDamageType::StaticClass()
            );
        }
    }

    // --- 이펙트 (히트 여부 무관) ---
    const FVector MuzzleLocation =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();

    Multicast_PlayMuzzleEffect(MuzzleLocation);
    if (PreHit.bBlockingHit)
        Multicast_PlayImpactEffect(PreHit.ImpactPoint, PreHit.ImpactNormal);
}
```

> **include 추가 필요** (EPCombatComponent.cpp):
> ```cpp
> #include "Types/EPTypes.h"
> #include "GameFramework/DamageType.h"
> #include "GameFramework/GameStateBase.h"
> ```

---

## Step 6. Physics Asset 에디터 설정

> 코드 외 에디터 작업. 한 번만 하면 됨.

1. `Content/Characters/MetaHuman/` 에서 `PHA_EPCharacter` 열기 (없으면 생성)
2. 각 본 선택 → `Add Shape` → 구체 or 캡슐 추가
   - `head`: 구체
   - `spine_03`, `spine_01`: 캡슐
   - 팔/다리 본: 캡슐
3. 각 콜리전 바디의 **Collision Response** → `ECC_GameTraceChannel1`을 `Block`으로 설정
4. 기타 채널(WorldStatic 등)은 `Ignore`로 설정
   - 캐릭터 히트박스가 환경에 반응하면 판정 오류 발생

---

## 체크리스트

- [ ] `EPTypes.h`: `FEPBoneSnapshot`, `FEPHitboxSnapshot`, `GEPHitBones` 추가
- [ ] `EPCharacter.h`: 히스토리 버퍼 멤버 선언
- [ ] `EPCharacter.cpp`: `Tick` 기록, `SaveHitboxSnapshot`, `GetSnapshotAtTime`
- [ ] `EPCharacter.cpp`: `TakeDamage` — `FPointDamageEvent` 캐스트 + 부위 배율
- [ ] `EPCombatComponent.h`: `Server_Fire` 파라미터에 `float ClientFireTime` 추가
- [ ] `EPCombatComponent.cpp`: `RequestFire` — `GetServerWorldTimeSeconds()` 전달
- [ ] `EPCombatComponent.cpp`: `Server_Fire_Implementation` — 리와인드 + `ApplyPointDamage`
- [ ] Physics Asset 에디터: 본 콜리전 바디 + `ECC_GameTraceChannel1` Block 설정
