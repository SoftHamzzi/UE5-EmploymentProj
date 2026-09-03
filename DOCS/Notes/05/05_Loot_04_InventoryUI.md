# Step 04 — InventoryUI (정사각형 격자 + 부피 게이지 + 드래그)

> 마스터 기획: `05_Loot_DOCS.md` (§4-6)
> 선행: `05_Loot_03_Inventory.md` — `OnInventoryChanged` 델리게이트가 있어야 한다

---

## 목표

Tab으로 인벤토리 화면을 열어 착용 슬롯 12칸과 컨테이너 구획들을 보고, **드래그로 정리·착용·이동한다.**

> **★ 2026-08-23에 범위가 커졌다.** 8차까지 이 단계는 *"표시 전용 목록"* 이었다. 9차 기획 확대(슬롯 12개)로 **드래그가 선택이 아니라 필수**가 됐고 — 착용·해제·핫바 배정이 전부 드래그다 — 화면도 목록에서 **정사각형 격자**로 바뀌었다. 결정 근거는 `DOCS/Mine/EquipmentSlots.md`, 레이아웃 판단은 아래 04-0.

**완료 조건**

- [ ] Tab → 인벤토리 화면이 열리고 닫힌다. 마우스 커서 모드 전환
- [ ] 아이템을 주우면 **폴링 없이** 즉시 갱신된다
- [ ] `Icon`이 없는 아이템도 플레이스홀더로 표시되고 이름은 보인다
- [ ] **아무것도 안 입으면 오른쪽이 비어 있다.** 상의를 입으면 상의 구획이, 외투를 입으면 **상의 위에** 외투 구획이 끼어든다
- [ ] **칸 3개인데 게이지가 `7 / 20`이다** — AK(4) + 권총(2) + 붕대(1). 분절 게이지가 아이템별 몫을 보여준다
- [ ] **현금뭉치 둘은 칸 하나에 합산 금액** (`bFungible`). 붕대 3개는 **칸 3개** (스택 없음)
- [ ] **드래그로 컨테이너 A → B 이동이 된다** (배낭 → 외투)
- [ ] **★ 드래그로 A의 아이템과 B의 아이템을 교환할 수 있다** — 양쪽 용량을 **교환 후 상태로** 판정한다 (04-7)
- [ ] **★ 교환이 실패하면 아무 일도 일어나지 않는다** — 한쪽만 옮겨간 중간 상태가 없다
- [ ] **드래그로 착용/해제/핫바 배정이 된다.** 착용 슬롯에 든 것은 **칸을 안 먹는다**
- [ ] **남은 용량보다 큰 아이템을 끌면 `＋ 남은 용량` 블록이 빨갛게 되고 숫자가 `18 → 22 / 20`으로 보인다** — 게이지에도 넘치는 구간이 뜬다
- [ ] **같은 컨테이너 안 순서를 바꾸면 서버가 기억한다** — 창을 닫았다 열어도, 배낭을 벗었다 입어도 그대로다 (04-8)
- [ ] **★ 배낭을 버렸다 다시 주우면 안의 아이템 순서가 그대로다** (Step 03 03-4 · 함정 4n)
- [ ] 항목 우클릭(또는 선택 + G) → 버리기가 동작한다
- [ ] 인벤토리를 연 채로 이동·사격이 된다 (`GameAndUI` 확인)

---

## 04-분할 — 04-A(표시) / 04-B(드래그)

**완료 조건이 15개다.** 8차가 그 숫자를 보고 03을 셋으로 쪼갰고, 여기서도 같은 판단을 한다(10차 검수 §8).

| | 범위 | 완료 조건 | 검증 수단 |
|---|---|---|---|
| **04-A 표시** | 04-0 · 04-1 · 04-2 · 04-3 · 04-4 · 04-6 | 1~6, 13, 14 (8개) | `EP.Inv.Add` / **`EP.Inv.Move`** 디버그 커맨드 (14차) |
| **04-B 드래그** | 04-5 · 04-7 · 04-8 | 7~12, 15 (7개) | 마우스 조작 |

**분할선의 근거는 의존성이다.** 04-A는 Step 03만 있으면 검증되지만, **04-B는 `Server_MoveEntry`·`SwapEntries`가 있어야 한다.** (`ReorderEntry` 내부 함수는 03-A라 이미 있고 `EP.Inv.Reorder`로 계약이 닫혀 있다 — 04-B가 여는 것은 **`Server_ReorderEntry` RPC와 드래그 UI**다.) 즉 04-A가 끝나는 지점이 **RPC 표면이 처음 열리기 직전**이고, 8차·9차가 쓴 기준(*"검증 표면은 소비자를 따라간다"*)이 그대로 분할선이 된다.

> **★ 04-A는 `EP.Inv.Add` / `EP.Inv.Move` 디버그 커맨드로 검증한다.** 드래그가 없으면 착용시킬 수단이 없어 **완료 조건 4(*"상의를 입으면 구획이 뜬다"*)를 검증할 방법이 사라진다.** Step 01·03이 쓴 방식과 같다. 이걸 04-A 범위에 안 적으면 04-A가 *"눈으로만 확인"* 이 된다.
>
> > **★ `EP.Inv.Add`에는 `[Container]` 인자가 있다 (13차, 03-9).** `EP.Inv.Add <ItemId> [Container] [PlayerIndex]` — 기본값은 `-1`(본체)이다. **본체가 0칸으로 전환되면 인자 없는 형태로는 아이템을 하나도 만들 수 없으므로**(`GetUsedSlots(-1) + SlotSize <= 0`이 언제나 거짓) **04-A의 검증 절차는 `EP.Inv.Add Shirt_Basic -1`이 아니라 `EP.Inv.Add <ItemId> <배낭EntryId>` 형태로 적어야 한다.** 04-A는 03-B(`StartingEquipment`)를 선행으로 두므로 **그 시점에는 이미 0칸이다.**
>
> **★★ `EP.Inv.Equip`은 만들지 않는다 (14차).** 13차가 만든 **`EP.Inv.Move <EntryId> <NewParent> <SlotId>`**(03-A)가 이미 그 일을 한다 — `EP.Inv.Move <id> -1 Torso`가 곧 착용이다. 별도 커맨드는 **같은 내부 함수 `MoveEntry`를 부르는 얇은 별칭**이라 어휘만 둘로 늘린다.
>
> > **이것과 함께 `Server_EquipBackpack`도 없어졌다 (14차, `05_Loot_03_Inventory.md` 03-2).** 13차는 그 RPC를 *"`EP.Inv.Equip`이 첫 호출자"* 라는 근거로 04-A에 놔뒀는데, **커맨드는 RPC가 아니라 내부 함수를 직접 부른다**(`EP.Inv.Reorder`→`ReorderEntry`와 같은 형태). 착용의 클라 표면은 **04-B의 `Server_MoveEntry` 하나**다.

> **★ 8차의 03-7 사고와 같은 것이 하나 있다 — 04-A가 칸을 `UUserWidget`으로 만들어야 한다.** 04-B의 드롭이 **칸 위젯의 `NativeOnDrop`에 걸리기 때문**이다(04-1). 04-A에서 칸을 `UImage`＋`UTextBlock` 조합으로 가볍게 만들면 04-B에서 **전부 다시 만든다.** 04-2가 이미 `WBP_ItemCell`로 적었으므로 지금은 맞고, **04-A를 줄이려는 유혹에서 지키기 위해 이유를 여기 적어둔다.**

> **버리기(04-5)를 04-B에 두는 이유:** 우클릭 버리기만 보면 04-A여도 되지만 **드래그 아웃 존**이 04-5에 있고 그건 드래그다. 우클릭 경로만 04-A로 당기면 **04-5가 두 구간에 걸친다** — *"문서 절을 쪼개지 않는다"* 는 8차 원칙(경계에서 stale이 세 번 났다)을 지킨다.

---

## 04-0. 레이아웃 판단 — 왜 정사각형 격자인가

**아이템은 2D 크기를 갖지 않는다.** `FEPItemData::SlotSize`는 `int32` 하나이고(`EPItemData.h:39`) 용량은 **합산**이다(§4-6). 그래서 타르코프식 2D 배치(`FIntPoint Size` + `GridPos` + 회전 + 겹침 검증)로 가지 않는다 — 그건 `FEPItemData`·`FEPInventoryEntry`·`CanFit`·`MoveEntry`·UI가 전부 바뀌는 **Step 03 재작성 규모**다.

**대신 칸 하나 = 아이템 하나로 그리고, 부피(`SlotSize`)는 숫자로 보여준다.**

```
AK-74(4) + 권총(2) + 붕대(1)  →  칸은 3개, 게이지는 7 / 20
```

**자료구조 변경이 0이다.** `SlotSize`·`ContainerCapacity`·`GetUsedSlots`·`CanFit` 전부 Step 03 설계 그대로이고, 밸런스(총이 무겁다)도 유지된다. **바뀌는 것은 이 문서의 위젯뿐이다.**

### ★ 그래서 칸 개수가 가변이다 — 고정 격자를 그리면 안 된다

용량 20짜리 배낭에 들어갈 수 있는 **아이템 개수**는 1개(`SlotSize=20`)부터 20개(전부 1)까지다. **20칸을 미리 그려두면 그림과 게이지가 어긋나고**, 사용자는 *"칸이 17개 남았는데 왜 안 들어가지?"* 를 겪는다.

**격자는 배치가 아니라 나열이다. 진실은 게이지다.** 이 한 줄이 아래 04-2 설계 전체의 근거다.

```
┌─ 배낭 ────────────────────────────────────────┐
│  ████ ██ █ ░░░░░░░░░░░░░              7 / 20 │  ← 분절 게이지
│  AK  권총 붕대                                │
│  ┌─────┬─────┬─────┬───────────────────────┐ │
│  │ AK  │ 권총 │ 붕대 │                       │ │
│  │12/30│     │  ×3 │      ＋ 남은 용량 13    │ │
│  │   ④│   ②│   ①│                       │ │
│  └─────┴─────┴─────┴───────────────────────┘ │
│                        ↑ 하나의 드롭 영역     │
└───────────────────────────────────────────────┘
```

**부피를 세 겹으로 알린다.**

| 장치 | 역할 |
|---|---|
| **분절 게이지** | 어느 아이템이 얼마를 먹는지 한눈에. 칸에 hover하면 해당 구간이 밝아지고 반대도 된다 |
| **칸 우하단 배지 `④`** | 개별 값. 게이지를 안 봐도 알 수 있다 |
| **`7 / 20` 숫자** | 최종 진실 |

**분절 게이지가 이 레이아웃의 핵심이다.** *"칸 3개인데 7"* 이라는 어긋남을 설명하는 장치가 이것 하나뿐이라, 없으면 플레이어가 규칙을 배우지 못한다.

### ★★ 빈 칸을 N개 그리지 않는다 — 개수를 그리는 순간 다시 거짓말이 된다

**10차 검수가 뒤집은 지점이다.** 초안은 *"고정 5열 × 가변 행, 최소 2행"* 이었는데, 그러면 **빈 칸 개수가 레이아웃 부산물인데 플레이어는 그걸 정보로 읽는다.**

```
┌─────┬─────┬─────┬─────┬─────┐
│ AK  │ 권총 │ 붕대 │     │     │   ← 빈 칸 2개가 "2개 더 들어간다"로 읽힌다
└─────┴─────┴─────┴─────┴─────┘      실제로는 붕대라면 13개, AK라면 3개
```

**게이지와 격자가 서로 다른 숫자를 말한다.** 위에서 고정 격자를 배제한 이유가 형태만 바꿔 그대로 남는다.

**그래서 빈 자리를 세지 않고 한 덩어리로 만든다.** 아이템 칸 뒤에 **`＋ 남은 용량 13`** 블록 하나가 붙어 남은 줄을 채운다.

| | 효과 |
|---|---|
| **개수를 말하지 않는다** | 셀 수 있는 것이 없으니 오해할 것도 없다 |
| **드롭 대상이 명시적이다** | *"여기 놓으면 맨 뒤"* 가 그림으로 보인다 — 04-5의 드래그 아웃 존과 같은 원리 |
| **드래그 중 색이 하나에만 붙는다** | 빈 칸 N개가 동시에 빨개지지 않는다 |
| **위젯 수가 아이템 수에만 비례한다** | 04-2의 재생성 비용이 용량과 무관해진다 |

**대신 드래그 중에 살아난다.**

```
AK-74(④)를 끈다 · 남은 용량 13   →  남은 용량 블록 초록 · 숫자가  7 → 11 / 20
AK-74(④)를 끈다 · 남은 용량 2    →  남은 용량 블록 빨강 · 숫자가 18 → 22 / 20  + 게이지에 넘치는 구간
붕대(①)를 끈다  · 남은 용량 2    →  초록 · 18 → 19 / 20
```

**"몇 개 더 들어가나"에는 원래 답이 없다** — 아이템마다 다르기 때문이다. **답이 없는 질문을 그림이 유도하지 않게 하는 것**이 최선이고, *남은 용량 숫자*가 진짜 답에 가장 가깝다. 거절 사유도 *"자리가 없다"* 가 아니라 **"이만큼 모자라다"** 로 보인다.

> **★ 색이 아니라 숫자가 답한다.** 초안은 초록/빨강만 뒀는데, *"17칸 남았는데 왜 안 들어가지"* 를 그 순간에 답하는 것은 색이 아니라 **`7 → 11 / 20`** 같은 미리보기 숫자다. 색은 가부만 말하고 **얼마나 모자란지는 말하지 못한다.**

### 착용 슬롯과의 차이

```
┌─ 착용 ──────────┐   ┌─ 소지품 ───────────────┐
│ ┌──┐┌──┐        │   │ ██████░░░░░░  4 / 10   │
│ │귀││얼굴│       │   │ ┌──┬──┬────────────┐  │
│ └──┘└──┘        │   │ │구급│붕대│＋ 남은 6 │  │
│ ┌──┐┌──┐        │   │ └──┴──┴────────────┘  │
│ │상의││외투│      │   └────────────────────────┘
│ └──┘└──┘        │        ↑ 칸 수는 아이템 수, 이름 없음
│ ┌──┐┌──┐        │
│ │하의││등 │      │
│ └──┘└──┘        │
│ ┌──┐┌──┐        │
│ │팔목││신발│      │
│ └──┘└──┘        │
│ ────────────    │
│ ┌──┬──┬──┬──┐   │   ← 빈 슬롯에 옅은 아이콘을 깔아
│ │ 1│ 2│ 3│ 4│   │      무엇이 들어가는지 보인다
│ └──┴──┴──┴──┘   │
└─────────────────┘
```

**착용 슬롯에는 `SlotSize` 배지를 붙이지 않는다.** 슬롯에 든 것은 칸을 안 먹으므로(`GetUsedSlots`가 `SlotId != None`을 건너뛴다) 숫자를 보이면 **거짓말이 된다.** 이 차이가 *"슬롯은 공짜, 컨테이너는 부피"* 라는 규칙을 시각적으로 가르친다.

> **나중에 §7-1(월드 컨테이너 루팅)이 오면 왼쪽에 열이 하나 더 붙는다** — 타르코프 배치다. 지금 2열로 만들되 착용 열이 가운데로 밀릴 수 있게 잡아둔다.

---

## 04-1. 범위 통제 — 먼저 읽을 것

**드래그가 이번 단계의 본체다.** 기획상 착용·해제·핫바 배정·컨테이너 간 이동이 전부 드래그라 뺄 수가 없다. 대신 **다른 것을 뺀다.**

| 넣는 것 | |
|---|---|
| 정사각형 격자 + 분절 게이지 | 04-0 · 04-2 |
| 착용 12슬롯 (착용 8 + 핫바 1~4) | 04-2 |
| 드래그 — 컨테이너 간 이동 · **교환** · 착용/해제 | 04-7 |
| 같은 컨테이너 안 순서 바꾸기 | 04-8. **서버를 안 거친다** |
| 버리기 (드래그 아웃 + 우클릭) | 04-5 |

| 넣지 않는 것 | 이유 |
|---|---|
| **핫바 5~0 (`HotbarRefs`)** | 필드는 이 단계에서 선언하지만 **청소는 Step 05**다(`EquipmentSlots.md` §4). UI도 Step 05로 미룬다 — 1~4와 의미가 달라 설명 장치가 따로 필요하다 |
| 2D 격자(테트리스) 배치 | 데이터 모델에 2D 크기가 없다. 04-0 |
| 자동 정렬 (이름순/종류순) | 04-8의 로컬 순서 위에 얹으면 되는 순수 추가분이다. **드래그가 먼저 돌아야 한다** |
| 툴팁 / 상세 스펙 패널 | `FEPItemData`에 `Description`이 있으니 나중에 붙이면 된다 |
| 부착물 UI (`Optic` 등) | §7-3. 슬롯 표현은 같지만 **무기 상세 화면**이 따로 필요하다 |
| `UListView` | 아이템 타입이 `UObject*`로 고정돼 있다(`ListView.h:38`). **위젯 풀링은 반대로 넣는다** — 04-2 |

> **★ 8차까지의 *"이 단계는 표시 전용"* 은 더 이상 유효하지 않다.** 그 전제 아래 *"조작은 우클릭/숫자키로"* 라고 적혀 있었는데, 착용 슬롯이 12개가 되면서 숫자키로 감당할 수 없게 됐다. **우클릭과 숫자키는 드래그의 보조 수단으로 남긴다** — 우클릭 = 버리기/사용, 숫자 1~4 = 핫바 전환.

### 드래그 비용을 줄이는 결정 둘

**① 히트 테스트를 직접 짜지 않는다. 위젯 트리가 판정하고, 검증은 한 곳에 모은다.**

초안은 *"칸 위젯은 드롭을 안 받고 격자가 좌표로 판정한다"* 였다. **10차 검수가 뒤집었다** — Slate의 `OnDrop`은 **버블 라우팅**이라 커서 아래 **가장 깊은 위젯부터** 위로 올라가며 불리고 **처음 `Handled()`를 반환한 곳에서 멈춘다.**

```cpp
// Slate/Private/Framework/Application/SlateApplication.cpp:5523
Reply = FEventRouter::Route<FReply>(this, FEventRouter::FBubblePolicy(LocalWidgetsUnderPointer), ...
        const FReply TempDropReply = CurWidget.Widget->OnDrop(CurWidget.Geometry, LocalDropEvent);
// FBubblePolicy: WidgetIndex = Widgets.Num()-1 에서 시작해 --  (:382-406)
// OnDragOver도 같은 정책 (:5827, :5834)
```

**즉 `HitTestCell(Geo, Ev)`은 엔진이 이미 하는 일을 지오메트리 산술로 다시 짜는 것이다.** 그리고 그 산술은 패딩·스크롤 오프셋·DPI 스케일·`UniformGridPanel`의 `SlotPadding`을 전부 스스로 처리해야 한다 — 하나라도 틀리면 **가장자리에서만 어긋나는 버그**가 된다.

**초안이 격자 수신을 고른 근거 둘은 다른 방법으로 해소된다.** 둘 다 *"칸이 검증을 갖는다"* 를 전제했는데, 그럴 필요가 없다.

| 근거 | 해소 |
|---|---|
| *"같은 검증이 N개 위젯에 흩어진다"* | **흩어지지 않는다.** 칸은 **자기 정체만 넘긴다** — 검증은 패널의 `HandleDrop` 한 곳 |
| *"빈 칸에 떨어뜨린 경우를 아무도 안 받는다"* | 그 경우가 **없어졌다**(04-0: 빈 칸을 안 그린다). 남은 용량 블록과 여백은 **버블링으로 패널이 받는다** |

```cpp
// 칸 — 한 줄. 검증 없음
bool UEPItemCellWidget::NativeOnDrop(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* Op)
{
    return OwnerPanel->HandleDrop(FEPCellHit{ DisplayIndex, EntryId }, Op);
}

// 패널 — 남은 용량 블록·여백에 놓은 경우. 버블링으로 여기 온다
bool UEPContainerPanel::NativeOnDrop(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* Op)
{
    return HandleDrop(FEPCellHit{ INDEX_NONE, INDEX_NONE }, Op);   // 맨 뒤
}

// ★ 검증·분기는 여기 한 곳 (04-7)
bool UEPContainerPanel::HandleDrop(const FEPCellHit& Hit, UDragDropOperation* Op);
```

**`HandleDrop`이 유일한 판정 지점이라는 목표는 그대로 지켜지고, 지오메트리 산술이 0줄이 된다.** 덤으로 `NativeOnDragEnter`/`Leave`로 **그 칸만** 하이라이트하는 것이 공짜가 된다 — `OnDragEnter`는 경로의 모든 위젯에 `FNoReply`로 뿌려져 중간에 멈추지도 않는다(`SlateApplication.cpp:5799`). 격자가 좌표로 받으면 하이라이트 대상도 좌표로 다시 계산해야 한다.

**② 검증은 서버 함수를 그대로 부른다.** 드래그 중 유효/무효 표시는 클라가 하지만, 판정식은 **서버와 같은 소스**를 본다. UI에 별도 판정식을 쓰면 *"초록인데 서버가 거절"* 이 난다.

| 목적지 | 부르는 것 |
|---|---|
| 컨테이너(수납) | `CanFit(Container, ItemId)` |
| **슬롯** | **`CanPlaceInSlot(Parent, SlotId, ItemId)`** (13차 신설, 03-2) |

> **★ `SlotPriority`·`BodySlots`를 UI가 직접 읽지 않는다 (13차 반영).** 10차까지 이 줄은 *"판정식은 `CanFit` / `SlotPriority` / `BodySlots`"* 였는데, **13차가 그 둘을 보는 코드를 `CanPlaceInSlot` 하나로 뽑았다.** UI가 `SlotPriority.Contains(SlotId)`를 다시 쓰면 **검사 3(정합)과 검사 4(중복)가 빠져** 함정 12b가 정확히 재현된다 — *"초록인데 서버가 거절"*. 그리고 `BodySlots`는 곧 자리가 바뀐다(§8 미정 #10) — **함수를 부르면 그 이전이 UI에 안 보인다.**
>
> **★★ 단, 아이템이 든 슬롯 위에서는 `CanPlaceInSlot`이 `false`다.** 검사 4가 *"그 자리가 비었나"* 이기 때문이다. 그런데 **그 드롭은 거절이 아니라 교환**이다(04-7). 슬롯 하이라이트를 `CanPlaceInSlot` 하나로 칠하면 **점유된 슬롯이 전부 빨갛게 된다** — 04-7의 네 번째 인자로 푼다.

## 04-2. 위젯 구성

```
WBP_Inventory (UEPInventoryWidget)              ← 화면 전체. 평소 Collapsed
├─ EquipColumn (UEPEquipPanel)                  ← 왼쪽. 고정 12칸
│    ├─ WearGrid  (UniformGridPanel 2×4)        ← 착용 8. 슬롯 이름이 붙어 있다
│    └─ HotbarRow (HorizontalBox ×4)            ← 핫바 1~4
│         └─ WBP_EquipSlot × 12 (UEPEquipSlotWidget)
│              ├─ ItemIcon / EmptyHint (Image)  ← 비면 옅은 부위 아이콘
│              └─ RarityBorder (Image)
└─ ContainerColumn (ScrollBox)                  ← 오른쪽. 구획이 동적으로 들어온다
     └─ WBP_ContainerPanel × N (UEPContainerPanel)
          ├─ HeaderText      (TextBlock)        "배낭"
          ├─ CapacityText    (TextBlock)        "7 / 20"
          ├─ CapacityBar     (UEPSegmentedBar)  ← ★ 분절 게이지 (04-0)
          ├─ CellBox         (UDynamicEntryBox) ← ★ EDynamicBoxType::Wrap. 풀링 내장
          │    └─ WBP_ItemCell × N (UEPItemCellWidget)   ← 아이템 수만큼만
          └─ Remainder       (UEPRemainderWidget) ← ★ "＋ 남은 용량 13". 하나. 드롭을 받는다

WBP_ItemCell (UEPItemCellWidget)                ← 정사각형 한 칸
├─ ItemIcon    (Image)
├─ ChargesText (TextBlock)   ← "12/30" · "×3" · "₩1,240". 0이면 숨김
├─ SizeBadge   (TextBlock)   ← ★ 우하단 SlotSize. 착용 슬롯에는 없다 (04-0)
└─ RarityBorder(Image)       ← EEPItemRarity로 색상
```

**행 위젯(`UEPInventoryRowWidget`)이 칸 위젯(`UEPItemCellWidget`)으로 바뀌었다.** 이름·칸수 텍스트가 빠지고 아이콘 + 배지 + `Charges`만 남는다. 이름은 툴팁(범위 밖)이나 hover 시 하단 바에 띄운다.

```cpp
// 컨테이너 한 구획. 본체·상의·외투·배낭·팔목이 전부 같은 클래스를 쓴다
UCLASS()
class EMPLOYMENTPROJ_API UEPContainerPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    // Container == INDEX_NONE 이면 본체
    void Rebuild(UEPInventoryComponent* Inv, int32 Container, FText Header);

    // ★ 판정은 여기 한 곳. 칸도 남은 용량 블록도 자기 정체만 넘긴다 (04-1)
    bool HandleDrop(const FEPCellHit& Hit, UDragDropOperation* Op);

    // 여백에 놓은 경우 — 버블링으로 여기 온다
    virtual bool NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;
    virtual bool NativeOnDragOver(const FGeometry&, const FDragDropEvent&, UDragDropOperation*) override;

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UDynamicEntryBox>  CellBox;      // ★ Wrap
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UEPRemainderWidget> Remainder;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock>        HeaderText;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock>        CapacityText;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UEPSegmentedBar>   CapacityBar;

private:
    int32 ContainerId = INDEX_NONE;      // 드롭 판정이 이 값을 쓴다
};

// ★ 드롭 지점의 정체. 두 값이 독립이라 하나의 int32로 못 합친다 (04-7)
USTRUCT()
struct FEPCellHit
{
    GENERATED_BODY()
    int32 DisplayIndex = INDEX_NONE;   // 격자에서 몇 번째 자리인가
    int32 EntryId      = INDEX_NONE;   // 그 자리에 아이템이 있으면 그 번호
};

UCLASS()
class EMPLOYMENTPROJ_API UEPInventoryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitWithInventory(UEPInventoryComponent* InInventory);
    void ToggleVisible();

protected:
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget)) TObjectPtr<UEPEquipPanel> EquipColumn;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UScrollBox>    ContainerColumn;

    UPROPERTY(EditAnywhere, Category = "Inventory")
    TSubclassOf<UEPContainerPanel> ContainerPanelClass;

private:
    void RefreshEntries();

    TWeakObjectPtr<UEPInventoryComponent> Inventory;
    FDelegateHandle ChangedHandle;
};
```

> **★ 선언·위젯 트리·구현 셋이 일치해야 한다.** `BindWidget`은 WBP에 같은 이름의 위젯이 없으면 **위젯 블루프린트 컴파일에서 실패**한다. 8차에서 3중 불일치가 한 번 났다(`ItemList`/`CapacityText`를 상위 위젯이 직접 `BindWidget`했는데 트리에서는 구획 안에 있었다). **`ContainerColumn`의 자식은 `BindWidget`이 아니라 런타임 생성이므로 이 함정에 안 걸린다** — 대신 `ContainerPanelClass`를 에디터에서 지정하는 것을 잊으면 구획이 하나도 안 그려진다.

### ★ 구획은 고정이 아니라 매 갱신마다 다시 만든다

착용에 따라 구획이 생기고 사라지므로 `BindWidget`으로 고정할 수 없다.

```cpp
void UEPInventoryWidget::RefreshEntries()
{
    EquipColumn->Rebuild(Inventory.Get());
    ContainerColumn->ClearChildren();

    // ① 본체 — 언제나 있다. ★ 최종 0칸으로 확정됐다 (13차, 05_Loot_DOCS.md §4-6·§8 미정 #9)
    //   테스트 중에만 MaxSlots=10. 0이 되면 GetCapacity가 0을 돌려주고 이 구획은 그냥 안 그려진다
    MakePanel(INDEX_NONE, NSLOCTEXT("EP", "Body", "본체"));

    // ② 착용 컨테이너 — 기획이 정한 순서대로. 외투 → 상의 → 하의 → 배낭 → 팔목
    for (const FName& SlotId : GetDefault<UEPLootDeveloperSettings>()->ContainerOrder)
    {
        const int32 Id = Inventory->GetEntryInSlot(INDEX_NONE, SlotId);
        if (Id == INDEX_NONE)                    continue;   // 안 입었다
        if (Inventory->GetCapacity(Id) <= 0)     continue;   // 신발·귀·얼굴
        MakePanel(Id, SlotDisplayName(SlotId));
    }
}
```

**`if`가 하나도 안 늘어난다.** *"상의를 입으면 구역이 뜬다"* 가 조회 결과로 나오고, *"신발은 구역이 안 생긴다"* 가 `ContainerCapacity == 0`으로 나온다(`EPItemData.h:42`). **부위마다 분기를 쓰면 슬롯을 추가할 때마다 이 함수를 연다.**

```cpp
// UEPLootDeveloperSettings — BodySlots 옆에 둔다. ★ 임시 자리다 (§8 미정 #10)
UPROPERTY(config, EditAnywhere, Category = "Inventory")
TArray<FName> ContainerOrder;   // ["Coat", "Torso", "Legs", "Back", "Wrist"]
```

> **★ 이 필드의 최종 자리는 `UEPLootDeveloperSettings`가 아니다 (14차, `05_Loot_DOCS.md` §8 미정 #10).** 그 클래스의 규칙이 *"에셋 참조와 디버그만 둔다"* 로 명문화되면서, **캐릭터가 무엇으로 이루어졌는가**에 답하는 셋(`BodySlots` · `ContainerOrder` · `StartingEquipment`)이 전부 `UEPPawnInventoryData`(DataAsset)로 갈 자리가 됐다. **Lyra도 게임플레이 구성은 DataAsset이 든다**(`ULyraPawnData : UPrimaryDataAsset` — `LyraPawnData.h:24-53`).
>
> **지금 옮기지 않는 이유는 소비자가 둘뿐이기 때문이다** — `CanPlaceInSlot`의 검사 3과 **이 함수**. 즉 **04-3이 그 두 번째 소비자다.** 옮길 때 고칠 곳은 `GetDefault<...>()->ContainerOrder` 한 줄이고, 이행 트리거는 **로비(직업별 시작 장비)** 또는 **§7-1(월드 컨테이너에는 몸 슬롯이 없다)** 이다.

> **★ `GetInsertionOrder()`(Step 03 03-B)는 이 배열을 *그대로* 반환하지 않는다.** 반환형이 `TArray<int32>`(컨테이너 **EntryId**)이고 `ContainerOrder`는 `TArray<FName>`(슬롯 **이름**)이다. 그리고 **`ContainerOrder`에는 본체가 없다** — 본체는 슬롯이 아니기 때문이다. `GetInsertionOrder()`는 **맨 앞에 `INDEX_NONE`을 붙이고** 그 뒤에 `ContainerOrder`를 이름→`GetEntryInSlot(INDEX_NONE, 이름)`으로 옮긴 것을 잇는다.
>
> ```cpp
> // 지금은 [INDEX_NONE, 배낭Id] — 위 ①과 ②가 그리는 순서와 같은 모양이다
> TArray<int32> Out;  Out.Add(INDEX_NONE);          // ★ 본체가 빠지면 아무것도 안 주워진다
> for (const FName& S : GetDefault<UEPLootDeveloperSettings>()->ContainerOrder)
>     if (const int32 Id = GetEntryInSlot(INDEX_NONE, S); Id != INDEX_NONE) Out.Add(Id);
> ```
>
> **`ContainerOrder`가 공유되는 것은 "순서"이지 "목록"이 아니다.** 표시 순서와 획득 시 채우는 순서가 지금은 같기 때문에 배열 하나를 둘이 본다. *"주울 때 팔목은 마지막에 채운다"* 같은 게 나오면 목록이 둘로 갈리는데, **`GetInsertionOrder()`가 이미 함수라 쪼개는 비용이 0이다** — §7-1이 그 함수를 만든 이유 그대로다.

### 칸 채우기 — 순서는 로컬, 부피는 서버

```cpp
void UEPContainerPanel::Rebuild(UEPInventoryComponent* Inv, int32 Container, FText Header)
{
    ContainerId = Container;
    CellBox->Reset(/*bDeleteWidgets=*/false);          // ★ 파괴가 아니라 풀로 회수 (아래)

    // ★ 표시 순서는 SortKey다 — 서버 발급, 복제됨 (04-8). 정렬 함수는 컴포넌트에 있다
    const TArray<int32> Ordered = Inv->GetSortedContents(Container);   // ★ SortKey 순 (04-8)

    int32 i = 0;
    for (int32 EntryId : Ordered)
    {
        FEPInventoryEntry E;
        if (!Inv->FindEntry(EntryId, E)) continue;

        // ★ NewObject가 아니다 — UDynamicEntryBox가 풀에서 꺼내 준다
        UEPItemCellWidget* Cell = CellBox->CreateEntry<UEPItemCellWidget>();
        Cell->SetEntry(E, i, this);                         // 아이콘 · Charges · SizeBadge · DisplayIndex
        ++i;
    }

    // ★ 빈 칸을 N개 그리지 않는다. 남은 용량 블록 하나가 남은 줄을 채운다 (04-0)
    const int32 Used = Inv->GetUsedSlots(Container);
    const int32 Max  = Inv->GetCapacity(Container);
    Remainder->SetRemaining(Max - Used);

    HeaderText  ->SetText(Header);
    CapacityText->SetText(FText::Format(NSLOCTEXT("EP", "Cap", "{0} / {1}"), Used, Max));
    CapacityBar ->SetSegments(Ordered, Inv, Max);           // ★ 아이템별 구간 (04-0)
}
```

### ★ 위젯을 매번 새로 만들지 않는다 — 엔진이 이 문제에 이름과 도구를 갖고 있다

초안은 *"엔트리가 20~30개니 수동 재생성이 정합적"* 이라고 적었다. **10차 검수가 뒤집었다.** UMG 코어에 정확히 이 문제를 위한 것이 있다.

```
// UMG/Public/Blueprint/UserWidgetPool.h:14
Pools UUserWidget instances to minimize UObject and SWidget allocations
for UMG elements with dynamic entries.
   @see UListView
   @see UDynamicEntryBox
```

**그리고 완성품이 `UDynamicEntryBox`다.**

```
// UMG/Public/Components/DynamicEntryBoxBase.h:23-27
Base for widgets that support a dynamic number of auto-generated entries
at both design- and run-time.
```

| | 초안 | `UDynamicEntryBox` |
|---|---|---|
| 위젯 재사용 | 없음 (매 갱신 `CreateWidget` N개) | **풀링** — `FUserWidgetPool EntryWidgetPool`(`DynamicEntryBoxBase.h:179`) |
| 행·열 계산 | `AddChildToUniformGrid(w, i/C, i%C)` 수동 | 없음 — `EDynamicBoxType::Wrap`이 흘린다(`DynamicEntryBoxBase.h:14-22`) |
| 창 크기 대응 | 열 수 고정 5 | 자동 |
| 비우기 | `ClearChildren()` = 파괴 | `Reset(bDeleteWidgets = **false**)` — 기본이 회수(`DynamicEntryBox.h:51`) |
| 의존성 | — | **UMG 코어다.** CommonUI가 늘지 않는다 |

**고정 5열·최소 2행이 필요 없어진 것이 컸다** — 빈 칸을 안 그리기로 하면서(04-0) 격자가 *"아이템을 흘려 담는 상자"* 가 됐고, 그건 `Wrap`이 하는 일 그대로다.

> **`UUniformGridPanel`을 유지해도 된다** — 그때는 `FUserWidgetPool`을 직접 들고 `Reset` 대신 `GetOrCreateInstance`를 쓴다. **하지 않으면 안 되는 것은 "매 갱신 `CreateWidget` N개"** 다. `UListView`를 뺀 8차 판정은 그대로 유효하지만(`UObject*` 고정), **그때 함께 배제된 것은 아이템 타입이지 풀링이 아니다** — `UListView` 자신도 `FUserWidgetPool`을 쓴다(`ListViewBase.h`).

**`EntryId` 오름차순 정렬이 `GetSortedContents()`로 바뀌었다.** 8차의 정렬은 *"FastArray가 클라 배열 순서를 보장하지 않으니 안정된 기준이 필요하다"* 는 이유였고(`FastArraySerializer.h:54`), **그 이유는 그대로다.** 다만 기준이 `EntryId`가 아니라 복제된 `SortKey`가 되고, **그 키를 `InsertEntry`가 "형제 맨 뒤"로 발급하므로 8차 규칙(새 항목은 끝에 붙는다)이 기본값으로 남는다.** 정렬 함수가 컴포넌트에 있어 **서버도 같은 것을 쓴다**(04-8).

> **★ `UListView`로는 가지 않는다.** 아이템 타입이 `UObject*`로 고정돼 있다.
> ```cpp
> // UMG/Public/Components/ListView.h:38
> class UListView : public UListViewBase, public ITypedUMGListView<UObject*>
> ```
> 도입하면 엔트리마다 UObject 래퍼를 만들어 넘겨야 한다 — **방금 지운 `UEPItemInstance`를 UI 전용으로 부활시키는 것**이고 수명 관리까지 딸려온다. **값 타입 설계와 안 맞는 것은 `UListView`의 아이템 타입이지 풀링이 아니다** — 풀링은 `UDynamicEntryBox`로 받는다(위).

> `GetEntries()`는 Step 03에서 추가하는 읽기 전용 뷰다. **반환 참조는 그 프레임 안에서만 유효하다** — `FindEntry`가 값으로 돌려주는 이유이기도 하다.

---

## 04-3. 갱신은 이벤트로 — 폴링 금지

**항목별 콜백 3종을 쓰지 않는다.** `PostReplicatedAdd`는 **항목마다** 불려(`FastArraySerializer.h:1163`) 한 번의 수신에 목록 재생성이 항목 수만큼 돈다. 대신 직렬화기 쪽 콜백 하나를 쓴다 (Step 03 03-7).

```cpp
// FEPInventoryList
void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters&)
{
    if (Owner) Owner->OnInventoryChanged.Broadcast();     // 수신 1회당 1회
}
```

**Add/Change/Remove를 구분할 필요가 없다** — 어차피 전체 재생성이고, 빠뜨릴 콜백 자체가 없어진다.

> **`Owner`가 세팅돼 있어야 한다.** `UEPInventoryComponent` 생성자의 `Entries.Owner = this;`(Step 03 03-2)가 없으면 콜백이 델리게이트에 닿지 못하고, 증상은 **"서버는 정상인데 클라 UI가 영원히 갱신 안 됨"** 이다. 원인이 UI나 복제로 보여서 엉뚱한 데를 판다.

위젯은 `OnInventoryChanged`를 구독해 `RefreshEntries()`를 부른다.

```cpp
void UEPInventoryWidget::InitWithInventory(UEPInventoryComponent* InInventory)
{
    if (Inventory.IsValid() && ChangedHandle.IsValid())
        Inventory->OnInventoryChanged.Remove(ChangedHandle);      // ★ 리스폰 재바인딩 안전

    Inventory = InInventory;
    if (!Inventory.IsValid()) return;

    ChangedHandle = Inventory->OnInventoryChanged.AddUObject(
        this, &UEPInventoryWidget::RefreshEntries);

    RefreshEntries();          // ★ 구독 직후 현재 상태를 1회 반영
}
```

**`NativeTick`에서 인벤토리를 읽지 않는다.** 인벤토리는 초당 수십 번 바뀌는 값이 아니다. 이건 `UEPHUDWidget`이 어트리뷰트를 구독하는 방식과 같고, `UEPSkillSlotWidget`이 `NativeTick`을 쓰는 것과는 다르다 — 저쪽은 GE 잔여 시간이 매 프레임 변하기 때문이다.

> **구독 직후 1회 반영을 빠뜨리면** 이미 아이템을 들고 있는 상태에서 창을 처음 열었을 때 빈 화면이 나온다. 다음 획득이 일어나야 채워진다. `InitWithASC` 계열에서 반복적으로 나오는 실수다.

> `UnbindAll` 패턴은 `UEPHUDWidget`에 이미 있다. 같은 형태로 `NativeDestruct`에서 해제한다.

---

## 04-4. 칸 위젯

```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPItemCellWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetEntry(const FEPInventoryEntry& Entry);

protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;

    // ★ 드래그 시작만 여기서. 드롭은 격자가 받는다 (04-1 · 04-7)
    virtual void   NativeOnDragDetected(const FGeometry&, const FPointerEvent&,
                                        UDragDropOperation*&) override;

    UPROPERTY(meta = (BindWidget))         TObjectPtr<UImage>     ItemIcon;
    UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock> ChargesText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SizeBadge;    // 착용 슬롯엔 없다
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>     RarityBorder;

    UPROPERTY(EditAnywhere, Category = "Style")
    TMap<EEPItemRarity, FLinearColor> RarityColors;

    // 아이콘이 없거나 로드 전에 쓰는 대체 텍스처
    UPROPERTY(EditAnywhere, Category = "Style")
    TObjectPtr<UTexture2D> PlaceholderIcon;

private:
    int32 EntryId = INDEX_NONE;
    TSharedPtr<FStreamableHandle> IconHandle;
};
```

### 아이콘은 비동기 로드 + 플레이스홀더

```cpp
void UEPItemCellWidget::SetEntry(const FEPInventoryEntry& Entry)
{
    EntryId = Entry.EntryId;

    const FEPItemData* Data = /* DefinitionSubsystem->FindData(Entry.ItemId) */;
    if (!Data) return;

    // ★ 이름은 칸에 안 들어간다 — hover 시 하단 바에 띄운다 (툴팁은 범위 밖)
    // ★ SizeBadge는 컨테이너 칸에만 있다. 착용 슬롯은 칸을 안 먹으므로 숫자가 거짓말이 된다 (04-0)
    if (SizeBadge) SizeBadge->SetText(FText::AsNumber(Data->SlotSize));

    // Charges가 0인 아이템(잡템·퀘스트)은 숫자를 숨긴다
    ChargesText->SetVisibility(Entry.State.Charges > 0
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    ChargesText->SetText(FText::AsNumber(Entry.State.Charges));

    if (RarityBorder)
        if (const FLinearColor* C = RarityColors.Find(Data->Rarity))
            RarityBorder->SetColorAndOpacity(*C);

    ItemIcon->SetBrushFromTexture(PlaceholderIcon);      // 먼저 플레이스홀더
    /* Definition->Icon 비동기 로드 후 교체 — CreateWeakLambda */
}
```

**`Charges`를 표시할 수 있는 것이 이 설계 전환의 직접적 이득이다.** 서버 전용 인스턴스 방식에서는 **가방 속 두 번째 소총의 잔탄을 UI에 표시할 방법이 아예 없었다**(핸들 → 서버 전용 객체 → 클라가 못 읽음). GAS `Ammo` 어트리뷰트는 장착 무기 하나만 커버한다.

**현재 `Icon`이 설정된 아이템은 하나도 없다.** Step 00에서 만든 `DA_AmmoBox_545` / `DA_Bandage` 등도 `Icon`을 비워뒀다. 플레이스홀더 + 이름 표시가 **이번 단계의 실질적 표시 수단**이다.

> 비동기 로드 콜백은 `CreateWeakLambda`를 쓴다. 로드 도착 전에 창을 닫거나 목록이 다시 만들어지면 일반 람다는 죽은 위젯을 건드린다. **매번 칸을 재생성하므로(04-2) 이 경로가 자주 돈다** — 고정 슬롯 방식보다 더 중요하다.

---

## 04-5. 버리기 연결

```cpp
FReply UEPItemCellWidget::NativeOnMouseButtonDown(
    const FGeometry& Geo, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() == EKeys::RightMouseButton && EntryId != INDEX_NONE)
    {
        /* PlayerController → Server_DropItem(EntryId) */
        return FReply::Handled();
    }
    return FReply::Unhandled();
}
```

- **UI는 `EntryId`만 보낸다.** 서버가 그 엔트리를 확인하고 판정한다 (Step 03 03-5)
- 스택이 없으므로 "몇 개 버릴까"라는 질문이 없다. 우클릭 = 그 아이템 하나
- **클라이언트에서 엔트리를 미리 지우지 않는다.** 서버 응답으로 `PreReplicatedRemove`가 오면 그때 사라진다. 예측하면 거부됐을 때 되돌려야 한다

### ★ 드래그 아웃 — "패널 바깥 아무데나"로 만들지 않는다

기획은 *"바깥으로 끌면 버린다"* 인데, 그대로 구현하면 **드래그하다 손이 미끄러질 때마다 총이 바닥에 떨어진다.** 구획 사이 여백·스크롤 영역·화면 가장자리가 전부 "바깥"이 되어 오조작이 잦다.

```
┌─ 착용 ──────┬─ 소지품 ──────┐
│             │               │
└─────────────┴───────────────┘
      ╔═══════════════════╗
      ║   🗑  여기로 버림   ║      ← 드래그를 시작할 때만 나타난다
      ╚═══════════════════╝
```

**드래그 중에만 하단에 명시적 존을 띄운다.** 기획(바깥으로 끌기)과 어긋나지 않으면서 오조작이 훨씬 적다. 존 바깥에서 놓으면 **아무 일도 일어나지 않는다**(원위치).

> **구획 *안*의 여백은 "바깥"이 아니다.** 버블링으로 그 구획의 `NativeOnDrop`이 받아 **맨 뒤로 보내기**가 된다(04-1 · 04-7). 버리기 존은 **구획 어디에도 안 걸린 드롭**만 받는다 — 두 경로가 겹치지 않는다.

> `DropCooldownEndTime`(Step 03 03-5)이 살아 있으면 존을 회색으로 그린다. 서버가 어차피 거부하므로 **왕복을 아끼는 게 아니라 이유를 미리 보여주는 것**이다.

---

## 04-6. 입력과 모드 전환

- `IA_ToggleInventory` (Tab) → `AEPPlayerController`에 UPROPERTY + 게터 (기존 스킬 입력과 동일 패턴)
- 열 때: `SetInputMode(FInputModeGameAndUI)` + `bShowMouseCursor = true`
- 닫을 때: `FInputModeGameOnly` + 커서 숨김

> **`FInputModeUIOnly`를 쓰지 않는다.** 인벤토리를 연 상태에서도 주변 상황이 보이고 이동할 수 있어야 한다 — 익스트랙션 슈터에서 가방을 여는 동안 완전히 무방비가 되는 건 의도된 리스크지만, **입력이 통째로 막히면 리스크가 아니라 버그처럼 느껴진다.** 타르코프도 인벤토리 중 이동이 된다.

`UEPHUDWidget`과 별개 위젯으로 두고 `AEPPlayerController::InitHUD`에서 같이 생성·바인딩한다.

---

## 04-7. ★★ 드래그 — 이동과 교환

**세 가지가 같은 조작이다.** 출발지와 목적지가 무엇이냐만 다르다.

| 드래그 | 서버 호출 |
|---|---|
| 컨테이너 A **빈 칸** → 컨테이너 B | `Server_MoveEntry(id, B, NAME_None)` |
| 컨테이너 → **착용 슬롯** (착용) | `Server_MoveEntry(id, INDEX_NONE, "Torso")` |
| 착용 슬롯 → **컨테이너** (해제) | `Server_MoveEntry(id, B, NAME_None)` |
| **아이템 위에** 떨어뜨림 (교환) | `Server_SwapEntries(A, B)` ★ |
| **같은 컨테이너 안** 순서 바꾸기 | **`Server_ReorderEntry(EntryId, PrevEntryId)`** — 용량·슬롯을 안 본다 (04-8 · 03-2) |
| 패널 바깥 (버리기) | `Server_DropItem(id)` (04-5) |

### Step 03이 미뤄둔 RPC 둘을 여기서 연다

```cpp
UFUNCTION(Server, Reliable) void Server_MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId);
UFUNCTION(Server, Reliable) void Server_SwapEntries(int32 EntryIdA, int32 EntryIdB);
```

**Step 03은 `MoveEntry`(내부 계약)만 만들고 RPC 표면을 열지 않았다.** 그때는 `NewParent`·`NewSlotId`를 정당하게 만들어낼 UI가 없어서 *"소비자보다 검증 표면을 먼저 여는 것"* 이었다(`EquipmentSlots.md` §6). **이 단계가 그 소비자다.**

### ★ 교환은 원자적이어야 한다 — 순차 `MoveEntry` 두 번으로는 안 된다

배낭의 AK(4)와 외투의 붕대(1)를 바꾼다고 하자. 외투가 `9 / 10`이라면:

```
MoveEntry(AK, 외투, None)   →  9 - 0 + 4 = 13 > 10   실패
                               ↑ 붕대가 아직 안 나갔다
```

**중간 상태에서 자리가 없어 실패한다.** 그런데 교환 후 상태는 `9 - 1 + 4 = 12`… 이것도 초과다. 그럼 초과가 아닌 예로 바꾸면 — 외투가 `7 / 10`이고 AK(4) ↔ 붕대(1)면 교환 후는 `7 - 1 + 4 = 10`으로 **정확히 들어간다.** 그런데 순차로 하면 `7 + 4 = 11`로 **실패한다.**

**즉 순차 적용은 성립하는 교환을 거절한다.** 그래서 `MoveEntry` 두 번이 아니라 별도 함수다.

```cpp
// 두 엔트리의 (ParentEntryId, SlotId)를 맞바꾼다. 판정은 교환 후 상태로 한 번에
bool SwapEntries(int32 A, int32 B);
```

| # | 검사 | 비고 |
|---|---|---|
| 1 | 둘 다 `ContainsEntry` | 조작된 요청 |
| 2 | **A가 B의 조상이거나 B가 A의 조상이 아닌가** | 배낭과 그 안의 총을 바꾸면 사이클. `MoveEntry` 검사 6과 같은 이유 |
| 3 | 목적지가 슬롯이면 **`CanPlaceInSlot(상대부모, 상대SlotId, 내ItemId, 나)`** ×2 | `MoveEntry` 검사 2·3·4를 13차가 뽑았다. **네 번째 인자가 이 단계에서 붙는다** — 아래 |
| 4 | **목적지가 컨테이너면 교환 후 용량** | 아래 |
| 5 | 같은 부모 ＋ 둘 다 `SlotId == None` | **거절한다.** 순수 표시 순서라 서버가 볼 일이 아니다 (04-8) |

#### ★★ `CanPlaceInSlot`을 그대로 부르면 슬롯 교환이 **전부** 거절된다 (15차 신설)

**13차가 `MoveEntry`의 검사 2·3·4를 `CanPlaceInSlot(Parent, SlotId, ItemId)` 하나로 뽑았다**(03-2). 검사표 3번은 이제 그 함수를 두 번 부르는 것이 되는데, **마지막 줄이 교환과 정면으로 부딪힌다.**

```cpp
// 03-2 CanPlaceInSlot 의 마지막 줄
return GetEntryInSlot(Parent, SlotId) == INDEX_NONE;   // 검사 4 — 그 자리가 비어 있나
```

**교환에서 그 자리는 언제나 차 있다 — 상대가 거기 있다.** 그게 교환의 정의다.

```
핫바1 AK ↔ 핫바2 권총
   CanPlaceInSlot(-1, "Hotbar2", Weapon_AK74)  →  GetEntryInSlot(-1,"Hotbar2") = 권총  →  false
   ⇒ 슬롯이 걸린 교환이 하나도 성립하지 않는다
```

**`MoveEntry`에는 이 문제가 없다.** 검사 4가 *"차 있으면 교체가 아니라 실패"* 이고 그게 의도이기 때문이다(03-2). **교환만 예외이고, 예외의 정체는 "나가는 사람을 세지 마라"** 다 — `MoveEntry`가 **검사 0**으로 *"엔트리가 이미 그 슬롯에 있으면 검사 4가 자기 자신을 찾아 거절한다"* 를 막은 것과 **같은 종류의 것**이다(03-2 ★ 검사 0이 검사 4보다 앞인 것은 계약이다).

**네 번째 인자를 붙인다. 소비자가 이 단계에 있으므로 여기서 붙인다.**

```cpp
// 03-2 — 기본값이 INDEX_NONE이라 기존 호출자 둘(MoveEntry · AddSubtree)은 한 글자도 안 바뀐다
bool CanPlaceInSlot(int32 Parent, FName SlotId, FName ItemId,
                    int32 IgnoreEntryId = INDEX_NONE) const;

    // 검사 4 — 그 자리가 비어 있나. 지금 나가는 중인 엔트리는 세지 않는다
    const int32 Occupant = GetEntryInSlot(Parent, SlotId);
    return Occupant == INDEX_NONE || Occupant == IgnoreEntryId;
```

```cpp
// SwapEntries — 서로를 무시 대상으로 넘긴다
if (!CanPlaceInSlot(B.ParentEntryId, B.SlotId, A.ItemId, B.EntryId)) return false;
if (!CanPlaceInSlot(A.ParentEntryId, A.SlotId, B.ItemId, A.EntryId)) return false;
```

**세 번째 소비자가 UI다.** 04-1 ②의 슬롯 하이라이트도 같은 인자를 쓴다 — 끌고 있는 엔트리가 이미 그 슬롯에 있으면(제자리) 자기 자신 때문에 빨개지는 것을 막는다.

> **★ `CanPlaceInSlot`을 안 부르고 검사 2·3만 직접 쓰는 안은 기각한다.** 그러면 판정식이 다시 두 곳이 되고, **13차가 `CanPlaceInSlot`을 뽑은 이유(*"호출자가 보증하는 계약은 호출자가 늘어나면 깨진다"*)가 세 번째 호출자에서 곧바로 깨진다.** §7-3 부착물 교체(조준경 ↔ 조준경)가 정확히 이 경로로 온다.

> **★ 인자를 03에 미리 넣지 않는 것이 이 문서의 규칙이다.** `Server_MoveEntry`(9차)·`Server_ReorderEntry`(11차)·`Server_EquipBackpack`(14차)에 세 번 적용한 것과 같다 — **검증 표면은 소비자를 따라간다.** 03-2에는 *"04-7이 네 번째 인자를 붙인다"* 한 줄만 적어둔다.

#### ★★ `SortKey`도 함께 맞바꾼다 — 필드는 둘이 아니라 **셋**이다 (15차 신설)

**11차가 `SortKey`를 도입하면서 `MoveEntry`에 재발급 규칙이 붙었다**(03-2, *"부모가 바뀌었으면 `KeySpace_NextAtEnd`로 재발급"*). **교환도 부모가 바뀌는 연산이므로 같은 문제를 받는다.**

```
본체:  … 붕대 1,000,000        (오래 쓴 컨테이너)
배낭:  칫솔 0   가위 65536      (새 컨테이너)

붕대 ↔ 칫솔 교환. SortKey를 안 건드리면
   → 붕대가 배낭 안에서 1,000,000   ← 목적지와 무관한 값 (함정 4x가 그대로 재현)
   → 칫솔이 본체 안에서 0
```

**그런데 교환에서는 `KeySpace_NextAtEnd`를 부를 필요가 없다 — 맞바꾸면 끝이다.** 각자가 상대의 자리를 물려받는데 **그 키는 목적지 스코프에서 이미 유효하고 중복도 없다**(방금 상대가 비웠다). `MoveEntry`가 재발급인 것은 **혼자 들어가서 물려받을 자리가 없기 때문**이다.

```cpp
// 필드 셋을 맞바꾼다. Swap이라 임시 하나면 된다
Swap(A.ParentEntryId, B.ParentEntryId);
Swap(A.SlotId,        B.SlotId);
Swap(A.SortKey,       B.SortKey);        // ★ 15차. 빠뜨리면 함정 4x가 교환 경로로 되살아난다
Entries.MarkItemDirty(A);
Entries.MarkItemDirty(B);
```

**같은 컨테이너 안 교환이 검사 5에서 거절되므로**(위 표) **두 키가 같은 스코프에서 맞바뀌는 경우는 슬롯끼리뿐이고, 그쪽은 표시 목록에 안 나와 무해하다.**

> **★ 이 계약이 §7-4에서 값을 한다** (`05_Loot_DOCS.md` §7-4 · 03-2). 탄창이 별도 아이템이 되면 재장전이 `SwapEntries(꽂힌탄창, 새탄창)` 한 줄인데, **빈 탄창이 새 탄창의 화면 자리를 물려받아** *"제자리에서 탄창만 바뀐"* 것으로 보인다. `SortKey`를 안 바꾸면 빈 탄창이 **엉뚱한 자리로 튄다.**

#### ★★ 용량식에 `SlotSize`를 무조건 더하고 빼면 안 된다 (10차 정정)

초안은 이랬다.

```cpp
const int32 UsedPA = GetUsedSlots(PA) - SizeA + SizeB;   // ✗
const int32 UsedPB = GetUsedSlots(PB) - SizeB + SizeA;   // ✗
if (UsedPA > GetCapacity(PA) || UsedPB > GetCapacity(PB)) return false;
```

**`GetUsedSlots`는 슬롯에 든 것을 애초에 안 센다**(`if (!E.SlotId.IsNone()) continue;` — `05_Loot_03A_Core.md:1586`). 그런데 위 식은 슬롯에 든 쪽의 크기까지 빼고 더한다. **교환은 `(ParentEntryId, SlotId)`를 통째로 맞바꾸므로 B는 A의 `SlotId`를 물려받는다** — A의 자리가 칸을 먹는 자리였을 때만 그 자리에서 칸 회계가 움직인다.

```cpp
// ★ 크기 항은 그쪽이 칸을 먹는 자리일 때만 붙는다
const int32 DeltaPA = A.SlotId.IsNone() ? (SizeB - SizeA) : 0;
const int32 DeltaPB = B.SlotId.IsNone() ? (SizeA - SizeB) : 0;

if (PA == PB)
{
    if (GetUsedSlots(PA) + DeltaPA + DeltaPB > GetCapacity(PA)) return false;
}
else
{
    if (GetUsedSlots(PA) + DeltaPA > GetCapacity(PA)) return false;
    if (GetUsedSlots(PB) + DeltaPB > GetCapacity(PB)) return false;
}

// 전부 통과한 뒤에야 쓴다. 엔트리당 필드 셋(Parent·SlotId·SortKey)만 고치고 MarkItemDirty 두 번
```

**증상은 "용량 초과"가 아니라 "성립하는 교환의 거절"이다.** 10차 답변은 *"11/10을 통과시킨다"* 로 판정했는데, **대조해 보니 그 방향으로는 안 샌다** — 두 수를 `||`로 보기 때문에 어긋나는 경우마다 **둘 중 하나가 정확한 값**이고 그쪽이 걸러낸다. 틀린 항이 붙는 쪽은 언제나 *실제 변화가 0인* 쪽이고, 거기서는 원래 값이 이미 용량 이내였다. **그래서 이 버그는 조용히 넘기지 않고 조용히 막는다** — 그리고 그건 `SwapEntries`를 만든 이유 자체를 무너뜨린다.

| 반례 | 실제 | 초안 식 |
|---|---|---|
| **본체 10/10에서 핫바1 AK(4) ↔ 핫바2 권총(2)** | 둘 다 슬롯이라 **칸 변화 0.** 10/10 유지 → 성립 | `10 - 2 + 4 = 12 > 10` → **거절** |
| **§7-3 부착물 교체** — 무기1의 조준경(1) ↔ 무기2의 조준경(2) | 둘 다 슬롯. 변화 0 → 성립 | 무기의 `GetCapacity`는 **0**이다. `0 - 1 + 2 = 1 > 0` → **언제나 거절** |
| 본체 10/10에서 핫바1 AK(4) ↔ 본체 구급상자(3) | `10 - 3 + 4 = 11 > 10` → 거절이 **맞다** | 한쪽 수가 11이라 `||`가 잡는다 ✅ |

**첫 줄이 핵심이다 — 핫바 무기 자리 바꾸기는 이 단계의 주요 조작인데, 본체가 꽉 차 있으면 안 된다.** 그리고 *"가방이 꽉 찼을 때만"* 이라 UI 버그로 보인다.

> **`PA == PB`를 합산으로 적는 이유.** 오늘은 검사 5(*"같은 부모 ＋ 둘 다 `SlotId == None`"* 거절)가 **두 델타가 동시에 0이 아닌 유일한 경우를 미리 잘라내서** 합산과 개별 검사의 결과가 같다. 그래도 합산으로 적는다 — **검사 5를 완화하는 순간(예: 같은 컨테이너 안 교환을 서버로 되돌리면) 개별 검사는 틀린 답을 낸다.** 지금 값을 하는 것이 아니라 나중에 조용히 깨지지 않게 하는 형태다.

> **★ 검사가 전부 끝난 뒤에 쓴다.** 하나라도 쓰고 나서 다음 검사에서 실패하면 **한쪽만 옮겨간 상태**가 남고, 그건 `Client_OnInventoryActionFailed`로 알려도 되돌릴 수 없다. 완료 조건 *"교환이 실패하면 아무 일도 일어나지 않는다"* 가 이걸 본다.

> **★ 살아 있는 엔트리에 통째 대입 금지가 여기서도 걸린다.** `E = Other`로 바꾸면 `ReplicationID`가 리셋돼 수신 측이 **삭제+추가**로 본다(`FastArraySerializer.h:302-323`). `ParentEntryId` · `SlotId` · **`SortKey`** **세 필드만** 고치고 `MarkItemDirty(E)`를 부른다(위 15차 절). 함정 4g(Step 03)와 같은 것이다.

> **`SwapEntries`는 Step 03에 넣지 않는다.** 소비자가 이 단계의 드래그뿐이고, `MoveEntry`와 달리 Step 03에 래퍼가 될 호출자도 없다. 9차 판정(*"원자적 교체는 Step 04로"*) 그대로다.

### 드롭 판정 — `HandleDrop` 한 곳. 지오메트리 산술 0줄

**드롭을 받는 것은 칸·남은 용량 블록이고, 판정하는 것은 패널이다**(04-1). 칸은 자기 정체(`FEPCellHit`)만 넘긴다.

```cpp
bool UEPContainerPanel::HandleDrop(const FEPCellHit& Hit, UDragDropOperation* Op)
{
    const FEPItemDragPayload* P = /* Op에서 꺼낸다 */;

    // ★ 같은 컨테이너 = 자리 바꾸기. 용량도 슬롯도 안 건드리는 별도 RPC다 (04-8 · 03-2)
    //   Server_MoveEntry로 보내면 CanFit이 자기 크기를 두 번 센다 (함정 12d)
    if (P->SourceContainer == ContainerId)
    {
        Inventory->Server_ReorderEntry(P->EntryId, PrevIdFor(Hit, P->EntryId));
        return true;
    }

    if (Hit.EntryId == INDEX_NONE) Inventory->Server_MoveEntry(P->EntryId, ContainerId, NAME_None);
    else                           Inventory->Server_SwapEntries(P->EntryId, Hit.EntryId);
    return true;
}
```

#### ★★ 초안의 두 결함 (10차)

**① `Hit` 하나가 두 가지를 뜻했다.** 드롭 지점을 **표시 인덱스**로 쓰는 경로(→ `Server_ReorderEntry`의 앞 이웃 계산)와 **`EntryId`** 로 쓰는 경로(`Server_SwapEntries(int32 A, int32 B)`)가 있다. 초안은 같은 변수를 한 줄 건너 양쪽에 넘겼다. **둘 다 `int32`라 컴파일된다** — 그래서 `FEPCellHit`으로 이름을 갈라 구조적으로 막는다(04-2).

**② 같은 컨테이너의 여백에 놓으면 서버로 가서 거절됐다.** 초안의 첫 조건은 `SourceContainer == ContainerId **&& Hit != INDEX_NONE**` 이라, 자기 컨테이너의 빈 자리에 놓으면 조건이 거짓이 되어 아래로 흘러 **`Server_MoveEntry(EntryId, ContainerId, NAME_None)`** 을 불렀다. 이미 그 컨테이너 안에 있는 엔트리다.

```
MoveEntry 검사 5:  CanFit(NewParent, ItemId) = GetUsedSlots(NewParent) + SlotSize <= Capacity
                   → 엔트리가 이미 거기 있으므로 자기 크기가 두 번 세어진다

배낭 18/20의 AK(4)를 배낭 여백으로 드래그  →  18 + 4 = 22 > 20  →  거절
```

**증상은 *"가방이 좀 차면 아이템을 맨 뒤로 못 보낸다"*, 그리고 널널할 때는 서버 왕복만 낭비하면서 된다.** 재현 조건이 용량이라 UI 버그로 보이지 않는다. **초안은 04-8이 세운 규칙(*"같은 컨테이너 안은 서버가 볼 것이 없다"*)을 자기 구현에서 어겼다.**

> **★ `DisplayIndex == INDEX_NONE`이면 맨 뒤다.** `FMath::Clamp(-1, 0, N)`은 **0(맨 앞)** 이라 여백에 놓았는데 맨 앞으로 튄다. 계약을 04-8의 앞 이웃 변환 한 곳에 박는다 (함정 13d).

> **`MoveEntry` 쪽 방어는 이미 들어갔다 — 03-2의 검사 0이다** (*"목적지가 지금 자리와 같으면 `false`"*, 검사 목록 맨 앞). UI가 안 부르게 됐어도 `Server_MoveEntry`는 열려 있고 조작된 클라이언트는 같은 요청을 만들 수 있으므로, **04가 이 경로를 안 부르는 것과 03이 막는 것은 둘 다 필요하다.** 검사 0은 검사 4보다 **앞이어야 한다** — 뒤에 두면 제자리 이동에서 검사 4가 **자기 자신**을 찾아 거절한다(03-2).

### 드래그 중 피드백

**판정식은 서버와 같은 함수를 쓴다**(04-1).

```
남은 용량 ≥ SlotSize   →  남은 용량 블록 초록 · 게이지에 들어갈 구간 미리보기 · 숫자  7 → 11 / 20
남은 용량 <  SlotSize   →  빨강 · 게이지에 넘치는 만큼 빨간 구간 ·           숫자 18 → 22 / 20
아이템 위               →  그 칸만 노랑 (교환). 교환 후 양쪽 용량으로 판정
빈 슬롯 위              →  CanPlaceInSlot(Parent, SlotId, ItemId) 가 false면 빨강 (단검에 조준경 금지)
찬 슬롯 위              →  노랑 (교환). ★ CanPlaceInSlot은 검사 4에서 false다 — 네 번째 인자로 상대를 무시한다
```

**거절 사유가 "자리가 없다"가 아니라 "이만큼 모자라다"로 보이는 것**이 04-0에서 빈 칸의 개수를 포기한 대가로 얻는 것이다. **그리고 그 "이만큼"을 말하는 것은 색이 아니라 `7 → 11 / 20` 숫자다** — 색은 가부까지만 답한다.

> **★ `NativeOnDragOver`에서 매 프레임 `GetUsedSlots`를 부르지 않는다.** `GetUsedSlots`는 **드래그 중에 바뀌지 않는다** — 서버 상태가 바뀌는 유일한 계기는 `PostReplicatedReceive`이고 그건 알림을 쏜다(Step 03 03-7). **드래그 시작에 한 번 캐고 알림이 오면 버린다.**
>
> ```cpp
> // NativeOnDragDetected에서 구획마다 한 번
> Payload->CachedUsed.Add(ContainerId, Inventory->GetUsedSlots(ContainerId));
> // OnInventoryChanged가 오면 무효화
> ```
>
> 캐시가 필요해서가 아니라 **부르는 횟수가 틀렸기 때문이다.** 9차가 12칸 슬롯 조회에 대해 내린 판정(*"순회 한 번"*)과 같은 형태다.

---

## 04-8. 같은 컨테이너 안 순서 — **서버가 든다** (11차에 뒤집힘)

**10차까지 이 절은 반대로 적혀 있었다.** 근거는 *"서버 로직 중 아이템 순서를 보는 곳이 0곳"* 이었고 **그 사실 자체는 지금도 맞다.**

| 서버가 하는 일 | 순서를 보나 |
|---|---|
| `GetUsedSlots` / `CanFit` | ✗ 합산이다 |
| `TryAutoEquip` | ✗ `SlotPriority`를 본다 |
| `GetInsertionOrder` | ✗ **컨테이너** 순서지 아이템 순서가 아니다 |
| `RemoveEntry` 캐스케이드 | ✗ `ParentEntryId`로만 판정 |
| `FindFungibleEntryId` | **△ 배열 순서를 본다.** 단 그건 서버 `Entries.Items`의 물리적 순서지 **표시 순서가 아니다** (아래) |

> **★ `FindFungibleEntryId`만 정확히 적어둔다(10차).** 병합 대상이 여럿일 때 **어느 현금뭉치에 합쳐질지는 배열 순서가 정한다.** 그 배열은 `RemoveSelf`가 `RemoveAtSwap`을 쓰면 뒤섞이고 수신 측에서도 순서가 보장되지 않는다(`FastArraySerializer.h:54`). **무해한 진짜 이유는 "첫 번째를 찾으니 순서와 무관"이 아니라 "그 축에서는 원소가 서로 교환 가능하다"** 이다 — 현금뭉치 둘은 구분할 수 없다. **`bFungible`인데 구분 가능한 상태를 갖는 아이템(예: `Durability`가 다른)이 생기면 이 문장이 깨진다.**

### ★★ 뒤집은 이유는 "권한"이 아니라 **"지속"** 이다

*"서버가 순서를 볼 필요가 없다"* 와 *"순서를 서버에 두면 안 된다"* 는 **다른 명제**였고, 10차는 앞을 근거로 뒤를 주장했다.

**클라 로컬로 두면 순서를 저장할 곳이 따로 필요하다.** 그리고 그 저장이 `EntryId`와 충돌한다:

```
NextEntryId는 컴포넌트 필드고 초기값이 1이다 (EPInventoryComponent.h)
   → 매치마다 1번부터 다시 발급된다
   → 그런데 ULocalPlayerSaveGame은 디스크에 남아 매치를 넘어 산다

지난 매치:  Order[7] = [3, 12, 5]     ← 7번은 배낭
이번 매치:  7번은 다른 사람의 조끼, 3·12·5도 전혀 다른 아이템
            대조 코드는 번호만 본다  →  통과시킨다  →  엉뚱한 순서
```

세션 도장을 찍어 무효화하면 **지속이 한 번도 발휘되지 않아** 애초에 인메모리와 같아진다. **클라 로컬은 "지속"을 줄 수 없다.**

**서버에 두면 저장 코드가 0줄이다.** 로드맵 5단계(`DOCS.md §5` 14번)가 엔트리 배열을 저장하므로 `SortKey`가 필드면 그냥 따라간다. 설계 근거 전문은 `05_Loot_03_Inventory.md` 03-1 `SortKey` 절.

### ★ RPC 표면은 이 단계에서 연다 (11차 검수)

**Step 03이 만드는 것은 내부 함수 `ReorderEntry(EntryId, PrevEntryId)`까지다.** `Server_ReorderEntry`는 여기서 연다 — 9차가 `Server_MoveEntry`에 적용한 규칙 그대로, **검증 표면을 소비자보다 먼저 열지 않는다.**

```cpp
// 04-B — 얇은 래퍼. 게이트가 여기 붙는다
void UEPInventoryComponent::Server_ReorderEntry_Implementation(int32 EntryId, int32 PrevEntryId)
{
    if (!CanMutateInventory()) return;      // ★ 상태 변경 RPC의 유일한 게이트 (03-5)
    ReorderEntry(EntryId, PrevEntryId);     // 검증은 전부 저 안에 있다 (03-2)
}
```

> Step 03은 `EP.Inv.Reorder` 커맨드로 `ReorderEntry`를 직접 불러 계약을 먼저 닫는다. **RPC가 없어도 순서·재정규화·조작 거부가 전부 검증된다.**

### 이 단계가 하는 일 — 셋뿐이다

```cpp
// ① 그린다 — 정렬은 컴포넌트가 한다. UI에 순서 자료구조가 없다
for (int32 Id : Inventory->GetSortedContents(ContainerId))
    CellBox->CreateEntry<UEPItemCellWidget>()->SetFromEntry(Id);

// ② 놓는다 — 앞 이웃을 보낸다. 인덱스가 아니다 (03-2)
Inventory->Server_ReorderEntry(P->EntryId, PrevEntryIdAtDrop);   // RPC는 이 단계에서 연다

// ③ 낙관적으로 먼저 그린다 — 아래
```

**`Resolve` / `MoveTo` / `ULocalPlayerSaveGame` / `ULocalPlayerSubsystem` / `FEPContainerOrder`가 전부 사라졌다.** 끊어진 참조 청소도, 세션 검사도 없다 — `SortKey`는 엔트리와 함께 죽는다.

> **`FastArraySerializer.h:54`의 "클라 배열 순서 미보장"은 그대로 유효하다** (함정 1). 그래서 **배열 순서로 그리면 안 되는 것도 그대로다** — 다만 정렬 키가 로컬 맵이 아니라 복제된 필드일 뿐이다.

### ★ 드롭 지점 → **앞 이웃**으로 바꾸는 것은 UI의 일이다

`Server_ReorderEntry`가 인덱스가 아니라 `PrevEntryId`를 받으므로(03-2), 격자가 그 변환을 한다.

```cpp
// FEPCellHit.DisplayIndex = 격자에서 몇 번째 자리에 놓았나 (04-2)
// 그 자리의 "앞"에 있는 것이 Prev다. 자기 자신은 목록에서 빠져야 한다
TArray<int32> Sorted = Inventory->GetSortedContents(ContainerId);
Sorted.Remove(DraggedEntryId);                       // ★ 자기를 뺀 뒤에 센다

const int32 Idx = (Hit.DisplayIndex == INDEX_NONE) ? Sorted.Num()   // 여백 = 맨 뒤
                                                   : FMath::Clamp(Hit.DisplayIndex, 0, Sorted.Num());
const int32 PrevEntryId = (Idx == 0) ? INDEX_NONE : Sorted[Idx - 1];
```

> **★ `Sorted.Remove(DraggedEntryId)`를 먼저 한다.** 안 하면 아이템을 **자기보다 뒤로** 옮길 때 한 칸씩 모자란다 — 3번을 5번 자리로 끌면 4번 자리에 앉는다. 앞으로 옮길 때는 정상이라 **한쪽 방향에서만 난다.**
>
> **★ `INDEX_NONE`은 맨 뒤다** — `Clamp(-1, 0, N)`은 0(맨 앞)이라 10차에 같은 실수가 있었다(함정 13d). 계약을 여기 한 곳에 박는다.

### ★ 낙관적 적용 — 예측이 아니다

**재배치는 실패할 수 없는 연산이다**(03-2). 용량도 슬롯도 사이클도 안 건드리므로 정상 클라이언트에서 서버가 거절할 이유가 없다. **그래서 롤백 케이스가 없다.**

```
드롭 → 로컬 SortKey를 추정값으로 덮고 즉시 다시 그린다
     → Server_ReorderEntry 발사
     → 복제 도착 → PostReplicatedReceive → 다시 그린다 (서버 값으로 수렴)
```

- **되돌릴 상태를 저장하지 않는다.** Step 03의 클라이언트 예측(재생·롤백)과 **다른 종류**다 — 이건 그냥 *"먼저 그리고 나중에 덮는다"* 다
- **1차는 넣지 않는다.** 서버 응답을 기다려도 되고, 체감이 나쁠 때 붙이면 된다
- 넣을 때 주의: 추정 키는 서버와 다를 수 있다(다른 클라의 획득이 끼어들면). **키 값이 아니라 순서만 맞으면 되므로** 도착한 값으로 덮는 것으로 충분하다

> ### ★★ *"클라 전용이라 서버 계약을 안 건드린다"* 가 12차 이후로 반쯤 거짓이다 (13차 답변)
>
> **① 제자리 드롭은 서버가 조용히 아무것도 안 보낸다.**
>
> ```
> 드롭 → 클라가 로컬 SortKey를 추정값으로 덮는다
>      → Server_ReorderEntry
>      → 서버: 제자리다  →  12차의 조기 반환(함정 4u)  →  AssignSortKey를 안 부른다
>      → 아무것도 복제되지 않는다
>      ⇒ 클라의 추정 키가 영영 안 덮인다
> ```
>
> **표시 순서는 맞아서 안 보인다.** 문제는 **다음 드래그의 추정이 오염된 키에서 출발한다**는 것이다. *"도착한 값으로 덮는다"* 가 성립하려면 도착해야 하는데, **12차가 도착하지 않는 경우를 만들었다.**
> → **넣을 때 클라도 먼저 조기 반환한다.** 서버의 4u와 같은 판정을 `GetSortedContents`만으로 할 수 있다.
>
> **② 추정 키를 `GetSortedContents`로 만들면 안 된다.** 서버는 **부모 전체**를 본다(함정 4q·4s). 클라가 표시 목록으로 추정하면 **슬롯 형제와 동률이 나서 잠깐 엉뚱한 자리에 그려진다.** 같은 계산을 하려면 `KeySpace_*` 넷이 공개돼야 하는데 지금 전부 **private**이다 — **그 순간 "서버 계약을 안 건드린다"가 깨진다.**
> → **더 싼 답: 키를 추정하지 말고 표시 순서만 로컬로 뒤집는다.** 어차피 *"키 값이 아니라 순서만 맞으면 된다"* 가 이 절의 전제다.

### ★ "전부 옮기기"의 우회로가 사라진다

```
배낭(20/20 가득) → 외투(빈 칸 7)로 Shift+클릭 "전부 옮기기"
  → 7칸어치만 들어간다. 무엇이? 플레이어는 "위에서부터"를 기대한다
```

10차는 *"클라가 자기 순서대로 `Server_MoveEntry`를 N번 부른다"* 는 우회로를 적어뒀다. **서버가 `GetSortedContents`를 직접 부르면 된다** — 이게 10차가 *"순서를 서버가 봐야 하는 유일한 연산"* 이라고 이름 붙여 남긴 그 자리다.

> 이 단계에서 만들지는 않는다. **우회로가 필요 없어졌다는 것만 적어둔다.**

### ★ 자동 정렬(이름순·희귀도순)도 서버로 간다

`RenormalizeSortKeys`(03-2)가 *"현재 순서를 유지한 채 간격만 다시 깐다"* 인데, **비교 함수만 바꾸면 그대로 정렬 버튼이다.** `Server_SortContainer(Container, ESortMode)` 하나이고 UI에 정렬 코드가 안 생긴다.

> 만들지 않는다 — **확장점 이름만 남긴다.** 드래그가 먼저 돌아야 한다.

### 빈 칸을 중간에 남길 수 없다

칸 수가 가변이라 절대 위치(`GridIndex`)를 정의할 수 없기 때문이다(04-0). 순서만 있고 빈 칸은 언제나 끝이다 — 미네크래프트식 *"3번 칸을 비워두기"* 는 지원하지 않는다. **`SortKey`는 상대 순서이지 좌표가 아니다.**

> **§7-1의 2D 격자로 가면 이 제약이 풀린다.** 그때 `SortKey` → `FIntPoint Location`인데 **호출 지점이 동일하다**(03-1). 클라 로컬로 뒀다면 같은 이행이 *"04-8을 버리고 03을 다시 연다"* 였다.

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | 배열 순서대로 그림 | `FastArray`가 클라 배열 순서를 보장하지 않는다(`FastArraySerializer.h:54`). 줍고 버릴 때마다 칸이 자리를 바꿈 | `GetSortedContents()` (04-2 · 04-8) |
| 2 | 구독 직후 1회 반영 누락 | 창을 처음 열면 빈 화면. 다음 획득 때 채워짐 | `InitWithInventory` 끝에서 `RefreshEntries()` |
| 3 | **`Entries.Owner = this` 누락** (Step 03 03-2) | 클라 UI가 **영원히** 갱신 안 됨. 원인이 UI/복제로 보인다 | 04-3 ★ |
| 3b | 항목별 콜백 3종을 씀 | 수신 1회에 목록 재생성이 항목 수만큼. `PostReplicatedChange` 누락 시 잔탄만 옛날 값 | `PostReplicatedReceive` 하나 (04-3) |
| 4 | `NativeTick`에서 폴링 | 불필요한 비용, FastArray를 쓴 의미가 없어짐 | 콜백 구독 |
| 5 | 재바인딩 시 기존 핸들 미해제 | 리스폰 후 `RefreshEntries`가 2번씩 불림 | `Remove(ChangedHandle)` 선행 |
| 6 | 아이콘 로드에 일반 람다 | 목록 재생성/창 닫기 후 콜백 도착 시 크래시 | `CreateWeakLambda` |
| 7 | 플레이스홀더 없음 | 아이콘이 없어 목록이 전부 비어 보임 → 검증 불가 | 04-4 |
| 8 | 게이지를 엔트리 **개수**로 계산 | 무기(5칸)를 넣어도 1만 오름 → 합산 로직 버그가 무증상 | `GetUsedSlots(Container)` (04-2) |
| 8b | 컨테이너를 **한 목록에** 합쳐 그림 | 왜 못 넣는지 플레이어가 이해 못 함. GAME.md의 "통합되지 않는다"가 화면에 안 드러남 | 구획마다 격자 + 게이지 (04-2) |
| **8d** | **용량 칸 수만큼 고정 격자를 그림** | `SlotSize`가 아이템마다 달라 **칸 3개인데 게이지는 7**이 된다. *"17칸 남았는데 왜 안 들어가지?"* | 격자는 나열, **진실은 게이지** (04-0) |
| **8e** | **분절 게이지를 빼고 단색 바만 씀** | *"칸 3개인데 7"* 을 설명하는 장치가 사라진다. 플레이어가 규칙을 배우지 못함 | `UEPSegmentedBar` (04-0) |
| **8f** | **착용 슬롯에도 `SizeBadge`를 붙임** | 슬롯에 든 것은 칸을 안 먹는데(`GetUsedSlots`가 건너뜀) 숫자가 보인다 — **거짓말이다** | 착용 슬롯은 배지 없음 (04-0) |
| **8g** | **빈 칸을 N개 그림** (고정 열 × 최소 행) | 개수가 레이아웃 부산물인데 플레이어는 정보로 읽는다. **게이지와 격자가 다른 숫자를 말한다** — 8d가 형태만 바꿔 남는다 | **`＋ 남은 용량 N` 한 덩어리** (04-0) ★★ |
| **8h** | **매 갱신 `CreateWidget` N개** | 구획 6 × 엔트리 30이 획득·이동마다. GC 압력이 실측된다 | `UDynamicEntryBox(Wrap)` — 풀링 내장 (04-2) |
| 8c | 부착물 자식을 목록에 섞어 그림 | 조준경이 가방 목록에 따로 뜬다 (§7-3 도입 후) | `SlotId.IsNone()` 필터 (04-2) |
| 9 | `FInputModeUIOnly` | 인벤토리 중 이동 불가. 버그처럼 느껴짐 | `GameAndUI` |
| 10 | 클라에서 엔트리 선삭제 | 서버가 거부하면 아이템이 사라진 채로 남음 | 예측 금지 |
| **11** | **교환을 `MoveEntry` 두 번으로 구현** | 중간 상태에 상대가 아직 안 빠져서 **성립하는 교환이 거절된다.** 더 나쁘게는 한쪽만 옮겨간 채 실패 | `SwapEntries` — 교환 후 상태로 한 번에 판정 (04-7) ★★ |
| **11b** | **검사 도중에 필드를 씀** | 뒤 검사에서 실패하면 **한쪽만 옮겨간 상태**가 남고 되돌릴 수 없다 | 전부 통과 뒤에 쓴다 (04-7) ★★ |
| **11c** | **교환을 `E = Other` 통째 대입으로** | `ReplicationID`가 리셋돼 수신 측이 **삭제+추가**로 본다. 내부 struct 델타가 사라진다 | 필드 **셋**만 + `MarkItemDirty` (04-7). Step 03 함정 4g와 같은 것 |
| **11e** | **교환의 슬롯 판정에 `CanPlaceInSlot`을 그대로 부름** | 검사 4가 *"그 자리가 비었나"* 인데 **교환에서 그 자리는 언제나 차 있다 — 상대가 거기 있다.** 증상은 *"슬롯이 걸린 교환이 하나도 안 된다"* 이고, 핫바 무기 자리 바꾸기·§7-3 조준경 교체가 **전부** 막힌다. 컴파일도 되고 수납 교환은 정상이라 슬롯 UI 버그로 보인다 | `IgnoreEntryId` 네 번째 인자로 상대를 세지 않는다 (04-7 · 03-2). `MoveEntry` 검사 0과 **같은 종류의 것** ★★ |
| **11f** | **교환에서 `SortKey`를 안 맞바꿈** | 컨테이너를 건너는 교환에서 **옛 컨테이너의 키를 들고 새 컨테이너에 들어간다** — Step 03 함정 4x가 교환 경로로 되살아난다. *"가끔 이상한 자리에 들어간다"* 로 인지되고, §7-4 재장전이 오면 **빈 탄창이 엉뚱한 자리로 튄다** | `Swap(A.SortKey, B.SortKey)` — 교환이라 **재발급이 아니다**, 상대의 유효한 키를 물려받는다 (04-7) ★★ |
| **11d** | **교환의 용량 판정에 `SlotSize`를 무조건 더하고 뺌** | `GetUsedSlots`는 **슬롯에 든 것을 애초에 안 센다.** 실제 변화가 0인 쪽에 크기 항이 붙어 **성립하는 교환이 거절된다** — 본체가 꽉 차면 핫바 무기 자리를 못 바꾸고, 크기 다른 조준경 둘은 **언제나** 못 바꾼다(무기 `GetCapacity` == 0) | `SlotId.IsNone()` 조건부 델타 ＋ `PA == PB` 합산 (04-7) ★★ |
| **12** | **격자가 좌표로 히트 테스트(`HitTestCell`)** | 패딩·스크롤 오프셋·DPI·`SlotPadding`을 스스로 처리해야 한다. **가장자리에서만 어긋난다.** Slate가 이미 하는 일의 재구현 | 칸이 `OnDrop`을 받고 버블링으로 패널에. 판정은 `HandleDrop` 한 곳 (04-1) ★★ |
| **12c** | **드롭 지점을 `int32` 하나로** | 같은 변수가 한 줄 건너 **표시 인덱스**와 **`EntryId`** 를 뜻한다. **둘 다 `int32`라 컴파일된다** | `FEPCellHit { DisplayIndex, EntryId }` (04-2 · 04-7) ★★ |
| **12b** | **UI가 자체 판정식을 씀** | 드래그 중엔 초록인데 **서버가 거절한다.** 판정식이 두 곳이라 반드시 어긋난다 | **`CanFit`(수납) / `CanPlaceInSlot`(슬롯)** 재사용 (04-1). ★ `SlotPriority`·`BodySlots`를 UI가 직접 읽으면 **검사 3·4가 빠져** 이 함정이 그대로 난다 (13차 반영) |
| **13** | **같은 컨테이너 안 자리 바꾸기를 `Server_MoveEntry`로** | `MoveEntry`의 `CanFit`이 **자기 크기를 두 번 센다** → 가방이 찰수록 거절된다. **11차에 뒤집힌 항목** — 서버로 보내는 것 자체는 이제 맞고, **어느 RPC로 보내느냐**가 문제다 | `Server_ReorderEntry` (04-8 · 03-2) ★★ |
| **13b** | **드롭 지점을 앞 이웃으로 바꿀 때 자기를 안 뺌** | 아이템을 **자기보다 뒤로** 옮길 때 한 칸씩 모자란다 — 3번을 5번 자리로 끌면 4번에 앉는다. **앞으로 옮길 때는 정상**이라 한쪽 방향에서만 난다 | `Sorted.Remove(DraggedEntryId)` 를 먼저 (04-8) ★★ |
| **13c** | **재배치에 롤백을 만듦** | 되돌릴 상태를 저장하고 서버 거절을 처리하는 코드가 생기는데 **정상 클라에서 그 분기는 안 돈다.** 재배치는 용량·슬롯·사이클을 안 건드려 **실패할 수 없다** | 낙관적 적용은 *"먼저 그리고 나중에 덮는다"* 뿐 (04-8) |
| **13d** | **드롭 지점 `INDEX_NONE`을 `Clamp`로만 처리** | `Clamp(-1, 0, N)` = **0 = 맨 앞.** 여백에 놓아 뒤로 보내려 했는데 맨 앞으로 튄다 | `INDEX_NONE` → `Sorted.Num()` 을 변환 한 곳에 (04-8) |
| **13e** | **순서를 클라 로컬 세이브(`ULocalPlayerSaveGame`)에 둠** | `NextEntryId`가 매치마다 1부터 재발급되는데 세이브는 디스크에 남는다 → **지난 매치의 순서가 이번 매치의 같은 번호에 적용된다.** 세션 검사로 막으면 지속이 한 번도 안 되어 **인메모리와 같아진다.** 10차 설계였다 | `SortKey`를 엔트리에 (04-8 · 03-1) ★★ |
| **13f** | **UI가 `SortKey` 값을 직접 계산해 서버에 보냄** | 클라가 보낸 키를 서버가 그대로 쓰면 **겹치거나 범위를 벗어난 값을 심을 수 있다.** 증상은 순서가 무작위가 되는 것 | 클라는 **앞 이웃(`EntryId`)** 만 보내고 키는 서버가 만든다 (03-2) ★★ |
| **14** | **`NativeOnDragOver`에서 매 프레임 `GetUsedSlots`** | 드래그 중엔 값이 안 바뀌는데 구획마다 O(N)을 매 프레임 돈다 | 드래그 시작에 한 번 캐고 `OnInventoryChanged`에 무효화 (04-7) |

---

## 이 단계에서 하지 않는 것

- **자동 정렬(이름순·종류순·희귀도순)** → **`RenormalizeSortKeys`의 비교 함수만 바꾸면 된다**(03-2). `Server_SortContainer(Container, ESortMode)` 하나이고 UI에 정렬 코드가 안 생긴다. **드래그가 먼저 돌아야 한다.** 넣을 때는 구획 헤더마다 독립 컨트롤로 — 전역 버튼 하나면 *"어느 구획을?"* 이 모호해진다
- 툴팁 / 아이템 상세 패널
- 장착 강조 표시 → **Step 05** (`ActiveHotbarIndex` → `GetEquippedEntryId()`). 이 단계는 **슬롯에 무엇이 들었나**까지만 그린다
- **핫바 5~0 UI** → **Step 05.** `HotbarRefs` 필드는 이 단계에서 선언하지만 청소가 Step 05(`RemoveSelf` 한 줄)라 UI도 같이 미룬다. 그릴 때는 **1~4와 시각적으로 갈라야 한다** — 5~0은 참조라 아이템이 구획에도 그대로 보이므로, 테두리만/반투명 + 구획 칸에 `⑤` 배지를 붙이지 않으면 *"5번에 걸었는데 왜 가방에 그대로 있지?"* 가 난다
- 부착물 UI (`Optic`/`Muzzle`/`Grip`/`Mag`) → **§7-3.** 슬롯 표현은 같지만 **무기 상세 화면**이 따로 필요하다
- 월드 컨테이너 루팅 UI → **§7-1.** 그때 왼쪽에 열이 하나 더 붙는다 (04-0)
- 2D 격자(테트리스) 배치 → **하지 않는다.** 데이터 모델에 2D 크기가 없고, 넣으면 Step 03 재작성이다 (04-0)
- `UListView` → 아이템 타입이 `UObject*` 고정이라 안 쓴다. **풀링은 반대로 넣는다** — `UDynamicEntryBox(Wrap)` (04-2)
- **"전부 옮기기"(Shift+클릭)** → 04-8에 이름만 적어둔다. **11차에 우회로가 필요 없어졌다** — 서버가 `GetSortedContents`를 직접 부른다

---

## 변경 이력

| 날짜 | 무엇 |
|---|---|
| 2026-08-26 (15차 — 03의 13·14차 반영) | **★★ `SwapEntries`가 03의 13차 변경 둘에 걸린다.** ① **`CanPlaceInSlot`을 그대로 부르면 슬롯 교환이 전부 거절된다** — 검사 4가 *"그 자리가 비었나"* 인데 교환에서 그 자리는 상대가 차지하고 있다. `IgnoreEntryId` 네 번째 인자를 **이 단계에서** 붙인다(검증 표면은 소비자를 따라간다 — 9·11·14차와 같은 규칙). ② **`SortKey`도 맞바꾼다** — 안 바꾸면 함정 4x가 교환 경로로 되살아난다. 재발급이 아니라 교환이라 `KeySpace_NextAtEnd`가 필요 없다. 함정 **11e·11f** 신설, 11c의 *"필드 둘"* → **셋**. 04-1 ②의 판정식 목록을 **`CanFit` / `CanPlaceInSlot`** 으로(`SlotPriority`·`BodySlots` 직독 금지, 함정 12b 대응 교체). `ContainerOrder`에 **§8 미정 #10** 표시 — 04-3이 그 두 번째 소비자다. `EP.Inv.Add`의 **`[Container]` 인자** 명시(본체 0칸 이후 04-A 검증 절차). `MoveEntry` 방어는 **이미 검사 0으로 들어갔다.** 줄 참조 `03_Inventory.md:747` → `:1530` |
| 2026-08-23 (11차 검수) | **`Server_ReorderEntry` RPC 표면이 이 단계로 왔다** — 9차가 `Server_MoveEntry`에 적용한 규칙(검증 표면을 소비자보다 먼저 열지 않는다). Step 03은 내부 함수 `ReorderEntry`까지만 만들고 `EP.Inv.Reorder`가 그것을 직접 부른다. `CanMutateInventory()` 게이트가 이 래퍼에 붙는다 |
| 2026-08-23 (11차) | **★★ 순서를 서버로 옮겼다 — 10차 결론을 뒤집는다.** 근거 *"서버 로직 중 순서를 보는 곳이 0곳"* 은 지금도 사실이지만, 그건 **"서버가 순서를 볼 필요가 없다"** 이지 **"순서를 서버에 두면 안 된다"** 가 아니었다. 뒤집은 이유는 **지속** — `NextEntryId`가 매치마다 1부터 재발급되므로 클라 세이브는 지난 매치의 순서를 이번 매치의 같은 번호에 적용한다. `FEPInventoryEntry::SortKey`(`int32` 희소, `Step = 1<<16`) 도입, `Server_ReorderEntry(EntryId, PrevEntryId)` 신설, 정렬은 `GetSortedContents`로 **클라·서버 공용**. **`Resolve`/`MoveTo`/`ULocalPlayerSaveGame`/`ULocalPlayerSubsystem`/`FEPContainerOrder` 전부 삭제.** "전부 옮기기" 우회로와 자동 정렬 우회로가 같이 사라짐. 함정 13~13f 전면 개편, 12d는 13으로 흡수. 완료 조건 15개(＋2, 순서 지속 · 배낭 되줍기) |
| 2026-08-23 (10차) | **04-A(표시) / 04-B(드래그)로 분할.** `SwapEntries` 용량식 교정(`SlotId.IsNone()` 조건부 델타 + `PA == PB` 합산) — **10차 답변의 "11/10을 통과시킨다"는 대조 결과 반대였다. 증상은 성립하는 교환의 거절이다.** 드롭을 **칸이 받고 버블링으로 패널이 판정**(`HitTestCell` 폐기), `FEPCellHit`으로 `Hit` 타입 혼동 제거, *"같은 컨테이너면 조건 없이 로컬"*. 순서 지속을 **`ULocalPlayerSaveGame`** 으로(`ULocalPlayerSubsystem`은 홀더만). 빈 칸 N개 → **`＋ 남은 용량` 한 덩어리**. `UDynamicEntryBox(Wrap)` 풀링. `MoveTo`의 `INDEX_NONE` = 맨 뒤. `GetInsertionOrder()`가 `ContainerOrder`를 그대로 반환하지 않음을 명시. 함정 9건 추가(8g·8h · 11d · 12c·12d · 13d~13f · 14) + 12 뒤집음 |
| 2026-08-25 (14차) | **`EP.Inv.Equip` 폐기 — 04-A의 검증 커맨드는 `EP.Inv.Add` ＋ `EP.Inv.Move`다.** 13차가 만든 `EP.Inv.Move`가 이미 착용을 표현한다(`<id> -1 Torso`). 딸려서 **`Server_EquipBackpack`도 삭제** — 13차가 이 문서의 `EP.Inv.Equip`을 *"첫 호출자"* 로 보고 04-A에 옮겼는데 **커맨드는 RPC를 안 지난다.** 착용의 클라 표면은 04-B의 `Server_MoveEntry` 하나로 확정 |
| 2026-08-25 | 13차 답변 — **본체 0칸 확정 반영**(04-3은 이미 `Capacity <= 0`으로 걸러 코드 변경 0). **04-8 낙관적 적용에 주의 둘** — 12차의 제자리 조기 반환(4u) 때문에 *"도착한 값으로 덮는다"* 가 성립하지 않는 경우가 있고, 추정 키를 표시 목록으로 만들면 함정 4q가 클라에서 재현된다 |
| 2026-08-23 | **레이아웃을 목록 → 정사각형 격자로.** `SlotSize`는 배지 + 분절 게이지로 표현(04-0). 드래그가 범위에 들어옴 — 컨테이너 간 **이동·교환**(04-7), 로컬 순서(04-8). `Server_MoveEntry` / `Server_SwapEntries` 개설. 함정 11건 추가(8d~8f · 11~11c · 12~12b · 13~13c) |
| 2026-08-22 | 9차 검수 — 슬롯 12개로 확대. `GetEquippedBackpack()` 호출부 유지, 12칸 슬롯 UI 예고 |
