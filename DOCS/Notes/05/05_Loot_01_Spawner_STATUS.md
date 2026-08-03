# 05_Loot_01_Spawner — 구현 상태

**전체 상태: 구현 완료 / 검증 1건 미완 (`EP.Loot.RollTable`이 출력을 안 한다)**

> 세션 시작 시 이 파일을 반드시 읽을 것.
> 현재 코드 상태의 정확한 스냅샷. 문서(`05_Loot_01_Spawner.md`)의 예정 코드와 혼동 금지.
> 최종 확인: 2026-08-02 (코드 직접 대조)

---

## 완료 조건 대조

| # | 완료 조건 | 상태 |
|---|---|---|
| 1 | 맵에 스포너를 놓고 PIE 2인 → 서버·클라 양쪽에서 같은 아이템이 같은 위치에 | ✅ 사용자 확인 |
| 2 | **`EP.Loot.RollTable LT_Floor_Common 1000` → 등급 비율이 기획표(50/30/15/5)와 일치** | ❌ **검증 불가 — 커맨드가 아무것도 안 찍는다.** 아래 |
| 3 | 어떤 스포너도 참조하지 않는 테이블도 `RollTable`이 이름으로 찾는다 | ✅ **간접 확인됨** — 테이블을 못 찾으면 `EPLootDebugCommands.cpp:44`가 Error를 찍는데 침묵했다. 즉 `LoadPrimaryAsset` → `GetPrimaryAssetObject` 경로가 동작한다 |
| 4 | `EP.Loot.Respawn` → 기존 픽업이 정리되고 새로 굴려진다 | ✅ 사용자 확인 |
| 5 | `WorldMesh`가 없는 아이템도 플레이스홀더로 보인다 | ✅ `EPPickup.cpp:63-64` |
| 6 | 픽업을 향해 쏴도 총알이 안 막히고, 픽업 위를 걸어도 안 걸린다 | ✅ `EPPickup.cpp:33-35` |

---

## 01-1 — `UEPLootTable` + `RollLootTable`

**상태: 완료**

- `UEPLootTable : UPrimaryDataAsset` — `Entries`(`TArray<FEPLootEntry>`) / `EmptyWeight`
- `GetPrimaryAssetId()`에 **`override final`** (`EPLootTable.h:38`) — Step 00에서 무기 DA가 겪은 타입 굽기 사고(함정 #14) 재발 차단
- `FEPLootEntry` — `Weight`(`ClampMin=0`) / `ItemId` / `SubTable`
- `RollLootTable(const UEPLootTable*, FName&) → bool` (`EPLootTable.cpp:97-100`), 내부 `RollInternal(..., Depth)`
- **`EmptyWeight`는 루트에서만 계산된다** (`.cpp:20`, `:33`). 하위 테이블에 값이 있으면 무시하고 Warning (`:21-26`)
- 깊이 상한 8 — 순환 참조 방어
- `IsDataValid()` (`#if WITH_EDITOR`, `.cpp:57-95`) — 3종 검사: 빈 Entries+EmptyWeight 0 / `ItemId`·`SubTable` 배타성 / 자기 참조

### 반환 규약 (Step 03 이후에도 유지)

| 반환 | `OutItemId` | 뜻 |
|---|---|---|
| `true` | `NAME_None` | **정상 — 빈 결과** (`EmptyWeight` 당첨) |
| `true` | 유효 | 정상 — 아이템 |
| `false` | `NAME_None` | **데이터 오류** (깊이 초과 / 총 가중치 0 / `ItemId`도 `SubTable`도 없는 엔트리) |

> `AEPItemSpawner::SpawnLoot`가 이 셋을 정확히 구분해 처리한다 (`EPItemSpawner.cpp:67-73`).

### 잔여 (기능 무관, 미수정)

| 위치 | 내용 |
|---|---|
| `EPLootTable.cpp:13` | `if (!Table \|\| Depth > MaxDepth)`가 합쳐져 있어 **null 테이블에도 "롤 깊이 초과 - 순환 참조 의심" 로그가 뜬다.** 원인이 정반대인데 같은 문구라 진단이 어긋난다 |
| `EPLootTable.cpp:78` | `NSLOCTEXT` 키 오타 — `LootEntryAmbigious` (→ `Ambiguous`). 키일 뿐이라 동작 무관 |

> **`#include "Misc/DataValidation.h"`(`:6`)가 `#if WITH_EDITOR` 밖인 것은 문제가 아니다.** 엔진 헤더(`CoreUObject/Public/Misc/DataValidation.h`)에 `WITH_EDITOR` 가드가 하나도 없고 `FDataValidationContext`는 모든 빌드에서 정의된다. **이전 검토에서 지적했던 것을 철회한다.**

---

## 01-2 — `AEPItemSpawner`

**상태: 완료**

- `bReplicates = false` (`EPItemSpawner.cpp:19`) — 서버 전용 마커. 스폰된 픽업만 복제된다
- **`RootComponent`가 `USceneComponent`이고 빌보드는 `SetupAttachment`** (`:21-29`)
  - **함정 #21 회피 확인.** `CreateEditorOnlyDefaultSubobject`는 `GIsEditor == false`면 nullptr을 반환하므로(`UObjectGlobals.cpp:6039`) 빌보드를 루트로 삼았다면 Standalone/패키지에서 모든 스포너가 월드 원점으로 갔다. PIE에서는 정상이라 **에디터에서 절대 안 잡히는 종류**였다
  - `#if WITH_EDITORONLY_DATA` + `if (Billboard)` 이중 가드
- `SpawnLoot()` / `ClearLoot()` public (`.h:22-23`), 둘 다 `HasAuthority()` early return
- `GetSpawnPoint()` — `SpawnRadius` 원형 분산 + `bAlignToGround` 접지 트레이스(`ECC_Visibility`, 위 200 / 아래 500, 착지점 +5cm)
- `RollCount` / `SpawnRadius` / `bAlignToGround` 전부 `EditAnywhere` 노출 (`.h:30-36`)
- `SpawnedPickups`가 `TWeakObjectPtr` — `ClearLoot`이 이미 죽은 픽업을 안전하게 건너뛴다

### ★ `InitPickup`이 `SpawnActor`와 같은 프레임에 불린다

`EPItemSpawner.cpp:87-92` — 스폰 직후 즉시 호출.

**함정 #16이 여기서 막힌다.** `AEPPickup`이 `DORM_Initial`이라, 다음 프레임으로 미뤘다면 클라가 `ItemId = NAME_None`인 초기 상태를 받고 그대로 휴면에 들어가 **영원히 안 고쳐진다.**

---

## 01-3 — GameMode 연동

**상태: 완료**

```cpp
// EPGameMode.cpp:140-145
void AEPGameMode::HandleMatchHasStarted()
{
    for (TActorIterator<AEPItemSpawner> It(GetWorld()); It; ++It)
        It->SpawnLoot();

    Super::HandleMatchHasStarted();   // ← 여기서 RestartPlayer + NotifyBeginPlay
    ...
}
```

**루프가 `Super::` 앞이라는 것이 핵심이다.** `AGameMode::HandleMatchHasStarted`(`GameMode.cpp:203-221`)가 그 안에서 `RestartPlayer` → `NotifyBeginPlay`를 부르므로, 뒤에 두면 플레이어 폰이 먼저 생긴다.

스폰 시점이 스포너 `BeginPlay`가 아니라 GameMode의 `MatchState`인 이유는 `05_Loot_DOCS.md` §4-3.

---

## 01-4 — `AEPPickup`

**상태: 완료**

| 항목 | 코드 | 막는 함정 |
|---|---|---|
| `NetDormancy = DORM_Initial` | `.cpp:25` | 바닥 아이템 수백 개의 유휴 대역폭 |
| **`ItemId`만 복제** (`DOREPLIFETIME`, `.cpp:50`) | `State`는 `UPROPERTY()`만 | **#10 — 바닥 무기 잔탄 노출 → 교전 흔적 추론** |
| `SetCollisionResponseToAllChannels(ECR_Ignore)` | `.cpp:35` | **#15 — 플레이어가 바닥 아이템에 걸려 올라탄다** |
| `SetNetCullDistanceSquared(25000000.f)` | `.cpp:27` | **#1 — 비제곱값** (= 5,000cm) |
| `SetNetUpdateFrequency(1.f)` | `.cpp:28` | **#2 — 5.5 deprecated 필드 직접 대입** |
| `IsNetMode(NM_DedicatedServer)` 조기 반환 | `.cpp:60` | **#9 — 데디서버가 시각 에셋을 로드** |
| `CreateWeakLambda(this, ...)` | `.cpp:75` | **#8 — 로드 도착 전 픽업 파괴 시 크래시** |
| 플레이스홀더 → 실제 메시 순서 | `.cpp:63-64` → `:73-79` | 로드 지연 중 투명 픽업 |
| `ApplyVisual()`을 `InitPickup`·`OnRep_ItemId` 양쪽에서 | `.cpp:43`, `:55` | 서버(리슨)와 클라가 갈림 |
| `bClaimed` (private, `.h:45`) + `IsClaimed()` (`.h:23`) | | Step 02 동시 획득 경쟁 준비 |

> **`bClaimed`는 Step 01에서 읽는 코드가 없다.** 값어치는 Step 03에서 나온다 — 인벤토리 삽입이 실패해 픽업이 살아남는 갈래가 그때 생긴다. `EP.Loot.List`가 마지막 열로 노출한다.

---

## 01-5 — 설정 + 디버그 커맨드

### `UEPLootDeveloperSettings` — 필드는 완료, **2개는 죽어 있다**

```cpp
TSoftObjectPtr<UDataTable>  ItemDataTable;           // ✅ Step 00부터 사용
TSoftObjectPtr<UStaticMesh> PlaceholderPickupMesh;   // ✅ EPPickup.cpp:63
TSoftClassPtr<AEPPickup>    PickupClass;             // ✅ EPItemSpawner.cpp:52-54
bool bEnableLootDebugLog    = false;                 // ❌ 읽는 코드 없음
bool bEnableSpawnerDebugDraw = false;                // ❌ 읽는 코드 없음
```

`PickupClass`가 **`TSoftClassPtr`** 인 것이 6차 검수 반영분이다 — config에 `TSubclassOf`를 쓰면 BP 클래스를 `.ini` 문자열로 못 푼다.

`PickupClass`가 비면 `AEPPickup::StaticClass()`로 폴백 (`EPItemSpawner.cpp:52-53`). 현재 `DefaultGame.ini:39`는 `/Script/EmploymentProj.EPPickup` — C++ 클래스 직접 지정.

### ★ `EP.Loot.RollTable` — 집계만 하고 **출력이 없다**

```cpp
// EPLootDebugCommands.cpp:51-68
int32 Empty = 0, Failed = 0;
TMap<FName, int32> PerItem;
TArray<int32> PerRarity;
for (int32 i=0; i<Count; i++) { ... 집계 ... }
}          // ← 69행. 람다가 여기서 끝난다. UE_LOG가 한 줄도 없다
```

**변수가 전부 대입되므로 미사용 경고도 안 뜬다.** 커맨드는 정상 실행되고 아무것도 안 찍는다.

**이것 때문에 완료 조건 2를 수행할 수 없다.** 중첩 롤과 `EmptyWeight` 규칙이 확률적으로 맞는지 아직 아무도 모른다.

> 침묵이 증명하는 것도 있다 — `.cpp:44`의 "LootTable을 찾을 수 없습니다" Error가 안 떴으므로 **AssetManager 등록·에디터 재시작·`LT_*` 에셋 생성이 전부 성공했다**(완료 조건 3). 실패했다면 그 Error가 떴다.

필요한 것은 `EPLootDebugCommands.cpp:68` 뒤의 출력 블록 하나다. 형식은 `05_Loot_01_Spawner.md:1395-1404`에 있다. **분모가 `Count`가 아니라 `Count - Empty - Failed`** 인 것이 핵심 (함정 #20).

### `EP.Loot.Respawn` / `EP.Loot.List`

**상태: 완료**

- `Respawn` — `NM_Client` 차단 후 전 스포너 `ClearLoot()` → `SpawnLoot()` (`.cpp:72-93`)
- `List` — 서버·클라 양쪽 실행 가능. 대조가 곧 검증이다 (`.cpp:95-123`)
- 셋 다 `ECVF_Cheat` + `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)` (`.cpp:13`)

#### 잔여 — `List`의 클라 분기가 `Charges`를 그대로 찍는다

`.cpp:117-118`. `State`는 의도적으로 비복제라(함정 #10) 클라에서는 **항상 0**이다. 헤더에 `[server-only]` 표시는 붙였지만(`:103-104`) 값도 `Claimed`처럼 `-`로 찍는 편이 낫다 — 안 그러면 서버/클라 대조 시 0이 복제 버그로 보인다.

---

## 에셋

| | 상태 |
|---|---|
| `Content/Data/Loot/LT_Floor_Common` | ✅ |
| `Content/Data/Loot/LT_Rarity_Common` / `_Uncommon` / `_Rare` | ✅ |
| `DefaultGame.ini:15` — `LootTable` PrimaryAssetType 등록 (`Directories=/Game/Data/Loot`, `bIsEditorOnly=False`, `CookRule=AlwaysCook`) | ✅ |
| `Content/Loot/BP_EPItemSpawner` | ✅ |
| DA 6종의 `WorldMesh` | ✅ 채움 — 단 **`/Game/_Import/SurvivalProps/Models/*`를 가리킨다.** 아래 |
| DA의 `Icon` | ✅ 채움 (임시 — 겹치지 않게 임의 할당). **읽는 코드는 Step 04부터** |

> **★ `_Import/`는 `.gitignore` 대상이다.** 클론하면 `WorldMesh` 6개가 전부 없어서 픽업이 플레이스홀더 큐브로 나온다. 소프트 포인터 + 폴백이라 크래시는 없다. **팩 재다운로드로 복구하는 것이 확정된 방침이다** (2026-08-02 사용자 결정). 재임포트 시 경로가 `/Game/_Import/SurvivalProps/`여야 참조가 살아난다.

---

## Step 02에 넘기는 것

| | |
|---|---|
| `bClaimed` / `IsClaimed()` | 동시 획득 경쟁. Step 02 02-3·02-5 |
| 픽업의 전 채널 `ECR_Ignore` | Step 02가 `EP_TraceChannel_Interact` **하나만** 연다. **이 줄을 "정리"하면 트레이스가 아무것도 못 맞힌다** (Step 02 함정 #5c) |
| `AEPPickup::Destroy()` | Step 02 `OnInteract()`이 부른다. Step 03에서 `AddItem()` 성공 갈래로 바뀌는 유일한 지점 |
| `EP.Loot.List`의 `Claimed` 열 | Step 02 선점 검증 수단 |

---

## 남은 작업 (우선순위)

| # | 무엇 | 왜 |
|---|---|---|
| **1** | `EP.Loot.RollTable` 출력 블록 (`EPLootDebugCommands.cpp:68` 뒤) | **완료 조건 2가 미검증이다.** 확률은 눈으로 못 믿는다 |
| 2 | `EPLootTable.cpp:13` — `!Table`과 `Depth > MaxDepth` 분리 | null과 순환이 같은 로그를 낸다 |
| 3 | `bEnableLootDebugLog` / `bEnableSpawnerDebugDraw` 소비 또는 삭제 | 죽은 설정 필드 2개 |
| 4 | `EP.Loot.List` 클라 분기의 `Charges` → `-` | 대조 시 오독 유발 |
| 5 | `LootEntryAmbigious` 오타 | 무해 |

**1번을 하기 전에는 "Step 01 완료"라고 쓰지 않는 편이 낫다.** 나머지 넷은 Step 02와 병행해도 된다.
