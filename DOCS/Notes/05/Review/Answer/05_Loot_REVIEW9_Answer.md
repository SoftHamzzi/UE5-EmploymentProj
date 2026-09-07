# 검수 답변 9차 — 슬롯 12개 기획 확대가 Step 03을 뒤집는가

> 작성일: 2026-08-22
> 요청서: `05_Loot_REVIEW9_Request.md` / 제안 본체: `DOCS/Mine/EquipmentSlots.md`
> 근거: UE 5.7 엔진 직독(`C:\Program Files\Epic Games\UE_5.7\Engine`) · Lyra 직독(`LyraStarterGame`) · 프로젝트 소스 직독
> **기억으로 단정한 API는 없다.** 인용은 전부 §12 표에 파일·줄로 있다

---

## 0. 판정 요약

| 항목 | 판정 | 한 줄 근거 |
|---|---|---|
| **§2 자료구조** | **A — `SlotId`가 진실** | 엔진 자신이 A다. `USceneComponent`는 `AttachParent` + `AttachSocketName`을 **자식**에 둔다(`SceneComponent.h:109,112-113`) |
| §2 Lyra 반례 | **반례가 아니다** | Lyra의 `Slots`는 **균질한 위치 인덱스 3칸**이고 `ULyraEquipmentDefinition`에는 슬롯 필드가 **아예 없다**. 이름 붙은 이종 슬롯 12개를 표현하는 구조가 아니다 |
| §2-5 O(N) | **대비하지 않는다** | GAS가 같은 자료구조를 **맨몸 선형 탐색**으로 조회한다(`GameplayEffect.cpp:3323-3333`, 호출부 27곳) |
| **★ 새 결함 1** | **`GetEntryInSlot(FName)`에 부모 인자가 없다** | 8차의 `FindFungibleEntryId` 결함과 **같은 모양**. 소총 두 정에 조준경을 못 단다 (§3-1) |
| **★ 새 결함 2** | **`SlotId`↔`ParentEntryId` 정합 불변식이 없다** | "가방 안에 든 상의를 입고 있다"가 표현 가능하다. 칸도 안 먹고 착용 효과도 받는다 (§3-2) |
| §3 `MoveEntry` | **함수는 만든다. `Server_MoveEntry`는 만들지 않는다** | 계약이 맞다 — 단 RPC 표면은 UI를 따라간다(8차 규칙의 일관 적용) (§4) |
| §3-4-2 사이클 검사 | **지금 넣는다** | "도달 불가 분기의 에러 처리"가 아니라 **`RemoveEntry` 재귀가 이미 의존하는 전제**를 계속 참이게 하는 코드다 |
| §3-4-3 교체 | **"차 있으면 실패"가 맞다** | Lyra도 같다 — `AddItemToSlot`이 `Slots[i]==nullptr`일 때만 대입한다(`.cpp:169-179`) |
| §4 핫바 이원화 | **맞다** | 합치면 칸의 정의가 **런타임 문자열 파싱**에 의존하게 된다 |
| §4-2 `HotbarRefs` 청소 | **Step 03에 넣지 않는다** | 제안의 *"제거 경로가 셋"* 이 **사실이 아니다.** 셋 다 `RemoveSelf` 하나로 모인다 (§5-3) |
| §5 `AllowedSlots` | **`TArray<FName>` 유지. 이름을 `SlotPriority`로** | 태그 컨테이너는 **순서를 표현하지 못한다**(집합 의미 + 체크박스 트리 UI) |
| §5-2-3 부착물 통합 | **부분적으로 틀렸다** | 아이템 쪽은 같은 필드로 되지만 **무기 쪽 필드는 여전히 필요하다** (§3-3) |
| §6 본체 10칸 | **유지가 맞다** | `GetInsertionOrder()`도 맞다. 단 **자동 장착과의 순서 관계를 명시해야 한다** (§7-3) |
| §7 파급 목록 | **불완전하다 — 7건 누락** | `BACKLOG.md` B-5 / `StudyPath.md` / `LOOT_STATUS.md` 확정표 2행 / 04 문서 3곳 / 05 문서 9곳 / `05_Loot_DOCS.md` 3곳 / 03 문서 3곳 (§9) |
| §7-5 03-A/B/C | **재조정한다** | `GetEntryInSlot`·`MoveEntry`·`SlotPriority`가 전부 **03-A**로 (§10) |

**한 줄 결론:** 기획 확대는 자료구조를 뒤집지 않는다. **`SlotId`가 진실이 되는 방향이 맞고, 엔진이 같은 선택을 한 선례가 있다.** 다만 제안에는 8차와 같은 모양의 결함이 하나 더 있고(§3), 근거 하나가 사실이 아니다(§5-3).

---

## 1. 전제 정리 — 8차 정정 확인

**한 가지를 먼저 인정한다.** 8차 답변은 *"`Build.cs`는 손댈 필요가 없다 — `NetCore`가 `Engine`의 Public 의존이라 전이로 들어온다"* 라고 적었다. **틀렸다.** 이번 세션에 `LNK2019: Z_Construct_UScriptStruct_FFastArraySerializerItem`으로 실증됐고, 문서와 `EmploymentProj.Build.cs:11` 모두 수정 완료된 것을 확인했다.

정확한 이유는 요청서 §1-4가 적은 대로다 — **public include 경로는 전파되지만 import 라이브러리는 직접 의존한 모듈 것만 링크 줄에 들어간다.** 그래서 컴파일은 통과하고 링크에서만 터진다.

> **아직 안 고쳐진 곳이 하나 있다.** `LOOT_STATUS.md:70`의 확정표 `FEPInventoryEntry 위치` 행이 여전히 *"`Build.cs` 수정 불필요(`NetCore`가 `Engine`의 Public 의존)"* 이다. **§9-8에 넣는다.**

**§2-2(a)의 CLAUDE.md 인용은 이번 라운드에서 정확하다.** 8차에서는 같은 조항이 반대 방향으로 작동했지만(문서에 이름이 있는 것은 `Server_DropItem`이었다), 이번에는 *"한 값을 두 경로가 봐야 하면 둘 다 볼 수 있는 곳에 둔다"* 가 그대로 A를 가리킨다. **같은 규칙이 두 번 다른 답을 낸 것이 아니라, 두 번 다 "이미 있는 곳"을 가리켰다.**

**되돌리는 비용이 0이라는 §1-1의 진단이 맞다.** `Private/Inventory/EPInventoryComponent.cpp`는 엔진 템플릿 그대로이고 로직이 없다. 지금이 무료 구간이다.

---

## 2. ★ 최대 주제 판정 — **A. `SlotId`가 장착의 유일한 진실이다**

### 2-1. 결정적 근거 — 엔진 자신이 A로 짜여 있다

요청서와 제안서 둘 다 이 선례를 언급하지 않았다. **`USceneComponent`가 정확히 `ParentEntryId` + `SlotId`다.**

```cpp
// Runtime/Engine/Classes/Components/SceneComponent.h:109-119
UPROPERTY(ReplicatedUsing = OnRep_AttachParent)
TObjectPtr<USceneComponent> AttachParent;            // ← 부모가 누구인가

UPROPERTY(ReplicatedUsing = OnRep_AttachSocketName)
FName AttachSocketName;                              // ← 그 부모의 "어느 이름 붙은 자리"인가

UPROPERTY(ReplicatedUsing = OnRep_AttachChildren, Transient)
TArray<TObjectPtr<USceneComponent>> AttachChildren;  // ← 파생 색인. Transient
```

세 줄 전부가 이번 논쟁의 답이다.

**① 진실은 자식에 있다.** "이 컴포넌트는 어느 부모의 어느 소켓에 붙어 있는가"를 **자식이 `FName` 하나로** 들고 있다. 부모가 `TMap<FName, USceneComponent*>` 같은 슬롯 표를 들고 있지 **않다.** 우리 제안(`ParentEntryId` + `SlotId`)과 필드 대 필드로 같다.

**② 그 `FName`은 복제되고 전용 `OnRep`을 가진다.** 소켓 이름이 자주 바뀌는 값이라는 것을 엔진이 인정하고 처리한 결과다. §8-5의 `FName` 대역폭 걱정에 대한 엔진의 답이 이것이다.

**③ 그런데 엔진도 자식 목록을 든다 — `Transient`로.** 이것이 §2-5의 O(N) 문제에 대한 정답 형태다. **색인이 필요하면 파생 색인을 붙인다. 진실을 옮기지 않는다.** `AttachChildren`은 저장되지 않고 재구축되며, 아무도 이것을 "어디에 붙어 있는가"의 답으로 쓰지 않는다.

> **B의 제안은 이 셋 중 ③만 남기고 ①을 버리자는 것이다.** 엔진은 정확히 반대로, ①을 진실로 두고 ③을 파생으로 뒀다.

**같은 패턴이 하나 더 있다** — 애님 몽타주의 슬롯이다.

```cpp
// Runtime/Engine/Classes/Animation/AnimMontage.h:83-92
struct FSlotAnimationTrack
{
    UPROPERTY(EditAnywhere, Category=Slot)
    FName SlotName;                     // ← 트랙 자신이 슬롯 이름을 든다
    UPROPERTY(EditAnywhere, Category=Slot)
    FAnimTrack AnimTrack;
};
```

**UE에서 "이름 붙은 자리"의 관용 표현은 `FName`이고, 그 이름은 자리에 들어가는 쪽이 든다.** 이것이 §8-3의 답이기도 하다.

### 2-2. Lyra의 `Slots`는 슬롯 시스템이 아니다 — 반례가 성립하지 않는다

요청서 §2-3이 인용은 정확히 했다. **해석이 틀렸다.**

`ULyraEquipmentDefinition` 전체를 읽었다. **슬롯 이름 필드가 없는 게 아니라, 장비 계층에 슬롯 개념 자체가 없다.**

```cpp
// LyraEquipmentDefinition.h:36-56 — 헤더 전체. 필드가 셋뿐이다
UCLASS(Blueprintable, Const, Abstract, BlueprintType)
class ULyraEquipmentDefinition : public UObject
{
    TSubclassOf<ULyraEquipmentInstance>          InstanceType;
    TArray<TObjectPtr<const ULyraAbilitySet>>    AbilitySetsToGrant;
    TArray<FLyraEquipmentActorToSpawn>           ActorsToSpawn;
};
```

```cpp
// LyraEquipmentManagerComponent.cpp:148,167 — 슬롯 파라미터가 없다
ULyraEquipmentInstance* ULyraEquipmentManagerComponent::EquipItem(TSubclassOf<ULyraEquipmentDefinition> EquipmentClass);
void                    ULyraEquipmentManagerComponent::UnequipItem(ULyraEquipmentInstance* ItemInstance);
```

**Lyra는 "머리 슬롯에 뭐가 있나"를 물을 수 없다.** 물을 필요가 없기 때문이다 — 던지는 질문이 `GetActiveSlotItem()` 하나뿐이다(`LyraQuickBarComponent.cpp:149-152`). **장비 계층은 슬롯 표가 아니라 집합이다.**

그리고 퀵바의 `Slots`는:

```cpp
UPROPERTY() int32 NumSlots = 3;                                        // .h:66
UPROPERTY(ReplicatedUsing=OnRep_Slots) TArray<TObjectPtr<...>> Slots;  // .h:73-74
```

**균질한 위치 인덱스 3칸이다.** 0번과 1번에 종류 제한이 없고, 이름도 없고, 서로 교환 가능하다. `GetNextFreeItemSlot()`이 그냥 앞에서부터 `nullptr`을 찾는다(`.cpp:154-167`).

우리가 필요한 것은 **이름이 붙고 종류 제한이 서로 다른 이종 슬롯 12개 + 무기마다 붙는 부착 슬롯 4개**다. Lyra의 표현을 그대로 쓰면 `"Torso"` ↔ `4`를 잇는 표가 어딘가에 필요하고, **그 표가 바로 §5의 `AllowedSlots`가 없애려는 하드코딩이다.** 별도 배열은 하드코딩을 하나 없애고 하나 만든다.

> **정리:** Lyra의 선택은 "슬롯을 어떻게 표현할까"의 답이 아니라 **"슬롯이 균질하고 위치로 식별될 때"** 의 답이다. 그 조건은 우리 핫바 5~0에는 성립하고(§2-6) 12개 장비 슬롯에는 성립하지 않는다.

### 2-3. §2-4의 해석을 한 칸 교정한다 — 용량이 아니라 **트리**다

요청서 §2-4는 *"Lyra가 별도 배열로 갈 수 있는 것은 용량 계산을 안 하기 때문"* 이라고 해석했다. **절반만 맞다. 용량은 결과이고 원인은 다른 데 있다.**

**원인은 우리 아이템이 트리에 있다는 것이다.**

Lyra의 아이템은 "인벤토리에 있다" 하나의 상태만 갖는다. 우리 아이템은 `ParentEntryId`로 **어디에 있는지가 이미 값으로 표현되어 있다.** `"Back"`에 매인 배낭도, 배낭 속 소총도, 총에 달린 조준경도 전부 같은 문장 *"나는 X의 Y 자리에 있다"* 의 인스턴스다.

**별도 배열은 이 문장을 반으로 쪼갠다.** `ParentEntryId`는 엔트리에 있고 `SlotId`에 해당하는 정보는 배열에 있게 된다. 그러면:

- "이 아이템 어디 있어?"가 **두 자료구조 조회**가 된다
- 둘이 어긋난 상태(`Slots[3] == 5`인데 5번 엔트리의 `ParentEntryId`가 배낭)가 **표현 가능**해진다
- 그 어긋남을 막는 코드가 생기고, 그 코드를 빠뜨리는 경로가 생긴다

용량 계산(`GetUsedSlots`)은 이 어긋남이 **처음 눈에 보이는 곳**일 뿐이다. Lyra에 용량이 없다는 것은, 어긋나도 아무도 안 본다는 뜻이다.

> **§2-2(d)가 이 주제의 진짜 핵심이다.** `if (!E.SlotId.IsNone()) continue;`(`05_Loot_03_Inventory.md:577`) 한 줄이 *"칸을 먹는다"* 의 **정의 전체**다. B로 가면 이 정의가 "그리고 장비 배열에 없어야 한다"로 늘어나고, 정의가 두 자료구조에 걸친다.

### 2-4. ★ B의 결함이 Lyra 자신에게서 실측된다

**요청서가 B의 선례로 든 코드가 B의 실패 모드를 그대로 보여준다.**

`ULyraInventoryManagerComponent::RemoveItemInstance`(`.cpp:189-197`)를 읽었다. **인벤토리에서 아이템을 빼면서 퀵바를 건드리지 않는다.** 그리고 `RemoveItemFromSlot`의 호출부를 전수 조사했다:

```
$ grep -rn "RemoveItemFromSlot" LyraGame/
Equipment/LyraQuickBarComponent.cpp:181   ← 정의
Equipment/LyraQuickBarComponent.h:52      ← 선언
```

**호출부가 0곳이다.** 블루프린트에서 부르라고 열어둔 함수고, C++ 어디에서도 인벤토리 제거와 연결되어 있지 않다.

즉 Lyra에서 **인벤토리에서 사라진 아이템이 퀵바에 남는 것은 구조적으로 가능하다.** Lyra가 이걸로 죽지 않는 이유는 단 하나 — `TObjectPtr`이 강참조라 객체가 살아 있기 때문이다. 잘못된 상태이되 크래시는 아니다.

**우리는 그 완충이 없다.** `int32 EntryId`는 강참조가 아니다. 별도 배열로 가면 §2-2(c)가 지적한 죽은 번호 문제가 **Lyra가 가진 유일한 방어막 없이** 재현된다.

> 그래서 §2-6-2("B라면 죽은 번호를 어떻게 막는가")의 답은 **"Lyra 방식으로는 못 막는다"** 이다. B를 택하면 `RemoveEntry`에 배열 청소를 넣어야 하고, 그건 A가 문법적으로 없애주는 바로 그 코드다.

### 2-5. §2-6-3 — A의 세 대가 판정

| 대가 | 판정 | 근거 |
|---|---|---|
| 조회 O(N) | **대비하지 않는다** | §8-4. GAS가 같은 자료구조에서 같은 짓을 27곳에서 한다 |
| UI 12슬롯 × N | **대비한다 — 단 컴포넌트가 아니라 UI에** | 아래 |
| 알림이 거칠다 | **대비하지 않는다** | Lyra도 거칠다. `OnRep_Slots`가 **`Slots` 배열 전체**를 메시지에 실어 브로드캐스트한다(`.cpp:205-213`) |

**12슬롯 UI만 실제 조치가 필요하다.** 12 × N 순회를 피하는 방법은 캐시가 아니라 **순회 한 번**이다.

```cpp
// Step 04 위젯 쪽. 컴포넌트에 두지 않는다
TMap<FName, int32> SlotMap;                       // 갱신 알림 1회당 1회 구축
for (const FEPInventoryEntry& E : Inv->GetEntries())
    if (!E.SlotId.IsNone() && E.ParentEntryId == INDEX_NONE)
        SlotMap.Add(E.SlotId, E.EntryId);
```

**왜 컴포넌트가 아니라 UI인가.** 컴포넌트에 두면 그 `TMap`이 두 번째 진실이 되고, `MarkItemDirty`마다 무효화하는 코드가 생긴다. 정확히 A가 피하려던 것이다. UI에 두면 **수명이 한 프레임**이라 어긋날 수 없다. 03-7이 트랜잭션당 알림 1회를 보장하므로(8차 확정) 구축 횟수도 1회다.

> `PostReplicatedReceive`에서 `TMap`을 만드는 방식(§8-4의 질문)도 되지만, 그건 컴포넌트에 두는 것과 같아진다. **읽는 쪽에서 만든다.**

### 2-6. A를 택하되, B가 옳은 자리가 하나 있다 — 핫바 5~0

이 판정은 "Lyra가 틀렸다"가 아니다. **Lyra의 표현은 그 조건이 성립하는 곳에서 우리도 쓴다.**

| | 12개 장비 슬롯 | 핫바 5~0 |
|---|---|---|
| 이름이 있나 | **있다** (`Torso`, `Optic`…) | 없다. 번호뿐 |
| 종류 제한이 서로 다른가 | **다르다** | 같다 (제한 없음) |
| 아이템이 그 자리로 **이동**하는가 | **한다** (칸에서 빠진다) | **안 한다** (인벤토리에 남는다) |
| **표현** | **A — `SlotId`** | **B — `TArray<int32> HotbarRefs`** |

마지막 줄이 결정적이다. **5~0은 소유가 아니라 참조다.** 아이템은 여전히 어느 컨테이너에 속해 있고 칸을 먹는다. 소유 관계를 나타내는 `SlotId`에 참조를 적으면 §4-1이 지적한 대로 칸 계산이 조용히 깨진다.

**Lyra의 `Slots`도 정확히 참조다** — `AddItemToSlot`이 포인터만 세팅하고 인벤토리에서 빼지 않는다(`.cpp:169-179`). 요청서 §2-4 표의 *"퀵바 아이템이 인벤토리에 남아 있다"* 행이 이미 적었다.

> **그래서 §4의 이원화는 "예외"가 아니라 두 표현이 각자 맞는 자리에 간 것이다.** 이 프레임으로 보면 §4-3-1의 "하나로 합칠 수 있나"는 자동으로 아니오가 된다.

---

## 3. ★ 요청서에 없던 결함 둘

### 3-1. `GetEntryInSlot`에 부모 인자가 없다 — 8차와 같은 모양

8차가 `FindFungibleEntryId`에서 잡은 결함의 결론이 확정표에 이렇게 남았다:

> `FindFungibleEntryId` — **`(int32 Container, FName ItemId)`** — 컨테이너 인자를 빼면 배낭 속 현금이 본체 현금과 합쳐지고, 무증상이다가 배낭을 벗을 때 딸려 나온다. (`LOOT_STATUS.md:69`)

제안서 §3·§7-3이 내놓은 시그니처는 이것이다:

```cpp
int32 GetEntryInSlot(FName SlotId) const;      // ← 부모 인자가 없다
```

**`Optic`은 무기마다 하나씩 있는 슬롯이다.** AK와 M4를 둘 다 들고 AK에 조준경을 달면:

```
GetEntryInSlot("Optic")  →  AK의 조준경을 찾는다
MoveEntry(스코프2, M4, "Optic")
    └─ "Optic 슬롯이 비었는가" 검사 → 차 있다 → 실패
```

**M4에는 영원히 조준경을 못 단다.** 그리고 이 검사는 §3-2 표의 세 번째 줄, 즉 `MoveEntry`의 핵심 검사다.

`Hotbar1`~`Hotbar4`와 착용 8칸은 몸에 하나뿐이라 **우연히** 맞는다. **부착 슬롯 4종에서만 틀린다** — 그래서 Step 03에서는 절대 안 걸리고, §7-3 부착물을 붙이는 날 나타난다.

### 3-2. ★ 더 나쁜 것 — `SlotId`와 `ParentEntryId`의 정합 불변식이 없다

`SlotId`가 진실이 되면 **표현할 수 있지만 무의미한 상태**가 생긴다.

```
상의   ParentEntryId = 배낭Id,  SlotId = "Torso"
       └─ "가방 안에 들어 있는 상의를 입고 있다"
```

지금 설계에는 이걸 막는 것이 없다. 그리고 이 상태는 `GetUsedSlots`에서 **칸을 안 먹고**(`SlotId != None`) `GetEntryInSlot("Torso")`에서 **입은 것으로 잡힌다.** 배낭 안에 상의를 넣어두면 칸도 안 먹고 착용 효과도 받는다.

**규칙은 두 줄이면 된다.**

```
장비 슬롯(핫바 1~4 + 착용 8)      →  ParentEntryId == INDEX_NONE 이어야 한다
부착 슬롯(Optic/Muzzle/Grip/Mag)  →  ParentEntryId == 그 무기 엔트리
```

그런데 **"이 슬롯 이름이 장비 슬롯인가 부착 슬롯인가"를 아는 코드가 설계에 없다.** `AllowedSlots`도 답하지 못한다 — 아이템이 *들어갈 수 있는* 곳의 목록일 뿐이다.

### 3-3. 그래서 §5-2-3의 "부착물도 같은 필드가 처리한다"는 틀렸다

제안서 §5는 *"부착물 판정도 같은 필드가 처리한다 — §7-3이 따로 필드를 만들 필요가 없다"* 고 적었다. **아이템 쪽만 맞다.**

| 질문 | 누가 답하나 | 있나 |
|---|---|---|
| 이 조준경이 `Optic`에 들어갈 수 있나 | 아이템의 `AllowedSlots` | ✅ §5가 만든다 |
| **이 무기에 `Optic` 슬롯이 있나** | **무기 Definition** | ❌ **없다** |
| `Optic`이 장비 슬롯인가 부착 슬롯인가 | ? | ❌ **없다** |

두 번째가 없으면 **단검에 조준경이 달린다.** 세 번째가 없으면 §3-2를 못 막는다.

### 3-4. 처방 — 시그니처 하나 + 필드 둘

```cpp
// ① 부모 인자를 붙인다 — FindFungibleEntryId와 같은 이유, 같은 모양
int32 GetEntryInSlot(int32 Parent, FName SlotId) const;
int32 GetEquippedBackpack() const { return GetEntryInSlot(INDEX_NONE, TEXT("Back")); }

// ② 무기가 자기 부착 슬롯을 든다 — UEPWeaponDefinition (§7-3에서 소비)
UPROPERTY(EditDefaultsOnly, Category = "Weapon")
TArray<FName> AttachmentSlots;      // 예: ["Optic", "Muzzle", "Grip", "Mag"]

// ③ 몸에 있는 슬롯 목록 — 전역이므로 UEPLootDeveloperSettings (6차 확정)
UPROPERTY(config, EditAnywhere) TArray<FName> BodySlots;   // 핫바 4 + 착용 8
```

**③이 있으면 §3-2의 검사가 `MoveEntry` 안에서 몇 줄이 된다.**

```cpp
const bool bIsBodySlot = GetDefault<UEPLootDeveloperSettings>()->BodySlots.Contains(NewSlotId);

if (bIsBodySlot && NewParent != INDEX_NONE)
    return false;                                       // 가방 안에서 입을 수 없다

if (!bIsBodySlot && !NewSlotId.IsNone())
{
    // 부착이다 — 부모 무기가 그 슬롯을 갖고 있어야 한다 (§7-3에서 활성화)
    const UEPWeaponDefinition* W = GetWeaponDefOf(NewParent);
    if (!W || !W->AttachmentSlots.Contains(NewSlotId)) return false;
}
```

> **③을 `UEPLootDeveloperSettings`에 두는 이유:** 두 소비자(`MoveEntry`의 검증, Step 04 UI의 슬롯 그리기)가 같은 목록을 봐야 하고, UI에는 물어볼 인벤토리 인스턴스가 없을 수도 있다. 6차의 *"전역 데이터 참조는 `UDeveloperSettings`"* 확정과 같은 자리다.

> **③은 지금 넣고 ②는 §7-3으로 미룬다.** ③이 없으면 Step 04 드래그가 열리는 순간 §3-2가 실재하고, 그때 `MoveEntry`를 다시 열어야 한다. ②는 소비자가 `MoveEntry`의 부착 갈래 하나뿐이고 그 갈래가 Step 03·04에 도달 불가다. **다만 §5가 "필요 없다"고 적은 것은 지금 고친다.**

---

## 4. `MoveEntry` 판정 — **함수는 만든다. RPC는 만들지 않는다**

### 4-1. 계약이 맞다 — 이 프로젝트가 이미 두 번 적용한 규칙이다

§3-3이 CLAUDE.md의 두 문장을 맞세웠다. **둘 중 하나가 틀린 게 아니라, "만들지 않는다" 쪽이 적용 대상이 아니다.**

> *"두 번째 구현자가 없는 **인터페이스·베이스 클래스**"*

`MoveEntry`는 인터페이스도 베이스 클래스도 아니다. **단일 클래스의 단일 함수**다. 다형성이 없으므로 "두 번째 구현자"라는 개념이 성립하지 않는다. 이 조항은 `UEPGA_InventoryAction` 같은 것을 겨냥한 문장이고(8차에서 실제로 그렇게 쓰였다), 함수 하나에는 걸리지 않는다.

**결정적인 것은 다른 데 있다 — 이 프로젝트는 이미 같은 패턴을 두 번 썼다.**

| 무엇 | 유일한 변경 지점 | 출처 |
|---|---|---|
| 엔트리 추가 | `InsertEntry(Parent, ItemId, State, SlotId)` — *"번호 발급·삽입·`MarkItemDirty`의 유일한 지점"* | `05_Loot_03_Inventory.md:290-292` |
| `Charges` | `SetEntryCharges` — *"★ 유일한 쓰기 지점"* | `:253` |
| **`ParentEntryId` + `SlotId`** | **없다** | ← 구멍 |

**세 번째만 비어 있다.** 그리고 8차가 확인한 대로, 살아 있는 엔트리를 잘못 만지면 `ReplicationID`가 리셋되어 수신 측에 **삭제+추가**로 보인다(`FastArraySerializer.h:302-323`). `MarkItemDirty`를 빠뜨리면 아예 안 나간다.

`MoveEntry`는 새 계약이 아니라 **이미 두 번 세운 규칙의 세 번째 적용**이다. CLAUDE.md §3의 *"바뀐 줄은 요청 또는 §2가 승인한 확장점으로 추적된다"* 를 만족한다.

### 4-2. ★ 그러나 `Server_MoveEntry`는 지금 만들지 않는다

**8차 규칙을 일관되게 적용하면 이 결론이 나온다.** 8차가 세운 문장은 *"서버가 이미 소유한 상태에 대한 변경 요청 → 컴포넌트의 서버 RPC"* 였다. 그건 **RPC를 열어도 된다**는 허가지 **얼마나 넓게 열지**의 답이 아니다.

| | `Server_EquipBackpack(int32 EntryId)` | `Server_MoveEntry(int32, int32, FName)` |
|---|---|---|
| 클라가 정하는 값 | 엔트리 하나 | 엔트리 + **목적지 부모** + **슬롯 이름** |
| Step 03의 정당한 UI | 배낭 착용 | **없다** — 드래그가 Step 04다 |
| 검증해야 할 것 | `ContainsEntry` + 배낭인가 | 위 전부 + §3-2 + §3-4 + 사이클 |

**Step 03에는 `NewParent`와 `NewSlotId`를 정당하게 만들어낼 UI가 없다.** 그런데 RPC를 열면 조작된 클라이언트는 만들 수 있다. **검증 표면을 소비자보다 먼저 여는 것**이고, 이건 8차가 `Server_DropItem`을 고른 이유(*"서버가 이미 소유한 상태"*)와 무관한 순수한 공격 표면 확대다.

> **정리:** `MoveEntry`(내부 계약)는 지금. `Server_MoveEntry`(외부 표면)는 Step 04의 드래그와 함께. `Server_EquipBackpack`은 **`MoveEntry`를 부르는 얇은 래퍼**로 지금 남긴다 — 제안서 §6이 적은 그대로다. 다만 **래퍼가 남는 게 과도기가 아니라 최종형이다** — 좁은 RPC는 넓은 RPC보다 낫다.

### 4-3. 사이클 검사 — **지금 넣는다.** 에러 처리가 아니라 전제 복원이다

§3-4-2가 CLAUDE.md의 *"도달 불가한 분기의 에러 처리"* 를 걱정했다. **분류가 틀렸다.**

`RemoveEntry`의 재귀에는 지금 **무한 재귀를 막는 장치가 없다.** 안전한 이유가 문서에 명시돼 있다:

```
// 05_Loot_03_Inventory.md:393-394
// ④ 자식은 그 다음. 부모가 이미 배열에서 빠져 사이클이 성립하지 않는다
```

이 문장은 **"입력이 트리다"라는 전제 위에 서 있다.** `MoveEntry`는 그 전제를 깰 수 있는 **유일한 함수**다(`InsertEntry`는 새 노드만 만들고 `RemoveEntry`는 노드를 없앤다).

즉 사이클 검사는 "일어나지 않을 일에 대한 에러 처리"가 아니라 **이미 존재하는 안전성 논증을 계속 참이게 하는 코드**다. 없으면 `:393`의 주석이 거짓말이 된다.

CLAUDE.md §2: *"나중에 넣기 비싼 것은 지금 넣는다 — 식별자 안정성, 복제 조건, **계약(반환 규약·순서)**"*. 재귀 종료 조건은 계약이다.

```cpp
// 5줄. NewParent에서 위로 걸어 올라가 자기를 만나면 사이클이다
for (int32 P = NewParent; P != INDEX_NONE; )
{
    if (P == EntryId) return false;
    FEPInventoryEntry E;
    P = FindEntry(P, E) ? E.ParentEntryId : INDEX_NONE;
}
```

> **비용 비교가 결정적이다.** 안 넣었을 때의 증상은 예외도 로그도 아니라 **전용 서버 프로세스가 멈추는 것**이다. 5줄과 그것을 맞바꾸지 않는다.

### 4-4. "슬롯이 차 있으면 실패" — 맞다. Lyra도 같다

```cpp
// LyraQuickBarComponent.cpp:169-179
void ULyraQuickBarComponent::AddItemToSlot(int32 SlotIndex, ULyraInventoryItemInstance* Item)
{
    if (Slots.IsValidIndex(SlotIndex) && (Item != nullptr))
    {
        if (Slots[SlotIndex] == nullptr)      // ★ 비었을 때만 대입한다
        {
            Slots[SlotIndex] = Item;
            OnRep_Slots();
        }
    }
}
```

**교체(swap)를 지원하지 않고 조용히 실패한다.** 우리는 `Client_OnInventoryActionFailed`가 있으니 조용하지 않게 할 수 있다.

**§3-4-3이 걱정한 "중간 상태에서 첫 아이템이 갈 곳이 없다"는 실재하지만, 그것이 원자적 교체의 근거가 되지 않는다.** 가방이 꽉 찼으면 해제가 실패하고, 실패하면 **교체 전체가 아무 일도 안 한 상태로 끝난다.** 이게 올바른 결과다. 원자적 교체를 만들면 오히려 *"밀려난 아이템은 어디로 가는가"* 를 정의해야 하고, 그 답이 "가방" 또는 "바닥"인데 둘 다 Step 04의 드래그 UI가 정하는 문제다.

> **교체는 Step 04로 미룬다.** 그때는 드래그의 출발지와 목적지가 둘 다 명시되어 있어서 밀려난 아이템의 행선지가 자명하다.

---

## 5. 핫바 이원화 — 맞다. 단 청소 근거 하나가 사실이 아니다

### 5-1. §4-3-1 이원화 — 맞다. 합치면 칸의 정의가 문자열 파싱이 된다

§4-1의 논증(*"5~0에 `SlotId`를 쓰면 칸 계산에서 빠진다"*)이 맞다. **한 가지를 더한다.**

대안(`SlotId`에 넣고 칸 계산에서 `Hotbar5`~`Hotbar0`만 예외)을 택하면 이 줄이

```cpp
if (!E.SlotId.IsNone()) continue;                    // 지금: 칸의 정의 전체
```

이렇게 된다.

```cpp
if (!E.SlotId.IsNone() && !IsShortcutSlot(E.SlotId)) continue;   // 이름을 파싱해야 한다
```

**`IsShortcutSlot`은 `FName`을 문자열로 보고 판정할 수밖에 없다.** 칸 계산이 명명 규칙에 의존하게 되고, 슬롯 이름을 바꾸는 순간 조용히 깨진다. §2-2(d)에서 A를 고른 이유(*칸의 정의가 한 줄*)를 스스로 무효화한다. **거부.**

### 5-2. ★ A의 미청구 배당금 — `ActiveHotbarIndex`는 stale이 될 수 없다

제안서가 청구하지 않은 이득이 하나 있다. **`ActiveHotbarIndex`는 청소가 필요 없다.**

```cpp
int32 GetEquippedEntryId() const
{
    return (ActiveHotbarIndex >= 0 && ActiveHotbarIndex < 4)
        ? GetEntryInSlot(INDEX_NONE, HotbarSlotName(ActiveHotbarIndex))   // 없으면 INDEX_NONE
        : INDEX_NONE;
}
```

**인덱스가 가리키는 것이 엔트리가 아니라 슬롯**이기 때문이다. 1번에 든 총을 버리면 `ActiveHotbarIndex`는 여전히 0이지만 `GetEntryInSlot(INDEX_NONE, "Hotbar1")`이 `INDEX_NONE`을 돌려준다. **죽은 번호가 생길 문법이 없다.**

이것이 §2-2(c)와 같은 종류의 이득이고, 함정 **3h**가 사라지는 것과 같은 이유다. 그리고 이것 때문에 §5-3의 판단이 갈린다 — **`HotbarRefs`는 `EntryId`를 직접 들기 때문에 이 보호를 못 받는다.**

### 5-3. ★ §4-2의 근거가 사실이 아니다 — 제거 경로는 셋이 아니라 하나다

제안서 §4는 청소를 Step 03에 넣는 근거로 이렇게 적었다.

> *"세팅하는 경로는 Step 05에 있지만, 제거 경로는 Step 03에서 이미 셋이다(버리기·사용·캐스케이드). 나중에 넣으면 셋을 찾아다닌다."*

**확인 결과 셋이 아니다.** 03-2의 설계상 셋 다 한 함수로 모인다.

```
Server_DropItem   ─┐
SetEntryCharges 0 ─┼→ RemoveEntry → RemoveEntryInternal → RemoveSelf   ← 여기 하나
캐스케이드        ─┘                    └→ RemoveChildrenRecursive ────┘
```

`RemoveSelf`는 이미 *"배열에서 빼고 `MarkArrayDirty`"* 의 유일한 지점으로 선언돼 있다(`05_Loot_03_Inventory.md:294`). **Step 05에서 `HotbarRefs`를 세팅할 때 청소를 붙일 곳은 정확히 한 줄이다.**

따라서 CLAUDE.md §2 기준으로 이건 *"나중에 넣기 비싼 것"* 이 아니다. 그리고 Step 03 내내 **길이 0인 배열을 도는 루프**라 8차가 지적한 *"Step 03 내내 항상 거짓인 분기"* 패턴을 하나 더 만든다.

**판정:**

| | 지금(Step 03) | 나중 |
|---|---|---|
| `ActiveHotbarIndex` 필드 | **선언한다** | — |
| `HotbarRefs` 필드 | **선언하지 않는다** | Step 04 (드래그 배정) |
| `HotbarRefs` 청소 | — | **Step 05. `RemoveSelf` 한 줄** |

> **`ActiveHotbarIndex`만 지금 선언하는 이유:** 이건 `EquippedEntryId`를 **대체**하는 필드다. 문서가 이미 그 자리에 필드를 선언하고 `DOREPLIFETIME_CONDITION`과 세이브 목록에 넣어뒀다(`:278, :490, :319`). 필드가 하나 늘어나는 게 아니라 **이름과 의미가 바뀌는 것**이라 지금이 맞다.

> **잊지 않기 위한 장치:** `HotbarRefs` 청소를 `05_Loot_05_Equipment.md`의 **완료 조건으로 이름을 붙여 적는다.** 8차가 `EquippedEntryId` 건에서 쓴 방법과 같다(`05_Loot_05_Equipment.md:19`). **코드에 죽은 루프를 남기는 것보다 완료 조건에 한 줄을 남기는 쪽이 싸다.**

### 5-4. §4-3-2 자료형 — `TArray<int32>` 고정 6칸이 맞다

`TMap<int32,int32>`는 안 된다 — **빈 슬롯을 표현하지 못한다.** 5·7번만 걸려 있으면 UI가 6칸을 그릴 때 어느 칸이 비었는지를 `Contains`로 물어야 하고, 이건 §2-3에서 B가 A보다 나았던 바로 그 성질(*"빈 슬롯을 표현할 수 있다"*)을 버리는 것이다.

**5~0은 균질하고 위치로 식별되므로 배열이 정확히 맞다** — §2-6의 표 그대로다. 생성자에서 `HotbarRefs.Init(INDEX_NONE, 6);`로 길이를 고정한다. Lyra의 `Slots`도 `NumSlots`로 같은 일을 한다(`.h:66`).

---

## 6. `AllowedSlots` — `TArray<FName>` 유지. 이름을 바꾼다

### 6-1. §5-2-1 자료형 — `TArray<FName>`

| 후보 | 판정 | 이유 |
|---|---|---|
| **`TArray<FName>`** | ✅ | `FEPInventoryEntry::SlotId`가 이미 `FName`이다. 다른 걸 쓰면 **비교마다 변환**이 생긴다 |
| `FGameplayTagContainer` | ❌ | **순서를 표현하지 못한다** (아래) |
| `EEPEquipSlot` 비트마스크 | ❌ | 슬롯 추가 = 열거형 편집 = **코드 변경**. §5의 전제를 정면으로 깬다. 부착 슬롯까지 같은 열거형을 나눠 쓰게 된다 |

**태그 컨테이너가 안 되는 이유가 결정적이다.**

```cpp
// GameplayTagContainer.h:625,629
TArray<FGameplayTag> GameplayTags;
TArray<FGameplayTag> ParentTags;      // 부모 태그까지 펼쳐 든다
```

내부는 배열이지만 **의미가 집합**이다. `HasTag`는 `ParentTags`까지 보며(`:304-310`), 비교 연산도 순서를 보지 않는다. 실무적으로 더 큰 문제는 **에디터 UI가 체크박스 트리**라는 것이다 — DT를 채우는 사람에게 순서를 보여줄 방법도, 바꿀 방법도 없다.

**§5의 핵심 아이디어가 "배열 순서 = 우선순위"인데, 태그 컨테이너는 그 아이디어를 담을 수 없다.**

> 그리고 §2-1에서 본 대로 **엔진의 이름 붙은 슬롯 관용구는 `FName`이다** — `AttachSocketName`(`SceneComponent.h:113`), `FSlotAnimationTrack::SlotName`(`AnimMontage.h:88`). 태그는 "요구사항·분류"에 쓰이지 "자리 이름"에 쓰이지 않는다. 프로젝트에 `EPNativeGameplayTags.h`가 있는 것은 사실이지만 그건 GAS 어빌리티·이펙트용이고, **슬롯 이름은 GAS 계층을 지나가지 않는다.**

### 6-2. ★ §5-2-2 순서 = 우선순위 — 견고하게 만드는 방법은 필드 분리가 아니다

**걱정이 타당하다.** DT 행을 채우는 사람이 순서의 의미를 모르면 조용히 틀린다.

**그러나 `PreferredSlot`을 따로 두면 안 된다.** `PreferredSlot`은 반드시 `AllowedSlots`에 있어야 하고, 그 정합을 지키는 코드가 생기고, 빠뜨리는 경로가 생긴다. **§2에서 A를 고른 이유와 같은 이유로 거부된다.**

**대신 이름이 의미를 지고 가게 한다.**

```cpp
// FEPItemData
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
TArray<FName> SlotPriority;
// 이 아이템이 들어갈 수 있는 장비/부착 슬롯. ★ 순서가 곧 자동 배정 우선순위다.
// 비어 있으면 어느 슬롯에도 들어가지 못한다 (일반 아이템)
```

`AllowedSlots`라는 이름은 **집합처럼 읽힌다.** 그래서 순서를 바꿔도 되는 것처럼 보인다. `SlotPriority`는 그 오해가 불가능하다. **한 단어짜리 수정이 실패 모드 하나를 없앤다.**

> 값의 성격은 그대로다 — 포함 여부 검사(`Contains`)와 우선순위 순회 둘 다 이 배열 하나로 한다. 바뀌는 것은 이름과 주석뿐이다.

### 6-3. §5-2-3 부착 슬롯 통합 — **§3-3 참조.** 아이템 쪽만 맞다

`SlotPriority`에 `"Optic"`을 적는 것은 옳다. 그러나 §5가 *"§7-3이 따로 필드를 만들 필요가 없다"* 고 결론지은 것은 **틀렸다** — 무기 쪽 `AttachmentSlots`와 전역 `BodySlots`가 여전히 필요하다.

### 6-4. 지금 넣는 것이 맞다

§5의 논거(*"미루면 Step 05에서 기존 DT 행을 전부 다시 채워야 한다"*)가 맞다. **한 가지를 덧붙인다** — Step 03에서도 값이 하나 필요하다. 배낭 행의 `SlotPriority = ["Back"]`이 없으면 `MoveEntry(id, -1, "Back")`의 슬롯 검사가 통과할 근거가 없다. **읽는 코드가 0곳이 아니라 1곳이다.**

---

## 7. 본체 10칸 · `GetInsertionOrder()`

### 7-1. §6-3-1 본체 10칸 유지 — 맞다

완료 조건 2·4·5·6이 전부 *"칸이 모자라면"* 위에 서 있고, 지금 없애면 **착용 아이템 DT 행 + 착용 경로 + UI 그룹 표현**이 Step 03 범위로 들어온다. Step 03은 이미 다른 단계 두 개 분량이다.

**§8의 "없애는 비용이 0에 가깝다"도 대체로 맞다.** 다만 조건이 하나 더 있다: `GetCapacity(INDEX_NONE)`이 `MaxSlots`를 돌려주는 **특수 분기**가 남아 있는 한(03-2), 본체는 "용량 0인 특수 컨테이너"로 자연히 퇴화하지 못한다. **"코드 변경 없음"은 정확히는 "이 분기 하나만"이다.** 그 분기를 데이터로 옮기는 것은 §8 시점에 해도 된다.

### 7-2. §6-3-2 반환 타입 — 지금은 어느 쪽이든 무관하다

호출 빈도가 **획득 1회당 1회**다(초당 수십 회가 아니다). 어느 쪽도 측정 가능한 차이를 만들지 않는다.

`TArray<int32, TInlineAllocator<4>>`는 range-for에 그대로 쓰이고 힙 할당을 없애므로 써서 손해가 없다. **다만 이걸로 시간을 쓰지 않는다** — 호출부가 `for (int32 C : ...)` 하나뿐이라 나중에 바꿔도 비용이 0이다. **지금 값을 하는 것은 "함수가 있다"는 사실 자체지 그 시그니처가 아니다.**

### 7-3. ★ §6-3-3 자동 장착과 삽입 순서 — 만나지 않는다. **순차다**

기획이 *"총을 주우면 자동으로 핫바에 들어간다"* 이므로 장비가 먼저다. 그런데 이 둘은 같은 경로에서 경쟁하는 게 아니라 **단계가 다르다.**

```cpp
// AEPPickup::OnInteract — 03-4
// ① 장비 슬롯 시도 (SlotPriority 순서대로 빈 곳)
int32 NewId = Inv->TryAutoEquip(Payload);        // 실패하면 INDEX_NONE

// ② 실패했으면 컨테이너 (GetInsertionOrder 순서대로)
if (NewId == INDEX_NONE)
    for (int32 C : Inv->GetInsertionOrder())
        if ((NewId = Inv->AddSubtree(C, Payload)) != INDEX_NONE) break;
```

**①과 ②는 판정 기준이 다르다.** ①은 *슬롯이 비었는가*, ②는 *칸이 남았는가*. 하나로 합치려 하면 `GetInsertionOrder()`가 "컨테이너 또는 슬롯"을 섞어 반환해야 하고 소비자(`AddSubtree`)가 둘을 구분해야 한다. **섞지 않는다.**

> **`TryAutoEquipBackpack` → `TryAutoEquip`으로 일반화한다.** 지금 배낭 전용 함수를 만들면 무기·상의·헬멧이 들어올 때 같은 함수가 넷이 된다. 일반형은 *"`SlotPriority`를 순회하며 `GetEntryInSlot(INDEX_NONE, S) == INDEX_NONE`인 첫 슬롯에 `MoveEntry`"* 한 줄짜리 루프이고, **Step 03에서는 배낭 행의 `["Back"]` 하나만 돌아 지금 동작과 정확히 같다.** 배낭 자동 착용을 Step 03에 두는 결정(3차 확정)은 그대로다.

> **서브트리를 통째로 장비 슬롯에 넣는 경우가 있다** — 조준경이 달린 총을 주울 때다. 그래서 ①도 `AddSubtree`를 거쳐야 하고 **루트에만 `SlotId`를 세팅한다.** `AddSubtree`의 *"칸 검사는 루트만"* 계약(§8 확정표)이 여기서도 맞아떨어진다 — 루트가 슬롯에 들어가면 칸 검사 자체를 건너뛴다.

---

## 8. 실무 조사 5건

### 8-1. 용량이 있는 UE 인벤토리 선례 — **엔진·플러그인에 없다**

`UE_5.7/Engine/Plugins` 전체를 훑었다. **인벤토리·장비 플러그인이 없다.** Lyra가 유일한 샘플이고 §1-3이 확인한 대로 용량 개념이 없다(`LyraInventoryManagerComponent.cpp:159-163`, `//@TODO`와 함께 무조건 `true`).

**그래서 "용량 있는 인벤토리의 선례"로 판단하는 길은 없다.** 대신 **구조가 같은 엔진 시스템**이 답을 준다 — §2-1의 `USceneComponent`다. 그것은 "부모 + 이름 붙은 자리 + 용량 없음"이지만, **논쟁 대상인 "진실을 어디에 두는가"는 정확히 같은 질문**이고 엔진은 자식에 뒀다.

> 이 항목은 *"찾아봤지만 없다"* 가 정직한 답이다. 없는 선례를 만들어 인용하지 않는다.

### 8-2. `FLyraAppliedEquipmentEntry`에 슬롯 이름이 없는 이유 — **슬롯 개념이 없어서다**

§2-2에 근거를 폈다. 요약:

- `ULyraEquipmentDefinition`은 필드가 셋뿐이고 슬롯 관련이 하나도 없다(`LyraEquipmentDefinition.h:36-56`, 헤더 전체)
- `EquipItem(TSubclassOf<ULyraEquipmentDefinition>)` / `UnequipItem(ULyraEquipmentInstance*)` — **슬롯 파라미터가 없다**(`.cpp:148,167`)
- 즉 요청서 §8-2의 두 가설 중 **둘 다 아니다.** "동시에 여러 부위에 못 붙어서"도, "`ULyraEquipmentDefinition`이 슬롯을 들고 있어서"도 아니다. **장비 계층은 슬롯이 아니라 집합이고, 슬롯은 퀵바에만 위치 인덱스로 존재한다**

Lyra가 던지는 질문은 `GetActiveSlotItem()` 하나다. *"내 머리에 뭐가 있나"* 를 물을 일이 없으므로 답할 구조도 없다.

### 8-3. `TArray<FName>` vs `FGameplayTagContainer` — **`FName`이 관용구다**

§6-1에 폈다. 요약: 엔진의 "이름 붙은 자리"는 전부 `FName`(`AttachSocketName`, `SlotName`), 태그 컨테이너는 **집합 의미 + 순서 표현 불가 + 체크박스 트리 UI**.

### 8-4. FastArray 선형 조회가 실무에서 문제가 되는가 — **GAS가 그렇게 한다**

**GAS의 활성 이펙트 컨테이너가 우리와 정확히 같은 자료구조다.**

```cpp
// GameplayEffect.h:1334, 1639
struct FActiveGameplayEffect           : public FFastArraySerializerItem { ... };
struct FActiveGameplayEffectsContainer : public FFastArraySerializer     { ... };
```

그리고 핸들로 찾는 **유일한 조회 함수가 맨몸 선형 탐색이다.**

```cpp
// GameplayEffect.cpp:3323-3333
FActiveGameplayEffect* FActiveGameplayEffectsContainer::GetActiveGameplayEffect(const FActiveGameplayEffectHandle Handle)
{
    for (FActiveGameplayEffect& Effect : this)
    {
        if (Effect.Handle == Handle) return &Effect;
    }
    return nullptr;
}
```

**호출부가 `GameplayEffect.cpp`에 13곳, `AbilitySystemComponent.cpp`에 14곳이다.** 쿨다운 조회, 스택 처리, 주기 실행, 제거 — 전부 게임플레이 핫 패스다. **에픽이 이 자료구조에서 O(N)을 문제로 보지 않는다는 가장 강한 증거다.** 우리 N은 수십이고 GAS의 N도 수십이다.

> **캐시를 붙인다면 어디인가:** `PostReplicatedReceive`에서 `TMap`을 만드는 방식은 동작하지만 **컴포넌트에 두 번째 진실을 만든다.** §2-5에 적은 대로 **읽는 쪽(위젯)에서 알림 1회당 1회 만드는 것**이 옳다. 그리고 지금은 아무것도 만들지 않는다.

> 참고로 `FFastArraySerializer`는 이미 `TMap<int32,int32> ItemMap`을 들고 있지만(`FastArraySerializer.h:416`), 키가 `ReplicationID`라 우리 `EntryId`·`SlotId` 조회에는 쓸 수 없다.

### 8-5. `FName` 필드의 복제 비용 — **바뀔 때만 나가고, 그때는 문자열이다**

**두 사실이 합쳐져야 답이 나온다.**

**① 커스텀 `FName`은 문자열로 나간다.**

```cpp
// CoreNet.cpp:344-360 (StaticSerializeName, 쓰기 측)
const EName* InEName = InName.ToEName();
uint8 bHardcoded = InEName && ShouldReplicateAsInteger(*InEName, InName);
Ar.SerializeBits(&bHardcoded, 1);
if (bHardcoded) { /* 인덱스 */ }
else
{
    FString OutString = InName.GetPlainNameString();   // ★ 문자열 전체
    int32   OutNumber = InName.GetNumber();
}
```

`"Hotbar1"`은 엔진의 하드코딩 `EName`이 아니므로 **문자열 경로**를 탄다. 대략 길이(4B) + 문자(8B) + Number(4B) ≈ **16바이트**, `int32`의 4바이트 대비 **약 4배**다.

**② 그런데 안 바뀌면 안 나간다 — 내부 struct 델타가 기본 켜져 있다.**

```cpp
// FastArraySerializer.cpp:24-36 — 기반 생성자가 스스로 켠다
FFastArraySerializer::FFastArraySerializer()
    : ...
    , DeltaFlags(EFastArraySerializerDeltaFlags::None)
{
    SetDeltaSerializationEnabled(true);      // ★
}
```

```
// FastArraySerializer.h:218-221
Delta Serialization for inner structs is now enabled by default. That means that when a ReplicationKey
changes, we will compare the current state of the struct to the last sent state, tracking changelists and
only sending properties that changed exactly like the standard replication path.
```

**결론:**

| 상황 | `SlotId`가 나가나 |
|---|---|
| 사격으로 `Charges`가 준다 | ❌ 안 나간다. 바뀐 프로퍼티만 나간다 |
| 장착/해제로 `SlotId`가 바뀐다 | ✅ 나간다. ~16바이트 |
| 엔트리가 새로 생긴다 | ✅ 나간다 (전 프로퍼티) |

**즉 §8-5의 걱정 — "`SlotId`는 장착/해제마다 바뀐다" — 는 걱정할 수준이 아니다.** 장착/해제는 사람이 손으로 하는 초당 1회 미만의 행위이고, 그때 16바이트가 나간다. **대비하지 않는다.**

> **★ 단 조건이 하나 있다.** 이 이득은 엔트리가 **살아 있는 채로 수정될 때**만 성립한다. 8차가 확인한 대로 살아 있는 원소에 통째로 대입하면 `ReplicationID`가 `INDEX_NONE`으로 리셋되어(`FastArraySerializer.h:302-323`) 수신 측에 **삭제+추가**로 보이고, 그러면 **전 프로퍼티가 다시 나간다.** 8차의 *"살아 있는 원소에 통째 대입 금지"* 규칙이 **여기서 두 번째 이유를 얻는다** — `MoveEntry`는 반드시 필드 두 개만 고치고 `MarkItemDirty(Item)`을 부른다.

> `LOOT_STATUS.md:73`의 확정표 `인벤토리 복제` 행이 이미 *"(내부 struct 델타 기본 활성)"* 이라고 적고 있다. **이번 조사로 근거가 소스로 확정됐다.**

---

## 9. ★ §7 파급 목록 — 불완전하다. 7건이 빠졌다

요청서 §7이 *"빠뜨린 것이 이번 검수의 가장 큰 위험"* 이라고 적었다. 전 문서에서 `EquippedEntryId` / `EquippedBackpackEntryId` / `GetEquippedBackpack` / `SetEquippedEntryId`를 전수 조사했다. **§7에 없는 것이 7건 있다.**

| # | 위치 | 무엇이 뒤집히나 | 위험도 |
|---|---|---|---|
| **1** | **`DOCS/BACKLOG.md:124, 129-133, 240`** | **B-5 항목 전체.** *"진실은 `EquippedEntryId`(int32). 무기 액터는 파생값"* 이 B-5의 문장이다. 진실이 **`ActiveHotbarIndex` → `GetEquippedEntryId()`** 로 바뀌면 서술이 통째로 낡는다. `:129`는 `LOOT_STATUS.md` 장비 슬롯 결정을, `:130`은 `05_Loot_05_Equipment.md:119`를 **명시적으로 가리킨다** | ★★ B-5는 `LOOT_STATUS.md:27`이 Step 05 이월 3건 중 *"안 지키면 나중이 비싸진다"* 로 지목한 항목이다 |
| **2** | **`DOCS/Mine/StudyPath.md:926-935`** | 세션 5의 **"정답"** 이 *"맞다. `EquippedEntryId`(int32)다"* 이고 다이어그램(`:932`)까지 있다. **사용자가 읽고 외우는 문서**라 틀린 채 두면 가장 비싸다 | ★★ |
| **3** | **`LOOT_STATUS.md:54`, `:76`** | 확정표 **두 행.** `:54` 배낭 행(*"`EquippedBackpackEntryId` 별도 필드(TMap 아님)"*), `:76` 장비 슬롯 행(*"필드 둘 … 슬롯이 셋이 되면 그때 `TMap`"*). §7-1은 `05_Loot_DOCS.md` §8만 지목했는데 **`LOOT_STATUS.md`가 자기 확정표를 따로 갖고 있다** | ★★ 진실의 원천 문서다 |
| **4** | **`05_Loot_04_InventoryUI.md:38, :113, :331`** | §7 대상표는 04를 *"드래그 이동의 소비자"* 로만 적었다. 실제로는 **`:113`이 `GetEquippedBackpack()`을 호출**하고 `:38`·`:331`이 *"장비 슬롯 UI = `EquippedEntryId` 강조"* 다. **12칸 슬롯 UI**가 되면 04의 범위 자체가 늘어난다(§2-5) | ★★ |
| **5** | **`05_Loot_05_Equipment.md:19-20, :91, :119, :124, :134, :141, :173, :181, :262`** | §7-4는 `05-4:180` 한 줄만 적었다. 실제로는 **완료 조건(`:19-20`)·흐름도(`:91`)·코드(`:119,124`)·순서 규칙(`:134,141`)·필드 선언 참조(`:173,181`)·함정 8(`:262`)** 이 전부 걸린다 | ★★ |
| **6** | **`05_Loot_DOCS.md:169, :521, :590`** | §7-1은 §8 확정표(`:802`)만 적었다. `:521`이 **§4-8 본문**(*"별도 장비 슬롯 배열을 만들지 않고 … `EquippedEntryId`"*), `:169`가 색인, `:590`이 단계표다 | ★ |
| **7** | **`05_Loot_03_Inventory.md:422-430, :1151, :1288-1289`** | §7-3이 9곳을 적었으나 셋이 빠졌다. **`:422-430`** 은 *"★★ `EquippedEntryId`도 `RemoveEntry`가 비운다"* 절 **전체**(8차 §5-1의 결과물)로 **통째로 삭제**된다. **`:1151`** 은 `SetEquippedEntryId`가 알림을 쏜다는 규칙 — 함수 자체가 사라진다. **`:1288-1289`** 는 범위 밖 목록 | ★★ |

**§7과 무관하게 낡은 것이 하나 더 있다** (§1 참조):

| # | 위치 | 문제 |
|---|---|---|
| 8 | `LOOT_STATUS.md:70` | `FEPInventoryEntry 위치` 행이 아직 *"`Build.cs` 수정 불필요"* 다. **8차 정정이 이 행에 반영되지 않았다** |

### 9-1. §7-5(유지되는 것) 검토 — 대체로 맞다

| §7-5의 주장 | 판정 |
|---|---|
| `FEPInventoryEntry` 필드 5개 변경 없음 | ✅ 맞다 |
| `ParentEntryId` 평면 표현 | ✅ |
| `AddSubtree`의 `OldToNew` 재매핑 | ✅ — **단 §7-3의 서브트리 자동 장착이 새 소비자다**(루트에만 `SlotId` 세팅) |
| `RemoveEntry` 캐스케이드·전위 순회 계약 | ✅ — **단 4단계 중 ①(write-back) 안의 두 분기가 §5-2에 의해 사라진다.** 순서 계약 자체는 유지 |
| 함정 **3b** 남는다 | ✅ 맞다. write-back 소실은 값의 문제다 |
| `bFungible` / `FindFungibleEntryId(Container, ...)` | ✅ — **그리고 `GetEntryInSlot`이 같은 교훈을 못 받았다** (§3-1) |
| 03-A/B/C 분할 재확인 필요 | ✅ 맞다 → §10 |

---

## 10. 03-A/B/C 분할 재조정

**§7-5의 마지막 줄이 맞다.** 8차의 03-7 사고와 같은 패턴이다 — *"03-B가 쓰는 것이 03-A에 없으면 컴파일이 안 된다."*

| 구간 | 들어가는 것 | 완료 조건 |
|---|---|---|
| **03-A 코어** | 03-1 · 03-2 · 03-3 · **03-7** · 03-9<br>**＋ `GetEntryInSlot(Parent, SlotId)`**<br>**＋ `MoveEntry` (사이클 검사 포함)**<br>**＋ `ActiveHotbarIndex` 필드**<br>**＋ `FEPItemData::SlotPriority` (DT 필드 + 배낭 행)**<br>**＋ `UEPLootDeveloperSettings::BodySlots`** | 2~6 |
| **03-B 배낭** | 03-6 + `GetCapacity(컨테이너)`<br>**＋ `Server_EquipBackpack` → `MoveEntry` 래퍼**<br>**＋ `TryAutoEquip` (일반형)** | 7 |
| **03-C 줍기·버리기** | 03-4 · 03-5<br>**＋ `GetInsertionOrder()`** | 1, 8~13 + 이월 2건 |

**근거:**

- **`MoveEntry`가 03-A여야 하는 이유는 03-7과 같다.** 03-B의 `Server_EquipBackpack`이 이것의 래퍼이므로, 정의가 03-B에 있으면 래퍼와 본체가 같은 구간에 갇혀 "얇은 래퍼"라는 설계가 검증되지 않는다. 그리고 `MoveEntry`는 `RemoveEntry`·`AddSubtree`에 의존하지 않으므로 **03-A 단독 컴파일 조건을 깨지 않는다.**
- **`GetEntryInSlot`이 03-A인 이유:** 03-B의 `GetCapacity`가 배낭을 찾는 데 쓰고, 03-A의 `GetUsedSlots`도 슬롯 판정을 한다.
- **`SlotPriority`가 03-A인 이유:** DT 필드 추가는 **코드가 아니라 데이터 마이그레이션**이라 늦을수록 비싸다(§6-4). 03-B의 `TryAutoEquip`이 첫 소비자다.
- **`BodySlots`가 03-A인 이유:** `MoveEntry`의 §3-2 검사가 이걸 읽는다. `MoveEntry`와 같은 구간이어야 한다.
- **`GetInsertionOrder`가 03-C인 이유:** 소비자가 `AEPPickup::OnInteract`(03-4) 하나뿐이고 `AddSubtree`에 의존한다.

> **03-A의 검증 문장은 그대로 유지된다** — *"`RemoveEntry`/`AddSubtree` 없이 컴파일·실행된다."* 위 추가분 어느 것도 그 둘을 부르지 않는다. 확인함.

---

## 11. 권장 작업 순서

**아래는 제안이다. 적용 여부는 사용자가 결정한다.**

| # | 작업 | 대상 | 왜 이 순서인가 |
|---|---|---|---|
| **1** | **§2 판정 적용 — `SlotId`가 진실.** `05_Loot_DOCS.md:802`·`:521`, `LOOT_STATUS.md:54`·`:76` 확정표 4행 교체 | 확정표 | 이게 안 정해지면 §3~§7이 전부 무의미하다. **확정표가 4곳에 흩어져 있으므로 한 번에 고친다** |
| **2** | **★ `GetEntryInSlot(Parent, SlotId)` — 부모 인자를 붙인다.** `EquipmentSlots.md` §3·§7-3 수정 | `EquipmentSlots.md` | **문서에 잘못된 시그니처가 박히기 전에.** 8차의 `FindFungibleEntryId`가 문서를 먼저 고쳐서 살았다 |
| **3** | **★ `BodySlots` + `SlotId`↔`ParentEntryId` 정합 검사**를 `MoveEntry` 절에 명시 | `EquipmentSlots.md` §6, 03-2 | 없으면 "가방 안에 든 상의를 입는다"가 표현 가능하다 (§3-2) |
| **4** | `MoveEntry`는 만들고 `Server_MoveEntry`는 만들지 않는다 — §6의 RPC 블록 삭제, `Server_EquipBackpack` 유지 | `EquipmentSlots.md` §6 | 검증 표면을 소비자보다 먼저 열지 않는다 (§4-2) |
| **5** | 사이클 검사를 **계약**으로 승격 — `RemoveEntry:393` 주석과 상호 참조 | 03-2 | 안 넣으면 서버가 멈춘다 (§4-3) |
| **6** | **`AllowedSlots` → `SlotPriority` 개명** + 주석 | `EquipmentSlots.md` §5, `EPItemData.h` | 이름이 순서 의미를 지고 가게 (§6-2) |
| **7** | **§5의 *"부착물은 따로 필드가 필요 없다"* 삭제** + `AttachmentSlots`를 §7-3 예정 항목으로 | `EquipmentSlots.md` §5 | 틀린 결론이다 (§3-3) |
| **8** | **`HotbarRefs`를 Step 03에서 빼고 Step 05 완료 조건으로** — §4의 *"제거 경로 셋"* 문장 정정 | `EquipmentSlots.md` §4, `05_Loot_05_Equipment.md` | 근거가 사실이 아니다 (§5-3) |
| **9** | **03-A/B/C 분할 재조정** — §10 표 | `05_Loot_03_Inventory.md`, `LOOT_STATUS.md`, `05_Loot_03_Inventory_STATUS.md` | 8차 03-7 사고 재발 방지 |
| **10** | **§9 누락 7건 반영** — 특히 **`BACKLOG.md` B-5**와 **`StudyPath.md:926-935`** | 7개 파일 | B-5는 Step 05 이월 목록에 이름이 있고, StudyPath는 사용자가 외우는 문서다 |
| **11** | `LOOT_STATUS.md:70`의 `Build.cs` 서술 정정 | `LOOT_STATUS.md` | 8차 정정 미반영분 (§1) |
| **12** | `TryAutoEquipBackpack` → `TryAutoEquip` 일반화 + 획득 2단계 명시 | 03-4 · 03-6 | §7-3 |
| **13** | **03 골격 코드 4건** — `EPInventoryTypes.h` include / `Owner`를 `TObjectPtr<UActorComponent>`로 / `SetIsReplicatedByDefault(true)` / `bCanEverTick=false` | 코드 | 검수 대상은 아니나 **`SetIsReplicatedByDefault`는 완료 조건 1·12에 직결**된다 |

**하지 않는 것:**

- 별도 장비 슬롯 배열(B)
- `EquippedEntryId` / `EquippedBackpackEntryId` 유지
- `FGameplayTagContainer` / `EEPEquipSlot` 비트마스크
- `PreferredSlot` 별도 필드
- `Server_MoveEntry` (Step 04)
- `HotbarRefs` 필드·청소 (Step 04/05)
- `UEPWeaponDefinition::AttachmentSlots` 실제 추가 (§7-3) — **문서에 예고만**
- `GetEntryInSlot` 결과 캐시 / `PostReplicatedReceive`의 `TMap`
- 본체 10칸 제거 (§7-1)
- 원자적 슬롯 교체 (Step 04)

---

## 12. 인용 목록

**엔진** — `C:\Program Files\Epic Games\UE_5.7\Engine\Source`

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `Runtime/Engine/Classes/Components/SceneComponent.h:109` | `TObjectPtr<USceneComponent> AttachParent` | §2-1 |
| `…SceneComponent.h:112-113` | `UPROPERTY(ReplicatedUsing=OnRep_AttachSocketName) FName AttachSocketName` | §2-1, §8-3 |
| `…SceneComponent.h:118-119` | `AttachChildren` — `Transient` + `ReplicatedUsing` | §2-1, §2-5 |
| `Runtime/Engine/Classes/Animation/AnimMontage.h:83-92` | `FSlotAnimationTrack::SlotName` (`FName`) | §2-1, §8-3 |
| `…AnimMontage.h:688` | `TArray<FSlotAnimationTrack> SlotAnimTracks` | §2-1 |
| `Runtime/CoreUObject/Private/UObject/CoreNet.cpp:306-360` | `StaticSerializeName` — 하드코딩 `EName`이 아니면 **문자열**로 복제 | §8-5 |
| `Runtime/Net/Core/Private/Net/Serialization/FastArraySerializer.cpp:24-36` | 기반 생성자가 `SetDeltaSerializationEnabled(true)` 호출 | §8-5 |
| `Runtime/Net/Core/Classes/Net/Serialization/FastArraySerializer.h:218-221` | *"Delta Serialization for inner structs is now enabled by default"* | §8-5 |
| `…FastArraySerializer.h:302-323` | 복사 생성자·`operator=`가 `ReplicationID`를 `INDEX_NONE`으로 리셋 | §8-5 |
| `…FastArraySerializer.h:395-404` | `EFastArraySerializerDeltaFlags` | §8-5 |
| `…FastArraySerializer.h:416` | `TMap<int32,int32> ItemMap` (키가 `ReplicationID`) | §8-4 |
| `…FastArraySerializer.h:1395-1400` | 델타 활성 판정 | §8-5 |
| `Runtime/GameplayTags/Classes/GameplayTagContainer.h:625,629` | `GameplayTags` + `ParentTags` — 집합 의미 | §6-1 |
| `…GameplayTagContainer.h:304-310` | `HasTag`가 부모 태그까지 본다 | §6-1 |
| `Plugins/Runtime/GameplayAbilities/…/Public/GameplayEffect.h:1334, 1639` | `FActiveGameplayEffect(sContainer)`가 FastArray | §8-4 |
| `…/Private/GameplayEffect.cpp:3323-3345` | 핸들 조회가 **맨몸 선형 탐색** | §2-5, §8-4 |
| `…/Private/GameplayEffect.cpp` · `AbilitySystemComponent.cpp` | `GetActiveGameplayEffect(` 호출 **13곳 + 14곳** | §8-4 |
| `Engine/Plugins/` (전수) | **인벤토리·장비 플러그인 없음** | §8-1 |

**Lyra** — `C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame\Source\LyraGame`

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `Equipment/LyraEquipmentDefinition.h:36-56` | **헤더 전체.** 필드 셋, 슬롯 관련 **0개** | §2-2, §8-2 |
| `Equipment/LyraEquipmentManagerComponent.cpp:148, 167` | `EquipItem(TSubclassOf<...>)` / `UnequipItem(Instance*)` — **슬롯 파라미터 없음** | §2-2, §8-2 |
| `Equipment/LyraQuickBarComponent.h:66` | `int32 NumSlots = 3` | §2-2, §5-4 |
| `Equipment/LyraQuickBarComponent.h:73-77` | `Slots` / `ActiveSlotIndex` | §2-2 |
| `Equipment/LyraQuickBarComponent.cpp:149-152` | `GetActiveSlotItem()` — Lyra가 던지는 유일한 슬롯 질문 | §2-2, §8-2 |
| `Equipment/LyraQuickBarComponent.cpp:154-167` | `GetNextFreeItemSlot()` — 선형 탐색 | §2-2 |
| `Equipment/LyraQuickBarComponent.cpp:169-179` | `AddItemToSlot` — **`nullptr`일 때만 대입**(교체 없음). 인벤토리에서 빼지 않음 | §2-6, §4-4 |
| `Equipment/LyraQuickBarComponent.cpp:205-213` | `OnRep_Slots`가 **배열 전체**를 메시지에 실어 브로드캐스트 | §2-5 |
| `Inventory/LyraInventoryManagerComponent.cpp:159-163` | `CanAddItemDefinition`이 `//@TODO`와 함께 무조건 `true` | §2-3, §8-1 |
| `Inventory/LyraInventoryManagerComponent.cpp:189-197` | `RemoveItemInstance`가 **퀵바를 건드리지 않는다** | §2-4 |
| `grep -rn "RemoveItemFromSlot" LyraGame/` | 정의·선언뿐, **호출부 0곳** | §2-4 |
| `Inventory/LyraInventoryManagerComponent.h:112-113` | `TObjectPtr<UActorComponent> OwnerComponent` — 구체 타입을 안 쓴다 | §11-13 |

**프로젝트**

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `EmploymentProj.Build.cs:11` | `NetCore` 추가 확인 | §1 |
| `Public/Data/EPItemData.h:36-58` | `MaxStack`/`SlotSize`/`ContainerCapacity`/`bFungible` | §6 |
| `Public/Types/EPTypes.h:42-50` | `EEPItemType` 다섯 — 무기 세분 없음 | §6 |
| `Public/Types/EPTypes.h:76-86` | `FEPItemState` | §10 |
| `Public/Inventory/EPInventoryComponent.h:19, 21` | `FEPInventoryEntry` 불완전 타입, `Owner` 전방선언 없음 | §11-13 |
| `Private/Inventory/EPInventoryComponent.cpp:10` | `SetIsReplicatedByDefault` 없음, `bCanEverTick=true` | §11-13 |
| `05_Loot_03_Inventory.md:253` | `SetEntryCharges` — *"★ 유일한 쓰기 지점"* | §4-1 |
| `05_Loot_03_Inventory.md:290-292` | `InsertEntry` — *"삽입·`MarkItemDirty`의 유일한 지점"* | §4-1 |
| `05_Loot_03_Inventory.md:294` | `RemoveSelf` — *"배열에서 빼고 `MarkArrayDirty`"* | §5-3 |
| `05_Loot_03_Inventory.md:355-395` | `RemoveEntryInternal` 4단계. `:393` 사이클 안전성 주석 | §4-3, §5-3 |
| `05_Loot_03_Inventory.md:577` | `GetUsedSlots`의 `if (!E.SlotId.IsNone()) continue;` | §2-3, §5-1 |
| `LOOT_STATUS.md:69` | 8차 확정 — `FindFungibleEntryId(Container, ItemId)` | §3-1 |
| `LOOT_STATUS.md:73` | 확정표 *"내부 struct 델타 기본 활성"* | §8-5 |
