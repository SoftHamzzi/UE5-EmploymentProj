# Loot 단계 전체 진행 상황

> 세션 시작 시 이 파일을 먼저 읽을 것.
> 현재 Step 확인 후 해당 Step의 STATUS 파일을 추가로 읽는다.
> 마스터 기획: `05_Loot_DOCS.md`

---

## 진행 상황

- [ ] 05_Loot_00 ItemCore (아이템 계층 정비 + `FEPItemState` + Definition 서브시스템) ← **현재**
- [ ] 05_Loot_01 Spawner (루트테이블 + 스포너 + 픽업)
- [ ] 05_Loot_02 Interaction (IEPInteractable + 상호작용 컴포넌트 + 서버 검증)
- [ ] 05_Loot_03 Inventory ← **가장 큰 단계.** 완료 조건 13개로 다른 단계 두 개 분량이라 셋으로 나눠 진행한다
  - [ ] **03-A 코어** (03-1·2·3·9) — 칸 합산 / `bFungible` / `COND_OwnerOnly`. `RemoveEntry` 없이 단독 실행됨
  - [ ] **03-B 배낭** (03-6 + `GetCapacity`) — 자동 착용 + 독립 풀
  - [ ] **03-C 버리기** (03-4·5·7) — `RemoveEntry` / `AddSubtree` / 캐스케이드. **함정표 ★★ 4건 중 3건이 여기**
- [ ] 05_Loot_04 InventoryUI (아이템 목록 + 칸 수 게이지)
- [ ] 05_Loot_05 Equipment (무기 장착 흐름 이관 + 탄약 소유권 정리)

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
| **배낭** | 줍고 **빈 슬롯이면 자동 착용.** `EquippedBackpackEntryId` 별도 필드(TMap 아님). 교체는 벗고 다시 줍기 |
| **서브트리 삽입** | `AddSubtree()` + **`EntryId` 재매핑.** 없으면 배낭을 버렸다 주울 때 내용물이 증발한다 |
| **갱신 알림** | `FEPInventoryList::PostReplicatedReceive` **하나** (수신 1회당 1회). 항목별 콜백 3종을 쓰지 않는다 + 서버는 스코프 가드 |
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
| 장비 슬롯 | 별도 배열 없이 `EquippedEntryId` / `EquippedBackpackEntryId` **필드 둘** — 핸들 이중화 불필요. 슬롯이 셋이 되면 그때 `TMap` |
| 인벤토리 복제 조건 | **`COND_OwnerOnly`** — 남의 가방이 보이면 치트 + 대역폭 |
| 식별자 | **`FEPInventoryEntry::EntryId`** (int32, 서버 발급, 단조 증가, **재번호 없음**). `FGuid InstanceId` / `SchemaVersion`은 **제거** |
| 상태 초기화 | **Definition의 virtual `InitState(const FEPItemData&, FEPItemState&)`**. static 팩토리 2종 제거 |
| **DT vs DA 배치 원칙** | **① 표로 나란히 조정하는 값은 DT** (`SlotSize`/`SellPrice`/`bFungible`/`InitialCharges`/`ContainerCapacity`) **② 에셋 참조·virtual·타입 전용은 DA** (`WorldMesh`/`Icon`/`InitState`/`MaxAmmo`). 판정선 = "모든 아이템이 값을 갖는가" |
| Definition 보유 대상 | **모든 아이템.** 예외 없음 (`WorldMesh` + `Icon` + `InitState`가 전부 필요) |
| 픽업 복제 | `DORM_Initial` + Tick off. **복제 상태가 불변이라 `FlushNetDormancy()` 불필요** |
| `EmptyWeight` | **루트 테이블에서만 유효** — 하위 적용 시 등급 확률이 침식됨 |
| Definition 로딩 | **매치 전 전량 상주**(AssetManager). 소프트는 `WorldMesh`/`Icon`/`WeaponMesh`만 |
| 무기 액터 스폰 책임 | **`UEPCombatComponent`** 유지 — 인벤토리는 `EquipFromInventory(EntryId)`로 넘김 |
| 드랍 / 장착 RPC 파라미터 | **`EntryId`** |
| `MarkItemDirty` | **`AddEntryCharges()` 안에서 보장.** 원시 엔트리를 밖으로 내보내지 않으므로 호출자가 잊을 방법이 없다 |
| 권한 검사 | **early return** (`if (!GetOwner()->HasAuthority()) return;`). `check()`는 Shipping에서도 크래시하고 프로젝트 관례와 다르다 |
| DT 캐시 | `FEPItemData` **값 복사**. 행 포인터 캐시 금지(리임포트 시 댕글링) |
| `UEPLootTable` 로딩 | `EPLootTable` PrimaryAssetType 등록 — `EP.Loot.RollTable`이 이름으로 찾으려면 필요 |
| 서버 상호작용 검증 | `Server_Interact`에서 **거리 재검증 + `CanInteract()` 재호출**. 없으면 `DropCooldown`이 서버에서 무시됨 |
| 액터 복제 설정 API | `SetNetCullDistanceSquared()` / `SetNetUpdateFrequency()` — 5.5부터 필드 직접 대입 deprecated |
| `FInstancedStruct` 전환 기준 | **세 번째 아이템 카테고리가 자기 전용 필드를 요구할 때.** 지금 쓰면 프로퍼티 델타를 잃는다 |

### 기존 코드에서 반드시 손대야 할 것

| 위치 | 조치 | 단계 |
|---|---|---|
| `UEPItemInstance` / `UEPWeaponInstance` **클래스 파일 전체** | **삭제.** `FEPItemState`(USTRUCT)로 대체. 호출처 0이라 비용 없음 | Step 00 |
| `UEPItemInstance::InstanceId`(FGuid) / `SchemaVersion` | 위 삭제에 포함. 각각 "읽는 코드 없음" / "세이브 포맷의 속성" | Step 00 |
| `UEPItemDefinition` | `virtual InitState(const FEPItemData&, FEPItemState&)` + `GrantedAbility` + `IsDataValid()` 추가 | Step 00 |
| `UEPWeaponDefinition::MaxAmmo` (`uint8`) | `int32`로 변경 — `Charges`/어트리뷰트와 캐스팅 정리 | Step 00 |
| `UEPWeaponDefinition::GetPrimaryAssetId()` | **오버라이드 제거.** 지금 `"WeaponDef"`를 반환해 상위(`"ItemDef"`)와 타입이 갈린다 → 한 타입만 등록하면 무기 Definition이 로드 안 됨 | Step 00 |
| `FEPItemData` | **신규 3필드: `bFungible` / `InitialCharges` / `ContainerCapacity`** (전부 DT — 배치 원칙 참조) | Step 00 |
| `DT_Items.uasset` | 비무기 6행 추가(`AmmoBox_545`/`Bandage`/`Scrap_Paper`/`Resume`/**`Cash_10000`**/**`Backpack_Small`**) + **각각 Definition 에셋**. 현재 3행이 전부 무기라 기본 `InitState()`·합치기·컨테이너 경로를 검증할 대상이 없다. 무기 `SlotSize`를 4~5로 올린다. **`SellPrice`를 반드시 채운다** — 기본값 100이라 `Cash_10000`이 100원에 팔린다 | Step 00 |
| `DOCS/GAME.md` | **인벤토리 절 전면 개정 완료** — 6슬롯 → 칸 합산(본체 10칸) + 배낭 + 스택 없음 + 내구도 신설. 결정 A는 구현 결정이 아니라 **기획 변경**이었다 | (완료) |
| Project Settings → Asset Manager | `ItemDef` PrimaryAssetType 등록 (`AssetBaseClass = EPItemDefinition`, 전량 상주용). 현재 `Map`/`PrimaryAssetLabel`만 등록돼 있음 | Step 00 |
| Project Settings → Asset Manager | `EPLootTable` PrimaryAssetType 등록 (`RollTable` 이름 조회용) | Step 01 |
| Project Settings → Collision | `EP_TraceChannel_Interact` 신규 채널. `ECC_Visibility` 재사용하면 픽업 앞 잡동사니가 상호작용을 막는다 | Step 02 |
| `AEPCharacter` 생성자 | `InteractionComponent`(Step 02) / `InventoryComponent`(Step 03) 추가 + 게터. `CombatComponent`/`RewindComponent` 옆 | Step 02·03 |
| `AEPPlayerController` | `InteractAction`(**F**) / `ToggleInventoryAction`(Tab) UPROPERTY + 게터 — 기존 Dash/Heal/Shield 패턴 | Step 02·04 |
| `EPCombatComponent.cpp:177` `InitAmmo(MaxAmmo)` | **제거.** 버리기가 들어오면 12/30 무기를 버렸다 줍기만 해도 30/30이 되는 익스플로잇 | Step 05 |
| `EPCombatComponent` `Init*` → `Set*` | `Init*`은 어트리뷰트 델리게이트를 안 쏜다 → 장착해도 HUD 탄약이 안 바뀜. **`MaxAmmo`를 `Ammo`보다 먼저** 세팅(`PreAttributeChange`가 `[0, MaxAmmo]`로 클램프) | Step 05 |
| `UEPCombatComponent::UnequipWeapon()` | 잔탄 write-back(`AddEntryCharges`). **교체·버리기·사망 세 경로가 전부 여기를 거치게** 만들어 한 곳에만 둔다. null 가드 필수 | Step 05 |
| `UEPInventoryComponent::RemoveEntry()` | **제거된 서브트리를 반환한다.** 장착 검사(자식 포함)·write-back·캐스케이드를 내부에서 보장 — 호출자가 지킬 순서가 없다 | **Step 03** |
| Project Settings → Asset Manager | 두 타입 모두 **`Is Editor Only = false`**. 기존 `Map`/`PrimaryAssetLabel`이 `True`라 따라 하면 **패키지 빌드에서만** 리스트가 빈다 | Step 00·01 |
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
| 상호작용 / 픽업 / 인벤토리 | 클래스 자체가 없음 |

> **설계 변경 이력 (2026-07-26).** 검증 기록은 전부 `Review/` 아래에 있다.
>
> | 차수 | 결정 | 기록 |
> |---|---|---|
> | 1차 | 개체 상태를 `UObject` → `USTRUCT` | `Review/05_Loot_REVIEW_StructMigration.md` |
> | 2차 | **스택 폐지 + 칸 수 합산**(타르코프식), `bFungible` 합치기, 불변식을 코드로 | `Review/05_Loot_REVIEW2_*.md` |
> | 중간 | **배낭(중첩 컨테이너)** 확정 — `ParentEntryId`가 Step 03으로 앞당겨짐 | 사용자 기획 |
> | 3차 | **실무성 검수** — 버리기 잔탄 소실 등 설계 결함 3건, 미선언 심볼 9개, 문서 중복 9건 | `Review/05_Loot_REVIEW3_*.md` |
>
> **3차에서 문서 구조를 바꿨다.** stale 참조가 세 번 연속 남은 원인이 "사실 하나가 3~4곳에 적혀 있고 매번 하나가 남는 것"이었다. **마스터 §4는 결정과 근거만, Step 문서가 코드·함정표·엔진 인용을 단독으로 갖는다.** 1007 → 839줄이고 고칠 곳이 절반이다.

무기는 `EPGameMode.cpp:81`에서 `DefaultWeaponClass`를 직접 스폰. 이 흐름의 인벤토리 이관은 **Step 05 범위 안**이다 (`05_Loot_DOCS.md` §4-8) — `DefaultLoadout : TArray<FName>` 기반 지급으로 교체하고, 무기 액터 스폰 책임은 `UEPCombatComponent`에 남긴다.

---

## 세션 시작 템플릿

```
@LOOT_STATUS.md @05_Loot_0X_XXX_STATUS.md
Step X 진행.
```
