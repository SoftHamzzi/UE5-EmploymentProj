# 장비 슬롯 · 핫바 · 인벤토리 그룹 — 설계 결정

> 작성일: 2026-08-22
> 근거: 사용자 기획 확정(언턴드 방식) + 프로젝트 코드 직독(`EPItemData.h:39-42`, `EPTypes.h:43-50`, `05_Loot_03_Inventory.md`) + `GAME.md` §인벤토리·§장비
> 관련: `DOCS/Notes/05/05_Loot_03_Inventory.md` (자료구조), `05_Loot_05_Equipment.md` (장착), `05_Loot_DOCS.md` §7-3·§8

---

## 0. 결론 먼저

**언턴드식으로 간다.** 인벤토리 용량은 **착용한 옷에서 나온다.** 장비 슬롯은 칸을 먹지 않는다.

**자료구조는 바꾸지 않는다.** `ParentEntryId` + `SlotId` 조합이 장비·핫바·부착물·컨테이너를 전부 표현한다. `05_Loot_03_Inventory.md` 03-1의 `FEPInventoryEntry`가 그대로 쓰인다.

**바뀌는 것은 다섯이다.**
1. `EquippedEntryId` / `EquippedBackpackEntryId` **필드를 없앤다** — `SlotId`가 유일한 진실이 된다 (§3)
2. 핫바가 **두 종류**로 갈린다 — 1~4는 장착, 5~0은 바로가기 (§4)
3. `FEPItemData`에 **`SlotPriority`** 가 생긴다 — 어느 슬롯에 들어갈 수 있는가 **＋ 그 순서가 우선순위** (§5)
4. **`MoveEntry()`** 가 엔트리 이동의 유일한 연산이 된다 — 장착·해제·드래그·부착이 전부 이것 하나다 (§6)
5. **`UEPLootDeveloperSettings::BodySlots`** 가 생긴다 — 몸에 붙는 슬롯인지 무기에 붙는 슬롯인지를 아는 **유일한 곳** (§6)

> **9차 검수(2026-08-22) 반영본이다.** 초안에서 바뀐 것은 §11에 모아뒀다. 큰 것 셋 — `GetEntryInSlot`에 **부모 인자**가 붙었고(§3), `SlotId`↔`ParentEntryId` **정합 불변식**이 생겼으며(§6), `HotbarRefs`는 **Step 03에서 빠졌다**(§4).

---

## 1. 슬롯 목록

### 1-1. 핫바 — 10칸

| 키 | `SlotId` | 종류 제한 | 배정 | 성격 |
|---|---|---|---|---|
| 1 | `Hotbar1` | 무기 전부 | 자동 | **장착** |
| 2 | `Hotbar2` | 무기 전부 | 자동 | **장착** |
| 3 | `Hotbar3` | 보조무기 | 자동 | **장착** |
| 4 | `Hotbar4` | 근접무기 | 자동 | **장착** |
| 5~0 | — | 제한 없음 | 드래그 | **바로가기** (§4) |

1~4는 직접 수정할 수 없다. 무기를 주우면 자동으로 들어간다. 교체·해제는 드래그로 한다 — 바깥으로 끌면 버린다.

### 1-2. 착용 — 8칸

| `SlotId` | 부위 | 인벤토리 그룹 |
|---|---|---|
| `Torso` | 상의 | ✅ |
| `Legs` | 하의 | ✅ |
| `Coat` | 외투 | ✅ |
| `Back` | 등 (가방) | ✅ |
| `Wrist` | 팔목 | ✅ (타르코프 Pouch 역할) |
| `Ears` | 귀 | ✗ |
| `Face` | 얼굴 | ✗ |
| `Feet` | 신발 | ✗ |

**아무것도 안 입으면 인벤토리가 0칸이다.** 그룹 크기는 아이템마다 다르다 (소형 배낭 10칸, 대형 20칸 등).

### 1-3. 부착물 — 무기에 매달린다 (§7-3, 깊이 1)

`Optic` / `Muzzle` / `Grip` / `Mag`

**부모가 무기 엔트리라는 것만 다르다.** 나머지는 위 둘과 동일한 표현이다.

---

## 2. 자료구조 — `SlotId` 하나로 전부 표현된다

```
상의        ParentEntryId = -1,      SlotId = "Torso"     ← 칸 안 먹음. 10칸을 연다
├─ 붕대     ParentEntryId = 상의,    SlotId = None        ← 상의 그룹의 칸을 먹는다
└─ 현금     ParentEntryId = 상의,    SlotId = None

배낭        ParentEntryId = -1,      SlotId = "Back"      ← 칸 안 먹음. 20칸을 연다
└─ 소총     ParentEntryId = 배낭,    SlotId = None

AK-74       ParentEntryId = -1,      SlotId = "Hotbar1"   ← 칸 안 먹음
└─ 스코프   ParentEntryId = AK-74,   SlotId = "Optic"     ← 칸 안 먹음
```

**규칙은 두 줄이다.**

```
SlotId != None   →  장비/부착이다. 칸을 먹지 않는다
ParentEntryId    →  어느 컨테이너에 속하는가 (-1이면 몸)
```

이미 그렇게 짜여 있다:

```cpp
// 05_Loot_03_Inventory.md:577  GetUsedSlots
if (E.ParentEntryId != Container) continue;
if (!E.SlotId.IsNone())           continue;   // ★ 이 한 줄이 장비·핫바·부착물을 전부 뺀다
```

**"컨테이너를 여는가"에 분기가 필요 없다.** `FEPItemData::ContainerCapacity`(`EPItemData.h:42`)가 0이면 안 열린다. 신발에 0을 주면 그것으로 끝이다.

---

## 3. 결정 ① — `SlotId`가 장착의 유일한 진실이다

### 없애는 것

```cpp
// 03-2 현행 — 삭제한다
UPROPERTY(Replicated) int32 EquippedEntryId         = INDEX_NONE;
UPROPERTY(Replicated) int32 EquippedBackpackEntryId = INDEX_NONE;
```

### 대신 파생 게터

```cpp
// ★ 부모 인자가 반드시 있어야 한다 — 아래 절
int32 GetEntryInSlot(int32 Parent, FName SlotId) const;      // 없으면 INDEX_NONE
int32 GetEquippedBackpack() const { return GetEntryInSlot(INDEX_NONE, TEXT("Back")); }
```

### ★ 부모 인자를 빼면 부착 슬롯에서 깨진다

**초안은 `GetEntryInSlot(FName)` 이었다. 8차의 `FindFungibleEntryId` 결함과 같은 모양이다.**

`Optic`은 **무기마다 하나씩** 있는 슬롯이다. AK와 M4를 둘 다 들고 AK에 조준경을 달면:

```
GetEntryInSlot("Optic")          →  AK의 조준경을 찾는다
MoveEntry(스코프2, M4, "Optic")
    └─ "Optic 슬롯이 비었는가" 검사 → 차 있다 → 실패
```

**M4에는 영원히 조준경을 못 단다.** 그리고 이 검사는 §6 `MoveEntry`의 핵심 검사다.

`Hotbar1`~`Hotbar4`와 착용 8칸은 몸에 하나뿐이라 **우연히** 맞는다. 부착 슬롯 4종에서만 틀리므로 **Step 03에서는 절대 안 걸리고 §7-3 부착물을 붙이는 날 나타난다.** 8차가 `FindFungibleEntryId(Container, ItemId)`로 얻은 교훈이 여기 그대로 적용된다(`LOOT_STATUS.md:69`).

### 왜

**진실이 두 곳에 있으면 갈린다.** `SlotId == "Back"`이 이미 "이 배낭은 등에 있다"를 말하는데 필드가 따로 있으면 둘을 동기화하는 코드가 생기고, 그 코드를 빠뜨리는 경로가 생긴다. CLAUDE.md §2의 *"한 값을 두 경로가 봐야 하면 둘 다 볼 수 있는 곳에 둔다"* 에 정면으로 걸린다.

**슬롯이 2개에서 12개로 늘어난다.** 필드 방식이면 필드가 12개가 되거나 `TMap<EEPEquipSlot, int32>`를 따로 복제해야 한다. `SlotId`는 **이미 엔트리 안에 있고 이미 복제된다.** 추가 대역폭이 0이다.

**`RemoveEntry`의 분기 두 개가 통째로 사라진다.**

```cpp
// 03-2 현행 — 번호 비우기 두 줄이 불필요해진다
if (EntryId == EquippedEntryId)         { ...; EquippedEntryId = INDEX_NONE; }
if (EntryId == EquippedBackpackEntryId) { EquippedBackpackEntryId = INDEX_NONE; }
```

엔트리가 배열에서 빠지면 파생값도 자동으로 사라진다. **죽은 번호를 남길 방법이 문법적으로 없다.**

> **함정표에서 두 항목이 삭제된다** — `05_Loot_03_Inventory.md`의 **3h**(`EquippedEntryId`를 `UnequipWeapon`에 맡김 → 죽은 번호)와 **5b**(`EquippedBackpackEntryId`를 안 비움 → 유령 구획). 둘 다 구조적으로 발생 불가능해진다. **이것이 이 결정의 실질 배당금이다.**
>
> **★ 3b는 Step 03 범위에서 표현 불가능해진다 — 9차 답변과 다르게 반영했다.** 9차는 *"3b는 남는다. write-back 소실은 값의 문제다"* 로 판정했으나 **장착의 정의가 바뀐 것을 반영하지 않았다.** 새 설계에서 장착은 `SlotId == "Hotbar1"` **＋** `ParentEntryId == INDEX_NONE`이다(§6의 `BodySlots` 불변식). 그러므로 *"배낭 속 무기를 **장착한 채**"* 라는 상태 자체가 만들어지지 않는다 — 장착하는 순간 `MoveEntry`가 배낭에서 꺼낸다.
>
> **사라진 것은 시나리오지 계약이 아니다.** 캐스케이드가 각 노드에서 검사하는 재귀 구조는 그대로 둔다. 함정표 3b의 문장은 **미정 #7로 옮긴다** — 핫바 5~0으로 컨테이너 안 아이템을 손에 드는 것이 허용되면 같은 모양이 되살아난다(§10).

### ★ write-back 순서가 새 의미로 중요해진다

**필드가 사라지면 `RemoveEntryInternal`의 4단계 순서가 더 강하게 걸린다.**

```cpp
// ① write-back — 스냅샷·제거보다 먼저
if (EntryId == GetEquippedEntryId())          // ← 파생값. 배열을 읽는다
    C->UnequipWeapon();                       // 비울 번호가 없다
// ② 스냅샷
// ③ RemoveSelf(EntryId)                      ← 이 줄 이후 GetEquippedEntryId()는 INDEX_NONE
// ④ 캐스케이드
```

현행은 `EquippedEntryId`가 **저장된 값**이라 `RemoveSelf` 뒤에 읽어도 살아 있었다. 새 설계에서는 `GetEquippedEntryId()`가 **배열에서 파생**되므로 `RemoveSelf` 이후에는 아무것도 못 찾는다.

**결과가 갈린다.** 이전에는 순서를 어기면 `INDEX_NONE`을 향한 write-back(= 눈에 보이는 버그). 이제는 **write-back이 아예 안 불린다**(= 잔탄이 조용히 사라짐). `05_Loot_05_Equipment.md:141`의 순서 규칙은 유지되지만 **근거 문장이 바뀐다** — *"`RemoveEntry`가 먼저 비우면"* 이 아니라 *"`RemoveSelf`가 먼저 돌면 대상을 못 찾는다"* 다.

### 대가

조회가 O(N) 선형 탐색이 된다. **N은 수십이고, 매 프레임 도는 경로가 아니다** — 장착 시점과 UI 갱신 시점에만 돈다. 사격마다 도는 `AddEntryCharges(GetEquippedEntryId(), -1)`도 비교 수십 회다.

캐시가 필요해지면 그때 붙인다. **캐시는 나중에 붙일 수 있지만 이중 진실은 나중에 못 뗀다.**

### 남는 진짜 상태 — 활성 핫바

**`SlotId`로 표현되지 않는 것이 하나 있다.** "1번과 2번 중 지금 어느 쪽을 손에 들고 있는가."

```cpp
UPROPERTY(Replicated) int32 ActiveHotbarIndex = INDEX_NONE;   // 0~9
```

이건 파생값이 아니라 **독립된 상태**다. 진실이 하나이므로 §2 위반이 아니다.

```cpp
int32 GetEquippedEntryId() const;   // ActiveHotbarIndex → 엔트리
```

---

## 4. 결정 ② — 핫바가 두 종류다

같은 UI 줄에 있지만 **자료구조가 다르다.** 이걸 하나로 합치려 하면 반드시 깨진다.

| | 1~4 | 5~0 |
|---|---|---|
| 아이템이 어디 있나 | **몸에** (인벤토리 밖) | **인벤토리 그룹 안** |
| 칸 | 안 먹는다 | **먹는다** |
| 표현 | `SlotId = "HotbarN"` — 엔트리 자신의 필드 | `EntryId`를 가리키는 **참조** |
| 배정 | 자동 | 드래그 |

```cpp
// ★ Step 04에서 선언한다. Step 03에는 넣지 않는다 (아래)
UPROPERTY(Replicated) TArray<int32> HotbarRefs;   // 6칸 고정. 인덱스 0~5 = 핫바 5~0. 값은 EntryId
// 생성자: HotbarRefs.Init(INDEX_NONE, 6);
```

**`TMap<int32,int32>`가 아니라 `TArray`인 이유:** 맵은 **빈 슬롯을 표현하지 못한다.** 5·7번만 걸려 있을 때 UI가 6칸을 그리려면 칸마다 `Contains`를 물어야 한다. 5~0은 균질하고 위치로 식별되므로 배열이 정확히 맞다 — Lyra의 `Slots`도 `NumSlots`로 길이를 고정한다(`LyraQuickBarComponent.h:66`).

### 5~0에 `SlotId`를 쓰면 안 된다

쓰는 순간 `GetUsedSlots`의 `if (!E.SlotId.IsNone()) continue;` 에 걸려 **그 아이템이 칸 계산에서 빠진다.** "인벤토리에 남아있고 단축키만 걸린다"는 기획이 조용히 깨진다. 증상은 *"붕대를 5번에 걸면 가방이 한 칸 늘어난다"* 이고, 원인이 UI로 보인다.

### 끊어진 참조를 견뎌야 한다

5번에 걸어둔 붕대를 다 쓰면 엔트리가 사라지는데 `HotbarRefs[0]`은 그걸 모른다.

**`RemoveEntry`가 청소한다.** 사용 시점 검사(`ContainsEntry`)만으로 두면 UI가 죽은 슬롯을 계속 그린다.

> **★ 초안 정정 — 필드도 청소도 Step 03에 넣지 않는다.** 초안은 *"제거 경로가 Step 03에서 이미 셋(버리기·사용·캐스케이드)이라 나중에 넣으면 셋을 찾아다닌다"* 고 적었다. **사실이 아니다.** 03-2 설계상 셋이 한 지점으로 모인다:
>
> ```
> Server_DropItem   ─┐
> SetEntryCharges 0 ─┼→ RemoveEntry → RemoveEntryInternal → RemoveSelf   ← 여기 하나
> 캐스케이드         ─┘                    └→ RemoveChildrenRecursive ────┘
> ```
>
> `RemoveSelf`는 이미 *"배열에서 빼고 `MarkArrayDirty`"* 의 유일한 지점으로 선언돼 있다(`05_Loot_03_Inventory.md:285`). 청소를 붙일 곳은 **나중에도 정확히 한 줄**이라 CLAUDE.md §2의 *"나중에 넣기 비싼 것"* 이 아니다. 반대로 지금 넣으면 Step 03 내내 **길이 0인 배열을 도는 루프**가 남아, 8차가 지적한 *"Step 03 내내 항상 거짓인 분기"* 패턴을 하나 더 만든다.
>
> | | 지금 (Step 03) | 나중 |
> |---|---|---|
> | `ActiveHotbarIndex` 필드 | **선언한다** | — |
> | `HotbarRefs` 필드 | ✗ | **Step 04** (드래그 배정) |
> | `HotbarRefs` 청소 | ✗ | **Step 05.** `RemoveSelf` 한 줄 |
>
> **`ActiveHotbarIndex`만 지금 선언하는 이유:** 필드가 하나 느는 게 아니라 **`EquippedEntryId`의 이름과 의미가 바뀌는 것**이다. 문서가 이미 그 자리에 선언·`DOREPLIFETIME_CONDITION`·세이브 목록을 갖고 있다(`05_Loot_03_Inventory.md:278`·`:490`·`:319`).
>
> **잊지 않기 위한 장치:** `HotbarRefs` 청소를 `05_Loot_05_Equipment.md`의 **완료 조건에 이름을 붙여** 적는다. 8차가 `EquippedEntryId` 건에서 쓴 방법과 같다. 코드에 죽은 루프를 남기는 것보다 완료 조건 한 줄이 싸다.

> **★ `ActiveHotbarIndex`는 반대로 청소가 필요 없다 — 초안이 청구하지 않은 이득이다.**
>
> ```cpp
> int32 GetEquippedEntryId() const
> {
>     return (ActiveHotbarIndex >= 0 && ActiveHotbarIndex < 4)
>         ? GetEntryInSlot(INDEX_NONE, HotbarSlotName(ActiveHotbarIndex))   // 없으면 INDEX_NONE
>         : INDEX_NONE;
> }
> ```
>
> **인덱스가 가리키는 것이 엔트리가 아니라 슬롯**이기 때문이다. 1번에 든 총을 버려도 `ActiveHotbarIndex`는 0으로 남지만 조회가 `INDEX_NONE`을 돌려준다. **죽은 번호가 생길 문법이 없다.** 이것이 함정 3h가 사라지는 것과 같은 이유이고, `HotbarRefs`가 `EntryId`를 **직접** 들어 이 보호를 못 받는 것과 대비된다.

> **`EntryId`를 재활용하지 않기로 한 것이 여기서 또 값을 한다.** 청소를 빠뜨려도 낡은 참조가 **엉뚱한 아이템을 가리킬 수 없다.** 재활용했다면 5번에 걸어둔 붕대 자리에 나중에 주운 수류탄이 들어앉는다.

### 참고 — 배정 출처 제한이 나중에 붙을 수 있다

지금은 어느 인벤토리 그룹에서든 5~0으로 끌 수 있다. 나중에 **상의·하의·외투에서만** 가능하게 바꿀 수 있다 (등가방은 안 됨 — 타르코프의 "가방에서 즉시 못 꺼냄"과 같은 긴장 장치).

**그래서 판정을 함수로 둔다.**

```cpp
bool CanAssignToHotbar(int32 EntryId) const;   // 지금은 ContainsEntry만 본다
```

호출부에 인라인으로 쓰면 나중에 세 곳을 찾아다닌다.

---

## 5. 결정 ③ — `SlotPriority`가 슬롯 배정을 데이터로 만든다

`EEPItemType`(`EPTypes.h:43-50`)에는 `Weapon` 하나뿐이라 주무기/보조/근접을 구분할 수 없다. 열거형을 늘리면 슬롯이 늘 때마다 코드가 따라 는다.

```cpp
// FEPItemData에 추가
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
TArray<FName> SlotPriority;
// 이 아이템이 들어갈 수 있는 장비/부착 슬롯. ★ 순서가 곧 자동 배정 우선순위다.
// 비어 있으면 어느 슬롯에도 들어가지 못한다 (일반 아이템)
```

| 아이템 | `SlotPriority` |
|---|---|
| AK-74 | `["Hotbar1", "Hotbar2"]` |
| 권총 | `["Hotbar3", "Hotbar1", "Hotbar2"]` |
| 도끼 | `["Hotbar4", "Hotbar1", "Hotbar2"]` |
| 배낭 | `["Back"]` |
| 스코프 | `["Optic"]` |
| 붕대 | `[]` |

**배열 순서가 곧 자동 배정 우선순위다.** 권총을 주우면 3번이 비었으면 3번, 아니면 1번, 그다음 2번. **전용 슬롯을 앞에 둔다는 규칙 하나로 분기가 사라진다.**

### 왜 `AllowedSlots`가 아니라 `SlotPriority`인가

**이름이 의미를 지고 가야 한다.** `AllowedSlots`는 **집합처럼 읽혀서** 순서를 바꿔도 되는 것처럼 보인다. DT 행을 채우는 사람이 순서의 의미를 모르면 **조용히 틀린다** — 권총 행을 `["Hotbar1","Hotbar2","Hotbar3"]`로 적으면 권총이 주무기 칸을 먹는다.

`PreferredSlot`을 **따로 두는 것은 거부한다.** 그 필드는 반드시 `SlotPriority`에 있어야 하고, 정합을 지키는 코드가 생기고, 빠뜨리는 경로가 생긴다. §3에서 이중 진실을 거부한 것과 같은 이유다. **한 단어짜리 개명이 실패 모드 하나를 없앤다.**

### `FGameplayTagContainer`가 아닌 이유

| 후보 | 판정 | 이유 |
|---|---|---|
| **`TArray<FName>`** | ✅ | `FEPInventoryEntry::SlotId`가 이미 `FName`이다. 다른 걸 쓰면 비교마다 변환이 생긴다 |
| `FGameplayTagContainer` | ✗ | **순서를 표현하지 못한다.** 내부는 배열이지만 의미가 집합이고(`GameplayTagContainer.h:625,629`의 `GameplayTags`＋`ParentTags`), 에디터 UI가 **체크박스 트리**라 DT를 채우는 사람에게 순서를 보여줄 방법이 없다 |
| `EEPEquipSlot` 비트마스크 | ✗ | 슬롯 추가 = 열거형 편집 = **코드 변경.** §5의 전제를 정면으로 깬다 |

> 엔진의 *"이름 붙은 자리"* 관용구도 `FName`이다 — `USceneComponent::AttachSocketName`(`SceneComponent.h:112-113`), `FSlotAnimationTrack::SlotName`(`AnimMontage.h:87-88`). 태그는 요구사항·분류에 쓰이지 자리 이름에 쓰이지 않는다. 프로젝트에 `EPNativeGameplayTags.h`가 있지만 그건 GAS 어빌리티·이펙트용이고 **슬롯 이름은 GAS 계층을 지나가지 않는다.**

### ★ 초안 정정 — 부착물은 이 필드만으로 안 된다

초안은 *"부착물 판정도 같은 필드가 처리한다 — §7-3이 따로 필드를 만들 필요가 없다"* 고 적었다. **아이템 쪽만 맞다.**

| 질문 | 누가 답하나 | 있나 |
|---|---|---|
| 이 조준경이 `Optic`에 들어갈 수 있나 | 아이템의 `SlotPriority` | ✅ 이 절이 만든다 |
| **이 무기에 `Optic` 슬롯이 있나** | **`UEPWeaponDefinition::AttachmentSlots`** | ✗ **§7-3 예정** |
| **`Optic`이 장비 슬롯인가 부착 슬롯인가** | **`UEPLootDeveloperSettings::BodySlots`** | ✗ **§6에서 지금 만든다** |

두 번째가 없으면 **단검에 조준경이 달린다.** 세 번째가 없으면 §6의 정합 불변식을 못 세운다.

```cpp
// UEPWeaponDefinition — §7-3에서 추가한다. 지금은 예고만
UPROPERTY(EditDefaultsOnly, Category = "Weapon")
TArray<FName> AttachmentSlots;      // 예: ["Optic", "Muzzle", "Grip", "Mag"]
```

### 지금 넣는다

Step 03에서 실제로 쓰이는 `SlotId`는 `"Back"` 하나뿐이다. **그래도 지금 넣는다** — 미루면 Step 05에서 필드를 추가하는 순간 **기존 DT 행을 전부 다시 채워야** 한다. DT 필드 추가는 코드가 아니라 **데이터 마이그레이션**이라 늦을수록 비싸다.

**그리고 읽는 코드가 0곳이 아니라 1곳이다.** 배낭 행의 `SlotPriority = ["Back"]`이 없으면 `MoveEntry(id, -1, "Back")`의 슬롯 검사가 통과할 근거가 없다(§6).

---

## 6. 결정 ④ — `MoveEntry()`가 이동의 유일한 연산이다

`SlotId`가 진실이 되면 장착·해제·드래그·부착이 **전부 같은 연산**이 된다. 바뀌는 것이 `ParentEntryId`와 `SlotId` 둘뿐이기 때문이다.

```cpp
bool MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId);
```

| 동작 | 호출 |
|---|---|
| 배낭 매기 | `MoveEntry(id, -1, "Back")` |
| 무기 장착 | `MoveEntry(id, -1, "Hotbar1")` |
| 해제 | `MoveEntry(id, 상의Id, None)` |
| 컨테이너 간 이동 | `MoveEntry(id, 배낭Id, None)` |
| 부착물 달기 | `MoveEntry(id, 총Id, "Optic")` |

`Server_EquipBackpack`은 **이것의 얇은 래퍼**로 남는다. → **★ 14차에 삭제됐다. §15 참조.**

### ★ 그러나 `Server_MoveEntry`는 지금 만들지 않는다 — 초안 정정

초안은 `MoveEntry`와 같이 `Server_MoveEntry(int32, int32, FName)`를 만든다고 적었다. **8차 규칙을 일관되게 적용하면 반대 결론이 나온다.** 8차의 문장은 *"서버가 이미 소유한 상태에 대한 변경 요청 → 컴포넌트의 서버 RPC"* 였다. 그건 **RPC를 열어도 된다**는 허가지 **얼마나 넓게 열지**의 답이 아니다.

| | `Server_EquipBackpack(int32)` | `Server_MoveEntry(int32, int32, FName)` |
|---|---|---|
| 클라가 정하는 값 | 엔트리 하나 | 엔트리 **＋ 목적지 부모 ＋ 슬롯 이름** |
| Step 03의 정당한 UI | 배낭 착용 | **없다** — 드래그는 Step 04 |
| 검증해야 할 것 | `ContainsEntry` + 배낭인가 | 위 전부 ＋ 정합 ＋ 사이클 ＋ 슬롯 |

**Step 03에는 `NewParent`와 `NewSlotId`를 정당하게 만들어낼 UI가 없다.** 그런데 RPC를 열면 조작된 클라이언트는 만들 수 있다. **소비자보다 검증 표면을 먼저 여는 것**이고, 이건 8차가 `Server_DropItem`을 고른 이유(*"서버가 이미 소유한 상태"*)와 무관한 순수한 공격 표면 확대다.

> **`MoveEntry`(내부 계약)는 지금. `Server_MoveEntry`(외부 표면)는 Step 04의 드래그와 함께.** ~~그리고 `Server_EquipBackpack` 래퍼가 남는 것은 **과도기가 아니라 최종형이다** — 좁은 RPC가 넓은 RPC보다 낫다.~~
>
> **★★ 뒷문장은 14차에 뒤집혔다 (§15).** 앞문장이 참이면 뒷문장이 거짓이다 — **04-B가 넓은 문을 열기 때문이다.**

### 왜 지금인가

**이건 편의 함수가 아니라 계약이다.** `MoveEntry` 없이 가면 Step 04의 드래그, Step 05의 장착, §7-3의 부착이 **각자 자기 함수를 만들고 `MarkItemDirty` 호출을 각자 빠뜨린다.** CLAUDE.md §2의 *"나중에 넣기 비싼 것은 지금 넣는다 — 계약(반환 규약·순서)"* 에 해당한다.

문서에 이름이 이미 있다 — ~~`Server_EquipBackpack`(03-6)~~(14차 삭제), 드래그 이동(Step 04), 부착물(§7-3). **`MoveEntry` 자체의 판정은 그대로다** — 없어진 것은 그 위의 RPC 래퍼뿐이고, 남은 소비자 셋(커맨드·드래그·부착)이 전부 `MoveEntry`를 부른다.

### 안에 들어가는 검사

| # | 검사 | 이유 |
|---|---|---|
| 1 | `ContainsEntry(EntryId)` | 조작된 요청 |
| 2 | `NewSlotId`가 그 아이템의 `SlotPriority`에 있는가 | §5. `None`이면 통과 |
| 3 | **`SlotId`↔`ParentEntryId` 정합** | ★ 아래. 없으면 *"가방 안에 든 상의를 입는다"* 가 표현된다 |
| 4 | `GetEntryInSlot(NewParent, NewSlotId)`가 비었는가 | 이미 차 있으면 교체가 아니라 실패 |
| 5 | `NewSlotId == None`이면 `CanFit(NewParent, ItemId)` | 옮겨갈 컨테이너에 자리가 있나 |
| 6 | `NewParent`가 자기 자손이 아닌가 | **사이클 금지.** 안 막으면 `RemoveEntry` 재귀가 무한이 된다 |

### ★ 검사 3 — `SlotId`와 `ParentEntryId`의 정합 불변식

**`SlotId`가 진실이 되면 표현할 수 있지만 무의미한 상태가 생긴다.** 초안에는 이걸 막는 것이 없었다.

```
상의   ParentEntryId = 배낭Id,  SlotId = "Torso"
       └─ "가방 안에 들어 있는 상의를 입고 있다"
```

이 상태는 `GetUsedSlots`에서 **칸을 안 먹고**(`SlotId != None`) `GetEntryInSlot`에서 **입은 것으로 잡힌다.** 배낭에 상의를 넣어두면 칸도 안 먹고 착용 효과도 받는다.

**규칙은 두 줄이다.**

```
장비 슬롯 (핫바 1~4 + 착용 8)      →  ParentEntryId == INDEX_NONE 이어야 한다
부착 슬롯 (Optic/Muzzle/Grip/Mag)  →  ParentEntryId == 그 무기 엔트리
```

그런데 **"이 슬롯 이름이 장비 슬롯인가 부착 슬롯인가"를 아는 코드가 없다.** `SlotPriority`도 답하지 못한다 — 아이템이 *들어갈 수 있는* 곳의 목록일 뿐이다. **그래서 목록을 하나 만든다.**

```cpp
// UEPLootDeveloperSettings (Step 00에서 생성됨) — 여기서 확장
UPROPERTY(config, EditAnywhere, Category = "Inventory")
TArray<FName> BodySlots;      // 핫바 1~4 + 착용 8 = 12개
```

```cpp
const bool bIsBodySlot = GetDefault<UEPLootDeveloperSettings>()->BodySlots.Contains(NewSlotId);

if (bIsBodySlot && NewParent != INDEX_NONE)
    return false;                                   // 가방 안에서 입을 수 없다

if (!bIsBodySlot && !NewSlotId.IsNone())
{
    // 부착이다 — 부모 무기가 그 슬롯을 갖고 있어야 한다 (§7-3에서 활성화)
    const UEPWeaponDefinition* W = GetWeaponDefOf(NewParent);
    if (!W || !W->AttachmentSlots.Contains(NewSlotId)) return false;
}
```

> **전역에서 읽어야 하는 이유:** 소비자가 둘이다 — `MoveEntry`의 검증과 **Step 04 UI의 슬롯 그리기**. UI에는 물어볼 인벤토리 인스턴스가 없을 수도 있다. ~~6차의 *"전역 데이터 참조는 `UDeveloperSettings`"* 확정과 같은 자리다.~~
>
> **★★ 뒷문장은 14차에 철회했다 (§15-5).** 근거가 *"전역에서 닿아야 한다"* 까지만 말하고 **선반을 특정하지 않는다.** `UEPLootDeveloperSettings`는 **임시 자리**이고 최종 자리는 `UEPPawnInventoryData`(DataAsset)다 — `05_Loot_DOCS.md` §8 미정 #10.
>
> **`BodySlots`는 지금, `AttachmentSlots`는 §7-3에.** `BodySlots`가 없으면 Step 04 드래그가 열리는 순간 위 상태가 실재하고, 그때 `MoveEntry`를 다시 열어야 한다. `AttachmentSlots`는 소비자가 `MoveEntry`의 부착 갈래 하나뿐이고 그 갈래가 Step 03·04에 도달 불가다.

### ★ 검사 6 — 사이클 검사는 "도달 불가 분기의 에러 처리"가 아니다

CLAUDE.md §2의 *"도달 불가한 분기의 에러 처리는 만들지 않는다"* 에 걸리는 것처럼 보이지만 **분류가 틀렸다.**

`RemoveEntry`의 재귀에는 **무한 재귀를 막는 장치가 없다.** 안전한 이유가 문서에 명시돼 있다:

```
// 05_Loot_03_Inventory.md:393
// ④ 자식은 그 다음. 부모가 이미 배열에서 빠져 사이클이 성립하지 않는다
```

**이 문장은 "입력이 트리다"라는 전제 위에 서 있다.** `MoveEntry`는 그 전제를 깰 수 있는 **유일한 함수**다 — `InsertEntry`는 새 노드만 만들고 `RemoveEntry`는 노드를 없앤다. 즉 사이클 검사는 *일어나지 않을 일에 대한 방어*가 아니라 **이미 존재하는 안전성 논증을 계속 참이게 하는 코드**다. 없으면 `:393`의 주석이 거짓말이 된다.

CLAUDE.md §2: *"나중에 넣기 비싼 것은 지금 넣는다 — 식별자 안정성, 복제 조건, **계약(반환 규약·순서)**"*. **재귀 종료 조건은 계약이다.**

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

### 교체(swap)는 Step 04로 미룬다

검사 4는 *"차 있으면 실패"* 다. **Lyra도 같다** — `AddItemToSlot`이 `Slots[i] == nullptr`일 때만 대입하고 조용히 실패한다(`LyraQuickBarComponent.cpp:169-179`). 우리는 `Client_OnInventoryActionFailed`가 있으니 조용하지 않게 할 수 있다.

*"해제→장착 두 번 부르는 중간 상태에서 첫 아이템이 갈 곳이 없다"* 는 걱정은 실재하지만 **원자적 교체의 근거가 되지 않는다.** 가방이 꽉 찼으면 해제가 실패하고, 실패하면 **교체 전체가 아무 일도 안 한 상태로 끝난다.** 이게 올바른 결과다. 원자적 교체를 만들면 오히려 *"밀려난 아이템은 어디로 가는가"* 를 정의해야 하고, 그 답("가방" 또는 "바닥")은 **드래그의 출발지가 명시되는 Step 04에서만 자명하다.**

### 반환 규약

`bool`. 실패 사유는 필요해지면 `FText` 아웃 파라미터로 늘린다 — `Client_OnInventoryActionFailed`가 이미 있다(03-5).

---

## 7. 지금 만드는 확장점

CLAUDE.md §2 기준 — 전부 **문서에 이름이 있고**, 나중에 넣으면 호출부를 찾아다녀야 한다.

### 7-1. `GetInsertionOrder()` — 획득 시 어느 컨테이너부터 보는가

현행은 이름이 박혀 있다:

```cpp
// EPPickup.cpp — 03-4 현행
int32 NewId = Inv->AddSubtree(INDEX_NONE, Payload);                       // 본체
if (NewId == INDEX_NONE && Inv->GetEquippedBackpack() != INDEX_NONE)
    NewId = Inv->AddSubtree(Inv->GetEquippedBackpack(), Payload);         // 배낭
```

상의·하의·외투가 들어오면 `if`가 다섯 개가 되고, **본체 10칸이 사라지면 첫 줄이 죽은 코드가 된다.**

```cpp
TArray<int32> GetInsertionOrder() const;   // 지금은 [INDEX_NONE, 배낭Id]
```

**어느 순서인가는 데이터로 뺀다.**

```cpp
// UEPLootDeveloperSettings — BodySlots 옆
UPROPERTY(config, EditAnywhere, Category = "Inventory")
TArray<FName> ContainerOrder;   // ["Coat", "Torso", "Legs", "Back", "Wrist"]
```

**Step 04 UI가 같은 배열을 본다** — 구획을 그리는 순서다(외투가 상의 **위에** 끼어든다). 소비자가 둘이므로 전역에 둔다. *"주울 때 팔목은 마지막에 채운다"* 처럼 표시 순서와 삽입 순서가 갈리면 목록이 둘이 되는데, **`GetInsertionOrder()`가 이미 함수라 쪼개는 비용이 0이다.**

> **★ 공유되는 것은 *순서*이지 *목록*이 아니다 (10차).** `GetInsertionOrder()`는 `ContainerOrder`를 **그대로 반환하지 않는다** — 반환형이 `TArray<int32>`(컨테이너 EntryId)이고 `ContainerOrder`는 `TArray<FName>`(슬롯 이름)이며, 무엇보다 **`ContainerOrder`에는 본체가 없다.** 본체는 슬롯이 아니기 때문이다.
>
> ```cpp
> TArray<int32> Out;  Out.Add(INDEX_NONE);          // ★ 본체를 직접 붙인다
> for (const FName& S : GetDefault<UEPLootDeveloperSettings>()->ContainerOrder)
>     if (const int32 Id = GetEntryInSlot(INDEX_NONE, S); Id != INDEX_NONE) Out.Add(Id);
> ```
>
> **빠뜨리면 테스트 중(`MaxSlots=10`) 아무것도 안 들어간다.** ★ 13차에 **본체는 0칸으로 확정**됐다 — Step 03 완료 조건 2~6이 전부 본체 위에 서 있어 03-C에서 즉시 걸린다. 9차가 `[INDEX_NONE, Back]`으로 적었던 것이 10차에 `ContainerOrder`(슬롯 이름 5개)로 옮겨가면서 **본체가 조용히 빠질 뻔한 자리다.**

```cpp
// AEPPickup::OnInteract — 03-4. ★ 2단계다. 섞지 않는다
// ① 장비 슬롯 시도 (SlotPriority 순서대로 빈 곳)
int32 NewId = Inv->TryAutoEquip(Payload);            // 실패하면 INDEX_NONE

// ② 실패했으면 컨테이너 (GetInsertionOrder 순서대로)
if (NewId == INDEX_NONE)
    for (int32 C : Inv->GetInsertionOrder())
        if ((NewId = Inv->AddSubtree(C, Payload)) != INDEX_NONE) break;
```

**본체 10칸 제거가 데이터 변경으로 끝나려면 이 함수가 있어야 한다.**

### 7-1-1. 자동 장착과 삽입 순서는 만나지 않는다 — 순차다

기획이 *"총을 주우면 자동으로 핫바에 들어간다"* 이므로 **장비가 먼저다.** 그런데 ①과 ②는 같은 경로에서 경쟁하는 게 아니라 **판정 기준이 다르다** — ①은 *슬롯이 비었는가*, ②는 *칸이 남았는가*. 하나로 합치려 하면 `GetInsertionOrder()`가 "컨테이너 또는 슬롯"을 섞어 반환해야 하고 소비자(`AddSubtree`)가 둘을 구분해야 한다. **섞지 않는다.**

> **`TryAutoEquipBackpack` → `TryAutoEquip`으로 일반화한다.** 배낭 전용 함수를 만들면 무기·상의·헬멧이 들어올 때 같은 함수가 넷이 된다. 일반형은 *"`SlotPriority`를 순회하며 `GetEntryInSlot(INDEX_NONE, S) == INDEX_NONE`인 첫 슬롯에 `MoveEntry`"* 한 줄짜리 루프이고, **Step 03에서는 배낭 행의 `["Back"]` 하나만 돌아 지금 동작과 정확히 같다.** 배낭 자동 착용을 Step 03에 두는 결정(3차 확정)은 그대로다.

> **서브트리를 통째로 장비 슬롯에 넣는 경우가 있다** — 조준경이 달린 총을 주울 때다. 그래서 ①도 `AddSubtree`를 거치고 **루트에만 `SlotId`를 세팅한다.** `AddSubtree`의 *"칸 검사는 루트만"* 계약(8차 확정)이 여기서도 맞아떨어진다 — 루트가 슬롯에 들어가면 칸 검사를 건너뛴다.

### 7-2. `CanAssignToHotbar(EntryId)` — §4 참조

### 7-3. `GetEntryInSlot(int32 Parent, FName SlotId)` — §3 참조

슬롯이 12개이므로 슬롯마다 게터를 만들지 않는다. `GetEquippedBackpack()`은 이것의 얇은 래퍼로만 남긴다. **부모 인자는 선택이 아니다** — §3의 부착 슬롯 절.

### 7-4. `UEPLootDeveloperSettings::BodySlots` — §6 참조

몸에 붙는 슬롯인지 무기에 붙는 슬롯인지를 아는 유일한 곳. 소비자가 둘이다 — `MoveEntry` 검증, Step 04 UI 슬롯 그리기.

---

## 7-A. 만들지 않는 것

CLAUDE.md §2 *"상상한 확장점"* 에 걸린다. 9차 검수에서 명시적으로 제외됐다.

| 안 만드는 것 | 언제 | 이유 |
|---|---|---|
| `Server_MoveEntry` | Step 04 | 소비자보다 검증 표면을 먼저 열지 않는다 (§6) |
| `HotbarRefs` 필드·청소 | Step 04 / 05 | 제거 경로가 `RemoveSelf` 하나로 모인다 (§4) |
| `UEPWeaponDefinition::AttachmentSlots` | §7-3 | 소비자가 도달 불가한 갈래 하나뿐 (§5) |
| `GetEntryInSlot` 결과 캐시 / `PostReplicatedReceive`의 `TMap` | 안 만든다 | 컴포넌트에 두면 두 번째 진실이 된다. 필요하면 **읽는 쪽(위젯)** 이 알림 1회당 1회 만든다 |
| 원자적 슬롯 교체 | Step 04 | 밀려난 아이템의 행선지를 드래그 UI가 정한다 (§6) |
| 별도 장비 슬롯 배열 | 안 만든다 | §3 |
| `PreferredSlot` 별도 필드 | 안 만든다 | §5 |

> **O(N) 선형 탐색에 대비하지 않는 근거가 하나 더 있다.** GAS의 활성 이펙트 컨테이너가 우리와 **정확히 같은 자료구조**(`FActiveGameplayEffect : FFastArraySerializerItem` / `FActiveGameplayEffectsContainer : FFastArraySerializer`)인데, 핸들 조회가 **맨몸 선형 탐색**이다(`GameplayEffect.cpp:3323-3333`). 호출부가 `GameplayEffect.cpp` 13곳 + `AbilitySystemComponent.cpp` 14곳 — 쿨다운·스택·주기 실행 전부 핫 패스다. **에픽이 이 자료구조에서 O(N)을 문제로 보지 않는다.** 우리 N도 GAS의 N도 수십이다.

---

## 8. 본체 10칸 — 유지하되 언제든 없앤다

**시간 제약으로 이번에는 `MaxSlots = 10`을 유지한다.** 언턴드 기준으로는 아무것도 안 입으면 인벤토리가 0칸이어야 하지만, Step 03의 완료 조건 2~6이 전부 본체 10칸 위에 서 있다.

**없애는 비용은 §7-1이 있으면 0에 가깝다.**

| | 작업 |
|---|---|
| 코드 | 없음 |
| 데이터 | `MaxSlots = 0`, 상의·하의 DT 행 추가 (`ContainerCapacity` 지정) |
| 문서 | `GAME.md:158` 수정 |

착용 아이템이 그냥 컨테이너 엔트리이므로 **본체는 "용량 0인 특수 컨테이너"로 자연스럽게 퇴화한다.**

> **★ 단 조건이 하나 있다.** `GetCapacity(INDEX_NONE)`이 `MaxSlots`를 돌려주는 **특수 분기**가 남아 있는 한(03-2), 본체는 저절로 퇴화하지 못한다. 위 표의 *"코드 없음"* 은 정확히는 *"이 분기 하나만"* 이다. 그 분기를 데이터로 옮기는 것은 이 시점에 해도 된다 — `MoveEntry`나 `SlotId` 설계와 무관하다.

> 반대로 §7-1 없이 진행하면 획득 경로를 다시 짜야 하고, 그때는 `AddSubtree` 호출부가 픽업·디버그 커맨드·컨테이너 UI(§7-1 타르코프식 컨테이너) 셋으로 늘어나 있다.

---

## 9. 기존 문서와 충돌하는 것 — 수정 대상

> **초안의 목록은 불완전했다.** 9차 검수가 `EquippedEntryId` / `EquippedBackpackEntryId` / `GetEquippedBackpack` / `SetEquippedEntryId`를 전수 조사해 **7건 누락**을 찾았다. 아래는 전수 조사 결과를 반영한 완전 목록이다(`DOCS/Notes/05/Review/`는 **역사 기록이므로 고치지 않는다**).

### 9-1. Step 03 착수 전에 반드시

| 문서 | 위치 | 현행 | 조치 |
|---|---|---|---|
| `05_Loot_03_Inventory.md` | 03-1 | `SlotId` "Step 03에선 항상 None" | 값 목록 확정 (§1) |
| `05_Loot_03_Inventory.md` | 03-2 `:245-255` | 게터가 필드를 읽음 | `GetEntryInSlot(Parent, SlotId)` 파생 |
| `05_Loot_03_Inventory.md` | 03-2 `:278-279`·`:490-491` | 장착 필드 2개 + `DOREPLIFETIME` | **`ActiveHotbarIndex` 하나만** |
| `05_Loot_03_Inventory.md` | 03-2 `:369-376` | `RemoveEntry` 번호 비우기 | **삭제.** `HotbarRefs` 청소는 넣지 않는다 (§4) |
| `05_Loot_03_Inventory.md` | 03-2 `:422-430` | *"★★ `EquippedEntryId`도 `RemoveEntry`가 비운다"* 절 | **절 통째로 삭제** → write-back 순서 절로 대체 (§3) |
| `05_Loot_03_Inventory.md` | 03-2 | — | **`MoveEntry` 절 신설** (§6) |
| `05_Loot_03_Inventory.md` | 03-2 `:460` | 캐스케이드 근거가 *"무기가 배낭에 들어가는 일이 흔하다"* | 근거 교체 — 장착 무기는 배낭에 못 들어간다 (§3) |
| `05_Loot_03_Inventory.md` | 03-4 `:723-724` | 본체→배낭 하드코딩 | `TryAutoEquip` → `GetInsertionOrder()` 2단계 (§7-1-1) |
| `05_Loot_03_Inventory.md` | 03-6 `:1096-1106` | `EquippedBackpackEntryId` 검사 | `GetEntryInSlot(INDEX_NONE, "Back")` |
| `05_Loot_03_Inventory.md` | 03-7 `:1151` | *"`SetEquippedEntryId`도 알림을 쏜다"* | 함수가 사라진다 → `MoveEntry`가 쏜다 |
| `05_Loot_03_Inventory.md` | 함정표 `:1249`·`:1255`·`:1264` | 3b / 3h / 5b | **3h·5b 삭제**, 3b는 미정 #7로 이동 |
| `05_Loot_03_Inventory.md` | `:319` 세이브 | 장착 필드 2개 | `ActiveHotbarIndex` |
| `05_Loot_03_Inventory.md` | `:642` | *"`GetEquippedBackpack()`이 아직 `INDEX_NONE`"* | 파생 게터로 문장만 조정 |
| `05_Loot_03_Inventory.md` | `:1288-1289` | 범위 밖 목록 + *"항상 거짓인 분기"* 경고 | 분기가 사라지므로 문장 교체 |
| `LOOT_STATUS.md` | `:54` 배낭 행 | *"`EquippedBackpackEntryId` 별도 필드"* | `SlotId = "Back"` |
| `LOOT_STATUS.md` | `:63` | *"`Build.cs` 수정 불필요"* | **8차 정정 미반영분.** `NetCore` 추가 필요 |
| `LOOT_STATUS.md` | `:76` 장비 슬롯 행 | *"필드 둘 … 셋이 되면 `TMap`"* | `SlotId`가 진실 |
| `EPItemData.h` | `:39-56` | — | **`SlotPriority` 추가** (코드) |
| `EPLootDeveloperSettings.h` | — | — | **`BodySlots` 추가** (코드) |

### 9-2. Step 03 작성과 함께 (문서 정합)

| 문서 | 위치 | 조치 |
|---|---|---|
| **`DOCS/BACKLOG.md`** | **`:124`·`:129-133`·`:240`** | **★ B-5 항목 전체.** *"진실은 `EquippedEntryId`(int32)"* 가 B-5의 문장이다. 진실이 `ActiveHotbarIndex` → `GetEquippedEntryId()`로 바뀌면 서술이 통째로 낡는다. `LOOT_STATUS.md:27`이 B-5를 *"안 지키면 나중이 비싸진다"* 로 지목한다 |
| **`DOCS/Mine/StudyPath.md`** | **`:926-935`** | **★ 세션 5의 "정답"** 이 *"맞다. `EquippedEntryId`(int32)다"* 이고 다이어그램(`:932`)까지 있다. **사용자가 읽고 외우는 문서**라 틀린 채 두면 가장 비싸다 |
| `05_Loot_DOCS.md` | `:169`·`:521`·`:590`·`:802`·`:823` | 색인 / §4-8 본문 / 단계표 / §8 확정표 / 영속화 목록 |
| `05_Loot_04_InventoryUI.md` | `:38`·`:113`·`:331` | `:113`이 `GetEquippedBackpack()`을 **호출**한다. `:38`·`:331`은 *"장비 슬롯 UI = `EquippedEntryId` 강조"* → **12칸 슬롯 UI**로 범위가 는다 |
| `05_Loot_05_Equipment.md` | `:19-20`·`:91`·`:119`·`:124`·`:134`·`:141`·`:173`·`:181`·`:262` | 완료 조건 / 흐름도 / 코드 / 순서 규칙 / 필드 선언 참조 / 함정 8. **＋ `HotbarRefs` 청소를 완료 조건에 신규 추가** (§4) |
| `05_Loot_01_Spawner.md` | `:770` | *"`EquippedEntryId`가 셋이 되면 `TMap`"* 을 비유로 씀 → 비유가 무효 |

### 9-3. 나중에

| 문서 | 위치 | 조치 |
|---|---|---|
| `GAME.md` | `:158` | 본체 10칸이 과도기임을 명시 (§8) |
| `GAME.md` | `:178-180` | 무기 슬롯 2 + 배낭 1 → **핫바 10 + 착용 8** |
| `05_Loot_DOCS.md` | §8 미정 #5 | 무기 2정 — **확정으로 이동** |

### 9-4. 03-A/B/C 분할 재조정

**8차의 03-7 사고와 같은 패턴이다** — *"03-B가 쓰는 것이 03-A에 없으면 컴파일이 안 된다."*

| 구간 | 들어가는 것 | 완료 조건 |
|---|---|---|
| **03-A 코어** | 03-1 · 03-2 · 03-3 · 03-7 · 03-9<br>**＋ `GetEntryInSlot(Parent, SlotId)`**<br>**＋ `MoveEntry` (정합·사이클 검사 포함)**<br>**＋ `ActiveHotbarIndex` 필드**<br>**＋ `FEPItemData::SlotPriority` (DT 필드 + 배낭 행)**<br>**＋ `UEPLootDeveloperSettings::BodySlots`** | 2~6 |
| ~~**03-B 배낭**~~ (13차에 **구간째 삭제** — 남은 새 코드가 래퍼 하나였고 호출자가 0개였다) | 03-6 + `GetCapacity(컨테이너)`<br>~~**＋ `Server_EquipBackpack` → `MoveEntry` 래퍼**~~ (14차 삭제)<br>**＋ `TryAutoEquip` (일반형)** → 13차에 아래 구간으로 | 7 |
| **03-C 줍기·버리기** (13차에 **03-B**로 이름이 바뀌고 `AddSubtree`·`TryAutoEquip`·`StartingEquipment`가 합류했다) | 03-4 · 03-5<br>**＋ `GetInsertionOrder()`** | 1, 8~13 + 이월 2건 |

- **`MoveEntry`가 03-A인 이유는 03-7과 같다.** ~~03-B의 `Server_EquipBackpack`이 이것의 래퍼이므로, 정의가 03-B에 있으면 래퍼와 본체가 같은 구간에 갇혀 "얇은 래퍼"라는 설계가 검증되지 않는다.~~ `MoveEntry`는 `RemoveEntry`·`AddSubtree`에 의존하지 않으므로 **03-A 단독 컴파일 조건을 깨지 않는다.**
  > **★ 근거가 13·14차에 바뀌었지만 결론은 같다 (§15).** 래퍼가 없어졌으므로 이유는 이제 **`EP.Inv.Move`(03-A)가 `MoveEntry`를 직접 부른다**는 것이다.
- **`GetEntryInSlot`이 03-A인 이유:** 03-B의 `GetCapacity`가 배낭을 찾는 데 쓰고, 03-A의 `GetUsedSlots`도 슬롯 판정을 한다.
- **`SlotPriority`가 03-A인 이유:** DT 필드 추가는 코드가 아니라 **데이터 마이그레이션**이라 늦을수록 비싸다. 03-B의 `TryAutoEquip`이 첫 소비자다.
- **`BodySlots`가 03-A인 이유:** `MoveEntry`의 정합 검사가 읽는다.
- **`GetInsertionOrder`가 03-C인 이유:** 소비자가 `AEPPickup::OnInteract`(03-4) 하나뿐이고 `AddSubtree`에 의존한다.

> **03-A의 검증 문장은 그대로 유지된다** — *"`RemoveEntry`/`AddSubtree` 없이 컴파일·실행된다."* 위 추가분 어느 것도 그 둘을 부르지 않는다.

---

## 10. 미정

| # | 항목 | 비고 |
|---|---|---|
| 1 | **핫바 5~0의 아이템을 "손에 든다"는 것이 무엇인가** | 무기는 `AEPWeapon` 액터를 스폰한다(05-3). 붕대에는 대응물이 없다. 소모품 사용을 어빌리티로 갈지 별도 경로로 갈지 |
| 2 | 착용 아이템에 방어력·이동속도 등 스탯이 붙는가 | 붙으면 GAS 어트리뷰트 연동 지점이 생긴다 |
| 3 | 팔목("클라우드")이 다른 착용과 다른 점이 있는가 | 현재 설계에서는 `ContainerCapacity`를 가진 착용 아이템일 뿐이라 **구분이 없다.** UI 표기만 다르다면 코드 추가가 없다 |
| 4 | 슬롯이 비면 자동으로 다음 무기를 손에 드는가 | `ActiveHotbarIndex`가 가리키던 무기를 버렸을 때의 동작 |
| 5 | 착용 아이템을 벗을 때 안의 내용물 | 배낭과 같이 **통째로 나간다**가 자연스럽다 (`RemoveEntry` 캐스케이드가 이미 그렇게 한다) |
| 6 | ~~장비 슬롯 교체를 한 번에 할 것인가~~ | **✅ 해소 (2026-08-23).** `SwapEntries(A, B)` — Step 04. `MoveEntry` 두 번으로는 **성립하는 교환이 거절된다**(중간 상태에 상대가 안 빠져 있다). 설계: `05_Loot_04_InventoryUI.md` 04-7 |
| 7 | **핫바 5~0으로 컨테이너 안 아이템을 "손에 들" 수 있는가** | ★ 함정 3b가 되살아나는 유일한 경로다. 허용하면 *"배낭 속 무기를 든 채 배낭을 버린다"* 가 다시 표현 가능해지고, 그때 `RemoveEntryInternal` ①의 write-back 검사가 **`GetEquippedEntryId()` 하나로는 부족해진다.** Step 04 드래그 설계와 함께 결정 (§3) |
| 8 | `GetCapacity(INDEX_NONE)`의 `MaxSlots` 특수 분기를 언제 데이터로 옮기나 | §8. 본체 10칸 제거의 유일한 코드 잔여물 |

---

## 11. 9차 검수 반영 이력

**요청서:** `DOCS/Notes/05/Review/05_Loot_REVIEW9_Request.md` / **답변:** `05_Loot_REVIEW9_Answer.md` (2026-08-22)

### 11-1. 답변대로 반영한 것

| # | 초안 | 반영 | 근거 |
|---|---|---|---|
| 1 | `SlotId`가 진실 (A안) | **유지 — A로 확정** | `USceneComponent`가 정확히 같은 형태다. `AttachParent` + `AttachSocketName`을 **자식**에 두고, 자식 목록(`AttachChildren`)은 **`Transient` 파생 색인**이다(`SceneComponent.h:108-119`). **진실은 자식에, 색인은 파생으로.** Lyra의 `Slots`는 반례가 아니다 — 이름 없는 **균질 위치 인덱스 3칸**이고 `ULyraEquipmentDefinition`에는 슬롯 필드가 아예 없다(`LyraEquipmentDefinition.h:36-56`, 헤더 전체) |
| 2 | `GetEntryInSlot(FName)` | **`(int32 Parent, FName)`** | §3. 8차 `FindFungibleEntryId`와 같은 결함 |
| 3 | 정합 불변식 없음 | **`BodySlots` + 검사 3 추가** | §6 |
| 4 | `Server_MoveEntry` 지금 | **Step 04로** | §6 |
| 5 | `AllowedSlots` | **`SlotPriority`로 개명** | §5 |
| 6 | *"부착물은 따로 필드 불필요"* | **틀렸다 — `AttachmentSlots` 예고** | §5 |
| 7 | `HotbarRefs` 지금 (*"제거 경로 셋"*) | **Step 04/05로.** 근거가 사실이 아니었다 — 셋 다 `RemoveSelf` 하나로 모인다 | §4 |
| 8 | 파급 목록 | **7건 추가** — `BACKLOG.md` B-5 / `StudyPath.md` / `LOOT_STATUS.md` 3행 / 04 문서 3곳 / 05 문서 9곳 / `05_Loot_DOCS.md` 5곳 / 03 문서 3곳 | §9 |
| 9 | 03-A/B/C | **재조정** | §9-4 |

### 11-2. 답변과 다르게 반영한 것

**① 함정 3b — 답변은 *"남는다"* 로 판정했으나 표현 불가능해진다.**

답변 §9-1은 *"함정 3b 남는다 ✅ 맞다. write-back 소실은 값의 문제다"* 로 적었다. **장착의 정의가 바뀐 것을 반영하지 않은 판정이다.** 새 설계에서 장착은 `SlotId == "HotbarN"` **＋** `ParentEntryId == INDEX_NONE`이므로 *"배낭 속 무기를 장착한 채"* 라는 전제가 성립하지 않는다. **문장은 미정 #7로 옮겼다** — 핫바 5~0이 컨테이너 안 아이템을 들 수 있게 되면 되살아난다.

**② 답변이 짚지 않은 것 — write-back 순서 계약의 성격이 바뀐다.**

답변 §9-1은 *"순서 계약 자체는 유지"* 로만 적었다. 실제로는 **위반했을 때의 증상이 바뀐다.** `EquippedEntryId`가 저장된 값일 때는 `RemoveSelf` 뒤에 읽어도 살아 있어 *"`INDEX_NONE`을 향한 write-back"* 이었지만, 파생 게터가 되면 **write-back이 아예 안 불린다.** `05_Loot_05_Equipment.md:141`의 근거 문장을 바꿔야 한다 — §3.

**③ 답변의 줄 번호 일부가 어긋난다.** `LOOT_STATUS.md`의 `Build.cs` 서술은 `:70`이 아니라 **`:63`** 이다. 실제 위치로 §9에 반영했다.

### 11-3. 확인만 하고 안 바꾼 것

- **O(N) 선형 탐색에 대비하지 않는다** — GAS가 같은 자료구조를 맨몸 선형 탐색으로 조회하고 호출부가 27곳이다(`GameplayEffect.cpp:3323-3333`). §7-A
- **`FName` 복제 비용에 대비하지 않는다** — 커스텀 `FName`은 문자열로 나가지만(`CoreNet.cpp:344-360`) **내부 struct 델타가 기본 켜져 있어**(`FastArraySerializer.cpp:35`, `.h:218-221`) 안 바뀌면 안 나간다. 장착/해제는 초당 1회 미만이고 그때 ~16바이트다. **단 조건이 있다** — 살아 있는 원소에 통째 대입하면 `ReplicationID`가 리셋되어 전 프로퍼티가 다시 나간다. `MoveEntry`는 반드시 **필드 둘만 고치고 `MarkItemDirty(Item)`** 을 부른다
- **용량 있는 UE 인벤토리 선례는 없다** — `UE_5.7/Engine/Plugins` 전체에 인벤토리·장비 플러그인이 없고 Lyra는 `CanAddItemDefinition`이 `//@TODO`와 함께 무조건 `true`다(`LyraInventoryManagerComponent.cpp:159-163`). **그래서 판단 근거는 Lyra가 아니라 `USceneComponent`다**

---

## 12. 10차 검수 반영 이력 (2026-08-23)

**주제는 Step 04의 격자 UI였고, 이 문서와 겹치는 것은 셋이다.**

### 12-1. 답변대로 반영한 것

| # | 무엇 | 어디에 |
|---|---|---|
| 1 | **`GetInsertionOrder()`는 `ContainerOrder`를 그대로 반환하지 않는다** — 반환형이 `TArray<int32>`이고 **본체(`INDEX_NONE`)를 맨 앞에 직접 붙인다.** `ContainerOrder`에는 본체가 없다(슬롯이 아니다). 빠뜨리면 **본체 10칸에 아무것도 안 들어간다** | §7-1 |
| 2 | **함정 4k를 코드 주석 지시로 승격** — 순서를 바꿔도 컴파일되고, 증상이 *"0으로 덮인다"* 가 아니라 *"안 불린다"* 라 더 조용하다 | `05_Loot_03_Inventory.md` 03-2 |
| 3 | **함정 3b 미정 이동이 옳았다는 확인** — 10차가 9차 판정을 정정했다. §11-2 ①이 그대로 유효하다 | §10 미정 #7 |

### 12-2. 답변과 다르게 반영한 것

**① `SwapEntries` 용량식 — 고칠 식은 답변과 같지만 증상이 반대다.**

답변은 *"본체 10/10 + 핫바의 AK(4) ↔ 본체 구급상자(3) → **11/10을 통과시킨다**"* 로 판정했다. **대조해 보니 그 방향으로는 안 샌다.** 초안 식은 `UsedPA`와 `UsedPB` **두 수를 `||`로** 보는데, 어긋나는 경우마다 **둘 중 하나가 정확한 값**이고 그쪽이 걸러낸다 — 답변이 든 반례에서도 `10 - 3 + 4 = 11`이 나오는 쪽이 있어 **거절된다.** 답변은 두 수 중 통과하는 쪽만 보고 `||`를 세지 않았다.

**틀린 항이 붙는 쪽은 언제나 *실제 변화가 0인* 쪽**(슬롯에 든 쪽)이고, 거기서는 원래 값이 이미 용량 이내였다. **그래서 이 버그는 용량을 넘기지 못한다 — 대신 성립하는 교환을 거절한다.**

| 실제 반례 | 실제 | 초안 식 |
|---|---|---|
| 본체 10/10에서 **핫바1 AK(4) ↔ 핫바2 권총(2)** | 둘 다 슬롯이라 칸 변화 0 → 성립 | `10 - 2 + 4 = 12 > 10` → **거절** |
| §7-3 **조준경(1) ↔ 조준경(2)** (다른 무기끼리) | 변화 0 → 성립 | 무기의 `GetCapacity`는 **0**. `0 - 1 + 2 = 1 > 0` → **언제나 거절** |

**첫 줄이 아프다 — 핫바 무기 자리 바꾸기는 주요 조작인데 본체가 꽉 차면 안 된다.** 그리고 그건 `SwapEntries`를 별도 함수로 만든 이유(*"순차 적용은 성립하는 교환을 거절한다"*) 자체를 무너뜨린다. **식은 답변대로 `SlotId.IsNone()` 조건부 델타로 고쳤고, 심각도와 증상만 다시 적었다.**

**② 답변이 남겨둔 오류 하나 — `MoveTo`의 클램프.**

답변 §4-2의 수정안은 `MoveTo(..., Hit.DisplayIndex)`에 *"빈 영역이면 맨 뒤 (MoveTo가 클램프)"* 라고 주석을 달았다. **`FMath::Clamp(-1, 0, N)`은 0 — 맨 뒤가 아니라 맨 앞이다.** 답변 자신이 아래 각주에서 *"`INDEX_NONE`이면 맨 뒤로 계약을 명시하라"* 고 적었으면서 코드는 그대로 뒀다. `MoveTo` 본문을 고쳤다(함정 13d).

**③ `PA == PB` 합산이 오늘은 값을 하지 않는다.**

답변 §3-3은 *"델타 형태로 고치면 **반드시** 합산해야 한다"* 로 적었다. **오늘은 검사 5(*"같은 부모 ＋ 둘 다 `SlotId == None`"* 거절)가 두 델타가 동시에 0이 아닌 유일한 경우를 잘라내서** 합산과 개별 검사의 결과가 같다. **그래도 합산으로 적었다** — 검사 5를 완화하는 순간 개별 검사가 틀린 답을 내기 때문이다. *지금* 필요한 것이 아니라 *나중에 조용히 깨지지 않게* 하는 형태라고 이유를 바꿔 적었다.

**④ 답변의 줄 번호 두 곳.** `FindFungibleEntryId` 확정표 행은 `LOOT_STATUS.md:69`가 아니라 **`:60`**, `GAME.md`의 부착물 문장은 `:180`이 아니라 **`:182`** 다. `OnDragOver`의 버블 라우팅은 `SlateApplication.cpp:5799`가 아니라 **`:5827`·`:5834`** 이고 `:5799`는 `OnDragEnter`(`FNoReply` — 중간에 멈추지 않는다)다. 실제 위치로 적었다.

### 12-3. 이 문서 밖에서 스스로 추가한 것

**`MoveEntry`에 검사 0(제자리 거절)을 넣었다.** 답변은 같은 결함(`CanFit`이 자기 크기를 두 번 셈)을 **UI 쪽에서만** 막았다 — *"같은 컨테이너면 조건 없이 로컬"*. 그런데 **`Server_MoveEntry`는 Step 04에서 열리고 조작된 클라이언트는 같은 요청을 만들 수 있다.** 한 줄로 닫히고, *"목적지가 같으면 할 일이 없다"* 는 계약으로도 맞다 (`05_Loot_03_Inventory.md` 03-2 검사 0, 함정 4l).

---

## 13. 11차 — 아이템 순서를 서버로 (2026-08-23)

10차 반영 직후 *"가방을 벗었다 다시 입으면 순서가 초기화되나"* 라는 질문에서 시작해 **10차의 순서 설계를 뒤집었다.** 검수가 아니라 설계 결정이다.

### 13-1. 벗었다 입기는 원래 안전했다

배낭 벗기는 `MoveEntry(배낭Id, INDEX_NONE, NAME_None)` — `SlotId`만 바뀌고 **`EntryId`는 그대로**다. 내용물도 `ParentEntryId = 배낭Id`에 그대로 매달려 있다. 문제는 다른 데 있었다.

### 13-2. 클라 로컬 세이브는 **지속을 줄 수 없다**

```
NextEntryId는 컴포넌트 필드, 초기값 1  →  매치마다 1부터 재발급
ULocalPlayerSaveGame은 디스크에 남는다  →  매치를 넘어 산다

지난 매치의 Order[7] = [3, 12, 5]  가  이번 매치의 7번(다른 아이템)에 적용된다
```

대조 코드는 번호만 보므로 우연히 살아 있는 번호를 통과시킨다. **세션 도장을 찍어 무효화하면 지속이 한 번도 발휘되지 않아 인메모리와 같아진다.** 즉 10차가 `ULocalPlayerSaveGame`으로 바꾸며 청구한 이득(*"재접속·기기 변경에도 순서 유지"*)이 **로드맵상 성립하지 않았다.**

서버에 두면 로드맵 5단계가 엔트리 배열을 저장하므로 **저장 코드가 0줄**이다.

### 13-3. 결정

| | |
|---|---|
| 키 | **`FEPInventoryEntry::SortKey`(`int32`)**, 형제 스코프, `Step = 1<<16` 희소 |
| 쓰기 지점 | **`AssignSortKey` 하나** — `InsertEntry`/`SetEntryCharges`/`RemoveSelf`/`MoveEntry` 패턴의 5번째 |
| 자리 바꾸기 | **`Server_ReorderEntry(EntryId, PrevEntryId)`** — 인덱스가 아니라 **앞 이웃** |
| 정렬 | **`GetSortedContents`** — 클라·서버 공용 |
| 되줍기 | **`bIsRoot`가 `ParentEntryId`와 `SortKey`를 동시에 관장** — 루트는 버리고 자식은 보존 |

**§7-1의 2D 격자로 갈 때 `SortKey` → `FIntPoint Location`이고 호출 지점이 동일하다.** 필드 교체이지 구조 변경이 아니다. 클라 로컬로 뒀다면 같은 이행이 *"04-8을 버리고 03을 다시 연다"* 였다.

### 13-4. Claude의 권고가 뒤집힌 건이다

Claude의 첫 권고는 **"지금은 옮기지 마라"** 였다. 근거는 *"서버 세이브(로드맵 14번)가 마감 범위에 없으니 매치 안에서는 이득이 0"* — 로드맵 사실 자체는 맞다. 사용자 판단은 **설계를 세이브에 맞춰 두는 쪽**이었고, 실제로 옮기고 나니 `Resolve`/`MoveTo`/세이브 클래스 둘/`FEPContainerOrder`와 **함정 5건이 사라져 코드량이 오히려 줄었다.** 권고가 이행 비용을 과대평가했다.

### 13-5. 검증 중 정정한 것 둘

1. **키 타입.** 처음 `double`을 권했으나 5단계 2차의 **외부 DB(REST)** 왕복에서 부동소수가 지뢰이고 `Dump` 가독성이 나빠 **`int32` 희소**로 바꿨다. 이분 여유 52회 → 16회지만 둘 다 실사용에서 "거의 안 남"이라 남는 차이가 그쪽뿐이다.
2. **대역폭 근거.** `FastArraySerializer.h:1474-1485`(`NetSerializeStruct` = 구조체 전체)를 인용해 *"조밀 재번호는 항목 전체를 N번 보낸다"* 고 적었는데 **그건 폴백 경로**다. 기본은 `:1398-1401` → `:1645`이고 **바뀐 프로퍼티만** 나간다(`:218-219`). 스태시 280칸 기준 12.6KB가 아니라 **~3.4KB + 280개 changelist 비교** — 방향은 같고 크기가 4배 작다.

### 13-6. 11차 검수가 잡은 결함 둘 (2026-08-23)

**설계는 유지됐고 구현 힌트 두 곳이 틀렸다.**

**① 키 공간을 `GetSortedContents`로 구했다.** 그 함수는 `SlotId.IsNone()`으로 슬롯을 거르는데, `KeySpace_NextAtEnd`(당시 `NextKeyAtEndOf`)가 그걸 빌려 쓰면 **슬롯에 든 형제의 키가 안 보인다.**

```
① 무기를 핫바에 꽂는다  →  키를 그대로 들고 표시 목록에서만 빠진다
② 뭘 줍는다             →  안 보이는 그 키와 같은 값을 발급받는다   ← 동률
③ 무기를 뺀다           →  본체에 같은 키가 둘
```

**세 단계 전부 정상 플레이이고 가장 흔한 조작 순서다.** 03-1의 스펙(*"형제 스코프"*)은 원래 옳았고 구현 힌트가 그걸 어겼다. 고치니 **예외 셋이 사라지고**(`InsertEntry` 삼항 · `MoveEntry`의 `NewSlotId` 조건 · `RenormalizeSortKeys` 필터) **꽂았다 빼면 원래 자리로 돌아오는** 부수 효과까지 생겼다.

> **`GetSortedContents`는 "그릴 것을 고르는" 함수이지 "키 공간을 정의하는" 함수가 아니다.** 이름이 비슷해서 빌려 쓴 것이 원인이다.

**② 재정규화를 이분 고갈에만 걸었다.** 맨 앞(`-Step`)·맨 뒤(`+Step`)는 **무한 증감**이고, `int32` 오버플로가 나면 부호가 뒤집혀 **맨 앞으로 보낸 것이 맨 뒤에 나타난다.** 그 상태는 재정규화가 안 걸려 **영구적**이다. `KeySpace_NextAtEnd`는 **줍기마다** 최대 키를 올리므로 세션을 넘어 사는 스태시(5단계)에서 누적된다.

### 13-7. 답변과 다르게 판정한 것 셋

1. **`FFastArraySerializerItem` 파생 개수.** 답변은 *"엔진 전체 6개"* 라 했으나 Runtime(2)＋GameplayAbilities(4)만 센 것이고 **플러그인 전체로는 12개 이상**이다(UIFramework 2 · OnlineFramework 1 · InstancedActors 1 · MassGameplay 1 · 테스트 3+). **결론은 그대로 선다** — 직접 확인한 `FUIFrameworkSlotBase`·`FUIFrameworkWidgetTreeEntry`에도 순서 필드가 없다. **엔진의 복제되는 위젯 트리조차 형제 순서를 복제하지 않는다**는 것이 오히려 더 강한 근거다.
2. **`double` 기각 근거를 교체하지 않고 재배치했다.** 답변은 DB/REST·Dump를 *"부차적"* 이라며 고갈 판정으로 갈아끼우라 했는데 **셋 다 유효하다.** 고갈 판정을 앞세우되 나머지를 지우지 않는다 — 지우면 5단계 2차에서 다시 올라온다.
3. **일정 지적이 답변보다 크다.** 답변은 *"Step 03 전체를 1주로 잡았다면"* 이라 썼지만 **사용자 추정 1주는 Step 03·04·05 전부**다. 어긋난 폭이 답변이 말한 것보다 크다. 구간 단위로 바꾸라는 권고는 수용한다.

---

## 14. 12차 검수 (2026-08-23) — 슬롯 아이템은 자리를 지킨다

### 14-1. 확정

> **장착은 아이템을 가방에서 꺼내는 것이 아니다. 칸만 돌려주고 순서 자리는 남긴다.**
> 그래서 뺐다 꽂아도 원래 자리로 돌아오고, **그 사이에 넣은 아이템은 그 앞에 선다.**

**대안(슬롯에 들어갈 때 자리를 포기)은 기각됐다.** 키를 맨 뒤로 밀어도 **표시 목록 기준 맨 뒤 발급이 그 구간을 침범해 같은 동률이 재현되고**, 키 공간에서 아예 빼면 11차가 없앤 `NewSlotId` 예외가 모양만 바꿔 돌아온다. **§13-6의 교정이 이미 답이었다.**

> **이 판정은 핫바 1~4와 착용 8슬롯에만 걸린다.** 핫바 5~0은 `SlotId`를 안 쓰고 아이템이 컨테이너에 그대로 남으므로(§5) 애초에 해당이 없다.

### 14-2. 요청서가 든 반례가 사실이 아니었다

12차 요청서 §2-2는 *"같은 화면 자리에 놓았는데 결과가 다르다"* 를 근거로 A를 의심했다. **앞 이웃 API에서 표현할 수 없는 시나리오였다.**

```cpp
// 04-8 — Prev 는 항상 GetSortedContents 의 원소다
const int32 PrevEntryId = (Idx == 0) ? INDEX_NONE : Sorted[Idx - 1];
// 그리고 ReorderEntry 가 슬롯인 Prev 를 거부한다
```

**화면의 같은 자리 = 같은 `Idx` = 같은 `Prev` = 같은 키다.** *"붕대 뒤"* 와 *"구급상자 앞"* 은 사용자에게 같은 자리이고 **API에서도 같은 호출**이다. **11차가 앞 이웃을 고른 선택이 이 모호성까지 같이 없앴는데**, 요청서가 자기 설계의 이득을 과소평가했다.

**그리고 03-2 ★ 노트에 적힌 증상 자체가 틀렸다** — *"AK가 X보다 앞에 나타난다"* 고 적었으나 계산하면 **뒤에** 나타난다(`붕대 0 < X 32768 < AK 65536 < 구급상자 131072`). X는 놓은 자리를 지킨다.

### 14-3. ★ Claude가 만든 회귀 하나 — 무한 재귀

12차 요청서를 쓰며 `ReorderEntry`의 맨 앞 분기를 `NewKey = PrevKey`로 다시 짜면서, **그 분기에 걸려 있던 `PrevEntryId != INDEX_NONE` 가드를 지웠다.** 결과는 *"아이템 2개 이상인 컨테이너에서 하나를 맨 앞으로 끌면 무한 재귀 → 서버 프로세스 종료"* 다.

**답변은 이걸 11차 답변의 코드 스케치 탓으로 돌렸지만(*"내 잘못이다"*), 11차 반영 때는 Claude가 그 조건을 유지했다.** 라이브 버그는 12차 수정에서 들어갔다.

> **교훈은 "검수 답변의 스케치를 믿지 말라"가 아니라 — 분기 식을 다시 짜면 그 분기에 걸린 가드도 다시 검산한다** 이다.

### 14-4. ★ "UE에 선례가 없다"가 거짓이었다 — 두 번 틀렸다

11차 답변 §10-2가 *"엔진 전체 파생 6개, 순서 필드 0개"* 라 했고, **Claude도 `FFastArraySerializerItem` 직계 파생만 열어보고 *"엔진의 복제되는 위젯 트리조차 형제 순서를 복제하지 않는다"* 고 보고했다.** 둘 다 틀렸다.

```cpp
// UIFStackBox.h:45-47 — FUIFrameworkStackBoxSlot (SlotBase 를 상속한 쪽에 있다)
/** Index in the array the Slot is. The position in the array can change when replicated. */
UPROPERTY() int32 Index = INDEX_NONE;

// UIFPlayerComponent.h:56-57
UPROPERTY(BlueprintReadWrite, ...) int32 ZOrder = 0;
```

**주석이 우리 문제를 그대로 적고 있다.** 그리고 `UIFStackBox.cpp:53-60`의 재번호 루프가 **`RenormalizeSortKeys`와 같은 함수**라, *"조밀로 하면 재정규화 코드가 사라진다"* 가 거짓이라는 11차 판정을 **엔진 소스가 직접 증명한다.**

**바로잡으니 근거가 강해졌다** — *"선례가 없어 원칙으로 정한다"* 보다 *"엔진도 같은 이유로 같은 필드를 두는데, 조밀을 고른 조건이 우리와 다르다"* 가 훨씬 낫다.


---

## 15. 13·14차 — `Server_EquipBackpack`이 없어졌다 (2026-08-25)

**§6이 내린 결정 둘 중 하나가 뒤집혔다.**

| §6의 문장 | 지금 |
|---|---|
| *"`Server_MoveEntry`는 지금 만들지 않는다"* | ✅ **그대로** — 04-B가 첫 소비자 |
| *"`Server_EquipBackpack` 래퍼가 남는 것은 과도기가 아니라 **최종형**이다"* | ❌ **삭제** |

### 15-1. 근거가 앞문장에 잡아먹혔다

*"좁은 RPC가 넓은 RPC보다 낫다"* 는 `Server_MoveEntry`와의 **비교**다. 그런데 §6 자신이 *"`Server_MoveEntry`는 Step 04의 드래그와 함께"* 라고 적었고, **04-7이 실제로 그것을 연다.**

```
04-B :  Server_MoveEntry(id, INDEX_NONE, "Torso")     착용
        Server_MoveEntry(id, INDEX_NONE, "Back")      ← 래퍼가 하던 일 그대로
                                                        05_Loot_04_InventoryUI.md:610, 619
```

**넓은 문이 열린 뒤의 좁은 문은 공격 표면을 하나도 줄이지 않는다.** 조작된 클라는 넓은 쪽을 부른다. **좁은 래퍼는 그게 유일한 문일 때만 좁힌다** — §6은 그 조건이 영구히 참일 것처럼 읽었다.

### 15-2. 그리고 호출자가 처음부터 0개였다

| 시점 | 착용 경로 | 무엇을 부르나 |
|---|---|---|
| Step 03 자동 착용 | `OnInteract` → `TryAutoEquip` → `AddSubtree` | **서버 내부. RPC를 안 지난다** |
| Step 03 검증 | `EP.Inv.Move <id> -1 Back` (13차 신설) | **내부 `MoveEntry` 직접** |
| 04-A 검증 | `EP.Inv.Move <id> -1 Torso` | **내부 `MoveEntry` 직접** |
| 04-B 드래그 | `Server_MoveEntry` | 일반 RPC |
| 벗기 | `Server_DropItem` | 03-6 확정 (2026-08-24) |

**13차는 *"Step 03에 호출자 0개"* 까지 찾고 04-A로 옮기는 데서 멈췄다.** 근거가 *"`EP.Inv.Equip`이 첫 호출자"* 였는데 **`EP.Inv.Equip`은 콘솔 커맨드**이고, `05_Loot_03_Inventory.md`가 두 번 확립한 규칙(*"커맨드는 내부 함수를 직접 부른다. RPC 표면을 열지 않는다"*)이 그대로 적용된다. **옮긴 자리에도 0개였다.**

### 15-3. 배낭이 특별할 근거는 §7이 이미 없앴다

§7의 `TryAutoEquip`이 *"배낭 전용 함수를 만들지 않는다 — 무기·상의·헬멧이 들어올 때 같은 함수가 넷이 된다"* 로 일반형을 골랐다. **그 규칙을 자동 착용 경로에는 적용하고 RPC 이름에는 적용하지 않았다.**

슬롯 배정은 `SlotPriority`(아이템)와 `BodySlots`(설정)가 답한다 — **상의·하의·외투·배낭이 전부 같은 모양인데 `"Back"`만 함수 이름에 하드코딩돼 있었다.** 13차가 상의·하의를 컨테이너로 확정하면서 그 비대칭이 눈에 띄었다.

### 15-4. Claude의 판정이 두 번 연속 부족했던 건이다

9차 답변이 래퍼를 *"최종형"* 이라 못박았고, 13차 답변이 *"호출자 0개"* 를 찾고도 삭제 대신 이동을 골랐다. **둘 다 사용자 지적으로 뒤집혔다** — §13-4와 같은 종류의 기록이다.

> **남는 것:** 착용의 클라 표면은 **04-B의 `Server_MoveEntry` 하나**, 서버 내부 계약은 **`MoveEntry`**, 디버그 표면은 **`EP.Inv.Move`**. `EP.Inv.Equip`(04-A)도 같은 이유로 폐기했다 — `EP.Inv.Move`의 얇은 별칭이다.

### 15-5. `BodySlots`의 자리도 임시다 — 6차 인용을 철회한다

**사용자 지적:** *"장착 슬롯을 `LootDeveloperSettings`에 두는 게 이상하다."* 맞다.

**Lyra 직독.** `ULyraDeveloperSettings`(`LyraDeveloperSettings.h:39`) · `ULyraCosmeticDeveloperSettings`(`:25`) · `ULyraWeaponDebugSettings`(`:14`)가 **전부 `config=EditorPerProjectUserSettings`** 다 — 개발자 개인 설정이고 빌드에 안 실린다. 내용도 `ExperienceOverride` · 봇 수 오버라이드 · `bTestFullGameFlowInPIE` · 자주 쓰는 맵 목록이다.

**게임플레이 구성은 DataAsset이 든다.**

```cpp
// Character/LyraPawnData.h:24-53
class ULyraPawnData : public UPrimaryDataAsset
{ TSubclassOf<APawn> PawnClass; TArray<TObjectPtr<ULyraAbilitySet>> AbilitySets;
  TObjectPtr<ULyraInputConfig> InputConfig; TSubclassOf<ULyraCameraMode> DefaultCameraMode; };
```

**슬롯 개수조차 컴포넌트 필드다** — `ULyraQuickBarComponent::NumSlots = 3`(`LyraQuickBarComponent.h:63-64`). **Lyra는 "슬롯이 무엇이 있나"를 설정에 넣은 적이 없다.**

#### 이 프로젝트 자신의 파일이 이미 증거다

```cpp
// Public/Data/EPLootDeveloperSettings.h:14-34 — 다섯이 전부 "어디 있는 무엇을 쓰나" ＋ 디버그
TSoftObjectPtr<UDataTable> ItemDataTable;  bool bEnableLootDebugLog;  bool bEnableSpawnerDebugDraw;
TSoftObjectPtr<UStaticMesh> PlaceholderPickupMesh;  TSoftClassPtr<AEPPickup> PickupClass;
```

**6차의 문장은 *"전역 **데이터 참조**"* 이지 *"전역 데이터"* 가 아니다.** §6이 그 구분을 흘렸고, 13차가 `StartingEquipment`를 같은 근거로 붙이며 반복했다.

#### 진짜 문제는 선반이 아니라 스코프다

```
"Torso"라는 이름이 몸 슬롯인가        ← 전역. BodySlots가 답한다
이 소유자에게 Torso가 있는가          ← 소유자별. 전역 싱글턴은 못 답한다
```

**§6의 검사 3은 뒤를 물어야 하는데 앞을 묻는다.** 어긋남이 §7-1에서 드러난다 — 나무 상자의 인벤토리에 `MoveEntry(상의, INDEX_NONE, "Torso")`를 보내면 **검사 3도 검사 5도 통과해 상자가 상의를 입는다.** 지금은 상자의 `MoveEntry`를 부르는 표면이 없어 **우연히** 도달 불가다.

#### 그래도 지금 옮기지 않는다

읽는 곳이 둘(`CanPlaceInSlot` · Step 04 UI)이라 이전이 두 줄이고, 지금 DataAsset을 만들면 **소비자가 하나인 계층**이 는다(CLAUDE.md §2). **이름만 적어둔다** — `05_Loot_DOCS.md` §8 미정 #10을 `BodySlots`·`ContainerOrder`·`StartingEquipment` 셋으로 넓혔다. 트리거는 **로비** 아니면 **§7-1**.
