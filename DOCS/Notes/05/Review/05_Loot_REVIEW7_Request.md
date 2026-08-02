# 검수 요청 7차 — Step 02 상호작용: GAS를 탈 것인가, 그리고 방금 고친 5건이 맞는가

> 작성일: 2026-08-02
> 6차: `05_Loot_REVIEW6_Request.md` / `_Answer.md` (전역 데이터 참조 위치 — `UDeveloperSettings` 유지로 확정)
> 시점: **Step 01 구현 마무리 중, Step 02 코드 0줄.** 지금이 설계를 바꿀 수 있는 마지막 지점이다
> 성격: **① 아키텍처 판단 하나(§2) + ② 방금 문서에 반영한 판정 5건의 검증(§3).** 둘의 무게가 다르다 — §2가 뒤집히면 §3의 절반이 무의미해진다

---

## 0. 사용자 입장 (먼저 밝힌다)

**Step 02 문서를 방금 Claude가 7군데 고쳤다.** 확정 오류 2건, 구현 불가 1건, 계약 변경 1건, 관례 정리 3건이다. 사용자는 그 진단을 검증 없이 받았다.

**두 가지를 요청한다.**

1. **§2를 먼저 판정해달라.** 문서가 채택한 `Server_Interact` 직접 RPC 경로는 **이 프로젝트에 남아 있는 유일한 서버 RPC가 된다.** 이걸 모르고 문서를 고쳤다면 §3의 절반(RPC 선언 위치, 컴포넌트 복제, 실패 회신 경로)은 애초에 틀린 질문에 답한 것이다
2. **§3의 5건은 근거까지 같이 검증해달라.** 특히 3-2는 "PIE 첫 테스트에서 바로 걸린다"고 단언했는데, 그 단언이 틀리면 없는 함정을 문서에 새겼다

---

## 1. 현재 상태 (사실만)

### 1-1. 진행

| | 상태 |
|---|---|
| Step 00 (ItemCore) | 완료 |
| Step 01 (Spawner) | `EPItemSpawner.cpp` 완성 / `AEPPickup` 미완 / `EPLootDebugCommands.cpp` 작성 중 |
| **Step 02 (Interaction)** | **코드 0줄.** 문서만 있다 |

### 1-2. ★ 이 프로젝트에 서버 RPC가 하나도 없다

```
$ grep -rn "UFUNCTION(Server" Public/
(결과 없음)
```

GAS 마이그레이션으로 `Server_Fire` / `Server_Reload`가 사라졌고, **모든 게임플레이 입력이 어빌리티 태그로 간다.**

```cpp
// EPCharacter.cpp:388-397
void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
    if (!CombatComponent) return;
    if (ASC)
        ASC->TryActivateAbilitiesByTag(
            FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Item_PrimaryUse));
}
```

`Input_Reload` / `Input_Dash`(`:420`) / `Input_Heal`(`:426`) / `Input_Shield`(`:432`) 전부 같은 형태다. **예외가 없다.**

그런데 `05_Loot_02_Interaction.md`는 `Server_Interact(AActor* Target)`를 직접 선언한다.

### 1-3. 확인된 사실 (다시 파지 말 것 — 전부 직독 확인함)

| 사실 | 출처 |
|---|---|
| `SpawnDefaultPawnFor`(→`BeginPlay`)가 `Possess`보다 **먼저** | `GameModeBase.cpp:1310` vs `:1326`→`:1379` |
| `AController::SetPawn`은 `Controller->Pawn`만 세팅. `Pawn->Controller`는 `PossessedBy`에서 | `GameModeBase.cpp:1313`, `Controller.cpp:316` |
| `ECC_GameTraceChannel2` = `Projectile` (이미 사용 중) | `DefaultEngine.ini:307` |
| `EPTypes.h`에는 `Weapon`(Channel1) 상수 하나뿐 — `Projectile` 상수 없음 | `EPTypes.h:96` |
| 생성자 서브오브젝트는 `RF_DefaultSubObject` → 이름이 네트워크 안정 | `Obj.cpp:5945` |
| `UEPCombatComponent`는 `SetIsReplicatedByDefault(true)` | `EPCombatComponent.cpp:34` |
| `AEPCharacter`가 `PossessedBy` / `OnRep_Controller`를 **이미 오버라이드** | `EPCharacter.h:80-81` |
| `AEPPickup::bClaimed`는 `private` | `EPPickup.h:43` |
| `UEPItemDefinitionSubsystem::FindData`는 `const` | `EPItemDefinitionSubsystem.h:23` |
| `AEPPlayerController::HUDWidget`은 `private`, `Client_OnKill`/`Client_PlayHitConfirmSound`가 이미 있음 | `EPPlayerController.h` |
| **Lyra에 GAS 기반 상호작용 모듈이 존재** (내용 미확인) | `LyraStarterGame/Source/LyraGame/Interaction/` — `IInteractableTarget.h`, `InteractionOption.h`, `InteractionQuery.h`, `Abilities/LyraGameplayAbility_Interact.*`, `Abilities/GameplayAbilityTargetActor_Interact.*`, `Tasks/` |

---

## 2. ★ 최대 주제 — `Server_Interact` 직접 RPC인가, `GA_Interact`인가

### 2-1. 문서의 현재 설계

```
F키 → UEPInteractionComponent::Input_Interact()
    → Server_Interact(FocusedActor)          [Server, Reliable]
        → 1. IsValid  2. Implements<>  3. 거리  4. CanInteract  5. OnInteract
        → 실패 시 PC->Client_OnInteractFailed(Reason)
```

탐지는 로컬 클라이언트가 0.1초 틱으로 라인 트레이스, 판정은 서버 재검증.

### 2-2. 반대 근거 — 이 경로에 걸리는 것들

**(a) 관례가 여기서만 깨진다.** §1-2대로 프로젝트에 서버 RPC가 0개다. 이걸 넣으면 "입력 → 어빌리티"라는 단일 규칙에 예외가 하나 생긴다.

**(b) 문서 스스로 두 경로를 만든다.** 02-1이 이렇게 적어 두었다.

> `GetInteractDuration() > 0`인 채널링은 **GAS 어빌리티로 구현한다.** 상호작용 컴포넌트는 채널링을 직접 만들지 않고 어빌리티를 활성화만 한다.

즉 **즉시 상호작용은 RPC, 채널링 상호작용은 GAS**가 된다. 같은 F키가 대상에 따라 다른 배관을 탄다. §7-1 컨테이너가 들어오는 순간 두 경로가 동시에 살아 있다.

**(c) GAS가 이미 주는 것을 다시 만든다.** 예측, 태그 기반 잠금(`State.Casting` — 사격 중 상호작용 금지 같은 것), 쿨다운(Step 03의 `DropCooldown`이 정확히 GE 쿨다운 모양이다), 코스트, 실패 태그 회신(`OnAbilityFailed`)이 전부 이미 있다. `Client_OnInteractFailed`는 GAS의 실패 회신을 손으로 다시 만드는 것에 가깝다.

### 2-3. 찬성 근거 — 그런데도 RPC일 수 있는 이유

- **상호작용은 어빌리티가 아니다.** 데미지도 코스트도 애님도 없다. GAS에 올리면 `UGameplayAbility` 한 겹, 태그 하나, 부여 시점 관리가 추가된다
- **대상 전달이 번거롭다.** `Server_Interact(AActor*)`는 파라미터 한 개다. GAS로 하려면 `FGameplayAbilityTargetData`를 만들거나 어빌리티가 서버에서 다시 트레이스해야 한다. **후자면 클라 탐지 결과와 서버 판정 대상이 갈릴 수 있다** — 문서가 세운 "클라는 요청, 서버가 결정" 원칙과 미묘하게 다르다
- **포트폴리오 규모.** 6차에서 `DA_EPGameData`를 "과잉 구조"로 기각한 것과 같은 논리가 여기도 적용될 수 있다

### 2-4. 판정 요청

1. **A(직접 RPC) / B(GA_Interact) / C(혼합 — 즉시는 RPC, 채널링만 GAS) 중 무엇인가.** C는 문서의 현재 상태인데, 이게 **의도된 절충인지 미처리 모순인지**부터 판정해달라
2. B라면 **대상 전달을 어떻게 하는가.** `TargetData`인가, 서버 재트레이스인가. Lyra가 `GameplayAbilityTargetActor_Interact`를 두고 있는 이유가 이것이라면 그 구조를 인용해달라
3. B라면 **`IEPInteractable` 인터페이스는 유지되는가**, 아니면 Lyra의 `IInteractableTarget` + `FInteractionOption` 모양으로 가야 하는가. 후자면 02-1의 4함수 설계가 통째로 바뀐다
4. B라면 **어빌리티는 언제 부여하는가.** 상시 부여인가, Lyra처럼 근처 대상에서 동적으로 받아오는가. 후자는 우리 규모에 과한가
5. **A/C로 남길 경우**, "GAS 프로젝트에 서버 RPC가 하나 있다"는 상태를 어떻게 정당화해 문서에 적을 것인가. 근거 없이 예외로 두면 다음 시스템에서 또 갈린다

> **CLAUDE.md §2 기준으로 보면**: `GA_Interact`는 어느 문서에도 이름이 없다. 반면 "채널링은 GAS로"는 02-1에 이름이 있다. 그래서 이 질문은 *"문서를 먼저 고칠 것인가"* 이기도 하다.

---

## 3. 방금 문서에 반영한 판정 5건 — 검증 요청

**전부 이번 세션에 Claude가 진단하고 즉시 문서에 반영했다.** 사용자는 검증하지 않았다.

### 3-1. `ECC_GameTraceChannel2` → `Channel3` (확정 오류라고 판정)

문서에 `Channel2`로 적혀 있었으나 `DefaultEngine.ini:307`에서 `Projectile`이 이미 쓰고 있다.

- 실제로 같은 번호에 `+DefaultChannelResponses`를 두 줄 넣으면 **무슨 일이 일어나는가?** 에디터가 거부하는가, 나중 줄이 이기는가, 조용히 둘 다 남는가
- `Projectile` 채널은 `bTraceType=False`(오브젝트 타입)다. 우리 `Interact`는 `bTraceType=True`로 잡았는데 맞는가

### 3-2. ★ "틱 판정을 `BeginPlay`에 두면 리슨서버 호스트가 영영 못 줍는다"

근거는 §1-3의 `GameModeBase.cpp` 순서다. 대응으로 `PossessedBy` / `OnRep_Controller`에서 `RefreshTickEnabled()`를 부르게 했다.

- **이 인과가 실제로 성립하는가.** `BeginPlay` 시점에 호스트 폰의 `Controller`가 정말 null인가 — `SpawnDefaultPawnFor` 내부나 `PostActorConstruction` 경로에서 먼저 채워질 여지는 없는가
- **소유 클라이언트 쪽은?** 복제 폰의 `BeginPlay`(`PostNetInit`)가 `OnRep_Controller`보다 먼저인가 나중인가. 순서가 보장되는가
- `bStartWithTickEnabled = false` + 두 훅이 **충분한가.** 리스폰·언빙의·`UnPossessed`에서 새는 곳은 없는가

### 3-3. "5·6단계는 컴포넌트가 실행할 수 없다" → 8단계를 5단계로 축소

`bClaimed`가 `AEPPickup`의 `private`이고 `IEPInteractable`에는 그 개념이 없으므로, 확인은 `CanInteract()`에 흡수하고 마킹/되돌리기는 `OnInteract()` 안으로 옮겼다.

- **동시성 주장이 맞는가.** *"RPC는 게임 스레드 순차 처리이고 `CanInteract`→`OnInteract` 사이에 양보 지점이 없으므로 단계를 나눠도 합쳐도 안전하다"* 고 적었다. 이 진술이 정확한가
- §2가 B(GAS)로 판정되면 **예측(prediction)이 끼어든다.** 그때도 이 주장이 유지되는가 — 클라 예측 실행이 `bClaimed`를 건드리면 안 되는데, 그 방어를 어디에 두는가

### 3-4. `OnInteract`을 `void` → `bool` + `FText& OutReason`

근거: 문서 스스로 "부분 획득이면 되돌리고 픽업을 남긴다"는 실패 갈래를 적어 두었는데 `void`면 호출자가 실패를 모른다. 구현체가 넷(픽업·컨테이너·자판기·탈출)으로 예고돼 있어 나중 변경 비용이 크다.

- **반환 계약이 이 모양이 맞는가.** `bool` + `OutReason`인가, `enum` 결과 코드인가, `FGameplayTag` 실패 사유인가(§2가 B면 태그가 자연스럽다)
- `CanInteract`(사전 판정)과 `OnInteract`(실행 중 실패)이 **둘 다 `OutReason`을 갖는 게 중복은 아닌가.** 하나로 합칠 여지가 있는가

### 3-5. `SetIsReplicatedByDefault(false)` → `true`, RPC 선언 위치 분리

- `Server_Interact` → `UEPInteractionComponent` (`InteractRange`가 여기 있으므로)
- `Client_OnInteractFailed` → `AEPPlayerController` (`Client_OnKill` 옆, `HUDWidget`이 거기 `private`)
- 복제 플래그를 `true`로. 근거: *"`false`여도 `RF_DefaultSubObject`라 RPC는 나가지만(`Obj.cpp:5945`), 그걸 아는 사람만 이 코드를 안 의심한다. 복제 프로퍼티가 0개라 비용도 0"*

- **`false`여도 RPC가 나간다는 진술이 정확한가.** 동적 스폰 액터의 서브오브젝트라 `IsFullNameStableForNetworking`은 false일 텐데(아우터가 불안정), 그래도 `WriteContentBlockHeader`의 "아우터 GUID + 이름" 경로로 해석되는가
- **RPC를 두 클래스에 나눠 두는 게 맞는가.** 한 흐름의 요청과 회신이 다른 파일에 있다. 컴포넌트에 몰거나 PC에 몰거나 하는 편이 낫지 않은가
- §2가 B면 이 세 항목은 **전부 무의미해진다.** 그 경우 그렇다고 말해달라

---

## 4. 미결 — 사용자가 결정해야 한다고 남겨둔 것

### 4-1. 라인 트레이스 vs 스피어 스윕

현재 `LineTraceSingleByChannel`, `InteractRange = 250`. 2.5m에서 20cm 물체는 시야각 4.6°라 **동작은 한다.**

- 반경 15~20cm 스윕이 조작감이 낫다고 보는데, 카메라가 벽에 붙었을 때 시작 지점이 지오메트리 안에 들어가는 갈래가 새로 생긴다. **실무에서 어느 쪽인가**
- §2가 B로 가면 Lyra의 타깃 액터가 이걸 이미 정해놨을 수 있다 — 그렇다면 그 값을 인용해달라

### 4-2. 상호작용 대상이 픽업만이 아니게 될 때의 채널 열기

Step 01의 픽업은 전 채널 `Ignore`이고 02가 `Interact` 하나만 연다. **§7의 컨테이너·자판기, 로드맵 12의 탈출 지점은 콜리전을 이미 갖고 있을 액터다.** 그때 `Interact` 채널을 어디에 여는가 — 액터마다 손으로 한 줄씩인가, 공통 베이스나 콜리전 프리셋(`BP_Interactable` / `PhysicsAsset` 프로필)을 두는가.

**이건 지금 정하지 않으면 세 번 반복해서 잊는 종류로 보인다.** 다만 CLAUDE.md §2 기준으로 "공통 베이스"는 문서에 이름이 없다.

---

## 5. ★ 실무 조사 요청

우리 판단만으로 결정하지 않겠다. **가능하면 실제 소스를 근거로.**

1. **Lyra의 `Interaction/` 모듈 전체 구조.** `IInteractableTarget` / `FInteractionOption` / `InteractionQuery` / `LyraGameplayAbility_Interact` / `GameplayAbilityTargetActor_Interact` / `Tasks/`가 서로 어떻게 맞물리는가. **특히 "대상을 어떻게 서버에 전달하는가"와 "어빌리티를 언제 부여하는가"** (§2-4의 2·4번)
2. **그 구조가 우리 규모에 과한가.** 6차 답변이 `DA_EPGameData`를 기각한 논리를 여기에도 적용할 수 있는가, 아니면 상호작용은 사정이 다른가
3. **GAS 프로젝트에서 "그냥 서버 RPC"를 남기는 게 실무에서 흔한가.** 흔하다면 가르는 기준이 무엇인가 (예측이 필요한가? 코스트/쿨다운이 있는가? 애님이 있는가?)
4. **3-2의 `BeginPlay` / `PossessedBy` 순서**를 엔진 소스로 확정해달라. 서버·리슨서버·소유클라 세 경우 전부

> 로컬 경로: 엔진 `C:\Program Files\Epic Games\UE_5.7\Engine`, Lyra `C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame`, GAS 문서 `C:\Github\GASDocumentation`. **기억으로 Lyra API를 단정하지 말 것** — 6차에서 인용 정확도가 유용했다.

---

## 6. 재론하지 않았으면 하는 것

| | 근거 |
|---|---|
| `FEPItemState` 값 타입 / 스택 폐지 / `bFungible` | 1·2차 확정 |
| DT/DA 두 계층 유지 | 3차 §5 확정 |
| 전역 에셋 참조를 `UEPLootDeveloperSettings`에 두는 것 | **6차 확정.** `DA_EPGameData` 재론 불필요 |
| `PickupClass`가 아이템별이 아니라 전역인 것 | 6차 §4-4 확정 |
| 픽업의 전 채널 `Ignore` + Dormancy 설계 | 5차 확정, Step 01 구현 완료 |
| Step 01의 콘솔 커맨드 3종 / `UEPItemDefinitionSubsystem::Get()` 신설 | 이번 세션 확정, 구현 중. **Step 02와 무관** |
| F키 (E 아님) | 기획 확정 |

---

## 7. 대상 파일

| 파일 | 관계 |
|---|---|
| **`05_Loot_02_Interaction.md`** | **검수 대상 본체.** 이번 세션에 7군데 수정됨 |
| `EPCharacter.cpp:388-432` | §1-2의 근거 — 모든 입력이 GAS로 간다 |
| `Public/GAS/EPGA_Skill_Base.h:31` | `CastTime` — 02-1이 채널링을 위임하겠다고 한 대상 |
| `Public/GAS/EPNativeGameplayTags.h` | §2가 B면 `TAG_Ability_Interact`가 여기 붙는다 |
| `Public/Core/EPCharacter.h:80-81` | `PossessedBy` / `OnRep_Controller` — 3-2의 대응 지점 |
| `Public/Core/EPPlayerController.h` | `Client_OnKill` 관례 / `HUDWidget` private — 3-5 |
| `Public/Loot/EPPickup.h:43` | `bClaimed` private — 3-3의 발단 |
| `Config/DefaultEngine.ini:306-307` | 3-1의 채널 충돌 |
| `Public/Types/EPTypes.h:96` | 채널 상수 — `Weapon` 하나뿐 |
| `05_Loot_01_Spawner.md` 01-4 | 픽업 콜리전 — 02가 여기에 정확히 한 줄 얹는다 |
| `05_Loot_DOCS.md` §4-5, §7 | 서버 검증 선언 / 컨테이너·자판기 — 4-2의 근거 |
| `LyraStarterGame/Source/LyraGame/Interaction/` | §5-1 조사 대상 |
