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
- Character: 1인칭 카메라, WASD 이동, 점프
- DataAsset: UEPWeaponData, UEPItemData

**Sprint 리팩토링 필요:**
현재 Sprint는 Server RPC + ReplicatedUsing 패턴으로 구현되어 있으나,
CMC 확장(SavedMove) 방식으로 전환한다. (CMC.md 참조)

이유: Server RPC 방식은 클라 예측과 서버 재시뮬레이션이 불일치하여 보정(스냅)이 잦아질 수 있다.
CMC 확장은 Sprint 상태를 이동 입력 패킷에 포함해, 예측/보정이 일치하도록 보장한다.

**ADS도 동일 패턴 적용:**
ADS(에임)도 이동속도를 변경하는 상태이므로 CMC 확장으로 처리한다. (FLAG_Custom_1)

---

## 1. Source 폴더 구조 (2단계 추가분)

```
Public/
├── Core/
│   ├── EPCharacter.h         ← 수정: HP, 무기, 앉기, 사망. Sprint/ADS → CMC로 이관
│   ├── EPPlayerController.h  ← 수정: Fire, Crouch, ADS InputAction 추가
│   ├── EPCorpse.h            ← 신규: 시체 액터
│   └── EPGameMode.h          ← 수정: 사망 처리 함수
├── Movement/
│   └── EPCharacterMovement.h ← 신규: CMC 확장 (Sprint + ADS)
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
├── Movement/
│   └── EPCharacterMovement.cpp
├── Combat/
│   └── EPWeapon.cpp
└── Animation/
    └── EPAnimInstance.cpp
```

---

## 2. 클래스 설계

### 2-1. UEPCharacterMovement (← UCharacterMovementComponent 상속)

**역할**: Sprint/ADS를 이동 예측 시스템에 통합. SavedMove로 클라-서버 동기화 보장.

```cpp
// EPCharacterMovement.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EPCharacterMovement.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API UEPCharacterMovement : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    // --- 이동 상태 플래그 (복제 안 함, CompressedFlags로 전송) ---
    uint8 bWantsToSprint : 1;
    uint8 bWantsToAim : 1;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float SprintSpeed = 650.f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float AimSpeed = 200.f;

    // --- CMC 오버라이드 ---
    virtual float GetMaxSpeed() const override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};
```

```cpp
// EPCharacterMovement.cpp
float UEPCharacterMovement::GetMaxSpeed() const
{
    if (bWantsToSprint && IsMovingOnGround())
        return SprintSpeed;
    if (bWantsToAim)
        return AimSpeed;
    return Super::GetMaxSpeed();
}

void UEPCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    bWantsToAim = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}
```

**SavedMove 확장** (같은 .cpp에):

```cpp
class FSavedMove_EPCharacter : public FSavedMove_Character
{
public:
    uint8 bSavedWantsToSprint : 1;
    uint8 bSavedWantsToAim : 1;

    virtual void Clear() override;
    virtual uint8 GetCompressedFlags() const override;
    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character,
                                float MaxDelta) const override;
    virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
                            FNetworkPredictionData_Client_Character& ClientData) override;
    virtual void PrepMoveFor(ACharacter* C) override;
};

// GetCompressedFlags
uint8 FSavedMove_EPCharacter::GetCompressedFlags() const
{
    uint8 Result = Super::GetCompressedFlags();
    if (bSavedWantsToSprint)
        Result |= FSavedMove_Character::FLAG_Custom_0;
    if (bSavedWantsToAim)
        Result |= FSavedMove_Character::FLAG_Custom_1;
    return Result;
}

// SetMoveFor - 현재 CMC 상태를 SavedMove에 저장
void FSavedMove_EPCharacter::SetMoveFor(ACharacter* C, float InDeltaTime,
    FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
    Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
    UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(C->GetCharacterMovement());
    bSavedWantsToSprint = CMC->bWantsToSprint;
    bSavedWantsToAim = CMC->bWantsToAim;
}

// PrepMoveFor - SavedMove에서 CMC로 복원 (리플레이 시)
void FSavedMove_EPCharacter::PrepMoveFor(ACharacter* C)
{
    Super::PrepMoveFor(C);
    UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(C->GetCharacterMovement());
    CMC->bWantsToSprint = bSavedWantsToSprint;
    CMC->bWantsToAim = bSavedWantsToAim;
}

// CanCombineWith - Sprint/ADS 상태가 다르면 합치지 않음
bool FSavedMove_EPCharacter::CanCombineWith(const FSavedMovePtr& NewMove,
    ACharacter* Character, float MaxDelta) const
{
    FSavedMove_EPCharacter* Other = static_cast<FSavedMove_EPCharacter*>(NewMove.Get());
    if (bSavedWantsToSprint != Other->bSavedWantsToSprint) return false;
    if (bSavedWantsToAim != Other->bSavedWantsToAim) return false;
    return Super::CanCombineWith(NewMove, Character, MaxDelta);
}
```

**Client Prediction Data** (같은 .cpp에):

```cpp
class FNetworkPredictionData_Client_EPCharacter : public FNetworkPredictionData_Client_Character
{
public:
    explicit FNetworkPredictionData_Client_EPCharacter(const UCharacterMovementComponent& ClientMovement)
        : FNetworkPredictionData_Client_Character(ClientMovement) {}

    virtual FSavedMovePtr AllocateNewMove() override
    {
        return FSavedMovePtr(new FSavedMove_EPCharacter());
    }
};

// CMC에서
FNetworkPredictionData_Client* UEPCharacterMovement::GetPredictionData_Client() const
{
    if (!ClientPredictionData)
    {
        UEPCharacterMovement* MutableThis = const_cast<UEPCharacterMovement*>(this);
        MutableThis->ClientPredictionData =
            new FNetworkPredictionData_Client_EPCharacter(*this);
    }
    return ClientPredictionData;
}
```

**핵심 포인트:**
- Sprint/ADS는 UPROPERTY 복제가 아니라, **CompressedFlags로 이동 패킷에 포함**
- 서버가 동일 입력으로 재시뮬레이션 → 클라 예측과 결과 일치
- FLAG_Custom_0 = Sprint, FLAG_Custom_1 = ADS (총 4개까지 사용 가능)
- Server RPC 불필요 (이동 시스템이 자체적으로 서버에 전달)

---

### 2-2. AEPCharacter 변경

**Sprint 리팩토링**: 기존 Server RPC + ReplicatedUsing 제거 → CMC로 이관.

```cpp
// EPCharacter.h - 제거할 것
// UPROPERTY(ReplicatedUsing=OnRep_IsSprinting) bool bIsSprinting;  ← 제거
// UFUNCTION(Server, Reliable) void Server_SetSprinting(bool);      ← 제거
// void OnRep_IsSprinting();                                        ← 제거

// EPCharacter.h - 변경/추가할 것

// Constructor에서 기본 CMC 대신 커스텀 CMC 사용
AEPCharacter(const FObjectInitializer& ObjectInitializer);

// --- Getter (CMC에서 읽기) ---
bool GetIsSprinting() const;
bool GetIsAiming() const;

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

// --- 전투 ---
UFUNCTION(Server, Reliable)
void Server_Fire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);

UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayFireEffect(FVector MuzzleLocation);

virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser) override;

void Die(AController* Killer);

// --- 입력 핸들러 ---
void Input_Crouch(const FInputActionValue& Value);
void Input_UnCrouch(const FInputActionValue& Value);
void Input_StartADS(const FInputActionValue& Value);
void Input_StopADS(const FInputActionValue& Value);
void Input_Fire(const FInputActionValue& Value);
```

**커스텀 CMC를 사용하는 Constructor**:
```cpp
// EPCharacter.cpp
AEPCharacter::AEPCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UEPCharacterMovement>(
        ACharacter::CharacterMovementComponentName))
{
    // 기존 Constructor 내용...
}
```

**Sprint/ADS 입력 → CMC 플래그 설정**:
```cpp
void AEPCharacter::Input_StartSprint(const FInputActionValue& Value)
{
    if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
    {
        CMC->bWantsToSprint = true;
    }
}

void AEPCharacter::Input_StopSprint(const FInputActionValue& Value)
{
    if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
    {
        CMC->bWantsToSprint = false;
    }
}

void AEPCharacter::Input_StartADS(const FInputActionValue& Value)
{
    if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
    {
        CMC->bWantsToAim = true;
        CMC->bWantsToSprint = false; // ADS 시 Sprint 해제
    }
    // FOV 변경 (로컬만)
    if (IsLocallyControlled())
        FirstPersonCamera->SetFieldOfView(60.f);
}

void AEPCharacter::Input_StopADS(const FInputActionValue& Value)
{
    if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
    {
        CMC->bWantsToAim = false;
    }
    if (IsLocallyControlled())
        FirstPersonCamera->SetFieldOfView(90.f);
}

// Getter
bool AEPCharacter::GetIsSprinting() const
{
    if (const UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
        return CMC->bWantsToSprint;
    return false;
}

bool AEPCharacter::GetIsAiming() const
{
    if (const UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
        return CMC->bWantsToAim;
    return false;
}
```

---

### 2-3. AEPWeapon (← AActor 상속)

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<UEPWeaponData> WeaponData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TObjectPtr<USkeletalMeshComponent> WeaponMesh;

    void Fire(FVector Origin, FVector Direction);

    UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo)
    int32 CurrentAmmo;

    UFUNCTION()
    void OnRep_CurrentAmmo();

protected:
    float LastFireTime = 0.f;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

---

### 2-4. AEPCorpse (← AActor 상속)

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
    void InitializeFromCharacter(AEPCharacter* DeadCharacter);

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> CorpseMesh;

    UPROPERTY(Replicated)
    FString OwnerPlayerName;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

---

### 2-5. UEPAnimInstance (← UAnimInstance 상속)

**역할**: AnimBP의 C++ 베이스. CMC에서 Sprint/ADS 상태를 읽는다.

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

    FVector Velocity = Owner->GetVelocity();
    Speed = Velocity.Size2D();

    FRotator AimRotation = Owner->GetBaseAimRotation();
    FRotator MovementRotation = Velocity.ToOrientationRotator();
    Direction = FRotator::NormalizeAxis(MovementRotation.Yaw - AimRotation.Yaw);

    if (ACharacter* Char = Cast<ACharacter>(Owner))
    {
        bIsInAir = Char->GetCharacterMovement()->IsFalling();
        bIsCrouching = Char->bIsCrouched;

        // CMC에서 Sprint/ADS 상태 읽기
        if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(Char->GetCharacterMovement()))
        {
            bIsSprinting = CMC->bWantsToSprint;
            bIsAiming = CMC->bWantsToAim;
        }
    }

    AimPitch = AimRotation.Pitch;
}
```

**핵심**: Sprint/ADS는 Character가 아닌 **CMC에서 직접 읽음**. 서버/클라 모두 CMC에 상태가 있으므로 AnimBP도 정확히 동기화됨.

---

### 2-6. AEPPlayerController 확장

```cpp
// EPPlayerController.h에 추가
UPROPERTY(EditDefaultsOnly, Category = "Input")
TObjectPtr<UInputAction> FireAction;

UPROPERTY(EditDefaultsOnly, Category = "Input")
TObjectPtr<UInputAction> AimAction;

UPROPERTY(EditDefaultsOnly, Category = "Input")
TObjectPtr<UInputAction> CrouchAction;

FORCEINLINE UInputAction* GetFireAction() const { return FireAction; }
FORCEINLINE UInputAction* GetAimAction() const { return AimAction; }
FORCEINLINE UInputAction* GetCrouchAction() const { return CrouchAction; }
```

---

### 2-7. AEPGameMode 확장

```cpp
// EPGameMode.h에 추가
void OnPlayerKilled(AController* Killer, AController* Victim);
```

```cpp
// EPGameMode.cpp
void AEPGameMode::OnPlayerKilled(AController* Killer, AController* Victim)
{
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

---

## 3. UEPWeaponData 수정

현재 `MaxAmmo` 필드가 누락됨. 추가 필요:

```cpp
// EPWeaponData.h - 추가
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
int32 MaxAmmo = 30;

// 기존 FireMode 위의 잘못된 주석("최대 탄약") 수정
```

---

## 4. 구현 순서

### Step 1: CMC 확장 + Sprint 리팩토링

기존 Server RPC 방식을 CMC SavedMove 방식으로 전환.

1. `Movement/EPCharacterMovement.h/.cpp` 생성
   - UEPCharacterMovement: bWantsToSprint, bWantsToAim, GetMaxSpeed, UpdateFromCompressedFlags
   - FSavedMove_EPCharacter: GetCompressedFlags, SetMoveFor, PrepMoveFor, CanCombineWith
   - FNetworkPredictionData_Client_EPCharacter: AllocateNewMove
2. EPCharacter Constructor를 `FObjectInitializer` 방식으로 변경
   - `SetDefaultSubobjectClass<UEPCharacterMovement>(CharacterMovementComponentName)`
3. EPCharacter에서 기존 Sprint 복제 코드 제거:
   - `UPROPERTY(ReplicatedUsing=OnRep_IsSprinting) bIsSprinting` → 제거
   - `UFUNCTION(Server, Reliable) Server_SetSprinting` → 제거
   - `OnRep_IsSprinting` → 제거
   - `GetLifetimeReplicatedProps`에서 `bIsSprinting` 제거
4. Input_StartSprint/StopSprint → CMC의 bWantsToSprint 설정으로 변경
5. WalkSpeed/SprintSpeed → CMC로 이관
6. **빌드 확인**

**테스트**: PIE 2인에서 Sprint 동작 확인. 서버 보정 시 스냅이 줄었는지 확인.

---

### Step 2: 앉기 (Crouch)

ACharacter의 Crouch()는 CMC가 네트워크를 자동 처리한다.

1. EPPlayerController에 `CrouchAction` 추가
2. EPCharacter::SetupPlayerInputComponent에서 바인딩
3. Input_Crouch → `Crouch()`, Input_UnCrouch → `UnCrouch()`
4. Constructor에서 `GetCharacterMovement()->NavAgentProps.bCanCrouch = true;`
5. **빌드 확인**

**테스트**: 앉기 동작 확인. 2인 접속 시 상대에게 보이는지.

---

### Step 3: 무기 시스템 (기초)

1. `Combat/EPWeapon.h/.cpp` 생성
   - Constructor: `bReplicates = true`, WeaponMesh 생성
   - `GetLifetimeReplicatedProps`: CurrentAmmo (COND_OwnerOnly)
2. EPCharacter에 `EquippedWeapon` 추가 (ReplicatedUsing)
3. 무기 장착: 캐릭터 메시 소켓에 AttachToComponent
4. OnRep_EquippedWeapon: 클라이언트에서 무기 시각 업데이트
5. EPWeaponData에 MaxAmmo 필드 추가
6. **빌드 확인**

**테스트**: 캐릭터 손에 무기 보임. 2인 접속 시 상대 무기도 보임.

---

### Step 4: 사격 (Server RPC + 이펙트)

서버 권한 히트스캔.

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
   - 연사 속도/탄약 검증
   - 서버 레이캐스트 (`LineTraceSingleByChannel`)
   - 히트 시 `ApplyDamage`
   - `Multicast_PlayFireEffect` 호출
4. Multicast_PlayFireEffect_Implementation: 총구/탄착 이펙트
5. **빌드 확인**

**테스트**: 2인에서 사격 → 히트 로그 확인. 양쪽 이펙트 확인.

---

### Step 5: HP + 데미지

1. EPCharacter에 `HP`, `MaxHP` 추가 (ReplicatedUsing = OnRep_HP)
2. TakeDamage 오버라이드:
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
           Die(EventInstigator);
       return ActualDamage;
   }
   ```
3. OnRep_HP: 클라이언트 피격 피드백
4. **빌드 확인**

**테스트**: 사격으로 HP 감소 확인. 클라이언트 복제 확인.

---

### Step 6: 사망 + Corpse

1. `Core/EPCorpse.h/.cpp` 생성
   - Constructor: `bReplicates = true`
   - InitializeFromCharacter: 메시/위치 복사
2. EPCharacter::Die 구현:
   ```cpp
   void AEPCharacter::Die(AController* Killer)
   {
       if (!HasAuthority()) return;

       FActorSpawnParameters Params;
       Params.SpawnCollisionHandlingOverride =
           ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
       AEPCorpse* Corpse = GetWorld()->SpawnActor<AEPCorpse>(
           AEPCorpse::StaticClass(), GetActorTransform(), Params);
       if (Corpse)
           Corpse->InitializeFromCharacter(this);

       if (AEPGameMode* GM = GetWorld()->GetAuthGameMode<AEPGameMode>())
           GM->OnPlayerKilled(Killer, GetController());

       SetActorHiddenInGame(true);
       SetActorEnableCollision(false);
   }
   ```
3. **빌드 확인**

**테스트**: HP 0 → Corpse 스폰, 캐릭터 숨김, KillCount 증가.

---

### Step 7: ADS (에임)

CMC 확장으로 이미 FLAG_Custom_1에 ADS가 포함됨. 입력 바인딩 + FOV만 추가.

1. EPPlayerController에 `AimAction` 추가
2. EPCharacter에 Input_StartADS/StopADS 바인딩:
   - CMC의 `bWantsToAim` 설정
   - Sprint 중 ADS → Sprint 해제 (`bWantsToSprint = false`)
3. FOV 변경 (로컬만):
   ```cpp
   if (IsLocallyControlled())
       FirstPersonCamera->SetFieldOfView(bAiming ? 60.f : 90.f);
   ```
4. **빌드 확인**

**테스트**: ADS 시 속도 감소 + FOV 변경. 2인에서 상대 속도 변화 확인.

---

### Step 8: 애니메이션

1. `Animation/EPAnimInstance.h/.cpp` 생성
   - NativeUpdateAnimation: CMC에서 Sprint/ADS 읽기
2. 에디터에서 AnimBP 생성 (ABP_EPCharacter):
   - Parent: UEPAnimInstance
   - **Locomotion**: 블렌드스페이스 (Speed, Direction)
   - **Sprint**: bIsSprinting 조건
   - **Jump/Fall**: bIsInAir
   - **Crouch**: bIsCrouching
   - **Aim Offset**: AimPitch
   - **Fire 몽타주**: 상체 반동
3. EPCharacter Mesh에 AnimBP 할당
4. 사격 시 몽타주:
   ```cpp
   // Multicast_PlayFireEffect에서
   if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
       Anim->Montage_Play(FireMontage);
   ```
5. **빌드 확인**

**테스트**: 이동/점프/앉기/달리기 애니메이션 전환. 2인에서 상대 애니메이션 동기화.

---

## 5. 복제/동기화 정리표

### EPCharacter

| 데이터 | 동기화 방식 | 조건 | 용도 |
|--------|------------|------|------|
| Sprint 상태 | **CMC CompressedFlags** (FLAG_Custom_0) | 이동 패킷에 포함 | 속도 변경 + 예측 일치 |
| ADS 상태 | **CMC CompressedFlags** (FLAG_Custom_1) | 이동 패킷에 포함 | 속도 변경 + 예측 일치 |
| Crouch 상태 | **CMC 내장** (bIsCrouched) | 자동 복제 | 캡슐 높이 + 속도 |
| HP | UPROPERTY ReplicatedUsing | COND_None | 모두에게 (체력바) |
| EquippedWeapon | UPROPERTY ReplicatedUsing | COND_None | 무기 시각 동기화 |

### AEPWeapon

| 데이터 | 동기화 방식 | 조건 | 용도 |
|--------|------------|------|------|
| CurrentAmmo | UPROPERTY ReplicatedUsing | COND_OwnerOnly | 소유자만 탄약 확인 |

### RPC 정리

| RPC | 타입 | 용도 |
|-----|------|------|
| Server_Fire | Server, Reliable | 발사 요청 (위치+방향) |
| Multicast_PlayFireEffect | NetMulticast, Unreliable | 총구/탄착 이펙트 |
| Client_OnKill | Client, Reliable | 킬 피드백 |

**Server_SetSprinting / Server_SetAiming은 삭제됨** — CMC가 이동 패킷으로 자체 처리.

---

## 6. Server RPC vs CMC 확장 비교

| | Server RPC 방식 (기존) | CMC 확장 방식 (변경) |
|--|----------------------|---------------------|
| 동기화 | 별도 RPC + UPROPERTY 복제 | 이동 패킷에 포함 (자동) |
| 클라 예측 | 불일치 가능 (보정 잦음) | 일치 (SavedMove에 상태 포함) |
| 코드량 | 적음 | 많음 (SavedMove 확장) |
| 대역폭 | RPC + Replicated 변수 | CompressedFlags 비트만 추가 |
| 적합한 상황 | 비이동 상태 (무기 교체 등) | **이동속도에 영향 주는 상태 (Sprint, ADS, Crouch)** |

---

## 7. 멀티플레이어 테스트

에디터 설정:
1. Play 드롭다운 → Net Mode: `Play As Client`
2. Number of Players: 2
3. `Run Dedicated Server` 체크

확인 항목:
- Sprint/ADS 시 상대의 이동속도가 변하는가 (CMC 동기화)
- Sprint/ADS 전환 시 스냅/튐이 없는가 (예측 일치)
- 앉기가 상대에게 보이는가 (CMC 내장)
- 사격 이펙트가 양쪽에서 보이는가
- HP 감소 + 사망 + Corpse 정상 작동

---

## 8. 완료 기준

- [ ] CMC 확장: Sprint/ADS가 SavedMove 기반으로 동기화됨
- [ ] Sprint 전환 시 멀티에서 스냅/튐 없음
- [ ] 앉기: Ctrl로 앉기/일어서기, 상대에게 보임
- [ ] 무기: 캐릭터에 무기 장착, 상대에게 보임
- [ ] 사격: 서버 권한 레이캐스트, 히트 시 데미지 적용
- [ ] 이펙트: 총구/탄착 이펙트가 모든 클라이언트에서 보임
- [ ] HP: 복제됨, 피격 시 감소
- [ ] 사망: HP 0 → Corpse 스폰, 캐릭터 숨김
- [ ] KillCount: 킬러에게만 증가 (COND_OwnerOnly)
- [ ] ADS: CMC 기반 속도 감소 + FOV 변경
- [ ] AnimBP: 이동/점프/앉기/달리기/ADS 애니메이션 전환
- [ ] 사격 몽타주: 발사 시 반동 애니메이션

---

## 9. 주의사항

### FObjectInitializer Constructor
커스텀 CMC를 사용하려면 Character Constructor 시그니처가 변경됨:
```cpp
// 기존
AEPCharacter::AEPCharacter() { ... }

// 변경 (FObjectInitializer 사용)
AEPCharacter::AEPCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UEPCharacterMovement>(
        ACharacter::CharacterMovementComponentName))
{
    // 기존 내용 동일
}
```
이것이 CMC 교체의 유일한 방법. `CreateDefaultSubobject`로는 이미 부모가 생성한 CMC를 교체할 수 없다.

### FLAG_Custom 한계
`FSavedMove_Character`에 `FLAG_Custom_0` ~ `FLAG_Custom_3` 총 4개만 있다.
Sprint + ADS로 2개 사용. 나머지 2개는 향후 확장용으로 예약.

### Crouch는 CMC 확장 불필요
Crouch는 CMC에 이미 내장되어 있다. `bCanCrouch = true` 설정 후 `Crouch()/UnCrouch()` 호출만 하면
서버 동기화, 예측, 캡슐 높이 조정이 자동으로 처리된다.

### Server_Fire는 유지
사격은 이동과 무관한 이벤트이므로 Server RPC가 적절하다.
CMC 확장은 **이동속도에 영향을 주는 상태**에만 사용.

### FVector_NetQuantize
```
FVector_NetQuantize: 위치 양자화 (소수점 1자리, 대역폭 절감)
FVector_NetQuantizeNormal: 방향 양자화 (16비트 각도)
```
사격 RPC 파라미터에 사용하여 네트워크 트래픽 절감.
