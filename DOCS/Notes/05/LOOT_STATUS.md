# Loot 단계 전체 진행 상황

> 세션 시작 시 이 파일을 먼저 읽을 것.
> 현재 Step 확인 후 해당 Step의 STATUS 파일을 추가로 읽는다.
> 마스터 기획: `05_Loot_DOCS.md`

---

## 진행 상황

- [x] 05_Loot_00 ItemCore (아이템 계층 정비 + `FEPItemState` + Definition 서브시스템) — **`EP.Item.Dump` → `9, 9`.** 상세: `05_Loot_00_ItemCore_STATUS.md`
- [~] 05_Loot_01 Spawner (루트테이블 + 스포너 + 픽업) — **구현 완료 / 검증 1건 미완.** 상세: `05_Loot_01_Spawner_STATUS.md`
  - PIE 확인됨: 서버·클라 픽업 일치, `Respawn`, 플레이스홀더, 콜리전 무시
  - ❌ **`EP.Loot.RollTable`에 출력 블록이 없어**(`EPLootDebugCommands.cpp:69`) 등급 비율 50/30/15/5를 아직 검증하지 못했다
- [x] 05_Loot_02 Interaction (`IEPInteractable` + **`UEPGA_Interact`** + 서버 검증 + HUD 프롬프트) — **구현 완료, PIE 동작 확인. 태그 `step5-2`.** 상세: `05_Loot_02_Interaction_STATUS.md`
  - F로 획득·프롬프트·리슨서버 호스트 전부 정상. 7차 검수대로 **직접 서버 RPC 0개** 유지
  - ⚠️ 완료 조건 3(사거리 밖 거부)·4(동시 F 경쟁)는 **코드에만 있고 실행된 적이 없다** — 정상 플레이로 재현할 수단이 없다. Step 03의 `DropCooldown`이 같은 경로를 쓰므로 그때 함께 검증
  - ⚠️ 완료 조건 7(`UnPossessed` 후 틱 종료)은 **리스폰 경로가 없어 검증 불가.** 문서가 이미 예고한 것(`05_Loot_02_Interaction.md:24`) — "테스트가 통과했으니 `NotifyControllerChanged` 훅이 불필요하다"로 읽지 말 것
- [ ] 05_Loot_03 Inventory ← **현재. 가장 큰 단계.** 완료 조건 **19개**로 다른 단계 두 개 분량이라 **둘로 나눠**(03-A 코어 / 03-B 줍기·버리기) 진행한다. **13차 답변으로 가운데 구간(배낭)이 없어졌고, 14차에 `Server_EquipBackpack`이 함수째 삭제됐다** — 04-A에도 호출자가 0개였다(`EP.Inv.Equip`은 커맨드라 내부 함수를 직접 부른다). **골격만(로직 0줄).** `Public/Inventory/` · `Private/Inventory/` 생성됨, `Build.cs`에 `NetCore` 추가 완료. **9차 검수(2026-08-22)로 03-A 범위가 늘었다** — `MoveEntry` / `GetEntryInSlot` / `SlotPriority` / `BodySlots`. **10차(2026-08-23)에 `MoveEntry` 검사 0(제자리 거절)과 `GetInsertionOrder()`의 본체 선두가 추가됐다.** 상세: `05_Loot_03_Inventory_STATUS.md`
  - **8차 검수 요청 작성됨** (`Review/05_Loot_REVIEW8_Request.md`) — 최대 주제는 **`Server_DropItem` 직접 RPC 대 `UEPGA_DropItem`**. 7차가 세운 "게임플레이 입력의 진입점은 어빌리티 하나다"를 Step 03 문서가 한 단계 만에 깬다
  - [ ] **03-A 코어** (03-1·2·3·**7**·9) — 완료 조건 **2~6**. 칸 합산 / `bFungible` / `COND_OwnerOnly`. `RemoveEntry`·`AddSubtree` 없이 단독 실행됨
  - [ ] **03-B 배낭** (03-6 + `GetCapacity(컨테이너)`) — 완료 조건 **7**. 자동 착용 + 독립 풀. 아직 못 버린다
  - [ ] **03-B 줍기·버리기** (03-4·5·6) — 완료 조건 **1, 7의 전반, 8~13, 16** + 이월 2건. **＋ `AddSubtree`·`TryAutoEquip`·`StartingEquipment`**(13차 답변). `RemoveEntry` / `AddSubtree` / 캐스케이드 / `Server_DropItem`. **함정표 ★★ 4건 중 3건이 여기**
  > **★ 03-7(알림)은 03-A다** (8차 검수). `FScopedInventoryNotify`를 03-3의 `AddItem`·`SetEntryCharges`가 쓰므로, 정의가 03-C에 있으면 **03-A가 컴파일되지 않는다.** 근거: `05_Loot_03_Inventory.md:51-56`
- [ ] 05_Loot_04 InventoryUI (**정사각형 격자 + 분절 게이지 + 드래그**) — 10차 검수로 **둘로 나눔**
  - [ ] **04-A 표시** (04-0·1·2·3·4·6) — 완료 조건 **1~6, 13, 14**(8개). `EP.Inv.Add`/**`EP.Inv.Move`** 커맨드로 검증한다(14차에 `EP.Inv.Equip` 폐기). **칸은 `UUserWidget`으로 만든다** — 04-B의 드롭이 칸의 `NativeOnDrop`에 걸린다
  - [ ] **04-B 드래그** (04-5·7·8) — 완료 조건 **7~12**(6개). `Server_MoveEntry`·`Server_SwapEntries`가 여기서 열린다
- [ ] 05_Loot_05 Equipment (무기 장착 흐름 이관 + 탄약 소유권 정리)
  > **★ 이 단계에 얹을 이월 항목 3건** — `DOCS/BACKLOG.md` **B-1**(무기 FX를 `WeaponDefinition`으로) / **B-3**(`case Hitscan: default:`) / **B-5**(`GetEquippedWeapon()` 대신 `GetEquippedEntryId()`를 새 코드의 진입점으로). 셋 다 여기서 하면 거의 공짜고, **B-5를 안 지키면 나중이 비싸진다**

추후 (기획 확정, 구현 미정) — `05_Loot_DOCS.md` §7
- [ ] 컨테이너 + 검색 시간 (GAS `CastTime` 구조 재사용)
- [ ] 자판기 (상자 배출 방식)
- [ ] **무기 부착물 (배그식 — 깊이 1).** 남은 것은 슬롯 스키마·스탯 합산·메시 부착·부착 RPC뿐

> **★ 배낭이 Step 03에 들어오면서 부착물의 구조적 비용이 대부분 선불된다** — `ParentEntryId`/`SlotId`, 서브트리 픽업, 자식 캐스케이드, `AddSubtree`가 전부 거기서 만들어진다.
>
> **Step 00~05에서 지켜야 할 유일한 것: `EntryId`의 안정성.**
> 서버 발급 / 단조 증가 / **재번호 없음**. 배열 인덱스를 쓰거나 번호를 재사용하면 부모 참조가 성립하지 않아 배낭도 부착물도 원천 봉쇄된다.

---

## 확정된 설계 결정 (상세는 `05_Loot_DOCS.md`)

| 항목 | 결정 |
|---|---|
| **개체 상태의 형태** | **`FEPItemState` (USTRUCT, 값 타입).** `UEPItemInstance` / `UEPWeaponInstance` / `UEPItemInstanceSubsystem` / `int32` 핸들 **전부 삭제** |
| **스택** | **없다.** 붕대 3개 = 엔트리 3개. 탄약은 탄약상자의 `Charges`, 돈은 현금뭉치의 `Charges` |
| **균질 아이템 합치기** | **한다.** `FEPItemData::bFungible`(현금뭉치·탄약상자)이면 `Charges` 합산. 칸이 안 늘어 스택의 부담이 하나도 안 따라온다 |
| **인벤토리 용량** | **컨테이너별 칸 수 합산.** 본체 **10칸**(GAME.md) + 배낭은 **별도 풀**. 통합하지 않는다 |
| **중첩 컨테이너** | **`ParentEntryId`.** 배낭·부착물·상자가 전부 같은 표현. 자기 타입 재귀는 `Class.cpp:974` Fatal이라 중첩 struct를 쓰지 않는다 |
| **불변식 강제** | **`RemoveEntry(Id, &Out)`이 제거된 서브트리를 반환한다.** 스냅샷을 얻는 유일한 방법이 제거하는 것이라 **순서를 뒤집는 게 문법적으로 불가능**하다. 장착 검사·write-back·자식 캐스케이드가 그 안에서 노드마다 돈다 |
| **`RemoveEntry` 4단계 순서** | ① write-back → ② 스냅샷(루트는 `Parent=INDEX_NONE`으로 정규화) → ③ **자기 제거** → ④ 캐스케이드. 각 단계가 하나씩 막는다(잔탄 소실 / `AddSubtree` 계약 / 무한 재귀) |
| **`RemoveEntry` ↔ `AddSubtree` 계약** | 반환 배열은 **전위 순회, `In[0]`이 루트, 루트의 `Parent`는 `INDEX_NONE`.** 자식은 원본 `Parent` 보존. **양쪽이 다 컴파일되므로 이 계약을 안 적으면 아무도 안 걸린다** |
| **`Charges` 쓰기** | **`SetEntryCharges`가 유일한 쓰기 지점**(클램프·`MarkItemDirty`·알림). `AddEntryCharges`는 위임. write-back은 대입이라 `Set`, 합치기·소비는 `Add` |
| **배낭** | 줍고 **빈 슬롯이면 자동 착용.** 별도 필드 없음 — **`SlotId == "Back"` 이 곧 진실**(9차 확정). 교체는 벗고 다시 줍기, **벗기 = 바닥 드랍**(12차). **착용은 칸 검사를 지나지 않는다**(13차) — `AddSubtree(Parent, SlotId, In)`이 `SlotId`가 있으면 `CanFit`을 건너뛴다 |
| **인벤토리 용량** | **본체 0칸.** 수납은 **착용 컨테이너에서만** 나온다 (13차, 2026-08-25). 상의 `11/10` · 하의 `6/5` · 배낭A `15/12` · 배낭B `10/8`. `MaxSlots=10`은 **테스트값**. 스폰 시 `StartingEquipment`로 상의·하의를 입힌다. 규칙은 **`Capacity < SlotSize`**(등호 없음 — 넣기 판정의 `≤`와 다른 식) |
| **서브트리 삽입** | `AddSubtree()` + **`EntryId` 재매핑.** 없으면 배낭을 버렸다 주울 때 내용물이 증발한다. **칸 검사는 루트만** — 자식은 서브트리 내부로 들어가므로 늘리면 되줍기가 깨진다 |
| **갱신 알림** | `FEPInventoryList::PostReplicatedReceive` **하나** (수신 1회당 1회). 항목별 콜백 3종은 **선언조차 하지 않는다** — 이름 가림으로 링크 에러가 난다(`FastArraySerializer.h:341,349,356` 기반 no-op + `:1139,1163,1174` 무조건 호출). 서버는 스코프 가드 |
| **★ 클라 → 서버 요청 경로 (8차)** | **둘뿐이다.** ① **월드 상호작용 / 시간·비용·애님이 붙는 행동 → 어빌리티** ② **서버가 이미 소유한 상태의 변경 요청 → `UEPInventoryComponent`의 서버 RPC.** 7차의 *"입력 진입점은 어빌리티 하나다"* 를 이 문장이 대체한다 |
| **드랍·장착 RPC** | **`Server_DropItem`(03-5) / `Server_MoveEntry`·`Server_SwapEntries`·`Server_ReorderEntry`(04-B) 직접 RPC.** ~~`Server_EquipBackpack`~~ 은 **14차에 삭제** — 04-B가 넓은 `Server_MoveEntry`를 여는 이상 좁은 래퍼가 표면을 안 줄이고, 호출자도 0개였다. `FGameplayEventData`에 `int32`가 없고(`GameplayAbilityTypes.h:246-284`) `EventMagnitude`를 식별자로 쓴 선례가 엔진·Lyra 통틀어 0건. **Lyra의 유일한 손수 만든 서버 RPC가 정확히 이것이다**(`LyraQuickBarComponent.h:30`, `SetActiveSlotIndex(int32)`). 죽음·시전 게이트는 `CanMutateInventory()` 한 곳 |
| **드랍 순서** | **스폰 → `RemoveEntry` → `InitPickup`.** 제거를 먼저 하면 스폰 실패 시 서브트리가 증발한다 |
| **`FindFungibleEntryId`** | **`(int32 Container, FName ItemId)`** — 컨테이너 인자를 빼면 배낭 속 현금이 본체 현금과 합쳐지고, 무증상이다가 배낭을 벗을 때 딸려 나간다 |
| **삽입 지점** | private **`InsertEntry(Parent, ItemId, State, SlotId)` 하나.** `AddItem`·`AddSubtree`가 공유한다. `AddSubtree`가 자식에 `AddItem`을 부르면 `bFungible` 합치기와 `CanFit`이 동시에 깨진다 |
| **`bIsRoot`** | **private `RemoveEntryInternal`에만.** public에 두면 루트 정규화를 건너뛸 문법이 생겨 계약이 깨진다 |
| **`FEPInventoryEntry` 위치** | **`Public/Inventory/EPInventoryTypes.h` (신규).** `EPTypes.h`는 거의 모든 파일이 include하고, 컴포넌트 헤더에 두면 `Loot/`가 컴포넌트 전체에 의존한다. **`Build.cs`에 `NetCore` 추가 필요** — `Engine.Build.cs:86`이 Public 의존으로 갖고 있지만 모듈러 빌드에서 UBT는 **public include 경로만 전파하고 import 라이브러리는 직접 의존한 모듈 것만** 링크 줄에 넣는다. 컴파일은 통과하고 **링크에서 `LNK2019`** 가 난다(실증됨) |
| **`DropCooldown`** | **`AEPPickup`의 복제되는 `float DropCooldownEndTime`** + `GetServerWorldTimeSeconds()`. 타이머 핸들이면 서버 전용 상태라 클라 프롬프트가 회색이 안 된다. GE 쿨다운도 아니다 — **주체가 픽업이지 플레이어가 아니다** |
| `FEPItemData::MaxStack` | **읽지 않는다.** 스택 부활용 예약 필드로 남김 (사용자 지시) |
| `FEPItemState::Durability` | **유지.** 무기가 사용한다. GAME.md에 내구도를 정식 편입했다. 열쇠·붕대의 "사용 횟수"는 `Durability`가 아니라 `Charges`다 |
| 인벤토리 부착 | **Character** — 사망 시 소실이 규칙과 일치 |
| 인벤토리 복제 | **FFastArraySerializer + POD**, `FEPItemState`를 엔트리에 **내장** (내부 struct 델타 기본 활성) |
| 픽업 액터 (복제) | **`ItemId`만.** `FEPItemState`는 서버 전용 — 바닥 무기 잔탄 노출은 정보 은폐 위반 |
| 픽업 액터 (상태) | 스포너가 뿌린 것도 버린 것도 **언제나 유효한 `State` 보유.** 획득은 언제나 값 대입 |
| 루트 테이블 | **가중치 + 중첩(SubTable)** — GAME.md 등급 확률 50/30/15/5 보존. **수량 필드 없음** |
| 부분 획득 | **없다** — 전부 아니면 전무 (픽업 하나 = 아이템 하나) |
| 스폰 시점 | GameMode의 `MatchState`가 지시 (스포너 `BeginPlay` 아님) |
| 아이템 버리기 | **포함**(Step 03). `Pickup->State = Entry.State` 값 복사 |
| 무기 장착 이관 | **포함**(Step 05). 탄약은 `Entry.State.Charges`가 진실, GAS `Ammo`는 뷰 |
| 장비 슬롯 | **별도 배열도 필드도 없다 — `FEPInventoryEntry::SlotId`가 유일한 진실**(9차 확정). 슬롯 12개(핫바 1~4 + 착용 8) + 부착 4. 엔진 자신이 같은 형태다 — `USceneComponent`가 `AttachParent`＋`AttachSocketName`을 **자식**에 두고 자식 목록은 `Transient` 파생 색인(`SceneComponent.h:108-119`) |
| 장비 슬롯 — 남는 상태 | **`ActiveHotbarIndex`(int32) 하나만.** "1번과 2번 중 어느 쪽을 들었나"는 `SlotId`로 표현되지 않는다. **가리키는 것이 엔트리가 아니라 슬롯이라 죽은 번호가 생길 문법이 없다** |
| 슬롯 조회 | **`GetEntryInSlot(int32 Parent, FName SlotId)`** — 부모 인자는 선택이 아니다. `Optic`은 무기마다 하나씩 있어 인자를 빼면 **두 번째 무기에 부착물을 못 단다**(4f와 같은 모양) |
| 엔트리 이동 | **`MoveEntry(EntryId, NewParent, NewSlotId)`가 `ParentEntryId`＋`SlotId`를 고치는 유일한 지점.** 장착·해제·드래그·부착이 전부 이것 하나. 검사 7개 중 **정합(`BodySlots`)·사이클·제자리 거절**이 필수 |
| 슬롯 종류 판정 | **`UEPLootDeveloperSettings::BodySlots`** — 몸에 붙는 슬롯 12개 목록. 없으면 *"가방 안에 든 상의를 입는다"* 가 표현된다. 소비자 둘(검증 / Step 04 UI) |
| 슬롯 자격·우선순위 | **`FEPItemData::SlotPriority`(`TArray<FName>`)** — **배열 순서가 곧 자동 배정 우선순위.** 태그 컨테이너는 순서를 표현하지 못하고 에디터 UI가 체크박스 트리다 |
| **인벤토리 화면** | **정사각형 격자.** 칸 하나 = 아이템 하나, 부피(`SlotSize`)는 **배지 + 분절 게이지**로 (2026-08-23). **2D 배치로 가지 않는다** — 데이터 모델에 2D 크기가 없고 넣으면 Step 03 재작성이다 |
| 격자 칸 개수 | **가변이다. 고정 격자를 그리면 안 된다** — 용량 20에 아이템이 1~20개 들어갈 수 있어 그림과 게이지가 어긋난다. **격자는 나열, 진실은 게이지** |
| **빈 칸** | **N개 그리지 않는다**(10차). 개수는 레이아웃 부산물인데 플레이어가 정보로 읽어 **고정 격자 문제가 형태만 바꿔 남는다.** 아이템 칸 뒤에 **`＋ 남은 용량 N` 블록 하나**. 드롭 대상이자 드래그 피드백 대상이 그 하나다 |
| **넘침 피드백** | **색이 아니라 숫자.** `7 → 11 / 20`(초록) / `18 → 22 / 20`(빨강). 색은 가부까지만 답하고 *"이만큼 모자라다"* 는 못 말한다 |
| **칸 위젯 생성** | **`UDynamicEntryBox(EDynamicBoxType::Wrap)`** — `FUserWidgetPool` 내장(`DynamicEntryBoxBase.h:179`), `Reset` 기본값이 파괴가 아니라 **회수**(`DynamicEntryBox.h:51`). UMG 코어라 CommonUI 의존이 안 는다. `UListView` 배제(`UObject*` 고정)는 유효하지만 **그때 배제된 것은 아이템 타입이지 풀링이 아니다** |
| **드롭 라우팅** | **칸이 `OnDrop`을 받고 버블링으로 패널이 판정한다.** `HitTestCell` 지오메트리 산술은 **만들지 않는다** — Slate가 이미 `FBubblePolicy`로 깊은 위젯부터 라우팅한다(`SlateApplication.cpp:5523`, `:382-406`). 검증은 `HandleDrop` 한 곳 |
| **드롭 지점 타입** | **`FEPCellHit { DisplayIndex, EntryId }`** — `int32` 하나로 두면 같은 변수가 **표시 인덱스**와 **`EntryId`** 를 오가고 **둘 다 컴파일된다** |
| **같은 컨테이너 드롭** | **조건 없이 로컬이다.** 여백에 놓았다고 `Server_MoveEntry`로 흘리면 `CanFit`이 자기 크기를 두 번 세어 **가방이 찰수록 "맨 뒤로 보내기"가 거절된다** |
| 구획 순서 | **`UEPLootDeveloperSettings::ContainerOrder`** — `["Coat","Torso","Legs","Back","Wrist"]`. **`GetInsertionOrder()`는 이 배열을 그대로 반환하지 않는다** — 반환형이 `TArray<int32>`(컨테이너 EntryId)이고 **본체(`INDEX_NONE`)를 맨 앞에 직접 붙인다.** `ContainerOrder`에는 본체가 없다(슬롯이 아니다). 공유되는 것은 *순서*이지 *목록*이 아니다 |
| **아이템 순서** | **서버가 든다 — `FEPInventoryEntry::SortKey`(`int32`)**(11차, 10차 뒤집음). *"서버 로직 중 순서를 보는 곳이 0곳"* 은 지금도 사실이지만 그건 **"볼 필요가 없다"** 이지 **"두면 안 된다"** 가 아니었다. 뒤집은 이유는 **지속** — 아래 |
| **왜 클라 로컬이 안 되나** | `NextEntryId`는 컴포넌트 필드고 초기값 1이라 **매치마다 재발급**되는데 `ULocalPlayerSaveGame`은 디스크에 남는다 → 지난 매치의 `Order[7]`이 이번 매치의 7번(다른 아이템)에 적용된다. 세션 도장으로 막으면 **지속이 한 번도 발휘되지 않아 인메모리와 같아진다.** 반면 `SortKey`는 로드맵 5단계가 엔트리 배열을 저장하므로 **저장 코드 0줄**로 따라간다 |
| **★ 일반 규칙 (11차)** | **저장되는 색인의 키는 저장소만큼 오래 살아야 한다.** 짧은 키로 긴 저장소를 색인하면 실패가 *"못 읽는다"* 가 아니라 **"엉뚱한 것을 읽는다"** 로 나타난다 — 번호가 재사용되므로 죽은 참조가 **살아 있는 것처럼 보인다.** 10차가 `ULocalPlayerSaveGame`을 승인하며 **저장소의 수명만 보고 키의 수명을 안 봤다** |
| **키 공간 ≠ 표시 목록** | `KeySpace_*` 셋과 `RenormalizeSortKeys`는 **부모가 같은 것 전부**를 본다(슬롯 포함). `GetSortedContents`는 **그릴 것을 고르는** 함수라 슬롯을 거른다. 혼동하면 *"무기를 꽂았다 빼는"* 정상 조작에서 **동률이 난다**(함정 4q). 부수 효과로 **꽂았다 빼면 원래 자리로 돌아온다** |
| **장착과 순서 자리** | **A 확정 (12차).** *"장착은 아이템을 가방에서 꺼내는 것이 아니다. 칸만 돌려주고 순서 자리는 남긴다."* 뺐다 꽂으면 원래 자리로 돌아오고 **그 사이에 넣은 아이템은 그 앞에 선다.** 대안 B(자리를 포기)는 기각 — 키를 맨 뒤로 밀어도 **표시 목록 기준 발급이 그 구간을 침범해 같은 동률이 재현**되고, 키 공간에서 빼면 11차가 없앤 `NewSlotId` 예외가 돌아온다 |
| **`Prev` vs 틈** | **스코프가 갈린다.** `Prev`는 *"무엇 뒤에 놓으려 했나"*(의도) → **표시 목록**. 틈은 *"어떤 숫자를 줘야 하나"*(표현) → **부모 전체**. 통일하면 한쪽이 깨진다 — 표시 목록으로 통일하면 4q·4s, 부모 전체로 통일하면 **사용자가 가리킬 수 없는 `Prev`** 를 API가 받는다. 헬퍼에 `KeySpace_` 접두어를 붙여 이름으로 막는다 |
| **재정규화 트리거** | **세 분기 공통이되 판정은 둘로 나눈다** — 경계(`bOutOfRange`, 세 분기)와 고갈(`bNoGap`, **`Prev`가 있을 때만**). 한 식에 묶으면 **맨 앞 분기가 항상 참이 되어 무한 재귀**한다(함정 4t). 종료는 `ReorderEntryInternal(bRetry)` ＋ `ensure`로 **문법이 보장**한다. 경계 판정은 맨 앞/맨 뒤의 **무한 증감**을 막는다 — `int32` 오버플로 후 상태는 재정규화가 안 걸려 **영구적**이다(함정 4r). `KeySpace_NextAtEnd`가 **유일 발급 지점**이라 발급 쪽 가드도 거기 하나 |
| **RPC 표면** | **`ReorderEntry`(내부)는 03-A, `Server_ReorderEntry`(RPC)는 04-B.** 9차가 `MoveEntry`/`Server_MoveEntry`에 적용한 규칙 그대로 — 검증 표면을 소비자보다 먼저 열지 않는다. `EP.Inv.Reorder`가 내부 함수를 직접 불러 계약을 먼저 닫는다 |
| **키 배치** | **희소 정수** — `SortKeyStep = 1 << 16`, 사이 삽입은 `(Prev+Next)/2`. **조밀(0,1,2…)을 기각한 이유는 대역폭이 아니다** — `RenormalizeSortKeys`가 **곧 조밀 재번호 루프**라 "조밀로 하면 그 코드가 사라진다"는 이득이 **존재하지 않는다.** 희소는 중점+가드 5줄을 더 쓰고 **O(N)을 O(1)로** 만든다. `double` 기각의 결정적 사유는 **고갈 판정이 부동소수 동등 비교**(`Mid == Prev`)가 되는 것 — 정답인데 영원히 의심받는다 |
| **키의 스코프** | **형제(같은 `ParentEntryId`) 안에서만.** 다른 컨테이너와 값이 겹쳐도 무관하다. `SlotId`가 있으면 슬롯이 곧 자리라 무의미 |
| **자리 바꾸기 RPC** | **`Server_ReorderEntry(EntryId, PrevEntryId)`** — **인덱스가 아니라 앞 이웃.** 인덱스는 `bFungible` 병합·자동 획득이 목록을 밀면 한 칸 어긋난다. 검증도 *"Prev가 같은 컨테이너의 수납 형제인가"* 한 줄이고 **다중 선택으로 넓힐 수 있다.** `MoveEntry`와 별도인 이유: 용량·슬롯·사이클을 안 봐서 **실패할 수 없다** |
| **키를 누가 만드나** | **서버만.** 클라는 앞 이웃(`EntryId`)만 보낸다 — 클라가 키 값을 보내면 겹치거나 범위 밖 값을 심을 수 있다 |
| **배낭 되줍기와 순서** | **`RemoveEntryInternal` ②의 `bIsRoot`가 `ParentEntryId`와 `SortKey`를 동시에 관장한다.** 루트는 둘 다 버리고(목적지 키 체계로 들어간다) **자식은 둘 다 보존**한다 — 부모가 방금 만들어진 빈 컨테이너라 충돌이 없다. **버린 배낭을 되주우면 내용물 순서가 그대로 산다** |
| **"전부 옮기기"(Shift+클릭)** | 10차가 *"클라가 `Server_MoveEntry`를 N번"* 이라는 우회로를 적어뒀는데 **11차에 필요 없어졌다** — 서버가 `GetSortedContents`를 직접 부른다. 자동 정렬도 마찬가지로 `RenormalizeSortKeys`의 비교 함수 교체다 |
| **2D 격자로 갈 때** | `SortKey` → `FIntPoint Location`인데 **호출 지점이 동일하다**(`InsertEntry`·`MoveEntry`·재배치 RPC·스냅샷·루트/자식). **필드 교체이지 구조 변경이 아니다.** 클라 로컬로 뒀다면 *"04-8을 버리고 03을 다시 연다"* 였다. 단 **`CanFit`(스칼라 합산)은 어느 설계로 가든 재작성**이다 |
| **`FindFungibleEntryId`와 순서** | 배열 순서에 의존하지만 그건 서버 `Entries.Items`의 물리적 순서지 **표시 순서가 아니다.** 무해한 이유는 *"첫 번째를 찾으니 무관"* 이 아니라 **"그 축에서 원소가 교환 가능하다"** — 현금뭉치 둘은 구분 불가다. **`bFungible`인데 구분 가능한 상태를 갖는 아이템이 생기면 깨진다** |
| 자리 바꾸기 | **`Server_ReorderEntry(EntryId, PrevEntryId)` — Step 03-A.** `MoveEntry`와 별도다: 용량·슬롯·사이클을 안 보므로 **실패할 수 없고**, 그래서 클라가 낙관적으로 먼저 그려도 롤백이 없다. `EP.Inv.Reorder`로 UI보다 먼저 닫는다 |
| 교환(swap) | **`SwapEntries(A, B)` — Step 04-B.** `MoveEntry` 두 번으로는 안 된다: 중간 상태에 상대가 안 빠져 있어 **성립하는 교환이 거절된다.** 판정은 교환 후 상태로 한 번에, 쓰기는 전부 통과 뒤에 |
| **교환의 용량식** | **크기 항은 그쪽이 칸을 먹는 자리일 때만 붙는다**(10차). `GetUsedSlots`가 슬롯에 든 것을 애초에 안 세기 때문 — `DeltaPA = A.SlotId.IsNone() ? (SizeB - SizeA) : 0`, `PB`도 대칭. `PA == PB`면 **합산.** 무조건 가감하면 **성립하는 교환이 거절된다** (본체가 꽉 차면 핫바 무기 자리를 못 바꾸고, 크기 다른 조준경 둘은 언제나) |
| **교환의 원자성** | **한 프레임 안의 `MarkItemDirty` 둘은 함께 도착한다 — 확인함.** `NumChanged`가 변경 전부를 한 헤더에 싣고 상한 초과도 경고뿐(`FastArraySerializer.h:985-1001`, `.cpp:10`), 커스텀 델타는 프로퍼티 단위 한 덩어리(`RepLayout.cpp:4583`), 분할 번치는 `bPartialFinal`까지 모아서 올린다(`DataChannel.cpp:867-911`). **단 검사와 쓰기가 두 프레임에 걸치면 이 보장이 사라진다** — *"전부 통과한 뒤에 쓴다"* 가 원자성까지 지키고 있다 |
| **`MoveEntry` 제자리 거절** | **검사 0으로 넣는다**(10차). 목적지가 지금 자리와 같으면 검사 5의 `CanFit`이 **자기 크기를 두 번 센다.** UI가 안 부르게 됐어도 `Server_MoveEntry`는 열려 있다 |
| 핫바 5~0 | **`TArray<int32> HotbarRefs` — Step 04.** 1~4와 자료구조가 다르다. 5~0은 **소유가 아니라 참조**라 아이템이 인벤토리에 남고 칸을 먹는다. `SlotId`를 쓰면 칸 계산에서 조용히 빠진다 |
| 인벤토리 복제 조건 | **`COND_OwnerOnly`** — 남의 가방이 보이면 치트 + 대역폭 |
| 식별자 | **`FEPInventoryEntry::EntryId`** (int32, 서버 발급, 단조 증가, **재번호 없음**). `FGuid InstanceId` / `SchemaVersion`은 **제거** |
| 상태 초기화 | **Definition의 virtual `InitState(const FEPItemData&, FEPItemState&)`**. static 팩토리 2종 제거 |
| **DT vs DA 배치 원칙** | **① 표로 나란히 조정하는 값은 DT** (`SlotSize`/`SellPrice`/`bFungible`/`InitialCharges`/`ContainerCapacity`) **② 에셋 참조·virtual·타입 전용은 DA** (`WorldMesh`/`Icon`/`InitState`/`MaxAmmo`). 판정선 = "모든 아이템이 값을 갖는가" |
| Definition 보유 대상 | **모든 아이템.** 예외 없음 (`WorldMesh` + `Icon` + `InitState`가 전부 필요) |
| 픽업 복제 | `DORM_Initial` + Tick off. **복제 상태가 불변이라 `FlushNetDormancy()` 불필요.** 동적 스폰이라 `DORM_Initial`의 특별 취급(`IsNetStartupActor()` 요구, `NetDriver.cpp:8347`)은 적용되지 않는다 — 한 번 복제된 뒤 휴면한다 |
| 픽업 컬링 | **아직 못 본 픽업만 컬링된다.** 한 번 본 픽업은 멀어져도 클라에 남는 것이 **정상**이다 — 휴면 진입 후에는 릴러번시 검사에 도달하지 않는다(`NetDriver.cpp:5429` → `:5389`). 팝핑과 채널 재생성을 없애기 위한 것 |
| 픽업 콜리전 | **Step 01: 전 채널 `ECR_Ignore` + `ECC_WorldDynamic`.** C++ 프리미티브 기본값이 `BlockAll`이라(`PrimitiveComponent.cpp:356`) 손대지 않으면 플레이어가 바닥 아이템에 걸린다. Step 02가 `EP_TraceChannel_Interact` **하나만** 연다 |
| `InitPickup` 호출 시점 | **`SpawnActor`와 같은 프레임.** 복제는 프레임 끝(`LevelTick.cpp:1900`)에 현재 값으로 나간다. 넘기면 클라가 `NAME_None`을 받고 휴면해 영구 고착. `SpawnActorDeferred`는 불필요 |
| `PickupClass` | **`UEPLootDeveloperSettings`에 `TSoftClassPtr<AEPPickup>`.** 스포너(01)와 버리기(03) **두 스폰 경로가 같은 값을 봐야** 하는데 버리기에는 물어볼 스포너가 없다. **`TSubclassOf`는 안 된다** — config + `TSubclassOf`에 BP 경로를 주면 CDO 생성 시점에 동기 로드되고(`UObjectGlobals.cpp:4379` → `PropertyBaseObject.cpp:596`) `LOAD_NoWarn`이라 실패가 조용하다. 폴백은 **소비 지점**에 둔다 (6차 §8) |
| `BP_Pickup`의 경계 | **액터 수준만 넣는다** (연출 타임라인·프롬프트 위젯·오디오 컴포넌트). **획득 사운드·VFX·메시는 `UEPItemDefinition`** — 아이템마다 다르기 때문. Lyra도 같다: `ULyraPickupDefinition`이 `DisplayMesh`/`PickedUpSound`/`PickedUpEffect`를 들고 **액터 클래스 필드는 없다**. 어기면 아이템마다 BP를 만들게 된다 |
| 전역 데이터 참조 위치 | **`UDeveloperSettings` 유지.** `DA_EPGameData` 도입 안 함 — Lyra 자신이 `ULyraAudioSettings : UDeveloperSettings`에 소프트 에셋 참조 9개+를 두고, `ULyraGameData`는 GE 3개짜리 통일 뿐이다. config 에셋 경로를 1개로 줄이는 구조가 아니다(Lyra는 20개+) (6차 §3·§5) |
| `.ini` 경로의 리네임 안전성 | **자동으로 안 고쳐진다.** 엔진이 리네임 다이얼로그에 *"config INI … may need Find/Replace … assets can be missing from cooked builds"*(`AssetRenameManager.cpp:463`)라고 직접 말한다. `TSoftObjectPtr`의 이득은 리다이렉터가 아니라 **탐지·경고·에셋 피커** — 즉 **깨질 때 시끄럽다**는 것 (6차 §1) |
| `GetPrimaryAssetId()` | **`final`.** 하위 클래스 재오버라이드가 Step 00 `WeaponDef` 사건의 실제 원인이었다. 순서 규칙은 사람이, `final`은 컴파일러가 지킨다 |
| `EmptyWeight` | **루트 테이블에서만 유효** — 하위 적용 시 등급 확률이 침식됨 |
| Definition 로딩 | **매치 전 전량 상주**(AssetManager). 소프트는 `WorldMesh`/`Icon`/`WeaponMesh`만 |
| 무기 액터 스폰 책임 | **`UEPCombatComponent`** 유지 — 인벤토리는 `EquipFromInventory(EntryId)`로 넘김 |
| 드랍 / 장착 RPC 파라미터 | **`EntryId`** |
| `MarkItemDirty` | **`AddEntryCharges()` 안에서 보장.** 원시 엔트리를 밖으로 내보내지 않으므로 호출자가 잊을 방법이 없다 |
| 권한 검사 | **early return** (`if (!GetOwner()->HasAuthority()) return;`). `check()`는 Shipping에서도 크래시하고 프로젝트 관례와 다르다 |
| DT 캐시 | `FEPItemData` **값 복사**. 행 포인터 캐시 금지(리임포트 시 댕글링) |
| `UEPLootTable` 로딩 | `EPLootTable` PrimaryAssetType 등록 — `EP.Loot.RollTable`이 이름으로 찾으려면 필요 |
| 서버 상호작용 검증 | `UEPGA_Interact::ActivateAbility`에서 **거리 재검증 + `CanInteract()` 재호출**. 없으면 `DropCooldown`이 서버에서 무시됨 |
| 상호작용 진입점 | **`UEPGA_Interact` 어빌리티** (7차). 직접 서버 RPC를 만들지 않는다 — 대상은 `FGameplayEventData::Target`으로 실려 간다 |
| 액터 복제 설정 API | `SetNetCullDistanceSquared()` / `SetNetUpdateFrequency()` — 5.5부터 필드 직접 대입 deprecated |
| `FInstancedStruct` 전환 기준 (**상태** 축) | **세 번째 아이템 카테고리가 자기 전용 필드를 요구할 때.** 지금 쓰면 프로퍼티 델타를 잃는다 |
| Definition 확장 방식 (**정의** 축) | **상속 유지.** `UEPItemDefinition` ← `UEPWeaponDefinition`. Lyra식 프래그먼트 조합(`ULyraInventoryItemDefinition::Fragments`)으로 가지 않는다 — 아래 |

### Definition을 프래그먼트로 바꾸지 않는 이유 — 그리고 바꿀 신호

프래그먼트는 UE에서 관용구다. Lyra(`LyraInventoryItemDefinition.h:43`)뿐 아니라 **GAS가 5.3에서 이 구조로 리팩터했고**(`GameplayEffect.h:2460-2462` `TArray<TObjectPtr<UGameplayEffectComponent>> GEComponents`), `UGameFeatureAction`도 같다. 세 곳의 `UCLASS` 지정자가 `DefaultToInstanced + EditInlineNew + Abstract`로 판박이다.

**그런데 프래그먼트가 푸는 문제를 우리는 이미 상속으로 풀었다.**

| 프래그먼트의 이득 | 우리 상태 |
|---|---|
| ① 에디터에 그 아이템에 의미 있는 필드만 보인다 | **이미 얻고 있다.** `DA_Bandage`(`UEPItemDefinition`, 5필드)를 열어도 `MaxAmmo`가 안 보인다 — 클래스가 다르다 |
| ② 베이스 클래스를 안 건드리고 확장 | **소비자가 없다.** Game Feature 플러그인이 있어야 값이 나온다 |

Lyra의 문제는 *"Definition 하나가 모든 아이템 종류의 필드를 들어서 대부분이 비어 있다"* 였다. 우리 `UEPWeaponDefinition`의 20여 필드는 **모든 무기가 실제로 쓰는 값**이라 그 문제가 성립하지 않는다.

**전환 비용 (측정치, 2026-07-30):** `WeaponDef->` 호출 지점 **34곳** / 6개 파일(전투 경로 전부) + 무기 DA **3종 재작성**. UPROPERTY를 인스턴스드 서브오브젝트로 옮기면 **기존 값이 자동으로 안 따라오므로** 20필드 × 3DA = 60개를 손으로 다시 넣어야 하고, 그러려면 DA를 재저장해야 한다 — Step 00 함정 #1의 그 작업이다.

**미룬다고 비싸지지 않는다.** 비용이 `무기 DA 수 × 호출 지점 수`에 비례하는데 둘 다 천천히 는다. `PickupClass`의 `TSubclassOf` 건과 다른 점이 이것이다 — 그건 작성 중인 코드에 들어가는 중이라 "지금이 최저"가 맞았다.

> **★ 바꿀 신호: 한 아이템이 두 카테고리의 전용 필드를 *동시에* 요구할 때.**
> 예 — "장착 가능한 컨테이너"(파우치 슬롯 달린 방탄조끼). 상속은 다중 상속이 안 되므로 그때 물리적으로 막힌다.
>
> **그때도 전부 바꾸지 않는다.** `UEPItemDefinition`에 `TArray<TObjectPtr<UEPItemFragment>> Fragments`를 추가해 **새로 생긴 축만** 프래그먼트로 빼고, `UEPWeaponDefinition`은 상속으로 남긴다. GAS도 기존 필드를 한 번에 옮기지 않고 컴포넌트를 병행했다.

### 기존 코드에서 반드시 손대야 할 것

| 위치 | 조치 | 단계 |
|---|---|---|
| `UEPItemInstance` / `UEPWeaponInstance` **클래스 파일 전체** | **삭제.** `FEPItemState`(USTRUCT)로 대체. 호출처 0이라 비용 없음 | Step 00 |
| `UEPItemInstance::InstanceId`(FGuid) / `SchemaVersion` | 위 삭제에 포함. 각각 "읽는 코드 없음" / "세이브 포맷의 속성" | Step 00 |
| `UEPItemDefinition` | `virtual InitState(const FEPItemData&, FEPItemState&)` + `GrantedAbility` + `IsDataValid()` 추가 | Step 00 |
| `UEPWeaponDefinition::MaxAmmo` (`uint8`) | `int32`로 변경 — `Charges`/어트리뷰트와 캐스팅 정리 | Step 00 |
| `UEPWeaponDefinition::GetPrimaryAssetId()` | **오버라이드 제거.** 지금 `"WeaponDef"`를 반환해 상위(`"ItemDef"`)와 타입이 갈린다 → 한 타입만 등록하면 무기 Definition이 로드 안 됨 | Step 00 |
| `FEPItemData` | **신규 3필드: `bFungible` / `InitialCharges` / `ContainerCapacity`** (전부 DT — 배치 원칙 참조) | Step 00 |
| `DT_Items.uasset` | **완료 — 9행 / DA 9종.** 비무기 6행(`AmmoBox_545`/`Bandage`/`Scrap`/`Resume`/`Cash_10000`/`Backpack_B`) **＋ 13차로 `Shirt_Basic`·`Pants_Basic` 2행 추가 필요** + 각각 Definition 에셋. **행 값은 미검증** — `Cash_10000.SellPrice`(기본 100), `bFungible`, `Backpack_B.ContainerCapacity`(＋`SlotSize`보다 작은지 — 13차), 무기 `SlotSize`를 눈으로 확인할 것 | Step 00 (완료) |
| `DOCS/GAME.md` | **인벤토리 절 전면 개정 완료** — 6슬롯 → 칸 합산(본체 10칸) + 배낭 + 스택 없음 + 내구도 신설. 결정 A는 구현 결정이 아니라 **기획 변경**이었다 | (완료) |
| Project Settings → Asset Manager | `ItemDef` PrimaryAssetType 등록 (`AssetBaseClass = EPItemDefinition`, 전량 상주용). 현재 `Map`/`PrimaryAssetLabel`만 등록돼 있음 | Step 00 |
| Project Settings → Asset Manager | `EPLootTable` PrimaryAssetType 등록 (`RollTable` 이름 조회용) | Step 01 |
| Project Settings → Collision | `EP_TraceChannel_Interact` 신규 채널 — **`ECC_GameTraceChannel3`**(1·2는 `WeaponTrace`·`Projectile`이 쓴다, `DefaultEngine.ini:306-307`). **반드시 `DefaultResponse=ECR_Ignore`, `bTraceType=True`**. `ECR_Block`으로 만들면 모든 프리미티브가 새 채널도 막아 `ECC_Visibility` 재사용과 결과가 같다(`CollisionProfile.cpp:470`). 기존 `WeaponTrace`(`DefaultEngine.ini:306`)와 같은 형태 | Step 02 |
| `AEPCharacter` 생성자 | `InteractionComponent`(Step 02) / `InventoryComponent`(Step 03) 추가 + 게터. `CombatComponent`/`RewindComponent` 옆 | Step 02·03 |
| `AEPPlayerController` | `InteractAction`(**F**) / `ToggleInventoryAction`(Tab) UPROPERTY + 게터 — 기존 Dash/Heal/Shield 패턴 | Step 02·04 |
| `EPCombatComponent.cpp:177` `InitAmmo(MaxAmmo)` | **제거.** 버리기가 들어오면 12/30 무기를 버렸다 줍기만 해도 30/30이 되는 익스플로잇 | Step 05 |
| `EPCombatComponent` `Init*` → `Set*` | `Init*`은 어트리뷰트 델리게이트를 안 쏜다 → 장착해도 HUD 탄약이 안 바뀜. **`MaxAmmo`를 `Ammo`보다 먼저** 세팅(`PreAttributeChange`가 `[0, MaxAmmo]`로 클램프) | Step 05 |
| `UEPCombatComponent::UnequipWeapon()` | 잔탄 write-back(`AddEntryCharges`). **교체·버리기·사망 세 경로가 전부 여기를 거치게** 만들어 한 곳에만 둔다. null 가드 필수 | Step 05 |
| `UEPInventoryComponent::RemoveEntry()` | **제거된 서브트리를 반환한다.** 장착 검사(자식 포함)·write-back·캐스케이드를 내부에서 보장 — 호출자가 지킬 순서가 없다 | **Step 03** |
| Project Settings → Asset Manager | 두 타입 모두 **`Is Editor Only = false` + `Cook Rule = AlwaysCook`**. 둘이 `ModifyCook`의 같은 `if` 한 줄에 걸려 있어(`AssetManager.cpp:4738`) 하나만 틀려도 **패키지 빌드에서만** 리스트가 빈다. `Unknown`은 "아무도 하드 참조하지 않으면 안 나간다"는 뜻이고, Definition DA는 런타임에 타입으로 긁어오므로 **레지스트리 관점에서 고아다** | Step 00·01 |
| `EPGameMode::HandleStartingNewPlayer` | `DefaultWeaponClass` → `DefaultLoadout : TArray<FName>` | Step 05 |
| `UEPCombatComponent::EquipWeapon(AEPWeapon*)` | 유지 + `EquipFromInventory(int32 EntryId)` 추가해 위임. 무기 액터 스폰 책임은 여기 남긴다 | Step 05 |
| `AEPWeapon::GetMaxAmmo()` | 신규. 지금은 `WeaponDef->MaxAmmo`를 그대로 반환하고, 부착물이 오면 여기서 합산. `GetDamage()`와 같은 기존 패턴 | Step 05 |
| `AEPGameMode::HandleMatchHasStarted()` | `Super::` **앞에서** 스포너 `SpawnLoot()` 순회 호출 | Step 01 |

---

## 시작 시점 코드 상태 (2026-07-26)

아이템 데이터 계층이 **전부 데드코드**다. **Step 00**이 조회 경로와 팩토리를 가동시키고, Step 01이 첫 게임플레이 소비자가 된다.

| 심볼 | 상태 |
|---|---|
| `FEPItemData` | 선언만. 참조 코드 0 |
| `DT_Items.uasset` | 존재. 읽는 코드 0 |
| `UEPItemDefinition` | 클래스 + `DA_AK74_*` 3종 존재 |
| `UEPItemInstance::CreateInstance()` | 호출처 0 |
| `UEPWeaponInstance::CreateWeaponInstance()` | 호출처 0 |
| `UEPGameInstance` | 빈 껍데기 |
| ~~픽업~~ | **Step 01에서 생김** — `AEPPickup` / `AEPItemSpawner` / `UEPLootTable` (`Public/Loot/`, `Private/Loot/`) |
| 상호작용 / 인벤토리 | 클래스 자체가 없음 |

> **설계 변경 이력 (2026-07-26).** 검증 기록은 전부 `Review/` 아래에 있다.
>
> | 차수 | 결정 | 기록 |
> |---|---|---|
> | 1차 | 개체 상태를 `UObject` → `USTRUCT` | `Review/05_Loot_REVIEW_StructMigration.md` |
> | 2차 | **스택 폐지 + 칸 수 합산**(타르코프식), `bFungible` 합치기, 불변식을 코드로 | `Review/05_Loot_REVIEW2_*.md` |
> | 중간 | **배낭(중첩 컨테이너)** 확정 — `ParentEntryId`가 Step 03으로 앞당겨짐 | 사용자 기획 |
> | 3차 | **실무성 검수** — 버리기 잔탄 소실 등 설계 결함 3건, 미선언 심볼 9개, 문서 중복 9건 | `Review/05_Loot_REVIEW3_*.md` |
> | 4차 | 3차 반영 확인 + 판단 5건 | `Review/05_Loot_REVIEW4_*.md` |
> | 5차 | **Step 01 단독 검수 (Step 00 구현 후 첫 검수).** 콜리전 기본값 `BlockAll`, 완료 조건 6 오류, `SpawnLoot()` 본문 부재, 롤 반환 규약 구멍 | `Review/05_Loot_REVIEW5_*.md` |
> | 6차 | **전역 데이터 참조 위치 (Lyra 소스 직독).** `UDeveloperSettings` 유지 확정, config `TSubclassOf` → `TSoftClassPtr`, `.ini` 리네임 근거 정정 | `Review/05_Loot_REVIEW6_*.md` |
> | 7차 | **Step 02를 GAS로 확정 (2026-08-02).** `Server_Interact` 직접 RPC 폐기 → `UEPGA_Interact` + `FGameplayEventData::Target`. 틱 훅을 `NotifyControllerChanged()` 하나로. 상호작용 채널 `GameTraceChannel2`→`3`(`Projectile` 충돌). Lyra `Interaction/` 모듈은 **가져오지 않는다** | `Review/05_Loot_REVIEW7_*.md` |
> | 8차 | **Step 03 검수 (2026-08-04) — 드랍은 RPC로 확정.** 7차의 *"입력 진입점은 어빌리티 하나다"* 를 **"월드 상호작용은 어빌리티 / 서버가 소유한 상태 변경은 RPC"** 로 대체. 항목 콜백 3종은 **링크 에러**라 선언 금지. `bIsRoot` private 분리, `EquippedEntryId` 대칭, `FindFungibleEntryId`에 컨테이너 인자, `DropCooldownEndTime` 복제. **문서 반영 완료** | `Review/05_Loot_REVIEW8_*.md` |
> | 9차 | **슬롯 12개 기획 확대 검수 (2026-08-22) — `SlotId`가 장착의 유일한 진실로 확정.** 언턴드식 12슬롯(핫바 4 + 착용 8) + 부착 4. `EquippedEntryId`/`EquippedBackpackEntryId` **필드 삭제**, `ActiveHotbarIndex` 하나만 남김. `MoveEntry`/`GetEntryInSlot(Parent, SlotId)`/`SlotPriority`/`BodySlots` 신설. `Server_MoveEntry`·`HotbarRefs`는 Step 04로. **문서 반영 완료** | `Review/05_Loot_REVIEW9_*.md` |
> | 10차 | **Step 04 격자 UI 검수 (2026-08-23) — Step 04를 04-A(표시)/04-B(드래그)로 분할.** 순서를 클라 로컬에 두는 판단은 **유지**되되 지속을 **`ULocalPlayerSaveGame`** 으로(받아들이기로 했던 대가 둘이 불필요했다). `SwapEntries` 용량식을 `SlotId.IsNone()` 조건부 델타로 교정, 드롭을 **칸이 받고 버블링으로 패널이 판정**(`HitTestCell` 폐기), `FEPCellHit`, *"같은 컨테이너면 조건 없이 로컬"*, 빈 칸 N개 → **`＋ 남은 용량` 한 덩어리**, `UDynamicEntryBox(Wrap)` 풀링, `MoveEntry` 검사 0(제자리 거절), `GetInsertionOrder()`의 본체 선두. **문서 반영 완료** | `Review/05_Loot_REVIEW10_*.md` |
> | **11차** | **아이템 순서를 서버로 (2026-08-23) — 10차 결론을 사용자 판단으로 뒤집음.** *"저장했을 때도 순서를 유지하는 것이 맞다."* `FEPInventoryEntry::SortKey`(`int32` 희소, `Step=1<<16`), `AssignSortKey` 단일 쓰기 지점, `Server_ReorderEntry(EntryId, PrevEntryId)`(인덱스 아님), `GetSortedContents` 클라·서버 공용, `RenormalizeSortKeys`. **`RemoveEntryInternal` ②의 `bIsRoot`가 두 필드를 관장** → 배낭 되줍기에 내용물 순서가 산다. `ULocalPlayerSaveGame` 계열 **전부 삭제**. **문서 반영 완료** | 검수 아님 — 설계 결정 |
> 
> | **11차 검수** | **11차 설계를 검수 (2026-08-23) — 10차 답변이 §5를 스스로 철회했다.** *"저장소를 검증하고 키를 검증하지 않았다."* 새 결함 둘을 잡았다: **① `SortKey` 동률**(키 공간을 `GetSortedContents`로 구해서 — 정상 플레이로 재현) **② 재정규화가 이분 고갈에만 걸림**(맨 앞/맨 뒤가 무한 증감, 오버플로 후 영구적). `Server_ReorderEntry` RPC를 **04-B로**(9차 규칙), `Owner`를 `TObjectPtr<UActorComponent>`로(Lyra), 03-D 신설은 기각. **문서 반영 완료** | `Review/05_Loot_REVIEW11_*.md` |
> 
> | **12차 검수** | **12차 요청을 검수 (2026-08-23) — ★ 무한 재귀를 잡았다.** 재정규화 가드에서 `PrevEntryId != INDEX_NONE`이 빠져 **맨 앞 드래그가 서버를 죽인다**(함정 4t). 제자리 드롭 조기 반환(4u). `KeySpace_` 접두어. **§2 의미론은 A 확정** — B는 같은 동률이 재현되고 요청서 §2-2의 반례는 **앞 이웃 API에서 표현 불가능**했다. **★ *"UE에 선례가 없다"* 가 거짓** — `FUIFrameworkStackBoxSlot::Index`. **문서 반영 완료** | `Review/05_Loot_REVIEW12_*.md` |
> | **13차 검수** | **Step 03 착수 전 자체 검수 (2026-08-25) — 결함 19건.** `MoveEntry`가 **`FindEntry` 복사본에 쓰고 있었다**(무동작인데 `true`), 루트 스냅샷이 **`SlotId`를 안 버려** `AddSubtree`가 검사 3·4를 우회, **자동 착용이 `CanFit`을 지나** *"등이 비었는데 못 맨다"*, `PostReplicatedReceive` **컴파일 불가**(11차 타입 변경 미반영), `SortKey`의 `INDEX_NONE` 센티널이 **키 −1과 충돌**, `InsertEntry`가 자기를 키 공간에 포함, **`MoveEntry`를 부르는 커맨드가 0개**(Step 04까지 안 돎). 사용자 결정 4건: **`AddSubtree(Parent, SlotId, In)`**(기본값 금지) · **루트가 `SlotId`도 버린다** · **가드를 "단일 쓰기 지점 다섯"에**(사설 래퍼 기각) · **본체 0칸 + 용량표 확정**. `AddSubtree`·`TryAutoEquip`·`StartingEquipment`가 **03-C로 내려갔다**. **문서 반영 완료**(03·DOCS·GAME·STATUS) | `Review/05_Loot_REVIEW_Inventory.md` ＋ `05_Loot_REVIEW13_Request.md` |
> | **14차 (사용자 지적)** | **`Server_EquipBackpack` 삭제 (2026-08-25).** *"어차피 옷도 하의도 컨테이너인데 배낭 전용 RPC를 만들 이유가 있었나"* — 셋 다 맞았다. ① 9차의 *"좁은 RPC가 넓은 RPC보다 낫다"* 는 **04-B가 `Server_MoveEntry`를 여는 이상 성립하지 않는다**(넓은 문이 열린 뒤의 좁은 문은 표면을 안 줄인다) ② **13차가 옮긴 04-A에도 호출자가 0개** — 근거였던 `EP.Inv.Equip`이 **콘솔 커맨드**라 규칙대로면 내부 함수를 직접 부른다 ③ **배낭이 특별할 근거가 없다** — `SlotPriority`＋`BodySlots`가 상의·하의·외투·배낭을 같은 모양으로 만들고, `TryAutoEquip`에 이미 적용한 규칙이 RPC 이름에만 안 적용돼 있었다. **`EP.Inv.Equip`(04-A)도 `EP.Inv.Move`의 별칭이라 폐기.** 착용의 클라 표면은 **04-B `Server_MoveEntry` 하나** | `Mine/EquipmentSlots.md` §15 |
> | **13차 답변** | **자체 검수 21건과 결정 5건을 검수 (2026-08-25) — 판정은 전부 유지, 근거 셋이 교체됐다.** 새 결함 **12건**: **★ `MoveEntry`도 키를 재부모 뒤에 구한다**(`InsertEntry`와 같은 결함), **`FScopedInventoryNotify` 정의가 소스에 없다**(첫 빌드 컴파일 에러), **`AddSubtree`에 슬롯 검증이 0** → `CanPlaceInSlot` 추출, **`SlotSize ≥ 1` 검증 없음**(본체 0칸에서 `0+0<=0`), **`EP.Inv.Add`에 컨테이너 인자 없음**(0칸 전환이 03-A를 죽인다), **STATUS 스냅샷이 소스보다 뒤**(결함 #1·#3·#4는 이미 고쳐져 있었다), **DOCS §4-7 획득 절차가 `TryAutoEquip`을 모른다**. **★★ 옛 03-B(배낭) 구간 삭제** — `Server_EquipBackpack`이 Step 03에 호출자 0개. **★ 근거 교정 셋**: 래퍼 기각의 *"엔진 관례와 싸운다"* 는 **거짓**(`MarkAbilitySpecDirty`가 감싼다) / `Cap == SlotSize`는 **익스플로잇이 아니다**(비용 0 ＋ 세 전제) / `StartingEquipment`는 **6차와 같은 자리가 아니다**(→ 미정 #10). **답변의 수치 하나는 틀려서 안 따랐다**(32,763 → 실제 **32,764**). **문서 반영 완료** | `Review/05_Loot_REVIEW13_Answer.md` |
> | **15차 (파일 분할)** | **`05_Loot_03_Inventory.md`(2692줄)를 03-A / 03-B로 쪼갰다 (2026-08-26).** 8차의 *"파일을 쪼개지 않는다"* 를 뒤집는다 — 근거였던 *"`RemoveEntry`가 경계에서 갈린다"* 가 두 번 무너졌다(13차가 가운데 구간 삭제 / 제거 경로 셋이 전부 03-B). **★ 직접적 계기: 열 함수의 본문이 통째로 빠진 것을 검수 여덟 번 동안 아무도 못 봤다** — `FindEntry`·`ContainsEntry`·`RemoveSelf`·`AssignSortKey`·`KeySpace_Min`·`KeySpace_NextAbove`·`KeyOf`·`GetEntryInSlot`·`FindFungibleEntryId`·`GetEquippedEntryId`. **03-A-부록 신설**로 열 개 본문을 썼다. 함정표는 대응 열로 라우팅(A 44 / B 16, 겹치는 4는 양쪽), **완료 조건 번호 1~20 유지**. 옛 파일은 **리다이렉트 스텁**으로 남긴다(다른 문서에 이름 참조가 142곳). `Review/` 하위는 그 시점의 기록이라 손대지 않았다 | `05_Loot_03A_Core.md` · `05_Loot_03B_PickupDrop.md` |
> | **15차** | **03의 13·14차를 04·05에 대조 (2026-08-26) — 검수가 아니라 정합 작업이다.** 04에서 **하드 결함 둘**: ① **`SwapEntries`가 `CanPlaceInSlot`을 그대로 부르면 슬롯 교환이 전부 거절된다** — 검사 4(*"그 자리가 비었나"*)가 교환에서 언제나 거짓이다. `IgnoreEntryId` 네 번째 인자를 **04-B에서** 붙인다 ② **`SortKey`를 안 맞바꿔 함정 4x가 교환 경로로 되살아난다** — 재발급이 아니라 `Swap` 셋이면 끝(상대의 유효한 키를 물려받는다). 05에서 **하드 결함 둘**: ③ **`Server_Equip(EntryId)`가 배정과 활성을 뭉갰다** — 13차의 검사 0 때문에 **정상 경로에서 `false`가 나 숫자키가 안 먹는다.** `Server_SetActiveHotbarIndex(int32)`로 가름(Lyra `SetActiveSlotIndex` 직독, 배정 `AddItemToSlot`은 RPC가 아니다) ④ **`DefaultLoadout`이 0칸 본체에 넣고 있었다** → `GetInsertionOrder()` 순회 ＋ `StartingEquipment` → `DefaultLoadout` 순서를 계약으로. 함정 신설 04에 2건(11e·11f) · 05에 4건(12·12b·13·13b). **문서 반영 완료**(04·05·03) | `05_Loot_04_InventoryUI.md` · `05_Loot_05_Equipment.md` 변경 이력 |
> | **장비 효과 · 컴포넌트 분할** | **실무 표준 대비 기획 정리 (2026-08-26) — 검수가 아니라 확장점 명명이다.** *"인벤토리에 ASC를 쓰면 더 좋지 않나"* 라는 질문에서 시작했다. **답은 "저장은 FastArray, 효과만 GAS"** 이고 Lyra가 그 형태다(`ULyraInventoryManagerComponent : UActorComponent`). **어트리뷰트로는 아이템을 표현할 수 없다** — 스칼라가 `float` 하나이고 개수가 컴파일 타임 고정이다. 실무에 가까워지는 둘을 §7에 이름으로 남겼다. **§7-5 장비 효과** — 방어구가 오면 `AbilitySetsToGrant` ＋ `GiveToAbilitySystem`/`TakeFromAbilitySystem`(`LyraEquipmentDefinition.h:49-51` · `LyraEquipmentManagerComponent.cpp:91-118`). **훅이 `MoveEntry` 한 곳**인 것이 9차 결정의 두 번째 배당금이고, **핸들은 엔트리에 넣지 않는다**(서버 전용인데 엔트리는 복제된다). **§7-6 컴포넌트 3분할** — Lyra는 소유자로 나눈다(`UActorComponent`/`UPawnComponent`/`UControllerComponent`). **우리는 `SlotId`가 엔트리에 있어 데이터가 안 움직인다** — 함수만 옮긴다. **트리거는 §7-1**(상자는 *"가진 것"* 만 있고 *"입은 것"* 이 없는 첫 소유자). **둘 다 지금 만들지 않는다** — 소비자 0개 | `05_Loot_DOCS.md` §7-5 · §7-6 |
> 
> | **탄창 기획** | **미정 #1의 ⓐ(탄창 아이템화) 구현 기획을 문서화 (2026-08-24) — 검수가 아니라 설계 정리다.** *"15발 남은 탄창은 어디로 가나"* 라는 질문에서 시작했다. **재장전 = `SwapEntries` 한 줄**로 떨어져 *"빈 탄창을 어디로 보내나"* 와 칸 검사가 동시에 사라진다(총량이 안 변한다). 신규 API는 `FindReloadMag` 하나. **`AmmoType`을 둘로 갈랐다** — 규격 `Caliber`는 Definition(정적), 탄종 `LoadedAmmoId`는 `FEPItemState`이되 **미정 #6과 함께 온다**(지금은 소비자 없음). **★ `05_Loot_05_Equipment.md:73`의 *"`GetMaxAmmo()`가 부착물 준비의 전부"* 를 정정** — 그건 ⓑ의 답이고 ⓐ의 `MaxAmmo`는 **탄창 엔트리**라 읽는 주체가 `UEPCombatComponent`다. **Step 05에 미리 넣을 것 셋**(`WriteBackAmmo()` 추출 · `UEPEquipSlotWidget::ParentEntryId` · `InjectAmmoFromMag`). **문서 반영 완료** | `05_Loot_DOCS.md` §7-4 |
> 
> **★ 시점은 "Step 05 직후"로 못 박았다 — 기술적 비용이 아니라 검증 순서 때문이다.** 자료구조 변경은 **0**이고(§7-3이 이미 그렇게 설계했다) 바뀌는 것은 대부분 인자뿐이라, 사용자가 *"지금 가면 뭐가 많이 바뀌나"* 를 물었을 때 답은 *"거의 없다"* 였다. 그럼에도 미룬 이유는 ⓐ 작업의 대부분이 **"이미 도는 코드를 한 칸 옮기는 것"** 이라서다 — `Entry.State.Charges ↔ Ammo` 왕복이 ⓑ로 한 번 검증되지 않은 채 옮기면 *"탄창 자식 엔트리의 잔탄이 이상하다"* 에서 **주입이 틀린 건지 탄창 참조가 틀린 건지 가를 방법이 없다.** 지금 `RemoveSelf`조차 빈 스텁이다(`EPInventoryComponent.cpp:105`).
> 
> **부수 효과 — 11차 검수가 남긴 지적 하나가 닫힌다.** *"순서를 서버로 옮겼는데 서버 소비자가 사실상 하나뿐"* 이었는데, `FindReloadMag`가 `GetSortedContents()`로 탄창을 고르면서 **사용자가 드래그로 정렬한 순서가 곧 재장전 우선순위**가 된다. 정책 필드 0개. `SwapEntries`도 `SortKey` 교환 계약(04-7)의 **두 번째 소비자**를 얻는다.
> 
> | **컨테이너 중첩** | **배낭 안 배낭을 유한하게 막았다 (2026-08-24) — 사용자 결정.** *"지금 가방 안에 가방이 되나"* 라는 질문에서 시작했다. **되고 있었다** — 검사 6은 사이클만 막고 `CanFit`은 종류를 안 본다. **결정: 모든 컨테이너가 `ContainerCapacity < SlotSize`** ⇒ 깊어질수록 `SlotSize`가 작아져 유한. **런타임 코드 0줄**(`MoveEntry` 검사 8번을 안 붙인다). DT 컬럼이라 규칙만으로는 새 행에서 조용히 깨지므로 **`IsDataValid()`에 한 줄**(Step 00). **★ 그 규칙이 벗기와 충돌한다** — 용량을 키우려면 `SlotSize`를 키워야 하는데 본체로 벗으면 `CanFit`에 걸려 **배낭 상한 9칸**이 된다 ⇒ **벗기를 `Server_DropItem`으로 확정.** **문서 반영 완료** | `05_Loot_DOCS.md` §4-6 · `05_Loot_00_ItemCore.md` · `05_Loot_03_Inventory.md` 03-6 |
> 
> **★ 상정한 적이 없던 것이다.** 자료구조는 *"깊이와 무관"* 하게 일부러 만들었지만(§4-6·§7-3) **"컨테이너를 컨테이너에 넣을 수 있는가"** 라는 질문은 문서 어디에도 없었다 — §7-3은 오히려 *"무제한 중첩으로 넓히고 싶어지면"* 이라며 **넓히는 쪽만** 준비했다. 즉 **막을 수 있게 만든 게 아니라 막을 생각을 안 한 것**이고, 컨테이너의 정의가 `ContainerCapacity > 0`이라는 **부수효과로만** 존재했던 것이 뿌리다. §4-6에 정의를 명문화했다.
> 
> **부수 — §4-7의 상자 문장을 손봤다.** *"상자를 안 열고 가방에 넣어 탈출하는 것도 가능하다"*(`:669`)가 새 규칙과 반대를 말하게 된다. **`SlotSize(상자) ≤ Capacity(가방)`일 때만** 성립하고 **큰 상자는 못 넣는 게 의도**라고 못 박았다 — 안 그러면 상자로 용량을 무한 증식할 수 있다. 원문의 *"제약이 소멸했다"* 는 **표현의 제약**이지 게임 규칙이 아니다.
> 
> **명시적 깊이 상한(`MaxContainerDepth`)은 기각.** 검사 6이 이미 부모 사슬을 도니 3줄이면 되지만 위 둘로 목적이 달성되고 **확장점이 문서에 이름으로 없다**(CLAUDE.md §2). 여는 신호를 적어 뒀다 — *"용량은 키우고 싶은데 `SlotSize`를 못 키운다"*.
> 
> **12차 검수 답변과 다르게 판정한 것 하나 — 무한 재귀의 귀속.** 답변 §1-2는 *"11차 답변 §4의 코드 스케치가 조건을 떨어뜨렸다 — 내 잘못이다"* 로 적었다. **스케치가 위험했던 것은 맞지만 라이브 버그는 그게 아니다.** 11차 반영 때 Claude는 `PrevEntryId != INDEX_NONE`을 **그대로 유지했고**(당시 맨 앞 분기는 `KeyOf(Sorted[0]) - Step`이라 그 조건이 유일한 방어였다), **12차 요청서를 쓰며 맨 앞 분기를 `NewKey = PrevKey`로 다시 짜면서 그 조건을 지웠다.** 즉 **Claude가 12차 수정에서 넣은 회귀다.** 답변에 책임을 넘기면 *"검수 답변의 코드 스케치를 그대로 쓰면 안 된다"* 라는 엉뚱한 교훈이 남는다 — 실제 교훈은 **"분기 식을 다시 짜면 그 분기에 걸린 가드도 다시 검산한다"** 이다.
>
> **★ 그리고 11차 §10-2의 *"순서 필드 0개"* 는 Claude도 같이 틀렸다.** Claude가 `FFastArraySerializerItem` **직계 파생**만 열어보고 *"엔진의 복제되는 위젯 트리조차 형제 순서를 복제하지 않는다"* 고 사용자에게 보고했는데, **순서 필드는 그것을 상속한 슬롯 타입에 있다**(`FUIFrameworkStackBoxSlot::Index`, `FUIFrameworkGameLayerSlot::ZOrder`). 답변의 개수 정정(14건)이 맞고, **Claude의 "선례 없음" 결론도 함께 철회한다.** 바로잡으니 설계 근거가 **강해졌다** — 엔진이 같은 이유(`FastArraySerializer.h:54`)로 같은 필드를 두고, 조밀을 고른 조건이 우리와 다르다는 대비가 선다.

> **11차 검수 답변과 다르게 판정한 것 셋.**
> ① **`FFastArraySerializerItem` 파생이 "엔진 전체 6개"가 아니다.** 답변은 Runtime(2) + GameplayAbilities(4)만 셌는데 **플러그인 전체로는 12개 이상**이다 — UIFramework 2, OnlineFramework 1, InstancedActors 1, MassGameplay 1, 테스트 플러그인 3+. **다만 결론은 그대로 선다**: 직접 확인한 `FUIFrameworkSlotBase`(위젯 id + 패딩/정렬)와 `FUIFrameworkWidgetTreeEntry`(Parent/Child 쌍)에도 **순서 필드가 없다.** 엔진의 복제되는 위젯 트리조차 **형제 순서를 복제하지 않는다.**
> ② **`double` 기각 근거를 "교체"하지 않고 "재배치"했다.** 답변은 DB/REST와 Dump 가독성을 *"부차적"* 이라며 고갈 판정으로 갈아끼우라고 했는데, **셋 다 유효한 근거다.** 고갈 판정을 앞세우되 나머지 둘을 지웠다가 5단계 2차에서 다시 올라오게 두지 않는다.
> ③ **§8-3의 일정 지적이 오히려 약하다.** 답변은 *"Step 03 전체를 1주로 잡았다면"* 이라고 썼지만 **사용자 추정 1주는 Step 03·04·05 전부**다(메모리 `project-remaining-scope`). 즉 어긋난 폭이 답변이 말한 것보다 크다. 일정 단위를 구간으로 바꾸라는 권고는 그대로 수용한다.
>
> **11차는 검수가 아니라 설계 결정이다.** 10차 반영 직후 *"가방을 벗었다 입으면 순서가 초기화되나"* 라는 질문에서 시작해 **클라 로컬 세이브의 세션 충돌**(`NextEntryId`가 매치마다 1부터 재발급되는데 세이브는 디스크에 남는다)을 찾았고, 그걸 막는 세션 검사를 넣으면 **지속이 한 번도 발휘되지 않아 인메모리와 같아진다**는 것까지 확인했다. 즉 **10차가 청구한 이득(*"재접속·기기 변경에도 순서 유지"*)이 로드맵상 성립하지 않았다.**
>
> **Claude의 첫 권고는 "지금은 옮기지 마라"였고 사용자가 뒤집었다.** 권고 근거는 *"서버 세이브(로드맵 14번)가 마감 범위에 없어 매치 안에서는 이득이 0"* 이었는데, 사용자 판단은 **설계를 세이브에 맞춰 두는 쪽**이다. 실제로 옮기고 나니 `Resolve`/`MoveTo`/세이브 클래스 둘/`FEPContainerOrder`와 **함정 5건이 사라져 코드량이 오히려 줄었다** — 권고가 비용을 과대평가했다.
>
> **키 타입은 검증 중에 한 번 바뀌었다.** 처음 `double`을 제안했으나 ① 5단계 2차의 **외부 DB(REST)** 왕복에서 부동소수가 지뢰이고 ② `Dump` 가독성이 나빠 **`int32` 희소 배치**로 바꿨다. 이분 여유가 52회 → 16회로 줄지만 **둘 다 실사용에서 "거의 안 남"** 이라 차이가 그쪽뿐이다.
>
> **그리고 대역폭 근거를 한 번 정정했다.** `FastArraySerializer.h:1474-1485`(`NetSerializeStruct` = 구조체 전체)만 보고 *"조밀 재번호는 항목 전체를 N번 보낸다"* 고 적었는데, **그건 폴백 경로**다. 기본은 `:1398-1401` → `:1645`의 `FastArrayDeltaSerialize_DeltaSerializeStructs`이고 **바뀐 프로퍼티만** 나간다(`:218-219`). 스태시 280칸 기준 12.6KB가 아니라 **~3.4KB + 280개 changelist 비교**다 — 방향은 같고 크기가 4배 작다.
> 
> **10차에서 답변과 다르게 판정한 것 하나.** 답변은 `SwapEntries` 용량식이 *"본체를 11/10으로 통과시킨다"* 고 했는데, **대조해 보니 그 방향으로는 안 샌다** — 두 수를 `||`로 보기 때문에 어긋나는 경우마다 둘 중 하나가 정확한 값이고 그쪽이 걸러낸다. **틀린 항이 붙는 쪽은 언제나 실제 변화가 0인 쪽**이고 거기서는 원래 값이 이미 용량 이내였다. **실제 증상은 반대 — 성립하는 교환의 거절이다**(본체가 꽉 차면 핫바 무기 자리 바꾸기, 크기 다른 조준경 교체는 언제나). **고칠 식은 답변과 같지만 심각도와 증상이 다르다.** 그리고 답변이 제시한 드롭 핸들러 수정에 남아 있던 *"빈 영역이면 맨 뒤 (MoveTo가 클램프)"* 도 틀렸다 — `FMath::Clamp(-1, 0, N)`은 **0 = 맨 앞**이라 `MoveTo` 쪽을 함께 고쳤다(함정 13d).
>
> **8차에서 7차 판정이 뒤집히지 않았다 — 뒤집힌 것은 거기 붙인 일반화 문장이다.** 7차가 Step 02를 GAS로 고른 근거 셋(대상이 **액터**라 `FGameplayEventData::Target`이 공짜 / 채널링이 02-1에 이름으로 예고 / 여러 기능이 걸림)은 전부 유효하고, **셋 다 드랍에는 성립하지 않는다.** 결정 증거는 `grep -rn "UFUNCTION(Server" LyraGame/` = **1건**이고 그 하나가 인벤토리 슬롯 변경(`int32`)이라는 것 — 직독 확인함.
>
> **8차가 잡아낸 것 중 요청서에 없던 것 둘:** ① `FindFungibleEntryId`에 컨테이너 인자가 없어 **배낭 속 현금이 본체로 탈출**한다(무증상). ② 항목 콜백을 선언만 하면 이름 가림으로 **링크 에러**다 — "무의미한 선언"이 아니었다.
>
> **반영 중 Claude가 추가로 찾은 것 둘 (답변에도 없음):** ③ 03-5가 `RemoveEntry`를 스폰보다 먼저 해서 **스폰 실패 시 서브트리가 증발**한다. ④ `Server_DropItem`의 실패가 전부 조용한 `return`이라 Step 02가 세운 *"실패를 삼키지 않는다"* 가 깨진다 → `Client_OnInteractFailed`를 `Client_OnInventoryActionFailed`로 일반화.
>
> **답변과 한 곳 다르게 반영했다.** 8차는 `UnequipWeapon`의 `SetEquippedEntryId(INDEX_NONE)`(`05_Loot_05_Equipment.md:124`)을 걷어내라고 했으나 **홀스터·사망 경로가 남는다.** `RemoveEntry`가 비우게 하되 `:124`도 유지하고, **`RemoveEntry`가 그것에 의존하지 않는다**로 적었다 — 겹치는 대입 한 줄보다 그쪽 구멍이 비싸다.
>
> **9차 판정의 결정적 근거는 Lyra가 아니라 엔진이다.** Lyra의 `QuickBarComponent::Slots`는 반례로 보이지만 **이름 없는 균질 위치 인덱스 3칸**이고 `ULyraEquipmentDefinition`에는 슬롯 필드가 아예 없다(`LyraEquipmentDefinition.h:36-56`, 헤더 전체 / `EquipItem`·`UnequipItem`에 슬롯 파라미터 없음). Lyra는 *"내 머리에 뭐가 있나"* 를 물을 구조가 없다. 대신 **`USceneComponent`가 정확히 우리 형태다** — `AttachParent` ＋ `AttachSocketName`(`FName`)을 **자식**에 두고, 자식 목록(`AttachChildren`)은 `Transient` **파생 색인**이다(`SceneComponent.h:108-119`). **진실은 자식에, 색인은 파생으로.**
>
> **답변과 한 곳 다르게 반영했다(9차).** 답변은 함정 **3b**를 *"남는다 — write-back 소실은 값의 문제"* 로 판정했으나, **장착의 정의가 바뀐 것을 반영하지 않은 판정이다.** 장착이 `SlotId == "HotbarN"` **＋** `ParentEntryId == INDEX_NONE`이 되면서 *"배낭 속 무기를 장착한 채"* 라는 전제가 성립하지 않는다. 3b의 원래 문장은 **`EquipmentSlots.md` §10 미정 #7**로 옮겼다 — 핫바 5~0이 컨테이너 안 아이템을 들 수 있게 되면 되살아난다.
>
> **답변이 짚지 않은 것 하나 — write-back 순서 계약의 성격이 바뀐다.** `EquippedEntryId`가 저장된 필드일 때는 `RemoveSelf` 뒤에 읽어도 값이 살아 있어 순서를 어기면 *"`INDEX_NONE`을 향한 write-back"* 이었다. 파생 게터가 되면 **write-back이 아예 안 불려 잔탄이 조용히 사라진다.** 함정 **4k**로 추가했다.
>
> **7차에서 이전 판단 두 개가 뒤집혔다.** ① *"이 프로젝트에 서버 RPC가 없다"* → 틀렸다. `TryActivateAbilitiesByTag`가 `ServerTryActivateAbility`(`AbilitySystemComponent.h:1723`)를 부른다. 깨지는 규칙은 *"서버 RPC를 안 쓴다"*가 아니라 *"입력 진입점은 어빌리티 하나다"*였다. ② *"틱 판정을 `BeginPlay`에 두면 리슨서버 호스트가 첫 테스트에서 걸린다"* → **첫 스폰은 통과한다.** `AGameMode`가 `RestartPlayer` 뒤에 `NotifyBeginPlay`를 부른다(`GameMode.cpp:208-221`). 깨지는 것은 리스폰이고, **이 프로젝트엔 아직 리스폰 경로가 없다.**
>
> **3차에서 문서 구조를 바꿨다.** stale 참조가 세 번 연속 남은 원인이 "사실 하나가 3~4곳에 적혀 있고 매번 하나가 남는 것"이었다. **마스터 §4는 결정과 근거만, Step 문서가 코드·함정표·엔진 인용을 단독으로 갖는다.** 1007 → 839줄이고 고칠 곳이 절반이다.

무기는 `EPGameMode.cpp:81`에서 `DefaultWeaponClass`를 직접 스폰. 이 흐름의 인벤토리 이관은 **Step 05 범위 안**이다 (`05_Loot_DOCS.md` §4-8) — `DefaultLoadout : TArray<FName>` 기반 지급으로 교체하고, 무기 액터 스폰 책임은 `UEPCombatComponent`에 남긴다.

---

## 세션 시작 템플릿

```
@LOOT_STATUS.md @05_Loot_0X_XXX_STATUS.md
Step X 진행.
```
