# UE 클라이언트 프로그래머 취업 준비 문서

## 1. 참고 채용공고 (PUBG Uropa - 모바일)

### 필수요건
- UE 게임플레이 프레임워크에 대한 깊은 이해
- 게임모드 및 기믹(기능적 요소) 개발 경험
- 리플리케이션을 활용한 네트워크 기반 콘텐츠 개발 경험
- 서버-클라이언트 동기화 및 데이터 전송 메커니즘에 대한 깊은 이해
- C++에 대한 심층적인 이해 및 실무 경험

### 우대요건
- 모바일 게임 출시 및 라이브 운영 경험
- 데디케이티드 서버를 활용한 게임모드 설계 및 구현 경험
- Gameplay Ability System (GAS) 활용 경험
- 최적화 기법 및 메모리 관리에 대한 실무 지식
- 버전 관리 시스템(Git 등)을 통한 협업 경험

> 모바일 특화 공고이지만, 사실상 "UE 클라이언트 프로그래머"의 정석 스택을 그대로 요구하는 공고.
> 핵심: C++로 UE 내부 구조를 이해하고 게임플레이를 설계할 수 있는 사람.

---

## 2. 공고 기반 필요 역량 분석

### 핵심 (필수 충족)

| 영역 | 세부 항목 |
|------|----------|
| Gameplay Framework | GameMode, GameState, PlayerController, PlayerState, Pawn/Character, Component, Actor Lifecycle, Tick/BeginPlay/Possess 흐름 |
| GameMode/기믹 설계 | 제한 시간, 점수, 라운드, 스폰, 승패 조건 등 룰 기반 게임 설계 |
| Replication | Replicated, ReplicatedUsing, OnRep, RPC(Server/Client/NetMulticast), Authority 개념 |
| 네트워크 심화 | Client Prediction, Reconciliation, Hit Validation, Lag Compensation, Movement Sync |
| UE C++ | UCLASS/UPROPERTY/UFUNCTION, Reflection, UObject 라이프사이클, 메모리 관리(GC vs New) |
| GAS | Ability, Attribute, Effect, Tag - 스킬/버프/디버프/쿨타임/상태이상 구현 |

### 부가 (구현 예정)

| 영역 | 비고 |
|------|------|
| Animation | AnimBP, 스테이트 머신, 몽타주 (이동/사격/재장전/사망), 네트워크 동기화 |
| Map/Level Design | 맵 제작 (창고/컨테이너 구역, 파밍 포인트, 탈출 지점, 자판기 배치) |
| UI/HUD | 체력바, 탄약, 타이머, 킬 피드, 인벤토리 UI (UMG, 최소한으로) |
| Inventory + Economy | 슬롯 기반 인벤토리, 아이템 판매, 자판기 비용 |
| Asset & Data Driven | UPrimaryDataAsset 기반 무기/아이템/자판기 테이블 |

### 후순위

| 영역 | 비고 |
|------|------|
| Effective Modern C++ | 엔진 코드 읽기용, 취업 직결은 아님 |
| DX12 / 그래픽스 | 게임플레이 클라 포지션에서는 불필요 |

---

## 3. 구현 로드맵 (4단계)

### 1단계: Gameplay Framework + Reflection
- GameMode에서 MatchState 관리 (Waiting -> Playing -> Ended)
- GameState에 RemainingTime 복제
- PlayerController에서 스폰/리스폰 요청
- PlayerState에 Kills, Extracted 복제 (COND_OwnerOnly). Money는 인벤토리 아이템으로 처리.
- 모든 핵심 변수 UPROPERTY로 노출/복제
- 핵심 함수 UFUNCTION(Server, Reliable) / BlueprintCallable 등 적절히 표시
- Data-driven: UDataAsset / UPrimaryDataAsset으로 무기/아이템 정의

### 2단계: Replication + Movement Component
- 기본 이동/점프는 CharacterMovement가 네트워크 처리
- 확장 요소: 달리기(속도 변경), 앉기/일어서기, 에임(속도/회전 제한)
- 복제 원칙: 서버가 Authority, 클라 입력은 "요청", 서버가 "결정"
- 필수 구현:
  - `bIsSprinting` : ReplicatedUsing=OnRep_IsSprinting
  - `EquippedWeapon` : Replicate
  - `HP` : Replicate + OnRep으로 UI 갱신

### 3단계: Client Prediction / Reconciliation / Lag Compensation
- 이동 예측/보정은 CharacterMovement가 처리. 사격/히트에서 직접 구현.

**Hit Validation (서버 권한 검증)**
- 클라는 "이 시점에 이 방향으로 쐈다"만 전송
- 서버가 레이캐스트로 판정
- RPC: `Server_Fire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir, float ClientFireTime)`

**Lag Compensation (서버 리와인드)**
- 서버에서 각 캐릭터의 과거 위치/회전 캡슐을 링버퍼로 저장 (100ms 간격, 1~2초치)
- ClientFireTime을 서버 시간으로 환산
- 해당 시각의 히트박스 복원 -> 레이캐스트 -> 판정 후 원상복구

**Reconciliation (보정)**
- 서버에서 확정 이벤트 전송
- 클라는 결과에 맞게 VFX/사운드 정리

### 4단계: GAS (스킬 시스템)
- AttributeSet: HP, Stamina, Shield
- GameplayEffect: 데미지, 힐, 버프
- GameplayAbility: Dash, Heal, ShieldOn (3개면 충분)
- 면접에서 보는 것: Ability 발동 흐름, Attribute 변화, Effect 적용/태그 처리, 네트워크 동작 방식

### 5단계: Persistence (영속 데이터)
- 4단계까지 완료 후 진행
- 1차: USaveGame으로 로컬 저장 구조 구현 (스태시 인벤토리, 플레이어 진행도, 재화)
- 2차: 외부 DB 연동 (데디서버 ↔ REST API ↔ DB). 치트 방지를 위해 서버에서만 DB 접근
- 대상 데이터: 인벤토리/스태시, 계정(레벨/재화), 퀘스트 진행도, 상점/거래 기록

---

## 4. 포트폴리오 프로젝트 설계

### 컨셉: EmploymentProj (취업 테마 익스트랙션 슈터)
- 작은 맵(창고/컨테이너 구역), 2~8인 멀티
- 루프: 랜덤 스폰 -> 파밍(바닥 루팅 + 자판기) -> 교전(플레이어/AI) -> 탈출 -> 보상
- 핵심 시스템: 자판기(뽑기) - 돈 투입 → 5초 대기(소리 전파) → 확률 아이템 배출
- 진행 목표: 퀘스트 아이템 수집 → "취업" 달성
- 게임 디자인 상세: GAME.md 참조

### Gameplay Framework 배치

| 클래스 | 역할 | 권한 |
|--------|------|------|
| GameMode | 라운드 규칙, 스폰 규칙, 승패/탈출 판정, 매치 상태 전환, 자판기 결과 판정 | 서버 전용 |
| GameState | 라운드 시간, 매치 상태 (플레이어 수는 비공개 → GameMode 전용) | 복제됨 |
| PlayerController | 입력 처리, UI, 서버에 명령 요청(RPC), 자판기 상호작용 요청 | 소유자 전용 |
| PlayerState | 킬 수(COND_OwnerOnly)/탈출 여부(COND_OwnerOnly)/퀘스트 진행도. 돈은 인벤토리 아이템. | 복제됨 |
| Pawn/Character | 이동, 애니, 무기, 피격, 능력(GAS) | 복제됨 |
| HUD/UMG | UI 표시 | 클라 전용(최소만) |

### 시스템별 기술 포인트

| 시스템 | 기술 증명 포인트 |
|--------|-----------------|
| 자판기(뽑기) | 서버 권한 확률 판정, 상태 복제(사용중/대기), 사운드 Multicast RPC, DataAsset 기반 아이템 테이블 |
| AI 적 | Behavior Tree, 감지/추적/사격, 서버 권한 AI 로직 |
| 사격/히트 | 서버 권한 레이캐스트, Lag Compensation, Hit Validation |
| Extraction | Zone 진입 판정(서버), 탈출 상태 복제(PlayerState), 타이머 |
| Inventory | 슬롯 기반 아이템 관리, 장비 장착/해제 |
| Economy | 아이템 판매, 자판기 비용, 킬/탈출 보상 |
| Quest | 퀘스트 아이템 수집 진행도 관리, 완료 조건 판정 |
| Data Driven | UPrimaryDataAsset으로 무기/아이템/자판기 테이블 정의 |
| Animation | AnimBP 스테이트 머신, 몽타주 (사격/재장전/사망), 네트워크 복제 (SimulatedProxy 동기화) |
| Map | 레벨 디자인, 파밍 포인트/자판기/탈출 지점 배치, 라이팅, 내비메시 (AI 이동용) |
| UI/HUD | UMG 위젯 (체력바, 탄약, 타이머, 킬 피드, 인벤토리 화면) |

### Git 전략
- `main` (항상 빌드됨)
- `feature/net-fire`, `feature/gas`, `feature/extraction` 등 기능별 브랜치
- PR 템플릿: 변경 요약 / 테스트 방법

---

## 5. 실행 순서 (압축)

1. 싱글에서 매치 흐름 (GameMode/GameState)
2. 멀티 접속/랜덤 스폰/이동 (기본 Replication)
3. 캐릭터 애니메이션 (AnimBP, 스테이트 머신, 이동/점프 블렌드)
4. 사격 RPC + 서버 히트 판정 + 사격 몽타주
5. HP 복제 + 피격 처리 + 사망 애니메이션
6. Lag Compensation (히스토리/리와인드)
7. 자판기 시스템 (서버 판정 + 상태 복제 + Multicast 사운드)
8. AI 적 (Behavior Tree + 서버 권한 로직)
9. GAS로 대시/힐/실드
10. 인벤토리 + 장비 + 판매
11. Extraction + 퀘스트 수집
12. UI/HUD (체력바, 탄약, 타이머, 킬 피드, 인벤토리 화면)
13. 맵 제작 (레벨 디자인, 파밍 포인트, 자판기/탈출 지점 배치, 내비메시)
14. USaveGame으로 영속 데이터 구조 (스태시, 진행도)
15. 외부 DB 연동 (REST API + DB, 서버 권한 접근)
