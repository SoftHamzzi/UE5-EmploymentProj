# 1단계 구현 설계서: Gameplay Framework + Reflection

> 이 문서는 01_GameplayFramework.md의 학습 내용을 기반으로,
> 실제 코드를 작성하기 위한 **구체적인 클래스 설계와 구현 순서**를 정의한다.

---

## 0. 설계 원칙

**이 프로젝트는 취업 포트폴리오용이다.** 단순히 동작하는 코드가 아니라, 구조적으로 완전한 코드를 목표로 한다.

- **Public/Private 폴더 분리**: 단일 모듈이라도 UE5 표준 모듈 구조(Public/Private)를 따른다. 실무 멀티모듈 프로젝트와 동일한 구조를 보여주기 위함.
- **접근 제한자 명확히**: 외부에서 접근할 필요 없는 멤버는 protected/private으로. UPROPERTY 지정자도 최소 권한 원칙.
- **헤더 include 최소화**: 전방 선언(forward declaration) 적극 사용. .cpp에서만 실제 include.
- **주석은 "왜"에 집중**: 무엇을 하는지는 코드가 말하게 하고, 왜 이 방식을 선택했는지를 주석으로.
- **서버 권한 원칙 철저**: 모든 게임 로직은 서버에서 실행. 클라이언트는 요청만. HasAuthority() 체크 습관화.

---

## 1. Source 폴더 구조

```
Source/
├── EmploymentProj.Target.cs
├── EmploymentProjEditor.Target.cs
└── EmploymentProj/
    ├── EmploymentProj.Build.cs
    ├── EmploymentProj.h              (모듈 헤더 - LOG 카테고리 등)
    ├── EmploymentProj.cpp            (모듈 구현)
    │
    ├── Public/                       ← 외부 모듈에 노출 가능한 헤더
    │   ├── Core/
    │   │   ├── EPGameMode.h
    │   │   ├── EPGameState.h
    │   │   ├── EPPlayerController.h
    │   │   ├── EPPlayerState.h
    │   │   └── EPCharacter.h
    │   ├── Data/
    │   │   ├── EPWeaponData.h
    │   │   └── EPItemData.h
    │   └── Types/
    │       └── EPTypes.h
    │
    └── Private/                      ← 모듈 내부 구현
        ├── Core/
        │   ├── EPGameMode.cpp
        │   ├── EPGameState.cpp
        │   ├── EPPlayerController.cpp
        │   ├── EPPlayerState.cpp
        │   └── EPCharacter.cpp
        └── Data/
            ├── EPWeaponData.cpp
            └── EPItemData.cpp
```

### 네이밍 규칙
- 접두사 `EP` (EmploymentProj) 사용
- Actor 클래스: `AEP___` (예: AEPCharacter)
- Object 클래스: `UEP___` (예: UEPWeaponData)
- Struct: `FEP___` (예: FEPItemInfo)
- Enum: `EEP___` (예: EEPMatchPhase)
- 파일명: 접두사 없이 `EP___` (예: EPGameMode.h)

---

## 2. 공용 타입 정의 (EPTypes.h)

가장 먼저 만들어야 할 파일. 다른 클래스들이 참조하는 Enum/Struct 모음.

```cpp
// EPTypes.h
#pragma once

#include "CoreMinimal.h"
#include "EPTypes.generated.h"

// 매치 진행 단계 (GameState에서 복제용)
// AGameMode의 내부 MatchState(FName)와 별개로, 게임 로직용 단순 enum
UENUM(BlueprintType)
enum class EEPMatchPhase : uint8
{
    Waiting    UMETA(DisplayName = "Waiting"),    // 접속 대기
    Playing    UMETA(DisplayName = "Playing"),    // 매치 진행
    Ended      UMETA(DisplayName = "Ended")       // 매치 종료
};

// 아이템 등급
UENUM(BlueprintType)
enum class EEPItemRarity : uint8
{
    Common,     // 일반 (50%)
    Uncommon,   // 고급 (30%)
    Rare,       // 희귀 (15%)
    Legendary   // 전설 (5%)
};

// 무기 발사 모드
UENUM(BlueprintType)
enum class EEPFireMode : uint8
{
    Single,     // 단발
    Auto        // 연사
};
```

---

## 3. 클래스별 헤더 스켈레톤

### 3-1. AEPGameMode (← AGameMode 상속)

**역할**: 서버 전용. 매치 규칙, 스폰, 상태 전환.

```cpp
// EPGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "EPGameMode.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API AEPGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    AEPGameMode();

protected:
    // --- 매치 설정 ---

    // 매치 제한 시간 (초). 에디터에서 조정 가능.
    UPROPERTY(EditDefaultsOnly, Category = "Match")
    float MatchDuration = 300.f;  // 5분

    // 매치 시작에 필요한 최소 플레이어 수
    UPROPERTY(EditDefaultsOnly, Category = "Match")
    int32 MinPlayersToStart = 1;

    // 매치 타이머 핸들
    FTimerHandle MatchTimerHandle;

    // --- AGameMode 오버라이드 ---

    // 플레이어 로그인 완료 시
    virtual void PostLogin(APlayerController* NewPlayer) override;

    // 플레이어 로그아웃 시
    virtual void Logout(AController* Exiting) override;

    // 스폰 위치 결정 (랜덤 배정)
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

    // MatchState 변경 시 호출
    virtual void HandleMatchHasStarted() override;
    virtual void HandleMatchHasEnded() override;

    // 매치 시작 가능 여부
    virtual bool ReadyToStartMatch_Implementation() override;

    // --- 매치 로직 ---

    // 매치 타이머 틱 (1초마다)
    void TickMatchTimer();

    // 매치 종료 처리 (시간 초과)
    void EndMatchByTimeout();

    // 생존 플레이어 확인 → 전원 탈출/사망 시 매치 종료
    void CheckMatchEndConditions();
};
```

**핵심 포인트:**
- `AGameMode` 상속 → `MatchState` 기반 (WaitingToStart → InProgress → WaitingPostMatch) 활용
- `ReadyToStartMatch_Implementation()`: MinPlayersToStart 충족 시 true 반환 → 자동으로 InProgress 전환
- `HandleMatchHasStarted()`: 타이머 시작, GameState에 매치 시작 알림
- `HandleMatchHasEnded()`: 결과 정산
- `ChoosePlayerStart_Implementation()`: 사용되지 않은 PlayerStart를 랜덤 선택

---

### 3-2. AEPGameState (← AGameState 상속)

**역할**: 모든 클라이언트에 복제되는 게임 상태.

```cpp
// EPGameState.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Types/EPTypes.h"
#include "EPGameState.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API AEPGameState : public AGameState
{
    GENERATED_BODY()

public:
    AEPGameState();

    // --- 복제 변수 ---

    // 매치 남은 시간 (초). 서버에서 매 초 갱신.
    UPROPERTY(ReplicatedUsing = OnRep_RemainingTime, BlueprintReadOnly, Category = "Match")
    float RemainingTime;

    // 현재 매치 단계 (UI 표시용)
    UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, BlueprintReadOnly, Category = "Match")
    EEPMatchPhase MatchPhase;

    // 생존 플레이어 수
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
    int32 AlivePlayerCount;

    // --- OnRep 콜백 ---

    UFUNCTION()
    void OnRep_RemainingTime();

    UFUNCTION()
    void OnRep_MatchPhase();

    // --- 서버 전용 함수 ---

    // GameMode에서 호출. 남은 시간 설정.
    void SetRemainingTime(float NewTime);

    // GameMode에서 호출. 매치 단계 변경.
    void SetMatchPhase(EEPMatchPhase NewPhase);

protected:
    // Replication 등록
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

**핵심 포인트:**
- `RemainingTime`: OnRep으로 클라에서 UI 갱신 트리거
- `MatchPhase`: 게임 로직용 단순 enum (AGameMode의 MatchState와 동기화)
- `GetLifetimeReplicatedProps()`: 모든 Replicated 변수를 등록하는 필수 오버라이드

---

### 3-3. AEPPlayerController (← APlayerController 상속)

**역할**: 소유 클라이언트의 입력/UI 처리, 서버 RPC 요청.

```cpp
// EPPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EPPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

UCLASS()
class EMPLOYMENTPROJ_API AEPPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AEPPlayerController();

protected:
    // --- Enhanced Input ---

    // 기본 Input Mapping Context (이동, 점프, 시점 등)
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    // 각 Input Action (에디터에서 할당)
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> JumpAction;

    // --- 오버라이드 ---

    virtual void BeginPlay() override;

    // Input Mapping Context 등록
    virtual void OnPossess(APawn* InPawn) override;
};
```

**핵심 포인트:**
- Enhanced Input 사용 (UE5 표준)
- `BeginPlay()`에서 Input Mapping Context를 Subsystem에 등록
- Input Action 바인딩은 Character 쪽에서 처리 (SetupPlayerInputComponent)
- 1단계에서는 기본 이동 입력만. RPC는 2단계 이후에 추가.

---

### 3-4. AEPPlayerState (← APlayerState 상속)

**역할**: 플레이어별 복제 데이터.

```cpp
// EPPlayerState.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "EPPlayerState.generated.h"

UCLASS()
class EMPLOYMENTPROJ_API AEPPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    AEPPlayerState();

    // --- 복제 변수 ---

    // 소지금
    UPROPERTY(ReplicatedUsing = OnRep_Money, BlueprintReadOnly, Category = "Stats")
    int32 Money;

    // 킬 수
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
    int32 KillCount;

    // 탈출 성공 여부
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
    bool bIsExtracted;

    // 사망 여부
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stats")
    bool bIsDead;

    // --- OnRep 콜백 ---

    UFUNCTION()
    void OnRep_Money();

    // --- 서버 전용 함수 ---

    // 돈 추가 (킬 보상, 탈출 보너스 등)
    void AddMoney(int32 Amount);

    // 킬 카운트 증가
    void AddKill();

    // 탈출 처리
    void SetExtracted(bool bExtracted);

    // 사망 처리
    void SetDead(bool bDead);

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

**핵심 포인트:**
- `Money`만 OnRep 사용 (UI 갱신 필요). 나머지는 단순 Replicated.
- Setter 함수는 서버에서만 호출. Authority 체크 포함.
- 1단계에서는 변수 선언 + 복제 등록까지. 실제 로직은 2단계에서.

---

### 3-5. AEPCharacter (← ACharacter 상속)

**역할**: 월드에서의 물리적 존재. 이동, 카메라.

```cpp
// EPCharacter.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EPCharacter.generated.h"

class UCameraComponent;
class UInputAction;
struct FInputActionValue;

UCLASS()
class EMPLOYMENTPROJ_API AEPCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AEPCharacter();

protected:
    // --- 컴포넌트 ---

    // 1인칭 카메라
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    // --- 오버라이드 ---

    virtual void BeginPlay() override;

    // Enhanced Input 바인딩
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // --- 입력 핸들러 ---

    // 이동 (WASD)
    void Input_Move(const FInputActionValue& Value);

    // 시점 (마우스)
    void Input_Look(const FInputActionValue& Value);

    // 점프
    void Input_Jump(const FInputActionValue& Value);
    void Input_StopJumping(const FInputActionValue& Value);
};
```

**핵심 포인트:**
- Constructor에서 `CreateDefaultSubobject<UCameraComponent>` → CapsuleComponent에 Attach
- `SetupPlayerInputComponent()`에서 Enhanced Input Action 바인딩
- ACharacter 상속 → CharacterMovementComponent 자동 포함 (이동/점프/네트워크 처리)
- 1단계에서는 이동 + 카메라만. 달리기/앉기/무기는 2단계.

---

### 3-6. UEPWeaponData (← UPrimaryDataAsset 상속)

**역할**: 무기 속성 정의. 에디터에서 DataAsset으로 생성.

```cpp
// EPWeaponData.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/EPTypes.h"
#include "EPWeaponData.generated.h"

UCLASS(BlueprintType)
class EMPLOYMENTPROJ_API UEPWeaponData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 무기 이름
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FName WeaponName;

    // 탄당 데미지
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    float Damage = 20.f;

    // 연사 속도 (초당 발수)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    float FireRate = 5.f;

    // 반동 크기
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    float Recoil = 1.f;

    // 탄퍼짐 (기본값, 이동/점프 시 증가)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    float Spread = 0.5f;

    // 최대 탄약
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    int32 MaxAmmo = 30;

    // 발사 모드
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    EEPFireMode FireMode = EEPFireMode::Auto;

    // PrimaryDataAsset ID
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
```

---

### 3-7. UEPItemData (← UPrimaryDataAsset 상속)

**역할**: 일반 아이템 속성 정의.

```cpp
// EPItemData.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/EPTypes.h"
#include "EPItemData.generated.h"

UCLASS(BlueprintType)
class EMPLOYMENTPROJ_API UEPItemData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // 아이템 이름
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    FName ItemName;

    // 아이템 설명
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    FText Description;

    // 등급
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    EEPItemRarity Rarity = EEPItemRarity::Common;

    // 판매 가격
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    int32 SellPrice = 100;

    // 퀘스트 아이템 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    bool bIsQuestItem = false;

    // 인벤토리 슬롯 차지 수 (기본 1)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    int32 SlotSize = 1;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
```

---

## 4. GameMode 기본 클래스 등록

GameMode에서 사용할 기본 클래스를 Constructor에서 지정:

```cpp
// EPGameMode.cpp - Constructor
AEPGameMode::AEPGameMode()
{
    // 기본 클래스 설정
    DefaultPawnClass = AEPCharacter::StaticClass();
    PlayerControllerClass = AEPPlayerController::StaticClass();
    PlayerStateClass = AEPPlayerState::StaticClass();
    GameStateClass = AEPGameState::StaticClass();
}
```

그리고 `.uproject` 또는 `DefaultEngine.ini`에서 이 GameMode를 기본으로 지정하거나,
에디터의 **World Settings → GameMode Override**에서 AEPGameMode를 선택.

---

## 5. 구현 순서

각 단계는 빌드 + 에디터에서 확인 가능한 단위로 나눈다.

### Step 1: 프로젝트 뼈대 (빌드 확인)
1. `Types/EPTypes.h` 생성 (Enum 정의)
2. 기존 `EP_GameModeBase` 파일 삭제
3. `Core/EPGameMode.h/.cpp` 생성 (빈 Constructor만)
4. `Core/EPGameState.h/.cpp` 생성 (빈 Constructor만)
5. `Core/EPPlayerController.h/.cpp` 생성 (빈 Constructor만)
6. `Core/EPPlayerState.h/.cpp` 생성 (빈 Constructor만)
7. `Core/EPCharacter.h/.cpp` 생성 (빈 Constructor만)
8. **빌드 확인** → 컴파일 에러 없는지

**테스트**: 에디터 실행 → World Settings에서 AEPGameMode 선택 가능한지 확인

### Step 2: Character 기본 이동
1. EPCharacter Constructor에서 카메라 컴포넌트 생성
2. EPCharacter::SetupPlayerInputComponent()에서 입력 바인딩
3. EPPlayerController::BeginPlay()에서 Mapping Context 등록
4. **빌드 확인**

**테스트**: PIE(Play In Editor)에서 WASD 이동 + 마우스 시점 + 점프 동작 확인

### Step 3: GameMode 매치 흐름
1. EPGameMode Constructor에서 기본 클래스 설정
2. ReadyToStartMatch 구현 (최소 인원 체크)
3. HandleMatchHasStarted: 타이머 시작, GameState 업데이트
4. HandleMatchHasEnded: 타이머 정리
5. TickMatchTimer: 매 초 GameState.RemainingTime 갱신
6. **빌드 확인**

**테스트**: PIE에서 게임 시작 → 로그로 MatchState 전환 확인 (WaitingToStart → InProgress)

### Step 4: GameState 복제
1. GetLifetimeReplicatedProps 구현
2. OnRep 함수 구현 (로그 출력)
3. SetRemainingTime / SetMatchPhase 구현
4. **빌드 확인**

**테스트**: 데디서버 + 2클라이언트로 테스트 → 클라에서 RemainingTime 감소 확인

### Step 5: PlayerState 복제
1. GetLifetimeReplicatedProps 구현
2. Money/KillCount/bIsExtracted/bIsDead 복제 등록
3. Setter 함수 구현 (Authority 체크)
4. OnRep_Money 구현 (로그 출력)
5. **빌드 확인**

**테스트**: 콘솔 명령으로 서버에서 Money 변경 → 클라에서 반영 확인

### Step 6: 스폰 시스템
1. EPGameMode::ChoosePlayerStart_Implementation: 사용되지 않은 PlayerStart 랜덤 선택
2. 맵에 PlayerStart 여러 개 배치
3. PostLogin에서 로그 출력
4. **빌드 확인**

**테스트**: 2인 이상 접속 시 다른 위치에 스폰되는지 확인

### Step 7: DataAsset
1. UEPWeaponData / UEPItemData 클래스 생성
2. GetPrimaryAssetId 구현
3. **빌드 확인**

**테스트**: 에디터에서 Content Browser → 우클릭 → Miscellaneous → Data Asset → UEPWeaponData 선택하여 Pistol/Rifle 에셋 생성 가능한지 확인

---

## 6. 멀티플레이어 테스트 방법

에디터에서 데디서버 + 클라이언트 테스트:

1. **에디터 상단** → Play 버튼 옆 드롭다운
2. **Net Mode**: `Play As Listen Server` 또는 `Play As Client`
3. **Number of Players**: 2~3
4. `Run Dedicated Server` 체크

또는 콘솔에서:
```
# 데디서버 실행
UE5Editor.exe ProjectName MapName -server -log

# 클라이언트 접속
UE5Editor.exe ProjectName 127.0.0.1 -game -log
```

---

## 7. 완료 기준

1단계가 끝났다고 할 수 있는 조건:

- [ ] 에디터에서 AEPGameMode가 기본 GameMode로 설정됨
- [ ] PIE에서 캐릭터가 스폰되고 WASD/마우스/점프로 이동 가능
- [ ] 매치 시작 시 MatchState가 InProgress로 전환되고 타이머 작동
- [ ] 제한 시간 만료 시 매치 종료 (WaitingPostMatch 전환)
- [ ] 2인 접속 시 각각 다른 위치에 스폰
- [ ] 클라이언트에서 RemainingTime이 실시간 감소 (복제 확인)
- [ ] 클라이언트에서 PlayerState 변수(Money 등) 복제 확인
- [ ] DataAsset으로 Pistol, Rifle 에셋 생성 가능
- [ ] 모든 핵심 변수에 UPROPERTY 매크로 적용
- [ ] 모든 클래스에 UCLASS, GENERATED_BODY 적용

---

## 8. 주의사항

### GetLifetimeReplicatedProps 패턴
모든 Replicated 변수는 이 함수에서 등록해야 한다:
```cpp
void AEPGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AEPGameState, RemainingTime);
    DOREPLIFETIME(AEPGameState, MatchPhase);
    DOREPLIFETIME(AEPGameState, AlivePlayerCount);
}
```
- `#include "Net/UnrealNetwork.h"` 필수
- `Super::` 호출 빠뜨리지 말 것

### Authority 체크
서버 전용 함수에서는 반드시 확인:
```cpp
void AEPPlayerState::AddMoney(int32 Amount)
{
    if (!HasAuthority()) return;  // 서버에서만 실행
    Money += Amount;
}
```

### Constructor vs BeginPlay
- **Constructor**: CreateDefaultSubobject로 컴포넌트 생성. 게임 로직 X.
- **BeginPlay**: 게임 로직 초기화. GetWorld(), 다른 Actor 참조 가능.
- Constructor에서 GetWorld() 호출하면 nullptr.

### Enhanced Input 등록 위치
```cpp
// EPPlayerController::BeginPlay 또는 OnPossess에서
if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
    ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
{
    Subsystem->AddMappingContext(DefaultMappingContext, 0);
}
```
- 반드시 LocalPlayer가 유효한 시점에서 호출
- BeginPlay에서 하는 것이 가장 안전

### .Build.cs 모듈 의존성
현재 `EnhancedInput`은 이미 추가되어 있음. 추가로 필요한 것:
- `Net/UnrealNetwork.h` 사용 시 별도 모듈 추가 불필요 (Engine에 포함)
- GAS 추가 시 (4단계): `GameplayAbilities`, `GameplayTags`, `GameplayTasks` 추가 필요
