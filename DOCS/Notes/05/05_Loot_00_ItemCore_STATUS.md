# 05_Loot_00_ItemCore — 구현 상태

**전체 상태: 완료 (PIE 검증 완료 — `EP.Item.Dump` → `DataCache = 9, Definitions = 9`)**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷. 문서(`05_Loot_00_ItemCore.md`)의 예정 코드와 혼동 금지.
> 최종 확인: 2026-07-30 (코드 직접 대조)

---

## 00-0 — `PrimaryAssetType` 이원화 해소

**상태: 완료**

### 완료된 것
- `UEPWeaponDefinition::GetPrimaryAssetId()` **오버라이드 제거.** `EPWeaponDefinition.h`에 남은 오버라이드는 `InitState`(`:106`) 하나뿐
- `UEPItemDefinition::GetPrimaryAssetId()`가 유일한 정의 — `EPItemDefinition.cpp:41-44`, `FPrimaryAssetId(TEXT("ItemDef"), GetFName())`
- **무기 DA 3종 재저장 완료.** 코드 수정만으로는 안 고쳐졌다 (아래 버그 참조)

### 미적용 (판단 보류 — 사용자 몫)
- `GetPrimaryAssetId()`에 **`final`** 미적용. `EPItemDefinition.h:44`는 `virtual ... override`뿐
  - 오버라이드는 이미 제거됐으므로 **지금 고장난 것은 없다.** `final`은 되돌아오는 것을 막는 용도다
  - Step 01의 `UEPLootTable`에는 `final`을 넣기로 확정 (`05_Loot_01_Spawner.md` 01-1). 두 클래스가 갈리지 않게 하려면 여기도 붙인다

---

## 00-1 — `FEPItemState`

**상태: 완료**

- `EPTypes.h:76-85` — `USTRUCT`, `Charges = 0` / `Durability = 100.0f`. 8바이트 POD

---

## 00-2 — `UEPItemDefinition`

**상태: 완료**

### 완료된 것
- `virtual void InitState(const FEPItemData&, FEPItemState&) const` — `EPItemDefinition.h:41`, 구현 `.cpp:48-51` (`State.Charges = Data.InitialCharges;`)
- `GrantedAbility` (`TSubclassOf<UGameplayAbility>`) — `.h:38`
- `WorldMesh` / `Icon` (`TSoftObjectPtr`) — `.h:31, 35`
- `ItemDataRow` (`FDataTableRowHandle`) — `.h:27`
- `IsDataValid()` 구현 — `.cpp:10-38`. 검사 3종: `ItemId` 비었는지 / `ItemDataRow.RowName != ItemId` / **DT Row의 `ItemDefinition` 역참조가 이 에셋을 가리키는지**

---

## 00-3 — `UEPWeaponDefinition`

**상태: 완료**

- `MaxAmmo`가 `int32` (`.h:53`) — `uint8`에서 변경 완료
- `InitState` 오버라이드 (`.h:106`) — `State.Charges = MaxAmmo;`
- `GetPrimaryAssetId()` 오버라이드 제거 (00-0)

---

## 00-4 — `UEPItemInstance` / `UEPWeaponInstance` 삭제

**상태: 완료**

- `Public/Data/` / `Private/Data/`에 해당 파일 없음. `FGuid InstanceId` / `SchemaVersion`도 함께 소멸
- 남은 파일: `EPItemData.h` / `EPItemDefinition.h` / `EPItemDefinitionSubsystem.h` / `EPLootDeveloperSettings.h` / `EPWeaponDefinition.h`

---

## 00-5 — `UEPItemDefinitionSubsystem`

**상태: 완료.** `EPItemDefinitionSubsystem.cpp` 203줄

### 완료된 것
- `Initialize()` — `BuildDataCache()` → `LoadAllDefinitions()` 순서 (`.cpp:13-19`)
- `Deinitialize()` — 세 캐시 + 핸들 Reset (`.cpp:21-27`)
- `FindData` / `FindDefinition` / `MakeItemState` (`.cpp:29-53`)
- `BuildDataCache()` — `ForeachRow` + `RowName != ItemId` 경고, **값 복사** (`.cpp:55-76`)
- `LoadAllDefinitions()` — **`GetPrimaryAssetIdList()`의 개수로 등록 여부 판정**, `LoadPrimaryAssets(Ids)` → `WaitUntilComplete()` (`.cpp:78-101`)
- `BuildDefinitionCache()` — **`GetPrimaryAssetObjectList()`로 수집** (`.cpp:103-148`)
  - `ItemId` 비었는지 / 중복 / DT 행 없음 검사
  - **역방향 검사 완료** (`.cpp:140-148`) — DT 행에 대응하는 DA가 없으면 경고. 이 로그가 `Weapon_AK74_FastProj` 누락을 이름으로 찾아냈다
- 헤더에 `#include "Data/EPItemData.h"` (`.h:7`) — `TMap<FName, FEPItemData>`가 **값**을 담으므로 완전 타입 필요
- `GetDataCacheNum()` / `GetDefinitionCacheNum()` — `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)` 가드 (`.h:27-30`)

### 사소한 잔여 (기능 무관)
- `.cpp:88-90`이 탭이 아니라 스페이스. 파일 나머지는 탭
- `.cpp:93`은 `LoadPrimaryAssets(Ids)`로 이미 정리됨 (`LoadPrimaryAssetsWithType` 아님)

---

## 00-6 — 아이템 생성 헬퍼

**상태: 완료**

- `MakeItemState(FName, FEPItemState&) → bool` (`.h:25`, `.cpp:40-53`). 별도 서브시스템 없음
- 실패 시 `Row=OK/없음 Definition=OK/없음`을 함께 찍어 **어느 쪽이 빠졌는지** 로그만으로 갈린다

---

## 00-7 — 설정

**상태: 완료. 선택 개선 1건**

- `UEPLootDeveloperSettings` (`Config = Game, DefaultConfig`) — `ItemDataTable` (`TSoftObjectPtr<UDataTable>`)

### 선택 — 피커 필터 (6차 §9-3, 형태는 정정됨)

```cpp
UPROPERTY(Config, EditAnywhere, Category = "Data",
          meta = (RequiredAssetDataTags = "RowStructure=/Script/EmploymentProj.EPItemData"))
TSoftObjectPtr<UDataTable> ItemDataTable;
```

Project Settings 피커에 **`FEPItemData` 행을 쓰는 DT만** 뜬다. 두 번째 DT가 생겨도 잘못 고를 수 없다.

> **6차 답변의 `AllowedClasses` 권고는 실효가 없어 채택하지 않았다.** `TSoftObjectPtr<UDataTable>`은 템플릿 인자로 이미 필터된다 — Lyra가 그 메타를 쓰는 자리(`LyraAudioSettings.h:224`)는 타입 없는 `FSoftObjectPath`라 필요한 것이었다. 타입이 못 거르는 것은 **행 구조체**이고, 그건 레지스트리 태그(`DataTable.cpp:363` `RowStructure`)로 거른다(`SPropertyEditorAsset.cpp:258`).
- `DefaultGame.ini:33-34` — `ItemDataTable=/Game/Data/DT_Items.DT_Items`
- `DefaultGame.ini:14` — `ItemDef` PrimaryAssetType 등록
  - `AssetBaseClass="/Script/EmploymentProj.EPItemDefinition"`, `Directories=/Game/Data`, **`bIsEditorOnly=False`**, `CookRule=AlwaysCook`, `bApplyRecursively=True`

---

## 00-8 — DT 행 + Definition 에셋

**상태: 완료 (9행 / DA 9종)**

### DT 행 ↔ Definition 에셋 (파일 대조 확인)

| 행 | Definition 에셋 | 검증하는 것 | 쓰이는 단계 |
|---|---|---|---|
| `Weapon_AK74_HitScan` | `Weapons/DA_AK74_HitScan` | `UEPWeaponDefinition::InitState` (`Charges = MaxAmmo`) | 01·05 |
| `Weapon_AK74_SlowProj` | `Weapons/DA_AK74_SlowProj` | 〃 | 01·05 |
| `Weapon_AK74_FastProj` | `Weapons/DA_AK74_FastProj` | 〃 | 01·05 |
| `AmmoBox_545` | `Ammo/DA_AmmoBox_545` | **기본 `InitState`** (`Charges = InitialCharges`) + `bFungible` | 01·03-A |
| `Bandage` | `Consumable/DA_Bandage` | 기본 `InitState` | 01 |
| **`Cash_10000`** | `Misc/DA_Cash_10000` | **`bFungible` 합치기** (`Charges` 합산) | **03-A** |
| **`Backpack_Small`** | `Misc/DA_Backpack_Small` | **`ContainerCapacity`** / 배낭 자동 착용 / 서브트리 | **03-B** |
| `Scrap` | `Misc/DA_Scrap` | 루트 테이블 등급 분포 (일반 등급 채우기) | 01 |
| `Resume` | `QuestItem/DA_Resume` | 〃 + `bIsQuestItem` | 01 |

`FEPItemData` 신규 3필드 전부 존재 — `ContainerCapacity`(`:42`) / `InitialCharges`(`:45`) / `bFungible = false`(`:56`)

> **★ 이름이 `Scrap_Paper`가 아니라 `Scrap`이다.** 문서(`05_Loot_00_ItemCore.md` 00-8, `05_Loot_01_Spawner.md` 01-1, `05_Loot_DOCS.md` §4-2)에 `Scrap_Paper`로 적혀 있던 것을 전부 `Scrap`으로 맞췄다. **행 이름이 진실이고 문서가 따라간다.**

**PIE 확인: `EP.Item.Dump` → `9, 9`.** 9행이 전부 Definition을 갖고, 짝 없는 쪽이 양방향 모두 없다(`BuildDefinitionCache()`의 두 로그가 침묵).

### ★ `9, 9`가 증명하지 않는 것 — 행의 **값**

`Dump`는 **짝이 맞는지**만 센다. 아래 넷은 기본값 그대로여도 `9, 9`가 나오고, **틀린 것이 드러나는 시점이 한참 뒤다.**

| 확인할 것 | 기본값이면 생기는 일 | 드러나는 시점 |
|---|---|---|
| **`Cash_10000`의 `SellPrice`** | 기본값 `100` — **1만원짜리가 100원에 팔린다** | 상점이 생길 때 (한참 뒤) |
| `Cash_10000` / `AmmoBox_545`의 `bFungible` | `false`면 합치기 경로를 영영 안 탄다 — **03-A가 검증 대상 없이 구현된다** | Step 03-A |
| `Backpack_Small`의 `ContainerCapacity` | `0`이면 배낭이 아무것도 못 담는다 | Step 03-B |
| 무기 `SlotSize` | `1`이면 칸 합산 검증이 무의미하다 (4~5로 올린다) | Step 03-A |

**`InitialCharges`만 지금 바로 보인다** — `EP.Item.State AmmoBox_545` / `Cash_10000`의 `Charges`·`SlotSize` 열. 나머지 셋은 DT를 열어 눈으로 본다.

> Step 01은 넷 중 어느 것에도 의존하지 않는다. **Step 03 착수 전에 확인하면 된다.**

---

## 00-9 — 검증 커맨드

**상태: 완료**

- `EP.Item.State <ItemId>` — `.cpp:165-194`. `MakeItemState` 결과 + `Charges`/`Durability`/`SlotSize` + 실제 Definition 클래스명
- `EP.Item.Dump` — `.cpp:196-212`. `DataCache` / `Definitions` 두 수
- 둘 다 `ECVF_Cheat` + `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)`

> **인자는 `ItemId`이지 에셋 이름이 아니다.** `EP.Item.State Weapon_AK74_HitScan` (O) / `EP.Item.State DA_AK74_HitScan` (X).
> AssetManager는 에셋 이름(`ItemDef:DA_AK74_HitScan`)으로 알고, `DefinitionCache`는 `Def->ItemId`로 키를 잡는다. `BuildDefinitionCache()`가 그 다리다.

---

## 구현 중 발견된 버그 (2건, 둘 다 수정 완료)

> **★ 둘은 순서대로 나온 별개의 버그다. 섞으면 안 된다.**
>
> ```
> ① Definitions = 0     ← 핸들 null 오판정. 무기만이 아니라 5개 전부 없었다
>    ↓ 고침 (판정 방식 교체)
> ② Definitions = 5, 3  ← ①을 고친 뒤에 남은 것. 이때 빠진 3개가 정확히 무기다
>    ↓ 고침 (무기 DA 재저장)
> ③ 5, 5
> ```
>
> **핸들 null은 `0`을 만들었지 `3`을 만들지 않았다.** `3`은 ①을 고친 다음에 나온 숫자이고, 그래서 원인이 다르다. ②의 근거는 당시 무기 `.uasset` 3개에서 `WeaponDef` 문자열이, 비무기 2개에서 `ItemDef`가 나온 것이다.

### 1. `Definitions = 0` — 핸들 null을 실패로 판정했다

문서(당시)가 `if (!DefinitionHandle.IsValid()) { 로그; return; }` 가드를 지시했고, **항상 여기서 빠져나갔다.**

`ChangeBundleStateForPrimaryAssets`는 `CurrentState.IsSame(NewBundleState)`이면 `continue`하고(`AssetManager.cpp:2195-2199`), `AllHandles`가 비면 `CreateCombinedHandle`이 nullptr을 반환한다(`:2298`).

> **핸들 null = "새로 로드할 게 없다"이지 "에셋이 없다"가 아니다.** 에디터 세션에서는 DA가 이미 상주 중인 경우가 정상이라 거의 항상 이쪽으로 빠진다.

**수정:** 등록 여부는 `GetPrimaryAssetIdList()`의 **개수**로, 결과는 `GetPrimaryAssetObjectList()`의 **내용**으로 판정. 핸들은 대기용으로만.

### 2. `Definitions = 3` (5여야 함) — `PrimaryAssetType`이 `.uasset`에 구워져 있었다

**원인은 코드가 아니라 에셋이었다.** 무기 DA 3종이 `GetPrimaryAssetId()`가 `"WeaponDef"`를 반환하던 시절에 저장돼, 그 문자열이 애셋 레지스트리 태그로 박혀 있었다.

- `FAssetData::GetPrimaryAssetId()`(`AssetData.cpp:692-703`)는 **저장된 태그만 읽고 클래스에 묻지 않는다**
- `AssetManager.cpp:1396-1426`이 타입 불일치로 `continue`

**00-0의 "오버라이드 제거"만으로는 안 고쳐졌다. 무기 DA를 전부 다시 저장해야 했다.**

> **증상이 들쭉날쭉했던 이유:** `ARFilter.bIncludeOnlyOnDiskAssets = !GIsEditor || IsRunningCookCommandlet();`(`:1089`). 에디터에서는 메모리에 올라온 애셋의 `FAssetData`를 살아 있는 객체로 다시 만들어 **stale 태그를 우회한다.** 해당 DA를 에디터에서 열어두면 되살아났다. **패키지 빌드에서는 예외 없이 전부 빠진다.**
>
> `IssuedWarnings`(`:1414`)가 `static TSet`이라 **타입 쌍당 경고가 딱 한 줄**이다. 5개 중 3개가 빠졌는데 로그는 한 줄이었다.

**일반화:** 애셋 레지스트리에 구워지는 값을 바꾸면 **기존 에셋을 전부 재저장해야 한다.** 상세와 진단법은 `05_Loot_00_ItemCore.md` **함정 #1**.

---

## 문서 오류 (수정 완료)

| 문서가 적었던 것 | 실제 | 조치 |
|---|---|---|
| `if (!DefinitionHandle.IsValid()) return;` | 핸들 null은 정상 경로 | 00-5 재작성 + 함정 절 추가 |
| `struct FEPItemData;` 전방 선언으로 충분 | `TMap<FName, FEPItemData>`가 **값**을 담아 완전 타입 필요 — **코드가 맞고 문서가 틀렸다** | 00-5 정정 |
| "오버라이드만 제거하면 된다" | `.uasset` 재저장 필요 | 00-0 경고 + 함정 #1 상세 신설 |

---

## 6차 검수(2026-07-30) 이후 — Step 00에 바뀐 것

**구현을 달리해야 할 것은 없다.** 로직·구조·클래스 관계 전부 그대로다.

| | 성격 |
|---|---|
| `00_ItemCore.md:610`의 `TSoftObjectPtr` 근거 문장 교체 | **문서만.** *"리다이렉터가 따라간다"* → *"자동으로 안 고쳐진다. 이득은 **깨질 때 시끄러운 것**"*. **결정 자체는 그대로** |
| `ItemDataTable`에 `RequiredAssetDataTags` | **선택.** 위 00-7 |
| `GetPrimaryAssetId()`에 `final` | **선택.** 5차부터 보류 중 (00-0) |
| Definition을 프래그먼트 조합으로 전환 | **하지 않는다.** 상속 유지 — 근거·비용·전환 신호는 `LOOT_STATUS.md` |
| 전역 에셋 참조를 `DA_EPGameData`로 이동 | **하지 않는다.** `UDeveloperSettings` 유지 (Lyra가 같은 것을 한다) |

---

## Step 01에 넘긴 것

| | |
|---|---|
| **등록 ≠ 로드** | `GetPrimaryAssetObject`도 메모리에 없으면 nullptr. `EP.Loot.RollTable`이 직접 로드한다 (`05_Loot_01_Spawner.md` 01-5) |
| **핸들 null 모호성** | 판정은 언제나 **결과 포인터**로 |
| **`PrimaryAssetType` 굽기** | 코드 컴파일 → 등록 → 재시작 → 에셋 생성 순서 + `final` (01-1, 함정 #14) |
| **`UEPLootDeveloperSettings` 확장** | `PlaceholderPickupMesh` / `PickupClass` / 디버그 플래그 2종 (01-5) |
