# Post 2-6 작성 가이드 — 전투 네트워킹 (HP / 사격 / 사망 / 피격 피드백)

> **예상 제목**: `[UE5] 추출 슈터 2-6. 멀티플레이어 전투 구현 — HP 복제, 사망 래그돌, 피격 피드백`
> **참고 문서**: DOCS/Notes/02_Implementation.md Step 4~6

---

## 개요

**이 포스팅에서 다루는 것:**
- HP 복제, 서버 권한 TakeDamage, 사망 처리 (셀프 래그돌)
- 피격 애니메이션/사운드를 멀티플레이어에서 동기화하는 방법
- "챡"(히트 확인)과 "윽"(피격음)이 다른 클라이언트에게 들려야 하는 이유

**왜 이렇게 구현했는가 (설계 의도):**
- 서버만 판정하고, 클라는 이펙트/사운드만 표현 → 치팅 불가
- Corpse 별도 Actor 대신 셀프 래그돌 → 복잡도 감소, LeaderPose 덕에 자동으로 Face/Outfit도 따라감
- 피격 피드백 분리(챡/윽)는 Stage 3 Lag Compensation과 연결되는 중요한 설계

---

## 구현 전 상태 (Before)

```cpp
// 기존: HP가 복제 없이 서버에만 존재
UPROPERTY()
float HP = 100.f;  // 복제 안 됨 → 클라에서 항상 100으로 보임
```

---

## 구현 내용

### 1. HP 복제

```cpp
// EPCharacter.h
UPROPERTY(ReplicatedUsing = OnRep_HP, BlueprintReadOnly, Category = "Combat")
float HP = 100.f;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
float MaxHP = 100.f;

UFUNCTION()
void OnRep_HP(float OldHP);

// GetLifetimeReplicatedProps
DOREPLIFETIME(AEPCharacter, HP);

// OnRep — 클라이언트에서 UI 업데이트 등
void AEPCharacter::OnRep_HP(float OldHP)
{
    // 예: 체력바 위젯 업데이트
    // 예: 피격 화면 효과 (BloodOverlay)
}
```

### 2. 서버 권한 TakeDamage

```cpp
float AEPCharacter::TakeDamage(float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority()) return 0.f;  // 반드시 서버에서만

    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    HP = FMath::Clamp(HP - ActualDamage, 0.f, MaxHP);

    // 피격 피드백 3종 (서버에서 호출)
    Multicast_PlayHitReact();        // 피격 애니메이션 (모두에게)
    Multicast_PlayPainSound();       // 피격음 "윽" (모두에게)
    if (AEPPlayerController* InstigatorPC = Cast<AEPPlayerController>(EventInstigator))
        InstigatorPC->Client_PlayHitConfirmSound();  // 히트 확인음 "챡" (쏜 사람만)

    if (HP <= 0.f) Die(EventInstigator);

    ForceNetUpdate();  // HP 즉시 복제 (다음 틱 기다리지 않음)
    return ActualDamage;
}
```

### 3. 피격 피드백 3종 분리 설명

**독자가 헷갈리는 부분이므로 표로 명확하게:**

| 피드백 | RPC 종류 | 실행 위치 | 들리는 대상 |
|--------|----------|-----------|-------------|
| 피격 애니메이션 | `Multicast_PlayHitReact` (Unreliable) | 서버+모든 클라 | 눈에 보이는 모두 |
| 피격음 "윽" | `Multicast_PlayPainSound` (Unreliable) | 서버+모든 클라 | 모두 |
| 히트 확인음 "챡" | `Client_PlayHitConfirmSound` (Unreliable) | 쏜 사람 클라만 | 쏜 사람만 |

**챡/윽 분리의 이유:**
- "챡"은 내가 적을 맞혔다는 확인 피드백 → 나만 들어야 함
- "윽"은 캐릭터가 맞는 연기 → 주변 모두가 들어야 함 (공간감)
- Stage 3 Lag Compensation과 연결: `Client_PlayHitConfirmSound`는 서버 히트 판정 후 호출되는 정확한 피드백

```cpp
// EPCharacter.h
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayHitReact();

UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayPainSound();

// EPPlayerController.h
UFUNCTION(Client, Unreliable)
void Client_PlayHitConfirmSound();

// 구현
void AEPCharacter::Multicast_PlayHitReact_Implementation()
{
    if (HitReactMontage && GetMesh()->GetAnimInstance())
        GetMesh()->GetAnimInstance()->Montage_Play(HitReactMontage);
}
```

### 4. HitReact 몽타주 — AdditiveHitReact 슬롯

**왜 Additive 슬롯인가:**
```
DefaultSlot → 현재 포즈를 대체 (로코모션이 멈춤)
AdditiveHitReact → 현재 포즈 위에 더해짐 (이동 중에도 피격 반응)
```

AnimGraph 배치:
```
[DefaultSlot]          ← Fire/Reload 몽타주
    ↓
[Slot: AdditiveHitReact]  ← 피격 반응 (맨 마지막에 배치)
    ↓
[Output Pose]
```

> **스크린샷 위치**: AnimGraph에서 AdditiveHitReact 슬롯 배치 모습

### 5. 사망 처리 — 셀프 래그돌

```cpp
// EPCharacter.h
UFUNCTION(NetMulticast, Reliable)
void Multicast_Die();

// 구현
void AEPCharacter::Multicast_Die_Implementation()
{
    // 캡슐 콜리전 비활성화 (벽 통과 방지)
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Body 메시를 Ragdoll 물리로 전환
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetSimulatePhysics(true);

    // Face/Outfit은 LeaderPoseComponent로 Body를 따라감 → 별도 처리 불필요
}

// 서버에서 Die 호출
void AEPCharacter::Die(AController* Killer)
{
    if (!HasAuthority()) return;

    // GameMode에 킬 알림
    if (AEPGameMode* GM = GetWorld()->GetAuthGameMode<AEPGameMode>())
        GM->OnPlayerKilled(Killer, GetController());

    Multicast_Die();  // 모든 클라에서 래그돌 처리
}
```

**Corpse 별도 액터 대신 셀프 래그돌을 선택한 이유:**
- Corpse Actor 스폰 → 복제 설정, 메시 복사, 인벤토리 이전 등 복잡도 증가
- 셀프 래그돌 → LeaderPoseComponent 덕에 Body 하나만 물리 켜면 Face/Outfit 자동으로 따라감
- 향후 루팅 시스템은 시체 Actor와 별개로 인벤토리 컴포넌트로 해결 가능

### 6. ForceNetUpdate 사용 이유

```cpp
// HP = 0.f 된 직후 즉시 복제 (다음 틱까지 기다리지 않음)
ForceNetUpdate();
```

UE5 복제는 기본적으로 틱마다 배치 처리. 사망처럼 즉각 반응이 필요한 상황에서 `ForceNetUpdate()`로 강제 즉시 복제.

---

## 결과

**확인 항목:**
- 피격 시 AdditiveHitReact 몽타주 재생 (이동 중에도)
- 쏜 사람에게만 "챡" 소리, 맞은 캐릭터 주변 모두에게 "윽" 소리
- HP 0 → 모든 클라에서 래그돌 발동
- HP바(있다면) 모든 클라에서 동기화

**한계 및 향후 개선:**
- 현재 히트 판정은 서버 현재 위치 기준 → Stage 3에서 Lag Compensation으로 과거 위치 기준으로 교체
- `Client_PlayHitConfirmSound`가 Stage 3 Lag Compensation 구현의 출발점

---

## 참고

- `DOCS/Notes/02_Implementation.md` Step 4, 5, 6
- UE5 Montage, AdditiveSlot 문서
- `ForceNetUpdate` 공식 문서
