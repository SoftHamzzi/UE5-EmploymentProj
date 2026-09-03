# 검수 요청 9차 — 기획 확대(언턴드식 슬롯 12개)가 Step 03 자료구조를 뒤집는가

> 작성일: 2026-08-22
> 8차: `05_Loot_REVIEW8_Request.md` / `_Answer.md` (Step 03 — **`Server_DropItem` 직접 RPC로 확정**, 03-A/B/C 분할 재조정)
> 시점: **Step 03 골격만 존재(105줄, 실질 0).** 8차 이후 문서는 여러 번 수정됐고 코드는 아직 없다
> 성격: **기획이 확대됐다.** 장비 슬롯이 2개(무기1+배낭1)에서 **12개**(핫바 4 + 착용 8)로 늘고, 인벤토리 용량이 착용 아이템에서 나오는 구조가 된다. **§8 확정표 두 줄을 뒤집는 제안**을 담고 있다

---

## 0. 사용자 입장 (먼저 밝힌다)

**기획이 확정적으로 바뀌었다.** 언턴드(Unturned) 방식이다 — 옷을 입어야 인벤토리 칸이 생기고, 핫바가 10칸이고, 착용 슬롯이 8개다. 자세한 것은 §1-2.

**Claude가 자료구조를 바꾸지 않는 방향으로 판단했다.** `ParentEntryId` + `SlotId` 조합이 이미 전부 표현한다는 것이다. 대신 **`EquippedEntryId` / `EquippedBackpackEntryId` 필드를 삭제**하고 `SlotId`를 유일한 진실로 삼자고 제안했다(§2).

**그런데 Lyra는 정확히 반대로 한다.** 별도 배열(`TArray<TObjectPtr<...>> Slots`)에 슬롯을 담고, 인벤토리 엔트리는 슬롯을 모른다(§1-3, §2-3). **이 대비가 이번 검수의 핵심이다.**

**네 가지를 요청한다.**

1. **§2를 먼저 판정해달라.** `SlotId`를 진실로 삼는 것이 옳은가, Lyra처럼 별도 배열이 옳은가. Claude는 전자로 판단했고 근거를 §2-2에 폈지만, **Lyra의 반례가 강하다.** 이게 뒤집히면 §3~§5가 전부 무의미해진다
2. **§3(`MoveEntry`)이 계약인지 과설계인지 판정해달라.** CLAUDE.md §2는 *"두 번째 구현자가 없는 인터페이스를 만들지 않는다"* 고 하는데, Claude는 *"나중에 넣기 비싼 계약"* 으로 분류했다. 둘 중 하나가 틀렸다
3. **§4~§6은 근거 검증이다.** 핫바 이원화·`AllowedSlots`·본체 10칸 유지. 판단보다 "빠뜨린 게 있는가"를 봐달라
4. **§7의 파급 목록이 완전한지 확인해달라.** 8차 확정 두 줄을 뒤집는 것이라, 뒤집으면 딸려 오는 것을 빠뜨렸을 가능성이 있다

---

## 1. 현재 상태 (사실만)

### 1-1. 진행

| | 상태 |
|---|---|
| Step 00 (ItemCore) | 완료 |
| Step 01 (Spawner) | 완료. `EP.Loot.RollTable` 1000회 등급 비율 검증만 미실행 |
| Step 02 (Interaction) | 완료. PIE 2인 검증, 태그 `step5-2` |
| **Step 03 (Inventory)** | **골격 105줄.** `EPInventoryTypes.h`(19줄) / `EPInventoryComponent.h`(52줄) / `.cpp`(34줄). **로직 0줄** |

**8차 이후 실제로 짜인 것은 `FEPInventoryEntry` 필드 5개와 `FEPInventoryList` + 트레이트뿐이다.** 되돌리는 비용이 사실상 없다 — **지금이 구조를 바꿀 마지막 무료 구간이다.**

> 참고: 현재 골격에 컴파일 문제 3건이 있다(`.generated.h` include 누락, `Net/Serialization/FastArraySerializer.h`·`Types/EPTypes.h` 미포함, `TArray<class FEPInventoryEntry>`가 불완전 타입). **이번 검수 대상이 아니다** — 사용자에게 이미 보고했고 기계적 수정이다.

### 1-2. 확정된 새 기획 (사용자 발화 그대로)

**장비 슬롯 12개. 전부 인벤토리 칸을 먹지 않는다.**

| 그룹 | 슬롯 | 종류 제한 | 배정 | 인벤토리 그룹을 여는가 |
|---|---|---|---|---|
| 핫바 1 | `Hotbar1` | 무기 전부 | **자동** | ✗ |
| 핫바 2 | `Hotbar2` | 무기 전부 | **자동** | ✗ |
| 핫바 3 | `Hotbar3` | 보조무기 | **자동** | ✗ |
| 핫바 4 | `Hotbar4` | 근접무기 | **자동** | ✗ |
| 착용 | `Torso` 상의 | | 드래그 | ✅ |
| 착용 | `Legs` 하의 | | 드래그 | ✅ |
| 착용 | `Coat` 외투 | | 드래그 | ✅ |
| 착용 | `Back` 등(가방) | | 드래그 | ✅ |
| 착용 | `Wrist` 팔목 | | 드래그 | ✅ (타르코프 Pouch) |
| 착용 | `Ears` 귀 | | 드래그 | ✗ |
| 착용 | `Face` 얼굴 | | 드래그 | ✗ |
| 착용 | `Feet` 신발 | | 드래그 | ✗ |

**핫바 5~0(6칸)은 성격이 다르다.** 인벤토리 그룹에서 드래그해 배정하되, **아이템은 인벤토리에 그대로 남고 단축키만 걸린다.** 칸도 그대로 먹는다.

**용량이 착용에서 나온다.** 아무것도 안 입으면 인벤토리가 0칸. 그룹 크기는 아이템마다 다르다(10칸/20칸 등). 각 그룹은 독립된 풀이다.

**핫바 1~4는 직접 수정할 수 없다.** 무기를 주우면 자동으로 들어간다. 교체·해제는 드래그, 바깥으로 끌면 버린다.

### 1-3. 확인된 사실 (다시 파지 말 것 — 전부 이번 세션에 직독 확인함)

| 사실 | 출처 |
|---|---|
| **Lyra의 퀵바는 별도 배열이다** — `UPROPERTY(ReplicatedUsing=OnRep_Slots) TArray<TObjectPtr<ULyraInventoryItemInstance>> Slots;` + `int32 ActiveSlotIndex` | `LyraQuickBarComponent.h:73-77` |
| **Lyra의 인벤토리 엔트리는 슬롯을 모른다** — 필드가 `Instance` / `StackCount` / `LastObservedCount` 셋뿐 | `LyraInventoryManagerComponent.h:43-65` |
| **Lyra의 장비는 또 다른 FastArray다** — `FLyraAppliedEquipmentEntry`(`EquipmentDefinition` / `Instance` / `GrantedHandles`). 슬롯 이름 필드 없음 | `LyraEquipmentManagerComponent.h:26-49` |
| **★ Lyra에는 용량 개념이 아예 없다** — `CanAddItemDefinition`이 `//@TODO`와 함께 무조건 `true`를 반환한다 | `LyraInventoryManagerComponent.cpp:159-163` |
| `AddItemToSlot`은 포인터만 세팅한다. **인벤토리에서 제거하지 않는다** — 아이템이 양쪽에 동시에 존재한다 | `LyraQuickBarComponent.cpp:169-179` |
| Lyra 게임 모듈에서 손으로 쓴 서버 RPC는 **`SetActiveSlotIndex` 하나**. 검증이 `Slots.IsValidIndex(NewIndex) && (ActiveSlotIndex != NewIndex)` 한 줄 | `LyraQuickBarComponent.h:30-31`, `.cpp:137` |
| `FEPItemData`에 `ContainerCapacity`(:42)·`SlotSize`(:39)·`bFungible`(:56)이 이미 있다. **슬롯 종류를 나타내는 필드는 없다** | `EPItemData.h:39-56` |
| `EEPItemType`은 `Weapon`/`Ammo`/`Consumable`/`QuestItem`/`Misc` 다섯. **주무기·보조·근접 구분 없음** | `EPTypes.h:43-50` |
| `GetUsedSlots`가 `if (!E.SlotId.IsNone()) continue;` 로 부착물을 칸 계산에서 뺀다 | `05_Loot_03_Inventory.md:577` |
| `GAME.md:163`은 "주무기 1 + 보조무기 1", `:158`은 "본체 인벤토리 10칸" | `GAME.md` |
| `FFastArraySerializerItem`의 복사 생성자·`operator=`가 `ReplicationID`를 `INDEX_NONE`으로 리셋한다 | `FastArraySerializer.h:302-323` |

### 1-4. 8차 이후 확정된 것 (배경)

- **드랍은 `Server_DropItem` 직접 RPC**로 확정. "클라가 서버에 요청하는 경로는 둘 — 월드 상호작용/시간 붙는 행동은 어빌리티, 서버가 이미 소유한 상태의 변경 요청은 컴포넌트 서버 RPC"
- **03-7(알림)이 03-A로 이동.** `FScopedInventoryNotify`를 03-3이 쓰므로
- **`Build.cs`에 `NetCore` 추가 필요** — 이번 세션에 링크 에러로 실증됨(`LNK2019: Z_Construct_UScriptStruct_FFastArraySerializerItem`). 문서의 "손댈 필요 없다"는 서술은 **틀렸고 수정 완료**

---

## 2. ★ 최대 주제 — `SlotId`를 장착의 유일한 진실로 삼을 것인가

### 2-1. 제안 (Claude의 판단)

```cpp
// 삭제
UPROPERTY(Replicated) int32 EquippedEntryId         = INDEX_NONE;
UPROPERTY(Replicated) int32 EquippedBackpackEntryId = INDEX_NONE;

// 대신 — 엔트리의 SlotId를 읽는 파생 게터
int32 GetEntryInSlot(FName SlotId) const;
int32 GetEquippedBackpack() const { return GetEntryInSlot(TEXT("Back")); }

// 남는 진짜 상태 하나 (SlotId로 표현 불가)
UPROPERTY(Replicated) int32         ActiveHotbarIndex = INDEX_NONE;  // 0~9
UPROPERTY(Replicated) TArray<int32> HotbarRefs;                      // 핫바 5~0
```

표현은 이렇게 된다:

```
상의        ParentEntryId = -1,      SlotId = "Torso"     ← 칸 안 먹음. 그룹을 연다
├─ 붕대     ParentEntryId = 상의,    SlotId = None
배낭        ParentEntryId = -1,      SlotId = "Back"      ← 칸 안 먹음. 그룹을 연다
└─ 소총     ParentEntryId = 배낭,    SlotId = None
AK-74       ParentEntryId = -1,      SlotId = "Hotbar1"   ← 칸 안 먹음
└─ 스코프   ParentEntryId = AK-74,   SlotId = "Optic"     ← §7-3 부착물
```

### 2-2. 찬성 근거

**(a) 이중 진실이 안 생긴다.** `SlotId == "Back"`이 이미 "이 배낭은 등에 있다"를 말하는데 필드가 따로 있으면 동기화 코드가 생기고 그걸 빠뜨리는 경로가 생긴다. CLAUDE.md §2의 *"한 값을 두 경로가 봐야 하면 둘 다 볼 수 있는 곳에 둔다"* 에 직결된다.

**(b) 슬롯이 2 → 12개다.** 필드 방식이면 필드가 12개가 되거나 `TMap<EEPEquipSlot, int32>`를 따로 복제해야 한다. `SlotId`는 **이미 엔트리 안에 있고 이미 복제된다.** 추가 대역폭 0.

**(c) `RemoveEntry`의 번호 비우기 분기 두 개가 사라진다.** 엔트리가 배열에서 빠지면 파생값도 자동으로 사라져 **죽은 번호를 남길 방법이 문법적으로 없다.** 함정표 **3h**(`EquippedEntryId`를 `UnequipWeapon`에 맡김 → 죽은 번호)와 **5b**(유령 구획)가 통째로 삭제된다.

**(d) ★ 칸 계산이 이미 `SlotId`를 본다.** `GetUsedSlots`의 `if (!E.SlotId.IsNone()) continue;` 한 줄이 **장비·핫바·부착물을 전부 칸 계산에서 뺀다.** 별도 배열 방식이면 이 줄로는 부족하고, "장비 배열에 있는가"를 추가로 조회해야 한다 — **한 칸 판정이 두 자료구조를 봐야 한다.**

### 2-3. ★ 반대 근거 — Lyra는 정반대로 한다

**Lyra는 슬롯을 인벤토리 엔트리에 두지 않는다.**

```cpp
// LyraQuickBarComponent.h:73-77 — 별도 배열, 인덱스가 곧 슬롯
UPROPERTY(ReplicatedUsing=OnRep_Slots)
TArray<TObjectPtr<ULyraInventoryItemInstance>> Slots;

UPROPERTY(ReplicatedUsing=OnRep_ActiveSlotIndex)
int32 ActiveSlotIndex = -1;
```

```cpp
// LyraInventoryManagerComponent.h:43-65 — 엔트리는 슬롯을 모른다
struct FLyraInventoryEntry : public FFastArraySerializerItem
{
    TObjectPtr<ULyraInventoryItemInstance> Instance;
    int32 StackCount;
    int32 LastObservedCount;   // NotReplicated
};
```

**별도 배열의 장점이 실제로 있다.**
- 슬롯 조회가 **O(1)** — `Slots[Index]`. 우리 제안은 O(N) 선형 탐색
- **빈 슬롯을 표현할 수 있다** — `Slots[2] == nullptr`. `SlotId` 방식은 "없다"를 전체 순회 후에야 안다
- **슬롯 순서·개수가 자료구조에 박혀 있다** — `NumSlots = 3`. UI가 그대로 그린다
- `OnRep_Slots` 하나로 퀵바만 갱신된다 — 인벤토리 전체 재생성이 아니다

### 2-4. ★ 그런데 Lyra의 사정이 우리와 다르다 (이 대비를 판정해달라)

| | Lyra | 우리 |
|---|---|---|
| **용량 개념** | **없다.** `CanAddItemDefinition`이 `//@TODO`와 함께 무조건 `true` (`LyraInventoryManagerComponent.cpp:159-163`) | **핵심이다.** 칸 합산이 완료 조건 2~6 |
| 퀵바 아이템이 인벤토리에 | **남아 있다.** `AddItemToSlot`이 포인터만 세팅(`:169-179`) | **나가야 한다.** 안 나가면 칸을 먹는다 |
| 엔트리 표현 | `UObject*` 포인터 | **값 타입**(1·2차 확정) |
| 컨테이너 중첩 | 없다 | `ParentEntryId` (§4-6 확정) |
| 슬롯 수 | 3 | **12 + 부착물 4** |

**Claude의 해석:** Lyra가 별도 배열로 갈 수 있는 것은 **용량 계산을 안 하기 때문**이다. 칸을 세는 순간 "이 아이템이 장비인가"를 칸 계산 루프가 알아야 하고, 그러면 엔트리 자신이 답을 들고 있는 편이 자연스럽다.

**이 해석이 맞는가.** 아니면 용량이 있어도 별도 배열이 낫고, 칸 계산 쪽에서 장비 배열을 조회하면 되는가.

### 2-5. 알고 있는 대가

- **조회가 O(N)**. N은 수십(본체 10칸 + 배낭 20칸 기준 최대 30여 개). 매 프레임 도는 경로는 아니다 — 장착 시점, UI 갱신 시점, 그리고 **사격마다 도는 `AddEntryCharges(GetEquippedEntryId(), -1)`**
- **빈 슬롯 조회도 O(N)** — UI가 12개 슬롯을 그리려면 12 × N. 순회 한 번으로 `TMap<FName,int32>`를 만들어 넘기는 방식이 필요할 수 있다
- **알림이 거칠어진다** — `PostReplicatedReceive` 하나로 UI 전체 재갱신(03-7 확정). 퀵바만 바뀌어도 인벤토리 전체를 다시 그린다

### 2-6. 판정 요청

1. **A(`SlotId`가 진실) / B(Lyra식 별도 배열) 중 무엇인가.** 근거를 §2-4의 대비에 대해 명시적으로 답해달라
2. **B라면** 칸 계산(`GetUsedSlots`)이 장비 배열을 어떻게 조회하는가. 그리고 `RemoveEntry`의 죽은 번호 문제(§2-2c)를 어떻게 막는가
3. **A라면** §2-5의 세 가지 대가 중 지금 대비해야 할 것이 있는가. 특히 **UI가 12개 슬롯을 그리는 비용**

---

## 3. `MoveEntry()` — 계약인가 과설계인가

### 3-1. 제안

```cpp
bool MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId);

UFUNCTION(Server, Reliable)
void Server_MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId);
```

`SlotId`가 진실이 되면 바뀌는 것이 `ParentEntryId`와 `SlotId` 둘뿐이므로, 다음이 전부 한 연산이 된다.

| 동작 | 호출 | 어느 단계 |
|---|---|---|
| 배낭 매기 | `MoveEntry(id, -1, "Back")` | **03-6 (지금)** |
| 컨테이너 간 이동 | `MoveEntry(id, 배낭Id, None)` | Step 04 (드래그) |
| 무기 장착 | `MoveEntry(id, -1, "Hotbar1")` | Step 05 |
| 해제 | `MoveEntry(id, 상의Id, None)` | Step 05 |
| 부착물 달기 | `MoveEntry(id, 총Id, "Optic")` | §7-3 |

`Server_EquipBackpack`(03-6)은 이것의 얇은 래퍼가 된다.

### 3-2. 안에 들어가는 검사

| 검사 | 이유 |
|---|---|
| `ContainsEntry(EntryId)` | 조작된 요청 |
| `NewSlotId`가 `AllowedSlots`에 있는가 | §5. `None`이면 통과 |
| `NewSlotId` 슬롯이 비었는가 | 차 있으면 실패 (교체는 UI가 두 번 부른다) |
| `NewSlotId == None`이면 `CanFit(NewParent, ItemId)` | 옮겨갈 컨테이너 자리 |
| **`NewParent`가 자기 자손이 아닌가** | **배낭을 자기 안에 넣기 금지.** 안 막으면 서브트리가 순환하고 `RemoveEntry` 재귀가 무한이 된다 |

### 3-3. 상충하는 두 규칙

**"만든다" 쪽** — CLAUDE.md §2: *"나중에 넣기 비싼 것은 지금 넣는다 — 식별자 안정성, 복제 조건, **계약(반환 규약·순서)**"*. 그리고 *"지금 소비자가 하나여도, 문서에 두 번째 소비자가 예고돼 있으면 그 자리를 만든다"*. 위 표에서 Step 04·05·§7-3 소비자가 문서에 이름으로 있다.

**"만들지 않는다" 쪽** — 같은 절: *"두 번째 구현자가 없는 인터페이스·베이스 클래스"*, *"도달 불가한 분기의 에러 처리"*. **Step 03에서 실제로 도는 호출은 `MoveEntry(id, -1, "Back")` 하나뿐이다.** 나머지 넷은 전부 다음 단계다. `AllowedSlots` 검사도 Step 03에서는 항상 통과한다(슬롯이 `"Back"` 하나).

### 3-4. 판정 요청

1. **`MoveEntry`를 지금 넣는가, `Server_EquipBackpack`만 두고 Step 04에서 일반화하는가**
2. **사이클 검사(자기 자손)를 지금 넣는가.** Step 03에서는 배낭을 배낭에 넣는 UI 경로가 없어 도달 불가하다. 그런데 도달하면 **서버가 멈춘다**. "도달 불가한 분기의 에러 처리"의 예외로 볼 것인가
3. **"슬롯이 차 있으면 실패"가 맞는가.** UI가 해제→장착 두 번 부르는 것으로 충분한가, 원자적 교체(swap)가 필요한가. 두 번 부르면 **중간 상태에서 첫 아이템이 갈 곳이 없을 수 있다**(가방이 꽉 참)

---

## 4. 핫바 이원화 — 1~4와 5~0이 자료구조가 다르다

### 4-1. 제안

| | 1~4 | 5~0 |
|---|---|---|
| 아이템이 어디 있나 | **몸에** (인벤토리 밖) | **인벤토리 그룹 안** |
| 칸 | 안 먹는다 | **먹는다** |
| 표현 | `SlotId = "HotbarN"` | `TArray<int32> HotbarRefs` (참조) |
| 배정 | 자동 | 드래그 |

**5~0에 `SlotId`를 쓰면 안 된다.** 쓰는 순간 `GetUsedSlots`의 `if (!E.SlotId.IsNone()) continue;` 에 걸려 그 아이템이 칸 계산에서 빠진다. 증상은 *"붕대를 5번에 걸면 가방이 한 칸 늘어난다"* 이고 원인이 UI로 보인다.

### 4-2. 끊어진 참조

5번에 걸어둔 붕대를 다 쓰면 엔트리가 사라지는데 `HotbarRefs[0]`은 그걸 모른다. **`RemoveEntry`가 청소한다**로 제안했다.

**세팅 경로는 Step 05인데 제거 경로는 Step 03에 이미 셋이다** — 버리기·사용·캐스케이드. 그래서 필드 선언과 청소는 Step 03, 세팅은 Step 05로 갈랐다.

> 8차가 지적한 *"Step 03 내내 항상 거짓인 분기"*(`EquippedEntryId`)와 같은 패턴이 하나 더 생긴다. **의도적이지만 옳은지 확인해달라.**

### 4-3. 판정 요청

1. **이원화가 맞는가.** 하나로 합치는 표현이 있는가 (예: `SlotId`에 넣되 칸 계산에서 `Hotbar5`~`Hotbar0`만 예외 처리 — Claude는 **예외가 칸 계산에 들어가는 순간 더 나쁘다**고 봤다)
2. **`HotbarRefs`를 `TArray<int32>` 고정 6칸으로 두는 것이 맞는가.** `TMap<int32,int32>`나 `ActiveHotbarIndex`처럼 별도 필드가 나은가
3. **끊어진 참조를 `RemoveEntry`에서 청소하는 것이 맞는가.** 아니면 조회 시점 검사(`ContainsEntry`)로 충분한가

---

## 5. `AllowedSlots` — 슬롯 종류 제한의 형태

### 5-1. 제안

```cpp
// FEPItemData에 추가
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
TArray<FName> AllowedSlots;   // 비어 있으면 어느 장비 슬롯에도 못 들어간다
```

| 아이템 | 값 |
|---|---|
| AK-74 | `["Hotbar1", "Hotbar2"]` |
| 권총 | `["Hotbar3", "Hotbar1", "Hotbar2"]` |
| 도끼 | `["Hotbar4", "Hotbar1", "Hotbar2"]` |
| 배낭 | `["Back"]` |
| 스코프 | `["Optic"]` |
| 붕대 | `[]` |

**배열 순서가 곧 자동 배정 우선순위다.** 권총을 주우면 3번 → 1번 → 2번 순으로 빈 곳을 찾는다. **전용 슬롯을 앞에 둔다는 규칙 하나로 분기가 사라진다.**

`EEPItemType`에 `Weapon` 하나뿐이라(`EPTypes.h:45`) 주무기·보조·근접을 구분할 수 없다. 열거형을 늘리면 슬롯이 늘 때마다 코드가 따라 는다.

### 5-2. 판정 요청

1. **`TArray<FName>`이 맞는가.** `FGameplayTagContainer`(태그 계층 `Item.Slot.Hotbar1`)나 `EEPEquipSlot` 비트마스크가 나은가. 프로젝트에 NativeGameplayTags가 이미 있다(`EPNativeGameplayTags.h`)
2. **"배열 순서 = 우선순위"가 데이터로서 견고한가.** DT를 채우는 사람이 순서의 의미를 모르면 조용히 틀린다. 별도 필드(`PreferredSlot`)로 분리해야 하는가
3. **부착물 슬롯(`Optic` 등)을 같은 필드로 처리하는 것이 맞는가.** §7-3이 별도 필드를 만들 필요가 없어지지만, "장비 슬롯"과 "부착 슬롯"은 검사 주체가 다르다(전자는 캐릭터, 후자는 무기 Definition)

---

## 6. 본체 10칸 유지 + `GetInsertionOrder()`

### 6-1. 결정

**시간 제약으로 `MaxSlots = 10`을 유지한다.** 언턴드 기준으로는 아무것도 안 입으면 0칸이어야 하지만, Step 03 완료 조건 2~6이 전부 본체 10칸 위에 서 있다. **나중에 데이터 변경만으로 없앨 수 있게** 만드는 것이 목표다.

### 6-2. 그러려면 획득 경로의 하드코딩을 빼야 한다

```cpp
// EPPickup.cpp — 03-4 현행. 컨테이너 이름이 둘 박혀 있다
int32 NewId = Inv->AddSubtree(INDEX_NONE, Payload);                       // 본체
if (NewId == INDEX_NONE && Inv->GetEquippedBackpack() != INDEX_NONE)
    NewId = Inv->AddSubtree(Inv->GetEquippedBackpack(), Payload);         // 배낭
```

```cpp
// 제안
TArray<int32> GetInsertionOrder() const;   // 지금은 [INDEX_NONE, Back]

for (int32 C : Inv->GetInsertionOrder())
    if ((NewId = Inv->AddSubtree(C, Payload)) != INDEX_NONE) break;
```

### 6-3. 판정 요청

1. **본체 10칸 유지가 Step 03에서 옳은 판단인가.** 아니면 지금 없애는 게 오히려 싼가 (완료 조건 다섯 개를 다시 짜야 한다)
2. **`GetInsertionOrder()`의 반환이 `TArray<int32>`가 맞는가.** 매 획득마다 배열을 만든다 — `TArray<int32, TInlineAllocator<4>>`가 나은가
3. **무기를 주웠을 때 `AllowedSlots`의 빈 핫바가 인벤토리보다 우선인가.** 즉 획득 순서가 "핫바 → 컨테이너"인가 "컨테이너만"인가. 기획은 *"총을 주우면 자동으로 들어간다"* 이므로 전자로 보이는데, 그러면 `GetInsertionOrder`와 자동 배정이 **한 경로에서 만난다**

---

## 7. 파급 — 8차까지 확정된 것 중 뒤집히는 것

**이 목록이 완전한지 확인해달라.** 빠뜨린 것이 이번 검수의 가장 큰 위험이다.

### 7-1. `05_Loot_DOCS.md` §8 확정표

| 항목 | 현행 확정 | 제안 |
|---|---|---|
| 장비 슬롯 | **별도 배열 없음 — `EquippedEntryId` 하나** | `SlotId`가 진실. 필드 삭제 |
| §8 미정 #5 (무기 2정) | 미정. "Step 05는 1정, `TMap<EEPEquipSlot,int32>`로 확장" | **확정으로 이동.** `TMap`이 아니라 `SlotId` |

### 7-2. `GAME.md`

| 위치 | 현행 | 제안 |
|---|---|---|
| :158 | 본체 인벤토리 10칸 | **과도기임을 명시** (§6) |
| :178-180 | 무기 슬롯 2 + 배낭 1 | **핫바 10 + 착용 8** |

### 7-3. `05_Loot_03_Inventory.md` — 9곳

03-1(`SlotId` 값 목록) / 03-2(게터·필드·`DOREPLIFETIME`·`RemoveEntry`·`MoveEntry` 신설) / 03-4(`GetInsertionOrder`) / 03-6(`GetEntryInSlot`) / 함정표(3h·5b 삭제, 3b 수정) / `:319` 세이브 항목.

### 7-4. `05_Loot_05_Equipment.md` 05-4

> "슬롯이 셋(주무기/보조/배낭)이 되면 `TMap<EEPEquipSlot, int32>`로 간다 — §8 미정 #5. 지금은 필드 둘이라 맵이 과하다"

이 문장이 통째로 대체된다.

### 7-5. Claude가 유지된다고 본 것

| | 근거 |
|---|---|
| `FEPInventoryEntry` 필드 5개 | 변경 없음. `SlotId`의 **값**만 늘어난다 |
| `ParentEntryId` 평면 표현 | 착용 컨테이너가 배낭과 같은 구조 |
| `AddSubtree`의 `OldToNew` 재매핑 | 변경 없음 |
| `RemoveEntry` 캐스케이드·전위 순회 계약 | 변경 없음 |
| 함정 **3b**(캐스케이드가 자기 자신만 장착 검사) | **남는다.** write-back 소실은 번호가 아니라 값의 문제 |
| `bFungible` / `FindFungibleEntryId(Container, ...)` | 변경 없음 |
| 03-A/B/C 분할 | **재확인 필요** — `GetEntryInSlot`이 03-6(03-B)에서 쓰이므로 03-A에 있어야 한다 |

**마지막 줄이 8차의 03-7 사고와 같은 패턴이다.** 분할선을 다시 봐달라.

---

## 8. ★ 실무 조사 요청

우리 판단만으로 결정하지 않겠다. **가능하면 실제 소스를 근거로.**

1. **용량(칸/무게) 시스템이 있는 UE 프로젝트에서 장비 슬롯을 어떻게 표현하는가.** Lyra는 용량이 없어서 참고가 제한적이다(§1-3). 엔진 샘플·플러그인에 다른 사례가 있는가
2. **Lyra의 `FLyraAppliedEquipmentEntry`에 슬롯 이름이 없는 이유.** 장비가 여러 부위에 동시에 붙는 게임이 아니라서인가, 아니면 슬롯 개념 자체를 `ULyraEquipmentDefinition`이 들고 있는가. `LyraEquipmentManagerComponent.cpp`의 `AddEntry`/`RemoveEntry` 구현과 `ULyraEquipmentDefinition`을 봐달라
3. **`TArray<FName>`으로 슬롯 호환을 표현하는 선례가 있는가.** 아니면 `FGameplayTagContainer`가 관용구인가 (§5-2)
4. **FastArray에서 "특정 조건의 원소를 찾는" 조회가 O(N)인 것이 실무에서 문제가 된 사례가 있는가.** 캐시를 붙인다면 어디에 붙이는 것이 관용적인가 — `PostReplicatedReceive`에서 `TMap`을 다시 만드는 방식이 흔한가
5. **`FFastArraySerializerItem` 파생 struct에서 `FName` 필드가 자주 바뀔 때의 대역폭.** `SlotId`는 장착/해제마다 바뀐다. `FName` 복제 비용이 `int32` 대비 유의미한가 (`NetSerialization`의 `FName` 처리)

> 로컬 경로: 엔진 `C:\Program Files\Epic Games\UE_5.7\Engine`, Lyra `C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame`, GAS 문서 `C:\Github\GASDocumentation`. **기억으로 Lyra API를 단정하지 말 것** — 6~8차에서 인용 정확도가 유용했다.

---

## 9. 재론하지 않았으면 하는 것

| | 근거 |
|---|---|
| `FEPItemState` 값 타입 / 스택 폐지 / `bFungible` 합치기 | 1·2차 확정 |
| DT/DA 두 계층 유지 | 3차 §5 확정 |
| 전역 에셋 참조를 `UEPLootDeveloperSettings`에 | 6차 확정 |
| 픽업의 전 채널 `Ignore` + Dormancy | 5·7차 확정, 구현 완료 |
| Step 02가 `UEPGA_Interact`로 가는 것 | 7차 확정, 구현·검증 완료 |
| **드랍이 `Server_DropItem` 직접 RPC인 것** | **8차 확정.** §3의 `Server_MoveEntry`도 같은 판정을 따른다 |
| `EntryId`(int32, 서버 발급, 재번호 없음) / `ParentEntryId` 평면 표현 | 1·2차 확정 |
| `COND_OwnerOnly` / 인벤토리 부착 위치 = Character | §8 확정표 |
| `UsedSlots` 캐시 안 함 / 복제 안 함 | §4-6 확정. **단 §2-5의 O(N)은 별개 주제다** |
| FastArray + 값 타입 엔트리 | §8 확정표 |
| 항목 단위 콜백을 쓰지 않고 `PostReplicatedReceive` 하나로 | 8차 확정 (03-7) |
| **기획 자체** (슬롯 12개, 용량이 착용에서 나옴, 핫바 10칸) | **사용자 확정.** 구현 방식만 논한다 |

---

## 10. 대상 파일

| 파일 | 관계 |
|---|---|
| **`DOCS/Mine/EquipmentSlots.md`** | **이번 제안의 본체.** 이번 세션에 작성 |
| `05_Loot_03_Inventory.md` | 수정 대상 9곳 (§7-3) |
| `05_Loot_DOCS.md` §4-6 / §4-8 / §7-3 / §8 | 뒤집히는 확정 (§7-1) |
| `05_Loot_05_Equipment.md` 05-4 | 대체되는 문장 (§7-4) |
| `05_Loot_04_InventoryUI.md` | 드래그 이동의 소비자 (§3) |
| `GAME.md:152-184` | §7-2 |
| `Public/Inventory/EPInventoryTypes.h` · `EPInventoryComponent.h` · `Private/Inventory/EPInventoryComponent.cpp` | 현재 골격 105줄 |
| `Public/Data/EPItemData.h:39-56` | `AllowedSlots` 추가 지점 (§5) |
| `Public/Types/EPTypes.h:43-50, 77-86` | `EEPItemType` / `FEPItemState` |
| `LyraQuickBarComponent.h:73-77` · `.cpp:137,169-179` | §2-3 반례 |
| `LyraInventoryManagerComponent.h:43-65` · `.cpp:159-163` | §2-3, §2-4 |
| `LyraEquipmentManagerComponent.h:26-49` | §2-3, §8-2 |
| `FastArraySerializer.h:302-323` | 복사 시맨틱 (§4-2 청소 논의) |
