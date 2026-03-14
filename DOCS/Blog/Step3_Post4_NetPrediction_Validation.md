# Post 3-4 작성 가이드 — 서버 검증 강화 + 탄도 방식 분리

> **예상 제목**: `[UE5] 추출 슈터 3-4. 서버 검증과 탄도 분리: FireRate 조작 방지와 EEPBallisticType`
> **참고 문서**: `DOCS/Notes/03_NetPrediction.md` Section 4, `DOCS/Notes/03_NetPrediction_Implementation.md` Step 1~7, 13
>
> ⚠️ **작성 전 구현 필요 항목:**
> - `EEPBallisticType` enum (EPTypes.h)
> - `BallisticType` / `ProjectileClass` / `PelletCount` (EPWeaponDefinition.h)
> - `LastServerFireTime` + Origin 검증 (EPCombatComponent)
> - `Server_Fire_Implementation` switch 구조
> - `AEPWeapon::Fire(AimDir, ClientFireTime, OutPellets)` 오버로드

---

## 개요

**이 포스팅에서 다루는 것:**
- 2단계 `Server_Fire`에 없던 서버 독립 검증 3단계 추가
- `EEPBallisticType`으로 발사 방식을 데이터로 분리
- 결정론적 RNG 산탄총 펠릿 생성

**왜 이렇게 구현했는가 (설계 의도):**
- `LocalLastFireTime`은 클라이언트 측 — 조작된 클라이언트는 이를 무시하고 RPC 스팸 가능
- `LastServerFireTime`은 서버 메모리에만 존재 → 클라이언트 접근 불가
- `EEPBallisticType`을 WeaponDefinition 데이터로 두면 무기 기획자가 코드 없이 탄도 방식 변경 가능

---

## 구현 전 상태 (Before)

```cpp
// BoneHitbox 완료 직후 Server_Fire — 검증이 부족
void UEPCombatComponent::Server_Fire_Implementation(...)
{
    // CanFire()만 체크 — FireRate 초과 RPC 스팸 방어 없음
    if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;

    // 탄도 분기 없음 — 모두 히트스캔으로만 처리
    FVector SpreadDir = Direction;
    EquippedWeapon->Fire(SpreadDir);
    HandleHitscanFire(Owner, Origin, { SpreadDir }, ClientFireTime);
    Multicast_PlayMuzzleEffect(MuzzleLocation);
}
```

**문제점:**
- `EquippedWeapon->CanFire()`의 `LastFireTime`은 서버 내부지만, 악의적 클라이언트가 `Server_Fire` RPC를 FireRate보다 빠르게 전송하면 `CanFire()` 타이밍에 따라 통과 가능
- 투사체 무기를 추가하면 분기 코드가 Server_Fire 안에 직접 쌓임

---

## 구현 내용

### 1. 서버 검증 3단계

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
    const FVector_NetQuantize& Origin,
    const FVector_NetQuantizeNormal& Direction,
    float ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;
    AEPCharacter* Owner = GetOwnerCharacter();
    if (!Owner) return;

    // ── 검증 1: FireRate (서버 독립 시계) ────────────────────────────────
    // LastServerFireTime은 서버 메모리에만 존재 → 클라이언트 조작 불가
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ServerNow    = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    const float FireInterval = 1.f / EquippedWeapon->WeaponDef->FireRate;
    if (ServerNow - LastServerFireTime < FireInterval) return; // 스팸 즉시 거부
    LastServerFireTime = ServerNow;

    // ── 검증 2: 탄약/무기 상태 ───────────────────────────────────────────
    if (!EquippedWeapon->CanFire()) return;

    // ── 검증 3: Origin 위치 (벽 너머 조작 방지) ─────────────────────────
    // 클라가 보낸 Origin이 서버 캐릭터 위치와 200cm 이상 차이나면 거부
    // 200cm: 이동 예측 오차 + 무기 오프셋(팔 길이) 포함 여유치
    constexpr float MaxOriginDrift = 200.f;
    if (FVector::DistSquared(Origin, Owner->GetActorLocation()) > FMath::Square(MaxOriginDrift))
        return;

    // ── 탄도 분기 ────────────────────────────────────────────────────────
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
            EquippedWeapon->Fire(SpreadDir); // 단일 방향, 탄약 차감
            HandleProjectileFire(Owner, Origin, SpreadDir);
            break;
        }
    }

    const FVector MuzzleLocation =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();
    Multicast_PlayMuzzleEffect(MuzzleLocation);
}
```

**검증 3단계 요약:**

| 단계 | 검증 항목 | 이유 |
|------|-----------|------|
| 1 | FireRate (LastServerFireTime) | 클라이언트 조작 불가한 서버 독립 시계 |
| 2 | CanFire() — 탄약/WeaponState | 실제 발사 가능 상태 확인 |
| 3 | Origin drift (200cm) | 벽 너머 조준 조작 방지 |

### 2. EEPBallisticType 설계

```cpp
// Public/Types/EPTypes.h
// EEPFireMode(Single/Burst/Auto)는 트리거 방식 → 별개 enum
UENUM(BlueprintType)
enum class EEPBallisticType : uint8
{
    Hitscan,        // 즉발 LineTrace (라이플, SMG)
    ProjectileFast, // 고속 투사체 — 서버 시뮬, 클라 코스메틱 (소총탄 수준)
    ProjectileSlow, // 저속 투사체 — Actor 복제 (수류탄, 로켓)
};
```

**고속/저속을 분리하는 이유:**

| | ProjectileFast | ProjectileSlow |
|---|---|---|
| 속도 | ~870m/s (소총탄) | ~30m/s (수류탄) |
| 복제 틱(30Hz) 간격 | 29m 이동 → 뚝뚝 끊김 | 1m 이동 → 부드러움 |
| 서버 Actor | bReplicates = false | bReplicates = true |
| 클라 렌더링 | Multicast로 코스메틱 스폰 | 복제 Actor 자동 추적 |

### 3. WeaponDefinition 확장

```cpp
// Public/Data/EPWeaponDefinition.h 추가
#include "Types/EPTypes.h"
class AEPProjectile; // forward declare

// --- 탄도 ---
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistics")
EEPBallisticType BallisticType = EEPBallisticType::Hitscan;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistics",
    meta = (EditCondition = "BallisticType != EEPBallisticType::Hitscan"))
TSubclassOf<AEPProjectile> ProjectileClass;

// 산탄총용 — 1이면 단일(라이플/SMG), 2 이상이면 다중(샷건)
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ballistics",
    meta = (EditCondition = "BallisticType == EEPBallisticType::Hitscan", ClampMin = 1))
int32 PelletCount = 1;
```

> **에디터 설정**: DA_AK74에서 BallisticType = Hitscan, PelletCount = 1 설정

### 4. AEPWeapon::Fire 오버로드 — 결정론적 RNG

```cpp
// 기존: Fire(FVector& OutDirection) — 단일 방향 반환
// 추가: Fire(AimDir, ClientFireTime, OutPellets) — N개 펠릿 방향 반환

void AEPWeapon::Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets)
{
    if (!WeaponDef) return;
    CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);

    // 결정론적 시드: ClientFireTime × 1000 → 밀리초 단위 int32
    // 클라이언트와 서버가 동일 연산 → 동일 펠릿 방향 보장
    const int32 Seed = FMath::FloorToInt(ClientFireTime * 1000.f);
    FRandomStream RandStream(Seed);

    const int32 Count = FMath::Max(1, WeaponDef->PelletCount);
    OutPellets.Reserve(Count);

    for (int32 i = 0; i < Count; ++i)
    {
        const float Angle = FMath::DegreesToRadians(WeaponDef->BaseSpread);
        OutPellets.Add(FMath::VRandCone(AimDir, Angle, RandStream));
    }
}
```

**클라이언트 방향을 그대로 받지 않는 이유:**

```
[취약한 방식] 클라에서 펠릿 방향을 서버로 전송
→ 핵 클라이언트: 모든 펠릿을 조준점(0 스프레드)으로 전송 가능
→ 서버가 그대로 판정 → 무적 산탄총 핵 허용

[올바른 방식] 클라/서버 동일 시드로 독립 계산
→ 클라이언트가 어떤 방향을 보내도 서버는 자체 계산한 방향 사용
→ 스프레드 조작 불가
```

**시드 충돌이 없는 이유:**
- `ClientFireTime`은 `GS->GetServerWorldTimeSeconds()` 기준
- 밀리초 단위 변환 → 같은 플레이어가 연속 발사해도 7ms 이상 차이 (128Hz에서도)
- 다른 플레이어가 정확히 같은 밀리초에 발사해도 서로 다른 무기 → 문제 없음

---

## 결과

**확인 항목 (PIE Dedicated Server):**
- 클라이언트 콘솔에서 `stat net` → FireRate 초과 RPC 가 서버에서 차단되는지 로그 확인
- Origin이 200cm 초과 위치에서 Server_Fire 직접 호출 시 조용히 거부 확인
- DA_AK74에서 PelletCount = 5 설정 후 산탄총 모드 테스트 (5방향 트레이스)

**한계 및 향후 개선:**
- `LastServerFireTime`은 수동 관리 → GAS 단계에서 Cooldown GameplayEffect로 교체
- Origin drift 200cm는 보수적 값 — 투사체 무기 추가 시 재조정 필요

---

## 참고

- `DOCS/Notes/03_NetPrediction.md` Section 4 (RPC 설계, 결정론적 RNG)
- `DOCS/Notes/03_NetPrediction_Implementation.md` Step 1~7, 13
