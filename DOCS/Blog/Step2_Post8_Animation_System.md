# Post 2-8 작성 가이드 — 애니메이션 시스템 구현

> **예상 제목**: `[UE5] 추출 슈터 2-8. Lyra 스타일 Linked Anim Layer + FABRIK 왼손 IK + 크로스헤어`
> **참고 문서**: DOCS/Mine/Animation.md 전체, DOCS/Mine/Crosshair.md

---

## 개요

**이 포스팅에서 다루는 것:**
- Lyra 스타일 Linked Anim Layer 구조 구현 (무기 교체 시 애니메이션 레이어 교체)
- AimOffset, Orientation Warping, FABRIK 왼손 IK 적용
- 크로스헤어 HUD 구현

**왜 이렇게 구현했는가 (설계 의도):**
- 무기마다 AnimBP를 통째로 교체하면 다중 무기 지원 시 유지보수 불가
- Lyra의 Layer Interface 방식은 무기 AnimBP만 교체해도 전체 애니메이션이 바뀜
- Property Access로 Thread-Safe하게 메인 AnimBP 변수를 무기 AnimBP에서 읽음

---

## 구현 전 상태 (Before)

- 리타게팅된 시퀀스는 있지만 AnimBP가 없어 캐릭터가 T-포즈
- 무기를 들어도 이동/조준 애니메이션이 없음

---

## 구현 내용

### 1. 전체 아키텍처 개요

다이어그램을 반드시 보여줄 것:

```
[캐릭터 메시에 적용]
ABP_EPCharacter (메인 AnimBP)
│  Parent C++: UEPAnimInstance
│  역할: 이동/조준 상태 관리, 레이어 슬롯 제공
│
│  [Linked Anim Layer 슬롯]
│  FullBody_IdleWalkRun  ← 비어있음 (무기 AnimBP가 채움)
│  FullBody_Crouch       ← 비어있음
│  FullBody_Jump         ← 비어있음
│
└── LinkAnimClassLayers(ABP_RifleAnimLayers) ─→ 슬롯이 채워짐

ABP_RifleAnimLayers (라이플 무기 AnimBP)
│  Parent: ABP_EPWeaponAnimLayersBase
│  역할: ALI_EPWeapon 구현, 리타겟된 에셋 할당
│
│  GetMainAnimBPThreadSafe() → ABP_EPCharacter
│      └─ Speed, Direction → BlendSpace Player에 연결
```

**무기 교체 시:**
```cpp
// CombatComponent::EquipWeapon()
Owner->GetMesh()->LinkAnimClassLayers(NewWeapon->WeaponDef->WeaponAnimLayer);
// → ABP_RifleAnimLayers가 모든 슬롯을 채움
// → 무기 교체 = 애니메이션 교체 (단 한 줄)
```

### 2. ALI_EPWeapon (Animation Layer Interface)

에디터: Content Browser → Animation → Animation Layer Interface

```
ALI_EPWeapon:
├── FullBody_IdleWalkRun()   → 이동 애니메이션
├── FullBody_Crouch()        → 앉기 애니메이션
├── FullBody_Jump()          → 점프 애니메이션
└── UpperBody_AimFire()      → 조준/사격 상체
```

> **스크린샷 위치**: ALI 에디터에서 레이어 함수 목록

### 3. UEPAnimInstance (메인 AnimBP C++ 백엔드)

```cpp
// EPAnimInstance.h — 핵심 변수만 표시
UPROPERTY(BlueprintReadOnly, Category = "Movement")
float Speed = 0.f;

UPROPERTY(BlueprintReadOnly, Category = "Movement")
float Direction = 0.f;

UPROPERTY(BlueprintReadOnly, Category = "Combat")
bool bIsAiming = false;

UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
float AimPitch = 0.f;

UPROPERTY(BlueprintReadOnly, Category = "AimOffset")
float AimYaw = 0.f;

// EPAnimInstance.cpp — NativeUpdateAnimation 핵심
Direction = UKismetAnimationLibrary::CalculateDirection(Velocity, Character->GetActorRotation());
bIsSprinting = Character->GetIsSprinting();  // CMC에서 직접 읽음
AimPitch = FMath::ClampAngle(AimRotation.Pitch, -90.f, 90.f);
AimYaw = FMath::ClampAngle(
    FRotator::NormalizeAxis(AimRotation.Yaw - Character->GetActorRotation().Yaw),
    -90.f, 90.f);
```

**주의**: `UKismetAnimationLibrary::CalculateDirection` 사용하려면 Build.cs에 `AnimGraphRuntime` 모듈 추가 필요

### 4. ABP_EPCharacter AnimGraph 노드 순서

순서가 중요하므로 다이어그램으로 표현:

```
[Locomotion State Machine]
    └─ Grounded State: FullBody_IdleWalkRun 레이어 1회 호출
       (Idle/Walk/Jog를 단일 State + BlendSpace로 통합)
    ↓
[Orientation Warping]
  ├─ Direction 연결
  └─ Details: Spine Bone(spine_01), IK Foot Root(ik_foot_root), IK Feet(ik_foot_l/r)
    ↓
[Rotate Root Bone: Yaw = RootYawOffset]   ← 제자리 회전 오프셋 (향후)
    ↓
[Cache Pose: "Locomotion"]
    ↓
[Layered Blend Per Bone]                  ← spine_01 이상에 AimOffset 적용
  Base: Cached Locomotion
  Blend 0: AimOffset (Pitch=AimPitch, Yaw=AimYaw, Base=Cached)
    ↓
[FABRIK: 왼손 IK]
    ↓
[DefaultSlot]                             ← Fire, Reload, Death 몽타주
    ↓
[Output Pose]
```

**Rotate Root Bone 위치가 왜 Layered Blend Per Bone 앞인가:**
> Rotate Root Bone은 하체의 Root를 오프셋함.
> Layered Blend Per Bone 앞에 두면 상체(spine_01 이상)에는 오프셋이 전달되지 않고,
> AimOffset이 상체를 정면 방향으로 복원함.
> 뒤에 두면 팔/총도 같이 오프셋되어 이상하게 보임.

> **스크린샷 위치**: AnimGraph 전체 노드 연결 화면

### 5. Linked Anim Layer 제약 — 단일 Grounded State 이유

**독자가 처음에 흔히 하는 실수:**
```
❌ 잘못된 방법:
Idle State → FullBody_IdleWalkRun 레이어
Walk State → FullBody_IdleWalkRun 레이어  ← 같은 레이어 중복!
Jog State  → FullBody_IdleWalkRun 레이어  ← 중복!
```

**UE5 제약**: 같은 레이어 함수는 ABP 당 1개 인스턴스만 생성됨 → 중복 배치는 의미 없음

```
✅ 올바른 방법:
단일 Grounded State → FullBody_IdleWalkRun 레이어 1회만
    └─ 레이어 내부 BlendSpace Player가 Speed/Direction으로 Idle→Walk→Jog 블렌딩
```

> **스크린샷 위치**: Locomotion SM에서 Grounded 단일 State 구조

### 6. Property Access (Thread-Safe)

무기 AnimBP에서 메인 AnimBP 변수 읽기:

```
ABP_EPWeaponAnimLayersBase AnimGraph 안에서:
1. Get Main Anim Blueprint Thread Safe 노드
2. Cast to ABP_EPCharacter
3. .Speed → BlendSpace Player X축
4. .Direction → BlendSpace Player Y축
5. .bIsAiming → 조건 분기
```

**왜 C++에서 복사하지 않는가:**
- Property Access는 UE5가 내부적으로 워커 스레드 안전 타이밍에 값을 복사
- EPWeaponAnimInstance에 중복 변수 선언 불필요
- NativeUpdateAnimation의 게임 스레드보다 효율적

### 7. Orientation Warping 설정

```
노드 핀:
  Component Pose → Locomotion SM 출력
  Alpha → 1.0
  Orientation Angle → Direction 변수

Details 패널 (핀이 아닌 설정값 — 경고가 여기서 해결됨):
  Spine Bone: spine_01
  IK Foot Root: ik_foot_root
  IK Foot Bones: ik_foot_l, ik_foot_r
```

**Target Time / Current Anim Asset / Current Anim Asset Time:**
- Distance Matching용 핀 → 현재 단계에서는 연결 불필요

### 8. FABRIK 왼손 IK

**오른손 IK는 필요 없는 이유:**
- 애니메이션 자체가 오른손 그립 위치로 제작됨 (Lyra 방식)
- 오른손 IK 추가 시 오히려 부자연스러워짐

**왼손만 FABRIK으로 보정:**

```
무기 메시에 소켓 추가:
  └─ LeftHandIK (손잡이 위치에 배치)

EPAnimInstance.cpp NativeUpdateAnimation():
// 왼손 IK Transform 계산
FTransform WorldLeftHandIK = WeaponMesh->GetSocketTransform(FName("LeftHandIK"));
// WorldSpace 그대로 FABRIK에 전달
LeftHandIKTransform = WorldLeftHandIK;

AnimGraph FABRIK 노드 설정:
  Effector Transform Space: BCS_WorldSpace
  Effector Transform: LeftHandIKTransform
  Tip Bone: hand_l (또는 middle_01_l)
  Root Bone: lowerarm_l
```

**hand_l을 Tip Bone으로 쓸 때:**
- 손목 기준이라 완벽하지 않음
- 소켓 위치로 미세 조정 가능 (소켓 위치 = 결국 hand_l이 도착할 위치)

> **스크린샷 위치**: 무기 메시 에디터에서 LeftHandIK 소켓 위치, AnimGraph FABRIK 노드

### 9. 크로스헤어 HUD

```cpp
// EPCrosshairWidget.h
UCLASS()
class UEPCrosshairWidget : public UUserWidget
{
    UPROPERTY(BlueprintReadWrite, Category = "Crosshair")
    float CrosshairSpread = 0.f;
};

// EPPlayerController.cpp — BeginPlay
if (IsLocalController() && CrosshairWidgetClass)
{
    CrosshairWidget = CreateWidget<UEPCrosshairWidget>(this, CrosshairWidgetClass);
    if (CrosshairWidget) CrosshairWidget->AddToViewport();
}
```

**IsLocalController()가 필요한 이유:**
- PlayerController는 서버에도 존재 → 서버에서 BeginPlay 실행 시 HUD 생성 시도 → 충돌
- 다른 클라이언트의 PlayerController도 서버에 존재

**WBP_Crosshair 구성:**
```
Canvas Panel
├─ Center Dot (4x4 흰색)
├─ Line Top    (앵커 중앙, Y: -12)
├─ Line Bottom (앵커 중앙, Y: +12)
├─ Line Left   (앵커 중앙, X: -12)
└─ Line Right  (앵커 중앙, X: +12)
```

CrosshairSpread에 따라 각 라인 Position 오프셋 조정:
```
Line Top Position Y = -12 - (CrosshairSpread * 20)
```

> **스크린샷 위치**: 화면에 크로스헤어가 표시된 모습, WBP_Crosshair 에디터

---

## 결과

**확인 항목:**
- 캐릭터가 이동 시 방향에 맞는 애니메이션 재생 (Forward/Backward/Strafe)
- 시야 상하 이동 시 상체(팔/총)만 AimOffset에 따라 움직임, 하체는 유지
- 무기 장착 시 왼손이 LeftHandIK 소켓 위치로 이동
- 화면 중앙에 크로스헤어 표시

**한계 및 향후 개선:**
- 왼손 그립이 정확하지 않을 수 있음 (소켓 위치 수동 조정 필요)
- Turn In Place 미완성 (임계값 초과 시 발 재정렬 없음) → GAS 이후 구현 예정
- JogStart/JogStop 없음 → 이동 시작/정지 시 발 미끄러짐 → Distance Matching으로 개선 예정

---

## 참고

- `DOCS/Mine/Animation.md` — 전체 설계
- `DOCS/Mine/Crosshair.md` — 크로스헤어 설계
- Lyra Sample Project 애니메이션 시스템 분석
- UE5 Linked Anim Layer 공식 문서
