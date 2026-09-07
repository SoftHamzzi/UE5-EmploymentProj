# 2일 코스 — 내가 만든 것을 설명할 수 있게 만들기

> **독자는 나 하나다.** 남에게 보여줄 글이 아니라, 면접에서 아무 데나 짚혔을 때 말이 나오게 만드는 훈련표다.
>
> 목표는 "다 아는 것"이 아니라 **흐름 두 개를 백지에 그릴 수 있는 것**이다.
> ① 좌클릭 → 상대가 죽기까지 ② 바닥 아이템 → 내 손에 들리기까지
> 이 둘이 프로젝트의 8할을 관통한다. 나머지는 거기 매달린 가지다.

---

## 이 문서를 쓰는 법

**읽지 말고 답해라.** 각 세션은 이렇게 생겼다.

```
목표      — 이 세션이 끝나면 뭘 말할 수 있어야 하는가
경로      — 실제 파일:줄. IDE로 직접 열어서 순서대로 따라간다
확인할 것 — 열었을 때 눈으로 확인할 사실
질문      — ★ 여기가 본체
```

### 1회전 완료 (2026-08-08) — 이제 답이 적혀 있다

세션 1~5를 한 번 돌았고, **내 답 + 정답**을 각 질문에 달았다. 판정 기호는 이렇다.

| | 뜻 | 다시 볼 것 |
|---|---|---|
| ✅ | 맞다 | 안 봐도 된다. 그대로 면접에서 쓴다 |
| ⚠️ | **결론은 맞는데 이유가 다르다** | **여기가 3단계 질문에서 무너지는 자리다.** 이유를 다시 말해봐라 |
| ❌ | 틀렸다 | 정답을 읽고, 소스를 열어 확인하고, 다시 답해봐라 |
| 🔲 | 못 풀었다 | 정답이 적혀 있다. 외우지 말고 **왜 그런지**만 잡아라 |

**31문항 중 ✅ 14 / ⚠️ 6 / ❌ 4 / 🔲 7.**

> **★ 자기 진단:** ❌·⚠️ 10개 중 **6개가 "결론은 맞고 이유가 다른"** 형태다.
> 결론만 아는 상태이고, 그게 정확히 면접 3단계에서 무너지는 모양이다.
> 반대로 잘 맞힌 것(3-⑧, 4-⑧)은 **이유부터** 말했다. 2회전의 목표는 답이 아니라 **이유**다.

### 소스 자료 세 가지와 각각의 함정

| 자료 | 쓰는 곳 | 함정 |
|---|---|---|
| **현재 소스** (`Source/EmploymentProj/`) | **1순위. 항상 여기가 진실** | — |
| `Desktop/revise/posts/` 14편 | 01~03단계의 *왜*와 *실패 기록* | **설명하는 코드가 지금 코드와 다르다.** `Server_Fire`/`Server_Reload`/`AEPCharacter::HP`는 GAS 이관으로 **없어졌다**. 개념만 가져오고 심볼은 믿지 마라 |
| `DOCS/Notes/04`, `05` | GAS·Loot 단계 (**포스트 없음**) | STATUS 파일이 진실의 원천. 단계 문서는 예정 코드일 뿐 |

### 네 번째 자료 — 브랜치에 그 시절 코드가 그대로 있다

**브랜치가 학습 단계와 1:1로 맞는다.** 포스트가 설명하는 코드를 옆에 띄워놓고 읽을 수 있다는 뜻이다.

| 브랜치 | `EPCharacter.cpp` | 대응 |
|---|---|---|
| `feature-gameplay-framework` | 149줄 | 포스트 1-1~1-4 |
| `feature-replication` | 346줄 | 포스트 2-1~2-6 |
| `feature-netprediction` | 427줄 | 포스트 3-1~3-3 |
| `feature-gas` | 519줄 | Notes/04 |
| `feature-loot` | 543줄 | **현재** |

```bash
git show feature-replication:EmploymentProj/Source/EmploymentProj/Public/Combat/EPCombatComponent.h
git diff feature-netprediction feature-gas -- EmploymentProj/Source/EmploymentProj/Private/Combat/EPCombatComponent.cpp
git log --oneline feature-gas -- <파일>
```

**셋 다 읽기 전용이라 작업 트리를 안 건드린다.** 위험한 건 `git checkout <브랜치>` 뿐이니 그것만 하지 마라
(IDE에서 볼 거면 Git 패널의 *Compare with Branch* — 이것도 읽기 전용).

> **★ diff가 문서보다 잘 남는다.** 설명을 읽는 것과 *"이게 왜 사라졌지"* 를 스스로 묻는 건 다르다.
> 각 세션의 **[비교]** 항목이 그 용도다. 시간이 모자라면 세션 3·4의 비교만이라도 해라.

### 일정

| | 세션 | 시간 | 1회전 |
|---|---|---|---|
| **1일차** | 1. 뼈대 — 누가 무엇을 소유하는가 | 1.5h | ✔ |
| | 2. 복제 — 값이 어떻게 남에게 가는가 | 2.5h | ✔ |
| | 3. **흐름 ①** 좌클릭 → 사망 | 3h | ✔ |
| **2일차** | 4. 예측과 되감기 | 2.5h | ✔ |
| | 5. **흐름 ②** 바닥 → 손 | 3h | ✔ |
| | 6. 자가 점검 (백지 테스트) | 1.5h | — |

---

# 세션 1 — 뼈대 (1.5h)

**목표:** *"이 게임에서 X는 어디 살고 왜 거기 사는가"*에 즉답한다.

## 경로

```
Public/Core/EPGameMode.h        ← 서버에만 존재
Public/Core/EPGameState.h       ← 서버가 만들고 전원에게 복제
Public/Core/EPPlayerState.h     ← 플레이어당 하나, 전원에게 복제
Public/Core/EPPlayerController.h← 서버 + 소유 클라에만
Public/Core/EPCharacter.h       ← 월드의 몸
```

## 확인할 것

- `EPPlayerState.h:60` — **ASC가 여기 산다** (`EPPlayerState.cpp:14`에서 생성)
- `EPCharacter.h:143` — 캐릭터에도 `ASC` 필드가 있다. 그런데 `EPCharacter.cpp:132`, `:156`을 보면 **PlayerState 것을 캐시해온 포인터**다. 두 개가 아니다
- `EPCharacter.cpp:521` — `InitAbilityActorInfo(PS, this)`. Owner와 Avatar가 갈린다
- `EPGameMode.cpp` — `:73` `HandleStartingNewPlayer` / `:90` `ChoosePlayerStart` / `:112` `OnPlayerKilled` / `:140` `HandleMatchHasStarted` / `:192` `TickMatchTimer`

## ★ 질문

### 1. ASC를 왜 Character가 아니라 PlayerState에 뒀나? 캐릭터에 뒀다면 무엇이 깨지나? ✅

*힌트: 죽으면 캐릭터 액터는 어떻게 되고, PlayerState는 어떻게 되나*

**내 답** — Character에 두면 리스폰 시 ASC가 초기화된다. PlayerState는 캐릭터 액터를 파괴해도 유지된다.

**정답** — 맞다. 덧붙일 것 하나: 그래서 `InitAbilityActorInfo(PS, this)`(`EPCharacter.cpp:521`)에서 **Owner와 Avatar가 갈린다.** Owner(PS)는 죽어도 남고, Avatar(Character)는 리스폰마다 갈아 끼운다. 리스폰 때 이 함수를 **다시** 불러 Avatar만 교체하는 것이 핵심이다.

---

### 2. `EPCharacter.h:143`의 `ASC`는 소유 포인터인가 빌려온 포인터인가? GC는 이걸 어떻게 보나? ⚠️

**내 답** — PlayerState에서 빌려온 포인터. GC는 `TObjectPtr`이라 볼 수 있다 (정확히 모름).

**정답** — 앞은 맞고 뒤가 반만 맞다.

```cpp
// EPCharacter.h:142-143
UPROPERTY()
TObjectPtr<UAbilitySystemComponent> ASC;
```

GC가 보는 이유는 `TObjectPtr`이라서가 아니라 **`UPROPERTY()`가 붙어서**다. `UPROPERTY` 없는 raw 포인터는 GC가 모른다 — 대상이 수거돼도 포인터가 남아 댕글링이 된다.

**그래서 의미가 갈린다: 의도는 "빌려온 포인터"인데 GC에게는 "강한 참조"로 보인다.** 소유자는 PlayerState인데 GC 관점에선 캐릭터도 소유자다. 헷갈리기 쉬운 자리라 좋은 질문거리다.

---

### 3. GameMode에 접근하려는 코드가 클라이언트에서 돌면? 크래시인가 nullptr인가, 왜? ✅

**내 답** — nullptr. GameMode는 서버 전용이라 클라이언트로 복제되지 않는다.

**정답** — 맞다. `GetWorld()->GetAuthGameMode()`가 클라에서 `nullptr`을 준다. 크래시는 **반환값을 안 검사했을 때** 그다음 줄에서 난다.

> 설계 의도가 여기 있다. GameMode는 **규칙의 소유자**라 클라가 아예 볼 수 없어야 한다. 클라가 알아야 하는 결과값(남은 시간, 매치 상태)만 GameState로 내려간다. **"못 보내는 것"이 아니라 "안 보내는 것"이다.**

---

### 4. `ChoosePlayerStart`는 언제 누가 부르나? 서버에만 있어도 되는 이유는? ⚠️

**내 답** — GameState에서 게임 시작 흐름에 도달했을 때. 정확히 어디서 부르는지는 모름. 서버에만 있어도 되는 이유는 플레이어는 스폰될 위치만 알면 되기 때문.

**정답** — 뒤는 맞고 **앞이 틀렸다. GameState가 아니라 GameMode 자신이다.**

```
AGameModeBase::RestartPlayer        GameModeBase.cpp:1263
      └→ FindPlayerStart                        :1195
            └→ ChoosePlayerStart   ← 우리가 오버라이드한 것
```

`HandleStartingNewPlayer`(우리 `EPGameMode.cpp:73`)도 같은 경로로 들어간다.

서버에만 있어도 되는 이유는 더 정확히 말하면 — **클라는 "어디서 시작할지"를 알 필요조차 없다.** 스폰된 결과 위치가 액터 복제로 내려오면 끝이다. 선택 *과정*이 클라에 있으면 오히려 위험하다(어디에 적이 스폰되는지 알게 된다).

---

### 5. 매치 타이머를 GameMode가 아니라 GameState에 둬야 하는 값은? ✅

**내 답** — `int32 RemainingTime`. GameState는 클라에 복제되니 남은 시간을 확인할 수 있다.

**정답** — 맞다. 한 줄로 다듬으면: **"판단은 GameMode, 표시는 GameState."** 타이머를 *굴리는* 것(`TickMatchTimer`, `EPGameMode.cpp:192`)과 매치를 *끝내는* 결정은 서버에만 있고, 클라 HUD가 그릴 숫자만 GameState로 내려간다. 같은 값을 양쪽에 두면 두 시계가 갈린다.

**막히면:** `revise/posts/2026-02-06-EP_Gameplay_Framework-1.md` ~ `-4.md`

---

# 세션 2 — 복제 (2.5h)

**목표:** *"이 값이 왜 저쪽에 보이는가 / 왜 안 보이는가"*를 매번 근거를 대고 답한다.

## 경로

```
Private/Core/EPCharacter.cpp:509        GetLifetimeReplicatedProps
Private/GAS/EPAttributeSet.cpp:89       GetLifetimeReplicatedProps
Private/GAS/EPAttributeSet.cpp:100~120  OnRep_Health / MaxHealth / Ammo / MaxAmmo / MoveSpeedMultiplier
Private/Combat/EPCombatComponent.cpp:250 GetLifetimeReplicatedProps
Private/Combat/EPCombatComponent.cpp:144 OnRep_EquippedWeapon
Private/Loot/EPPickup.cpp:75            GetLifetimeReplicatedProps
```

## 확인할 것

- `GetLifetimeReplicatedProps`가 프로젝트에 **몇 개** 있는지 세어봐라. 각각 `DOREPLIFETIME`인지 `DOREPLIFETIME_CONDITION`인지, 조건이 뭔지
- `EPPickup.cpp:75` — **`ItemId`만 복제된다.** `FEPItemState`는 서버 전용이다
- `EPCombatComponent.cpp:233`, `:241`, `:257` — Multicast RPC 세 개

## ★ 질문

### 1. `UPROPERTY(Replicated)`만 적고 등록을 안 하면? 컴파일 에러? 런타임 경고? 조용히 무시? ✅

**내 답** — 조용히 무시되며 복제되지 않는다.

**정답** — 맞다. 그리고 **이게 이 프로젝트에서 세 번 반복된 실수다.**

| 대상 | 빠진 것 |
|---|---|
| `UEPItemInstance` 전 필드 | `Replicated` 지정·등록·서브오브젝트 등록 **전부** |
| `AEPWeapon::MaxAmmo` | `Replicated`인데 `GetLifetimeReplicatedProps` 미등록 |
| `BoneDamageMultiplierMap`, `TraceDistanceCm` | `UPROPERTY` 자체가 없음 |

**교훈 한 줄로:** *"UE에서 `UPROPERTY`/`GetLifetimeReplicatedProps`는 문법이 아니라 계약이다. 빠뜨리면 실패하는 게 아니라 조용히 없던 일이 된다."* — 컴파일도 되고 경고도 없고 크래시도 안 난다. **다른 경로가 그럴듯한 결과를 만들어주면 끝까지 모른다.**

---

### 2. 바닥에 떨어진 무기의 남은 탄약을 복제하면 안 되는 이유는? ❌

**내 답** — 습득할 때 무기 액터를 삭제하니 복제한 탄약 변수 참조도 소실된다.

**정답** — 이유가 다르다. 참조 소실 문제가 아니다.

**클라가 그 값을 쓸 일이 없어서다.** 클라는 바닥의 총을 **그리기만** 하면 되고, 그리는 데 필요한 건 `ItemId` 하나다(`EPPickup.cpp:82,:87` `OnRep_ItemId → ApplyVisual`). 남은 탄약은 **주운 순간 서버가 인벤토리로 옮기면 그만**이라 클라를 거칠 필요가 없다.

그리고 안 보내는 게 **치트 방지**이기도 하다 — 바닥에 뭐가 얼마나 들었는지 클라가 알면 메모리를 읽어 파밍 경로를 최적화할 수 있다.

> **일반화:** *"클라가 화면에 그리는 데 필요한 최소값만 내려보낸다."* 이게 `FEPItemState`가 서버 전용인 이유고, 인벤토리를 만들 때도 그대로 적용될 원칙이다.

---

### 3. `OnRep_`은 서버에서도 불리나? 안 불린다면 서버는 같은 처리를 어떻게 하나? ❌

**내 답** — `OnRep_`은 클라에서 호출된다. 서버에서는 `UFUNCTION(Server, Reliable)`이 붙은 함수를 호출해줘야 한다.

**정답** — 앞은 맞고 **뒤가 틀렸다.** 서버가 자기 자신에게 Server RPC를 부를 일은 없다.

**이 프로젝트가 실제로 하는 건 "코드를 두 번 쓰는 것"이다.** `EPCombatComponent.cpp:164` `EquipWeapon`(서버 전용)을 보면 `OnRep_EquippedWeapon`(`:144`)과 **같은 코드**가 다시 적혀 있다 — Attach + `LinkAnimClassLayers`.

흔한 대안은 서버에서 `OnRep_EquippedWeapon()`을 **손으로 한 번 불러주는 것**이다.

> **★ 이건 중복 코드다.** 한쪽만 고치면 서버와 클라의 장착 결과가 갈린다. 지금은 두 곳 다 맞지만 구조적으로 갈릴 수 있는 자리 — 답을 적으면서 발견한 종류의 문제고, 이런 게 면접에서 좋은 소재다.

---

### 4. Multicast RPC는 나중에 접속한 사람에게 어떻게 되나? 그래서 뭘 보내면 안 되나? 🔲

**내 답** — 전송되지 않는다. 뒤는 모르겠다.

**정답** — 앞은 맞다. Multicast는 **부르는 그 순간 접속해 있고 릴러번트한 사람에게만** 간다. 늦게 온 사람은 없던 일이다.

**보내면 안 되는 것: 지속되는 상태 전부.**

| | 어떻게 보내나 | 왜 |
|---|---|---|
| 총구 화염, 착탄, 피격음 | **Multicast** ✅ | 한순간 났다 사라진다. 놓쳐도 손해가 없다 |
| 장착한 무기, 체력, 매치 상태 | **`Replicated` + `OnRep_`** | 늦게 온 사람도 **현재 값**을 받아야 한다 |

"장착 무기"를 Multicast로 보내면 늦게 들어온 사람 화면에서 그 사람은 **영원히 맨손**이다. 그래서 `EquippedWeapon`이 `ReplicatedUsing=OnRep_EquippedWeapon`인 것이다.

> **한 줄:** *"Multicast는 사건(event), 복제 변수는 상태(state)."*

---

### 5. `COND_OwnerOnly` / `COND_SkipOwner` — 어디 쓰였고, 바꿔 달면 뭐가 깨지나? 🔲

**내 답** — 클라 본인에게만 보여야 하는 정보 / 다른 사람들에게 보여줘야 하는 정보. 뭐가 깨지는지는 모르겠다.

**정답** — 용도 구분은 맞다. 깨지는 건 이렇다.

**`COND_OwnerOnly`를 떼면** — 남의 정보가 나한테 온다. 대역폭 낭비이자 **치트 소재**다(적의 잔탄을 알게 된다).

**`COND_SkipOwner`를 떼면** — 낭비를 넘어 **화면이 튄다.** 내가 이미 예측으로 아는 값을 서버가 되돌려 보내면 **내 예측을 덮어쓴다.** 값이 같으면 무해하지만 한 프레임이라도 어긋나면 되감겼다 돌아온다.

> 엔진이 이걸 알고 있다. `bIsCrouched`가 `COND_SimulatedOnly`인 이유가 정확히 이것이다(`Character.cpp:1727`) — **본인은 로컬에서 `Crouch()`를 직접 불러 이미 안다.** 되돌려 보낼 이유가 없다.

---

### 6. Attribute는 왜 `float`이 아니라 `FGameplayAttributeData`인가? `OnRep_`이 `OldValue`를 받는 이유는? ⚠️

**내 답** — GAS와 연동해야 하니까. `OldValue`를 받는 이유는 모르겠다.

**정답** — "연동"보다 구체적인 이유가 있다.

**(a) `FGameplayAttributeData`인 이유** — `BaseValue`와 `CurrentValue` **두 개**를 들고 있어야 한다. 버프로 이동속도가 1.5배가 됐을 때, 버프가 끝나면 원래 값으로 돌아가야 한다. `float` 하나로는 "원래 얼마였는지"를 잃는다. Infinite/Duration GE의 모디파이어가 `CurrentValue`만 건드리고 `BaseValue`는 남겨두는 구조다.

**(b) `OldValue`인 이유** — GAS는 *값이 얼마인지*가 아니라 ***얼마나 변했는지***로 델리게이트를 돌린다.

```cpp
// EPAttributeSet.cpp:100~120
GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, Health, OldValue);
```

이 매크로가 `OldValue`를 ASC에 넘겨줘야 `GetGameplayAttributeValueChangeDelegate` 구독자(예: `EPCharacter.cpp:527`의 MoveSpeed 핸들러, HUD 위젯)가 울린다. **`OldValue` 없이는 복제로 값이 바뀌었을 때 클라의 델리게이트가 안 울린다** — 서버에선 되는데 클라 HUD만 안 갱신되는 증상이 이것이다.

## [비교] 복제가 들어오기 전 / 후

```bash
git diff feature-gameplay-framework feature-replication -- EmploymentProj/Source/EmploymentProj/Private/Core/EPCharacter.cpp
```

149줄 → 346줄. **늘어난 200줄이 전부 "혼자 하던 것을 남에게 알리는 비용"이다.**
`GetLifetimeReplicatedProps`·`OnRep_`·RPC가 어디에 얼마나 붙었는지 세어봐라.

**막히면:** `revise/posts/2026-02-27-EP_Replication-2·3·4.md`, `2026-03-01-EP_Replication-5.md`

---

# 세션 3 — 흐름 ① 좌클릭 → 사망 (3h)

**여기가 이 프로젝트의 척추다.** 시간을 제일 많이 쓴다.

**목표:** 좌클릭부터 상대 체력이 깎이고 HUD가 바뀌기까지를 **끊김 없이 말로 설명**한다.

## 경로 — 순서대로 열어라

```
1  Private/Core/EPCharacter.cpp:418        TryActivateAbilitiesByTag  ← 입력의 진입점
2  Private/GAS/EPGA_Item_PrimaryUse.cpp:15     NetExecutionPolicy = LocalPredicted
3                                    :24-25    ActivationBlockedTags (Dead / Reloading)
4                                    :42       CommitAbility
5                                    :50       GetServerWorldTimeSeconds  ← 시각을 여기서 찍는다
6                                    :53-57    IsNetAuthority → HandleServerFire
7                                    :59-69    !IsNetAuthority → 로컬 이펙트
8                                    :98-115   ApplyCooldown (SetByCaller ← FireRate)
9  Private/Combat/EPCombatComponent.cpp:57     HandleServerFire
10                                     :292    HandleHitscanFire
11 Private/Combat/EPServerSideRewindComponent.cpp:297  ConfirmHitscan
12                                              :256   GetHitscanCandidates
13 Private/Combat/EPCombatComponent.cpp:269    ApplyGEDamage
14                                     :362    GetTagDamageMultiplier
15 Private/GAS/EPAttributeSet.cpp:27           PostGameplayEffectExecute  ← 체력이 실제로 깎이는 곳
16                              :11            PreAttributeChange        ← 클램프
17 Public/GAS/EPGA_Death.h                     사망
```

## 확인할 것

- 1번에서 **직접 서버 RPC를 부르지 않는다.** 태그로 어빌리티를 켠다
- 6번과 7번이 `if (IsNetAuthority)` / `if (!IsNetAuthority)`로 **완전히 갈린다**
- 11번은 서버에서만 도는 컴포넌트다 (`SetIsReplicatedByDefault(false)`)
- 15번 `PostGameplayEffectExecute`와 16번 `PreAttributeChange`의 **호출 순서와 역할 차이**

## ★ 질문

### 1. `LocalPredicted`가 정확히 뭘 예측하나? 클라가 미리 하는 일과 서버만 하는 일을 나눠라 ⚠️

**내 답** — 핑 간격 사이에 클라가 먼저 행동을 실행한다. 서버는 실행 가능한지 검증하고 안 되면 롤백한다.

**정답** — 앞은 맞다. **"롤백"이라는 단어가 문제다** (②와 이어진다).

`:53-69`가 답을 준다 — **둘은 다른 일을 한다. 같은 일을 두 번 하는 게 아니다.**

| | 하는 일 |
|---|---|
| **클라** (`:59-69`) | `PlayLocalMuzzleEffect` / `SpawnLocalCosmeticProjectile` — **연출만** |
| **서버** (`:53-57`) | `HandleServerFire` → SSR `ConfirmHitscan` → 데미지 — **판정만** |
| **양쪽 공통** (`:42`) | `CommitAbility` — 탄약 Cost GE + 쿨타임 GE |

**클라는 판정을 예측하지 않는다.** "맞았다"를 미리 말하지 않는다. 예측하는 건 **"쏠 수 있다"와 "탄약이 준다"** 둘뿐이다. 그래서 서버가 거부해도 되돌릴 게 별로 없다 — 애초에 되돌릴 수 있는 것만 예측했기 때문이다.

---

### 2. 예측이 틀렸을 때 되돌리는 코드가 이 프로젝트에 있나? 없다면 왜 티가 안 나나? ✅

**내 답** — 엔진 GAS 내부가 처리한다. `PredictionKey`로 이펙트와 무브를 찾아 롤백하지만 나머지 부수효과는 되돌아가지 않는다.

**정답** — 맞다. 정확하다. 줄 번호까지 붙이면:

```cpp
// AbilitySystemComponent_Abilities.cpp:2245-2298  ClientActivateAbilityFailed
FPredictionKeyDelegates::BroadcastRejectedDelegate(PredictionKey);  // ① 예측 GE 제거
Ability->CurrentActivationInfo.SetActivationRejected();             // ② 상태 표시
Ability->K2_EndAbility();                                           // ③ 종료
```

**①이 되돌리는 건 예측으로 적용한 GameplayEffect뿐이다.** 탄약 Cost GE가 여기 해당한다. `PlayLocalMuzzleEffect`가 스폰한 Niagara·Sound는 **GAS가 알지도 못하고 되돌리는 코드도 없다.**

**티가 안 나는 이유:** 총구 화염은 0.1초 만에 끝난다. 그 사이에 서버 거부가 돌아오지도 않는다. 그리고 애초에 거부될 일이 거의 없다 — `CanActivateAbility`(`:76`)에서 미리 막기 때문이다.

> **★ 그래서 "GAS에는 범용 롤백이 없다"가 정확한 문장이다.**
> GAS의 예측은 *"틀리면 되돌린다"*가 아니라 ***"되돌릴 수 있는 것만 예측한다"***에 가깝다.
> 되돌아가는 건 셋뿐 — **예측 GE / 예측 몽타주 / `K2_EndAbility()`.**

---

### 3. `:50`에서 왜 `GetServerWorldTimeSeconds()`인가? `GetWorld()->GetTimeSeconds()`를 쓰면? ❌

**내 답** — 클라 기준 서버 월드 시간 예측. 현재 클라 월드 시간을 쓰면 RTT/2만큼 차이난다.

**정답** — **RTT/2가 아니다. 임의값이다.**

`GetWorld()->GetTimeSeconds()`는 **그 월드가 시작된 뒤 흐른 시간**이다. 클라 월드는 클라가 레벨을 로드할 때 0에서 시작했고, 서버 월드는 서버가 켜질 때 시작했다. **서버가 10분 돌고 있었으면 600초 차이다.** RTT와 아무 상관이 없다.

```cpp
// GameStateBase.cpp:144-149
double AGameStateBase::GetServerWorldTimeSeconds() const
{
    return World->GetTimeSeconds() + ServerWorldTimeSecondsDelta;
}
```

`ServerWorldTimeSecondsDelta`가 그 임의 오프셋을 메워주는 값이다. RTT/2는 이 델타를 **주기적으로 보정할 때** 쓰이지, 두 시계의 차이가 아니다.

**깨지는 방식:** 조금 빗나가는 게 아니라 **아예 안 된다.** 리와인드가 수백 초 전을 찾다가 히스토리(0.5초) 밖으로 나가 전부 실패한다.

---

### 4. `HandleServerFire`가 받는 `Origin`을 서버가 검증하나? ✅

**내 답** — 클라 액터와 `Origin` 간 거리를 계산해 차이가 크면 막는다. 안 하면 허공에서 융단폭격이 가능해진다.

**정답** — 맞다. `EPCombatComponent.cpp` `constexpr float MaxOriginDrift = 200.f`.

> 한계도 같이 알아두면 좋다. **200cm 안에서는 여전히 자유롭다.** 벽 뒤에서 살짝 밀어 쏘는 건 막지 못한다. "완전 검증"이 아니라 **"터무니없는 값만 거른다"**가 정확한 표현이다.

---

### 5. `GetHitscanCandidates`로 후보를 먼저 추리는 이유는? ✅

**내 답** — 총알 궤적 주변 후보만 가져와 검증한다. 없으면 월드 내 모든 액터를 검증해야 할 수 있다.

**정답** — 맞다. 그리고 **비용이 두 겹**이라는 게 핵심이다.

리와인드는 단순 조회가 아니라 **본 하나하나의 물리 바디를 과거 위치로 실제로 옮기는 것**이다(`SetBodyTransform(TeleportPhysics)`). 후보를 안 추리면 **모든 캐릭터의 모든 본을 되감았다가 되돌려야** 한다. 브로드페이즈(패딩 50cm)로 후보를 먼저 줄이는 이유가 이것이다.

> 보너스 — 후보군은 **보안 장치**이기도 하다. `CandidateSet.Contains(HitChar)`로 후보 밖 히트를 차단한다.

---

### 6. `PreAttributeChange`와 `PostGameplayEffectExecute` 중 데미지는 어디서? ⚠️

**내 답** — `PostGameplayEffectExecute`. `PreAttributeChange`는 어트리뷰트 초기화용이다.

**정답** — 결론은 맞고 **이유가 틀렸다. `PreAttributeChange`는 초기화용이 아니라 클램프용이다.**

```cpp
// EPAttributeSet.cpp:11-25
void UEPAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    if (Attribute == GetHealthAttribute())
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    ...
}
```

**모든 변경 경로 앞에 서는 최종 관문**이다. GE든 직접 Set이든 전부 여기를 지난다.

**데미지를 여기서 못 하는 진짜 이유:** `PreAttributeChange`는 **누가 왜 바꾸는지를 모른다.** `NewValue` 하나만 받는다. 반면 `PostGameplayEffectExecute`는 `FGameplayEffectModCallbackData`로 **소스·타겟·Spec·Context를 전부** 받는다. "누가 때렸나"를 알아야 킬 크레딧을 주고 `EPGA_Death`를 띄울 수 있다.

**반대로 하면:** 클램프를 Post에 두면 **음수 체력이 한 프레임 존재하고** 그 사이 델리게이트가 음수를 들고 울린다. 데미지를 Pre에 두면 가해자를 몰라 킬 판정이 안 된다.

---

### 7. 사망 판정을 GE가 아니라 어빌리티(`EPGA_Death`)로 뺀 이유는? ⚠️

**내 답** — 죽음을 검증하는 과정이 필요해서?

**정답** — 검증이 아니라 **"죽음은 값이 아니라 절차"**라서다.

GE가 하는 일은 **수치를 바꾸는 것**뿐이다. 그런데 죽으면 해야 할 게 줄줄이 있다 — 몽타주 재생, 래그돌 전환, 입력 차단, 무기 드롭, 리스폰 타이머, GameMode에 `OnPlayerKilled` 통보. **이건 값 변경이 아니라 시간이 걸리는 절차다.**

어빌리티로 두면 공짜로 따라오는 것들:

- `State.Dead` 태그로 **다른 어빌리티를 자동 차단**한다 (`EPGA_Item_PrimaryUse.cpp:25` `ActivationBlockedTags`)
- **중복 실행이 막힌다** — 시체에 두 발 더 맞아도 사망 처리가 한 번만 돈다
- `ServerOnly`라 클라가 자기 죽음을 선언할 수 없다

> `EPGA_Death.cpp:12-14`를 보면 `ServerOnly` + `InstancedPerActor` + `bServerRespectsRemoteAbilityCancellation = false`다. **마지막 줄이 "클라가 자기 사망 어빌리티를 끝내달라고 요청해도 무시한다"**는 뜻이다.

---

### 8. 머리를 맞으면 데미지가 더 들어가는 경로는? 예전 방식은 왜 폐기됐나 ⚠️

**내 답** —
1. `EPWeaponDefinition` 상속 BP에 `TMap<FGameplayTag, float> TagDamageMultiplierMap` 세팅
2. `EPPhysicalMaterial`의 `MaterialTags`로 피직스 에셋에 태그 세팅
3. `HandleHitscanFire` → `FHitResult`에서 피직스 머티리얼 추출 → 태그 비교로 배율 추출
4. `ApplyGEDamage` → `GE_Damage`의 `TAG_Data_Damage`에 전달 → `IncomingDamage` 어트리뷰트로

폐기 이유는 이전 방식이 하드코딩되어 있었고 GAS 통합을 위해서.

**정답** — **경로 4단계는 전부 맞다. 잘했다.** 폐기 이유만 틀렸다.

**`BoneDamageMultiplierMap`은 하드코딩된 게 아니라 한 번도 채워진 적이 없다.**

```cpp
// EPWeaponDefinition.h — 커밋 158e8b1~1 기준
// 부위별 대미지(GAS 이후 태그 기반으로 수정)
TMap<FName, float> BoneDamageMultiplierMap;   // ← UPROPERTY가 없다
```

`UPROPERTY`가 없으니 에디터에 안 뜨고 직렬화도 안 된다. 그리고 저장소 전체에 **값을 넣는 코드가 한 줄도 없다** — 읽는 곳(`GetBoneMultiplier`)만 있다. **항상 1.0을 반환하고 있었다.**

머리가 아팠던 건 순전히 `UEPPhysicalMaterial`의 약점 배율 덕이었다. **다른 경로가 그럴듯한 결과를 만들어줘서 끝까지 몰랐다.**

**그래서 정확한 폐기 이유는:** *"하드코딩이라"*가 아니라 ***"작동하지 않는 걸 몰랐고, 태그로 옮기면서 `UPROPERTY`가 붙어서야 값이 보이기 시작했다"***. 세션 2 ①의 그 패턴이다.

> **⚠️ 블로그와 맞출 것.** `Step4_Post6_HitZoneDamage.md`와 `revise/posts/2026-03-14-EP_NetPrediction-3.md` 수정본에 이 사실이 적혀 있다. 여기 답과 두 글의 서술이 갈리면 안 된다.

## [비교] ★ 이 프로젝트 최대의 diff — RPC → GAS

```bash
git show feature-replication:EmploymentProj/Source/EmploymentProj/Public/Combat/EPCombatComponent.h
```

`:60-63`에 **`Server_Fire`와 `Server_Reload`가 살아 있다.** 지금은 없다.
사라진 자리에 `EPGA_Item_PrimaryUse` / `EPGA_Item_Reload`가 들어왔다.

**답할 것:** 직접 서버 RPC를 어빌리티로 바꿔서 **얻은 것**과 **잃은 것**을 각각 두 개씩.

**내 답** — 얻은 것: GAS를 사용한 확장성, GAS 자체 예측으로 인한 편리성? / 잃은 것: 오버헤드, 간단한 작업도 거칠 과정이 많음.

**정답** — 방향은 맞는데 **너무 추상적이다.** *"확장성"*은 면접에서 바로 되물어온다 — *"구체적으로 뭐가 확장되나요?"* 이렇게 답해야 한다.

**얻은 것**

1. **입력 경로가 하나가 됐다.** `Character`는 이제 "발사"가 뭔지 모른다. 태그 하나만 던진다(`EPCharacter.cpp:418`). 증거는 스킬 3종이 **똑같은 세 줄**로 붙었다는 것 (`:447`, `:453`, `:459`)
2. **FireRate가 코드에서 데이터로 갔다.** `LastServerFireTime` 수동 비교 → `GE_FireCooldown` + `SetByCaller`. 새 무기 = 새 DataAsset, **코드 무변경**
3. **상태가 복제되는 태그가 됐다.** `WeaponState` enum은 복제되지 않아 **다른 클라가 "쟤 재장전 중"을 알 수 없었다.** `State.Reloading`은 GE `GrantedTags`로 내려간다

**잃은 것**

1. **RPC가 늘었다.** 한 발에 활성화·타겟데이터·종료가 각각 나간다. **Ability Batching 미적용** — 발사 빈도가 높은 게임이라 적용 가치가 있는데 보류했다
2. **콜스택이 엔진 안으로 들어갔다.** `Server_Fire`는 F12 한 번이면 끝이었다. 지금은 `TryActivateAbilitiesByTag` → 엔진 → `ActivateAbility`
3. **흐름이 C++만 봐서는 안 보인다.** GE/GA가 BP 에셋이라 `GE_FireCooldown`의 실제 Duration은 에디터를 열어야 안다
4. **예측이 공짜가 아니다.** 되돌릴 수 있는 것과 없는 것을 직접 구분해서 코드를 배치해야 한다 (②·`CanActivateAbility` 이야기)

> **★ 이 표가 "GAS를 왜 쓰셨어요"의 답이다.** 교과서 문장(*"확장성이 좋아서"*)이 아니라 **내 코드에서 실제로 사라진 줄과 늘어난 RPC**로 답한다.

**막히면:** `DOCS/Notes/04/04_GAS_02_DamagePipeline.md`, `04_GAS_03_PrimaryUse.md`, `04_GAS_06_HitZoneDamage.md`
※ 8번은 `revise/posts/2026-03-14-EP_NetPrediction-3.md`에 **폐기된 이유**가 적혀 있다

---

# 세션 4 — 예측과 되감기 (2.5h)

**목표:** *"클라가 먼저 움직이는데 어떻게 안 갈라지나"*와 *"과거를 어떻게 되감나"*를 각각 3분씩 설명한다.

## 경로 A — 이동 예측

```
Private/Movement/EPCharacterMovement.cpp:50   FSavedMove_EPCharacter
                                        :74   GetCompressedFlags   ← 클라 → 서버
                                        :86   SetMoveFor
                                        :98   PrepMoveFor
                                        :108  CanCombineWith
                                        :42   UpdateFromCompressedFlags ← 서버가 받는 곳
                                        :45-46 FLAG_Custom_0 / _1
                                        :35   GetMaxSpeed
                                        :130  GetPredictionData_Client
```

## 경로 B — 되감기

```
Private/Movement/EPCharacterMovement.cpp:19   OnMovementUpdated  ← 스냅샷 타이밍의 시작점
Private/Combat/EPServerSideRewindComponent.cpp   (454줄, 통째로 읽어라)
    스냅샷 저장 → GetSnapshotAtTime → 보간 → 리와인드 → Narrow Trace → 복원
```

## ★ 질문

### 1. `GetCompressedFlags`는 몇 비트인가? 커스텀 플래그가 5개 필요해지면? ✅

**내 답** — 기본 4비트 + 커스텀 4비트 = 8비트. `FCharacterNetworkMoveData`를 상속해 `Serialize()` 필드에 싣는다.

**정답** — 맞다. 엔진 정의로 확인하면 (`CharacterMovementComponent.h:3059-3067`):

```cpp
FLAG_JumpPressed   = 0x01,   FLAG_WantsToCrouch = 0x02,
FLAG_Reserved_1    = 0x04,   FLAG_Reserved_2    = 0x08,
FLAG_Custom_0      = 0x10,   FLAG_Custom_1      = 0x20,
FLAG_Custom_2      = 0x40,   FLAG_Custom_3      = 0x80,
```

**커스텀은 4개(`0x10`~`0x80`)이고 우리는 2개를 썼다.** `Reserved_1/2`는 엔진용이라 손대면 안 된다.

> 확장 답도 맞다. 그리고 `FCharacterNetworkMoveData`는 비트가 아니라 **임의 타입**을 실을 수 있다는 게 진짜 장점이다 — 조준 각도 같은 float도 보낼 수 있다.

---

### 2. `bWantsToSprint`는 복제되나? 옆 사람 캐릭터의 질주 애니메이션은? ⚠️

**내 답** — 복제되지 않는다.

**정답** — 맞다. **그런데 질문의 뒷부분이 본체다.**

`bWantsToSprint`/`bWantsToAim`이 세팅되는 경로는 둘뿐 — 내 입력(`EPCharacter.cpp:347, 382`)과 서버의 `UpdateFromCompressedFlags`(`EPCharacterMovement.cpp:45-46`). **시뮬레이티드 프록시에는 세 번째 경로가 없다.**

> **다른 사람 화면에서 내 `bIsSprinting` / `bIsAiming`은 영원히 `false`다.** (`EPAnimInstance.cpp:31-32`)

**지금까지 안 보였던 이유:** `Speed`는 정직하게 온다. `FRepMovement`가 `Location`뿐 아니라 **`LinearVelocity`**를 같이 싣고, 스프린트 시 속도가 650이 되므로 속도 기반 블렌드스페이스는 정상으로 보인다.

**웅크리기는 왜 되나 — 엔진이 절반을 더 갖고 있다**

| | 의도 플래그 (클라→서버) | 복제되는 결과 상태 | 애님이 읽는 것 |
|---|---|---|---|
| 웅크리기 | `CMC::bWantsToCrouch` | **`ACharacter::bIsCrouched`** | `bIsCrouched` ✅ |
| 스프린트 | `CMC::bWantsToSprint` | **없음** | `bWantsToSprint` ❌ |
| 조준 | `CMC::bWantsToAim` | **없음** | `bWantsToAim` ❌ |

```
CMC::Crouch()  →  CharacterOwner->SetIsCrouched(true)   CharacterMovementComponent.cpp:3202
ACharacter::bIsCrouched  (replicatedUsing, COND_SimulatedOnly)   Character.h:544 / Character.cpp:1727
OnRep_IsCrouched()  →  프록시의 CMC 플래그를 역으로 복원        Character.cpp:400-416
```

**즉 "복제를 까먹었다"가 아니다.** `FSavedMove`/`UpdateFromCompressedFlags`는 크라우치를 정확히 베꼈고, 크라우치가 가진 **나머지 절반(복제되는 결과 상태)**이 없는 것이다.

> 조치는 `DOCS/BACKLOG.md` **B-8**. 먼저 확인할 것 — **애님 BP가 `bIsAiming`을 상체 포즈 분기에 실제로 쓰는가.** 쓰면 버그(즉시 처리), 안 쓰면 ADS 포즈 붙일 때 함께.

---

### 3. `CanCombineWith`가 하는 일은? 부모가 이미 `MaxSpeed`를 비교하는데 왜 우리 것이 필요한가? ⚠️

**내 답** — 한정된 시간 동안의 이동 정보를 하나로 통합한다. 달리기와 조준 bool 상태가 있기 때문에.

**정답** — 앞은 맞다. **뒤가 반만 맞다 — 질문의 함정이 "부모가 이미 비교하는데"에 있다.**

부모 `FSavedMove_Character::CanCombineWith`는 이미 `MaxSpeed`를 비교한다. 그러니 **지상 질주는 부모만으로 걸러진다** — 속도가 650 vs 600으로 다르니까.

**공중에서 무너진다.**

```cpp
// EPCharacterMovement.cpp:37
if (bWantsToSprint && IsMovingOnGround()) Base = SprintSpeed;
```

`IsMovingOnGround()` 때문에 **공중에서는 질주 여부와 무관하게 같은 `MaxSpeed`가 나온다.** 그래서 공중에서 플래그가 바뀐 두 무브가 부모 기준으로는 "같은 무브"로 보여 합쳐진다. 합쳐지면 플래그 전환이 사라지고, 착지 순간 서버와 클라의 속도가 갈린다.

> **우리 `CanCombineWith`가 실제로 값을 하는 건 공중뿐이다.** 이렇게 답하면 "코드를 읽고 답한 사람"으로 들린다.

---

### 4. `PrepMoveFor`는 언제 불리나? `SetMoveFor`와 짝인 이유는? ✅

**내 답** — 클라 이동을 롤백시키기 위해. `SetMoveFor`로 SavedMove에 저장하고 `PrepMoveFor`로 CMC에 복원한다.

**정답** — 맞다. 시점을 더 정확히 하면:

```
평시    SetMoveFor  → 매 무브마다 현재 CMC 상태를 SavedMove에 박제
보정 시 PrepMoveFor → 서버 교정 후, 저장해둔 무브들을 재생(replay)하기 직전
                     각 무브의 상태를 CMC에 되살린다
```

**핵심은 "재생"이다.** 서버가 "네 위치는 여기야"라고 고쳐주면, 클라는 그 지점부터 **아직 서버가 처리 못 한 무브들을 다시 실행**한다. 그때 각 무브 당시의 `bWantsToSprint`를 복원하지 않으면 재생 결과가 원래와 달라진다. `SetMoveFor`가 박제, `PrepMoveFor`가 해동이다.

---

### 5. ★ 스냅샷을 왜 `TG_PostPhysics`에서 찍나? 시각은 왜 `OnMovementUpdated`에서 넘겨받나? ❌

**내 답** — 해당 틱그룹에서 모든 이동 계산이 완료되기 때문. 서버에서 이동 정보를 받으면 뚝뚝 끊긴 상태로 받기 때문에 그 순간을 포착하기 위함.

**정답** — **두 질문 다 답이 아니다. 그리고 이게 이 프로젝트에서 제일 값어치 있는 이야기다.**

**두 개를 분리해야 한다. 이유가 서로 다르다.**

**(a) 왜 `TG_PostPhysics`인가 — 본 때문이다**

리와인드는 캡슐이 아니라 **본별 히트박스**를 되감는다. 본 Transform은 애니메이션과 물리가 돈 **뒤에야** 그 프레임의 최종값이 된다. `TG_PrePhysics`에서 찍으면 몸통은 이번 프레임인데 팔다리는 지난 프레임 포즈인 스냅샷이 나온다.

**"이동 정보를 끊겨서 받는 것"과 무관하다. 서버 자신의 본이 아직 갱신 전이기 때문이다.**

**(b) 왜 시각은 `OnMovementUpdated`에서 받나 — 월드 시간이 프레임당 한 번만 전진하기 때문이다**

```cpp
// LevelTick.cpp — UWorld::Tick
1545:  BroadcastTickDispatch(...);        // ServerMove RPC 처리 → OnMovementUpdated
1581:      TimeSeconds += DeltaSeconds;   // ← 시간은 '그 뒤'에 전진
1749:  RunTickGroup(TG_PostPhysics);      // ← SSR은 '그 다음'
```

`GetServerWorldTimeSeconds()`는 `World->TimeSeconds`를 그대로 읽는다(`GameStateBase.cpp:144-149`). 그래서 **같은 틱인데도**:

- `TickDispatch`(1545)에서 읽으면 → **직전 프레임의** 시각
- `TG_PostPhysics`(1749)에서 읽으면 → **이번 프레임의** 시각

**차이가 정확히 한 프레임(60fps면 ≈16.6ms)이다.**

> **★ 이 세 줄짜리 엔진 근거를 대느냐 못 대느냐가 이 프로젝트에서 제일 큰 차이를 만든다.**
> 결론(*시각은 CMC에서, 본은 PostPhysics에서*)은 처음부터 맞았다. **근거만 틀렸었다.**
> 예전에 "0.025ms 차이"라고 적었던 것도 같은 이유로 틀렸다 — 단위도 자릿수도 아니고, **한 프레임**이다.
> 그리고 올바른 근거가 훨씬 강하다. *"항상 정확히 한 틱"*이라는 관찰을 0.025ms로는 설명할 수 없지만 이것으로는 설명된다.

---

### 6. 30ms 고정 간격일 때 오차가 242cm였다. 원인은? 지금은 얼마인가? ❌

**내 답** — 위와 동일하게 서버에서 이동 정보를 묶어서 받아 끊겨 보이기 때문. 지금은 10cm 이내.

**정답** — 원인도 수치도 틀렸다. **지금은 2.3cm다.**

**원인:** 30ms 타이머는 **틱과 무관하게** 돌았다. 타임스탬프는 찍는 순간의 현재 시각이라 늘 최신인데, 위치는 그 순간 액터에서 읽은 값이고 그건 **그 프레임의 이동이 반영되기 전** 값일 수 있었다.

> **결과: "시각은 t인데 위치는 t−1프레임"인 스냅샷이 저장됐다.**

보간은 그 스냅샷들을 믿고 계산하니 **한 칸씩 밀린 위치**를 낸다. 리와인드 창 0.5초, 스냅샷 간격 16.6ms니 **한 프레임 오차 = 스냅샷 한 칸**이다.

**고친 방식:** `CMC::OnMovementUpdated` → `MarkPositionUpdated()` 플래그 → SSR이 `TG_PostPhysics`에서 저장. **"움직였을 때 찍는다"로 바꾼 것**이지 간격을 줄인 게 아니다.

**242cm → 2.3cm. 약 100배.** 이 숫자를 외워둬라 — 면접에서 "무엇을 얻었나"에 답할 수 있는 몇 안 되는 수치다.

---

### 7. `ClientFireTime`을 서버가 믿나? 조작하면 뭘 할 수 있고 뭘 못 하나? ✅

**내 답** — 완전히 믿지는 않고 차이값을 확인한다. 조작하면 발사 시간을 조절할 수 있지만 너무 먼 값으로는 안 된다.

**정답** — 맞다. 경계를 구체적으로 하면 — 리와인드 창이 **0.5초**(`MaxRewindSeconds`)라 그 밖의 값은 잘린다.

**할 수 있는 것:** 0.5초 안에서 자기에게 유리한 순간을 고른다. 상대가 엄폐로 들어가기 직전 시점을 지정하면 벽 뒤의 적을 맞힐 수 있다.
**못 하는 것:** 5초 전으로 돌아가기, 미래 지정.

> **이게 랙 보상의 본질적 한계다.** 없앨 수 없고 **창을 좁히는 것**만 가능하다. 0.5초는 "핑 250ms까지 커버"와 "치트 여지"를 맞바꾼 값이다.

---

### 8. 랙 보상은 불공정을 없애나? 안 없앤다면 누구에게 옮겨가나? ✅

**내 답** — 공격받는 사람. 안 없애면 공격한 사람.

**정답** — 맞다. 짧은데 정확하다. 풀어 쓰면:

**랙 보상 없이는** 핑 높은 **공격자**가 손해를 본다 — 보이는 대로 쐈는데 안 맞는다.
**랙 보상을 넣으면** 그 손해가 **피격자**에게 간다 — 엄폐 뒤로 들어왔는데 맞는다("behind cover death").

**불공정은 사라지지 않고 이동한다.** 이 프로젝트가 **공격자 쪽 편의를 택한** 이유는 추출 슈터라 교전이 짧고, "쐈는데 안 맞는다"가 "숨었는데 맞았다"보다 이탈을 더 유발하기 때문.

> 이 답은 **기술이 아니라 판단을 묻는 질문**이다. "없앤다"고 답하면 트레이드오프를 모르는 사람이 된다.

## [비교] 242cm 버그가 고쳐지는 순간

```bash
git log --oneline feature-netprediction -- EmploymentProj/Source/EmploymentProj/Private/Combat/EPServerSideRewindComponent.cpp
git show <위에서 고른 커밋>
```

30ms 고정 인터벌이 빠지고 `OnMovementUpdated` → 플래그 → `TG_PostPhysics` 경로가 들어온 **그 커밋**을 찾아라. (`c49532b` — *feat: SSR 스냅샷 타이밍 보정*)

**답할 것:** 왜 타임스탬프는 멀쩡한데 위치만 틀렸나. 그리고 오차가 왜 하필 *"항상 정확히 한 틱"* 이었나.

**내 답** — 시간은 달라져도 다른 클라의 위치가 연속이 아닌 끊어져서 나오기 때문. 한 틱마다 위치가 재계산되기 때문.

**정답** — ⑥과 같은 오해다. **네트워크 탓이 아니다.**

**타임스탬프가 멀쩡했던 이유:** 타이머가 찍는 순간의 시각을 그대로 넣었으니 당연히 맞다.
**위치만 틀렸던 이유:** 같은 순간에 액터에서 읽은 위치가 **그 프레임의 이동 반영 전** 값이었다.

**"항상 정확히 한 틱"이었던 이유가 결정적 단서다.** 네트워크 지터라면 오차가 **들쭉날쭉**해야 한다. 항상 일정하다는 건 **구조적 오프셋**이라는 뜻이고, 그러면 원인은 코드 안에 있다.

> **★ 이 추론 과정이 답보다 중요하다.** *"오차의 분포를 보고 원인의 종류를 좁혔다"* — 증상 → 가설 → 반증 → 계측 → 원인이 다 들어 있는 디버깅 서사다. 3분 설명 소재로 제일 좋다.

**막히면:** `revise/posts/2026-03-09-EP_NetPrediction-1.md`, `2026-03-14-EP_NetPrediction-2.md`, `DOCS/Mine/Rewind.md`

---

# 세션 5 — 흐름 ② 바닥 → 손 (3h)

**목표:** 아이템이 생겨서 손에 들리기까지를 설명한다. **이건 다음에 만들 인벤토리의 설계 이해와 같은 작업이다.**

## 경로

```
1  Private/Core/EPGameMode.cpp:140              HandleMatchHasStarted → SpawnLoot
2  Private/Loot/EPItemSpawner.cpp:36            SpawnLoot
3  Public/Loot/EPLootTable.h                    가중치 + 중첩 SubTable
4  Private/Loot/EPPickup.cpp:39                 InitPickup   ← 스폰과 같은 프레임
5                             :82,:87           OnRep_ItemId → ApplyVisual
6  Private/Interaction/EPInteractionComponent.cpp:54  UpdateFocus (클라 포커스)
7                                                :24  Input_Interact
8  Private/GAS/EPGA_Interact.cpp:18-21          GameplayEvent 트리거
9                               :30-34          서버가 아니면 즉시 종료
10                              :52             Data->Target
11                              :69-75          거리 재검증
12                              :77-79          CanInteract → OnInteract
13 Private/Loot/EPPickup.cpp:56,:66             CanInteract / OnInteract
14 Private/Data/EPItemDefinitionSubsystem.cpp:44,:50  FindDefinition / MakeItemState
```

## 3계층을 손으로 그려라

```
FEPItemData (DataTable 행)  →  UEPItemDefinition (DataAsset)  →  FEPItemState (런타임 값)
                    ↑ 셋을 잇는 것이 ItemId (FName)
```

## ★ 질문

### 1. 왜 셋으로 나눴나? 하나로 합치면 뭐가 안 되나? 판정선은? ✅

**내 답** — `FEPItemData`는 디자이너가 DT에서 밸런스를 맞추기 위함. `UEPItemDefinition`은 다양한 에셋 연결부. `FEPItemState`는 아이템 현재 상태값. 합치면 아이템을 버렸을 때 탄창에 남은 탄약값이 사라진다.

**정답** — 맞다. **판정선을 한 줄씩 붙이면 완성된다.**

| 계층 | 판정선 | 왜 |
|---|---|---|
| `FEPItemData` (DT 행) | **숫자, 표로 비교하고 싶은 것** | 엑셀처럼 한눈에 밸런스를 본다 |
| `UEPItemDefinition` (DA) | **에셋 참조 / `virtual` / 타입 전용 필드** | DT 행은 에셋을 들면 전부 로드된다. DA는 필요할 때만 |
| `FEPItemState` (런타임) | **인스턴스마다 다른 값** | 같은 AK라도 잔탄이 다르다 |

**합치면 안 되는 이유 두 겹:**
1. **DT+DA를 합치면** — 행 하나가 메시·사운드·애님을 들고 있어 테이블을 여는 순간 전부 메모리에 올라온다
2. **Definition+State를 합치면** — 쓰신 답 그대로. 정의는 **아이템 종류당 하나**, 상태는 **아이템 개수당 하나**다

---

### 2. `InitPickup`을 `SpawnActor`와 같은 프레임에 불러야 하는 이유는? ✅

**내 답** — 클라에서 액터가 스폰된 후 영원히 휴면에 들어간다. 휴면 전에 초기화해야 한다.

**정답** — 맞다. 정확히는 이렇다.

`DORM_Initial` 액터는 **최초 복제 한 번만** 나가고 그 뒤로는 갱신이 없다. 그 최초 번들에 실리는 건 **그 순간의 프로퍼티 값**이다.

**한 프레임 늦으면:** 클라는 `ItemId`가 기본값(`NAME_None`)인 상태로 액터를 받는다. → `OnRep_ItemId`가 안 울리거나 빈 값으로 울린다 → **`ApplyVisual`이 아무 메시도 못 붙인다.** 그리고 갱신이 안 오니 **영원히 투명한 픽업**이 바닥에 남는다.

> 서버 화면에선 멀쩡하다. **데디 서버에서만 드러나는 종류**다.

---

### 3. 상호작용은 왜 어빌리티인가? 드랍은 왜 직접 RPC인가? 가르는 기준은? ⚠️🔲

**내 답** — 1. GAS 통합 여부로 혼란을 주지 않기 위해 2. GAS를 재사용할 수 있어서 3. 모르겠다. 판단 기준은 모르겠다.

**정답** — 세 가지는 이렇다 (`Status/GAS_STATUS.md` 7차 검수).

1. **이미 모든 게임플레이 입력이 어빌리티 태그로 간다** (`EPCharacter.cpp:388-435`). 상호작용만 직접 RPC면 그 하나가 예외가 된다
2. **`ActivationBlockedTags`가 공짜로 필요하다** — 죽은 상태(`State.Dead`)에서 줍기가 막혀야 하고, 이건 어빌리티면 한 줄이다
3. **채널링이 예정돼 있다** — 상자 열기에 `CastTime`이 붙는다. GE 쿨다운·중단 처리가 전부 GAS에 이미 있다

**★ 가르는 기준 한 문장:**

> ***"게임플레이 규칙(차단·쿨다운·채널링)이 걸리면 어빌리티, 단순 상태 변경이면 RPC."***

**드랍이 반대인 이유:** 버리기에는 그런 게 **하나도 없다.** 시간도 안 걸리고, 쿨다운도 없고, **죽으면서 버릴 수도 있어야 한다** — 오히려 `State.Dead`에 막히면 안 된다. 어빌리티로 만들면 얻는 게 없고 경로만 길어진다.

---

### 4. `:69-75`에서 서버가 거리를 다시 재는 이유는? 클라가 이미 쟀는데 ✅

**내 답** — 클라는 요청, 서버는 결정이기 때문이다.

**정답** — 맞다. 한 줄로 정확하다. 구체적으로는 — 클라의 `UpdateFocus`(`EPInteractionComponent.cpp:54`)는 **UI를 띄우기 위한** 계산이다. 그 결과를 서버가 믿으면 **클라가 "저 멀리 상자에 닿았다"고 주장**할 수 있다.

> 세션 3 ④(`Origin` 200cm 검증)와 **같은 원칙의 다른 사례**다. *"클라가 보낸 위치·거리·시각은 전부 주장이다."* 면접에서 이 셋을 묶어 말하면 원칙을 이해한 걸로 들린다.

---

### 5. 픽업이 `DORM_Initial`인데 `FlushNetDormancy()`가 필요 없는 이유는? ⚠️

**내 답** — `SpawnActor`를 쓰기에 `DORM_Initial`의 효과를 받지 못하고, 첫 복제 이후 항상 휴면과 동일한 상태를 유지하기 때문이다.

**정답** — 뒤는 맞고 **앞이 틀렸다.** `SpawnActor`로 만들어도 `DORM_Initial`은 그대로 작동한다.

**진짜 이유:** `FlushNetDormancy()`는 **휴면 중인 액터의 값이 바뀌어서 지금 당장 내보내야 할 때** 부르는 것이다. 그런데 픽업은 **최초 복제 이후 값이 바뀌지 않는다.** `ItemId`는 `InitPickup`에서 한 번 정해지고 끝이다.

> **보낼 게 없으니 깨울 이유가 없다.** ②와 한 쌍이다 — *"같은 프레임에 초기화한다"*와 *"그 뒤로 안 바뀐다"*가 합쳐져야 `DORM_Initial`이 성립한다. 나중에 잔탄이 바뀌는 픽업을 만들면 그때는 `FlushNetDormancy()`가 필요해진다.

---

### 6. 한 번 본 픽업이 멀어져도 클라에 남는 게 정상인 이유는? 🔲

**내 답** — 위와 같은 이유. 릴러번시와 휴면의 관계는 모르겠다.

**정답** — **둘은 다른 축이다.**

| | 묻는 것 |
|---|---|
| **릴러번시** | *"지금 이 클라에게 **보낼 대상**인가"* |
| **휴면** | *"보낼 **게** 있는가"* |

**핵심:** 릴러번시를 잃어도 **이미 만들어진 액터는 클라에서 지워지지 않는다.** 갱신만 멈춘다. (`bAlwaysRelevant`, `NetCullDistanceSquared` 등으로 조절)

그래서 한 번 본 픽업은 멀어져도 남아 있고, 다시 가까워지면 그 사이 변경분을 받는다. **매번 지웠다 만들면 그게 훨씬 비싸다.**

> **주의할 점:** 그 사이에 **서버에서 누가 주워 갔으면** 액터 파괴가 릴러번시 밖이라 안 왔을 수 있다. 다시 가까워질 때 정리된다. **"클라에 보이는 픽업이 실제로 거기 있다는 보장은 없다"** — 그래서 `:69-75`에서 서버가 다시 검증하는 것이다 (④와 연결).

---

### 7. 인벤토리를 `FFastArraySerializer`로 만드는 이유는? 8. `EntryId`가 재번호되면 안 되는 이유는? 🔲

**내 답** — 아직 문서를 안 봐서 모른다.

**정답** — **지금 외우지 마라.** 인벤토리 구현(Step 03) 직전에 보는 게 낫다. 그때 보면 설계 근거로 바로 쓰인다. 다만 방향만:

- **`FFastArraySerializer`** — 일반 `TArray`는 원소 하나가 바뀌면 **배열 전체**를 다시 보낸다. FastArray는 **바뀐 원소만** 보내고 `PreReplicatedRemove`/`PostReplicatedAdd` 콜백을 준다
- **`EntryId` 안정성** — 배낭·부착물이 **부모 `EntryId`를 참조**한다. 재번호되면 그 참조가 전부 엉뚱한 아이템을 가리킨다. **나중에 넣기 제일 비싼 종류(CLAUDE.md §2)라 처음부터 지켜야 한다**

**읽을 곳:** `Status/LOOT_STATUS.md` 결정표 → `05_Loot_03_*.md`

---

### 9. `GrantedAbility`(`.h:38`)는 왜 쓰이지도 않는데 Step 00에 들어갔나? ✅

**내 답** — 추후 총, 도끼, 붕대처럼 아이템마다의 어빌리티를 적용하기 위해서.

**정답** — 맞다. **§2의 "상상한 확장점"과 뭐가 다른지**가 이 질문의 후반부다.

> **판정 기준: *"이 확장점이 `DOCS/` 어딘가에 이름으로 적혀 있는가?"***

`GrantedAbility`는 적혀 있다 — `05_Loot_DOCS.md:561`(*"루트 테이블에 붕대·회복키트가 들어가는데 사용할 방법이 없다. 구현하지는 않되 자리는 잡는다"*), `05_Loot_00_ItemCore.md:889`, `GAME.md:77`. **두 번째 소비자가 문서에 예고돼 있다.**

반면 `AEPEquippable` 베이스 클래스(BACKLOG **B-7**)는 **파생이 하나뿐이라 안 만든다.** 같은 "미래 대비"인데 하나는 만들고 하나는 안 만드는 이유가 이 판정선이다.

---

### 10. `case Hitscan: default:`에 `Melee`를 추가하면? 한쪽만 고치면 각각 어떤 증상? ❌

**내 답** — CombatComponent, PrimaryUse 둘 중 하나에만 Melee를 추가하면 작동하지 않는다.

**정답** — **질문의 함정을 놓쳤다.** `EEPBallisticType`은 enum 하나라 **추가하면 양쪽이 자동으로 새 값을 본다.** "한쪽에만 추가"라는 건 없다.

문제는 반대다. **양쪽 다 새 값을 보는데, 아무도 처리하지 않는다. 그리고 증상이 비대칭이다.**

| | 코드 | `Melee` 추가 시 증상 |
|---|---|---|
| **서버** `EPCombatComponent.cpp:71-73` | `case Hitscan: default:` | **도끼가 히트스캔으로 발사된다.** `default:`가 있어 `-Wswitch` 경고도 안 뜬다 |
| **클라** `EPGA_Item_PrimaryUse.cpp:66` | `if (== ProjectileFast)` | 아무 연출도 안 난다. **무증상** |

**한쪽은 오작동, 한쪽은 침묵.** 그래서 디버깅이 어렵다 — 클라는 조용하니 서버 로직만 의심하게 된다.

> `BACKLOG.md` **B-3**(`default:` 분리) / **B-4**(같은 enum을 두 경로가 따로 봄). B-3은 **두 줄이고 지금이 제일 싸다.**

---

### 11. 붕대는 `AEPWeapon`이 아니다. `GetEquippedWeapon()`을 안 거치려면 무엇이 진실의 원천이어야 하나? ✅

**내 답** — 착용 중인 아이템의 id?

**정답** — 맞다. **인벤토리 엔트리**다.

> **★ 2026-08-22 갱신 — 형태가 한 번 더 바뀌었다.** 이 답을 적을 때는 *"`EquippedEntryId`라는 `int32` 필드"* 였다. 슬롯이 2개에서 **12개**로 늘면서(핫바 4 + 착용 8) **필드가 사라지고 `FEPInventoryEntry::SlotId`가 진실이 됐다.** 필드 방식이면 필드가 12개가 되거나 `TMap`을 따로 복제해야 하는데, `SlotId`는 **이미 엔트리 안에 있고 이미 복제된다.** 근거·반례 검토는 `DOCS/Mine/EquipmentSlots.md` §3·§11.

지금은 `UEPCombatComponent::EquippedWeapon`(`AEPWeapon*`)이 유일한 소스라 진실처럼 보인다. **붕대는 `AEPWeapon`이 아니라 여기 못 들어온다.**

```
지금     진실 = AEPWeapon*              →  무기가 아닌 것은 표현 불가
바꿀 것  진실 = 엔트리의 SlotId          →  무기 액터는 거기서 나오는 파생값
         남는 상태 = ActiveHotbarIndex   →  "1번과 2번 중 어느 쪽을 들었나"
```

**왜 `ActiveHotbarIndex`만 남는가.** `SlotId`는 *"이 총은 1번 칸에 있다"* 를 말하지만 *"지금 손에 든 것이 1번인가 2번인가"* 는 말하지 못한다. 그건 파생값이 아니라 독립된 상태라 필드로 남는다. **그리고 이 필드는 엔트리가 아니라 슬롯을 가리키므로 죽은 번호가 생기지 않는다** — 1번의 총을 버려도 인덱스는 0으로 남고 조회가 `INDEX_NONE`을 돌려준다.

**★ Step 05에서 지킬 것:** 새 코드의 진입점으로 `GetEquippedWeapon()`을 쓰지 않는다. `GetEquippedEntryId()`를 먼저 만들고 무기 쪽이 그걸 통해 액터를 찾게 하면 **B-7이 거의 공짜**가 된다. (`BACKLOG.md` **B-5** — *안 지키면 나중이 비싸지는 유일한 항목*) **9차 이후로는 더 싸다** — `GetEquippedEntryId()`가 Step 03에서 파생 게터로 이미 선언된다.

## [비교] UObject를 struct로 갈아엎은 이유

```bash
git show feature-gas:EmploymentProj/Source/EmploymentProj/Public/Data/EPItemInstance.h
```

`feature-gas`에는 `UEPItemInstance` / `UEPWeaponInstance`가 있고 **지금은 없다** — `FEPItemState`(USTRUCT)로 전부 대체됐다.

**내 답** — 내부 하나의 값이 바뀌면 UObject 전체를 복제해야 한다. struct는 값 자체를 들고 온다?

**정답** — 방향은 맞는데 **진짜 비용은 다른 데 있다.**

**UObject를 인벤토리에 담으면:**

1. **서브오브젝트 등록이 필요하다** — `ReplicateSubobjects()`를 오버라이드해 인스턴스 하나하나를 등록해야 한다. 안 하면 **아무것도 복제되지 않는다**
2. **포인터라서 리졸브 순서 문제가 생긴다** — 배열은 도착했는데 가리키는 객체가 아직 안 온 상태가 존재한다. 클라에서 한 프레임 `nullptr`이 보인다
3. **개수만큼 오브젝트가 생긴다** — 아이템 40개면 UObject 40개. GC 부담

**struct로 두면 셋 다 사라진다.** 값이 배열 안에 **인라인**이라 등록할 서브오브젝트도, 리졸브할 포인터도 없다. 그리고 `FFastArraySerializer`를 붙여 **바뀐 원소만** 보낼 수 있다.

**★ "호출처가 0이라 비용 없이 갈아엎었다"가 왜 다행이었나:**

`UEPItemInstance`는 **한 번도 복제된 적이 없었다.** `Replicated` 지정도, `GetLifetimeReplicatedProps`도, 서브오브젝트 등록도 **전부 없었다.** 즉 **데드코드였다.**

> 세션 2 ①의 그 패턴이 또 나온다. 다만 이번엔 **운이 좋았다** — 쓰이기 전에 발견돼서 지우는 데 비용이 0이었다. `BoneDamageMultiplierMap`은 세 편의 블로그에 "작동한다"고 쓴 뒤에 발견됐다. **같은 실수인데 발견 시점이 값을 갈랐다.**

**막히면:** `DOCS/Notes/05/Status/LOOT_STATUS.md`(결정표가 답의 절반), `05_Loot_00_ItemCore.md`, `05_Loot_02_Interaction.md`, `Review/05_Loot_REVIEW7·8_*.md`

---

# 세션 6 — 자가 점검 (1.5h)

**소스를 닫고** 백지에 한다. 이게 실제 면접 조건이다.

> **★ 1회전 결과를 반영한 변경 — 그림 말고 "왜"를 먼저 적어라.**
> 틀린 10개 중 6개가 *"결론은 맞고 이유가 다른"* 형태였다. **그림은 이미 그릴 수 있다. 못 하는 건 그림이 아니다.**

## A. 백지 그리기 (40분)

1. **흐름 ①**을 화살표로 그린다. 클래스 이름만으로. 서버/클라가 갈리는 지점에 선을 긋는다
2. **흐름 ②**를 같은 방식으로
3. 그린 다음 소스를 열어 **빠뜨린 것**을 다른 색으로 채운다 — 빠뜨린 게 곧 안 잡힌 부분이다
4. **★ 각 화살표 옆에 "왜 여기서 갈리나"를 한 단어씩 적는다.** 이게 못 적히는 화살표가 진짜 구멍이다

## B. 3분 설명 (30분)

타이머를 켜고 **소리 내어** 말한다. 셋 중 아무거나 하나:

- "랙 보상을 어떻게 구현하셨어요?"
- "GAS를 왜 쓰셨어요? 안 쓰면 뭐가 힘든가요?"
- "이 프로젝트에서 제일 어려웠던 게 뭐였어요?"

**말이 끊기는 지점이 곧 안 잡힌 지점이다.** 그 지점만 소스로 돌아간다.

> **첫 번째 질문은 세션 4 ⑤·⑥·[비교]가 통째로 답이다.** 지금 그 셋이 전부 ❌다. **여기부터 다시 답해봐라** — 다른 건 틀려도 되는데 이건 이 프로젝트에서 제일 잘 팔릴 이야기라, 여기만 정확하면 나머지 몇 개를 상쇄한다.
>
> 두 번째 질문은 **세션 3 [비교]의 얻은 것/잃은 것 표**가 답이다.

## C. ❌·⚠️ 재답변 (20분)

**우선순위 — 이 순서로 다시 답해봐라.**

| | 항목 | 왜 |
|---|---|---|
| 1 | **4-⑤ ⑥ [비교]** | 이 프로젝트 최고의 이야기인데 셋 다 틀렸다 |
| 2 | **3-③** (시계) | 4-⑤와 한 뿌리다. 같이 잡힌다 |
| 3 | **3-⑧** (본 배율) | **블로그 세 편과 서술이 갈린다.** 안 맞추면 모순이 남는다 |
| 4 | 3-⑥ (Pre/Post) | 이유만 갈아 끼우면 된다 |
| 5 | 2-②③, 5-⑩ | 결론은 이미 안다 |
| 6 | 4-②③ | 오늘 답이 적혔으니 읽고 넘어가도 된다 |

**세션 5 ⑦⑧은 하지 마라.** 인벤토리 구현 직전에 보는 게 낫다.

---

## 2일 뒤 도달점

- [x] 세션 1~5 1회전 — 31문항 답변 완료
- [ ] 흐름 ① ②를 백지에 그린다
- [ ] "서버에서만 도는 것"을 5개 이상 즉답한다
- [ ] 이 프로젝트의 **실패담** 두 개를 수치와 함께 말한다 (**242cm → 2.3cm** / 본 배율이 왜 죽었나)
- [ ] ❌ 4개를 다시 답해서 ✅로 바꾼다 — 특히 **4-⑤⑥**
- [ ] 못 푼 질문 목록이 손에 있다

**마지막이 제일 중요하다.** 못 푼 질문을 아는 것과 모르는 것의 차이가, 면접에서 *"거기까진 확인 못 했습니다"* 와 *침묵*의 차이다.

---

## 2회전에서 확인할 것 (에디터 필요 / 미해결)

| | 내용 | 어디 |
|---|---|---|
| 🔲 | 애님 BP가 `bIsAiming`을 **상체 포즈 분기에 쓰는가** — 쓰면 실제 버그다 | `BACKLOG.md` B-8 |
| 🔲 | `EquipWeapon`과 `OnRep_EquippedWeapon`의 **중복 코드** — 정리할지 | 세션 2 ③ |
| 🔲 | `FFastArraySerializer` / `EntryId` | 인벤토리 구현 직전 |
