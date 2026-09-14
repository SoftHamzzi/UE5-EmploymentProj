# Animation 시스템 설계 (Lyra 스타일 Linked Anim Layer)

## 1. 아키텍처 개요

```
┌─────────────────────────────────────────────┐
│ ABP_EPCharacter (메인 AnimBP)               │
│  Parent C++: UEPAnimInstance                │
│  Implements: ALI_EPWeapon                   │
│                                             │
│  ┌─ Locomotion StateMachine ──────────────┐ │
│  │  Idle ←→ Walk/Run ←→ Crouch           │ │
│  │  ↕ Jump/Fall                           │ │
│  └────────────────────────────────────────┘ │
│                                             │
│  ┌─ Linked Anim Layer (ALI_EPWeapon) ────┐ │
│  │  FullBody_IdleWalkRun  ← 빈 슬롯      │ │
│  │  UpperBody_AimFire     ← 빈 슬롯      │ │
│  │  FullBody_Jump         ← 빈 슬롯      │ │
│  │  FullBody_Crouch       ← 빈 슬롯      │ │
│  └────────────────────────────────────────┘ │
│                                             │
│  Aim Offset: AimPitch / AimYaw              │
│  DefaultSlot (몽타주용)                      │
└─────────────────────────────────────────────┘
           ↑ LinkAnimClassLayers()
┌─────────────────────────────────────────────┐
│ ABP_EPWeaponAnimLayersBase                  │
│  Parent C++: UEPWeaponAnimInstance          │
│  Implements: ALI_EPWeapon                   │
│                                             │
│  각 레이어 함수에 Sequence/BS Player 노드   │
│  → Promote to Asset Override                │
└─────────────────────────────────────────────┘
           ↑ 상속
┌─────────────────────────────────────────────┐
│ ABP_RifleAnimLayers (무기 AnimBP)           │
│                                             │
│  AnimGraphOverrides (에셋 오버라이드 에디터) │
│  FullBody_IdleWalkRun → BS_Rifle_IdleWalkRun│
│  UpperBody_AimFire    → 라이플 조준 시퀀스  │
│  FullBody_Jump        → 라이플 점프 시퀀스  │
│  FullBody_Crouch      → 라이플 앉기 시퀀스  │
└─────────────────────────────────────────────┘
```

무기 교체 시 `LinkAnimClassLayers(ABP_RifleAnimLayers)`로 교체하면 모든 레이어가 한번에 바뀜.

## 2. C++ 클래스

### 2-1. UEPAnimInstance

메인 AnimBP의 C++ 백엔드. 캐릭터/CMC에서 상태를 읽어 AnimBP에 전달.

```
Public/Animation/EPAnimInstance.h
Private/Animation/EPAnimInstance.cpp
```

```cpp
// EPAnimInstance.h
UCLASS()
class EMPLOYMENTPROJ_API UEPAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    // Weapon AnimBP에서 Property Access로 읽어갈 변수들
    // BlueprintReadOnly + public = Property Access 가능
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float Speed = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    float Direction = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsSprinting = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsFalling = false;

    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    bool bIsCrouching = false;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bIsAiming = false;

    UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
    float AimPitch = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
    float AimYaw = 0.f;

protected:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
    TWeakObjectPtr<class AEPCharacter> CachedCharacter;
};
```

```cpp
// EPAnimInstance.cpp
#include "Animation/EPAnimInstance.h"
#include "Core/EPCharacter.h"
#include "Movement/EPCharacterMovement.h"
#include "KismetAnimationLibrary.h"  // AnimGraphRuntime 모듈 필요

void UEPAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    CachedCharacter = Cast<AEPCharacter>(TryGetPawnOwner());
}

void UEPAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    AEPCharacter* Character = CachedCharacter.Get();
    if (!Character) return;

    FVector Velocity = Character->GetVelocity();
    Speed = Velocity.Size2D();

    FRotator AimRotation = Character->GetBaseAimRotation();
    // CalculateDirection은 deprecated → UKismetAnimationLibrary 사용
    Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, Character->GetActorRotation());

    bIsSprinting = Character->GetIsSprinting();
    bIsAiming = Character->GetIsAiming();
    bIsFalling = Character->GetCharacterMovement()->IsFalling();
    bIsCrouching = Character->bIsCrouched;

    AimPitch = FMath::ClampAngle(AimRotation.Pitch, -90.f, 90.f);
    AimYaw = FMath::ClampAngle(
        FRotator::NormalizeAxis(AimRotation.Yaw - Character->GetActorRotation().Yaw),
        -90.f, 90.f);
}
```

> **Build.cs**: `AnimGraphRuntime` 모듈 추가 필요 (`KismetAnimationLibrary.h` 사용 시)

### 2-2. UEPWeaponAnimInstance

무기 AnimBP의 C++ 베이스. **최소 구조만 유지.**

변수나 NativeUpdateAnimation을 두지 않는다. 메인 AnimBP 변수 접근은 **Blueprint의 Property Access**로 처리.

```cpp
// EPWeaponAnimInstance.h
UCLASS()
class EMPLOYMENTPROJ_API UEPWeaponAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
};
```

> **왜 변수를 두지 않는가?**
> Lyra와 동일한 이유: WeaponAnimInstance는 무기 레이어 전용 인스턴스로 캐릭터 로코모션 데이터가 필요 없다.
> Speed/Direction 등은 메인 AnimBP(UEPAnimInstance)에만 존재하고, 무기 레이어 AnimBP에서
> `GetMainAnimBPThreadSafe()` → Property Access로 Thread-Safe하게 직접 읽어온다.

### 2-3. EPWeaponDefinition — AnimLayer 필드

`WeaponAnimLayer`는 `UEPWeaponDefinition`에 이미 포함되어 있다. (Item.md 참조)

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
TSubclassOf<UAnimInstance> WeaponAnimLayer;
```

### 2-4. CombatComponent — AnimLayer 링크

```cpp
void UEPCombatComponent::EquipWeapon(AEPWeapon* NewWeapon)
{
    if (!GetOwner()->HasAuthority() || !NewWeapon) return;
    if (EquippedWeapon) UnequipWeapon();

    EquippedWeapon = NewWeapon;
    AEPCharacter* Owner = GetOwnerCharacter();
    NewWeapon->AttachToComponent(Owner->GetMesh(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("WeaponSocket"));

    if (NewWeapon->WeaponDef && NewWeapon->WeaponDef->WeaponAnimLayer)
        Owner->GetMesh()->LinkAnimClassLayers(NewWeapon->WeaponDef->WeaponAnimLayer);
}

void UEPCombatComponent::UnequipWeapon()
{
    if (!GetOwner()->HasAuthority() || !EquippedWeapon) return;
    AEPCharacter* Owner = GetOwnerCharacter();

    if (EquippedWeapon->WeaponDef && EquippedWeapon->WeaponDef->WeaponAnimLayer)
        Owner->GetMesh()->UnlinkAnimClassLayers(EquippedWeapon->WeaponDef->WeaponAnimLayer);

    EquippedWeapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    EquippedWeapon = nullptr;
}

// OnRep — 클라이언트 측 처리
void UEPCombatComponent::OnRep_EquippedWeapon()
{
    AEPCharacter* Owner = GetOwnerCharacter();
    if (!Owner || !EquippedWeapon) return;

    EquippedWeapon->AttachToComponent(Owner->GetMesh(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("WeaponSocket"));

    if (EquippedWeapon->WeaponDef && EquippedWeapon->WeaponDef->WeaponAnimLayer)
        Owner->GetMesh()->LinkAnimClassLayers(EquippedWeapon->WeaponDef->WeaponAnimLayer);
}
```

## 3. Blueprint 에셋

### 3-1. ALI_EPWeapon (Animation Layer Interface)

에디터에서 생성: Content Browser → Animation → Animation Layer Interface

| 레이어 함수 | 용도 |
|---|---|
| `FullBody_IdleWalkRun` | 무기 들고 대기/걷기/뛰기 |
| `UpperBody_AimFire` | 조준/사격 상체 |
| `FullBody_Jump` | 점프/낙하 |
| `FullBody_Crouch` | 앉기 이동 |

### 3-2. ABP_EPCharacter (메인 AnimBP)

- **Parent Class**: UEPAnimInstance
- **Skeleton**: 메타휴먼 Body 스켈레톤
- **Implements**: ALI_EPWeapon

AnimGraph 구조 (노드 순서 중요):

> **Linked Anim Layer는 ABP 전체에서 레이어 함수당 1번만 인스턴싱**된다.
> 같은 레이어를 여러 State에 중복 배치해도 동일 인스턴스를 공유해 의미가 없다.
> 따라서 Idle/Walk/Jog를 별도 State로 분리하지 않고 **단일 Grounded State** 안에서 BlendSpace로 처리한다.

```
[Locomotion State Machine]
    ├── Grounded (Idle+Walk+Jog 통합)  → FullBody_IdleWalkRun 레이어 1회 호출
    │       BlendSpace가 Speed 값으로 내부에서 Idle→Walk→Jog 블렌딩
    ├── Crouch   (bIsCrouching)        → FullBody_Crouch 레이어
    └── Jump SM  (bIsFalling)
            ├── JumpStart (0.2s)
            ├── InAir     (loop)
            └── Land      (0.3s)       → FullBody_Jump 레이어
    ↓
[Orientation Warping]                   ← 하체를 이동 방향으로 워핑 (AnimationWarping 플러그인)
    ↓
[Rotate Root Bone: Yaw=RootYawOffset]   ← 제자리 회전 오프셋 (하체 버팀)
    ↓
[Cache Pose: "LocomotionWithTurn"]
    ↓
[Layered Blend Per Bone]                ← spine_01 이상 상체에 AimOffset 적용
    ├── Base Pose: Cached LocomotionWithTurn
    └── Blend Poses 0: AimOffset (Pitch=AimPitch, Yaw=AimYaw)
              └── Base Pose: Cached LocomotionWithTurn
    ↓
[Two-Bone IK: hand_l]                   ← 왼손 총 그립 IK
    ↓
[DefaultSlot]                           ← 몽타주 (Fire, Reload, Death)
    ↓
[Output Pose]
```

> **Layered Blend Per Bone 설정**: Branch Filters → Bone Name `spine_01`, Blend Depth 0
> 이 순서가 맞아야 팔/총이 항상 시야 방향을 향하고 하체만 turn in place가 적용됨

### 3-3. ABP_EPWeaponAnimLayersBase (무기 레이어 베이스 ABP)

- **Parent Class**: UEPWeaponAnimInstance
- **Implements**: ALI_EPWeapon
- **역할**: 각 레이어 함수에 Sequence Player / BlendSpace Player 노드를 두고 Asset Override로 노출

각 레이어 함수 그래프에서:
1. Sequence Player 또는 BlendSpace Player 노드 추가
2. 우클릭 → **Promote to Asset Override**
3. 자식 ABP의 에셋 오버라이드 에디터에 슬롯이 노출됨

> BlendSpace를 사용하는 레이어(IdleWalkRun)는 반드시 **BlendSpace Player** 노드를 사용해야 함.
> Sequence Player로는 BlendSpace 할당 불가.

### 3-4. ABP_RifleAnimLayers (라이플 무기 AnimBP)

- **Parent Class**: ABP_EPWeaponAnimLayersBase
- **Skeleton**: 메타휴먼 Body 스켈레톤

에셋 오버라이드 에디터(AnimGraphOverrides)에서 슬롯에 리타겟된 시퀀스/BlendSpace 할당:

| 슬롯 | 할당 에셋 |
|---|---|
| `FullBody_IdleWalkRun` | BS_Rifle_IdleWalkRun (BlendSpace 2D) |
| `FullBody_Crouch` | MM_Rifle_Crouch_Idle (시퀀스) |
| `FullBody_Jump` | MM_Rifle_Jump_Start (시퀀스) |
| `UpperBody_AimFire` | MM_Rifle_Idle_ADS (시퀀스) |

### 3-5. BS_Rifle_IdleWalkRun (BlendSpace 2D)

- **Skeleton**: 메타휴먼 Body 스켈레톤
- X축: Direction (-180 ~ 180)
- Y축: Speed (0 ~ 600)

| Direction | Speed | 시퀀스 |
|---|---|---|
| 0 | 0 | MM_Rifle_Idle_Hipfire |
| 0 | 200 | MM_Rifle_Walk_Fwd |
| 0 | 600 | MM_Rifle_Jog_Fwd |
| 180 | 200 | MM_Rifle_Walk_Bwd |
| 180 | 600 | MM_Rifle_Jog_Bwd |
| 90 | 200 | MM_Rifle_Walk_Right |
| 90 | 600 | MM_Rifle_Jog_Right |
| -90 | 200 | MM_Rifle_Walk_Left |
| -90 | 600 | MM_Rifle_Jog_Left |

> 현재는 단순 BlendSpace 2D 사용. 추후 GAS 단계에서 Lyra의 Cardinal Direction + Stride Warping 방식으로 교체 예정.

### 3-6. 에셋 배치

```
Content/Characters/MetaHuman/Animations/
├── ABP_EPCharacter
├── LayersInterface/
│   └── ALI_EPWeapon
├── Rifle/
│   ├── ABP_EPWeaponAnimLayersBase
│   ├── ABP_RifleAnimLayers
│   └── BS_Rifle_IdleWalkRun
├── AimOffsets/              (리타겟된 AimOffset 포즈 시퀀스)
├── Locomotion/Rifle/        (리타겟된 이동 시퀀스)
└── Actions/                 (리타겟된 Fire, Reload, Death 등)
```

## 4. Property Access (Lyra 방식)

무기 AnimBP에서 메인 AnimBP 변수에 Thread-Safe하게 접근하는 방법.

**ABP_EPWeaponAnimLayersBase AnimGraph에서:**
1. `Get Main Anim Blueprint Thread Safe` 노드 추가
2. Cast to `ABP_EPCharacter`
3. Property Access로 Speed, Direction, bIsAiming 등 직접 읽기
4. BlendSpace Player / Sequence Player 핀에 연결

```
GetMainAnimBPThreadSafe()
    → Cast to ABP_EPCharacter
        → .Speed     → BlendSpace Player Speed 핀
        → .Direction → BlendSpace Player Direction 핀
        → .bIsAiming → 조건 분기
```

**왜 C++에서 복사하지 않는가:**
- Property Access는 UE5가 내부적으로 안전한 타이밍에 값을 복사 → 워커 스레드에서 읽음 (Thread-Safe)
- NativeUpdateAnimation의 게임 스레드 복사보다 효율적
- EPWeaponAnimInstance에 중복 변수 선언 불필요

## 5. 리타게팅 (MetaHuman 적용)

Lyra 마네킹 애니메이션 → MetaHuman Body 스켈레톤으로 변환.

**필요 에셋:**
- `IK_Mannequin` (Lyra 소스 IK Rig)
- MetaHuman Body 기반 IK Rig (MetaHuman 플러그인 내 존재)
- `RTG_MannequinToMetaHuman` (IK Retargeter)

**리타겟 대상:**
- Locomotion/Rifle 시퀀스 전체
- AimOffset 포즈 시퀀스 전체
- Actions (Fire, Reload, Death, HitReact) 시퀀스

**BlendSpace / AimOffset:**
리타게터에서 직접 처리 불가. 리타겟된 시퀀스로 새로 생성 또는 복제 후 재할당.

## 6. 데이터 흐름

```
매 프레임:
Character/CMC
    → UEPAnimInstance::NativeUpdateAnimation()
        → Speed, Direction, bIsAiming 등 갱신
        → ABP_EPCharacter StateMachine이 State 전환
        → Linked Anim Layer가 ABP_RifleAnimLayers 레이어 호출
        → ABP_RifleAnimLayers가 GetMainAnimBPThreadSafe()로 Speed/Direction 읽어 BlendSpace 재생

무기 장착:
CombatComponent::EquipWeapon()
    → GetMesh()->LinkAnimClassLayers(ABP_RifleAnimLayers)
    → 메인 AnimBP의 빈 레이어 슬롯이 ABP_RifleAnimLayers로 채워짐

사격:
Multicast_PlayFireEffect()
    → GetMesh()->GetAnimInstance()->Montage_Play(FireMontage)
    → DefaultSlot을 통해 상체 몽타주 재생
```

## 7. 미구현 시스템 상세

### 7-1. 양손 IK (Two-Hand Grip)

총을 두 손으로 자연스럽게 잡기 위한 IK. **AimOffset이 상체를 돌리면 손목 각도가 그립과 틀어지므로 양손 모두 IK 보정이 필요하다.**

**문제 상황:**
- AimOffset이 spine_01 이상을 회전 → 팔/손도 같이 이동
- 무기는 WeaponSocket(hand_r)에 붙어있어 손과 같이 움직이지만 손목 회전/각도가 그립과 맞지 않음
- 특히 위/아래 시점 이동 시 오른손 그립이 틀어짐, 왼손은 핸드가드 위치에서 벗어남

**올바른 IK 방향 (Lyra 방식):**
```
기존: 손 위치 → 무기 따라옴  (손이 주인, 무기가 따라감)
정답: 무기 위치 → 손을 IK로 끌어당김  (무기가 기준, 손이 맞춰짐)
```
AimOffset 이후 무기의 IK 소켓 위치로 양손을 IK 보정하면, 어떤 자세에서도 손이 그립을 유지.

**구현 순서:**

**1. 무기 메시에 IK 소켓 추가 (BP_WeaponAK74)**
- 무기 메시 에디터 → 소켓 추가
- `RightHandIK`: 오른손 그립 위치 (피스톨 그립 부근)
- `LeftHandIK`: 왼손 그립 위치 (핸드가드 부근)
- Preview Asset으로 캐릭터 메시 올려두고 실제 손 위치에 맞게 조정

**2. EPAnimInstance에 IK 변수 추가**
```cpp
// EPAnimInstance.h
UPROPERTY(BlueprintReadOnly, Category = "IK")
FTransform RightHandIKTransform;

UPROPERTY(BlueprintReadOnly, Category = "IK")
FTransform LeftHandIKTransform;

// EPAnimInstance.cpp NativeUpdateAnimation()
if (UEPCombatComponent* Combat = Character->GetCombatComponent())
{
    if (AEPWeapon* Weapon = Combat->GetEquippedWeapon())
    {
        UMeshComponent* WeaponMesh = Weapon->GetWeaponMesh();
        if (WeaponMesh)
        {
            RightHandIKTransform = WeaponMesh->GetSocketTransform(FName("RightHandIK"));
            LeftHandIKTransform  = WeaponMesh->GetSocketTransform(FName("LeftHandIK"));
        }
    }
}
```

**3. AnimGraph — AimOffset 이후에 IK 적용**
```
[Layered Blend Per Bone (AimOffset)]
    ↓
[Two-Bone IK: hand_r]    ← 오른손 그립 보정
    IK Bone: hand_r
    Effector: RightHandIKTransform (Component Space)
    ↓
[Two-Bone IK: hand_l]    ← 왼손 그립 보정
    IK Bone: hand_l
    Effector: LeftHandIKTransform (Component Space)
    ↓
[DefaultSlot]
```

> **IK 비활성화 조건**: 무기 미장착 시 IK Transform이 없으므로 `bHasWeapon` 조건으로 IK Weight를 0으로 설정해야 함.
> AnimGraph에서 Two-Bone IK 노드의 Alpha 핀에 `bHasWeapon` (0 또는 1) 연결.

---

### 7-2. Turn In Place (제자리 회전)

마우스로 시점을 돌릴 때 다리는 버티다가 임계값 초과 시 발을 돌려 재정렬.

**EPAnimInstance에 추가:**
```cpp
// EPAnimInstance.h
UPROPERTY(BlueprintReadOnly, Category = "TurnInPlace")
float RootYawOffset = 0.f;      // AnimGraph Rotate Root Bone에 연결

private:
float PreviousActorYaw = 0.f;

// EPAnimInstance.cpp NativeInitializeAnimation()
if (AEPCharacter* Char = CachedCharacter.Get())
    PreviousActorYaw = Char->GetActorRotation().Yaw;

// EPAnimInstance.cpp NativeUpdateAnimation() 끝에 추가
float CurrentYaw = Character->GetActorRotation().Yaw;
float YawDelta = FMath::FindDeltaAngleDegrees(PreviousActorYaw, CurrentYaw);
PreviousActorYaw = CurrentYaw;

// 너무 작은 delta는 jitter 방지용 데드존
if (FMath::Abs(YawDelta) < 0.5f) YawDelta = 0.f;

if (Speed > 10.f)
{
    // 이동 중: 빠르게 정렬
    RootYawOffset = FMath::FInterpTo(RootYawOffset, 0.f, DeltaSeconds, 8.f);
}
else
{
    // 정지 중: Actor 회전만큼 메시가 버팀, ±90° 클램프
    RootYawOffset = FMath::ClampAngle(RootYawOffset - YawDelta, -90.f, 90.f);
}
```

**AnimGraph 배치:**
- Locomotion SM 직후, Layered Blend Per Bone 이전에 Rotate Root Bone 배치
- Rotate Root Bone: Yaw 핀에 `RootYawOffset` 연결
- 이렇게 해야 하체만 오프셋되고 상체는 AimOffset이 복원

**Turn 애니메이션 (선택, 추후):**
- `|RootYawOffset| >= 90°` 일 때 Turn_Left/Turn_Right 애니메이션 재생 후 offset 리셋
- Root motion이 있는 Turn 시퀀스 필요 (MM_TurnLeft_90, MM_TurnRight_90)

---

### 7-3. 시작/정지 애니메이션 (Start / Stop)

이동 시작과 정지 시 발이 미끄러지는 느낌을 없애는 짧은 전환 애니메이션.

**단순 State Machine 방식 (현재 단계):**

Locomotion SM에 JogStart / JogStop 상태 추가:
```
Idle
 └─→ [JogStart 0.2s]  (Speed > 10 진입 시)
       └─→ Walk/Run
             └─→ [JogStop 0.2s]  (Speed < 10 진입 시)
                   └─→ Idle
```
- JogStart / JogStop은 Automatic Rule Based on Sequence Player 체크
- 애니메이션: MM_Jog_Start, MM_Jog_Stop (리타겟 필요)

**Distance Matching 방식 (Lyra 방식, 추후):**
- `AnimationWarping` + `MotionWarping` 플러그인 필요
- `Predict Ground Movement Stop Location` 노드로 정지 예상 지점 계산
- Stride Warping으로 발자국을 실제 이동거리에 맞게 조정
- Lyra의 `UAnimDistanceMatchingLibrary` 참조

---

### 7-4. Orientation Warping (방향 워핑)

스트레이핑 시 발이 이동 방향을 향하도록 하체를 워핑. Lyra에서 발 미끄러짐을 없애는 핵심 시스템.

**활성화:**
- Project Settings → Plugins → **Animation Warping** 활성화
- AnimGraph에서 `Orientation Warping` 노드 사용 가능

**노드 핀 연결:**

| 핀 | 연결 | 설명 |
|---|---|---|
| Component Pose | Locomotion SM 출력 | 필수 |
| Alpha | 1.0 (상수) | 워핑 강도. 이동 중에만 1.0으로 해도 됨 |
| Orientation Angle | `Direction` 변수 | 이동 방향 각도 (-180~180) |
| Target Time | 연결 안 함 | Distance Matching용, 현재 단계 불필요 |
| Current Anim Asset | 연결 안 함 | Distance Matching용, 현재 단계 불필요 |
| Current Anim Asset Time | 연결 안 함 | Distance Matching용, 현재 단계 불필요 |

**노드 Details 패널 설정 (핀이 아닌 설정값):**

노드를 선택하면 왼쪽 Details에 Bone 설정이 나온다. 여기서 경고가 발생하므로 반드시 지정해야 함.

| 항목 | MetaHuman 본 이름 |
|---|---|
| **Spine Bone** | `spine_01` |
| **IK Foot Root Bone** | `ik_foot_root` |
| **IK Foot Bones** (Left) | `ik_foot_l` |
| **IK Foot Bones** (Right) | `ik_foot_r` |

> MetaHuman Body 스켈레톤은 Mannequin 기반이라 위 IK 본들이 존재한다.
> Skeleton Tree에서 확인 가능 (`ik_foot_root` → `ik_foot_l`, `ik_foot_r` 구조).
> 만약 존재하지 않으면 Spine Bone만 설정하면 발 IK 없이 상/하체 분리는 동작한다.

**AnimGraph 배치:**
```
[Locomotion SM]
    ↓
[Orientation Warping]   ← Direction 연결, Details에서 본 이름 설정
    ↓
[Rotate Root Bone]
    ↓
...
```

**효과:**
- 이동 방향으로 발/다리가 자연스럽게 향함
- 상체(spine_01 이상)는 워핑 대상에서 제외 → AimOffset이 담당

---

### 7-5. AimOffset 적용 구조

**현재 문제**: AimOffset을 Rotate Root Bone 이후에 적용하면 상체가 root offset에 따라 틀어짐.

**올바른 구조:**
```
[Rotate Root Bone]           ← 하체용 root 오프셋
    ↓
[Cache: LocomotionWithTurn]
    ↓
[Layered Blend Per Bone]     ← spine_01 이상에 AimOffset 오버라이드
    Base: Cached Pose
    Blend 0: AimOffset (AimPitch, AimYaw)
```

Layered Blend Per Bone 설정:
- Branch Filters → `spine_01`, Blend Depth: 0
- Blend Mode: Linear (Weight 1.0)

---

### 7-6. Jump/Land 상태 머신

Jump SM 내 상태와 전환 조건:

| 상태 | 시퀀스 | 전환 조건 |
|---|---|---|
| JumpStart | MM_Jump_Start | bIsFalling == true (진입), Sequence 끝나면 InAir |
| InAir | MM_Jump_InAir (loop) | bIsFalling == false → Land |
| Land | MM_Jump_Land | Sequence 끝나면 Grounded |

- JumpStart → InAir: **Automatic Rule Based on Sequence Player** 체크
- Land → Grounded: **Automatic Rule Based on Sequence Player** 체크
- InAir → Land: bIsFalling == false 조건

## 9. 구현 우선순위

### Phase 1 — 기본 동작 (현재)
1. WeaponSocket → hand_r 소켓 추가 (총 잡기)
2. ABP_EPCharacter AnimGraph 노드 순서 수정 (Rotate Root Bone 위치)
3. Layered Blend Per Bone spine_01 설정 확인
4. Jump SM (JumpStart/InAir/Land) 연결
5. HUD 크로스헤어 추가

### Phase 2 — 자연스러운 이동
6. RootYawOffset + Rotate Root Bone (Turn In Place 기본)
7. Two-Bone IK (왼손 그립)
8. JogStart / JogStop 상태 추가

### Phase 3 — Lyra 수준 polish (GAS 이후)
9. Orientation Warping 플러그인 활성화 및 적용
10. Distance Matching (Start/Stop 정밀 제어)
11. Turn 애니메이션 (MM_TurnLeft_90, MM_TurnRight_90)

## 10. 주의사항

- **같은 스켈레톤**: 메인 AnimBP와 무기 AnimBP는 반드시 같은 Skeleton 사용
- **LinkAnimClassLayers 타이밍**: 서버 EquipWeapon + 클라이언트 OnRep 양쪽에서 호출
- **BlendSpace Player vs Sequence Player**: BlendSpace 할당 레이어는 반드시 BlendSpace Player 노드 사용
- **CalculateDirection deprecated**: `UKismetAnimationLibrary::CalculateDirection` 사용 (AnimGraphRuntime 모듈 필요)
- **AimPitch 복제**: ACharacter 내부 RemoteViewPitch로 자동 복제됨
- **Fire 몽타주**: DefaultSlot 통해 상체만 영향. Multicast RPC에서 재생
- **Thread-Safe**: Property Access는 워커 스레드에서 안전. NativeUpdateAnimation은 게임 스레드
