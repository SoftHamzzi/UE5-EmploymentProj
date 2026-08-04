# Step 02 — Interaction (상호작용 인터페이스 + 서버 검증)

> 마스터 기획: `05_Loot_DOCS.md` (§4-4 동시 획득 경쟁, §4-5)
> 선행: `05_Loot_01_Spawner.md` — 바닥에 픽업이 있어야 한다

---

## 목표

**F키**로 픽업에 상호작용하고 **서버가 판정**한다. 이 단계에는 인벤토리가 없으므로 픽업은 파괴되고 로그만 남는다.

**이 컴포넌트는 이번 단계에서 가장 오래 쓰이는 자산이다.** 픽업·컨테이너(§7-1)·자판기(§7-2)·탈출 지점(로드맵 12)이 전부 같은 진입점을 쓴다.

**완료 조건**

- [ ] 조준선에 픽업을 담으면 HUD에 "줍기 — 붕대" 프롬프트가 뜬다
- [ ] F → 픽업이 파괴되고 서버 로그에 획득이 찍힌다
- [ ] **사거리 밖에서 보낸 요청을 서버가 거부**한다 (치트 시뮬레이션: RPC 직접 호출)
- [ ] PIE 2인이 동시에 F → **한 명만 성공**하고 나머지는 실패 사유를 받는다
- [ ] 로컬 컨트롤러가 아닌 캐릭터에서는 트레이스 틱이 아예 안 돈다
- [ ] **리슨서버 창(PIE 1번)에서도 프롬프트가 뜬다**
- [ ] **`UnPossessed` 후 틱이 꺼진다** — 함정 7b는 첫 스폰으로 검증되지 않는다. 아래를 본다

> **★ 함정 7b는 이 완료 조건들로 잡히지 않는다.** 첫 스폰은 리슨서버 호스트도 **통과한다**(02-2에서 엔진 순서로 증명). 깨지는 것은 **리스폰**이고, 이 프로젝트에는 아직 리스폰 경로가 없다(`RestartPlayer` 호출 0건). 즉 **지금은 어떻게 짜도 테스트가 통과한다.** 대응을 넣되 *"테스트가 통과했으니 함정이 없다"*로 읽지 않는다.

---

## 02-1. `IEPInteractable` — 처음부터 4함수

```cpp
UINTERFACE(MinimalAPI)
class UEPInteractable : public UInterface { GENERATED_BODY() };

class EMPLOYMENTPROJ_API IEPInteractable
{
    GENERATED_BODY()
public:
    virtual FText GetInteractText() const = 0;

    // 상호작용 가능 여부 + 불가 사유. 프롬프트 표시와 서버 판정 양쪽에서 쓴다
    virtual bool CanInteract(AEPCharacter* Interactor, FText& OutReason) const = 0;

    // 채널링 시간(초). 0이면 즉시
    virtual float GetInteractDuration() const { return 0.f; }

    // ★ 서버에서만 호출된다. 성공 여부를 돌려준다 — 이유는 아래
    virtual bool OnInteract(AEPCharacter* Interactor, FText& OutReason) = 0;
};
```

**나중에 인터페이스를 넓히면 이미 구현한 모든 클래스를 건드려야 한다.** 그래서 지금 확장 지점을 다 확보한다.

| 함수 | 픽업(이번) | 컨테이너(§7-1) | 자판기(§7-2) | 탈출(로드맵 12) |
|---|---|---|---|---|
| `GetInteractText` | "줍기 — 붕대" | "검색" | "1000원 투입" | "탈출" |
| `CanInteract` | `DropCooldown` / 인벤 여유 | 이미 검색됨 | 돈 부족 | 조건 미달 |
| `GetInteractDuration` | `0` | 검색 N초 | `5` | 대기 시간 |
| `OnInteract` | 인벤 삽입 후 파괴 | 내용물 UI 개방 | 돈 차감 + 배출 | 탈출 처리 |

### ★ `OnInteract`이 `void`면 안 되는 이유

02-3의 서버 검증 절차는 마지막 갈래로 **"부분 획득이면 `bClaimed`를 되돌리고 픽업을 남긴다"**를 갖고 있다. 즉 **`OnInteract`은 실패할 수 있다.** 그런데 `void`를 반환하면 호출자(컴포넌트)는 실패를 알 수 없고, `Client_OnInteractFailed`를 보낼 근거가 없다.

- 컨테이너: 다른 플레이어가 방금 열었다
- 자판기: 차감 직전에 돈이 빠졌다
- 픽업(Step 03): 가방에 자리가 없다

**여기서 `bool`로 바꾸는 비용은 0이고, 구현체가 넷으로 늘어난 뒤 바꾸는 비용은 전부 재작업이다.** 위 표가 이미 네 구현체를 예고하고 있다 — CLAUDE.md §2의 *"나중에 넣기 비싼 것(계약·반환 규약)은 지금 넣는다"*가 정확히 이 경우다.

> **`Instigator`라는 인자 이름은 쓰지 않는다.** `AActor::Instigator`(`APawn*`, `Actor.h`)가 이미 있어서 `AEPPickup` 멤버 함수 안에서 **멤버를 가린다**. 컴파일은 되지만 `Instigator->`가 어느 쪽인지 읽는 사람이 매번 확인해야 한다. `Interactor`로 쓴다.

> **`UINTERFACE`에서 `BlueprintType`을 뺐다.** 아래 "BlueprintNativeEvent로 만들지 않는 이유"와 정면으로 모순이었다. 어떤 함수도 `BlueprintCallable`이 아니므로 BP는 이 인터페이스로 아무것도 못 한다 — 변수 타입으로 선언만 가능한 죽은 플래그였다.

> **상호작용 전체가 GAS 어빌리티 `UEPGA_Interact` 하나로 간다.** 즉시(`GetInteractDuration() == 0`)든 채널링(`> 0`)이든 같은 어빌리티다. 컴포넌트는 **대상을 고르고 이벤트를 쏘는 것까지만** 하고 판정은 어빌리티가 한다 — 근거는 02-2 "왜 `Server_Interact`가 아니라 `GA_Interact`인가".
>
> 채널링이 붙을 때 `UEPGA_Skill_Base`의 `CastTime` + `State.Casting` 구조(`EPGA_Skill_Base.h:31`)를 그대로 쓴다. 진행도 게이지(`WBP_CastGauge`)·피격 중단·스킬 잠금이 전부 재사용된다. 이번 단계는 duration이 전부 0이라 그 갈래를 타지 않는다.

> **★ `FText`를 RPC로 보내는 비용을 알고 쓴다.** `FTextProperty`에는 `NetSerializeItem` 오버라이드가 없어서 기본 `SerializeItem`으로 떨어진다 — **네임스페이스 + 키 + 소스 문자열이 통째로** 나간다. 실패 회신은 실패 1회당 1번이라 허용 가능하다. **다만 이걸 모르고 "프롬프트 텍스트도 서버에서 보내자"로 번지면 그때는 비싸다** — 프롬프트는 0.1초마다 갱신된다. (비교: `FGameplayTag`는 패킹된 net index로 나간다 — `GameplayTagContainer.cpp:1286-1299`)

### `UINTERFACE`를 `BlueprintNativeEvent`로 만들지 않는 이유

픽업·컨테이너·자판기는 전부 C++ 액터이고, 상호작용 판정은 **서버 권한 로직**이다. BP에서 오버라이드할 수 있게 열면 판정을 BP로 옮기고 싶은 유혹이 생기고, 그러면 서버 검증이 BP 그래프로 흩어진다. 순수 C++ 인터페이스로 둔다.

---

## 02-2. `UEPInteractionComponent`

```cpp
UCLASS(meta = (BlueprintSpawnableComponent))
class EMPLOYMENTPROJ_API UEPInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEPInteractionComponent();

    void Input_Interact();      // 입력 바인딩에서 호출. 어빌리티 이벤트를 쏜다 — 아래

    // ★ AEPCharacter::NotifyControllerChanged() 에서 호출 — 아래
    void RefreshTickEnabled();

    // ★ 판정은 UEPGA_Interact가 하고 값은 여기 있다. 게터를 연다
    FORCEINLINE float GetInteractRange() const { return InteractRange; }
    FORCEINLINE float GetServerRangeTolerance() const { return ServerRangeTolerance; }

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*) override;

    UPROPERTY(EditDefaultsOnly, Category = "Interaction")
    float InteractRange = 250.f;

    // 서버 재검증 시 허용 오차 (지연 보정)
    UPROPERTY(EditDefaultsOnly, Category = "Interaction")
    float ServerRangeTolerance = 100.f;

private:
    void UpdateFocus();

    UPROPERTY()
    TObjectPtr<AActor> FocusedActor;
};
```

### 틱은 0.1초로 낮추고, 로컬이 아니면 끈다

```cpp
UEPInteractionComponent::UEPInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;         // 매 프레임 트레이스 금지
    PrimaryComponentTick.bStartWithTickEnabled = false;   // ★ 빙의 전에는 안 돈다

    SetIsReplicatedByDefault(true);                   // 관례 맞춤 — 아래
}

void UEPInteractionComponent::RefreshTickEnabled()
{
    const AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner());
    SetComponentTickEnabled(Owner && Owner->IsLocallyControlled());
}
```

**프롬프트 표시는 100ms 지연이 체감되지 않는다.** 반면 매 프레임 트레이스는 그대로 낭비다. 8인 매치에서 서버가 8개 캐릭터의 트레이스를 매 프레임 도는 건 더 낭비인데, 애초에 **탐지는 로컬 클라이언트만** 한다.

### ★ 이 판정을 `BeginPlay`에 두면 **리스폰에서** 조용히 깨진다

`IsLocallyControlled()`는 `Controller`를 보고, `Controller`는 `APawn::PossessedBy`의 `SetController()`(`Pawn.cpp:655`)에서만 채워진다. `AController::SetPawn`(`Controller.cpp:526-535`)은 `Controller->Pawn`만 세팅한다. 그래서 `BeginPlay`와 `PossessedBy`의 **순서**가 문제다.

**그런데 그 순서가 상황마다 다르다.** `SpawnActor`가 `BeginPlay`를 즉시 부르는 것은 **월드가 이미 begun play일 때뿐**이기 때문이다.

```cpp
// Actor.cpp:4482
bool bRunBeginPlay = !bDeferBeginPlayAndUpdateOverlaps && (BeginPlayCallDepth > 0 || World->HasBegunPlay());
```

| 경우 | 순서 | `BeginPlay`의 `IsLocallyControlled()` |
|---|---|---|
| **PIE 리슨서버 호스트, 첫 스폰** | `PossessedBy` → `BeginPlay` | ✅ **true — 통과한다** |
| 서버가 보는 원격 클라 폰 | `BeginPlay` → `PossessedBy` | false (어차피 로컬 아님. 무해) |
| 소유 클라의 복제 폰 | 보통 `OnRep_Controller` → `BeginPlay` | 보통 true. **보장 아님** |
| **리스폰 (호스트 포함)** | `BeginPlay` → `PossessedBy` | ❌ **false — 여기서 깨진다** |

**첫 스폰이 통과하는 이유**는 `AEPGameMode : AGameMode`(`EPGameMode.h:14`)의 매치 상태 기계에 있다.

```
GameInstance.cpp:537  SpawnPlayActor → PostLogin → HandleStartingNewPlayer
                        MatchState == EnteringMap → 폰이 안 생긴다
GameInstance.cpp:565  PlayWorld->BeginPlay() → GameMode->StartPlay()
  GameMode.cpp:157-160   HandleMatchIsWaitingToStart:  if (!ReadyToStartMatch()) NotifyBeginPlay();
                           NumPlayers==1, MinPlayersToStart==1 (EPGameMode.h:36,
                           EPGameMode.cpp:183-188) → ready == true
                           → ★ NotifyBeginPlay를 건너뛴다. World->HasBegunPlay()는 아직 false
  GameMode.cpp:141       StartMatch() → HandleMatchHasStarted()
  GameMode.cpp:208-215     RestartPlayer(호스트) → 폰 스폰(BeginPlay 지연) → Possess ★
  GameMode.cpp:221         NotifyBeginPlay() → 이제서야 폰의 BeginPlay. Controller는 이미 있다
```

엔진 주석이 이 순서를 그대로 말한다 — `// First fire BeginPlay, if we haven't already in waiting to start match`(`GameMode.cpp:220`). `World->SetBegunPlay(true)`는 `AWorldSettings::NotifyBeginPlay`(`WorldSettings.cpp:367`) 한 곳에서만 켜진다.

리스폰은 반대다. 그때는 `World->HasBegunPlay()`가 이미 true라 `SpawnDefaultPawnFor`(`GameModeBase.cpp:1310`) 안에서 `BeginPlay`가 즉시 돌고, `Possess`는 `:1379`에서 나중에 온다.

**즉 "첫 테스트에서 바로 걸린다"가 아니라, 첫 테스트는 통과하고 리스폰에서 깨진다.** 이게 더 나쁘다 — 완료 조건이 통과해서 **함정이 없다고 판단하고 대응을 지우게 된다.**

> **현재 이 프로젝트에는 리스폰 경로가 없다**(`RestartPlayer` 호출 0건). 그래서 지금은 어느 방식으로 짜도 안 걸린다. 대응을 넣는 것은 옳지만, **"지금은 안 걸린다"를 알고 넣어야** 나중에 테스트 결과와 문서가 안 싸운다.

> 소유 클라 쪽 순서도 **보장이 아니다.** `OnRep_Controller`(RepNotify, `DataChannel.cpp:3485`)가 `PostNetInit`→`BeginPlay`(`:3495-3503`, `Actor.cpp:4638-4654`)보다 먼저지만, `Controller` 참조가 unmapped면 뒤로 밀린다. **순서에 기대는 설계 자체가 틀렸다.**

### ★ 훅은 하나면 된다 — `NotifyControllerChanged()`

엔진에 서버 빙의 / 소유 클라 복제 / **언빙의** 셋을 묶는 가상 함수가 있다.

```cpp
// Pawn.h:382
/** Call to notify about a change in controller, on both the server and owning client. ... */
ENGINE_API virtual void NotifyControllerChanged();
```

호출 지점이 셋이다 — `PossessedBy`(`Pawn.cpp:688`) / `OnRep_Controller`(`:622`) / `UnPossessed`(`:714`). 클라 쪽 게이트 기본값도 켜져 있다(`Controller.cpp:34` `bAlwaysNotifyClientOnControllerChange = true`).

```cpp
// EPCharacter.h — PossessedBy / OnRep_Controller 옆 (EPCharacter.h:80-81)
virtual void NotifyControllerChanged() override;

// EPCharacter.cpp
void AEPCharacter::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();
    if (InteractionComponent) { InteractionComponent->RefreshTickEnabled(); }
}
```

| | `PossessedBy` + `OnRep_Controller` 두 훅 | `NotifyControllerChanged` 하나 |
|---|---|---|
| 서버 빙의 | ✅ | ✅ |
| 소유 클라 | ✅ | ✅ |
| **언빙의** | ❌ 틱이 켜진 채 남는다 | ✅ |
| 오버라이드 수 | 2 | 1 |
| 기존 오버라이드 건드림 | 두 함수에 한 줄씩 | 안 건드림 |

> `UpdateFocus()` 선두의 `IsLocallyControlled()` 확인은 **그대로 둔다.** 저건 이 문제의 해결책이 아니라(틱이 꺼져 있으면 불리지도 않는다) 언빙의 직후 한 틱의 안전장치다. **두 훅 방식에서는 그게 "한 틱"이 아니라 영구적으로 도는 빈 호출이 된다** — 단일 훅으로 가는 이유가 이것이다.

### ★ 왜 `Server_Interact`가 아니라 `GA_Interact`인가

이 프로젝트에서 **모든 게임플레이 입력은 어빌리티 태그로 간다**(`EPCharacter.cpp:388-435`). 사격·재장전·대시·힐·실드 전부, 예외가 없다.

**그건 "서버 RPC를 안 쓴다"는 뜻이 아니다.** `TryActivateAbilitiesByTag` 한 줄이 `ServerTryActivateAbility`(`AbilitySystemComponent.h:1723`, `..._Abilities.cpp:1899-1928`)를 부른다. 서버 RPC는 이미 쓰고 있다.

> **★ 규칙 문장이 8차에서 정정됐다 (2026-08-04).** 7차는 여기에 *"게임플레이 입력의 진입점은 어빌리티 하나다"* 라고 적었는데, **그 일반화가 너무 넓었다.** Step 03의 드랍(`int32 EntryId`)에 적용하면 `FGameplayEventData`에 정수 자리가 없어 인코딩 규약을 사게 된다.
>
> **정정된 문장:**
> ① **월드 상호작용**, 그리고 **시간·비용·애님이 붙는 행동** → **어빌리티**
> ② **서버가 이미 소유한 상태에 대한 변경 요청** → **소유 컴포넌트의 서버 RPC**
>
> **상호작용은 ①이다** — 클라가 월드를 조회해 대상을 고르고, 서버가 거리·유효성을 세계 상태로 재검증해야 하며, 대상이 **액터**라 `FGameplayEventData::Target`이 공짜다. 이 절의 판정은 그대로 유효하다. 근거와 반대편 사례는 `05_Loot_03_Inventory.md` 03-5.

`Server_Interact`를 직접 만들면 아래를 손으로 다시 만든다.

| GAS가 이미 주는 것 | 직접 RPC에서 다시 만들 것 |
|---|---|
| `ActivationBlockedTags`에 `State.Dead` — **이미 세 곳에 있다**(`EPGA_Item_PrimaryUse.cpp:24`, `EPGA_Item_Reload.cpp:24`, `EPGA_Skill_Base.cpp:17`) | 죽은 상태 확인 |
| `ActivationBlockedTags` / `ActivationRequiredTags` | 사격 중·재장전 중 상호작용 금지 |
| GE 쿨다운 | Step 03 `DropCooldown`을 손으로 만든 타임스탬프 맵으로 |
| `CastTime` + `State.Casting` + `WBP_CastGauge`(`EPGA_Skill_Base.h:31`) | 채널링 — 02-1이 이미 GAS에 위임한다고 적었다 |
| `NetSecurityPolicy` 검사(`..._Abilities.cpp:2052-2058`) | 조작 클라 차단 |

**그리고 "즉시는 RPC / 채널링은 GAS"는 절충이 아니라 모순이다.** 경계가 `GetInteractDuration()`이라는 **데이터 값**에 달려 있어서, 자판기의 duration을 5→0으로 바꾸면 그 대상의 네트워크 경로가 통째로 바뀐다. §7-1 컨테이너가 들어오면 두 경로가 **동시에** 살아 있게 되고, 그때 "실패 회신은 어느 쪽으로?", "`bClaimed` 선점은 예측 실행에서도 도는가?"를 두 번 답해야 한다.

**순증 비용은 어빌리티 클래스 하나다.** 그 자리에는 이미 형제가 셋 있다(`EPGA_Item_PrimaryUse`, `EPGA_Item_Reload`, `EPGA_Skill_Base`).

### 대상은 어떻게 서버에 전달하는가 — `FGameplayEventData::Target`

`FGameplayAbilityTargetData`를 만들 필요도, 어빌리티가 서버에서 다시 트레이스할 필요도 **없다.** `FGameplayEventData`에 `Target`이 있고, 이 구조체가 **통째로 서버 RPC의 파라미터다.**

```cpp
// GameplayAbilityTypes.h:256
TObjectPtr<const AActor> Target;

// AbilitySystemComponent.h:1726 — Payload 전체가 서버로 간다
void ServerTryActivateAbilityWithEventData(
    FGameplayAbilitySpecHandle AbilityToActivate, bool InputPressed,
    FPredictionKey PredictionKey, FGameplayEventData TriggerEventData);
```

```cpp
// UEPInteractionComponent::Input_Interact() — 클라 로컬
void UEPInteractionComponent::Input_Interact()
{
    if (!FocusedActor) return;

    AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner());
    UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
    if (!ASC) return;

    FGameplayEventData Payload;
    Payload.EventTag   = EmpGameplayTags::TAG_Ability_Interact;
    Payload.Instigator = Owner;
    Payload.Target     = FocusedActor;   // ★ 대상 전달의 전부

    ASC->HandleGameplayEvent(EmpGameplayTags::TAG_Ability_Interact, &Payload);
}
```

`UEPGA_Interact`는 `AbilityTriggers`에 `GameplayEvent` / `TAG_Ability_Interact`를 하나 등록한다. `NetExecutionPolicy = LocalPredicted` — **클라가 서버 RPC를 쏘려면 이 정책이어야 한다.**

**대상은 클라가 정하고 서버는 그 대상을 받는다.** 이 문서가 세운 *"클라는 요청, 서버가 결정"* 원칙이 그대로다. 서버가 재트레이스해서 대상이 갈리는 갈래는 아예 생기지 않는다.

> **`Payload.Target`은 복제 액터여야 한다.** 오브젝트 참조로 직렬화되므로 서버가 발급한 NetGUID가 필요하다. `AEPPickup`은 복제 액터라 문제없고, §7-1 컨테이너·§7-2 자판기도 그래야 한다 — 어차피 그래야 한다.
>
> 참고로 Lyra는 이 길을 쓰지 않고 **서버도 자기 라인 트레이스를 돌린다**(`LyraGameplayAbility_Interact.cpp:78-122`, 스캐너가 `LocalPredicted`라 서버에도 인스턴스가 있다). 이유는 Lyra의 F키가 어빌리티 *활성화*가 아니라 이미 떠 있는 어빌리티의 *트리거*이기 때문이다. 우리는 F키가 곧 활성화라 파라미터가 자연스럽게 실린다.

### 예측은 하지 않는다

선점 경쟁(`bClaimed`)이 있어서 **예측이 틀리면 픽업이 사라졌다 다시 나타난다.** 그리고 Step 02에는 예측으로 가릴 지연이 없다(애님도 이펙트도 없다).

**방어는 `ActivateAbility` 첫 줄 하나에 둔다.**

```cpp
void UEPGA_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 클라 예측 실행: 아무것도 하지 않는다. 판정은 전부 서버다.
    if (!ActorInfo->IsNetAuthority())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ false, /*bWasCancelled*/ false);
        return;
    }
    // ... 02-3의 5단계
}
```

**`AEPPickup::OnInteract` 안에 `HasAuthority()` 가드를 또 넣지 않는다.** 그러면 방어가 두 곳으로 흩어지고, `IEPInteractable` 구현체가 넷으로 늘면 넷 다 넣어야 한다. **인터페이스 주석의 `// ★ 서버에서만 호출된다`가 계약이고, 그 계약을 지키는 곳은 호출자 하나다.**

### 사거리 값은 컴포넌트에 남는다

`InteractRange`·`ServerRangeTolerance`는 `UEPInteractionComponent`에 두고 어빌리티가 게터로 읽는다. 어빌리티가 아바타 액터를 이미 들고 있어서 한 줄이다.

```cpp
const UEPInteractionComponent* IC = Interactor->GetInteractionComponent();
const float MaxDistSq = FMath::Square(IC->GetInteractRange() + IC->GetServerRangeTolerance());
```

**어빌리티 CDO에 값을 복제해 두지 않는다.** CLAUDE.md §2의 *"한 값을 두 경로가 봐야 하면 둘 다 볼 수 있는 곳에 둔다"* — 두 곳에 두면 반드시 갈린다.

### `Client_OnInteractFailed`는 그대로 남는다

**GAS는 실패 *사유*를 주지 않는다.** `ClientActivateAbilityFailed(Handle, PredictionKey)`(`AbilitySystemComponent.h:1747`)에는 사유 파라미터가 없고, `InternalTryActivateAbilityFailureTags`는 서버 로컬 변수다.

그래서 **`AEPPlayerController`에 둔다.** `Client_OnKill`·`Client_PlayHitConfirmSound`(`EPPlayerController.h:40-45`)와 같은 줄이고, 실패 문구를 띄울 `HUDWidget`이 거기 `private`이라 **다른 선택지가 없다.**

**원칙: 요청은 값이 있는 곳에, 회신은 표시가 있는 곳에.**

### `SetIsReplicatedByDefault`

`Server_Interact`가 없어졌으므로 **RPC 때문에 필요하지는 않다.** `UEPCombatComponent`(`EPCombatComponent.cpp:34`)와 관례를 맞추려면 `true`, 아니어도 무방하다 — **결정할 필요가 없어졌다.**

복제 프로퍼티가 0개라 **반복** 대역폭 비용은 0이다. 엄밀히는 커넥션당 `FObjectReplicator` 1개 + 컨텐츠 블록 헤더 1회가 든다(`NetDriver.cpp:3245` → `DataChannel.cpp:4552-4560`).

### 탐지

```cpp
void UEPInteractionComponent::UpdateFocus()
{
    AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner());
    if (!Owner || !Owner->IsLocallyControlled()) return;

    FVector Start, Dir;
    /* 카메라 위치/전방 획득 — FirstPersonCamera */

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);

    AActor* NewFocus = nullptr;
    if (GetWorld()->LineTraceSingleByChannel(
            Hit, Start, Start + Dir * InteractRange, EP_TraceChannel_Interact, Params))
    {
        if (Hit.GetActor() && Hit.GetActor()->Implements<UEPInteractable>())
            NewFocus = Hit.GetActor();
    }

    if (NewFocus == FocusedActor) return;
    FocusedActor = NewFocus;
    /* HUD 프롬프트 갱신 (02-4) */
}
```

**전용 트레이스 채널 `EP_TraceChannel_Interact`를 만든다.** 기존 `EP_TraceChannel_Weapon`(`EPTypes.h`)과 같은 패턴이다. `ECC_Visibility`를 재사용하면 벽·소품이 전부 걸려서 픽업 앞의 잡동사니가 상호작용을 막는다.

### ★ 채널을 만드는 것만으로는 안 풀린다 — `DefaultResponse`가 핵심이다

새 채널을 `DefaultResponse = ECR_Block`으로 만들면 그 값이 **기본 응답 컨테이너에 들어간다.**

```cpp
// CollisionProfile.cpp:470
FCollisionResponseContainer::DefaultResponseContainer.SetResponse(
    (ECollisionChannel)EnumIndex, CustomChannel.DefaultResponse);
```

그러면 반응을 따로 지정하지 않은 **모든** 프리미티브(벽·바닥·소품)가 새 채널도 막는다 — **`ECC_Visibility` 재사용과 결과가 똑같다.** 함정을 피했다고 생각하며 그대로 밟는다.

**해결책은 "새 채널"이 아니라 "기본 응답 `Ignore` + 상호작용 대상만 `Block`"이다.** 이 프로젝트에 이미 관례가 있다.

```ini
; DefaultEngine.ini:306-307 — 이미 있는 두 채널
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="WeaponTrace")
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel2,DefaultResponse=ECR_Ignore,bTraceType=False,bStaticObject=False,Name="Projectile")

; 새로 추가 — DefaultResponse와 bTraceType을 반드시 이렇게
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel3,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="Interact")
```

```cpp
// EPTypes.h:96 옆에 한 줄 — 기존 WeaponTrace와 같은 패턴
static constexpr ECollisionChannel EP_TraceChannel_Interact = ECC_GameTraceChannel3;
```

> **★ `GameTraceChannel2`가 아니다.** 그 번호는 **`Projectile`이 이미 쓰고 있다**(`DefaultEngine.ini:307`). 이 문서는 원래 `Channel2`라고 적혀 있었다 — 그대로 넣으면 같은 채널에 이름이 둘 붙는다. `EPTypes.h`에 `Projectile` 상수가 없어서(`:96`에 `Weapon` 하나뿐) 소스만 보면 비어 보이는 게 함정이다. **채널 번호는 `EPTypes.h`가 아니라 `DefaultEngine.ini`에서 센다.**

> **중복시키면 나중 것이 조용히 버려진다.** 에디터가 거부하지도, 나중 줄이 이기지도, 둘 다 남지도 않는다. `CollisionProfile.cpp:401-408`이 `TraceTypeMapping`/`ObjectTypeMapping` 중복을 보고 `RemoveAt` + `continue`한다 — `:470`의 `SetResponse`에 **도달하지도 못한다.** 경고는 뜨지만(`LogCollisionProfile`, 기본 verbosity Warning) 엔진 초기화 로그에 묻힌다.
>
> 그리고 버려진 쪽의 `bTraceType`이 안 먹으므로, `Projectile`이 이기면 `Channel2`는 **오브젝트 타입**으로 남는다. 그래도 `LineTraceSingleByChannel(..., ECC_GameTraceChannel2, ...)`는 컴파일도 되고 실행도 된다 — 트레이스는 채널 번호만 보지 메타데이터를 안 본다. **증상은 "상호작용 트레이스가 `Projectile`이 막는 것들을 같이 맞힌다"로 나온다.** 채널 번호를 의심하기 전에 픽업 콜리전부터 파게 된다.
>
> 어느 쪽이 이기는지도 `TArray::Sort`(`:381-385`)가 안정 정렬이 아니라 **보장이 없다.** 엔진 버전에 따라 바뀔 수 있는 종류의 버그다.

> **Lyra가 우리와 같은 문안을 쓴다.** `LyraStarterGame/Config/DefaultEngine.ini:216`
> `+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="Lyra_TraceChannel_Interaction")`
> — 채널 번호만 다르고 `DefaultResponse`·`bTraceType`·`bStaticObject`가 전부 같다. **함정 5b(`ECR_Block`으로 만들지 마라)의 독립 증거다** — Lyra의 커스텀 채널 5개(`:216-220`)가 전부 `ECR_Ignore`다.

**Step 01의 픽업이 전 채널 `Ignore`로 만들어져 있으므로**(`05_Loot_01_Spawner.md` 01-4) 여기서 정확히 한 줄만 열면 맞물린다.

```cpp
    Mesh->SetCollisionResponseToChannel(EP_TraceChannel_Interact, ECR_Block);
```

> `.ini` 설정은 코드보다 잊기 쉽다. Step 00의 `Is Editor Only` 사건이 정확히 그 부류였다.

---

## 02-3. ★ 서버 검증 — 5단계 절차

```
F키 → UEPInteractionComponent::Input_Interact()                          [클라 로컬]
    → ASC->HandleGameplayEvent(TAG_Ability_Interact, &Payload{Target=FocusedActor})
        → (LocalPredicted) ServerTryActivateAbilityWithEventData          [엔진 제공 서버 RPC]
            → UEPGA_Interact::ActivateAbility                             [서버]

                 0. IsNetAuthority() 아니면 즉시 EndAbility (02-2 "예측은 하지 않는다")
                 1. TriggerEventData->Target이 유효한가 (IsValid && !IsActorBeingDestroyed)
                 2. Target이 IEPInteractable을 구현하는가   ← 아무 액터나 보낼 수 있다
                 3. ★ 거리 재검증
                      DistSq(Character, Target) <= (InteractRange + ServerRangeTolerance)^2
                 4. ★ IEPInteractable::CanInteract(Interactor, OutReason) == true 인가
                      bClaimed, DropCooldown, 컨테이너 "이미 검색됨", 자판기 돈 부족이 전부 여기서
                 5. IEPInteractable::OnInteract(Interactor, OutReason) 이 true를 돌려줬는가

                 하나라도 실패 → PC->Client_OnInteractFailed(OutReason). 조용히 return 하지 않는다
                 EndAbility
```

**`bClaimed`의 마킹·해제는 이 절차에 없다.** 대상 안에서 일어난다 — 02-5 참조.

> **어빌리티로 옮겨도 절차는 그대로다. 사는 곳만 바뀐다.** 오히려 세지는데, `ActivationBlockedTags`에 `State.Dead`를 한 줄 붙이면 *"죽었을 때 상호작용 금지"*가 1단계보다 **앞에서** 걸린다 — 손으로 쓰는 `IsDead()` 확인이 없어진다.

### ★ 원래 8단계였다 — 왜 줄었는가

이 문서는 원래 `bClaimed == false 확인`(5)과 `bClaimed = true 마킹`(6)을 컴포넌트 절차에 따로 세워 두었다. **컴포넌트는 그 두 줄을 쓸 수 없다.**

```cpp
// EPPickup.h:42-43
private:
    bool bClaimed = false;
```

`bClaimed`는 `AEPPickup`의 `private`이고, 애초에 `IEPInteractable`에는 그 개념이 없다 — 자판기에는 `bClaimed`가 아예 없다. **컴포넌트가 알아야 하는 건 "선점됐는가"가 아니라 "된다/안 된다"뿐이다.**

- 확인은 이미 `CanInteract()`가 한다(02-5). 4단계에 흡수 — 5단계는 **중복이었다**
- 마킹은 `OnInteract()`의 첫 줄에서 한다. 대상만이 자기 선점 규칙을 안다
- 되돌리기(원래 8단계)도 같은 함수 안이다. **되돌릴 값을 세운 함수가 되돌린다**

> **같은 프레임 2요청이 4단계와 마킹 사이로 끼어들 수 없다.** 수신 RPC는 `UNetDriver::TickDispatch` → `UActorChannel::ProcessBunch` → `ProcessEvent` 경로로 **게임 스레드에서 차례로** 실행되고, `CanInteract` → `OnInteract` 사이에 양보 지점이 없다. 나눠 놨을 때 안전했던 이유도 원래 이것이었지 단계를 쪼개서가 아니다. **어빌리티로 옮겨도 같다** — 활성화도 같은 RPC 경로를 탄다.

> **★ 이 보장은 `OnInteract`이 동기적으로 끝날 때만이다.** §7-1 컨테이너가 *"내용물 UI를 연다"*로 구현되면 `OnInteract`은 즉시 반환하고 실제 트랜잭션은 여러 프레임 뒤에 끝난다. **그때는 `bClaimed`가 아니라 "누가 열어 두었는가"라는 다른 상태가 필요하다.** 지금 만들지는 않는다 — 여기 적어 두는 이유는 같은 함정을 두 번 파지 않기 위해서다.

### 3·4단계가 왜 ★인가

**이 둘을 빠뜨리면 서버 검증이 사실상 없어진다.** §4-5가 "서버가 거리와 대상 유효성을 재검증한다", "`CanInteract()`는 서버가 다시 호출해 판정한다"고 선언해도, 이 절차에 호출이 없으면 **클라이언트가 프롬프트를 안 그릴 뿐 RPC는 그대로 통과한다.**

특히 Step 03의 `DropCooldown`(버린 직후 0.5초 재획득 금지)이 `CanInteract()`로 구현되므로, 4단계가 없으면 쿨다운이 서버에서 강제되지 않는다.

**클라이언트의 트레이스 결과는 표시용일 뿐이다.** 클라도 `CanInteract()`를 호출해 프롬프트를 회색 처리하지만, 판정은 서버가 다시 한다. 사격 경로에서 이미 확립한 원칙(클라는 "요청", 서버가 "결정")과 같다.

### `bClaimed` — 동시 획득 경쟁

두 플레이어가 같은 픽업에 동시에 F를 누르는 상황은 **반드시 발생한다.** `bClaimed`를 인벤토리 삽입보다 **먼저** 세워야 두 요청이 같은 프레임에 들어와도 하나만 통과한다.

- `bClaimed`는 **복제하지 않는다.** 서버 내부 상태이고, 결과는 액터 파괴로 클라에 전달된다
- **Step 03에서 "가방에 자리가 없음" 실패가 생기면 `bClaimed`를 되돌려야 한다.** 안 되돌리면 픽업이 살아남은 채 `bClaimed`가 true로 굳어 **아무도 그 아이템을 다시 못 줍는다.** 스택이 없으므로 갈래는 "성공→파괴 / 실패→해제" 둘뿐이다

> **Step 02 단독으로는 이 필드가 없어도 동작한다.** `Destroy()`가 즉시 pending kill로 만들어 두 번째 요청이 1단계에서 걸리기 때문이다. 그래서 **`bClaimed`의 진짜 값어치는 Step 03에서 나온다** — 삽입이 실패해 픽업이 살아남는 갈래가 그때 생긴다. 지금 안 세우면 그때 세울 자리를 찾느라 `OnInteract`을 다시 뜯는다.

> 서버에서 `bClaimed`가 켜졌는지는 `EP.Loot.List`의 마지막 열로 확인한다(`05_Loot_01_Spawner.md` 01-5). Step 01에서 `AEPPickup::IsClaimed()`를 공개하기로 한 이유가 이것이다.

### 실패를 조용히 삼키지 않는다

```cpp
// AEPPlayerController — Client_OnKill 옆 (EPPlayerController.h:40-45)
UFUNCTION(Client, Reliable)
void Client_OnInteractFailed(const FText& Reason);
```

늦은 요청·사거리 초과·조건 미달을 전부 회신한다. **아무 반응이 없으면 플레이어는 입력이 씹혔다고 느낀다.** 서버 로그만 남기고 끝내면 QA에서 "가끔 E가 안 먹혀요"로 올라온다.

---

## 02-4. HUD 프롬프트

`UEPHUDWidget`에 최소한만 추가한다.

```cpp
// UEPHUDWidget
UPROPERTY(meta = (BindWidgetOptional))
TObjectPtr<UTextBlock> InteractPrompt;

void SetInteractPrompt(const FText& Text, bool bEnabled);   // bEnabled false면 회색
```

```cpp
// AEPPlayerController — public. HUDWidget이 private이므로 여기를 통해야 한다
void SetInteractPrompt(const FText& Text, bool bEnabled);
```

- `BindWidgetOptional`로 둔다 — WBP에 아직 없어도 컴파일·실행이 깨지지 않는다
- 컴포넌트가 HUD를 직접 참조하지 않고 `AEPPlayerController`를 거친다. HUD 수명(리스폰 시 재생성)에 컴포넌트가 묶이지 않게 하기 위함. **`HUDWidget`이 실제로 `private`이므로**(`EPPlayerController.h`) 이건 취향이 아니라 유일한 경로다
- 포커스가 풀리면(`NewFocus == nullptr`) **빈 텍스트로 반드시 다시 부른다.** 안 부르면 프롬프트가 화면에 남는다
- `CanInteract()`가 false면 사유와 함께 회색으로 표시한다. **눌러보고 나서 실패하는 것보다 낫다**

---

## 02-5. `AEPPickup`의 인터페이스 구현 (이번 단계 버전)

```cpp
FText AEPPickup::GetInteractText() const
{
    // Step 01에서 만든 정적 접근자 — 01-5 "GetItemSubsystem은 여기서 못 부른다"
    const UEPItemDefinitionSubsystem* Defs = UEPItemDefinitionSubsystem::Get(this);
    const FEPItemData* Row = Defs ? Defs->FindData(ItemId) : nullptr;

    // 스택이 없으므로 개수 표기가 없다 — 픽업 하나 = 아이템 하나
    return FText::Format(NSLOCTEXT("EP", "PickupFmt", "줍기 — {0}"),
        Row ? Row->DisplayName : FText::FromName(ItemId));
}

bool AEPPickup::CanInteract(AEPCharacter* Interactor, FText& OutReason) const
{
    if (bClaimed) { OutReason = /* "이미 획득됨" */; return false; }
    return true;                    // 인벤토리 여유 확인은 Step 03
}

bool AEPPickup::OnInteract(AEPCharacter* Interactor, FText& OutReason)
{
    bClaimed = true;                // ★ 무엇을 하기도 전에 — 02-3

    // ★ Step 03에서 AddItem() 호출로 대체되는 유일한 지점
    //    실패하면 bClaimed = false; OutReason 채우고 return false;
    UE_LOG(LogTemp, Log, TEXT("[Pickup] %s 획득 (인벤토리 미구현)"), *ItemId.ToString());
    Destroy();
    return true;
}
```

> **`GetInteractText()`는 클라이언트에서 불린다**(프롬프트). `Row`가 null인 경우 — 데이터 오류 — 에 빈 문자열을 돌려주면 "F가 안 먹힌다"로 오해되므로 `ItemId`를 그대로 보여준다. **디버그 표시가 침묵보다 낫다.**

> `FindData`는 이미 `const`이므로(`EPItemDefinitionSubsystem.h:23`) `const` 포인터로 받는 데 문제가 없다. `Get()`이 비-const를 돌려주지만 대입은 성립한다.

> **★ Step 02와 03의 경계.** Step 02 시점에는 인벤토리가 없다. `CanInteract()`의 "인벤 여유 확인"과 `OnInteract()`의 "인벤 삽입"은 Step 03에서 채운다. §3이 내세운 "임시 코드가 안 생긴다"를 지키려면, **위 `UE_LOG` + `Destroy()` 한 덩어리가 Step 03에서 대체되는 유일한 지점**이어야 한다. 다른 곳에 임시 처리를 흩뿌리면 안 된다.

> 여유 확인 없이 파괴하므로 Step 02 단독으로는 부분 획득을 검증할 수 없다. 그건 Step 03의 완료 조건이다.

---

## 02-6. 입력 배선

기존 스킬 입력과 같은 패턴이다.

- `IA_Interact` 생성 → `Content/Characters/InputActions/`. **IMC에 `F` 키로 바인딩** (E 아님)
- `AEPPlayerController`에 `InteractAction` UPROPERTY + FORCEINLINE 게터 (Dash/Heal/Shield와 동일)
- `AEPCharacter::SetupPlayerInputComponent`에서 null 가드 후 `Triggered` 바인딩 → `InteractionComponent->Input_Interact()`
- `AEPCharacter` 생성자에 `InteractionComponent = CreateDefaultSubobject<UEPInteractionComponent>(...)` 추가 (`CombatComponent`/`RewindComponent` 옆) + `GetInteractionComponent()` 게터 (어빌리티가 사거리 값을 읽는다)
- **`AEPCharacter::NotifyControllerChanged()` 오버라이드에서 `RefreshTickEnabled()` 한 줄** (`EPCharacter.h:80-81`의 `PossessedBy`/`OnRep_Controller` 옆에 새로 추가). **이 한 줄이 함정 7b의 전부다** — 기존 두 오버라이드는 건드리지 않는다
- `EPTypes.h:96` 옆에 `EP_TraceChannel_Interact` 상수 + `DefaultEngine.ini`에 `GameTraceChannel3` 한 줄

**GAS 쪽 (§02-2 판정)**

- `EPNativeGameplayTags.h/.cpp`에 `TAG_Ability_Interact` 선언·정의 (`TAG_Ability_Item_Reload` 아래)
- `Public/GAS/EPGA_Interact.h` + `Private/GAS/EPGA_Interact.cpp` — 생성자에서 `NetExecutionPolicy = LocalPredicted`, `InstancingPolicy`는 형제들과 맞추고, `AbilityTriggers`에 `GameplayEvent` / `TAG_Ability_Interact` 1개
- `ActivationBlockedTags.AddTag(TAG_State_Dead)` — 형제 셋과 같은 줄(`EPGA_Item_PrimaryUse.cpp:24` 등)
- **`DefaultAbilities` 배열(`EPCharacter.h:61`, `EPCharacter.cpp:135-137`)에 `GA_Interact` 한 항목** — BP에서 추가. 동적 부여는 하지 않는다(어빌리티가 하나뿐이라 부여할 게 없다)

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | 서버에서 거리 재검증 누락 | 맵 반대편 아이템을 RPC로 획득 가능 | 3단계 |
| 2 | 서버에서 `CanInteract()` 미호출 | Step 03의 `DropCooldown`이 무력화 | 4단계 |
| 3 | `bClaimed`를 삽입 후에 세움 | 같은 프레임 2요청이 둘 다 성공 → 아이템 복사 | `OnInteract()` 첫 줄 (02-5) |
| 4 | 실패 시 조용히 return | "가끔 E가 안 먹혀요" QA 리포트 | `Client_OnInteractFailed` |
| 5 | `ECC_Visibility` 재사용 | 픽업 앞 잡동사니가 상호작용을 막음 | 전용 채널 `EP_TraceChannel_Interact` |
| 5b | 전용 채널을 **`DefaultResponse=ECR_Block`** 으로 생성 | **#5와 증상이 똑같다.** 채널을 만들어 함정을 피했다고 믿으며 그대로 밟는다 | `DefaultResponse=ECR_Ignore` + 픽업만 `SetCollisionResponseToChannel(..., ECR_Block)` (02-2) |
| 5c | Step 01 픽업의 전 채널 `Ignore`를 "정리" | 이 채널도 같이 닫혀 트레이스가 아무것도 못 맞힌다 | 그 줄에 의존이 걸려 있다 — `05_Loot_01_Spawner.md` 01-4 |
| 5d | 새 채널을 **`GameTraceChannel2`** 로 추가 | `Projectile`과 번호 충돌 (`DefaultEngine.ini:307`). `EPTypes.h`에는 `Weapon`만 있어서 소스만 보면 비어 보인다 | `GameTraceChannel3` (02-2) |
| 6 | 매 프레임 트레이스 | 불필요한 비용 | `TickInterval = 0.1f` |
| 7 | 시뮬프록시에서도 틱 | 8인이면 8배 낭비 | 로컬 아니면 `SetComponentTickEnabled(false)` |
| **7b** | **틱 on/off 판정을 `BeginPlay`에 둠** | **리스폰 후 프롬프트가 영영 안 뜬다.** 첫 스폰(PIE 호스트)은 `Possess`가 먼저라 **통과해서**, 함정이 없다고 판단하고 대응을 지우게 된다 | `NotifyControllerChanged()`에서 `RefreshTickEnabled()` (02-2) |
| **7c** | 그 대응을 `PossessedBy` + `OnRep_Controller` **둘로 나눔** | `UnPossessed`가 빠져 언빙의 후에도 틱이 영구히 돈다. `UpdateFocus()` 가드가 오동작은 막지만 빈 호출은 계속된다 | 훅 하나 — `NotifyControllerChanged()` (`Pawn.cpp:688`/`:622`/`:714`) |
| 8 | `Implements<>` 확인 누락 | 조작된 클라가 임의 액터를 보내면 크래시 | 2단계 |
| 9 | 임시 획득 처리를 여러 곳에 분산 | Step 03에서 지울 곳을 놓침 | `OnInteract()` 한 곳에만 |
| **10** | **`OnInteract()`을 `void`로 선언** | Step 03에서 실패 회신 경로가 없어 인터페이스를 다시 뜯는다. 그때는 구현체가 넷 | 처음부터 `bool` + `OutReason` (02-1) |
| 11 | 포커스 해제 시 프롬프트를 안 지움 | 다른 곳을 봐도 "줍기 — 붕대"가 남는다 | `NewFocus == nullptr`에도 갱신 (02-4) |
| **12** | **`Server_Interact` 직접 RPC로 감** | 대상이 **액터**라 `FGameplayEventData::Target`이 공짜인데 그걸 버린다. 죽음 확인·쿨다운·채널링을 손으로 다시 만들고, 채널링이 붙는 순간 F키가 두 배관을 탄다 | `UEPGA_Interact` + `FGameplayEventData::Target` (02-2) |
| 13 | `OnInteract` 안에 `HasAuthority()` 가드를 또 넣음 | 방어가 흩어져 구현체 넷에 전부 넣어야 한다 | `ActivateAbility` 첫 줄 하나 (02-2) |

---

## 이 단계에서 하지 않는 것

- 인벤토리 삽입 / 부분 획득 / `bClaimed` 되돌리기 → **Step 03**
- 채널링(`GetInteractDuration() > 0`) → 컨테이너·자판기 시점. 어빌리티는 이미 있으므로 `CastTime` 갈래만 채우면 된다
- 상호작용 대상 하이라이트(아웃라인) → 필요해지면 그때
- **어빌리티 코스트·쿨다운(GE)** → Step 03 `DropCooldown`에서 값어치가 난다. Step 02엔 붙일 것이 없다
- **Lyra식 `FInteractionOption` / `IInteractionInstigator` / 동적 어빌리티 부여** → **만들지 않는다.** Lyra의 전제는 *"대상마다 실행할 어빌리티가 다르다"*인데 우리는 픽업·컨테이너·자판기·탈출이 전부 `GA_Interact` 하나다. 옵션 구조체는 필드가 0개가 되고, 중재자는 선택지가 하나라 호출되지 않으며, 부여 태스크는 빈 루프가 된다
- **스피어 스윕 조준 관용** → 라인 트레이스를 유지한다. Lyra의 포커스 탐지도 라인이고(`AbilityTask_WaitForInteractableTargets.cpp:19-33`), 스윕은 시작점이 지오메트리 안이면 결과가 정의되지 않는다(`bStartPenetrating`) — 벽에 붙은 상자에서 바로 걸린다. 나중에 관용이 필요하면 스윕이 아니라 **Lyra식 2단**(넓은 오버랩으로 후보 수집 → 시야 중심에 가장 가까운 것 선택)으로 간다
