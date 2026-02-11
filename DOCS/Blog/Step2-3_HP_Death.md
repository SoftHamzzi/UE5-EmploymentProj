# [UE5 C++] 2단계-3: HP/데미지 + 사망/Corpse

> 02_Implementation Step 5 (HP + 데미지), Step 6 (사망 + Corpse)

---

## 1. 이번 글에서 다루는 것

- HP / MaxHP: ReplicatedUsing으로 모든 클라이언트에 동기화
- TakeDamage: UE5 기본 데미지 프레임워크 활용
- 서버 권한 사망 판정: HP <= 0 → Die()
- AEPCorpse: 사망 시 시체 Actor 스폰
- GameMode::OnPlayerKilled: 킬 카운트 + 매치 흐름 연동

---

## 2. 전체 흐름

```
[서버]
Server_Fire → 레이캐스트 히트 → ApplyDamage(피격 Actor)
  → AEPCharacter::TakeDamage()
       ├─ HP 감소
       ├─ HP <= 0 → Die()
       │    ├─ Corpse 스폰 + 초기화
       │    ├─ GameMode::OnPlayerKilled()
       │    │    ├─ KillerPS->AddKill()
       │    │    ├─ KillerPC->Client_OnKill()
       │    │    └─ AlivePlayerCount-- → CheckMatchEndConditions()
       │    └─ 캐릭터 숨김/콜리전 해제
       └─ HP 복제 → 클라이언트 OnRep_HP

[클라이언트]
OnRep_HP() → 피격 피드백 (체력바, 히트 이펙트 등)
```

---

## 3. HP 시스템

### 3-1. 프로퍼티 선언

```cpp
// EPCharacter.h
UPROPERTY(ReplicatedUsing = OnRep_HP, BlueprintReadOnly, Category = "Combat")
float HP = 100.f;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
float MaxHP = 100.f;

UFUNCTION()
void OnRep_HP(float OldHP);
```

<!--
  HP: ReplicatedUsing
  - 모든 클라이언트에 복제 (COND_None) → 체력바 등 UI 표시용
  - OnRep_HP(float OldHP): 이전 값을 파라미터로 받아 데미지량 계산 가능

  MaxHP: 복제하지 않음
  - EditDefaultsOnly → 모든 캐릭터가 같은 값
  - 런타임에 변경하지 않으므로 복제 불필요
-->

### 3-2. GetLifetimeReplicatedProps

```cpp
void AEPCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AEPCharacter, HP);
    DOREPLIFETIME(AEPCharacter, EquippedWeapon);
}
```

<!--
  COND_None (기본):
  - HP는 모든 클라이언트에 복제 → 상대 체력바 표시 가능
  - 만약 소유자에게만 보여주려면 COND_OwnerOnly로 변경
-->

### 3-3. TakeDamage

```cpp
float AEPCharacter::TakeDamage(float DamageAmount,
    FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!HasAuthority()) return 0.f;

    float ActualDamage = Super::TakeDamage(
        DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    HP = FMath::Clamp(HP - ActualDamage, 0.f, MaxHP);

    if (HP <= 0.f)
        Die(EventInstigator);

    return ActualDamage;
}
```

<!--
  HasAuthority() 체크:
  - TakeDamage는 서버에서만 실행되어야 함
  - 클라이언트에서 호출되면 무시 (HP 조작 방지)

  UE5 TakeDamage 프레임워크:
  - UGameplayStatics::ApplyDamage() → AActor::TakeDamage() 호출
  - FDamageEvent: 데미지 타입 정보 (Point, Radial 등)
  - EventInstigator: 데미지를 유발한 Controller (킬러)
  - DamageCauser: 데미지를 유발한 Actor (무기, 투사체 등)

  FMath::Clamp:
  - HP가 0 미만이나 MaxHP 초과가 되지 않도록
-->

### 3-4. OnRep_HP

```cpp
void AEPCharacter::OnRep_HP(float OldHP)
{
    float Delta = HP - OldHP;
    if (Delta < 0.f)
    {
        // 피격 이펙트 (체력바 업데이트, 화면 효과 등)
    }
}
```

<!--
  클라이언트에서만 호출됨:
  - 서버에서는 호출 안 됨 → 서버 피드백이 필요하면 별도 처리
  - OldHP 파라미터: OnRep은 새 값이 이미 쓰여진 후 호출됨. 이전 값은 파라미터로 받음
  - Delta < 0: 데미지. Delta > 0: 회복
-->

---

## 4. 사망 시스템

### 4-1. Die()

```cpp
void AEPCharacter::Die(AController* Killer)
{
    if (!HasAuthority()) return;

    // Corpse 스폰
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AEPCorpse* Corpse = GetWorld()->SpawnActor<AEPCorpse>(
        AEPCorpse::StaticClass(), GetActorTransform(), Params);
    if (Corpse)
        Corpse->InitializeFromCharacter(this);

    // GameMode에 킬 통보
    if (AEPGameMode* GM = GetWorld()->GetAuthGameMode<AEPGameMode>())
        GM->OnPlayerKilled(Killer, GetController());

    // 캐릭터 숨김
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}
```

<!--
  왜 bIsDead를 PlayerState에 두지 않는가:
  - 시체는 별도 Actor(AEPCorpse)로 관리
  - 캐릭터는 숨기기만 하고, 리스폰 시 다시 표시
  - PlayerState에 bIsDead를 두면 복제 + 상태 관리 복잡도 증가

  GetAuthGameMode<>():
  - 서버에서만 유효 (GameMode는 서버 전용)
  - 클라이언트에서는 nullptr 반환
-->

### 4-2. GameMode::OnPlayerKilled

```cpp
void AEPGameMode::OnPlayerKilled(AController* Killer, AController* Victim)
{
    // 킬러 킬 카운트 증가
    if (Killer && Killer != Victim)
    {
        if (AEPPlayerState* KillerPS = Killer->GetPlayerState<AEPPlayerState>())
            KillerPS->AddKill();

        if (AEPPlayerController* KillerPC = Cast<AEPPlayerController>(Killer))
        {
            AEPCharacter* VictimChar = Victim ?
                Cast<AEPCharacter>(Victim->GetPawn()) : nullptr;
            KillerPC->Client_OnKill(VictimChar);
        }
    }

    AlivePlayerCount--;
    CheckMatchEndConditions();
}
```

<!--
  Killer != Victim 체크: 자살 시 킬 카운트 증가 방지
  Client_OnKill: 킬러에게만 킬 피드백 (Client RPC)
  AlivePlayerCount--: 매치 종료 조건 확인
-->

---

## 5. AEPCorpse — 시체 Actor

### 5-1. 헤더

```cpp
UCLASS()
class EMPLOYMENTPROJ_API AEPCorpse : public AActor
{
    GENERATED_BODY()

public:
    AEPCorpse();
    void InitializeFromCharacter(AEPCharacter* DeadCharacter);

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> CorpseMesh;

    UPROPERTY(Replicated)
    FString OwnerPlayerName;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

### 5-2. InitializeFromCharacter

```cpp
void AEPCorpse::InitializeFromCharacter(AEPCharacter* DeadCharacter)
{
    if (!DeadCharacter) return;

    // 메시 복사
    if (USkeletalMeshComponent* CharMesh = DeadCharacter->GetMesh())
    {
        CorpseMesh->SetSkeletalMesh(CharMesh->GetSkeletalMeshAsset());
        CorpseMesh->SetAnimInstanceClass(nullptr);
        // 래그돌 물리 시뮬레이션
        CorpseMesh->SetSimulatePhysics(true);
        CorpseMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    }

    // 플레이어 이름 저장
    if (APlayerState* PS = DeadCharacter->GetPlayerState())
        OwnerPlayerName = PS->GetPlayerName();
}
```

<!--
  래그돌:
  - SetSimulatePhysics(true): 시체가 물리 영향 받음
  - AnimInstance 제거: 애니메이션 정지, 물리만 적용

  OwnerPlayerName:
  - Replicated → 클라이언트에서 "누구의 시체인지" 표시 가능
  - 나중에 루팅 시스템 등에 활용

  bReplicates = true:
  - 서버에서 스폰 → 클라이언트에 자동 생성
-->

---

## 6. 복제 정리

### 프로퍼티

| 데이터 | 위치 | 방식 | 조건 | 용도 |
|--------|------|------|------|------|
| HP | AEPCharacter | ReplicatedUsing | COND_None | 체력바 |
| OwnerPlayerName | AEPCorpse | Replicated | COND_None | 시체 식별 |

### RPC

| RPC | 타입 | 용도 |
|-----|------|------|
| Client_OnKill | Client, Reliable | 킬러에게 킬 피드백 |

### 서버 권한 흐름

```
ApplyDamage (서버만)
  → TakeDamage (HasAuthority 체크)
    → HP 감소 (복제)
    → Die (서버만)
      → Corpse 스폰 (복제)
      → OnPlayerKilled (GameMode, 서버 전용)
```

---

## 7. 배운 점 / 삽질 기록

<!--
  - TakeDamage의 HasAuthority 체크 중요성
  - OnRep_HP의 OldHP 파라미터 활용법
  - Corpse vs bIsDead 설계 판단
  - 래그돌 물리 설정 시 주의점
  - GetAuthGameMode이 클라이언트에서 nullptr인 점
  - 기타 ...
-->

---

## 8. 다음 단계

<!-- Step2-4: 애니메이션 시스템에서 다룰 내용 미리보기 -->
