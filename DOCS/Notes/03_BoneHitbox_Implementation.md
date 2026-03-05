# 3단계 외전 구현서: BoneHitbox

> 기준 문서: `03_BoneHitbox.md`
> 전제 조건: 기본 `Server_Fire` RPC가 존재하고 빌드 통과 상태
> 목표: **현재 코드에서 바로 따라 구현 가능한 순서** 제공
> 문서 우선순위: 구현 시 충돌하면 `03_BoneHitbox_Implementation.md`를 기준으로 한다.

---

## 0. 구현 철학

1. **판정 책임 분리** — `CombatComponent`: 트레이스·배율 계산, `EPCharacter`: HP·사망만
2. **FBodyInstance 사용** — `SetBoneTransformByName`(포즈 변경)이 아닌 물리 바디 직접 이동
3. **시간 소스 통일** — `GS->GetServerWorldTimeSeconds()` (서버·클라이언트 시계 일치)
4. **GAS 연결 예약** — Damage Block을 `ApplyPointDamage` 래퍼로 고립

### 하드코딩 금지 원칙 (이번 수정 핵심)

아래 값들은 코드 리터럴로 두지 않고 설정값으로 관리한다.

- LagComp 윈도우 (`MaxRewindSeconds`)
- 히스토리 기록 간격 (`SnapshotIntervalSeconds`)
- 히스토리 길이 (`MaxHistoryCount`)
- 브로드 페이즈 여유 거리 (`BroadPhasePaddingCm`)
- 히트스캔 사거리 (`TraceDistanceCm`)
- 본 배율 (`BoneDamageMultiplierMap`)

권장 소스:
- 무기 고유값: `UEPWeaponDefinition`
- 시스템 공통값: `UEPCombatDeveloperSettings`(UDeveloperSettings)

---

## 1. 수정/추가 대상 파일

**수정:**
- `Public/Types/EPTypes.h`
- `Public/Core/EPCharacter.h`
- `Private/Core/EPCharacter.cpp`
- `Public/Combat/EPCombatComponent.h`
- `Private/Combat/EPCombatComponent.cpp`

**신규:**
- `Public/Combat/EPPhysicalMaterial.h`
- `Private/Combat/EPPhysicalMaterial.cpp`

**에디터:**
- Physics Asset — 본 바디 추가 + WeaponTrace Block + PM 할당

### 1-1. 신규 클래스/파일 폴더 가이드

아래 경로를 기준으로 생성한다.

#### A. Physical Material 클래스
- 헤더: `EmploymentProj/Source/EmploymentProj/Public/Combat/EPPhysicalMaterial.h`
- 소스: `EmploymentProj/Source/EmploymentProj/Private/Combat/EPPhysicalMaterial.cpp`

이유:
- 히트 판정/데미지 계산에서 `EPCombatComponent`가 직접 참조한다.
- 현재 프로젝트 구조상 Combat 도메인에 두는 것이 가장 자연스럽다.

#### B. Combat DeveloperSettings 클래스 (권장)
- 헤더: `EmploymentProj/Source/EmploymentProj/Public/Combat/EPCombatDeveloperSettings.h`
- 소스: `EmploymentProj/Source/EmploymentProj/Private/Combat/EPCombatDeveloperSettings.cpp`

이유:
- `MaxRewindSeconds`, `SnapshotIntervalSeconds`, `BroadPhasePaddingCm` 등
  전투 공통 튜닝값을 한곳에서 관리하기 위함.

#### C. 데이터 필드 추가 위치
- 무기별 값 (`TraceDistanceCm`, `BoneDamageMultiplierMap`)은
  `EmploymentProj/Source/EmploymentProj/Public/Data/EPWeaponDefinition.h`에 추가.
- 구현이 필요하면 `EmploymentProj/Source/EmploymentProj/Private/Data/EPWeaponDefinition.cpp` 사용.

#### D. 스냅샷 타입 위치
- `FEPBoneSnapshot`, `FEPHitboxSnapshot`, `EP_TraceChannel_Weapon` 상수는
  `EmploymentProj/Source/EmploymentProj/Public/Types/EPTypes.h` 유지.

---

--- 

## Step 0) 트레이스 채널 확인 + EP_TraceChannel_Weapon 상수 정의

### 0-1. DefaultEngine.ini 채널 확인

`Config/DefaultEngine.ini`에 아래 줄이 있어야 한다:

```ini
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="WeaponTrace")
```

이미 존재하고 `DefaultResponse=ECR_Ignore`이면 변경 불필요.

> **왜 Ignore여야 하는가**: Default가 Block이면 벽/바닥도 이 채널에 반응한다.
> 리와인드 시 캐릭터가 과거 위치(벽 안쪽일 수 있음)로 이동하면
> LineTrace가 벽에 먼저 막혀 히트 판정 실패가 발생한다.

### 0-2. EP_TraceChannel_Weapon 상수 정의

파일: `Public/Types/EPTypes.h` (파일 하단, GENERATED_BODY 없는 일반 상수)

```cpp
// WeaponTrace = ECC_GameTraceChannel1 (DefaultEngine.ini 확인 후 번호 일치시킬 것)
static constexpr ECollisionChannel EP_TraceChannel_Weapon = ECC_GameTraceChannel1;
```

> 채널 번호는 DefaultEngine.ini의 `Channel=ECC_GameTraceChannelN`에서 N 확인.
> 현재 프로젝트는 `ECC_GameTraceChannel1`.

### 0-3. Combat 설정값 컨테이너 추가 (권장)

하드코딩 제거를 위해 `UDeveloperSettings`를 추가한다.

```cpp
UCLASS(Config=Game, DefaultConfig)
class UEPCombatDeveloperSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="LagComp")
    float MaxRewindSeconds = 0.2f;

    UPROPERTY(Config, EditAnywhere, Category="LagComp")
    float SnapshotIntervalSeconds = 0.05f; // 20Hz

    UPROPERTY(Config, EditAnywhere, Category="Trace")
    float BroadPhasePaddingCm = 50.f;

    UPROPERTY(Config, EditAnywhere, Category="Trace")
    float DefaultTraceDistanceCm = 10000.f;
};
```

또는 무기별 값은 `UEPWeaponDefinition`으로 이동:

```cpp
UPROPERTY(EditDefaultsOnly, Category="Weapon|Trace")
float TraceDistanceCm = 10000.f;

UPROPERTY(EditDefaultsOnly, Category="Weapon|Damage")
TMap<FName, float> BoneDamageMultiplierMap;
```

---

## Step 1) Physics Asset 에디터 설정

> 코드 작업 전에 완료해야 런타임에서 `GetBodyInstance`가 올바른 바디를 반환한다.

### 1-1. 콜리전 바디 추가

`BP_EPCharacter`의 Physics Asset 열기:

| 본 | 형태 | 배율 목적 |
|----|------|----------|
| `head` | Sphere | 헤드샷 2.0x |
| `spine_03`, `spine_01` | Capsule | 상하체 1.0x |
| `upperarm_l/r`, `lowerarm_l/r` | Capsule | 팔 0.75x |
| `thigh_l/r`, `calf_l/r` | Capsule | 다리 0.75x |

각 바디: `Physics Type` → **Kinematic** (애니메이션만 따라감, 물리 시뮬레이션 없음)

### 1-2. WeaponTrace 채널 Block 설정

모든 히트박스 바디 선택 (`Ctrl+A`) →
`Collision Responses` → `WeaponTrace`: **Block**

> 환경 메시(Static Mesh, Landscape)는 Default Ignore를 그대로 유지한다.

### 1-3. Physics Asset 저장

`File → Save` 또는 `Ctrl+S` — 저장 전 바디 이름이 본 이름과 일치하는지 확인
(`head`, `spine_03` 등 — Physics Asset이 자동으로 본 이름을 바디 이름으로 사용)

---

## Step 1-5) 서버 애니메이션 Tick 강제 활성화

> **왜 필요한가:**
> `GetBoneTransform`은 애니메이션 그래프 평가 결과를 반환한다.
> UE5 기본 설정은 화면에 보이지 않으면 애니메이션 Tick을 줄이거나 끈다.
> 전용 서버는 렌더링이 없으므로 기본값을 그대로 두면
> **서버에서 본 Transform이 갱신되지 않아 스냅샷이 항상 같은 포즈**로 고정된다.

파일: `Private/Core/EPCharacter.cpp` → 생성자(또는 BeginPlay)에 추가

```cpp
// 서버에서 스켈레탈 메시 애니메이션이 항상 평가되도록 강제
// 기본값(OnlyTickMontagesWhenNotRendered 등)은 전용 서버에서 본 위치 갱신을 중단한다
GetMesh()->VisibilityBasedAnimTickOption =
    EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
```

> **AlwaysTickPoseAndRefreshBones**: 화면에 보이지 않아도 매 프레임 애니메이션 평가 + 본 Transform 갱신.
> 서버는 렌더링이 없으므로 이 옵션 없이는 본 스냅샷이 정적 포즈로 고정된다.
>
> 퍼포먼스: 서버에서는 렌더링이 없어 CPU 비용만 발생. 캐릭터 수가 많아지면
> 부하가 될 수 있으나 현재 프로젝트 규모에서는 문제없다.

---

## Step 2) EPTypes.h — FEPBoneSnapshot + FEPHitboxSnapshot 추가

파일: `Public/Types/EPTypes.h`

기존 enum 선언들 아래에 추가:

```cpp
// ─── Hitbox Snapshot ───────────────────────────────────────────────────────
// 서버 히스토리에 저장되는 본 단위 스냅샷.
// 복제되지 않는다 — 서버 전용 Lag Compensation 데이터.

USTRUCT()
struct FEPBoneSnapshot
{
    GENERATED_BODY()

    UPROPERTY() FName      BoneName;
    UPROPERTY() FTransform WorldTransform; // 서버 시뮬레이션 기준 본 월드 Transform
};

USTRUCT()
struct FEPHitboxSnapshot
{
    GENERATED_BODY()

    UPROPERTY() float   ServerTime = 0.f;
    UPROPERTY() FVector Location   = FVector::ZeroVector;  // Broad Phase용 (캐릭터 루트)
    UPROPERTY() TArray<FEPBoneSnapshot> Bones;             // Narrow Phase 리와인드용
};
```

---

## Step 3) EPCharacter.h — 히트박스 관련 선언 추가

파일: `Public/Core/EPCharacter.h`

### 3-1. include 추가 (파일 상단)

```cpp
#include "Types/EPTypes.h"
```

### 3-2. public 섹션에 추가

```cpp
// Lag Compensation — 서버에서 GetSnapshotAtTime 호출
FEPHitboxSnapshot GetSnapshotAtTime(float TargetTime) const;

// 사망 여부 — Broad Phase에서 이미 죽은 캐릭터 제외
bool IsDead() const { return HP <= 0; }
```

### 3-3. protected 섹션에 추가

```cpp
// 히트박스 스냅샷 히스토리 (서버 전용, 복제 없음)
static const TArray<FName> HitBones;      // 기록할 본 목록
float SnapshotAccumulator = 0.f;          // Tick 누적

// 하드코딩 대신 설정값에서 읽는다.
// 예: CombatSettings->SnapshotIntervalSeconds (기본 0.05f)
// 예: CombatSettings->MaxRewindSeconds      (기본 0.2f)
int32 MaxHistoryCount = 0;

// 시간 오름차순으로 유지된다 — [0]이 가장 오래됨, [Last]가 가장 최신
// 링버퍼 대신 단순 배열을 사용해 GetSnapshotAtTime의 탐색 순서를 보장한다
TArray<FEPHitboxSnapshot> HitboxHistory;

// 서버 Tick에서 SnapshotInterval마다 호출
void SaveHitboxSnapshot();
```

### 3-4. BeginPlay에서 MaxHistoryCount 초기화

`MaxHistoryCount`는 반드시 `BeginPlay`에서 계산해 초기화한다.

```cpp
void AEPCharacter::BeginPlay()
{
    Super::BeginPlay();

    const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();
    const float Interval = FMath::Max(0.01f, CombatSettings->SnapshotIntervalSeconds);
    const float RewindWindow = FMath::Max(0.05f, CombatSettings->MaxRewindSeconds);

    // 리와인드 윈도우 + 경계 보간 여유 2프레임
    MaxHistoryCount = FMath::CeilToInt(RewindWindow / Interval) + 2;
}
```

---

## Step 4) EPCharacter.cpp — HitBones 정의 + SaveHitboxSnapshot

파일: `Private/Core/EPCharacter.cpp`

### 4-1. include 추가

```cpp
#include "GameFramework/GameStateBase.h"
#include "Combat/EPCombatDeveloperSettings.h"
```

### 4-2. HitBones 정의 (.cpp 상단, 클래스 외부)

```cpp
const TArray<FName> AEPCharacter::HitBones =
{
    TEXT("head"),
    TEXT("spine_03"), TEXT("spine_01"),
    TEXT("upperarm_l"), TEXT("upperarm_r"),
    TEXT("lowerarm_l"), TEXT("lowerarm_r"),
    TEXT("thigh_l"),    TEXT("thigh_r"),
    TEXT("calf_l"),     TEXT("calf_r")
};
```

### 4-3. Tick에서 SaveHitboxSnapshot 호출

기존 `BeginPlay`에 추가하거나 `TickComponent`를 활용:

```cpp
void AEPCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 서버에서만 스냅샷 기록
    if (!HasAuthority()) return;

    const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();
    SnapshotAccumulator += DeltaTime;

    if (SnapshotAccumulator >= CombatSettings->SnapshotIntervalSeconds)
    {
        SnapshotAccumulator = 0.f;
        SaveHitboxSnapshot();
    }
}
```

> `Tick`이 기본적으로 켜져 있는지 확인: 생성자에 `PrimaryActorTick.bCanEverTick = true;` 필요.
> 이미 설정되어 있다면 추가 불필요.

### 4-4. SaveHitboxSnapshot 구현

```cpp
void AEPCharacter::SaveHitboxSnapshot()
{
    // 전제: GetMesh()->VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones
    // 이 설정 없이는 전용 서버에서 본 Transform이 갱신되지 않아
    // 모든 스냅샷이 동일한 정적 포즈로 기록된다.

    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ServerNow = GS ? GS->GetServerWorldTimeSeconds()
                               : GetWorld()->GetTimeSeconds();

    FEPHitboxSnapshot Snapshot;
    Snapshot.ServerTime = ServerNow;
    Snapshot.Location   = GetActorLocation();

    for (const FName& BoneName : HitBones)
    {
        const int32 BoneIndex = GetMesh()->GetBoneIndex(BoneName);
        if (BoneIndex == INDEX_NONE) continue;

        FEPBoneSnapshot Bone;
        Bone.BoneName       = BoneName;
        Bone.WorldTransform = GetMesh()->GetBoneTransform(BoneIndex);
        Snapshot.Bones.Add(Bone);
    }

    // 시간 오름차순 배열 유지.
    // MaxHistoryCount = CeilToInt(MaxRewindSeconds / SnapshotIntervalSeconds) + 2 권장.
    if (HitboxHistory.Num() >= MaxHistoryCount)
        HitboxHistory.RemoveAt(0);
    HitboxHistory.Add(Snapshot);
}
```

---

## Step 5) EPCharacter.cpp — GetSnapshotAtTime 구현

```cpp
FEPHitboxSnapshot AEPCharacter::GetSnapshotAtTime(float TargetTime) const
{
    if (HitboxHistory.IsEmpty())
        return FEPHitboxSnapshot{};

    // 경계 처리: 과거면 First, 미래면 Last
    if (TargetTime <= HitboxHistory[0].ServerTime)
        return HitboxHistory[0];
    if (TargetTime >= HitboxHistory.Last().ServerTime)
        return HitboxHistory.Last();

    // TargetTime을 감싸는 Before/After 탐색
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

    // 정상적으로는 경계 처리에서 이미 걸러짐. 예외 시 안전 반환.
    if (!Before || !After)
        return HitboxHistory.Last();

    const float Denom = After->ServerTime - Before->ServerTime;
    const float Alpha = (Denom > KINDA_SMALL_NUMBER)
                      ? (TargetTime - Before->ServerTime) / Denom
                      : 0.f;

    FEPHitboxSnapshot Result;
    Result.ServerTime = TargetTime;
    Result.Location   = FMath::Lerp(Before->Location, After->Location, Alpha);

    // per-bone Transform 보간
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

## Step 6) EPCombatComponent.h — 새 함수 선언 + Server_Fire 시그니처 변경

파일: `Public/Combat/EPCombatComponent.h`

### 6-1. forward declaration 추가

```cpp
class UEPPhysicalMaterial;
```

### 6-2. RequestFire 시그니처 변경

```cpp
// 기존: void RequestFire(const FVector& Origin, const FVector& Direction);
// 변경: ClientFireTime 추가
void RequestFire(const FVector& Origin, const FVector& Direction, float ClientFireTime);
```

### 6-3. Server_Fire 시그니처 변경

```cpp
// 기존: void Server_Fire(const FVector& Origin, const FVector& Direction);
// 변경:
UFUNCTION(Server, Reliable)
void Server_Fire(const FVector_NetQuantize& Origin,
                 const FVector_NetQuantizeNormal& Direction,
                 float ClientFireTime);
```

> `FVector_NetQuantize` / `FVector_NetQuantizeNormal`으로 교체해 네트워크 대역폭 절약.

### 6-4. private 섹션에 추가

```cpp
private:
    // Broad Phase — 총알 경로 근방 후보 선정
    TArray<AEPCharacter*> GetHitscanCandidates(
        AEPCharacter*          Owner,
        const FVector&         Origin,
        const TArray<FVector>& Directions,
        float                  ClientFireTime) const;

    // Rewind + Trace + 복구 + 데미지
    void HandleHitscanFire(
        AEPCharacter*          Owner,
        const FVector&         Origin,
        const TArray<FVector>& Directions,
        float                  ClientFireTime);

    // 부위 배율
    float GetBoneMultiplier(const FName& BoneName) const;
    static float GetMaterialMultiplier(const UPhysicalMaterial* PM);
```

---

## Step 7) EPCombatComponent.cpp — FEPRewindEntry + GetHitscanCandidates

파일: `Private/Combat/EPCombatComponent.cpp`

### 7-1. include 추가

```cpp
#include "EngineUtils.h"              // TActorIterator
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/GameStateBase.h"
#include "Combat/EPCombatDeveloperSettings.h"
#include "Combat/EPPhysicalMaterial.h"
#include "Types/EPTypes.h"
```

### 7-2. FEPRewindEntry (파일 상단, 클래스 외부)

```cpp
// AEPCharacter*에 의존하므로 EPTypes.h에 넣지 않는다.
// 복제 불필요 — 리와인드 중 임시 사용 후 즉시 복구된다.
struct FEPRewindEntry
{
    AEPCharacter*           Character  = nullptr;
    TArray<FEPBoneSnapshot> SavedBones;
};
```

### 7-3. GetHitscanCandidates 구현

```cpp
TArray<AEPCharacter*> UEPCombatComponent::GetHitscanCandidates(
    AEPCharacter*          Owner,
    const FVector&         Origin,
    const TArray<FVector>& Directions,
    float                  ClientFireTime) const
{
    TArray<AEPCharacter*> Candidates;

    for (TActorIterator<AEPCharacter> It(GetWorld()); It; ++It)
    {
        AEPCharacter* Char = *It;
        if (!Char || Char == Owner) continue;
        if (Char->IsDead()) continue; // 이미 사망 — 리와인드 대상 제외

        // 과거 위치 기준 Broad Phase (현재 위치 기준이면 이동한 대상을 놓침)
        const FEPHitboxSnapshot Snap = Char->GetSnapshotAtTime(ClientFireTime);
        const float CapsuleRadius    = Char->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();
        const float BroadRadius      = CapsuleRadius + CombatSettings->BroadPhasePaddingCm;

        for (const FVector& Dir : Directions)
        {
            const float TraceDistanceCm = EquippedWeapon && EquippedWeapon->WeaponDef
                ? EquippedWeapon->WeaponDef->TraceDistanceCm
                : CombatSettings->DefaultTraceDistanceCm;
            const FVector End = Origin + Dir * TraceDistanceCm;
            if (FMath::PointDistToSegment(Snap.Location, Origin, End) <= BroadRadius)
            {
                Candidates.Add(Char);
                break; // 이미 후보 — 나머지 방향 체크 불필요
            }
        }
    }

    return Candidates;
}
```

---

## Step 8) EPCombatComponent.cpp — HandleHitscanFire 구현

```cpp
void UEPCombatComponent::HandleHitscanFire(
    AEPCharacter*          Owner,
    const FVector&         Origin,
    const TArray<FVector>& Directions,
    float                  ClientFireTime)
{
    // ── [Rewind Block] ────────────────────────────────────────────────────────
    // 0. 리와인드 윈도우 클램프 (설정값 초과 = 현재 시점으로 클램프)
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();
    const float ServerNow = GS ? GS->GetServerWorldTimeSeconds()
                               : GetWorld()->GetTimeSeconds();
    if (ServerNow - ClientFireTime > CombatSettings->MaxRewindSeconds)
        ClientFireTime = ServerNow;

    // 1. Broad Phase — 총알 경로 근방 후보 선정
    const TArray<AEPCharacter*> Candidates =
        GetHitscanCandidates(Owner, Origin, Directions, ClientFireTime);

    // 2. 본 단위 리와인드 + 현재 물리 바디 Transform 저장
    TArray<FEPRewindEntry> RewindEntries;
    RewindEntries.Reserve(Candidates.Num());

    for (AEPCharacter* Char : Candidates)
    {
        FEPRewindEntry& Entry = RewindEntries.AddDefaulted_GetRef();
        Entry.Character = Char;

        const FEPHitboxSnapshot Snap = Char->GetSnapshotAtTime(ClientFireTime);

        for (const FEPBoneSnapshot& Bone : Snap.Bones)
        {
            FBodyInstance* Body = Char->GetMesh()->GetBodyInstance(Bone.BoneName);
            if (!Body) continue;

            FEPBoneSnapshot Saved;
            Saved.BoneName       = Bone.BoneName;
            Saved.WorldTransform = Body->GetUnrealWorldTransform();
            Entry.SavedBones.Add(Saved);

            Body->SetBodyTransform(Bone.WorldTransform, ETeleportType::TeleportPhysics);
        }
    }

    // 3. Narrow Phase — 모든 후보가 과거 위치에 있는 상태에서 N방향 LineTrace
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HitscanFire), false);
    Params.AddIgnoredActor(Owner);
    if (EquippedWeapon) Params.AddIgnoredActor(EquippedWeapon);
    Params.bReturnPhysicalMaterial = true; // Hit.PhysMaterial 확보 (약점 PM 판별용)

    TArray<FHitResult> ConfirmedHits;
    for (const FVector& Dir : Directions)
    {
        const float TraceDistanceCm = EquippedWeapon && EquippedWeapon->WeaponDef
            ? EquippedWeapon->WeaponDef->TraceDistanceCm
            : CombatSettings->DefaultTraceDistanceCm;
        const FVector End = Origin + Dir * TraceDistanceCm;
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(
                Hit, Origin, End, EP_TraceChannel_Weapon, Params)
            && Cast<AEPCharacter>(Hit.GetActor()) != nullptr)
        {
            ConfirmedHits.Add(Hit);
        }
    }

    // 4. 일괄 복구 (반드시 Narrow Phase 직후 — 순서 변경 금지)
    for (const FEPRewindEntry& Entry : RewindEntries)
    {
        for (const FEPBoneSnapshot& Saved : Entry.SavedBones)
        {
            FBodyInstance* Body = Entry.Character->GetMesh()->GetBodyInstance(Saved.BoneName);
            if (Body)
                Body->SetBodyTransform(Saved.WorldTransform, ETeleportType::TeleportPhysics);
        }
    }

    // ── [Damage Block] ────────────────────────────────────────────────────────
    // GAS 전환 시 GameplayEffectSpec + SetByCaller로 교체
    for (const FHitResult& Hit : ConfirmedHits)
    {
        if (!Hit.GetActor()) continue;

        const float BaseDamage       = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
        const float BoneMultiplier   = GetBoneMultiplier(Hit.BoneName);
        const float MaterialMultiplier = GetMaterialMultiplier(Hit.PhysMaterial.Get());
        const float FinalDamage      = BaseDamage * BoneMultiplier * MaterialMultiplier;

        UE_LOG(LogTemp, Log,
            TEXT("[BoneHitbox] Bone=%s PM=%s Base=%.1f Bone×=%.2f Mat×=%.2f Final=%.1f"),
            *Hit.BoneName.ToString(),
            Hit.PhysMaterial.IsValid() ? *Hit.PhysMaterial->GetName() : TEXT("None"),
            BaseDamage, BoneMultiplier, MaterialMultiplier, FinalDamage);

        UGameplayStatics::ApplyPointDamage(
            Hit.GetActor(),
            FinalDamage,
            (Hit.ImpactPoint - Origin).GetSafeNormal(),
            Hit,
            Owner->GetController(),
            Owner,
            UDamageType::StaticClass()
        );
    }
}
```

### GetBoneMultiplier / GetMaterialMultiplier

```cpp
float UEPCombatComponent::GetBoneMultiplier(const FName& BoneName) const
{
    // 권장: WeaponDefinition 또는 CombatSettings의 맵으로 관리
    // TMap<FName, float> BoneDamageMultiplierMap;
    // 예: head=2.0, spine_03=1.0, upperarm_l=0.75 ...
    if (EquippedWeapon && EquippedWeapon->WeaponDef)
        if (const float* Found = EquippedWeapon->WeaponDef->BoneDamageMultiplierMap.Find(BoneName))
            return *Found;

    // 누락 본은 기본 배율 1.0 + 경고 로그
    UE_LOG(LogTemp, Verbose, TEXT("[BoneHitbox] Bone multiplier fallback: %s"), *BoneName.ToString());
    return 1.0f;
}

float UEPCombatComponent::GetMaterialMultiplier(const UPhysicalMaterial* PM)
{
    if (const UEPPhysicalMaterial* EPM = Cast<UEPPhysicalMaterial>(PM))
    {
        // 현재 단계(Pre-GAS): bool/배율 기반
        if (EPM->bIsWeakSpot) return EPM->WeakSpotMultiplier;

        // GAS 단계: PhysicalMaterial의 GameplayTagContainer 기반으로 판정
        // 예: if (EPM->MaterialTags.HasTag(TAG_Gameplay_Zone_WeakSpot)) ...
    }
    return 1.0f;
}
```

---

## Step 9) EPCombatComponent.cpp — Server_Fire_Implementation 교체

기존 `Server_Fire_Implementation`을 `HandleHitscanFire`를 호출하는 방식으로 교체한다.

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector_NetQuantize&      Origin,
    const FVector_NetQuantizeNormal& Direction,
    float                           ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;

    FVector SpreadDir = Direction;
    EquippedWeapon->Fire(SpreadDir); // 탄약 감소

    AEPCharacter* Owner = GetOwnerCharacter();

    // 히트스캔: 방향 배열로 감싸 HandleHitscanFire에 전달 (산탄총 확장 대비)
    const TArray<FVector> Directions = { SpreadDir };
    HandleHitscanFire(Owner, Origin, Directions, ClientFireTime);

    // 이펙트 (판정과 무관하게 발사 시 항상 재생)
    const FVector MuzzleLocation =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();

    Multicast_PlayMuzzleEffect(MuzzleLocation);
}
```

---

## Step 10) EPCombatComponent.cpp — RequestFire 업데이트

파일: `Private/Combat/EPCombatComponent.cpp`

`RequestFire`가 `ClientFireTime`을 서버에 전달하도록 수정한다.

```cpp
void UEPCombatComponent::RequestFire(
    const FVector& Origin, const FVector& Direction, float ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;
    if (EquippedWeapon->CurrentAmmo <= 0) return;

    AEPCharacter* Owner = GetOwnerCharacter();

    // 연사속도 체크 (클라이언트 측)
    const float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
    const float CurrentTime  = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LocalLastFireTime < FireInterval) return;
    LocalLastFireTime = CurrentTime;

    // ClientFireTime을 서버에 전달
    Server_Fire(Origin, Direction, ClientFireTime);

    // 로컬 반동
    if (Owner->IsLocallyControlled())
    {
        Owner->AddControllerPitchInput(-EquippedWeapon->GetRecoilPitch());
        Owner->AddControllerYawInput(FMath::RandRange(
            -EquippedWeapon->GetRecoilYaw(), EquippedWeapon->GetRecoilYaw()));
    }
}
```

파일: `Private/Core/EPCharacter.cpp`의 `Input_Fire` 업데이트:

```cpp
void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
    if (!CombatComponent) return;

    // GetWorld()->GetTimeSeconds()는 클라이언트 로컬 시간이다.
    // 서버의 GetServerWorldTimeSeconds()와 기준점이 달라 항상 오프셋이 존재한다.
    // AGameStateBase::GetServerWorldTimeSeconds()는 클라이언트에서도 호출 가능하며
    // UE 엔진이 서버 시간을 클라이언트에 동기화해주므로 서버와 직접 비교할 수 있다.
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ClientFireTime = GS ? GS->GetServerWorldTimeSeconds()
                                    : GetWorld()->GetTimeSeconds();

    CombatComponent->RequestFire(
        FirstPersonCamera->GetComponentLocation(),
        FirstPersonCamera->GetForwardVector(),
        ClientFireTime
    );
}
```

---

## Step 11) UEPPhysicalMaterial 신규 클래스

### 11-1. Header

파일: `Public/Combat/EPPhysicalMaterial.h`

```cpp
#pragma once

#include "PhysicalMaterials/PhysicalMaterial.h"
#include "EPPhysicalMaterial.generated.h"

// 약점 여부와 배율을 Physics Asset 바디에 직접 바인딩하기 위한 커스텀 PM.
// GAS 전환 시 MaterialTags + WeaponDefinition 배율 맵을 사용한다.
UCLASS()
class EMPLOYMENTPROJ_API UEPPhysicalMaterial : public UPhysicalMaterial
{
    GENERATED_BODY()

public:
    // GAS 확장용: Lyra 방식과 동일하게 바디에 GameplayTag 부여
    UPROPERTY(EditDefaultsOnly, Category = "Damage")
    FGameplayTagContainer MaterialTags;

    UPROPERTY(EditDefaultsOnly, Category = "Damage")
    bool bIsWeakSpot = false;

    UPROPERTY(EditDefaultsOnly, Category = "Damage",
        meta = (EditCondition = "bIsWeakSpot"))
    float WeakSpotMultiplier = 2.0f;
};
```

### 11-2. Source

파일: `Private/Combat/EPPhysicalMaterial.cpp`

```cpp
#include "Combat/EPPhysicalMaterial.h"
```

### 11-3. 에디터에서 PM 에셋 생성

1. `Content Browser` → `Content/Data/` 우클릭 → `Physics → Physical Material`
2. Parent Class: `EPPhysicalMaterial` 선택
3. 이름: `PM_WeakSpot`
4. `bIsWeakSpot = true`, `WeakSpotMultiplier = <설정값>`으로 저장

### 11-4. Physics Asset에 PM 할당 (Step 1 보충)

Physics Asset `PHA_EPCharacter` 열기 →
`head` 바디 선택 → `Simple Collision Physical Material` → `PM_WeakSpot` 할당 → 저장

---

## Step 12) EPCharacter.cpp — TakeDamage 단순화

파일: `Private/Core/EPCharacter.cpp`

배율 계산이 `HandleHitscanFire`로 이전되었으므로 TakeDamage에서 제거한다.

기존 코드를 아래로 교체:

```cpp
float AEPCharacter::TakeDamage(
    float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority()) return 0.f;

    // 배율은 HandleHitscanFire에서 이미 적용된 FinalDamage가 넘어온다.
    HP = FMath::Clamp(HP - DamageAmount, 0.f, static_cast<float>(MaxHP));

    Multicast_PlayHitReact();
    Multicast_PlayPainSound();

    if (AEPPlayerController* InstigatorPC = Cast<AEPPlayerController>(EventInstigator))
        InstigatorPC->Client_PlayHitConfirmSound();

    if (HP <= 0.f) Die(EventInstigator);

    ForceNetUpdate();
    return DamageAmount;
}
```

---

## Step 13) 빌드 확인

```bash
UnrealBuildTool.exe EmploymentProj Win64 Development \
  -project="EmploymentProj/EmploymentProj.uproject"
```

오류가 없으면 에디터 실행 후 Step 14로.

---

## Step 14) 테스트 절차 + 트러블슈팅

### 테스트 환경

PIE → `New Editor Window (PIE)` → Number of Players: 2 → Dedicated Server 모드

### 테스트 체크리스트

```
[ ] 빌드 오류 없음
[ ] PIE 실행 후 크래시 없음

[ ] 로그 확인: [BoneHitbox] 출력 있음
[ ] 헤드 히트: Bone=head, Bone×=2.00, Final = BaseDamage × 2.0
[ ] 몸통 히트: Bone=spine_03, Bone×=1.00, Final = BaseDamage × 1.0
[ ] 사지 히트: Bone=upperarm_l 등, Bone×=0.75, Final = BaseDamage × 0.75

[ ] PM=PM_WeakSpot이면 Mat×=2.0 (head에 PM_WeakSpot 할당 시)
[ ] 리와인드 후 복구 정상 — 발사 후 캐릭터 외형 이상 없음
[ ] bReturnPhysicalMaterial: Hit.PhysMaterial.IsValid() == true (로그에서 PM 이름 확인)
```

### 트러블슈팅

| 증상 | 원인 | 해결 |
|------|------|------|
| `Hit.BoneName`이 항상 None | Physics Asset 바디 이름이 본 이름과 불일치 | PA 에디터에서 바디 이름 확인 |
| 일부 본만 배율이 안 먹음 | 스켈레톤 본 이름과 BoneDamageMultiplierMap 키 불일치 | 시작 시 본 이름 검증 로그 추가, 누락 키 채우기 |
| `Hit.PhysMaterial`이 nullptr | `bReturnPhysicalMaterial = false` 또는 PM 미할당 | Step 8 Params 확인, Step 11 PM 할당 |
| 히트 판정 전혀 안됨 | Physics Asset 바디가 WeaponTrace를 Block하지 않음 | Step 1-2 채널 설정 확인 |
| `FBodyInstance*`가 nullptr | 해당 본에 Physics Asset 바디 없음 | Step 1-1 바디 추가 확인 |
| 리와인드 후 캐릭터 자세 이상 | 복구 루프 누락 또는 순서 오류 | Step 8 "4. 일괄 복구" 위치 확인 |
| 데미지가 항상 BaseDamage 그대로 | `GetBoneMultiplier`가 호출 안됨 또는 BoneName이 빈 값 | 로그로 BoneName 확인 |
| 헤드샷 배율 2.0이 두 번 적용됨 | TakeDamage에 배율 계산이 남아 있음 | Step 12 TakeDamage 정리 확인 |
| HitboxHistory 항상 비어 있음 | SaveHitboxSnapshot이 호출 안됨 또는 서버가 아님 | Step 4-3 Tick + HasAuthority 확인 |
| 모든 스냅샷 본 Transform이 동일 (T-포즈 고정) | 서버에서 애니메이션 Tick 꺼짐 | Step 1-5 VisibilityBasedAnimTickOption 확인 |
| 리와인드가 전혀 작동 안 함 (클램프 항상 발동) | ClientFireTime이 클라이언트 로컬 시간으로 전송됨 | Step 10 Input_Fire에서 GS->GetServerWorldTimeSeconds() 확인 |
| RequestFire 컴파일 오류 | Input_Fire 시그니처 미업데이트 | Step 10 Input_Fire 확인 |
| 저프레임 서버에서 판정이 불안정 | 히스토리 간격/길이가 프레임 종속으로 설정됨 | SnapshotInterval + MaxHistoryCount를 설정값으로 고정 |

### 구현 완료 기준

- BaseDamage 50 기준:
  - 헤드: 100 데미지
  - 몸통: 50 데미지
  - 사지: 37.5 데미지
- `PM_WeakSpot` PM 할당 부위에서 추가 배율 적용됨
- 리와인드 → 복구 사이클 후 캐릭터 포즈 이상 없음

---

## NetPrediction 진입 전 준비 작업

> BoneHitbox 테스트까지 통과한 후에 진행한다.
> 코드 동작을 바꾸지 않고 **NetPrediction이 분기를 추가할 자리를 미리 만드는** 작업이다.

### 준비 이유

BoneHitbox 완료 후 `Server_Fire`는 Hitscan만 처리한다.
NetPrediction은 투사체(Projectile) 분기를 추가해야 한다.
지금 `EPTypes.h`와 `EPWeaponDefinition.h`, `Server_Fire`를 확장해두면
NetPrediction 구현서를 따라갈 때 기존 코드를 건드리지 않고 `switch` 케이스만 추가할 수 있다.

---

### 준비 Step A) EPTypes.h — EEPBallisticType enum 추가

파일: `Public/Types/EPTypes.h`

기존 enum들 아래에 추가:

```cpp
UENUM(BlueprintType)
enum class EEPBallisticType : uint8
{
    Hitscan,         // 즉시 판정 (소총, SMG, 권총 등)
    ProjectileFast,  // 클라이언트 예측 투사체 (고속 — 로켓, 유탄 등)
    ProjectileSlow   // 서버 권한 투사체 (느린 투사체 — 화살, 에너지 볼 등)
};
```

---

### 준비 Step B) EPWeaponDefinition.h — BallisticType + ProjectileClass 추가

파일: `Public/Data/EPWeaponDefinition.h`

forward declaration 추가 (헤더 상단):
```cpp
class AEPProjectile; // NetPrediction 단계에서 구현
```

`Damage` 필드 아래 `Weapon|Ballistic` 카테고리 추가:

```cpp
// --- 탄도 타입 (NetPrediction 분기 기준) ---
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistic")
EEPBallisticType BallisticType = EEPBallisticType::Hitscan;

// ProjectileFast / ProjectileSlow 무기에서만 사용
// AEPProjectile은 NetPrediction 단계에서 구현한다
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistic",
    meta = (EditCondition = "BallisticType != EEPBallisticType::Hitscan"))
TSubclassOf<AEPProjectile> ProjectileClass;
```

> `AEPProjectile`은 아직 없으므로 컴파일 오류가 발생할 수 있다.
> 오류가 발생하면 `TSubclassOf<AActor> ProjectileClass`로 임시 대체하거나,
> `AEPProjectile` 빈 클래스를 미리 생성한다 (헤더와 cpp만 — 로직 없음).

---

### 준비 Step C) Server_Fire_Implementation — switch 구조로 교체

파일: `Private/Combat/EPCombatComponent.cpp`

현재 `HandleHitscanFire`를 바로 호출하는 구조를 `switch`로 감싼다.
동작은 바뀌지 않는다 — Hitscan 케이스만 있으므로 결과 동일.

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector_NetQuantize&       Origin,
    const FVector_NetQuantizeNormal& Direction,
    float                            ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;
    if (!EquippedWeapon->WeaponDef) return;

    FVector SpreadDir = Direction;
    EquippedWeapon->Fire(SpreadDir);

    AEPCharacter* Owner = GetOwnerCharacter();

    // NetPrediction 단계에서 ProjectileFast / ProjectileSlow 케이스를 추가한다.
    switch (EquippedWeapon->WeaponDef->BallisticType)
    {
        case EEPBallisticType::Hitscan:
        default:
        {
            const TArray<FVector> Directions = { SpreadDir };
            HandleHitscanFire(Owner, Origin, Directions, ClientFireTime);
            break;
        }
        // case EEPBallisticType::ProjectileFast: HandleProjectileFastFire(...); break;
        // case EEPBallisticType::ProjectileSlow: HandleProjectileSlowFire(...); break;
    }

    const FVector MuzzleLocation =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();

    Multicast_PlayMuzzleEffect(MuzzleLocation);
}
```

---

### 준비 작업 완료 체크리스트

```
[ ] EEPBallisticType enum 추가 (EPTypes.h)
[ ] EPWeaponDefinition에 BallisticType / ProjectileClass 추가
[ ] Server_Fire_Implementation을 switch 구조로 교체
[ ] 빌드 통과 확인
[ ] PIE에서 기존 동작 유지 확인 (Hitscan default 케이스)
```

완료 후 상태:
- `HandleHitscanFire` — 완성, 변경 없음
- `FBodyInstance` 리와인드/복구 — 완성, 변경 없음
- `FEPHitboxSnapshot` — 완성, 변경 없음
- NetPrediction은 `switch`의 주석 해제 + 각 핸들러 함수 구현만 하면 된다
