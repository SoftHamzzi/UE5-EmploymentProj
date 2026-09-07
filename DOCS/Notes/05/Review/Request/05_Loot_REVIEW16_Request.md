# 검수 요청 16차 — **Step 04가 단독 검수를 처음 받는다**

> 작성일: 2026-08-26
> 13차: `05_Loot_REVIEW13_Request.md` / `_Answer.md` (**Step 03 자체 검수 21건 ＋ 답변의 새 결함 12건.** 반영 완료)
> 14차: 검수 없음 — **사용자 지적으로 `Server_EquipBackpack` 삭제** (`Mine/EquipmentSlots.md` §15)
> 15차: 검수 없음 — **03의 13·14차를 04·05에 대조 반영** (이 요청의 §1이 그 결과다)
> **검수 대상: `05_Loot_04_InventoryUI.md` (975줄)**
> 시점: **Step 03 골격만.** 헤더 선언 ＋ 생성자 ＋ `RemoveEntry` 3종만 본문이 있고 나머지는 빈 스텁이다. **03-A 미착수, Step 04는 한 줄도 없다**

---

## 0. 사용자 입장 (먼저 밝힌다)

**이 문서는 여태 단독으로 검수받은 적이 없다.**

| 차수 | 04를 다뤘나 |
|---|---|
| 9차 | 슬롯 12개 확대의 **여파**로만 (호출부 유지·12칸 UI 예고) |
| **10차** | **크게 다뤘다** — 04-A/04-B 분할, `SwapEntries` 용량식, 드롭 라우팅, 함정 9건. **다만 요청서의 본론은 03이었다** |
| 11차 | `SortKey`가 04-8을 뒤집으며 **딸려서** |
| 12차 | `ReorderEntry` 무한 재귀 — **03 버그다.** 04는 한 줄 |
| 13차 | **0줄.** 요청서에 04가 안 나온다 |

**즉 04는 "03을 고칠 때마다 뒤따라 고쳐진 문서"다.** 함정이 30개까지 늘었고 위젯 트리·풀링·드롭 라우팅까지 정해져 있는데, **그 자체를 처음부터 끝까지 본 사람이 없다.** 그리고 지금이 마지막 기회다 — 03-A에 착수하면 04는 **곧 구현 대상**이 된다.

**15차에 04를 03에 맞춰 대조했고 여섯 군데를 고쳤다.** 그중 둘은 *"04를 지금 문서대로 짜면 실제로 깨진다"* 였고, 고치면서 **03의 함수 시그니처를 건드리는 결정**을 하나 했다. **그 판정이 맞는지가 §1이다.**

**§2는 성격이 다르다.** 15차 대조는 *"03이 바뀐 것을 04가 아는가"* 만 봤다 — 04 **자체의** 빈 자리는 안 봤다. 다시 읽으며 여섯 개를 찾았는데, **전부 "설계가 없다"거나 "두 절이 서로 다른 말을 한다"** 부류이고 03의 13차 자체 검수가 찾은 것과 같은 모양이다(*"결정 사이를 잇는 자리"*).

---

## 이번에 볼 것 — 넷

| | 무엇 | 왜 |
|---|---|---|
| **★ §1** | 15차가 04에 넣은 **판정 둘** | 본론 A. 하나는 **03의 시그니처를 바꾼다** |
| **★ §2** | 04 자체의 **빈 자리 여섯** | 본론 B. *"이름만 있고 설계가 없다"* 넷 ＋ *"두 절이 충돌"* 둘 |
| **§3** | **04-A/04-B 분할이 여전히 맞나** | 15차가 04-B에 **03 헤더를 고치는 일**을 넣었다 |
| **§4** | 놓친 것이 더 있나 | 04는 **처음 통독되는 문서**다 |

---

# §1. 15차의 판정 둘 — `SwapEntries`가 03의 13차 변경에 걸렸다

## 1-1. `CanPlaceInSlot`을 그대로 부르면 **슬롯 교환이 전부 거절된다**

### 사실관계

13차가 `MoveEntry`의 검사 2·3·4를 하나로 뽑았다(03-2).

```cpp
bool UEPInventoryComponent::CanPlaceInSlot(int32 Parent, FName SlotId, FName ItemId) const
{
    if (SlotId.IsNone()) return true;
    // 검사 2 — SlotPriority.Contains(SlotId)
    // 검사 3 — 몸 슬롯이면 Parent == INDEX_NONE, 부착 슬롯이면 Parent == 그 무기
    return GetEntryInSlot(Parent, SlotId) == INDEX_NONE;      // ★ 검사 4
}
```

**04-7 `SwapEntries`의 검사표 3번은 10차에 `SlotPriority ＋ BodySlots 정합`으로 적혀 있었다.** 13차 이후로 그건 `CanPlaceInSlot`을 두 번 부르는 것이 되는데, **마지막 줄이 교환과 정면으로 부딪힌다.**

```
핫바1 AK ↔ 핫바2 권총
   CanPlaceInSlot(-1, "Hotbar2", Weapon_AK74)
      → GetEntryInSlot(-1, "Hotbar2") = 권총 (상대다)
      → false
```

**교환에서 목적지 슬롯은 언제나 차 있다 — 그게 교환의 정의다.** 슬롯이 걸린 교환(핫바 무기 자리 바꾸기, §7-3 조준경 교체)이 **하나도 성립하지 않는다.** 컴파일되고 수납 교환은 정상이라 **슬롯 UI 버그로 보인다.**

### 15차의 판정

**`MoveEntry`에는 이 문제가 없다** — 검사 4가 *"차 있으면 교체가 아니라 실패"* 이고 그게 의도다(03-2). **교환만 예외이고, 예외의 정체는 "나가는 사람을 세지 마라"** 다. 그리고 **13차가 같은 것을 이미 한 번 풀었다** — 검사 0을 검사 4보다 앞에 둔 이유가 *"엔트리가 이미 그 슬롯에 있으면 검사 4가 **자기 자신**을 찾아 거절한다"* 였다.

**네 번째 인자를 붙였다.**

```cpp
// 03-2 — 기본값이 INDEX_NONE이라 기존 호출자 둘(MoveEntry · AddSubtree)은 한 글자도 안 바뀐다
bool CanPlaceInSlot(int32 Parent, FName SlotId, FName ItemId,
                    int32 IgnoreEntryId = INDEX_NONE) const;

    const int32 Occupant = GetEntryInSlot(Parent, SlotId);
    return Occupant == INDEX_NONE || Occupant == IgnoreEntryId;
```

```cpp
// SwapEntries — 서로를 무시 대상으로 넘긴다
if (!CanPlaceInSlot(B.ParentEntryId, B.SlotId, A.ItemId, B.EntryId)) return false;
if (!CanPlaceInSlot(A.ParentEntryId, A.SlotId, B.ItemId, A.EntryId)) return false;
```

**세 번째 소비자가 UI다** — 드래그 중 슬롯 하이라이트도 같은 인자를 쓴다(04-1 ②).

### ★ 물을 것 셋

**① 기각한 대안이 맞나.** *"`SwapEntries`가 `CanPlaceInSlot`을 안 부르고 검사 2·3만 직접 쓴다"* 를 기각했다. 근거는 **13차가 `CanPlaceInSlot`을 뽑은 이유가 세 번째 호출자에서 곧바로 깨진다**는 것이다(*"호출자가 보증하는 계약은 호출자가 늘어나면 깨진다"*). §7-3 부착물 교체가 정확히 그 경로로 온다.

**② 기본 인자가 맞나.** 오버로드나 별도 함수(`CanSwapIntoSlot`)가 아니라 **기본값 있는 네 번째 인자**를 골랐다. 근거는 *"기존 호출자 둘이 한 글자도 안 바뀐다"* 인데, **기본 인자는 "잊어도 컴파일되는 인자"** 이기도 하다 — `AddSubtree`가 그 성질 때문에 13차에 지적받았다(*"기본값을 주지 않는다"*, 03-4). **같은 이유로 여기서도 기본값이 위험한가?** 지금은 *"잊으면 더 엄격해질 뿐"*(교환이 거절될 뿐 새는 것이 없다)이라 방향이 안전하다고 봤다.

**③ 03에 미리 넣지 않은 것이 맞나.** 03-2에는 **예고 한 줄만** 적고 실제로 붙이는 것은 04-B로 뒀다. 근거는 이 문서가 세 번 적용한 규칙(*"검증 표면은 소비자를 따라간다"* — `Server_MoveEntry` 9차, `Server_ReorderEntry` 11차, `Server_EquipBackpack` 14차)이다. **그런데 저 셋은 전부 RPC이고 이건 내부 함수의 시그니처다.** 규칙이 그대로 적용되나? → **§3에서 다시 묻는다.**

---

## 1-2. `SwapEntries`가 `SortKey`도 맞바꾼다

### 사실관계

10차의 04-7은 *"두 엔트리의 **(ParentEntryId, SlotId)** 를 맞바꾼다"* 였다. **11차가 `SortKey`를 도입하면서 `MoveEntry`에 재발급 규칙이 붙었는데**(*"부모가 바뀌었으면 `KeySpace_NextAtEnd`로 재발급"*, 03-2) **04-7은 갱신되지 않았다.**

```
본체:  … 붕대 1,000,000        (오래 쓴 컨테이너)
배낭:  칫솔 0   가위 65536      (새 컨테이너)

붕대 ↔ 칫솔 교환. SortKey를 안 건드리면
   → 붕대가 배낭 안에서 1,000,000      ← 03의 함정 4x가 교환 경로로 그대로 재현
```

### 15차의 판정

**교환에서는 `KeySpace_NextAtEnd`를 부를 필요가 없다 — 맞바꾸면 끝이다.** 각자가 상대의 자리를 물려받는데 **그 키는 목적지 스코프에서 이미 유효하고 중복도 없다**(방금 상대가 비웠다). `MoveEntry`가 재발급인 것은 **혼자 들어가서 물려받을 자리가 없기 때문**이다.

```cpp
Swap(A.ParentEntryId, B.ParentEntryId);
Swap(A.SlotId,        B.SlotId);
Swap(A.SortKey,       B.SortKey);        // ★ 15차
Entries.MarkItemDirty(A);
Entries.MarkItemDirty(B);
```

**같은 컨테이너 안 교환은 검사 5가 이미 거절한다**(*"같은 부모 ＋ 둘 다 `SlotId == None`"*). 그래서 **두 키가 같은 스코프에서 맞바뀌는 경우는 슬롯끼리뿐이고, 그쪽은 표시 목록에 안 나와 무해하다.**

> **03이 이 계약을 이미 전제하고 있었다** — 14차가 §7-4(ⓐ 재장전)에 *"**부모·`SlotId`·`SortKey`를 함께 교환한다는 계약**이 거기서 그대로 값을 한다: 빈 탄창이 새 탄창의 화면 자리를 물려받아 **제자리에서 탄창이 바뀐** 것으로 보인다"* 고 적었다. **04만 몰랐다.**

### ★ 물을 것 둘

**① 무해 논증이 맞나.** *"같은 스코프에서 맞바뀌는 경우는 슬롯끼리뿐이라 무해하다"* — **검사 5를 완화하면 이 논증이 깨진다.** 10차가 용량식에 대해 정확히 같은 우려를 적어뒀다(*"검사 5를 완화하는 순간 개별 검사는 틀린 답을 낸다"*). **`Swap(SortKey)`는 그때도 옳은가?** 같은 컨테이너 안 두 아이템의 키를 맞바꾸면 그건 **정확히 자리 바꾸기**이므로 옳아 보이는데, 확인받고 싶다.

**② `PA == PB`이고 한쪽만 슬롯인 경우.** 본체의 핫바1 AK ↔ 본체 수납 구급상자. 부모가 같아 재부모가 아니지만 **AK는 표시 목록에 없다가 들어오고, 구급상자는 나간다.** AK가 물려받는 키는 **구급상자의 키**라 본체 표시 목록에서 정확히 그 자리다 — 맞다고 봤는데, 반대로 구급상자가 물려받는 AK의 키는 **슬롯에 있던 동안 아무도 안 쓰던 값**이다. 그게 본체의 다른 키와 **충돌할 수 있나?** (`AssignSortKey`가 슬롯 엔트리의 키를 어떻게 두는지가 여기 걸린다 — 03-2 *"장착한 아이템은 자리를 지킨다"*.)

---

# §2. 04 자체의 빈 자리 여섯

**여기부터는 15차 대조가 안 본 것이다.** 04를 처음부터 끝까지 읽으며 찾았다.

## 2-1. ★★ `NativeOnDrop`의 **반환값 계약이 없다** — 거절된 드롭이 "맨 뒤로 보내기"로 둔갑한다

04-1이 세운 라우팅은 이렇다.

```cpp
// 칸 — 한 줄. 검증 없음
bool UEPItemCellWidget::NativeOnDrop(...)
{
    return OwnerPanel->HandleDrop(FEPCellHit{ DisplayIndex, EntryId }, Op);
}

// 패널 — 여백에 놓은 경우. 버블링으로 여기 온다
bool UEPContainerPanel::NativeOnDrop(...)
{
    return HandleDrop(FEPCellHit{ INDEX_NONE, INDEX_NONE }, Op);   // 맨 뒤
}
```

**`HandleDrop`이 `false`를 돌려주면 칸의 `NativeOnDrop`도 `false`를 돌려주고, 그건 `Unhandled`라 버블링이 계속된다.** 그 다음에 걸리는 것이 **바로 그 패널의 `NativeOnDrop`이고, 그건 `Hit = {INDEX_NONE, INDEX_NONE}` = 맨 뒤다.**

```
용량 초과로 거절된 드롭  →  칸 false  →  버블링  →  패널이 "맨 뒤로 보내기"로 처리
```

**04-7의 `HandleDrop` 스케치는 모든 분기에서 `return true`라 지금은 안 드러난다.** 그런데 04-1이 *"검증은 패널의 `HandleDrop` 한 곳"* 이라고 못 박았으므로 **거기서 `false`가 나오는 것이 설계된 동작**이다.

**필요한 것은 반환값의 의미를 가르는 것으로 보인다.**

```
"내가 이 드롭의 주인이다"  (라우팅)   →  반환값
"이 드롭이 성립한다"       (판정)     →  별개
```

즉 **칸은 언제나 `true`(Handled)를 돌려주고**, 성립 여부는 `HandleDrop` 안에서 소비하거나 out-param으로 빼야 한다. **이 판정이 맞나? 그리고 이게 진짜 결함인가, 아니면 내가 Slate 라우팅을 잘못 읽었나?**

> 04-1이 인용한 근거는 `SlateApplication.cpp:5523` ＋ `FBubblePolicy`(`:382-406`)이고 *"처음 `Handled()`를 반환한 곳에서 멈춘다"* 다. **그 문장대로면 `false`는 안 멈춘다.**

## 2-2. ★★ `FEPItemDragPayload`가 **정의된 적이 없다**

04-7과 04-7 각주가 두 번 쓴다.

```cpp
const FEPItemDragPayload* P = /* Op에서 꺼낸다 */;    // 04-7 :796
P->SourceContainer / P->EntryId

Payload->CachedUsed.Add(ContainerId, Inventory->GetUsedSlots(ContainerId));   // 04-7 :848
```

**04 어디에도 선언이 없다.** `UDragDropOperation` 파생인지, 그것의 `Payload`(`UObject*`)에 담기는 별개 타입인지, 필드가 무엇인지가 안 정해져 있다. **그리고 `NativeOnDragDetected`가 그것을 만드는 코드도 없다** — 04-4에 오버라이드 **선언만** 있다(`:513-514`).

**드래그가 04-B의 본체인데 시작점이 비어 있다.** `FEPCellHit`은 10차에 *"두 값이 독립이라 하나의 `int32`로 못 합친다"* 며 구조를 정해줬는데, **페이로드는 그 대우를 못 받았다.**

지금까지 나온 요구를 모으면 최소 이렇다.

| 필드 | 쓰는 곳 |
|---|---|
| `EntryId` | 04-7 `HandleDrop` 전부 |
| `SourceContainer` | 04-7 *"같은 컨테이너면 `Server_ReorderEntry`"* |
| `ItemId` 또는 `SlotSize` | 04-7 드래그 피드백 (`남은 용량 ≥ SlotSize`) |
| `TMap<int32,int32> CachedUsed` | 04-7 각주 (매 프레임 `GetUsedSlots` 금지) |
| **`SourceSlotId`** | ★ 15차 `IgnoreEntryId`·해제 판정에 필요해 보인다 |

**물을 것: `UDragDropOperation` 파생이 맞나, 아니면 `UDragDropOperation::Payload`에 `UObject` 하나를 다는 엔진 관례가 맞나?** Lyra/엔진 직독 근거가 있으면 좋겠다.

## 2-3. ★ 04-8의 두 문장이 서로 충돌한다

```
04-8 "이 단계가 하는 일 — 셋뿐이다"
   ① 그린다 — 정렬은 컴포넌트가 한다. **UI에 순서 자료구조가 없다**

04-8 13차 각주 ②
   → **더 싼 답: 키를 추정하지 말고 표시 순서만 로컬로 뒤집는다**
```

**"표시 순서를 로컬로 뒤집는다"는 곧 UI가 순서를 든다는 뜻이다.** `GetSortedContents()`는 컴포넌트 함수라 서버 값만 돌려주므로, 낙관적 적용을 하려면 **그 결과를 UI가 복사해 재배열한 배열을 어딘가 들고 있어야 하고 복제가 오면 버려야 한다.**

11차가 클라 로컬 순서를 뒤집은 이유가 **지속**이었지 *"UI가 순서를 잠깐도 들면 안 된다"* 가 아니었으므로 **모순은 아닐 수 있다.** 다만 **①의 문장이 그대로면 구현자가 ②를 못 짠다.**

> 04-8이 *"1차는 넣지 않는다"* 로 낙관적 적용 자체를 미뤄뒀으므로, **가장 싼 답은 ②를 "넣을 때"로 명시적으로 미루고 ①을 손대지 않는 것**으로 보인다. 판정을 받고 싶다.

## 2-4. ★ `UEPSegmentedBar` · `UEPRemainderWidget`이 **이름만 있다**

둘 다 **04-A 범위**(04-2)이고 **완료 조건 5(*"칸 3개인데 게이지가 7/20"*)의 유일한 표현 장치**인데, 설계가 0줄이다. 나온 것은 호출 두 줄뿐이다.

```cpp
Remainder  ->SetRemaining(Max - Used);
CapacityBar->SetSegments(Ordered, Inv, Max);      // ★ 위젯이 인벤토리 컴포넌트를 통째로 받는다
```

**`SetSegments`가 `UEPInventoryComponent*`를 받는 것이 특히 걸린다.** 게이지가 컴포넌트를 뒤져 `FindEntry` → `FindData` → `SlotSize`를 스스로 모은다는 뜻인데, **04-2의 `Rebuild`가 이미 그 순회를 돌고 있다.** 같은 순회가 두 번 돈다.

**대안으로 보이는 것:** `Rebuild`의 순회에서 `TArray<FEPSegment>{ EntryId, SlotSize, Rarity }`를 만들어 넘긴다. 위젯이 인벤토리를 모르게 되고 순회도 한 번이다. **04-0이 요구한 hover 연동**(칸 ↔ 게이지 구간 상호 강조)도 `EntryId`가 구간에 있어야 성립한다.

**04-A가 이 둘 없이는 완료 조건 5를 못 채우는데, 04-A를 시작할 만큼 정해져 있나?**

## 2-5. `UDynamicEntryBox`가 **가상화를 하지 않는다** — 10차가 풀링만 보고 골랐다

10차의 근거는 `FUserWidgetPool` 직독이었고 **풀링에 대해서는 정확하다.** 그런데 같은 헤더의 첫 문단이 이렇다.

```
// UMG/Public/Components/DynamicEntryBox.h:12
Note that entries here are *not* virtualized as they are in the list views,
so generally this should be avoided if you intend to scroll through lots of items.
```

**04-2의 `ContainerColumn`은 `UScrollBox`다** — 구획 6개가 세로로 쌓이고 각 구획이 최대 용량만큼 칸을 갖는다. 배낭 20칸 + 상의 10 + 하의 5 + … 이면 **화면 밖 위젯이 실재한다.**

**풀링과 가상화는 다른 것이다.** 풀링은 *"만들고 부수는 비용"* 을 없애고, 가상화는 *"보이지 않는 것은 아예 안 만든다"* 다. 10차는 앞만 보고 골랐다.

**그래도 결론은 유지될 수 있다고 본다** — 8차가 `UListView`를 뺀 이유(**아이템 타입 `UObject*` 고정**, `ListView.h:38`)는 그대로이고, 실제 규모(**최대 백 단위**)는 엔진 문장이 경계하는 *"lots of items"* 와 거리가 있다. **다만 근거에 이 문장이 빠져 있는 것은 사실이고, 04-2가 그 대비를 표로 적어놓았으므로 거기 한 줄이 필요해 보인다.**

## 2-6. 04-3 코드에 **선언 없는 호출 셋**

```cpp
MakePanel(INDEX_NONE, NSLOCTEXT("EP", "Body", "본체"));   // 선언 없음
MakePanel(Id, SlotDisplayName(SlotId));                   // SlotDisplayName 선언 없음
```

`UEPInventoryWidget`의 `protected` 목록(04-2)에 **`RefreshEntries`만** 있다. 사소하지만 **`SlotDisplayName`은 사소하지 않다** — `"Torso"` → `"상의"` 매핑이 **어디 사는가**가 미정이고, 후보가 셋이다.

| 후보 | 문제 |
|---|---|
| 위젯에 `TMap<FName, FText>` | 슬롯이 늘 때마다 위젯을 연다 |
| `UEPLootDeveloperSettings` | **14차가 금지했다** — *"에셋 참조와 디버그만"* |
| **`UEPPawnInventoryData`** (§8 미정 #10) | **아직 없다** |

**미정 #10이 셋(`BodySlots`·`ContainerOrder`·`StartingEquipment`)을 관장하는데, 여기 넷째가 나온 것으로 보인다.** 그리고 이건 **표시 문자열**이라 성격이 또 다르다(로컬라이즈 대상). **미정 #10에 합류시키는 게 맞나, 아니면 04가 자체적으로 드는 게 맞나?**

---

# §3. 04-A/04-B 분할이 여전히 맞나

**15차가 `IgnoreEntryId` 추가를 04-B에 넣었다.** 그런데 그건 **`UEPInventoryComponent`의 헤더를 고치는 일**이고, 그 시점에 03-A는 이미 끝나 있다.

```
03-A 완료 (CanPlaceInSlot 3인자로 검증 끝)
   → 03-B 완료
      → 04-A 완료
         → 04-B에서 CanPlaceInSlot에 인자를 하나 붙인다   ← 여기
```

**세 번 적용한 규칙은 RPC에 대한 것이었다** — `Server_MoveEntry`(9차)·`Server_ReorderEntry`(11차)·`Server_EquipBackpack`(14차). 근거는 **공격 표면**이었다(*"소비자보다 검증 표면을 먼저 여는 것"*). **내부 함수의 기본 인자에는 그 근거가 성립하지 않는다** — 조작된 클라이언트가 부를 수 있는 것이 아니다.

**그래서 두 가지가 가능해 보인다.**

| | 03-A에서 4인자로 만든다 | 04-B에서 붙인다 (15차 선택) |
|---|---|---|
| 근거 | 공격 표면이 아니므로 규칙 대상이 아니다. **나중에 헤더를 다시 여는 일이 없다** | 소비자가 없는 인자를 미리 만들지 않는다 (CLAUDE.md §2 — *"두 번째 구현자가 없는 인터페이스"*) |
| 위험 | 03-A 검증 중 **그 인자를 지나는 경로가 0개**다 — 13차가 `Server_EquipBackpack`을 죽인 이유 그대로 | 04-B 착수 시 **03 헤더 + `.cpp`를 다시 연다.** 세 단계 전 파일이다 |

**CLAUDE.md §2의 판단 기준(*"이 확장점이 `DOCS/` 어딘가에 이름으로 적혀 있는가"*)으로는 03-A에서 만들어도 통과한다** — 15차가 03-2에 이름을 적어뒀기 때문이다. **어느 쪽이 맞나?**

> **부수 질문:** 같은 물음이 `SwapEntries` 자체에도 걸린다. 04-7의 함수인데 **몸통은 `UEPInventoryComponent`에 산다**(단일 쓰기 지점 다섯 중 둘을 부른다 — 03-7 가드 표가 *"`SwapEntries`(04-B)도 이걸 두 번 부른다"* 고 이미 적었다). **04-B는 03 파일을 여는 단계인가?** 그렇다면 §3의 답은 자동으로 정해진다.

---

# §4. 놓친 것이 더 있나

**04는 이번이 첫 통독이다.** 특히 아래는 **내가 판정할 근거가 부족해 그대로 둔 것들**이다.

| | 무엇 | 왜 못 봤나 |
|---|---|---|
| ① | **`UEPEquipPanel`(왼쪽 12칸)의 `Rebuild`가 04-3 코드에 `EquipColumn->Rebuild(Inventory.Get())` 한 줄뿐** | 12칸을 어떻게 그리는지, `BodySlots` 순서를 어디서 받는지가 없다. §2-6과 같은 자리 |
| ② | **드롭 대상이 착용 슬롯일 때의 라우팅** | 04-1이 세운 것은 **컨테이너 패널** 기준이다. `UEPEquipSlotWidget`은 `HandleDrop`을 부를 `OwnerPanel`이 다르다(`UEPEquipPanel`). 판정 함수가 둘이 되나? |
| ③ | **`CachedUsed` 무효화 후의 동작** | *"`OnInventoryChanged`가 오면 무효화"* 까지만 있고 **그 다음 `NativeOnDragOver`가 무엇을 쓰는지**가 없다. 다시 캐면 되는데 안 적혀 있다 |
| ④ | **`FEPCellHit`이 `USTRUCT`인데 `UPROPERTY`가 0개** | 복제도 BP 노출도 안 하면 `USTRUCT`일 이유가 없어 보인다. 리플렉션이 필요한 곳이 있나? |
| ⑤ | **완료 조건 15개 ↔ 04-A/04-B 표의 배분** | 04-A가 *"1~6, 13, 14"* 인데 **13(순서 지속)의 조작 수단은 04-B의 드래그다.** `EP.Inv.Reorder`(03-A)로 검증한다면 **04-A가 검증하는 것은 "창을 닫았다 열어도 그대로"** 라는 UI 쪽뿐이다 — 그게 의도인가? |
| ⑥ | **05와의 경계** | 15차에 05도 대조해 하드 결함 둘을 고쳤다(`Server_Equip` 폐기 · `DefaultLoadout`이 0칸 본체에 넣던 것). **04가 05로 미뤄둔 것 둘**(핫바 5~0 UI · 장착 강조)이 05에 제대로 도착했는지는 봤는데, **반대 방향**(05가 04에 요구하는 것)은 못 봤다 |

---

## 부록 — 읽을 순서

```
1. 05_Loot_04_InventoryUI.md          ★ 검수 대상 (975줄)
2. 05_Loot_03_Inventory.md  03-2      §1이 걸린 곳 (CanPlaceInSlot · MoveEntry 검사 0~6 · SortKey)
3. Mine/EquipmentSlots.md   §3·§4     배정 ↔ 활성, 핫바 두 종류
4. 05_Loot_DOCS.md          §8 미정 #10   §2-6이 걸린 곳
```

## 부록 — 지금 코드 상태 (04 검수에 필요한 만큼만)

```
Public/Inventory/EPInventoryComponent.h    선언 완료 (주석 포함). CanPlaceInSlot은 3인자
Public/Inventory/EPInventoryTypes.h        FEPInventoryEntry
Private/Inventory/EPInventoryComponent.cpp 생성자 3줄 + RemoveEntry/RemoveEntryInternal/
                                           RemoveChildrenRecursive 만 본문. 나머지 빈 스텁
Step 04                                    파일 없음
```

**남은 골격 결함 (03-A 착수 전 사용자가 고친다):** `FScopedInventoryNotify` 정의 부재 · `TArray<class FEPInventoryEntry>`의 `class` 키워드(C4099) · `KeySpace_NextAbove`/`KeyOf` 반환형 `int32` → `bool`.
