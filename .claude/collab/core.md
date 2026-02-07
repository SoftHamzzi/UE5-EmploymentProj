# Core Framework 작업 문서

> 담당 에이전트: core-framework
> 담당 영역: Core/ 폴더의 모든 클래스

---

## 담당 파일

```
Public/Core/
├── EPGameMode.h
├── EPGameState.h
├── EPPlayerController.h
├── EPPlayerState.h
├── EPCharacter.h
└── EPCorpse.h

Private/Core/
├── EPGameMode.cpp
├── EPGameState.cpp
├── EPPlayerController.cpp
├── EPPlayerState.cpp
├── EPCharacter.cpp
└── EPCorpse.cpp

Public/Types/
└── EPTypes.h
```

---

## 완료된 작업

### Step 1: 스켈레톤 클래스 (done)
- 모든 Core 클래스 헤더/cpp 생성
- EPTypes.h 열거형 정의
- 빌드 확인 완료
- 태그: step1

### Step 2: Character 이동/입력 (done)
- Enhanced Input 바인딩 (Move, Look, Jump, Sprint)
- 1인칭 카메라 생성
- PlayerController에서 MappingContext 등록
- InputAction getter 추가
- 태그: step2

---

## 진행 중 작업

(없음)

---

## 대기 중 작업

### Step 3: GameMode 매치 흐름
- [ ] Constructor에서 기본 클래스 설정
- [ ] ReadyToStartMatch 구현
- [ ] HandleMatchHasStarted/Ended 구현
- [ ] TickMatchTimer 구현

### Step 6: 스폰 시스템
- [ ] ChoosePlayerStart_Implementation
- [ ] PlayerStart 랜덤 선택 로직

### Step 7: DataAsset
- [ ] EPWeaponData 클래스 구현
- [ ] EPItemData 클래스 구현
- [ ] GetPrimaryAssetId 구현

---

## replication 에이전트에 요청할 사항

- Step 4, 5에서 GetLifetimeReplicatedProps 구현 필요
- EPCorpse의 복제 설정 검토 필요
- Client_OnKill RPC 구현 검토 필요

---

## 참조 문서

- DOCS/Notes/01_Implementation.md
- DOCS/GAME.md
