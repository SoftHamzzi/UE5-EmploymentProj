# 05_Loot_03_Inventory — 구현 상태

**전체 상태: 골격만. 로직 0줄.**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷. 문서(`05_Loot_03_Inventory.md`)의 예정 코드와 혼동 금지.
> 최종 확인: 2026-08-25 (소스 3종 **재대조** ＋ 13차 검수·답변 ＋ **14차** 반영)

**소스 확인 결과 (2026-08-25 재대조)** — 세 파일 전부 존재. **골격이 8월 22일 스냅샷보다 훨씬 앞서 있다.**

| 파일 | 줄 | 상태 |
|---|---|---|
| `Public/Inventory/EPInventoryTypes.h` | 22 | `FEPInventoryEntry` 필드 **6개**(`SortKey` 포함) 선언 완료 |
| `Public/Inventory/EPInventoryComponent.h` | 131 | `FEPInventoryList` ＋ `TStructOpsTypeTraits` ＋ **컴포넌트 함수 40여 개 선언 완료.** 파일 앞부분 include·전방선언 3종도 들어갔다 |
| `Private/Inventory/EPInventoryComponent.cpp` | 189 | 생성자 3줄 완료(`SetIsReplicatedByDefault(true)` · `bCanEverTick = false` · `Entries.Owner = this`). **`RemoveEntry` / `RemoveEntryInternal` / `RemoveChildrenRecursive` 세 개만 본문이 있고 나머지는 빈 스텁** |

> **★ 옛 스냅샷이 *"엔진 템플릿 그대로 / `bCanEverTick = true`"* 라고 적고 있었다 (13차 답변).** 셋 다 사실이 아니었다 — **결함 #1·#3·#4가 이미 고쳐진 뒤**였다. `CLAUDE.md`가 STATUS를 진실의 원천으로 못박았으므로 **이 표가 소스보다 뒤에 있으면 안 된다.**

**`EmploymentProj.Build.cs:11`에 `NetCore` 추가 완료** — 없으면 `LNK2019: Z_Construct_UScriptStruct_FFastArraySerializerItem`. 실증됨.

### 남은 골격 결함 3건 (03-A 착수 전)

| # | 위치 | 문제 |
|---|---|---|
| **1** | `EPInventoryComponent.h:20` | **`TArray<class FEPInventoryEntry> Items;` — `class` 키워드가 남았다.** `#include`(`:8`)는 들어갔으므로 불완전 타입은 해결됐지만, `FEPInventoryEntry`는 `struct`라 **MSVC C4099**(`first seen using 'struct' now seen using 'class'`). include가 있는 지금은 **전방선언 흉내를 낼 이유도 없다** |
| **2** | `EPInventoryComponent.h:22` | **`TObjectPtr<UActorComponent> Owner`로 교체한다** (11차 결정). ~~전방 선언~~은 11차가 **기각한 대안**이다. **＋ `PostReplicatedReceive`에 `Cast<UEPInventoryComponent>`** — **둘은 같은 커밋이어야 한다.** 지금은 구체 타입이라 컴파일되고, **11차 결정을 적용하는 순간** 캐스트 없이는 깨진다 |
| **3** | `EPInventoryComponent.cpp:104` | **`FScopedInventoryNotify`의 정의가 소스 어디에도 없다.** 헤더 `:130`의 `friend struct` 선언은 **불완전 타입**이라 인스턴스를 못 만든다 — **03-A 첫 빌드에서 막힌다.** 정의는 `.cpp` 상단(03-7 참조) |

> **~~#3 `SetIsReplicatedByDefault`~~ · ~~#4 `bCanEverTick`~~ 은 이미 고쳐졌다** (2026-08-25 확인).

### 9차(2026-08-22) 기획 확대로 추가된 코드 항목

| 위치 | 추가할 것 |
|---|---|
| `Public/Data/EPItemData.h` | `TArray<FName> SlotPriority` — **순서가 곧 자동 배정 우선순위.** DT 배낭 행에 `["Back"]` |
| `Public/Data/EPLootDeveloperSettings.h` | `TArray<FName> BodySlots` — 몸 슬롯 12개 목록 |

### 11차(2026-08-23)에서 추가된 코드 항목 — 순서를 서버로

| 파일 | 추가 |
|---|---|
| `Public/Inventory/EPInventoryTypes.h` | **`FEPInventoryEntry::SortKey`(`int32`)** — 형제 스코프. `SlotId`가 있으면 무의미 |
| `Public/Inventory/EPInventoryComponent.h` | **public** `GetSortedContents(Container)` · `ReorderEntry(EntryId, PrevEntryId)` — **RPC가 아니다**<br>**private** `AssignSortKey` · `RenormalizeSortKeys` · `KeySpace_NextAtEnd`(**`const` 아님**) · `KeySpace_Min` · `KeySpace_NextAbove` · `KeyOf` · `ReorderEntryInternal(bRetry)` · `SortKeyStep = 1<<16` · `SortKeyGuard`<br>**＋ 파일 앞부분:** `#include "Inventory/EPInventoryTypes.h"` · 전방선언 3개 · `Owner`를 `TObjectPtr<UActorComponent>`로 |
| `Private/Inventory/EPInventoryComponent.cpp` | `InsertEntry`가 키 발급 · `MoveEntry`가 **부모 변경 시 재발급**(함정 4m) · `RemoveEntryInternal` ②가 **`bIsRoot`일 때만 `SortKey=0`**(함정 4n) · `AddSubtree`가 **자식 키 복원** |
| 디버그 | `EP.Inv.Reorder <EntryId> <PrevEntryId> [PlayerIndex]` (서버 전용) · `EP.Inv.Dump`에 `SortKey` 열, **행을 `SortKey` 순으로** |

> **★ `Server_ReorderEntry`(RPC)는 04-B다 (11차 검수).** 9차가 `Server_MoveEntry`에 적용한 규칙 그대로 — 소비자(드래그)가 거기다. `EP.Inv.Reorder`가 내부 함수 `ReorderEntry`를 직접 불러 **RPC 없이 계약이 닫힌다.**

> **★★ 12차 검수가 잡은 것 — `ReorderEntry`가 맨 앞에서 무한 재귀했다(함정 4t).** 재정규화 가드에 `PrevEntryId != INDEX_NONE`이 없으면 맨 앞 분기(`NewKey == PrevKey`)에서 조건이 **항상 참**이고 재정규화가 그걸 못 바꾼다. **아이템 2개 이상 컨테이너에서 하나를 맨 앞으로 끌면 서버가 죽는다.** 경계/고갈 판정을 분리하고 `ReorderEntryInternal(bRetry)` ＋ `ensure`로 종료를 문법으로 보장한다. **제자리 드롭 조기 반환(4u)** 도 같이 들어갔다.

> **★ 11차 검수가 잡은 결함 둘이 반영돼 있다.**
> - **함정 4q — 키 공간을 `GetSortedContents`로 구하면 동률이 난다.** `KeySpace_*` 셋과 `RenormalizeSortKeys`는 **부모가 같은 것 전부**를 본다(슬롯 포함). **접두어가 스코프를 이름에 싣는다** — 같은 혼동이 두 번(4q·4s) 났다. `InsertEntry`의 삼항과 `MoveEntry`의 `NewSlotId` 조건이 **사라졌다** — 예외 셋이 없어지고 코드가 줄었다
> - **함정 4r — 재정규화는 세 분기 공통이다.** 이분 고갈뿐 아니라 맨 앞/맨 뒤의 무한 증감도 막는다. 발급 쪽 가드는 `KeySpace_NextAtEnd` 안에 있다(유일 발급 지점). **재배치 쪽 가드는 12차에서 경계/고갈로 분리됐다**(함정 4t)

> **나머지는 03-A다 — 단 스냅샷의 루트/자식 규칙만 03-B다.** `InsertEntry`가 발급하고 `MoveEntry`가 재발급하므로 정의가 뒤에 있으면 03-A가 컴파일되지 않는다. 반면 루트/자식 분기는 `RemoveEntry`/`AddSubtree`가 있어야 표현된다.

> **★ `EP.Inv.Reorder`가 없으면 `Server_ReorderEntry`는 두 단계 뒤(04-B 드래그)에야 처음 실행된다.** 04-A가 `EP.Inv.Add`/`EP.Inv.Move`를 요구하는 것과 같은 이유다 (14차에 `EP.Inv.Equip` 폐기).

> **★ `RenormalizeSortKeys`는 완료 조건이 없으면 죽은 코드가 된다.** 같은 틈에 ~16회 꽂아야 도달하므로 정상 개발 중에는 한 번도 안 돈다. **완료 조건 16(*"같은 틈에 20번 연속으로 꽂아도 순서가 안 깨진다"*)이 그것을 강제한다.**

### 14차 (2026-08-25) — 사용자 지적. `Server_EquipBackpack` 삭제

> 질문: *"어차피 옷도 컨테이너고 하의도 컨테이너인데 `EquipBackpack`을 만들 이유가 있었나."*

| 위치 | 변경 |
|---|---|
| `EPInventoryComponent.h` | **`Server_EquipBackpack` 선언 삭제.** 착용의 클라 표면은 **04-B의 `Server_MoveEntry` 하나**로 확정. 13차가 붙이라고 했던 `UFUNCTION(Server, Reliable)`도 같이 없어진다 |
| 디버그 (04-A) | **`EP.Inv.Equip` 폐기** — `EP.Inv.Move <id> -1 Torso`가 정확히 그 일이다. 얇은 별칭이라 어휘만 둘로 늘린다 |
| 함정 **9f** | `Server_EquipBackpack` 전용 → **`Server_*` 일반**으로. Step 03의 대상은 `Server_DropItem` 하나, 04-B에서 셋이 는다 |

**근거 셋.**

1. **9차의 *"좁은 RPC가 넓은 RPC보다 낫다"* 는 `Server_MoveEntry`와의 비교인데, 04-B가 그것을 연다**(`05_Loot_04_InventoryUI.md:610,619`). 넓은 문이 열린 뒤의 좁은 문은 **공격 표면을 하나도 줄이지 않는다** — 조작된 클라는 `Server_MoveEntry(id, -1, "Back")`을 부른다
2. **13차가 옮긴 04-A에도 호출자가 0개다.** 근거였던 `EP.Inv.Equip`은 **콘솔 커맨드**이고, 문서가 두 번 확립한 규칙(*"커맨드는 내부 함수를 직접 부른다. RPC 표면을 열지 않는다"*)이 그대로 적용된다
3. **배낭이 특별할 근거가 없다.** `SlotPriority`(아이템) ＋ `BodySlots`(설정)가 상의·하의·외투·배낭을 같은 모양으로 만든다. 문서 자신이 `TryAutoEquip`에 *"배낭 전용 함수를 만들지 않는다"* 를 적용해 놓고 **RPC 이름에만 적용하지 않았다**

> **13차 판정이 부족했던 건이다** — *"호출자 0개"* 를 찾고도 **삭제가 아니라 이동**을 골랐다. 반영: `05_Loot_03_Inventory.md`(03-2 별도 절 재작성·03-6 RPC 삭제·함정 9f·체크포인트 ④·변경 이력), `05_Loot_04_InventoryUI.md`(04-A 커맨드), `Mine/EquipmentSlots.md` **§15 신설**.

---

### 13차 **답변** 반영 (2026-08-25) — `Review/05_Loot_REVIEW13_Answer.md`

| 위치 | 변경 |
|---|---|
| `EPInventoryComponent.h` | **`bool CanPlaceInSlot(int32 Parent, FName SlotId, FName ItemId) const`** 신설 — `MoveEntry` 검사 2·3·4를 뽑는다. 소비자 셋(`MoveEntry` · `AddSubtree` · §7-3) |
| `EPInventoryComponent.cpp` | **`MoveEntry`가 키를 재부모 *전에* 구한다** — `InsertEntry`와 같은 결함이었다(함정 4x가 두 함수로 넓어졌다). 안 고치면 키가 컨테이너 사이로 전염된다<br>**`FScopedInventoryNotify` 정의를 `.cpp` 상단에** — 골격 결함 #3 |
| `Public/Data/EPItemData.h` 검증 | **`IsDataValid`에 `SlotSize >= 1`** — 본체 0칸에서 `0 + 0 <= 0`이 참이라 0칸 아이템이 무한히 들어간다 |
| 디버그 | **`EP.Inv.Add <ItemId> [Container] [PlayerIndex]`** — `[Container]`가 없으면 `MaxSlots = 0` 전환이 03-A 완료 조건 아홉 개를 죽인다 |
| **구간** | **옛 03-B(배낭) 삭제.** `Server_EquipBackpack` → 04-A(→ **14차에 함수째 삭제**), 완료 조건 7 후반 → **03-A 마지막 검증** |

> **★ 답변의 수치 하나가 틀려서 안 따랐다.** 완료 조건 18의 경계 도달 횟수를 답변은 32,763회라 했는데(`65,536 × 32,763 = 2,147,278,848`이라 적었으나 실제는 `2,147,155,968`), 직접 계산하면 **32,764회**다 — `-65536 × 32764 = -2,147,221,504 = MIN_int32 + SortKeyGuard`. 문서에는 **32,764**로 적었다.

> **답변이 권했으나 안 하기로 한 것:** 이분 중간값의 `int64` 캐스트(도달에 6만 회 필요 — 답변 자신이 선택 사항으로 뒀다), 함정표 `4a` 결번 채우기.

---

### 13차 검수(2026-08-25)에서 추가·변경된 코드 항목

> 근거: `Review/05_Loot_REVIEW_Inventory.md`. 결정 4건(A-2·A-3·A-5·D-2)은 사용자 확정.

| 위치 | 변경 |
|---|---|
| `EPInventoryComponent.h` | **`AddSubtree(int32 Parent, FName SlotId, const TArray&)`** — 목적지가 `(Parent, SlotId)` 쌍. **기본값 금지**<br>**`KeySpace_NextAbove` / `KeyOf`를 `bool` 반환 + out 파라미터로** — `INDEX_NONE` 센티널이 키 −1과 충돌한다(함정 4w)<br>~~**`Server_EquipBackpack`에 `UFUNCTION(Server, Reliable)`**~~ → **14차에 선언째 삭제**(함정 9f는 `Server_*` 일반으로)<br>`Owner`를 `TObjectPtr<UActorComponent>`로 (골격 결함 #2) |
| `EPInventoryComponent.cpp` | **`MoveEntry`가 배열 원소의 참조에 쓴다** — `FindEntry` 복사본에 쓰면 무동작인데 `true`(함정 4v). 검사 6의 지역 변수는 `Cur`로 개명<br>**`RemoveEntryInternal` ②가 `bIsRoot`일 때 `SlotId = NAME_None`도** (함정 4z)<br>**`InsertEntry`가 키를 `AddDefaulted` 전에 발급** (함정 4x)<br>**`AddSubtree`가 `SlotId`면 `CanFit`을 건너뛴다** (함정 4y)<br>**`GetOwner<AEPCharacter>()` 널 가드** (§7-1 대비)<br>**가드를 "단일 쓰기 지점 다섯"에 건다** — `InsertEntry`·`SetEntryCharges`·`RemoveSelf`·`AssignSortKey`·**`MoveEntry`** |
| `EPInventoryComponent.h` (`FEPInventoryList`) | **`PostReplicatedReceive`에 `Cast<UEPInventoryComponent>`** — 지금 문서대로면 컴파일 안 됨 |
| `Public/Data/EPLootDeveloperSettings.h` | **`TArray<FName> StartingEquipment`** — `["Shirt_Basic","Pants_Basic"]`. 소비자는 `BeginPlay` → `TryAutoEquip` (03-B) |
| `DT_Items` | **용량표 확정** — 상의 `SlotSize 11 / Cap 10`, 하의 `6 / 5`, 배낭A `15 / 12`, 배낭B `10 / 8`. 규칙은 **`Capacity < SlotSize`**(등호 없음). `MaxSlots`는 **테스트값 10**이고 최종 **0** |
| 디버그 | **`EP.Inv.Move <EntryId> <NewParent> <SlotId> [PlayerIndex]`** (서버 전용) — 없으면 `MoveEntry`가 Step 04까지 한 줄도 안 돈다 |

> **★★ 구간이 하나 움직였다.** `AddSubtree` · `TryAutoEquip` · `StartingEquipment`가 **03-B로 내려갔다.** 셋 다 호출자가 03-B(`OnInteract`)에 있어서, 03-B에 두면 **죽은 코드와 그것을 깨울 커맨드(`EP.Inv.AutoEquip`)가 같이 늘어난다.** 9차가 다섯 개를 03-A로 **올린** 것과 방향만 반대이고 기준은 같다 — *"호출자와 같은 구간에."*

---

### 10차(2026-08-23)에서 추가된 코드 항목

| 위치 | 추가할 것 |
|---|---|
| `Public/Data/EPLootDeveloperSettings.h` | `TArray<FName> ContainerOrder` — `["Coat","Torso","Legs","Back","Wrist"]`. **`GetInsertionOrder()`(03-B)는 이 배열을 그대로 반환하지 않는다** — 맨 앞에 `INDEX_NONE`(본체)을 붙이고 이름을 `GetEntryInSlot`으로 EntryId로 옮긴다. **본체가 빠지면 테스트값(`MaxSlots=10`) 동안 완료 조건 2~6이 전부 걸린다.** 본체를 0칸으로 내린 뒤에는 `CanFit`이 걸러내므로 무해하다 (13차) |
| `MoveEntry` 검사 목록 | **검사 0 — 목적지가 지금 자리와 같으면 `false`.** 없으면 검사 5의 `CanFit`이 자기 크기를 두 번 센다. `Server_MoveEntry`가 Step 04에서 열리므로 UI가 안 불러도 경로가 남는다 |
| `RemoveEntryInternal` | **①→③ 순서 계약을 코드 주석으로.** 순서를 바꿔도 컴파일되고, 증상이 *"0으로 덮인다"* 가 아니라 *"안 불린다"* 라 더 조용하다 (함정 4k) |

**전부 03-A다.** DT 필드 추가는 코드가 아니라 데이터 마이그레이션이라 늦을수록 비싸고, `BodySlots`는 `MoveEntry`의 정합 검사가 읽는다.

---

## 진행 — 둘로 나눈다 (13차에 셋에서 줄었다)

완료 조건 19개는 다른 단계 두 개 분량이라 작업을 둘로 나눈다. **파일은 쪼개지 않는다** (경계에서 `RemoveEntry`가 갈려 stale이 세 번 연속 난 것이 이유 — 문서 §체크포인트).

| | 범위 | 완료 조건 | 상태 |
|---|---|---|---|
| **03-A 코어** | 03-1 · 03-2 · 03-3 · **03-7** · 03-9<br>**＋ `GetEntryInSlot(Parent, SlotId)`**<br>**＋ `MoveEntry` (정합·사이클 검사 포함)**<br>**＋ `GetCapacity` 통째로** (컨테이너 갈래 포함 — `EP.Inv.Move`가 부른다)<br>**＋ `ActiveHotbarIndex` 필드**<br>**＋ `FEPItemData::SlotPriority`**<br>**＋ `UEPLootDeveloperSettings::BodySlots`**<br>**＋ `SortKey` 일습**<br>**＋ `EP.Inv.Move` · `EP.Inv.Add [Container]` (13차)**<br>**＋ `CanPlaceInSlot` (13차 답변)** | 2~6, **7의 후반**, 14·15·17~19 | ⬜ 미착수 |
| **03-B 줍기·버리기** | 03-4 · 03-5 · 03-6<br>**＋ `GetInsertionOrder()`**<br>**＋ `AddSubtree` · `TryAutoEquip` · `StartingEquipment`** (13차) | 1, 7의 전반, 8~13, 16 + 이월 2건 | ⬜ 미착수 |

> **★★ 옛 03-B(배낭) 구간이 없어졌다 (13차 답변).** 거기 남은 새 코드가 `Server_EquipBackpack`(3줄) 하나였는데 **Step 03에 호출자가 0개**다 — 자동 착용은 서버 내부(`TryAutoEquip`), 벗기는 `Server_DropItem`, 검증은 `EP.Inv.Move`, 수동 착용 UI는 Step 04. **9차가 `Server_MoveEntry`에, 11차가 `Server_ReorderEntry`에 적용한 규칙이 여기만 빠져 있었다** → 13차는 **04-A로 보냈다**(`EP.Inv.Equip`이 있는 자리). 완료 조건 7의 후반(독립 풀)은 **03-A의 마지막 검증**으로 간다.
>
> **★★ 14차에 함수째 없어졌다** — `EP.Inv.Equip`이 **콘솔 커맨드**라 `EP.Inv.Reorder`·`EP.Inv.Move`와 같이 **내부 함수를 직접 부른다.** 04-A에도 호출자가 0개였다. 아래 별도 절.

> **★ 03-7(알림)은 03-A다.** `FScopedInventoryNotify`를 03-3의 `AddItem`·`SetEntryCharges`가 쓰므로 정의가 03-B에 있으면 **03-A가 컴파일되지 않는다.** 8차 검수에서 옮겨졌고, `LOOT_STATUS.md`가 한동안 옛 분할선(03-B에 03-7)을 들고 있었다 — 2026-08-20 수정.

> **★ 9차(2026-08-22)에서 같은 이유로 다섯 개가 03-A로 왔다.** ~~03-B의 `Server_EquipBackpack`이 `MoveEntry`의 래퍼이고~~(14차 삭제 — 지금 근거는 **`EP.Inv.Move`가 `MoveEntry`를 직접 부른다**), `GetCapacity`가 `GetEntryInSlot`을 쓴다. **`MoveEntry`는 `RemoveEntry`·`AddSubtree`에 의존하지 않으므로 03-A 단독 컴파일 조건을 깨지 않는다** — 확인함. `SlotPriority`는 DT **데이터 마이그레이션**이라 늦을수록 비싸고, `BodySlots`는 `MoveEntry`의 정합 검사가 읽는다.

> **★ 함정표 ★★는 03-B에 몰려 있고, 9차에서 03-A에 4건(4h·4i·4j·4k)이 추가됐다.**

> **★ 9차에서 03-A 코드가 늘었다.** `MoveEntry`(검사 7개) + `GetEntryInSlot` + `BodySlots`/`SlotPriority` 데이터로 **03-A가 이전 추정보다 무겁다.** 대신 `EquippedEntryId`/`EquippedBackpackEntryId` 관련 분기와 함정 2건(3h·5b)이 사라져 일부 상쇄된다.

---

## 완료 조건 대조

| # | 완료 조건 | 담당 | 상태 |
|---|---|---|---|
| 1 | 주운 아이템이 인벤토리에 들어가고 클라이언트에 복제된다 | 03-B | ⬜ |
| 2 | 붕대 3개를 주우면 엔트리가 3개다 (스택 없음) | 03-A | ⬜ |
| 3 | 현금뭉치 둘 → 엔트리 1개, `Charges` 합산 (`bFungible`) | 03-A | ⬜ |
| 4 | 칸이 모자라면 **아무것도 안 들어가고** 픽업이 그대로 남는다 | 03-A | ⬜ |
| 5 | 가방이 꽉 차도 현금뭉치·탄약상자는 들어간다 | 03-A | ⬜ |
| 6 | 무기(`SlotSize=5`) → `UsedSlots` +5 (개수가 아니라 **칸 수**) | 03-A | ⬜ |
| 7 | 배낭을 주우면 자동 착용 ＋ **별도 풀**이 열린다 | **후반 03-A / 전반(자동 착용) 03-B** | ⬜ |
| 8 | 배낭을 버리면 안의 아이템이 같이 나가고 **고아 엔트리가 없다** | 03-B | ⬜ |
| 9 | 배낭을 버렸다 되주우면 내용물이 그대로 돌아온다 (`Parent` 유지) | 03-B | ⬜ |
| 10 | 12/30까지 쏘고 버렸다 주우면 `Charges`가 12 그대로 | 03-B | ⬜ |
| 11 | 버린 직후 0.5초 동안 회색 프롬프트 + 서버 거부 | 03-B | ⬜ |
| 12 | 다른 클라이언트에 내 인벤토리가 복제되지 않는다 (`COND_OwnerOnly`) | 03-A | ⬜ |
| 13 | 줍고 버려도 기존 `EntryId`가 **재번호되지 않는다** | 03-B | ⬜ |
| 14 | **★ `EP.Inv.Reorder`로 자리를 바꾸면 `Dump` 순서가 바뀌고 클라·서버가 같다** (11차) | 03-A | ⬜ |
| 15 | **★ 다른 컨테이너로 옮기면 목적지 맨 뒤** — 옛 키를 들고 가지 않는다 (함정 4m) | 03-A | ⬜ |
| 16 | **★ 배낭에 4개 이상 넣고 순서를 섞은 뒤 버렸다 주우면 그 순서 그대로** (11차) | 03-B | ⬜ |
| 17 | **★ 핫바에 꽂았다 빼면 원래 자리로 돌아온다** — 동률이 안 난다 (함정 4q) | 03-A | ⬜ |
| 18 | **★ 이분 고갈 → 재정규화 / 맨 앞 20회 → `ensure` 없이 순서가 바뀐다** (함정 4r·4t) | 03-A | ⬜ |
| 19 | **★ 원래 자리에 도로 놓으면 `SortKey`가 안 바뀐다** (함정 4u) | 03-A | ⬜ |

> **★ 15·17은 `EP.Inv.Move` 없이는 검증할 수 없다 (13차).** `MoveEntry`를 부르는 커맨드가 하나도 없어서 **03-A가 만드는 코드가 Step 04까지 한 줄도 안 돌 예정이었다.** `EP.Inv.Move <EntryId> <NewParent> <SlotId>`를 03-A에 넣는다 — `EP.Inv.Reorder`가 `ReorderEntry`를 직접 부르는 것과 같은 형태다.

**★ Step 02에서 이월된 검증 2건** (번호를 매기지 않는다 — 위 13개가 Step 03의 완료 조건이고 이 둘은 Step 02의 미필이다)

| | 이월 항목 | 상태 |
|---|---|---|
| 02-3 | **사거리 밖 거부** — `EPGA_Interact.cpp:69-75`가 한 번도 돈 적이 없다 | ⬜ 03-B |
| 02-4 | **동시 F 경쟁** — `bClaimed` 선점. 거부당한 쪽 인벤토리가 **비어 있어야** 한다 | ⬜ 03-B |

> **둘 다 03-B에서 본다.** 그 전에는 버린 픽업을 만드는 경로 자체가 없다.

> **★ 완료 조건 10의 "12/30"은 이 단계에서 확인할 수 없다.** `Charges`를 12로 만들려면 발사가 필요하고 그건 장착 상태여야 하는데 장착 경로가 Step 05다. **여기서는 "`EP.Inv.Drop` 후 `Charges` 보존"까지만** 본다 — 값 복사가 도는지는 그걸로 증명된다.

---

## 설계 문답 (개발 기록)

> 구현 전 설계 확인 과정에서 나온 질문과 결론. **결정의 근거를 남기는 것이 목적**이라, 나중에 같은 의문이 다시 올라올 때 여기부터 본다.

### Q. `EntryId`가 증가만 하면 오버플로우가 나지 않나? 빠르게 줍고 버리면?

**결론: 도달 불가능하다. `int32` 유지.**

`int32` 최대는 **21억 4748만**이다. `NextEntryId`는 `UEPInventoryComponent`의 서버 전용 필드고 복제하지 않으므로 **수명이 매치 단위**다.

| 가정 | 소진까지 |
|---|---|
| 매치 30분, 초당 1회 줍기/버리기 | 1,800개 사용 (0.0001%) |
| **초당 100회를 쉬지 않고** (매크로로도 불가능) | **248일 연속** |
| 세이브 도입 후, 하루 1,000개씩 계정 누적 | **5,800년** |

정말 문제가 되면 그때 `int64`로 넓히면 되고, `ParentEntryId`와 RPC 시그니처만 바꾸는 **기계적 변경**이다. 지금 선불할 이유가 없다.

> **단, 세이브(로드맵 5단계)에는 `NextEntryId`를 반드시 넣는다.** 빠뜨리면 로드 후 1부터 재발급해 기존 엔트리와 충돌하고, 하필 `ParentEntryId`를 오염시켜 아이템이 엉뚱한 컨테이너로 들어간다. 오버플로우보다 이쪽이 압도적으로 현실적인 위험이다.

### Q. 그럼 링크드 리스트가 낫지 않나?

**결론: 아니다. 문제를 풀지 못하고, 풀려고 하면 더 나쁜 버그를 만든다.**

**① 식별 문제가 그대로 남는다.** 리스트로 바꿔도 클라가 서버에 *"이걸 버려"* 를 보내려면 번호가 필요하다. **포인터는 복제되지 않는다.** 결국 노드마다 ID를 붙이게 되고 원점이다.

**② 델타 복제를 통째로 잃는다.** `FFastArraySerializer`는 아이템 배열이 **최상위 `UPROPERTY` `TArray`** 일 것을 요구한다(`FastArraySerializer.h:721-728`). 리스트로 가면 인벤토리 전체를 매번 통짜로 보낸다.

**③ 번호 재사용(free list)은 ABA 버그를 만든다 — 이게 핵심이다.**

```
클라: Server_DropItem(5) 발사
      ↓ (RTT 사이에)
서버: 5번이 이미 제거됨 → 번호 재활용 → 새로 주운 AK가 5번이 됨
      ↓
서버: 5번을 버림 → 엉뚱한 아이템이 바닥에 떨어진다
```

단조 증가면 5번은 **영영 없는 번호**라 요청이 그냥 실패한다. 재사용은 그 보호막을 없애는 대가로 **절대 오지 않을 오버플로우**를 막는 거래다.

`ParentEntryId`도 같다 — 배낭 번호가 재활용되면 그 안의 아이템이 **다른 배낭에 매달린다.** 무증상이고, 산발적이고, 재현이 안 된다.

> **한 줄 요약:** 20억 번은 도달 불가고, 번호 재사용은 **도달 가능한** 버그를 만든다. 일어나지 않을 일을 막으려고 일어날 일을 사는 거래라 하지 않는다.

> 배열 인덱스를 안 쓰는 근거는 별개로 엔진 주석에 있다 — *"the \*order\* of the list is not guaranteed to be identical between client and server in all cases."* (`FastArraySerializer.h:54`)

### Q. 총도 배낭처럼 컨테이너를 갖고 부착물을 담는 건가?

**결론: 링크는 같고 판정 경로가 다르다. 총은 컨테이너가 아니다.**

| | 배낭 (수납) | 총 (부착) |
|---|---|---|
| `SlotId` | `NAME_None` | `Optic` / `Muzzle` / ... |
| 제한 | **칸 수** (`ContainerCapacity`) | **슬롯 이름** (슬롯당 하나) |
| `GetUsedSlots` | 합산됨 | `continue` — **아예 안 센다** |
| `GetCapacity` | DT의 `ContainerCapacity` | **0** |

`GetCapacity(총) == 0`이라 총에 뭘 **수납**하는 건 원천 봉쇄다. 조준경이 들어가는 건 `CanFit`을 통과해서가 아니라 **칸 검사를 거치지 않기 때문**이다.

`ParentEntryId` 하나로 배낭·부착물·상자가 전부 같은 표현이 되는 것이 이 설계의 이득이고, 그래서 **`SlotId`를 Step 03에 미리 넣는다** — 읽는 코드는 아직 0곳이지만, 없으면 용량 판정식을 §7-3에서 다시 써야 한다. 부착 깊이는 **1**이다 (조준경에 또 붙이지 않는다).

---

## 남은 작업

Step 03 전체. **03-A → 03-B → 03-B** 순서로 진행하고, 각 구간 끝에서 멈춰 검증한다.

| 구간 | 멈춰서 검증할 것 |
|---|---|
| 03-A | `EP.Inv.Add`로 칸 합산 · `bFungible` · `COND_OwnerOnly`. **`RemoveEntry`/`AddSubtree` 없이 컴파일·실행된다** |
| 03-B | 배낭을 주우면 두 번째 풀이 열리는가. 아직 못 버린다 |
| 03-B | `RemoveEntry` / `AddSubtree` / 캐스케이드 / `Server_DropItem` + 이월 2건 |
