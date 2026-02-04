# 2단계: Replication + Movement Component

## 1. Replication 개요

Replication은 서버의 게임 상태를 클라이언트에 동기화하는 UE5의 핵심 네트워크 메커니즘.
서버가 유일한 진실(Authority)이고, 클라이언트는 서버가 보내주는 데이터를 받아 표시한다.

### 핵심 특성
- **서버 → 클라이언트 단방향** (프로퍼티 복제)
- **RPC는 양방향** (클라→서버, 서버→클라, 서버→전체)
- **델타 압축**: 변경된 프로퍼티만 전송
- **즉시가 아님**: NetUpdateFrequency에 따라 주기적으로 전송

---

## 2. Actor Replication 활성화

Actor가 복제되려면 세 가지 조건이 모두 충족되어야 한다:

1. `bReplicates = true` (생성자에서 설정)
2. 클라이언트에 대해 **Net Relevant** (거리 내, 또는 bAlwaysRelevant)
3. 변경된 복제 프로퍼티 또는 대기 중인 RPC가 존재

```cpp
AMyActor::AMyActor()
{
    bReplicates = true;
    bAlwaysRelevant = false;
    NetUpdateFrequency = 100.f;    // 초당 최대 업데이트 횟수
    MinNetUpdateFrequency = 2.f;   // 부하 시 최소 횟수
}
```

### 기본적으로 복제되는 클래스
| 클래스 | 복제 대상 |
|--------|----------|
| APlayerController | 소유 클라이언트에만 |
| APlayerState | 모든 클라이언트 |
| AGameStateBase | 모든 클라이언트 |
| ACharacter | 모든 클라이언트 (bReplicates 기본 true) |
| AGameModeBase | 복제 안 됨 (서버 전용) |

---

## 3. 프로퍼티 복제

### GetLifetimeReplicatedProps 등록

복제할 프로퍼티는 반드시 이 함수에서 등록해야 한다:

```cpp
#include "Net/UnrealNetwork.h"

void AMyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); // 반드시 호출

    DOREPLIFETIME(AMyCharacter, Health);
    DOREPLIFETIME_CONDITION(AMyCharacter, AmmoCount, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(AMyCharacter, bIsSprinting, COND_SkipOwner);
}
```

### DOREPLIFETIME 조건

| 조건 | 동작 |
|------|------|
| `COND_None` | 모든 클라이언트에 항상 복제 (기본) |
| `COND_InitialOnly` | 최초 1회만 복제. 이후 변경 안 보냄 (팀 색상 등) |
| `COND_OwnerOnly` | 소유 클라이언트에만 복제 (탄약, 쿨타임 등 개인 데이터) |
| `COND_SkipOwner` | 소유자 제외 전체에 복제 (3인칭 애니 상태 등) |
| `COND_SimulatedOnly` | SimulatedProxy인 클라이언트에만 |
| `COND_AutonomousOnly` | AutonomousProxy(조작 중인 클라)에만 |
| `COND_Custom` | PreReplication()에서 연결별로 수동 결정 |

### Replicated vs ReplicatedUsing

**Replicated** - 값이 복제되지만 콜백 없음:
```cpp
UPROPERTY(Replicated)
int32 Score;
```

**ReplicatedUsing** - 값 복제 + 클라이언트에서 변경 시 콜백 호출:
```cpp
UPROPERTY(ReplicatedUsing = OnRep_Health)
float Health;

UFUNCTION()
void OnRep_Health();
// 또는 이전 값을 받을 수 있음:
void OnRep_Health(float OldHealth);
```

### OnRep 콜백 규칙
- **클라이언트에서만 호출됨** (서버에서는 호출 안 됨)
- 새 값이 이미 프로퍼티에 쓰여진 후 호출됨
- 같은 배치의 모든 프로퍼티가 업데이트된 후 OnRep들이 호출됨
- 서버에서도 반응이 필요하면 별도로 로직 호출해야 함

### 완전한 패턴 예시

```cpp
// Header
UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Combat")
float Health = 100.f;

UFUNCTION()
void OnRep_Health(float OldHealth);

// Source
void AMyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMyCharacter, Health);
}

void AMyCharacter::OnRep_Health(float OldHealth)
{
    float Delta = Health - OldHealth;
    if (Delta < 0.f)
    {
        ShowDamageNumber(FMath::Abs(Delta));
    }
    UpdateHealthBar(Health);
}
```

---

## 4. RPC (Remote Procedure Call)

프로퍼티 복제가 자동/주기적이라면, RPC는 **이벤트 기반**으로 1회 실행.

### Server RPC (클라 → 서버)

```cpp
// Header
UFUNCTION(Server, Reliable, WithValidation)
void Server_Fire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);

// Source - 구현 함수는 _Implementation 접미사
void AMyCharacter::Server_Fire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)
{
    // 서버에서 실행됨
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 10000.f, ECC_Visibility))
    {
        ApplyDamage(Hit.GetActor(), 25.f);
    }
}

// 검증 함수 - false 반환 시 해당 클라이언트 강제 접속 해제
bool AMyCharacter::Server_Fire_Validate(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)
{
    return !Origin.IsZero();
}
```

### Client RPC (서버 → 소유 클라)

```cpp
UFUNCTION(Client, Reliable)
void Client_ShowHitMarker(float Damage);

void AMyCharacter::Client_ShowHitMarker_Implementation(float Damage)
{
    // 소유 클라이언트에서만 실행
    SpawnHitMarkerWidget(Damage);
}
```

### NetMulticast RPC (서버 → 서버 + 모든 클라)

```cpp
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayFireEffect(FVector MuzzleLocation);

void AMyCharacter::Multicast_PlayFireEffect_Implementation(FVector MuzzleLocation)
{
    // 서버 + 모든 클라이언트에서 실행
    UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, MuzzleLocation);
    UGameplayStatics::PlaySoundAtLocation(GetWorld(), FireSound, MuzzleLocation);
}
```

### Reliable vs Unreliable

| | Reliable | Unreliable |
|--|----------|------------|
| 전달 보장 | O (재전송) | X (유실 가능) |
| 순서 보장 | O (같은 Actor 내) | X |
| 대역폭 | 높음 | 낮음 |
| 사용처 | 게임 로직 (발사, 아이템 획득, 상태 변경) | 이펙트 (총구 섬광, 발소리, 파티클) |

**주의: Reliable RPC를 매 프레임 호출하면 버퍼 오버플로로 클라이언트 연결 끊김.**
고빈도 데이터는 프로퍼티 복제 사용.

### RPC 실행 정리

| RPC 타입 | 호출 위치 | 실행 위치 | 사용 예시 |
|----------|----------|----------|----------|
| Server | 소유 클라 | 서버 | 발사, 상호작용, 구매 |
| Client | 서버 | 소유 클라만 | 히트마커, UI 메시지, 개인 피드백 |
| NetMulticast | 서버 | 서버 + 모든 클라 | 폭발, 사운드, 애니메이션 |

---

## 5. Authority와 Role

### HasAuthority()

```cpp
if (HasAuthority())
{
    // 서버에서 실행 중 (또는 스탠드얼론)
    Health -= Damage;
}
```

### ENetRole

모든 복제 Actor에는 두 가지 Role이 있다:

| Role | 의미 |
|------|------|
| ROLE_Authority | 이 머신이 이 Actor의 권한자 (서버에서 모든 Actor) |
| ROLE_AutonomousProxy | 로컬 플레이어가 직접 조작하는 Pawn (소유 클라) |
| ROLE_SimulatedProxy | 다른 플레이어의 Pawn (비소유 클라) |

**3인 게임 예시 (데디케이티드 서버):**

| 머신 | Player A Pawn | Player B Pawn |
|------|--------------|--------------|
| 서버 | Authority | Authority |
| 클라 A | AutonomousProxy | SimulatedProxy |
| 클라 B | SimulatedProxy | AutonomousProxy |

### Role 기반 분기 패턴

```cpp
if (HasAuthority())
{
    // 서버: AI 실행, 데미지 적용, 아이템 스폰
}
if (IsLocallyControlled())
{
    // 소유 클라: 크로스헤어, 1인칭 모델, 입력 처리
}
if (GetLocalRole() == ROLE_SimulatedProxy)
{
    // 비소유 클라: 위치 보간, 애니메이션 스무딩
}
```

---

## 6. Net Relevancy / Priority / Update Frequency

### Relevancy (관련성)
- `bAlwaysRelevant = true`: 항상 모든 클라에 복제 (GameState, PlayerState)
- `bOnlyRelevantToOwner = true`: 소유 연결에만 (PlayerController)
- `NetCullDistanceSquared`: 거리 기반 컬링 (기본값 매우 큼)
- 비관련 Actor는 클라에서 파괴됨. 다시 관련되면 새로 초기 복제.

### Priority
```cpp
// 기본 Actor: 1.0, PlayerController/Pawn: 3.0
// 실제 우선순위 = NetPriority * 마지막 업데이트 이후 시간
```

### 강제 업데이트
```cpp
ForceNetUpdate(); // 다음 네트워크 틱에서 즉시 복제
```

---

## 7. CharacterMovementComponent 네트워킹

### 자동 처리되는 것
CMC는 이동 예측-검증-보정을 자동으로 처리한다:

1. **클라이언트 예측**: 매 프레임 이동 시뮬레이션 실행, FSavedMove에 입력 저장
2. **서버 검증**: 클라이언트 이동 입력을 받아 서버에서 재실행
3. **서버 보정**: 결과가 다르면 클라이언트에 보정 전송
4. **클라이언트 리플레이**: 보정 수신 시, 보정 위치에서 미확인 입력을 재실행

### CMC 확장 (Sprint 예시)

Sprint 같은 커스텀 상태를 네트워크에서 올바르게 처리하려면 FSavedMove를 확장해야 한다:

```cpp
// 커스텀 CMC
UCLASS()
class UMyCharacterMovement : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    uint8 bWantsToSprint : 1;

    virtual float GetMaxSpeed() const override
    {
        if (bWantsToSprint && IsMovingOnGround())
            return MaxWalkSpeed * 1.5f;
        return Super::GetMaxSpeed();
    }

    virtual void UpdateFromCompressedFlags(uint8 Flags) override
    {
        Super::UpdateFromCompressedFlags(Flags);
        bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    }

    virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};
```

```cpp
// 커스텀 SavedMove
class FSavedMove_MyCharacter : public FSavedMove_Character
{
public:
    uint8 bSavedWantsToSprint : 1;

    virtual void Clear() override;
    virtual uint8 GetCompressedFlags() const override;
    virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
                                float MaxDelta) const override;
    virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
                            FNetworkPredictionData_Client_Character& ClientData) override;
    virtual void PrepMoveFor(ACharacter* C) override;
};
```

### SimulatedProxy 스무딩
다른 플레이어(SimulatedProxy)는 보간/추출로 부드럽게 표시:
- `NetworkSmoothingMode`: Disabled, Linear, Exponential (기본)
- 캡슐은 즉시 이동, 메시 오프셋으로 시각적 스무딩

---

## 8. EmploymentProj 2단계 구현 체크리스트

- [ ] Character에 `bReplicates = true`, `SetReplicateMovement(true)` 확인
- [ ] 이동 구현 (CMC 기본: 걷기, 점프)
  - [ ] 달리기: CMC 확장 또는 `MaxWalkSpeed` 변경 + 상태 복제
  - [ ] 앉기: `Crouch()`/`UnCrouch()` (CMC 기본 지원)
  - [ ] 에임(ADS): 이동속도 감소 상태 복제
- [ ] 복제 프로퍼티 구현
  - [ ] `bIsSprinting` : `ReplicatedUsing=OnRep_IsSprinting`
  - [ ] `EquippedWeapon` : `Replicated`
  - [ ] `HP` : `ReplicatedUsing=OnRep_HP` (UI 갱신용)
- [ ] 사격 RPC 기본 구현
  - [ ] `Server_Fire()` : 서버에서 레이캐스트
  - [ ] `Multicast_PlayFireEffect()` : 총구 이펙트/사운드
- [ ] 1인칭 카메라 설정
- [ ] 멀티플레이어 테스트 (PIE 2인)

---

## 9. 참고 자료

### 공식 문서
- Networking Overview: `dev.epicgames.com/documentation/en-us/unreal-engine/networking-overview-for-unreal-engine`
- Property Replication: `dev.epicgames.com/documentation/en-us/unreal-engine/property-replication-in-unreal-engine`
- RPCs: `dev.epicgames.com/documentation/en-us/unreal-engine/rpcs-in-unreal-engine`
- Actor Role and Authority: `dev.epicgames.com/documentation/en-us/unreal-engine/actor-role-and-remoterole-in-unreal-engine`

### UE5 소스 코드
- `Engine/Source/Runtime/Engine/Public/Net/UnrealNetwork.h` - DOREPLIFETIME 매크로
- `Engine/Source/Runtime/Engine/Classes/GameFramework/CharacterMovementComponent.h`
- `Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h` - bReplicates, HasAuthority
- `Engine/Source/Runtime/Engine/Classes/Engine/ActorChannel.h` - 복제 채널

### 검색 키워드
- "UE5 property replication tutorial"
- "UE5 RPC Server Client NetMulticast"
- "UE5 CharacterMovementComponent networking custom"
- "UE5 DOREPLIFETIME conditions"
- "UE5 OnRep callback pattern"
- "UE5 extend CMC SavedMove sprint"
