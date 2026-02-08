# CMC 기반 Sprint 구현 정리

이 문서는 Sprint를 **CharacterMovementComponent(CMC) 확장 + SavedMove**로 구현하는 방법을 요약한다.  
목표는 클라 예측/서버 보정 흐름에 Sprint 상태를 포함시켜 **멀티에서 스냅/튐을 최소화**하는 것.

---

## 왜 CMC 확장인가

단순히 `MaxWalkSpeed`를 서버 RPC로 변경하면, 클라 예측이 완전히 일치하지 않아 보정이 잦아질 수 있다.  
CMC 확장은 Sprint 상태를 **이동 입력 패킷**에 포함해, 서버 재시뮬레이션과 동일한 결과를 보장한다.

---

## 구현 개요

1. `UCharacterMovementComponent`를 상속한 커스텀 CMC 생성  
2. `FSavedMove_Character` 확장 → Sprint 플래그 저장  
3. `GetCompressedFlags()`에 Sprint 플래그를 넣어 전송  
4. `UpdateFromCompressedFlags()`에서 복원  
5. `GetMaxSpeed()`에서 Sprint 속도 반영  

---

## 클래스 구성

### 1) 커스텀 CMC

```cpp
UCLASS()
class UEPCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	uint8 bWantsToSprint : 1;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeed = 650.f;

	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};
```

### 2) SavedMove 확장

```cpp
class FSavedMove_EPCharacter : public FSavedMove_Character
{
public:
	uint8 bSavedWantsToSprint : 1;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel,
	                        FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* Character) override;
};
```

### 3) Client Prediction Data

```cpp
class FNetworkPredictionData_Client_EPCharacter : public FNetworkPredictionData_Client_Character
{
public:
	explicit FNetworkPredictionData_Client_EPCharacter(const UCharacterMovementComponent& ClientMovement);
	virtual FSavedMovePtr AllocateNewMove() override;
};
```

---

## 핵심 함수 로직 요약

### UEPCharacterMovement::GetMaxSpeed

```cpp
return bWantsToSprint ? SprintSpeed : Super::GetMaxSpeed();
```

### UEPCharacterMovement::UpdateFromCompressedFlags

```cpp
Super::UpdateFromCompressedFlags(Flags);
bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
```

### FSavedMove_EPCharacter::GetCompressedFlags

```cpp
uint8 Result = Super::GetCompressedFlags();
if (bSavedWantsToSprint)
	Result |= FSavedMove_Character::FLAG_Custom_0;
return Result;
```

### FSavedMove_EPCharacter::SetMoveFor

```cpp
bSavedWantsToSprint = MyMovementComponent->bWantsToSprint;
```

### FSavedMove_EPCharacter::PrepMoveFor

```cpp
MyMovementComponent->bWantsToSprint = bSavedWantsToSprint;
```

---

## 캐릭터 입력 흐름

1. 입력 시 `bWantsToSprint` 설정 (로컬 즉시 반응)  
2. CMC가 이동 패킷에 Sprint 플래그 포함  
3. 서버가 동일 입력으로 재시뮬레이션  
4. 클라 보정 시 Sprint 상태도 함께 반영

---

## 장점

- 클라 예측과 서버 보정이 일치  
- 멀티에서 스냅/드드득 감소  
- Sprint 상태가 이동 입력과 함께 동기화됨

---

## 참고

- `FSavedMove_Character::FLAG_Custom_0` ~ `FLAG_Custom_3` 사용 가능  
- Sprint/ADS 같은 상태는 **가능하면 CMC 확장 패턴** 권장  
