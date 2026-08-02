# 검수 답변 7차 — Step 02 상호작용: GAS로 간다. 그리고 3-2는 증상이 틀렸다

> 작성일: 2026-08-02
> 요청: `05_Loot_REVIEW7_Request.md`
> 이전: 6차 `05_Loot_REVIEW6_Request.md` / `_Answer.md`
> 근거: UE 5.7 엔진 소스 직독 + Lyra(`C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame`) 직독. 인터넷 인용 없음

---

## 0. 판정 요약

| 항목 | 판정 | 한 줄 |
|---|---|---|
| **§1-2 전제** — "이 프로젝트에 서버 RPC가 하나도 없다" | ⚠️ **반만 맞다** | 손으로 쓴 `UFUNCTION(Server)`가 0개인 것이지, 서버 RPC가 0개인 게 아니다. `TryActivateAbilitiesByTag` 한 줄이 `ServerTryActivateAbility`를 부른다 |
| **§2 A/B/C** | ✅ **B — `GA_Interact`** | 단, Lyra의 `FInteractionOption` 체계가 아니라 **어빌리티 하나짜리 B** |
| §2-4 ②  대상 전달 | ✅ **해결됨** | `FGameplayEventData::Target`이 RPC 파라미터다(`GameplayAbilityTypes.h:256`). TargetData도 서버 재트레이스도 필요 없다. **§2-3의 가장 센 반대 근거가 무너진다** |
| §2-4 ③  `IEPInteractable` | ✅ **유지** | Lyra가 `FInteractionOption`을 만든 이유는 **대상마다 어빌리티가 다르기 때문**이다. 우리는 하나다 |
| §2-4 ④  부여 시점 | ✅ **상시 부여** | `EPCharacter.h:61`의 `DefaultAbilities`에 한 줄. Lyra의 동적 부여는 우리 규모에 과하다 |
| §2-4 ⑤  A/C 유지 시 정당화 | 📄 §2-7에 문안 제공 | 사용자가 A를 고를 경우에 대비 |
| **3-1** 채널 `Channel2`→`Channel3` | ✅ **맞다.** 근거도 보강됨 | 중복 시 **나중 것이 조용히 버려진다**(`CollisionProfile.cpp:401-408`). 그리고 Lyra가 우리와 **완전히 같은 문안**을 쓴다 |
| **3-2** `BeginPlay` 함정 | 🔀 **결론은 맞고 증상이 틀렸다** | 리슨서버 호스트는 **첫 스폰에서 통과한다.** 깨지는 건 **리스폰**이다. 문서대로 테스트하면 "함정이 없네"로 잘못 닫는다 |
| 3-2 대응(두 훅) | ⚠️ **부족하다** | `UnPossessed`가 빠져 있다. 엔진에 **`NotifyControllerChanged()`** 단일 훅이 있다(`Pawn.h:382`) |
| **3-3** 동시성 주장 | ✅ **정확하다** | 예측이 들어와도 유지된다 — 단 방어 지점을 명시해야 한다(§3-3) |
| **3-4** `bool` + `FText&` | ✅ **유지.** 단 경고 하나 | `FText`는 RPC로 **통째로 직렬화된다**(`FTextProperty`에 `NetSerializeItem` 없음). 회신 경로에는 태그/enum이 싸다 |
| **3-5** `false`여도 RPC가 나가는가 | ✅ **나간다** | 다만 **메커니즘 설명이 틀렸다.** "아우터 GUID + 이름"이 아니라 **Default GUID + 상대 경로 익스포트**다 |
| 3-5 RPC 두 클래스 분리 | ✅ 맞다 — 그런데 §2가 B면 절반이 사라진다 | `Server_Interact`와 `SetIsReplicatedByDefault` 논의가 통째로 무의미해진다. `Client_OnInteractFailed`는 남는다 |
| **4-1** 라인 vs 스윕 | ✅ **라인 유지** | Lyra도 라인이다(`LineTraceMultiByProfile`). 스윕은 실무에서 소수 |
| **4-2** 채널 여는 곳 | ✅ **콜리전 프리셋** | Lyra가 `Interactable_OverlapDynamic` 프리셋을 `.ini`에 두고 있다(`DefaultEngine.ini:213`). 액터마다 한 줄도, 공통 베이스도 아니다 |

**한 줄 요약:** 이번 요청의 답은 **"B로 가라, 그리고 대상 전달은 이미 엔진이 해준다"**이고, 검증 5건 중 **3-2 하나가 실제로 위험**하다 — 판정은 맞는데 **증상이 틀려서, 테스트가 통과하면 그 판정을 되돌리게 된다.**

---

## 1. ★ 먼저 전제를 고친다 — "서버 RPC가 0개"가 아니다

§1-2의 grep 결과는 사실이다. `Public/`에 `UFUNCTION(Server)`가 없다. 하지만 결론이 다르다.

```cpp
// EPCharacter.cpp:394
ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Item_PrimaryUse));
```

이 한 줄이 클라이언트에서 실행되면 GAS 내부에서 이렇게 간다.

```cpp
// AbilitySystemComponent_Abilities.cpp:1899-1928 (LocalPredicted 분기)
FScopedPredictionWindow ScopedPredictionWindow(this, true);
ActivationInfo.SetPredicting(ScopedPredictionKey);

if (TriggerEventData)
{
    ServerTryActivateAbilityWithEventData(Handle, Spec->InputPressed, ScopedPredictionKey, *TriggerEventData);  // ← 서버 RPC
}
else
{
    CallServerTryActivateAbility(Handle, Spec->InputPressed, ScopedPredictionKey);                              // ← 서버 RPC
}
```

```cpp
// AbilitySystemComponent.h:1723
UE_API void ServerTryActivateAbility(FGameplayAbilitySpecHandle AbilityToActivate, bool InputPressed, FPredictionKey PredictionKey);
```

**그러니까 이 프로젝트의 모든 사격·재장전·대시·힐·실드가 이미 서버 RPC를 타고 있다.** `UEPCombatComponent`가 `SetIsReplicatedByDefault(true)`인 이유도 결국 그것이다.

### 이게 §2 판정에 무슨 차이를 만드는가

§2-2(a)의 "관례가 여기서만 깨진다"는 **여전히 유효하지만 이유가 다르다.** 깨지는 것은 *"서버 RPC를 안 쓴다"*는 규칙이 아니라 — 그런 규칙은 존재한 적이 없다 — **"게임플레이 입력의 진입점은 어빌리티 태그 하나다"**라는 규칙이다.

그리고 그 규칙이 깨지면 구체적으로 이것들이 갈린다.

| GAS 경로가 이미 주고 있는 것 | `Server_Interact` 직접 RPC에서 다시 만들어야 하는 것 |
|---|---|
| `ActivationBlockedTags.AddTag(TAG_State_Dead)` — 3개 어빌리티에 이미 있다 | 죽은 상태 확인 `if (Char->IsDead()) return;` |
| `ActivationBlockedTags` / `ActivationRequiredTags` | 사격 중·재장전 중 상호작용 금지 로직 |
| `ClientActivateAbilityFailed`(`AbilitySystemComponent.h:1747`) | 실패 회신의 절반 |
| `NetSecurityPolicy` 검사(`..._Abilities.cpp:2052-2058`) | 조작 클라 차단 |
| GE 쿨다운(Step 03 `DropCooldown`) | 손으로 만든 타임스탬프 맵 |
| `CastTime` + `State.Casting` + `WBP_CastGauge`(`EPGA_Skill_Base.h:31`) | 채널링 — 02-1이 이미 GAS에 위임하겠다고 적어 놨다 |

**"관례 하나가 예외를 만든다"가 아니라, "이미 지어진 배관 여섯 개를 안 쓰고 옆에 새 배관을 판다"가 정확한 서술이다.**

---

## 2. ★ §2 판정 — **B**

### 2-1. 먼저 C부터: **의도된 절충이 아니라 미처리 모순이다**

02-1은 이렇게 적어 두었다.

> `GetInteractDuration() > 0`인 채널링은 **GAS 어빌리티로 구현한다.** 상호작용 컴포넌트는 채널링을 직접 만들지 않고 어빌리티를 활성화만 한다.

그런데 02-2는 `Server_Interact(AActor* Target)`를 선언한다. 이 둘을 같이 두면 **같은 F키가 대상의 `GetInteractDuration()` 값에 따라 다른 네트워크 경로를 탄다.**

C가 절충이 되려면 두 경로의 **경계가 안정적**이어야 한다. 그런데 경계가 데이터(`GetInteractDuration()`)에 달려 있다. §7-2 자판기의 duration을 5초에서 0으로 바꾸는 순간 그 대상의 배관이 통째로 바뀐다. **데이터 값 하나가 네트워크 경로를 바꾸는 설계는 절충이 아니다.**

그리고 §7-1 컨테이너가 들어오면 두 경로가 동시에 살아 있게 되고, 그때 물어야 하는 질문이 늘어난다 — 실패 회신은 어느 쪽으로? `bClaimed` 선점은 예측 실행에서도 도는가? 쿨다운은 GE인가 손으로 만든 맵인가?

**C는 기각한다.**

### 2-2. ★ §2-3의 가장 센 반대 근거가 사실이 아니다

요청 §2-3은 이렇게 적었다.

> **대상 전달이 번거롭다.** `Server_Interact(AActor*)`는 파라미터 한 개다. GAS로 하려면 `FGameplayAbilityTargetData`를 만들거나 어빌리티가 서버에서 다시 트레이스해야 한다.

**셋째 길이 있고, 그게 표준이다.** `FGameplayEventData`에 `Target`이 있다.

```cpp
// GameplayAbilityTypes.h:233-284
USTRUCT(BlueprintType)
struct FGameplayEventData
{
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ...)
    FGameplayTag EventTag;                        // :248

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ...)
    TObjectPtr<const AActor> Instigator;          // :252

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ...)
    TObjectPtr<const AActor> Target;              // :256  ★
    ...
};
```

그리고 이 구조체가 **통째로 서버 RPC의 파라미터다.**

```cpp
// AbilitySystemComponent.h:1726
UE_API void ServerTryActivateAbilityWithEventData(
    FGameplayAbilitySpecHandle AbilityToActivate, bool InputPressed,
    FPredictionKey PredictionKey, FGameplayEventData TriggerEventData);   // ← Payload 전체가 서버로 간다
```

```cpp
// AbilitySystemComponent_Abilities.cpp:1992-1995
void UAbilitySystemComponent::ServerTryActivateAbilityWithEventData_Implementation(
    FGameplayAbilitySpecHandle Handle, bool InputPressed, FPredictionKey PredictionKey, FGameplayEventData TriggerEventData)
{
    InternalServerTryActivateAbility(Handle, InputPressed, PredictionKey, &TriggerEventData);
}
```

즉 **`Payload.Target = FocusedActor` 한 줄이 `Server_Interact(AActor*)`와 정확히 같은 일을 한다.** 대상은 클라가 정하고 서버는 그 대상을 받는다 — 문서가 세운 *"클라는 요청, 서버가 결정"* 원칙이 그대로다. §2-3이 걱정한 "서버 재트레이스로 대상이 갈리는" 갈래는 **아예 생기지 않는다.**

호출 쪽은 이렇다.

```cpp
// UEPInteractionComponent::Input_Interact()  — 클라 로컬
void UEPInteractionComponent::Input_Interact()
{
    if (!FocusedActor) return;

    AEPCharacter* Owner = Cast<AEPCharacter>(GetOwner());
    UAbilitySystemComponent* ASC = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
    if (!ASC) return;

    FGameplayEventData Payload;
    Payload.EventTag   = EmpGameplayTags::TAG_Ability_Interact;
    Payload.Instigator = Owner;
    Payload.Target     = FocusedActor;              // ★ 이 한 줄이 대상 전달의 전부다

    ASC->HandleGameplayEvent(EmpGameplayTags::TAG_Ability_Interact, &Payload);
}
```

`GA_Interact`는 `AbilityTriggers`에 `GameplayEvent` / `TAG_Ability_Interact`를 하나 등록해 두면 된다.

> **주의 하나.** `Payload.Target`은 오브젝트 참조로 직렬화된다. `AEPPickup`은 **복제 액터**이므로 서버가 발급한 NetGUID로 해석된다 — 문제없다. 복제되지 않는 액터를 넣으면 서버에서 null로 도착한다. §7-1 컨테이너·§7-2 자판기도 복제 액터여야 한다는 뜻이고, 어차피 그래야 한다.

### 2-3. Lyra는 왜 이렇게 안 하는가 — 그리고 왜 우리는 그래도 되는가

Lyra의 `Interaction/` 모듈은 이 문제를 훨씬 크게 푼다. §5에 전체 구조를 적었고, 여기서는 판정에 필요한 한 가지만 뽑는다.

**Lyra의 핵심 전제는 "대상마다 실행할 어빌리티가 다르다"이다.**

```cpp
// InteractionOption.h:34-50
// 1) Place an ability on the avatar that they can activate when they perform interaction.
UPROPERTY(EditAnywhere, BlueprintReadOnly)
TSubclassOf<UGameplayAbility> InteractionAbilityToGrant;

// - OR -

// 2) Allow the object we're interacting with to have its own ability system and interaction ability, that we can activate instead.
UPROPERTY(BlueprintReadOnly)
TObjectPtr<UAbilitySystemComponent> TargetAbilitySystem = nullptr;

UPROPERTY(BlueprintReadOnly)
FGameplayAbilitySpecHandle TargetInteractionAbilityHandle;
```

문 액터는 문 여는 어빌리티를, 픽업은 줍는 어빌리티를 **자기가 들고 온다.** 그래서 `IInteractableTarget::GatherInteractionOptions()`가 옵션 배열을 만들고, 근처를 스캔해 그 어빌리티를 **동적으로 부여**하고(`AbilityTask_GrantNearbyInteraction`), 옵션이 여러 개면 `IInteractionInstigator`가 고르고, 옵션마다 위젯이 다르다.

**우리 프로젝트의 표는 정반대다.** 02-1의 네 구현체 표를 다시 보면:

| | 픽업 | 컨테이너 | 자판기 | 탈출 |
|---|---|---|---|---|
| 실행할 어빌리티 | `GA_Interact` | `GA_Interact` | `GA_Interact` | `GA_Interact` |
| 다른 것 | `OnInteract()` 본문 | `OnInteract()` 본문 | `OnInteract()` 본문 | `OnInteract()` 본문 |

**어빌리티는 하나고, 달라지는 것은 인터페이스 구현뿐이다.** 그러면 `FInteractionOption`은 필드가 하나뿐인 구조체로 쪼그라들고, `GrantNearbyInteraction`은 부여할 게 없어서 빈 루프가 되고, `IInteractionInstigator`는 선택지가 하나라 호출되지 않는다.

**6차에서 `DA_EPGameData`를 기각한 논리가 여기에 그대로 적용된다.** 다만 결론이 반대 방향인 이유가 있다 —

- 6차: `DA_EPGameData`는 **문서에 이름이 없고**, 참조가 3개뿐이고, 옮겨도 문제가 안 줄었다 → 기각
- 7차: `GA_Interact`는 **문서에 이름이 없지만**(CLAUDE.md §2 기준으로 걸린다), 02-1이 *"채널링은 GAS 어빌리티로 구현한다"*고 **이미 적어 놨다.** `GA_Interact`는 새 계층이 아니라 **그 문장의 일관된 완성**이다. 반면 `FInteractionOption` 체계는 문서에 이름이 없고 소비자도 없다 → 그건 기각

**판정: B. 단 Lyra의 옵션 체계는 가져오지 않는다.**

### 2-4. B가 실제로 드는 비용 — 세어 봤다

| 추가 | 위치 | 분량 |
|---|---|---|
| `TAG_Ability_Interact` | `EPNativeGameplayTags.h`(`TAG_Ability_Item_Reload` 아래) + `.cpp` | 2줄 |
| `UEPGA_Interact` | `Public/GAS/` + `Private/GAS/` | 헤더 ~25줄 / cpp ~55줄 |
| `DefaultAbilities` 한 항목 | **이미 있는 배열**(`EPCharacter.h:61`, `EPCharacter.cpp:135-137`) | BP에서 1항목 |
| `AbilityTriggers` 1개 | `UEPGA_Interact` 생성자 | 4줄 |

| 사라짐 | |
|---|---|
| `UFUNCTION(Server, Reliable) void Server_Interact(AActor*)` | 선언 + `_Implementation` |
| `SetIsReplicatedByDefault(true/false)` 논쟁 전체 | 3-5의 절반 |
| 손으로 쓰는 죽음/상태 확인 | `ActivationBlockedTags` |
| Step 03 `DropCooldown` 자체 구현 | GE 쿨다운 |

**순증은 어빌리티 클래스 하나다.** 그리고 그 클래스는 이미 3개 형제(`EPGA_Item_PrimaryUse`, `EPGA_Item_Reload`, `EPGA_Skill_Base`)가 있는 자리에 넷째로 들어간다.

### 2-5. B가 **주지 않는** 것 — 정직하게

| 기대 | 실제 |
|---|---|
| 실패 사유가 클라로 자동 회신된다 | ❌ **아니다.** `ClientActivateAbilityFailed(Handle, PredictionKey)`(`AbilitySystemComponent.h:1747`)에는 **사유 파라미터가 없다.** `InternalTryActivateAbilityFailureTags`는 서버 로컬 변수다 |
| 예측이 공짜로 좋다 | ⚠️ **여기선 원하지 않는다.** 선점 경쟁(`bClaimed`)이 있어서 예측이 틀리면 픽업이 사라졌다 다시 나타난다 |
| 코스트/쿨다운이 지금 필요하다 | ❌ Step 02엔 없다. Step 03의 `DropCooldown`에서 값어치가 난다 |

그래서:

- **`Client_OnInteractFailed`는 그대로 유지한다.** 그리고 이건 관례를 안 깬다 — `AEPPlayerController`에 `Client_OnKill`·`Client_PlayHitConfirmSound`가 **이미 두 개 있다**(`EPPlayerController.h:40-45`). 3-5의 배치 판단은 §2가 B여도 그대로 살아남는다.
- **예측은 끈다.** `NetExecutionPolicy = LocalPredicted`로 두되(클라가 서버 RPC를 쏘려면 이 정책이어야 한다) **클라 실행부에서 아무것도 하지 않는다.**

```cpp
void UEPGA_Interact::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // 클라 예측 실행: 아무것도 하지 않는다. 판정은 전부 서버다.
    // 선점(bClaimed) 경쟁이 있어서 예측이 틀리면 픽업이 사라졌다 다시 나타난다.
    if (!ActorInfo->IsNetAuthority())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ false, /*bWasCancelled*/ false);
        return;
    }
    ...
}
```

### 2-6. B에서 02-3의 5단계는 어디로 가는가

**절차는 그대로다. 사는 곳만 바뀐다.**

```
F키 → UEPInteractionComponent::Input_Interact()          [로컬]
    → ASC->HandleGameplayEvent(TAG_Ability_Interact, &Payload{Target=FocusedActor})
        → (LocalPredicted) ServerTryActivateAbilityWithEventData  [엔진 제공 서버 RPC]
            → UEPGA_Interact::ActivateAbility  [서버]
                 1. TriggerEventData->Target 유효한가
                 2. IEPInteractable 구현하는가
                 3. ★ 거리 재검증
                 4. ★ CanInteract(Interactor, OutReason)
                 5. OnInteract(Interactor, OutReason) == true 인가
                 실패 → PC->Client_OnInteractFailed(OutReason)
                 EndAbility
```

**3·4단계가 ★인 이유도 그대로다.** 오히려 세지는데, 어빌리티에는 `ActivationBlockedTags`가 있어서 *"죽었을 때 상호작용 금지"* 같은 것이 3·4단계 앞에 태그 한 줄로 붙는다.

한 가지 주의: **`InteractRange`·`ServerRangeTolerance`가 컴포넌트에 있고 판정은 어빌리티에서 한다.** 3-5가 *"값을 가지러 다시 컴포넌트로 와야 한다"*고 지적한 그 문제가 방향만 바꿔서 남는다. 답: **어빌리티가 `AEPCharacter`에서 컴포넌트를 가져와 값을 읽는다.** 어차피 아바타 액터를 이미 들고 있어서 한 줄이다.

```cpp
const UEPInteractionComponent* IC = Interactor->GetInteractionComponent();
const float MaxDistSq = FMath::Square(IC->GetInteractRange() + IC->GetServerRangeTolerance());
```

CLAUDE.md §2 기준 — 이건 *"한 값을 두 경로가 봐야 하면 둘 다 볼 수 있는 곳에 둔다"*의 그 자리다. 컴포넌트에 두고 게터를 여는 게 맞다. 어빌리티 CDO에 값을 복제해 두면 갈린다.

### 2-7. 그래도 A로 남기기로 한다면 — 문서에 적을 정당화 문안

§2-4의 5번은 "근거 없이 예외로 두면 다음 시스템에서 또 갈린다"는 지적이고, 옳다. A를 고를 경우 **아래 조건 중 하나 이상이 참이어야 하고, 참인 이유를 그대로 적어야 한다.**

> **`Server_Interact`를 직접 RPC로 두는 근거 (A를 고를 경우)**
>
> 상호작용은 어빌리티가 아니다 — 코스트도, 쿨다운도, 애님도, 예측도 없다. GAS에 올리면 얻는 것(`ActivationBlockedTags`, GE 쿨다운, `ClientActivateAbilityFailed`) 중 **Step 02 시점에 실제로 쓰는 것이 하나도 없다.**
>
> **이 근거는 Step 03에서 만료된다.** `DropCooldown`이 들어오는 순간 GE 쿨다운이 필요해지고, 그때 이 결정을 다시 연다. **그 시점에 구현체가 넷(픽업·컨테이너·자판기·탈출)으로 늘어나 있으면 전환 비용이 네 배가 된다.**
>
> 채널링(`GetInteractDuration() > 0`)은 GAS로 간다는 02-1의 문장은 **그대로 유효하되**, 그때 상호작용 경로가 둘이 된다는 것을 알고 남기는 것이다.

**이 문안을 쓸 수 있으면 A로 가도 된다.** 두 번째 문단을 못 쓰겠으면 B다.

---

## 3. §3 — 방금 반영한 5건 검증

### 3-1. `ECC_GameTraceChannel2` → `Channel3` — ✅ 맞다. 근거가 더 세진다

#### 같은 번호에 두 줄을 넣으면 무슨 일이 일어나는가

에디터가 거부하지 않고, 나중 줄이 이기지도 않고, 둘 다 남지도 않는다. **먼저 처리된 쪽이 이름을 갖고, 나머지는 배열에서 제거된다.**

```cpp
// CollisionProfile.cpp:381-385  — 먼저 Channel 번호로 정렬한다
DefaultChannelResponses.Sort([](const FCustomChannelSetup& Rhs, const FCustomChannelSetup& Lhs)->bool
    { return (Rhs.Channel < Lhs.Channel); });

// CollisionProfile.cpp:401-408
if (TraceTypeMapping.Contains(EnumIndex) || ObjectTypeMapping.Contains(EnumIndex))
{
    UE_LOG(LogCollisionProfile, Warning,
        TEXT("Cannot map multiple responses to the same collision channel (%d); ignoring '%s' "), EnumIndex, *DisplayValue);
    DefaultChannelResponses.RemoveAt(ChannelResponseIndex);
    --ChannelResponseIndex;
    continue;                       // ← :470의 SetResponse에도 도달하지 못한다
}
```

세 가지가 따라 나온다.

1. **경고는 뜬다.** `DEFINE_LOG_CATEGORY_STATIC(LogCollisionProfile, Warning, All)`(`:11`)이라 기본 verbosity가 Warning이다. 다만 `UCollisionProfile::LoadProfileConfig`는 **엔진 초기화 중**에 돌아서 로그 맨 위에 묻힌다. *"조용하지는 않지만 못 본다."*
2. **버려진 쪽은 `DefaultResponse`도 적용되지 않는다.** `continue`가 `:470`의 `SetResponse`를 건너뛴다. 우리 경우 둘 다 `ECR_Ignore`라 여기서는 차이가 없다.
3. **`bTraceType`이 갈린다.** `Projectile`이 이기면 `Channel2`는 **오브젝트 타입**으로 남는다. 그래도 `LineTraceSingleByChannel(..., ECC_GameTraceChannel2, ...)`는 **컴파일도 되고 실행도 된다** — 트레이스는 채널 번호만 보지 `bTraceType` 메타데이터를 안 본다. 그래서 증상이 *"상호작용 트레이스가 발사체가 막는 것들을 같이 맞힌다"*로 나온다. **이게 진짜 함정이고, 채널 번호를 의심하기 전에 픽업 콜리전부터 파게 된다.**

> 어느 쪽이 이기는지는 `TArray::Sort`가 **안정 정렬이 아니다.** 항목이 적으면 삽입 정렬로 떨어져 사실상 `.ini` 순서가 유지되지만, **보장은 없다.** 즉 "둘 중 뭐가 이기는지"가 엔진 버전에 따라 바뀔 수 있는 종류의 버그다. 더더욱 넣으면 안 된다.

#### `bTraceType=True`가 맞는가 — ✅ 맞다. Lyra가 우리와 **글자까지 같다**

```ini
; Lyra/Config/DefaultEngine.ini:216
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="Lyra_TraceChannel_Interaction")
```

```ini
; 우리 문서(02-2)의 제안
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel3,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="Interact")
```

`Channel` 번호만 다르고 나머지가 동일하다. **함정 5b(`DefaultResponse=ECR_Block`으로 만들면 안 된다)도 Lyra가 그대로 증언한다** — Lyra의 5개 커스텀 채널이 전부 `ECR_Ignore`다(`:216-220`).

`bTraceType=True`의 실제 효과도 확인했다.

```cpp
// CollisionProfile.cpp:429-438
if (CustomChannel.bTraceType)
{
    Enum->SetMetaData(*TraceType, *TraceValue, EnumIndex);          // 에디터 드롭다운에서 Trace Channel로 뜬다
    FCollisionQueryFlag::Get().RemoveFromAllObjectsQueryFlag(CustomChannel.Channel);  // "모든 오브젝트" 쿼리에서 빠진다
    TraceTypeMapping.Add(CustomChannel.Channel);
}
```

우리는 `LineTraceSingleByChannel`(트레이스 쿼리)을 쓰므로 `True`가 맞고, `bStaticObject`는 오브젝트 타입에서만 쓰이므로(`:448-452`) `False`가 맞다.

**판정: 3-1은 확정. 문서 수정 유지.**

---

### 3-2. ★ **인과는 반만 성립한다. 그리고 예측한 증상이 틀렸다**

이게 이번 요청에서 실제로 위험한 항목이다.

#### 결론부터

| 경우 | `BeginPlay` 시점의 `IsLocallyControlled()` | 문서 예측 |
|---|---|---|
| **PIE 리슨서버 호스트, 첫 스폰** | ✅ **true** — `Possess`가 먼저다 | ❌ 문서는 false라고 했다 |
| 서버가 보는 원격 클라 폰 | false — `BeginPlay`가 먼저 | (어차피 로컬 아님. 무해) |
| 소유 클라의 복제 폰 | 보통 true — `OnRep_Controller`가 먼저 | 문서 미언급 |
| **리스폰 (호스트 포함)** | ❌ **false** — `BeginPlay`가 먼저 | ✅ 여기서만 문서가 맞다 |

**즉 문서대로 "PIE 첫 테스트에서 바로 걸린다"고 믿고 테스트하면 프롬프트가 정상으로 뜬다.** 그러면 함정 7b가 과장이라고 판단하고 두 훅을 지우게 된다. 그리고 리스폰을 붙이는 순간 조용히 깨진다. **틀린 증상은 없는 함정보다 나쁘다.**

#### 왜 그런가 — 엔진 순서

문서가 인용한 `GameModeBase.cpp:1310 / 1313 / 1326 / 1379`은 **전부 사실이다.** 확인했다.

```cpp
// GameModeBase.cpp:1310   APawn* NewPawn = SpawnDefaultPawnFor(NewPlayer, StartSpot);
// GameModeBase.cpp:1313       NewPlayer->SetPawn(NewPawn);
// GameModeBase.cpp:1326   FinishRestartPlayer(NewPlayer, SpawnRotation);
// GameModeBase.cpp:1379       NewPlayer->Possess(NewPlayer->GetPawn());
```

**빠진 조건이 하나 있다.** `SpawnActor`가 `BeginPlay`를 즉시 부르는 것은 **월드가 이미 begun play일 때뿐이다.**

```cpp
// Actor.cpp:4482
bool bRunBeginPlay = !bDeferBeginPlayAndUpdateOverlaps && (BeginPlayCallDepth > 0 || World->HasBegunPlay());
```

그리고 `World->HasBegunPlay()`가 참이 되는 지점은 여기 하나다.

```cpp
// WorldSettings.cpp:353-369
void AWorldSettings::NotifyBeginPlay()
{
    UWorld* World = GetWorld();
    if (!World->GetBegunPlay())
    {
        World->OnWorldPreBeginPlay.Broadcast();
        for (FActorIterator It(World); It; ++It) { It->DispatchBeginPlay(true); }
        World->SetBegunPlay(true);                  // ← :367
    }
}
```

이제 **`AEPGameMode : public AGameMode`**(`EPGameMode.h:14`)이므로 매치 상태 기계를 탄다. PIE 리슨서버 호스트의 실제 순서는 이렇다.

```
GameInstance.cpp:537   LocalPlayer->SpawnPlayActor(...)
                         → Login → PostLogin → AGameMode::HandleStartingNewPlayer_Implementation (GameMode.cpp:526)
                             MatchState == EnteringMap → IsMatchInProgress() false, WaitingToStart도 아님
                             → 폰이 안 생긴다

GameInstance.cpp:565   PlayWorld->BeginPlay()
  World.cpp:6063         GameMode->StartPlay()
    GameMode.cpp:136       SetMatchState(WaitingToStart) → HandleMatchIsWaitingToStart()
    GameMode.cpp:158         if (!ReadyToStartMatch()) GetWorldSettings()->NotifyBeginPlay();
                             ★ NumPlayers==1 이고 AEPGameMode::MinPlayersToStart==1 (EPGameMode.h:36)
                               → ReadyToStartMatch() == true → NotifyBeginPlay를 건너뛴다
    GameMode.cpp:141       ReadyToStartMatch() → StartMatch() → SetMatchState(InProgress)
      GameMode.cpp:203       HandleMatchHasStarted()
      GameMode.cpp:208-215     RestartPlayer(호스트 PC)
                                 → SpawnDefaultPawnFor  ← World->HasBegunPlay() == false 이므로 BeginPlay 지연
                                 → SetPawn → FinishRestartPlayer → Possess → PossessedBy   ★ 여기서 Controller 세팅
      GameMode.cpp:221         GetWorldSettings()->NotifyBeginPlay()
                                 → 이제서야 폰의 BeginPlay()가 돈다. Controller는 이미 있다
```

**호스트의 첫 폰은 `PossessedBy` → `BeginPlay` 순서다.**

리스폰은 정반대다. 그때는 `World->HasBegunPlay()`가 이미 true라 `SpawnDefaultPawnFor` 안에서 `BeginPlay`가 즉시 돌고(`Actor.cpp:4482`), `Possess`는 `:1379`에서 나중에 온다. **문서의 인과가 성립하는 건 이 경우다.**

> 현재 프로젝트에는 리스폰 경로가 아직 없다(`RestartPlayer` 호출 0건). **그래서 지금은 어느 방식이든 안 걸린다.** 대응을 넣는 것은 옳지만, **"지금 안 걸린다"를 알고 넣어야** 나중에 테스트 결과와 문서가 안 싸운다.

#### `BeginPlay` 시점에 호스트 폰의 `Controller`가 채워질 다른 경로가 있는가 — 없다

`SpawnDefaultPawnFor` 내부는 `SpawnActor<APawn>` 한 줄이다(`GameModeBase.cpp:1240-1246`). `PostActorConstruction`은 `Controller`를 건드리지 않는다. `AController::SetPawn`(`Controller.cpp:526-535`)은 `Controller->Pawn`만 세팅하고 `Pawn->Controller`는 `APawn::PossessedBy`의 `SetController(NewController)`(`Pawn.cpp:655`)에서만 채워진다. **문서의 이 부분은 정확하다.**

#### 소유 클라 쪽 순서 — `OnRep_Controller`가 먼저다. **다만 보장은 아니다**

```cpp
// DataChannel.cpp:3469-3486   먼저 모든 리플리케이터의 PostReceivedBunch() → 여기서 RepNotify가 돈다
ObjectReplicator->PostReceivedBunch();

// DataChannel.cpp:3495-3503
// After all properties have been initialized, call PostNetInit. This should call BeginPlay() ...
if (Actor && bSpawnedNewActor)
{
    Actor->PostNetInit();                       // → Actor.cpp:4638-4654 → DispatchBeginPlay()
}
```

`APawn::Controller`는 `DOREPLIFETIME(APawn, Controller)`(`Pawn.cpp:1282`)로 조건 없이 복제된다. 초기 번치에 실려 오면 `OnRep_Controller`가 `BeginPlay`보다 **먼저** 돈다.

**그런데 보장이 아니다.** `Controller`는 오브젝트 참조라서 클라가 그 `APlayerController`의 NetGUID를 아직 모르면 **unmapped**로 남고, `OnRep_Controller`는 나중 프레임에 뒤늦게 돈다(`DataChannel.cpp:3458-3462`, `UnmappedReplicators`). 그러면 `BeginPlay`가 먼저다.

**→ 그래서 "순서에 기대는 설계"가 애초에 틀렸고, 이벤트 훅으로 가는 문서의 방향이 옳다.** 이유만 바꾸면 된다.

#### ★ 두 훅 대신 **`NotifyControllerChanged()` 하나**를 쓴다

문서는 `PossessedBy` + `OnRep_Controller` 두 곳을 오버라이드한다. **`UnPossessed`가 빠져 있다.** 그리고 엔진에 정확히 이 셋을 묶는 가상 함수가 있다.

```cpp
// Pawn.h:382
/** Call to notify about a change in controller, on both the server and owning client. This calls the above event and delegate */
ENGINE_API virtual void NotifyControllerChanged();
```

호출 지점이 셋이다.

```cpp
// Pawn.cpp:685-689   PossessedBy 안 — 서버
if (OldController != NewController) { ReceivePossessed(GetController()); NotifyControllerChanged(); }

// Pawn.cpp:620-623   OnRep_Controller 안 — 소유 클라
if (bNotifyControllerChange) { NotifyControllerChanged(); }

// Pawn.cpp:714       UnPossessed 안 — 서버
NotifyControllerChanged();
```

그리고 클라 쪽 게이트의 기본값이 우리에게 유리하다.

```cpp
// Controller.cpp:34
bool bAlwaysNotifyClientOnControllerChange = true;     // 기본 켜짐

// Pawn.cpp:597-598
bool bNotifyControllerChange = UE::Gameplay::CVars::bAlwaysNotifyClientOnControllerChange ?
    (ThisController != PreviousController) :   // 기본: PreviousController와 다르면 언제나 알린다
    (ThisController == nullptr);
```

**제안:**

```cpp
// EPCharacter.h — PossessedBy / OnRep_Controller 옆
virtual void NotifyControllerChanged() override;

// EPCharacter.cpp
void AEPCharacter::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();
    // 서버 빙의 / 소유 클라 복제 / 언빙의 세 경우가 전부 여기로 온다 (Pawn.cpp:688, :622, :714)
    if (InteractionComponent) { InteractionComponent->RefreshTickEnabled(); }
}
```

| | 문서(두 훅) | `NotifyControllerChanged` |
|---|---|---|
| 서버 빙의 | ✅ | ✅ |
| 소유 클라 | ✅ | ✅ |
| **언빙의** | ❌ 틱이 켜진 채 남는다 | ✅ |
| 오버라이드 수 | 2 | 1 |
| 기존 오버라이드 건드림 | 두 함수에 한 줄씩 추가 | 안 건드림 |

> **언빙의 누수의 실제 크기는 작다.** `UpdateFocus()` 선두의 `IsLocallyControlled()` 가드가 잡아서 오동작은 없고, 0.1초마다 빈 호출이 도는 정도다. 다만 문서가 그 가드를 *"언빙의 후 한 틱의 안전장치"*라고 적어 놨는데, 두 훅 방식에서는 **한 틱이 아니라 영구**다. 그 문장도 같이 고쳐야 한다.

#### `bStartWithTickEnabled = false` + 훅이 충분한가

✅ 충분하다. 단 조건이 하나 있다 — **`RefreshTickEnabled()`가 컴포넌트 등록 이후에 불려야 한다.** 등록은 `PostActorConstruction`의 `InitializeComponents`(`Actor.cpp:4421`)에서 끝나고, `PossessedBy`/`OnRep_Controller`/`UnPossessed`는 전부 그 이후다. 문제없다.

**판정: 3-2의 대응(훅에서 판정)은 유지. 근거 문장과 증상 예측은 통째로 교체. 훅은 `NotifyControllerChanged()` 하나로.**

---

### 3-3. 8단계 → 5단계 축소 — ✅ 맞다

#### 동시성 주장의 정확도

> *"RPC는 게임 스레드 순차 처리이고 `CanInteract`→`OnInteract` 사이에 양보 지점이 없으므로 단계를 나눠도 합쳐도 안전하다"*

**정확하다.** 수신 RPC는 `UNetConnection`의 번치 처리 → `UActorChannel::ProcessBunch` → `ProcessEvent` 경로로 **게임 스레드에서** 실행되고, 이 경로는 `UNetDriver::TickDispatch` 안이다. 두 클라의 요청은 같은 프레임에 들어와도 **차례로** 실행된다. `CanInteract`와 `OnInteract` 사이에는 `co_await`도, 타이머도, 지연 호출도 없다.

한 가지만 문장에 덧붙이는 게 좋다: **이 보장은 "`OnInteract`이 동기적으로 끝날 때"만이다.** §7-1 컨테이너가 *"내용물 UI를 연다"*로 구현되면 `OnInteract`은 즉시 반환하고 실제 트랜잭션은 여러 프레임 뒤에 끝난다. **그때는 `bClaimed`가 아니라 "누가 열어 두었는가"라는 다른 상태가 필요하다.** 지금 결정할 일은 아니지만 문서에 한 줄 남겨야 나중에 같은 함정을 두 번 안 판다.

#### §2가 B가 되면 예측이 끼어드는가

**우리 설계에서는 안 끼어든다.** §2-5에서 클라 실행부를 통째로 비웠다.

```cpp
if (!ActorInfo->IsNetAuthority()) { EndAbility(...); return; }
```

**방어를 어디에 두는가에 대한 답: `UEPGA_Interact::ActivateAbility`의 첫 줄 하나다.** `AEPPickup::OnInteract` 안에 `HasAuthority()` 가드를 또 넣지 않는다 — 그러면 방어가 두 곳으로 흩어지고, `IEPInteractable` 구현체가 넷으로 늘면 넷 다 넣어야 한다. **인터페이스 주석의 `// ★ 서버에서만 호출된다`가 계약이고, 그 계약을 지키는 곳은 호출자 하나다.**

**판정: 3-3 유지. 두 문장 추가(비동기 `OnInteract` 경고 / 예측 방어 지점).**

---

### 3-4. `OnInteract`을 `bool` + `FText&`로 — ✅ 유지. 단 경고 하나

#### 반환 계약이 이 모양이 맞는가

맞다. 근거가 세 개다.

1. **구현체가 넷으로 예고돼 있다**(02-1 표). `void` → `bool` 전환 비용은 나중에 4배가 된다. CLAUDE.md §2의 *"나중에 넣기 비싼 것(계약·반환 규약)은 지금 넣는다"*가 정확히 이 경우다. 이건 5차·6차에서도 같은 논리로 통과한 종류다.
2. `enum` 결과 코드는 **소비자가 하나(HUD 텍스트)뿐이라** 값어치가 없다. 분기해서 다르게 처리할 곳이 없으면 enum은 텍스트 매핑 테이블만 늘린다.
3. `FGameplayTag`는 §2가 B여도 **지금은 아니다.** 태그 → 텍스트 매핑 테이블이 필요하고, 그 테이블은 어느 문서에도 이름이 없다.

#### ★ 그런데 `FText`를 RPC 파라미터로 보내는 비용을 알고 있어야 한다

`FTextProperty`에는 `NetSerializeItem` 오버라이드가 **없다.** 그래서 기본 구현으로 떨어져 `SerializeItem`을 그대로 탄다 — **네임스페이스 + 키 + 소스 문자열이 통째로 나간다.** 반면 `FGameplayTag`는 패킹된 net index로 나간다.

```cpp
// GameplayTagContainer.cpp:1286-1299
bool FGameplayTag::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
    NetSerialize_Packed(Ar, Map, bOutSuccess);   // 태그 테이블 인덱스로 압축
    ...
}
```

**실무 판단: 그래도 `FText`로 간다.** 실패 회신은 실패 1회당 1번이고 빈도가 낮다(초당 수십 번 나가는 것이 아니다). 수십~수백 바이트를 실패할 때만 낸다. **다만 이걸 모르고 나중에 "프롬프트 텍스트도 서버에서 보내자"로 번지면 그때는 비싸진다.** 문서에 한 줄 박아 둔다.

#### `CanInteract`와 `OnInteract`이 둘 다 `OutReason`을 갖는 게 중복인가

**아니다. 답하는 질문이 다르다.**

| | `CanInteract` | `OnInteract` |
|---|---|---|
| 언제 | 매 0.1초, **클라에서도** | 서버에서 1회 |
| 무엇 | *"지금 누르면 되는가"* | *"눌러서 실행했더니 어떻게 됐는가"* |
| 사유의 소비자 | HUD 회색 프롬프트(로컬) | `Client_OnInteractFailed`(RPC) |
| 예 | "이미 획득됨", "돈 부족" | "다른 사람이 방금 열었다", "가방에 자리 없음" |

**`OnInteract`의 실패 집합은 `CanInteract`가 알 수 없는 것들이다** — 정의상 "검사 시점 이후에 바뀐 것" 또는 "실행해 봐야 아는 것". 합칠 수 없다.

**판정: 3-4 유지. `FText` RPC 비용 한 줄 추가.**

---

### 3-5. `SetIsReplicatedByDefault(false)` → `true`, RPC 선언 위치 분리

#### `false`여도 RPC가 나간다는 진술 — ✅ **나간다. 그런데 이유가 다르다**

문서는 이렇게 적었다.

> 생성자 서브오브젝트는 `RF_DefaultSubObject`를 갖고, `UObject::IsNameStableForNetworking`(`Obj.cpp:5945`)이 그 플래그를 보므로 이름으로 해석된다.

그리고 요청 §3-5는 정확히 옳은 지점을 의심했다 — *"동적 스폰 액터의 서브오브젝트라 `IsFullNameStableForNetworking`은 false일 텐데"*.

**요청의 의심이 맞다. 그리고 그래도 RPC는 나간다 — 다른 경로로.**

1. 호출 자체는 오너에게 위임된다. 컴포넌트의 복제 플래그를 **보지 않는다.**

```cpp
// ActorComponent.cpp:1210-1220
int32 UActorComponent::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
    if ((Function->FunctionFlags & FUNC_Static)) { return GEngine->GetGlobalFunctionCallspace(...); }
    AActor* MyOwner = GetOwner();
    return (MyOwner ? MyOwner->GetFunctionCallspace(Function, Stack) : FunctionCallspace::Local);
}
```

2. 패키지맵이 이 컴포넌트를 지원 대상으로 인정한다.

```cpp
// ActorComponent.cpp:2924
bool UActorComponent::IsSupportedForNetworking() const { return GetIsReplicated() || IsNameStableForNetworking(); }

// ActorComponent.cpp:2919
return bNetAddressable || (Super::IsNameStableForNetworking() && (CreationMethod != EComponentCreationMethod::UserConstructionScript));

// Obj.cpp:5941-5946
bool UObject::IsNameStableForNetworking() const
{ return HasAnyFlags(RF_WasLoaded | RF_DefaultSubObject | RF_ClassDefaultObject) || IsNative() || IsDefaultSubobject(); }
```

`GetIsReplicated()`가 false여도 `RF_DefaultSubObject` 덕에 두 번째 항이 true → `IsSupportedForNetworking()` true → `FNetGUIDCache::SupportsObject`(`PackageMapClient.cpp:3159-3163`) 통과.

3. **여기서 문서와 갈린다.** `IsFullNameStableForNetworking()`은 아우터(동적 스폰 폰)가 불안정해서 **false**다(`Obj.cpp:5949-5957`). 그래서 이 컴포넌트는 **동적 오브젝트**로 분류되고(`PackageMapClient.cpp:3184-3191`), 클라는 NetGUID를 발급할 권한이 없다.

```cpp
// PackageMapClient.cpp:3248-3255
if (!bIsNetGUIDAuthority)
{
    // We cannot make or assign new NetGUIDs
    // Generate a default GUID, which signifies we write the full path
    return FNetworkGUID::GetDefault();
}
```

4. **Default GUID면 "경로를 쓴다"는 뜻이고**, 아우터 GUID(폰 — 이건 서버가 발급해 클라가 알고 있다)와 **컴포넌트 이름**을 같이 보낸다.

```cpp
// PackageMapClient.cpp:925-932
if (NetGUID.IsDefault())
{
    check(!IsNetGUIDAuthority());       // 오직 클라만 default guid를 보낸다
    ExportFlags.bHasPath = 1;
    Ar << ExportFlags.Value;
}
// :959-960
ObjectPathName = Object->GetName();     // "InteractionComponent"
ObjectOuter    = Object->GetOuter();    // 폰
// :974-976
FNetworkGUID OuterNetGUID = GuidCache->GetOrAssignNetGUID(ObjectOuter);
InternalWriteObject(Ar, OuterNetGUID, ObjectOuter, TEXT(""), nullptr);
```

5. 서버는 그 이름으로 찾는다.

```cpp
// PackageMapClient.cpp:1211-1240
if (NetGUID.IsDefault())
{
    check(IsNetGUIDAuthority());
    Object = StaticFindObject(UObject::StaticClass(), ObjOuter, *ObjectName, EFindObjectFlags::None);
    if (Object == NULL) { UE_LOG(..., Warning, TEXT("Unable to resolve default guid from client: ...")); return NetGUID; }
    ...
    NetGUID = GuidCache->GetOrAssignNetGUID(Object);   // 서버가 정식 GUID 발급
    HandleUnAssignedObject(Object);                    // 클라에 다시 익스포트 → 이후엔 압축된 GUID
}
```

**결론: 문서의 결론(“`false`여도 RPC는 나간다”)은 맞고, 근거 문장은 부정확하다.** 정확히는 —

> `IsSupportedForNetworking()`이 `RF_DefaultSubObject` 덕에 true라 참조는 가능하지만, `IsFullNameStableForNetworking()`은 아우터가 동적이라 false다. 그래서 클라는 **Default GUID + 컴포넌트 이름 + 아우터 GUID**를 보내고, 서버가 `StaticFindObject`로 해석한 뒤 정식 NetGUID를 발급해 되돌려 준다. **첫 호출만 문자열 경로 비용을 내고 이후엔 압축된다.**

그리고 실패했을 때가 조용하지 않다. 서버가 서브오브젝트를 못 찾으면:

```cpp
// DataChannel.cpp:4858-4869
if (IsServer)
{
    if (!SubObj)
    {
        UE_LOG(LogNetTraffic, Error, TEXT("ReadContentBlockHeader: Client attempted to create sub-object. Actor: %s"), *Actor->GetName());
        Bunch.SetError();
        AddToChainResultPtr(Bunch.ExtendedError, ENetCloseResult::ContentBlockHeaderInvalidCreate);
        return nullptr;
    }
    return SubObj;
}
```

**RPC가 무시되는 게 아니라 번치가 에러가 되고 연결이 닫힌다.** 그래서 이 경로를 "된다더라"로 넘기면 안 된다는 문서의 정서 자체는 옳다.

#### 그러면 `true`가 맞는가 — ✅ 맞다. 단 "비용 0"은 정확히 0은 아니다

`true`로 하면 컴포넌트가 액터의 복제 서브오브젝트 목록에 들어가고, RPC 첫 발신 시점에 리플리케이터가 만들어진다.

```cpp
// NetDriver.cpp:3245
Ch->PrepareForRemoteFunction(TargetObj);

// DataChannel.cpp:4552-4560
void UActorChannel::PrepareForRemoteFunction(UObject* TargetObj)
{
    // Make sure we create a replicator in case we destroy a sub object before we ever try to replicate its properties, ...
    if (Connection && Connection->Driver && Connection->Driver->IsServer()) { FindOrCreateReplicator(TargetObj); }
}
```

**정확한 비용:** 커넥션당 `FObjectReplicator` 1개(메모리 수십 바이트) + 컨텐츠 블록 헤더 1회. 복제 프로퍼티가 0개라 **대역폭 반복 비용은 진짜로 0이다.** 문서의 *"비용은 0이다"*는 실질적으로 맞지만, 엄밀히는 *"반복 비용이 0이다"*로 쓰는 게 낫다.

**그리고 §2가 B로 가면 이 항목 전체가 사라진다.** `Server_Interact`가 없어지므로 `SetIsReplicatedByDefault`를 굳이 `true`로 할 이유가 없어진다 — 다만 **`UEPCombatComponent`와 관례를 맞춘다**는 두 번째 근거는 남으므로 `true`로 둬도 무방하다. 결정할 필요가 없어진다는 뜻이다.

#### RPC를 두 클래스에 나눠 두는 게 맞는가

**§2가 B면 나눌 것도 없다.** 서버 방향은 GAS가 가져가고, 클라 방향(`Client_OnInteractFailed`)만 남아서 `AEPPlayerController`에 들어간다 — `Client_OnKill`·`Client_PlayHitConfirmSound`(`EPPlayerController.h:40-45`) 옆이고, `HUDWidget`이 거기 `private`이므로 **다른 선택지가 없다.**

A로 남길 경우에도 문서의 배치가 맞다. *"한 흐름의 요청과 회신이 다른 파일에 있다"*는 지적은 사실이지만, 대안 둘 다 더 나쁘다 —

- 컴포넌트에 몰기: `Client_OnInteractFailed`가 `HUDWidget`(private)에 못 닿는다. PC를 거치는 함수를 또 만들어야 한다
- PC에 몰기: `InteractRange`·`ServerRangeTolerance`가 컴포넌트에 있어서 판정할 때마다 컴포넌트를 가져와야 한다

**"요청은 값이 있는 곳에, 회신은 표시가 있는 곳에"** — 이 문장을 문서에 적으면 다음 시스템에서 안 갈린다.

**판정: 3-5의 결론 전부 유지. 근거 문장 하나(§3-5의 GUID 설명) 교체. §2가 B면 앞 두 항목은 무의미해진다 — 그렇다고 명시.**

---

## 4. §4 — 미결 2건

### 4-1. 라인 트레이스 vs 스피어 스윕 — **라인 유지**

**Lyra가 라인이다.** 포커스 탐지는 단일 라인 트레이스다.

```cpp
// AbilityTask_WaitForInteractableTargets.cpp:19-33
void UAbilityTask_WaitForInteractableTargets::LineTrace(FHitResult& OutHitResult, const UWorld* World,
    const FVector& Start, const FVector& End, FName ProfileName, const FCollisionQueryParams Params)
{
    TArray<FHitResult> HitResults;
    World->LineTraceMultiByProfile(HitResults, Start, End, ProfileName, Params);
    ...
    if (HitResults.Num() > 0) { OutHitResult = HitResults[0]; }
}
```

Lyra의 기본값도 참고가 된다.

```cpp
// LyraGameplayAbility_Interact.h:48-52
UPROPERTY(EditDefaultsOnly) float InteractionScanRate  = 0.1f;      // 우리 TickInterval과 동일
UPROPERTY(EditDefaultsOnly) float InteractionScanRange = 500;       // 부여용 구체 오버랩 반경

// AbilityTask_WaitForInteractableTargets_SingleLineTrace.h:38-39
float InteractionScanRange = 100;                                    // 라인 트레이스 기본 사거리
```

**정리:** Lyra는 **구체 오버랩(넓게, 어빌리티 부여용)과 라인 트레이스(좁게, 포커스 결정용)를 분리**한다. 우리는 부여를 안 하므로 라인만 남는다. 그리고 `InteractRange = 250`은 Lyra의 라인 기본값 100과 스캔 500 사이라 합리적이다.

**스윕을 안 쓰는 이유 — 요청이 스스로 짚은 그것이 실무의 이유다.** 스윕의 시작점이 지오메트리 안이면 결과가 정의되지 않는다(`bStartPenetrating`). 벽에 붙는 상황은 상호작용에서 흔하다(상자가 벽 앞에 있다). 그리고 **2.5m/20cm에서 시야각 4.6°면 라인으로 충분하다는 요청의 계산이 맞다.**

> 나중에 조준 관용이 필요하면 스윕이 아니라 **Lyra식 2단 구조**로 가는 게 안전하다 — 넓은 오버랩으로 후보를 모으고 그중 시야 중심에 가장 가까운 것을 고른다. 시작점 침투 문제가 안 생긴다. **지금 만들지는 않는다.**

### 4-2. ★ 상호작용 대상이 늘 때 채널을 어디서 여는가 — **콜리전 프리셋**

**Lyra의 답이 명확하다. 프리셋이다.**

```ini
; Lyra/Config/DefaultEngine.ini:213
+Profiles=(Name="Interactable_OverlapDynamic",CollisionEnabled=QueryOnly,bCanModify=True,ObjectTypeName="PhysicsBody",
  CustomResponses=((Channel="Pawn",Response=ECR_Overlap),(Channel="Visibility",Response=ECR_Ignore),
  (Channel="Camera",Response=ECR_Ignore),(Channel="PhysicsBody",Response=ECR_Ignore),(Channel="Vehicle",Response=ECR_Ignore),
  (Channel="Destructible",Response=ECR_Ignore),(Channel="Lyra_TraceChannel_Interaction",Response=ECR_Overlap),
  (Channel="Lyra_TraceChannel_Weapon_Multi",Response=ECR_Ignore)),HelpMessage="")
```

Lyra는 `LyraPawnMesh`·`LyraPawnCapsule`(`:211-212`)도 같은 방식으로 만들어 뒀다. **액터마다 `SetCollisionResponseToChannel` 한 줄씩도 아니고, 공통 C++ 베이스도 아니다.**

**요청의 판단이 맞다 — 지금 정하지 않으면 세 번 반복해서 잊는 종류다.** 그리고 CLAUDE.md §2 기준으로도 통과한다:

- *"공통 베이스"*는 문서에 이름이 없다 → **만들지 않는다** ✅ (요청의 자기 판단이 옳다)
- *"콜리전 프리셋"*은 **클래스가 아니라 설정이다.** 계층을 하나도 안 늘린다. `.ini` 한 줄과 컴포넌트 디테일 패널의 드롭다운 하나

**다만 Step 02에서 프리셋을 지금 만들 필요는 없다.** 소비자가 하나(`AEPPickup`)뿐이라 `SetCollisionResponseToChannel` 한 줄이 더 싸다. **정해야 하는 것은 "두 번째 소비자가 생기면 프리셋으로 간다"는 문장이다.**

```
> **§7-1 컨테이너 / §7-2 자판기 / 로드맵 12 탈출 지점이 들어올 때** — 이때가 두 번째 소비자다.
> 그 시점에 `.ini`에 `EP_Interactable` 콜리전 프리셋을 만들고 세 액터가 그것을 쓴다.
> Lyra가 `Interactable_OverlapDynamic`(`DefaultEngine.ini:213`)으로 하는 것과 같다.
> **액터마다 `SetCollisionResponseToChannel` 줄을 늘리지 않는다** — 그러면 네 번째 액터에서 반드시 하나 빠뜨린다.
> Step 02 시점에는 소비자가 `AEPPickup` 하나라 코드 한 줄이 더 싸다.
```

> **Lyra는 `Block`이 아니라 `Overlap`을 쓴다는 점도 기록해 둔다.** `LineTraceMulti` + `HitResults[0]`이라 오버랩도 결과에 들어온다. 우리는 `LineTraceSingleByChannel`이라 **`Block`이어야 맞다.** 프리셋을 만들 때 이 둘을 섞으면 아무것도 안 잡힌다.

---

## 5. §5 실무 조사 — Lyra `Interaction/` 전체 구조

전부 직독이다. 파일 17개, 1091줄.

### 5-1. 구조

```
Interaction/
├─ IInteractableTarget.h            상호작용 가능 대상. 함수 2개
├─ IInteractionInstigator.h         옵션이 여러 개일 때 고르는 중재자. 함수 1개
├─ InteractionOption.h              ★ 핵심 구조체 — "이 대상으로 무엇을 할 수 있는가" 한 항목
├─ InteractionQuery.h               요청자 정보 (아바타 / 컨트롤러 / 임의 UObject)
├─ InteractionStatics.*             액터/컴포넌트에서 IInteractableTarget 뽑아내는 헬퍼
├─ Abilities/
│  ├─ LyraGameplayAbility_Interact.*        ★ 상시 켜져 있는 "스캐너" 어빌리티
│  └─ GameplayAbilityTargetActor_Interact.* 라인 트레이스 타깃 액터 (TargetData 경로용)
└─ Tasks/
   ├─ AbilityTask_GrantNearbyInteraction.*        서버: 근처 대상의 어빌리티를 부여
   ├─ AbilityTask_WaitForInteractableTargets.*    공통: 옵션 갱신 + 조준 보정
   └─ AbilityTask_..._SingleLineTrace.*           로컬: 0.1초 라인 트레이스
```

### 5-2. 어떻게 맞물리는가

```
[상시] ULyraGameplayAbility_Interact
       ActivationPolicy = OnSpawn / InstancedPerActor / LocalPredicted   (LyraGameplayAbility_Interact.cpp:24-26)
       │
       ├─(서버만) UAbilityTask_GrantNearbyInteraction                     (:34-38 — ROLE_Authority 게이트)
       │     0.1초 타이머 → OverlapMultiByChannel(반경 500, Lyra_TraceChannel_Interaction)
       │     → 걸린 대상들의 GatherInteractionOptions() 호출
       │     → Option.InteractionAbilityToGrant 를 ASC->GiveAbility()   (GrantNearbyInteraction.cpp:80-90)
       │        ※ InteractionAbilityCache 로 클래스당 1회만. 회수는 안 한다
       │
       └─(BP 그래프) UAbilityTask_WaitForInteractableTargets_SingleLineTrace
             0.1초 타이머 → 라인 트레이스 → GatherInteractionOptions()
             → UpdateInteractableOptions() 에서 CanActivateAbility()로 필터   (WaitForInteractableTargets.cpp:143-148)
             → 바뀌었으면 InteractableObjectsChanged 브로드캐스트
             → UpdateInteractions() → 인디케이터 위젯 갱신                    (Interact.cpp:41-76)

[F키] (별도 BP 어빌리티) → ULyraGameplayAbility_Interact::TriggerInteraction()  (:78-122)
       CurrentOptions[0] 을 꺼내
       FGameplayEventData Payload { EventTag=Ability.Interaction.Activate, Instigator=아바타, Target=대상액터 }
       → InteractableTarget->CustomizeInteractionEventData(...)   대상이 페이로드를 고칠 기회
       → Option.TargetAbilitySystem->TriggerAbilityFromGameplayEvent(Option.TargetInteractionAbilityHandle, ...)
```

### 5-3. §2-4 ②에 대한 Lyra의 실제 답 — **대상을 네트워크로 안 보낸다**

`TriggerInteraction()`은 `TriggerAbilityFromGameplayEvent`를 부르는데, 이건 **로컬 호출**이다. 그리고 스캐너 어빌리티가 `LocalPredicted`라 **서버에도 인스턴스가 있고, 서버도 자기 라인 트레이스를 돌려 자기 `CurrentOptions`를 갖고 있다.**

**즉 Lyra는 요청 §2-3이 걱정한 "서버 재트레이스" 쪽을 택했다.** 그래서 서버가 8인분 라인 트레이스를 0.1초마다 돌린다(초당 80회 — 실측상 무시 가능).

**그리고 Lyra는 대안도 같이 갖고 있다.** `AGameplayAbilityTargetActor_Interact`(`:16-56`)는 `AGameplayAbilityTargetActor_Trace` 파생이고, `WaitTargetData` 태스크와 짝지어 쓰면 **클라 조준 결과가 `FGameplayAbilityTargetDataHandle`로 서버에 복제된다.** 레티클·확인 입력이 필요한 상호작용용이다.

**우리는 셋째 길(§2-2)을 쓴다.** `HandleGameplayEvent(&Payload)`로 어빌리티를 띄우면 `Payload.Target`이 **엔진 RPC 파라미터로 그대로 서버에 간다.** Lyra가 이 길을 안 쓴 이유는 명확하다 — Lyra의 F키는 *"대상의 ASC에 있는 대상별 어빌리티를 실행"*이라 스캐너 어빌리티가 이미 떠 있는 상태에서 **활성화가 아니라 트리거**를 하기 때문이다. 우리는 F키가 곧 어빌리티 활성화라 파라미터가 자연스럽게 실린다.

### 5-4. §2-4 ④에 대한 Lyra의 답 — **둘 다**

| | 무엇 | 언제 |
|---|---|---|
| 스캐너(`ULyraGameplayAbility_Interact`) | 상시 | `ActivationPolicy = OnSpawn` |
| 대상별 실행 어빌리티 | **동적** | 서버가 반경 500 안을 0.1초마다 훑어 `GiveAbility` |

동적 부여의 대가도 소스에 그대로 보인다 — **회수 코드가 없다.** `InteractionAbilityCache`(`GrantNearbyInteraction.cpp:83-89`)는 한 번 부여한 클래스를 기억할 뿐 멀어져도 안 뺀다. 세션이 길어지면 ASC에 어빌리티가 쌓인다. Lyra가 그걸 감수한 이유는 **어빌리티 종류가 유한하기 때문**이다.

**우리 규모에서는 이 전부가 불필요하다.** 어빌리티가 하나라 `DefaultAbilities`(`EPCharacter.h:61`)에 넣으면 끝이다.

### 5-5. §5-2 — 이 구조가 우리 규모에 과한가: **과하다**

| Lyra 요소 | 존재 이유 | 우리에게 |
|---|---|---|
| `FInteractionOption` | 대상마다 어빌리티/위젯이 다르다 | 어빌리티 하나 → **필드 0개짜리 구조체** |
| `GatherInteractionOptions()` | 한 대상이 옵션 여러 개를 낼 수 있다 | 옵션 1개 고정 → `CanInteract` + `GetInteractText`로 충분 |
| `IInteractionInstigator` | 옵션이 여럿일 때 메뉴로 고른다 | 호출될 일이 없다 |
| `GrantNearbyInteraction` | 대상별 어빌리티를 미리 부여해야 한다 | 부여할 게 없다 → **빈 루프** |
| `CustomizeInteractionEventData` | 벽 스위치가 문 액터를 타깃으로 바꾼다 | §7에 그런 예가 없다 |
| `IndicatorManager` 연동 | 3D 월드 인디케이터 시스템이 따로 있다 | 우리는 HUD 텍스트 한 줄 |
| **`Lyra_TraceChannel_Interaction` 채널 설정** | — | ✅ **그대로 쓴다** (§3-1) |
| **콜리전 프리셋으로 대상 지정** | — | ✅ **두 번째 소비자부터** (§4-2) |
| **`FGameplayEventData`로 상호작용 표현** | — | ✅ **그대로 쓴다** (§2-2) |

**6차에서 `DA_EPGameData`를 기각한 논리와 정확히 같은 결과다.** 다만 §2에서 B를 택하는 것은 이것과 별개다 — **B는 Lyra의 구조를 가져오는 게 아니라 GAS 자체를 쓰는 것**이고, GAS는 이 프로젝트에 이미 있다.

### 5-6. §5-3 — GAS 프로젝트에서 "그냥 서버 RPC"를 남기는가

**직접 인용할 수 있는 근거는 Lyra 하나뿐이고, Lyra는 안 남긴다.** 인터넷 조사는 하지 않았으므로 "실무에서 흔한가"에 대해 통계로 답하지 않는다. 대신 **소스에서 읽어낼 수 있는 판정 기준**을 정리한다.

| 이걸 물어라 | 어빌리티로 | 그냥 RPC로 |
|---|---|---|
| 상태 태그로 막아야 하는가? (`State.Dead`, `State.Reloading`) | ✅ | |
| 쿨다운/코스트가 있는가, 또는 **곧 생기는가**? | ✅ | |
| 예측이 필요한가? | ✅ | |
| 애님 몽타주가 붙는가? | ✅ | |
| 게임플레이 이펙트를 적용하는가? | ✅ | |
| **위 다섯 개가 전부 아니고 앞으로도 아닌가?** | | ✅ |

우리 상호작용은 1번이 지금 참이고(`ActivationBlockedTags`에 `State.Dead`가 이미 세 곳에 있다), 2번이 Step 03에서 참이 되고(`DropCooldown`), 4번이 §7에서 참이 된다(채널링). **여섯 개 중 셋이 이미 잡혀 있다.**

### 5-7. §5-4 — `BeginPlay` / `PossessedBy` 순서 확정

§3-2에 전부 적었다. 요약만:

| | 순서 | 근거 |
|---|---|---|
| **서버 — 월드 begun play 전 스폰**(PIE 호스트 첫 스폰) | `PossessedBy` → `BeginPlay` | `Actor.cpp:4482` + `WorldSettings.cpp:367` + `GameMode.cpp:158, 208-221` |
| **서버 — 월드 begun play 후 스폰**(리스폰, 늦게 접속한 클라의 폰) | `BeginPlay` → `PossessedBy` | `Actor.cpp:4482` + `GameModeBase.cpp:1310 → 1379` |
| **소유 클라 — 복제 폰** | 보통 `OnRep_Controller` → `BeginPlay`, **보장 아님** | `DataChannel.cpp:3485` vs `:3503`, `Actor.cpp:4638-4654`, 참조 unmapped 시 역전 |
| **시뮬 프록시** | `Controller`가 null이거나 원격 → 어느 순서든 false | `Pawn.cpp:1282` |

---

## 6. 우선순위 작업 목록

### 지금 하는 것

| # | 무엇 | 왜 지금인가 |
|---|---|---|
| **1** | **§2를 B로 확정하고 `05_Loot_02_Interaction.md`를 고친다** | Step 02 코드가 0줄이다. 이 지점을 지나면 `IEPInteractable` 구현체가 넷으로 늘어난 뒤에 바꾸게 된다 |
| **2** | **3-2의 근거·증상 문단 교체 + `NotifyControllerChanged()` 단일 훅** | **틀린 증상이 가장 위험하다.** 지금 테스트하면 통과해서 함정 7b를 지우게 된다 |
| 3 | §4-2 프리셋 문장을 `05_Loot_DOCS.md` §7 옆에 한 줄 | 세 번 반복해서 잊을 종류라는 요청의 판단이 맞다 |
| 4 | 3-1 근거 보강 (중복 시 조용히 버려진다 + Lyra 문안 일치) | 판정은 이미 맞음. 근거만 |
| 5 | 3-5의 GUID 설명 문단 교체 | 결론은 맞고 메커니즘이 틀렸다. §2가 B면 이 문단 자체가 축소된다 |
| 6 | 3-3에 두 문장 (비동기 `OnInteract` 경고 / 예측 방어 지점) | |
| 7 | 3-4에 한 문장 (`FText` RPC 직렬화 비용) | |

### 지금 하지 않는 것

- ❌ `FInteractionOption` / `IInteractionInstigator` / `GatherInteractionOptions` — 소비자가 없다
- ❌ `AbilityTask_GrantNearbyInteraction` 방식의 동적 부여 — 부여할 어빌리티가 하나다
- ❌ `AGameplayAbilityTargetActor` / `WaitTargetData` — `FGameplayEventData::Target`으로 충분하다
- ❌ 콜리전 프리셋 실제 생성 — 두 번째 소비자가 생길 때
- ❌ 스피어 스윕 — Lyra도 라인이다
- ❌ 실패 사유의 `FGameplayTag`화 — 매핑 테이블이 문서에 없다

---

## 7. 제안 문안

> 아래는 **제안 문안**이다. 적용 여부는 사용자가 결정한다.

### 7-1. `05_Loot_02_Interaction.md` 02-2 — `Server_Interact` 절 전체를 대체

````markdown
### 왜 `Server_Interact`가 아니라 `GA_Interact`인가

이 프로젝트에서 **모든 게임플레이 입력은 어빌리티 태그로 간다**(`EPCharacter.cpp:388-435`). 예외가 없다.
그런데 그건 "서버 RPC를 안 쓴다"는 뜻이 아니다 — `TryActivateAbilitiesByTag` 한 줄이
`ServerTryActivateAbility`(`AbilitySystemComponent.h:1723`)를 부른다. **서버 RPC는 이미 쓰고 있다.**

`Server_Interact`를 직접 만들면 다음을 손으로 다시 만든다.

| GAS가 이미 주는 것 | 직접 RPC에서 다시 만들 것 |
|---|---|
| `ActivationBlockedTags`에 `State.Dead` (3개 어빌리티에 이미 있다) | 죽은 상태 확인 |
| GE 쿨다운 | Step 03 `DropCooldown` |
| `CastTime` + `State.Casting` + `WBP_CastGauge` (`EPGA_Skill_Base.h:31`) | 채널링 — 아래에서 GAS에 위임한다고 이미 적었다 |
| `NetSecurityPolicy` 검사 | 조작 클라 차단 |

**그리고 "즉시는 RPC / 채널링은 GAS"는 절충이 아니라 모순이다.**
경계가 `GetInteractDuration()`이라는 **데이터 값**에 달려 있어서, 자판기의 duration을 5→0으로 바꾸면
그 대상의 네트워크 경로가 통째로 바뀐다. §7-1 컨테이너가 들어오면 두 경로가 동시에 살아 있게 된다.

### 대상은 어떻게 서버에 전달하는가 — `FGameplayEventData::Target`

`FGameplayEventData`에 `Target`이 있고(`GameplayAbilityTypes.h:256`),
이 구조체가 **통째로 서버 RPC 파라미터다**(`AbilitySystemComponent.h:1726`,
`AbilitySystemComponent_Abilities.cpp:1921-1923`).

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
    Payload.Target     = FocusedActor;      // ★ 대상 전달의 전부. TargetData도 서버 재트레이스도 필요 없다

    ASC->HandleGameplayEvent(EmpGameplayTags::TAG_Ability_Interact, &Payload);
}
```

`UEPGA_Interact`는 `AbilityTriggers`에 `GameplayEvent` / `TAG_Ability_Interact`를 하나 등록한다.
`NetExecutionPolicy = LocalPredicted` — **클라가 서버 RPC를 쏘려면 이 정책이어야 한다.**

> **`Payload.Target`은 복제 액터여야 한다.** 오브젝트 참조로 직렬화되므로 서버가 발급한 NetGUID가 필요하다.
> `AEPPickup`은 복제 액터라 문제없고, §7-1 컨테이너·§7-2 자판기도 그래야 한다.

### 예측은 하지 않는다

선점 경쟁(`bClaimed`)이 있어서 예측이 틀리면 픽업이 사라졌다 다시 나타난다.
**방어는 `ActivateAbility` 첫 줄 하나에 둔다.** `AEPPickup::OnInteract` 안에 또 넣지 않는다 —
그러면 `IEPInteractable` 구현체가 넷으로 늘 때 넷 다 넣어야 한다.

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

### 사거리 값은 컴포넌트에 남는다

`InteractRange`·`ServerRangeTolerance`는 `UEPInteractionComponent`에 두고 어빌리티가 게터로 읽는다.
어빌리티 CDO에 값을 복제해 두면 두 곳이 갈린다.

```cpp
const UEPInteractionComponent* IC = Interactor->GetInteractionComponent();
const float MaxDistSq = FMath::Square(IC->GetInteractRange() + IC->GetServerRangeTolerance());
```

### `Client_OnInteractFailed`는 그대로 남는다

**GAS는 실패 사유를 안 준다.** `ClientActivateAbilityFailed(Handle, PredictionKey)`(`AbilitySystemComponent.h:1747`)에는
사유 파라미터가 없고, `InternalTryActivateAbilityFailureTags`는 서버 로컬 변수다.

그래서 `AEPPlayerController`에 그대로 둔다 — `Client_OnKill`·`Client_PlayHitConfirmSound`(`EPPlayerController.h:40-45`)와
같은 줄이고, 실패 문구를 띄울 `HUDWidget`이 거기 `private`이다.

**원칙: 요청은 값이 있는 곳에, 회신은 표시가 있는 곳에.**

### `SetIsReplicatedByDefault`

`Server_Interact`가 없어졌으므로 **RPC 때문에 필요하지는 않다.**
`UEPCombatComponent`(`EPCombatComponent.cpp:34`)와 관례를 맞추려면 `true`, 아니어도 무방하다.
복제 프로퍼티가 0개라 반복 비용은 0이다(커넥션당 `FObjectReplicator` 1개 + 컨텐츠 블록 헤더 1회는 든다).
````

### 7-2. 같은 문서 — "★ 이 판정을 `BeginPlay`에 두면…" 절 전체를 대체

````markdown
### ★ 틱 on/off 판정을 `BeginPlay`에 두면 **리스폰에서** 조용히 깨진다

`IsLocallyControlled()`는 `Controller`를 보고, `Controller`는 `APawn::PossessedBy`의
`SetController()`(`Pawn.cpp:655`)에서만 채워진다. 그래서 `BeginPlay`와 `PossessedBy`의 **순서**가 문제다.

**그런데 그 순서가 상황마다 다르다.** `SpawnActor`가 `BeginPlay`를 즉시 부르는 것은
**월드가 이미 begun play일 때뿐**이기 때문이다.

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
  GameMode.cpp:158      HandleMatchIsWaitingToStart: if (!ReadyToStartMatch()) NotifyBeginPlay();
                          NumPlayers==1, MinPlayersToStart==1(EPGameMode.h:36) → ready == true
                          → ★ NotifyBeginPlay를 건너뛴다 (World->HasBegunPlay()는 아직 false)
  GameMode.cpp:141      StartMatch() → HandleMatchHasStarted()
  GameMode.cpp:208-215    RestartPlayer(호스트) → 폰 스폰(BeginPlay 지연) → Possess ★
  GameMode.cpp:221        NotifyBeginPlay() → 이제서야 폰의 BeginPlay. Controller는 이미 있다
```

리스폰은 `World->HasBegunPlay()`가 이미 true라 `SpawnDefaultPawnFor`(`GameModeBase.cpp:1310`) 안에서
`BeginPlay`가 즉시 돌고, `Possess`는 `:1379`에서 나중에 온다.

**즉 "첫 테스트에서 바로 걸린다"가 아니라, 첫 테스트는 통과하고 리스폰에서 깨진다.**
이게 더 나쁘다 — 완료 조건 6이 통과해서 함정이 없다고 판단하게 된다.

> 소유 클라 쪽 순서도 **보장이 아니다.** `OnRep_Controller`(RepNotify, `DataChannel.cpp:3485`)가
> `PostNetInit`→`BeginPlay`(`:3503`, `Actor.cpp:4638-4654`)보다 먼저지만,
> `Controller` 참조가 unmapped면 뒤로 밀린다. **순서에 기대는 설계 자체가 틀렸다.**

### ★ 훅은 하나면 된다 — `NotifyControllerChanged()`

엔진에 서버 빙의 / 소유 클라 복제 / **언빙의** 셋을 묶는 가상 함수가 있다.

```cpp
// Pawn.h:382
/** Call to notify about a change in controller, on both the server and owning client. ... */
ENGINE_API virtual void NotifyControllerChanged();
```

호출 지점: `PossessedBy`(`Pawn.cpp:688`) / `OnRep_Controller`(`:622`) / `UnPossessed`(`:714`).
클라 쪽 게이트 기본값도 켜져 있다(`Controller.cpp:34` `bAlwaysNotifyClientOnControllerChange = true`).

```cpp
// EPCharacter.h — PossessedBy / OnRep_Controller 옆
virtual void NotifyControllerChanged() override;

// EPCharacter.cpp
void AEPCharacter::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();
    if (InteractionComponent) { InteractionComponent->RefreshTickEnabled(); }
}
```

**`PossessedBy`/`OnRep_Controller` 두 곳에 나눠 넣는 것보다 낫다** — 그 방식은 `UnPossessed`가 빠져서
언빙의 후에도 틱이 계속 돈다. `UpdateFocus()` 선두의 가드가 오동작은 막지만,
그 가드는 "한 틱의 안전장치"가 아니라 **영구적으로 도는 빈 호출**이 된다.
````

### 7-3. 같은 문서 — 함정 표 7b 행 교체

```markdown
| **7b** | **틱 on/off 판정을 `BeginPlay`에 둠** | **리스폰 후 프롬프트가 영영 안 뜬다.** 첫 스폰(PIE 호스트)은 `Possess`가 먼저라 **통과해서**, 함정이 없다고 판단하게 된다 | `NotifyControllerChanged()`에서 `RefreshTickEnabled()` (02-2). `PossessedBy`+`OnRep_Controller` 둘로 나누면 `UnPossessed`가 샌다 |
```

### 7-4. 같은 문서 — 3-1 근거 보강 (02-2의 `Channel2` 경고 인용문 아래에 추가)

```markdown
> **같은 번호에 두 줄을 넣으면 나중 것이 조용히 버려진다.** 에디터가 거부하지도, 나중 줄이 이기지도 않는다.
> `CollisionProfile.cpp:401-408`이 `TraceTypeMapping`/`ObjectTypeMapping` 중복을 보고 `RemoveAt` + `continue`한다 —
> `:470`의 `SetResponse`에도 도달하지 못한다. 경고는 뜨지만(`LogCollisionProfile`, 기본 Warning) 엔진 초기화 로그에 묻힌다.
> **증상은 "상호작용 트레이스가 `Projectile`이 막는 것들을 같이 맞힌다"로 나온다** — 채널 번호를 의심하기 전에 픽업 콜리전부터 파게 된다.
> 어느 쪽이 이기는지도 `TArray::Sort`가 안정 정렬이 아니라 보장이 없다.

> **Lyra가 우리와 같은 문안을 쓴다.** `LyraStarterGame/Config/DefaultEngine.ini:216`
> `+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="Lyra_TraceChannel_Interaction")`
> — 채널 번호만 다르고 `DefaultResponse`·`bTraceType`·`bStaticObject`가 전부 같다. 함정 5b의 독립 증거다.
```

### 7-5. `05_Loot_DOCS.md` §7 옆 — 4-2 프리셋 문장

```markdown
> **상호작용 채널을 여는 곳은 두 번째 소비자부터 콜리전 프리셋이다.**
> Step 02 시점에는 소비자가 `AEPPickup` 하나라 `SetCollisionResponseToChannel(EP_TraceChannel_Interact, ECR_Block)` 한 줄이 더 싸다.
> **§7-1 컨테이너 / §7-2 자판기 / 로드맵 12 탈출 지점이 들어올 때** `.ini`에 `EP_Interactable` 프리셋을 만들고 셋이 그것을 쓴다.
> Lyra가 `Interactable_OverlapDynamic`(`LyraStarterGame/Config/DefaultEngine.ini:213`)으로 하는 것과 같다.
> **액터마다 한 줄씩 늘리지 않는다** — 네 번째 액터에서 반드시 하나 빠뜨린다. 공통 C++ 베이스는 만들지 않는다(계층이 늘어난다).
> 우리는 `LineTraceSingleByChannel`이므로 프리셋의 응답은 **`Block`**이다 — Lyra는 `LineTraceMulti`라 `Overlap`을 쓴다. 섞으면 아무것도 안 잡힌다.
```

### 7-6. 같은 문서 02-1 — `OnInteract` 반환 계약 절에 한 문장 추가

```markdown
> **`FText`를 RPC로 보내는 비용을 알고 쓴다.** `FTextProperty`에는 `NetSerializeItem` 오버라이드가 없어서
> 기본 `SerializeItem`으로 떨어진다 — 네임스페이스 + 키 + 소스 문자열이 통째로 나간다.
> 실패 회신은 실패 1회당 1번이라 허용 가능하지만, **프롬프트 텍스트를 서버에서 보내는 쪽으로 번지면 그때는 비싸다.**
> (비교: `FGameplayTag`는 패킹된 net index로 나간다 — `GameplayTagContainer.cpp:1286-1299`)
```

### 7-7. 같은 문서 02-3 — 동시성 각주에 한 문장 추가

```markdown
> **이 보장은 `OnInteract`이 동기적으로 끝날 때만이다.** §7-1 컨테이너가 "내용물 UI를 연다"로 구현되면
> `OnInteract`은 즉시 반환하고 실제 트랜잭션은 여러 프레임 뒤에 끝난다.
> **그때는 `bClaimed`가 아니라 "누가 열어 두었는가"라는 다른 상태가 필요하다.** 지금 만들지는 않는다.
```

---

## 8. 인용 출처

| 주장 | 파일 : 줄 |
|---|---|
| 컴포넌트 RPC 호출은 오너에게 위임 (복제 플래그를 안 본다) | `Components/ActorComponent.cpp:1210-1220` |
| `IsSupportedForNetworking = GetIsReplicated() \|\| IsNameStableForNetworking` | `Components/ActorComponent.cpp:2924` |
| `RF_DefaultSubObject`면 이름이 안정 / 아우터가 불안정하면 풀네임은 불안정 | `UObject/Obj.cpp:5941-5946`, `:5949-5957` |
| 클라는 NetGUID를 발급 못 함 → Default GUID(= 경로를 쓴다) | `PackageMapClient.cpp:3248-3255` |
| Default GUID면 아우터 GUID + 이름을 보낸다 | `PackageMapClient.cpp:925-932`, `:959-960`, `:974-976` |
| 서버가 `StaticFindObject`로 해석하고 정식 GUID 발급 | `PackageMapClient.cpp:1211-1240` |
| 서브오브젝트를 못 찾으면 번치 에러 → 연결 종료 | `DataChannel.cpp:4858-4869` |
| RPC 발신 시 리플리케이터 생성 | `NetDriver.cpp:3245`, `DataChannel.cpp:4552-4560` |
| `SpawnDefaultPawnFor` → `SetPawn` → `FinishRestartPlayer` → `Possess` | `GameModeBase.cpp:1310`, `:1313`, `:1326`, `:1379` |
| `SetPawn`은 `Controller->Pawn`만 / `Pawn->Controller`는 `PossessedBy` | `Controller.cpp:526-535`, `Pawn.cpp:650-655` |
| `BeginPlay`는 월드가 begun play일 때만 즉시 실행 | `Actor.cpp:4482` |
| `World->SetBegunPlay(true)`는 `NotifyBeginPlay`에서만 | `WorldSettings.cpp:353-369` |
| PIE: `SpawnPlayActor`가 `World->BeginPlay()`보다 먼저 | `GameInstance.cpp:537`, `:565` |
| `HandleMatchIsWaitingToStart`가 ready면 `NotifyBeginPlay`를 건너뜀 | `GameMode.cpp:149-162` |
| `HandleMatchHasStarted`: `RestartPlayer` → 그다음 `NotifyBeginPlay` | `GameMode.cpp:203-221` |
| `HandleStartingNewPlayer`: 매치 진행 중일 때만 `RestartPlayer` | `GameMode.cpp:526-545` |
| `NotifyControllerChanged()` 가상 함수 | `Pawn.h:382`, `Pawn.cpp:719-730` |
| 호출 지점 3곳 (빙의 / 복제 / 언빙의) | `Pawn.cpp:688`, `:622`, `:714` |
| `bAlwaysNotifyClientOnControllerChange` 기본 true | `Controller.cpp:34` |
| `APawn::Controller`는 조건 없이 복제 | `Pawn.cpp:1282` |
| 클라: RepNotify → 그다음 `PostNetInit`(→`BeginPlay`) | `DataChannel.cpp:3485`, `:3495-3503`, `Actor.cpp:4638-4654` |
| 채널 중복 시 나중 것을 버리고 경고 | `Collision/CollisionProfile.cpp:381-385`, `:401-408` |
| `bTraceType=True`의 효과 / `DefaultResponse` 적용 지점 | `Collision/CollisionProfile.cpp:429-453`, `:470` |
| `FGameplayEventData::Target` | `Abilities/GameplayAbilityTypes.h:233-284` (Target `:256`) |
| 이벤트 데이터가 서버 RPC 파라미터 | `AbilitySystemComponent.h:1726`, `..._Abilities.cpp:1899-1928`, `:1992-1995` |
| 서버 측 수신 / 보안 정책 / 실패 회신 | `..._Abilities.cpp:2020-2085` (`:2052-2058`, `:2081`) |
| `ClientActivateAbilityFailed`에 사유 없음 | `AbilitySystemComponent.h:1747` |
| `FGameplayTag`는 패킹 직렬화 | `GameplayTags/Private/GameplayTagContainer.cpp:1286-1299` |
| **Lyra** 인터페이스 2함수 | `Interaction/IInteractableTarget.h:41-51` |
| **Lyra** `FInteractionOption` 두 갈래(부여 / 대상 ASC) | `Interaction/InteractionOption.h:34-50` |
| **Lyra** 스캐너 어빌리티 정책 / 서버 게이트 | `Abilities/LyraGameplayAbility_Interact.cpp:24-26`, `:34-38` |
| **Lyra** `TriggerInteraction` — 로컬 트리거, 대상 전송 없음 | `Abilities/LyraGameplayAbility_Interact.cpp:78-122` |
| **Lyra** 동적 부여(구체 오버랩 + `GiveAbility`, 회수 없음) | `Tasks/AbilityTask_GrantNearbyInteraction.cpp:36`, `:59`, `:80-90` |
| **Lyra** 포커스 탐지는 라인 트레이스 | `Tasks/AbilityTask_WaitForInteractableTargets.cpp:19-33` |
| **Lyra** 스캔 주기 0.1s / 반경 500 / 라인 100 | `LyraGameplayAbility_Interact.h:48-52`, `..._SingleLineTrace.h:38-39` |
| **Lyra** 상호작용 채널 설정 (우리와 동일 문안) | `LyraStarterGame/Config/DefaultEngine.ini:216` |
| **Lyra** 콜리전 프리셋으로 대상 지정 | `LyraStarterGame/Config/DefaultEngine.ini:213` |
| 프로젝트: `DefaultAbilities` 배열이 이미 있다 | `EPCharacter.h:61`, `EPCharacter.cpp:135-137` |
| 프로젝트: `ActivationBlockedTags`에 `State.Dead` 3곳 | `EPGA_Item_PrimaryUse.cpp:24`, `EPGA_Item_Reload.cpp:24`, `EPGA_Skill_Base.cpp:17` |
| 프로젝트: `AEPGameMode : AGameMode`, `MinPlayersToStart = 1` | `EPGameMode.h:14`, `:36` |
| 프로젝트: Client RPC 선례 2개 | `EPPlayerController.h:40-45` |
