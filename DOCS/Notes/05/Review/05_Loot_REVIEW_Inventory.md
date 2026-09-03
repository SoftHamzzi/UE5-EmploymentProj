# 검토 — `05_Loot_03_Inventory.md` (03-A 착수 직전)

> 작성일: 2026-08-25
> 대상: `DOCS/Notes/05/05_Loot_03_Inventory.md` (2062줄, 12차 검수까지 반영) ＋ `05_Loot_03_Inventory_STATUS.md`
> 대조: `Public/Inventory/EPInventoryTypes.h` · `EPInventoryComponent.h` · `Private/Inventory/EPInventoryComponent.cpp` (골격, 로직 0줄)
> 시점: 설계 검수 12회 / 실행 검증 0회. **다음 행동이 03-A 구현**이라는 전제로 본다
>
> 이 문서는 **검토 기록**이다. 확정은 `LOOT_STATUS.md`, 설계 반영은 `05_Loot_03_Inventory.md`에 한다.

---

## 결론 요약

**문서의 결정들은 옳다.** 12차까지의 판정(희소 `SortKey` / `bIsRoot` private / 루트만 칸 검사 / RPC 표면 미개방)은 전부 유지한다. 문제는 **결정이 아니라 결정 사이를 잇는 자리**에 몰려 있고, 그 자리는 대부분 **9차(`SlotId` ＋ `MoveEntry`)와 11차(`SortKey`)가 나중에 들어오면서 앞 절이 갱신되지 않은 지점**이다.

| 구분 | 건수 | 성격 |
|---|---|---|
| **A. 구현하면 즉시 깨진다** | 5 | 컴파일 에러 · 조용한 무동작 · 자동 착용 불가 |
| **B. 조용히 틀린다** | 4 | 센티널 충돌 · 미정의 순서 · 게이트 누락 |
| **C. 검증 도구·구간 구조** | 4 | 완료 조건 19개 중 3개를 Step 03에서 확인 불가 ＋ 구간 의존 1건 |
| **D. 문서 정합** | 8 | 예시가 최근 확정(§4-6)과 충돌 · 죽은 참조 · STATUS 미갱신 |
| **E. 문제 없음(확인함)** | 5 | 의심스러워 보이나 정상 |

> **C-4와 D-8은 반영 도중에 나왔다** — 앞의 결정을 문서에 적용하다 드러난 것이라 검토 시점에는 안 보였다.

**A-2 · A-3은 뿌리가 하나다** — *"`AddSubtree`의 루트가 슬롯에 대해 무엇을 하는가"* 가 문서 어디에도 없다. 이 하나가 03-B(자동 착용)와 03-C(되줍기) 양쪽을 동시에 막는다.

**착수 순서 권고:** A 다섯 개를 문서에 반영한 뒤 03-A를 시작한다. B·C는 03-A 코드에 닿지 않는 것이 섞여 있으므로 해당 구간 직전에 처리해도 된다. D는 언제 해도 된다.

---

## A. 구현하면 즉시 깨지는 것

### A-1. `MoveEntry`의 쓰기 블록에서 `E`가 어디서 오는지 정의돼 있지 않다 — 그대로 쓰면 **아무 일도 안 일어난다**

**위치:** 03-2 *"★★ 검사를 다 통과한 뒤"* (`:813-822`)

```cpp
// 전부 통과한 뒤에 쓴다 (검사 도중 쓰기 금지 — 함정 11b)
const int32 OldParent = E.ParentEntryId;      // ← E가 선언된 적이 없다
E.ParentEntryId = NewParent;
E.SlotId        = NewSlotId;
Entries.MarkItemDirty(E);
```

이 문서는 **`FindEntry`가 값 복사**라고 두 곳에서 못박았다(03-2 *"★ 포인터가 아니라 값으로 돌려준다"*, *"★ 엔트리 포인터를 밖으로 내보내지 않는다"*). 그런데 같은 함수의 **검사 6 예시가 `FEPInventoryEntry E;` ＋ `FindEntry(P, E)`로 지역 복사본을 `E`라는 이름으로 만든다.** 위 블록을 그 아래에 그대로 이어 붙이면:

- `E`는 **부모 사슬을 걷다 마지막으로 방문한 조상의 복사본**이다
- 대입은 복사본에만 닿고, `MarkItemDirty(E)`는 **배열 밖 임시 객체의 `ReplicationID`를 건드린다**
- **컴파일된다. `MoveEntry`는 `true`를 반환한다. 배열은 그대로다**

**증상:** 배낭을 매도 아무 일이 없다. 무기를 꽂아도 안 꽂힌다. `EP.Inv.Dump`의 `Parent`/`SlotId` 열이 안 바뀌므로 *"`Server_EquipBackpack`이 안 불린다"* 로 오진하고 RPC 쪽을 판다.

**같은 문제를 `SetEntryCharges`는 이미 옳게 보여준다** — `for (FEPInventoryEntry& E : Entries.Items)`로 **참조**를 잡는다(03-3). `MoveEntry`만 그 한 줄이 빠져 있다.

**고칠 방향:** `MoveEntry` 블록을 `SetEntryCharges`와 같은 형태로 통일한다. 검사 6의 사슬 탐색 변수는 `E`가 아닌 이름(`Cur`)으로 바꿔 **같은 이름이 두 의미를 갖지 않게** 한다.

> 함정표에 넣을 것: **`MoveEntry`가 `FindEntry`의 복사본에 쓴다** — *"장착이 통째로 무동작인데 반환값은 `true`"*.

---

### A-2. `AddSubtree`가 **루트의 `SlotId`** 를 어떻게 다루는지 정의돼 있지 않다

**위치:** 03-2 `RemoveEntryInternal` ② (`:660` 부근) ＋ 03-4 `AddSubtree` (`:1495` 부근)

```cpp
// RemoveEntryInternal ② — 루트 정규화
if (bIsRoot) { Snapshot.ParentEntryId = INDEX_NONE; Snapshot.SortKey = 0; }
//                                                   ↑ SlotId는 건드리지 않는다

// AddSubtree — 삽입
const int32 NewId = InsertEntry(NewParent, Src.ItemId, Src.State, Src.SlotId);
//                                                                 ↑ 루트의 SlotId가 그대로 들어간다
```

문서는 *"★ `bIsRoot`가 **두 필드**를 동시에 관장한다"* 고 적었다. **셋이어야 한다.** 지금 상태에서 **매고 있던 배낭을 버렸다 되주우면** 이렇게 된다.

| `AddSubtree`의 `Container` | 결과 | 판정 |
|---|---|---|
| `INDEX_NONE` (본체) | `Parent=-1, SlotId="Back"` → **자동으로 매진다** | 우연히 맞다. 그래서 완료 조건 9가 통과한다 |
| 이미 다른 배낭을 매고 있음 | `"Back"` 슬롯에 **엔트리가 둘** | `GetEntryInSlot`이 먼저 찾은 것을 돌려준다 → **유령 배낭** |
| 배낭Id (본체가 꽉 참) | `Parent=배낭, SlotId="Back"` | **정확히 함정 4i의 상태다** — 칸도 안 먹고 착용으로 잡힌다 |

**세 번째 줄이 핵심이다.** 함정 4i(*"가방 안에 든 상의를 입고 있다"*)를 막으려고 `MoveEntry`에 검사 3(정합 불변식)을 넣었는데, **`AddSubtree`는 `MoveEntry`를 거치지 않는다.** 검사 3을 우회하는 경로가 문서 안에 이미 있었던 것이다.

**그리고 동작이 내용물 유무로 갈린다.**

```cpp
if (In.Num() == 1) return AddItem(Container, In[0].ItemId, In[0].State);   // ← SlotId를 버린다
```

- **빈 배낭**을 버렸다 주우면 → `AddItem` → `SlotId = NAME_None` → **안 매진다**
- **내용물이 든 배낭**을 버렸다 주우면 → 위 경로 → `SlotId = "Back"` → **매진다**

같은 조작이 안에 뭐가 들었느냐로 갈린다. 무기(`SlotId="Hotbar1"`)도 같다.

**고칠 방향 — 두 안이 있고 A안을 권한다.**

| | A. **루트 정규화가 `SlotId`도 버린다** | B. `AddSubtree`가 루트를 슬롯째 복원한다 |
|---|---|---|
| 바꾸는 곳 | `if (bIsRoot) { Parent=NONE; SortKey=0; **SlotId=NAME_None;** }` 한 줄 | `AddSubtree`에 슬롯 검증(검사 2·3·4)을 복제 |
| 되줍기 동작 | 컨테이너에 **아이템으로** 들어간다. 착용은 `TryAutoEquip`(①단계)이 한다 | 착용 상태가 그대로 복원된다 |
| 검사 3 우회 | **사라진다** — 슬롯에 들어가는 경로가 `MoveEntry` 하나로 유지된다 | 남는다. 검증이 두 곳으로 갈린다 |
| 03-4와의 관계 | ① `TryAutoEquip` → ② 컨테이너 라는 **이미 있는 2단계**가 그대로 답이다 | 2단계가 ①을 건너뛰는 세 번째 경로가 된다 |

**A안이면 03-4의 흐름이 저절로 옳아진다** — 되주울 때 착용 여부는 *"그때 슬롯이 비어 있는가"* 로 결정되고, 그게 `TryAutoEquip`의 정의다. 문서가 이미 *"★ 루트에만 `SlotId`를 세팅한다"*(03-2 `TryAutoEquip` 주석)고 적어둔 것과도 맞는다.

> `RemoveEntry` ↔ `AddSubtree` 계약 문단(`:683`)의 인용문도 함께 고친다:
> *"루트의 `ParentEntryId`는 `INDEX_NONE`으로 정규화되며 `SortKey`는 0으로 버려진다"* → **`SlotId`는 `NAME_None`으로 버려진다**를 추가.

**★ 확정 (2026-08-25): A안.** 사용자 판정 — *"Back이 비어 있으면 매지고, 차 있으면 아이템으로 들어가고, 아이템 자리도 없으면 못 줍는다."* 이 셋은 전부 `TryAutoEquip` ①단계 → 컨테이너 ②단계라는 **이미 있는 흐름**이고, 스냅샷이 개입할 자리가 없다.

> **안 만들기로 한 확장 하나 — 이름만 남긴다.** *"원래 있던 슬롯을 우선 시도"* (Hotbar2에서 버린 무기가 Hotbar2로 돌아온다). `RemoveEntry`가 `SlotId`를 **호출자에게 따로 알려주고** `TryAutoEquip(In, FName PreferredSlot)`이 그것부터 훑는 형태다. **A안이 이 확장을 막지 않는다** — 힌트일 뿐 판정은 여전히 `TryAutoEquip`이 하므로 검증 우회가 안 생긴다. 지금은 소비자가 없고 기획서에 이름도 없어 만들지 않는다 (CLAUDE.md §2).

---

### A-3. `TryAutoEquip`이 **본체 칸에 막힌다** — 본체가 꽉 차면 배낭을 못 맨다

**위치:** 03-6 (`:1806` 부근)

```
줍는다 → TryAutoEquip
    GetEntryInSlot(INDEX_NONE, S) == INDEX_NONE  → AddSubtree 후 루트에 MoveEntry(id, -1, S)
```

`AddSubtree`가 어느 컨테이너로 가는지 안 적혀 있는데, 검사 3이 **몸 슬롯은 `ParentEntryId == INDEX_NONE`** 을 요구하므로 **본체를 경유할 수밖에 없다.** 그러면 `AddSubtree` → `CanFit(INDEX_NONE, 배낭)` 을 지난다.

```
본체 10칸 중 8칸 사용, 배낭(SlotSize 9)을 줍는다
  ① TryAutoEquip → AddSubtree(본체) → CanFit: 8 + 9 = 17 > 10  →  실패
  ② GetInsertionOrder() → 본체 실패, 매고 있는 배낭 없음      →  실패
  ⇒ "가방에 자리가 없습니다"
```

**등이 비어 있는데 배낭을 못 맨다.** 그리고 §4-6이 `ContainerCapacity < SlotSize`를 확정했으므로 **쓸 만한 배낭일수록 `SlotSize`가 커서 더 자주 걸린다.**

**완료 조건 7은 통과한다** — 빈 인벤토리에서 시작하기 때문이다. 실제 플레이에서만 난다.

#### ★★ 그리고 이건 버그가 아니라 **전제가 무너진 것**이다 (2026-08-25)

본체 경유는 *"본체에 칸이 좀 남아 있다"* 를 깔고 있다. **그 전제가 곧 사라진다.**

> **사용자 확정:** *"나중에 아무것도 안 입은 상태에선 슬롯만 있고 따로 인벤토리가 없을 것 같다."*

즉 `MaxSlots = 10`은 **임시 값**이고, 최종적으로는 수납 용량이 전부 **착용한 컨테이너(상의·조끼·배낭)에서** 나온다. 본체 용량이 0이 되면 `CanFit(INDEX_NONE, …)`이 **항상 거짓**이므로:

| | 지금 | 본체 0칸이 되면 |
|---|---|---|
| 본체 경유 자동 착용 | *"본체가 차면 가끔 안 매진다"* | **영영 아무것도 자동 착용되지 않는다** |

**문서에 이 확장점의 이름이 반쯤 있다** — 03-2 `GetInsertionOrder()` 주석: *"**본체 10칸이 사라져도 이 함수의 데이터만 바뀐다**"*. 그러나 *"사라진다"* 가 **0칸인가 슬롯만 남는가**는 적혀 있지 않다. **§8 미정에 항목으로 올린다** — 안 그러면 CLAUDE.md §2 기준으로 "상상한 확장점"과 구분되지 않는다.

> **`MaxSlots` 필드는 남긴다.** 0을 넣으면 `GetCapacity(INDEX_NONE)`이 0을 돌려주고 나머지 코드는 그대로 돈다. 필드를 없애면 `GetInsertionOrder`의 맨 앞 `INDEX_NONE`까지 같이 빼야 한다. **지금은 10으로 두고 밸런싱 값으로만 다룬다** — 그러면 이 결정을 지금 안 해도 된다.

#### 고칠 방향 — `AddSubtree`만 위치를 **반쪽으로** 받고 있다

이 설계에서 **위치는 언제나 `(Parent, SlotId)` 쌍**이다. 인접한 두 함수는 이미 둘 다 받는다.

```cpp
int32 InsertEntry(int32 Parent,  FName ItemId, const FEPItemState&, FName SlotId);   // 둘 다
bool  MoveEntry  (int32 EntryId, int32 NewParent, FName NewSlotId);                  // 둘 다
int32 AddSubtree (int32 Container, const TArray<FEPInventoryEntry>& In);             // ← 하나뿐
```

**A-2와 A-3이 둘 다 여기서 터진 것은 우연이 아니다.** *"슬롯 정보를 어디서 얻나"* 에 답이 없으니 한쪽은 **스냅샷에서 몰래 새어 들어오고**(A-2), 다른 한쪽은 **아예 표현이 안 됐다**(A-3).

```cpp
// 권고안 — 목적지를 나란히 받는다. 기본값을 주지 않는다
int32 AddSubtree(int32 Parent, FName SlotId, const TArray<FEPInventoryEntry>& In);
```

| | 뒤에 붙이고 기본값 (`RootSlotId = NAME_None`) | **`(Parent, SlotId)`를 앞에 나란히** |
|---|---|---|
| 읽힘 | *"옵션이 하나 붙었다"* | *"목적지를 받는다"* — `MoveEntry`와 같은 어휘 |
| 빠뜨리면 | **조용히 수납이 된다** (A-3이 그대로 재발) | **컴파일이 안 된다** |
| 호출부 | 대부분 안 씀 | 매번 *"슬롯인가 수납인가"* 를 답한다 |

**기본값을 주면 안 된다.** A-2가 난 원인이 정확히 *"아무도 슬롯을 안 정해서 스냅샷 값이 흘러 들어간 것"* 인데, 기본값은 그 상태를 **다시 문법으로 허용한다.** 이 문서의 기준(*"규율이 아니라 형태로 막는다"*)이 여기서는 **명시 강제**다.

**호출부는 셋뿐이다.**

```cpp
AddSubtree(C,          NAME_None, Payload);   // OnInteract ② — 컨테이너 수납
AddSubtree(INDEX_NONE, S,         Payload);   // TryAutoEquip — 몸 슬롯
AddSubtree(INDEX_NONE, NAME_None, Sub);       // §7-1 월드 컨테이너에서 꺼내기
```

- **칸 검사 분기가 `MoveEntry`와 한 글자도 다르지 않다** — 검사 5가 `NewSlotId == NAME_None`일 때만 도는 그것이다. 새 개념이 아니라 **같은 규칙의 두 번째 적용**이라 §7-3 부착물도 `AddSubtree(총Id, "Optic", Payload)`로 그냥 성립한다
- `TryAutoEquip`이 `InsertEntry`를 직접 쓰는 대안은 **`OldToNew` 재매핑을 복제하게 되므로** 하지 않는다

> **안 깔끔한 부분:** 인자가 셋이 되고 그중 둘이 목적지다. 넷째(§7-1의 `AActor* Source`)가 붙는 날에는 구조체를 받고 싶어질 것이다. **지금은 아니다** — 호출부 셋이 전부 리터럴을 넘긴다.

**★ 확정 (2026-08-25):** 위 시그니처. 근거는 *"인자를 하나 늘린다"* 가 아니라 **"`AddSubtree`만 위치를 반쪽으로 받고 있었다"** 이다.

---

### A-4. 03-7의 `PostReplicatedReceive`가 **컴파일되지 않는다**

**위치:** 03-7 (`:1867` 부근) ↔ 03-2 `Owner` 주석 (`:540` 부근)

```cpp
// 03-2 (11차) — Owner의 타입
UPROPERTY(NotReplicated) TObjectPtr<UActorComponent> Owner;
//  → "델리게이트를 부를 때만 Cast<UEPInventoryComponent>한다"

// 03-7 — 실제 호출부. 11차 이전 그대로다
void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters&)
{
    if (Owner) Owner->OnInventoryChanged.Broadcast();   // ★ UActorComponent에는 그 멤버가 없다
}
```

11차가 `Owner`의 타입을 바꾸면서 **유일한 사용처를 안 고쳤다.** 03-2가 *"`Cast`한다"* 고 말로 적어둔 것이 전부다.

**증상:** 03-A 첫 빌드에서 컴파일 에러. 크지 않지만, **`Entries.Owner = this`가 빠지면 UI가 영원히 안 갱신된다는 ★★ 경고(함정 3d) 바로 옆 자리라** 사소해 보이는 곳에 진짜 문제가 앉아 있다.

**고칠 방향:** 03-7의 코드 블록을 캐스트 형태로 교체한다.

```cpp
if (UEPInventoryComponent* C = Cast<UEPInventoryComponent>(Owner))
    C->OnInventoryChanged.Broadcast();
```

> **현재 코드는 아직 `TObjectPtr<UEPInventoryComponent> Owner`다**(`EPInventoryComponent.h:22`). STATUS의 "골격 결함 #2"가 *"전방 선언 필요"* 라고 적고 있는데, 11차 결정은 *"타입을 `UActorComponent`로 바꿔라"* 다. **STATUS 안에서 같은 줄에 서로 다른 처방이 둘 있다** → D-5.

---

### A-5. `MoveEntry` · `ReorderEntry`에 **스코프 가드가 없다** — 03-7의 열거가 9차·11차 이전 것이다

**위치:** 03-7 (`:1880` 부근)

> *"`AddItem` / `AddSubtree` / `RemoveEntry` / `SetEntryCharges` 선두에 가드를 놓으면 … "*
> *"**`MarkItemDirty`를 부르는 함수는 전부 가드를 지난다**는 규칙 하나로 정리된다"*

**뒤 문장이 규칙이고 앞 문장이 목록인데, 목록이 규칙을 안 지킨다.** 9차의 `MoveEntry`와 11차의 `ReorderEntry`/`AssignSortKey`가 그 뒤에 들어왔다. 그리고 두 함수의 **코드 블록에도 가드가 없다**(`:813` · `:848`) — 구현자가 블록을 그대로 옮기면 알림이 안 나간다.

같은 문단이 *"**`MoveEntry`도 알림을 쏜다.** Step 04가 장비 슬롯 12칸과 장착 강조를 그리므로 … 안 쏘면 배낭을 매도 구획이 안 열리고 무기를 바꿔도 테두리가 안 옮겨간다"* 라고 **증상까지 정확히 적어놓고** 코드에는 안 넣었다.

**증상:** 리슨서버 호스트에서 `EP.Inv.Reorder` / 배낭 착용이 화면에 안 반영된다. 클라이언트는 `PostReplicatedReceive`로 갱신되므로 **호스트에서만 안 되는 버그**가 되어 "복제 문제"로 오진한다.

#### 먼저 — 이 가드가 지키는 것의 크기 (2026-08-25)

가드는 둘을 겸한다.

| | 하는 일 | 빠뜨리면 |
|---|---|---|
| ㉠ **배칭** | 배낭 하나 버릴 때 N+2회 나갈 `Broadcast`를 1회로 | **알림이 아예 안 간다** ← A-5가 이것 |
| ㉡ **재진입 방지** | `Entries.Items` 순회 중 구독자가 배열을 못 건드린다 | 순회 중 재할당 → 댕글링 |

**그런데 서버 쪽 `Broadcast`의 소비자가 사실상 없다.** 클라 UI는 `PostReplicatedReceive`로 받고(가드와 무관), 데디케이티드 서버에는 UI가 없다. **㉠이 지키는 것은 리슨서버 호스트 / PIE 화면 하나**이고, 12회 `Broadcast` = 20칸 그리드 재생성 12회라 **실측하면 비용이 아니다.**

> **즉 실무 기준으로는 배칭 자체가 없어도 됐다.** 그럼에도 가드를 걷어내지 않는 이유는 ㉠이 아니라 **㉡** 이다 — `RenormalizeSortKeys`가 `Entries.Items` 포인터를 들고 도는 자리가 실재한다.

#### ★ 확정 (2026-08-25) — **이 문서가 이미 가진 표를 쓴다**

03-7의 목록이 낡은 진짜 이유는 **그게 이 문서의 다른 표와 따로 노는 두 번째 목록이라서**다.

```
03-7 가드 목록 :  AddItem · AddSubtree · RemoveEntry · SetEntryCharges
단일 쓰기 지점 :  InsertEntry · SetEntryCharges · RemoveSelf · AssignSortKey · MoveEntry
```

**아래가 정확히 "`MarkItemDirty` / `MarkArrayDirty`를 부르는 함수 전부"다.** 그리고 이 표는 **문서가 이미 성실히 관리한다** — 9차가 `MoveEntry`를, 11차가 `AssignSortKey`를 올렸고 *"`SwapEntries`(04-B)도 이걸 두 번 부른다"* 까지 적혀 있다. **놓친 건 이 표가 아니라 03-7의 별도 목록이었다.**

```
㉡ 필수 :  단일 쓰기 지점 다섯   ← 빠지면 알림이 안 간다
㉠ 선택 :  공개 진입점            ← 빠지면 알림이 여러 번 간다 (03-7이 이미 "무해"로 판정)
```

- `ReorderEntry`는 **가드가 필요 없다** — `AssignSortKey`가 갖는다
- `MoveEntry`는 **필요하다** — `MarkItemDirty`를 직접 부른다
- `SwapEntries` · ⓐ의 재장전 · §7-3 부착물은 **자동으로 옳다** — 전부 다섯을 경유한다

**새 규칙이 0개다.** 03-7을 *"단일 쓰기 지점 표의 다섯이 가드를 갖는다. 공개 진입점의 가드는 배칭이다"* 로 다시 쓰면 **지킬 표가 하나로 합쳐진다.**

> **기각한 안 — `MarkItemDirty`를 감싼 사설 래퍼(`DirtyItem`/`DirtyArray`).** 확장성은 가장 좋지만 **엔진 관례와 싸운다** — `MarkItemDirty`는 UE 표준 이름이라 문서·샘플·Lyra·자동완성이 전부 그것을 가리키고, *"우리 프로젝트에선 래퍼를 쓰세요"* 는 실무에서 가장 잘 안 지켜지는 종류의 규칙이다. 게다가 `Entries`가 같은 클래스 멤버라 **강제할 문법이 없다** — 신호가 잘 보일 뿐 여전히 규율이다. **없어도 되는 어휘를 하나 만드는 안이었다.**

---

## B. 조용히 틀리는 것

### B-1. `KeySpace_NextAbove`의 실패 센티널 `INDEX_NONE`이 **유효한 `SortKey` 값과 충돌한다**

**위치:** 03-2 (`:930` 부근)

```cpp
int32 KeySpace_NextAbove(int32 Container, int32 Key, int32 Exclude) const;  // 없으면 INDEX_NONE
...
const int32 NextKey = KeySpace_NextAbove(Container, PrevKey, EntryId);
const bool  bTail   = (NextKey == INDEX_NONE);
```

**`SortKey`는 음수가 될 수 있다.** 맨 앞 이동이 `KeySpace_Min(Container) - SortKeyStep`이므로 0 → −65536 → −131072 … 로 내려간다(문서가 함정 4r에서 직접 인정한 성질이다). 그리고 **−1은 도달 가능한 키다.**

```
(−65536, 0) 구간에 연속으로 꽂는다
  −32768, −16384, −8192, −4096, −2048, −1024, −512, −256, −128, −64, −32, −16, −8, −4, −2, −1
                                                                                        ↑ 16회째
```

**완료 조건 18의 ①(*"같은 틈에 16회 삽입 → 이분 고갈"*)이 그 구간에서 돌면 정확히 이 값을 만든다.**

키가 −1인 형제가 생기면 `KeySpace_NextAbove`가 그를 찾아도 **호출자는 "다음이 없다"로 읽는다** → `bTail = true` → `NewKey = PrevKey + SortKeyStep`. 즉 **바로 뒤에 놓으라고 했는데 한 칸 건너뛴 자리에 놓이고**, 그 자리에 이미 다른 키가 있으면 **동률**이 난다. 그리고 고갈 판정(`bNoGap`)이 `bTail` 때 꺼지므로 **재정규화도 안 걸린다.**

**같은 모양이 하나 더 있다** — `KeyOf(EntryId)`의 *"없으면 0"* (`:934`). 0도 유효한 키다. 지금은 모든 호출부가 앞에서 `FindEntry`로 존재를 확인하므로 도달하지 않지만, **네 번째 읽기 지점이 생기면 조용히 틀린다.**

**고칠 방향:** 값과 실패를 같은 채널로 돌려주지 않는다.

```cpp
bool KeySpace_NextAbove(int32 Container, int32 Key, int32 Exclude, int32& OutKey) const;
bool KeyOf(int32 EntryId, int32& OutKey) const;
```

> 이 문서가 `AddItem`에 대해 내린 판정(*"★ `INDEX_NONE`은 truthy다 … `EntryId`를 돌려받는 이득의 대가로 붙는 함정"*)과 **같은 종류인데, `EntryId`는 1부터라 0/−1이 절대 안 나오고 `SortKey`는 나온다.** 그 차이가 문서에 적혀 있지 않다.

---

### B-2. `InsertEntry`가 키를 발급할 때 **자기 자신이 이미 배열에 들어가 있다**

**위치:** 03-3 `InsertEntry` (`:1225` 부근) ＋ `KeySpace_NextAtEnd` (`:1245` 부근)

```cpp
FEPInventoryEntry& E = Entries.Items.AddDefaulted_GetRef();
E.EntryId       = NewId;
E.ParentEntryId = Parent;          // ★ 이 시점에 새 엔트리가 이미 키 공간의 형제다
...
E.SortKey = KeySpace_NextAtEnd(Parent);   // ← Max를 구할 때 자기(SortKey=0)를 센다
```

`KeySpace_NextAtEnd`는 *"부모가 같은 것 전부"* 를 도므로 **아직 값이 안 들어간 자기 자신(`SortKey = 0`)을 포함한다.** 결과가 셋 갈린다.

| | 명세대로(자기 제외) | 지금 코드대로(자기 포함) |
|---|---|---|
| **빈 컨테이너의 첫 아이템** | `bAny == false` → **0** | `Max = 0` → **65536** |
| 키가 전부 음수인 컨테이너 | 실제 최대 + Step | **65536** (실제 최대와 무관) |
| `if (!bAny) return 0;` | 도는 분기 | **죽은 분기** — `bAny`는 항상 참이다 |

**순서가 깨지지는 않는다**(65536은 어떤 음수보다 크다). 그러나 **명세와 코드가 서로 다른 동작을 말하고 있고, 문서의 예시가 그중 한쪽 편을 들고 있다.**

```
03-9 EP.Inv.Dump 예시
  1   -1   Bandage          ...   SortKey 65536     ← 첫 아이템인데 0이 아니다
  4    3   Weapon_AK74      ...   SortKey 65536     ← 배낭의 첫 자식인데 0이 아니다
```

**예시가 "자기 포함" 동작을 그리고 있다.** 어느 쪽이 의도인지 문서가 답하지 않는다.

**고칠 방향:** `InsertEntry`가 **키를 먼저 구하고 그 다음에 배열에 넣는다.** 한 줄 순서 바꾸기이고, `if (!bAny) return 0;`이 살아나며 예시도 `0 / 65536`으로 자연스러워진다. 부수적으로 *"`Max > MAX_int32 - SortKeyGuard`면 재정규화"* 가 **반쯤 초기화된 엔트리를 재정규화 대상에 넣는 상태**도 사라진다.

---

### B-3. `Server_EquipBackpack`에 `CanMutateInventory()` 게이트가 없다

**위치:** 03-2 (`:454`) · 03-5 (`:1615` 부근) · 03-6 (`:1800` 부근)

03-5는 `CanMutateInventory()`를 *"★ **상태 변경 RPC의 유일한 게이트.** 죽음·시전 확인이 여기 한 곳에 있다"* 로 정의했다. 그런데 **실제로 부르는 곳으로 문서에 적힌 것은 `Server_DropItem` 하나뿐이다.** `Server_EquipBackpack`은 03-6 어디에도 게이트가 없다.

**증상:** 죽은 뒤 / 시전 중에 배낭을 맬 수 있다. 지금은 `State.Dead`를 세우는 경로가 얇아 티가 안 나지만, **사망 시 드랍(§8 미정 #4)이 들어오는 순간 "죽으면서 배낭을 매는" 경쟁이 생긴다.**

**고칠 방향:** 03-6의 `Server_EquipBackpack_Implementation` 첫 줄에 게이트를 명시한다. 그리고 03-5의 *"유일한 게이트"* 문장을 **"모든 `Server_*`의 첫 줄"** 로 바꿔 04-B의 `Server_ReorderEntry`·`Server_MoveEntry`가 자동으로 포함되게 한다 — 지금 문장은 *"게이트가 한 곳에 있다"* 를 말할 뿐 *"모든 RPC가 그것을 지난다"* 를 말하지 않는다.

---

### B-4. `GetOwner<AEPCharacter>()->` 무보호 역참조 — 문서가 §7-1에서 **캐릭터가 아닌 소유자를 이미 허용했다**

**위치:** 03-2 `RemoveEntryInternal` ① (`:625` 부근, 현재 코드 `EPInventoryComponent.cpp:109`)

```cpp
if (UEPCombatComponent* C = GetOwner<AEPCharacter>()->GetCombatComponent())
//                          ↑ AEPCharacter가 아니면 nullptr → 크래시
```

같은 문서 03-2가 *"★ §7-1 월드 컨테이너가 이 두 함수로 **이미** 성립한다"* 고 적었다 — `Container->RemoveEntry(Id, &Sub)`. **월드 컨테이너 액터는 `AEPCharacter`가 아니다.**

지금은 `EntryId == GetEquippedEntryId()`가 컨테이너에서 항상 거짓(`ActiveHotbarIndex == INDEX_NONE`)이라 **우연히 안 죽는다.** 즉 §7-1이 실제로 올 때 *"컨테이너에서 아이템을 꺼내면 서버가 죽는다"* 가 될 수 있는데, 그 시점에는 이 줄이 왜 안전했는지 아무도 기억하지 않는다.

**고칠 방향:** 한 단어다.

```cpp
if (AEPCharacter* Ch = GetOwner<AEPCharacter>())
    if (UEPCombatComponent* C = Ch->GetCombatComponent())
        C->UnequipWeapon();
```

> 이 문서 기준으로는 *"도달 불가 분기의 에러 처리"* 가 아니라 **"나중에 넣기 비싼 계약"** 쪽이다 — 검사 6(사이클)을 5줄과 맞바꾸지 않기로 한 것과 같은 판정.

---

## C. 완료 조건을 검증할 도구가 없다

### C-1. 완료 조건 15 · 17을 **부를 커맨드가 03-9에 없다**

| 완료 조건 | 필요한 호출 | 03-9의 커맨드 |
|---|---|---|
| 15. 다른 컨테이너로 옮기면 목적지 맨 뒤에 붙는다 (함정 4m) | `MoveEntry(id, 배낭, None)` | **없다** |
| 17. 핫바에 꽂았다 빼면 원래 자리로 돌아온다 (함정 4q) | `MoveEntry(id, -1, "Hotbar1")` → `MoveEntry(id, -1, None)` | **없다** |

03-9가 정의하는 것은 `Dump` / `DumpAll` / `Add` / `Drop` / `Reorder` 다섯이다. **`MoveEntry`를 부르는 커맨드가 하나도 없다.** `Server_MoveEntry`는 Step 03에서 안 열기로 했고(옳다), `Server_EquipBackpack`은 `"Back"` 하나만 넣는다.

문서 자신이 `EP.Inv.Equip`을 **두 번 언급**하는데(`:983`, `:1931`) 둘 다 *"04-A가 요구한 것과 같은 이유다"* 라는 **인용**이고, 정의는 `05_Loot_04_InventoryUI.md:45`에 있다 — **04-A가 만드는 커맨드다.**

**그 결과:** 11·12차가 가장 공들인 두 함정(4q 동률, 4m 키 미재발급)이 **Step 04까지 한 번도 실행되지 않는다.** 그리고 4q의 증상은 *"손으로 맞춰둔 배치가 조금씩 무너진다"* 라 **Step 04에서 발견되면 UI 버그로 오진한다.**

**고칠 방향:** 03-9에 커맨드 하나를 추가한다.

```
> EP.Inv.Move <EntryId> <NewParent> <SlotId> [PlayerIndex]    # 서버 전용. SlotId가 "-" 이면 NAME_None
```

- **내부 함수 `MoveEntry`를 직접 부른다** — `EP.Inv.Reorder`가 `ReorderEntry`를 직접 부르는 것과 정확히 같은 형태이고, 같은 이유로 **RPC 표면을 열지 않는다**
- 이걸 넣으면 `EP.Inv.Equip`(04-A)은 이것의 **얇은 별칭**이 되거나 아예 필요 없어진다
- 검사 0~6 일곱 개 전부와 정합 검사(4i)·사이클(4j)도 여기서 처음 실행된다 — **지금은 03-A에 구현하지만 Step 04까지 한 줄도 안 도는 코드다**

---

### C-2. 완료 조건 18의 ②는 **고쳐진 코드에서 재정규화가 돌지 않는다**

**완료 조건 원문:**

> ② **맨 앞으로 20회 반복 이동**(`PrevEntryId = -1`) → 재정규화가 돌고 **서버가 살아 있다.**

**맨 앞 20회로는 경계에 닿지 못한다.**

```
NewKey = KeySpace_Min - SortKeyStep     20회 →  −20 × 65536  =  −1,310,720
bOutOfRange 조건 :  NewKey <= MIN_int32 + SortKeyGuard  =  −2,147,221,504
bNoGap 조건      :  PrevEntryId != INDEX_NONE  →  맨 앞에서는 항상 거짓 (12차가 넣은 그 가드)
```

경계까지 가려면 **32,767회**가 필요하다. 20회에서는 **두 조건 다 거짓 → 재정규화가 안 돈다.**

**테스트가 무가치하지는 않다** — 12차가 고친 무한 재귀(함정 4t)의 회귀 테스트로는 작동한다. 가드를 빼면 `bNoGap`이 항상 참이 되어 `ensure`가 울리거나(`bRetry` 있음) 서버가 멈춘다(`bRetry` 없음). **틀린 것은 *"재정규화가 돌고"* 라는 관찰 문구다.** 고쳐진 코드에서 그 말은 참이 아니고, 검증하는 사람은 *"안 도는데?"* 에서 멈춘다.

**고칠 방향:** 문구를 실제 관찰 가능한 것으로 바꾼다.

> ② 맨 앞으로 20회 반복 이동 → **`ensure`가 울리지 않고 순서가 매번 실제로 바뀐다.** 키는 `−Step`씩 내려간다(`Dump`의 `SortKey` 열). **함정 4t의 회귀 테스트다** — 가드를 잘못 쓰면 여기서 `ensure`가 울리거나(`bRetry` 있음) 서버가 멈춘다(`bRetry` 없음)
>
> ③ **경계 재정규화는 커맨드로 도달할 수 없다**(32,767회). `SortKeyGuard`를 일시적으로 크게 잡아 확인하거나, 확인하지 않고 **①의 이분 고갈 경로 하나로 `RenormalizeSortKeys`가 살아 있음을 증명**한다

---

### C-3. STATUS의 완료 조건 대조표가 **13개** — 11·12차가 추가한 6개가 없다

| | 문서 | STATUS |
|---|---|---|
| 완료 조건 개수 | **19** (§목표) | 표에 **13행** |
| 없는 것 | — | 14(Reorder) · 15(컨테이너 이동) · 16(순서 보존 되줍기) · 17(핫바 왕복) · 18(재정규화 두 경로) · 19(제자리 드롭) |

STATUS의 산문은 *"완료 조건 19개는 다른 단계 두 개 분량이라"* 로 **19를 알고 있다.** 표만 안 자랐다. **CLAUDE.md가 *"진행 상태의 진실의 원천은 STATUS 파일이다"* 로 못박았으므로** 이 표가 곧 체크리스트인데, 여섯 개가 없으면 03-A를 끝내고도 *"다 됐다"* 가 된다.

여섯 개의 담당 구간은 전부 **03-A**다(`ReorderEntry`·`MoveEntry`·키 공간이 전부 03-A). C-1의 `EP.Inv.Move`가 없으면 15·17은 03-A에서 확인 불가이므로 **C-1과 함께 처리해야 한다.**

---

### C-4. 03-B가 03-C의 `AddSubtree`를 부른다 — 구간 의존 (반영 중 드러남)

**`TryAutoEquip`(03-B)이 `AddSubtree`(03-C)를 부른다.** A-3이 `AddSubtree(Parent, SlotId, In)`로 슬롯 경로를 시그니처에 올리면서 드러났을 뿐, 초안의 *"`AddSubtree` 후 루트에 `MoveEntry`"* 때도 있던 의존이다. 그리고 완료 조건 7의 앞 절(*"배낭을 **주우면** 자동으로 매진다"*)은 줍기 경로(`OnInteract` = 03-C)를 요구해 **03-B 단독으로는 검증할 수 없다.**

| 안 | 새 커맨드 | 죽은 코드 |
|---|---|---|
| ㉮ `AddSubtree`를 03-A로 올린다 | **＋1** (`EP.Inv.AutoEquip`) — `TryAutoEquip`의 진짜 호출자도 03-C라 03-B에서 돌리려면 필요하다 | `AddSubtree`가 03-A·03-B 내내 안 돈다 |
| **㉯ `TryAutoEquip`을 03-C로 내린다** | **0** — 03-B는 03-A에 이미 있는 `EP.Inv.Add` ＋ `EP.Inv.Move`로 검증된다 | **없다** |

**✅ 확정 (2026-08-25) — ㉯.** `AddSubtree` · `TryAutoEquip` · `StartingEquipment`가 **03-C로 내려갔다.**

- **㉮는 아무것도 고치지 못한다** — 올려도 `TryAutoEquip`이 03-B에서 호출자가 없어, C-1이 방금 지적한 *"Step 04까지 한 줄도 안 돈다"* 를 한 구간 규모로 재생산한다
- **9차가 다섯 개를 03-A로 올린 것과 방향만 반대고 기준은 같다** — *"호출자와 같은 구간에."* 9차는 **쓰는 쪽이 앞**이었고 여기는 **쓰는 쪽이 뒤**다
- §7-1 월드 컨테이너 · 사망 드랍 · 시체 루팅이 전부 `RemoveEntry` ↔ `AddSubtree` 쌍을 쓴다. 둘을 같은 구간에 두는 것이 그 확장들과도 맞는다

> **03-A의 *"`RemoveEntry`/`AddSubtree` 없이 컴파일된다"* 는 유지된다.** 그리고 `GetCapacity`가 **통째로** 03-A로 왔다 — `EP.Inv.Move`가 컨테이너로 옮기므로 컨테이너 갈래도 03-A에서 실행된다. *"함수를 반만 만든다"* 가 없어졌다.

---

## D. 문서 정합

| # | 무엇 | 어디 | 조치 |
|---|---|---|---|
| **D-1** | **`Server_EquipBackpack`의 `UFUNCTION` 유무가 절마다 다르다.** 03-2는 `void Server_EquipBackpack(int32);` (매크로 없음), 03-6은 `UFUNCTION(Server, Reliable)`. **현재 코드는 03-2를 따라 매크로 없이 들어갔다**(`EPInventoryComponent.h:76`) — 그대로면 **RPC가 아니라 로컬 함수**이고, 클라에서 부르면 클라에서만 돌아 아무 일도 안 일어난다 | 03-2 `:454` / 03-6 `:1800` / 코드 `:76` | 03-2를 `UFUNCTION(Server, Reliable)`로 통일. `_Implementation` 규약도 명시 |
| **D-2** | **`EP.Inv.Dump` 예시가 §4-6의 `ContainerCapacity < SlotSize`를 위반한다.** `Backpack_Small SlotSize 2` ＋ `Backpack(3) : 5 / 12`(용량 12). §4-6대로면 `IsDataValid`가 **거부하는 DT 행**이다. 03-1의 `ParentEntryId` 예시(`Backpack_Small` 안에 3칸＋5칸)도 용량 8 이상을 전제하므로 `SlotSize ≥ 9`가 돼야 한다 | 03-9 `:1913` / 03-1 `:230` | **✅ 수치 확정 (2026-08-25) — 아래 별도 절.** 예시를 그 표로 갈아끼운다. 안 고치면 구현자가 예시대로 DT를 채우고 `IsDataValid`에서 막힌다 |
| **D-3** | **함정표 꼬리 주석이 9차 확정 이전 문장이다.** *"본체 10칸 / 무기 5칸이라 **무기가 배낭에 들어가는 일이 흔하므로 3b는 이론이 아니다**"* ↔ 같은 표의 3b 행: *"**9차 확정으로 '배낭 속 무기 장착'은 표현 불가능해졌다**"* | `:2032` | 주석을 3b가 **여전히 유효한 이유**(착용 컨테이너 중첩 · 미정 #7)로 교체. *"★★ 4건"* 이 어느 넷인지도 명시 |
| **D-4** | **`함정 11b` 참조가 이 문서에 없다.** `:816`이 *"검사 도중 쓰기 금지 — 함정 11b"* 라고 하는데 이 문서의 함정표는 10c에서 끝난다. 11b는 **`05_Loot_04_InventoryUI.md:912`** 의 번호다 | `:816` | *"(`05_Loot_04_InventoryUI.md` 함정 11b)"* 로 출처를 밝히거나 이 표에 자기 번호로 옮긴다. **문서마다 함정 번호가 독립이라 무출처 인용은 죽은 참조다** |
| **D-5** | **STATUS 안에서 같은 줄에 처방이 둘이다.** "골격 결함 #2" = *"`Owner`가 클래스 선언보다 앞 → **전방 선언 필요**"*, 11차 표 = *"`Owner`를 **`TObjectPtr<UActorComponent>`로**"*. 후자가 채택된 결정이고 전자는 그것이 **기각한** 대안이다(03-2 ★ 노트: *"구체 타입으로 두면 전방선언이 필요해지고 … `TArray<class FEPInventoryEntry>`처럼 전방선언 흉내를 내게 된다"*) | STATUS 결함표 #2 | 결함 #2를 *"`TObjectPtr<UActorComponent>`로 교체 ＋ 델리게이트 호출부에 `Cast`"* 로 다시 쓴다. A-4와 같이 처리 |
| **D-6** | **함정표에 `4a`가 없고 정렬이 뒤섞였다.** 4 → 4e → 4f … 4l → **4t → 4u → 4s → 4q → 4r** → 4m → 4n → 4o → 4p → **4k** → 4b → 4c → 4d. 12차가 새 항목을 **본문 등장 순서**로 끼워 넣었다 | `:1980-2033` | 번호순 재정렬. 함정표는 **증상을 겪은 뒤 찾아보는 색인**이므로 순서가 곧 기능이다 |
| **D-7** | **`EP.Inv.Dump`의 *"행은 `SortKey` 순으로 찍는다"* 가 부모를 넘으면 뜻이 없다.** `SortKey`는 형제 스코프인데(03-1) 전역 정렬을 지시하고 있다. 예시 자체도 `65536 / 131072 / 65536`으로 전역 오름차순이 아니다 | `:1918` | **"부모별로 묶고 그 안에서 `SortKey` 순"** 으로 고친다. 원래 의도(*"배열 순서로 찍으면 순서 버그가 안 보인다"*)는 그대로 달성된다 |
| **D-8** | **`DT_Items`의 `Backpack_Small` 행이 §4-6을 위반한다 — 실물 에셋에 이미 들어가 있다.** `SlotSize 2 / ContainerCapacity 12`(`05_Loot_00_ItemCore.md:668`). §4-6이 확정되기 **전에** 적힌 값이고 Step 00은 *"완료"* 로 표시돼 있어 **`DT_Items.uasset`에 그대로 저장돼 있을 것**이다. `IsDataValid()`를 넣는 순간 에디터에서 걸린다 | `05_Loot_00_ItemCore.md:668` · `DT_Items.uasset` | 문서는 `Backpack_B`(`10 / 8`) ＋ `Shirt_Basic`(`11 / 10`) · `Pants_Basic`(`6 / 5`)로 교체했다. **에셋 실물 수정이 남아 있다** — DA 2종(`DA_Shirt_Basic`·`DA_Pants_Basic`)도 함께 |

---

### ★ D-2 확정 — 용량표 (2026-08-25)

| 아이템 | `SlotSize` (차지) | `ContainerCapacity` (제공) | 비고 |
|---|---|---|---|
| **본체** (`INDEX_NONE`) | — | **0** | 테스트 중 `MaxSlots = 10`, **곧 제거한다** |
| 기본 상의 | 11 | 10 | 컨테이너를 **잃을 수 있다** (아래) |
| 기본 하의 | 6 | 5 | 〃 |
| 배낭 A | 15 | 12 | 어디에도 안 들어간다(15 > 12, 15 > 10) — **의도** |
| 배낭 B | 10 | 8 | 상의(10)·배낭A(12)에 들어간다 |

**배낭은 테스트용 2종.** 종류는 계속 는다 — DT 행 추가로 끝난다.

#### ★★ 부등호가 둘이고 서로 다른 식이다 — 헷갈리면 무한 중첩이 열린다

```
넣기 판정   :  SlotSize(넣을 것)  ≤  Capacity(담을 것)     ← ≤ 다. B(10)를 상의(10)에 넣는다
데이터 규칙 :  Capacity(X)        <  SlotSize(X)           ← < 다. 등호를 허용하면 안 된다
```

**아래를 `≤`로 풀면 §4-6의 증명이 무너진다.**

```
SlotSize(A) ≤ Capacity(B) < SlotSize(B)   ⇒  SlotSize(A) <  SlotSize(B)   깊이 유한 ✅
SlotSize(A) ≤ Capacity(B) ≤ SlotSize(B)   ⇒  SlotSize(A) ≤  SlotSize(B)   깊이 무한 ❌
```

`SlotSize 10 / Cap 10`짜리 행 하나면 **그 가방이 자기 안에 들어간다**(10 ≤ 10). 용량이 늘지는 않지만(±0) **깊이가 안 막혀** `RemoveEntry` 재귀·UI 중첩·세이브가 전부 상한을 잃는다.

- **확정한 수치는 전부 `<`를 만족한다** (15>12, 10>8, 11>10, 6>5)
- **`IsDataValid`도 이미 `<`로 짜여 있다** (`Capacity >= SlotSize`면 에러 — `05_Loot_00_ItemCore.md`). **바꿀 코드가 없다**

#### ★ 상의·하의는 컨테이너를 잃을 수 있다 — 그 이행이 **데이터 둘**이다

> **사용자 확정 (2026-08-25):** *"상의 하의는 container는 사라질 수도 있다. 가방과 외투만 남을 가능성도 있다."*

| 손잡이 | 값 |
|---|---|
| `DT_Items` 상의 행의 `ContainerCapacity` | 10 → **0** (§4-6: 컨테이너 = `Capacity > 0`) |
| `UEPLootDeveloperSettings::ContainerOrder` | `["Coat","Torso","Legs","Back","Wrist"]` → `["Coat","Back"]` |

**코드 변경 0.** `GetCapacity`가 0을 돌려주고 `CanFit`이 항상 거짓이 되며, `GetInsertionOrder`에 남아 있어도 그냥 지나간다(두 번째 손잡이는 헛도는 판정을 없애는 것뿐이라 **안 해도 동작이 맞다**). `BodySlots`·검사 3은 **무관** — 슬롯은 남고 착용 효과만 남는다.

> **§4-9의 *"컨테이너 여부를 타입 계층이 아니라 값으로"* 가 값을 하는 자리다.** `UEPClothingDefinition : UEPContainerDefinition` 같은 계층이었으면 클래스를 갈아엎어야 했다.

- **공짜가 아닌 것 하나:** 그때 스폰 직후 수납 칸이 0이 되므로 `StartingEquipment`에 **외투나 배낭이 들어가야 한다.** 그것도 데이터다
- **`SlotSize` 11/6은 "컨테이너인 동안"의 최소값이다.** `Capacity`가 0이 되면 `0 < SlotSize`는 아무 값이나 만족하므로 *"접으면 2칸"* 같은 값으로 내려간다. **지금은 최소값으로 둔다** — 규칙이 자명하고 내리는 건 DT 한 칸이다

#### ★ 새 작업 — 시작 장비 (03-B)

> **사용자 확정:** *"스폰 시 기본 상의와 하의를 입고 시작한다."* 로비가 생기면 거기서 고르지만, **지금은 테스트 환경이라 `UEPLootDeveloperSettings`.**

```cpp
// UEPLootDeveloperSettings — ContainerOrder / BodySlots 옆
UPROPERTY(config, EditAnywhere, Category = "Inventory")
TArray<FName> StartingEquipment;      // ["Shirt_Basic", "Pants_Basic"]
```

캐릭터 `BeginPlay`(서버)에서 `SlotPriority`대로 `TryAutoEquip`을 돌린다. **새 경로가 아니다** — 결정 2로 `AddSubtree(INDEX_NONE, SlotId, In)`가 이미 그 모양이다.

**본체가 10칸인 동안은 급하지 않지만, 0칸이 되는 순간 이게 없으면 아무것도 못 줍는다.** 배낭 자동 착용과 같은 함수를 쓰므로 **03-B 범위**다.

---

## E. 의심스러워 보이나 문제 없음 (확인함)

| 무엇 | 판정 |
|---|---|
| `GetSortedContents` / `RenormalizeSortKeys`가 `TArray<const FEPInventoryEntry*>`를 정렬하면서 람다는 `const FEPInventoryEntry&`를 받는다 | **정상.** `Sorting.h:28,96`의 `TDereferenceWrapper<T*, PREDICATE_CLASS>`가 포인터 배열을 자동으로 역참조한다 |
| `RenormalizeSortKeys`가 `Entries.Items` 포인터를 들고 `AssignSortKey`(→`MarkItemDirty`)를 돈다 | **정상.** `MarkItemDirty`는 `ReplicationID`/`ReplicationKey`만 건드리고 배열을 재할당하지 않는다 |
| `AddSubtree`의 칸 검사가 루트만 | **정확하다.** 함정 10c가 옳고, 늘리면 완료 조건 9가 깨진다 |
| `bIsRoot`를 `RemoveEntryInternal` private로 내린 것 | **옳다.** A-2는 이 분리의 문제가 아니라 **분리된 정규화가 필드 하나를 빠뜨린 것**이다 |
| `EntryId` 단조 증가 / free list 기각 | **옳다.** STATUS의 ABA 논증 그대로. `int32` 유지 |

---

## 반영 순서 제안

**✅ 전부 반영 완료 (2026-08-25).**

| 문서 | 무엇 |
|---|---|
| `05_Loot_03_Inventory.md` (2062 → 2343) | A·B·C 전부 ＋ 체크포인트 재조정(C-4) ＋ 함정 **4v·4w·4x·4y·4z·9f** 신설 ＋ 함정표 번호순 재정렬 ＋ 13차 변경 이력 |
| `05_Loot_DOCS.md` | §4-6에 **본체 0칸 절 · 용량표 · 부등호 둘** 신설, 예시 교체, *"`EntryId`로 정렬"*·*"`SlotId`는 Step 03에서 항상 `NAME_None`"* 두 행 교정(11차·9차 미반영분), §8 확정 3행 ＋ **미정 #8·#9 신설**, §9 DeveloperSettings 필드 3개 |
| `GAME.md` | 인벤토리 절 재작성 — *"본체 인벤토리: 10칸"* → *"몸에는 수납 칸이 없다"*, 컨테이너 중첩 규칙, 장비 절에 의류 슬롯 |
| `05_Loot_00_ItemCore.md` | **D-8** — DT 행 교체 ＋ `Capacity < SlotSize` 규칙 절 |
| STATUS 3종 ＋ `LOOT_STATUS.md` | 완료 조건 **13 → 19행**, 골격 결함 #2 처방 교정, 13차 코드 항목표, 구간표 재조정, 검수 이력 13차 |
| `05_Loot_04_InventoryUI.md` | 본체 0칸 확정 반영(이미 예고돼 있던 자리) |

**남은 것 — 문서가 아니라 에셋이다.**

| | 무엇 |
|---|---|
| ⬜ | `DT_Items.uasset` — `Backpack_Small` 행을 `Backpack_B`(`10 / 8`)로, `Shirt_Basic`·`Pants_Basic` 2행 추가 |
| ⬜ | `DA_Shirt_Basic` · `DA_Pants_Basic` 생성 (`Misc/`) |
| ⬜ | `Backpack_A`(`15 / 12`)는 **03-C 검증 때** — *"A 안에 B는 되고 A 안에 A는 안 된다"* 를 보이는 용도라 그 전에는 소비자가 없다 |

**D-2만 파급이 이 문서 밖으로 나간다.** `Capacity < SlotSize`(§4-6, 2026-08-24)가 03 문서의 용량 숫자를 전부 무효화했고, **본체 0칸 확정(2026-08-25)이 `GAME.md`와 충돌한다.**

| 문서 | 무엇 |
|---|---|
| `05_Loot_03_Inventory.md` | A·B·C·D 전부. **여기부터 확실히 한다** |
| `05_Loot_DOCS.md` | §4-6 부등호 문장(넣기 `≤` / 데이터 `<` 분리) · §7-2 예시 숫자 · **§8 미정 2건 신설**(본체 0칸 / 상의·하의 컨테이너 제거) |
| `GAME.md` | `:152-161` 인벤토리 절 — *"플레이어 본체 인벤토리: 10칸"* 이 **틀린 문장이 됐다** |
| `05_Loot_03_Inventory_STATUS.md` | 완료 조건 19행 · 골격 결함 #2 · `StartingEquipment`(03-B) |
| `05_Loot_04_InventoryUI.md` · `05_Loot_05_Equipment.md` | 용량 예시가 같은 종류로 깨졌는지 **함께 훑는다** |

---

## 결정이 필요한 것 (사용자)

| # | 질문 | 상태 |
|---|---|---|
| 1 | **A-2** — 루트 스냅샷이 `SlotId`를 **버리는가**(A) 슬롯째 복원하는가(B) | **✅ 확정 (2026-08-25) — A안.** 착용 판정은 `TryAutoEquip` 하나. 슬롯 진입 경로가 `MoveEntry`/`TryAutoEquip` 둘로 고정된다 |
| 2 | **A-3** — `AddSubtree`가 슬롯을 어떻게 받는가 | **✅ 확정 (2026-08-25)** — `AddSubtree(int32 Parent, FName SlotId, const TArray&)`. **기본값 없음.** 본체 용량이 0이 될 예정이라 본체 경유는 사양으로 받을 수 없다 |
| 3 | **A-5** — 가드를 어디 두는가 | **✅ 확정 (2026-08-25)** — **단일 쓰기 지점 표의 다섯**이 가드를 갖는다(㉡ 필수). 공개 진입점의 가드는 배칭(㉠ 선택). **사설 래퍼는 기각** — 엔진 관례와 싸우고 강제할 문법도 없다. 03-7의 별도 목록을 없앤다 |
| 4 | **D-2** — 착용 컨테이너 용량표 | **✅ 확정 (2026-08-25)** — 본체 0칸 / 상의 `11-10` / 하의 `6-5` / 배낭A `15-12` / 배낭B `10-8`. 규칙은 **`Capacity < SlotSize`**(등호 없음). 시작 장비는 `StartingEquipment`(03-B). 상의·하의의 컨테이너 제거는 **데이터 둘**로 끝난다 — D-2 확정 절 참조 |

> **결정 1·2가 맞물린다** — A안이 `SlotId`를 스냅샷에서 지우고, 새 시그니처가 그것을 **호출자에게서 명시적으로** 받는다. 슬롯을 채우는 경로가 둘로 고정되고 **둘 다 *"그 슬롯이 지금 비었나"* 를 먼저 본다.**
>
> **파생 작업 2건:** ① `05_Loot_DOCS.md` §8 미정에 *"본체 인벤토리 0칸 — 수납은 착용 컨테이너에서만"* 을 항목으로 올린다. ② 03-2 `GetInsertionOrder()` 주석의 *"본체 10칸이 사라져도"* 에 그 항목을 링크한다.


---

## 후속 — 13차 답변이 잡은 것 (2026-08-25)

`05_Loot_REVIEW13_Request.md`로 검수를 받았다. **21건의 판정과 결정 5건은 전부 유지됐고**, 새 결함 **12건**과 **근거 교정 3건**이 돌아왔다. 전문은 `05_Loot_REVIEW13_Answer.md`.

### 이 문서의 판정 중 **근거가 틀렸던 것** 셋

| | 이 문서가 쓴 근거 | 실제 |
|---|---|---|
| **A-5** 래퍼 기각 | *"엔진 관례와 싸운다 — `MarkItemDirty`가 UE 표준 이름"* | **거짓.** 엔진이 감싼다 — `UAbilitySystemComponent::MarkAbilitySpecDirty`(`AbilitySystemComponent_Abilities.cpp:980`)가 `MarkItemDirty`를 감싸고 헤더가 호출자에게 그걸 쓰라고 지시한다(`:1112`, 네 곳). **결론은 유지, 근거는 *"감싸는 이유는 안에 할 일이 있을 때다"* 로 교체** |
| **D-2** `Cap == SlotSize` | *"무한 중첩 익스플로잇"* | **약하다.** N겹으로 쌓아도 총 용량 순증이 0이라 할 사람이 없다. 근거는 *"비용 0 ＋ 세 곳의 전제 ＋ 되돌릴 손잡이가 이미 이름으로 있다"* |
| **A-3/§2-⑤** `StartingEquipment` | *"6차의 `UDeveloperSettings` 확정과 같은 자리"* | **다른 자리다.** 6차 근거는 *"인스턴스 없이 읽는 소비자가 둘 이상"* 인데 이 필드는 소비자가 하나다. 실제 선례는 `UGameMapsSettings::GlobalDefaultGameMode` 모양이고 최종 자리는 `ULyraPawnData` 형태 → §8 미정 #10 |

### 새로 나온 12건 중 코드에 닿는 것

| | 무엇 |
|---|---|
| **N-1** | **B-2와 같은 결함이 `MoveEntry`에 하나 더 있었다** — 재부모 **뒤에** 키를 구해 자기가 옛 키를 든 채 목적지의 형제로 잡힌다. 키가 컨테이너 사이로 전염된다 |
| **N-7** | **`FScopedInventoryNotify` 정의가 소스에 없다** — `friend` 선언은 불완전 타입이라 인스턴스를 못 만든다. **03-A 첫 빌드에서 막힌다** |
| **N-2** | **`AddSubtree`에 슬롯 검증이 0이다** — *"슬롯 진입 경로가 둘"* 이 API 수준에서 거짓이었고 §7-3이 셋째 호출자다 → `CanPlaceInSlot` 추출 |
| **N-4** | **`SlotSize ≥ 1` 검증이 없다** — 본체 0칸에서 `0 + 0 <= 0`이 참이라 0칸 아이템이 무한히 들어간다 |
| **N-5** | **`EP.Inv.Add`에 컨테이너 인자가 없다** — `MaxSlots = 0` 전환이 03-A 완료 조건 아홉 개를 죽인다 |
| **N-8·N-11** | **이 문서의 D-5 처방이 부분적이었다** — STATUS 스냅샷 자체가 소스보다 뒤였고(결함 #1·#3·#4는 이미 고쳐져 있었다), `class` 키워드가 남아 있다 |
| **N-9** | **`05_Loot_DOCS.md`의 획득 절차가 `TryAutoEquip`·`AddSubtree`·`GetInsertionOrder`를 모른다** — D-2가 *"파급이 문서 밖으로 나간다"* 고 짚은 그 방향인데 이 문서가 안 훑었다 |

### 구간이 한 번 더 바뀌었다

**옛 03-B(배낭)를 없앴다.** C-4가 `TryAutoEquip`을 내려보내고 나니 **남은 새 코드가 `Server_EquipBackpack`(3줄) 하나인데 Step 03에 호출자가 0개**였다 — 9차·11차가 `Server_MoveEntry`·`Server_ReorderEntry`에 두 번 적용한 규칙이 여기만 빠져 있었다. **04-A로 보내고 구간을 둘(03-A 코어 / 03-B 줍기·버리기)로 줄였다.**

### 답변이 틀린 것 하나

완료 조건 18의 경계 도달 횟수를 **32,763회**라 했는데(`65,536 × 32,763 = 2,147,278,848`이라 적었으나 실제는 `2,147,155,968`), 직접 계산하면 **32,764회**다.

```
-65536 × 32763 = -2,147,155,968   >  MIN_int32 + SortKeyGuard(-2,147,221,504)   안 걸린다
-65536 × 32764 = -2,147,221,504   =  MIN_int32 + SortKeyGuard                    걸린다
```

**결론(20회로는 못 간다)은 그대로**이고 문서에는 **32,764**로 적었다. 내가 원래 적었던 32,767도 틀렸다.

### 안 따르기로 한 것

- **이분 중간값의 `int64` 캐스트** — 도달에 6만 회 이상이 필요하고 답변 자신이 선택 사항으로 뒀다
- **함정표 `4a` 결번 채우기** — 색인 기능을 안 해친다
