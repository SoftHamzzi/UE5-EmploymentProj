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
├── SkeletalMesh  (참조용)
├── MetaHuman     (에디터 유틸리티, 런타임 무관)
└── LODSync       (LOD 동기화)
```

추가 변수 (무시 가능):
- `LiveLinkSubject`, `UseLiveLink`, `UseARKit` — 실시간 모캡/페이셜 캡처용. 게임에서 사용 안 함

## 2. 통합 방식: Leader Pose Component

Epic 공식 권장. Body 메시의 본 포즈를 Face/Outfit이 **따라가게** 하는 방식.

핵심 한 줄:
```cpp
FaceMesh->SetLeaderPoseComponent(GetMesh());
```
- Face가 별도 애니메이션 계산을 하지 않고 Body의 본 포즈를 복사
- CPU 절약, 동기화 보장
- Modular Character 시스템의 기반 (의상/헤어 교체 가능)

## 3. 변경 사항

### 3-1. EPCharacter.h

```cpp
// 전방선언 추가
class UGroomComponent;

// protected 변수 추가
// --- 메타휴먼 ---
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MetaHuman")
TObjectPtr<USkeletalMeshComponent> FaceMesh;
```

Groom(Hair, Eyebrows 등)은 캐릭터마다 조합이 다르므로 BP에서 추가.

### 3-2. EPCharacter.cpp (생성자)

**기존:**
```cpp
AEPCharacter::AEPCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UEPCharacterMovement>(
        ACharacter::CharacterMovementComponentName))
{
    GetMesh()->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::WorldSpaceRepresentation;

    CombatComponent = CreateDefaultSubobject<UEPCombatComponent>(TEXT("CombatComponent"));

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>("Camera");
    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->SetupAttachment(GetMesh(), FName("head"));
    FirstPersonCamera->SetRelativeLocationAndRotation(FirstPersonCameraOffset, FRotator(0.0f, 90.0f, -90.0f));
    bUseControllerRotationYaw = true;
    // ... Movement 설정
}
```

**변경:**
```cpp
AEPCharacter::AEPCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UEPCharacterMovement>(
        ACharacter::CharacterMovementComponentName))
{
    // --- Body Mesh 설정 ---
    // GetMesh() = ACharacter 기본 SkeletalMeshComponent = Body 역할
    GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -90.f));
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

    // --- Face Mesh ---
    FaceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));
    FaceMesh->SetupAttachment(GetMesh());
    FaceMesh->SetLeaderPoseComponent(GetMesh());  // Body 본 따라감

    // --- 카메라 ---
    // 주의: 메타휴먼의 head 소켓 이름이 다를 수 있음 → BP에서 확인 필요
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

### 3-3. BP_EPCharacter에서 설정

에디터에서:
1. `GetMesh()` (Body) → 메타휴먼 Body 스켈레탈 메시 에셋 지정
2. `FaceMesh` → 메타휴먼 Face 스켈레탈 메시 에셋 지정
3. **Add Component**로 Groom 추가:
   - Hair (GroomComponent) → GetMesh()에 Attach
   - Eyebrows (GroomComponent) → GetMesh()에 Attach
   - Eyelashes (GroomComponent) → GetMesh()에 Attach
   - Beard/Mustache/Fuzz → 해당 캐릭터에 필요한 것만
4. LODSync → Add Component, Body를 Drive, 나머지를 Driven으로 설정
5. 카메라 head 소켓 이름 확인 (메타휴먼에서 이름이 다를 수 있음)
6. `FirstPersonCameraOffset` 조정 (메타휴먼 머리 크기에 맞게)

### 3-4. EPCorpse — Face 메시 복사 추가

**EPCorpse.h:**
```cpp
protected:
    // Body 메시
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> CorpseMesh;

    // Face 메시
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkeletalMeshComponent> FaceMesh;

    // 복제용 에셋 참조
    UPROPERTY(ReplicatedUsing = OnRep_CorpseMeshAsset)
    TObjectPtr<USkeletalMesh> CorpseMeshAsset;

    UPROPERTY(ReplicatedUsing = OnRep_CorpseFaceAsset)
    TObjectPtr<USkeletalMesh> CorpseFaceAsset;

    UFUNCTION()
    void OnRep_CorpseMeshAsset();
    UFUNCTION()
    void OnRep_CorpseFaceAsset();
    void ApplyCorpseMesh();
```

**EPCorpse.cpp 생성자:**
```cpp
AEPCorpse::AEPCorpse()
{
    bReplicates = true;
    CorpseMesh = CreateDefaultSubobject<USkeletalMeshComponent>("CorpseMesh");
    RootComponent = CorpseMesh;

    FaceMesh = CreateDefaultSubobject<USkeletalMeshComponent>("FaceMesh");
    FaceMesh->SetupAttachment(CorpseMesh);
    FaceMesh->SetLeaderPoseComponent(CorpseMesh);  // Body 래그돌 따라감
}
```

**InitializeFromCharacter:**
```cpp
void AEPCorpse::InitializeFromCharacter(AEPCharacter* DeadCharacter)
{
    if (!HasAuthority() || !DeadCharacter) return;

    // Body
    USkeletalMeshComponent* SrcBody = DeadCharacter->GetMesh();
    if (SrcBody && SrcBody->GetSkeletalMeshAsset())
        CorpseMeshAsset = SrcBody->GetSkeletalMeshAsset();

    // Face
    USkeletalMeshComponent* SrcFace = DeadCharacter->GetFaceMesh();
    if (SrcFace && SrcFace->GetSkeletalMeshAsset())
        CorpseFaceAsset = SrcFace->GetSkeletalMeshAsset();

    ApplyCorpseMesh();
}
```

**ApplyCorpseMesh:**
```cpp
void AEPCorpse::ApplyCorpseMesh()
{
    if (CorpseMeshAsset)
    {
        CorpseMesh->SetSkeletalMeshAsset(CorpseMeshAsset);
        CorpseMesh->SetCollisionProfileName(TEXT("Ragdoll"));

        // 1프레임 지연 후 물리 활성화 (바닥 관통 방지)
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            CorpseMesh->SetSimulatePhysics(true);
        });
    }

    if (CorpseFaceAsset)
    {
        FaceMesh->SetSkeletalMeshAsset(CorpseFaceAsset);
        // Face는 LeaderPose로 Body 래그돌을 따라감 — 별도 물리 불필요
    }
}

void AEPCorpse::OnRep_CorpseMeshAsset() { ApplyCorpseMesh(); }
void AEPCorpse::OnRep_CorpseFaceAsset() { ApplyCorpseMesh(); }
```

**GetLifetimeReplicatedProps:**
```cpp
DOREPLIFETIME(AEPCorpse, CorpseMeshAsset);
DOREPLIFETIME(AEPCorpse, CorpseFaceAsset);
```

### 3-5. EPCharacter.h — Getter 추가

```cpp
public:
    // 기존
    UCameraComponent* GetCameraComponent() const;
    UEPCombatComponent* GetCombatComponent() const;
    // 추가
    FORCEINLINE USkeletalMeshComponent* GetFaceMesh() const { return FaceMesh; }
```

## 4. Groom은 Corpse에 필요 없는 이유

- Groom(Hair 등)은 순수 비주얼. 시체에서 머리카락이 없어도 게임플레이에 영향 없음
- Groom 복제는 비용 대비 효과가 낮음
- 필요하면 나중에 추가 가능

## 5. 주의사항

- **카메라 소켓**: 메타휴먼의 head 본 이름이 기존 TutorialTPP와 다를 수 있음. 스켈레톤 에디터에서 확인
- **캡슐 크기**: 메타휴먼 체형에 맞게 CapsuleComponent 반지름/높이 조정 필요
- **FirstPersonPrimitiveType**: `GetMesh()->FirstPersonPrimitiveType` 기존 설정이 메타휴먼에서도 필요한지 확인
- **Mesh 위치/회전**: `SetRelativeLocation/Rotation` 값은 메타휴먼 스켈레톤에 맞게 조정
- **AnimBP**: 메타휴먼용 AnimBP를 Body 메시에 지정해야 함 (Step 8에서 처리)