# MetaHuman 통합 가이드

## 1. 현재 상태

메타휴먼 크리에이터로 생성한 BP는 **AActor 기반**이며, 컴포넌트 구조:
```
Root
├── Body          (SkeletalMeshComponent) — 신체
├── Face          (SkeletalMeshComponent) — 얼굴
│   ├── Hair      (GroomComponent)
│   ├── Eyebrows  (GroomComponent)
│   ├── Eyelashes (GroomComponent)
│   ├── Beard     (GroomComponent)
│   ├── Mustache  (GroomComponent)
│   └── Fuzz      (GroomComponent)
├── SkeletalMesh  (SkeletalMeshComponent) — 의상(Outfit)
├── MetaHuman     (에디터 유틸리티, 런타임 무관)
└── LODSync       (LOD 동기화)
```

무시 가능:
- `LiveLinkSubject`, `UseLiveLink`, `UseARKit` — 실시간 모캡/페이셜 캡처용. 게임에서 사용 안 함
- `MetaHuman` 컴포넌트 — 에디터 커스터마이징 연동용. 런타임 게임플레이 무관
- `LODSync` — BP에서 Add Component로 추가. C++ 불필요

참고: 의상(SkeletalMesh)은 디테일 패널에서 메시 에셋이 비어있지만, **Construction Script 또는 MetaHuman 컴포넌트가 런타임에 동적 할당**한다.
실제 의상 에셋은 `Content/MetaHumans/캐릭터이름/` 폴더에서 찾거나, PIE 실행 후 Outliner에서 확인.

## 2. 통합 방식: Leader Pose Component

Epic 공식 권장. Body 메시의 본 포즈를 Face/Outfit이 **따라가게** 하는 방식.

```cpp
FaceMesh->SetLeaderPoseComponent(GetMesh());
OutfitMesh->SetLeaderPoseComponent(GetMesh());
```
- Face/Outfit이 별도 애니메이션 계산을 하지 않고 Body의 본 포즈를 복사
- CPU 절약, 동기화 보장
- Modular Character 시스템의 기반 (의상/헤어 교체 가능)

## 3. ACharacter 매핑

| ACharacter 컴포넌트 | 메타휴먼 BP 대응 | 역할 |
|---|---|---|
| `GetMesh()` (기본 내장) | Body | 신체, 애니메이션 주체 |
| `FaceMesh` (C++ 추가) | Face | 얼굴, LeaderPose |
| `OutfitMesh` (C++ 추가) | SkeletalMesh | 의상, LeaderPose |
| Groom (BP 추가) | Hair/Eyebrows 등 | 비주얼, 캐릭터마다 다름 |

## 4. 변경 사항

### 4-1. EPCharacter.h

```cpp
// 전방선언
class UGroomComponent;

// protected 변수
// --- 메타휴먼 ---
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MetaHuman")
TObjectPtr<USkeletalMeshComponent> FaceMesh;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MetaHuman")
TObjectPtr<USkeletalMeshComponent> OutfitMesh;
```

```cpp
// public Getter
FORCEINLINE USkeletalMeshComponent* GetFaceMesh() const { return FaceMesh; }
FORCEINLINE USkeletalMeshComponent* GetOutfitMesh() const { return OutfitMesh; }
```

### 4-2. EPCharacter.cpp (생성자)

```cpp
AEPCharacter::AEPCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UEPCharacterMovement>(
        ACharacter::CharacterMovementComponentName))
{
    // --- Body Mesh 설정 ---
    GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

    // --- Face Mesh ---
    FaceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));
    FaceMesh->SetupAttachment(GetMesh());
    FaceMesh->SetLeaderPoseComponent(GetMesh());

    // --- Outfit Mesh ---
    OutfitMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Outfit"));
    OutfitMesh->SetupAttachment(GetMesh());
    OutfitMesh->SetLeaderPoseComponent(GetMesh());

    // --- Camera ---
    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>("Camera");
    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->SetupAttachment(GetMesh(), FName("head"));
    FirstPersonCamera->SetRelativeLocationAndRotation(FirstPersonCameraOffset, FRotator(0.0f, 90.0f, -90.0f));
    bUseControllerRotationYaw = true;

    // --- Combat ---
    CombatComponent = CreateDefaultSubobject<UEPCombatComponent>(TEXT("CombatComponent"));

    // --- Movement ---
    UEPCharacterMovement* Movement = Cast<UEPCharacterMovement>(GetCharacterMovement());
    Movement->JumpZVelocity = 420.f;
    Movement->AirControl = 0.5f;
    Movement->BrakingDecelerationFalling = 700.f;
    Movement->NavAgentProps.bCanCrouch = true;
    Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
}
```

### 4-3. BP_EPCharacter에서 설정

에디터에서:
1. `GetMesh()` (Mesh) → 메타휴먼 Body 스켈레탈 메시 에셋 지정
2. `FaceMesh` → 메타휴먼 Face 스켈레탈 메시 에셋 지정
3. `OutfitMesh` → 메타휴먼 의상 스켈레탈 메시 에셋 지정 (Content/MetaHumans/ 에서 찾기)
4. **Add Component**로 Groom 추가:
   - Hair (GroomComponent) → GetMesh()에 Attach
   - Eyebrows (GroomComponent) → GetMesh()에 Attach
   - Eyelashes (GroomComponent) → GetMesh()에 Attach
   - Beard/Mustache/Fuzz → 해당 캐릭터에 필요한 것만
5. LODSync → Add Component, Body를 Drive, 나머지를 Driven으로 설정
6. 카메라 head 소켓 이름 확인 (메타휴먼에서 이름이 다를 수 있음)
7. `FirstPersonCameraOffset` 조정 (메타휴먼 머리 크기에 맞게)
8. CapsuleComponent 반지름/높이 → 메타휴먼 체형에 맞게 조정

### 4-4. 사망 처리 — 캐릭터 셀프 래그돌

EPCorpse 별도 스폰 대신, 캐릭터 자체를 래그돌 처리한다.
Face/Outfit은 LeaderPose로 Body를 따라가므로 Body에만 래그돌 적용하면 된다.

```cpp
// EPCharacter.cpp — Multicast_Die_Implementation
void AEPCharacter::Multicast_Die_Implementation()
{
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetSimulatePhysics(true);
}
```

## 5. 주의사항

- **카메라 소켓**: 메타휴먼의 head 본 이름이 기존 TutorialTPP와 다를 수 있음. 스켈레톤 에디터에서 확인
- **캡슐 크기**: 메타휴먼 체형에 맞게 CapsuleComponent 반지름/높이 조정 필요
- **Mesh 위치/회전**: `SetRelativeLocation/Rotation` 값은 메타휴먼 스켈레톤에 맞게 조정
- **AnimBP**: 메타휴먼용 AnimBP를 Body 메시에 지정해야 함 (Step 8에서 처리)
- **Groom 시뮬레이션**: 머리카락 흩날림 → Groom 컴포넌트의 `Enable Simulation` 체크 해제로 끌 수 있음