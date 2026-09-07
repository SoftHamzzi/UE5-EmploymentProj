# 검수 요청 10차 — Step 04 UI 재설계(정사각형 격자 + 드래그)와 "순서를 서버에 두지 않는다"

> 작성일: 2026-08-23
> 9차: `05_Loot_REVIEW9_Request.md` / `_Answer.md` (**`SlotId`가 장착의 유일한 진실**로 확정, 03-A/B/C 재조정)
> 시점: **Step 03 골격만(로직 0줄), Step 04 미착수.** 9차 반영은 문서에 완료됨
> 성격: **Step 04의 성격이 바뀌었다.** 8차까지 *"표시 전용 목록"* 이던 것이 **드래그가 본체인 격자 UI**가 됐다. 그리고 **엔트리에 순서 필드를 넣지 않기로** 판단했는데, 이것이 이번 검수의 최대 주제다

---

## 0. 사용자 입장 (먼저 밝힌다)

**9차 판정은 전부 수용해 문서에 반영했다.** `SlotId`가 진실, `MoveEntry`, `GetEntryInSlot(Parent, SlotId)`, `SlotPriority`, `BodySlots`, 03-A/B/C 재조정까지. **딱 한 곳만 다르게 반영했고 §1-4에 적었다.**

**이번에 기획이 또 한 겹 구체화됐다.** 인벤토리 화면이 **정사각형 격자**가 되고, **드래그로 정리·이동·교환**이 가능해야 한다(§1-2). 그래서 Step 04를 다시 썼다(347줄 → 715줄).

**Claude가 내린 판단 중 셋이 검증을 받아야 한다.**

1. **★ 아이템 순서를 서버에 두지 않는다** (§2). 엔트리에 `SortIndex`를 넣지 않고 클라이언트 로컬 `ULocalPlayerSubsystem`에 둔다. **근거는 "서버 로직 중 순서를 보는 곳이 0곳"** 인데, **Lyra는 반대로 `Slots` 배열을 복제한다.** 이게 뒤집히면 §2 전체 + Step 03의 `InsertEntry`/`MoveEntry`/`RemoveSelf`가 다시 열린다
2. **`SwapEntries(A, B)`를 별도 함수로 만든다** (§3). `MoveEntry` 두 번으로는 **성립하는 교환이 거절된다**는 것이 근거다. 그런데 CLAUDE.md §2는 함수를 늘리는 것에 보수적이다
3. **격자를 "용량 칸 수"로 그리지 않는다** (§4). `SlotSize`가 아이템마다 달라 칸 개수가 가변이기 때문인데, **이건 소스로 검증할 수 없는 UX 판단이다.** 대안을 못 본 게 있는지 봐달라

**그리고 §6을 봐달라 — 범위가 통제되는가.** 9차에서 Step 04에 12칸 슬롯 UI가 들어왔고, 이번에 드래그·교환·로컬 순서가 들어왔다. **Step 03보다 커지면 안 된다**는 것이 8차부터의 원칙인데, 지금 완료 조건이 13개다.

---

## 1. 현재 상태 (사실만)

### 1-1. 진행

| | 상태 |
|---|---|
| Step 00~02 | ✅ 구현·검증 완료 |
| Step 03 | **골격만.** `Public/Inventory/` · `Private/Inventory/` 생성, `EPInventoryTypes.h`는 필드 5개 선언 완료, 컴포넌트는 **엔진 템플릿 그대로**(로직 0줄). `Build.cs`에 `NetCore` 추가 완료 |
| Step 04 | **미착수.** 문서만 이번에 재작성 (347 → 715줄) |
| Step 05 | 미착수 |

**되돌리는 비용이 여전히 0이다.** 이번 검수 대상은 전부 문서이고 코드가 없다.

### 1-2. 확정된 기획 (사용자 발화 그대로)

> *"레이아웃은 오른쪽에 아무것도 없다가 상의를 입으면 상의 인벤토리 구역이 뜨고, 하의를 입으면 하의 인벤토리 구역이 뜨도록 할거야. 외투를 입으면 상의 위에 인벤토리 구역이 끼어드는거야. 그러니까 그룹끼리도 순서가 있어. **외투, 상의, 하의, 배낭, 팔목.**"*

> *"칸은 하나처럼 보이지만, 사실 아이템이 차지하는 슬롯크기가 있어서(이게 `SlotSize`임) **AK47(SlotSize4), 붕대(SlotSize1), 권총(SlotSize2)** 이런식으로 있으면, **칸은 3개처럼 보이지만, 차지하는 슬롯수는 7**이 되는거야."*

> *"컨테이너 A에 있던 아이템을 컨테이너 B로도 옮길수 있고, **컨테이너 A의 아이템과 컨테이너 B의 아이템과 교환도 가능**해야해."*

> *"플레이어가 인벤토리를 직접 정리할수 있게 하는게 좋다 치자."*

**타르코프식 2D 배치는 명시적으로 배제됐다.** 사용자가 "정사각형 슬롯"을 원하되 `SlotSize`는 숫자로 유지하는 쪽을 골랐다.

### 1-3. 확인된 사실 (다시 파지 말 것 — 전부 이번 세션에 직독 확인함)

| 사실 | 출처 |
|---|---|
| **★ Lyra에는 인벤토리 드래그앤드롭 UI가 C++에 존재하지 않는다** — `grep -rln "DragDrop\|NativeOnDrop\|OnDragDetected" LyraGame/` = **0건** | Lyra 전수 |
| **Lyra의 퀵바 `Slots`는 복제된다** — `UPROPERTY(ReplicatedUsing=OnRep_Slots) TArray<TObjectPtr<...>> Slots` + `int32 ActiveSlotIndex`. **즉 Lyra는 "칸 배치"를 서버에 둔다** | `LyraQuickBarComponent.h:66,73-77` |
| `OnRep_Slots`가 **`Slots` 배열 전체**를 메시지에 실어 브로드캐스트한다 | `LyraQuickBarComponent.cpp:205-213` |
| `AddItemToSlot`은 `Slots[i] == nullptr`일 때만 대입한다 — **교체(swap)를 지원하지 않고 조용히 실패** | `LyraQuickBarComponent.cpp:169-179` |
| **Lyra에는 용량 개념이 없다** — `CanAddItemDefinition`이 `//@TODO`와 함께 무조건 `true` | `LyraInventoryManagerComponent.cpp:159-163` |
| UMG 드래그 훅은 셋 — `NativeOnDragDetected` / `NativeOnDragOver` / `NativeOnDrop` | `UMG/Public/Blueprint/UserWidget.h:1612-1616` |
| `UUniformGridPanel::AddChildToUniformGrid(UWidget*, int32 Row, int32 Col)` | `UMG/Public/Components/UniformGridPanel.h:67` |
| `ULocalPlayerSubsystem`이 존재한다 | `Runtime/Engine/Public/Subsystems/LocalPlayerSubsystem.h` |
| `UListView`는 아이템 타입이 `UObject*`로 고정 — 값 타입 엔트리에 못 쓴다 | `UMG/Public/Components/ListView.h:38` |
| **FastArray는 클라 배열 순서를 보장하지 않는다** — 수신 시 `RemoveAtSwap`을 쓴다 | `FastArraySerializer.h:54`, `:1193` |
| `FFastArraySerializerItem`의 복사 생성자·`operator=`가 `ReplicationID`를 리셋 | `FastArraySerializer.h:302-323` |
| 내부 struct 델타가 기본 활성 — **바뀐 프로퍼티만 나간다** | `FastArraySerializer.cpp:35`, `.h:218-221` |
| `FEPItemData`에 `SlotSize`(:39)·`ContainerCapacity`(:42)·`bFungible`(:56). **2D 크기 필드는 없다** | `EPItemData.h:39-56` |
| `GetUsedSlots`가 `if (!E.SlotId.IsNone()) continue;` 로 슬롯에 든 것을 칸 계산에서 뺀다 | `05_Loot_03_Inventory.md` 03-3 |

### 1-4. 9차 판정 중 다르게 반영한 것 (배경 — 재론 대상이 아니다)

9차 답변은 함정 **3b**를 *"남는다 — write-back 소실은 값의 문제"* 로 판정했으나, **장착의 정의가 바뀐 것을 반영하지 않은 판정이라고 보고 다르게 적었다.** 새 설계에서 장착은 `SlotId == "HotbarN"` **＋** `ParentEntryId == INDEX_NONE`이므로 *"배낭 속 무기를 장착한 채"* 라는 전제가 성립하지 않는다. 문장은 `EquipmentSlots.md` §10 미정 #7로 옮겼다 — **핫바 5~0이 컨테이너 안 아이템을 들 수 있게 되면 되살아난다.**

그리고 9차 답변이 짚지 않은 것을 하나 추가했다 — **write-back 순서 계약의 성격이 바뀐다.** `EquippedEntryId`가 저장된 필드일 때는 `RemoveSelf` 뒤에 읽어도 값이 살아 있어 순서 위반이 *"`INDEX_NONE`을 향한 write-back"* 이었지만, 파생 게터가 되면 **write-back이 아예 안 불려 잔탄이 조용히 사라진다.** 함정 4k로 적었다.

---

## 2. ★ 최대 주제 — 아이템 순서를 서버에 두지 않는 것

### 2-1. 제안 (Claude의 판단)

**엔트리에 `SortIndex`를 넣지 않는다.** 표시 순서는 클라이언트 로컬에 둔다.

```cpp
UCLASS()
class UEPInventoryLayout : public ULocalPlayerSubsystem
{
    TMap<int32, TArray<int32>> Order;   // Key = 컨테이너 EntryId (-1 = 본체)

public:
    TArray<int32> Resolve(const UEPInventoryComponent* Inv, int32 Container) const;
    bool          MoveTo (const UEPInventoryComponent* Inv, int32 Container,
                          int32 EntryId, int32 NewIndex);
};
```

**`Resolve`가 매 갱신마다 서버 배열과 대조한다.**

```
저장된 목록에 있는데 인벤토리에 없는 EntryId  →  버린다
인벤토리에 있는데 목록에 없는 EntryId        →  EntryId 오름차순으로 맨 뒤에 붙인다
```

같은 컨테이너 안 순서 바꾸기는 **서버 RPC를 타지 않는다.** 다른 드래그(컨테이너 간 이동·교환·착용)는 전부 서버를 탄다.

### 2-2. 찬성 근거

**(a) 서버 로직 중 순서를 보는 곳을 전수 조사했다 — 0곳이다.**

| 서버가 하는 일 | 순서를 보나 |
|---|---|
| `GetUsedSlots` / `CanFit` | ✗ 합산이다 |
| `TryAutoEquip` | ✗ `SlotPriority`를 본다 |
| `GetInsertionOrder` | ✗ **컨테이너** 순서지 아이템 순서가 아니다 |
| `RemoveEntry` 캐스케이드 | ✗ `ParentEntryId`로만 판정 |
| `FindFungibleEntryId` | ✗ 첫 번째를 찾는다 |

**(b) 인벤토리는 `COND_OwnerOnly`라 남이 볼 수도 없다.** 조작해도 자기 화면만 바뀐다 — 치트 가치가 0이다.

**(c) 서버에 두면 치르는 값이 크다.** 드래그마다 **RPC 왕복**(반응이 RTT만큼 늦거나 예측을 따로 만들어야 한다) + 재배치마다 **형제 N개 복제** + `InsertEntry`/`MoveEntry`/`RemoveSelf` **세 곳에 유지 코드** + **검증 표면 확대.**

**(d) 청소 코드가 문법적으로 사라진다.** `Resolve`가 매번 대조하므로 죽은 `EntryId`가 남아도 무해하다. 이건 9차가 `HotbarRefs`에 대해 *"`RemoveSelf` 한 줄"* 을 요구한 것과 대비되는 이득이고, Step 03의 *"모든 파생값을 매 갱신마다 처음부터 다시 계산한다"* 와 같은 원리다.

### 2-3. ★ 반대 근거 — Lyra는 배치를 복제한다

**`ULyraQuickBarComponent::Slots`는 `ReplicatedUsing=OnRep_Slots`다**(`.h:73-77`). 즉 **Lyra는 "몇 번 칸에 무엇이 있나"를 서버 권위로 둔다.** 우리가 로컬로 두려는 것과 정면으로 다르다.

그리고 실무적으로도:

- **재접속·사망 시 순서가 날아간다.** `USaveGame`을 붙여도 **그 기기에서만** 유지된다
- **자동 정렬 모드**(§6-1의 후속 항목)도 로컬이면 계정 이전이 안 된다
- 대부분의 상용 인벤토리 게임은 배치를 서버/계정에 저장한다

### 2-4. ★ 그런데 Lyra의 `Slots`는 표시가 아니다 (이 대비를 판정해달라)

Claude의 해석은 이렇다. **틀렸으면 지적해달라.**

| | Lyra `Slots` | 우리 표시 순서 |
|---|---|---|
| 무엇을 정하나 | **무엇이 장착되는가** — `GetActiveSlotItem()`이 전투에 쓰인다 | 화면에 그리는 차례뿐 |
| 서버가 읽나 | **읽는다** (`ActiveSlotIndex` → 장착) | **0곳** (§2-2a) |
| 남이 보나 | 장착은 액터로 보인다 | `COND_OwnerOnly` |

**즉 Lyra의 `Slots`는 우리 `SlotId`(장착의 진실, 9차 확정)에 대응하지, 표시 순서에 대응하지 않는다.** 우리 `SlotId`는 이미 복제되고 있으므로 **그 축에서는 Lyra와 다르지 않다** — 다른 것은 *"어느 칸에 그릴까"* 라는 추가 축뿐이고, Lyra에는 그 축이 없다(퀵바 3칸이 곧 배치다).

**이 해석이 맞는가?** 맞다면 Lyra는 반례가 아니다.

### 2-5. 알고 있는 대가

| 대가 | Claude의 판단 |
|---|---|
| 재접속·사망 시 순서 소실 | **받아들인다.** 필요하면 `USaveGame`으로 그 기기까지는 커버 |
| 다른 기기에서 순서 다름 | 받아들인다 |
| `Resolve`가 O(N²) (`Contains` 중첩) | N이 수십. 필요하면 `TSet` |
| 나중에 서버로 옮기려면? | `UEPInventoryLayout`이 서버에서 받은 배열로 `Order`를 채우고, **서버는 불투명한 `TArray<int32>`로 저장만 하지 해석하지 않는다** |

### 2-6. 판정 요청

1. **§2-4의 해석이 맞는가** — Lyra의 `Slots`가 우리 `SlotId`에 대응하고 표시 순서에는 대응하지 않는다는 것
2. **"서버 로직 중 순서를 보는 곳이 0곳"이 정말 0곳인가.** §2-2(a) 표에서 빠뜨린 소비자가 있는가. 특히 **나중에 생길 것** — 예를 들어 *"가방에서 자동으로 뭔가 꺼낸다"* 류
3. **로컬로 갔다가 서버로 옮기는 비용이 정말 싼가.** §2-5의 마지막 줄이 낙관적인가
4. **`ULocalPlayerSubsystem`이 이런 UI 로컬 상태를 두는 관용적 자리인가.** 위젯 멤버 / `ULocalPlayer` 확장 / `USaveGame` 중 다른 것이 맞는가

---

## 3. `SwapEntries(A, B)` — 별도 함수여야 하는가

### 3-1. 제안

```cpp
bool SwapEntries(int32 A, int32 B);          // Step 04. Step 03에는 넣지 않는다

UFUNCTION(Server, Reliable) void Server_MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId);
UFUNCTION(Server, Reliable) void Server_SwapEntries(int32 A, int32 B);
```

**두 RPC 모두 Step 04에서 처음 연다.** 9차가 *"소비자보다 검증 표면을 먼저 열지 않는다"* 로 미뤄둔 것이고, 이 단계가 그 소비자다.

### 3-2. ★ `MoveEntry` 두 번으로는 안 되는 이유

```
외투 7/10,  AK(SlotSize 4) ↔ 붕대(SlotSize 1)

교환 후 상태:  7 - 1 + 4 = 10   ✅ 정확히 들어간다
순차 적용:     7 + 4     = 11   ❌ 거절된다 (붕대가 아직 안 빠졌다)
```

**순차 적용은 성립하는 교환을 거절한다.** 그리고 순서를 뒤집어도 반대쪽에서 같은 일이 난다.

검사는 여섯이고 `MoveEntry`와 거의 같되, **용량만 "상대가 빠진 뒤" 기준**이다.

```cpp
const int32 UsedPA = GetUsedSlots(PA) - SizeA + SizeB;
const int32 UsedPB = GetUsedSlots(PB) - SizeB + SizeA;
if (UsedPA > GetCapacity(PA) || UsedPB > GetCapacity(PB)) return false;
// ★ 전부 통과한 뒤에야 쓴다. 하나라도 먼저 쓰면 한쪽만 옮겨간 상태가 남는다
```

### 3-3. 고려한 대안

| 대안 | 왜 안 골랐나 |
|---|---|
| A. `MoveEntry` 두 번 + 실패 시 UI가 되돌림 | 되돌릴 수 없다. 첫 번째가 성공하고 두 번째가 실패하면 **한쪽만 옮겨간 상태**가 남는다 |
| B. `MoveEntry(..., int32 DisplacedEntryId = INDEX_NONE)` 인자 추가 | 인자가 4개가 되고, `Displaced`가 있을 때만 도는 분기가 함수 안에 생긴다. **호출부 전부가 기본값을 지나간다** |
| C. **`SwapEntries(A, B)` 별도 함수** (제안) | 인자가 둘이고 "무엇을 어디로"가 아니라 "둘을 맞바꾼다"라 의미가 다르다 |

### 3-4. 판정 요청

1. **C가 맞는가, B가 맞는가.** CLAUDE.md §2의 *"두 번째 구현자가 없는 인터페이스를 만들지 않는다"* 가 여기 걸리는가 — 9차는 `MoveEntry`에 대해 *"인터페이스도 베이스 클래스도 아닌 단일 함수라 안 걸린다"* 고 판정했는데, 같은 논리가 여기도 적용되는가
2. **Step 04에 두는 것이 맞는가.** `MoveEntry`는 03-B의 `Server_EquipBackpack`이 래퍼라서 03-A로 갔다. `SwapEntries`에는 Step 03 호출자가 없다 — 이 판단이 9차 기준과 일관되는가
3. **★ 검사 목록에 빠진 것이 있는가.** 특히 **A와 B의 부모가 같은 경우**(순수 표시 순서)를 서버에서 거절하기로 했는데, 한쪽이 슬롯에 있고 한쪽이 컨테이너에 있는 경우(장착 교체)는 부모가 둘 다 `INDEX_NONE`이라 **같은 부모로 잡힌다.** 지금 설계는 `SlotId`까지 봐서 가르는데, 이 판정식이 안전한가
4. **★ 교환이 클라이언트에 원자적으로 보이는가.** 엔트리 둘을 `MarkItemDirty`하면 **같은 수신에 함께 도착하는가**, 아니면 하나만 먼저 도착해 한 프레임 이상한 상태가 보일 수 있는가. `PostReplicatedReceive`가 1회만 불리면 UI는 한 번만 다시 그리지만, **번들이 갈릴 수 있는지를 확인해달라**

---

## 4. 격자 레이아웃 — 칸 개수가 가변인 문제

### 4-1. 문제

`SlotSize`가 아이템마다 다르므로 **용량 20짜리 배낭에 들어갈 수 있는 아이템 개수는 1개(`SlotSize=20`)부터 20개(전부 1)까지다.**

**고정 20칸 격자를 그리면 그림과 게이지가 어긋난다** — 아이템 3개(4+2+1=7)면 격자엔 3칸이 차 있는데 게이지는 `7/20`이고, 플레이어는 *"17칸 남았는데 왜 안 들어가지?"* 를 겪는다.

### 4-2. 제안

**격자는 배치가 아니라 나열이다. 진실은 게이지다.**

```
┌─ 배낭 ────────────────────────────────────────┐
│  ████ ██ █ ░░░░░░░░░░░░░              7 / 20 │  ← 분절 게이지
│  AK  권총 붕대                                │
│  ┌─────┬─────┬─────┬─────┬─────┐             │
│  │ AK  │ 권총 │ 붕대 │     │     │             │
│  │   ④│   ②│   ①│     │     │             │
│  └─────┴─────┴─────┴─────┴─────┘             │
└───────────────────────────────────────────────┘
```

| 장치 | 역할 |
|---|---|
| **분절 게이지** | 아이템별 몫을 한눈에. 칸 hover 시 해당 구간이 밝아진다 |
| **칸 우하단 배지 `④`** | 개별 `SlotSize` |
| **`7 / 20`** | 최종 진실 |

**빈 칸은 개수가 무의미하다.** 고정 5열 × 가변 행, 최소 2행. **드래그 중에만 색으로 말한다** — 남은 용량 ≥ `SlotSize`면 초록, 아니면 빨강 + 게이지에 넘치는 만큼 미리보기.

**착용 슬롯에는 배지를 안 붙인다.** 슬롯에 든 것은 칸을 안 먹으므로(`GetUsedSlots`가 건너뜀) 숫자가 거짓말이 된다.

### 4-3. 판정 요청

**이 절은 소스로 검증되지 않는 UX 판단이다.** 그걸 알고 묻는다.

1. **고려하지 못한 대안이 있는가.** 예: 칸 크기를 `SlotSize`에 비례시키기(→ 정사각형이 깨지고 2D로 가는 길), 아이템이 여러 칸을 실제로 점유(→ 2D), 목록 유지
2. **분절 게이지가 *"칸 3개인데 7"* 을 설명하기에 충분한가.** 더 직접적인 장치가 있는가
3. **빈 칸의 개수를 포기한 것이 옳은가.** *"몇 개 더 들어가나"* 를 알고 싶은 요구를 게이지만으로 감당할 수 있는가

---

## 5. 위젯 구성과 드롭 판정

### 5-1. 제안

```
WBP_Inventory
├─ EquipColumn (UEPEquipPanel)          ← 왼쪽. 고정 12칸 (착용 8 + 핫바 1~4)
└─ ContainerColumn (ScrollBox)          ← 오른쪽. 구획이 런타임 생성
     └─ WBP_ContainerPanel × N
          ├─ CapacityBar (UEPSegmentedBar)
          └─ CellGrid (UniformGridPanel) → WBP_ItemCell × N
```

**구획은 `BindWidget`으로 고정할 수 없다** — 착용에 따라 생기고 사라진다. `ContainerOrder`(전역 설정)를 돌며 `GetEntryInSlot(INDEX_NONE, SlotId)`이 유효하고 `GetCapacity > 0`인 것만 만든다. **분기가 하나도 안 는다.**

**드롭은 격자가 받고 칸은 안 받는다.**

```cpp
bool UEPContainerPanel::NativeOnDrop(const FGeometry& Geo, const FDragDropEvent& Ev, UDragDropOperation* Op)
{
    const int32 Hit = HitTestCell(Geo, Ev);              // 빈 칸이면 INDEX_NONE
    if (P->SourceContainer == ContainerId && Hit != INDEX_NONE)
        return GetLayout()->MoveTo(Inventory, ContainerId, P->EntryId, Hit);  // 로컬
    if (Hit == INDEX_NONE) Inventory->Server_MoveEntry(P->EntryId, ContainerId, NAME_None);
    else                   Inventory->Server_SwapEntries(P->EntryId, Hit);
    return true;
}
```

**칸마다 `NativeOnDrop`을 구현하면** 같은 검증이 N개 위젯에 흩어지고 **빈 칸에 떨어뜨린 경우를 아무도 안 받는다.**

### 5-2. 판정 요청

1. **★ 컨테이너가 드롭을 받고 좌표로 판정하는 것이 UMG 관용구인가.** **Lyra에는 드래그 UI가 아예 없어서**(§1-3) 참고할 선례가 없다. 엔진 샘플·플러그인·`CommonUI`에 사례가 있는가
2. **`HitTestCell`을 지오메트리로 직접 계산하는 것이 맞는가.** `UUniformGridPanel`에서 마우스 위치 → (행,열)을 얻는 관용적 방법이 따로 있는가
3. **매 갱신마다 위젯 전체 재생성이 이 규모에서 견디는가.** 엔트리 20~30개 × 구획 최대 6개. 8차는 *"값 타입 설계에서는 수동 재생성이 정합적"* 이라고 판정했는데, **격자가 되면서 위젯 수가 늘었다**(빈 칸도 위젯이다)
4. **드래그 중 `NativeOnDragOver`에서 매 프레임 `GetUsedSlots`를 부르는 것이 괜찮은가.** O(N) 선형 순회 × 매 프레임이다

---

## 6. ★ 범위 통제 — Step 04가 Step 03보다 커지는가

**8차부터의 원칙은 *"Step 04를 Step 03보다 크게 만들지 않는다"* 였다.** 그런데 두 번 커졌다.

| 언제 | 무엇이 들어왔나 |
|---|---|
| 9차 (2026-08-22) | 장비 슬롯 UI가 2개 → **12개** |
| 10차 (이번) | **드래그**(이동·교환·착용) + 격자 + 로컬 순서 |

**뺀 것:**

| 뺀 것 | 어디로 |
|---|---|
| 자동 정렬 (이름순·종류순·희귀도순) | 나중. 로컬 순서 위에 얹는 순수 추가분 |
| 핫바 5~0 UI | Step 05 (`HotbarRefs` 청소와 함께) |
| 툴팁 / 상세 패널 | 나중 |
| 부착물 UI | §7-3 |
| 위젯 풀링 | 커지면 |

### 6-1. 판정 요청

1. **이 범위가 한 단계로 성립하는가.** 완료 조건이 13개다 — **Step 03과 같은 수**이고, 8차는 그걸 보고 03을 셋으로 쪼갰다. **Step 04도 쪼개야 하는가?** 쪼갠다면 경계는 어디인가 (Claude의 후보: **04-A 표시**(격자·게이지·12슬롯) / **04-B 드래그**(이동·교환·로컬 순서))
2. **드래그를 더 뺄 수 있는가.** 착용/해제만 남기고 컨테이너 간 이동·교환을 Step 05로? — 기획이 *"교환도 가능해야 한다"* 이므로 **뺄 수 없다고 판단했는데**, 순서 문제라면 뺄 수 있다
3. **자동 정렬을 뺀 것이 옳은가.** 수동 정리보다 자동 정렬이 **더 자주 쓰이는 기능**일 수 있고, 그렇다면 드래그보다 먼저일 수도 있다

---

## 7. 파급 — 이번에 뒤집힌 것

### 7-1. `05_Loot_04_InventoryUI.md` — 전면 재작성 (347 → 715줄)

| 절 | 무엇 |
|---|---|
| 제목·목표 | *"아이템 목록 + 칸 수 게이지"* → *"정사각형 격자 + 부피 게이지 + 드래그"* |
| **04-0** 신설 | 레이아웃 판단 (§4) |
| 04-1 | *"표시 전용"* 폐기. 넣는 것/빼는 것 재정의 (§6) |
| 04-2 | 위젯 트리 재작성. `VerticalBox` → `UniformGridPanel`, 구획 런타임 생성 |
| 04-4 | `UEPInventoryRowWidget` → `UEPItemCellWidget` |
| 04-5 | **드래그 아웃 존** 추가 (*"바깥 아무데나"* 는 오조작이 잦다) |
| **04-7** 신설 | 드래그 — 이동과 교환 (§3) |
| **04-8** 신설 | 로컬 순서 (§2) |
| 함정 | **11건 추가** — 8d/8e/8f (격자·게이지·배지), 11/11b/11c (교환), 12/12b (드롭 판정), 13/13b/13c (로컬 순서) |

### 7-2. 다른 문서

| 문서 | 무엇 |
|---|---|
| `05_Loot_03_Inventory.md` | 교체 절에 `SwapEntries` 이름 부여 + *"`MoveEntry` 두 번이 안 되는 이유"* |
| `LOOT_STATUS.md` 확정표 | **5행 추가** — 화면 형태 / 칸 가변 / `ContainerOrder` / 순서 로컬 / `SwapEntries` |
| `05_Loot_DOCS.md` | 단계표 04행 + **UI 범위 통제 문단**(*"표시 전용"* 이 뒤집혔음 명시) |
| `EquipmentSlots.md` | §7-1에 `ContainerOrder`, **미정 #6(원자적 교체) 해소** |
| `05_Loot_03_Inventory_STATUS.md` | 코드 항목에 `ContainerOrder` 추가 |

### 7-3. 새로 생기는 것 (Step 03 코드에 추가)

```cpp
// UEPLootDeveloperSettings — 03-A
UPROPERTY(config, EditAnywhere) TArray<FName> BodySlots;        // 9차
UPROPERTY(config, EditAnywhere) TArray<FName> ContainerOrder;   // ★ 10차. ["Coat","Torso","Legs","Back","Wrist"]
```

**`GetInsertionOrder()`(03-C)와 Step 04 UI가 같은 배열을 본다** — 표시 순서와 획득 시 채우는 순서가 지금은 같기 때문이다. **이게 옳은가?** *"주울 때 팔목은 마지막에 채운다"* 처럼 갈릴 여지가 있는데, `GetInsertionOrder()`가 이미 함수라 쪼개는 비용이 0이라고 판단했다.

### 7-4. 판정 요청

**빠뜨린 파급이 있는가.** 9차에서 요청서의 파급 목록에 **7건이 빠져 있었다** — 특히 `BACKLOG.md` B-5와 `StudyPath.md`(사용자가 읽고 외우는 문서). 이번에도 같은 실수가 있을 수 있다.

---

## 8. ★ 실무 조사 요청

우리 판단만으로 결정하지 않겠다. **가능하면 실제 소스를 근거로.**

1. **UMG 인벤토리 드래그앤드롭의 관용구.** Lyra에 없으므로(§1-3) 엔진 샘플·플러그인·`CommonUI`·템플릿에 사례가 있는가. **컨테이너가 드롭을 받고 좌표로 판정**하는가, **칸마다 받는가**
2. **`ULocalPlayerSubsystem`이 UI 로컬 상태를 두는 자리로 관용적인가.** 엔진/Lyra에서 `ULocalPlayerSubsystem`을 무엇에 쓰는지, 그리고 UI 설정을 어디에 저장하는지
3. **★ FastArray에서 원소 둘을 같은 프레임에 `MarkItemDirty`하면 클라에 함께 도착하는가.** 갈릴 수 있다면 교환이 한 프레임 깨져 보인다 (§3-4-4). `FNetFastTArrayBaseState` / 번들 분할 경로를 봐달라
4. **위젯 매 갱신 전체 재생성이 견디는 규모.** 엔진에 이 패턴의 선례나 경고가 있는가. `UUniformGridPanel`에 자식 수십 개를 매번 다시 붙이는 비용
5. **`SlotSize` 같은 스칼라 부피를 정사각형 격자로 그리는 선례.** 상용 게임에서 *"칸 개수 ≠ 용량"* 을 어떻게 설명하는가 (분절 게이지 말고 다른 장치)

> 로컬 경로: 엔진 `C:\Program Files\Epic Games\UE_5.7\Engine`, Lyra `C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame`, GAS 문서 `C:\Github\GASDocumentation`. **기억으로 API를 단정하지 말 것** — 6~9차에서 인용 정확도가 유용했다.

---

## 9. 재론하지 않았으면 하는 것

| | 근거 |
|---|---|
| `FEPItemState` 값 타입 / 스택 폐지 / `bFungible` 합치기 | 1·2차 확정 |
| DT/DA 두 계층 / 전역 참조를 `UEPLootDeveloperSettings`에 | 3·6차 확정 |
| Step 02가 `UEPGA_Interact`로 가는 것 / 드랍이 `Server_DropItem` 직접 RPC | 7·8차 확정, 일부 구현 완료 |
| `EntryId`(int32, 서버 발급, 재번호 없음) / `ParentEntryId` 평면 표현 | 1·2차 확정 |
| **`SlotId`가 장착의 유일한 진실 / `MoveEntry` / `GetEntryInSlot(Parent, SlotId)` / `SlotPriority` / `BodySlots`** | **9차 확정.** §2·§3은 그 위에 얹는 것이다 |
| 항목 단위 콜백 금지, `PostReplicatedReceive` 하나 | 8차 확정 (03-7) |
| `UListView`를 쓰지 않는 것 | 8차 확정. 아이템 타입이 `UObject*` 고정 |
| **2D 격자(타르코프식) 배치** | **사용자 확정으로 배제.** `FEPItemData`에 2D 크기가 없고 넣으면 Step 03 재작성이다 |
| **기획 자체** (격자 형태, 구획 순서, 교환 가능, 직접 정리) | **사용자 확정.** 구현 방식만 논한다 |

---

## 10. 대상 파일

| 파일 | 관계 |
|---|---|
| **`DOCS/Notes/05/05_Loot_04_InventoryUI.md`** | **이번 제안의 본체.** 전면 재작성 (715줄) |
| `DOCS/Mine/EquipmentSlots.md` | 9차 결정 본체. §7-1에 `ContainerOrder` 추가 |
| `05_Loot_03_Inventory.md` | `MoveEntry` 절 · 교체 절 (§3) |
| `LOOT_STATUS.md` | 확정표 5행 추가 (§7-2) |
| `05_Loot_DOCS.md` §4-6 / 단계표 / UI 범위 통제 | 뒤집힌 문단 (§7-2) |
| `Public/Data/EPLootDeveloperSettings.h` | `BodySlots` · `ContainerOrder` 추가 지점 |
| `Public/Data/EPItemData.h:39-56` | `SlotSize` / `ContainerCapacity` / `SlotPriority` |
| `Public/Inventory/EPInventoryTypes.h` · `EPInventoryComponent.h` | 현재 골격 |
| `UMG/Public/Blueprint/UserWidget.h:1612-1616` | 드래그 훅 3종 |
| `UMG/Public/Components/UniformGridPanel.h:67` | `AddChildToUniformGrid` |
| `Runtime/Engine/Public/Subsystems/LocalPlayerSubsystem.h` | §2-1 |
| `LyraQuickBarComponent.h:66,73-77` · `.cpp:169-179,205-213` | §2-3 반례 |
| `FastArraySerializer.h:54,302-323,1193` · `.cpp:35` | 순서 비보장 · 복사 시맨틱 · 델타 |
