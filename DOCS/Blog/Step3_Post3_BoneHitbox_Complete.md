# Post 3-3 작성 가이드 — 판정 연결 + 부위 데미지 + 클라 예측 이펙트

> **예상 제목**: `[UE5] 추출 슈터 3-3. 히트스캔 완성: 부위 배율, 클라이언트 예측 이펙트, 래그돌 처리`
> **참고 문서**: `DOCS/Notes/03_BoneHitbox_Implementation.md` Step 5~12, `DOCS/Notes/03_NetPrediction_Implementation.md` Step 6

---

## 개요

**이 포스팅에서 다루는 것:**
- `HandleHitscanFire` — SSR 위임 구조와 Damage Block 분리 설계
- 부위별 데미지 배율 (`GetBoneMultiplier` + `UEPPhysicalMaterial`)
- `RequestFire`에서 총구 이펙트 즉시 예측 재생 (RTT 없음)
- `Multicast_PlayMuzzleEffect` 중복 방지 패턴
- 래그돌 사망 시 Groom(머리카락) 처리 문제와 해결

**왜 이렇게 구현했는가 (설계 의도):**
- Damage Block을 SSR 위임과 분리 → GAS 단계에서 `ApplyPointDamage`만 `GameplayEffectSpec`으로 교체하면 끝
- 총구 이펙트 즉시 예측: RTT만큼의 딜레이가 조작감을 크게 해침 → 클라이언트 체감 우선
- Groom은 Physics Asset 래그돌과 궁합이 최악 — 현실적 우선순위에서 숨기기 선택

---

## 구현 내용

### 1. HandleHitscanFire — SSR 위임 패턴

```cpp
// EPCombatComponent.cpp (private)
void UEPCombatComponent::HandleHitscanFire(
    AEPCharacter* Owner, const FVector& Origin,
    const TArray<FVector>& Directions, float ClientFireTime)
{
    if (!Owner || !Owner->GetServerSideRewindComponent()) return;

    // [Rewind Block] SSR에 완전 위임
    // → Broad Phase, 리와인드, Narrow Trace, 복구, 디버그 전부 SSR 내부
    TArray<FHitResult> ConfirmedHits;
    Owner->GetServerSideRewindComponent()->ConfirmHitscan(
        Owner, EquippedWeapon, Origin, Directions, ClientFireTime, ConfirmedHits);

    // [Damage Block] GAS 전환 시 GameplayEffectSpec으로 교체
    for (const FHitResult& Hit : ConfirmedHits)
    {
        if (!Hit.GetActor()) continue;

        const float BaseDamage         = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
        const float BoneMultiplier     = GetBoneMultiplier(Hit.BoneName);
        const float MaterialMultiplier = GetMaterialMultiplier(Hit.PhysMaterial.Get());
        const float FinalDamage        = BaseDamage * BoneMultiplier * MaterialMultiplier;

        UGameplayStatics::ApplyPointDamage(
            Hit.GetActor(), FinalDamage,
            (Hit.ImpactPoint - Origin).GetSafeNormal(),
            Hit, Owner->GetController(), Owner, UDamageType::StaticClass());

        Multicast_PlayImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal);
    }
}
```

**블록 분리의 의미:**

| 블록 | 내용 | GAS 이후 변화 |
|------|------|--------------|
| Rewind Block | SSR::ConfirmHitscan | 변경 없음 |
| Damage Block | ApplyPointDamage | GameplayEffectSpec으로 교체 |
| Impact Effect | Multicast_PlayImpactEffect | 변경 없음 |

### 2. 부위별 데미지 배율

**GetBoneMultiplier:**

```cpp
float UEPCombatComponent::GetBoneMultiplier(const FName& BoneName) const
{
    // WeaponDefinition에 TMap<FName, float> BoneDamageMultiplierMap 저장
    // DA_AK74 에셋에서 에디터로 설정
    if (EquippedWeapon && EquippedWeapon->WeaponDef)
        if (const float* Found = EquippedWeapon->WeaponDef->BoneDamageMultiplierMap.Find(BoneName))
            return *Found;

    // 목록에 없는 본 → 기본 배율 1.0 + Verbose 로그
    UE_LOG(LogTemp, Verbose, TEXT("[BoneHitbox] Unknown bone: %s"), *BoneName.ToString());
    return 1.0f;
}
```

**DA_AK74 에셋 배율 예시:**

| 본 | 배율 | 비고 |
|----|------|------|
| head | 2.0 | 헤드샷 2배 |
| neck_01 | 1.5 | |
| spine_03 / spine_02 | 1.0 | 기본 |
| upperarm / lowerarm | 0.75 | 팔 감소 |
| thigh / calf | 0.75 | 다리 감소 |

**UEPPhysicalMaterial — MaterialMultiplier:**

```cpp
// Public/Combat/EPPhysicalMaterial.h
UCLASS()
class UEPPhysicalMaterial : public UPhysicalMaterial
{
    GENERATED_BODY()
public:
    // Physics Asset에서 head 바디에 이 PM 할당
    UPROPERTY(EditDefaultsOnly, Category = "Damage")
    bool bIsWeakSpot = false;

    UPROPERTY(EditDefaultsOnly, Category = "Damage",
        meta = (EditCondition = "bIsWeakSpot", ClampMin = 1.0f))
    float WeakSpotMultiplier = 2.0f;
};
```

**최종 데미지 계산:**

```
FinalDamage = BaseDamage × BoneMultiplier × MaterialMultiplier

예: AK-74 BaseDamage=35, head 본 (BoneMultiplier=2.0), 약점 PM (MaterialMultiplier=2.0)
→ 35 × 2.0 × 2.0 = 140 (즉사)

예: 팔 (BoneMultiplier=0.75), 일반 PM (MaterialMultiplier=1.0)
→ 35 × 0.75 × 1.0 = 26.25
```

**PhysicalMaterial이 필요한 이유:**

```cpp
// LineTrace 시 반드시 bReturnPhysicalMaterial = true 설정
FCollisionQueryParams Params;
Params.bReturnPhysicalMaterial = true;
// → Hit.PhysMaterial에 바디에 할당된 PM이 채워짐
// → PhysicalMaterial 없으면 Hit.PhysMaterial.IsValid() == false
```

> **에디터 설정**: Physics Asset에서 head 바디 선택 → Physical Material 슬롯에 약점용 PM 할당

### 3. 서버 검증 로그

```cpp
UE_LOG(LogTemp, Log,
    TEXT("[BoneHitbox] Bone=%s PM=%s Base=%.1f Bone*=%.2f Mat*=%.2f Final=%.1f"),
    *Hit.BoneName.ToString(),
    Hit.PhysMaterial.IsValid() ? *Hit.PhysMaterial->GetName() : TEXT("None"),
    BaseDamage, BoneMultiplier, MaterialMultiplier, FinalDamage);
```

### 4. 클라이언트 예측 이펙트 — Muzzle 즉시 재생

**문제: Multicast만 쓰면 RTT만큼 딜레이**
```
클라이언트: 발사 → Server_Fire RPC 전송
서버: RPC 수신 → Multicast_PlayMuzzleEffect
클라이언트: Multicast 수신 → 이펙트 재생  ← RTT 후 (100ms면 0.1초 딜레이)
```

**해결: RequestFire에서 즉시 로컬 재생**

```cpp
void UEPCombatComponent::RequestFire(const FVector& Origin, const FVector& Direction, float ClientFireTime)
{
    // ... 검증 ...

    AEPCharacter* Owner = GetOwnerCharacter();
    if (Owner && Owner->IsLocallyControlled())
    {
        // RTT 없이 즉시 재생 — Multicast보다 RTT만큼 빠름
        const FVector MuzzleLocation =
            (EquippedWeapon->WeaponMesh && EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket")))
            ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
            : EquippedWeapon->GetActorLocation();
        PlayLocalMuzzleEffect(MuzzleLocation);
    }

    Server_Fire(Origin, Direction, ClientFireTime);

    if (Owner && Owner->IsLocallyControlled())
    {
        // 반동도 즉시 예측 — 서버 확인 전에 적용
        float Pitch = EquippedWeapon->GetRecoilPitch();
        float Yaw = FMath::RandRange(-EquippedWeapon->GetRecoilYaw(), EquippedWeapon->GetRecoilYaw());
        Owner->AddControllerPitchInput(-Pitch);
        Owner->AddControllerYawInput(Yaw);
    }
}
```

**Multicast에서 발사자 중복 방지:**

```cpp
void UEPCombatComponent::Multicast_PlayMuzzleEffect_Implementation(const FVector_NetQuantize& MuzzleLocation)
{
    AEPCharacter* OwnerChar = GetOwnerCharacter();
    if (OwnerChar && OwnerChar->IsLocallyControlled()) return; // 발사자는 이미 재생됨

    PlayLocalMuzzleEffect(MuzzleLocation);
}
```

**전체 흐름:**

```
[발사자 클라이언트]
RequestFire → PlayLocalMuzzleEffect (즉시) ← RTT 없음
           → Server_Fire RPC ─────────────────────────→ [서버]
                                                          HandleHitscanFire
                                                          Multicast_PlayMuzzleEffect
[발사자]  IsLocallyControlled() → return              ← 중복 방지
[다른 클라] IsLocallyControlled() → false → PlayLocal ← 다른 클라에게는 정상 재생
```

### 5. ClientFireTime — 시간 기준 통일

```cpp
// AEPCharacter::Input_Fire (EPCharacter.cpp)
void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
    if (!CombatComponent) return;

    // GS->GetServerWorldTimeSeconds(): GameState가 복제하는 서버 기준 시간
    // SSR HitboxHistory도 동일 기준으로 기록 → 리와인드 시각이 정확히 일치
    const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
    const float ClientFireTime = GS
        ? GS->GetServerWorldTimeSeconds()
        : GetWorld()->GetTimeSeconds();

    CombatComponent->RequestFire(
        FirstPersonCamera->GetComponentLocation(),
        FirstPersonCamera->GetForwardVector(),
        ClientFireTime);
}
```

**GetWorld()->GetTimeSeconds() vs GS->GetServerWorldTimeSeconds():**

| | GetWorld()->GetTimeSeconds() | GS->GetServerWorldTimeSeconds() |
|---|---|---|
| 기준 | 로컬 시계 | 서버 시계 (복제) |
| 클라/서버 차이 | RTT/2만큼 차이 | 동기화됨 |
| 리와인드 정확도 | 핑에 따라 오차 발생 | 정확 |

### 6. 래그돌 사망 — Groom 처리

**문제: 래그돌 활성화 시 머리카락이 하늘로 올라가는 현상**

```
원인:
1. GetMesh()->SetSimulatePhysics(true) 호출
2. 물리 시뮬레이션은 ComponentSpaceTransforms를 갱신
3. LeaderPose(SetLeaderPoseComponent)는 BoneSpaceTransforms를 복사
4. 물리 시뮬 중 BoneSpaceTransforms는 갱신 안됨 → 사망 직전 포즈로 고정
5. Groom 가이드 커브 루트가 FaceMesh 소켓에 바인딩 → 소켓이 고정된 위치에 남음
6. Groom 시뮬레이션이 가이드 커브 위치를 기준으로 날아감 → 머리카락 상승
```

**해결: 래그돌 시 Groom 숨기기**

```cpp
void AEPCharacter::Multicast_Die_Implementation()
{
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetSimulatePhysics(true);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();

    // FaceMesh 아래의 Groom 컴포넌트들만 숨김
    // FaceMesh 자체는 건드리지 않음 (얼굴 메시 유지)
    if (FaceMesh)
    {
        TArray<USceneComponent*> FaceChildren;
        FaceMesh->GetChildrenComponents(false, FaceChildren); // false: 직접 자식만
        for (USceneComponent* Child : FaceChildren)
            Child->SetVisibility(false, true); // true: 자손도 숨김
    }
}
```

**FaceMesh->SetVisibility(false)가 아닌 이유:**
- `SetVisibility(false)`를 FaceMesh에 직접 호출하면 얼굴 메시 자체가 사라짐
- 원하는 건 Groom(머리카락, 수염)만 숨기기 → `GetChildrenComponents`로 자식만 처리

**현업에서의 이상적 해결법:**
- 별도 Corpse 액터 스폰 (Body + Face + Groom 컴포넌트를 모두 독립적으로 재구성)
- Groom의 Binding 에셋을 Corpse 바디 메시에 맞게 재베이크
- 현재 포트폴리오에서는 복잡도 대비 비용이 높아 숨기기로 처리

---

## 결과

**확인 항목 (PIE Dedicated Server + Client 2명):**
- 헤드샷 시 UE_LOG에서 `Bone=head, Final=70.0` (BaseDamage=35 × 2.0) 확인
- 팔 히트 시 `Bone=upperarm_l, Final=26.25` (35 × 0.75) 확인
- 사격 시 총구 이펙트가 버튼 누름과 동시에 (RTT 없이) 즉시 재생되는지 확인
- 래그돌 시 머리카락 사라짐 (하늘로 올라가지 않음) 확인
- 다른 클라이언트에서 보는 사격 이펙트 정상 재생 확인

**한계 및 향후 개선:**
- Groom 래그돌 완전 지원: Corpse 액터 스폰으로 전환 (GAS 단계 이후)
- `UEPPhysicalMaterial` MaterialTags → GAS 단계에서 `GameplayTagContainer` 기반으로 교체 예정

---

## 참고

- `DOCS/Notes/03_BoneHitbox_Implementation.md` Step 5~12
- `DOCS/Notes/03_NetPrediction_Implementation.md` Step 6 (RequestFire 현재 구현)
- `DOCS/Mine/Rewind.md` — HandleHitscanFire 위임 패턴 근거
