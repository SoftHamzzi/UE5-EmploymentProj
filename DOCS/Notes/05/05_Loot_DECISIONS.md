# Loot 결정 이력

> **추가만 한다.** 지우거나 고쳐 쓰지 않는다. 결정이 바뀌면 새 항목을 쓰고, 옛 항목의 상태를 `대체됨 → D-NNN`으로 바꾼다.
> 지금 유효한 설계: `05_Loot_Design.md` / 진행 상황: `Status/05_Loot_STATUS.md`
> 개발·리뷰 세션은 설계를 바꾸지 않는다 — 여기에 `제안` 항목을 쓰고 멈춘다. 설계 세션이 채택하거나 기각한다.

---

## 새 항목 형식

```
### D-NNN 제목
- 날짜 · 상태: 제안 | 채택 | 기각 | 대체됨 → D-NNN
- 맥락: 무엇이 문제였나
- 결정:
- 버린 대안과 이유:
- 근거: 파일:줄, Review 번호, PIE 확인
- 다시 열 조건:
```

다음 번호: **D-115**

---

## 이관분 (2026-10-02, `Status/05_Loot_STATUS.md` "확정된 설계 결정"에서)

> 원문 표를 **그대로** 옮기고 번호와 상태 칸만 붙였다. 결정 칸 하나에 결정·근거·버린 대안이 섞여 있고, 날짜와 차수는 원문에 있는 것만 있다.
> 상태가 "확인 필요"인 넷은 원문 안에서 서로 모순된다. 옮기면서 고치지 않았다 — 설계 세션이 판정한다 (`DOCS/SETUP_SessionRoles.md` §6).

| ID | 상태 | 항목 | 결정 |
|---|---|---|---|
| D-001 | 채택 | **개체 상태의 형태** | **`FEPItemState` (USTRUCT, 값 타입).** `UEPItemInstance` / `UEPWeaponInstance` / `UEPItemInstanceSubsystem` / `int32` 핸들 **전부 삭제** |
| D-002 | 채택 | **스택** | **없다.** 붕대 3개 = 엔트리 3개. 탄약은 탄약상자의 `Charges`, 돈은 현금뭉치의 `Charges` |
| D-003 | 채택 | **균질 아이템 합치기** | **한다.** `FEPItemData::bFungible`(현금뭉치·탄약상자)이면 `Charges` 합산. 칸이 안 늘어 스택의 부담이 하나도 안 따라온다 |
| D-004 | 대체됨 → D-011 (13차 본체 0칸, 2026-10-08 판정) | **인벤토리 용량** | **컨테이너별 칸 수 합산.** 본체 **10칸**(GAME.md) + 배낭은 **별도 풀**. 통합하지 않는다 |
| D-005 | 채택 | **중첩 컨테이너** | **`ParentEntryId`.** 배낭·부착물·상자가 전부 같은 표현. 자기 타입 재귀는 `Class.cpp:974` Fatal이라 중첩 struct를 쓰지 않는다 |
| D-006 | 채택 | **불변식 강제** | **`RemoveEntry(Id, &Out)`이 제거된 서브트리를 반환한다.** 스냅샷을 얻는 유일한 방법이 제거하는 것이라 **순서를 뒤집는 게 문법적으로 불가능**하다. 장착 검사·write-back·자식 캐스케이드가 그 안에서 노드마다 돈다 |
| D-007 | 채택 | **`RemoveEntry` 4단계 순서** | ① write-back → ② 스냅샷(루트는 `Parent=INDEX_NONE`으로 정규화) → ③ **자기 제거** → ④ 캐스케이드. 각 단계가 하나씩 막는다(잔탄 소실 / `AddSubtree` 계약 / 무한 재귀) |
| D-008 | 채택 | **`RemoveEntry` ↔ `AddSubtree` 계약** | 반환 배열은 **전위 순회, `In[0]`이 루트, 루트의 `Parent`는 `INDEX_NONE`.** 자식은 원본 `Parent` 보존. **양쪽이 다 컴파일되므로 이 계약을 안 적으면 아무도 안 걸린다** |
| D-009 | 채택 | **`Charges` 쓰기** | **`SetEntryCharges`가 유일한 쓰기 지점**(클램프·`MarkItemDirty`·알림). `AddEntryCharges`는 위임. write-back은 대입이라 `Set`, 합치기·소비는 `Add` |
| D-010 | 채택 | **배낭** | 줍고 **빈 슬롯이면 자동 착용.** 별도 필드 없음 — **`SlotId == "Back"` 이 곧 진실**(9차 확정). 교체는 벗고 다시 줍기, **벗기 = 바닥 드랍**(12차). **착용은 칸 검사를 지나지 않는다**(13차) — `AddSubtree(Parent, SlotId, In)`이 `SlotId`가 있으면 `CanFit`을 건너뛴다 |
| D-011 | 채택 (D-004를 대체, 2026-10-08 판정) | **인벤토리 용량** | **본체 0칸.** 수납은 **착용 컨테이너에서만** 나온다 (13차, 2026-08-25). 상의 `11/10` · 하의 `6/5` · 배낭A `15/12` · 배낭B `10/8`. `MaxSlots=10`은 **테스트값**. 스폰 시 `StartingEquipment`로 상의·하의를 입힌다. 규칙은 **`Capacity < SlotSize`**(등호 없음 — 넣기 판정의 `≤`와 다른 식) |
| D-012 | 채택 | **서브트리 삽입** | `AddSubtree()` + **`EntryId` 재매핑.** 없으면 배낭을 버렸다 주울 때 내용물이 증발한다. **칸 검사는 루트만** — 자식은 서브트리 내부로 들어가므로 늘리면 되줍기가 깨진다 |
| D-013 | 채택 | **갱신 알림** | `FEPInventoryList::PostReplicatedReceive` **하나** (수신 1회당 1회). 항목별 콜백 3종은 **선언조차 하지 않는다** — 이름 가림으로 링크 에러가 난다(`FastArraySerializer.h:341,349,356` 기반 no-op + `:1139,1163,1174` 무조건 호출). 서버는 스코프 가드 |
| D-014 | 채택 | **★ 클라 → 서버 요청 경로 (8차)** | **둘뿐이다.** ① **월드 상호작용 / 시간·비용·애님이 붙는 행동 → 어빌리티** ② **서버가 이미 소유한 상태의 변경 요청 → `UEPInventoryComponent`의 서버 RPC.** 7차의 *"입력 진입점은 어빌리티 하나다"* 를 이 문장이 대체한다 |
| D-015 | 채택 | **드랍·장착 RPC** | **`Server_DropItem`(03-5) / `Server_MoveEntry`·`Server_SwapEntries`·`Server_ReorderEntry`(04-B) 직접 RPC.** ~~`Server_EquipBackpack`~~ 은 **14차에 삭제** — 04-B가 넓은 `Server_MoveEntry`를 여는 이상 좁은 래퍼가 표면을 안 줄이고, 호출자도 0개였다. `FGameplayEventData`에 `int32`가 없고(`GameplayAbilityTypes.h:246-284`) `EventMagnitude`를 식별자로 쓴 선례가 엔진·Lyra 통틀어 0건. **Lyra의 유일한 손수 만든 서버 RPC가 정확히 이것이다**(`LyraQuickBarComponent.h:30`, `SetActiveSlotIndex(int32)`). 죽음·시전 게이트는 `CanMutateInventory()` 한 곳 |
| D-016 | 채택 | **드랍 순서** | **스폰 → `RemoveEntry` → `InitPickup`.** 제거를 먼저 하면 스폰 실패 시 서브트리가 증발한다 |
| D-017 | 채택 | **`FindFungibleEntryId`** | **`(int32 Container, FName ItemId)`** — 컨테이너 인자를 빼면 배낭 속 현금이 본체 현금과 합쳐지고, 무증상이다가 배낭을 벗을 때 딸려 나간다 |
| D-018 | 채택 | **삽입 지점** | private **`InsertEntry(Parent, ItemId, State, SlotId)` 하나.** `AddItem`·`AddSubtree`가 공유한다. `AddSubtree`가 자식에 `AddItem`을 부르면 `bFungible` 합치기와 `CanFit`이 동시에 깨진다 |
| D-019 | 채택 | **`bIsRoot`** | **private `RemoveEntryInternal`에만.** public에 두면 루트 정규화를 건너뛸 문법이 생겨 계약이 깨진다 |
| D-020 | 채택 | **`FEPInventoryEntry` 위치** | **`Public/Inventory/EPInventoryTypes.h` (신규).** `EPTypes.h`는 거의 모든 파일이 include하고, 컴포넌트 헤더에 두면 `Loot/`가 컴포넌트 전체에 의존한다. **`Build.cs`에 `NetCore` 추가 필요** — `Engine.Build.cs:86`이 Public 의존으로 갖고 있지만 모듈러 빌드에서 UBT는 **public include 경로만 전파하고 import 라이브러리는 직접 의존한 모듈 것만** 링크 줄에 넣는다. 컴파일은 통과하고 **링크에서 `LNK2019`** 가 난다(실증됨) |
| D-021 | 채택 | **`DropCooldown`** | **`AEPPickup`의 복제되는 `float DropCooldownEndTime`** + `GetServerWorldTimeSeconds()`. 타이머 핸들이면 서버 전용 상태라 클라 프롬프트가 회색이 안 된다. GE 쿨다운도 아니다 — **주체가 픽업이지 플레이어가 아니다** |
| D-022 | 채택 | `FEPItemData::MaxStack` | **읽지 않는다.** 스택 부활용 예약 필드로 남김 (사용자 지시) |
| D-023 | 대체됨 → D-099 (필드 → `Item.Stat.Durability` 스탯) | `FEPItemState::Durability` | **유지.** 무기가 사용한다. GAME.md에 내구도를 정식 편입했다. 열쇠·붕대의 "사용 횟수"는 `Durability`가 아니라 `Charges`다 |
| D-024 | 채택 | 인벤토리 부착 | **Character** — 사망 시 소실이 규칙과 일치 |
| D-025 | 채택 | 인벤토리 복제 | **FFastArraySerializer + POD**, `FEPItemState`를 엔트리에 **내장** (내부 struct 델타 기본 활성) |
| D-026 | 채택 | 픽업 액터 (복제) | **`ItemId`만.** `FEPItemState`는 서버 전용 — 바닥 무기 잔탄 노출은 정보 은폐 위반 |
| D-027 | 채택 | 픽업 액터 (상태) | 스포너가 뿌린 것도 버린 것도 **언제나 유효한 `State` 보유.** 획득은 언제나 값 대입 |
| D-028 | 채택 | 루트 테이블 | **가중치 + 중첩(SubTable)** — GAME.md 등급 확률 50/30/15/5 보존. **수량 필드 없음** |
| D-029 | 채택 | 부분 획득 | **없다** — 전부 아니면 전무 (픽업 하나 = 아이템 하나) |
| D-030 | 채택 | 스폰 시점 | GameMode의 `MatchState`가 지시 (스포너 `BeginPlay` 아님) |
| D-031 | 채택 | 아이템 버리기 | **포함**(Step 03). `Pickup->State = Entry.State` 값 복사 |
| D-032 | 채택 | 무기 장착 이관 | **포함**(Step 05). 탄약은 `Entry.State.Charges`가 진실, GAS `Ammo`는 뷰 |
| D-033 | 채택 | 장비 슬롯 | **별도 배열도 필드도 없다 — `FEPInventoryEntry::SlotId`가 유일한 진실**(9차 확정). 슬롯 12개(핫바 1~4 + 착용 8) + 부착 4. 엔진 자신이 같은 형태다 — `USceneComponent`가 `AttachParent`＋`AttachSocketName`을 **자식**에 두고 자식 목록은 `Transient` 파생 색인(`SceneComponent.h:108-119`) |
| D-034 | 채택 | 장비 슬롯 — 남는 상태 | **`ActiveHotbarIndex`(int32) 하나만.** "1번과 2번 중 어느 쪽을 들었나"는 `SlotId`로 표현되지 않는다. **가리키는 것이 엔트리가 아니라 슬롯이라 죽은 번호가 생길 문법이 없다** |
| D-035 | 채택 | 슬롯 조회 | **`GetEntryInSlot(int32 Parent, FName SlotId)`** — 부모 인자는 선택이 아니다. `Optic`은 무기마다 하나씩 있어 인자를 빼면 **두 번째 무기에 부착물을 못 단다**(4f와 같은 모양) |
| D-036 | 채택 | 엔트리 이동 | **`MoveEntry(EntryId, NewParent, NewSlotId)`가 `ParentEntryId`＋`SlotId`를 고치는 유일한 지점.** 장착·해제·드래그·부착이 전부 이것 하나. 검사 7개 중 **정합(`BodySlots`)·사이클·제자리 거절**이 필수 |
| D-037 | 채택 | 슬롯 종류 판정 | **`UEPLootDeveloperSettings::BodySlots`** — 몸에 붙는 슬롯 12개 목록. 없으면 *"가방 안에 든 상의를 입는다"* 가 표현된다. 소비자 둘(검증 / Step 04 UI) |
| D-038 | 채택 | 슬롯 자격·우선순위 | **`FEPItemData::SlotPriority`(`TArray<FName>`)** — **배열 순서가 곧 자동 배정 우선순위.** 태그 컨테이너는 순서를 표현하지 못하고 에디터 UI가 체크박스 트리다 |
| D-039 | 채택 | **인벤토리 화면** | **정사각형 격자.** 칸 하나 = 아이템 하나, 부피(`SlotSize`)는 **배지 + 분절 게이지**로 (2026-08-23). **2D 배치로 가지 않는다** — 데이터 모델에 2D 크기가 없고 넣으면 Step 03 재작성이다 |
| D-040 | 채택 | 격자 칸 개수 | **가변이다. 고정 격자를 그리면 안 된다** — 용량 20에 아이템이 1~20개 들어갈 수 있어 그림과 게이지가 어긋난다. **격자는 나열, 진실은 게이지** |
| D-041 | 채택 | **빈 칸** | **N개 그리지 않는다**(10차). 개수는 레이아웃 부산물인데 플레이어가 정보로 읽어 **고정 격자 문제가 형태만 바꿔 남는다.** 아이템 칸 뒤에 **`＋ 남은 용량 N` 블록 하나**. 드롭 대상이자 드래그 피드백 대상이 그 하나다 |
| D-042 | 채택 | **넘침 피드백** | **색이 아니라 숫자.** `7 → 11 / 20`(초록) / `18 → 22 / 20`(빨강). 색은 가부까지만 답하고 *"이만큼 모자라다"* 는 못 말한다 |
| D-043 | 채택 | **칸 위젯 생성** | **`UDynamicEntryBox(EDynamicBoxType::Wrap)`** — `FUserWidgetPool` 내장(`DynamicEntryBoxBase.h:179`), `Reset` 기본값이 파괴가 아니라 **회수**(`DynamicEntryBox.h:51`). UMG 코어라 CommonUI 의존이 안 는다. `UListView` 배제(`UObject*` 고정)는 유효하지만 **그때 배제된 것은 아이템 타입이지 풀링이 아니다** |
| D-044 | 채택 | **드롭 라우팅** | **칸이 `OnDrop`을 받고 버블링으로 패널이 판정한다.** `HitTestCell` 지오메트리 산술은 **만들지 않는다** — Slate가 이미 `FBubblePolicy`로 깊은 위젯부터 라우팅한다(`SlateApplication.cpp:5523`, `:382-406`). 검증은 `HandleDrop` 한 곳 |
| D-045 | 채택 | **드롭 지점 타입** | **`FEPCellHit { DisplayIndex, EntryId }`** — `int32` 하나로 두면 같은 변수가 **표시 인덱스**와 **`EntryId`** 를 오가고 **둘 다 컴파일된다** |
| D-046 | 채택 | **같은 컨테이너 드롭** | **조건 없이 로컬이다.** 여백에 놓았다고 `Server_MoveEntry`로 흘리면 `CanFit`이 자기 크기를 두 번 세어 **가방이 찰수록 "맨 뒤로 보내기"가 거절된다** |
| D-047 | 채택 | 구획 순서 | **`UEPLootDeveloperSettings::ContainerOrder`** — `["Coat","Torso","Legs","Back","Wrist"]`. **`GetInsertionOrder()`는 이 배열을 그대로 반환하지 않는다** — 반환형이 `TArray<int32>`(컨테이너 EntryId)이고 **본체(`INDEX_NONE`)를 맨 앞에 직접 붙인다.** `ContainerOrder`에는 본체가 없다(슬롯이 아니다). 공유되는 것은 *순서*이지 *목록*이 아니다 |
| D-048 | 채택 | **아이템 순서** | **서버가 든다 — `FEPInventoryEntry::SortKey`(`int32`)**(11차, 10차 뒤집음). *"서버 로직 중 순서를 보는 곳이 0곳"* 은 지금도 사실이지만 그건 **"볼 필요가 없다"** 이지 **"두면 안 된다"** 가 아니었다. 뒤집은 이유는 **지속** — 아래 |
| D-049 | 채택 | **왜 클라 로컬이 안 되나** | `NextEntryId`는 컴포넌트 필드고 초기값 1이라 **매치마다 재발급**되는데 `ULocalPlayerSaveGame`은 디스크에 남는다 → 지난 매치의 `Order[7]`이 이번 매치의 7번(다른 아이템)에 적용된다. 세션 도장으로 막으면 **지속이 한 번도 발휘되지 않아 인메모리와 같아진다.** 반면 `SortKey`는 로드맵 5단계가 엔트리 배열을 저장하므로 **저장 코드 0줄**로 따라간다 |
| D-050 | 채택 | **★ 일반 규칙 (11차)** | **저장되는 색인의 키는 저장소만큼 오래 살아야 한다.** 짧은 키로 긴 저장소를 색인하면 실패가 *"못 읽는다"* 가 아니라 **"엉뚱한 것을 읽는다"** 로 나타난다 — 번호가 재사용되므로 죽은 참조가 **살아 있는 것처럼 보인다.** 10차가 `ULocalPlayerSaveGame`을 승인하며 **저장소의 수명만 보고 키의 수명을 안 봤다** |
| D-051 | 채택 | **키 공간 ≠ 표시 목록** | `KeySpace_*` 셋과 `RenormalizeSortKeys`는 **부모가 같은 것 전부**를 본다(슬롯 포함). `GetSortedContents`는 **그릴 것을 고르는** 함수라 슬롯을 거른다. 혼동하면 *"무기를 꽂았다 빼는"* 정상 조작에서 **동률이 난다**(함정 4q). 부수 효과로 **꽂았다 빼면 원래 자리로 돌아온다** |
| D-052 | 채택 | **장착과 순서 자리** | **A 확정 (12차).** *"장착은 아이템을 가방에서 꺼내는 것이 아니다. 칸만 돌려주고 순서 자리는 남긴다."* 뺐다 꽂으면 원래 자리로 돌아오고 **그 사이에 넣은 아이템은 그 앞에 선다.** 대안 B(자리를 포기)는 기각 — 키를 맨 뒤로 밀어도 **표시 목록 기준 발급이 그 구간을 침범해 같은 동률이 재현**되고, 키 공간에서 빼면 11차가 없앤 `NewSlotId` 예외가 돌아온다 |
| D-053 | 채택 | **`Prev` vs 틈** | **스코프가 갈린다.** `Prev`는 *"무엇 뒤에 놓으려 했나"*(의도) → **표시 목록**. 틈은 *"어떤 숫자를 줘야 하나"*(표현) → **부모 전체**. 통일하면 한쪽이 깨진다 — 표시 목록으로 통일하면 4q·4s, 부모 전체로 통일하면 **사용자가 가리킬 수 없는 `Prev`** 를 API가 받는다. 헬퍼에 `KeySpace_` 접두어를 붙여 이름으로 막는다 |
| D-054 | 채택 | **재정규화 트리거** | **세 분기 공통이되 판정은 둘로 나눈다** — 경계(`bOutOfRange`, 세 분기)와 고갈(`bNoGap`, **`Prev`가 있을 때만**). 한 식에 묶으면 **맨 앞 분기가 항상 참이 되어 무한 재귀**한다(함정 4t). 종료는 `ReorderEntryInternal(bRetry)` ＋ `ensure`로 **문법이 보장**한다. 경계 판정은 맨 앞/맨 뒤의 **무한 증감**을 막는다 — `int32` 오버플로 후 상태는 재정규화가 안 걸려 **영구적**이다(함정 4r). `KeySpace_NextAtEnd`가 **유일 발급 지점**이라 발급 쪽 가드도 거기 하나 |
| D-055 | 채택 (D-064를 대체, 2026-10-08 판정) | **RPC 표면** | **`ReorderEntry`(내부)는 03-A, `Server_ReorderEntry`(RPC)는 04-B.** 9차가 `MoveEntry`/`Server_MoveEntry`에 적용한 규칙 그대로 — 검증 표면을 소비자보다 먼저 열지 않는다. `EP.Inv.Reorder`가 내부 함수를 직접 불러 계약을 먼저 닫는다 |
| D-056 | 일부 대체됨 → D-102 (키 폭·간격) | **키 배치** | **희소 정수** — `SortKeyStep = 1 << 16`, 사이 삽입은 `(Prev+Next)/2`. **조밀(0,1,2…)을 기각한 이유는 대역폭이 아니다** — `RenormalizeSortKeys`가 **곧 조밀 재번호 루프**라 "조밀로 하면 그 코드가 사라진다"는 이득이 **존재하지 않는다.** 희소는 중점+가드 5줄을 더 쓰고 **O(N)을 O(1)로** 만든다. `double` 기각의 결정적 사유는 **고갈 판정이 부동소수 동등 비교**(`Mid == Prev`)가 되는 것 — 정답인데 영원히 의심받는다 |
| D-057 | 채택 | **키의 스코프** | **형제(같은 `ParentEntryId`) 안에서만.** 다른 컨테이너와 값이 겹쳐도 무관하다. `SlotId`가 있으면 슬롯이 곧 자리라 무의미 |
| D-058 | 채택 | **자리 바꾸기 RPC** | **`Server_ReorderEntry(EntryId, PrevEntryId)`** — **인덱스가 아니라 앞 이웃.** 인덱스는 `bFungible` 병합·자동 획득이 목록을 밀면 한 칸 어긋난다. 검증도 *"Prev가 같은 컨테이너의 수납 형제인가"* 한 줄이고 **다중 선택으로 넓힐 수 있다.** `MoveEntry`와 별도인 이유: 용량·슬롯·사이클을 안 봐서 **실패할 수 없다** |
| D-059 | 채택 | **키를 누가 만드나** | **서버만.** 클라는 앞 이웃(`EntryId`)만 보낸다 — 클라가 키 값을 보내면 겹치거나 범위 밖 값을 심을 수 있다 |
| D-060 | 채택 | **배낭 되줍기와 순서** | **`RemoveEntryInternal` ②의 `bIsRoot`가 `ParentEntryId`와 `SortKey`를 동시에 관장한다.** 루트는 둘 다 버리고(목적지 키 체계로 들어간다) **자식은 둘 다 보존**한다 — 부모가 방금 만들어진 빈 컨테이너라 충돌이 없다. **버린 배낭을 되주우면 내용물 순서가 그대로 산다** |
| D-061 | 채택 | **"전부 옮기기"(Shift+클릭)** | 10차가 *"클라가 `Server_MoveEntry`를 N번"* 이라는 우회로를 적어뒀는데 **11차에 필요 없어졌다** — 서버가 `GetSortedContents`를 직접 부른다. 자동 정렬도 마찬가지로 `RenormalizeSortKeys`의 비교 함수 교체다 |
| D-062 | 채택 | **2D 격자로 갈 때** | `SortKey` → `FIntPoint Location`인데 **호출 지점이 동일하다**(`InsertEntry`·`MoveEntry`·재배치 RPC·스냅샷·루트/자식). **필드 교체이지 구조 변경이 아니다.** 클라 로컬로 뒀다면 *"04-8을 버리고 03을 다시 연다"* 였다. 단 **`CanFit`(스칼라 합산)은 어느 설계로 가든 재작성**이다 |
| D-063 | 채택 | **`FindFungibleEntryId`와 순서** | 배열 순서에 의존하지만 그건 서버 `Entries.Items`의 물리적 순서지 **표시 순서가 아니다.** 무해한 이유는 *"첫 번째를 찾으니 무관"* 이 아니라 **"그 축에서 원소가 교환 가능하다"** — 현금뭉치 둘은 구분 불가다. **`bFungible`인데 구분 가능한 상태를 갖는 아이템이 생기면 깨진다** |
| D-064 | 대체됨 → D-055 (11차 검수가 RPC를 04-B로, 2026-10-08 판정) | 자리 바꾸기 | **`Server_ReorderEntry(EntryId, PrevEntryId)` — Step 03-A.** `MoveEntry`와 별도다: 용량·슬롯·사이클을 안 보므로 **실패할 수 없고**, 그래서 클라가 낙관적으로 먼저 그려도 롤백이 없다. `EP.Inv.Reorder`로 UI보다 먼저 닫는다 |
| D-065 | 채택 | 교환(swap) | **`SwapEntries(A, B)` — Step 04-B.** `MoveEntry` 두 번으로는 안 된다: 중간 상태에 상대가 안 빠져 있어 **성립하는 교환이 거절된다.** 판정은 교환 후 상태로 한 번에, 쓰기는 전부 통과 뒤에 |
| D-066 | 채택 | **교환의 용량식** | **크기 항은 그쪽이 칸을 먹는 자리일 때만 붙는다**(10차). `GetUsedSlots`가 슬롯에 든 것을 애초에 안 세기 때문 — `DeltaPA = A.SlotId.IsNone() ? (SizeB - SizeA) : 0`, `PB`도 대칭. `PA == PB`면 **합산.** 무조건 가감하면 **성립하는 교환이 거절된다** (본체가 꽉 차면 핫바 무기 자리를 못 바꾸고, 크기 다른 조준경 둘은 언제나) |
| D-067 | 채택 | **교환의 원자성** | **한 프레임 안의 `MarkItemDirty` 둘은 함께 도착한다 — 확인함.** `NumChanged`가 변경 전부를 한 헤더에 싣고 상한 초과도 경고뿐(`FastArraySerializer.h:985-1001`, `.cpp:10`), 커스텀 델타는 프로퍼티 단위 한 덩어리(`RepLayout.cpp:4583`), 분할 번치는 `bPartialFinal`까지 모아서 올린다(`DataChannel.cpp:867-911`). **단 검사와 쓰기가 두 프레임에 걸치면 이 보장이 사라진다** — *"전부 통과한 뒤에 쓴다"* 가 원자성까지 지키고 있다 |
| D-068 | 채택 | **`MoveEntry` 제자리 거절** | **검사 0으로 넣는다**(10차). 목적지가 지금 자리와 같으면 검사 5의 `CanFit`이 **자기 크기를 두 번 센다.** UI가 안 부르게 됐어도 `Server_MoveEntry`는 열려 있다 |
| D-069 | 채택 | 핫바 5~0 | **`TArray<int32> HotbarRefs` — Step 04.** 1~4와 자료구조가 다르다. 5~0은 **소유가 아니라 참조**라 아이템이 인벤토리에 남고 칸을 먹는다. `SlotId`를 쓰면 칸 계산에서 조용히 빠진다 |
| D-070 | 대체됨 → D-101 | 인벤토리 복제 조건 | **`COND_OwnerOnly`** — 남의 가방이 보이면 치트 + 대역폭 |
| D-071 | 일부 대체됨 → D-098 (`FGuid` 제거 부분) | 식별자 | **`FEPInventoryEntry::EntryId`** (int32, 서버 발급, 단조 증가, **재번호 없음**). `FGuid InstanceId` / `SchemaVersion`은 **제거** |
| D-072 | 채택 | 상태 초기화 | **Definition의 virtual `InitState(const FEPItemData&, FEPItemState&)`**. static 팩토리 2종 제거 |
| D-073 | 채택 | **DT vs DA 배치 원칙** | **① 표로 나란히 조정하는 값은 DT** (`SlotSize`/`SellPrice`/`bFungible`/`InitialCharges`/`ContainerCapacity`) **② 에셋 참조·virtual·타입 전용은 DA** (`WorldMesh`/`Icon`/`InitState`/`MaxAmmo`). 판정선 = "모든 아이템이 값을 갖는가" |
| D-074 | 채택 | Definition 보유 대상 | **모든 아이템.** 예외 없음 (`WorldMesh` + `Icon` + `InitState`가 전부 필요) |
| D-075 | 채택 | 픽업 복제 | `DORM_Initial` + Tick off. **복제 상태가 불변이라 `FlushNetDormancy()` 불필요.** 동적 스폰이라 `DORM_Initial`의 특별 취급(`IsNetStartupActor()` 요구, `NetDriver.cpp:8347`)은 적용되지 않는다 — 한 번 복제된 뒤 휴면한다 |
| D-076 | 채택 | 픽업 컬링 | **아직 못 본 픽업만 컬링된다.** 한 번 본 픽업은 멀어져도 클라에 남는 것이 **정상**이다 — 휴면 진입 후에는 릴러번시 검사에 도달하지 않는다(`NetDriver.cpp:5429` → `:5389`). 팝핑과 채널 재생성을 없애기 위한 것 |
| D-077 | 채택 | 픽업 콜리전 | **Step 01: 전 채널 `ECR_Ignore` + `ECC_WorldDynamic`.** C++ 프리미티브 기본값이 `BlockAll`이라(`PrimitiveComponent.cpp:356`) 손대지 않으면 플레이어가 바닥 아이템에 걸린다. Step 02가 `EP_TraceChannel_Interact` **하나만** 연다 |
| D-078 | 채택 | `InitPickup` 호출 시점 | **`SpawnActor`와 같은 프레임.** 복제는 프레임 끝(`LevelTick.cpp:1900`)에 현재 값으로 나간다. 넘기면 클라가 `NAME_None`을 받고 휴면해 영구 고착. `SpawnActorDeferred`는 불필요 |
| D-079 | 채택 | `PickupClass` | **`UEPLootDeveloperSettings`에 `TSoftClassPtr<AEPPickup>`.** 스포너(01)와 버리기(03) **두 스폰 경로가 같은 값을 봐야** 하는데 버리기에는 물어볼 스포너가 없다. **`TSubclassOf`는 안 된다** — config + `TSubclassOf`에 BP 경로를 주면 CDO 생성 시점에 동기 로드되고(`UObjectGlobals.cpp:4379` → `PropertyBaseObject.cpp:596`) `LOAD_NoWarn`이라 실패가 조용하다. 폴백은 **소비 지점**에 둔다 (6차 §8) |
| D-080 | 채택 | `BP_Pickup`의 경계 | **액터 수준만 넣는다** (연출 타임라인·프롬프트 위젯·오디오 컴포넌트). **획득 사운드·VFX·메시는 `UEPItemDefinition`** — 아이템마다 다르기 때문. Lyra도 같다: `ULyraPickupDefinition`이 `DisplayMesh`/`PickedUpSound`/`PickedUpEffect`를 들고 **액터 클래스 필드는 없다**. 어기면 아이템마다 BP를 만들게 된다 |
| D-081 | 채택 | 전역 데이터 참조 위치 | **`UDeveloperSettings` 유지.** `DA_EPGameData` 도입 안 함 — Lyra 자신이 `ULyraAudioSettings : UDeveloperSettings`에 소프트 에셋 참조 9개+를 두고, `ULyraGameData`는 GE 3개짜리 통일 뿐이다. config 에셋 경로를 1개로 줄이는 구조가 아니다(Lyra는 20개+) (6차 §3·§5) |
| D-082 | 채택 | `.ini` 경로의 리네임 안전성 | **자동으로 안 고쳐진다.** 엔진이 리네임 다이얼로그에 *"config INI … may need Find/Replace … assets can be missing from cooked builds"*(`AssetRenameManager.cpp:463`)라고 직접 말한다. `TSoftObjectPtr`의 이득은 리다이렉터가 아니라 **탐지·경고·에셋 피커** — 즉 **깨질 때 시끄럽다**는 것 (6차 §1) |
| D-083 | 채택 | `GetPrimaryAssetId()` | **`final`.** 하위 클래스 재오버라이드가 Step 00 `WeaponDef` 사건의 실제 원인이었다. 순서 규칙은 사람이, `final`은 컴파일러가 지킨다 |
| D-084 | 채택 | `EmptyWeight` | **루트 테이블에서만 유효** — 하위 적용 시 등급 확률이 침식됨 |
| D-085 | 채택 | Definition 로딩 | **매치 전 전량 상주**(AssetManager). 소프트는 `WorldMesh`/`Icon`/`WeaponMesh`만 |
| D-086 | 채택 | 무기 액터 스폰 책임 | **`UEPCombatComponent`** 유지 — 인벤토리는 `EquipFromInventory(EntryId)`로 넘김 |
| D-087 | 채택 | 드랍 / 장착 RPC 파라미터 | **`EntryId`** |
| D-088 | 채택 | `MarkItemDirty` | **`AddEntryCharges()` 안에서 보장.** 원시 엔트리를 밖으로 내보내지 않으므로 호출자가 잊을 방법이 없다 |
| D-089 | 채택 | 권한 검사 | **early return** (`if (!GetOwner()->HasAuthority()) return;`). `check()`는 Shipping에서도 크래시하고 프로젝트 관례와 다르다 |
| D-090 | 채택 | DT 캐시 | `FEPItemData` **값 복사**. 행 포인터 캐시 금지(리임포트 시 댕글링) |
| D-091 | 채택 | `UEPLootTable` 로딩 | `EPLootTable` PrimaryAssetType 등록 — `EP.Loot.RollTable`이 이름으로 찾으려면 필요 |
| D-092 | 채택 | 서버 상호작용 검증 | `UEPGA_Interact::ActivateAbility`에서 **거리 재검증 + `CanInteract()` 재호출**. 없으면 `DropCooldown`이 서버에서 무시됨 |
| D-093 | 채택 | 상호작용 진입점 | **`UEPGA_Interact` 어빌리티** (7차). 직접 서버 RPC를 만들지 않는다 — 대상은 `FGameplayEventData::Target`으로 실려 간다 |
| D-094 | 채택 | 액터 복제 설정 API | `SetNetCullDistanceSquared()` / `SetNetUpdateFrequency()` — 5.5부터 필드 직접 대입 deprecated |
| D-095 | 채택 | `FInstancedStruct` 전환 기준 (**상태** 축) | **세 번째 아이템 카테고리가 자기 전용 필드를 요구할 때.** 지금 쓰면 프로퍼티 델타를 잃는다 |
| D-096 | 채택 | Definition 확장 방식 (**정의** 축) | **상속 유지.** `UEPItemDefinition` ← `UEPWeaponDefinition`. Lyra식 프래그먼트 조합(`ULyraInventoryItemDefinition::Fragments`)으로 가지 않는다 — 아래 |

### D-097 Definition을 프래그먼트로 바꾸지 않는 이유 — 그리고 바꿀 신호

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

---

## 2026-10-07 — 03 인벤토리 확장성 재검토 (사용자 승인)

> 03 / 03-A / 03-B 구현서 재검토에서 나왔다. 기준은 사용자가 정했다 — **"큰 게임을 만든다고 보고 확장성 위주로. 아이템이 적을 것이라고 판단하지 않는다. 나중에 수정하지 않아도 되는 형태."** 여섯 항목 모두 채택이고, 반영 대상은 `05_Loot_Design.md` · `Implementation/05_Loot_00_ItemCore.md` · `Implementation/05_Loot_03_Inventory_A_Core.md` · `_B_PickupDrop.md`다.

### D-098 아이템의 영구 식별자 — `FEPItemState::ItemUid` (`FGuid`)
- 날짜 · 상태: 2026-10-07 · 채택 (D-071의 *"`FGuid InstanceId` 제거"* 부분을 대체한다. `EntryId`는 그대로다) · **보충 → D-104**
- 맥락: `EntryId`는 **컴포넌트 안의 손잡이**라 버리고 줍거나 컨테이너를 오갈 때마다 새로 발급된다(`AddSubtree`의 `OldToNew`). 그래서 *"이 AK가 아까 그 AK인가"* 를 매치 안에서도 답할 수 없다. 익스트랙션 장르에서 가장 비싼 사고는 아이템 복제(dupe)이고, 실무는 아이템마다 GUID를 기본 키로 둔다(OpenMU — 아이템 행마다 GUID 기본 키라 같은 Id의 아이템이 둘일 수 없다). Design §4-1은 *"저장 시점에 발급, 필요해지면 필드 추가"* 로 미뤘는데, CLAUDE.md §2가 **"나중에 넣기 비싼 것 — 식별자 안정성"** 을 이름으로 적고 있어 서로 충돌했다.
- 결정:
  - `FEPItemState`에 `UPROPERTY(NotReplicated) FGuid ItemUid`. **상태 안에 두므로 값 복사로 저절로 따라간다** — 픽업 `Payload` · `AddSubtree` · 세이브에 코드 0줄.
  - **발급 지점은 하나 — `UEPItemDefinitionSubsystem::MakeItemState`.** 아이템이 세상에 처음 생기는 경로(스포너 · `StartingEquipment` · 치트)가 전부 여기를 지난다. virtual인 `InitState`에 두지 않는다 — 재정의하는 쪽이 빠뜨릴 수 있다.
  - **역할 분리:** `EntryId` = 이 컴포넌트 안에서 가리키는 번호(RPC 파라미터 · 트리 참조). `ItemUid` = 아이템의 정체(로그 · 세이브 · 복제 탐지). 둘을 섞지 않는다.
  - **합치기(`bFungible`):** 받는 엔트리의 `ItemUid`가 남고 흡수되는 쪽 것은 사라진다. 분할이 생기면 새 `ItemUid`를 발급한다.
  - **불변식:** 한 컴포넌트 안에서 `ItemUid`가 겹치지 않는다. `InsertEntry`가 색인(D-100)으로 검사해 `ensure` 후 거절한다 — 값 복사로 같은 아이템이 둘이 되는 버그를 **그 자리에서** 잡는다.
  - 복제하지 않는다. 클라는 `EntryId`로 가리킬 뿐 정체가 필요 없다.
- 버린 대안과 이유:
  - **저장 시점 발급(옛 §4-1):** 매치 중 이동·드랍 로그에서 같은 아이템을 이어서 추적할 수 없고, 복제 탐지가 사후 분석으로만 가능하다.
  - **`int64` 서버 카운터:** 매치 서버가 여러 대면 유일하지 않다.
  - **`EntryId`를 전역으로 만들기:** RPC 손잡이와 트리 참조에 정체가 섞여, 재매핑 규칙(`OldToNew`)이 정체를 바꾸는 꼴이 된다.
- 근거: CLAUDE.md §2 / `AddSubtree` 재매핑(`_B_PickupDrop.md` 03-4) / [On item duplication exploits](https://munique.net/item-duplication-exploits/)
- 다시 열 조건: 백엔드가 자기 ID 체계로 발급하게 될 때. 그때도 필드는 그대로 두고 **값의 출처만** 바뀐다.

### D-099 종류별 수치 상태 — `FEPItemState::Stats` (태그 + `int32`). `Durability` 필드 제거
- 날짜 · 상태: 2026-10-07 · 채택 (D-095는 그대로 유효하다 — **수치가 아닌** 전용 데이터의 전환 기준으로 범위가 좁아진다)
- 맥락: `FEPItemState`는 `Charges` · `Durability` 고정 필드 둘이다(`EPTypes.h:77-85`). 큰 게임에서는 종류별 상태가 갈린다 — 방어구 판 내구도, 열쇠 사용 횟수, 의료품 잔량, 무기 마모. 고정 필드로 가면 **종류가 늘 때마다 모든 엔트리 · 세이브 · 복제 스키마가 바뀐다.** 지금 `Durability`를 읽는 곳은 `EP.Item.State` 로그 한 줄뿐(`EPItemDefinitionSubsystem.cpp:212-213`)이라 바꾸는 비용이 지금이 최저다.
- 결정:
  - `USTRUCT() FEPItemStat { FGameplayTag Tag; int32 Value; }`, `FEPItemState`는 **`ItemUid` · `Charges` · `TArray<FEPItemStat> Stats`** 셋.
  - **`Charges`는 전용 필드로 남긴다.** 모든 카테고리가 쓰고(잔탄 · 금액 · 횟수) `bFungible` 합산과 잔탄 write-back이 매번 거치는 핫 경로다.
  - **`Durability`는 `Item.Stat.Durability` 스탯이 된다.** 값은 정수이고 0~10000(= 0.00~100.00%)이다. 정수인 이유는 `SortKey`의 `double` 기각(D-056)과 같다 — 비교가 정확하고 세이브 · DB 왕복이 안전하다.
  - 쓰기는 `SetEntryStat(EntryId, Tag, Value)` 하나로만 한다. **단일 쓰기 지점 표에 여섯 번째로 올린다** — 가드 · `MarkItemDirty`가 거기 있다.
  - **지금은 `Stats`가 비어 있다.** 초기값을 넣는 쪽은 `InitState`이고, 소비자(방어구 · 무기 마모)가 오는 단계에서 채운다. 필드를 지금 두는 이유는 **스키마**다 — 나중에 추가하는 것은 세이브 포맷 변경이다.
- 버린 대안과 이유:
  - **고정 필드를 계속 추가:** 위 맥락.
  - **`FInstancedStruct`:** `NetSerialize` 경로라(`InstancedStruct.h:163,286`) **안쪽 프로퍼티 델타를 잃는다** — D-095의 사유 그대로다. 반면 `TArray<USTRUCT>`는 FastArray의 내부 struct 델타가 **원소 단위 changelist**로 보낸다(`FastArraySerializer.h:218-219` *"exactly like the standard replication path"*).
  - **수치를 자식 엔트리로 표현:** 칸 계산 · 트리 · UI의 의미를 오염시킨다.
- 근거: Lyra `ULyraInventoryItemInstance::StatTags`(`FGameplayTagStackContainer`, `LyraInventoryItemInstance.h:72`) — 같은 문제를 같은 모양으로 푼다. 우리는 이것을 FastArray로 감싸지 않는다 — 엔트리 자체가 이미 FastArray 원소이고, 안쪽은 일반 `TArray` 델타로 충분하다.
- 다시 열 조건: **수치가 아닌** 개체 데이터(무작위로 굴린 옵션 목록, 각인 문자열)가 기획에 들어올 때 → D-095대로 `FInstancedStruct Extra`를 **추가**한다. 기존 세 필드와 경로는 안 바뀐다.

### D-100 파생 색인 — 조회를 선형 스캔에서 떼어낸다
- 날짜 · 상태: 2026-10-07 · 채택 · **일부 대체됨 → D-106** · **색인의 자리는 대체됨 → D-113** (컴포넌트 → `FEPInventoryList`) (03-3의 *"`UsedSlots`를 캐시하지 않는다"* 와 03-A-부록의 *"넷 다 O(N) 순회다. 캐시하지 않는다"* 를 대체한다)
- 맥락: `FindEntry` · `ContainsEntry` · `GetEntryInSlot` · `GetUsedSlots` · `KeySpace_*` · `AssignSortKey`가 전부 배열을 처음부터 끝까지 돈다. 그리고 그것들이 겹친다 — `RenormalizeSortKeys`가 형제마다 `AssignSortKey`(O(N))를 불러 **O(N²)**, `AddSubtree`가 자식마다 키를 두 번 구해 O(K·N), UI 갱신이 컨테이너마다 `GetUsedSlots`를 불러 O(C·N). 스태시 · 시체 루팅에서 N은 수백에서 수천이다.
- 결정:
  - 컴포넌트에 **복제도 세이브도 하지 않는 파생 색인**을 둔다 — `EntryId → 배열 인덱스` · `Parent → 자식 EntryId 목록` · `ItemUid → EntryId` · `컨테이너 → 사용 칸`.
  - **늘 통째로 다시 만들고 고쳐 쓰지 않는다.** `bIndexDirty`를 세우고, 다음 조회가 `EnsureIndex()`에서 O(N)으로 한 번 재구축한다.
  - **무효화는 구조를 바꾸는 쓰기 지점 셋**(`InsertEntry` · `RemoveSelf` · `MoveEntry`)과 **클라의 `PostReplicatedReceive` 하나**다. `AssignSortKey` · `SetEntryCharges` · `SetEntryStat`은 무효화하지 않는다 — 색인이 담는 것(위치 · 부모 · 슬롯 · 정체 · 칸)을 안 바꾼다.
  - 모든 조회는 색인을 지난다. 배열을 직접 도는 것은 `EnsureIndex()`뿐이다.
- 옛 금지 사유가 사라지는 이유:
  - *"추가 · 제거 · 복제 수신 세 경로를 전부 갱신해야 하고 하나만 빠져도 틀린다"* → 갱신하지 않고 **무효화만** 한다. 무효화 지점이 단일 쓰기 지점 표에 묶여 있어 빠뜨릴 자리가 새로 생기지 않는다.
  - *"부모 없는 자식이 먼저 도착하면 캐시를 어떻게 하나"* → 통째 재구축이라 도착 순서와 무관하다. 다음 수신 때 다시 만들면 저절로 맞는다 — 옛 문장이 말한 *"파생값을 매번 처음부터"* 를 그대로 지킨다.
- 버린 대안과 이유:
  - **증분 갱신:** `RemoveAtSwap`이 인덱스를 옮기므로 갱신 규칙이 생기고, 그 규칙이 곧 옛 금지 사유다.
  - **지금처럼 선형:** 위 맥락.
- 근거: 엔진도 같은 모양이다 — `USceneComponent::AttachChildren`이 `Transient` 파생 색인(`SceneComponent.h:108-119`, 9차가 `SlotId`의 근거로 쓴 그것). 원시 엔트리를 밖으로 내보내지 않는 구조(03-2)라 **공개 API는 한 글자도 안 바뀐다.**
- 다시 열 조건: 프로파일러에서 `EnsureIndex` 재구축이 보일 때 → 그때 증분 갱신으로 간다. 내부 변경이다.

### D-101 인벤토리 가시성은 **컴포넌트 단위 `COND_NetGroup`**이다
- 날짜 · 상태: 2026-10-07 · 채택 · **보충 → D-107** · **일부 대체됨 → D-110** (상자 · 시체의 그룹 등록 부분. 플레이어 `NetGroupOwner`는 그대로) (D-070을 대체한다 — *"남의 가방은 안 보인다"* 는 그대로이고 **수단**이 바뀐다)
- 맥락: 지금은 프로퍼티마다 `COND_OwnerOnly`다. **소유 연결이 없는 액터**(월드 상자 · 시체 · 차량)에 같은 컴포넌트를 달면 아무에게도 복제되지 않는다. §7-1과 사망 드랍(§8 미정 #4)이 오는 날 복제 조건을 **고쳐야** 한다 — "나중에 수정하지 않는다"에 정면으로 걸린다.
- 결정:
  - 컴포넌트가 `GetReplicationCondition()`을 재정의해 **`COND_NetGroup`** 을 돌려준다(`ActorComponent.h:639-640`). 프로퍼티 조건은 전부 **`COND_None`** 이다 — 걸러내는 것은 컴포넌트다. `COND_NetGroup`은 프로퍼티에 쓸 수 없다(`UnrealNetwork.cpp:47` `checkf`).
  - **플레이어:** 서버 `BeginPlay`에서 엔진의 특수 그룹 **`UE::Net::NetGroupOwner`** 에 등록한다(`UNetworkSubsystem` → `FNetConditionGroupManager::RegisterSubObjectInGroup`). 엔진이 그 그룹을 `COND_OwnerOnly`로 판정한다(`DataChannel.cpp:4136-4138`) — **지금과 같은 동작**이다.
  - **§7-1 상자 · 시체(그때 추가):** 컴포넌트를 **자기 고유 그룹**에 등록하고, 여는 사람의 PC를 `IncludeInNetConditionGroup`, 닫으면 `RemoveFromNetConditionGroup`(`PlayerController.h:1668-1671`). **팀원 공유가 기획에 오면 팀 그룹을 하나 더 등록한다** — 그룹은 여러 개 동시에 가질 수 있다(`NetConditionGroupManager.h:46-47`). 둘 다 **기존 코드를 고치지 않는 추가**다.
  - **전제:** 소유 액터가 `bReplicateUsingRegisteredSubObjectList = true`여야 한다(`ActorComponent.h:635`, 엔진 기본값 false — `ActorComponent.cpp:108`). `AEPCharacter` 생성자에 한 줄이다. 프로젝트에 `ReplicateSubObjects` 재정의가 없어 충돌하는 코드가 없다.
  - 레거시 복제(`DataChannel.cpp:4128-4148`)와 Iris(`ReplicationSystem.cpp:130` — `NetGroupOwner` 필터 그룹) 둘 다 지원한다.
- 버린 대안과 이유:
  - **프로퍼티 `COND_OwnerOnly` 유지:** 위 맥락 — §7-1에서 반드시 고친다.
  - **`IsNetRelevantFor`:** 액터 단위라 상자 메시까지 남에게 사라진다.
  - **ReplicationGraph 노드:** 프로젝트가 안 쓰고 레거시 전용이다.
  - **열 때 Client RPC로 스냅샷:** 복제 경로가 둘이 된다. 델타 · 순서 · 알림을 다시 짜야 한다.
- 근거: 위 엔진 줄 번호(UE 5.7 직독).
- 다시 열 조건: §7-1에서 **그룹에서 빠진 연결의 클라에 마지막 내용이 남는** 동작을 PIE로 확인한다. UI를 닫는 것으로 충분하지 않으면 그때 처리를 정한다 — 그룹 구조는 바뀌지 않는다.

### D-102 `SortKey`는 `int64`, 간격 `1 << 32`
- 날짜 · 상태: 2026-10-07 · 채택 (D-056의 키 폭 · 간격을 대체한다. 희소 배치 · 재정규화 · 키 공간 스코프 · `double` 기각은 그대로다)
- 맥락: `int32` · `1 << 16`이면 같은 틈에 16번 꽂으면 고갈되고, 형제 상한이 32767이다. 재검토 중 Claude가 *"아이템이 적으니 조밀 번호도 된다"* 고 했다가 사용자 기준(아이템이 적다고 가정하지 않는다)으로 철회했다 — **희소 키 자체는 Jira LexoRank · fractional indexing과 같은 표준 기법이고, 순서를 바꿀 때 dirty가 1개라는 것이 핵심이다.** 남는 질문은 폭뿐이다.
- 결정: `int64 SortKey`, `SortKeyStep = 1LL << 32`, `SortKeyGuard = SortKeyStep * 4`. 이분 여유 **32회**, 형제 상한 **2³¹**. 엔트리당 4바이트가 는다.
- 버린 대안과 이유: **`int32` 유지** — 세이브 포맷이 확정되기 전인 지금 바꾸는 비용이 최저이고, 확정 뒤에는 포맷 변경이다. **문자열 LexoRank** — 무한 이분이지만 비교 · 복제 · `Dump`가 비싸고, 재정규화가 이미 있어 이득이 없다.
- 근거: [Fractional indices](https://www.luxoft.com/blog/luxoft-algorithms-effective-efficient-fractional-indices) · [How to efficiently reorder items](https://yasoob.me/posts/how-to-efficiently-reorder-or-rerank-items-in-database/)
- 다시 열 조건: 2D 격자로 갈 때(D-062) — 필드가 `FIntPoint Location`으로 교체된다.

### D-103 큰 보관함은 별도 컴포넌트다 — 플레이어 배열에 합치지 않는다
- 날짜 · 상태: 2026-10-07 · 채택
- 맥락: Iris는 FastArray 변경을 `요소 인덱스 % 마스크 비트 수`로 표시한다(`FastArrayReplicationFragment.h:267,483`). 그래서 **배열 하나가 커질수록 변경 1건에 무관한 요소가 같이 전송된다** — Epic 답변은 *"modifying a single element can cause a number of elements (equal to the array's size divided by 64) to be replicated"*. 지금은 Iris가 꺼져 있다(UE 5.7 기본값 `CVarUseIrisReplication = 0`, `IrisConfig.cpp:12`; 프로젝트 Config에 설정 없음). 그러나 Epic의 방향이 Iris다.
- 결정: **"가진 것"의 소유자마다 `UEPInventoryComponent` 하나.** 스태시(로드맵 5단계) · 상자 · 시체 · 차량은 각자 컴포넌트를 갖고, 플레이어의 `Entries`에 합치지 않는다. 사이를 오가는 것은 §7-1이 이미 적은 `RemoveEntry` → `AddSubtree` 경로다(`_A_Core.md` 03-2). 제거는 계속 `RemoveAtSwap`이다 — Epic이 Iris에서 권하는 형태와 같다(*"In Iris, RemoveAtSwap should always be used"*).
- 버린 대안과 이유: **한 컴포넌트에 보관함까지:** 위 맥락 + D-101의 가시성을 컨테이너마다 다르게 줄 수 없다.
- 근거: [Epic 포럼 — FFastArraySerializer re-replicates on removal](https://forums.unrealengine.com/t/ffastarrayserializer-fully-re-replicates-on-element-removal-instead-of-sending-only-the-delta/2655967)
- 다시 열 조건: 없음.

---

## 2026-10-08 — 10-07 결정의 반박 검토 (Codex, 사용자 승인)

> 10-07 결정을 Codex(`gpt-6-astra`, medium)로 반박 검토했고, `review-verifier`가 6건 모두 소스 대조로 확인했다(CONFIRMED 5 · PARTIALLY 1). 그중 03에 닿는 셋(D-104~D-106)은 고치고, 미래 조건에만 닿는 둘은 D-107에 다시 열 조건으로 남긴다. 그룹 탈퇴 후 클라에 남는 데이터 문제는 D-101의 다시 열 조건에 이미 있다. 반영 대상은 `Implementation/05_Loot_03_Inventory_A_Core.md` · `_B_PickupDrop.md` · `Guide/05_Loot_03_Inventory_A_Core_Guide.md`다.

### D-104 정체 검사는 **합치기보다 앞**이다 — `CheckIdentity`
- 날짜 · 상태: 2026-10-08 · 채택 (D-098의 *"`InsertEntry`가 그 자리에서 잡는다"* 를 보충한다)
- 맥락: `AddItem`의 `bFungible` 분기는 기존 엔트리를 찾으면 `AddEntryCharges` 후 바로 성공을 돌려준다(`_A_Core.md` 03-3). 정체 검사는 그 뒤의 `InsertEntry`에만 있었다. 그래서 정체가 없는 `State`나 이미 이 컴포넌트에 있는 `ItemUid`를 들고 와도 **수량만 오르고 아무 진단이 없다.** `AddSubtree`의 원소 하나짜리 경로도 `AddItem`으로 빠지므로 같다.
- 결정:
  - 검사를 private `CheckIdentity(ItemId, State) const`로 뺀다. 내용은 그대로 두 줄이다 — **정체가 유효한가 · 색인에 이미 있는가.**
  - **`AddItem`이 데이터 확인 바로 뒤, 합치기 분기 앞에서 부른다.** `InsertEntry`도 그대로 부른다. 합치기 경로가 `InsertEntry`에 닿지 않기 때문이다.
  - D-098의 합치기 규칙(*받는 엔트리의 `ItemUid`가 남는다*)은 그대로다. **정상적인 합치기는 들어오는 정체가 이 컴포넌트에 없는 경우**이고, 검사는 그 경우를 막지 않는다.
- 버린 대안과 이유: **합치기에서만 따로 검사** — 검사가 두 벌이 되어 한쪽만 고쳐진다.
- 근거: Codex 지적 3 · `review-verifier` CONFIRMED(`_A_Core.md` 1687-1694 / 1709-1714, `_B_PickupDrop.md` 153-154)
- 다시 열 조건: 없음.

### D-105 `AddSubtree`의 넣기 전 검사 — 옛 번호도 겹치면 안 되고, 루트는 "0번 원소"로 정의한다
- 날짜 · 상태: 2026-10-08 · 채택
- 맥락: 10-07의 넣기 전 검사는 정체와 *"부모가 앞에 나왔는가"* 만 봤다. **옛 `EntryId`가 겹치면** 통과한 뒤 `OldToNew`가 덮어써져 뒤 원소가 엉뚱한 부모에 붙고, 반환되는 루트 번호도 틀린다(예: 루트 10, 자식 B=10, 자식 C의 부모=10 → C가 B 아래로 가고 B의 번호가 반환된다). **루트의 정의도 두 벌이었다** — 검사는 *"0번 원소"*, 삽입은 *"`ParentEntryId == INDEX_NONE`"*.
- 결정:
  - 검사에 **`In[0].ParentEntryId == INDEX_NONE`**, **옛 `EntryId`가 입력 안에서 유일**, **1번 이후 원소의 부모는 `INDEX_NONE`이 아니고 앞에 나왔다**를 더한다.
  - 루트는 **`i == 0`** 하나로 정의한다. 검사와 삽입이 같은 식을 쓴다. 삽입 루프의 부모 조회는 `FindChecked`다 — 검사가 보장한다.
- 버린 대안과 이유: **서버가 만든 입력만 들어온다고 보고 생략** — `RemoveEntry`의 출력에서는 중복이 안 나오지만, 문서가 *"전부 아니면 전무"* 를 약속했고 세이브 로드처럼 밖에서 오는 입력이 예고돼 있다(D-098 "세이브").
- 근거: Codex 지적 4 · `review-verifier` CONFIRMED(리뷰의 예시 입력을 `_B_PickupDrop.md` 174-209대로 추적)
- 다시 열 조건: 없음.

### D-106 서브트리 삽입 · 제거는 **일괄 경로**다 — 색인을 원소마다 다시 만들지 않는다
- 날짜 · 상태: 2026-10-08 · 채택 (D-100의 *"한 조작 안에서 무효화가 여러 번 나도 다음 조회 때 한 번만 돈다"* 를 대체한다. 통째 재구축 원칙은 그대로다)
- 맥락: 통째 재구축이 싼 것은 **무효화 사이에 조회가 없을 때**뿐이다. `AddSubtree`는 원소마다 `InsertEntry`(정체 검사 · 키 계산이 색인을 읽고, 끝에서 무효화) → `AssignSortKey`(`FindEntryMutable`이 다시 읽음)를 돌아 **원소마다 재구축** — O(K·N)이다. 재귀 제거도 `RemoveSelf`가 무효화한 직후 자식 조회가 재구축한다. D-100이 없애려던 차수가 그대로 남았다.
- 결정:
  - **삽입:** 물리적 추가를 private **`AppendEntry(Parent, ItemId, State, SlotId, SortKey)`**로 뗀다 — 번호 발급 · 추가 · `MarkItemDirty` · `MarkIndexDirty`만 하고 **색인을 읽지 않는다.** `InsertEntry` = `CheckIdentity` + `KeySpace_NextAtEnd` + `AppendEntry`. `AddSubtree`는 넣기 전 검사(D-105)와 루트 키 계산을 **색인 한 번**으로 끝내고, 루프에서는 `AppendEntry`만 부른다. 자식 키는 스냅샷 값을 바로 넘긴다 — `AssignSortKey`로 덮어쓰지 않는다.
  - **제거:** 재귀는 그대로 두되, 노드마다 **표시만** 한다(`TSet<int32> Doomed`). 물리적 제거는 public `RemoveEntry`가 재귀를 마친 뒤 **`RemoveSelf(Doomed)` 한 번**이다. `RemoveSelf`는 색인에서 배열 위치를 모아 **큰 것부터** `RemoveAtSwap`한다. 사이클은 `Doomed`가 막는다 — 이미 표시된 번호를 다시 만나면 돌아간다.
  - 결과: 서브트리 하나에 색인 재구축은 **조작 전 한 번**, 다음 조회 때 한 번이다. O(N + K log K).
  - 이름은 바꾸지 않는다. `InsertEntry` · `RemoveSelf`가 단일 쓰기 지점 표에 그대로 있고, `AppendEntry`는 `InsertEntry`의 몸통이다.
- 버린 대안과 이유:
  - **증분 갱신:** D-100이 버린 이유 그대로다(`RemoveAtSwap`이 인덱스를 옮긴다).
  - **무효화만 끝으로 미루기:** 루프 안의 조회가 옛 색인을 읽어 틀린다 — 리뷰도 같은 판단이다.
  - **프로파일러가 볼 때까지 미루기(D-100의 다시 열 조건):** 고칠 곳이 이 두 함수뿐이고 지금은 코드가 없어 비용이 최저다. 기준은 *"나중에 수정하지 않는다"* 다.
- 근거: Codex 지적 5 · `review-verifier` PARTIALLY CONFIRMED(복잡도는 CONFIRMED, 문서 문장은 *"작은 K에서만 맞다"*). `RemoveAtSwap`(`Array.h:2185`)이 마지막 원소를 빈자리로 옮기므로, 큰 위치부터 지우면 아직 안 지운 위치가 옮겨지지 않는다.
- 다시 열 조건: 프로파일러에서 **단일 엔트리 조작**의 재구축이 보일 때 → 증분 갱신. 내부 변경이다.

### D-107 D-101 보충 — 휴면 상자와 travel 보존 액터
- 날짜 · 상태: 2026-10-08 · 채택 (D-101에 다시 열 조건 둘을 더한다. 지금 코드는 안 바뀐다) · **상자 휴면 부분은 대체됨 → D-110** (상자가 내용물을 복제하지 않는다)
- 맥락: Codex 지적 1 · 2, `review-verifier` CONFIRMED. 둘 다 플레이어 인벤토리에는 닿지 않는다 — `AEPCharacter`는 휴면하지 않고(프로젝트의 `NetDormancy`는 `EPPickup.cpp:25` 하나), 프로젝트에 seamless travel 코드가 없다(C++ · ini에 `Seamless` · `ServerTravel` · `GetSeamlessTravelActorList` 0건. BP 설정은 미확인).
- 결정:
  - **§7-1 상자 · 시체:** 휴면 액터는 그룹 가입만으로는 복제되지 않는다 — 레거시 드라이버가 휴면 연결의 액터를 컴포넌트 필터보다 먼저 건너뛴다(`NetDriver.cpp:5307-5311`, `:5426-5432`. `IncludeInNetConditionGroup`은 그룹 배열만 바꾼다 — `PlayerController.cpp:6471-6478`). **열기 계약에 "그룹 가입 뒤 `FlushNetDormancy`"를 넣는다.** 열려 있는 동안 휴면에 다시 들어가지 않게 하는 정책도 그때 정한다.
  - **seamless travel로 인벤토리 소유 액터를 보존하게 되면:** 보존 액터는 `EndPlay` · `BeginPlay`를 다시 타지 않고(`World.cpp:8609-8626`, `Actor.cpp:4701`), 그룹 관리자는 월드 서브시스템이다(`NetworkSubsystem.h:12,33`). **새 월드에서 그룹을 다시 등록하는 지점**을 그때 만든다. 그 전까지 정책은 *"인벤토리 소유 액터는 travel 때 다시 만든다"* 다.
- 버린 대안과 이유: **지금 코드에 넣기** — 소비자(상자 · travel)가 없다(CLAUDE.md §2 "상상한 확장점").
- 근거: 위 줄 번호(UE 5.7, `review-verifier` 직독)
- 다시 열 조건: §7-1 착수 시 / `bUseSeamlessTravel`을 켤 때.


## 2026-10-08 — 개정 뒤 문서 불일치 판정 (구현 세션 대조 → 설계 세션)

> 판정만 하고 새 결정이 없는 것: **D-004 → D-011**(Design §4-6이 이미 본체 0칸), **D-064 → D-055**(11차 검수가 `Server_ReorderEntry`를 04-B로 옮겼고 A 03-2 · 04 문서가 그대로 따른다), **D-023 → D-099**(10-07에 상태 줄이 빠졌다). 상태 줄만 고쳤다.

### D-108 균질 아이템의 개체 상태는 `Charges`뿐이다 — `Stats`를 갖지 않는다
- 날짜 · 상태: 2026-10-08 · 채택 (D-099 보충. D-063이 적은 "깨지는 조건"을 막는다)
- 맥락: 옛 불변식은 *"균질 아이템은 `Durability`를 쓰지 않는다"* 였다(Design §4-6). D-099로 `Durability` 필드가 없어지고 수치 상태가 `Stats` 배열로 일반화됐는데 문장은 필드 하나만 가리킨 채 남았다. 합치기(A 03-3)는 들어오는 `State`에서 `Charges`만 더하고 나머지를 버린다 — **들어온 `Stats`는 진단 없이 사라진다.** D-063의 *"`bFungible`인데 구분 가능한 상태를 갖는 아이템이 생기면 깨진다"* 가 정확히 이 경우다.
- 결정:
  - 불변식을 **"`bFungible` 아이템은 `Stats`가 비어 있다"** 로 넓힌다. 내구도만이 아니라 어떤 스탯이든 있으면 균질이 아니다.
  - **검사는 `AddItem`의 `bFungible` 분기 맨 앞 한 곳**이다 — `ensureMsgf(InState.Stats.IsEmpty())`가 거짓이면 `INDEX_NONE`. 균질 엔트리가 처음 생기는 길과 합쳐지는 길이 둘 다 이 분기를 지난다(`AddSubtree`도 원소가 하나면 `AddItem`에 위임한다 — B 03-4). `CheckIdentity`(D-104)와 같은 자리 · 같은 형태다.
- 버린 대안과 이유:
  - **`SetEntryStat`에서 균질이면 거절:** 호출자가 0이고, 예정된 소비자(방어구 · 무기 마모)는 균질이 아니다. 도달 불가 분기의 방어다(CLAUDE.md §2) — `FindFungibleEntryId`에 `SlotId` 조건을 넣지 않은 판정(A 03-3)과 같은 근거.
  - **`IsDataValid`에서 검사:** `Stats` 초기값을 무엇이 정하는지(DT · Definition 필드 · `InitState` 코드)가 아직 없다(D-099 *"소비자가 오는 단계에서 채운다"*). 검사할 데이터 형태가 없다.
- 근거: D-063, D-099, A 03-3 `AddItem`, B 03-4 `AddSubtree`의 원소 1개 분기.
- 다시 열 조건: `Stats` 초기값을 데이터로 두는 단계가 오면 `IsDataValid`에 같은 검사를 더한다. 균질이면서 상태가 갈리는 아이템(예: 유통기한 있는 식량)이 기획에 들어오면 이 불변식 대신 **"같은 `Stats`끼리만 합친다"** 를 검토한다.

---

## 2026-10-08 — 아이템 수색 (사용자 기획)

### D-109 수색은 **아이템 단위 · 플레이어마다**다 — §7-1의 "상자 단위 검색"을 대체한다
- 날짜 · 상태: 2026-10-08 · 채택 (사용자 기획. §7-1 착수 때 구현한다)
- 맥락: 옛 §7-1은 상자를 N초 채널링하면 내용물이 한꺼번에 공개되고 *"이미 검색됨"* 이 모두에게 복제되는 설계였다. 사용자 기획은 타르코프식이다 — 상자를 열면 아이템이 회색 `?`로 보이고, 아이템마다 수색해야 정체가 드러나며, **누가 수색했는지는 사람마다 따로**다.
- 결정:
  - **아는 것의 단위는 (플레이어, `ItemUid`)** 다. `EntryId`는 상자 → 가방으로 옮기면 새로 발급되므로 열쇠가 될 수 없다(D-098). 서버가 플레이어마다 **아는 `ItemUid` 집합**을 든다. 서버 전용이고 이번 판(raid) 동안 산다 — 죽으면 사라진다. 어느 클래스에 둘지는 §7-6 분할 때 정한다.
  - **모르는 상태가 생기는 곳은 §7-1 상자 · 시체의 내용물뿐**이다. 바닥 픽업을 줍기 · 시작 장비 · 내가 넣은 것은 처음부터 안다. 그래서 §7-1 전까지 모든 엔트리는 아는 상태다.
  - **수색 시간 = `SearchSecondsPerSlot` × `SlotSize` × `SearchRarityMultiplier[Rarity]`.** 예: 희귀 총(4칸) = n × 4 × 1.2초. 두 값은 `UEPLootDeveloperSettings`에 둔다(`TMap<EEPItemRarity, float>` — 열거형은 `EPTypes.h:18`, 아이템 등급은 `FEPItemData::Rarity`). 판정 시각은 서버다.
  - **진행:** 상자를 열면 자동으로 순서대로 수색한다(`GetSortedContents` 순). 아이템을 눌러 그것부터 수색할 수도 있다. **한 사람이 동시에 수색하는 것은 언제나 하나**다 — 진행 중에 다른 것을 누르면 진행 중인 것을 **버리고** 바꾼다.
  - **중단:** 닫거나 바꾸면 진행 중이던 아이템의 진행도는 버린다. 끝낸 아이템만 남는다 — 서버가 드는 상태가 *"안다 / 모른다"* 하나다. 다시 열면 끝낸 것은 드러나 있고 나머지만 남는다.
  - **미확인 아이템도 가방에 넣을 수 있다.** 넣어도 정체는 모른다. 가방 안에서도 수색할 수 있다. 배낭 같은 컨테이너 아이템의 내용물은 각각 별개의 아이템이다.
  - **미확인 아이템은 장착 · 사용할 수 없다** — 수색이 먼저다. 검사는 `MoveEntry`(슬롯으로 가는 경로)와 사용 경로에 하나씩 둔다.
  - **미확인 아이템을 버리면** 픽업은 **진짜 메시**로 떨어진다(`ItemId` 복제 — D-026 그대로). 대신 그 아이템을 모르는 사람에게는 상호작용 문구가 `줍기 — ?`이고, 주워도 여전히 수색해야 한다. 아는 사람은 버렸다 주워도 안다(`ItemUid` 기준이라 저절로 그렇다). **메시가 보이므로 이것은 보안이 아니라 게임 규칙이다** — 수색은 *"써도 되는가"* 를 막는다.
  - **미확인 아이템이 보여주는 것은 크기(`SlotSize`)뿐**이다. 칸 계산 · 게이지가 그대로 돈다.
  - **수색 시간으로 희귀도를 짐작할 수 있는 것은 그대로 둔다** — *"오래 걸리네"* 가 긴장 장치다.
- 버린 대안과 이유:
  - **상자 단위 검색(옛 §7-1):** 사용자 기획이 아이템 단위다.
  - **진행도 보존(4초 중 3초 했으면 1초 남김):** 플레이어 × 아이템마다 시간을 저장해야 한다. 사용자가 *"버린다"* 를 골랐다.
  - **`EntryId`로 기억:** 옮기면 바뀐다.
- 근거: 사용자 답(2026-10-08), D-098, D-026.
- 다시 열 조건: 팀원끼리 수색 결과를 공유하는 기획이 오면 아는 집합의 주인을 플레이어에서 팀으로 넓힌다. 버린 미확인 픽업의 `?` 문구를 클라가 판정하는 방법(픽업은 `ItemUid`를 복제하지 않는다)은 §7-1 구현서에서 정한다 — 메시가 이미 공개라 UI 문제다.

### D-110 복제되는 엔트리 리스트는 언제나 **보는 사람 한 명의 것**이다 — 상자는 서버 안에만, 여는 사람마다 사본
- 날짜 · 상태: 2026-10-08 · 채택 (D-101의 상자 · 시체 부분과 D-107의 상자 휴면 부분을 대체한다)
- 맥락: D-109의 미확인 아이템은 **패킷에서도 숨긴다**(사용자 결정 — 상자 속을 치트로 미리 보지 못하게). 그런데 D-101 계획은 상자 리스트 하나를 연 사람들의 그룹에 복제한다. 리스트 하나는 받는 사람 모두에게 같은 내용이라 *"A는 수색했고 B는 안 했다"* 를 담을 수 없다. 숨기려면 가린 상자 리스트 ＋ 사람마다 정체를 보내는 리스트, **복제 경로가 둘**이 된다 — D-101이 *"열 때 Client RPC로 스냅샷"* 을 기각한 이유와 같다.
- 결정:
  - **상자 · 시체의 `UEPInventoryComponent`는 복제하지 않는다.** 진실은 서버에만 있다.
  - **상자를 열면 서버가 연 사람 쪽에 "열람 리스트"를 만든다.** 같은 `FEPInventoryList` · `FEPInventoryEntry`이고 그 사람에게만 간다(`NetGroupOwner` — 플레이어 가방과 같은 조건). **`EntryId`는 상자의 것을 그대로 쓴다** — 요청은 *"상자 X의 3번을 내 가방으로"* 이고 D-101 계획과 형태가 같다. 열람 리스트를 어느 컴포넌트가 드는지는 §7-6 분할 때 정한다.
  - **수명:** 열 때 상자의 현재 내용 ＋ 그 사람의 아는 집합(D-109)으로 **새로 만든다.** 닫으면 **버린다** — 클라에 지우기가 가므로 마지막 내용이 남지 않는다. 닫혀 있는 동안은 맞출 것이 없다.
  - **열려 있는 동안만 서버가 맞춘다.** ① 내 수색이 끝나면 내 열람 리스트의 그 엔트리에 정체를 채운다. ② 상자 리스트가 바뀌면(남이 꺼내 감 · 넣음) 열어 둔 사람 전원의 열람 리스트를 고친다. 상자 리스트를 고치는 함수는 전부 단일 쓰기 지점이므로 **알림 가드(`FScopedInventoryNotify`) 한 곳**에서 한다.
  - **엔트리는 공개 부분과 비공개 부분으로 갈린다.** 공개 = `EntryId` · `ParentEntryId` · `SlotId` · `SortKey` · 크기(`SlotSize`). 비공개 = `ItemId` · `State`. **보는 사람이 그 `ItemUid`를 알 때만 비공개 부분을 채워 보낸다.** 내 가방도 같은 규칙이다 — 보는 사람이 나 하나라 리스트 자체가 내 열람 리스트다. 필드 형태(서버 진실을 복제 안 하는 필드에 두고 복제 필드를 채우는 방식 등)는 §7-1 구현서에서 정한다. 채우는 곳은 단일 쓰기 지점과 공개 함수뿐이다.
  - **지금(03-A · 04) 하는 것은 계약 하나다 — 클라는 엔트리의 `ItemId` · `State`를 직접 읽지 않고 접근자 하나(`GetEntryView`)로 읽는다.** 비공개 부분이 비어 있으면 *"모름"* 으로 돌려준다. §7-1 전까지는 언제나 채워져 있다. 이 계약이 있으면 §7-1에서 엔트리를 가를 때 UI를 고치지 않는다. **03-A의 복제 형태는 지금 바뀌지 않는다.**
- 버린 대안과 이유:
  - **가린 상자 리스트 ＋ 사람마다 정체 리스트:** 복제 경로가 둘이다(맥락).
  - **UI 규칙으로만 숨김(정체는 복제, 클라가 아는 `ItemUid` 목록으로 `?`를 그림):** 경로는 하나지만 상자를 연 치트 사용자가 수색 없이 내용물을 다 읽는다. 사용자가 기각했다. D-098의 *"`ItemUid`를 복제하지 않는다"* 도 뒤집어야 했다.
  - **받는 사람마다 직렬화를 다르게:** FastArray가 지원하지 않는다. Iris로 가면 깨진다.
  - **연 사람마다 상자 쪽에 리스트(그룹 하나에 한 명):** 위 결정을 상자 쪽에 둔 것과 같은데, 사람마다 그룹을 관리해야 하고 상자 액터의 휴면(D-107)이 그대로 남는다.
  - **열려 있는 사람도 다시 열 때만 갱신:** 안전은 서버 검증이 지키지만, 남이 꺼내 간 아이템이 화면에 남고 집으면 실패한다. 같은 상자를 둘이 동시에 뒤지는 것은 이 장르에서 실제로 일어난다. 그리고 ①을 위해 열람 리스트를 고치는 코드가 이미 있어 ②는 그것을 전원에게 돌리는 것뿐이다.
- 대가: 서버가 연 사람 수만큼 사본을 든다(상자는 작고 동시 열람자는 적다). 사본을 맞추는 코드가 가드 한 곳에 생긴다.
- 덤: D-101의 다시 열 조건(*"그룹에서 빠진 연결의 클라에 마지막 내용이 남는다"*)과 D-107의 *"열기 계약에 `FlushNetDormancy`"* 가 함께 사라진다 — 상자 액터는 내용물을 복제하지 않는다.
- 근거: D-101, D-107, D-109, 사용자 답(2026-10-08 — *"상자 안은 패킷에서도 숨긴다"*, *"열어 두고 있는 사람만 서버가 맞춘다"*).
- 다시 열 조건: 한 상자를 동시에 여는 사람이 많아지는 기획(대형 공용 보관함 등)이 오면 사본 비용을 다시 잰다. 관전 · 리플레이가 오면 *"보는 사람 한 명"* 의 정의를 다시 본다.

---

## 2026-10-08 — STATUS "설계 문답" 이관 (`Status/05_Loot_03_Inventory_STATUS.md`에서)

> STATUS를 *"현재 코드 상태 + 남은 작업"* 만 남기도록 정리하면서, 진행 상태가 아니라 **설계 근거**인 문답 셋을 옮겼다. 원문의 결론과 수치는 그대로다. D-111의 "`ItemUid`만 남기기"는 구현 세션 제안으로 이번에 더했다.

### D-111 `EntryId`는 `int32` · 단조 증가 · 재사용 없음 — 그리고 `ItemUid`로 대체하지 않는다
- 날짜 · 상태: 2026-10-08 · 채택 (D-071의 근거 보충. D-098 보충)
- 맥락: 구현 전 문답에서 세 의문이 나왔다 — 증가만 하면 넘치지 않나, 링크드 리스트가 낫지 않나, (10-08) `ItemUid`가 생겼으니 `EntryId`를 없애면 안 되나.
- 결정:
  - **오버플로는 도달 불가 — `int32` 유지.** `NextEntryId`는 서버 전용 컴포넌트 필드라 수명이 매치다. 매치 30분 · 초당 1회면 1,800개(0.0001%), **초당 100회를 쉬지 않고 248일**, 세이브 후 하루 1,000개 누적이면 5,800년. 넓혀야 하면 `ParentEntryId`와 RPC 시그니처만 바꾸는 기계적 변경이다. **단 세이브(로드맵 5단계)에는 `NextEntryId`를 반드시 넣는다** — 빠뜨리면 로드 후 1부터 재발급해 기존 엔트리와 겹치고 `ParentEntryId`가 엉뚱한 컨테이너를 가리킨다. 오버플로보다 이쪽이 현실적인 위험이다.
  - **번호를 재사용하지 않는다 (ABA).** 클라가 `Server_DropItem(5)`를 보낸 사이 서버에서 5번이 제거되고 재활용되어 새 AK가 5번이 되면, **엉뚱한 아이템이 떨어진다.** 단조 증가면 5번은 영영 없는 번호라 요청이 그냥 실패한다. `ParentEntryId`도 같다 — 재활용되면 내용물이 다른 배낭에 매달린다(무증상 · 산발적). *일어나지 않을 일(오버플로)을 막으려고 일어날 일(ABA)을 사지 않는다.*
  - **`ItemUid`만 남기고 `EntryId`를 없애지 않는다.** ① 클라 손잡이로 쓰려면 `ItemUid`를 복제해야 하고 크기가 4배다(`FGuid` 16B vs `int32` 4B — 엔트리 · `ParentEntryId` · RPC 인자 전부). ② **버렸다 주워도 이름이 같다** — 늦게 도착한 요청이 다시 주운 아이템에 적용된다. 위 ABA와 같은 축이고, `EntryId`는 되주우면 새 번호라 막힌다. ③ dupe로 정체가 겹치면 **주소 자체가 모호해진다** — 탐지 대상 데이터를 주소로 쓰는 꼴이다. ④ 백엔드가 ID를 발급하게 되면(D-098 다시 열 조건) 출처가 바뀌는데, 그것이 RPC 손잡이 형식까지 끌고 간다. D-098은 반대 방향(`EntryId`를 전역으로)만 기각했었다.
- 버린 대안과 이유:
  - **`int64`로 선불:** 위 수치상 이유가 없다.
  - **링크드 리스트:** ① 식별 문제가 그대로다 — 포인터는 복제되지 않아 결국 노드마다 ID를 붙인다. ② `FFastArraySerializer`는 아이템 배열이 최상위 `UPROPERTY` `TArray`일 것을 요구한다(`FastArraySerializer.h:721-728`) — 델타 복제를 통째로 잃는다. ③ 노드 재사용(free list)은 위 ABA를 만든다.
  - **배열 인덱스:** 수신 측 순서를 엔진이 보장하지 않는다 — *"the order of the list is not guaranteed to be identical between client and server in all cases"* (`FastArraySerializer.h:54`).
  - **`ItemUid` 단일 식별자:** 위 ①~④.
- 근거: 위 수치와 엔진 줄 번호(옛 STATUS 문답), D-071, D-098.
- 다시 열 조건: 세이브가 계정 단위로 수십억 건을 쌓는 구조가 될 때(그때 `int64`).

### D-112 총은 컨테이너가 아니다 — 링크는 같고 판정 경로가 다르다
- 날짜 · 상태: 2026-10-08 · 채택 (옛 STATUS 문답 이관. D-005 · D-033 보충)
- 맥락: *"총도 배낭처럼 컨테이너를 갖고 부착물을 담는 건가?"*
- 결정: 부착물도 `ParentEntryId`로 총에 매달리지만 **판정이 다르다.**

  | | 배낭 (수납) | 총 (부착) |
  |---|---|---|
  | `SlotId` | `NAME_None` | `Optic` / `Muzzle` / … |
  | 제한 | **칸 수** (`ContainerCapacity`) | **슬롯 이름** (슬롯당 하나) |
  | `GetUsedSlots` | 합산 | `continue` — 아예 안 센다 |
  | `GetCapacity` | DT의 `ContainerCapacity` | **0** |

  `GetCapacity(총) == 0`이라 총에 무엇을 **수납**하는 것은 원천 봉쇄다. 조준경이 들어가는 것은 `CanFit`을 통과해서가 아니라 **칸 검사를 거치지 않기 때문**이다. 부착 깊이는 **1**이다(§7-3).
- 버린 대안과 이유: 원문 문답에 따로 없다.
- 근거: `ParentEntryId` 하나로 배낭 · 부착물 · 상자가 같은 표현이 되는 것이 이 설계의 이득이다. 그래서 `SlotId`를 Step 03에 미리 넣었다 — 없으면 용량 판정식을 §7-3에서 다시 써야 한다.
- 다시 열 조건: 총 안에 무언가를 **수납**하는 기획(예: 탄창 수납칸)이 올 때.

---

## 2026-10-08 — 인벤토리 컴포넌트의 데이터 전용 함수 이동 (구현 세션 질문 → 설계 세션)

### D-113 파생 색인과 원소 조회는 `FEPInventoryList`로, 수치 찾기는 `FEPItemState`로 — 03-A 완료 뒤, 03-B 착수 전
- 날짜 · 상태: 2026-10-08 · 제안 → **채택 (2026-10-08, 사용자 승인)** · **보충 → D-114** (D-100의 *"컴포넌트에 색인을 둔다"* 를 바꾼다. 통째 재구축 · 무효화 넷 · 일괄 경로(D-106)는 그대로다)
- 맥락: 1d로 `UEPInventoryComponent`에 함수가 늘어나는데, 그중 일부는 게임 규칙 없이 `Entries`나 `Stats`만 만진다. Lyra는 추가 · 삭제를 `FLyraInventoryList`에 둔다(`LyraInventoryManagerComponent.cpp:80-123`). 목표 경계: **컴포넌트 = 누가 · 언제 · 허락되는가, 리스트 · 구조체 = 데이터를 어떻게 찾고 바꾸는가.**
- 결정 (묶음은 구현 세션 질문의 ①~⑥):
  - **③ 옮긴다.** `FIndex` · `bIndexDirty` · `EnsureIndex` · `MarkIndexDirty` · `FindEntryPtr` · `FindEntryMutable`을 `FEPInventoryList`의 비복제 멤버로 둔다. 컴포넌트는 `Entries.EnsureIndex()` · `Entries.FindEntryMutable()`로 부른다.
    - `UsedSlotsOf`가 DT의 `SlotSize`를 읽는다. 리스트는 `UEPItemDefinitionSubsystem::Get(Owner)`(`EPItemDefinitionSubsystem.h:21`)로 얻는다. 컴포넌트의 private `Defs()`에 기대지 않는다.
    - **무효화를 리스트가 같이 한다.** 리스트에 구조 변경 함수 둘을 둔다. `Append(Entry)`는 추가 · `MarkItemDirty` · `MarkIndexDirty`를 한다. `RemoveAll(Ids)`는 큰 위치부터 `RemoveAtSwap` · `MarkArrayDirty` · `MarkIndexDirty`를 한다. 그러면 무효화 넷 중 `InsertEntry`(`AppendEntry`) · `RemoveSelf` · 클라 수신(리스트 자신의 `PostReplicatedReceive`) 셋은 **빠뜨릴 문법이 없다.** 손으로 부르는 곳은 `MoveEntry` 하나다.
    - 필드만 바꾸는 쓰기(키 · `Charges` · 수치)는 지금처럼 컴포넌트가 `Entries.MarkItemDirty(*E)`를 부른다. **단일 쓰기 지점 표는 그대로 컴포넌트에 있다.** 가드(`FScopedInventoryNotify`) · 권한 · 발급기(`NextEntryId`)도 컴포넌트다.
  - **② 따라서 없어진다.** `friend struct FEPInventoryList`는 `PostReplicatedReceive`가 `Owner->MarkIndexDirty()`를 부르려고 둔 것이다. 색인이 리스트로 가면 자기 함수가 된다. `friend struct FScopedInventoryNotify`는 남는다.
  - **④ 순수 부분만 옮긴다.** `FEPItemState`에 `FindStat(Tag) const`와 `SetStat(Tag, Value)`를 둔다. `SetStat`은 **바뀌었는가**를 `bool`로 돌려준다. 컴포넌트의 `SetEntryStat` · `GetEntryStat`은 남는다. `SetEntryStat`은 권한 · 가드 · `if (E->State.SetStat(...)) Entries.MarkItemDirty(*E)`를 맡는다. 원시 엔트리를 밖으로 내보내지 않는 구조(03-2)라 통로는 계속 컴포넌트다.
  - **⑤ 옮기지 않는다.** `KeySpace_*` 셋과 `KeyOf`는 함께 다닌다(함정 4q · 4s · 4w). `KeySpace_NextAtEnd`는 재정규화(쓰기)를 부르므로 컴포넌트를 떠날 수 없다. 넷을 갈라 두 곳에 두면 셋을 묶어 둔 이름 규칙이 깨진다.
  - **①과 ⑥은 그대로다.** `AppendEntry`는 가드 · 발급기 · `CheckIdentity`와 짝이라 컴포넌트에 남고, 몸통의 "추가 + dirty"만 리스트의 `Append`다.
  - **시점: 03-A PIE 표가 통과한 뒤, 03-B 코딩 전.** 동작이 같은 이동이라 03-A PIE 표를 그대로 회귀 검사로 쓴다. 03-B(`AddSubtree` 일괄 경로, `_B_PickupDrop.md:172`)가 색인을 직접 읽으므로 그 전에 모양을 굳힌다. 문서는 이 제안이 채택된 뒤에 고친다.
- 판단 근거 (구현 세션 질문 1~5):
  1. **D-100에 컴포넌트에 둔 이유가 없다.** D-100의 근거는 `USceneComponent::AttachChildren`(파생 색인이라는 모양)이지 "컴포넌트여야 한다"가 아니다. 수신 경로는 오히려 단순해진다(위 ②).
  2. **비복제 멤버를 FastArray 구조체에 두는 것은 엔진과 Lyra가 이미 하는 일이다.**
     - 엔진의 `FFastArraySerializer::ItemMap`(`FastArraySerializer.h:416`)은 `UPROPERTY`가 아닌 `ReplicationID → 인덱스` 색인이다. `MarkArrayDirty`가 비우고(`:457-459`), 개수가 다르면 다시 만든다(`:748-770`). D-100과 같은 "무효화 후 통째 재구축"이다.
     - Lyra `FGameplayTagStackContainer::TagToCountMap`(`GameplayTagStack.h:89`)도 FastArray 안의 비복제 조회 맵이다.
     - 복사: 섀도 버퍼를 처음 만들 때 `CopyCompleteValue`가 구조체를 통째로 복사한다(`RepLayout.cpp:7371-7384`). 이후 비교 · 복사 루프는 커스텀 델타 프로퍼티를 건너뛴다(`:5052`, `:5214`). 통째 복사는 `Items`와 색인을 **함께** 옮기므로 사본 안에서도 둘이 맞는다. 사본은 조회되지 않는다. 낡는 경우는 `Items`만 바뀌고 무효화가 빠질 때뿐인데, 위 `Append` · `RemoveAll`이 그 자리를 줄인다.
  3. **dirty 책임:** 위 결정 ③의 마지막 두 항목. 전보다 손으로 부르는 자리가 줄고(무효화 넷 → 하나), `MarkItemDirty` 자리는 그대로다.
  4. **`FEPItemState` 멤버 함수:** 지금 `EPTypes.h`에 메서드를 가진 구조체는 없지만 막는 관례도 없다. Lyra가 같은 문제(태그 → 정수)를 `FGameplayTagStackContainer::AddStack` · `GetStackCount` 멤버로 푼다(`GameplayTagStack.h:55-67`). D-099가 `StatTags`를 근거로 든 그 구조체다. 자유 함수보다 찾기 쉽다.
  5. **비용:**
     - `_A_Core.md`: 03-1 리스트 블록 · 03-2 헤더 블록 · 03-A-부록 "파생 색인" · "내부 접근 둘" · 03-7 수신 콜백 · `SetEntryStat` 본문, 그리고 `EnsureIndex()` 호출부(관련 이름 약 60곳 중 대부분이 기계적 치환).
     - `_B_PickupDrop.md`: 2곳(`:13`, `:172`).
     - 가이드: §2 "바꾼다" 표 한 줄(`:44`). 불변식 29~36은 클래스 이름을 쓰지 않아 **안 바뀐다.**
     - PIE 표도 **안 바뀐다.** 바뀌지 않는 것이 회귀 검사다.
     - 함정 표 1 · 11 · 11d는 문장의 주어만 바뀐다.
- 버린 대안과 이유:
  - **지금(03-A 도중) 옮기기:** 구현 세션이 1e~1g를 정답지와 대조할 수 없다. 사용자 측 입장과 같다.
  - **⑤까지 옮기기:** 위 ⑤.
  - **쓰기 함수(`SetEntryCharges` 등)까지 리스트로:** 권한 · 가드 · 알림이 컴포넌트에 있어 쓰기 지점이 두 층이 된다. 단일 쓰기 지점 표가 갈린다.
  - **안 옮기기:** 줄어드는 양은 크지 않다(색인 재구축 본문과 선언 약 60줄). 이득은 줄 수보다 ② · 무효화 자동화다. 그래서 "옮긴다"이되 범위를 ③ · ④로 좁힌다.
- 근거: 위 판단 근거 2 · 4의 줄 번호(UE 5.7, Lyra 로컬 소스 직독)
- 다시 열 조건: 없음. 실행은 03-A PIE 표 통과 직후, 03-B 코딩 전이다(`Status/05_Loot_03_Inventory_STATUS.md` 남은 작업 4a). 구현서 · 가이드는 그 시점에 설계 세션이 고친다.

### D-114 D-113 보충 — 번호 발급기 `NextEntryId`도 리스트로
- 날짜 · 상태: 2026-10-08 · 채택 (사용자 승인. D-113의 *"발급기(`NextEntryId`)도 컴포넌트다"* 를 바꾼다)
- 맥락: D-113 채택 뒤 Lyra와 대조했다. Lyra는 추가 · 삭제를 리스트에 두고(`LyraInventoryManagerComponent.cpp:80-126`) 조회는 컴포넌트에 둔다(`:204-262`) — D-113과 같은 경계다. 남은 차이 가운데 옮길 수 있는 것이 발급기 하나였다.
- 결정:
  - `NextEntryId`를 `FEPInventoryList`의 비복제 멤버로 옮긴다. **리스트 `Append`가 번호를 붙이고 돌려준다.**
  - 컴포넌트 `AppendEntry`는 가드 · `Entries.Append(...)`만 한다. `AddSubtree`의 옛 번호 → 새 번호 매핑은 그 반환값을 그대로 쓴다.
  - 발급 규칙은 그대로다(D-111) — `int32`, 증가만 하고 재사용하지 않는다. 서버만 부른다.
- 버린 대안과 이유:
  - **알림 가드(`FScopedInventoryNotify`)도 리스트로:** 가드는 컴포넌트의 `NotifyDepth` · `OnInventoryChanged`에 묶여 있고, *"수신 한 번에 알림 한 번"*(03-7)을 서버에서 지키는 장치다. Lyra는 항목마다 메시지를 보내 이 장치가 없다.
  - **권한 확인도 리스트로:** Lyra는 `check(HasAuthority())`로 크래시시킨다. 우리 쓰기 지점은 조용히 거절한다.
- 근거: 엔진도 발급 카운터를 직렬화 구조체 안에 둔다 — `FFastArraySerializer::IDCounter`(`FastArraySerializer.h:420`, `NotReplicated`).
- 다시 열 조건: 없음. 실행은 D-113과 같이 한다(STATUS 남은 작업 4a).

## 차수별 경위 (이관, `Status/05_Loot_STATUS.md` "설계 변경 이력"에서 원문 그대로)

> **설계 변경 이력 (2026-07-26).** 검증 기록은 전부 `Review/` 아래에 있다.
>
> | 차수 | 결정 | 기록 |
> |---|---|---|
> | 1차 | 개체 상태를 `UObject` → `USTRUCT` | `Review/05_Loot_REVIEW_StructMigration_Summary.md` |
> | 2차 | **스택 폐지 + 칸 수 합산**(타르코프식), `bFungible` 합치기, 불변식을 코드로 | `Review/05_Loot_REVIEW02_StackRemoval_*.md` |
> | 중간 | **배낭(중첩 컨테이너)** 확정 — `ParentEntryId`가 Step 03으로 앞당겨짐 | 사용자 기획 |
> | 3차 | **실무성 검수** — 버리기 잔탄 소실 등 설계 결함 3건, 미선언 심볼 9개, 문서 중복 9건 | `Review/05_Loot_REVIEW03_Practicality_*.md` |
> | 4차 | 3차 반영 확인 + 판단 5건 | `Review/05_Loot_REVIEW04_Followup3_*.md` |
> | 5차 | **Step 01 단독 검수 (Step 00 구현 후 첫 검수).** 콜리전 기본값 `BlockAll`, 완료 조건 6 오류, `SpawnLoot()` 본문 부재, 롤 반환 규약 구멍 | `Review/05_Loot_REVIEW05_Spawner_*.md` |
> | 6차 | **전역 데이터 참조 위치 (Lyra 소스 직독).** `UDeveloperSettings` 유지 확정, config `TSubclassOf` → `TSoftClassPtr`, `.ini` 리네임 근거 정정 | `Review/05_Loot_REVIEW06_GlobalDataRef_*.md` |
> | 7차 | **Step 02를 GAS로 확정 (2026-08-02).** `Server_Interact` 직접 RPC 폐기 → `UEPGA_Interact` + `FGameplayEventData::Target`. 틱 훅을 `NotifyControllerChanged()` 하나로. 상호작용 채널 `GameTraceChannel2`→`3`(`Projectile` 충돌). Lyra `Interaction/` 모듈은 **가져오지 않는다** | `Review/05_Loot_REVIEW07_Interaction_*.md` |
> | 8차 | **Step 03 검수 (2026-08-04) — 드랍은 RPC로 확정.** 7차의 *"입력 진입점은 어빌리티 하나다"* 를 **"월드 상호작용은 어빌리티 / 서버가 소유한 상태 변경은 RPC"** 로 대체. 항목 콜백 3종은 **링크 에러**라 선언 금지. `bIsRoot` private 분리, `EquippedEntryId` 대칭, `FindFungibleEntryId`에 컨테이너 인자, `DropCooldownEndTime` 복제. **문서 반영 완료** | `Review/05_Loot_REVIEW08_InventoryGAS_*.md` |
> | 9차 | **슬롯 12개 기획 확대 검수 (2026-08-22) — `SlotId`가 장착의 유일한 진실로 확정.** 언턴드식 12슬롯(핫바 4 + 착용 8) + 부착 4. `EquippedEntryId`/`EquippedBackpackEntryId` **필드 삭제**, `ActiveHotbarIndex` 하나만 남김. `MoveEntry`/`GetEntryInSlot(Parent, SlotId)`/`SlotPriority`/`BodySlots` 신설. `Server_MoveEntry`·`HotbarRefs`는 Step 04로. **문서 반영 완료** | `Review/05_Loot_REVIEW09_SlotExpansion_*.md` |
> | 10차 | **Step 04 격자 UI 검수 (2026-08-23) — Step 04를 04-A(표시)/04-B(드래그)로 분할.** 순서를 클라 로컬에 두는 판단은 **유지**되되 지속을 **`ULocalPlayerSaveGame`** 으로(받아들이기로 했던 대가 둘이 불필요했다). `SwapEntries` 용량식을 `SlotId.IsNone()` 조건부 델타로 교정, 드롭을 **칸이 받고 버블링으로 패널이 판정**(`HitTestCell` 폐기), `FEPCellHit`, *"같은 컨테이너면 조건 없이 로컬"*, 빈 칸 N개 → **`＋ 남은 용량` 한 덩어리**, `UDynamicEntryBox(Wrap)` 풀링, `MoveEntry` 검사 0(제자리 거절), `GetInsertionOrder()`의 본체 선두. **문서 반영 완료** | `Review/05_Loot_REVIEW10_GridUI_*.md` |
> | **11차** | **아이템 순서를 서버로 (2026-08-23) — 10차 결론을 사용자 판단으로 뒤집음.** *"저장했을 때도 순서를 유지하는 것이 맞다."* `FEPInventoryEntry::SortKey`(`int32` 희소, `Step=1<<16`), `AssignSortKey` 단일 쓰기 지점, `Server_ReorderEntry(EntryId, PrevEntryId)`(인덱스 아님), `GetSortedContents` 클라·서버 공용, `RenormalizeSortKeys`. **`RemoveEntryInternal` ②의 `bIsRoot`가 두 필드를 관장** → 배낭 되줍기에 내용물 순서가 산다. `ULocalPlayerSaveGame` 계열 **전부 삭제**. **문서 반영 완료** | 검수 아님 — 설계 결정 |
> 
> | **11차 검수** | **11차 설계를 검수 (2026-08-23) — 10차 답변이 §5를 스스로 철회했다.** *"저장소를 검증하고 키를 검증하지 않았다."* 새 결함 둘을 잡았다: **① `SortKey` 동률**(키 공간을 `GetSortedContents`로 구해서 — 정상 플레이로 재현) **② 재정규화가 이분 고갈에만 걸림**(맨 앞/맨 뒤가 무한 증감, 오버플로 후 영구적). `Server_ReorderEntry` RPC를 **04-B로**(9차 규칙), `Owner`를 `TObjectPtr<UActorComponent>`로(Lyra), 03-D 신설은 기각. **문서 반영 완료** | `Review/05_Loot_REVIEW11_ItemOrderServer_*.md` |
> 
> | **12차 검수** | **12차 요청을 검수 (2026-08-23) — ★ 무한 재귀를 잡았다.** 재정규화 가드에서 `PrevEntryId != INDEX_NONE`이 빠져 **맨 앞 드래그가 서버를 죽인다**(함정 4t). 제자리 드롭 조기 반환(4u). `KeySpace_` 접두어. **§2 의미론은 A 확정** — B는 같은 동률이 재현되고 요청서 §2-2의 반례는 **앞 이웃 API에서 표현 불가능**했다. **★ *"UE에 선례가 없다"* 가 거짓** — `FUIFrameworkStackBoxSlot::Index`. **문서 반영 완료** | `Review/05_Loot_REVIEW12_ReorderGap_*.md` |
> | **13차 검수** | **Step 03 착수 전 자체 검수 (2026-08-25) — 결함 19건.** `MoveEntry`가 **`FindEntry` 복사본에 쓰고 있었다**(무동작인데 `true`), 루트 스냅샷이 **`SlotId`를 안 버려** `AddSubtree`가 검사 3·4를 우회, **자동 착용이 `CanFit`을 지나** *"등이 비었는데 못 맨다"*, `PostReplicatedReceive` **컴파일 불가**(11차 타입 변경 미반영), `SortKey`의 `INDEX_NONE` 센티널이 **키 −1과 충돌**, `InsertEntry`가 자기를 키 공간에 포함, **`MoveEntry`를 부르는 커맨드가 0개**(Step 04까지 안 돎). 사용자 결정 4건: **`AddSubtree(Parent, SlotId, In)`**(기본값 금지) · **루트가 `SlotId`도 버린다** · **가드를 "단일 쓰기 지점 다섯"에**(사설 래퍼 기각) · **본체 0칸 + 용량표 확정**. `AddSubtree`·`TryAutoEquip`·`StartingEquipment`가 **03-C로 내려갔다**. **문서 반영 완료**(03·DOCS·GAME·STATUS) | `Review/05_Loot_REVIEW_Inventory_Summary.md` ＋ `05_Loot_REVIEW13_SelfReview21_Request.md` |
> | **14차 (사용자 지적)** | **`Server_EquipBackpack` 삭제 (2026-08-25).** *"어차피 옷도 하의도 컨테이너인데 배낭 전용 RPC를 만들 이유가 있었나"* — 셋 다 맞았다. ① 9차의 *"좁은 RPC가 넓은 RPC보다 낫다"* 는 **04-B가 `Server_MoveEntry`를 여는 이상 성립하지 않는다**(넓은 문이 열린 뒤의 좁은 문은 표면을 안 줄인다) ② **13차가 옮긴 04-A에도 호출자가 0개** — 근거였던 `EP.Inv.Equip`이 **콘솔 커맨드**라 규칙대로면 내부 함수를 직접 부른다 ③ **배낭이 특별할 근거가 없다** — `SlotPriority`＋`BodySlots`가 상의·하의·외투·배낭을 같은 모양으로 만들고, `TryAutoEquip`에 이미 적용한 규칙이 RPC 이름에만 안 적용돼 있었다. **`EP.Inv.Equip`(04-A)도 `EP.Inv.Move`의 별칭이라 폐기.** 착용의 클라 표면은 **04-B `Server_MoveEntry` 하나** | `Mine/EquipmentSlots.md` §15 |
> | **13차 답변** | **자체 검수 21건과 결정 5건을 검수 (2026-08-25) — 판정은 전부 유지, 근거 셋이 교체됐다.** 새 결함 **12건**: **★ `MoveEntry`도 키를 재부모 뒤에 구한다**(`InsertEntry`와 같은 결함), **`FScopedInventoryNotify` 정의가 소스에 없다**(첫 빌드 컴파일 에러), **`AddSubtree`에 슬롯 검증이 0** → `CanPlaceInSlot` 추출, **`SlotSize ≥ 1` 검증 없음**(본체 0칸에서 `0+0<=0`), **`EP.Inv.Add`에 컨테이너 인자 없음**(0칸 전환이 03-A를 죽인다), **STATUS 스냅샷이 소스보다 뒤**(결함 #1·#3·#4는 이미 고쳐져 있었다), **DOCS §4-7 획득 절차가 `TryAutoEquip`을 모른다**. **★★ 옛 03-B(배낭) 구간 삭제** — `Server_EquipBackpack`이 Step 03에 호출자 0개. **★ 근거 교정 셋**: 래퍼 기각의 *"엔진 관례와 싸운다"* 는 **거짓**(`MarkAbilitySpecDirty`가 감싼다) / `Cap == SlotSize`는 **익스플로잇이 아니다**(비용 0 ＋ 세 전제) / `StartingEquipment`는 **6차와 같은 자리가 아니다**(→ 미정 #10). **답변의 수치 하나는 틀려서 안 따랐다**(32,763 → 실제 **32,764**). **문서 반영 완료** | `Review/05_Loot_REVIEW13_SelfReview21_Answer.md` |
> | **15차 (파일 분할)** | **`05_Loot_03_Inventory.md`(2692줄)를 03-A / 03-B로 쪼갰다 (2026-08-26).** 8차의 *"파일을 쪼개지 않는다"* 를 뒤집는다 — 근거였던 *"`RemoveEntry`가 경계에서 갈린다"* 가 두 번 무너졌다(13차가 가운데 구간 삭제 / 제거 경로 셋이 전부 03-B). **★ 직접적 계기: 열 함수의 본문이 통째로 빠진 것을 검수 여덟 번 동안 아무도 못 봤다** — `FindEntry`·`ContainsEntry`·`RemoveSelf`·`AssignSortKey`·`KeySpace_Min`·`KeySpace_NextAbove`·`KeyOf`·`GetEntryInSlot`·`FindFungibleEntryId`·`GetEquippedEntryId`. **03-A-부록 신설**로 열 개 본문을 썼다. 함정표는 대응 열로 라우팅(A 44 / B 16, 겹치는 4는 양쪽), **완료 조건 번호 1~20 유지**. 옛 파일은 **리다이렉트 스텁**으로 남긴다(다른 문서에 이름 참조가 142곳). `Review/` 하위는 그 시점의 기록이라 손대지 않았다 | `05_Loot_03_Inventory_A_Core.md` · `05_Loot_03_Inventory_B_PickupDrop.md` |
> | **15차** | **03의 13·14차를 04·05에 대조 (2026-08-26) — 검수가 아니라 정합 작업이다.** 04에서 **하드 결함 둘**: ① **`SwapEntries`가 `CanPlaceInSlot`을 그대로 부르면 슬롯 교환이 전부 거절된다** — 검사 4(*"그 자리가 비었나"*)가 교환에서 언제나 거짓이다. `IgnoreEntryId` 네 번째 인자를 **04-B에서** 붙인다 ② **`SortKey`를 안 맞바꿔 함정 4x가 교환 경로로 되살아난다** — 재발급이 아니라 `Swap` 셋이면 끝(상대의 유효한 키를 물려받는다). 05에서 **하드 결함 둘**: ③ **`Server_Equip(EntryId)`가 배정과 활성을 뭉갰다** — 13차의 검사 0 때문에 **정상 경로에서 `false`가 나 숫자키가 안 먹는다.** `Server_SetActiveHotbarIndex(int32)`로 가름(Lyra `SetActiveSlotIndex` 직독, 배정 `AddItemToSlot`은 RPC가 아니다) ④ **`DefaultLoadout`이 0칸 본체에 넣고 있었다** → `GetInsertionOrder()` 순회 ＋ `StartingEquipment` → `DefaultLoadout` 순서를 계약으로. 함정 신설 04에 2건(11e·11f) · 05에 4건(12·12b·13·13b). **문서 반영 완료**(04·05·03) | `05_Loot_04_InventoryUI.md` · `05_Loot_05_Equipment.md` 변경 이력 |
> | **장비 효과 · 컴포넌트 분할** | **실무 표준 대비 기획 정리 (2026-08-26) — 검수가 아니라 확장점 명명이다.** *"인벤토리에 ASC를 쓰면 더 좋지 않나"* 라는 질문에서 시작했다. **답은 "저장은 FastArray, 효과만 GAS"** 이고 Lyra가 그 형태다(`ULyraInventoryManagerComponent : UActorComponent`). **어트리뷰트로는 아이템을 표현할 수 없다** — 스칼라가 `float` 하나이고 개수가 컴파일 타임 고정이다. 실무에 가까워지는 둘을 §7에 이름으로 남겼다. **§7-5 장비 효과** — 방어구가 오면 `AbilitySetsToGrant` ＋ `GiveToAbilitySystem`/`TakeFromAbilitySystem`(`LyraEquipmentDefinition.h:49-51` · `LyraEquipmentManagerComponent.cpp:91-118`). **훅이 `MoveEntry` 한 곳**인 것이 9차 결정의 두 번째 배당금이고, **핸들은 엔트리에 넣지 않는다**(서버 전용인데 엔트리는 복제된다). **§7-6 컴포넌트 3분할** — Lyra는 소유자로 나눈다(`UActorComponent`/`UPawnComponent`/`UControllerComponent`). **우리는 `SlotId`가 엔트리에 있어 데이터가 안 움직인다** — 함수만 옮긴다. **트리거는 §7-1**(상자는 *"가진 것"* 만 있고 *"입은 것"* 이 없는 첫 소유자). **둘 다 지금 만들지 않는다** — 소비자 0개 | `05_Loot_Design.md` §7-5 · §7-6 |
> 
> | **탄창 기획** | **미정 #1의 ⓐ(탄창 아이템화) 구현 기획을 문서화 (2026-08-24) — 검수가 아니라 설계 정리다.** *"15발 남은 탄창은 어디로 가나"* 라는 질문에서 시작했다. **재장전 = `SwapEntries` 한 줄**로 떨어져 *"빈 탄창을 어디로 보내나"* 와 칸 검사가 동시에 사라진다(총량이 안 변한다). 신규 API는 `FindReloadMag` 하나. **`AmmoType`을 둘로 갈랐다** — 규격 `Caliber`는 Definition(정적), 탄종 `LoadedAmmoId`는 `FEPItemState`이되 **미정 #6과 함께 온다**(지금은 소비자 없음). **★ `05_Loot_05_Equipment.md:73`의 *"`GetMaxAmmo()`가 부착물 준비의 전부"* 를 정정** — 그건 ⓑ의 답이고 ⓐ의 `MaxAmmo`는 **탄창 엔트리**라 읽는 주체가 `UEPCombatComponent`다. **Step 05에 미리 넣을 것 셋**(`WriteBackAmmo()` 추출 · `UEPEquipSlotWidget::ParentEntryId` · `InjectAmmoFromMag`). **문서 반영 완료** | `05_Loot_Design.md` §7-4 |
> 
> **★ 시점은 "Step 05 직후"로 못 박았다 — 기술적 비용이 아니라 검증 순서 때문이다.** 자료구조 변경은 **0**이고(§7-3이 이미 그렇게 설계했다) 바뀌는 것은 대부분 인자뿐이라, 사용자가 *"지금 가면 뭐가 많이 바뀌나"* 를 물었을 때 답은 *"거의 없다"* 였다. 그럼에도 미룬 이유는 ⓐ 작업의 대부분이 **"이미 도는 코드를 한 칸 옮기는 것"** 이라서다 — `Entry.State.Charges ↔ Ammo` 왕복이 ⓑ로 한 번 검증되지 않은 채 옮기면 *"탄창 자식 엔트리의 잔탄이 이상하다"* 에서 **주입이 틀린 건지 탄창 참조가 틀린 건지 가를 방법이 없다.** 지금 `RemoveSelf`조차 빈 스텁이다(`EPInventoryComponent.cpp:105`).
> 
> **부수 효과 — 11차 검수가 남긴 지적 하나가 닫힌다.** *"순서를 서버로 옮겼는데 서버 소비자가 사실상 하나뿐"* 이었는데, `FindReloadMag`가 `GetSortedContents()`로 탄창을 고르면서 **사용자가 드래그로 정렬한 순서가 곧 재장전 우선순위**가 된다. 정책 필드 0개. `SwapEntries`도 `SortKey` 교환 계약(04-7)의 **두 번째 소비자**를 얻는다.
> 
> | **컨테이너 중첩** | **배낭 안 배낭을 유한하게 막았다 (2026-08-24) — 사용자 결정.** *"지금 가방 안에 가방이 되나"* 라는 질문에서 시작했다. **되고 있었다** — 검사 6은 사이클만 막고 `CanFit`은 종류를 안 본다. **결정: 모든 컨테이너가 `ContainerCapacity < SlotSize`** ⇒ 깊어질수록 `SlotSize`가 작아져 유한. **런타임 코드 0줄**(`MoveEntry` 검사 8번을 안 붙인다). DT 컬럼이라 규칙만으로는 새 행에서 조용히 깨지므로 **`IsDataValid()`에 한 줄**(Step 00). **★ 그 규칙이 벗기와 충돌한다** — 용량을 키우려면 `SlotSize`를 키워야 하는데 본체로 벗으면 `CanFit`에 걸려 **배낭 상한 9칸**이 된다 ⇒ **벗기를 `Server_DropItem`으로 확정.** **문서 반영 완료** | `05_Loot_Design.md` §4-6 · `05_Loot_00_ItemCore.md` · `05_Loot_03_Inventory.md` 03-6 |
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
