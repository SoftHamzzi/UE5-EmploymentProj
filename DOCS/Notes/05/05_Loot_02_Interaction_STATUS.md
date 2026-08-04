# 05_Loot_02_Interaction — 구현 상태

**전체 상태: 구현 완료 / 정상 동작 확인 / 완료 조건 3건은 성격상 미검증**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷. 문서(`05_Loot_02_Interaction.md`)의 예정 코드와 혼동 금지.
> 최종 확인: 2026-08-03 (코드 직접 대조) / 태그 `step5-2`

---

## 완료 조건 대조

| # | 완료 조건 | 상태 |
|---|---|---|
| 1 | 조준선에 픽업을 담으면 HUD에 "줍기 — 붕대" 프롬프트가 뜬다 | ✅ 사용자 확인 |
| 2 | F → 픽업이 파괴되고 서버 로그에 획득이 찍힌다 | ✅ 사용자 확인 (`[Pickup] %s 획득`) |
| 3 | **사거리 밖 요청을 서버가 거부**한다 (치트 시뮬레이션) | ⚠️ **미검증.** 아래 |
| 4 | PIE 2인 동시 F → **한 명만 성공**, 나머지는 실패 사유 수신 | ⚠️ **미검증.** 아래 |
| 5 | 로컬 컨트롤러가 아닌 캐릭터에서는 트레이스 틱이 안 돈다 | ✅ **코드로 확인** — `bStartWithTickEnabled = false`(`EPInteractionComponent.cpp:19`) + `RefreshTickEnabled()`가 `IsLocallyControlled()`로만 켠다(`:43`) |
| 6 | 리슨서버 창(PIE 1번)에서도 프롬프트가 뜬다 | ✅ 사용자 확인 |
| 7 | `UnPossessed` 후 틱이 꺼진다 | ⚠️ **검증 불가 — 리스폰 경로가 프로젝트에 없다.** 아래 |

### ⚠️ 3·4·7이 미검증인 이유 (셋 다 성격이 다르다)

**3 — 치트 시뮬레이션 수단이 없다.**
7차 검수로 `Server_Interact` 직접 RPC가 폐기되면서 **손으로 부를 RPC가 사라졌다.** 사거리 검사(`EPGA_Interact.cpp:69-75`)는 코드에 있고 정상 경로에서 도달하지만, *"클라가 250cm 밖에서 이벤트를 쏘면 거부되는가"* 는 `Input_Interact`가 `FocusedActor`(트레이스 결과)만 넘기므로 **정상 플레이로는 재현 자체가 안 된다.** 검증하려면 디버그 커맨드가 필요하다.

**4 — 두 창에서 같은 프레임에 F를 누를 수단이 없다.**
선점 코드(`EPPickup.cpp:68` `bClaimed = true`가 `Destroy()`보다 먼저)는 함정 #3 대응으로 들어가 있다. 다만 **지금은 `bClaimed`가 실질적으로 무의미하다** — `OnInteract`이 항상 `true`를 반환하고 즉시 `Destroy()`하므로, 두 번째 요청은 `bClaimed` 검사가 아니라 `IsValid(Target)` 검사(`EPGA_Interact.cpp:53`)에서 걸린다. **`bClaimed`가 실제로 값어치를 내는 것은 Step 03에서 삽입 실패 갈래가 생긴 뒤다.**

**7 — 문서가 이미 예고했다** (`05_Loot_02_Interaction.md:24`).
> 첫 스폰은 리슨서버 호스트도 **통과한다.** 깨지는 것은 **리스폰**이고, 이 프로젝트에는 아직 리스폰 경로가 없다(`RestartPlayer` 호출 0건). 즉 **지금은 어떻게 짜도 테스트가 통과한다.**

대응은 들어가 있다 — `AEPCharacter::NotifyControllerChanged()`(`EPCharacter.cpp:144-148`) 훅 **하나**. 함정 7c(`PossessedBy` + `OnRep_Controller` 둘로 나눔)를 피했다. **"테스트가 통과했으니 이 줄은 필요 없다"로 읽지 말 것.**

---

## 02-1 — `IEPInteractable`

**상태: 완료**

```cpp
// Public/Interaction/EPInteractable.h:22-27
virtual FText GetInteractText() const = 0;
virtual bool  CanInteract(AEPCharacter* Interactor, FText& OutReason) const = 0;
virtual float GetInteractDuration() const { return 0.f; }
virtual bool  OnInteract(AEPCharacter* Interactor, FText& OutReason) = 0;
```

- **함정 #10 회피 확인.** `OnInteract`이 처음부터 `bool` + `OutReason`이다. 구현체가 넷(픽업·컨테이너·자판기·탈출)이 된 뒤에 바꾸는 비용을 선불했다
- `class AEPCharacter;` 전방 선언(`:9`) 필수 — `EPPickup.h:7`이 `EPTypes.h`(`:8`)보다 **먼저** 이 헤더를 include하므로 없으면 컴파일 에러
- **`GetInteractDuration()`은 선언만 되어 있고 읽는 코드가 0곳이다.** §7-1 컨테이너 검색 시간이 들어올 때 처음 소비된다. 지금 지우면 그때 인터페이스를 다시 뜯는다

---

## 02-2 — `UEPGA_Interact` (7차 검수 반영: 직접 RPC → GAS)

**상태: 완료**

```cpp
// EPGA_Interact.cpp:14-23
NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
bServerRespectsRemoteAbilityCancellation = false;

FAbilityTriggerData Trigger;
Trigger.TriggerTag    = EmpGameplayTags::TAG_Ability_Interact;   // "Ability.Interact"
Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
AbilityTriggers.Add(Trigger);

ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
```

### ★ `LocalPredicted`는 예측이 아니라 라우팅 스위치다

이 어빌리티는 아무것도 예측하지 않는다. 클라 인스턴스는 `ActivateAbility` 첫 줄에서 `IsNetAuthority()`가 false라 즉시 `EndAbility`한다(`:30-34`).

**그런데도 `LocalPredicted`가 아니면 동작하지 않는다.** `ServerOnly`/`ServerInitiated`면 클라에서 `AbilitySystemComponent_Abilities.cpp:1755`가 **RPC를 보내기 전에** `return false`한다. 정책이 곧 "서버로 갈 것인가"의 스위치다.

> 순서는 안전하다 — 서버 RPC(`:1923`)가 로컬 `CallActivateAbility`(`:1951`)보다 **먼저** 나간다. 클라 인스턴스가 즉시 종료해도 요청은 이미 떠난 뒤다.

### ★ `AbilityTriggers`이지 `AbilityTags`가 아니다

`HandleGameplayEvent`가 읽는 것은 `GameplayEventTriggeredAbilities`이고, 그 맵을 채우는 것은 `AbilityTriggers`다(`AbilitySystemComponent_Abilities.cpp:560`). **두 레지스트리는 별개다.** `AbilityTags`에 넣으면 F키가 아무 반응도 없다.

### ★★ `Super::ActivateAbility`를 부르면 안 된다 — 실제로 밟았다

```cpp
// 초기 구현 (버그)
Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);   // ← EndAbility 대신
```

**증상: F가 딱 한 번 먹히고 그 뒤로 영영 안 먹힌다.**

```
LogTemp: [Pickup] Backpack_Small 획득
LogAbilitySystem: Display: InternalServerTryActivateAbility: Rejecting ClientActivation of
    Default__BP_GA_Interact_C with PredictionKey [321/0]. InternalTryActivateAbility failed:
                                                                                    ↑ 비어 있다
```

**원인:** `UGameplayAbility::ActivateAbility`의 네이티브 분기(`GameplayAbility.cpp:904-934`)는 **주석뿐이고 실행문이 하나도 없다.** 서버 인스턴스가 영영 종료되지 않고, `InstancedPerActor` + `bRetriggerInstancedAbility == false`(선언만 있고 모듈 전체에서 대입되는 곳이 없다 — 제로 초기화)라 `:1821`에서 재활성화가 거부된다.

**실패 태그 문자열이 비어 있는 것이 이 경로의 지문이다.** `ActivationBlockedTags`·코스트·쿨다운 거부는 `InternalTryActivateAbilityFailureTags`를 채우는데, `:1804-1823` 경로는 채우지 않는다.

**수정:** `EndAbility(Handle, ActorInfo, ActivationInfo, true, false)` 무조건 호출 (`:45`).

### 서버 판정 — 5단계 (`TryInteract`, `:48-80`)

| 단계 | 코드 | 막는 함정 |
|---|---|---|
| ① 권한 | `:30` `IsNetAuthority()` | **#13** — 구현체 넷에 가드를 분산하지 않는다. 여기 한 줄 |
| ② 대상 유효 | `:53` `IsValid` + `IsActorBeingDestroyed` | 동시 요청의 두 번째가 여기서 걸린다 |
| ③ 인터페이스 | `:59` `Cast<IEPInteractable>` | **#8** — 조작된 클라의 임의 액터 |
| ④ 거리 | `:69-75` `InteractRange + ServerRangeTolerance` (250 + 100) | **#1** |
| ⑤ `CanInteract` → `OnInteract` | `:77-79` | **#2** — Step 03 `DropCooldown`이 여기 걸린다 |

실패 시 `PC->Client_OnInteractFailed(Reason)` (`:40-43`). **함정 #4(조용한 return) 대응.**

> **★ `:40`의 `!` 하나가 빠져 있었다.** `if (TryInteract(...) && PC)` — **성공했을 때** 실패 회신을 보냈다. `Reason`이 비어 `Client_OnInteractFailed_Implementation`이 `IsEmpty()`로 조기 반환(`EPPlayerController.cpp:40`)하는 바람에 증상이 없었고, **실패 사유만 영영 안 떴다.** 수정 완료.

---

## 02-3 — `UEPInteractionComponent`

**상태: 완료**

```cpp
// EPInteractionComponent.cpp:15-22
PrimaryComponentTick.bCanEverTick        = true;
PrimaryComponentTick.TickInterval        = 0.1f;      // 함정 #6
PrimaryComponentTick.bStartWithTickEnabled = false;   // 함정 #7
SetIsReplicatedByDefault(false);                      // 복제할 UPROPERTY가 0개
```

- `InteractRange = 250.f` / `ServerRangeTolerance = 100.f`, 둘 다 `EditDefaultsOnly` + `FORCEINLINE` 게터 (`EPInteractionComponent.h:24-25`) — **어빌리티가 이 값을 읽는다**
- `FocusedActor`는 `UPROPERTY() TObjectPtr<AActor>`, private
- `RefreshTickEnabled()`(`:40-44`)의 유일한 호출처는 `AEPCharacter::NotifyControllerChanged()` — **훅 하나** (함정 7b·7c)

### ★ 시점 획득 — `GetComponentLocation()` + `GetControlRotation().Vector()`

```cpp
// :62-63
const FVector Start = Cam->GetComponentLocation();
const FVector Dir   = Owner->GetControlRotation().Vector();
```

**`Cam->GetForwardVector()`가 아닌 이유가 둘이다.**

1. **한 프레임 stale.** 카메라의 `bUsePawnControlRotation` 보정은 `UCameraComponent::GetCameraView()` 안에서만 적용되고(`CameraComponent.cpp:425-436`), 그건 컴포넌트 틱(`TG_PrePhysics`)보다 뒤다
2. **축이 틀어져 있다.** 카메라가 `head` 본에 부착되고 `FRotator(0, 90, -90)` 보정이 박혀 있어(`EPCharacter.cpp:61-64`), 부모 본 트랜스폼이 그 상대 회전을 다시 적용한다

`GetActorEyesViewPoint()`도 쓸 수 없다 — `APawn::GetPawnViewLocation()`(`Pawn.cpp:334-337`)이 `ActorLocation + BaseEyeHeight`라 **본 부착을 무시한다.**

> 결과적으로 `EPGA_Item_PrimaryUse.cpp:48`(Origin)·`:56`(Dir)과 **같은 조합**이다. 사격과 상호작용이 같은 광선을 본다.

### 입력 → 이벤트 (`Input_Interact`, `:24-38`)

```cpp
FGameplayEventData Payload;
Payload.EventTag   = EmpGameplayTags::TAG_Ability_Interact;
Payload.Instigator = Owner;
Payload.Target     = FocusedActor;              // ★ 이게 서버 RPC 파라미터다
ASC->HandleGameplayEvent(EmpGameplayTags::TAG_Ability_Interact, &Payload);
```

`FGameplayEventData` **구조체 전체**가 `ServerTryActivateAbilityWithEventData`로 실려 간다. `FGameplayAbilityTargetData`도 서버 재트레이스도 필요 없다.

> Lyra는 이 길을 쓰지 않고 서버가 자기 라인 트레이스를 돌린다(`LyraGameplayAbility_Interact.cpp:78-122`). 이유는 Lyra의 F키가 어빌리티 *활성화*가 아니라 이미 떠 있는 어빌리티의 *트리거*이기 때문이다. **우리는 F키가 곧 활성화라 파라미터가 자연스럽게 실린다.**

---

## 02-4 — HUD 프롬프트

**상태: 완료**

| 위치 | 내용 |
|---|---|
| `EPHUDWidget.h:62` | `UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> InteractPrompt` |
| `EPHUDWidget.h:25` | `void SetInteractPrompt(const FText&, bool)` — **public** |
| `EPHUDWidget.cpp:67-82` | 빈 텍스트면 `Collapsed`, 아니면 `SetText` + 색상 + `HitTestInvisible` |
| `EPPlayerController.cpp:33-36` | 패스스루 — `HUDWidget`이 PC의 `private`이라 이 경로가 유일하다 |
| `EPInteractionComponent.cpp:78-95` | 포커스가 **바뀔 때만** 갱신 |

**함정 #11 대응 확인** — `NewFocus == nullptr`일 때도 `SetInteractPrompt(FText::GetEmpty(), false)`를 부른다(`:84-88`). 다른 곳을 봐도 프롬프트가 남지 않는다.

### ★ 회색(`bEnabled = false`) 갈래는 현재 도달 불가능한 코드다

```cpp
// :93-95
const bool bCan = Interactable->CanInteract(Owner, Reason);
PC->SetInteractPrompt(bCan ? Interactable->GetInteractText() : Reason, bCan);
```

`UpdateFocus()`는 **클라이언트에서** 돌고, `AEPPickup::CanInteract`가 보는 유일한 값 `bClaimed`는 **복제되지 않는다**(`EPPickup.h:50`, private, `UPROPERTY` 없음). 클라는 항상 `false`를 보므로 **`bCan`이 항상 true다.**

**Step 03의 `DropCooldown`이 이 갈래의 첫 실제 소비자다.** 지금 "안 쓰이니 지우자"로 판단하면 그때 다시 만든다.

---

## 02-5 — `AEPPickup` 인터페이스 구현

**상태: 완료 (Step 03에서 `OnInteract`이 바뀐다)**

```cpp
// EPPickup.cpp:36 — 순서가 중요하다. SetCollisionResponseToAllChannels 뒤여야 한다
Mesh->SetCollisionResponseToChannel(EP_TraceChannel_Interact, ECR_Block);
```

**함정 #5c 확인** — Step 01의 전 채널 `ECR_Ignore`(`:35`)에 의존이 걸려 있다. 그 줄을 "정리"하면 이 줄까지 무의미해진다.

```cpp
// :66-73 — Step 03에서 통째로 바뀔 유일한 지점 (함정 #9)
bool AEPPickup::OnInteract(AEPCharacter* Interactor, FText& OutReason)
{
    bClaimed = true;                                              // ★ 무엇보다 먼저 (함정 #3)
    UE_LOG(LogTemp, Log, TEXT("[Pickup] %s 획득"), *ItemId.ToString());
    Destroy();
    return true;
}
```

- `GetInteractText()`(`:47-54`) — `UEPItemDefinitionSubsystem::Get(this)` → `FindData(ItemId)->DisplayName`. DT에 없으면 `ItemId` 원문 폴백
- `CanInteract()`(`:56-64`) — **`bClaimed`만 본다.** `DropCooldown` 개념 없음 (Step 03에서 추가)
- `Interactor` 파라미터는 셋 다 아직 안 읽는다. 컨테이너/자판기가 소비자다

---

## 02-6 — 입력 배선

**상태: 완료**

| 위치 | 내용 |
|---|---|
| `EPPlayerController.h:93` | `InteractAction` UPROPERTY (`EditDefaultsOnly`) |
| `EPPlayerController.h:35` | `GetInteractAction()` FORCEINLINE 게터 |
| `EPCharacter.cpp:269-277` | null 가드 후 `ETriggerEvent::Triggered` 바인딩 |
| `EPCharacter.cpp:79` | 생성자에서 `CreateDefaultSubobject<UEPInteractionComponent>` |
| `EPCharacter.cpp:289-292` | `GetInteractionComponent()` — **어빌리티가 사거리를 읽는 통로** |
| `EPNativeGameplayTags.cpp:30` | `UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Interact, "Ability.Interact")` |
| `DefaultEngine.ini:308` | `GameTraceChannel3` = `Interact`, `DefaultResponse=ECR_Ignore`, `bTraceType=True` |
| `EPTypes.h:97` | `EP_TraceChannel_Interact = ECC_GameTraceChannel3` |

**바인딩 형태 주의** — 처음에 이렇게 썼다가 컴파일 에러가 났다.

```cpp
EnhancedInput->BindAction(..., this, InteractionComponent->Input_Interact());   // ❌ 호출식을 넘김
EnhancedInput->BindAction(..., InteractionComponent, &UEPInteractionComponent::Input_Interact);  // ✅
```

**함정 #5d 회피 확인** — `GameTraceChannel2`는 `Projectile`이 이미 쓰고 있다(`DefaultEngine.ini:307`). `EPTypes.h`에는 `Weapon`(Channel1) 상수만 있어서 **소스만 보면 2번이 비어 보인다.**

### 에셋

| | 상태 |
|---|---|
| `IA_Interact` + IMC에 **F** 키 바인딩 | ✅ |
| `BP_GA_Interact` (`UEPGA_Interact` 상속) | ✅ |
| `AEPCharacter::DefaultAbilities`에 `BP_GA_Interact` 항목 | ✅ |
| `WBP_HUD`에 `InteractPrompt` 이름의 `TextBlock` | ✅ — **이름이 정확히 일치해야 한다.** `BindWidgetOptional`이라 틀려도 컴파일·실행이 통과하고 프롬프트만 조용히 안 뜬다 |

---

## 알려진 제약 (버그 아님, 의도된 것)

### 벽이 상호작용 트레이스를 막지 않는다

`GameTraceChannel3`의 `DefaultResponse=ECR_Ignore`(`DefaultEngine.ini:308`)라 **월드 지오메트리가 이 채널을 통과시킨다.** 벽 두께 안이나 얇은 벽 너머의 픽업을 주울 수 있다.

- 이 값이 `ECR_Block`이면 **함정 #5b**를 밟는다 — 픽업 앞의 잡동사니가 상호작용을 막고, 증상이 `ECC_Visibility` 재사용(#5)과 똑같아 "전용 채널을 만들었으니 안전하다"고 믿으며 그대로 밟는다
- 사거리가 250cm라 실질 피해가 작고, **Lyra도 같은 방식이다**
- 막아야 할 때가 오면 `ECC_WorldStatic` 한 번을 추가로 트레이스해 가리는 방식이 맞다 — 채널의 `DefaultResponse`를 뒤집는 게 아니다

### Megascans 계열 메시는 콜리전이 없다

`DA_AmmoBox_545`가 `/Game/Megascans/3D_Assets/Military_Ammo_Can_uephdgehw/S_..._lod5_Var1`을 가리키는데, Megascans 에셋은 콜리전 없이 배포된다. `QueryOnly` 라인 트레이스가 그냥 통과해 **탄약상자만 안 주워졌다.**

**해결: 해당 스태틱 메시에 Box Simplified Collision 추가.** 다른 아이템은 콜리전이 있는 플레이스홀더 메시를 그리고 있어서 증상이 하나만 났다.

> 대안으로 `AEPPickup`에 전용 `UBoxComponent`를 두는 방법이 있으나, 그건 Step 01 01-4의 콜리전 설계를 바꾸는 일이라 **문서를 먼저 고쳐야 한다** (CLAUDE.md §2).

---

## Step 03에 넘기는 것

| | |
|---|---|
| `AEPPickup::OnInteract` (`EPPickup.cpp:66-73`) | **바뀌는 유일한 지점.** 로그+`Destroy()` → `AddSubtree()` 성공/실패 갈래 |
| `bClaimed` 되돌리기 | 실패 갈래가 Step 03에서 처음 생긴다. 되돌림 책임이 `OnInteract` 안인지 `UEPGA_Interact`인지 **아직 안 정해졌다** |
| `CanInteract`의 `DropCooldown` | 5단계 ⑤가 그 통로다. **이게 없으면 클라만 회색으로 그리고 서버는 통과시킨다** |
| 실패 프롬프트 회색 갈래 | 02-4 참조 — Step 03이 첫 실제 소비자 |
| `Client_OnInteractFailed` | "가방에 자리가 없습니다"가 이 경로를 탄다 |
| `IEPInteractable::GetInteractDuration()` | 여전히 미사용. §7-1까지 그대로 둔다 |

### ★ 프롬프트가 포커스 변화에만 갱신된다

`UpdateFocus()`는 `NewFocus == FocusedActor`면 즉시 `return`한다(`:78`). **같은 대상을 계속 보고 있는 동안에는 프롬프트가 절대 갱신되지 않는다.**

Step 03의 `DropCooldown`(0.5초)이 들어오면 문제가 된다 — 버린 픽업을 계속 보고 있으면 쿨다운이 끝나도 **회색 프롬프트가 그대로 남는다.** 대응은 Step 03에서 정한다 (시간 기반 재평가 / 쿨다운 종료 시 강제 갱신 중 하나).

---

## 남은 작업

| # | 무엇 | 왜 |
|---|---|---|
| 1 | 완료 조건 3·4 검증 수단 | 사거리 거부와 동시 획득 경쟁이 **코드에만 있고 한 번도 실행되지 않았다** |
| 2 | 리스폰 경로가 생기면 완료 조건 7 재검증 | `NotifyControllerChanged` 훅이 그때 처음 진짜로 시험된다 |

**둘 다 Step 03을 막지 않는다.** 다만 1번은 Step 03의 `DropCooldown`·`bClaimed` 되돌리기가 **정확히 같은 경로**를 쓰므로, 그때 함께 검증하는 것이 자연스럽다.
