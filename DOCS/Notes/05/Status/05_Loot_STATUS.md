# Loot 단계 전체 진행 상황

> 세션 시작 시 이 파일을 먼저 읽을 것.
> 현재 Step 확인 후 해당 Step의 STATUS 파일을 추가로 읽는다.
> 마스터 기획: `05_Loot_Design.md`

---

## 인수인계

| 항목 | 내용 |
|---|---|
| 갱신 | 2026-10-02, developer 세션(역할별 세션 도입 작업, `DOCS/SETUP_SessionRoles.md`) |
| 지금 단계 | **Step 03-A — 코드는 거의 다 있고, 버그 2건 수정과 첫 PIE 검증이 남았다.** 03-B(줍기·버리기)는 스텁 5개와 픽업 배선부터. 상세: `05_Loot_03_Inventory_STATUS.md` "남은 작업" |
| **다음 행동** | 사용자가 03-A 버그 2건(`KeyOf`가 `SortKey`를 반환, `AddItem` null 가드)을 고치고 PIE로 완료 조건을 돌린 뒤 → `/session-start 리뷰 Loot 03A`. 그다음 `/session-start 개발 Loot 03B` — **가이드 L2** (2026-10-02 사용자 결정) |
| 하지 말 것 | 버그 1번(`KeyOf`)을 고치기 전에 완료 조건 14를 통과로 치지 않는다. Step 문서의 `EP.Inv.*` 이름을 그대로 믿지 않는다 — 실제는 `EPInv*` 치트(Step STATUS 표) |
| 근거 | `05_Loot_03_Inventory_STATUS.md` 2026-09-30 재대조, 커밋 `909a8c0`(이름 변경)·`101632b`(결정 분리) |
| 막힌 점 / 사용자 확인 필요 | ① `05_Loot_DECISIONS.md`에서 서로 모순되는 결정 두 쌍(D-004↔D-011 본체 칸 수, D-055↔D-064 `Server_ReorderEntry` 단계) — 설계 세션이 판정한다 ② `CanPlaceInSlot`의 부착 슬롯 거절이 의도인지(Step STATUS "남은 작업" 2) |

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
  - **8차 검수 요청 작성됨** (`Review/05_Loot_REVIEW08_InventoryGAS_Request.md`) — 최대 주제는 **`Server_DropItem` 직접 RPC 대 `UEPGA_DropItem`**. 7차가 세운 "게임플레이 입력의 진입점은 어빌리티 하나다"를 Step 03 문서가 한 단계 만에 깬다
  - [ ] **03-A 코어** (03-1·2·3·**7**·9) — 완료 조건 **2~6**. 칸 합산 / `bFungible` / `COND_OwnerOnly`. `RemoveEntry`·`AddSubtree` 없이 단독 실행됨
  - [ ] **03-B 배낭** (03-6 + `GetCapacity(컨테이너)`) — 완료 조건 **7**. 자동 착용 + 독립 풀. 아직 못 버린다
  - [ ] **03-B 줍기·버리기** (03-4·5·6) — 완료 조건 **1, 7의 전반, 8~13, 16** + 이월 2건. **＋ `AddSubtree`·`TryAutoEquip`·`StartingEquipment`**(13차 답변). `RemoveEntry` / `AddSubtree` / 캐스케이드 / `Server_DropItem`. **함정표 ★★ 4건 중 3건이 여기**
  > **★ 03-7(알림)은 03-A다** (8차 검수). `FScopedInventoryNotify`를 03-3의 `AddItem`·`SetEntryCharges`가 쓰므로, 정의가 03-C에 있으면 **03-A가 컴파일되지 않는다.** 근거: `05_Loot_03_Inventory.md:51-56`
- [ ] 05_Loot_04 InventoryUI (**정사각형 격자 + 분절 게이지 + 드래그**) — 10차 검수로 **둘로 나눔**
  - [ ] **04-A 표시** (04-0·1·2·3·4·6) — 완료 조건 **1~6, 13, 14**(8개). `EP.Inv.Add`/**`EP.Inv.Move`** 커맨드로 검증한다(14차에 `EP.Inv.Equip` 폐기). **칸은 `UUserWidget`으로 만든다** — 04-B의 드롭이 칸의 `NativeOnDrop`에 걸린다
  - [ ] **04-B 드래그** (04-5·7·8) — 완료 조건 **7~12**(6개). `Server_MoveEntry`·`Server_SwapEntries`가 여기서 열린다
- [ ] 05_Loot_05 Equipment (무기 장착 흐름 이관 + 탄약 소유권 정리)
  > **★ 이 단계에 얹을 이월 항목 3건** — `DOCS/BACKLOG.md` **B-1**(무기 FX를 `WeaponDefinition`으로) / **B-3**(`case Hitscan: default:`) / **B-5**(`GetEquippedWeapon()` 대신 `GetEquippedEntryId()`를 새 코드의 진입점으로). 셋 다 여기서 하면 거의 공짜고, **B-5를 안 지키면 나중이 비싸진다**

추후 (기획 확정, 구현 미정) — `05_Loot_Design.md` §7
- [ ] 컨테이너 + 검색 시간 (GAS `CastTime` 구조 재사용)
- [ ] 자판기 (상자 배출 방식)
- [ ] **무기 부착물 (배그식 — 깊이 1).** 남은 것은 슬롯 스키마·스탯 합산·메시 부착·부착 RPC뿐

> **★ 배낭이 Step 03에 들어오면서 부착물의 구조적 비용이 대부분 선불된다** — `ParentEntryId`/`SlotId`, 서브트리 픽업, 자식 캐스케이드, `AddSubtree`가 전부 거기서 만들어진다.
>
> **Step 00~05에서 지켜야 할 유일한 것: `EntryId`의 안정성.**
> 서버 발급 / 단조 증가 / **재번호 없음**. 배열 인덱스를 쓰거나 번호를 재사용하면 부모 참조가 성립하지 않아 배낭도 부착물도 원천 봉쇄된다.

---

## 설계 결정

결정과 그 경위는 `05_Loot_DECISIONS.md`에 있다 (2026-10-02 이관). 지금 유효한 설계는 `05_Loot_Design.md`.

---

## 기존 코드에서 반드시 손대야 할 것

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

> 설계 변경 이력(차수별 경위)은 `05_Loot_DECISIONS.md` "차수별 경위"로 옮겼다.

무기는 `EPGameMode.cpp:81`에서 `DefaultWeaponClass`를 직접 스폰. 이 흐름의 인벤토리 이관은 **Step 05 범위 안**이다 (`05_Loot_Design.md` §4-8) — `DefaultLoadout : TArray<FName>` 기반 지급으로 교체하고, 무기 액터 스폰 책임은 `UEPCombatComponent`에 남긴다.

---

## 세션 시작 템플릿

`/session-start <역할> Loot <대상>` — 위 인수인계 칸의 "다음 행동"을 따른다. 끝낼 때는 `/handoff`.
