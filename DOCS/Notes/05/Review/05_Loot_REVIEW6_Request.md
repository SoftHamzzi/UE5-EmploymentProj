# 검수 요청 6차 — 전역 데이터 참조를 어디에 둘 것인가 (단일 주제)

> 작성일: 2026-07-30
> 5차: `05_Loot_REVIEW5_Request.md` / `_Answer.md` (Step 01 문서 검수, 12건 반영 완료)
> 시점: **Step 01 구현 착수 직후.** `Private/Loot/EPItemSpawner.cpp` 작성 중
> 성격: 문서 검수가 아니라 **아키텍처 판단 하나.** 코드 리뷰를 요청하는 것이 아니다

---

## 0. 사용자 입장 (먼저 밝힌다)

**"머지않은 미래에 바꿀 것이고 오래 걸리지 않는다면, 지금 하는 게 맞다."**

지금까지의 논의는 *"전역 에셋 참조가 4~5개를 넘을 때 옮기자"* 로 미루는 쪽이었다. 그런데 **미루는 것 자체가 비용**이다 — Step 03·04에서 참조가 늘어난 뒤에 옮기면 옮길 대상이 많아지고, 그 사이에 작성된 코드가 전부 옛 경로를 참조한다.

**이 전제를 검증해달라.** 맞으면 지금 하고, 틀리면(= 나중에 옮기는 비용이 실제로 안 늘거나, 옮길 필요가 애초에 없거나) 그 근거를 달라.

**그리고 §5의 실무 조사를 요청한다.** 인터넷 검색이든 엔진/Lyra 소스 직독이든, **상용 UE5 프로젝트가 실제로 어떻게 하는지** 확인해달라. 우리 판단만으로 결정하고 싶지 않다.

---

## 1. 현재 상태 (사실만)

### 1-1. `UDeveloperSettings` 두 개

```cpp
// Public/Combat/EPCombatDeveloperSettings.h  — 에셋 참조 없음. 스칼라 + 디버그 플래그뿐
float MaxRewindSeconds = 0.5f;
float BroadPhasePaddingCm = 50.f;
float DefaultTraceDistanceCm = 10000.f;
bool  bEnableSSRDebugDraw = false;    // 외 3개

// Public/Data/EPLootDeveloperSettings.h  — ★ 에셋 참조가 여기 있다
TSoftObjectPtr<UDataTable> ItemDataTable;                  // 현재 유일. 프로덕션 필수
TSoftObjectPtr<UStaticMesh> PlaceholderPickupMesh;         // Step 01에서 추가 예정
TSubclassOf<AEPPickup>      PickupClass;                   // Step 01에서 추가 예정
bool bEnableLootDebugLog / bEnableSpawnerDebugDraw;        // Step 01에서 추가 예정
```

```ini
; DefaultGame.ini:34
[/Script/EmploymentProj.EPLootDeveloperSettings]
ItemDataTable=/Game/Data/DT_Items.DT_Items
```

### 1-2. 소비자 (전부 `GetDefault<>`)

```
EPItemDefinitionSubsystem.cpp:57   ← ItemDataTable (Initialize에서 1회)
EPItemSpawner.cpp:32               ← PickupClass (Step 01, 작성 중)
EPServerSideRewindComponent.cpp:112, 264, 311   ← Combat 쪽, 에셋 참조 아님
```

### 1-3. 확인된 엔진 사실 (다시 파지 말 것)

| | |
|---|---|
| `UDeveloperSettings`는 **Runtime 모듈** | `Runtime/DeveloperSettings/Public/Engine/DeveloperSettings.h`. `#if WITH_EDITOR`는 섹션 표시 텍스트뿐 → **쉬핑에서 동작한다** |
| 쿡 강제는 `AlwaysCook`만 | `AssetManager.cpp:4731-4739`. `Unknown`은 "누가 하드 참조하면"이다 |
| `DT_Items`가 지금 쿡되는 이유 | **`.ini` 경로가 아니라** 각 DA의 `ItemDataRow`(`FDataTableRowHandle::DataTable`은 하드 참조 — `DataTable.h:408`) |

---

## 2. ★ 문서와 내 진단이 정면으로 모순된다 — 이것부터 판정해달라

기존 문서가 **두 곳에서** 이렇게 단언한다.

> `05_Loot_DOCS.md:823` / `05_Loot_00_ItemCore.md:610`
> **`FName` 경로가 아니라 `TSoftObjectPtr<UDataTable>`을 쓴다.** 문자열 경로는 에셋을 옮기거나 이름을 바꿔도 컴파일·저장이 통과하고, 런타임에 조용히 null이 된다. **소프트 포인터는 에디터가 참조를 추적해 리다이렉터를 따라간다.**

그런데 이번 논의에서 나(Claude)는 사용자에게 **정반대**로 말했다.

> "`.ini`의 경로는 그냥 문자열이라 애셋 레지스트리 참조가 아니다. 이름을 바꾸면 에디터가 에셋 참조는 고쳐주지만 **`.ini` 문자열은 안 고친다.** 리다이렉터가 남아 있으면 로드 시 따라가지만, `Fix Up Redirectors`를 돌리면 끊긴다."

**둘 중 하나는 틀렸고, 나는 어느 쪽도 검증하지 못했다.** 이게 이번 판단의 근거 절반을 차지하므로 먼저 확정해야 한다.

구분해서 봐야 할 것 같다.

| 저장 위치 | 애셋 레지스트리 의존인가 | 리네임 시 자동 수정되는가 |
|---|---|---|
| `.uasset` 프로퍼티의 `TSoftObjectPtr` | ? | ? |
| **네이티브 CDO의 `Config` 프로퍼티** (= `.ini` 텍스트) | ? | ? |

**질문:**
1. `Config` 프로퍼티의 `TSoftObjectPtr`는 애셋 레지스트리에 의존 간선을 만드는가? (만든다면 `ModifyCook` 논의가 달라진다)
2. 에셋을 리네임/이동하면 에디터가 **`.ini` 파일을 다시 쓰는가?** 아니면 리다이렉터에만 의존하는가?
3. `Fix Up Redirectors` 후에도 살아남는가?
4. **패키지 빌드에서는 어떻게 되는가?** (쿡된 빌드에 리다이렉터가 없다면?)

---

## 3. 후보 안

### A. 현행 유지 — `UDeveloperSettings`에 에셋 참조를 둔다

지금 상태. 추가 작업 0.

### B. `DA_EPGameData` (`UPrimaryDataAsset`) 도입 — Lyra 방식

```
DefaultGame.ini  ──(경로 문자열 1개)──▶  DA_EPGameData
                                          ├─ ItemDataTable
                                          ├─ PlaceholderPickupMesh
                                          ├─ PickupClass
                                          └─ (Step 03 사운드 / Step 04 아이콘·머티리얼)
```

**끊길 수 있는 문자열이 프로젝트 전체에 하나로 줄어든다.** DataAsset 안쪽은 전부 실제 에셋 참조라 쿡·리네임이 자동으로 따라온다 — **§2가 "내 진단이 맞다"로 판정되면 이 이점이 성립하고, 문서가 맞다면 성립하지 않는다.**

### C. 하이브리드 — 스칼라·플래그는 `UDeveloperSettings`, 에셋 참조만 DataAsset

B와 같지만 `MaxRewindSeconds` / 디버그 플래그는 안 옮긴다. 성격이 다른 값을 섞지 않는다는 논리.

### D. `ItemDataTable`도 AssetManager로 — config 경로를 아예 없앤다

`UDataTable`을 별도 `PrimaryAssetType`으로 등록하고 서브시스템이 타입으로 찾는다. `ItemDef`와 완전히 같은 방식이 되고 **`.ini` 경로가 0개**가 된다.

- 장점: 일관성. 이미 그 코드 경로가 있다(`GetPrimaryAssetIdList` → `GetPrimaryAssetObjectList`)
- 의문: DT가 여러 개가 되면 "어느 게 아이템 테이블인가"를 구분할 수 없다. 등록 디렉터리로 가르는 건 취약해 보인다

### E. 그 외 — 우리가 못 떠올린 것이 있다면

---

## 4. 판단 요청

### 4-1. ★ 사용자 전제가 맞는가 — "곧 바꿀 거면 지금"

구체적으로 **미루는 비용이 실제로 증가하는가?**

- Step 03·04에서 전역 에셋 참조가 늘어난다고 우리는 예상한다(획득/버리기 사운드, 기본 아이콘, 슬롯 머티리얼). **그 예상이 맞는가?** 아니면 그것들은 애초에 위젯 BP나 아이템 Definition에 들어갈 것이라 전역 참조가 안 늘어나는가?
- 옮기는 작업량이 **참조 개수에 비례**하는가, 아니면 **한 번 만들면 그 뒤는 필드 추가**라 개수와 무관한가? 후자라면 지금 할 이유가 약해진다

### 4-2. A~E 중 무엇인가. 그리고 **이 프로젝트 규모에서** 맞는가

포트폴리오 규모(개발자 1명, Step 05까지 + §7 추후)에서 B/C가 **과잉 구조**는 아닌가. Lyra는 팀 규모와 확장 범위가 다르다.

> 참고: CLAUDE.md §2를 이번에 **"Extensibility First"** 로 개정했다. 판단 기준은 *"이 확장점이 `DOCS/` 어딘가에 이름으로 적혀 있는가"* 다. `DA_EPGameData`는 **어느 문서에도 없다** — 그래서 이 요청 자체가 "문서를 먼저 고칠 것인가"를 묻는 것이기도 하다.

### 4-3. `PlaceholderPickupMesh`는 어디로 가는가

이 필드는 **임시**다 — `WorldMesh`를 가진 아이템이 하나도 없어서 존재하고, 실제 메시가 들어오면 폴백으로 강등된다. **임시 필드를 새 구조로 옮기는 것이 맞는가**, 아니면 애초에 옮길 대상이 아닌가.

### 4-4. `PickupClass`는 정말 전역인가

5차 이후 논의에서 세 후보가 나왔다.

| | 위치 | 세분화 |
|---|---|---|
| **채택** | `UEPLootDeveloperSettings` | 프로젝트 전체 하나 |
| 검토 후 기각 | `UEPItemDefinition::PickupClass` | **아이템별** |
| 기각 | `AEPItemSpawner` UPROPERTY | 스포너별 — 버리기 경로가 못 봄 |

아이템별을 기각한 근거는 *"픽업 액터가 아이템마다 달라야 할 이유가 실제로 없다 — 메시는 `WorldMesh`, 등급 이펙트는 `Rarity`, 배낭 내용물은 `Payload`로 전부 데이터가 해결한다"* 였다.

**§7의 컨테이너·자판기·부착물까지 보면 이 판단이 유지되는가?** 특히 부착물 달린 총이 바닥에 떨어질 때(§7-3) 같은 `AEPPickup`으로 되는가.

### 4-5. `TSubclassOf` vs `TSoftClassPtr`

`PickupClass`를 `TSubclassOf`로 정했다. 근거: *"스폰 순간 무조건 필요하므로 지연 로드 이득이 0이고, `LoadSynchronous()`와 null 폴백 분기가 사라진다."*

**BP 클래스를 `TSubclassOf`로 config에 두면 그 BP와 그 의존 에셋(메시·머티리얼·나이아가라)이 언제 로드되는가?** 하드 클래스 참조라 **모듈 로드 시점에 전부 끌려오는 것**이면 지연 로드를 버린 대가가 생각보다 클 수 있다.

---

## 5. ★ 실무 조사 요청

우리 판단만으로 결정하지 않겠다. **가능하면 실제 소스나 문서를 근거로** 확인해달라.

1. **Lyra가 실제로 어떻게 하는가.** `ULyraAssetManager` / `ULyraGameData`(또는 현재 이름)의 실제 구조. config에 남기는 경로가 몇 개이고 무엇인가. `UDeveloperSistings`와 병용하는가
2. **엔진 자체의 관례.** 에셋 참조를 `UDeveloperSettings`에 두는 엔진 클래스가 있는가, 있다면 어떤 종류의 참조인가
3. **상용 프로젝트/커뮤니티 관례.** "GameData asset vs DeveloperSettings"에 대한 실무 논의가 있는가. 어느 쪽이 왜 우세한가
4. **§2의 리다이렉터 문제에 대한 확정 답.** 이게 B의 이점 절반을 결정한다

> 검색이 가능하면 검색하고, 안 되면 엔진 소스(`C:\Program Files\Epic Games\UE_5.7\Engine`)와 `C:\Github\GASDocumentation`를 직독해달라. **Lyra 소스가 로컬에 없으면 없다고 말해달라** — 기억으로 Lyra API를 단정하지 말 것. 5차에서 인용 정확도가 높았던 것이 유용했다.

---

## 6. 재론하지 않았으면 하는 것

| | 근거 |
|---|---|
| `FEPItemState` 값 타입 / 스택 폐지 / 칸 합산 / `bFungible` | 1·2차 확정 |
| `Durability` / `MaxStack` 존치 | 3차 권고를 사용자가 기각 |
| DT/DA 두 계층 유지 | 3차 §5 확정 |
| `ItemDef` / `LootTable`의 `AlwaysCook` + `Is Editor Only = false` | 이번에 근거까지 확인 완료 |
| 5차에서 확정한 12건 (콜리전·Dormancy·롤 재구성·`SpawnLoot()` 본문 등) | 반영 완료. **이번 주제와 무관** |
| `AEPItemSpawner`가 `LootTable`을 하드 참조하는 것 | 루트 테이블이 스포너와 함께 살아야 한다. 이번 주제 아님 |

---

## 7. 대상 파일

| 파일 | 이번 주제와의 관계 |
|---|---|
| `Public/Data/EPLootDeveloperSettings.h` | **판단 대상** (현재 `ItemDataTable` 하나뿐) |
| `Private/Data/EPItemDefinitionSubsystem.cpp:57` | 소비자 1 |
| `Private/Loot/EPItemSpawner.cpp:32` | 소비자 2 (**작성 중**) |
| `Config/DefaultGame.ini:14, 33-34` | `ItemDef` 등록 + `ItemDataTable` 경로 |
| `Public/Combat/EPCombatDeveloperSettings.h` | 비교 대상 — 에셋 참조가 **없는** 설정 |
| `05_Loot_DOCS.md:140, 820-823` | **§2의 모순 지점 ①** |
| `05_Loot_00_ItemCore.md:606-610` | **§2의 모순 지점 ②** |
| `05_Loot_01_Spawner.md` (853줄) | `PlaceholderPickupMesh` / `PickupClass` 신설 위치 |
| `05_Loot_DOCS.md` §7 | 컨테이너·자판기·부착물 — 4-4 판단의 근거 |
| `CLAUDE.md` §2 | 개정된 판단 기준 ("문서에 이름이 있는가") |
