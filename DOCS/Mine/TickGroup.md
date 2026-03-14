# UE5 Tick 실행 순서

## ETickingGroup 전체 순서

```
UWorld::Tick()
│
├─ [TG_PrePhysics]          ← 기본값. 대부분의 게임플레이 로직
├─ [TG_StartPhysics]        ← Chaos::StartFrame() 호출 (직접 사용 X)
├─ [TG_DuringPhysics]       ← 물리 스레드와 병렬 실행 (물리 결과 불필요한 작업)
├─ [TG_EndPhysics]          ← 물리 스레드 완료 대기 + 결과 커밋 (직접 사용 X)
├─ [TG_PreCloth]
├─ [TG_StartCloth / TG_EndCloth]
├─ [TG_PostPhysics]         ← 물리 + 애니메이션 모두 완료된 첫 번째 시점
├─ [TG_PostUpdateWork]      ← 카메라 업데이트 완료
└─ [TG_LastDemotable]
```

| 열거값 | 설명 |
|--------|------|
| `TG_PrePhysics` | 물리 시작 전. 기본값. CMC, Character Tick 등 대부분의 게임플레이 로직 |
| `TG_StartPhysics` | `FChaosScene::StartFrame()` 호출. 직접 사용하지 않는다 |
| `TG_DuringPhysics` | 물리 스레드와 게임 스레드 병렬. HUD 등 물리 결과 불필요한 작업 |
| `TG_EndPhysics` | 물리 스레드 완료 대기 + Chaos 결과 커밋. 직접 사용하지 않는다 |
| `TG_PreCloth` | 리지드바디 완료 후, 클로스 시작 전 |
| `TG_PostPhysics` | 물리 + 클로스 + 애니메이션 모두 완료. 본 Transform 읽기 안전한 최초 시점 |
| `TG_PostUpdateWork` | 카메라 업데이트 완료. 카메라 의존 이펙트 처리 |
| `TG_LastDemotable` | 프레임 맨 끝으로 강등된 작업 집합 |
| `TG_NewlySpawned` | 틱 그룹이 아닌 특수 케이스. 각 그룹 처리 후 해당 프레임에 새로 스폰된 Actor 소화 |

---

## ACharacter 내부 실행 순서

```
[TG_PrePhysics]
  UCharacterMovementComponent::TickComponent()   ← 캡슐 위치 확정
      (CMC가 ACharacter의 Prerequisite으로 등록되어 있음)
      ↓ prerequisite 해제
  ACharacter::Tick()                             ← CMC 완료 후 실행
  USkeletalMeshComponent::TickComponent()
    → NativeUpdateAnimation()                    ← AnimGraph 평가
    → RefreshBoneTransforms()                    → [비동기 Worker Thread 시작]

[TG_StartPhysics ~ TG_EndPhysics]
  Chaos 물리 시뮬레이션 (비동기 Physics Thread)
  AnimTask 병렬 진행 중

[TG_PostPhysics]
  AnimTask 완료 보장 (EndTickGroup = TG_PostPhysics 블로킹)
  UEPServerSideRewindComponent::TickComponent()  ← 안전한 시점
    → SaveHitboxSnapshot()
    → GetBoneTransform() ← 현재 프레임 최신 확정값
```

---

## SkeletalMeshComponent 애니메이션의 비동기 처리

`USkeletalMeshComponent`는 두 개의 틱 그룹 설정을 사용한다:

```cpp
TickGroup    = TG_PrePhysics   // 시작 시점
EndTickGroup = TG_PostPhysics  // 완료 보장 시점
```

- `TG_PrePhysics`에서 AnimGraph 평가를 **비동기 Worker Thread**로 스케줄한다
- `TG_PostPhysics`가 블로킹 포인트 역할 — 이 시점에서 모든 AnimTask 완료 보장
- **`TG_PrePhysics`에서 `GetBoneTransform()` 호출 시 이전 프레임 데이터를 읽는다**

---

## CMC → Character Tick 순서 보장

`ACharacter::PostInitializeComponents()` 내부에서 엔진이 자동으로 Prerequisite을 등록한다:

```cpp
// 엔진 내부 (ACharacter)
CharacterMovement->PrimaryComponentTick.AddPrerequisite(
    this, this->PrimaryActorTick);
// → ACharacter::Tick은 항상 CMC::TickComponent 완료 후 실행
```

Actor Tick과 소속 Component Tick 간에는 기본적으로 순서 보장이 없다.
CMC-Character 관계처럼 명시적인 Prerequisite이 있어야 순서가 보장된다.

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
1. 물리 시뮬레이션 결과(Physics Asset 본 위치) 반영 완료 후 스냅샷 기록
2. 애니메이션 비동기 태스크 완료 후 현재 프레임 본 Transform 기록
3. `TG_PrePhysics`에서 기록하면 1프레임 이전 데이터가 스냅샷에 저장됨

### 서버에서 본 Transform 갱신 전제 조건

```cpp
// AEPCharacter 생성자 (EPCharacter.cpp)
GetMesh()->VisibilityBasedAnimTickOption =
    EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
```

서버는 렌더링이 없으므로 기본값에서 `TickPose()`가 호출되지 않는다.
이 설정 없이는 `TG_PostPhysics`에서 읽어도 스냅샷이 정적 포즈로 고정된다.
