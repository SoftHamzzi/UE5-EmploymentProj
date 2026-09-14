# Post 3-2 작성 가이드 — 서버 사이드 리와인드 컴포넌트 구현

> **예상 제목**: `[UE5] 추출 슈터 3-2. 서버 사이드 리와인드: 히스토리 기록과 본 단위 보간`
> **참고 문서**: `DOCS/Notes/03_BoneHitbox_Implementation.md` Step 2~4, `DOCS/Mine/Rewind.md`

---

## 개요

**이 포스팅에서 다루는 것:**
- `UEPServerSideRewindComponent` 설계와 책임 범위
- 매 Tick 본 Transform 히스토리 기록 구조
- `GetSnapshotAtTime` per-bone 보간
- `ConfirmHitscan` 전체 흐름 (Broad Phase → 리와인드 → Narrow Trace → 복구)
- 디버그 시각화 시스템

**왜 이렇게 구현했는가 (설계 의도):**
- Lag Compensation 로직을 AEPCharacter나 CombatComponent에 두면 두 클래스가 모두 비대해짐
- SSR 컴포넌트로 격리하면 GAS 어빌리티에서도 `ConfirmHitscan`을 재사용 가능
- `SetIsReplicatedByDefault(false)` — 히스토리는 서버만 필요, 복제 비용 0

---

## 구현 전 상태 (Before)

2단계까지 Lag Compensation이 없었다:

```cpp
// 2단계 Server_Fire — 현재 위치 기준 즉시 판정
void UEPCombatComponent::Server_Fire_Implementation(...)
{
    // 서버 RPC 수신 시점의 적 위치로 판정
    // → 클라가 쏠 때 적이 위치 A에 있었어도
    //   RTT 후 서버에서는 이미 위치 B → 빗나감
    GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params);
}
```

---

## 구현 내용

### 1. SSR 컴포넌트 책임 범위

```
UEPServerSideRewindComponent (서버 전용)
  ├── HitboxHistory          히트박스 스냅샷 배열 (시간 오름차순)
  ├── SaveHitboxSnapshot()   매 Tick 본 Transform 기록
  ├── GetSnapshotAtTime()    특정 서버 시각의 스냅샷 보간 반환
  ├── GetHitscanCandidates() Broad Phase — 후보 캐릭터 선정
  └── ConfirmHitscan()       Rewind → Narrow Trace → 복구 → 결과 반환
```

**CombatComponent와의 역할 분리:**
```
UEPCombatComponent::HandleHitscanFire
  → ConfirmHitscan(Owner, Weapon, Origin, Directions, ClientFireTime, OutHits)  // SSR에 위임
  → Damage Block (BoneMultiplier × MaterialMultiplier × BaseDamage)             // 여기서 처리
```

### 2. 히스토리 기록 — SaveHitboxSnapshot

```cpp
// EPServerSideRewindComponent.h
static const TArray<FName> HitBones; // 기록할 본 목록 (head, spine, arm, leg 등)
TArray<FEPHitboxSnapshot> HitboxHistory; // [0] 오래됨, [Last] 최신 (시간 오름차순)
float SnapshotAccumulator = 0.f;

// TickComponent에서 SnapshotInterval(0.03초)마다 호출
void UEPServerSideRewindComponent::SaveHitboxSnapshot()
{
    AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner());
    if (!Owner || !Owner->GetMesh()) return;

    FEPHitboxSnapshot Snapshot;
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    Snapshot.ServerTime = GS
        ? GS->GetServerWorldTimeSeconds()
        : GetWorld()->GetTimeSeconds();
    Snapshot.Location = Owner->GetActorLocation(); // Broad Phase용

    for (const FName& BoneName : HitBones)
    {
        const FBodyInstance* Body = Owner->GetMesh()->GetBodyInstance(BoneName);
        if (!Body) continue;

        FEPBoneSnapshot BoneSnap;
        BoneSnap.BoneName        = BoneName;
        BoneSnap.WorldTransform  = Body->GetUnrealWorldTransform();
        Snapshot.Bones.Add(BoneSnap);
    }

    HitboxHistory.Add(Snapshot);

    // MaxHistoryCount 초과 시 가장 오래된 항목 제거
    if (HitboxHistory.Num() > MaxHistoryCount)
        HitboxHistory.RemoveAt(0);
}
```

**링버퍼 대신 단순 배열을 쓰는 이유:**
- 링버퍼는 인덱스 관리가 필요 → GetSnapshotAtTime 탐색 시 순서 보장 불확실
- 단순 배열 + 앞에서 제거 → [0]이 항상 가장 오래된 항목 보장
- 배열 크기가 작아 성능 차이 무시 가능 (0.5초 / 0.03초 ≈ 17개)

**SnapshotInterval 결정:**
```
MaxRewindSeconds = 0.5초
SnapshotIntervalSeconds = 0.03초 (≈33Hz 샘플링)
MaxHistoryCount = ceil(0.5 / 0.03) = 17개
```

> `UEPCombatDeveloperSettings`에서 에디터에서 조정 가능.

### 3. GetSnapshotAtTime — per-bone 보간

```cpp
FEPHitboxSnapshot UEPServerSideRewindComponent::GetSnapshotAtTime(float TargetTime) const
{
    // HitboxHistory는 시간 오름차순 — Before/After 탐색
    const FEPHitboxSnapshot* Before = nullptr;
    const FEPHitboxSnapshot* After  = nullptr;

    for (const FEPHitboxSnapshot& Snap : HitboxHistory)
    {
        if (Snap.ServerTime <= TargetTime)
            Before = &Snap;
        else if (!After)
        {
            After = &Snap;
            break; // 오름차순이므로 첫 번째 After만 필요
        }
    }

    if (!Before && !After) return FallbackSnapshot(); // 히스토리 없음
    if (!Before)  return *After;
    if (!After)   return *Before;
    if (Before == After) return *Before;

    const float Range = After->ServerTime - Before->ServerTime;
    const float Alpha = Range > KINDA_SMALL_NUMBER
        ? FMath::Clamp((TargetTime - Before->ServerTime) / Range, 0.f, 1.f)
        : 0.f;

    FEPHitboxSnapshot Result;
    Result.ServerTime = TargetTime;
    Result.Location   = FMath::Lerp(Before->Location, After->Location, Alpha);

    // per-bone 보간 — FTransform::BlendWith는 내부적으로 Quat Slerp 사용
    for (int32 i = 0; i < Before->Bones.Num(); ++i)
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

**FTransform::BlendWith를 쓰는 이유:**
- `FMath::Lerp(FRotator, FRotator)` → 각도 랩어라운드(-179° ↔ 181°) 문제 발생
- `BlendWith`는 내부적으로 쿼터니언 Slerp → 올바른 회전 보간 보장

### 4. ConfirmHitscan 전체 흐름

```
[단계 요약]
1. MaxRewindSeconds 클램프 (0.5초 초과 = 조작 의심)
2. Broad Phase — GetHitscanCandidates()
3. 후보 전체 리와인드 (FBodyInstance::SetBodyTransform)
4. Narrow Phase — LineTrace N회 (단일/산탄총 공용)
5. 후보 전체 복구 (동일 API로 원래 Transform 복원)
6. OutConfirmedHits 반환
```

**FBodyInstance를 쓰는 이유:**

```cpp
// 잘못된 방법: SetBoneTransformByName
// → 애니메이션 포즈(BoneSpaceTransform) 변경 — 물리 바디 이동 안됨
// → 다음 틱에 덮어씌워져 리와인드 무효

// 올바른 방법: FBodyInstance::SetBodyTransform
const FBodyInstance* Body = Mesh->GetBodyInstance(BoneName);
Body->SetBodyTransform(SnapshotTransform, ETeleportType::TeleportPhysics);
// → 물리 바디(콜리전 프리미티브) 직접 이동 → LineTrace에 즉시 반응
// → ETeleportType::TeleportPhysics: 속도 초기화, 물리 시뮬레이션 재시작 없음
```

**리와인드 → 복구 코드 구조:**

```cpp
// 1. 현재 Transform 저장 + 리와인드
struct FEPRewindEntry {
    AEPCharacter*          Character;
    TArray<FEPBoneSnapshot> SavedBones;
};

TArray<FEPRewindEntry> Entries;
for (AEPCharacter* Char : Candidates)
{
    FEPRewindEntry& Entry = Entries.AddDefaulted_GetRef();
    Entry.Character = Char;
    // 현재 본 Transform 저장
    for (const FName& BoneName : HitBones)
    {
        FBodyInstance* Body = Char->GetMesh()->GetBodyInstance(BoneName);
        FEPBoneSnapshot Snap;
        Snap.BoneName       = BoneName;
        Snap.WorldTransform = Body->GetUnrealWorldTransform();
        Entry.SavedBones.Add(Snap);
    }
    // 과거 위치로 이동
    const FEPHitboxSnapshot PastSnap = GetSnapshotAtTime(ClientFireTime);
    for (const FEPBoneSnapshot& BoneSnap : PastSnap.Bones)
    {
        FBodyInstance* Body = Char->GetMesh()->GetBodyInstance(BoneSnap.BoneName);
        if (Body) Body->SetBodyTransform(BoneSnap.WorldTransform, ETeleportType::TeleportPhysics);
    }
}

// 2. Narrow Phase
TArray<FHitResult> ConfirmedHits;
FCollisionQueryParams Params;
Params.bReturnPhysicalMaterial = true; // Hit.PhysMaterial 획득 필수
Params.AddIgnoredActor(Shooter);
Params.AddIgnoredActor(EquippedWeapon);

for (const FVector& Dir : Directions)
{
    const FVector End = Origin + Dir * WeaponDef->TraceDistanceCm;
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, EP_TraceChannel_Weapon, Params))
    {
        AEPCharacter* HitChar = Cast<AEPCharacter>(Hit.GetActor());
        if (HitChar && CandidateSet.Contains(HitChar)) // 후보 외 히트 차단
            ConfirmedHits.Add(Hit);
    }
}

// 3. 복구 (반드시 Narrow Phase 직후, 지연 금지)
for (const FEPRewindEntry& Entry : Entries)
{
    for (const FEPBoneSnapshot& BoneSnap : Entry.SavedBones)
    {
        FBodyInstance* Body = Entry.Character->GetMesh()->GetBodyInstance(BoneSnap.BoneName);
        if (Body) Body->SetBodyTransform(BoneSnap.WorldTransform, ETeleportType::TeleportPhysics);
    }
}
```

**후보 외 히트 차단이 필요한 이유:**
```
리와인드 후 Trace를 쏘면, 후보가 아닌 캐릭터에도 맞을 수 있다.
(후보가 아닌 캐릭터는 현재 위치에 있음 — 우연히 경로에 있으면 히트)
CandidateSet.Contains() 필터로 이를 차단한다.
```

### 5. 디버그 시각화 시스템

`UEPCombatDeveloperSettings` (`Config/DefaultGame.ini`에 저장):

```cpp
// DeveloperSettings이므로 코드 변경 없이 에디터에서 토글 가능
UPROPERTY(Config, EditAnywhere, Category = "Debug|SSR")
bool bEnableSSRDebugDraw = false;

UPROPERTY(Config, EditAnywhere, Category = "Debug|SSR")
float SSRDebugDrawDuration = 3.f;
```

**디버그 색상 코드:**
| 색상 | 의미 |
|------|------|
| Blue | 리와인드 전 현재 물리 프리미티브 |
| Red  | 리와인드 후 과거 위치의 물리 프리미티브 |
| White | 트레이스 선 (Origin → End) |
| Yellow | 확정 히트 지점 |

```cpp
// Shipping/Test 빌드에서 자동 제거
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
if (Settings->bEnableSSRDebugDraw)
{
    DrawHitBonesPrimitivesForCharacter(World, Char, HitBones, FColor::Blue, Duration, Thickness);
    // 리와인드 후:
    DrawHitBonesPrimitivesForCharacter(World, Char, HitBones, FColor::Red, Duration, Thickness);
}
#endif
```

---

## 결과

**확인 항목 (PIE Dedicated Server + Client 2명):**
- `bEnableSSRDebugDraw = true` 설정 후 사격 시 Blue(현재)/Red(과거) 박스 표시
- 높은 핑 에뮬(Network Emulation 100ms) 상태에서도 클라 발사 시점 기준 히트 판정
- ConfirmedHits 로그에서 BoneName 확인 (head, spine 등)

**한계 및 향후 개선:**
- O(N) Broad Phase — 플레이어 수 증가 시 Spatial Hash로 교체 가능 (GetHitscanCandidates 함수만 교체)
- SnapshotInterval이 길면 보간 오차 발생 → `UEPCombatDeveloperSettings`로 조정

---

## 참고

- `DOCS/Notes/03_BoneHitbox_Implementation.md` Step 2~4
- `DOCS/Mine/Rewind.md` — SSR 분리 설계 근거
- `DOCS/Mine/Debug.md` — 디버그 시각화 가이드
