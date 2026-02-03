# 1단계: Gameplay Framework + Reflection

## 1. Gameplay Framework 개요

UE5의 Gameplay Framework는 멀티플레이어 게임의 구조를 정의하는 클래스 집합.
"어떤 데이터를 어디에 두고, 누가 소유하고, 누구에게 보이는가"를 결정하는 틀이다.

### 핵심 질문: "왜 이 클래스가 나뉘어 있는가?"

멀티플레이어에서는 서버/클라이언트가 서로 다른 정보를 가진다.
모든 것을 하나의 클래스에 넣으면 "이건 서버만 알아야 하는 건가, 모두가 알아야 하는 건가?"를 구분할 수 없다.
그래서 역할별로 클래스를 분리한 것이 Gameplay Framework.

---

## 2. 핵심 클래스 관계도

```
UGameInstance (프로세스 전체, 맵 전환에도 유지)
  └─ UWorld (현재 맵)
       ├─ AGameModeBase / AGameMode (서버에만 존재)
       ├─ AGameStateBase / AGameState (서버 + 모든 클라에 복제)
       └─ APlayerController (플레이어당 1개)
            ├─ APlayerState (서버 + 모든 클라에 복제)
            └─ APawn / ACharacter (Possess로 연결)
                 └─ UActorComponent들 (Movement, Mesh, etc.)
```

---

## 3. 각 클래스의 역할

### AGameModeBase / AGameMode
- **서버에만 존재**. 클라이언트에서 GetGameMode()하면 nullptr.
- 게임의 "규칙"을 정의: 스폰 위치 결정, 매치 상태 전환, 승패 판정
- AGameMode는 AGameModeBase를 상속하며 MatchState 관리 기능 추가

**EmploymentProj 적용:**
- MatchState 관리 (Waiting → Playing → Ended)
- 스폰 포인트 랜덤 배정
- 탈출 판정
- 자판기 결과 판정 (서버 권한 확률 계산)

**핵심 함수:**
```cpp
// 플레이어 로그인 시 호출 (서버)
virtual void PostLogin(APlayerController* NewPlayer);

// 기본 Pawn 클래스 결정
virtual UClass* GetDefaultPawnClassForController(AController* InController);

// 스폰 위치 결정
virtual AActor* ChoosePlayerStart(AController* Player);

// 매치 상태 변경 (AGameMode)
void SetMatchState(FName NewState);

// 매치 상태 변경 시 호출
virtual void HandleMatchHasStarted();
virtual void HandleMatchHasEnded();
```

**AGameMode의 MatchState 흐름:**
```
EnteringMap → WaitingToStart → InProgress → WaitingPostMatch → LeavingMap
```

---

### AGameStateBase / AGameState
- **서버 + 모든 클라이언트에 복제됨**
- "모두가 알아야 하는 게임 상태": 경과 시간, 남은 시간, 접속 플레이어 목록 등
- PlayerArray: 현재 접속한 모든 PlayerState의 배열

**EmploymentProj 적용:**
- RemainingTime (매치 남은 시간)
- 현재 MatchState
- 생존 플레이어 수

**핵심 멤버:**
```cpp
// 모든 PlayerState 배열 (복제됨)
TArray<APlayerState*> PlayerArray;

// 서버 월드 시간
double GetServerWorldTimeSeconds() const;
```

---

### APlayerController
- **소유 클라이언트에만 의미 있음**
- 입력 처리, UI 관리, 서버에 명령을 보내는 RPC의 주체
- Pawn을 Possess하여 조종
- 서버에도 존재하지만, 다른 클라이언트에서는 자신의 PlayerController만 존재

**EmploymentProj 적용:**
- 입력 처리 (Enhanced Input)
- 자판기 상호작용 요청 (Server RPC)
- UI 열기/닫기

**핵심 함수:**
```cpp
// Pawn 빙의
virtual void Possess(APawn* InPawn);
virtual void UnPossess();

// 입력 설정
virtual void SetupInputComponent();

// 서버 RPC 예시
UFUNCTION(Server, Reliable)
void Server_RequestUseVendingMachine(AVendingMachine* Machine);
```

---

### APlayerState
- **서버 + 모든 클라이언트에 복제됨**
- "이 플레이어에 대해 모두가 알아야 하는 정보"
- PlayerController가 사라져도(맵 전환 등) 유지될 수 있음

**EmploymentProj 적용:**
- Money (소지금)
- KillCount
- bIsExtracted (탈출 여부)
- 퀘스트 진행도

**핵심 멤버:**
```cpp
UPROPERTY(Replicated)
float Score;

UPROPERTY(Replicated)
FString PlayerNamePrivate;

// 소유 PlayerController
APlayerController* GetPlayerController() const;
```

---

### APawn / ACharacter
- **플레이어가 월드에서 조종하는 물리적 존재**
- APawn: 기본 (이동 로직 직접 구현)
- ACharacter: APawn + CapsuleComponent + CharacterMovementComponent + SkeletalMesh
- FPS에서는 ACharacter 사용이 일반적 (CMC의 네트워크 이동 처리 활용)

**ACharacter가 기본 제공하는 것:**
```cpp
UCapsuleComponent* CapsuleComponent;        // 충돌
USkeletalMeshComponent* Mesh;               // 3인칭 메시
UCharacterMovementComponent* CharacterMovement; // 이동 + 네트워크
```

---

### UActorComponent / USceneComponent
- **Actor에 붙는 모듈식 기능 단위**
- UActorComponent: Transform 없음 (순수 로직)
- USceneComponent: Transform 있음 (위치/회전/스케일)

**예시:**
- UCharacterMovementComponent (이동)
- UCameraComponent (카메라)
- UInventoryComponent (커스텀 - 인벤토리 로직)
- UAbilitySystemComponent (GAS)

---

## 4. Actor Lifecycle (생명주기)

### 생성 ~ 파괴 순서
```
Constructor (CDO 생성 시, 게임 중 X)
  → PostInitializeComponents()
  → BeginPlay()
  → Tick() (매 프레임)
  → EndPlay()
  → Destroyed()
  → GC에 의해 메모리 해제
```

### 주의사항
- **Constructor**: CDO(Class Default Object) 생성용. 게임 로직 넣지 말 것. CreateDefaultSubobject로 컴포넌트 생성.
- **PostInitializeComponents()**: 모든 컴포넌트 초기화 완료 후 호출. 컴포넌트 간 참조 설정에 적합.
- **BeginPlay()**: 게임 시작 시 호출. 게임 로직 초기화는 여기서.
- **Tick()**: 매 프레임. 가능하면 Timer나 이벤트 기반으로 대체 (성능).

### Possess 흐름
```
PlayerController::Possess(Pawn)
  → Pawn::PossessedBy(Controller)        // 서버에서 호출
  → PlayerController::OnPossess(Pawn)
  → Pawn::OnRep_Controller()             // 클라이언트에서 호출 (복제 후)
  → Pawn::SetupPlayerInputComponent()
```

---

## 5. Reflection System (리플렉션)

### 개념
C++은 런타임에 클래스/변수/함수 정보를 알 수 없다 (Java/C#과 다름).
UE5는 **UHT(Unreal Header Tool)**가 빌드 시 헤더를 파싱하여 메타데이터를 생성하는 방식으로 리플렉션을 구현.

### 왜 필요한가?
- **Replication**: 어떤 변수를 네트워크로 복제할지 런타임에 알아야 함
- **GC**: UObject 간 참조를 추적하여 가비지 컬렉션
- **Serialization**: 세이브/로드
- **Blueprint**: C++ 함수/변수를 에디터에서 접근
- **에디터 노출**: UPROPERTY의 EditAnywhere, VisibleAnywhere 등

### 핵심 매크로

**UCLASS()**
```cpp
UCLASS()
class AMyCharacter : public ACharacter
{
    GENERATED_BODY()
    // ...
};
```
- UObject 기반 클래스 등록
- GENERATED_BODY() 필수 (UHT가 생성한 코드 삽입)

**UPROPERTY()**
```cpp
// 에디터에서 편집 가능 + 네트워크 복제
UPROPERTY(EditAnywhere, Replicated)
float HP;

// 복제 + 변경 시 콜백
UPROPERTY(ReplicatedUsing = OnRep_HP)
float HP;

// Blueprint에서 읽기/쓰기
UPROPERTY(BlueprintReadWrite, Category = "Stats")
float Stamina;
```

주요 지정자:
| 지정자 | 의미 |
|--------|------|
| EditAnywhere | 에디터 디테일 패널에서 편집 가능 |
| VisibleAnywhere | 에디터에서 보이지만 편집 불가 |
| BlueprintReadOnly | BP에서 읽기만 |
| BlueprintReadWrite | BP에서 읽기/쓰기 |
| Replicated | 네트워크 복제 |
| ReplicatedUsing=함수명 | 복제 + 변경 시 콜백 호출 |
| Transient | 직렬화(저장) 안 함 |

**UFUNCTION()**
```cpp
// 서버 RPC
UFUNCTION(Server, Reliable)
void Server_Fire(FVector Origin, FVector Direction);

// 클라이언트 RPC
UFUNCTION(Client, Reliable)
void Client_OnHitConfirmed(FHitResult HitResult);

// 멀티캐스트
UFUNCTION(NetMulticast, Unreliable)
void Multicast_PlayFireEffect();

// Blueprint 호출 가능
UFUNCTION(BlueprintCallable, Category = "Combat")
void StartFiring();
```

**USTRUCT() / UENUM()**
```cpp
USTRUCT(BlueprintType)
struct FItemData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FName ItemName;

    UPROPERTY(EditAnywhere)
    int32 Value;
};

UENUM(BlueprintType)
enum class EMatchState : uint8
{
    Waiting,
    Playing,
    Ended
};
```

---

## 6. UObject와 메모리 관리

### UObject 기반 클래스 (GC 대상)
- AActor, UActorComponent, UGameInstance 등 모두 UObject 상속
- `NewObject<T>()` 로 생성
- 참조가 없으면 GC가 자동 수거
- UPROPERTY()로 선언된 포인터만 GC가 추적 → **반드시 UPROPERTY() 붙일 것**

```cpp
// 올바른 사용
UPROPERTY()
UMyObject* MyObj;

// 위험 - GC가 추적하지 못함, 댕글링 포인터 가능
UMyObject* MyObj; // UPROPERTY 없음
```

### Non-UObject (수동 관리)
- FStruct, 일반 C++ 클래스
- new/delete 또는 TSharedPtr/TUniquePtr 사용

### Actor 생성
```cpp
// 월드에 Actor 스폰
FActorSpawnParameters Params;
Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
AMyActor* Actor = GetWorld()->SpawnActor<AMyActor>(ActorClass, SpawnLocation, SpawnRotation, Params);
```

---

## 7. EmploymentProj 1단계 구현 체크리스트

- [ ] 커스텀 GameMode 생성 (AEPGameMode)
  - [ ] MatchState 관리 (Waiting → Playing → Ended)
  - [ ] 랜덤 스폰 포인트 배정 (ChoosePlayerStart 오버라이드)
- [ ] 커스텀 GameState 생성 (AEPGameState)
  - [ ] RemainingTime 복제
  - [ ] 현재 MatchState 복제
- [ ] 커스텀 PlayerController 생성 (AEPPlayerController)
  - [ ] Enhanced Input 설정
- [ ] 커스텀 PlayerState 생성 (AEPPlayerState)
  - [ ] Money, KillCount, bIsExtracted 복제
- [ ] 커스텀 Character 생성 (AEPCharacter)
  - [ ] 기본 이동 (CharacterMovementComponent)
  - [ ] 1인칭 카메라
- [ ] DataAsset 정의 (UEPWeaponData, UEPItemData)
  - [ ] 무기 속성: Damage, FireRate, Recoil, Spread, MaxAmmo, FireMode

---

## 8. 참고 자료

### 공식 문서
- Gameplay Framework 개요: `docs.unrealengine.com/5.0/en-US/gameplay-framework-in-unreal-engine/`
- GameMode and GameState: `docs.unrealengine.com/5.0/en-US/game-mode-and-game-state-in-unreal-engine/`
- Actor Lifecycle: `docs.unrealengine.com/5.0/en-US/unreal-engine-actor-lifecycle/`
- UObject: `docs.unrealengine.com/5.0/en-US/objects-in-unreal-engine/`
- UProperty: `docs.unrealengine.com/5.0/en-US/unreal-engine-uproperties/`

### UE5 소스 코드 (주요 헤더)
- `Engine/Source/Runtime/Engine/Classes/GameFramework/GameModeBase.h`
- `Engine/Source/Runtime/Engine/Classes/GameFramework/GameStateBase.h`
- `Engine/Source/Runtime/Engine/Classes/GameFramework/PlayerController.h`
- `Engine/Source/Runtime/Engine/Classes/GameFramework/PlayerState.h`
- `Engine/Source/Runtime/Engine/Classes/GameFramework/Character.h`

### 검색 키워드
- "UE5 Gameplay Framework overview"
- "UE5 GameMode vs GameModeBase"
- "UE5 Actor Lifecycle diagram"
- "UE5 UPROPERTY specifiers list"
- "UE5 reflection system UHT"
- "Unreal Engine Possess flow multiplayer"
