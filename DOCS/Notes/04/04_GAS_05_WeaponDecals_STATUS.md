# 04_GAS_05_WeaponDecals — 구현 상태

**전체 상태: 완료 (PIE 검수 완료)**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷.
> 구현 방식: `BP_PlayImpactEffect` BlueprintImplementableEvent + `Spawn Decal at Location` (무기 BP 담당).
> `04_GAS_05_Spread.md` Section 9의 `ImpactDecal UPROPERTY` 방식은 채택되지 않았음.

---

## 최종 아키텍처

```
[서버] HandleHitscanFire
  → ConfirmHitscan (캐릭터 히트 + 환경 히트 모두 수집)
  → 캐릭터 히트만 ApplyGEDamage
  → Multicast_PlayImpactEffect(TArray ImpactPoints, TArray ImpactNormals) — 전체 히트 배열 1회 전송

[클라이언트] Multicast_PlayImpactEffect_Implementation
  → 배열 루프 → PlayLocalImpactEffect 반복 호출

[클라이언트] PlayLocalImpactEffect
  → ImpactFX (Niagara)
  → ImpactSFX (Sound)
  → EquippedWeapon->BP_PlayImpactEffect (무기 BP에서 구현)
      → Spawn Decal at Location
      → IsValid 체크 후 Set Fade Screen Size
```

---

## C++ 구현 상태

### EPWeapon.h — `BP_PlayImpactEffect` (완료)

```cpp
UFUNCTION(BlueprintImplementableEvent)
void BP_PlayImpactEffect(const FVector& ImpactPoint, const FVector& ImpactNormal, uint8 SurfaceType);
```

### EPCombatComponent.h — `Multicast_PlayImpactEffect` 배열 시그니처 (완료)

```cpp
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayImpactEffect(const TArray<FVector_NetQuantize>& ImpactPoints, const TArray<FVector_NetQuantize>& ImpactNormals);
```

### EPCombatComponent.cpp — `PlayLocalImpactEffect` (완료)

- `ImpactFX`, `ImpactSFX` 스폰 후 `EquippedWeapon->BP_PlayImpactEffect` 호출

### EPCombatComponent.cpp — `Multicast_PlayImpactEffect_Implementation` (완료)

```cpp
for (int32 i = 0; i < ImpactPoints.Num(); ++i)
    PlayLocalImpactEffect(ImpactPoints[i], ImpactNormals[i]);
```

### EPCombatComponent.cpp — `HandleHitscanFire` 루프 (완료)

- `ImpactPoints.Add` / `ImpactNormals.Add`가 `if (HitChar)` 블록 **바깥**에 위치
- `if (!Hit.GetActor()) continue;` 가드 제거 — 환경 히트 통과 허용
- 루프 종료 후 `Multicast_PlayImpactEffect(ImpactPoints, ImpactNormals)` 1회 호출

### EPServerSideRewindComponent.cpp — `ConfirmHitscan` 환경 히트 추가 (완료)

```cpp
else if (!HitChar)  // 환경(벽/바닥) 히트 — 이펙트 재생 목적
{
    OutConfirmedHits.Add(Hit);
}
```

---

## 에디터 구현 상태

### BP_WeaponAK74 — `BP_PlayImpactEffect` 오버라이드 (완료)

- Spawn Decal at Location → Return Value IsValid → Set Fade Screen Size 패턴
- 데칼 머티리얼 연결 완료

---

## 발견 및 수정된 버그

### 샷건 10발 중 2발만 데칼 (수정 완료)

- **원인 1**: `Multicast_PlayImpactEffect` 동시 10회 호출 → Unreliable 드롭
  → 배열 RPC 1회 호출로 해결
- **원인 2**: `HandleHitscanFire` — `ImpactPoints.Add`가 `if (HitChar)` 블록 안에 있어 벽 히트 미수집
  → `if (HitChar)` 블록 밖으로 이동
- **원인 3**: `ConfirmHitscan`이 환경 히트를 `OutConfirmedHits`에 미추가
  → `else if (!HitChar)` 추가

### 게임 종료 시 Set Fade Screen Size null 오류 (수정 완료)

- **원인**: 월드 해체 중 `Spawn Decal at Location` null 반환
- **수정**: Return Value에 IsValid 체크 추가 (BP)
