# 탄흔 데칼 시스템 — SpawnDecalAtLocation 방식

> Spread 분포 시각화 목적.
> 탄착 위치에 데칼을 남겨 스프레드 패턴을 육안으로 확인한다.

---

## 1. 아키텍처

```
[서버] HandleHitscanFire
  → ConfirmHitscan (캐릭터 히트 + 환경 히트 모두 수집)
  → 캐릭터만 데미지 적용
  → Multicast_PlayImpactEffect (모든 히트)

[클라이언트] PlayLocalImpactEffect
  → ImpactFX (Niagara 파티클)
  → ImpactSFX (사운드)
  → EquippedWeapon->BP_PlayImpactEffect  ← 무기 BP에서 구현
      → Spawn Decal at Location
```

무기가 자신의 데칼을 책임지는 구조. 무기마다 다른 머티리얼을 쓸 수 있다.

---

## 2. C++ 현황 (완료)

### EPWeapon.h — `BP_PlayImpactEffect` (완료)

```cpp
UFUNCTION(BlueprintImplementableEvent)
void BP_PlayImpactEffect(const FVector& ImpactPoint, const FVector& ImpactNormal, uint8 SurfaceType);
```

### EPCombatComponent.cpp — `PlayLocalImpactEffect` (완료)

```cpp
void UEPCombatComponent::PlayLocalImpactEffect(const FVector& ImpactPoint, const FVector& ImpactNormal)
{
    const FRotator ImpactRot = ImpactNormal.Rotation();

    if (ImpactFX)
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactFX, ImpactPoint, ImpactRot);
    if (ImpactSFX)
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), ImpactSFX, ImpactPoint);

    if (EquippedWeapon)
        EquippedWeapon->BP_PlayImpactEffect(ImpactPoint, ImpactNormal, 0);
}
```

---

## 3. 선행 버그 수정 — ConfirmHitscan 환경 히트 누락

`EPServerSideRewindComponent.cpp` — ConfirmHitscan 내 트레이스 결과 처리:

```cpp
// 기존
AEPCharacter* HitChar = Cast<AEPCharacter>(Hit.GetActor());
if (HitChar && CandidateSet.Contains(HitChar))
{
    OutConfirmedHits.Add(Hit);
    // ... 디버그 코드 유지 ...
}

// 변경 후
AEPCharacter* HitChar = Cast<AEPCharacter>(Hit.GetActor());
if (HitChar && CandidateSet.Contains(HitChar))
{
    OutConfirmedHits.Add(Hit);
    // ... 디버그 코드 유지 ...
}
else if (!HitChar)  // 환경(벽/바닥) 히트 — 이펙트 재생 목적
{
    OutConfirmedHits.Add(Hit);
}
```

---

## 4. BP 구현 — 무기 BP에서 Spawn Decal at Location

`BP_WeaponAK74` → Functions → `BP_PlayImpactEffect` 오버라이드:

```
Event BP_PlayImpactEffect (ImpactPoint, ImpactNormal, SurfaceType)
  ↓
Spawn Decal at Location
  - Decal Material : [직접 제작한 머티리얼]
  - Decal Size     : (2, 10, 10)
  - Location       : ImpactPoint
  - Rotation       : Conv_VectorToRotator(ImpactNormal)
  - Life Span      : 30.0
```

> `ImpactNormal → Rotation`: `Conv_VectorToRotator` 노드. X축이 법선 방향을 향한다.

---

## 5. 머티리얼 설정

- 머티리얼 도메인: **Deferred Decal**
- 블렌드 모드: **DBuffer Translucent Color** (또는 Translucent)
- 텍스처: 알파 채널 있는 탄흔 모양 PNG 권장 (사각형 텍스처는 부자연스럽게 찍힘)
- DBuffer 사용 시: Project Settings → Rendering → **DBuffer Decals** 활성화

---

## 6. 완료 체크리스트

### C++ (완료)
- [x] `EPWeapon.h`: `BP_PlayImpactEffect` BlueprintImplementableEvent
- [x] `EPCombatComponent.cpp`: `PlayLocalImpactEffect` — `EquippedWeapon->BP_PlayImpactEffect` 호출
- [x] `EPCombatComponent.h`: `ImpactDecal` 관련 UPROPERTY 없음

### 코드 수정 필요
- [ ] `EPServerSideRewindComponent.cpp`: 환경 히트 `OutConfirmedHits` 추가 (Section 3)

### 에디터 작업
- [ ] 무기 BP에서 `BP_PlayImpactEffect` 구현 — Spawn Decal at Location (Section 4)
- [ ] 데칼 머티리얼에 알파 채널 있는 탄흔 텍스처 연결

### 검증
- [ ] PIE에서 벽 사격 → 탄흔 데칼 생성 확인
- [ ] 샷건(PelletCount > 1) 발사 → 여러 탄흔 확인
- [ ] Spread 패턴이 데칼로 육안 확인 가능한지 검증
