# Post 2-2 작성 가이드 — MetaHuman 통합

> **예상 제목**: `[UE5] 추출 슈터 2-2. MetaHuman을 ACharacter에 통합하기 (LeaderPoseComponent)`
> **참고 문서**: DOCS/Mine/MetaHuman.md

---

## 개요

**이 포스팅에서 다루는 것:**
- MetaHuman Creator에서 만든 캐릭터를 ACharacter C++ 클래스(EPCharacter)에 통합하는 방법
- 기본 마네킹 대신 MetaHuman을 쓸 때 생기는 구조적 차이 설명

**왜 이렇게 구현했는가 (설계 의도):**
- MetaHuman은 AActor 기반 BP로 제공되지만, 이 프로젝트는 서버 권한 멀티플레이어이므로 ACharacter가 반드시 필요
- LeaderPoseComponent 방식을 쓰면 Face/Outfit 메시가 Body 본 포즈를 그대로 따라가 CPU 절약 + 동기화 보장
- 향후 의상 교체(Modular Character)로 확장 가능

---

## 구현 전 상태 (Before)

- 기존 캐릭터: TutorialTPP 또는 기본 마네킹 스켈레탈 메시
- ACharacter의 기본 `GetMesh()` 하나로만 캐릭터 표현
- MetaHuman은 Body/Face/Outfit/Groom이 분리된 멀티 메시 구조

**왜 단순 메시 교체만으로는 안 되는가:**
- MetaHuman Face는 별도 SkeletalMeshComponent로 분리되어 있음
- Face가 Body 본을 따라가지 않으면 목/머리 연결이 끊어짐

---

## 구현 내용

### 1. MetaHuman BP 구조 분석

MetaHuman Creator 임포트 후 생성되는 BP의 컴포넌트 트리를 보여줄 것:

```
Root
├── Body (SkeletalMeshComponent)  ← ACharacter::GetMesh()에 해당
├── Face (SkeletalMeshComponent)  ← 별도 추가 필요
│   ├── Hair (GroomComponent)
│   └── Eyebrows/Eyelashes/... (GroomComponent)
├── SkeletalMesh (의상 Outfit)    ← 별도 추가 필요
└── LODSync
```

- `MetaHuman` 컴포넌트, `LiveLinkSubject` 등은 에디터 전용/모캡 전용 → 게임에서 무시

### 2. LeaderPoseComponent 패턴

```cpp
// EPCharacter.cpp 생성자
FaceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Face"));
FaceMesh->SetupAttachment(GetMesh());
FaceMesh->SetLeaderPoseComponent(GetMesh());  // Body 본 포즈를 따라감

OutfitMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Outfit"));
OutfitMesh->SetupAttachment(GetMesh());
OutfitMesh->SetLeaderPoseComponent(GetMesh());
```

핵심 설명:
- Face/Outfit은 자체 AnimInstance를 실행하지 않음 → CPU 절약
- Body(GetMesh())에 붙은 AnimBP 하나가 전체를 구동

### 3. FPS 시점 처리

FPS 게임에서 로컬 플레이어가 자신의 Face/Outfit을 볼 필요 없음 (1인칭 카메라):
```cpp
// 이 프로젝트에서는 bOwnerNoSee를 적용하지 않음
// 이유: 서드파티 TPS 시점도 사용할 가능성을 고려
// 대신 카메라를 head 본 안쪽에 배치하여 자연스럽게 보이지 않게 처리
```

> **스크린샷 위치**: BP_EPCharacter 컴포넌트 트리(Body/Face/Outfit/Groom 배치)

### 4. 사망 처리 — 셀프 래그돌

별도 Corpse 액터를 스폰하지 않고 캐릭터 자체를 래그돌 처리:

```cpp
void AEPCharacter::Multicast_Die_Implementation()
{
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
    GetMesh()->SetSimulatePhysics(true);
    // Face/Outfit은 LeaderPose로 Body를 따라가므로 별도 처리 불필요
}
```

**Corpse 액터 대신 셀프 래그돌을 선택한 이유:**
- 별도 Actor 스폰 시 네트워크 복제 복잡도 증가
- LeaderPoseComponent 덕분에 Body 하나만 래그돌 처리해도 Face/Outfit 모두 따라감

### 5. 에디터 설정 순서

```
BP_EPCharacter에서:
1. Mesh(Body) → MetaHuman Body 스켈레탈 메시 할당
2. Face → MetaHuman Face 스켈레탈 메시 할당
3. Outfit → MetaHuman 의상 스켈레탈 메시 할당
4. Add Component → Groom (Hair/Eyebrows 등)
5. Add Component → LODSync (Body를 Drive, Face/Outfit을 Driven)
6. CapsuleComponent 반지름/높이 조정 (메타휴먼 체형에 맞게)
7. AnimBP는 Body(GetMesh())에만 지정
```

> **스크린샷 위치**: 에디터에서 메타휴먼 캐릭터가 맵에 서 있는 모습

---

## 결과

**확인 항목:**
- PIE 실행 시 메타휴먼 캐릭터가 맵에 서 있음
- Face/Outfit이 Body 포즈를 따라 움직임 (T-포즈 상태에서 확인)
- 2인 접속 시 상대방에게도 메타휴먼 캐릭터가 보임
- 사망 시 셀프 래그돌 동작 (캡슐 콜리전 제거 + Ragdoll 물리)

**한계 및 향후 개선:**
- AnimBP는 아직 기본 마네킹용 → 다음 포스팅(Post 2-7)에서 리타게팅으로 해결
- Groom(머리카락) 시뮬레이션 비용 → Enable Simulation 해제로 임시 비활성화 가능

---

## 참고

- `DOCS/Mine/MetaHuman.md` — 통합 상세 설계
- MetaHuman Creator 공식 문서
- UE5 LeaderPoseComponent 문서
