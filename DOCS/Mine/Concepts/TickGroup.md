# UE5 Tick 실행 순서

> **검증:** 2026-09-01, UE 5.7 소스 직독 (`C:\Program Files\Epic Games\UE_5.7\Engine`).
> 이 문서의 모든 주장은 §인용 색인에 파일:줄로 대응된다.
> 이전 판에서 **여섯 군데를 고쳤다** — §고친 것 참고.

## ETickingGroup 전체 순서

```
UWorld::Tick()
│
├─ [TG_PrePhysics]          ← 기본값. 대부분의 게임플레이 로직
├─ [TG_StartPhysics]        ← 물리 시뮬레이션 시작 (Hidden, 직접 사용 X)
├─ [TG_DuringPhysics]       ← 물리 스레드와 병렬 실행 (물리 결과 불필요한 작업)
├─ [TG_EndPhysics]          ← 물리 스레드 완료 대기 + 결과 커밋 (Hidden, 직접 사용 X)
├─ [TG_PostPhysics]         ← 리지드바디 + 클로스 완료
│     ⋯ UpdateCameraManager (틱 그룹이 아니다. 아래 참고)
├─ [TG_PostUpdateWork]      ← 카메라 업데이트 완료
└─ [TG_LastDemotable]
```

**`TG_PreCloth` / `TG_StartCloth` / `TG_EndCloth`는 UE5에 없다.**
UE4 시절 열거값이고 5.7 `EngineBaseTypes.h:83-110`에는 존재하지 않는다.
클로스는 이제 `USkeletalMeshComponent::ClothTickFunction`이라는 **별도 틱 함수**로 처리된다
(`TickGroup = TG_PrePhysics`, `EndTickGroup = TG_PostPhysics` — `SkeletalMeshComponent.cpp:446-447`).
그래서 `TG_PostPhysics`에서 클로스가 끝나 있다는 결과는 같지만, 경로가 다르다.

| 열거값 | 엔진 주석 | 실제 쓰임 |
|--------|-----------|-----------|
| `TG_PrePhysics` | "물리 시뮬레이션 시작 전에 실행돼야 하는 것" | 기본값. CMC, Character, SkeletalMesh 틱 |
| `TG_StartPhysics` | "물리 시뮬레이션을 시작하는 특수 그룹" | `UMETA(Hidden)`. 직접 쓰지 않는다 |
| `TG_DuringPhysics` | "물리 시뮬레이션 작업과 병렬로 돌 수 있는 것" | HUD 등 물리 결과가 필요 없는 작업 |
| `TG_EndPhysics` | "물리 시뮬레이션을 끝내는 특수 그룹" | `UMETA(Hidden)`. `USkeletalMeshComponent::EndPhysicsTickFunction`이 여기 붙는다 |
| `TG_PostPhysics` | "**리지드바디와 클로스** 시뮬레이션이 끝나야 하는 것" | 본 Transform 읽기 안전 시점 (단, 애니메이션 보장은 §EndTickGroup의 별도 메커니즘) |
| `TG_PostUpdateWork` | "업데이트 작업이 끝나야 하는 것" | 카메라 갱신 완료 후. `Emitter.cpp:86`이 대표 사용처 |
| `TG_LastDemotable` | "맨 끝으로 강등된 것들의 catchall" | `UMETA(Hidden)` |
| `TG_NewlySpawned` | "틱 그룹이 아닌 특수 케이스" | 각 그룹 후 새로 스폰된 항목이 없을 때까지 반복 실행 |

**카메라 갱신은 틱 그룹이 아니다.** `TG_PostPhysics`(`LevelTick.cpp:1749`)와
`TG_PostUpdateWork`(`:1848`) **사이**에서 `PlayerController->UpdateCameraManager(DeltaSeconds)`가
직접 호출된다(`:1799` 부근). 엔진 주석: *"Update cameras last. This needs to be done before
NetUpdates, and after all actors have been ticked."*
`TG_PostUpdateWork`가 "카메라 완료 후"인 이유가 이것이다 — 카메라가 그 그룹에서 도는 게 아니다.

---

## ACharacter 내부 실행 순서

```
[TG_PrePhysics]
  UCharacterMovementComponent::TickComponent()   ← 캡슐 위치 확정
      ↓ prerequisite (Character.cpp:154)
  USkeletalMeshComponent::TickComponent()        ← CMC 완료 후 실행이 보장된다
    → TickPose() / AnimGraph 평가
    → RefreshBoneTransforms()                    → [비동기 Worker Thread 시작]

  ACharacter::Tick()                             ← ★ 순서 보장 없음. 아래 참고

[TG_StartPhysics ~ TG_EndPhysics]
  Chaos 물리 시뮬레이션 (비동기 Physics Thread)
  AnimTask 병렬 진행 중

[TG_PostPhysics]
  AnimTask 완료 보장 (PrimaryComponentTick.EndTickGroup — 아래 참고)
  UEPServerSideRewindComponent::TickComponent()  ← 안전한 시점
    → SaveHitboxSnapshot()
    → GetBoneTransform() ← 현재 프레임 최신 확정값
```

### Prerequisite이 거는 것은 "Mesh가 CMC 뒤"다

`ACharacter::PostInitializeComponents()` (`Character.cpp:139-170`):

```cpp
if (Mesh)
{
    CacheInitialMeshOffset(Mesh->GetRelativeLocation(), Mesh->GetRelativeRotation());

    // force animation tick after movement component updates
    if (Mesh->PrimaryComponentTick.bCanEverTick && CharacterMovement)
    {
        Mesh->PrimaryComponentTick.AddPrerequisite(
            CharacterMovement, CharacterMovement->PrimaryComponentTick);   // :154
    }
}
```

`AddPrerequisite(A, ATick)`은 **"나는 A 뒤에 돈다"**는 뜻이다.
따라서 이 줄이 보장하는 것은 **`Mesh` → CMC 뒤**이지, `ACharacter::Tick` → CMC 뒤가 아니다.

**`ACharacter::Tick`과 `CMC::TickComponent` 사이에는 엔진이 걸어주는 순서 보장이 없다.**
`CharacterMovementComponent.cpp`에서 `AddPrerequisite`을 쓰는 곳은
`PrePhysicsTickFunction` / `PostPhysicsTickFunction`(CMC 자신의 보조 틱, `:11765`, `:11770`)뿐이다.

이게 SSR에게 중요한 이유: 스냅샷의 위치·시각을 `ACharacter::Tick`에서 집어오려 했다면
CMC보다 먼저 돌 수도 있었다. **델리게이트(`OnMovementUpdated`)를 쓴 것이 이 문제를 통째로 우회한다** —
CMC가 이동을 확정한 그 자리에서 값을 밀어주므로 틱 순서에 의존하지 않는다.

---

## SkeletalMeshComponent 애니메이션의 비동기 처리

`USkeletalMeshComponent` 생성자 (`SkeletalMeshComponent.cpp:425-450`):

```cpp
PrimaryComponentTick.TickGroup   = TG_PrePhysics;   // :431
EndPhysicsTickFunction.TickGroup = TG_EndPhysics;   // :442
ClothTickFunction.TickGroup      = TG_PrePhysics;   // :446
ClothTickFunction.EndTickGroup   = TG_PostPhysics;  // :447  ← 이건 '클로스' 것이다
```

**`PrimaryComponentTick.EndTickGroup`은 생성자에서 정해지지 않는다.**
매 프레임 `TickComponent` 끝에서 다시 계산된다 (`:1919-1924`):

```cpp
const bool bDoLateEnd      = CVarAnimationDelaysEndGroup.GetValueOnGameThread() > 0;  // tick.AnimationDelaysEndGroup, 기본 1
const bool bRequiresPhysics = EndPhysicsTickFunction.IsTickFunctionRegistered();
const ETickingGroup EndTickGroup = bDoLateEnd && !bRequiresPhysics ? TG_PostPhysics : TG_PrePhysics;
ThisTickFunction->EndTickGroup = EndTickGroup;
```

CVar 설명이 그대로 말해준다:
*"skeletal meshes that **do not rely on physics simulation** will set their animation end tick group to `TG_PostPhysics`."*

`EndPhysicsTickFunction` 등록 조건 (`:778-782`):

```cpp
bool USkeletalMeshComponent::ShouldRunEndPhysicsTick() const
{
    return (bEnablePhysicsOnDedicatedServer || !IsNetMode(NM_DedicatedServer)) &&
           ((IsSimulatingPhysics() && RigidBodyIsAwake()) || ShouldBlendPhysicsBones());
}
```

| 메시 상태 | `bRequiresPhysics` | `EndTickGroup` | 의미 |
|---|---|---|---|
| 일반 (킨ematic 히트박스만) | false | **`TG_PostPhysics`** | AnimTask가 PostPhysics까지 늘어질 수 있다 → 그 그룹이 블로킹 포인트 |
| 래그돌·물리 블렌드 중 | true | `TG_PrePhysics` | AnimTask가 PrePhysics 안에서 끝나야 한다 (EndPhysics가 본을 필요로 하므로) |

**우리 캐릭터는 지금 첫 번째 줄이다** — 시뮬레이션도 블렌드도 안 하므로 `TG_PostPhysics`.
그래서 SSR이 `TG_PostPhysics`에서 읽는 것이 안전하다.

> **나중에 래그돌 사망을 넣으면 이 값이 뒤집힌다.** 그래도 SSR은 깨지지 않는다 —
> `TG_PrePhysics`로 당겨지는 건 "더 일찍 끝난다"는 뜻이라 `TG_PostPhysics` 읽기는 여전히 안전하다.
> (오히려 그때는 EndPhysics 틱이 본을 한 번 더 건드리므로, PostPhysics가 **유일하게** 안전한 시점이 된다.)

- `TG_PrePhysics`에서 AnimGraph 평가를 **비동기 Worker Thread**로 스케줄한다
- `EndTickGroup`이 블로킹 포인트 — 그 그룹이 끝나기 전에 AnimTask 완료가 보장된다
- **`TG_PrePhysics`에서 `GetBoneTransform()`을 부르면 이전 프레임 데이터를 읽는다**

---

## TickPrerequisite 의존성 설정

```cpp
// ComponentB가 완료된 후 ComponentA가 실행되도록 강제
ComponentA->AddTickPrerequisiteComponent(ComponentB);

// ActorB가 완료된 후 ActorA가 실행되도록 강제
ActorA->AddTickPrerequisiteActor(ActorB);
```

- 같은 틱 그룹 내에서도 Prerequisite으로 선후 관계 강제 가능
- 다른 틱 그룹 간의 순서는 그룹 번호로 이미 보장됨
- Actor Tick과 소속 Component Tick 간에는 **기본적으로 순서 보장이 없다**.
  명시적인 Prerequisite이 있어야 한다 (위 `Mesh` ← CMC가 그 예)

---

## 이 프로젝트에서의 적용

### UEPServerSideRewindComponent

```cpp
UEPServerSideRewindComponent::UEPServerSideRewindComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;  // 본 Transform 안전 읽기
    SetIsReplicatedByDefault(false);
}
```

`TG_PostPhysics`로 설정한 이유:
1. 애니메이션 비동기 태스크 완료 후 **현재 프레임** 본 Transform을 기록한다
   (메시의 `EndTickGroup`이 `TG_PostPhysics`이므로 — 위 표 참고)
2. 물리 결과 반영 후이므로, 나중에 래그돌·물리 블렌드가 붙어도 읽는 값이 최종값이다
3. `TG_PrePhysics`에서 기록하면 1프레임 이전 데이터가 스냅샷에 저장된다

관련: 스냅샷의 **시각·위치**는 여기서 읽지 않는다. TickDispatch에서 CMC가 넘겨준 값을 쓴다.
이유는 `DOCS/Mine/ServerSideRewind.md` §2.1 참고 — `TimeSeconds`가 TickDispatch **뒤에** 증가하기 때문이다.

### 서버에서 본 Transform 갱신 전제 조건

```cpp
// AEPCharacter 생성자 (EPCharacter.cpp:49)
GetMesh()->VisibilityBasedAnimTickOption =
    EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
```

**이 줄이 필요한 이유는 "엔진 기본값이 그렇지 않아서"가 아니다.**

- `USkeletalMeshComponent` 자신의 기본값은 이미 `AlwaysTickPoseAndRefreshBones`다
  (`SkeletalMeshComponent.cpp:436`, `SkinnedMeshComponent.cpp:460`)
- **`ACharacter` 생성자가 그걸 `AlwaysTickPose`로 낮춘다** (`Character.cpp:124`)

즉 되돌리는 것이지 새로 켜는 것이 아니다.

**그리고 서버에서 안 도는 것은 `TickPose()`가 아니라 `RefreshBoneTransforms()`다.**
열거값 이름 그대로다 — *"Always Tick, but **Refresh BoneTransforms only when rendered**."*

```cpp
// SkinnedMeshComponent.cpp:1612 — 포즈를 틱할 것인가
return ((VisibilityBasedAnimTickOption < EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered) || bRecentlyRendered);

// SkinnedMeshComponent.cpp:1617 — 본 Transform을 갱신할 것인가
return (bRecentlyRendered || (VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones));
```

`EVisibilityBasedAnimTickOption` 순서 (`SkinnedMeshComponent.h:93-113`):
`AlwaysTickPoseAndRefreshBones`(0) < `AlwaysTickPose`(1) < `OnlyTickMontagesAndRefreshBonesWhenPlayingMontages`(2)
< `OnlyTickMontagesWhenNotRendered`(3) < `OnlyTickPoseWhenRendered`(4)

데디케이티드 서버는 `bRecentlyRendered == false`다. 따라서 `AlwaysTickPose`(1)에서는:

| | 값 | 결과 |
|---|---|---|
| `ShouldTickPose()` | `1 < 4` → **true** | AnimGraph는 정상적으로 돈다 |
| `ShouldUpdateTransform()` | `false \|\| (1 == 0)` → **false** | `RefreshBoneTransforms()`를 건너뛴다 |

⇒ 애니메이션 상태는 갱신되는데 **본 배열은 그대로 남는다.**
이 설정 없이는 `TG_PostPhysics`에서 읽어도 스냅샷이 정적 포즈로 고정된다 (결론은 이전 판과 같다).

---

## 고친 것 (2026-09-01)

| # | 이전 판 | 실제 | 결론 바뀜? |
|---|---|---|---|
| 1 | `TG_PreCloth` / `TG_StartCloth` / `TG_EndCloth`가 순서에 있음 | UE5에 **없는 열거값**. 클로스는 `ClothTickFunction`으로 분리 | 예 |
| 2 | `TG_PostPhysics` = "물리 + 클로스 + **애니메이션** 완료" | 엔진 정의는 리지드바디 + 클로스. 애니메이션은 `EndTickGroup`이라는 별도 장치 | 아니오 (경로가 다름) |
| 3 | `CharacterMovement->PrimaryComponentTick.AddPrerequisite(this, this->PrimaryActorTick)` | 그런 줄은 없다. 실제는 `Mesh->...AddPrerequisite(CharacterMovement, ...)` (`Character.cpp:154`) | **예** — `ACharacter::Tick`은 CMC 뒤가 보장되지 않는다 |
| 4 | `EndTickGroup = TG_PostPhysics`가 생성자 상수 | 생성자의 그 줄은 **ClothTickFunction** 것. Primary의 것은 매 프레임 `:1921`에서 재계산되고 물리 사용 시 `TG_PrePhysics` | **예** — 조건부다 |
| 5 | "서버는 렌더링이 없으므로 기본값에서 `TickPose()`가 호출되지 않는다" | `TickPose()`는 호출된다. 건너뛰는 건 `RefreshBoneTransforms()` (`ShouldUpdateTransform` = false) | 아니오 (증상 동일) |
| 6 | "기본값" 때문에 설정이 필요하다 | 메시 기본값은 이미 옳다. **`ACharacter` 생성자가 낮춘다** (`Character.cpp:124`) | **예** — 원인이 다르다 |

---

## 인용 색인 (UE 5.7, `Engine/Source/Runtime/Engine/`)

| 파일 | 줄 | 내용 |
|---|---|---|
| `Classes/Engine/EngineBaseTypes.h` | 83-110 | `ETickingGroup` 전체 (PreCloth 계열 없음) |
| | 187 / 195 | `FTickFunction::TickGroup` / `EndTickGroup` |
| `Private/LevelTick.cpp` | 1721 / 1749 | `RunTickGroup(TG_PrePhysics)` / `(TG_PostPhysics)` |
| | ~1799 | `PlayerController->UpdateCameraManager(DeltaSeconds)` — 틱 그룹 사이 |
| | 1848 | `RunTickGroup(TG_PostUpdateWork)` |
| `Private/Character.cpp` | 124 | `Mesh->VisibilityBasedAnimTickOption = AlwaysTickPose` ← 여기서 낮춘다 |
| | 154 | `Mesh->PrimaryComponentTick.AddPrerequisite(CharacterMovement, ...)` |
| `Private/Components/SkeletalMeshComponent.cpp` | 431 / 436 | `TickGroup = TG_PrePhysics` / 기본 `AlwaysTickPoseAndRefreshBones` |
| | 442 / 446-447 | `EndPhysicsTickFunction` / `ClothTickFunction`의 틱 그룹 |
| | 734-756 | `RegisterEndPhysicsTick` |
| | 778-782 | `ShouldRunEndPhysicsTick` |
| | 1861-1864 | `CVarAnimationDelaysEndGroup` (`tick.AnimationDelaysEndGroup`, 기본 1) |
| | 1919-1924 | `EndTickGroup` 매 프레임 재계산 |
| `Private/Components/SkinnedMeshComponent.cpp` | 460 | 기본 `AlwaysTickPoseAndRefreshBones` |
| | 1612 | `ShouldTickPose()` |
| | 1617 | `ShouldUpdateTransform()` |
| `Classes/Components/SkinnedMeshComponent.h` | 93-113 | `EVisibilityBasedAnimTickOption` 순서 |
| `Private/Components/CharacterMovementComponent.cpp` | 11765 / 11770 | CMC 보조 틱의 Prerequisite (Character Tick과 무관) |
| `Private/Particles/Emitter.cpp` | 86 | `TG_PostUpdateWork` 사용 예 |
