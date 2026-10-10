# 05_Loot_03_Inventory — 구현 상태

**전체 상태: 03-A 구현 끝, 핵심 PIE 통과 (2026-10-11). 미검증 PIE(16 · 17 · 18 · 19)와 1a의 BP 지정자 하나가 남았다. 03-B는 미착수(스텁 5개).**

> 세션 시작 시 이 파일을 반드시 읽을 것. **현재 코드 상태와 남은 작업만** 적는다 — 구현서의 예정 코드와 혼동 금지. 결정 경위는 `05_Loot_DECISIONS.md`, 문서 개정 경위는 구현서 맨 아래 변경 이력.
> 최종 코드 대조: **2026-09-30** (소스 3종 + `EPInventoryCheats.cpp/h`).
> **10-04 보충:** `BodySlots` 데이터 선결, `CanPlaceInSlot` 부착 거절은 의도, 완료 조건 번호를 구현서에 맞춤. 버그 2건은 그대로(소스 재확인).
> **10-07 보충:** 확장성 재검토(D-098~D-103)로 이미 짠 03-A 코드에 바꿀 곳이 생겼다 — 남은 작업 1. 소스는 다시 읽지 않았다.
> **10-08 보충:** Codex 반박 검토(D-104~D-106)와 설계 판정(D-108)이 1e · 5에 붙었다. D-107은 지금 코드와 무관하다.
> **10-11 대조:** 코드(`EPTypes.h` · `EPInventoryTypes.h` · `EPInventoryComponent.h/.cpp` · `EPInventoryCheats.h/.cpp` · `EPCharacter.cpp` · `DefaultGame.ini`)를 읽었다. 1a~1g · 2 · 4a · 4b가 코드에 있다. PIE는 사용자가 돌렸다(아래 3).
> **10-09 대조:** 작업 중 코드(`git diff` — `EPTypes.h` · `EPInventoryTypes.h` · `EPItemDefinitionSubsystem.cpp`)를 읽었다. 1b · 1c 완료, 1a 진행 중(아래). 1d~1g는 아직 손대지 않았다.

---

## 남은 작업 (2026-10-07 갱신)

**~~Step 03 전체~~ — 03-A 로직은 거의 끝나 있다. 다만 2026-10-07 확장성 결정으로 바꿀 곳이 생겼다.**
가이드: `Guide/05_Loot_03_Inventory_A_Core_Guide.md` (읽기 + 개정, L2 — 불변식 1~36 · PIE 표 · 자기 점검 §A 1~7·15~19·24~26 / §B 8~14·20~23). **2026-10-07 · 10-08 변경분 반영 완료.**

| 순서 | 할 것 | 비고 |
|---|---|---|
| 0 | ~~**가이드 갱신**~~ — **2026-10-07 완료.** 가이드 §2 "바꾼다" 표가 1a~1g의 이름 목록이고, 불변식 23~35가 새 성질이다 | §A 15~19 · 24~26을 **코딩 전에** 답해 둔다 |
| 1 | **확장성 반영 (D-098~D-103)** — 정답지는 구현서다. 아래 a~g | 기존 버그 2건(`KeyOf` · `AddItem` null)은 이 작업에서 같이 고친다 — 구현서의 새 `KeyOf` 본문이 `SortKey`를 돌려주고 반환형이 `bool`이다 |
| 1a | `EPTypes.h` — `FEPItemStat` 신설, `FEPItemState` = `ItemUid`(`NotReplicated`) · `Charges` · `Stats`. **`Durability` 삭제** + `EP.Item.State` 로그 | `Implementation/05_Loot_00_ItemCore.md` 00-1 · 00-9. Step 00은 끝난 단계지만 이 변경은 03이 끌고 온다<br>**🔶 진행 중 (10-09):** `Durability` 삭제 · 로그 교체는 끝(소스에 `Durability` 0곳). `EPTypes.h`에 **구현서와 어긋난 곳 넷** — 선언 순서(컴파일 에러) · include · `Value` 초기값 · BP 지정자. 00-1 "옮겨 적을 때 지킬 넷" 표<br>**✅ 대부분 완료 (10-11)** — 선언 순서 · include · `Value = 0` OK. **남은 하나: 4번 BP 지정자** — `ItemUid` · `Stats` · `Tag` · `Value`가 아직 `BlueprintReadWrite`다(`Charges`만 ReadWrite여야 한다). `FindStat` · `SetStat`은 들어가 있다(D-113) |
| 1b | `MakeItemState` 끝에 `OutState.ItemUid = FGuid::NewGuid();` | 00-6. **발급 지점은 여기 하나**<br>**✅ 코드 있음 (10-09)** — `EPItemDefinitionSubsystem.cpp:62`. **PIE 통과 (10-11, 완료 조건 00)** |
| 1c | `EPInventoryTypes.h` — `SortKey`를 `int64`로 | 03-1 · D-102<br>**✅ 코드 있음 (10-09)** — `EPInventoryTypes.h:21`. `Dump`의 `%lld`(1g)와 키 함수(1d · 1e)도 `int64`로 바뀌었다 (10-11) |
| 1d | `EPInventoryComponent.h` — `FIndex` · `EnsureIndex` · `MarkIndexDirty` · `FindEntryPtr` · `FindEntryMutable` · `SetEntryStat`/`GetEntryStat` · `GetReplicationCondition` 재정의 · `BeginPlay`/`EndPlay` · `friend struct FEPInventoryList` · 키 함수 `int64` · **`CheckIdentity` · `AppendEntry`(2026-10-08)** | 03-2 헤더 블록<br>**✅ 코드 있음 (10-11)** — 색인 · `Find*`는 4a로 `FEPInventoryList`에 있다 |
| 1e | `EPInventoryComponent.cpp` — 조회 전부 색인 경유, 쓰기 지점 `FindEntryMutable`, **무효화 넷**(`InsertEntry`·`RemoveSelf`·`MoveEntry`·클라 수신), **정체 검사 `CheckIdentity`를 `AddItem` 합치기 앞 · `InsertEntry`에서 부른다(D-104)**, **`AddItem`의 `bFungible` 분기 맨 앞에서 `Stats`가 비어 있는지 검사한다(D-108)**, **`InsertEntry` = `CheckIdentity` + 키 + 몸통 `AppendEntry`(색인을 안 읽는다, D-106)**, `GetLifetimeReplicatedProps`에서 조건 제거, `PostReplicatedReceive`는 무효화 → `Broadcast`, `ReorderEntryInternal`의 제자리 판정을 키 계산 앞으로 + `uint64` 중점 | 03-2 · 03-3 · 03-7 · 03-A-부록<br>**✅ 코드 있음 (10-11).** 병합 · 용량 · 이동 · 복제는 PIE 통과. **재배치(16 · 18 · 19)는 PIE 미검증** |
| 1f | `AEPCharacter` 생성자 — `bReplicateUsingRegisteredSubObjectList = true;` | **빠뜨리면 인벤토리가 아무에게도 복제되지 않는다**(`BeginPlay`가 끊는다 — 함정 1)<br>**✅ 코드 있음 (10-11)** — `EPCharacter.cpp:92`. 복제 PIE 통과 |
| 1g | `EPInventoryCheats` — `EPInvDumpServer` 추가, `Dump`의 `SortKey`를 `%lld`, `Uid` 열 | 03-9<br>**✅ 완료 (10-11)** — `EPInvDumpServer`(`BlueprintAuthorityOnly`) · `SortKey` `%lld` · `Uid` 열(클라는 `-`). 덤프 본문은 `EPInventoryCheats.cpp`의 `DumpInventory` 하나다 |
| 2 | **데이터** — Project Settings의 `BodySlots` 12개, `DT_Items` 배낭·무기 행의 `SlotPriority` | 아래 "데이터 선결" 절. 없으면 슬롯 이동이 전부 거절된다<br>**🔶 부분 (10-11)** — `BodySlots` 12개는 `DefaultGame.ini:42-53`에 있다. `DT_Items` `SlotPriority`는 배낭 행만 확인됨(총 · 근접 행은 아직 없다) |
| 3 | **PIE 검증** — `EPInvAdd`/`EPInvMove`/`EPInvReorder`/`EPInvDump`/`EPInvDumpAll`/`EPInvDumpServer`로 03-A 몫(2~6, 4 전반, 7 후반, 13~19, 21)을 돌린다. **1 전엔 13·16·21을, 2 전엔 7 후반·17을 통과로 치지 않는다** | 절차는 가이드 §5. **18 ①은 이제 같은 틈 32회**다<br>**🔶 부분 통과 (10-11)** — 통과: 00 · 2 · 3 · 4 · 5 · 6 · 7 후반 · 14 · 15 · 이동 · 복제 · 21. **미검증: 16 · 17 · 18 ① ② · 19** — Step 04 UI에서 "한 틈에 32번 이상 끼워 넣기"를 시나리오에 넣어 확인한다 |
| 4 | **`IsFungible()` — 삭제 보류 (2026-10-11 사용자 결정).** UI에서 쓸 수 있어 남긴다. 지우거나 `Data->bFungible`에 위임할지는 Step 04에서 정한다 | 지금 반환값이 맞는지는 "함수별 상태" 그대로 |
| 4a | **D-113 · D-114 이동** — 색인 · 원소 접근 · 번호 발급기를 `FEPInventoryList`로, 수치 찾기를 `FEPItemState`로. **✅ 구현서 · 가이드 반영 완료, 코드 이동 완료 (2026-10-11).** 3의 통과분 뒤, 5 전에 한다 | 정답지: `_A_Core.md` 03-1 "리스트는 데이터를" 절 · 03-A-부록 · 03-7, `_00_ItemCore.md` 00-1. **검증 = 새 PIE 표 없음.** 03-A 통과 항목(00 · 2 · 3 · 4 · 5 · 6 · 7 후반 · 13 · 14 · 15 · 21 · 이동)을 다시 돌리고 + 컴파일 + 이동 전후 `EPInvDumpServer` 비교(`EntryId` · `Parent` · `SlotId` · 키 열은 줄 단위로 같다, `Uid`는 성질로) — 가이드 §5 끝<br>**회귀: 통과 항목 재실행 OK (사용자, 10-11).** 이동 전 기준 덤프는 찍지 않았다 |
| 4b | **디버그 덤프를 구현서대로 치트로 옮긴다 (2026-10-11 사용자 결정 — ⓐ).** 작업 폴더 코드의 `UEPInventoryComponent::DumpToLog`(`.h:231`)와 `Server_DumpToLog`(`UFUNCTION(Server, Reliable)`, `.h:235`)를 **없애고**, `EPInvDumpServer`를 `UFUNCTION(Exec, BlueprintAuthorityOnly)`로 두어 서버에서 덤프 함수를 돌린다(`_A_Core.md` 03-9). 처음 요청한 형태(헤더 · cpp · 호출부를 `#if !UE_BUILD_SHIPPING`)는 UHT가 거절한다 — `UFUNCTION`은 정해진 블록(`WITH_EDITOR` 등) 밖의 `#if` 안에 둘 수 없다(UE 5.7 `UhtHeaderFileParser.cs:715`, 허용 목록 `:120-123`). Shipping에서는 CheatManager가 생기지 않아(`PlayerController.cpp:1155-1171`) **덤프가 불릴 경로가 없다.** 코드와 리플렉션은 바이너리에 남는다 — 그것까지 빼려면 Lyra `LyraBotCheats`처럼 선언은 가드 밖, 본문만 `#if UE_WITH_CHEAT_MANAGER`(지금은 하지 않는다) | 확인: 개발 구성 빌드 → `EPInvDumpServer`가 서버 로그에 표를 찍는다(가이드 §5의 21). Shipping 구성 컴파일 통과. 컴포넌트에 `DumpToLog` 이름이 0곳<br>**✅ 코드 완료 (10-11)** — 컴포넌트의 `DumpToLog` · `Server_DumpToLog` 삭제(grep 0건), `EPInvDumpServer`는 `BlueprintAuthorityOnly` Exec. 개발 빌드 · PIE OK. **Shipping 구성 컴파일은 미확인** |
| 5 | **03-B** — `AddSubtree`(**넣기 전 전부 검사 — 함정 12c · 12d**, 루프는 `AppendEntry`만 — D-105 · D-106)/**`RemoveEntry` 일괄 제거**(재귀는 표시만, 끝에서 `RemoveSelf(Doomed)` 한 번 — D-106)/`TryAutoEquip`/`GetInsertionOrder`/`Server_DropItem_Implementation`/`SpawnPickupInFront` 본체 채우기 + `EPGA_Interact`(픽업 경로)가 `AddItem`을 실제로 부르게 배선. 지금은 F로 주워도 인벤토리에 안 들어간다(호출자 0곳, 직접 확인) | Step 02 이월 2건(02-3 사거리, 02-4 동시 F)도 여기서. 완료 조건 22 |

**요약: 03-A는 "이미 쓰인 코드를 2026-10-07 설계에 맞춰 고치고(버그 2개 포함) 처음으로 PIE에 태운다"이다.**
03-B(줍기 연동)는 여전히 처음부터다 — 스텁 5개와 pickup 배선이 남아 있다.

---

## 현재 코드 상태 (2026-09-30 대조)

### 디버그 커맨드 — `EPInv*` 치트

`EPInventoryCheats.cpp/h`가 `UCheatManagerExtension`으로 구현돼 있다(구현서 03-9). 구현서 앞쪽 완료 조건 문구와 옛 검수 이력의 `EP.Inv.*`는 아래 이름으로 읽는다.

| 옛 이름(`FAutoConsoleCommand`) | 지금 이름 | 시그니처 · 실행 위치 |
|---|---|---|
| `EP.Inv.Add` | `EPInvAdd` | `(ItemId, Container=-1)` — `BlueprintAuthorityOnly`라 서버로 라우팅, 친 창의 플레이어에게 |
| `EP.Inv.Move` | `EPInvMove` | `(EntryId, NewParent, SlotId)` — `"-"`가 `NAME_None`. 서버 |
| `EP.Inv.Reorder` | `EPInvReorder` | `(EntryId, PrevEntryId)` — 서버 |
| `EP.Inv.Drop` | `EPInvDrop` | `(EntryId)` — 서버. `Server_DropItem` 본체가 스텁이라 03-B 전엔 무동작 |
| `EP.Inv.Dump` | `EPInvDump` | 인자 없음. **로컬**(그 창의 복제본). 본체+컨테이너별 칸 사용량 |
| (`DumpAll`) | `EPInvDumpAll` | 인자 없음. **로컬**. 월드의 모든 `AEPCharacter` 엔트리 수 — 가시성(소유자 그룹) 검증용 |
| — | **`EPInvDumpServer`** (2026-10-07 신설, **아직 코드 없음**) | 인자 없음. **서버**(`BlueprintAuthorityOnly`). `Dump`와 같은 표 ＋ `Uid` 열 — `ItemUid`는 복제되지 않는다(D-098) |

**완료 조건 15·16·17(커맨드 필요)은 "커맨드가 없어서 검증 불가"가 아니라 "커맨드는 있고 PIE에서
안 돌려봤다"이다.**

### ★ 데이터 선결 — `BodySlots`가 비어 있다 (2026-10-04 확인)

`UEPLootDeveloperSettings::BodySlots`(`EPLootDeveloperSettings.h:37`)는 C++ 기본값이 없고,
`DefaultGame.ini`의 `[/Script/EmploymentProj.EPLootDeveloperSettings]` 절에도 항목이 없다 — **빈 배열이다.**
`CanPlaceInSlot`이 `BodySlots.Contains(SlotId)`로 거절하므로 **슬롯 이동(`EPInvMove … -1 Back`,
`Hotbar1` 등)이 전부 거절된다.** 완료 조건 7 후반·17은 이걸 채우기 전엔 PIE로 못 본다.

- 채울 것: `BodySlots` 12개(`Hotbar1`~`Hotbar4` + 착용 8, 이름은 `DOCS/Mine/EquipmentSlots.md`),
  `DT_Items` 배낭·무기 행의 `SlotPriority`(`["Back"]`, 무기는 핫바)
- 자리: Project Settings(→ `DefaultGame.ini`). 코드 변경 아님

### 함수별 상태 (`.cpp` 기준, 2026-09-30)

**진짜 구현됨 (스텁 아님):**
`AddItem` · `FindEntry` · `FindFungibleEntryId` · `GetUsedSlots` · `GetCapacity` · `CanFit` ·
`CanPlaceInSlot` · `GetEntryInSlot` · `GetEquippedEntryId` · `GetSortedContents` ·
`SetEntryCharges` · `AddEntryCharges` · `MoveEntry`(검사 0~6 전부) · `RemoveEntry` /
`RemoveEntryInternal` / `RemoveChildrenRecursive` / `RemoveSelf` · `ContainsEntry` ·
`InsertEntry`(키 발급이 `AddDefaulted` **전** — 함정 4x 반영 확인) · `AssignSortKey` ·
`RenormalizeSortKeys` · `KeySpace_NextAtEnd`(오버플로 가드 포함) · `KeySpace_Min` ·
`KeySpace_NextAbove`(반환형 `bool` — 헤더 코멘트가 요구하던 대로) · `ReorderEntry` /
`ReorderEntryInternal`(함정 4t·4u 가드 다 있다) · `GetLifetimeReplicatedProps`

**여전히 스텁 (03-B 몫, 예정대로):**
`AddSubtree` → `return 0` · `TryAutoEquip` → `return 0` · `GetInsertionOrder` → `return {}` ·
`Server_DropItem_Implementation` → `return`(본체 없음) · `SpawnPickupInFront` → `return nullptr`

**★ 진짜 버그 — `KeyOf`가 `SortKey`가 아니라 `EntryId`를 돌려준다**

```cpp
// EPInventoryComponent.cpp:465-474 (현재)
int32 UEPInventoryComponent::KeyOf(int32 EntryId, int32& OutKey) const
{
    for (const FEPInventoryEntry& E : Entries.Items)
    {
        if (E.EntryId != EntryId) continue;
        OutKey = E.EntryId;   // ← 버그. E.SortKey여야 한다
        return true;
    }
    return false;
}
```

`ReorderEntryInternal`이 `PrevEntryId != INDEX_NONE`일 때(맨 앞이 아닌 곳에 끼워 넣을 때)
`KeyOf(PrevEntryId, PrevKey)`로 기준 키를 구한다(`:495`). **`EntryId`가 `SortKey`로 둔갑해
들어가면 그 뒤 `KeySpace_NextAbove`·중간값 계산이 전부 엉뚱한 값 위에서 돈다.** 맨 앞
재배치(완료 조건 17 일부)는 이 경로를 안 타서 정상일 수 있지만, **일반적인 "이 항목 뒤에
놓기"(조건 14)는 현재 코드로는 못 믿는다.**

부수적으로 반환형도 헤더 코멘트(`// 반환 : 찾았는가 (※ 문서상 bool...)`)와 다르게 **`int32`로
선언돼 있다** — `KeySpace_NextAbove`는 이미 `bool`로 고쳐졌는데 `KeyOf`만 안 따라갔다. 같이
고친다.

**★ 잠재 버그 — `AddItem`의 널 역참조**

```cpp
// EPInventoryComponent.cpp:45-64
const FEPItemData* Data = Defs() ? Defs()->FindData(ItemId) : nullptr;
if (Data->bFungible)   // ← Data가 nullptr이면 즉시 크래시
```

DT에 없는 `ItemId`를 넘기면(오타, 마이그레이션 누락) 널 체크 없이 바로 역참조한다. 아래
`CanFit`은 같은 상황에서 `if (!Data) return false;`로 방어하는데 `AddItem`만 안 한다 — 같은
파일 안에서 방어 패턴이 갈린 사례.

**관찰 — `IsFungible()`이 죽은 스텁이다**

```cpp
bool UEPInventoryComponent::IsFungible(FName ItemId) const { return true; }
```

`AddItem`은 이 함수를 안 부르고 `Data->bFungible`을 직접 읽는다(위 코드). `IsFungible()`은
선언돼 있고 항상 `true`를 반환하는데 **호출자가 0곳**이라 지금은 무해하지만, 나중에 누가
"있으니까" 불러 쓰면 항상 `true`를 받는다. 지우거나 `Data->bFungible`을 돌려주게 고친다.

**관찰 — `CanPlaceInSlot`이 부착 슬롯(`Parent != INDEX_NONE`)을 전부 거절한다 → 의도된 동작이다.** 부착 갈래는 §7-3에서 연다(구현서 함정 `3j`). 헤더 코멘트(`:110`)의 *"부착 슬롯이면 그 무기의 EntryId"* 는 그때의 계약을 미리 적어 둔 것이다.

**`EmploymentProj.Build.cs:11`에 `NetCore` 추가 완료** — 없으면 `LNK2019: Z_Construct_UScriptStruct_FFastArraySerializerItem`.

---

## 진행 구간

완료 조건 22개(1~20 ＋ 2026-10-07의 21·22)를 둘로 나눈다. 구현서도 A · B 두 파일이다.

| | 범위 | 완료 조건 | 상태 |
|---|---|---|---|
| **03-A 코어** | 03-1 · 03-2 · 03-3 · **03-7** · 03-9<br>**＋ `GetEntryInSlot(Parent, SlotId)`**<br>**＋ `MoveEntry` (정합·사이클 검사 포함)**<br>**＋ `GetCapacity` 통째로** (컨테이너 갈래 포함 — `EP.Inv.Move`가 부른다)<br>**＋ `ActiveHotbarIndex` 필드**<br>**＋ `FEPItemData::SlotPriority`**<br>**＋ `UEPLootDeveloperSettings::BodySlots`**<br>**＋ `SortKey` 일습**<br>**＋ `EP.Inv.Move` · `EP.Inv.Add [Container]` (13차)**<br>**＋ `CanPlaceInSlot` (13차 답변)** | 2~6, 4 전반, **7의 후반**, 13~19, 21 | 🟨 코드 대부분 있음 · PIE 미검증 · 2026-10-07 변경분 미반영 |
| **03-B 줍기·버리기** | 03-4 · 03-5 · 03-6<br>**＋ `GetInsertionOrder()`**<br>**＋ `AddSubtree` · `TryAutoEquip` · `StartingEquipment`** (13차) | 1, 4 후반, 7의 전반, 8~12, 20, 22 + 이월 2건 | ⬜ 미착수 (스텁 5개) |

---

## 완료 조건 대조

> 03-A 항목의 ⬜는 *"코드가 없다"* 가 아니라 **"코드는 있는데 PIE로 안 돌려봤다"** 다. 번호는 구현서(`Implementation/05_Loot_03_Inventory_A_Core.md` · `_B_PickupDrop.md`)와 같다.

| # | 완료 조건 | 담당 | 상태 |
|---|---|---|---|
| 1 | 주운 아이템이 인벤토리에 들어가고 클라이언트에 복제된다 | 03-B | ⬜ |
| 2 | 붕대 3개를 넣으면 엔트리가 3개다 (스택 없음) | 03-A | ⬜ |
| 3 | 현금뭉치 둘 → 엔트리 1개, `Charges` 합산 (`bFungible`) | 03-A | ⬜ |
| 4 | 칸이 모자라면 **아무것도 안 들어간다**(전반) / 픽업이 그대로 남는다(후반, `bClaimed` 되돌림) | **전반 03-A / 후반 03-B** | ⬜ |
| 5 | 가방이 꽉 차도 현금뭉치·탄약상자는 들어간다 | 03-A | ⬜ |
| 6 | 무기(`SlotSize=5`) → `UsedSlots` +5 (개수가 아니라 **칸 수**) | 03-A | ⬜ |
| 7 | 배낭을 주우면 자동 착용(전반) ＋ **별도 풀**이 열린다(후반) | **후반 03-A / 전반 03-B** | ⬜ 후반은 **`BodySlots` 선결** |
| 8 | 배낭을 버리면 안의 아이템이 같이 나가고 **고아 엔트리가 없다** | 03-B | ⬜ |
| 9 | 배낭을 버렸다 되주우면 내용물이 그대로 돌아온다 (`Parent` 유지) | 03-B | ⬜ |
| 10 | 무기 `Charges`가 버렸다 주워도 보존된다 (12/30은 Step 05 이후) | 03-B | ⬜ |
| 11 | 버린 직후 0.5초 동안 회색 프롬프트 + 서버 거부 | 03-B | ⬜ |
| 12 | 스폰 직후 **상의·하의를 입고 시작한다** (`StartingEquipment`) | 03-B | ⬜ |
| 13 | 다른 클라이언트에 내 인벤토리가 복제되지 않는다 (소유자 그룹 — D-101) | 03-A | ⬜ **가시성 코드 변경 선결** (남은 작업 1) |
| 14 | 넣고 빼도 기존 `EntryId`가 **재번호되지 않는다** | 03-A ("빼기"는 03-B 이후 확인) | ⬜ |
| 15 | **★ 다른 컨테이너로 옮기면 목적지 맨 뒤** — 옛 키를 들고 가지 않는다 (함정 4m) | 03-A | ⬜ |
| 16 | **★ `EPInvReorder`로 자리를 바꾸면 `Dump` 순서가 바뀌고 클라·서버가 같다** (11차) | 03-A | ⬜ **`KeyOf` 버그 선결** |
| 17 | **★ 핫바에 꽂았다 빼면 원래 자리로 돌아온다** — 동률이 안 난다 (함정 4q) | 03-A | ⬜ **`BodySlots` 선결** |
| 18 | **★ 이분 고갈 → 재정규화 / 맨 앞 20회 → `ensure` 없이 순서가 바뀐다** (함정 4r·4t) | 03-A | ⬜ |
| 19 | **★ 원래 자리에 도로 놓으면 `SortKey`가 안 바뀐다** (함정 4u) | 03-A | ⬜ |
| 20 | **★ 배낭에 4개 이상 넣고 순서를 섞은 뒤 버렸다 주우면 그 순서 그대로** (11차) | 03-B | ⬜ |
| 21 | **정체** — `EPInvDumpServer`의 `Uid`가 엔트리마다 다르고, `EPInvMove`로 옮겨도 그대로 (2026-10-07, D-098) | 03-A | ⬜ **코드 없음** (남은 작업 1) |
| 22 | **정체** — 배낭을 버렸다 주우면 `EntryId`는 새 번호, `Uid`는 그대로 (2026-10-07, D-098) | 03-B | ⬜ |

**★ Step 02에서 이월된 검증 2건** (번호를 매기지 않는다 — 위 22개(1~20 ＋ 21·22)가 Step 03의 완료 조건이고 이 둘은 Step 02의 미필이다)

| | 이월 항목 | 상태 |
|---|---|---|
| 02-3 | **사거리 밖 거부** — `EPGA_Interact.cpp:69-75`가 한 번도 돈 적이 없다 | ⬜ 03-B |
| 02-4 | **동시 F 경쟁** — `bClaimed` 선점. 거부당한 쪽 인벤토리가 **비어 있어야** 한다 | ⬜ 03-B |

> **둘 다 03-B에서 본다.** 그 전에는 버린 픽업을 만드는 경로 자체가 없다.

---

> **옛 내용이 옮겨 간 곳 (2026-10-08 정리):** 9~14차 "추가된 코드 항목" 표와 구간 이동 경위 → `05_Loot_DECISIONS.md` "차수별 경위" · 구현서 A/B 변경 이력. `Server_EquipBackpack` 삭제 근거 셋 → `DOCS/Mine/EquipmentSlots.md` §15. 13차 답변의 *"32,764회 정정"* 과 *"권했으나 안 한 것"* → `Review/05_Loot_REVIEW_Inventory_Summary.md` 13차 답변 절. "설계 문답" 셋 → `05_Loot_DECISIONS.md` D-111 · D-112.
