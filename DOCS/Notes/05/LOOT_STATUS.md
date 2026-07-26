# Loot 단계 전체 진행 상황

> 세션 시작 시 이 파일을 먼저 읽을 것.
> 현재 Step 확인 후 해당 Step의 STATUS 파일을 추가로 읽는다.
> 마스터 기획: `05_Loot_DOCS.md`

---

## 진행 상황

- [ ] 05_Loot_00 ItemCore (기존 아이템 계층 정비 + 서브시스템 2종) ← **현재**
- [ ] 05_Loot_01 Spawner (루트테이블 + 스포너 + 픽업)
- [ ] 05_Loot_02 Interaction (IEPInteractable + 상호작용 컴포넌트 + 서버 검증)
- [ ] 05_Loot_03 Inventory (FastArray 복제 + 스택 병합 + 버리기)
- [ ] 05_Loot_04 InventoryUI (고정 슬롯 표시)
- [ ] 05_Loot_05 Equipment (무기 장착 흐름 이관 + 탄약 소유권 정리)

추후 (기획 확정, 구현 미정) — `05_Loot_DOCS.md` §7
- [ ] 컨테이너 + 검색 시간 (GAS `CastTime` 구조 재사용)
- [ ] 자판기 (상자 배출 방식)

---

## 확정된 설계 결정 (상세는 `05_Loot_DOCS.md`)

| 항목 | 결정 |
|---|---|
| 슬롯 형태 | **1x1 고정 슬롯** — 데이터 모델에 2D 크기 없음(`SlotSize`는 1차원 스칼라) |
| 인벤토리 부착 | **Character** — 사망 시 소실이 규칙과 일치 |
| 인벤토리 복제 | **FFastArraySerializer + POD 구조체**, `UEPItemInstance`는 서버 전용 |
| 픽업 액터 (복제) | `ItemId` + `Quantity`만. 인스턴스는 복제하지 않음 |
| 픽업 액터 (참조) | 서버 전용 `int32 InstanceHandle`. 스포너가 뿌린 것은 `INDEX_NONE`(획득 시 생성), **버려진 비스택 아이템은 유효한 핸들을 그대로 이관**(잔탄·내구도 보존) |
| 루트 테이블 | **가중치 + 중첩(SubTable)** — GAME.md 등급 확률 50/30/15/5 보존 |
| 부분 획득 | 허용 (`AddItem()`이 삽입된 개수 반환) |
| 스폰 시점 | GameMode의 `MatchState`가 지시 (스포너 `BeginPlay` 아님) |
| 아이템 버리기 | **포함**(Step 03). 인스턴스를 재생성하지 않고 픽업으로 이관 |
| 무기 장착 이관 | **포함**(Step 05). 탄약은 인스턴스가 진실, GAS `Ammo`는 뷰 |
| 장비 슬롯 | 별도 배열 없이 `EquippedHandle`(int32) 하나 |
| 인벤토리 복제 조건 | **`COND_OwnerOnly`** — 남의 가방이 보이면 치트 + 대역폭 |
| 식별자 | **`int32` 핸들** (서버 발급). `FGuid InstanceId`는 DB 영속용으로만 |
| 인스턴스 소유 | `UEPItemInstanceSubsystem`(World, 서버 전용)이 유일한 강참조 |
| 인스턴스 생성 | **Definition의 virtual `CreateInstance()`** — static 팩토리 2종은 제거 |
| 픽업 복제 | `DORM_Initial` + Tick off. `Quantity` 변경 시 `FlushNetDormancy()` |
| `EmptyWeight` | **루트 테이블에서만 유효** — 하위 적용 시 등급 확률이 침식됨 |
| 인스턴스 생성 대상 | **`MaxStack == 1`인 아이템만.** 스택 아이템은 `Handle = INDEX_NONE` (병합·분할이 정수 연산) |
| 슬롯 번호 | **`FEPInventoryEntry::SlotIndex`** 명시 필드 — FastArray는 클라 배열 순서를 보장하지 않음 |
| Definition 로딩 | **매치 전 전량 상주**(AssetManager). 소프트는 `WorldMesh`/`Icon`/`WeaponMesh`만 |
| 무기 액터 스폰 책임 | **`UEPCombatComponent`** 유지 — 인벤토리는 `EquipFromInventory(Handle)`로 핸들만 넘김 |
| 드랍 RPC 파라미터 | **`SlotIndex`** (FGuid도 Handle도 아님) |
| 인스턴스 수명 | 인벤토리/픽업 `EndPlay`에서 `Destroy(Handle)`. 이관 중에는 금지 (이관 프로토콜 준수) |
| DT 캐시 | `FEPItemData` **값 복사**. 행 포인터 캐시 금지(리임포트 시 댕글링) |
| `UEPLootTable` 로딩 | `EPLootTable` PrimaryAssetType 등록 — `EP.Loot.RollTable`이 이름으로 찾으려면 필요 |
| 서버 상호작용 검증 | `Server_Interact`에서 **거리 재검증 + `CanInteract()` 재호출**. 없으면 `DropCooldown`이 서버에서 무시됨 |
| 액터 복제 설정 API | `SetNetCullDistanceSquared()` / `SetNetUpdateFrequency()` — 5.5부터 필드 직접 대입 deprecated |

### 기존 코드에서 반드시 손대야 할 것

| 위치 | 조치 | 단계 |
|---|---|---|
| `UEPItemInstance::CreateInstance()` (static) | 제거 → Definition의 virtual 팩토리 | Step 00 |
| `UEPWeaponInstance::CreateWeaponInstance()` (static) | 제거 → 동상 | Step 00 |
| `UEPItemInstance::Quantity` | **제거.** 수량의 진실은 엔트리/픽업 하나뿐 | Step 00 |
| `UEPItemInstance::IsSupportedForNetworking()` | **제거.** 인스턴스는 복제하지 않음 | Step 00 |
| `UEPItemDefinition` | `virtual CreateInstance()` + `GrantedAbility` + `IsDataValid()` 추가 | Step 00 |
| `UEPWeaponDefinition::MaxAmmo` (`uint8`) | `int32`로 변경 — 인스턴스/어트리뷰트와 캐스팅 정리 | Step 00 |
| `UEPWeaponDefinition::GetPrimaryAssetId()` | **오버라이드 제거.** 지금 `"WeaponDef"`를 반환해 상위(`"ItemDef"`)와 타입이 갈린다 → 한 타입만 등록하면 무기 Definition이 로드 안 됨 | Step 00 |
| `DT_Items.uasset` | 스택 아이템 행 추가(`Ammo_762`/`Bandage`/`Scrap_Paper`) + 비스택 비무기(`Resume`). 현재 3행이 전부 무기라 `MaxStack > 1` 분기와 기본 인스턴스 경로를 검증할 대상이 없다 | Step 00 |
| Project Settings → Asset Manager | `ItemDef` PrimaryAssetType 등록 (`AssetBaseClass = EPItemDefinition`, 전량 상주용). 현재 `Map`/`PrimaryAssetLabel`만 등록돼 있음 | Step 00 |
| Project Settings → Asset Manager | `EPLootTable` PrimaryAssetType 등록 (`RollTable` 이름 조회용) | Step 01 |
| Project Settings → Collision | `EP_TraceChannel_Interact` 신규 채널. `ECC_Visibility` 재사용하면 픽업 앞 잡동사니가 상호작용을 막는다 | Step 02 |
| `AEPCharacter` 생성자 | `InteractionComponent`(Step 02) / `InventoryComponent`(Step 03) 추가 + 게터. `CombatComponent`/`RewindComponent` 옆 | Step 02·03 |
| `AEPPlayerController` | `InteractAction`(E) / `ToggleInventoryAction`(Tab) UPROPERTY + 게터 — 기존 Dash/Heal/Shield 패턴 | Step 02·04 |
| `EPCombatComponent.cpp:177` `InitAmmo(MaxAmmo)` | **제거.** 버리기가 들어오면 12/30 무기를 버렸다 줍기만 해도 30/30이 되는 익스플로잇 | Step 05 |
| `EPCombatComponent` `Init*` → `Set*` | `Init*`은 어트리뷰트 델리게이트를 안 쏜다 → 장착해도 HUD 탄약이 안 바뀜. **`MaxAmmo`를 `Ammo`보다 먼저** 세팅(`PreAttributeChange`가 `[0, MaxAmmo]`로 클램프) | Step 05 |
| `UEPCombatComponent::UnequipWeapon()` | 잔탄 write-back 추가. **교체·버리기·사망 세 경로가 전부 여기를 거치게** 만들어 한 곳에만 둔다 | Step 05 |
| `UEPCombatComponent` 신규 필드 | `int32 EquippedInstanceHandle`(서버 전용, 복제 X). 인벤토리의 `EquippedHandle`(복제, UI용)과 **별개** — 버리기에서 인벤토리가 먼저 비워져도 write-back 대상을 잃지 않게 | Step 05 |
| Project Settings → Asset Manager | 두 타입 모두 **`Is Editor Only = false`**. 기존 `Map`/`PrimaryAssetLabel`이 `True`라 따라 하면 **패키지 빌드에서만** 리스트가 빈다 | Step 00·01 |
| `EPGameMode::HandleStartingNewPlayer` | `DefaultWeaponClass` → `DefaultLoadout : TArray<FName>` | Step 05 |
| `UEPCombatComponent::EquipWeapon(AEPWeapon*)` | 유지 + `EquipFromInventory(int32 Handle)` 추가해 위임. 무기 액터 스폰 책임은 여기 남긴다 | Step 05 |
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

무기는 `EPGameMode.cpp:81`에서 `DefaultWeaponClass`를 직접 스폰. 이 흐름의 인벤토리 이관은 **Step 05 범위 안**이다 (`05_Loot_DOCS.md` §4-8) — `DefaultLoadout : TArray<FName>` 기반 지급으로 교체하고, 무기 액터 스폰 책임은 `UEPCombatComponent`에 남긴다.

---

## 세션 시작 템플릿

```
@LOOT_STATUS.md @05_Loot_0X_XXX_STATUS.md
Step X 진행.
```
