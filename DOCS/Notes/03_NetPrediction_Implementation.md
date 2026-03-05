# 3단계 구현서: NetPrediction

> 기준 문서: `03_NetPrediction.md`
> 목표: **지금 바로 따라 구현 가능한 순서** 제공
> 범위: Hit Validation + Lag Compensation + Reconciliation + 투사체 지원
> 비범위: 헤드샷/약점 배율은 구현하지 않음 (Chapter 4 BoneHitbox 단계)

---

## 0. 구현 철학

1. 서버 권한 판정 — 클라이언트는 방향만 보내고 서버가 직접 LineTrace + 리와인드 (Valorant 방식)
2. 클라 체감 예측 — 반동·총구 이펙트는 즉시 로컬 재생, 서버 판정 결과로 히트마커 표시
3. 판정 시점 보정 — HitboxHistory 리와인드로 클라 발사 시점 기준 판정 보장
4. 탄도 분기 — `EEPBallisticType`으로 Hitscan / ProjectileFast / ProjectileSlow 분리
5. GAS 연결 예약 — Damage Block은 `ApplyPointDamage` 래퍼로 고립, Chapter 4에서 GE로 교체

이번 단계 핵심: **"얼마나 아픈가"가 아닌 "맞았는가 + 어떤 방식으로"**

---

## 1. 수정/추가 대상 파일

**수정:**
- `Public/Types/EPTypes.h`
- `Public/Data/EPWeaponDefinition.h`
- `Public/Core/EPCharacter.h`
- `Private/Core/EPCharacter.cpp`
- `Public/Combat/EPCombatComponent.h`
- `Private/Combat/EPCombatComponent.cpp`
- `Public/Combat/EPWeapon.h`
- `Private/Combat/EPWeapon.cpp`

**신규:**
- `Public/Combat/EPProjectile.h`
- `Private/Combat/EPProjectile.cpp`

**프로젝트 설정:**
- Project Settings → Collision → Trace Channels (`EP_TraceChannel_Weapon` 추가)

---

## 2. Step-by-Step

---

## Step 0) 프로젝트 설정 — 히트박스 트레이스 채널 설정

파일: `Config/DefaultEngine.ini` (에디터 UI로 자동 저장)

히트스캔 LineTrace는 캐릭터 히트박스 전용 채널을 사용한다.
환경(벽, 바닥)과 채널을 분리해야 **리와인드된 캐릭터가 벽 안에 있을 때 LineTrace가 벽에 먼저 막히는 문제**를 방지할 수 있다.

**WeaponTrace 채널 이미 존재하는 경우 (현재 프로젝트):**

기존 `WeaponTrace` 채널을 그대로 사용한다. 단, Default Response를 반드시 `Ignore`로 변경해야 한다.

1. `Edit → Project Settings → Engine → Collision`
2. `Trace Channels` 목록에서 `WeaponTrace` 찾기
3. Default Response: `Block` → **`Ignore`** 로 변경
4. 저장

> **왜 Ignore여야 하는가:**
> Default가 Block이면 벽·바닥 등 모든 메시가 이 채널에 반응한다.
> 리와인드 시 캐릭터가 과거 위치(현재 서버 기준 벽 안쪽)로 이동하면,
> LineTrace가 캐릭터에 도달하기 전에 벽에 막혀 히트 판정 실패가 발생한다.
> Default를 Ignore로 두면 캐릭터 Physics Asset에 명시적으로 Block을 설정한 것만 감지한다.

**캐릭터 Physics Asset 설정:**
1. BP_EPCharacter의 Physics Asset 열기
2. 모든 바디(Capsule, Pelvis 등) 선택
3. Collision Responses → `WeaponTrace` 채널: **`Block`** 설정
4. 환경 메시(StaticMesh, Landscape 등)는 Default Ignore를 그대로 유지

**채널 번호 확인 및 코드 상수 정의:**

`Config/DefaultEngine.ini`에서 WeaponTrace의 채널 번호를 확인한다:
```ini
; DefaultEngine.ini
+CustomTraceChannel=(Channel=ECC_GameTraceChannelN,Name="WeaponTrace",...)
```

확인한 번호로 `EPTypes.h`에 상수를 정의한다.
채널 번호를 코드 곳곳에 하드코딩하면 채널 추가/삭제 시 모두 수정해야 하므로 한 곳에 정의한다:

```cpp
// EPTypes.h — 채널 번호를 한 곳에서 관리
// DefaultEngine.ini의 WeaponTrace 채널 번호를 확인 후 아래 값을 맞춘다
inline constexpr ECollisionChannel EP_TraceChannel_Weapon = EP_TraceChannel_Weapon; // N을 확인된 번호로 교체
```

이후 코드에서 `EP_TraceChannel_Weapon` 대신 `EP_TraceChannel_Weapon`을 사용한다.

---

## Step 1) EPTypes.h — EEPBallisticType + FEPHitboxSnapshot

파일: `Public/Types/EPTypes.h`

기존 enum 블록 아래에 추가한다.

```cpp
// 탄도 방식 — EEPFireMode(Single/Burst/Auto 트리거)와 별개
UENUM(BlueprintType)
enum class EEPBallisticType : uint8
{
    Hitscan,        // 즉발 LineTrace (라이플, SMG, 히트스캔 스킬)
    ProjectileFast, // 고속 투사체 — 서버 시뮬, 클라 코스메틱만 (소총탄, 권총탄)
    ProjectileSlow, // 저속 투사체 — Actor 복제 (수류탄, 로켓)
};

// 히트박스 히스토리 스냅샷 (캡슐 기준, 본 단위 확장은 03_BoneHitbox 참고)
USTRUCT()
struct FEPHitboxSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    float ServerTime = 0.f;                    // 기록 시점 서버 시간

    UPROPERTY()
    FVector Location = FVector::ZeroVector;    // 캐릭터 위치

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator; // 캐릭터 회전
};
```

---

## Step 2) EPWeaponDefinition.h — 탄도 필드 추가

파일: `Public/Data/EPWeaponDefinition.h`

헤더 상단에 추가:
```cpp
#include "Types/EPTypes.h" // EEPBallisticType 사용

// forward declare
class AEPProjectile;
```

기존 멤버 아래 `"Weapon|Ballistics"` 카테고리 추가:

```cpp
UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ballistics")
EEPBallisticType BallisticType = EEPBallisticType::Hitscan;

// Hitscan이 아닌 모든 탄도 방식에서 필요 (ProjectileFast/Slow 공용)
UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ballistics",
    meta = (EditCondition = "BallisticType != EEPBallisticType::Hitscan"))
TSubclassOf<AEPProjectile> ProjectileClass;

// 산탄총용 — 1이면 단일 펠릿 (라이플/SMG), 2 이상이면 다중 펠릿 (샷건)
UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ballistics",
    meta = (EditCondition = "BallisticType == EEPBallisticType::Hitscan", ClampMin = 1))
int32 PelletCount = 1;

// 스프레드 반각(도) — 결정론적 RNG로 이 범위 내에서 펠릿 방향 생성
UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ballistics",
    meta = (EditCondition = "BallisticType == EEPBallisticType::Hitscan", ClampMin = 0.f))
float BaseSpread = 0.f;
```

> 탄속(InitialSpeed)은 각 ProjectileClass Blueprint의
> `UProjectileMovementComponent::InitialSpeed`에서 설정한다.
> 무기마다 중력·탄속이 달라 WeaponDefinition에 두기 적절하지 않다.

---

## Step 3) EPCharacter.h — 히스토리 버퍼 선언

파일: `Public/Core/EPCharacter.h`

### 3-1. include

```cpp
#include "Types/EPTypes.h"
```

### 3-2. 멤버/함수 추가

```cpp
public:
    // 서버가 ClientFireTime 시점의 캡슐 위치를 반환 (보간 포함)
    FEPHitboxSnapshot GetSnapshotAtTime(float TargetTime) const;

    // GetHitscanCandidates에서 사망 필터링에 사용
    // 구현: return Health <= 0.f; (Health는 기존 멤버 또는 AttributeSet)
    bool IsDead() const;

protected:
    virtual void Tick(float DeltaTime) override;

private:
    // 60Hz: 16.7ms × 64 ≈ 1067ms / 30Hz: 33ms × 64 ≈ 2133ms
    // MaxLagCompWindow(200ms) 대비 충분한 마진
    static constexpr int32 MaxHitboxHistory = 64;

    UPROPERTY()
    TArray<FEPHitboxSnapshot> HitboxHistory;

    int32 HistoryIndex = 0;
    // HistoryTimer 없음 — 고정 간격 대신 매 틱 기록
    // 서버 틱레이트가 샘플링 간격을 자동으로 결정한다

    void SaveHitboxSnapshot();
```

> `UPROPERTY()`는 TArray 멤버의 GC 안정성을 보장한다.
> 히스토리는 서버 전용(`HasAuthority()`)으로만 기록하고 복제하지 않는다.

---

## Step 4) EPCharacter.cpp — 기록 + 보간 구현

파일: `Private/Core/EPCharacter.cpp`

### 4-1. include 추가

```cpp
#include "GameFramework/GameStateBase.h"
```

### 4-2. 생성자에서 Tick 보장

```cpp
PrimaryActorTick.bCanEverTick = true;
```

### 4-3. Tick — 매 틱 기록

```cpp
void AEPCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!HasAuthority()) return; // 서버만 기록

    // 매 틱마다 기록 — 서버 틱레이트가 샘플링 간격을 결정한다.
    // 고정 100ms 간격은 ClientFireTime이 두 스냅샷 중간에 오면
    // 최대 50ms치 이동 오차(~150cm@30km/h)를 유발한다.
    SaveHitboxSnapshot();
}
```

### 4-4. SaveHitboxSnapshot — 링버퍼 기록

```cpp
void AEPCharacter::SaveHitboxSnapshot()
{
    FEPHitboxSnapshot Snapshot;

    // GetServerWorldTimeSeconds(): GameState가 복제하는 서버 기준 시간.
    // RequestFire의 ClientFireTime과 동일 기준이어야 리와인드 시각이 정확히 맞는다.
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    Snapshot.ServerTime = GS
        ? GS->GetServerWorldTimeSeconds()
        : GetWorld()->GetTimeSeconds();
    Snapshot.Location   = GetActorLocation();
    Snapshot.Rotation   = GetActorRotation();

    if (HitboxHistory.Num() < MaxHitboxHistory)
    {
        HitboxHistory.Add(Snapshot);
    }
    else
    {
        HitboxHistory[HistoryIndex] = Snapshot;
        HistoryIndex = (HistoryIndex + 1) % MaxHitboxHistory;
    }
}
```

### 4-5. GetSnapshotAtTime — 선형 탐색 + 보간

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

    const float Range = After->ServerTime - Before->ServerTime;
    // KINDA_SMALL_NUMBER guard: Range == 0 (같은 시각 스냅샷) 이면 나눗셈 생략
    const float Alpha = Range > KINDA_SMALL_NUMBER
        ? FMath::Clamp((TargetTime - Before->ServerTime) / Range, 0.f, 1.f)
        : 0.f;

    FEPHitboxSnapshot Result;
    Result.ServerTime = TargetTime;
    Result.Location   = FMath::Lerp(Before->Location, After->Location, Alpha);
    // FMath::Lerp는 각도 랩어라운드(±179° ↔ 181°) 문제 발생
    // FQuat::Slerp로 올바른 구면 보간
    Result.Rotation = FQuat::Slerp(
        Before->Rotation.Quaternion(),
        After->Rotation.Quaternion(),
        Alpha).Rotator();

    return Result;
}
```

> **O(N) 탐색:** 링버퍼(64개) 선형 탐색. 향후 본 단위 히스토리 확장 시
> 버퍼를 시간 순 정렬 상태로 유지하면 이진 탐색 O(log N)으로 개선 가능.

---

## Step 5) EPCombatComponent.h — 시그니처 + 핸들러 선언

파일: `Public/Combat/EPCombatComponent.h`

### 5-1. include / forward declare

```cpp
#include "Types/EPTypes.h"  // EEPBallisticType

class AEPProjectile;
```

### 5-2. 멤버 추가 (private)

```cpp
private:
    // 클라이언트 측 연사속도 체크 — 불필요한 Server RPC 사전 차단
    float LocalLastFireTime  = -999.f;

    // 서버 전용 — 클라이언트가 조작 불가. FireRate 초과 RPC 차단에 사용
    float LastServerFireTime = -999.f;
```

### 5-3. RPC 시그니처 변경

기존:
```cpp
UFUNCTION(Server, Reliable)
void Server_Fire(const FVector& Origin, const FVector& Direction);
```

변경:
```cpp
UFUNCTION(Server, Reliable)
void Server_Fire(
    const FVector_NetQuantize&     Origin,
    const FVector_NetQuantizeNormal& Direction,
    float                          ClientFireTime);
```

### 5-4. Multicast 추가

```cpp
// ProjectileFast — 발사자 외 다른 클라이언트에게 코스메틱 투사체 스폰 요청
UFUNCTION(NetMulticast, Unreliable)
void Multicast_SpawnCosmeticProjectile(
    const FVector_NetQuantize&      MuzzleLoc,
    const FVector_NetQuantizeNormal& Direction);
```

### 5-5. 핸들러 선언 (private)

```cpp
private:
    // 히트스캔: Multi-Rewind → N방향 LineTrace → 복구 → 데미지
    // GAS 어빌리티 히트스캔 스킬도 이 함수를 호출한다
    void HandleHitscanFire(
        AEPCharacter*         Owner,
        const FVector&        Origin,
        const TArray<FVector>& Directions, // 단일 펠릿: {SpreadDir}, 산탄총: PelletCount개
        float                 ClientFireTime);

    // 투사체: ProjectileFast/Slow 분기 처리
    void HandleProjectileFire(
        AEPCharacter* Owner,
        const FVector& Origin,
        const FVector& Direction);

    // Broad Phase 후보 선정 — 향후 Spatial Hash로 교체 시 이 함수만 바꾼다
    TArray<AEPCharacter*> GetHitscanCandidates(
        AEPCharacter*         Owner,
        const FVector&        Origin,
        const TArray<FVector>& Directions,
        float                 ClientFireTime) const;
```

> `HandleHitscanFire`는 현재 `private`. Chapter 4에서 GAS 어빌리티가 히트스캔을
> 직접 호출할 필요가 생기면 `protected`로 변경하거나 public 래퍼를 추가한다.

---

## Step 6) RequestFire — ClientFireTime + 코스메틱 즉시 스폰

파일: `Private/Combat/EPCombatComponent.cpp`

### 6-1. include 추가

```cpp
#include "GameFramework/GameStateBase.h"
#include "Combat/EPProjectile.h"
```

### 6-2. RequestFire 수정

```cpp
void UEPCombatComponent::RequestFire(const FVector& Origin, const FVector& Direction)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;
    if (EquippedWeapon->CurrentAmmo <= 0) return;

    AEPCharacter* Owner = GetOwnerCharacter();

    // 클라이언트 측 연사속도 체크 (서버도 독립 검증 — 이중 방어)
    const float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
    const float CurrentTime  = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LocalLastFireTime < FireInterval) return;
    LocalLastFireTime = CurrentTime;

    // 서버 기준 시간 — HitboxHistory 기록과 동일 기준
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ClientFireTime = GS
        ? GS->GetServerWorldTimeSeconds()
        : GetWorld()->GetTimeSeconds();

    Server_Fire(Origin, Direction.GetSafeNormal(), ClientFireTime);

    if (!Owner || !Owner->IsLocallyControlled()) return;

    // 반동 예측 — 서버 확인 전 즉시 적용
    Owner->AddControllerPitchInput(-EquippedWeapon->GetRecoilPitch());
    Owner->AddControllerYawInput(FMath::RandRange(
        -EquippedWeapon->GetRecoilYaw(),
         EquippedWeapon->GetRecoilYaw()));

    // ProjectileFast: Multicast 왕복 없이 발사자 측 코스메틱 즉시 스폰.
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

    // 산탄총(PelletCount > 1): 서버와 동일 시드로 N개 펠릿 방향 생성 → 로컬 이펙트
    // const int32 Seed = FMath::FloorToInt(ClientFireTime * 1000.f);
    // FRandomStream RandStream(Seed);
    // EquippedWeapon->GenerateClientPellets(Direction, RandStream, PelletDirs);
    // → 각 방향으로 트레이서/이펙트 재생 (판정 없음, 순수 코스메틱)
}
```

---

## Step 7) Server_Fire_Implementation — 3단계 검증 + 탄도 분기

파일: `Private/Combat/EPCombatComponent.cpp`

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector_NetQuantize&      Origin,
    const FVector_NetQuantizeNormal& Direction,
    float                           ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;

    AEPCharacter* Owner = GetOwnerCharacter();
    if (!Owner) return;

    // ── 서버 사이드 검증 3단계 ────────────────────────────────────────────────

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

    // 3. Origin 위치 검증: 벽 너머 좌표로 Origin을 조작하는 것을 방지
    //    200cm: 이동 예측 오차 + 무기 Offset(팔 길이 등) 포함 여유치
    constexpr float MaxOriginDrift = 200.f;
    if (FVector::DistSquared(Origin, Owner->GetActorLocation()) > FMath::Square(MaxOriginDrift))
        return;

    // ── 탄도 분기 ─────────────────────────────────────────────────────────────

    if (EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::Hitscan)
    {
        // 서버가 결정론적 RNG로 펠릿 방향 배열 생성 (산탄총/단일 공용)
        // 클라이언트와 동일 시드(ClientFireTime * 1000) → 동일 방향 보장
        TArray<FVector> PelletDirs;
        EquippedWeapon->Fire(Direction, ClientFireTime, PelletDirs);
        // PelletCount == 1(라이플 등): PelletDirs.Num() == 1
        // PelletCount > 1(샷건):      PelletDirs.Num() == PelletCount

        HandleHitscanFire(Owner, Origin, PelletDirs, ClientFireTime);
    }
    else // ProjectileFast / ProjectileSlow 모두 HandleProjectileFire에서 분기
    {
        FVector SpreadDir = Direction;
        EquippedWeapon->Fire(SpreadDir); // 탄약 차감 + 단일 스프레드 (투사체는 단일 방향)
        HandleProjectileFire(Owner, Origin, SpreadDir);
    }
}
```

---

## Step 8) GetHitscanCandidates — Broad Phase

파일: `Private/Combat/EPCombatComponent.cpp`

### 8-1. include 추가

```cpp
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h" // TActorIterator
```

### 8-2. 파일 스코프 구조체 (EPCombatComponent.cpp 상단)

```cpp
// AEPCharacter*에 의존하므로 EPTypes.h에 넣지 않는다.
// 순환 의존을 막기 위해 .cpp 파일 스코프에 정의한다.
// 복제 불필요 — 리와인드 중 임시 사용 후 즉시 복구된다.
struct FEPRewindEntry
{
    AEPCharacter* Character     = nullptr;
    FVector       SavedLocation = FVector::ZeroVector;
    FRotator      SavedRotation = FRotator::ZeroRotator;
};
```

### 8-3. GetHitscanCandidates 구현

```cpp
TArray<AEPCharacter*> UEPCombatComponent::GetHitscanCandidates(
    AEPCharacter*         Owner,
    const FVector&        Origin,
    const TArray<FVector>& Directions,
    float                 ClientFireTime) const
{
    TArray<AEPCharacter*> Candidates;

    for (TActorIterator<AEPCharacter> It(GetWorld()); It; ++It)
    {
        AEPCharacter* Char = *It;
        if (!Char || Char == Owner) continue;
        if (Char->IsDead()) continue; // 이미 사망 — 리와인드 대상 제외

        // 과거 위치 기준 Broad Phase — 현재 위치 기준이면 이동한 대상을 놓친다
        const FEPHitboxSnapshot Snap = Char->GetSnapshotAtTime(ClientFireTime);
        const float CapsuleRadius    = Char->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float BroadPhaseRadius = CapsuleRadius + 50.f; // 50cm 여유

        // 산탄총: N개 방향 중 하나라도 근방을 지나면 후보 포함
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

---

## Step 9) HandleHitscanFire — Multi-Rewind + N-Trace + 복구

파일: `Private/Combat/EPCombatComponent.cpp`

흐름: **클램프 → Broad Phase → Multi-Rewind → N-Trace → 일괄 복구 → 데미지 → 이펙트**

```cpp
void UEPCombatComponent::HandleHitscanFire(
    AEPCharacter*         Owner,
    const FVector&        Origin,
    const TArray<FVector>& Directions,
    float                 ClientFireTime)
{
    // ── [Rewind Block] ────────────────────────────────────────────────────────
    // 무기/스킬 공통 인프라 — Chapter 4 GAS 어빌리티도 이 블록을 그대로 재사용한다.

    // 0. 리와인드 윈도우 클램프 (200ms 초과 = 조작으로 간주)
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ServerNow = GS
        ? GS->GetServerWorldTimeSeconds()
        : GetWorld()->GetTimeSeconds();
    if (ServerNow - ClientFireTime > 0.2f)
        ClientFireTime = ServerNow; // 현재 위치 기준으로 판정 (리와인드 없음)

    // 1. Broad Phase — 총알 경로 근방 후보 선정
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
            Snap.Location, Snap.Rotation,
            false, nullptr, ETeleportType::TeleportPhysics);
    }

    // 3. Narrow Phase — 모든 후보가 과거 위치에 있는 상태에서 N방향 LineTrace
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HitscanFire), false);
    Params.AddIgnoredActor(Owner);
    Params.AddIgnoredActor(EquippedWeapon);

    TArray<FHitResult> ConfirmedHits;
    for (const FVector& Dir : Directions)
    {
        const FVector End = Origin + Dir * 10000.f;
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(
                Hit, Origin, End, EP_TraceChannel_Weapon, Params)
            && Cast<AEPCharacter>(Hit.GetActor()) != nullptr)
        {
            ConfirmedHits.Add(Hit);
        }
    }

    // 4. 후보 전체 일괄 복구 (반드시 Narrow Phase 직후 — 순서 변경 금지)
    for (const FEPRewindEntry& Entry : RewindEntries)
    {
        Entry.Character->SetActorLocationAndRotation(
            Entry.SavedLocation, Entry.SavedRotation,
            false, nullptr, ETeleportType::TeleportPhysics);
    }

    // ── [Damage Block] ────────────────────────────────────────────────────────
    // Chapter 4 GAS 전환 시 GameplayEffectSpec + SetByCaller로 교체한다.
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

    // ── [Effect Block] ────────────────────────────────────────────────────────
    // EquippedWeapon 재검사 — Damage Block 이후 무기 해제 엣지케이스 방어
    if (!EquippedWeapon || !EquippedWeapon->WeaponMesh) return;

    const FVector MuzzleLoc =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();

    // 발사자 클라이언트는 RequestFire에서 이미 이펙트 재생 → Multicast는 다른 클라만 의미있음
    Multicast_PlayMuzzleEffect(MuzzleLoc);
    for (const FHitResult& Hit : ConfirmedHits)
        Multicast_PlayImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal);
}
```

---

## Step 10) HandleProjectileFire — Fast/Slow 분기

파일: `Private/Combat/EPCombatComponent.cpp`

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
    //   ProjectileFast Blueprint → bReplicates = false (서버 시뮬만, 복제 비용 0)
    //   ProjectileSlow Blueprint → bReplicates = true  (수류탄/로켓 자동 복제)
    AEPProjectile* Proj = GetWorld()->SpawnActor<AEPProjectile>(
        EquippedWeapon->WeaponDef->ProjectileClass,
        MuzzleLoc,
        Direction.GetSafeNormal().Rotation(),
        SpawnParams);

    if (!Proj) return;

    Proj->Initialize(EquippedWeapon->GetDamage(), Direction);

    // ProjectileFast: 발사자는 RequestFire에서 이미 로컬 스폰 완료.
    //                 Multicast는 SimulatedProxy(다른 클라)만을 위한 것.
    // ProjectileSlow: 복제 Actor가 클라 렌더링 담당 → Multicast 불필요.
    if (EquippedWeapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
        Multicast_SpawnCosmeticProjectile(MuzzleLoc, Direction.GetSafeNormal());

    Multicast_PlayMuzzleEffect(MuzzleLoc);
}
```

---

## Step 11) Multicast_SpawnCosmeticProjectile

파일: `Private/Combat/EPCombatComponent.cpp`

```cpp
void UEPCombatComponent::Multicast_SpawnCosmeticProjectile_Implementation(
    const FVector_NetQuantize&      MuzzleLoc,
    const FVector_NetQuantizeNormal& Direction)
{
    // 서버는 실제 Actor 보유 — 여기서 다시 스폰하면 중복
    if (HasAuthority()) return;

    // 발사자(AutonomousProxy)는 RequestFire에서 이미 스폰 완료 — 중복 방지
    ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
    if (OwnerChar && OwnerChar->IsLocallyControlled()) return;

    // EquippedWeapon이 이 Multicast보다 늦게 복제될 수 있다 — 반드시 null 체크
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef
        || !EquippedWeapon->WeaponDef->ProjectileClass) return;

    AEPProjectile* Cosmetic = GetWorld()->SpawnActor<AEPProjectile>(
        EquippedWeapon->WeaponDef->ProjectileClass,
        MuzzleLoc, Direction.Rotation());

    if (Cosmetic)
        Cosmetic->SetCosmeticOnly(); // 충돌/데미지 비활성화, 궤적 렌더링만
}
```

---

## Step 12) AEPProjectile — 신규 파일

### 12-1. EPProjectile.h

파일: `Public/Combat/EPProjectile.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EPProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class EMPLOYMENTPROJ_API AEPProjectile : public AActor
{
    GENERATED_BODY()

public:
    AEPProjectile();

    // HandleProjectileFire / RequestFire에서 호출 — 데미지와 발사 방향 주입
    void Initialize(float InDamage, const FVector& InDirection);

    // 충돌/데미지 비활성화 → 궤적 렌더링 전용 인스턴스로 변환
    // ProjectileFast 코스메틱 스폰 시 호출
    void SetCosmeticOnly();

protected:
    UPROPERTY(VisibleAnywhere, Category = "Projectile")
    TObjectPtr<USphereComponent> CollisionComp;

    UPROPERTY(VisibleAnywhere, Category = "Projectile")
    TObjectPtr<UProjectileMovementComponent> MovementComp;

private:
    float   BaseDamage      = 0.f;
    FVector LaunchDir       = FVector::ForwardVector;
    bool    bIsCosmeticOnly = false; // true이면 OnProjectileHit 조기 반환

    UFUNCTION()
    void OnProjectileHit(
        UPrimitiveComponent* HitComp,
        AActor*              OtherActor,
        UPrimitiveComponent* OtherComp,
        FVector              NormalImpulse,
        const FHitResult&    Hit);
};
```

### 12-2. EPProjectile.cpp

파일: `Private/Combat/EPProjectile.cpp`

```cpp
#include "Combat/EPProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AEPProjectile::AEPProjectile()
{
    // bReplicates는 각 Blueprint에서 설정한다
    // ProjectileFast BP → false / ProjectileSlow BP → true

    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->InitSphereRadius(5.f);
    CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
    // OnComponentHit 발동 조건: bSimulationGeneratesHitEvents = true 필수.
    // "Projectile" 프로파일에 포함되지 않으면 아래 줄을 명시적으로 추가한다.
    CollisionComp->SetNotifyRigidBodyCollision(true);
    CollisionComp->OnComponentHit.AddDynamic(this, &AEPProjectile::OnProjectileHit);
    SetRootComponent(CollisionComp);

    MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
    MovementComp->bRotationFollowsVelocity = true;
    // InitialSpeed / MaxSpeed / ProjectileGravityScale 은 각 Blueprint에서 설정한다
}

void AEPProjectile::Initialize(float InDamage, const FVector& InDirection)
{
    BaseDamage = InDamage;
    LaunchDir  = InDirection.GetSafeNormal();
}

void AEPProjectile::SetCosmeticOnly()
{
    bIsCosmeticOnly = true;
    // 충돌 비활성화 — 다른 액터를 관통하며 궤적만 그린다
    CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // 중력 스케일은 유지 — 클라/서버 궤적이 동일해야 설득력이 있다
}

void AEPProjectile::OnProjectileHit(
    UPrimitiveComponent*,
    AActor* OtherActor,
    UPrimitiveComponent*,
    FVector,
    const FHitResult& Hit)
{
    // 코스메틱 인스턴스는 충돌이 비활성화돼 있어 여기 진입하지 않지만, 방어적으로 체크
    if (bIsCosmeticOnly) return;

    // 서버만 데미지 처리 — 클라이언트 복제 Actor는 판정하지 않는다
    if (!HasAuthority()) return;

    // 투사체는 랙 보상 불필요 — 이동 시간이 지연을 흡수한다
    // [Chapter 4 GAS 전환 포인트] ApplyPointDamage → GameplayEffectSpec
    if (OtherActor)
    {
        UGameplayStatics::ApplyPointDamage(
            OtherActor,
            BaseDamage,
            LaunchDir,
            Hit,
            GetInstigatorController(),
            GetInstigator(),
            UDamageType::StaticClass());
    }

    Destroy();
}
```

---

## Step 13) AEPWeapon::Fire 오버로드 — 결정론적 RNG 펠릿 생성

파일: `Public/Combat/EPWeapon.h` + `Private/Combat/EPWeapon.cpp`

### 13-1. 헤더 선언 추가

```cpp
// 히트스캔 전용 오버로드 — 탄약 차감 + 결정론적 RNG로 N개 펠릿 방향 생성
// 단일 펠릿(라이플/SMG): PelletCount == 1 → OutPellets.Num() == 1
// 산탄총: PelletCount > 1 → OutPellets.Num() == PelletCount
void Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets);
```

### 13-2. 구현

```cpp
void AEPWeapon::Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets)
{
    if (!WeaponDef) return;

    // 탄약 차감
    CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);

    // 결정론적 시드: 클라이언트와 동일 연산 → 동일 펠릿 방향 보장
    // FloorToInt(* 1000): 밀리초 단위 int32 — 128Hz 틱에서도 충돌 없음
    const int32 Seed = FMath::FloorToInt(ClientFireTime * 1000.f);
    FRandomStream RandStream(Seed);

    const int32 Count = FMath::Max(1, WeaponDef->PelletCount);
    OutPellets.Reserve(Count);

    for (int32 i = 0; i < Count; ++i)
    {
        // BaseSpread 반각(도) 범위 내 랜덤 방향 — 원뿔 균등 분포
        const float Angle  = FMath::DegreesToRadians(WeaponDef->BaseSpread);
        const FVector Dir  = FMath::VRandCone(AimDir, Angle, RandStream);
        OutPellets.Add(Dir);
    }
}
```

---

## 3. 테스트 절차 (반드시 멀티로)

### PIE 설정
- Dedicated Server ON
- Client 2~3명

### 히트스캔 테스트 케이스

| 케이스 | 기대 결과 |
|--------|-----------|
| 낮은 핑 | 조준 시점 기준 히트 판정 |
| 높은 핑 (네트워크 에뮬) | 클라 발사 시점 기준 히트 보장 |
| 200ms 초과 핑 | 현재 위치 기준 판정 (리와인드 없음) |
| FireRate 초과 RPC | 서버에서 조용히 거부 |
| Origin 200cm 초과 | 서버에서 거부 |
| 사망한 적 조준 발사 | 데미지 없음 |

### 투사체 테스트 케이스

| 케이스 | 기대 결과 |
|--------|-----------|
| ProjectileFast 발사자 | RequestFire에서 즉시 코스메틱 궤적 |
| ProjectileFast 다른 클라 | Multicast로 코스메틱 수신 → 궤적 렌더링 |
| ProjectileSlow 발사 | 서버 Actor 복제 → 클라 자동 동기화 |
| 코스메틱 투사체 충돌 | 데미지 없음 (SetCosmeticOnly 확인) |

### 확인 로그 (HandleHitscanFire 상단에 추가)

```cpp
UE_LOG(LogTemp, Log,
    TEXT("[Hitscan] ServerNow=%.3f ClientFireTime=%.3f Delta=%.1fms Candidates=%d"),
    ServerNow, ClientFireTime, (ServerNow - ClientFireTime) * 1000.f, Candidates.Num());

// Narrow Phase 이후
UE_LOG(LogTemp, Log,
    TEXT("[Hitscan] ConfirmedHits=%d"), ConfirmedHits.Num());
```

---

## 4. 트러블슈팅

**1. 캐릭터가 안 맞는 경우**
- Physics Asset에서 각 히트 바디가 `EP_TraceChannel_Weapon = Block`인지 확인
- `GetHitscanCandidates`에서 후보가 0명인지 확인 (Broad Phase 반경 문제)
- `IsDead()` 구현 확인 — Step 3에서 선언, 내부에서 `Health <= 0.f` 반환

**2. 리와인드 후 위치가 틀어지는 경우**
- 복구 코드 누락 확인 (`SetActorLocationAndRotation` 두 번 호출 모두 있는지)
- `ETeleportType::TeleportPhysics` 사용 여부 확인

**3. ProjectileFast가 발사자에게 두 번 보이는 경우**
- `Multicast_SpawnCosmeticProjectile`의 `IsLocallyControlled()` 체크 확인
- `RequestFire`의 ProjectileFast 스폰 조건 확인

**4. 산탄총 클라/서버 방향 불일치**
- 시드 변환이 양쪽에서 동일한지 확인: `FMath::FloorToInt(ClientFireTime * 1000.f)`
- `FMath::VRandCone` 인자 순서 확인 (AimDir, Angle, RandStream)

**5. KINDA_SMALL_NUMBER 오류**
- `GetSnapshotAtTime`의 Range guard 확인

**6. 코스메틱 투사체가 데미지를 주는 경우**
- `OnProjectileHit`의 `bIsCosmeticOnly` guard 확인
- `SetCosmeticOnly()` 호출 타이밍 확인 (SpawnActor 직후)

---

## 5. 다음 단계 연결

이 단계 완료 후 Chapter 4(GAS)에서 교체할 부분:

| 현재 | Chapter 4 이후 |
|------|---------------|
| `ApplyPointDamage` (HandleHitscanFire Damage Block) | `GameplayEffectSpec` + `SetByCaller` |
| `ApplyPointDamage` (AEPProjectile::OnProjectileHit) | 동일 |
| `UDamageType::StaticClass()` | 커스텀 `UEPDamageType` |
| `LastServerFireTime` 수동 관리 | GAS Cooldown GameplayEffect |

Rewind Block(`GetHitscanCandidates` + Multi-Rewind + 복구)과 ProjectileFast/Slow 분기는
**Chapter 4에서도 그대로 유지된다.** 데미지 계산기만 교체하면 된다.
