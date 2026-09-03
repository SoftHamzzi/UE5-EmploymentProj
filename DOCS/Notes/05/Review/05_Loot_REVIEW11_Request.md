# 검수 요청 11차 — 아이템 순서를 서버로 옮긴다 (10차 결론을 같은 날 뒤집음)

> 작성일: 2026-08-23
> 10차: `05_Loot_REVIEW10_Request.md` / `_Answer.md` (**Step 04를 04-A/04-B로 분할**, 순서는 **클라 로컬 + `ULocalPlayerSaveGame`** 으로 확정)
> 시점: **Step 03 골격만(로직 0줄), Step 04 미착수.** 10차·11차 반영 모두 문서에 완료됨
> 성격: **검수가 아니라 설계 결정을 반영한 것이다.** 10차 답변이 확정한 순서 설계를 **같은 날 사용자 판단으로 뒤집었다.** 검증받을 것은 "뒤집을지"가 아니라 **"뒤집은 뒤의 설계가 맞는지"** 다

---

## 0. 사용자 입장 (먼저 밝힌다)

**10차 판정은 §3(용량식)·§4(드롭 라우팅)·§5(`ULocalPlayerSaveGame`) 포함 전부 반영했다.** 답변과 다르게 판정한 넷은 `LOOT_STATUS.md`에 적어뒀고 재론 대상이 아니다.

**그런데 반영 직후 사용자 질문 하나에서 10차 설계의 구멍이 나왔다.**

> *"컨테이너내 아이템 순서는 클라이언트만 안다 했잖아? 그럼 가방을 벗었다가 다시 입으면? 그러면 초기화되는거 아니야?"*

벗었다 입기는 안전했다 — `MoveEntry`는 `SlotId`만 바꾸고 `EntryId`는 그대로다. **그런데 파다 보니 다른 게 나왔다**(§2). 그리고 사용자가 **순서를 서버로 옮기기로 확정**했다.

> *"내가 보기엔 저장했을때도 순서를 유지하는것이 맞아."*

**Claude의 첫 권고는 "지금은 옮기지 마라"였고 사용자가 뒤집었다. 그 권고가 틀렸다는 것도 §2-5에 적었다.**

**이번에 검증받아야 할 것은 넷이다.**

1. **★ §2 — 10차 설계가 정말 "지속을 줄 수 없는" 것이 맞나.** 이 판단이 뒤집히면 11차 전체가 되돌아간다
2. **★ §3 — 키 배치를 `int32` 희소로 한 것.** `double`·조밀 재번호·컨테이너 배열 셋을 기각했는데 근거가 서는가. **여기서 Claude가 인용을 한 번 틀렸고 스스로 정정했다(§3-4)** — 그 정정이 맞는지도 봐달라
3. **§4 — `Server_ReorderEntry`를 `MoveEntry`와 별도 RPC로.** 10차 §3(`SwapEntries`)과 **같은 종류의 질문**이고, CLAUDE.md §2는 함수를 늘리는 것에 보수적이다
4. **★ §6 — 범위.** Step 03 완료 조건이 **13 → 17개**가 됐고 03-A가 또 무거워졌다. Step 03은 사용자가 **최대 1주**로 잡은 단계다

---

## 1. 현재 상태 (사실만)

### 1-1. 진행

| | 상태 |
|---|---|
| Step 00~02 | ✅ 구현·검증 완료 |
| Step 03 | **골격만.** `EPInventoryTypes.h` 필드 5개, `EPInventoryComponent.h` 선언 완료(문서와 일치), `.cpp`는 **생성자 + 빈 함수 스텁**. 로직 0줄 |
| Step 04·05 | 미착수 |

**되돌리는 비용이 여전히 0이다.** 코드가 없다.

### 1-2. 헤더가 이미 문서와 동기화돼 있다 (확인함)

`EPInventoryComponent.h`의 클래스 본문 선언 45줄이 10차 문서와 **문자 단위로 일치**한다. 어긋난 것은 파일 앞부분뿐이고 사용자에게 보고했다.

| | |
|---|---|
| `#include "Inventory/EPInventoryTypes.h"` 누락 | 사용자가 이미 고침 |
| `class UEPInventoryComponent;` / `AEPPickup;` / `UEPItemDefinitionSubsystem;` **전방선언 3개** | **문서에도 없다 — 문서 쪽 구멍이다** (§7-4) |
| `TArray<class FEPInventoryEntry>` 의 `class` (struct인데) | MSVC C4099 |

### 1-3. 확인된 사실 (다시 파지 말 것 — 전부 이번 세션 직독)

| 사실 | 출처 |
|---|---|
| FastArray 변경 항목은 **기본적으로 프로퍼티 델타**로 나간다 | `FastArraySerializer.h:218-219` 주석 + `:1398-1401` 분기 → `:1645` `FastArrayDeltaSerialize_DeltaSerializeStructs` |
| `:1474-1485`의 `NetSerializeStruct`(구조체 전체)는 **폴백 경로**다 | 같은 파일. `bSupportsFastArrayDeltaStructSerialization && HasDeltaBeenRequested`가 아닐 때 |
| 클라 배열 순서는 서버와 같다는 **보장이 없다** | `FastArraySerializer.h:54` |
| Lyra 인벤토리·장비에 **순서/정렬 필드가 하나도 없다** | `grep -rn "SortOrder\|SortIndex\|SortKey\|DisplayOrder" LyraGame/` = **0건** |
| `DECLARE_MULTICAST_DELEGATE`는 UHT가 안 보는 **평범한 typedef** | `Delegate.h:212-213` |
| 엔진에서 델리게이트를 **클래스 안**에 선언하는 것이 다수 | `Source/Runtime` `.h`: 파일 스코프 238 / 들여쓰기 417 |
| `NextEntryId`는 컴포넌트 필드고 **초기값 1** | `EPInventoryComponent.h:106` |
| 로드맵 **14번(USaveGame)·15번(DB)** 은 마감 전 구현 순서에 **없다** | `DOCS.md §5`, 사용자 확정 순서(인벤토리 → AI → 탈출 → 로비) |

### 1-4. 10차 판정 중 다르게 반영한 것 (배경 — 재론 대상 아님)

`SwapEntries` 용량식의 **증상 판정 역전**(넘김 → 성립하는 교환의 거절), 답변 수정안에 남아 있던 `Clamp(-1,0,N)` 오류, `PA == PB` 합산의 근거 재서술, 줄 번호 넷. 전부 `LOOT_STATUS.md` 10차 항목에 기록돼 있다.

---

## 2. ★ 최대 주제 — 10차의 클라 로컬 설계는 "지속"을 줄 수 있었나

### 2-1. 10차가 확정한 것

```
순서       = 클라이언트 로컬
지속       = UEPInventoryLayoutSave : ULocalPlayerSaveGame   (디스크)
런타임 홀더 = UEPInventoryLayout : ULocalPlayerSubsystem
대조       = Resolve() — 저장된 EntryId 목록과 살아있는 엔트리를 매 갱신 대조
```

근거는 **"서버 로직 중 아이템 순서를 보는 곳이 0곳"** 이었고, 답변이 `ULocalPlayerSaveGame`을 제시하며 *"재접속·기기 변경 시 순서 소실이라는 대가를 받아들일 필요가 없다"* 고 했다.

### 2-2. ★ 찾은 구멍 — `EntryId`가 세션을 넘지 못한다

```
NextEntryId 는 UEPInventoryComponent 의 필드고 초기값이 1이다
  → 매치가 끝나면 컴포넌트가 죽고, 다음 매치는 1번부터 다시 발급한다
  → 그런데 ULocalPlayerSaveGame 은 디스크에 남아 매치를 넘어 산다

지난 매치:  Order[7] = [3, 12, 5]     ← 7번은 배낭, 안에 붕대·소총·현금
이번 매치:  7번은 다른 사람의 조끼, 3·12·5도 전혀 다른 아이템
            Resolve 는 Live.Contains(3) 만 본다 → 통과시킨다 → 엉뚱한 순서
```

**`Resolve`는 번호만 대조하므로 이걸 못 막는다.** 증상은 *"가끔 순서가 이상하다"* 이고 재현 조건이 번호 충돌이다.

**Step 03 문서의 보장이 여기서 안 통한다.** 완료 조건의 *"`EntryId`가 재번호되지 않는다"* 와 04-8의 *"낡은 번호가 엉뚱한 아이템을 가리킬 수 없다"* 는 **한 세션 안에서만 참**이다.

### 2-3. 세션 도장을 찍으면 지속이 사라진다

```cpp
UPROPERTY() FGuid SessionId;    // 불일치면 Order 를 비운다
```

로드맵 14번(서버 세이브가 `NextEntryId`를 보존)이 오기 전까지는 **매치 시작마다 무조건 불일치**다. 즉 저장이 **한 번도 발휘되지 않고** 결과가 10차 이전 설계(서브시스템이 인메모리로 듦)와 같아진다.

**→ Claude의 결론: 클라 로컬은 "지속"을 줄 수 없다. 10차가 청구한 이득이 로드맵상 성립하지 않는다.**

### 2-4. 서버에 두면 저장 코드가 0줄이다

로드맵 5단계가 **엔트리 배열을 저장한다** — 03-2가 이미 `NextEntryId`·`ActiveHotbarIndex`를 세이브 목록에 넣어뒀다. `SortKey`가 엔트리의 `UPROPERTY`면 **그냥 따라간다.**

부수로 **버린 배낭을 되주울 때 내용물 순서가 산다** — 클라 로컬로는 불가능했다(`EntryId`가 전부 새로 발급되므로).

### 2-5. ★ Claude의 첫 권고가 틀렸다 (그대로 적는다)

Claude는 **"지금은 옮기지 마라"** 를 권했다. 근거 셋:

| 근거 | 사후 판정 |
|---|---|
| 서버 세이브(14번)가 마감 범위에 없어 **매치 안에서는 이득 0** | **사실이다.** 지금 당장 보이는 기능 차이는 §2-4의 배낭 되줍기뿐 |
| 5단계가 오면 그때 이식이 싸다 (`SortKey`는 필드 하나) | **사실이다** |
| 포트폴리오 답변으로도 클라 로컬 쪽이 할 말이 많다 | 주관 |

**그런데 이행 비용을 과대평가했다.** 옮기고 나니 `Resolve`·`MoveTo`·세이브 클래스 둘·`FEPContainerOrder`와 **함정 5건이 사라져 코드량이 오히려 줄었다.** *"지금 안 해도 나중에 싸다"* 는 논거가 *"지금 하면 지금도 싸다"* 를 가렸다.

### 2-6. 판정 요청

- **§2-2의 세션 충돌이 실재하는가.** `NextEntryId` 초기값 1 + 컴포넌트 수명 + `ULocalPlayerSaveGame` 수명, 이 셋의 조합이 맞는지
- **10차 답변의 `ULocalPlayerSaveGame` 제안이 잘못된 것인가, 아니면 "세션 도장 + 5단계 대기"가 여전히 나은 선택인가.** 후자라면 11차 전체가 되돌아간다
- **놓친 제3의 안이 있는가.** 예: 서버가 세션 GUID를 복제하고 클라가 그걸 키에 섞는 형태 — 검토했으나 *"서버가 이미 개입하는데 순서만 클라에 두는 이유가 없다"* 로 기각했다

> **기획 자체(*"저장했을 때도 순서를 유지한다"*)는 사용자 확정이라 재론 대상이 아니다.** 판정받을 것은 **그것을 클라 로컬로 달성할 수 있었는가**다.

---

## 3. ★ 키 배치 — `int32` 희소로 한 것

### 3-1. 채택안

```cpp
UPROPERTY() int32 SortKey = 0;                  // 형제(같은 Parent) 스코프
static constexpr int32 SortKeyStep = 1 << 16;   // 65536

// 맨 뒤 : MaxSiblingKey + Step        맨 앞 : FirstKey - Step
// 사이  : (PrevKey + NextKey) / 2
// 고갈  : RenormalizeSortKeys(Container) → 0, Step, 2*Step...
```

- **형제 안에서만 비교한다.** 다른 컨테이너와 값이 겹쳐도 무관
- **`SlotId`가 있으면 무의미하다** — 슬롯이 곧 자리
- **상한:** 65536 간격 × 32767개가 `int32`에 들어간다

### 3-2. 기각한 셋

| 안 | 기각 이유 |
|---|---|
| **조밀 정수 재번호**(0,1,2…) | 드래그 한 번에 형제 N개 dirty. 가방 20칸이면 무해하지만 **로드맵 14번의 스태시**(타르코프 기준 280칸)에서 매 드래그 N개 changelist 비교 + 전송 |
| **`double` 분수 키** | 이분 여유가 52회로 늘지만 **로드맵 15번(외부 DB, REST)** 왕복에서 부동소수가 알려진 지뢰이고 `EP.Inv.Dump`에 `0.37500000000000006`이 찍힌다. 16회 vs 52회는 실사용에서 **둘 다 "거의 안 남"** 이라 남는 차이가 그쪽뿐이다 |
| **컨테이너가 `TArray<int32> ContentOrder`를 듦** | dirty 1개는 같지만 ① **본체(`INDEX_NONE`)는 엔트리가 없어** 컴포넌트에 별도 배열이 필요하다(같은 개념이 두 집 — CLAUDE.md §2) ② 아이템이 죽을 때 **부모 배열 청소**가 생긴다. `SortKey`는 엔트리와 함께 죽어 **자가 청소**된다 |

### 3-3. ★ 분수 키는 도메인이 다른 도구가 아닌가

Figma·Linear·Jira(LexoRank)가 fractional indexing을 쓰지만 그건 **여러 사람이 동시에 편집하는 무한 목록**이고, 동시 삽입 충돌을 감수하는 대신 서버 왕복을 없애는 거래다. **우리는 서버가 유일한 발급자라 그 거래가 성립하지 않는다.**

그리고 **실제 게임 인벤토리는 전부 절대 위치**다 — 타르코프 `location:{x,y,r}`, 마인크래프트 슬롯 인덱스, WoW bag+slot. **상대 키를 쓰는 게임 인벤토리 선례를 Claude가 못 찾았다.** Lyra에도 순서 필드가 0건이다(§1-3).

**이게 §8 조사 요청의 핵심이다.** 우리 컨테이너는 가변 칸이라 절대 위치를 정의할 수 없어(04-0) 상대 키가 강제되는데, **그 전제 자체를 다시 봐야 하는지** 판단해달라.

### 3-4. ★ Claude가 인용을 한 번 틀렸고 정정했다

처음 사용자에게 이렇게 말했다.

> *"FastArray는 바뀐 항목의 구조체 전체를 보낸다"* — `FastArraySerializer.h:1474-1485`의 `NetSerializeStruct`

**그건 폴백 경로였다.** 기본은 `:1398-1401` → `:1645`이고 **바뀐 프로퍼티만** 나간다(`:218-219`).

| | 처음 말한 것 | 정정 후 |
|---|---|---|
| 스태시 280칸 조밀 재번호 | 12.6KB / 드래그 | **~3.4KB + 280개 changelist 비교** |

**방향은 같고 크기가 4배 작다.** 정정 후에도 희소를 유지한 이유는 *"대역폭보다 서버 CPU(항목마다 이전 전송 상태 비교)가 먼저 보인다"* 인데, **3.4KB가 조밀 재번호를 기각할 만큼인지**를 판정해달라. 아니라면 조밀 재번호가 더 단순하다 — **재정규화 코드가 통째로 사라진다.**

### 3-5. 판정 요청

- **`int32` 희소 vs 조밀 재번호.** 3.4KB + N개 changelist 비교가 재정규화 코드값을 하는가
- **`double` 기각 근거가 서는가** (DB/REST · Dump 가독성)
- **컨테이너 배열 기각 근거가 서는가** (본체에 엔트리가 없다 · 자가 청소)
- **§3-4의 경로 판정이 맞는가** — 기본이 정말 델타 경로인가

---

## 4. `Server_ReorderEntry`를 `MoveEntry`와 별도 RPC로

### 4-1. 제안

```cpp
UFUNCTION(Server, Reliable)
void Server_ReorderEntry(int32 EntryId, int32 PrevEntryId);   // INDEX_NONE = 맨 앞
```

| | `MoveEntry` | `Server_ReorderEntry` |
|---|---|---|
| 바꾸는 필드 | `ParentEntryId` + `SlotId` | `SortKey` |
| 용량 판정 | **한다**(검사 5) | 하지 않는다 — 컨테이너를 안 떠난다 |
| 슬롯 정합 / 사이클 | 한다 | 해당 없음 |
| **정상 클라에서 실패하나** | **한다**(가방이 차면) | **안 한다** |

마지막 줄이 설계를 가른다 — **재배치는 실패할 수 없어** 클라가 낙관적으로 먼저 그려도 롤백이 없다.

### 4-2. ★ 인덱스가 아니라 앞 이웃을 받는 이유

인덱스는 클라·서버 목록이 한 칸이라도 어긋나면 틀린 자리에 놓는다. **이 게임에서는 이론이 아니다** — `bFungible`이 현금·탄약을 조용히 합치고(03-3), 자동 획득이 드래그 도중 목록을 밀 수 있다.

부수 이득 둘: 검증이 *"Prev가 같은 컨테이너의 수납 형제인가"* **한 줄**이고, **다중 선택 드래그로 넓힐 수 있다**(`TArray<int32>`). 인덱스는 넓힐 방법이 없다.

### 4-3. 고려한 대안

| 대안 | 기각 |
|---|---|
| `MoveEntry`에 4번째 인자 추가 | 검사 7개 중 5개가 재배치에 무의미하고, **실패 가능성이 달라진다**(4-1 마지막 줄) |
| 클라가 `SortKey` 값을 직접 보냄 | 겹치거나 범위 밖 값을 심을 수 있다 (함정 13f) |

### 4-4. 판정 요청

- **10차 §3의 `SwapEntries` 판정과 같은 기준이 적용되는가.** 그때 답변은 *"`MoveEntry` 두 번으로 안 되는 이유가 실재하므로 별도 함수가 맞다"* 였다
- **`MoveEntry` / `SwapEntries` / `Server_ReorderEntry` 셋이 되는데 많은가.** 셋을 하나로 합칠 형태가 있는가
- **앞 이웃 vs 인덱스**

---

## 5. `bIsRoot`가 `ParentEntryId`와 `SortKey`를 동시에 관장하는 것

### 5-1. 제안

```cpp
// RemoveEntryInternal ② — 스냅샷
if (bIsRoot) { Snapshot.ParentEntryId = INDEX_NONE; Snapshot.SortKey = 0; }

// AddSubtree — 복원
if (Src.ParentEntryId != INDEX_NONE) AssignSortKey(NewId, Src.SortKey);   // 자식만
```

**루트**는 목적지 컨테이너의 키 체계로 들어가므로 버리고, **자식**은 *"새로 만들어질 빈 부모"* 안으로 들어가므로 보존한다. 그래서 **버린 배낭을 되주우면 내용물 순서가 그대로 산다.**

### 5-2. ★ 안 하면 나는 증상

`AddSubtree`가 `InsertEntry`의 "맨 뒤 발급"을 그대로 두면 순서가 **`In` 배열 순서**로 정해지는데, `In`은 `RemoveChildrenRecursive`가 `Entries.Items`를 순회해 만든 것이라 **FastArray 내부 순서**다. 화면 순서가 아니다. **아이템이 두세 개면 안 보인다.**

### 5-3. 판정 요청

- **자식 키 보존이 안전한가.** 근거는 *"부모가 방금 만들어진 빈 컨테이너라 형제 충돌이 없다"* 인데, `AddSubtree`가 **다른 인벤토리에서** 오는 경우(`Container->RemoveEntry` → `MyInv->AddSubtree`, 03-4)에도 참인가
- **`bIsRoot` 하나가 두 필드를 관장하는 것이 적절한가**, 아니면 `SortKey`는 별도 플래그/단계여야 하는가

---

## 6. ★ 범위 — Step 03이 또 커졌다

### 6-1. 숫자

| | 8차 | 9차 | 10차 | **11차** |
|---|---|---|---|---|
| Step 03 완료 조건 | 13 | 13 | 13 | **17** |
| Step 04 완료 조건 | — | — | 14 | **15** |
| `05_Loot_03_Inventory.md` | — | — | — | **1763줄** |
| `05_Loot_04_InventoryUI.md` | 347 | — | 715 | **934줄** |

**03-A에 이번에 들어온 것:** `SortKey` 필드 · `AssignSortKey` · `GetSortedContents` · `Server_ReorderEntry` · `RenormalizeSortKeys` · `NextKeyAtEndOf`/`KeyOf` · `EP.Inv.Reorder` 커맨드 · `Dump`의 `SortKey` 열.

9차에서 이미 `MoveEntry`(검사 7)·`GetEntryInSlot`·`BodySlots`·`SlotPriority`가 03-A로 왔다.

### 6-2. 상쇄되는 것

Step 04에서 **`Resolve`·`MoveTo`·`UEPInventoryLayoutSave`·`UEPInventoryLayout`·`FEPContainerOrder`와 함정 5건이 사라졌다.** 총량은 줄었지만 **03-A로 앞당겨졌다.**

### 6-3. 판정 요청

- **03-A가 한 덩어리로 감당 가능한가.** 03-D로 더 쪼개야 하는가
- **`SortKey` 일습을 03-A에 두는 근거가 서는가.** 근거는 *"`InsertEntry`가 발급하고 `MoveEntry`가 재발급하므로 정의가 뒤에 있으면 03-A가 컴파일되지 않는다"* 인데, **`Server_ReorderEntry`·`RenormalizeSortKeys`·`EP.Inv.Reorder`까지 03-A여야 하는가** — 이 셋은 컴파일 의존이 아니라 *"UI보다 먼저 닫는다"* 가 근거다
- **사용자가 Step 03을 최대 1주로 잡았다.** 이 범위가 현실적인가

---

## 7. 파급 — 이번에 바뀐 것

### 7-1. `05_Loot_03_Inventory.md` (1763줄)

| 위치 | 변경 |
|---|---|
| 03-1 | `FEPInventoryEntry::SortKey` 필드 + **설계 절 신설**(형제 스코프 · 희소 근거 · 기각 셋 · 2D 이행) |
| 03-1 | *"UI는 `EntryId` 오름차순으로 그린다"* → **`SortKey` 오름차순.** *"`EntryId`는 식별자이지 순서가 아니다"* |
| 03-2 | 선언 6개 추가, **단일 쓰기 지점 표에 5번째 행**(`AssignSortKey`) |
| 03-2 | `MoveEntry`에 **부모 변경 시 재발급** 절, `Server_ReorderEntry`/`GetSortedContents`/`RenormalizeSortKeys` 구현 절 |
| 03-2 | `RemoveEntryInternal` ②에 **루트/자식 키 분기**, 4단계 표 · `AddSubtree` 계약 갱신 |
| 03-3 | `InsertEntry`가 키 발급 + `NextKeyAtEndOf` |
| 03-4 | `AddSubtree`가 자식 키 복원 |
| 03-9 | `Dump`에 `SortKey` 열(행을 키 순으로) + `EP.Inv.Reorder` |
| 함정 | **4m~4p 4건 추가** |
| 완료 조건 | **13 → 17** |

### 7-2. `05_Loot_04_InventoryUI.md` (934줄)

04-8 **전면 교체**(클라 로컬 → 서버). `HandleDrop`이 `Server_ReorderEntry`로. 앞 이웃 변환 절 + 낙관적 적용 절 신설. **함정 13~13f 전면 개편, 12d는 13으로 흡수.** 완료 조건 15개.

### 7-3. 다른 문서

| 파일 | 변경 |
|---|---|
| `LOOT_STATUS.md` | 확정표 순서 항목을 **2행 → 9행**으로 재작성, 11차 이력 + Claude 권고가 뒤집힌 기록 |
| `05_Loot_03_Inventory_STATUS.md` | 11차 코드 항목 절 신설 |
| `05_Loot_DOCS.md` | UI 정렬 키 행 |
| `EquipmentSlots.md` | **§13 신설** (781줄) |

### 7-4. ★ 아직 안 한 것 둘 — 이것도 판정해달라

1. **`SwapEntries`의 `SortKey` 교환이 04-7에 한 줄로만 있다.** 함수 본문이 04-B라 지금 채우면 04-A/04-B 경계를 넘는다고 판단해 미뤘다. **맞는 판단인가**
2. **전방선언 3개가 03-1·03-2 코드블록에 없다**(§1-2). CLAUDE.md 관례(*"Forward declarations in headers"*)와 어긋나는데, **문서 코드블록이 그 수준까지 적어야 하는가**

---

## 8. ★ 실무 조사 요청

1. **상대 정렬 키를 쓰는 게임 인벤토리 선례가 있는가.** Claude가 찾은 것은 전부 **절대 위치**다(타르코프 `location:{x,y,r}` / 마인크래프트 슬롯 인덱스 / WoW bag+slot). Lyra는 순서 개념 자체가 없다(`grep` 0건). **가변 칸 컨테이너에서 순서를 어떻게 표현하는 것이 실무인가**
2. **UE에서 "복제되는 정렬 키"의 관례가 있는가.** `FFastArraySerializerItem`을 상속한 구조체에 순서 필드를 두는 엔진/Lyra/플러그인 사례
3. **희소 정수 키의 재정규화가 실무에서 실제로 도는가.** 문서에는 *"~16회 연속 같은 틈"* 이라 적었는데, 실제 사용 패턴에서 도달 빈도가 어느 정도인가
4. **`ULocalPlayerSaveGame` + 서버 발급 ID 조합의 알려진 함정.** §2-2의 세션 충돌이 일반적으로 알려진 패턴인가

---

## 9. 재론하지 않았으면 하는 것

| | 근거 |
|---|---|
| `FEPItemState` 값 타입 / 스택 폐지 / `bFungible` 합치기 | 1·2차 확정 |
| DT/DA 두 계층 / 전역 참조를 `UEPLootDeveloperSettings`에 | 3·6차 확정 |
| Step 02가 `UEPGA_Interact` / 드랍이 `Server_DropItem` 직접 RPC | 7·8차 확정, 일부 구현 완료 |
| `EntryId`(int32, 서버 발급, 세션 안에서 재번호 없음) / `ParentEntryId` 평면 표현 | 1·2차 확정 |
| `SlotId`가 장착의 유일한 진실 / `MoveEntry` / `GetEntryInSlot` / `SlotPriority` / `BodySlots` | 9차 확정 |
| Step 04를 04-A/04-B로 분할 / 드롭 버블링(`HitTestCell` 폐기) / `FEPCellHit` / 빈 칸 폐기 / `UDynamicEntryBox` | **10차 확정.** 11차는 04-8만 건드렸다 |
| `SwapEntries` 용량식(`SlotId.IsNone()` 조건부 델타 + `PA==PB` 합산) | 10차 확정 + 증상 판정 정정 완료 |
| 2D 격자(타르코프식) 배치 | **사용자 확정으로 배제.** §3-3에서 *대비*로만 언급한다 |
| **"저장했을 때도 순서를 유지한다"는 목표 자체** | **사용자 확정(11차).** §2는 *"그것을 클라 로컬로 달성할 수 있었는가"* 를 묻는 것이다 |

---

## 10. 대상 파일

| 파일 | 관계 |
|---|---|
| **`DOCS/Notes/05/05_Loot_03_Inventory.md`** | **이번 변경의 본체.** 03-1 `SortKey` 절 · 03-2 구현 절 (1763줄) |
| **`DOCS/Notes/05/05_Loot_04_InventoryUI.md`** | 04-8 전면 교체 · 04-7 `HandleDrop` (934줄) |
| `DOCS/Mine/EquipmentSlots.md` §13 | 결정 기록 · Claude 권고가 뒤집힌 경위 |
| `DOCS/Notes/05/LOOT_STATUS.md` | 확정표 순서 9행 · 11차 이력 · 정정 기록 |
| `DOCS/Notes/05/05_Loot_03_Inventory_STATUS.md` | 11차 코드 항목 |
| `DOCS/Notes/05/Review/05_Loot_REVIEW10_Answer.md` | **뒤집히는 대상.** §5가 `ULocalPlayerSaveGame`을 확정했다 |
| `DOCS/DOCS.md §5` | 로드맵 14·15번 — §2-4의 전제 |
| `Public/Inventory/EPInventoryComponent.h` · `EPInventoryTypes.h` | 현재 골격 |
| `Private/Inventory/EPInventoryComponent.cpp` | 생성자 + 빈 스텁 |
| `FastArraySerializer.h:54, 218-219, 1398-1401, 1474-1485, 1645` | §3-4 경로 판정 |
| `GameFramework/SaveGame.h:41` · `LyraSettingsShared.h:56-64` | 10차 §5의 근거 |
