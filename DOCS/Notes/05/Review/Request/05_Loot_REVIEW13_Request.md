# 검수 요청 13차 — 자체 검수 21건을 반영했다. **그 반영이 맞는지**를 본다

> 작성일: 2026-08-25
> 12차: `05_Loot_REVIEW12_Request.md` / `_Answer.md` (**`ReorderEntry` 무한 재귀** — 맨 앞 드래그가 서버를 죽였다. 반영 완료)
> 자체 검수: **`Review/05_Loot_REVIEW_Inventory.md`** ← **이번 요청의 본체다. 먼저 읽어야 한다**
> 시점: **Step 03 골격만(로직 0줄), Step 04 미착수.** 12차 답변 반영 완료
> 성격: **다르다.** 이번엔 외부에서 결함을 찾아달라는 요청이 아니다. **자체 검수로 21건을 찾아 이미 다 반영했고**, 그 반영에 **사용자 결정 5건**이 섞여 있다. 검수 대상은 **그 결정과 반영의 타당성**이다

---

## 0. 사용자 입장 (먼저 밝힌다)

**12차 답변은 전부 수용해 반영했다.** 무한 재귀(함정 4t), 제자리 드롭 조기 반환(4u), `KeySpace_` 접두어, `FUIFrameworkStackBoxSlot::Index` 선례 — 전부 들어갔다.

**그 뒤에 03 문서(2062줄)를 소스와 대조하며 자체 검수했다.** 12차까지의 검수는 전부 *"새로 추가한 설계가 맞나"* 를 봤는데, 이번엔 **"12번의 검수가 층층이 쌓인 문서가 지금 그대로 구현 가능한가"** 를 봤다. 결과가 21건이고, 성격이 앞선 검수들과 다르다.

> **결함 대부분이 "결정"이 아니라 "결정 사이를 잇는 자리"에 있었다.**
> 9차(`SlotId` ＋ `MoveEntry`)와 11차(`SortKey`)가 **나중에** 들어오면서 앞 절이 갱신되지 않은 지점들이다.
> 예: 11차가 `Owner`의 타입을 바꾸고 **유일한 사용처를 안 고쳐서** `PostReplicatedReceive`가 컴파일되지 않는다.

**21건은 전부 문서에 반영했다.** 이 요청은 *"놓친 게 더 있나"* 도 묻지만, 본론은 **§2의 결정 5건**이다. 그중 셋은 이전 검수의 결론을 **다르게 판정**했거나 **범위를 넓혔다.**

---

## 이번에 볼 것 — 넷

| | 무엇 | 왜 |
|---|---|---|
| **§1** | 자체 검수 21건의 **판정이 맞나** | 특히 A-1·A-2·B-1 세 개는 *"컴파일되는데 조용히 틀린다"* 부류다. 재현 논리를 확인받고 싶다 |
| **★ §2** | **사용자 결정 5건** | 본론. 셋이 앞 검수의 결론과 **다르다** |
| **§3** | **구간(03-A/B/C) 재조정** | 9차가 다섯 개를 03-A로 **올린** 규칙을, 이번엔 셋을 03-C로 **내리는** 데 썼다 |
| **§4** | **놓친 것이 더 있나** | 자체 검수의 한계 — 같은 사람이 쓴 문서를 같은 사람이 본다 |

**§1은 `Review/05_Loot_REVIEW_Inventory.md`에 전부 적혀 있다. 그 문서가 이 요청의 본체이고, 아래는 요약과 판정 요청이다.**

---

## 1. 자체 검수 21건 — 요약

| 구분 | 건수 |
|---|---|
| A. 구현하면 즉시 깨진다 | 5 |
| B. 조용히 틀린다 | 4 |
| C. 검증 도구·구간 구조 | 4 |
| D. 문서 정합 | 8 |

### 1-1. 특히 확인받고 싶은 셋

#### ① A-1 — `MoveEntry`가 `FindEntry`의 **복사본**에 쓰고 있었다

```cpp
// 03-2 검사 6 (사이클) — 지역 복사본을 E라는 이름으로 만든다
for (int32 P = NewParent; P != INDEX_NONE; )
{
    if (P == EntryId) return false;
    FEPInventoryEntry E;                          // ★ 여기
    P = FindEntry(P, E) ? E.ParentEntryId : INDEX_NONE;
}

// 03-2 "검사를 다 통과한 뒤" — E가 선언된 적이 없다
const int32 OldParent = E.ParentEntryId;          // ★ 위 E를 그대로 잇는다
E.ParentEntryId = NewParent;
E.SlotId        = NewSlotId;
Entries.MarkItemDirty(E);
```

이 문서는 **`FindEntry`가 값 복사**라고 두 곳에서 못박았다. 그러면 위 블록은 **부모 사슬을 걷다 마지막으로 방문한 조상의 복사본**에 쓰고, `MarkItemDirty`는 **배열 밖 임시 객체**를 건드린다.

- **컴파일된다. `MoveEntry`는 `true`를 반환한다. 배열은 그대로다**
- 증상: 배낭을 매도 아무 일이 없는데 반환값은 성공 → *"`Server_EquipBackpack`이 안 불린다"* 로 오진
- 처방: `SetEntryCharges`와 같은 형태(`for (FEPInventoryEntry& E : Entries.Items)`)로 통일 ＋ 검사 6의 변수를 `Cur`로 개명

**확인하고 싶은 것:** 이게 실제 결함이 맞나. 문서만 보고 구현하면 이 코드가 나오나, 아니면 *"당연히 참조를 잡지"* 라고 볼 사안인가.

#### ② A-2 — 루트 스냅샷이 `SlotId`를 보존해 `MoveEntry`의 검사를 **우회**했다

```cpp
// RemoveEntryInternal ②
if (bIsRoot) { Snapshot.ParentEntryId = INDEX_NONE; Snapshot.SortKey = 0; }
//                                                   ↑ SlotId는 안 건드린다
// AddSubtree
InsertEntry(NewParent, Src.ItemId, Src.State, Src.SlotId);   // ← "Back"이 그대로 들어간다
```

**매고 있던 배낭을 버렸다 되주우면 `SlotId = "Back"`이 실려 들어간다.**

| 목적지 | 결과 |
|---|---|
| 본체 | 자동으로 매진다 — **우연히 맞다.** 그래서 완료 조건 9가 통과해 버린다 |
| 이미 다른 배낭을 맴 | `"Back"` 슬롯에 엔트리가 둘 → **유령 배낭** |
| 배낭 안 | **정확히 함정 4i의 상태** — 칸도 안 먹고 착용으로 잡힌다 |

9차가 함정 4i를 막으려고 `MoveEntry`에 검사 3(정합)·4(중복)를 넣었는데, **`AddSubtree`가 `MoveEntry`를 안 거친다.** 우회 경로가 문서 안에 있었다.

**그리고 동작이 내용물 유무로 갈렸다** — `In.Num() == 1`이면 `AddItem`으로 빠져 `SlotId`를 버리므로, **빈 배낭은 안 매지고 내용물이 든 배낭만 매진다.**

#### ③ B-1 — `SortKey` 함수가 `INDEX_NONE`을 실패 센티널로 쓴다

```cpp
int32 KeySpace_NextAbove(int32 Container, int32 Key, int32 Exclude) const;  // 없으면 INDEX_NONE
const bool bTail = (NextKey == INDEX_NONE);
```

**`EntryId`는 1부터라 안전하지만 `SortKey`는 −1도 0도 유효한 값이다.** 맨 앞 이동이 `KeySpace_Min - SortKeyStep`이라 키가 음수로 내려가고(12차가 인정한 성질), **`(−65536, 0)` 구간에 16회 꽂으면 정확히 −1이 나온다.**

```
−32768, −16384, −8192, −4096, −2048, −1024, −512, −256, −128, −64, −32, −16, −8, −4, −2, −1
                                                                                      ↑ 16회째
```

**완료 조건 18 ①(*"같은 틈에 16회"*)이 그 구간에서 돌면 만들어진다.** 키가 −1인 형제를 `KeySpace_NextAbove`가 **찾아도** 호출자는 *"없다"* 로 읽어 `bTail = true` → `NewKey = PrevKey + Step`. **한 칸 건너뛴 자리에 놓이고, `bNoGap`이 `bTail`일 때 꺼지므로 재정규화도 안 걸린다.**

처방: `bool` 반환 ＋ out 파라미터. `KeyOf`의 *"없으면 0"* 도 같은 모양이다.

**확인하고 싶은 것:** −1 도달 경로 계산이 맞나. 그리고 이게 *"도달 불가 분기의 방어"*(CLAUDE.md §2가 금지)인지, 아니면 **계약**인지.

### 1-2. 나머지 18건 (목록만 — 상세는 검토 문서)

| # | 무엇 |
|---|---|
| A-3 | **자동 착용이 `CanFit`을 지난다** — 본체를 경유하므로. *"등이 비었는데 배낭을 못 맨다"* |
| A-4 | `PostReplicatedReceive`가 **컴파일 안 됨** — 11차가 `Owner`를 `TObjectPtr<UActorComponent>`로 바꾸고 유일한 사용처를 안 고침 |
| A-5 | `MoveEntry`·`ReorderEntry`에 **스코프 가드 없음** — 03-7의 목록이 9차·11차를 못 따라감 |
| B-2 | `InsertEntry`가 `AddDefaulted` **뒤에** 키를 발급 — 자기 자신(`SortKey=0`)이 키 공간의 형제가 되어 *"빈 컨테이너면 0"* 분기가 죽는다 |
| B-3 | `Server_EquipBackpack`에 `CanMutateInventory()` 없음 — *"유일한 게이트"* 라 적어놓고 실제 호출은 `Server_DropItem` 하나뿐 |
| B-4 | `GetOwner<AEPCharacter>()->` **무보호 역참조** — §7-1 월드 컨테이너를 같은 문서가 이미 허용했다 |
| C-1 | **`MoveEntry`를 부르는 커맨드가 0개** — 완료 조건 15·17을 Step 03에서 검증할 수 없고, 11·12차가 가장 공들인 함정 4q·4m이 **Step 04까지 한 줄도 안 돈다** |
| C-2 | 완료 조건 18 ②의 관찰 문구가 **고쳐진 코드에서 거짓** — 맨 앞 20회로는 경계까지 32,767회 부족해 재정규화가 안 돈다 |
| C-3 | STATUS의 완료 조건 대조표가 **13행** — 11·12차의 6개가 없다 |
| C-4 | **03-B가 03-C의 `AddSubtree`를 부른다** (§3) |
| D-1 | `Server_EquipBackpack`의 `UFUNCTION` 유무가 03-2와 03-6에서 다름. **코드는 03-2를 따라 매크로 없이 들어갔다** → RPC가 아니다 |
| D-2 | 용량 예시가 §4-6의 `Capacity < SlotSize`를 위반 (§2-④) |
| D-3 | 함정표 꼬리 주석이 9차 확정 **이전** 문장 — 3b 행과 정면으로 모순 |
| D-4 | `함정 11b` 참조가 이 문서에 없다 (04 문서 번호) |
| D-5 | STATUS 골격 결함 #2가 11차 결정과 **반대 처방** |
| D-6 | 함정표에 `4a`가 없고 정렬이 뒤섞임 (12차가 본문 등장 순서로 끼워 넣음) |
| D-7 | `Dump`의 *"행은 `SortKey` 순"* 이 부모를 넘으면 뜻이 없다 |
| D-8 | **`DT_Items`의 `Backpack_Small` 행이 §4-6 위반 — 실물 에셋에 이미 들어가 있다** |

---

## 2. ★ 사용자 결정 5건 — 여기가 본론이다

### 2-①. `AddSubtree`의 시그니처 — **목적지를 `(Parent, SlotId)` 쌍으로, 기본값 금지**

```cpp
// 전
int32 AddSubtree(int32 Container, const TArray<FEPInventoryEntry>& In);
// 후
int32 AddSubtree(int32 Parent, FName SlotId, const TArray<FEPInventoryEntry>& In);
```

**근거를 두 번 바꿨다.** 처음엔 *"인자를 하나 늘린다"* 로 적었는데, 다시 보니 **빠진 걸 채우는 것**이었다.

```cpp
int32 InsertEntry(int32 Parent,  FName ItemId, const FEPItemState&, FName SlotId);   // 둘 다
bool  MoveEntry  (int32 EntryId, int32 NewParent, FName NewSlotId);                  // 둘 다
int32 AddSubtree (int32 Container, ...);                                             // ← 하나뿐이었다
```

**A-2와 A-3이 둘 다 여기서 터진 것이 우연이 아니다.** *"슬롯 정보를 어디서 얻나"* 에 답이 없으니 한쪽은 **스냅샷에서 몰래 새어 들어오고**(A-2), 다른 한쪽은 **아예 표현이 안 됐다**(A-3).

**기본값(`FName RootSlotId = NAME_None`)을 주지 않는다.** A-2가 난 원인이 정확히 *"아무도 슬롯을 안 정해서 스냅샷 값이 흘러 들어간 것"* 인데, 기본값은 그 상태를 **다시 문법으로 허용한다.** 호출부는 셋뿐이다.

```cpp
AddSubtree(C,          NAME_None, Payload);   // OnInteract ② — 컨테이너 수납
AddSubtree(INDEX_NONE, S,         Payload);   // TryAutoEquip — 몸 슬롯
AddSubtree(INDEX_NONE, NAME_None, Sub);       // §7-1 월드 컨테이너
```

**판정 요청:** 인자 셋(그중 둘이 목적지)이 맞나. §7-1의 `AActor* Source`가 넷째로 붙을 날을 생각하면 **지금 구조체로 가야 하나**, 아니면 호출부가 셋뿐이니 리터럴로 충분한가.

### 2-②. 루트 스냅샷이 `SlotId`도 버린다 (A-2의 A안)

| | **A. 버린다 (채택)** | B. 슬롯째 복원 |
|---|---|---|
| 되줍기 | 컨테이너에 **아이템으로** 들어간다. 착용은 `TryAutoEquip`이 판정 | 착용 상태가 복원된다 |
| 검사 3·4 우회 | **사라진다** — 슬롯 진입이 `MoveEntry`/`TryAutoEquip` 둘로 고정 | 남는다. `AddSubtree`에 검증을 복제해야 한다 |

**사용자 판정:** *"Back이 비어 있으면 매지고, 차 있으면 아이템으로 들어가고, 아이템 자리도 없으면 못 줍는다."* — 셋 다 03-4의 ①`TryAutoEquip` → ②컨테이너 라는 **이미 있는 흐름**이고 스냅샷이 개입할 자리가 없다.

**대가:** *"Hotbar2에서 버린 무기가 Hotbar2로 돌아오지 않는다"*(`SlotPriority` 첫 빈 슬롯으로 간다). **`TryAutoEquip(In, FName PreferredSlot)` 한 인자로 나중에 붙일 수 있고 A안이 그걸 막지 않는다**고 판단해 지금은 안 만들었다.

**판정 요청:** A안이 맞나. 그리고 *"원래 슬롯 우선"* 을 **지금 안 만드는 것**이 CLAUDE.md §2 기준으로 맞나 — 이름은 문서에 적었지만 기획서에는 없다.

### 2-③. 알림 가드를 **"단일 쓰기 지점 표"** 에 건다 — 사설 래퍼 기각

03-7의 가드 목록이 **두 번 연속 낡았다.** 9차의 `MoveEntry`도 11차의 `ReorderEntry`도 안 올라왔다.

```
03-7 가드 목록 :  AddItem · AddSubtree · RemoveEntry · SetEntryCharges
단일 쓰기 지점 :  InsertEntry · SetEntryCharges · RemoveSelf · AssignSortKey · MoveEntry
```

**아래가 정확히 "`MarkItemDirty`/`MarkArrayDirty`를 부르는 함수 전부"이고, 그 표는 이미 성실히 관리된다** — 9차가 `MoveEntry`를, 11차가 `AssignSortKey`를 올렸고 *"`SwapEntries`도 이걸 두 번 부른다"* 까지 적혀 있다. **원인은 그게 따로 노는 두 번째 목록이었다는 것이다.**

| 층 | 어디 | 빠뜨리면 |
|---|---|---|
| ㉡ 재진입·필수 | **단일 쓰기 지점 다섯** | 알림이 안 간다 |
| ㉠ 배칭·선택 | 공개 진입점 | 알림이 여러 번 (03-7이 이미 "무해"로 판정) |

**기각한 안 — `MarkItemDirty`를 감싼 사설 래퍼(`DirtyItem`/`DirtyArray`).** 확장성은 가장 좋지만 **엔진 관례와 싸운다** — `MarkItemDirty`는 UE 표준 이름이라 문서·샘플·Lyra·자동완성이 전부 그것을 가리키고, `Entries`가 같은 클래스 멤버라 **강제할 문법도 없다.** 신호가 잘 보일 뿐 여전히 규율이다.

> **크기도 다시 쟀다.** 클라 UI는 `PostReplicatedReceive`로 받으므로(가드와 무관), **서버 쪽 `Broadcast`의 소비자는 리슨서버 호스트 / PIE 화면뿐**이다. 배낭 하나 버릴 때 12회 `Broadcast` = 20칸 그리드 재생성 12회라 **㉠은 실측하면 비용이 아니다.** 가드를 유지하는 이유는 **㉡** 이다.

**판정 요청:** 래퍼 기각이 맞나. 실무 UE 코드베이스에서 `MarkItemDirty`를 감싸는 사례가 있나 — **Lyra는 직접 부르는데, Lyra에는 배칭할 대상이 애초에 없다**(항목마다 메시지를 쏜다)는 것이 우리 판단이다.

### 2-④. ★ 용량표 — **본체 0칸.** 수납은 착용 컨테이너에서만

> **사용자 확정:** *"아무것도 안 입었을 때는 0칸이다. 지금은 테스트를 하고 있기에 본체에 10칸을 두지만 조금 있다 지울 것이다. 스폰 시 기본 상의와 하의를 입고 시작한다."*

| 아이템 | `SlotSize` (차지) | `ContainerCapacity` (제공) |
|---|---|---|
| **본체** | — | **0** ｜ `MaxSlots`. 테스트 중에만 10 |
| 기본 상의 | 11 | 10 |
| 기본 하의 | 6 | 5 |
| 배낭 A | 15 | 12 ｜ 어디에도 안 들어간다 — 의도 |
| 배낭 B | 10 | 8 ｜ 상의(10)·배낭A(12)에 들어간다 |

**이게 `GAME.md:158`의 *"플레이어 본체 인벤토리: 10칸"* 을 뒤집는다.** 기획서를 고쳤다.

#### 부등호가 둘이고 서로 다른 식이다

사용자가 처음 *"컨테이너는 차지하는 크기 **>=** 수납 가능 크기"* 라고 했는데, **`>=`면 같은 사용자가 "당연히 안 된다"고 한 것이 가능해진다.**

```
넣기 판정   :  SlotSize(넣을 것)  ≤  Capacity(담을 것)     ← ≤ 다. B(10)를 상의(10)에
데이터 규칙 :  Capacity(X)        <  SlotSize(X)           ← < 다. 등호 금지

SlotSize(A) ≤ Capacity(B) < SlotSize(B)   ⇒  SlotSize(A) <  SlotSize(B)   깊이 유한 ✅
SlotSize(A) ≤ Capacity(B) ≤ SlotSize(B)   ⇒  SlotSize(A) ≤  SlotSize(B)   깊이 무한 ❌
```

`SlotSize 10 / Cap 10`짜리 행 하나면 **그 가방이 자기 안에 들어간다.** 용량은 안 늘지만(±0) **깊이가 안 막혀** `RemoveEntry` 재귀·UI 중첩·세이브가 상한을 잃는다. 확정한 값은 전부 `<`를 만족하고 `IsDataValid()`도 이미 `<`다.

**판정 요청 둘.**
1. **깊이만 무한하고 용량은 안 느는 경우(`Cap == SlotSize`)를 정말 막아야 하나.** 익스플로잇으로 성립하나, 아니면 과잉인가
2. **상의·하의가 컨테이너를 잃는 안**(§8 미정 #8 — *"가방과 외투만 남을 가능성"*)의 이행이 **데이터 둘**(DT `Capacity` 0 ＋ `ContainerOrder`에서 제거)로 끝난다고 판단했다. 코드가 정말 0줄인가

### 2-⑤. 시작 장비 — `UEPLootDeveloperSettings::StartingEquipment`

```cpp
UPROPERTY(config, EditAnywhere, Category = "Inventory")
TArray<FName> StartingEquipment;      // ["Shirt_Basic", "Pants_Basic"]
```

**본체가 0칸이라 아무것도 안 입고 스폰하면 첫 아이템도 못 줍는다.** 캐릭터 `BeginPlay`(서버)에서 `SlotPriority`대로 `TryAutoEquip`을 부른다 — 결정 ①로 `AddSubtree(INDEX_NONE, SlotId, In)`가 이미 그 모양이라 새 경로가 아니다.

**사용자 의도:** *"로비 같은 게 생기면 로비에서 원하는 옷을 입고 나오겠지만, 지금은 테스트 환경이니 `UEPLootDeveloperSettings`가 맞아 보인다."* 로비가 오면 **배열을 채우는 쪽만** 바뀐다.

**판정 요청:** 6차의 *"전역 데이터 참조는 `UDeveloperSettings`"* 확정과 같은 자리로 봐도 되나. `GameMode`/`PlayerStart`/`DataAsset` 쪽이 나은 자리인가.

---

## 3. 구간 재조정 — 9차 규칙을 **반대 방향**으로 썼다

**`TryAutoEquip`(03-B)이 `AddSubtree`(03-C)를 부른다.** 결정 ①이 슬롯 경로를 시그니처에 올리면서 드러났을 뿐, 초안에도 있던 의존이다. 그리고 완료 조건 7의 앞 절(*"배낭을 **주우면** 자동으로 매진다"*)은 줍기 경로(`OnInteract` = 03-C)를 요구해 **03-B 단독으로는 검증할 수 없다.**

| 안 | 새 커맨드 | 죽은 코드 |
|---|---|---|
| ㉮ `AddSubtree`를 03-A로 올린다 | **＋1** (`EP.Inv.AutoEquip`) | `AddSubtree`가 03-A·03-B 내내 안 돈다 |
| **㉯ `TryAutoEquip`을 03-C로 내린다 (채택)** | **0** | **없다** |

**㉮가 아무것도 고치지 못하는 것이 판단 근거다.** `TryAutoEquip`의 진짜 호출자도 03-C라, 올려도 03-B에서 돌리려면 커맨드를 **또** 만들어야 한다 — C-1이 방금 지적한 *"Step 04까지 한 줄도 안 돈다"* 를 한 구간 규모로 재생산한다.

**9차는 다섯 개를 03-A로 올렸다.** 그때는 *"03-B가 쓰는 것이 03-A에 없으면 컴파일이 안 된다"* — **쓰는 쪽이 앞**이었다. 이번엔 **쓰는 쪽이 뒤**다. 기준은 같다: *"호출자와 같은 구간에."*

```
03-A  코어 ＋ SortKey 일습 ＋ MoveEntry ＋ GetCapacity(통째로) ＋ EP.Inv.Move
03-B  Server_EquipBackpack ＋ 다중 컨테이너 검증 (새 커맨드 0개)
03-C  줍기·버리기 ＋ AddSubtree ＋ TryAutoEquip ＋ StartingEquipment
```

**판정 요청:** ㉯가 맞나. 그리고 **03-B가 이 정도로 얇아지면 03-A에 합쳐야 하나** — 남는 것이 `Server_EquipBackpack`(3줄)과 검증뿐이다. 다만 *"두 번째 용량 풀이 처음 생기는 지점"* 이라는 검증 가치는 남는다고 봤다.

---

## 4. 놓친 것이 더 있나 — 자체 검수의 한계

**같은 사람이 쓴 문서를 같은 사람이 봤다.** 21건 중 상당수가 *"앞 절이 뒤 결정을 못 따라간 것"* 인데, 그 종류는 **쓴 사람이 가장 못 보는 것**이다.

특히 다음 축으로 봐 주면 좋겠다.

| 축 | 왜 |
|---|---|
| **11차·12차가 바꾼 것의 파급** | A-4(`Owner` 타입)와 A-5(가드 목록)가 **둘 다 "바꾸고 사용처를 안 고침"** 이었다. 같은 종류가 더 있을 것 같다 |
| **`SortKey`가 `int32`인 데서 오는 것** | B-1(센티널 −1)이 *"`EntryId`는 되는데 `SortKey`는 안 되는 것"* 이었다. 같은 혼동이 다른 함수에도 있나 |
| **본체 0칸의 파급** | 결정 ④가 방금 확정됐다. `GetInsertionOrder`·`CanFit`·UI 구획·완료 조건 2~6이 전부 본체 위에 서 있었다. 04·05 문서까지 훑었지만 확신이 없다 |
| **`EP.Inv.Move` 신설의 파급** | 03-A에서 `MoveEntry`가 처음 실행되면 **검사 0~6 일곱 개와 정합(4i)·사이클(4j)이 전부 처음 돈다.** 그 검사들이 지금 서로 모순 없이 짜여 있나 |

---

## 5. 반영 상태

**21건 전부 문서에 반영 완료.**

| 문서 | 무엇 |
|---|---|
| `05_Loot_03_Inventory.md` (2062 → 2343) | A·B·C 전부 ＋ 구간 재조정 ＋ 함정 **4v·4w·4x·4y·4z·9f** 신설 ＋ 함정표 번호순 재정렬 |
| `05_Loot_DOCS.md` | §4-6에 **본체 0칸 절 · 용량표 · 부등호 둘**, 예시 교체, §8 확정 3행 ＋ **미정 #8·#9**, §9 DeveloperSettings 필드 3개 |
| `GAME.md` | 인벤토리 절 재작성 ＋ 장비 절에 의류 슬롯 |
| `05_Loot_00_ItemCore.md` | DT 행 교체(D-8) ＋ `Capacity < SlotSize` 규칙 절 |
| STATUS 3종 ＋ `LOOT_STATUS.md` | 완료 조건 **13 → 19행**, 골격 결함 #2 처방 교정, 13차 코드 항목표, 구간표, 검수 이력 |

**남은 것은 문서가 아니라 에셋이다** — `DT_Items.uasset`의 `Backpack_Small` 행 교체 ＋ 옷 2행 추가 ＋ `DA_Shirt_Basic`/`DA_Pants_Basic` 생성.

---

## 참고 — 읽는 순서

1. **`Review/05_Loot_REVIEW_Inventory.md`** ← 21건 전문. **이게 본체다**
2. `05_Loot_03_Inventory.md` — 반영 결과 (§03-2 `MoveEntry` · §03-4 `AddSubtree` · §03-6 · §03-7 · §03-9)
3. `05_Loot_DOCS.md` §4-6 (용량표·부등호) · §8 (미정 #8·#9)
4. `05_Loot_03_Inventory_STATUS.md` — 13차 코드 항목표 · 구간표 · 완료 조건 19행

> **소스 상태:** `Public/Inventory/EPInventoryTypes.h` · `EPInventoryComponent.h` · `Private/Inventory/EPInventoryComponent.cpp` — **골격만. 함수 본문 대부분이 빈 스텁이다.** `RemoveEntryInternal`·`RemoveChildrenRecursive`·`RemoveEntry`만 본문이 있고, 그 셋도 이번 반영으로 바뀐다(A-2·B-4).
