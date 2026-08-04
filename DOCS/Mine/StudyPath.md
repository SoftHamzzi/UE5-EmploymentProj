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
질문      — ★ 여기가 본체. 답이 적혀 있지 않다. 내가 답한다
```

**질문에 답이 없는 건 실수가 아니다.** 답을 읽으면 안 남는다. 막히면 그 자리에서 소스를 열고, 그래도 안 되면 아래 "막히면 볼 곳"으로 간다. **3분 넘게 막히는 질문은 표시해두고 넘어간다** — 세션 6에서 몰아서 처리한다.

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

| | 세션 | 시간 |
|---|---|---|
| **1일차** | 1. 뼈대 — 누가 무엇을 소유하는가 | 1.5h |
| | 2. 복제 — 값이 어떻게 남에게 가는가 | 2.5h |
| | 3. **흐름 ①** 좌클릭 → 사망 | 3h |
| **2일차** | 4. 예측과 되감기 | 2.5h |
| | 5. **흐름 ②** 바닥 → 손 | 3h |
| | 6. 자가 점검 (백지 테스트) | 1.5h |

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

1. **ASC를 왜 Character가 아니라 PlayerState에 뒀나?** 캐릭터에 뒀다면 무엇이 깨지나?  
   *힌트: 죽으면 캐릭터 액터는 어떻게 되고, PlayerState는 어떻게 되나*  
내 풀이: ASC를 Character에 두면 플레이어 리스폰시, ASC가 초기화된다. PlayerState는 캐릭터 액터를 파괴해도 유지된다.
2. `EPCharacter.h:143`의 `ASC`는 소유하는 포인터인가 빌려온 포인터인가? **GC는 이걸 어떻게 보나?**  
내 풀이: PlayerState에서 빌려온 포인터이다. GC는 TObjectPtr이기에 볼수 있다?(정확히 모름)
3. **GameMode에 접근하려는 코드가 클라이언트에서 돌면 무슨 일이 일어나나?** 크래시인가 nullptr인가, 그리고 왜?  
내 풀이: nullptr일것같다. GameMode는 서버 전용이기에, 클라이언트로 복제되지 않는다.
4. `ChoosePlayerStart`는 언제 누가 부르나? 이게 서버에만 있어도 되는 이유는?  
내 풀이: GameState에서 게임 시작 흐름에 도달했을때 호출한다. 정확히 어디서 호출하는지는 모르겠다. 서버에만 있어도 되는 이유는 플레이어는 스폰될 위치만 알고 있으면 되기 때문?
5. 매치 타이머를 GameMode가 아니라 GameState에 둬야 하는 값은 뭐고 왜인가?  
내 풀이: int32 RemainingTime? GameState는 클라이언트에 복제되기에 남은 시간을 확인할 수 있다.

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

1. `UPROPERTY(Replicated)`만 적고 `GetLifetimeReplicatedProps`에 등록을 안 하면 어떻게 되나? **컴파일 에러? 런타임 경고? 조용히 무시?**
   *이 프로젝트에 실제로 그런 사례가 있었다 — `revise/posts/2026-02-27-EP_Replication-4.md`*  
답변: 조용히 무시되며, 복제되지 않는다.
2. 바닥에 떨어진 무기의 **남은 탄약을 복제하면 안 되는 이유**는? (`EPPickup.cpp:75`가 왜 `ItemId`만 보내나)  
답변: 습득할떄 해당 무기 액터를 삭제해야하기에, 복제한 남은 탄약 변수 참조도 소실된다.
3. **`OnRep_`은 서버에서도 불리나?** 안 불린다면, 서버에서 같은 처리를 하려면 어떻게 하나? `EPCombatComponent.cpp:144`가 그 예다  
답변: OnRep_는 클라이언트에서 호출된다. 서버에서는 FUNCTION(Server, Reliable)이 붙은 함수를 호출해주어야한다.
4. Multicast RPC는 **나중에 접속한 사람**에게 어떻게 되나? 그래서 뭘 Multicast로 보내면 안 되나?  
답변: 전송되지 않는다. 정확히 모르겠다.
5. `COND_OwnerOnly` / `COND_SkipOwner` — 이 프로젝트에서 각각 어디 쓰였고, **바꿔 달면 뭐가 깨지나?**  
답변: 클라본인에게만 보여야하는 정보, 다른 사람들에게 보여줘야하는 정보. 뭐가 깨지는지는 모르겠다.
6. Attribute는 왜 `float`이 아니라 `FGameplayAttributeData`인가? `OnRep_`이 `OldValue`를 받는 이유는?  
답변: GAS와 연동해야하기 때문에. OldValue를 받는 이유는 모르겠다.

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

1. **`LocalPredicted`가 정확히 뭘 예측하나?** 이 어빌리티에서 클라가 미리 하는 일과 서버만 하는 일을 나눠서 말해봐라 (`:53-69`가 답을 반쯤 준다)  
답변: 핑 간격 사이에 먼저 클라이언트 측에서 행동을 실행한다. 서버는 어빌리티가 실행될수 있는지 검증하고, 호출할수 없다면 롤백한다.
2. **예측이 틀렸을 때 되돌리는 코드가 이 프로젝트에 있나?** 없다면 왜 티가 안 나나?  
답변: 엔진 GAS 내부 코드에서 처리해주기 때문이다. PredictionKey를 통해 이펙트나 무브들을 찾고 롤백하지만, 나머지 부수효과는 되돌아가지 않는다.
3. `:50`에서 시각을 **왜 `GetServerWorldTimeSeconds()`로 찍나?** `GetWorld()->GetTimeSeconds()`를 쓰면 뭐가 깨지나?  
답변: 클라 기준 서버의 월드 시간 예측, 현재 클라 월드 시간을 예측하기에 RTT/2만큼 차이나게 된다. 
4. `HandleServerFire`가 받는 `Origin`을 **서버가 검증하나?** 안 한다면 클라가 뭘 할 수 있나?  
답변: 클라 플레이어 액터와 Origin간의 거리를 계산하여 차이가 많이 나는지 검증한다. 하지않으면 텅텅 빈 하늘에서 총알 융단폭격같은것이 가능해진다.
5. `GetHitscanCandidates`로 후보를 먼저 추리는 이유는? 이게 없으면 뭐가 비싸지나?  
답변: 총알 궤적 주변의 후보들을 가져와 그것들만 히트를 검증한다. 없으면 월드 내 모든 액터에 대해 히트 검증을 해야할 수 있다.
6. **`PreAttributeChange`와 `PostGameplayEffectExecute` 중 데미지를 여기서 처리해야 하는 건 어느 쪽이고 왜인가?** 반대로 하면 뭐가 깨지나  
답변: PostGameplayEffectExecute. PreAttributeChange는 어트리뷰트 초기화용이다.
7. 사망 판정을 GE가 아니라 어빌리티(`EPGA_Death`)로 뺀 이유는?  
답변: 죽음을 검증하는 과정이 필요해서?
8. **머리를 맞으면 데미지가 더 들어가는 건 지금 어떤 경로로 계산되나?** (`:362`) — 예전엔 다른 방식이었는데 그게 왜 폐기됐나
답변:
EPWeaponDefinition를 상속한 BP에 TMap<FGameplayTag, float> TagDamageMultiplierMap 세팅
EPPhysicalMaterial의 FGameplayTag MaterialTags를 통해 피직스에셋 태그가 세팅된 피직스 메테리얼 세팅
UEPCombatComponent::HandleHitscan 함수 -> FHitResult에서 피직스 메테리얼 추출 -> WeaponDef의 태그-대미지 배율과 피직스 메테리얼의 태그 비교하여 대미지 배율 추출
UEPCombatComponent::ApplyGEDamage 함수 -> GE_Damage 클래스의 TAG_Data_Damage에 대미지 전달 -> IncomingDamage 어트리뷰트로 들어감

폐기했던 이유는 이전 방식이 하드코딩되어 있었으며, GAS 통합을 위함

## [비교] ★ 이 프로젝트 최대의 diff — RPC → GAS

```bash
git show feature-replication:EmploymentProj/Source/EmploymentProj/Public/Combat/EPCombatComponent.h
```

`:60-63`에 **`Server_Fire`와 `Server_Reload`가 살아 있다.** 지금은 없다.
사라진 자리에 `EPGA_Item_PrimaryUse` / `EPGA_Item_Reload`가 들어왔다.

**답할 것:** 직접 서버 RPC를 어빌리티로 바꿔서 **얻은 것**과 **잃은 것**을 각각 두 개씩.
*"GAS를 왜 쓰셨어요"* 에 대한 답이 여기서 나온다 — 이 diff를 못 보면 그 답이 교과서 문장이 된다.

**막히면:** `DOCS/Notes/04/04_GAS_02_DamagePipeline.md`, `04_GAS_03_PrimaryUse.md`, `04_GAS_06_HitZoneDamage.md`
※ 8번은 `revise/posts/2026-03-14-EP_NetPrediction-3.md`에 **폐기된 이유**가 적혀 있다

답변:
얻은 것: GAS를 사용한 확장성, GAS 자체 예측으로 인한 편리성?
잃은 것: 오버헤드, 간단한 작업도 거칠 과정이 많음

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

1. `GetCompressedFlags`는 **몇 비트**를 쓸 수 있나? 스킬이 늘어서 커스텀 플래그가 5개 필요해지면 어떻게 하나?
2. **`bWantsToSprint`는 복제되나?** 그러면 옆 사람 캐릭터의 질주 애니메이션은 어떻게 되나?
   *(이건 실제로 문제다 — `revise/posts/2026-03-01-EP_Replication-6.md`)*
3. `CanCombineWith`가 하는 일은? **부모 구현이 이미 `MaxSpeed`를 비교하는데** 왜 우리 것이 따로 필요한가?
4. `PrepMoveFor`는 언제 불리나? `SetMoveFor`와 짝인 이유는?
5. **스냅샷을 왜 `TG_PostPhysics`에서 찍나?** 시각(`Time`)은 왜 거기서 안 찍고 `OnMovementUpdated`에서 넘겨받나?
   *(이 답이 이 프로젝트에서 제일 값어치 있는 이야기다. `revise/posts/2026-03-14-EP_NetPrediction-2.md`)*
6. 30ms 고정 간격으로 스냅샷을 찍었을 때 오차가 **242cm**였다. 원인은 뭐였고 지금은 얼마인가?
7. **`ClientFireTime`을 서버가 믿나?** 클라가 이 값을 조작하면 뭘 할 수 있고, 뭘 못 하나?
8. **랙 보상은 불공정을 없애나?** 없앤다면 누구의 불공정이고, 안 없앤다면 그건 누구에게 옮겨가나?

## [비교] 242cm 버그가 고쳐지는 순간

```bash
git log --oneline feature-netprediction -- EmploymentProj/Source/EmploymentProj/Private/Combat/EPServerSideRewindComponent.cpp
git show <위에서 고른 커밋>
```

30ms 고정 인터벌이 빠지고 `OnMovementUpdated` → 플래그 → `TG_PostPhysics` 경로가 들어온 **그 커밋**을 찾아라.
**답할 것:** 왜 타임스탬프는 멀쩡한데 위치만 틀렸나. 그리고 오차가 왜 하필 *"항상 정확히 한 틱"* 이었나.

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

1. **왜 셋으로 나눴나?** 하나로 합치면 뭐가 안 되나. 각 계층에 뭘 넣을지 판정선은 뭔가
   *힌트: `LOOT_STATUS.md`의 "DT vs DA 배치 원칙"*
2. `InitPickup`을 **`SpawnActor`와 같은 프레임에** 불러야 하는 이유는? 한 프레임 늦으면 클라에서 뭐가 보이나?
3. 상호작용을 **직접 서버 RPC가 아니라 어빌리티로** 만든 이유 세 가지는? (7차 검수)
   그리고 **드랍은 왜 반대로 직접 RPC인가?** (8차 검수) — 두 판단을 가르는 기준 한 문장을 말해봐라
4. `:69-75`에서 **거리를 서버가 다시 재는 이유**는? 클라가 이미 쟀는데
5. 픽업이 `DORM_Initial`인데 `FlushNetDormancy()`가 필요 없는 이유는?
6. **한 번 본 픽업이 멀어져도 클라에 남는 게 정상**인 이유는? (릴러번시와 휴면의 관계)
7. 인벤토리를 `FFastArraySerializer`로 만드는 이유는? 그냥 `TArray`를 복제하면 뭐가 비싼가
8. `EntryId`가 **재번호되면 안 되는 이유**는? (배낭·부착물이 왜 원천 봉쇄되나)
9. **`UEPItemDefinition::GrantedAbility`(`.h:38`)는 왜 쓰이지도 않는데 Step 00에 들어갔나?**
   *`05_Loot_DOCS.md:561`이 이유를 적어놨다. 그리고 §2의 "상상한 확장점"과 뭐가 다른지 말해봐라*
10. **`EPCombatComponent.cpp:71-73`의 `case Hitscan: default:`에 `Melee`를 추가하면 어떻게 되나?**
    같은 enum을 `EPGA_Item_PrimaryUse.cpp:66`도 따로 보고 있다 — **한쪽만 고쳤을 때 각각 어떤 증상**이 나오나?
11. 붕대는 `AEPWeapon`이 아니다. **`CombatComponent::GetEquippedWeapon()`을 거치지 않고 사용하려면** 무엇이 진실의 원천이어야 하나?
    *힌트: `LOOT_STATUS.md` 장비 슬롯 결정 — `EquippedEntryId`*

## [비교] UObject를 struct로 갈아엎은 이유

```bash
git show feature-gas:EmploymentProj/Source/EmploymentProj/Public/Data/EPItemInstance.h
```

`feature-gas`에는 `UEPItemInstance` / `UEPWeaponInstance`가 있고 **지금은 없다** — `FEPItemState`(USTRUCT)로 전부 대체됐다.
**답할 것:** UObject를 쓰면 인벤토리 복제에서 뭐가 비싸지나? 왜 struct로 바꾸니 그게 사라지나?
*(호출처가 0이라 비용 없이 갈아엎었다는 점도 같이 — 데드코드였던 게 왜 다행이었나)*

**막히면:** `DOCS/Notes/05/LOOT_STATUS.md`(결정표가 답의 절반), `05_Loot_00_ItemCore.md`, `05_Loot_02_Interaction.md`, `Review/05_Loot_REVIEW7·8_*.md`

---

# 세션 6 — 자가 점검 (1.5h)

**소스를 닫고** 백지에 한다. 이게 실제 면접 조건이다.

## A. 백지 그리기 (40분)

1. **흐름 ①**을 화살표로 그린다. 클래스 이름만으로. 서버/클라가 갈리는 지점에 선을 긋는다
2. **흐름 ②**를 같은 방식으로
3. 그린 다음 소스를 열어 **빠뜨린 것**을 다른 색으로 채운다 — 빠뜨린 게 곧 안 잡힌 부분이다

## B. 3분 설명 (30분)

타이머를 켜고 **소리 내어** 말한다. 셋 중 아무거나 하나:

- "랙 보상을 어떻게 구현하셨어요?"
- "GAS를 왜 쓰셨어요? 안 쓰면 뭐가 힘든가요?"
- "이 프로젝트에서 제일 어려웠던 게 뭐였어요?"

**말이 끊기는 지점이 곧 안 잡힌 지점이다.** 그 지점만 소스로 돌아간다.

## C. 표시해둔 질문 처리 (20분)

세션 1~5에서 3분 넘게 막혔던 질문들. **여기서도 안 풀리면 답을 찾지 말고 목록에 남긴다** — 그게 다음 주에 팔 것들이고, 동시에 **블로그에서 빼야 할 주장들**이다.

> 설명 못 하는 주장은 문서에 있으면 안 된다. 정교한 문장은 면접 기대치를 올리는데, 그 기대치를 감당하는 건 문서가 아니라 나다.

---

## 2일 뒤 도달점

- [ ] 흐름 ① ②를 백지에 그린다
- [ ] "서버에서만 도는 것"을 5개 이상 즉답한다
- [ ] 이 프로젝트의 **실패담** 두 개를 수치와 함께 말한다 (242cm → 2.3cm / 본 배율이 왜 죽었나)
- [ ] 못 푼 질문 목록이 손에 있다

**마지막이 제일 중요하다.** 못 푼 질문을 아는 것과 모르는 것의 차이가, 면접에서 *"거기까진 확인 못 했습니다"* 와 *침묵*의 차이다.
