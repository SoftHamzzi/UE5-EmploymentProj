# Animation 시스템 설계 (Lyra 스타일 Linked Anim Layer)

## 1. 아키텍처 개요

```
┌─────────────────────────────────────────────┐
│ ABP_EPCharacter (메인 AnimBP)               │
│  Parent C++: UEPAnimInstance                │
│                                             │
│  ┌─ Locomotion StateMachine ──────────────┐ │
│  │  Idle ←→ Walk ←→ Run ←→ Sprint        │ │
│  │  ↕ Jump/Fall ↕ Crouch                  │ │
│  └────────────────────────────────────────┘ │
│                                             │
│  ┌─ Linked Anim Layers (ALI_EPWeapon) ───┐ │
│  │  FullBody_IdleWalkRun  ← 빈 슬롯      │ │
│  │  UpperBody_AimFire     ← 빈 슬롯      │ │
│  │  FullBody_Jump         ← 빈 슬롯      │ │
│  └────────────────────────────────────────┘ │
│                                             │
│  Aim Offset: AimPitch / AimYaw              │
└─────────────────────────────────────────────┘
           ↑ LinkAnimClassLayers()
┌─────────────────────────────────────────────┐
│ ABP_Rifle (무기 AnimBP)                     │
│  Parent C++: UEPWeaponAnimInstance          │
│  Implements: ALI_EPWeapon                   │
│                                             │
│  FullBody_IdleWalkRun  → 라이플 이동 애님   │
│  UpperBody_AimFire     → 라이플 조준/사격   │
│  FullBody_Jump         → 라이플 점프/낙하   │
└─────────────────────────────────────────────┘
```

무기 교체 시 `LinkAnimClassLayers(ABP_Pistol)`로 교체하면 모든 레이어가 한번에 바뀜.

## 2. C++ 클래스

### 2-1. UEPAnimInstance

메인 AnimBP의 C++ 백엔드. 캐릭터/CMC에서 상태를 읽어 AnimBP에 전달.

```
Public/Animation/EPAnimInstance.h
Private/Animation/EPAnimInstance.cpp
```

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

protected:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // --- Locomotion ---
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

    // --- Combat ---
    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bIsAiming = false;

    // --- Aim Offset ---
    UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
    float AimPitch = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
    float AimYaw = 0.f;

private:
    TWeakObjectPtr<class AEPCharacter> CachedCharacter;
};
```

```cpp
// EPAnimInstance.cpp
#include "Animation/EPAnimInstance.h"
#include "Core/EPCharacter.h"
#include "Movement/EPCharacterMovement.h"

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

    // Locomotion
    FVector Velocity = Character->GetVelocity();
    Speed = Velocity.Size2D();

    FRotator AimRotation = Character->GetBaseAimRotation();
    Direction = CalculateDirection(Velocity, Character->GetActorRotation());

    bIsSprinting = Character->GetIsSprinting();
    bIsAiming = Character->GetIsAiming();
    bIsFalling = Character->GetCharacterMovement()->IsFalling();
    bIsCrouching = Character->bIsCrouched;

    // Aim Offset
    AimPitch = FMath::ClampAngle(AimRotation.Pitch, -90.f, 90.f);
    AimYaw = FMath::ClampAngle(
        FRotator::NormalizeAxis(AimRotation.Yaw - Character->GetActorRotation().Yaw),
        -90.f, 90.f);
}
```

### 2-2. UEPWeaponAnimInstance

무기별 AnimBP의 C++ 베이스. 무기 상태를 읽음.

```
Public/Animation/EPWeaponAnimInstance.h
Private/Animation/EPWeaponAnimInstance.cpp
```

```cpp
// EPWeaponAnimInstance.h
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EPWeaponAnimInstance.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API UEPWeaponAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

protected:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    // 메인 AnimBP에서 복사해올 변수 (레이어 내에서 사용)
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
};
```

```cpp
// EPWeaponAnimInstance.cpp
#include "Animation/EPWeaponAnimInstance.h"
#include "Animation/EPAnimInstance.h"

void UEPWeaponAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // 메인 AnimBP에서 변수 복사
    // GetLinkedAnimLayerInstanceByGroup 또는 GetOwningComponent로 메인 인스턴스 접근
    USkeletalMeshComponent* MeshComp = GetOwningComponent();
    if (!MeshComp) return;

    UEPAnimInstance* MainAnim = Cast<UEPAnimInstance>(MeshComp->GetAnimInstance());
    if (!MainAnim) return;

    // MainAnim의 Getter를 통해 복사 (Getter 추가 필요)
    // 또는 Property Access를 AnimBP에서 직접 사용
}
```

> 참고: Lyra에서는 Weapon AnimBP가 메인 AnimBP의 변수를 **Property Access** 노드로 직접 읽는다.
> C++에서 복사하는 것보다 BP에서 Property Access가 더 실무적이다.
> C++에는 최소 구조만 두고, 변수 접근은 BP Property Access로 처리.

### 2-3. EPWeaponData — AnimLayer 추가

```cpp
// EPWeaponData.h에 추가
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
TSubclassOf<UAnimInstance> WeaponAnimLayer;
```

### 2-4. CombatComponent — AnimLayer 링크

```cpp
// EPCombatComponent.cpp — SetEquippedWeapon 또는 EquipWeapon에서
void UEPCombatComponent::SetEquippedWeapon(AEPWeapon* Weapon)
{
    EquippedWeapon = Weapon;

    // AnimLayer 링크
    AEPCharacter* Owner = GetOwnerCharacter();
    if (Owner && Weapon && Weapon->WeaponData)
    {
        if (TSubclassOf<UAnimInstance> AnimLayer = Weapon->WeaponData->WeaponAnimLayer)
        {
            Owner->GetMesh()->LinkAnimClassLayers(AnimLayer);
        }
    }
}
```

무기 해제 시:
```cpp
void UEPCombatComponent::UnequipWeapon()
{
    AEPCharacter* Owner = GetOwnerCharacter();
    if (Owner && EquippedWeapon && EquippedWeapon->WeaponData)
    {
        if (TSubclassOf<UAnimInstance> AnimLayer = EquippedWeapon->WeaponData->WeaponAnimLayer)
        {
            Owner->GetMesh()->UnlinkAnimClassLayers(AnimLayer);
        }
    }
    EquippedWeapon = nullptr;
}
```

## 3. Blueprint 에셋

### 3-1. ALI_EPWeapon (Animation Layer Interface)

에디터에서 생성: Content Browser → Animation → Animation Layer Interface

레이어 함수 정의:

| 레이어 함수 | 용도 |
|---|---|
| `FullBody_IdleWalkRun` | 무기 들고 대기/걷기/뛰기 |
| `UpperBody_AimFire` | 조준/사격 상체 |
| `FullBody_Jump` | 점프/낙하 |
| `FullBody_Crouch` | 앉기 이동 |

### 3-2. ABP_EPCharacter (메인 AnimBP)

- **Parent Class**: UEPAnimInstance
- **Skeleton**: 메타휴먼 Body 스켈레톤
- **Implements**: ALI_EPWeapon (레이어 슬롯 생성용)

구조:
```
AnimGraph
├── Locomotion StateMachine
│   ├── Idle/Walk/Run → Linked Anim Layer: FullBody_IdleWalkRun
│   ├── Sprint → Speed 기반 블렌드
│   ├── Jump → Linked Anim Layer: FullBody_Jump
│   └── Crouch → Linked Anim Layer: FullBody_Crouch
├── Aim Offset (AimPitch, AimYaw)
├── Upper Body Slot → Linked Anim Layer: UpperBody_AimFire
└── Montage Slot (사격 반동 등)
```

### 3-3. ABP_Rifle (무기별 AnimBP)

- **Parent Class**: UEPWeaponAnimInstance
- **Skeleton**: 메타휴먼 Body 스켈레톤 (같은 스켈레톤이어야 함)
- **Implements**: ALI_EPWeapon

각 레이어 함수에 라이플 애니메이션 할당:
- `FullBody_IdleWalkRun` → 라이플 들고 걷기/뛰기 블렌드스페이스
- `UpperBody_AimFire` → 라이플 조준 포즈 + Aim Offset
- `FullBody_Jump` → 라이플 점프/낙하
- `FullBody_Crouch` → 라이플 앉기 이동

### 3-4. 에셋 배치

```
Content/Animation/
├── ALI_EPWeapon                    (Animation Layer Interface)
├── ABP_EPCharacter                 (메인 AnimBP)
├── Locomotion/
│   ├── BS_IdleWalkRun              (BlendSpace - Speed, Direction)
│   └── BS_CrouchWalkRun            (BlendSpace - 앉기)
└── Weapons/
    └── Rifle/
        ├── ABP_Rifle               (무기 AnimBP)
        ├── BS_Rifle_IdleWalkRun    (라이플 BlendSpace)
        └── AM_Rifle_Fire           (사격 몽타주)
```

## 4. 데이터 흐름

```
매 프레임:
Character/CMC
    → UEPAnimInstance::NativeUpdateAnimation()
        → Speed, Direction, bIsSprinting, bIsAiming, AimPitch 등 갱신
        → AnimBP StateMachine이 변수 기반으로 State 전환
        → Linked Anim Layer가 현재 링크된 무기 AnimBP의 애니메이션 재생

무기 장착:
CombatComponent::SetEquippedWeapon()
    → GetMesh()->LinkAnimClassLayers(ABP_Rifle)
    → 메인 AnimBP의 빈 레이어 슬롯이 ABP_Rifle의 구현으로 채워짐

사격:
Multicast_PlayFireEffect()
    → GetMesh()->GetAnimInstance()->Montage_Play(FireMontage)
```

## 5. Property Access (Lyra 방식)

무기 AnimBP(ABP_Rifle)에서 메인 AnimBP의 변수에 접근하는 방법:

C++로 복사하는 대신, **AnimBP 에디터에서 Property Access 노드** 사용:
1. ABP_Rifle의 AnimGraph에서 Property Access 노드 추가
2. `OwningComponent → AnimInstance → Speed` 경로로 메인 AnimBP 변수 직접 읽기
3. 또는 `Linked Anim Layer Instance → 변수` 경로

이 방식이 Lyra 실무 표준. C++에서 매 프레임 변수를 복사하는 오버헤드 없음.

## 6. 구현 순서

1. **C++ 클래스 생성** — EPAnimInstance, EPWeaponAnimInstance
2. **EPWeaponData에 WeaponAnimLayer 추가**
3. **CombatComponent에 LinkAnimClassLayers 추가**
4. **에디터**: ALI_EPWeapon 생성
5. **에디터**: ABP_EPCharacter 생성 (Locomotion StateMachine + Layer 슬롯)
6. **에디터**: ABP_Rifle 생성 (ALI 구현)
7. **에디터**: BlendSpace 생성 (Speed, Direction)
8. **BP_EPCharacter**: Mesh에 ABP_EPCharacter 할당
9. **DA_Rifle**: WeaponAnimLayer에 ABP_Rifle 지정
10. **테스트**: 이동/무기 장착/사격 애니메이션 확인

## 7. 주의사항

- **같은 스켈레톤**: 메인 AnimBP와 무기 AnimBP는 반드시 같은 Skeleton을 사용해야 함
- **LinkAnimClassLayers 타이밍**: 무기 장착 후 즉시 호출. OnRep에서도 호출해야 클라이언트에서 작동
- **NativeUpdateAnimation vs BlueprintUpdateAnimation**: C++이 성능 우위. 매 프레임 호출이므로 C++ 사용
- **AimPitch 복제**: AimPitch는 Character에서 GetBaseAimRotation()으로 얻음. 이미 ACharacter 내부에서 복제됨 (RemoteViewPitch)
- **Fire 몽타주**: Multicast RPC에서 재생. Montage는 상체만 영향 (Montage Slot 사용)
