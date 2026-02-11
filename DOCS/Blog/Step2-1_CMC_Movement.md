# [UE5 C++] 2단계-1: CMC 확장으로 Sprint/ADS/Crouch 네트워크 동기화

> 02_Implementation Step 1 (CMC 확장 + Sprint 리팩토링), Step 2 (Crouch), Step 7 (ADS)

---

## 1. 이번 글에서 다루는 것

- 기존 Server RPC Sprint를 CMC 확장(SavedMove)으로 리팩토링한 이유와 과정
- UEPCharacterMovement: 커스텀 CMC 클래스 구현
- FSavedMove_EPCharacter: 이동 입력 저장/복원/압축
- FNetworkPredictionData_Client_EPCharacter: 커스텀 SavedMove 할당
- FObjectInitializer로 기본 CMC 교체
- Sprint / ADS / Crouch 입력 처리
- 네트워크 흐름 (클라 예측 → 서버 검증 → 보정 리플레이)

---

## 2. 왜 Server RPC가 아닌 CMC 확장인가

### Server RPC 방식 (기존)

<!--
  기존에 Sprint를 어떻게 구현했는지 설명:
  - Server_SetSprinting RPC + bIsSprinting (ReplicatedUsing)
  - 문제: RPC 왕복 동안 클라-서버 속도 불일치 → 위치 어긋남 → 서버 보정 스냅
-->

```
클라: Sprint 입력 → Server_SetSprinting RPC → 서버 bIsSprinting=true → 복제 → 클라 OnRep
문제: RPC 도착 전까지 클라는 빠르게 달리는데 서버는 걷는 속도 → 위치 불일치 → 보정 스냅
```

### CMC 확장 방식 (변경)

<!--
  CMC 확장이 이 문제를 어떻게 해결하는지:
  - Sprint 플래그가 이동 패킷(CompressedFlags)에 포함
  - 서버가 같은 플래그로 재시뮬레이션 → 예측 일치 → 스냅 최소화
-->

```
클라: Sprint 입력 → CMC.bWantsToSprint=true → 이동 패킷에 FLAG_Custom_0 포함 → 서버 즉시 반영
서버 재시뮬레이션 시에도 같은 플래그 사용 → 예측-검증 일치 → 스냅 없음
```

### 비교표

| | Server RPC 방식 | CMC 확장 방식 |
|--|----------------|--------------|
| 동기화 | 별도 RPC + UPROPERTY 복제 | 이동 패킷에 포함 (자동) |
| 클라 예측 | 불일치 가능 (보정 잦음) | 일치 (SavedMove에 상태 포함) |
| 코드량 | 적음 | 많음 (SavedMove 확장 필요) |
| 대역폭 | RPC + Replicated 변수 | CompressedFlags 비트만 추가 |
| 적합한 상황 | 비이동 상태 (무기 교체 등) | **이동속도에 영향 주는 상태** |

---

## 3. 전체 구조 개요

### 클래스 관계

| 클래스 | 역할 |
|--------|------|
| `UEPCharacterMovement` | CMC 확장. Sprint/ADS 속도 제어, CompressedFlags 수신 |
| `FSavedMove_EPCharacter` | 매 프레임 이동 입력 저장/복원. 플래그를 CompressedFlags로 압축 |
| `FNetworkPredictionData_Client_EPCharacter` | 커스텀 SavedMove 인스턴스 생성 |
| `AEPCharacter` | FObjectInitializer로 기본 CMC 교체. 입력 → CMC 플래그 설정 |

### 호출 관계도

```
AEPCharacter (FObjectInitializer)
  └─ UEPCharacterMovement (기본 CMC 교체)
       ├─ GetMaxSpeed()               ← Sprint/ADS 속도 분기
       ├─ UpdateFromCompressedFlags() ← 서버에서 플래그 복원
       └─ GetPredictionData_Client()  ← 커스텀 SavedMove 연결
            └─ FNetworkPredictionData_Client_EPCharacter
                 └─ AllocateNewMove() → FSavedMove_EPCharacter
                      ├─ SetMoveFor()         ← CMC → SavedMove 저장
                      ├─ GetCompressedFlags() ← SavedMove → 비트 압축
                      ├─ PrepMoveFor()        ← SavedMove → CMC 복원
                      ├─ CanCombineWith()     ← 동일 입력 합치기
                      └─ Clear()              ← 초기화
```

---

## 4. UEPCharacterMovement — 커스텀 CMC

### 4-1. 헤더 (EPCharacterMovement.h)

```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPCharacterMovement : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    // CompressedFlags로 전송 (UPROPERTY 복제 아님)
    uint8 bWantsToSprint : 1;
    uint8 bWantsToAim : 1;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float SprintSpeed = 650.f;

    UPROPERTY(EditDefaultsOnly, Category = "Movement")
    float AimSpeed = 200.f;

    virtual float GetMaxSpeed() const override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};
```

<!--
  핵심:
  - bWantsToSprint/bWantsToAim은 UPROPERTY가 아님 → 프로퍼티 복제되지 않음
  - CompressedFlags(이동 패킷 비트필드)를 통해 전송
  - uint8 : 1 비트필드 → FLAG_Custom과 1:1 대응
-->

### 4-2. GetMaxSpeed()

```cpp
float UEPCharacterMovement::GetMaxSpeed() const
{
    if (bWantsToSprint && IsMovingOnGround()) return SprintSpeed;
    if (bWantsToAim) return AimSpeed;
    return Super::GetMaxSpeed();
}
```

<!--
  역할:
  - CMC가 매 프레임 이동 계산할 때 호출
  - Sprint: 지상에서만 적용 (공중에서는 무시)
  - ADS: Sprint보다 후순위
  - 클라이언트와 서버 양쪽에서 동일하게 호출 → 예측 일치 보장
-->

### 4-3. UpdateFromCompressedFlags()

```cpp
void UEPCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
    bWantsToAim   = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}
```

<!--
  호출 시점: 서버가 클라이언트 이동 패킷 수신 시
  역할: CompressedFlags 비트 → CMC 플래그로 복원
  FLAG_Custom_0~3: UE5가 제공하는 4개 커스텀 비트
  Super 호출로 기본 플래그(Jump, Crouch 등)도 처리
  이후 GetMaxSpeed()가 올바른 속도로 서버 재시뮬레이션
-->

### 4-4. GetPredictionData_Client()

```cpp
FNetworkPredictionData_Client* UEPCharacterMovement::GetPredictionData_Client() const
{
    if (!ClientPredictionData)
    {
        UEPCharacterMovement* MutableThis = const_cast<UEPCharacterMovement*>(this);
        MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_EPCharacter(*this);
    }
    return ClientPredictionData;
}
```

<!--
  역할: 기본 FSavedMove 대신 커스텀 FSavedMove_EPCharacter를 생성하도록 연결
  const_cast: const 함수에서 최초 1회 할당이 필요하므로
  싱글턴 패턴 — 한 번 생성 후 재사용
-->

---

## 5. FSavedMove_EPCharacter — 이동 입력 저장

<!--
  SavedMove란 무엇인가:
  - 클라이언트가 매 프레임 생성하는 이동 '스냅샷'
  - 서버 보정 시 이 스냅샷들을 리플레이하여 위치 재계산
  - 커스텀 플래그 없으면 리플레이 시 Sprint 상태 누락 → 걷기 속도로 이동하는 버그

  UCLASS가 아닌 일반 C++ 클래스:
  - .cpp 내부에 선언 (외부 참조 불필요)
  - Super 사용 불가 → 부모 클래스명(FSavedMove_Character::) 직접 사용
-->

### 5-1. 클래스 선언

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
```

### 5-2. Clear()

```cpp
void FSavedMove_EPCharacter::Clear()
{
    FSavedMove_Character::Clear();
    bSavedWantsToSprint = false;
    bSavedWantsToAim = false;
}
```

<!--
  호출 시점: SavedMove가 풀(pool)에서 재사용될 때
  역할: 이전 프레임 데이터 초기화
  안 하면: 이전 프레임 Sprint 상태가 다음 프레임에 남는 버그
-->

### 5-3. GetCompressedFlags()

```cpp
uint8 FSavedMove_EPCharacter::GetCompressedFlags() const
{
    uint8 Result = FSavedMove_Character::GetCompressedFlags();
    if (bSavedWantsToSprint) Result |= FSavedMove_Character::FLAG_Custom_0;
    if (bSavedWantsToAim)    Result |= FSavedMove_Character::FLAG_Custom_1;
    return Result;
}
```

<!--
  호출 시점: 클라이언트가 이동 패킷을 서버에 전송할 때
  역할: SavedMove 플래그 → 1바이트 비트필드로 압축

  왜 FSavedMove_Character::FLAG_Custom_0인가:
  - FLAG_Custom_0은 부모 클래스에 정의된 상수
  - FSavedMove_EPCharacter에는 해당 상수 없음

  왜 Super::가 아닌 FSavedMove_Character::인가:
  - FSavedMove는 UCLASS가 아닌 일반 C++ 클래스
  - Super는 GENERATED_BODY()가 생성하는 typedef → 일반 클래스에는 없음
  - 부모 클래스명을 직접 써야 함
-->

### 5-4. SetMoveFor()

```cpp
void FSavedMove_EPCharacter::SetMoveFor(
    ACharacter* C, float InDeltaTime, FVector const& NewAccel,
    FNetworkPredictionData_Client_Character& ClientData)
{
    FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
    UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(C->GetCharacterMovement());
    bSavedWantsToSprint = CMC->bWantsToSprint;
    bSavedWantsToAim = CMC->bWantsToAim;
}
```

<!--
  호출 시점: 이동 시뮬레이션 직전
  역할: CMC 현재 플래그 → SavedMove에 스냅샷 저장
  이 데이터가 GetCompressedFlags()로 서버에 전송되고,
  보정 시 PrepMoveFor()로 CMC에 복원됨
-->

### 5-5. PrepMoveFor()

```cpp
void FSavedMove_EPCharacter::PrepMoveFor(ACharacter* C)
{
    FSavedMove_Character::PrepMoveFor(C);
    UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(C->GetCharacterMovement());
    CMC->bWantsToSprint = bSavedWantsToSprint;
    CMC->bWantsToAim = bSavedWantsToAim;
}
```

<!--
  호출 시점: 서버 보정 후 클라이언트가 미확인 이동을 리플레이할 때
  역할: SavedMove에 저장된 플래그 → CMC에 복원
  이렇게 해야 리플레이 중 GetMaxSpeed()가 올바른 속도 반환

  주의 - 실제로 겪은 버그:
  처음에 CMC->bWantsToSprint = CMC->bWantsToSprint 로 자기 대입해버림
  반드시 bSavedWantsToSprint에서 읽어야 함
-->

### 5-6. CanCombineWith()

```cpp
bool FSavedMove_EPCharacter::CanCombineWith(
    const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
    FSavedMove_EPCharacter* Other = static_cast<FSavedMove_EPCharacter*>(NewMove.Get());
    if (bSavedWantsToSprint != Other->bSavedWantsToSprint) return false;
    if (bSavedWantsToAim != Other->bSavedWantsToAim) return false;
    return FSavedMove_Character::CanCombineWith(NewMove, Character, MaxDelta);
}
```

<!--
  역할: 연속된 동일 입력을 하나로 합쳐 대역폭 절약
  예: 10프레임 Sprint 유지 → 1개로 합침
  플래그가 다르면(Sprint→Walk 전환) 합칠 수 없음 → false
-->

---

## 6. FNetworkPredictionData_Client_EPCharacter

```cpp
class FNetworkPredictionData_Client_EPCharacter : public FNetworkPredictionData_Client_Character
{
public:
    explicit FNetworkPredictionData_Client_EPCharacter(
        const UCharacterMovementComponent& ClientMovement)
        : FNetworkPredictionData_Client_Character(ClientMovement) {}

    virtual FSavedMovePtr AllocateNewMove() override
    {
        return FSavedMovePtr(new FSavedMove_EPCharacter());
    }
};
```

<!--
  역할: CMC가 새 SavedMove 요청 시 FSavedMove_EPCharacter 생성
  이 클래스 없으면 기본 FSavedMove_Character 생성 → 커스텀 플래그 저장 안 됨
  AllocateNewMove() 하나만 오버라이드
-->

---

## 7. AEPCharacter — FObjectInitializer로 CMC 교체

### 7-1. FObjectInitializer란

<!--
  - UE5의 서브오브젝트 생성 커스터마이즈 메커니즘
  - ACharacter가 기본으로 만드는 UCharacterMovementComponent를 교체
  - CreateDefaultSubobject로는 이미 부모가 생성한 CMC를 교체 불가
  - 반드시 생성자 시그니처가 (const FObjectInitializer&)이어야 함
-->

### 7-2. 생성자

```cpp
AEPCharacter::AEPCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UEPCharacterMovement>(
        ACharacter::CharacterMovementComponentName))
{
    // 카메라, 메시 설정 ...

    UEPCharacterMovement* Movement = Cast<UEPCharacterMovement>(GetCharacterMovement());
    Movement->NavAgentProps.bCanCrouch = true;  // Crouch 활성화
}
```

<!--
  기존 Sprint 코드에서 제거한 것:
  - UPROPERTY(ReplicatedUsing=OnRep_IsSprinting) bIsSprinting
  - UFUNCTION(Server, Reliable) Server_SetSprinting
  - OnRep_IsSprinting
  - GetLifetimeReplicatedProps에서 bIsSprinting
  - WalkSpeed/SprintSpeed 멤버 변수 (CMC로 이관)
-->

---

## 8. 입력 처리

### 8-1. Sprint

```cpp
void AEPCharacter::Input_StartSprint(const FInputActionValue& Value)
{
    if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
        CMC->bWantsToSprint = true;
}

void AEPCharacter::Input_StopSprint(const FInputActionValue& Value)
{
    if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
        CMC->bWantsToSprint = false;
}
```

<!--
  핵심: Server RPC 호출 없음 — CMC 플래그만 설정
  다음 프레임 GetMaxSpeed()에서 SprintSpeed 반영
  서버 전송: SetMoveFor() → GetCompressedFlags() → FLAG_Custom_0
-->

### 8-2. ADS

```cpp
void AEPCharacter::Input_StartADS(const FInputActionValue& Value)
{
    if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
    {
        CMC->bWantsToAim = true;
        CMC->bWantsToSprint = false;  // ADS 중 Sprint 해제
    }
    if (IsLocallyControlled() && FirstPersonCamera)
        FirstPersonCamera->SetFieldOfView(60.f);
}

void AEPCharacter::Input_StopADS(const FInputActionValue& Value)
{
    if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
        CMC->bWantsToAim = false;
    if (IsLocallyControlled() && FirstPersonCamera)
        FirstPersonCamera->SetFieldOfView(90.f);
}
```

<!--
  ADS 시작 시 Sprint 강제 해제: 동시 활성화 방지
  FOV: IsLocallyControlled()로 로컬만. 시각 효과이므로 복제 불필요
-->

### 8-3. Crouch

```cpp
void AEPCharacter::Input_Crouch(const FInputActionValue& Value)  { Crouch(); }
void AEPCharacter::Input_UnCrouch(const FInputActionValue& Value) { UnCrouch(); }
```

<!--
  CMC 기본 지원:
  - bCanCrouch = true 설정만으로 네트워크 동기화 자동 처리
  - CMC 내부 FLAG_WantsToCrouch로 이미 CompressedFlags 처리됨
  - 별도 SavedMove 확장이나 RPC 불필요
-->

---

## 9. 네트워크 흐름 정리

### Sprint 입력 → 서버 반영 → 보정 흐름

```
[클라이언트]
1. Input_StartSprint() → CMC.bWantsToSprint = true
2. GetMaxSpeed() → SprintSpeed 반환 → 로컬 예측 이동
3. SetMoveFor() → bSavedWantsToSprint = true 저장
4. GetCompressedFlags() → FLAG_Custom_0 ON → 서버 전송

[서버]
5. UpdateFromCompressedFlags() → bWantsToSprint = true 복원
6. GetMaxSpeed() → SprintSpeed → 서버 위치 계산
7. 클라이언트 결과와 비교
   → 일치: ACK
   → 불일치: 보정 전송

[클라이언트 - 보정 시]
8. 서버 위치로 리셋
9. 미확인 SavedMove 리플레이:
   PrepMoveFor() → bWantsToSprint 복원 → GetMaxSpeed() → SprintSpeed로 재시뮬레이션
```

---

## 10. 배운 점 / 삽질 기록

<!--
  실제로 겪은 문제와 해결:
  - PrepMoveFor 자기 대입 버그
  - FSavedMove에서 Super 사용 불가 (UCLASS가 아님)
  - FObjectInitializer 패턴을 써야 하는 이유
  - FLAG_Custom 4개 한계
  - 기타 ...
-->

---

## 11. 다음 단계

<!-- Step2-2: 무기 장착 + 사격에서 다룰 내용 미리보기 -->
