# [UE5 C++] 2단계-4: 애니메이션 시스템 (AnimInstance + AnimBP)

> 02_Implementation Step 8 (애니메이션)

---

## 1. 이번 글에서 다루는 것

- UEPAnimInstance: AnimBP의 C++ 베이스 클래스
- NativeUpdateAnimation: CMC에서 이동 상태 읽기
- AnimBP (블루프린트) 구성: Locomotion, Sprint, Jump, Crouch, ADS
- Aim Offset: AimPitch로 상하 조준
- Fire 몽타주: Multicast RPC에서 재생
- 네트워크 동기화: 왜 AnimInstance가 자동으로 동기화되는가

---

## 2. 구조 개요

### C++ ↔ AnimBP 관계

```
UEPAnimInstance (C++)
  ├─ NativeUpdateAnimation()     ← 매 프레임 호출
  │    ├─ Speed, Direction        ← 블렌드스페이스 입력
  │    ├─ bIsInAir, bIsCrouching  ← 상태 머신 조건
  │    ├─ bIsSprinting, bIsAiming ← CMC에서 읽기
  │    └─ AimPitch                ← Aim Offset
  │
  └─ ABP_EPCharacter (AnimBP, Blueprint)
       ├─ Locomotion 블렌드스페이스 (Speed × Direction)
       ├─ State Machine: Idle/Walk → Sprint → Jump/Fall → Crouch
       ├─ Aim Offset: AimPitch
       └─ Fire Montage (상체 슬롯)
```

### 왜 AnimInstance가 네트워크에서 자동 동기화되는가

<!--
  AnimInstance 자체는 복제되지 않음. 하지만:
  - CMC의 이동 상태(Velocity, bIsCrouched)가 이미 복제됨
  - Sprint/ADS는 CMC CompressedFlags로 동기화됨
  - NativeUpdateAnimation()이 매 프레임 이 값들을 읽음
  → 서버/클라 모두 같은 입력 → 같은 애니메이션 재생
  → 별도 애니메이션 복제 불필요
-->

---

## 3. UEPAnimInstance

### 3-1. 헤더 (EPAnimInstance.h)

```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    // --- 이동 ---
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

    // --- 전투 ---
    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bIsAiming = false;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    float AimPitch = 0.f;

    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
```

<!--
  BlueprintReadOnly:
  - AnimBP(블루프린트)에서 읽기 전용으로 접근
  - C++에서만 값을 설정, AnimBP에서는 조건/입력으로 사용

  왜 NativeUpdateAnimation인가:
  - BlueprintUpdateAnimation의 C++ 버전
  - 매 프레임 호출 (Tick과 유사하지만 애니메이션 전용)
  - C++로 작성하면 Blueprint 노드 연결 없이 깔끔하게 처리
-->

### 3-2. NativeUpdateAnimation

```cpp
void UEPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    APawn* Owner = TryGetPawnOwner();
    if (!Owner) return;

    // 속도 + 방향
    FVector Velocity = Owner->GetVelocity();
    Speed = Velocity.Size2D();

    FRotator AimRotation = Owner->GetBaseAimRotation();
    FRotator MovementRotation = Velocity.ToOrientationRotator();
    Direction = FRotator::NormalizeAxis(MovementRotation.Yaw - AimRotation.Yaw);

    if (ACharacter* Char = Cast<ACharacter>(Owner))
    {
        bIsInAir = Char->GetCharacterMovement()->IsFalling();
        bIsCrouching = Char->bIsCrouched;

        // CMC에서 Sprint/ADS 읽기
        if (UEPCharacterMovement* CMC =
            Cast<UEPCharacterMovement>(Char->GetCharacterMovement()))
        {
            bIsSprinting = CMC->bWantsToSprint;
            bIsAiming = CMC->bWantsToAim;
        }
    }

    AimPitch = AimRotation.Pitch;
}
```

<!--
  각 변수 설명:

  Speed (Velocity.Size2D()):
  - 2D 속도 (수평면). Z축(낙하) 제외
  - 블렌드스페이스에서 Idle(0) ↔ Walk(200) ↔ Run(600) 블렌딩

  Direction:
  - 이동 방향과 조준 방향의 차이 (Yaw)
  - -180 ~ 180. 0=전방, 90=우측, -90=좌측, 180=후방
  - 블렌드스페이스에서 전/후/좌/우 애니메이션 블렌딩

  bIsInAir (IsFalling()):
  - 점프/낙하 중 여부
  - 상태 머신: Ground → InAir 전환 조건

  bIsCrouching (bIsCrouched):
  - ACharacter 기본 프로퍼티 (CMC가 자동 관리)
  - 앉기 애니메이션 전환 조건

  bIsSprinting / bIsAiming:
  - CMC에서 직접 읽기 (Character 프로퍼티 아님)
  - CMC가 서버/클라 모두에 상태를 가지므로 정확히 동기화됨

  AimPitch:
  - 조준 상하각
  - Aim Offset에서 상체 기울기 블렌딩
-->

---

## 4. AnimBP 구성 (에디터)

<!--
  에디터에서 ABP_EPCharacter를 어떻게 구성하는지 설명.
  스크린샷 추가 권장.
-->

### 4-1. AnimBP 생성

<!--
  1. Content Browser → Animation → Animation Blueprint
  2. Parent Class: UEPAnimInstance
  3. Skeleton: 사용 중인 캐릭터 스켈레톤 선택
-->

### 4-2. Locomotion 블렌드스페이스

<!--
  블렌드스페이스 (Speed × Direction):
  - 축: Horizontal = Direction (-180~180), Vertical = Speed (0~650)
  - 배치: Idle, Walk_Fwd, Walk_Bwd, Walk_Left, Walk_Right, Run 등
  - AnimGraph에서 Speed, Direction 변수를 블렌드스페이스 입력으로 연결
-->

### 4-3. State Machine

<!--
  상태:
  - Idle/Walk: 기본 Locomotion 블렌드스페이스
  - Sprint: bIsSprinting == true (달리기 전용 애니메이션)
  - InAir: bIsInAir == true (점프/낙하)
  - Crouch: bIsCrouching == true (앉기 Locomotion)

  전환 규칙:
  - Idle/Walk → Sprint: bIsSprinting && Speed > 0
  - Idle/Walk → InAir: bIsInAir
  - Idle/Walk → Crouch: bIsCrouching
  - Sprint → Idle/Walk: !bIsSprinting
  - InAir → Idle/Walk: !bIsInAir
-->

### 4-4. Aim Offset

<!--
  - AimPitch 값으로 상체 상하 기울기 적용
  - -90(아래) ~ 90(위) 범위
  - Locomotion 위에 레이어드 블렌드로 적용
-->

### 4-5. Fire 몽타주

<!--
  - 상체 슬롯에서 재생 (하체 이동 애니메이션과 독립)
  - Multicast_PlayFireEffect에서 Montage_Play() 호출
  - 짧은 반동 애니메이션 (0.2~0.3초)
-->

---

## 5. 사격 몽타주 연동

```cpp
// Multicast_PlayFireEffect_Implementation 내부
if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
{
    Anim->Montage_Play(FireMontage);
}
```

<!--
  왜 Multicast에서 몽타주를 재생하는가:
  - 사격 이펙트는 모든 클라이언트에서 보여야 함
  - Multicast RPC 안에서 몽타주 재생 → 모든 클라이언트에서 반동 애니메이션

  몽타주 슬롯:
  - UpperBody 슬롯 → 상체만 반동
  - 하체는 Locomotion 유지
-->

---

## 6. 캐릭터에 AnimBP 할당

<!--
  EPCharacter 생성자 또는 블루프린트에서:
  GetMesh()->SetAnimInstanceClass(ABP_EPCharacter::StaticClass());

  또는 블루프린트:
  Mesh 컴포넌트 → Anim Class → ABP_EPCharacter 선택
-->

---

## 7. 네트워크 동기화 정리

| 애니메이션 데이터 | 출처 | 동기화 방식 |
|------------------|------|------------|
| Speed, Direction | GetVelocity() | CMC 이동 복제 (자동) |
| bIsSprinting | CMC.bWantsToSprint | CompressedFlags (자동) |
| bIsAiming | CMC.bWantsToAim | CompressedFlags (자동) |
| bIsCrouching | bIsCrouched | CMC 내장 (자동) |
| bIsInAir | IsFalling() | CMC 이동 복제 (자동) |
| AimPitch | GetBaseAimRotation() | Controller Rotation 복제 (자동) |
| Fire 몽타주 | Multicast RPC | 명시적 RPC |

<!--
  핵심: 몽타주를 제외한 모든 애니메이션 데이터가 이미 복제되는 값에서 파생됨
  → AnimInstance 자체를 복제할 필요 없음
  → NativeUpdateAnimation이 복제된 값을 읽기만 하면 됨
-->

---

## 8. 배운 점 / 삽질 기록

<!--
  - NativeUpdateAnimation vs BlueprintUpdateAnimation 선택 기준
  - Direction 계산: NormalizeAxis 없으면 -180~180 범위 넘어감
  - Size2D vs Size: 낙하 중 Z 속도가 블렌드스페이스에 영향 주는 문제
  - AnimBP에서 C++ 변수 접근: BlueprintReadOnly가 없으면 안 보임
  - Aim Offset 설정 시 주의점
  - 기타 ...
-->

---

## 9. 2단계 전체 정리

<!--
  Step2-1 ~ Step2-4까지 구현한 전체 시스템 요약:
  - CMC 확장: Sprint/ADS CompressedFlags 동기화
  - 무기: 서버 스폰, 소켓 Attach, 탄약 복제
  - 사격: Server RPC 히트스캔, Multicast 이펙트
  - HP/사망: 서버 권한 데미지, Corpse 스폰
  - 애니메이션: CMC 데이터 → AnimBP → 자동 동기화

  다음 단계 예고 (3단계: GAS 등)
-->
