# 3단계 외전: 본 단위 히트박스 & 부위별 데미지

> **전제**: 03_NetPrediction.md의 Lag Compensation(서버 리와인드) 구조 위에서 동작
> **목표**: 캡슐 단일 판정 → 본 단위 판정으로 확장, 헤드샷/부위별 데미지 배율 적용

---

## 1. 캡슐 방식의 한계

```
현재 구조:
  LineTrace → 캡슐 하나에 충돌 → ApplyDamage(고정 데미지)

문제:
  - 머리를 맞춰도 발을 맞춰도 동일 데미지
  - 팔로 몸을 가려도 피해 발생 (캡슐이 팔을 포함하지 않을 수 있음)
  - 헤드샷 시스템 구현 불가
```

---

## 2. 구조 개요

### 캡슐 → Physics Asset 콜리전 바디

스켈레탈 메시의 각 본에 Physics Asset 콜리전 바디를 붙이고,
히트박스 전용 트레이스 채널을 사용해 LineTrace로 본 이름을 얻는다.

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

### 트레이스 채널 분리

| 채널 | 용도 |
|------|------|
| `ECC_Visibility` | 기존 환경(벽/바닥) 충돌 |
| `ECC_GameTraceChannel1` (Hitbox) | 본 단위 캐릭터 히트박스 전용 |

환경과 캐릭터 히트박스 채널을 분리하면 벽을 관통해 캐릭터만 맞는 오판정을 방지할 수 있다.

---

## 3. 스냅샷 구조 변경

캡슐 위치 하나 → 각 본의 월드 Transform 배열로 확장:

```cpp
USTRUCT()
struct FBoneSnapshot
{
    GENERATED_BODY()

    FName BoneName;
    FTransform WorldTransform;
};

USTRUCT()
struct FHitboxSnapshot
{
    GENERATED_BODY()

    float ServerTime = 0.f;
    TArray<FBoneSnapshot> Bones;
};
```

### 기록할 본 목록

전체 본을 기록하면 스냅샷 크기가 너무 커진다.
히트 판정에 실제로 필요한 본만 선택한다:

```cpp
static const TArray<FName> HitBones =
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

## 4. 히스토리 기록 (서버)

```cpp
void AEPCharacter::SaveHitboxSnapshot()
{
    FHitboxSnapshot Snapshot;
    Snapshot.ServerTime = GetWorld()->GetTimeSeconds();

    for (const FName& BoneName : HitBones)
    {
        FBoneSnapshot Bone;
        Bone.BoneName      = BoneName;
        Bone.WorldTransform = GetMesh()->GetBoneTransform(
            GetMesh()->GetBoneIndex(BoneName));
        Snapshot.Bones.Add(Bone);
    }

    // 링버퍼에 저장 (03_NetPrediction.md 구조 동일)
    if (HitboxHistory.Num() < MAX_HISTORY)
        HitboxHistory.Add(Snapshot);
    else
    {
        HitboxHistory[HistoryIndex] = Snapshot;
        HistoryIndex = (HistoryIndex + 1) % MAX_HISTORY;
    }
}
```

---

## 5. 리와인드 → 레이캐스트 → 복구

캡슐을 옮기는 대신 Physics Asset 바디를 본 Transform으로 직접 이동:

```cpp
void AEPCharacter::Server_Fire_Implementation(
    FVector_NetQuantize Origin,
    FVector_NetQuantizeNormal Direction,
    float ClientFireTime)
{
    // 리와인드 윈도우 제한 (03_NetPrediction.md 참고)
    const float MaxLagCompWindow = 0.2f;
    const float ServerNow = GetWorld()->GetTimeSeconds();
    if (ServerNow - ClientFireTime > MaxLagCompWindow)
        ClientFireTime = ServerNow;

    // --- 리와인드 ---
    struct FRewindEntry
    {
        AEPCharacter* Character;
        TArray<FBoneSnapshot> OriginalBones;
    };
    TArray<FRewindEntry> RewindEntries;

    for (AEPCharacter* OtherChar : GetAllOtherCharacters())
    {
        FRewindEntry Entry;
        Entry.Character = OtherChar;

        FHitboxSnapshot Snapshot = OtherChar->GetSnapshotAtTime(ClientFireTime);

        for (const FBoneSnapshot& Bone : Snapshot.Bones)
        {
            // 원본 Transform 저장
            FBoneSnapshot Original;
            Original.BoneName = Bone.BoneName;
            Original.WorldTransform = OtherChar->GetMesh()->GetBoneTransform(
                OtherChar->GetMesh()->GetBoneIndex(Bone.BoneName));
            Entry.OriginalBones.Add(Original);

            // 리와인드: 스냅샷 위치로 이동
            OtherChar->GetMesh()->SetBoneTransformByName(
                Bone.BoneName, Bone.WorldTransform, EBoneSpaces::WorldSpace);
        }

        RewindEntries.Add(Entry);
    }

    // --- 히트박스 전용 채널로 레이캐스트 ---
    FHitResult HitResult;
    const FVector End = Origin + Direction * WeaponRange;

    GetWorld()->LineTraceSingleByChannel(
        HitResult, Origin, End, ECC_GameTraceChannel1);  // Hitbox 채널

    // --- 복구 ---
    for (const FRewindEntry& Entry : RewindEntries)
    {
        for (const FBoneSnapshot& Original : Entry.OriginalBones)
        {
            Entry.Character->GetMesh()->SetBoneTransformByName(
                Original.BoneName, Original.WorldTransform, EBoneSpaces::WorldSpace);
        }
    }

    // --- 히트 처리 ---
    if (HitResult.GetActor())
    {
        UGameplayStatics::ApplyPointDamage(
            HitResult.GetActor(),
            WeaponDamage,
            Direction,
            HitResult,          // BoneName 포함
            GetOwner()->GetInstigatorController(),
            GetOwner(),
            UDamageType::StaticClass()
        );
    }
}
```

---

## 6. TakeDamage — 부위별 배율 적용

`ApplyPointDamage`로 넘어온 `FPointDamageEvent`에서 `BoneName` 추출:

```cpp
float AEPCharacter::TakeDamage(
    float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator,
    AActor* DamageCauser)
{
    if (!HasAuthority()) return 0.f;

    if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
    {
        const FPointDamageEvent* PointDmg =
            static_cast<const FPointDamageEvent*>(&DamageEvent);

        const FName HitBone = PointDmg->HitInfo.BoneName;

        float Multiplier = 1.0f;
        if (HitBone == "head")
            Multiplier = 2.0f;
        else if (HitBone == "upperarm_l" || HitBone == "upperarm_r" ||
                 HitBone == "lowerarm_l" || HitBone == "lowerarm_r" ||
                 HitBone == "thigh_l"    || HitBone == "thigh_r"    ||
                 HitBone == "calf_l"     || HitBone == "calf_r")
            Multiplier = 0.75f;

        DamageAmount *= Multiplier;
    }

    HP = FMath::Clamp(HP - DamageAmount, 0.f, (float)MaxHP);

    Multicast_PlayHitReact();
    Multicast_PlayPainSound();

    if (AEPPlayerController* InstigatorPC =
            Cast<AEPPlayerController>(EventInstigator))
        InstigatorPC->Client_PlayHitConfirmSound();

    if (HP <= 0) Die(EventInstigator);

    ForceNetUpdate();
    return DamageAmount;
}
```

---

## 7. ApplyDamage vs ApplyPointDamage

| | ApplyDamage | ApplyPointDamage |
|--|-------------|-----------------|
| HitResult 전달 | ❌ | ✅ |
| BoneName 접근 | ❌ | ✅ (`FPointDamageEvent`) |
| 부위별 배율 | 서버에서 미리 계산 필요 | TakeDamage 안에서 처리 가능 |
| 히트스캔 무기 | 부적합 | ✅ 적합 |
| DoT/낙사 | ✅ 적합 | 불필요 |

LineTrace 결과가 있는 히트스캔 무기는 항상 `ApplyPointDamage`.

---

## 8. 구현 체크리스트

- [ ] Physics Asset 본 콜리전 바디 설정 (head, spine, 팔, 다리)
- [ ] 히트박스 전용 트레이스 채널 추가 (Project Settings → Collision)
- [ ] Physics Asset 콜리전 채널을 Hitbox로 설정
- [ ] `FBoneSnapshot` / `FHitboxSnapshot` 구조체 정의
- [ ] `SaveHitboxSnapshot()` — 본 Transform 배열 기록
- [ ] `GetSnapshotAtTime()` — 보간 (03_NetPrediction.md 구조 그대로)
- [ ] `Server_Fire` — 본 단위 리와인드 + Hitbox 채널 레이캐스트
- [ ] `TakeDamage` — `FPointDamageEvent` 캐스트 + 부위별 배율
- [ ] `ApplyDamage` → `ApplyPointDamage` 교체
