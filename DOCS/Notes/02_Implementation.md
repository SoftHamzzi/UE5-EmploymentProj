# 2단계 구현 설계서: Replication + Combat + Animation

> 이 문서는 02_Replication.md의 학습 내용을 기반으로,
> 1단계에서 구현한 Gameplay Framework 위에 전투/이동/애니메이션을 구현하는 설계서이다.

---

## 0. 현재 상태 (1단계 완료 기준)

이미 구현된 것:
- GameMode: 매치 흐름 (Waiting → Playing → Ended), 타이머, 랜덤 스폰
- GameState: RemainingTime, MatchPhase 복제
- PlayerState: KillCount, bIsExtracted (COND_OwnerOnly)
- PlayerController: Enhanced Input (Move, Look, Jump, Sprint)
- Character: 1인칭 카메라, WASD 이동, 점프, **Sprint 복제 완료** (Server RPC + OnRep)
- DataAsset: UEPWeaponData, UEPItemData

Sprint는 이미 Server RPC + ReplicatedUsing 패턴으로 구현됨:
```cpp
// 이미 구현됨 - EPCharacter
UPROPERTY(ReplicatedUsing=OnRep_IsSprinting)
bool bIsSprinting;

UFUNCTION(Server, Reliable)
void Server_SetSprinting(bool bNewSprinting);
```

---

## 1. Source 폴더 구조 (2단계 추가분)

```
Public/
├── Core/
│   ├── EPCharacter.h         ← 수정: HP, 무기, 앉기, ADS, 사망
│   ├── EPPlayerController.h  ← 수정: Fire, Crouch, ADS InputAction 추가
│   ├── EPCorpse.h            ← 신규: 시체 액터
│   └── EPGameMode.h          ← 수정: 사망 처리 함수
├── Combat/
│   └── EPWeapon.h            ← 신규: 무기 액터
└── Animation/
    └── EPAnimInstance.h      ← 신규: AnimBP C++ 베이스

Private/
├── Core/
│   ├── EPCharacter.cpp
│   ├── EPPlayerController.cpp
│   ├── EPCorpse.cpp
│   └── EPGameMode.cpp
├── Combat/
│   └── EPWeapon.cpp
└── Animation/
    └── EPAnimInstance.cpp
```

---

## 2. 클래스 설계

### 2-1. AEPWeapon (← AActor 상속)

**역할**: 장착/발사 가능한 무기. 캐릭터 손에 부착.

```cpp
// EPWeapon.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EPWeapon.generated.h"

class UEPWeaponData;
class USkeletalMeshComponent;

UCLASS()
class EMPLOYMENTPROJ_API AEPWeapon : public AActor
{
    GENERATED_BODY()

public:
    AEPWeapon();

    // 무기 데이터 (스탯 참조)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<UEPWeaponData> WeaponData;

    // 무기 메시
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;

    // --- 서버 전용 ---
    // 발사 (서버에서 호출)
    void Fire(FVector Origin, FVector Direction);

    // 현재 탄약
    UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo)
    int32 CurrentAmmo;

    UFUNCTION()
    void OnRep_CurrentAmmo();

protected:
    // 마지막 발사 시각 (연사 속도 제한용, 서버 전용)
    float LastFireTime = 0.f;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

**핵심 포인트:**
- `bReplicates = true`: 서버에서 스폰 시 클라이언트에 복제
- `WeaponData`: DataAsset 참조. 데미지/연사속도 등 스탯.
- `CurrentAmmo`: 본인에게만 복제 (COND_OwnerOnly - 소유 캐릭터 기준)
- `LastFireTime`: 연사 속도 검증용 (서버에서만 사용, 복제 안 함)

---

### 2-2. AEPCorpse (← AActor 상속)

1단계에서 설계한 시체 액터. 실제 구현.

```cpp
// EPCorpse.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EPCorpse.generated.h"

class AEPCharacter;

UCLASS()
class EMPLOYMENTPROJ_API AEPCorpse : public AActor
{
    GENERATED_BODY()

public:
    AEPCorpse();

    // 사망한 캐릭터로부터 초기화 (서버)
    void InitializeFromCharacter(AEPCharacter* DeadCharacter);

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> CorpseMesh;

    // 사망 플레이어 이름
    UPROPERTY(Replicated)
    FString OwnerPlayerName;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

---

### 2-3. UEPAnimInstance (← UAnimInstance 상속)

**역할**: AnimBP의 C++ 베이스. 매 프레임 캐릭터 상태를 읽어 블루프린트에 전달.

```cpp
// EPAnimInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EPAnimInstance.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API UEPAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    // AnimBP에서 사용할 변수들
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float Speed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float Direction = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsInAir = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsCrouching = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsSprinting = false;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bIsAiming = false;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    float AimPitch = 0.f;

    // 매 프레임 업데이트
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
```

```cpp
// EPAnimInstance.cpp
void UEPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    APawn* Owner = TryGetPawnOwner();
    if (!Owner) return;

    // 이동 속도/방향
    FVector Velocity = Owner->GetVelocity();
    Speed = Velocity.Size2D();

    FRotator AimRotation = Owner->GetBaseAimRotation();
    FRotator MovementRotation = Velocity.ToOrientationRotator();
    Direction = FRotator::NormalizeAxis(MovementRotation.Yaw - AimRotation.Yaw);

    // 공중 여부
    if (ACharacter* Char = Cast<ACharacter>(Owner))
    {
        bIsInAir = Char->GetCharacterMovement()->IsFalling();
        bIsCrouching = Char->bIsCrouched;
    }

    // EP 전용 상태
    if (AEPCharacter* EPChar = Cast<AEPCharacter>(Owner))
    {
        bIsSprinting = EPChar->GetIsSprinting();
        bIsAiming = EPChar->GetIsAiming();
        AimPitch = AimRotation.Pitch;
    }
}
```

**핵심 포인트:**
- `NativeUpdateAnimation()`: C++에서 매 프레임 호출. BlueprintUpdateAnimation보다 성능 좋음.
- Speed/Direction: 이동 블렌드스페이스 입력용
- AimPitch: 상체 Aim Offset용 (위아래 조준)
- 이 변수들을 AnimBP (블루프린트)에서 스테이트 머신 전환 조건으로 사용

---

### 2-4. AEPCharacter 확장

```cpp
// EPCharacter.h에 추가할 멤버들

// --- HP ---
UPROPERTY(ReplicatedUsing = OnRep_HP, BlueprintReadOnly, Category = "Combat")
float HP = 100.f;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
float MaxHP = 100.f;

UFUNCTION()
void OnRep_HP(float OldHP);

// --- 무기 ---
UPROPERTY(ReplicatedUsing = OnRep_EquippedWeapon, BlueprintReadOnly, Category = "Combat")
TObjectPtr<AEPWeapon> EquippedWeapon;

UFUNCTION()
void OnRep_EquippedWeapon();

// --- ADS ---
UPROPERTY(ReplicatedUsing = OnRep_IsAiming)
bool bIsAiming = false;

UFUNCTION()
void OnRep_IsAiming();

UFUNCTION(Server, Reliable)
void Server_SetAiming(bool bNewAiming);

UPROPERTY(EditDefaultsOnly, Category = "Combat")
float AimWalkSpeed = 200.f;

// --- Getter ---
bool GetIsSprinting() const { return bIsSprinting; }
bool GetIsAiming() const { return bIsAiming; }

// --- 전투 ---
// 서버에서 호출: 발사 요청 처리
UFUNCTION(Server, Reliable)
void Server_Fire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);

// 모든 클라이언트에서 실행: 발사 이펙트
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayFireEffect(FVector MuzzleLocation);

// 데미지 수신 (UE5 데미지 시스템)
virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser) override;

// 사망 처리 (서버)
void Die(AController* Killer);

// --- 앉기 ---
// ACharacter::Crouch()/UnCrouch() 사용 (CMC 내장)
void Input_Crouch(const FInputActionValue& Value);
void Input_UnCrouch(const FInputActionValue& Value);

// --- ADS ---
void Input_StartADS(const FInputActionValue& Value);
void Input_StopADS(const FInputActionValue& Value);

// --- 사격 ---
void Input_Fire(const FInputActionValue& Value);
```

---

### 2-5. AEPPlayerController 확장

```cpp
// EPPlayerController.h에 추가할 InputAction

UPROPERTY(EditDefaultsOnly, Category = "Input")
TObjectPtr<UInputAction> FireAction;

UPROPERTY(EditDefaultsOnly, Category = "Input")
TObjectPtr<UInputAction> AimAction;

UPROPERTY(EditDefaultsOnly, Category = "Input")
TObjectPtr<UInputAction> CrouchAction;

// Getter
FORCEINLINE UInputAction* GetFireAction() const { return FireAction; }
FORCEINLINE UInputAction* GetAimAction() const { return AimAction; }
FORCEINLINE UInputAction* GetCrouchAction() const { return CrouchAction; }
```

---

### 2-6. AEPGameMode 확장

```cpp
// EPGameMode.h에 추가

// 플레이어 사망 처리 (Character::Die에서 호출)
void OnPlayerKilled(AController* Killer, AController* Victim);
```

```cpp
// EPGameMode.cpp
void AEPGameMode::OnPlayerKilled(AController* Killer, AController* Victim)
{
    // 킬러의 KillCount 증가
    if (Killer && Killer != Victim)
    {
        if (AEPPlayerState* KillerPS = Killer->GetPlayerState<AEPPlayerState>())
        {
            KillerPS->AddKill();
        }

        // 킬러에게 킬 피드백 (Client RPC)
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

---

## 3. UEPWeaponData 수정

현재 `MaxAmmo` 필드가 누락됨. 추가 필요:

```cpp
// EPWeaponData.h - 수정
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
int32 MaxAmmo = 30;

// 현재 FireMode 위에 있는 잘못된 주석("최대 탄약") 수정
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
EEPFireMode FireMode = EEPFireMode::Auto;
```

---

## 4. 구현 순서

### Step 1: 앉기 (Crouch)

ACharacter의 Crouch()는 CMC가 네트워크를 자동 처리한다. 별도 복제 코드 불필요.

1. EPPlayerController에 `CrouchAction` 추가
2. EPCharacter::SetupPlayerInputComponent에서 바인딩
3. Input_Crouch → `Crouch()`, Input_UnCrouch → `UnCrouch()`
4. Constructor에서 `GetCharacterMovement()->NavAgentProps.bCanCrouch = true;`
5. **빌드 확인**

**테스트**: PIE에서 앉기 동작 확인. 2인 접속 시 상대 캐릭터가 앉는 것 보이는지 확인.

---

### Step 2: 무기 시스템 (기초)

무기 액터 생성 + 캐릭터에 장착 + 복제.

1. `Combat/EPWeapon.h/.cpp` 생성
   - Constructor: `bReplicates = true`, WeaponMesh 생성
   - `GetLifetimeReplicatedProps`: CurrentAmmo 등록
2. EPCharacter에 `EquippedWeapon` 추가 (ReplicatedUsing)
3. EPGameMode에서 플레이어 스폰 시 기본 무기 지급:
   ```cpp
   void AEPGameMode::PostLogin(APlayerController* NewPlayer)
   {
       Super::PostLogin(NewPlayer);
       // 캐릭터 스폰 후 무기 지급은 HandleStartingNewPlayer 또는 별도 타이밍
   }
   ```
4. 무기 장착: 캐릭터 메시의 소켓에 AttachToComponent
5. OnRep_EquippedWeapon: 클라이언트에서 무기 시각 업데이트
6. **빌드 확인**

**테스트**: PIE에서 캐릭터 손에 무기가 보이는지. 2인 접속 시 상대 무기도 보이는지.

---

### Step 3: 사격 (Server RPC + 이펙트)

서버 권한 히트스캔 구현.

1. EPPlayerController에 `FireAction` 추가
2. EPCharacter::Input_Fire → Server_Fire RPC 호출
   ```cpp
   void AEPCharacter::Input_Fire(const FInputActionValue& Value)
   {
       if (!EquippedWeapon) return;
       FVector Origin = FirstPersonCamera->GetComponentLocation();
       FVector Direction = FirstPersonCamera->GetForwardVector();
       Server_Fire(Origin, Direction);
   }
   ```
3. Server_Fire_Implementation:
   - 연사 속도 검증 (LastFireTime)
   - 탄약 검증
   - 서버 레이캐스트 (`LineTraceSingleByChannel`)
   - 히트 시 `ApplyDamage`
   - `Multicast_PlayFireEffect` 호출
4. Multicast_PlayFireEffect_Implementation:
   - 총구 이펙트 (파티클/사운드)
   - 탄착 이펙트
5. **빌드 확인**

**테스트**: PIE 2인에서 사격 → 로그로 히트 확인. 양쪽 모두 총구 이펙트 보이는지.

---

### Step 4: HP + 데미지

HP 복제 + 데미지 → 사망 감지.

1. EPCharacter에 `HP`, `MaxHP` 추가
   - `ReplicatedUsing = OnRep_HP`
   - `GetLifetimeReplicatedProps`에 등록
2. `TakeDamage` 오버라이드:
   ```cpp
   float AEPCharacter::TakeDamage(float DamageAmount,
       FDamageEvent const& DamageEvent,
       AController* EventInstigator, AActor* DamageCauser)
   {
       if (!HasAuthority()) return 0.f;

       float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent,
           EventInstigator, DamageCauser);

       HP = FMath::Clamp(HP - ActualDamage, 0.f, MaxHP);

       if (HP <= 0.f)
       {
           Die(EventInstigator);
       }

       return ActualDamage;
   }
   ```
3. `OnRep_HP`:
   ```cpp
   void AEPCharacter::OnRep_HP(float OldHP)
   {
       // 데미지 받았을 때 클라이언트 피드백 (카메라 흔들림 등)
       if (HP < OldHP)
       {
           // TODO: 피격 이펙트
       }
   }
   ```
4. **빌드 확인**

**테스트**: 사격으로 상대 HP 감소 확인 (로그). HP가 클라이언트에 복제되는지 확인.

---

### Step 5: 사망 + Corpse

사망 처리 흐름: Character::Die → Corpse 스폰 → GameMode 알림.

1. `Core/EPCorpse.h/.cpp` 생성
   - Constructor: `bReplicates = true`, CorpseMesh 생성
   - `InitializeFromCharacter`: 메시/위치 복사
   - `GetLifetimeReplicatedProps`: OwnerPlayerName 등록
2. EPCharacter::Die 구현:
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
       {
           Corpse->InitializeFromCharacter(this);
       }

       // GameMode에 사망 알림
       if (AEPGameMode* GM = GetWorld()->GetAuthGameMode<AEPGameMode>())
       {
           GM->OnPlayerKilled(Killer, GetController());
       }

       // 캐릭터 숨김 + 충돌 비활성화
       SetActorHiddenInGame(true);
       SetActorEnableCollision(false);
   }
   ```
3. **빌드 확인**

**테스트**: HP 0 도달 시 캐릭터 숨겨지고 Corpse 스폰 확인. 킬러의 KillCount 증가 확인.

---

### Step 6: ADS (에임)

Sprint와 동일한 패턴: Server RPC + ReplicatedUsing.

1. EPPlayerController에 `AimAction` 추가
2. EPCharacter:
   - `bIsAiming` (ReplicatedUsing = OnRep_IsAiming)
   - Input_StartADS → `Server_SetAiming(true)`
   - Input_StopADS → `Server_SetAiming(false)`
   - Server_SetAiming_Implementation: bIsAiming 설정, 이동속도 변경
   - OnRep_IsAiming: 이동속도 변경 (AimWalkSpeed)
3. Sprint 중 ADS 시 Sprint 해제 로직
4. FOV 변경 (로컬 클라이언트만):
   ```cpp
   // OnRep_IsAiming 또는 로컬 처리
   if (IsLocallyControlled())
   {
       float TargetFOV = bIsAiming ? 60.f : 90.f;
       FirstPersonCamera->SetFieldOfView(TargetFOV);
   }
   ```
5. **빌드 확인**

**테스트**: ADS 시 FOV 변경 확인. 2인 접속 시 상대가 ADS 상태인지 확인 (이동속도).

---

### Step 7: 애니메이션

C++ AnimInstance + 에디터에서 AnimBP 구성.

1. `Animation/EPAnimInstance.h/.cpp` 생성
   - NativeUpdateAnimation: Speed, Direction, bIsInAir, bIsCrouching, bIsSprinting, bIsAiming, AimPitch
2. 에디터에서 AnimBP 생성 (ABP_EPCharacter):
   - Parent: UEPAnimInstance
   - **Locomotion 스테이트 머신**:
     - Idle/Walk/Run: 블렌드스페이스 (Speed, Direction)
     - Sprint: Speed > SprintThreshold
     - Jump/Fall: bIsInAir
     - Crouch: bIsCrouching
   - **Aim Offset**: AimPitch로 상체 위아래 조준
   - **Fire 몽타주 슬롯**: 사격 시 상체 반동 애니메이션
3. EPCharacter의 Mesh에 AnimBP 할당 (Constructor 또는 BP)
4. 사격 시 몽타주 재생:
   ```cpp
   // Multicast_PlayFireEffect에서
   if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
   {
       Anim->Montage_Play(FireMontage);
   }
   ```
5. **빌드 확인**

**테스트**: 이동/점프/앉기 시 애니메이션 전환. 2인 접속 시 상대 애니메이션이 보이는지.

---

## 5. 복제 정리표

### EPCharacter

| 변수 | 복제 방식 | 조건 | 용도 |
|------|----------|------|------|
| bIsSprinting | ReplicatedUsing | COND_SkipOwner | 타인에게 달리기 상태 전달 |
| bIsAiming | ReplicatedUsing | COND_SkipOwner | 타인에게 ADS 상태 전달 |
| HP | ReplicatedUsing | COND_None | 모두에게 (체력바 표시) |
| EquippedWeapon | ReplicatedUsing | COND_None | 무기 시각 동기화 |

- `bIsSprinting`, `bIsAiming`에 `COND_SkipOwner`: 소유자는 로컬에서 즉시 처리, 타인만 복제 필요
- HP: 모두에게 복제. 타인의 체력바도 볼 수 있도록 (타르코프와 다른 선택 — 원하면 COND_OwnerOnly로 변경 가능)

### AEPWeapon

| 변수 | 복제 방식 | 조건 | 용도 |
|------|----------|------|------|
| CurrentAmmo | ReplicatedUsing | COND_OwnerOnly | 소유자만 탄약 확인 |

### RPC 정리

| RPC | 타입 | 방향 | 용도 |
|-----|------|------|------|
| Server_SetSprinting | Server, Reliable | 클라→서버 | Sprint 상태 변경 요청 |
| Server_SetAiming | Server, Reliable | 클라→서버 | ADS 상태 변경 요청 |
| Server_Fire | Server, Reliable | 클라→서버 | 발사 요청 (위치+방향) |
| Multicast_PlayFireEffect | NetMulticast, Unreliable | 서버→모두 | 총구 이펙트/사운드 |
| Client_OnKill | Client, Reliable | 서버→킬러 | 킬 피드백 |

---

## 6. 멀티플레이어 테스트

에디터 설정:
1. Play 드롭다운 → Net Mode: `Play As Client`
2. Number of Players: 2
3. `Run Dedicated Server` 체크

확인 항목:
- 상대 플레이어의 이동/앉기/달리기/ADS가 보이는가
- 사격 이펙트가 양쪽에서 보이는가
- 사격 시 상대 HP가 감소하는가
- HP 0 시 Corpse가 스폰되는가
- 킬러의 KillCount가 증가하는가

---

## 7. 완료 기준

- [ ] 앉기: Ctrl로 앉기/일어서기, 2인 접속 시 상대에게 보임
- [ ] 무기: 캐릭터에 무기 장착, 상대에게 보임
- [ ] 사격: 서버 권한 레이캐스트, 히트 시 데미지 적용
- [ ] 이펙트: 총구/탄착 이펙트가 모든 클라이언트에서 보임 (Multicast)
- [ ] HP: 복제됨, 피격 시 감소
- [ ] 사망: HP 0 → Corpse 스폰, 캐릭터 숨김
- [ ] KillCount: 킬러에게만 증가 (COND_OwnerOnly)
- [ ] ADS: 우클릭으로 에임, FOV 변경, 이동속도 감소
- [ ] AnimBP: 이동/점프/앉기/달리기 애니메이션 전환
- [ ] 사격 몽타주: 발사 시 반동 애니메이션

---

## 8. 주의사항

### Server_Fire에서 치트 방지
```cpp
// 기본 검증: 연사 속도, 탄약
bool AEPCharacter::Server_Fire_Validate(
    FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)
{
    // 빈 방향 벡터 거부
    return !Direction.IsNearlyZero();
}
```
- `WithValidation` 추가 시 `_Validate` 함수 필요
- false 반환 → 해당 클라이언트 강제 접속 해제
- Lag Compensation은 3단계에서 구현. 2단계에서는 서버 현재 시점 레이캐스트만.

### Crouch는 복제 코드 불필요
`ACharacter::Crouch()`를 호출하면 CMC가 자동으로:
1. 서버에서 앉기 상태 변경
2. `bIsCrouched` 복제
3. 캡슐 높이 조정
4. 클라이언트 동기화

### OnRep vs 로컬 즉시 처리
Sprint/ADS에서 소유 클라이언트는 OnRep을 기다리지 않고 로컬에서 즉시 처리:
```cpp
void AEPCharacter::Input_StartADS(const FInputActionValue& Value)
{
    // 로컬에서 즉시 적용 (반응성)
    bIsAiming = true;
    GetCharacterMovement()->MaxWalkSpeed = AimWalkSpeed;

    if (IsLocallyControlled())
        FirstPersonCamera->SetFieldOfView(60.f);

    // 서버에 알림 (서버 + 타인 동기화)
    Server_SetAiming(true);
}
```
이렇게 하면 입력 → 즉시 반응. 서버는 확인만.

### FVector_NetQuantize
```
FVector_NetQuantize: 위치 양자화 (소수점 1자리, 대역폭 절감)
FVector_NetQuantizeNormal: 방향 양자화 (16비트 각도)
```
일반 FVector 대비 네트워크 트래픽을 크게 줄인다. 사격 RPC에 필수.
