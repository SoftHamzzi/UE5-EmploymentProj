# Post 2-7 작성 가이드 — 애니메이션 시스템 (리타게팅 + Linked Anim Layer)

> **예상 제목**: `[UE5] 추출 슈터 2-7. 애니메이션 시스템 — Lyra 리타게팅부터 Linked Anim Layer까지`
> **참고 문서**: DOCS/Mine/Animation.md, DOCS/Mine/Crosshair.md
> **스크린샷**: ref1.png (AnimGraph 전체), ref2.png (Locomotion 상태 머신)

---

## 개요

**이 포스팅에서 다루는 것:**
- Lyra 애니메이션을 MetaHuman 스켈레톤에 리타게팅하는 방법
- Lyra 스타일 Linked Anim Layer 구조로 무기별 애니메이션 교체
- FABRIK 왼손 IK + 크로스헤어 HUD

**왜 이 구조인가:**
- Lyra: Epic 공식 샘플, 라이선스 문제 없음, 퀄리티 높은 라이플 애니메이션 보유
- Linked Anim Layer: 무기 추가 시 새 무기 AnimBP만 만들면 됨. 메인 AnimBP 수정 불필요

---

## 구현 전 상태 (Before)

- MetaHuman 스켈레톤에 Lyra 애니메이션 직접 할당 불가 (스켈레톤 구조 다름)
- AnimBP 없음 → 캐릭터 T-포즈

---

## 구현 내용

### 1. Lyra 애니메이션 리타게팅

**IK Retargeter 2단계:**

```
[1단계] IK Rig — 스켈레톤의 본 체인을 의미적으로 정의
  Lyra:      IK_Mannequin (Lyra 내장)
  MetaHuman: MetaHuman 플러그인 내장 IK Rig 사용

[2단계] IK Retargeter 생성
  Source: IK_Mannequin
  Target: MetaHuman Body IK Rig
  → Export Selected Animations으로 리타겟 시퀀스 추출
```

**BlendSpace / AimOffset은 직접 재구성:**
- IK Retargeter로 직접 리타겟 불가
- 리타겟된 개별 시퀀스들로 새 BlendSpace/AimOffset 수동 생성

> **스크린샷**: IK Retargeter 프리뷰 (Source Mannequin vs Target MetaHuman 비교)

---

**### 2. Linked Anim Layer 전체 구조

```
┌─────────────────────────────────────────────┐
│ ABP_EPCharacter (메인 AnimBP)               │
│  Parent C++: UEPAnimInstance                │
│  Implements: ALI_EPWeapon                   │
│                                             │
│  ┌─ Locomotion StateMachine ─────────────┐  │
│  │  IdleWalkJog ←→ Crouch               │  │
│  │       ↕ JumpSources / Jump            │  │
│  └───────────────────────────────────────┘  │
│                                             │
│  ┌─ Linked Anim Layer 슬롯 (빈 슬롯) ───┐  │
│  │  FullBody_IdleWalkRun  ← 빈 슬롯     │  │
│  │  FullBody_Crouch       ← 빈 슬롯     │  │
│  │  FullBody_Jump         ← 빈 슬롯     │  │
│  └───────────────────────────────────────┘  │
│                                             │
│  AimOffset: AimPitch / AimYaw               │
│  DefaultSlot (Fire, Reload 몽타주)          │
└─────────────────────────────────────────────┘
              ↑ LinkAnimClassLayers()
┌─────────────────────────────────────────────┐
│ ABP_EPWeaponAnimLayersBase                  │
│  Parent C++: UEPWeaponAnimInstance          │
│  Implements: ALI_EPWeapon                   │
│                                             │
│  각 레이어 함수에 Sequence/BS Player 노드   │
│  → Promote to Asset Override으로 추상화     │
└─────────────────────────────────────────────┘
              ↑ 상속
┌─────────────────────────────────────────────┐
│ ABP_RifleAnimLayers (무기 AnimBP)           │
│                                             │
│  AnimGraphOverrides (에셋 오버라이드 에디터) │
│  FullBody_IdleWalkRun → BS_Rifle_IdleWalkJog│
│  FullBody_Crouch      → 라이플 앉기 시퀀스  │
│  FullBody_Jump        → 라이플 점프 시퀀스  │
└─────────────────────────────────────────────┘
```

**3계층 설계의 핵심:**
- `ABP_EPCharacter`: 슬롯만 정의, 에셋 모름
- `ABP_EPWeaponAnimLayersBase`: 레이어 구현 구조 정의, `Promote to Asset Override`로 에셋 추상화
- `ABP_RifleAnimLayers`: 실제 에셋만 꽂음 — 새 무기 추가 시 이 파일만 새로 만들면 됨

**무기 교체 = 한 줄:**
```cpp
// OnRep_EquippedWeapon에서
Owner->GetMesh()->LinkAnimClassLayers(NewWeapon->WeaponDef->WeaponAnimLayer);
```

---

### 3. Locomotion 상태 머신

> **스크린샷**: ref2.png (IdleWalkJog / Crouch / JumpSources / Jump)

상태 4개:

| State | 호출 레이어 | 전환 조건 |
|-------|------------|----------|
| `IdleWalkJog` | `FullBody_IdleWalkRun` | 기본 상태 |
| `Crouch` | `FullBody_Crouch` | bIsCrouching |
| `JumpSources` | — | 점프 입력 |
| `Jump` | `FullBody_Jump` | 공중 상태 |

**레이어 함수 중복 배치 금지:**
```
❌ Idle State → FullBody_IdleWalkRun
   Walk State → FullBody_IdleWalkRun  ← 같은 레이어 중복, 의미 없음
   Jog State  → FullBody_IdleWalkRun  ← UE5 제약: 레이어당 1인스턴스

✅ IdleWalkJog 단일 State → FullBody_IdleWalkRun 1회
   레이어 내부 BlendSpace가 Speed로 Idle→Walk→Jog 블렌딩
```

---

### 4. AnimGraph 노드 흐름

> **스크린샷**: ref1.png (AnimGraph 전체)

```
[Locomotion State Machine]
    ↓
[Rotate Root Bone: RootYawOffset]   ← 향후 Turn In Place 용
    ↓
[Cache Pose: "Locomotion"]
    ↓
[Layered Blend Per Bone: spine_01]  ← 상체에만 AimOffset 적용
  Base: Cached Locomotion
  Blend 0: AimOffset (AimPitch / AimYaw)
    ↓
[FABRIK: 왼손 IK]
    ↓
[DefaultSlot]                       ← Fire, Reload 몽타주
    ↓
[Output Pose]
```

**Rotate Root Bone을 Layered Blend Per Bone 앞에 두는 이유:**
하체 Root 오프셋이 상체(spine_01 이상)에 전달되기 전에 AimOffset이 상체를 복원함. 뒤에 두면 총도 같이 틀어짐.

---

### 5. Property Access (Thread-Safe)

무기 AnimBP에서 메인 AnimBP 변수를 읽는 UE5 권장 방식:

```
ABP_RifleAnimLayers AnimGraph:
  Get Main Anim Blueprint Thread Safe
    → (ABP_EPCharacter로 캐스트)
    → .Speed     → BlendSpace X축
    → .Direction → BlendSpace Y축
    → .bIsAiming → 조건 분기
```

중복 변수 선언 없이 워커 스레드 안전하게 값 복사.

---

### 6. FABRIK 왼손 IK

오른손은 애니메이션 자체가 그립 기준으로 제작됨 → IK 불필요.
왼손만 FABRIK으로 무기 소켓 위치로 보정:

```
무기 메시 소켓: LeftHandIK (손잡이 위치)

NativeUpdateAnimation():
  LeftHandIKTransform = WeaponMesh->GetSocketTransform("LeftHandIK");

FABRIK 노드:
  Effector Transform Space: BCS_WorldSpace
  Effector Transform: LeftHandIKTransform
  Tip Bone: hand_l
  Root Bone: lowerarm_l
```

> **스크린샷**: 무기 메시 에디터 LeftHandIK 소켓 위치

---

### 7. 크로스헤어 HUD

```cpp
// EPPlayerController.cpp — IsLocalController() 체크 필수
// PlayerController는 서버에도 존재 → 서버에서 HUD 생성 방지
if (IsLocalController())
{
    CrosshairWidget = CreateWidget<UEPCrosshairWidget>(this, CrosshairWidgetClass);
    CrosshairWidget->AddToViewport();
}
```

WBP_Crosshair: 중앙 점 + 상하좌우 라인 4개. `CurrentSpread` 값에 따라 라인 간격 조정.

---

## 결과

- 이동 방향에 맞는 애니메이션 (Forward/Strafe/Backward)
- 상하 시야에 따라 상체(팔/총)만 AimOffset 적용
- 무기 장착 시 왼손이 LeftHandIK 소켓으로 이동
- 화면 중앙 크로스헤어 표시

**향후 개선 (GAS 이후):**
- Turn In Place (임계값 초과 시 발 재정렬)
- JogStart/JogStop + Distance Matching (이동 시작/정지 미끄러짐 제거)**

---

## 참고

- `DOCS/Mine/Animation.md`
- `DOCS/Mine/Crosshair.md`
- UE5 Linked Anim Layer 공식 문서
