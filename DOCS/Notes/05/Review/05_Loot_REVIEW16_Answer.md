# 검수 답변 16차 — Step 04 첫 단독 검수

> 작성일: 2026-08-26
> 요청: `05_Loot_REVIEW16_Request.md`
> 대상: `05_Loot_04_InventoryUI.md` (1069줄, 15차 반영본)
> 대조: `05_Loot_03_Inventory.md`(2600줄+) · `05_Loot_05_Equipment.md` · `05_Loot_DOCS.md` §8 · `Mine/EquipmentSlots.md` · **소스 3종 직독** · **엔진 5.7 직독**(Slate 라우팅 · UMG 드래그 · UserWidgetPool · DynamicEntryBox) · Lyra
> 13차: `_Answer.md` — 21건 판정 ＋ 새 결함 12건. **반영 확인함.** 완료 조건 18의 32,764회는 **사용자 계산이 맞다**(내 32,763이 틀렸다 — `65536 × 32764 = 2,147,221,504`가 경계와 정확히 같다)

---

## 0. 판정 요약

| | 무엇 | 판정 | 한 줄 |
|---|---|---|---|
| **§1-1** | `IgnoreEntryId` 네 번째 인자 | ✅ **결함·처방 다 맞다** | 기각한 대안도 맞다. 기본값도 맞다 — **`AddSubtree`와 방향이 반대**인 것이 근거다 |
| **§1-2** | `Swap(SortKey)` | ✅ **맞다.** ❗ **무해 논증이 약하다** | 검사 5에 기대지 않는 **더 강한 논증**이 있다 — 치환은 유일성을 보존한다. 그래서 ①·② 둘 다 자동으로 답이 나온다 |
| **★ §1-3** | **15차 코드가 03의 계약 셋을 깬다** | ❗ **새 결함** | `AssignSortKey` 우회 · `MoveEntry`의 *"유일한 지점"* 위반 · **스코프 가드 없음.** 03-7이 *"`SwapEntries`는 자동으로 옳다"* 고 적어둔 것이 **거짓이 된다** |
| **§2-1** | `NativeOnDrop` 반환값 계약 | ✅ **실제 결함이다. Slate를 정확히 읽었다** | 엔진 소스로 확정 — `Route`의 루프 조건이 `!Reply.IsEventHandled()`다. 그리고 **증상이 더 나쁘다: 거절된 교환이 이동으로 둔갑한다** |
| **§2-2** | `FEPItemDragPayload` | ✅ **빈 자리가 맞다.** **파생이 맞다** | `Payload`(`UObject*`)를 쓰면 **8차가 지운 `UEPItemInstance`를 UI 전용으로 되살린다** — `UListView`를 뺀 이유와 같은 것이다 |
| **§2-3** | 04-8의 두 문장 충돌 | ✅ **모순은 아니다.** 판정: **①을 안 건드리고 ②를 "넣을 때"로 미룬다** | 단 ②는 지금 문장으로는 못 짠다 — 한 줄이 필요하다 |
| **§2-4** | 게이지·남은 용량 위젯 | ❗ **04-A를 시작할 만큼 안 정해져 있다** | `SetSegments(…, Inv, …)`는 04-1 ②와도 충돌한다. **드래그 미리보기 자리를 04-A가 미리 만들어야 한다** — 8차 03-7 사고가 04-A/04-B 사이에 또 있다 |
| **§2-5** | `UDynamicEntryBox` 가상화 | ✅ **결론 유지. 근거는 오히려 더 강하다** | **위젯 수가 용량이 아니라 아이템 수에 비례한다**(04-0이 그렇게 만들었다). 요청서가 자기 설계를 과소평가했다 |
| **§2-6** | `SlotDisplayName`의 자리 | ✅ **미정 #10에 합류.** 단 **"필드"가 아니라 "슬롯 정의"로 넓힌다** | **두 번째 고아를 찾았다 — `EmptyHint`(빈 슬롯 부위 아이콘)도 출처가 없다** |
| **§3** | `IgnoreEntryId`를 어디서 붙이나 | ✅ **15차 선택(04-B)이 맞다.** ❌ **근거는 틀렸다** | **부수 질문이 답이다 — 04-B는 이미 `UEPInventoryComponent`를 여는 단계다.** RPC 규칙은 여기 적용되지 않는다 |
| **§4** | 놓친 것 | **7건** | 최우선은 **아이콘 비동기 로드 × 풀링** — 10차가 풀링으로 바꾸면서 04-4의 방어가 무력해졌다 |

> **한 줄 결론:** 15차 판정 둘은 **결함 진단도 처방도 맞다.** 그런데 **그 처방을 코드로 적은 다섯 줄이 03의 계약 셋을 깬다**(§1-3). 그리고 §2-1은 **Slate 소스로 확정된 실제 결함**이고, 요청서가 적은 것보다 나쁘다. §4에서 **04-A 착수 전에 고쳐야 하는 것 하나**를 더 찾았다.

---

# §1. 15차의 판정 둘

## 1-1. `CanPlaceInSlot`의 네 번째 인자 — 결함도 처방도 맞다

### 결함 확인

**대조했고 사실이다.** `CanPlaceInSlot`의 마지막 줄이 `GetEntryInSlot(Parent, SlotId) == INDEX_NONE`이고(`05_Loot_03_Inventory.md:956`), 교환의 목적지 슬롯은 정의상 상대가 차지하고 있다. **슬롯이 걸린 교환이 전부 거절된다.**

**그리고 진단의 마지막 문장이 특히 정확하다** — *"예외의 정체는 '나가는 사람을 세지 마라'"*. 13차의 검사 0이 정확히 같은 것을 막았고(`엔트리가 이미 그 슬롯에 있으면 검사 4가 자기 자신을 찾아 거절한다`, 03-2), **13차가 그걸 "검사 순서"로 풀 수 있었던 이유는 나가는 사람과 들어오는 사람이 같았기 때문**이다. 교환에서는 둘이 다르므로 순서로 못 푼다 — **인자가 아니면 표현할 방법이 없다.** 처방이 맞다.

### ★ 물음 ① — 기각한 대안: **기각이 맞다**

> *"`SwapEntries`가 `CanPlaceInSlot`을 안 부르고 검사 2·3만 직접 쓴다"*

**기각이 맞고, 근거도 맞다.** 여기에 두 가지를 덧붙인다.

**ⓐ 이 프로젝트가 그 실수를 이미 두 번 했다.** 13차의 10d(`AddSubtree`가 검사 2·3·4를 안 돈다)와 3e(`MoveEntry`의 설명 블록을 그대로 인라인으로 치면 검사 2·4가 빠진다) — **둘 다 "검사 셋 중 일부만 쓴다"의 사례**이고, 함정표에 ★★로 남아 있다(`05_Loot_03_Inventory.md:2533`·`:2538`). 세 번째 호출자에서 같은 모양을 다시 만드는 안이다.

**ⓑ 검사 3이 §7-3에서 갈라진다.** 부착 슬롯 갈래는 `GetWeaponDefOf(Parent)->AttachmentSlots`를 본다(03-2). 그 코드를 `SwapEntries`가 복사하면 **§7-3이 활성화될 때 고칠 곳이 둘**이 되고, 그건 이 문서가 `CanFit`에 대해 금지한 것 그대로다(*"판정식이 세 곳에 흩어지면 반드시 어긋난다"*, 03-3).

### ★ 물음 ② — 기본 인자: **맞다. 그리고 `AddSubtree`와 방향이 반대인 것이 근거다**

> *"기본 인자는 '잊어도 컴파일되는 인자'이기도 하다 — `AddSubtree`가 그 성질 때문에 13차에 지적받았다. 같은 이유로 여기서도 위험한가?"*

**아니다. 그리고 그 비교가 정확히 판정선을 준다.**

| | `AddSubtree`의 `SlotId` (13차: 기본값 **금지**) | `CanPlaceInSlot`의 `IgnoreEntryId` (기본값 **허용**) |
|---|---|---|
| 잊으면 | **조용히 수납이 된다** — 검증을 지나지 않은 상태가 만들어진다 | **교환이 거절된다** — 아무 상태도 안 만들어진다 |
| 방향 | **느슨해진다** (검사를 건너뛴다) | **엄격해진다** (검사가 하나 더 걸린다) |
| 증상 | 무증상. §7-3에서 처음 보인다 | **즉시 보인다** — 첫 교환에서 안 된다 |
| 잘못된 상태가 남나 | **남는다** | 남지 않는다 |

**13차가 기본값을 금지한 이유는 "잊을 수 있다"가 아니라 "잊으면 검증 없는 상태가 만들어진다"** 였다. 여기는 반대 방향이고, **잘못 쓴 결과가 데이터가 아니라 거절**이다. 사용자 판정(*"잊으면 더 엄격해질 뿐이라 방향이 안전하다"*)이 정확하다.

**오버로드·별도 함수(`CanSwapIntoSlot`)를 안 고른 것도 맞다.** 둘 다 *"슬롯에 놓을 수 있나"* 라는 **같은 질문**이고 답하는 규칙이 같다. 이름을 가르면 `KeySpace_`/`GetSortedContents`처럼 **다른 질문**일 때만 값을 하는데(12차), 여기는 그 조건이 아니다.

> **★ 한 줄 덧붙일 것 — 인자 이름이 계약을 진다.** `IgnoreEntryId`는 *"무시하라"* 이지 *"거기 있어도 된다"* 가 아니다. **`Occupant == IgnoreEntryId`가 참인데 그 엔트리가 실제로는 안 나가는 호출**이 생기면 두 아이템이 한 슬롯에 앉는다. 지금 호출부 셋(교환 ×2, UI 하이라이트) 전부 *"이 엔트리는 이 자리를 떠나는 중"* 이 참이다. **04-7에 그 전제를 한 줄로 적어두면** 네 번째 호출자가 그걸 읽는다 — 13차가 `AddSubtree` 전제를 *"출처가 아니라 모양"* 으로 다시 쓴 것과 같은 처리다.

### ★ 물음 ③ — 03에 미리 안 넣은 것 → **§3에서 답한다. 선택은 맞고 근거가 틀렸다**

---

## 1-2. `Swap(SortKey)` — 맞다. 그런데 **무해 논증을 바꿔야 한다**

### 판정 확인

**결함이 맞다.** 11차가 `MoveEntry`에 재발급을 붙였고 13차가 그것을 *"재부모 **전에** 구한다"* 로 다시 고쳤는데(함정 4x), **교환은 그 두 번의 수정에 한 번도 안 딸려왔다.** 04-7이 10차의 *"필드 둘"* 그대로였다.

**그리고 처방 근거가 정확하다** — *"`MoveEntry`가 재발급인 것은 혼자 들어가서 물려받을 자리가 없기 때문"*. 이 한 줄이 *"왜 여기만 `KeySpace_NextAtEnd`를 안 부르나"* 에 답한다.

### ❗ 그런데 무해 논증이 필요 이상으로 약하다

15차가 쓴 논증은 이것이다.

> *"같은 컨테이너 안 교환은 검사 5가 거절한다. 그래서 두 키가 같은 스코프에서 맞바뀌는 경우는 슬롯끼리뿐이고, 그쪽은 표시 목록에 안 나와 무해하다."*

**세 겹으로 조건에 의존한다** — 검사 5가 있어야 하고, 슬롯끼리라야 하고, 표시 목록에 안 나와야 한다. 그래서 요청서가 스스로 *"검사 5를 완화하면 깨지나"* 를 묻게 됐다.

**조건이 하나도 필요 없는 논증이 있다.**

```
11차 확정:  키 공간은 "부모가 같은 것 전부"다 — 슬롯에 든 것도 그 공간에 있다 (함정 4q)
            ⇒ 한 부모 안에서 SortKey는 유일하다 (AssignSortKey가 유일한 발급 지점)

같은 부모(PA == PB):   두 원소의 값을 맞바꾸는 것은 **집합 위의 치환**이다.
                       치환은 유일성을 보존한다.  ⇒ 충돌 불가능
다른 부모(PA ≠ PB):    A가 물려받는 KeyB는 PB에서 유일했고,
                       그 소유자 B가 **같은 연산으로 PB를 떠난다.**  ⇒ 충돌 불가능 (대칭)
```

**두 경우가 전부다. `Swap(SortKey)`는 무조건 옳다.**

### ★ 물음 ① — 검사 5를 완화하면? **여전히 옳다**

위 논증에 검사 5가 안 들어간다. 그리고 **완화했을 때의 동작이 오히려 정확히 원하는 것**이다.

```
같은 컨테이너의 A(키 0)와 B(키 65536)를 교환
   → A가 65536, B가 0  →  표시 목록에서 자리가 정확히 뒤바뀐다  =  "자리 바꾸기"
```

**10차가 용량식에 대해 남긴 우려(*"검사 5를 완화하는 순간 개별 검사는 틀린 답을 낸다"*)는 `SortKey`에는 걸리지 않는다.** 용량식은 두 델타가 동시에 0이 아닌 경우에 갈리지만, `Swap`은 **어떤 경우에도 치환**이기 때문이다. **04-7의 무해 논증을 위 두 줄로 바꾸면 이 질문이 다시 안 올라온다.**

### ★ 물음 ② — `PA == PB`이고 한쪽만 슬롯: **충돌하지 않는다. 그리고 그 시나리오는 애초에 검사 2에서 막힌다**

**충돌 여부는 위 논증이 답한다** — 같은 부모면 치환이고, 두 키는 이미 그 공간에서 유일했다. **구급상자가 물려받는 AK의 키가 "슬롯에 있던 동안 아무도 안 쓰던 값"이라는 것이 걱정의 근거인데, 그게 정확히 반대다** — 13차가 확정한 *"장착한 아이템은 자리를 지킨다"* 는 **그 키가 계속 예약돼 있었다**는 뜻이고(12차 확정, 03-2), 그래서 다른 누구도 그 값을 못 받았다.

**그리고 물려받은 뒤의 의미도 옳다.**

```
본체:  붕대(0)   AK(65536, Hotbar1 슬롯)   구급상자(131072)
       표시 목록 = [붕대, 구급상자]

AK ↔ 구급상자 교환
   → AK가 131072 (슬롯에서 나와 구급상자의 자리로)
   → 구급상자가 65536 (핫바로 들어가며 AK가 지키던 자리를 물려받는다)
   → 표시 목록 = [붕대, AK]     ← AK가 구급상자가 있던 자리에 그대로 나타난다
```

**둘이 자리를 통째로 맞바꾼다 — 슬롯도, 순서 자리도.** 그게 교환의 정의이고, `Swap(SortKey)`가 그걸 한 줄로 표현한다.

> **★ 다만 그 시나리오는 **지금** 만들 수 없다.** 핫바 1~4는 종류 제한이 있다 — *"1·2 무기 전부 / 3 보조무기 / 4 근접무기"*(`EquipmentSlots.md` §1-1). **구급상자의 `SlotPriority`에 `Hotbar1`이 없으므로 검사 2가 먼저 거절한다.**
>
> **`PA == PB`이고 한쪽만 슬롯인 조합 자체가 좁다.** 몸 슬롯은 `Parent == INDEX_NONE`을 요구하므로(검사 3) 상대도 **본체 수납**이어야 하는데, **본체는 0칸으로 확정됐다**(13차). 부착 슬롯 쪽도 무기의 `GetCapacity`가 0이라 수납이 없다. **⇒ 이 조합은 `MaxSlots = 10` 테스트 기간에만 존재한다.**
>
> 그래도 논증은 그 기간에 대해서도 옳고, **본체가 다시 칸을 갖는 날에도 옳다.** 조건에 안 기대는 논증을 쓰는 값이 여기 있다.

---

## 1-3. ★★ 새 결함 — 15차의 다섯 줄이 03의 계약 **셋**을 깬다

**15차가 04-7에 적은 코드다.**

```cpp
Swap(A.ParentEntryId, B.ParentEntryId);
Swap(A.SlotId,        B.SlotId);
Swap(A.SortKey,       B.SortKey);
Entries.MarkItemDirty(A);
Entries.MarkItemDirty(B);
```

**03이 이 다섯 줄을 세 곳에서 금지하거나 다르게 약속하고 있다.**

### ⓐ `AssignSortKey` 우회 — 03-2가 **명시적으로 이 코드를 금지한다**

03-2의 "단일 쓰기 지점" 표에 이렇게 적혀 있다.

> \| `SortKey` \| `AssignSortKey` — *"화면 순서를 고치는 유일한 지점"* (11차)<br>★ **`SwapEntries`(04-B)도 이걸 두 번 부른다** — **`E.SortKey = Other.SortKey` 직접 대입 금지** \|
> — `05_Loot_03_Inventory.md:783`

**`Swap(A.SortKey, B.SortKey)`가 정확히 그 직접 대입이다.** 11차가 `SwapEntries`를 이름으로 지목해 *"이걸 두 번 부른다"* 까지 적어뒀는데, 15차가 그 줄을 못 보고 반대 코드를 적었다.

**고치는 형태는 두 줄이다.**

```cpp
const int32 KeyA = A.SortKey;          // ★ 먼저 뜬다 — 첫 호출이 A.SortKey를 덮는다
const int32 KeyB = B.SortKey;
AssignSortKey(A.EntryId, KeyB);
AssignSortKey(B.EntryId, KeyA);
```

> **★ 값을 먼저 뜨는 것이 계약이다.** `AssignSortKey(A, B.SortKey)` → `AssignSortKey(B, A.SortKey)`로 이어 쓰면 **두 번째가 이미 덮인 값을 읽어 두 키가 같아진다.** 컴파일되고, 증상은 *"교환하면 두 아이템이 같은 자리에 겹친다"* 다. `InsertEntry`가 `NextEntryId`를 *"참조보다 먼저 뜬다"* 로 처리한 것과 같은 종류다(03-3).

### ⓑ `MoveEntry`의 *"유일한 지점"* 이 깨진다

03-2의 같은 표 바로 아래 줄이다.

> \| **`ParentEntryId` + `SlotId`** \| **`MoveEntry`** — *"고치는 유일한 지점"* \|

`SwapEntries`가 둘을 직접 쓰면 **유일하지 않다.** 그런데 `MoveEntry`로 대신할 수는 없다 — **그게 `SwapEntries`를 만든 이유 자체**다(순차 적용은 성립하는 교환을 거절한다).

**즉 `SwapEntries`는 진짜로 두 번째 쓰기 지점이고, 문서가 그걸 인정해야 한다.** 숨기면 다음 사람이 *"유일한 지점"* 을 믿고 `MoveEntry`만 감사한다.

### ⓒ ★★ 스코프 가드가 없다 — 그리고 03-7이 *"자동으로 옳다"* 고 적어뒀다

13차가 확정한 03-7의 규칙이다.

> **㉡ 재진입·필수 — 단일 쓰기 지점 다섯**: `InsertEntry` · `SetEntryCharges` · `RemoveSelf` · `AssignSortKey` · `MoveEntry`
> - **`SwapEntries`(04-B) · ⓐ의 재장전(§7-4) · §7-3 부착물은 자동으로 옳다 — 전부 다섯을 경유한다**

**`SwapEntries`가 `MarkItemDirty`를 직접 부르는 순간 이 문장이 거짓이 된다.** 그리고 04-7의 코드에 `FScopedInventoryNotify`가 **없다.**

| 빠뜨리면 | 증상 |
|---|---|
| 알림이 두 번 나간다 (`MarkItemDirty` ×2) | 03-7이 *"무해"* 로 판정한 것 — 큰 문제는 아니다 |
| **재진입 방지가 없다** | 구독자가 인벤토리를 건드리면 `Entries.Items`를 도는 도중에 알림이 나간다 — **㉡이 지키는 것 그 자체** |

**그리고 ⓐ를 고치면 `AssignSortKey`가 가드를 갖고 있으므로 알림이 중간에 두 번 더 나간다** — 필드 셋을 고치는 한 번의 연산이 알림 넷을 쏜다. **`SwapEntries` 선두에 가드 하나를 놓으면 셋 다 닫힌다.**

### ★ 처방 — `SwapEntries`를 표의 **여섯 번째**로 올린다

```
단일 쓰기 지점 (03-2 · 03-7)
   InsertEntry · SetEntryCharges · RemoveSelf · AssignSortKey · MoveEntry · SwapEntries(04-B)
                                                                            ↑ 여섯 번째
```

그리고 두 문장을 고친다.

| 어디 | 지금 | 고칠 것 |
|---|---|---|
| 03-2 표 (`:783`) | *"`SwapEntries`도 `AssignSortKey`를 두 번 부른다"* | **맞다. 04-7의 코드가 그걸 안 지킨다** — 04-7을 `AssignSortKey` 형태로 |
| 03-2 표 (`:784`) | `ParentEntryId`+`SlotId`의 **유일한** 지점 = `MoveEntry` | *"**단독 이동**의 유일한 지점. 원자적 교환은 `SwapEntries`(04-B)가 두 번째다 — 순차 `MoveEntry`로 표현할 수 없기 때문이고, 그래서 그쪽도 같은 계약(필드만 고치고 `MarkItemDirty`)을 진다"* |
| 03-7 (`가드` 절) | *"`SwapEntries`는 자동으로 옳다 — 다섯을 경유한다"* | **`SwapEntries`를 ㉡ 목록에 올린다.** 경유하지 않는다 |
| 04-7 코드 | 가드 없음 | 선두에 `FScopedInventoryNotify Guard(this);` |

> **13차의 A-5가 정확히 이 모양이었다** — *"03-7의 가드 목록이 두 번 연속 낡았다. 원인은 그게 두 번째 목록이라서다."* 13차는 그걸 **표 하나로 합쳐** 고쳤는데, **15차가 그 표에 올리지 않고 새 쓰기 지점을 만들었다.** 표를 만든 지 하루 만에 같은 종류가 났다는 것이, **표를 유지하는 규율이 아직 습관이 아니라는 신호다.**

---

# §2. 04 자체의 빈 자리 여섯

## 2-1. ★★ `NativeOnDrop` 반환값 — **실제 결함이다. Slate를 정확히 읽었다**

### 엔진 소스로 확정한다

**세 단계 전부 확인했다.**

```cpp
// ① NativeOnDrop이 false면 Unhandled다
// UMG/Private/Slate/SObjectWidget.cpp:435-450
FReply SObjectWidget::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    ...
    if ( WidgetObject->NativeOnDrop( MyGeometry, DragDropEvent, NativeOp->GetOperation() ) )
        return FReply::Handled();
    ...
    return FReply::Unhandled();          // ← :449
}

// ② 라우팅 루프는 "핸들되지 않은 동안" 계속 돈다
// Slate/Private/Framework/Application/SlateApplication.cpp:452
for (; !Reply.IsEventHandled() && RoutingPolicy.ShouldKeepGoing(); RoutingPolicy.Next())

// ③ FBubblePolicy는 가장 깊은 곳에서 부모 쪽으로 간다
// SlateApplication.cpp:385-401
FBubblePolicy(const FWidgetPath& InRoutingPath) : WidgetIndex( InRoutingPath.Widgets.Num()-1 )
void Next() { --WidgetIndex; }
```

**`false`는 멈추지 않는다. 부모로 올라간다.** 04-1이 인용한 문장(*"처음 `Handled()`를 반환한 곳에서 멈춘다"*)이 정확했고, **그 문장의 대우가 이 결함이다.**

### ❗ 그리고 증상이 요청서가 적은 것보다 나쁘다

요청서는 *"거절된 드롭이 맨 뒤로 보내기로 둔갑한다"* 로 적었다. **`HandleDrop`의 분기를 따라가면 그것만이 아니다.**

```cpp
// 아이템 위에 놓았다 = 교환 요청. HandleDrop이 용량 부족으로 false를 냈다고 하자
칸의 NativeOnDrop  →  false  →  버블링
패널의 NativeOnDrop →  HandleDrop({ INDEX_NONE, INDEX_NONE })
                       → P->SourceContainer != ContainerId       (다른 컨테이너에서 왔다)
                       → Hit.EntryId == INDEX_NONE
                       → Server_MoveEntry(P->EntryId, ContainerId, NAME_None)   ★
```

**거절된 `SwapEntries`가 `MoveEntry`로 바뀌어 나간다.** 그리고 **그 이동은 성공할 수 있다** — 교환은 상대의 크기까지 계산해야 해서 거절됐지만, 단순 이동은 자기 크기만 보기 때문이다.

```
외투 7/10.  배낭의 AK(4)를 외투의 붕대(1) 위에 놓는다
   교환 판정:  7 - 1 + 4 = 10  ≤ 10   → 성립.  (거절되는 예를 만들려면 8/10)
외투 8/10.  같은 조작
   교환 판정:  8 - 1 + 4 = 11  > 10   → 거절
   버블링 →  이동 판정:  8 + 4 = 12 > 10  → 서버도 거절 ✅

외투 5/10, 붕대 대신 구급상자(3) 위에 AK(4)를 놓는다 … 교환은 성립한다
```

**두 판정이 항상 같은 답을 내지는 않는다** — 교환은 `-SizeB + SizeA`, 이동은 `+SizeA`라 **상대가 나보다 클 때만 교환이 더 관대하다.** 즉 *"교환은 거절인데 이동은 통과"* 는 **상대가 나보다 작을 때**이고 그건 흔하다. 결과는 **사용자가 교환을 요청했는데 아이템이 그냥 옮겨 들어간다.**

> **지금 안 드러나는 이유가 있다** — 04-7의 `HandleDrop` 스케치가 모든 분기에서 `return true`다. **그런데 04-1이 *"검증은 패널의 `HandleDrop` 한 곳"* 이라고 못박았으므로, 거기서 `false`가 나오는 것은 설계된 동작이다.** 구현자가 첫 `return false`를 쓰는 순간 열린다.

### ★ 판정 — 반환값을 라우팅에만 쓴다는 진단이 맞다

```
"내가 이 드롭의 주인이다"  (라우팅)   →  NativeOnDrop의 반환값
"이 드롭이 성립한다"       (판정)     →  HandleDrop 안에서 끝난다. 밖으로 안 나간다
```

**칸도 패널도 언제나 `true`를 돌려준다.** 근거 셋이다.

| | |
|---|---|
| ① | **칸은 패널 안에 있다.** 칸에 떨어뜨린 드롭의 주인은 언제나 그 패널이다 — 다른 후보가 없으므로 `false`가 뜻할 것이 없다 |
| ② | **버리기 존과 겹치지 않는다.** 04-5가 이미 *"버리기 존은 구획 어디에도 안 걸린 드롭만 받는다 — 두 경로가 겹치지 않는다"* 로 확정했다. 칸에서 `false`를 흘리면 **그 확정이 깨진다** |
| ③ | **거절을 사용자에게 알리는 장치가 이미 있다.** 드래그 중 빨강 ＋ `18 → 22 / 20` 미리보기(04-0)가 **드롭 전에** 답한다. 드롭 후의 반환값에 실을 정보가 없다 |

**`HandleDrop`의 `bool`은 남겨도 된다** — *"요청을 보냈는가"* 로 읽고 로그·`ensure`에 쓴다. 다만 **`NativeOnDrop`이 그것을 그대로 반환하지 않는다.**

```cpp
bool UEPItemCellWidget::NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation* Op)
{
    OwnerPanel->HandleDrop(FEPCellHit{ DisplayIndex, EntryId }, Op);
    return true;      // ★ 라우팅의 답이다. 성립 여부가 아니다 (16차)
}
```

> **★ 함정으로 남길 것.** *"`HandleDrop`의 판정 결과를 `NativeOnDrop`이 그대로 반환한다"* — 증상은 **거절된 교환이 이동으로 바뀌어 성공하는 것**이고, 재현 조건이 *"상대가 나보다 작고 컨테이너가 좀 찼을 때"* 라 **교환 로직 버그로 오진한다.**

---

## 2-2. ★★ `FEPItemDragPayload` — 빈 자리가 맞다. **`UDragDropOperation` 파생이 맞다**

### 빈 자리 확인

**04 전체에 선언이 없다.** `:796`과 `:848`이 쓰고, `NativeOnDragDetected`는 `:513`에 **오버라이드 선언만** 있다. 지적이 정확하고, **`FEPCellHit`은 10차에 구조를 받았는데 페이로드는 못 받았다**는 대비도 정확하다.

### ★ 판정 — 파생이 맞다. `Payload`를 쓰면 **8차가 지운 것을 되살린다**

**엔진이 두 방법을 같은 헤더에서 제시한다.**

```cpp
// UMG/Public/Blueprint/DragDropOperation.h:51
/** This class is the base drag drop operation for UMG, extend it to add additional data and add new functionality. */

// :66-71
/** The payload of the drag operation. This can be any UObject that you want to pass along as dragged data.
 *  If you were building an inventory screen this would be the UObject representing the item being moved to another slot. */
UPROPERTY(...) TObjectPtr<UObject> Payload;
```

**`Payload` 주석이 하필 인벤토리 화면을 예로 든다.** 그런데 **그 문장이 요구하는 것이 `UObject`다** — *"the UObject representing the item"*.

**이 프로젝트에는 아이템을 대표하는 `UObject`가 없다. 의도적으로 없앴다.**

| | |
|---|---|
| `UEPItemInstance` | **삭제됨** — 값 타입 설계(`FEPInventoryEntry`) |
| `UListView` 기각 근거 | *"아이템 타입이 `UObject*`로 고정돼 있다"*(`ListView.h:38`). *"도입하면 엔트리마다 UObject 래퍼를 만들어야 한다 — **방금 지운 `UEPItemInstance`를 UI 전용으로 부활시키는 것**"*(04-2) |

**`Payload`를 쓰는 것은 `UListView`를 쓰는 것과 정확히 같은 대가를 치른다.** 드래그 하나마다 래퍼 `UObject`를 만들고 수명을 관리하게 된다. **04-2가 이미 내린 판정이 여기에도 그대로 적용된다.**

**⇒ 파생이 맞다. 그리고 그건 엔진이 클래스 주석에서 직접 지시한 확장 지점이다**(*"extend it to add additional data"*).

> **★ C++ 선례는 0건이다 — 정확히 적어둔다.** 엔진 `Source` ＋ `Plugins` ＋ Lyra 전수 grep에서 `public UDragDropOperation` 파생이 **하나도 없다.** 이유는 *"파생이 틀려서"* 가 아니라 **UMG 드래그가 대개 블루프린트에서 파생되기 때문**이다(그 경로는 grep에 안 잡힌다). **주석이 지시하는 확장을 C++로 하는 것에 반례는 없다.**

### 필드 확정안

```cpp
UCLASS()
class UEPItemDragOperation : public UDragDropOperation
{
    GENERATED_BODY()
public:
    int32 EntryId         = INDEX_NONE;   // 끄는 것
    int32 SourceContainer = INDEX_NONE;   // ParentEntryId. "같은 컨테이너면 Reorder" 판정 (04-7)
    FName SourceSlotId;                   // ★ 슬롯에서 끌었나 — 아래
    FName ItemId;                         // SlotSize·Rarity 조회 (04-7 피드백)

    // 드래그 중 고정. OnInventoryChanged에 Empty() (04-7 각주)
    TMap<int32, int32> CachedUsed;
};
```

- **`UPROPERTY`가 필요 없다.** 복제도 BP 노출도 안 하고 `UObject`를 안 든다 — `FEPCellHit`과 같은 판정이다(§4-⑥)
- **`SourceSlotId`가 필요한 것이 맞다.** 요청서의 추정이 맞고, 근거가 **둘**이다
  - `HandleDrop`의 첫 분기 `P->SourceContainer == ContainerId`가 **슬롯에서 끈 것을 수납으로 오판한다.** 핫바1의 AK(`Parent == INDEX_NONE`)를 **본체 구획**에 놓으면 `INDEX_NONE == INDEX_NONE`이 참이라 **`Server_ReorderEntry`로 간다** — 그런데 이건 **해제**이고 `Server_MoveEntry(id, -1, None)`이어야 한다. `ReorderEntry`는 `if (!E.SlotId.IsNone()) return;`으로 조용히 아무 일도 안 한다(03-2) ⇒ **"핫바에서 본체로 빼면 아무 일도 안 일어난다"**
  - `IgnoreEntryId`를 넘기는 UI 하이라이트(04-1 ②)가 *"이 엔트리가 지금 어느 슬롯을 떠나는가"* 를 알아야 한다

> **★★ 위 첫 항목은 §2-2에 딸린 **별도 결함**이다.** 본체가 0칸이면 본체 구획이 안 그려져 지금은 도달 불가지만, **`Wrist`·`Torso` 같은 착용 컨테이너로 끌 때는 `SourceContainer`(=`INDEX_NONE`)와 `ContainerId`(=상의Id)가 달라 우연히 맞는다.** 즉 **본체가 칸을 가진 테스트 기간에만 나는 버그**이고 그때는 `MaxSlots=10`이라 자주 난다.
>
> **고치는 형태:** 첫 분기를 `P->SourceContainer == ContainerId **&& P->SourceSlotId.IsNone()**` 으로. 10차가 같은 자리에서 조건을 **뺀** 적이 있으므로(*"② 같은 컨테이너의 여백에 놓으면 서버로 가서 거절됐다"*), **왜 이 조건은 붙이고 그때 조건은 뺐는지**를 한 줄로 적어둬야 한다 — 그때 뺀 것은 `Hit != INDEX_NONE`(드롭 **지점**), 지금 붙이는 것은 `SourceSlotId`(드래그 **출처**)로 **다른 축**이다.

---

## 2-3. 04-8의 두 문장 — **모순은 아니다. ①을 안 건드리고 ②를 미룬다**

### 판정

**요청서의 진단이 맞다.** 11차가 뒤집은 것은 *"순서의 **소유권**"* 이었고(지속을 줄 수 없다), *"UI가 순서를 한 프레임도 들면 안 된다"* 를 주장한 적이 없다. **①의 *"UI에 순서 자료구조가 없다"* 는 정상 상태의 서술**이고, ②는 **낙관적 적용을 넣을 때만** 생기는 임시 상태다.

**그리고 요청서가 제안한 답이 맞다** — *"②를 '넣을 때'로 명시적으로 미루고 ①을 손대지 않는다."*

### 근거 — ①을 완화하면 11차가 지운 것이 문 하나를 다시 연다

①의 문장은 단순한 사실 서술이 아니라 **04-8이 삭제한 것들의 요약**이다.

> *"`Resolve` / `MoveTo` / `ULocalPlayerSaveGame` / `ULocalPlayerSubsystem` / `FEPContainerOrder`가 전부 사라졌다."*

**그 다섯이 전부 "UI가 순서를 든다"에서 나왔다.** ①을 *"UI가 순서를 들 수 있다"* 로 풀면 **어디까지 드는가**가 다시 열리고, 함정 13e(클라 로컬 세이브)가 *"잠깐만 드는 건데"* 로 재진입할 통로가 생긴다.

### 필요한 것은 한 줄이다

②의 각주에 **범위**를 못박는다.

> **②의 "표시 순서만 로컬로 뒤집는다"는 `UEPContainerPanel`의 `Rebuild`가 만든 `TArray<int32> Ordered`를 그 자리에서 한 번 재배열하는 것이다.** 위젯에 **필드로 남기지 않는다** — 다음 `RefreshEntries()`가 `GetSortedContents()`로 다시 만들고, 그것이 곧 "복제가 오면 버린다"이다. **저장할 것이 없으므로 ①(*"UI에 순서 자료구조가 없다"*)이 그대로 유지된다.**

**이러면 ②가 짜지고 ①이 안 바뀐다.** 재배열 결과가 **다음 `Rebuild`까지만 사는 지역 변수**라, `Resolve`도 세션 검사도 생길 자리가 없다.

> **★ 남은 문제 하나 — 재배열은 되지만 "다시 그리기"의 계기가 없다.** `RefreshEntries()`는 `OnInventoryChanged`가 부른다. 낙관적 적용은 **알림 없이** 다시 그려야 하므로 **드롭 핸들러가 `Rebuild`를 직접 부르는 경로**가 필요하다. 지금 04에 그 경로가 없다 — ②를 넣을 때 같이 정할 것으로 한 줄 적어둔다.

---

## 2-4. `UEPSegmentedBar` · `UEPRemainderWidget` — **04-A를 시작할 만큼 안 정해져 있다**

### 판정: 맞다. 그리고 세 가지가 걸려 있다

**둘 다 04-A 범위인데 나온 것은 호출 두 줄뿐이다.** 완료 조건 5(*"칸 3개인데 게이지가 7/20"*)와 11(*"넘치는 구간"*)의 **유일한 표현 장치**다.

### ⓐ `SetSegments(Ordered, Inv, Max)`는 04-1 ②와도 부딪힌다

요청서가 짚은 **이중 순회**가 맞다. 여기에 하나 더 있다.

04-1 ②가 세운 규칙은 *"UI는 판정식을 자기가 쓰지 않고 컴포넌트 함수를 부른다"* 인데, **게이지에 컴포넌트를 통째로 넘기면 위젯이 `FindEntry` → `FindData` → `SlotSize`를 **스스로 조립**하게 된다.** 그건 판정이 아니라 조회지만, **04-2의 `Rebuild`가 이미 그 조립을 하고 있으므로 조립식이 두 곳이 된다** — 이 문서가 세 번 금지한 모양이다.

**제안한 대안이 맞고, 04-0이 그걸 이미 요구한다.**

```cpp
// 평범한 struct. UPROPERTY 없음 (§4-⑥과 같은 판정)
struct FEPCapacitySegment
{
    int32          EntryId  = INDEX_NONE;   // ★ 04-0의 hover 연동이 이걸 요구한다
    int32          SlotSize = 0;
    EEPItemRarity  Rarity   = EEPItemRarity::Common;
};
```

**`EntryId`가 구간에 없으면 04-0이 명시한 것이 안 된다** — *"칸에 hover하면 해당 구간이 밝아지고 **반대도 된다**"*. 반대 방향(게이지 → 칸)은 구간이 자기가 어느 엔트리인지 알아야 성립한다. **즉 구조체는 성능 최적화가 아니라 04-0의 요구를 만족하는 유일한 형태다.**

### ⓑ ★★ 드래그 미리보기의 자리를 **04-A가 만들어야 한다** — 8차 03-7 사고가 또 있다

**04-0과 04-7이 요구하는 것이 넷인데, 그중 둘이 04-B다.**

| 무엇 | 어디 | 구간 |
|---|---|---|
| 아이템별 구간 그리기 | `SetSegments` | **04-A** |
| `＋ 남은 용량 N` | `SetRemaining` | **04-A** |
| **드래그 중 초록/빨강 ＋ 넘치는 구간** | ? | **04-B** (04-7) |
| **`7 → 11 / 20` 미리보기 숫자** | ? | **04-B** (04-0 ★ *"색이 아니라 숫자가 답한다"*) |

**04-A가 두 위젯을 만드는데, 04-B가 그 위젯에 상태를 하나씩 더 요구한다.** 이 문서가 이미 같은 것을 한 번 경고했다.

> *"★ 8차의 03-7 사고와 같은 것이 하나 있다 — **04-A가 칸을 `UUserWidget`으로 만들어야 한다.** 04-B의 드롭이 칸 위젯의 `NativeOnDrop`에 걸리기 때문이다. 04-A에서 가볍게 만들면 04-B에서 **전부 다시 만든다.**"* (04-분할)

**같은 문장이 게이지와 남은 용량 블록에 그대로 적용되는데 안 적혀 있다.** 04-A가 `UProgressBar` 하나로 때우면 04-B에서 넘치는 구간을 못 그린다.

**최소 API 넷을 04-2에 이름으로 확정하는 것을 권한다.**

```cpp
class UEPSegmentedBar : public UUserWidget       // 04-A가 만든다
{
    void SetSegments(const TArray<FEPCapacitySegment>& Segments, int32 Max);   // 04-A
    void SetPreview(int32 DeltaSlots);                                          // 04-B. 0이면 해제
    void HighlightSegment(int32 EntryId);                                       // 04-A (hover 연동)
};

class UEPRemainderWidget : public UUserWidget    // 04-A가 만든다
{
    void SetRemaining(int32 Remaining);                                         // 04-A
    void SetPreview(int32 DeltaSlots);                                          // 04-B. 부호로 색이 갈린다
};
```

- **`SetPreview`가 04-B여도 선언은 04-A에 있어야 한다** — 위젯 트리와 `BindWidget` 이름이 04-A에 고정되기 때문이다
- **`CapacityText`의 `7 → 11 / 20`은 위젯이 아니라 `UEPContainerPanel`이 만든다** — 이미 `BindWidget`으로 갖고 있다. **새 위젯이 필요 없다는 것을 적어두면** 구현자가 세 번째 위젯을 만들지 않는다

### ⓒ 그리는 방법 자체는 정할 것이 없다

**`UEPSegmentedBar`는 `UHorizontalBox`에 `UImage` N개를 `FSlateChildSize(ESlateSizeRule::Fill)`로 넣고 `SlotSize`를 비율로 주면 끝이다.** 남는 용량은 마지막에 회색 하나. **엔진에 이걸 위한 위젯은 없고**(`UProgressBar`는 단일 비율), 만들 것도 20줄 남짓이라 **04-2에 API만 적으면 충분하다** — 그리는 코드를 문서에 적을 필요는 없다.

---

## 2-5. `UDynamicEntryBox` 가상화 — **결론 유지. 근거가 오히려 더 강해진다**

### 인용 확인 — 다만 **두 문장을 다 인용해야 한다**

```
// UMG/Public/Components/DynamicEntryBox.h:10-14
 * A special box panel that auto-generates its entries at both design-time and runtime.
 * Useful for cases where you can have a varying number of entries, but it isn't worth
 * the effort or conceptual overhead to set up a list/tile view.
 * Note that entries here are *not* virtualized as they are in the list views,
 * so generally this should be avoided if you intend to scroll through lots of items.
```

**요청서가 인용한 것은 뒷문장이고, 앞문장은 우리 상황을 그대로 적고 있다** — *"가변 개수인데 list/tile view를 세울 만큼은 아닌 경우에 유용하다."* 이 문서가 `UListView`를 기각한 이유(`ListView.h:38`)와 **같은 판단을 엔진이 문장으로 갖고 있다.**

### ❗ 그리고 요청서의 규모 계산이 자기 설계보다 크다

> *"각 구획이 **최대 용량만큼** 칸을 갖는다. 배낭 20칸 + 상의 10 + 하의 5 + …"*

**04-0이 그렇게 안 그리기로 확정했다.**

> *"**빈 칸을 N개 그리지 않는다.** … 아이템 칸 뒤에 `＋ 남은 용량 13` 블록 **하나**가 붙어 남은 줄을 채운다."*
> *"**위젯 수가 아이템 수에만 비례한다** — 04-2의 재생성 비용이 **용량과 무관해진다**."*

**칸 위젯 수 = 아이템 개수다. 용량이 아니다.**

```
확정 용량표(13차)로 상한을 잡으면
   상의 10 + 하의 5 + 배낭A 12 + 외투 ? + 팔목 ?      ← 전부 1칸짜리로 꽉 채웠을 때
   ≈ 30~40 칸 위젯  ＋ 남은 용량 블록 5  ＋ 착용 슬롯 12
   ≈ 50~60
```

**엔진 문장이 경계하는 *"lots of items"* 는 리스트뷰가 필요한 규모** — 수백~수천이다. **50~60은 그 근처도 아니다.**

> **즉 04-0의 결정(빈 칸을 안 그린다)이 `Wrap`을 고른 근거이면서 동시에 가상화 걱정을 없앤 근거다.** 10차가 풀링만 보고 골랐다는 지적은 맞지만, **결과적으로 두 이유가 같은 결정을 가리킨다.**

### ★ 그리고 직접 `FUserWidgetPool`을 드는 대안에 **새 반대 근거가 하나 나왔다**

04-2가 *"`UUniformGridPanel`을 유지해도 된다 — 그때는 `FUserWidgetPool`을 직접 든다"* 고 적어뒀다. **그 경로에는 엔진이 경고를 붙여놨다.**

```
// UMG/Public/Blueprint/UserWidgetPool.h:17-20
WARNING: Be sure to release the pool's Slate widgets within the owning widget's ReleaseSlateResources
call to prevent leaking due to circular references.
Otherwise the cached references to SObjectWidgets will keep the UUserWidgets - and all that they reference - alive
```

**`UDynamicEntryBox`는 그걸 이미 한다.**

```cpp
// UMG/Private/Components/DynamicEntryBoxBase.cpp:28-33
void UDynamicEntryBoxBase::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    EntryWidgetPool.ReleaseAllSlateResources();
}
```

**직접 드는 대안은 이 한 줄을 빠뜨릴 수 있고, 증상은 "인벤토리를 여닫을수록 메모리가 는다"** 다. **04-2의 대안 문단에 이 경고를 붙여두면** 그쪽으로 갔을 때 무엇을 지켜야 하는지가 남는다.

### 문서 조치

04-2의 비교표에 **행 하나**를 더한다.

| | 초안(수동 재생성) | `UDynamicEntryBox` |
|---|---|---|
| **가상화** | 없음 | **없다** (`DynamicEntryBox.h:13-14`) — *"list view처럼 가상화되지 않으니 많은 항목을 스크롤할 거면 피하라"*. **우리는 위젯 수가 아이템 수(≈50)에 비례하므로 해당 없다** (04-0) |
| Slate 자원 해제 | 직접 | **`ReleaseSlateResources`가 풀을 놓는다**(`DynamicEntryBoxBase.cpp:32`). 직접 들면 **이걸 빠뜨리면 샌다** |

---

## 2-6. `SlotDisplayName`의 자리 — **미정 #10에 합류. 단 "필드"가 아니라 "슬롯 정의"로 넓힌다**

### 세 후보 중 둘은 이미 닫혀 있다

| 후보 | 판정 |
|---|---|
| `UEPLootDeveloperSettings` | **닫혔다.** 14차가 *"에셋 참조와 디버그만"* 으로 못박았다(`05_Loot_DOCS.md:1118-1120`) |
| 위젯에 `TMap<FName, FText>` | **지금은 이게 맞다** — 아래 |
| `UEPPawnInventoryData` | **최종 자리. 단 `TArray<FName>`으로는 못 받는다** — 아래 |

### ★★ 두 번째 고아를 찾았다 — `EmptyHint`도 출처가 없다

04-2의 위젯 트리에 이렇게 있다.

```
│         └─ WBP_EquipSlot × 12 (UEPEquipSlotWidget)
│              ├─ ItemIcon / EmptyHint (Image)  ← 비면 옅은 부위 아이콘
```

**"부위 아이콘"이 슬롯마다 다른 텍스처인데 어디서 오는지가 없다.** 그리고 04-0이 그걸 기능으로 요구한다 — *"빈 슬롯에 옅은 아이콘을 깔아 **무엇이 들어가는지 보인다**"*.

**⇒ 슬롯마다 붙는 표시용 값이 최소 둘이다.**

```
Torso  →  "상의"(FText)  ＋  T_SlotHint_Torso(UTexture2D)
Legs   →  "하의"        ＋  T_SlotHint_Legs
...  × 12
```

**`TArray<FName> BodySlots` 하나로는 둘 다 못 든다.** 그래서 답은 *"어느 필드에 넣나"* 가 아니라 **"미정 #10의 이행이 필드 이동이 아니라 타입 확장이다"** 이다.

```cpp
// 미정 #10이 UEPPawnInventoryData로 갈 때의 모양
USTRUCT()
struct FEPBodySlotDef
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly) FName                     SlotId;       // ← 지금의 BodySlots 원소
    UPROPERTY(EditDefaultsOnly) FText                     DisplayName;  // ← §2-6
    UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UTexture2D> EmptyHint;   // ← 새로 찾은 것
    UPROPERTY(EditDefaultsOnly) bool                      bIsContainer; // ← ContainerOrder가 흡수될 수 있다
};
```

**이건 미정 #10을 넓히는 것이지 새 항목이 아니다.** #10이 이미 *"`BodySlots`가 특히 그렇다 — 답해야 할 질문이 '이 소유자에게 그 슬롯이 있는가'"* 라고 적었는데, **`FName` 배열로는 그 질문에도 못 답한다.** 같은 이유가 표시값에도 적용된다.

### 그때까지는 — **위젯이 든다**

| | 왜 |
|---|---|
| `FText`·`TSoftObjectPtr<UTexture2D>`다 | **로컬라이즈 대상 ＋ 소프트 에셋.** 서버가 로드할 이유가 없는 값이라 게임플레이 데이터와 수명이 다르다 |
| 소비자가 클라 UI **하나**다 | 13차가 `StartingEquipment`에 쓴 판정선 그대로 — *"인스턴스 없이 읽어야 하는 소비자가 둘 이상"* 이 아니다 |
| *"슬롯이 늘 때마다 위젯을 연다"* 는 걱정은 안 맞다 | `UPROPERTY(EditDefaultsOnly) TMap<FName, FEPSlotVisual>`이면 **에디터에서 WBP를 연다.** 코드를 여는 게 아니다 |
| 버려질 때 싸다 | #10이 오면 **`TMap` 하나를 지우고 `Def->DisplayName`으로 바꾼다.** `ContainerOrder`가 *"한 줄"* 인 것과 같은 규모다 |

**`SlotDisplayName(SlotId)`는 그 `TMap`을 보는 헬퍼로 04-2의 `protected`에 선언한다** — `MakePanel`과 함께(→ §4-⑥).

> **★ 미정 #10에 한 줄 추가를 권한다:** *"이행은 필드 이동이 아니라 **타입 확장**이다 — `TArray<FName> BodySlots` → `TArray<FEPBodySlotDef>`. 표시 이름(`FText`)과 빈 슬롯 아이콘이 슬롯마다 붙기 때문이고(04-0·04-2), 그 둘은 **그때까지 Step 04 위젯이 든다**(클라 전용·로컬라이즈 대상)."*

---

# §3. 04-A/04-B 분할 — **15차 선택이 맞다. 근거는 부수 질문에 있다**

## 3-1. RPC 규칙은 여기 적용되지 않는다 — 요청서의 지적이 맞다

> *"세 번 적용한 규칙은 RPC에 대한 것이었다. 근거는 **공격 표면**이었다. 내부 함수의 기본 인자에는 그 근거가 성립하지 않는다."*

**맞다.** `Server_MoveEntry`(9차)·`Server_ReorderEntry`(11차)·`Server_EquipBackpack`(14차)의 근거를 원문으로 확인했다.

> *"Step 03에는 `NewParent`와 `NewSlotId`를 정당하게 만들어낼 UI가 없다. **그런데 RPC를 열면 조작된 클라이언트는 만들 수 있다.**"*(03-2 `:1205`)

**`CanPlaceInSlot`은 `const` 조회 함수이고 클라이언트가 부를 수 있는 것이 아니다.** 규칙의 근거 문장이 통째로 성립하지 않는다. **03-2와 04-7에 적힌 인용은 잘못된 인용이다.**

## 3-2. ★ 그런데 결론은 15차가 맞다 — **부수 질문이 답이다**

> **부수 질문:** *"04-B는 03 파일을 여는 단계인가? 그렇다면 §3의 답은 자동으로 정해진다."*

**여는 단계다. 그것도 크게 연다.**

| 04-B가 `UEPInventoryComponent`에 추가하는 것 | 어디 |
|---|---|
| `bool SwapEntries(int32 A, int32 B)` — **본문 40줄** | 04-7 |
| `UFUNCTION(Server, Reliable) void Server_MoveEntry(int32, int32, FName)` | 04-7 |
| `UFUNCTION(Server, Reliable) void Server_SwapEntries(int32, int32)` | 04-7 |
| `UFUNCTION(Server, Reliable) void Server_ReorderEntry(int32, int32)` | 04-8 |

**04-B는 이미 그 헤더와 `.cpp`를 여는 단계다.** 그러면 요청서 표의 *"04-B 착수 시 03 헤더 + `.cpp`를 다시 연다. **세 단계 전 파일이다**"* 라는 위험이 **0이 된다** — 다시 여는 게 아니라 그때 여는 것이 그 단계의 일이다.

**반대편 위험은 남는다.** 03-A에서 4인자로 만들면 **03-A → (구) 03-B → 03-C → 04-A 내내 `Occupant == IgnoreEntryId`가 한 번도 참이 되지 않는다.** 13차가 `Server_EquipBackpack`을 죽인 기준(*"호출자 0개"*)과 같은 방향이고, 규모만 작다.

```
      03-A에서 붙인다                04-B에서 붙인다 (15차)
위험   실행되지 않는 비교 하나가       파일을 다시 연다
       네 구간 동안 남는다            → ❌ 어차피 그 단계에 SwapEntries·RPC 셋을 넣으며 연다
```

**⇒ 04-B. 남는 위험이 한쪽에만 있다.**

## 3-3. 근거 문장을 바꾼다

| 어디 | 지금 | 고칠 것 |
|---|---|---|
| 03-2 `:968` | *"`Server_MoveEntry`(9차)·`Server_ReorderEntry`(11차)·`Server_EquipBackpack`(14차)에 세 번 적용한 규칙(검증 표면은 소비자를 따라간다)이 그대로다"* | ***"저 셋은 RPC였고 근거가 공격 표면이었다. 여기는 `const` 조회 함수라 그 근거가 없다. 04-B에 두는 이유는 다른 것이다 — **04-B가 `SwapEntries` 본문과 RPC 셋을 넣으며 이 클래스를 어차피 연다.** 다시 여는 비용이 0이므로, 실행되지 않는 비교를 네 구간 동안 들고 있을 이유가 없다"*** |
| 04-7 `:710` | 같은 인용 | 같은 문장으로 |

> **★ 이 정정이 값을 하는 이유:** 규칙을 잘못 인용해두면 **다음에 "내부 함수인데 소비자가 뒤에 있는 것"이 나왔을 때 자동으로 뒤로 밀린다.** 그런데 그중에는 **미리 넣어야 하는 것**(계약·반환 규약)이 섞여 있고, 13차가 `KeySpace_NextAbove`의 반환형에 대해 정확히 *"나중에 넣기 비싸다"* 로 판정했다. **판정선이 "RPC냐 아니냐"가 아니라 "그 단계가 그 파일을 여느냐 ＋ 나중에 바꾸는 비용이 얼마냐"** 라는 것을 남겨야 한다.

## 3-4. 분할선 자체는 그대로 맞다

**04-A(표시) / 04-B(드래그)의 경계가 여전히 정확하다.** `SwapEntries`·RPC 셋·`IgnoreEntryId`가 전부 04-B로 모이고, **04-A는 Step 03의 커맨드만으로 검증된다.** 다만 §2-4에서 지적한 대로 **04-A가 만드는 위젯에 04-B가 요구할 자리를 미리 뚫어야 하고**, §4-⑤의 검증 수단 한 줄이 빠져 있다.

---

# §4. 놓친 것

## 4-1. 요청서 §4의 여섯 항목

### ① `UEPEquipPanel`의 `Rebuild`가 한 줄뿐 — **맞다. §2-4·§2-6과 같은 구멍이다**

**`UEPEquipPanel`과 `UEPEquipSlotWidget`은 04 전체에서 위젯 트리(`:242`·`:245`)에만 나오고 클래스 선언이 없다.** 그런데 **완료 조건 10(착용/해제/핫바 배정)과 완료 조건 4의 절반이 그 위젯 위에 선다.**

**필요한 것은 넷이고 전부 이미 문서 어딘가에 근거가 있다.**

```cpp
UCLASS()
class UEPEquipPanel : public UUserWidget
{
    void Rebuild(UEPInventoryComponent* Inv);          // 12칸을 슬롯 정의 순서로 채운다
    bool HandleDrop(const FEPSlotHit& Hit, UDragDropOperation* Op);   // ★ ② 아래
protected:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UUniformGridPanel> WearGrid;    // 착용 8
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UHorizontalBox>    HotbarRow;   // 핫바 1~4
    UPROPERTY(EditAnywhere) TSubclassOf<UEPEquipSlotWidget>    SlotWidgetClass;
};

UCLASS()
class UEPEquipSlotWidget : public UUserWidget
{
    void SetSlot(int32 InParent, FName InSlotId, const FEPInventoryEntry* Occupant);
private:
    int32 ParentEntryId = INDEX_NONE;   // ★ 05-문서가 요구한다 — 아래 ⑥
    FName SlotId;
};
```

**순서를 어디서 받나:** `BodySlots`다(§2-6의 슬롯 정의). **`ContainerOrder`가 아니다** — 그건 컨테이너 **구획**의 순서이고 착용 슬롯 12칸의 배치와 다르다. 지금 문서가 그 구분을 안 하고 있고, `ContainerOrder`에는 `Ears`/`Face`/`Feet`가 없다.

### ② 드롭 대상이 착용 슬롯일 때 — **판정 함수가 둘이 맞다**

**`UEPEquipSlotWidget`의 `OwnerPanel`은 `UEPEquipPanel`이고, 그쪽은 `FEPCellHit`을 쓸 수 없다** — `DisplayIndex`가 뜻이 없고(슬롯은 순서가 없다) `PrevIdFor`도 안 부른다.

**둘로 가르는 것이 맞다. 근거는 이 문서 자신이 `CanFit`/`CanPlaceInSlot`을 안 합친 이유와 같다.**

> *"`CanFit`은 **수납일 때**, `CanPlaceInSlot`은 **슬롯일 때**로 **배타적**이다. 하나로 묶으면 `SlotId`의 `None` 여부로 갈리는 분기가 판정 함수 **안으로** 들어간다."*(03-2)

**드롭 판정도 똑같이 배타적이다.**

```cpp
// 평범한 struct. 컨테이너 쪽 FEPCellHit과 이름을 가른다
struct FEPSlotHit
{
    int32 ParentEntryId  = INDEX_NONE;   // 몸 슬롯이면 -1, 부착이면 무기 EntryId (05가 요구한다)
    FName SlotId;
    int32 OccupantEntryId = INDEX_NONE;  // 비었으면 INDEX_NONE
};

bool UEPEquipPanel::HandleDrop(const FEPSlotHit& Hit, UDragDropOperation* Op)
{
    if (Hit.OccupantEntryId == INDEX_NONE)
        Inventory->Server_MoveEntry(P->EntryId, Hit.ParentEntryId, Hit.SlotId);
    else
        Inventory->Server_SwapEntries(P->EntryId, Hit.OccupantEntryId);
    return true;      // ★ §2-1
}
```

**두 함수가 도합 6줄이고 겹치는 코드가 없다.** 합치면 `Hit`이 다시 두 가지를 뜻하게 되는데, **그게 정확히 10차가 `FEPCellHit`으로 막은 것(함정 12c)이다.**

> **★ 04-1의 문장을 한 줄 고쳐야 한다.** *"검증은 패널의 `HandleDrop` **한 곳**"* → *"**진짜 판정은 `CanFit`/`CanPlaceInSlot`(컴포넌트)에 있다.** 패널의 `HandleDrop`은 **어느 RPC로 보낼지를 고르는 곳**이고, 그 갈래가 배타적이라 **컨테이너용과 슬롯용 둘**이다." 지금 문장은 *"한 곳"* 을 위젯 개수로 읽게 해서 ②가 질문이 됐다.

### ③ `CachedUsed` 무효화 후 — **다시 캔다. 한 줄이면 된다**

```cpp
int32 UEPItemDragOperation::UsedOf(int32 Container, const UEPInventoryComponent* Inv)
{
    if (const int32* Hit = CachedUsed.Find(Container)) return *Hit;
    return CachedUsed.Add(Container, Inv->GetUsedSlots(Container));   // 없으면 그때 캔다
}
// OnInventoryChanged → CachedUsed.Empty();
```

**무효화가 `Empty()`이고 조회가 lazy면 "그 다음"이 저절로 정해진다.** 04-7의 각주가 *"드래그 시작에 **구획마다** 한 번"* 이라고 적었는데 **lazy면 실제로 hover한 구획만 캔다** — 더 적게 돌고 코드도 짧다.

> **함께 확인한 것 — 드래그 중 `RefreshEntries()`가 돌아도 드래그는 안 죽는다.** 드롭 오퍼레이션은 Slate 애플리케이션이 들고 있고 소스 위젯이 소유하지 않는다. `CellBox->Reset(false)`로 소스 칸이 풀에 회수돼도 **페이로드가 `EntryId`(포인터가 아니다)를 들고 있어 댕글링이 없다.** ★ 다만 **아이콘 로드는 다르다** → N-1.

### ④ `FEPCellHit`이 `USTRUCT`인데 `UPROPERTY` 0개 — **평범한 struct로 내린다**

**리플렉션이 필요한 곳이 없다.** `HandleDrop`은 `UFUNCTION`이 아니고, 복제도 BP 노출도 없다.

**그리고 `USTRUCT`로 두는 것이 능동적으로 나쁘다.** `USTRUCT`는 *"이 타입은 리플렉션을 안다"* 는 약속인데 필드에 `UPROPERTY`가 없어 **약속이 거짓**이다. 누군가 이걸 복제되는 컨테이너에 넣으면 **필드가 조용히 안 나간다.**

> **이 프로젝트가 정확히 그 혼동을 한 번 겪었다** — `TArray<class FEPInventoryEntry>`의 `class` 키워드(13차 N-11). **타입이 무엇인지를 문법이 잘못 말하는 것**이라 같은 종류다.

`FEPSlotHit`(위 ②)·`FEPCapacitySegment`(§2-4)·`UEPItemDragOperation`의 필드들도 **같은 판정**이다.

### ⑤ 완료 조건 13의 배분 — **04-A가 맞다. 단 검증 수단 한 줄이 빠졌다**

**완료 조건 13**: *"같은 컨테이너 안 순서를 바꾸면 서버가 기억한다 — 창을 닫았다 열어도, **배낭을 벗었다 입어도** 그대로다."*

**04-A가 검증하는 것이 무엇인지 정확히 말하면 배분이 맞다.**

| 무엇을 확인하나 | 어디 |
|---|---|
| 순서를 **바꾸는** 것 | 03-A의 `EP.Inv.Reorder` |
| **UI가 `SortKey` 순으로 그리는가** (배열 순서가 아니라) | **04-A** ← 함정 1이 여기 걸린다 |
| 창을 닫았다 열어도 그대로 | **04-A** — `RefreshEntries`가 매번 `GetSortedContents`를 다시 부른다 |
| 드래그로 바꾸는 것 | 04-B |

**04-A가 검증하는 것은 *"UI가 순서를 스스로 만들지 않는다"* 이고, 그게 함정 1(배열 순서대로 그림)의 유일한 검증 지점이다.** 배분이 옳다.

**❗ 그런데 04-분할 표의 검증 수단이 *"`EP.Inv.Add` / `EP.Inv.Move`"* 뿐이다.** `EP.Inv.Reorder`가 없으면 **완료 조건 13을 04-A에서 만들 수 없다.** 한 줄 추가한다.

> 04-A 검증 수단: `EP.Inv.Add <ItemId> [Container]` · `EP.Inv.Move` · **`EP.Inv.Reorder <EntryId> <PrevEntryId>`** — 셋 다 03-A 소속이다(03-9). **완료 조건 13은 `Reorder`로 순서를 바꾼 뒤 Tab을 닫았다 여는 것으로 닫힌다.**

### ⑥ 05와의 경계 — **반대 방향에 요구가 둘 있고, 04에 이름이 없다**

**05 문서를 훑었다. 04에 자리를 요구하는 것이 둘이다.**

**ⓐ `UEPEquipSlotWidget::ParentEntryId`** — 05가 명시적으로 요구한다.

> \| **`UEPEquipSlotWidget`에 `ParentEntryId`** (04-4) — `GetEntryInSlot(INDEX_NONE, SlotId)`의 `INDEX_NONE`을 필드로 \| 필드 하나 + 기본값 `INDEX_NONE` \| 무기 부착물 슬롯은 **부모가 무기 `EntryId`일 뿐 같은 위젯**이다. **안 빼두면 위젯을 복제하거나 Step 04를 다시 연다** \|
> — `05_Loot_05_Equipment.md:281`

**05가 *"04-4"* 라고 자리까지 지목했는데 04-4에 그 위젯 클래스가 없다.** 위 ①의 선언에 넣으면 닫힌다.

**ⓑ ★ 장착 강조가 `RarityBorder`와 자리를 다툰다** — 이건 05도 04도 모른다.

```
05:301   "Step 04의 칸 위젯에 장착 표시(테두리 강조)만 추가한다"
04-4     UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> RarityBorder;   // 희귀도 색
04-2     └─ RarityBorder(Image)  ← EEPItemRarity로 색상
```

**테두리가 하나뿐이다.** 05가 *"테두리 강조만 추가"* 하려면 **희귀도 색을 덮거나** 별도 이미지를 새로 얹어야 하는데, 후자는 **04-A가 만든 WBP를 여는 것**이다 — 05가 스스로 *"안 빼두면 Step 04를 다시 연다"* 고 경계한 것과 같은 상황이다.

**처방은 04-4에 한 줄이다.**

```cpp
UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> RarityBorder;    // 희귀도
UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> EquippedBorder;  // ★ 장착 강조. Step 05가 켠다
```

**04-A가 위젯을 만들 때 슬롯을 하나 더 두는 것뿐이고, Step 05는 `SetVisibility` 한 줄이 된다.** 04-분할이 이미 같은 논리를 쓴다 — *"04-A가 칸을 `UUserWidget`으로 만들어야 한다. 안 그러면 04-B에서 전부 다시 만든다."* **같은 문장이 04-A ↔ Step 05 사이에도 필요하다.**

---

## 4-2. 새로 찾은 것 — 7건

### ★★ N-1. 아이콘 비동기 로드 × 풀링 — **`CreateWeakLambda`가 더 이상 막지 못한다** (04-A. 최우선)

**04-4가 이렇게 적었다.**

> *"비동기 로드 콜백은 `CreateWeakLambda`를 쓴다. 로드 도착 전에 창을 닫거나 목록이 다시 만들어지면 일반 람다는 **죽은 위젯**을 건드린다. **매번 칸을 재생성하므로 이 경로가 자주 돈다.**"*

**10차가 그 전제를 바꿨다.** `UDynamicEntryBox`의 `Reset(bDeleteWidgets = false)`는 **파괴가 아니라 회수**다(04-2가 그렇게 골랐다).

| | 04-4가 상정한 것 | 10차 이후 실제 |
|---|---|---|
| 갱신 시 칸 위젯 | **파괴된다** | **풀로 회수됐다가 다른 아이템으로 다시 나온다** |
| `CreateWeakLambda` | 죽었으니 **콜백이 안 돈다** ✅ | **살아 있으니 콜백이 돈다** ❌ |
| 결과 | 안전 | **AK의 아이콘이 붕대 칸에 그려진다** |

**`TWeakObjectPtr`은 *"이 객체가 살아 있나"* 에 답하지 그 위젯이 **아직 같은 아이템인가**에는 답하지 못한다.** 그리고 풀링은 정확히 *"살아 있는 채로 다른 아이템이 되는 것"* 이다.

**증상:** 인벤토리를 열자마자 다른 것을 줍거나 옮기면 **아이콘 몇 개가 서로 바뀐다.** 아이콘이 없는 지금은 안 보이고, **에셋이 채워지는 순간 나타난다** — 그때는 아이콘 파이프라인을 판다.

**처방 둘. 둘 다 한 줄이다.**

```cpp
// ① 람다가 자기가 어느 엔트리를 위한 것인지 기억한다
const int32 RequestedId = Entry.EntryId;
Handle = Streamable.RequestAsyncLoad(Path, FStreamableDelegate::CreateWeakLambda(this,
    [this, RequestedId, Path]()
    {
        if (EntryId != RequestedId) return;      // ★ 회수돼 다른 아이템이 됐다
        ItemIcon->SetBrushFromTexture(...);
    }));

// ② 회수 시점에 취소한다 — UDynamicEntryBox가 그 훅을 갖고 있다
//    UMG/Public/Components/DynamicEntryBox.h:36
//    void Reset(TFunctionRef<void(WidgetT&)> ResetEntryFunc, bool bDeleteWidgets = false)
CellBox->Reset<UEPItemCellWidget>([](UEPItemCellWidget& C){ C.CancelPendingIcon(); });
```

**②가 엔진이 이 문제를 위해 둔 오버로드다** — `Reset`에 per-entry 리셋 훅이 있다. **04-2가 인자 없는 `Reset(false)`만 인용해서 이 오버로드를 못 봤다.**

> **함께 확인한 것:** `FUserWidgetPool`은 *"Slate 인스턴스가 해제되면 활성/비활성 전환에서 `NativeConstruct`/`NativeDestruct`가 불린다"* 고 적어뒀다(`UserWidgetPool.h:15-16`). **조건부라(`bReleaseSlate`) 여기 기대면 안 된다** — `Reset(bDeleteWidgets=false)` 경로에서 `NativeDestruct`가 온다는 보장이 없으므로 **①의 방어를 반드시 넣는다.**

### ★ N-2. 04-3의 `PostReplicatedReceive`가 **13차 이전 코드다**

```cpp
// 05_Loot_04_InventoryUI.md:463   ← 지금
if (Owner) Owner->OnInventoryChanged.Broadcast();

// 05_Loot_03_Inventory.md 03-7   ← 13차 A-4로 고쳐진 것
if (UEPInventoryComponent* C = Cast<UEPInventoryComponent>(Owner))
    C->OnInventoryChanged.Broadcast();
```

**`Owner`는 `TObjectPtr<UActorComponent>`다**(11차). 04-3의 코드는 **컴파일되지 않는다.**

**13차가 A-4로 잡은 바로 그 결함이고, 같은 코드 블록이 두 문서에 있는데 한쪽만 고쳐졌다.** 15차 대조가 `SwapEntries`에 집중하느라 이 블록을 안 봤다.

> **★ 이게 이번 검수에서 가장 반복적인 모양이다.** 같은 코드가 두 문서에 있으면 **반드시 한쪽만 고쳐진다.** 04-3의 그 블록은 **03-7의 내용을 설명하려고 복사한 것**이고 04가 구현할 코드가 아니다. **인용을 지우고 한 줄 참조로 바꾸는 것**을 권한다 — *"`FEPInventoryList::PostReplicatedReceive`가 알림을 쏜다(03-7). **코드는 그쪽에만 둔다.**"*

### ★ N-3. `HotbarRefs`를 Step 04에서 선언할 이유가 **없다**

04-1이 이렇게 적었다.

> *"**핫바 5~0(`HotbarRefs`)** — 필드는 **이 단계에서 선언하지만** 청소는 Step 05다. UI도 Step 05로 미룬다."*

**Step 04에서 그 필드를 읽는 곳도 쓰는 곳도 0이다.**

| | 어디 |
|---|---|
| 쓰는 곳 (배정) | **5~0 드래그 UI = Step 05** (04가 명시적으로 미뤘다) |
| 읽는 곳 (그리기) | **5~0 UI = Step 05** |
| 청소 | **Step 05** (`RemoveSelf` 한 줄) |

**그리고 이걸 Step 03에서 뺀 근거가 그대로 Step 04에도 적용된다.**

> *"지금 넣으면 Step 03 내내 **길이 0인 배열을 도는 루프**가 남아, 8차가 지적한 *'Step 03 내내 항상 거짓인 분기'* 패턴을 하나 더 만든다."*(`EquipmentSlots.md` §4)

**Step 04에서도 길이 6짜리 `INDEX_NONE` 배열이 복제만 되고 아무도 안 본다.** 13차가 `Server_EquipBackpack`을, 14차가 `EP.Inv.Equip`을 죽인 기준(**호출자 0개**)과 같다.

**권고: `HotbarRefs` 선언을 Step 05로 옮긴다.** `EquipmentSlots.md` §4의 표에서 *"`HotbarRefs` 필드 — Step 04"* 를 **Step 05**로. 그러면 **필드·배정·그리기·청소가 전부 한 단계에 모인다.**

> **반대 근거를 검토했다 — `ActiveHotbarIndex`는 왜 Step 03에서 선언했나.** 이유가 명시돼 있다: *"필드가 하나 느는 게 아니라 **`EquippedEntryId`의 이름과 의미가 바뀌는 것**이다."* 즉 **기존 필드의 대체**였다. `HotbarRefs`는 대체할 것이 없는 **순수 추가**라 같은 근거가 없다.

### N-4. `Remainder`의 `NativeOnDrop`은 필요 없다

04-2가 *"`Remainder` — `＋ 남은 용량 13`. 하나. **드롭을 받는다**"* 라고 적었다. **오버라이드가 필요 없다.**

**`Remainder`는 패널 안에 있으므로, 오버라이드하지 않으면 버블링이 패널까지 올라가고 패널이 `{INDEX_NONE, INDEX_NONE}` = 맨 뒤로 처리한다** — 04-1이 *"여백은 버블링으로 패널이 받는다"* 로 이미 확정한 그 경로다. **결과가 같고 코드가 0줄이다.**

**필요한 것은 드롭 수신이 아니라 하이라이트다** — `NativeOnDragEnter`/`Leave`로 초록/빨강을 칠하는 것(04-0). 04-2의 문장을 *"드롭을 받는다"* → **"드래그 중 색이 붙는 대상이다. 드롭은 버블링으로 패널이 받는다"** 로 바꾸면 구현자가 불필요한 오버라이드를 안 만든다.

### N-5. `MakePanel` / `SlotDisplayName` 선언 부재 (요청서 §2-6이 짚은 것 확정)

04-2의 `UEPInventoryWidget` `protected`에 `RefreshEntries`만 있다. 둘을 추가한다.

```cpp
void MakePanel(int32 Container, FText Header);
FText SlotDisplayName(FName SlotId) const;      // §2-6의 TMap을 본다
```

**`MakePanel`이 하는 일도 안 적혀 있다** — `CreateWidget<UEPContainerPanel>(ContainerPanelClass)` → `ContainerColumn->AddChild` → `Rebuild(Inv, Container, Header)`. 세 줄이지만 **`ContainerPanelClass`를 잊으면 구획이 하나도 안 그려진다**고 04-2가 이미 경고한 자리라, 함수 본문이 있어야 그 경고가 걸릴 곳이 생긴다.

### N-6. 완료 조건 4의 *"외투를 입으면 **상의 위에** 끼어든다"* 가 데이터에 안 걸려 있다

`ContainerOrder = ["Coat", "Torso", "Legs", "Back", "Wrist"]`라 **지금은 맞다.** 그런데 **완료 조건이 데이터 값에 의존한다는 것이 문서에 없다** — 누가 `ContainerOrder`를 밸런싱 이유로 재배열하면 **완료 조건이 조용히 거짓이 된다.**

**한 줄이면 된다:** *"이 순서는 `ContainerOrder`가 정한다(04-3). 완료 조건의 '상의 위에'는 그 배열의 첫 원소가 `Coat`이기 때문이고, **배열을 바꾸면 완료 조건 문구도 같이 바꾼다.**"*

### N-7. `UEPItemCellWidget::SetEntry`의 시그니처가 두 곳에서 다르다

```cpp
// 04-4 :503        void SetEntry(const FEPInventoryEntry& Entry);
// 04-2 :400        Cell->SetEntry(E, i, this);            // ← 인자 셋
// 04-8 :909        CellBox->CreateEntry<...>()->SetFromEntry(Id);   // ← 이름도 다르다
```

**셋이 서로 다르다.** `DisplayIndex`와 `OwnerPanel`은 **드롭 라우팅에 반드시 필요하다**(04-1의 `FEPCellHit{ DisplayIndex, EntryId }`, `OwnerPanel->HandleDrop`). **04-4의 1인자 선언이 낡았고, 04-8의 `SetFromEntry(Id)`는 존재하지 않는 함수다.**

```cpp
void SetEntry(const FEPInventoryEntry& Entry, int32 InDisplayIndex, UEPContainerPanel* InOwner);
```

**D-1(13차)과 같은 종류다** — 같은 함수가 절마다 다른 시그니처로 적혀 있고, 구현자는 먼저 읽은 쪽을 따른다.

---

# §5. 작업 목록

> **아래는 제안이다. 적용 여부는 사용자가 결정한다.**

## 04-A 착수 전 (하드)

| # | 무엇 | 어디 |
|---|---|---|
| **1** | **아이콘 로드 콜백에 `RequestedId` 비교 ＋ `Reset(ResetEntryFunc)` 훅** (N-1) | 04-2 · 04-4 |
| **2** | **`NativeOnDrop`은 언제나 `true`** — 라우팅과 판정을 가른다 (§2-1) | 04-1 · 04-7 ＋ 함정 신설 |
| **3** | **04-3의 `PostReplicatedReceive` 인용 삭제** — 03-7 참조 한 줄로 (N-2) | 04-3 |
| **4** | **`UEPEquipPanel` · `UEPEquipSlotWidget` 선언** (＋ `ParentEntryId`, ＋ `EquippedBorder`) (§4-①·⑥) | 04-2 · 04-4 |
| **5** | **`SetEntry` 시그니처 통일** (N-7) | 04-2 · 04-4 · 04-8 |
| **6** | **`UEPSegmentedBar`·`UEPRemainderWidget` API 넷 확정** ＋ `FEPCapacitySegment` (§2-4) | 04-2 |
| **7** | **`MakePanel` / `SlotDisplayName` 선언 ＋ 슬롯 표시값 `TMap`** (§2-6 · N-5) | 04-2 |

## 04-B 착수 전

| # | 무엇 | 어디 |
|---|---|---|
| **8** | **`SwapEntries`를 단일 쓰기 지점 표의 여섯 번째로** ＋ 가드 ＋ `AssignSortKey` 두 번 (§1-3) | 03-2 `:783-784` · 03-7 · 04-7 |
| **9** | **`UEPItemDragOperation` 파생 선언** (§2-2) | 04-7 |
| **10** | **`HandleDrop` 첫 분기에 `&& P->SourceSlotId.IsNone()`** — 핫바에서 본체로 빼면 무동작 (§2-2) | 04-7 |
| **11** | **`FEPSlotHit` ＋ `UEPEquipPanel::HandleDrop`** — 판정 함수가 둘인 이유를 `CanFit`/`CanPlaceInSlot` 선례로 (§4-②) | 04-1 · 04-7 |
| **12** | **`CachedUsed`를 lazy로** (§4-③) | 04-7 |

## 문서 정합

| # | 무엇 |
|---|---|
| **13** | **§1-2의 무해 논증 교체** — 검사 5에 안 기대는 치환 논증으로 |
| **14** | **§3의 근거 교체** — RPC 규칙이 아니라 *"04-B가 이미 이 클래스를 연다"* (03-2 `:968` · 04-7 `:710`) |
| **15** | **`IgnoreEntryId`의 전제 한 줄** — *"그 엔트리가 실제로 그 자리를 떠나는 호출에서만 넘긴다"* |
| **16** | **`HotbarRefs` 선언을 Step 05로** (N-3, `EquipmentSlots.md` §4 표) |
| **17** | **04-2의 가상화 행 추가** ＋ `ReleaseSlateResources` 경고 (§2-5) |
| **18** | **04-8 ②의 범위 한 줄** — 지역 `Ordered` 재배열이고 필드로 남기지 않는다 (§2-3) |
| **19** | **04-A 검증 수단에 `EP.Inv.Reorder`** (§4-⑤) |
| **20** | **`Remainder`는 드롭을 안 받는다** (N-4) / **`FEPCellHit`을 평범한 struct로** (§4-④) / **완료 조건 4가 `ContainerOrder`에 의존한다는 한 줄** (N-6) |
| **21** | **미정 #10에 "이행은 타입 확장"** — `TArray<FName>` → `TArray<FEPBodySlotDef>` (§2-6) |

## 하지 않기로 한 것

- **`CanPlaceInSlot`을 안 부르고 검사 2·3만 쓰기** — 기각 유지 (§1-1 ①)
- **오버로드 / `CanSwapIntoSlot` 별도 함수** — 같은 질문이다 (§1-1 ②)
- **`IgnoreEntryId`를 03-A에서 붙이기** — 04-B가 그 파일을 어차피 연다 (§3)
- **04-8 ①의 *"UI에 순서 자료구조가 없다"* 완화** — 11차가 지운 다섯이 다시 열린다 (§2-3)
- **`UDragDropOperation::Payload` 사용** — `UEPItemInstance`를 UI 전용으로 되살린다 (§2-2)
- **`UDynamicEntryBox` 재검토 / `UListView` 재도입** — 위젯 수가 아이템 수에 비례한다 (§2-5)
- **컨테이너용·슬롯용 `HandleDrop` 합치기** — `Hit`이 다시 두 가지를 뜻한다 (§4-②)
- **04-A/04-B 분할선 변경** — 그대로 맞다 (§3-4)

---

# §6. 인용

| 무엇 | 어디 | 확인한 것 |
|---|---|---|
| `NativeOnDrop`이 `false`면 `Unhandled` | `SObjectWidget.cpp:442-449` | §2-1 ① |
| **라우팅 루프가 `!Reply.IsEventHandled()`** | `SlateApplication.cpp:452` | §2-1 — **`false`는 버블링을 멈추지 않는다** |
| `FBubblePolicy`가 깊은 곳→부모 | `SlateApplication.cpp:385-401` | §2-1 ③ |
| *"extend it to add additional data"* | `DragDropOperation.h:51` | §2-2 — 파생이 의도된 확장 지점 |
| `Payload`가 **`UObject`** ＋ 인벤토리 예시 | `DragDropOperation.h:66-71` | §2-2 — 우리에게 그 `UObject`가 없다 |
| `UDragDropOperation` C++ 파생 **0건** | Engine `Source`+`Plugins` ＋ Lyra 전수 grep | §2-2 — 반례가 아니라 BP 관례 |
| *"list/tile view를 세울 만큼은 아닌 경우에 유용"* ＋ *"가상화되지 않는다"* | `DynamicEntryBox.h:10-14` | §2-5 — **두 문장을 다 인용해야 한다** |
| **`Reset(ResetEntryFunc, bDeleteWidgets)`** 오버로드 | `DynamicEntryBox.h:36-44` | N-1의 처방 ② |
| 풀은 **`ReleaseSlateResources`에서 놓아야 한다** (경고) | `UserWidgetPool.h:17-20` | §2-5 — 직접 드는 대안의 대가 |
| `UDynamicEntryBoxBase`가 그걸 이미 한다 | `DynamicEntryBoxBase.cpp:28-33` | §2-5 |
| 회수 시 `NativeConstruct`/`Destruct`는 **조건부** | `UserWidgetPool.h:15-16` | N-1 — 여기 기대면 안 된다 |
| `MarkItemDirty`가 `MarkArrayDirty`를 부른다 | `FastArraySerializer.h:441-454` | §1-3 ⓒ |
| `ULyraPawnData : UPrimaryDataAsset` | `LyraPawnData.h:25-53` | §2-6 — 미정 #10의 최종 자리 |
| 핫바 1~4의 **종류 제한** | `EquipmentSlots.md` §1-1 | §1-2 ② — 구급상자는 `Hotbar1`에 못 간다 |
| `HotbarRefs`를 Step 03에서 뺀 근거 | `EquipmentSlots.md` §4 | N-3 — 같은 근거가 Step 04에도 적용된다 |
| **`SwapEntries`도 `AssignSortKey`를 두 번 부른다 / 직접 대입 금지** | `05_Loot_03_Inventory.md:783` | §1-3 ⓐ — 15차 코드가 이걸 어긴다 |
| `MoveEntry`가 `Parent`+`SlotId`의 **유일한** 지점 | `05_Loot_03_Inventory.md:784` | §1-3 ⓑ |
| *"`SwapEntries`는 자동으로 옳다 — 다섯을 경유한다"* | `05_Loot_03_Inventory.md` 03-7 | §1-3 ⓒ — **거짓이 된다** |
| 키 공간은 **부모 전체**(슬롯 포함) | `05_Loot_03_Inventory.md` 함정 4q | §1-2 — 치환 논증의 전제 |
| `Server_MoveEntry`를 안 여는 근거가 **조작된 클라** | `05_Loot_03_Inventory.md:1205` | §3-1 — 규칙의 근거가 공격 표면이다 |
| `UEPEquipSlotWidget::ParentEntryId` 요구 | `05_Loot_05_Equipment.md:281` | §4-⑥ ⓐ |
| *"칸 위젯에 장착 표시(테두리 강조)만 추가"* | `05_Loot_05_Equipment.md:301` | §4-⑥ ⓑ — `RarityBorder`와 다툰다 |
| 04-A가 칸을 `UUserWidget`으로 만들어야 한다 | `05_Loot_04_InventoryUI.md` 04-분할 | §2-4 ⓑ — 같은 문장이 게이지에도 필요하다 |
| 소스 3종 직독 | `EPInventoryComponent.h`(131줄) · `.cpp`(189줄) · `EPInventoryTypes.h` | `CanPlaceInSlot`이 3인자인 것 확인 |
