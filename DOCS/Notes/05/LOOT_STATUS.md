# Loot 단계 전체 진행 상황

> 세션 시작 시 이 파일을 먼저 읽을 것.
> 현재 Step 확인 후 해당 Step의 STATUS 파일을 추가로 읽는다.
> 마스터 기획: `05_Loot_DOCS.md`

---

## 진행 상황

- [x] 05_Loot_00 ItemCore (아이템 계층 정비 + `FEPItemState` + Definition 서브시스템) — **`EP.Item.Dump` → `9, 9`.** 상세: `05_Loot_00_ItemCore_STATUS.md`
- [ ] 05_Loot_01 Spawner (루트테이블 + 스포너 + 픽업) ← **현재.** 5차 검수 반영 완료 (`Review/05_Loot_REVIEW5_*`)
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
| `DT_Items.uasset` | **완료 — 9행 / DA 9종.** 비무기 6행(`AmmoBox_545`/`Bandage`/`Scrap`/`Resume`/`Cash_10000`/`Backpack_Small`) + 각각 Definition 에셋. **행 값은 미검증** — `Cash_10000.SellPrice`(기본 100), `bFungible`, `Backpack_Small.ContainerCapacity`, 무기 `SlotSize`를 눈으로 확인할 것 | Step 00 (완료) |
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
| 상호작용 / 픽업 / 인벤토리 | 클래스 자체가 없음 |

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
