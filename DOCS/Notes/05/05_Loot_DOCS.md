# Loot 총괄 기획서 (EmploymentProj)

> 아이템 획득 파이프라인 전체 그림을 담은 마스터 문서.
> 세부 구현: `05_Loot_01_Spawner.md` ~ / 진행 상황: `LOOT_STATUS.md`
> 관련: `DOCS/Mine/Item.md`(아이템 아키텍처), `DOCS/GAME.md`(자판기·경제 기획)

---

## 1. 범위

맵에 아이템이 존재하고 → 플레이어가 줍고 → 인벤토리에 보관하는 데까지.

```
[스포너]  맵 배치, 확률 테이블로 판정      ← 이번 단계
    ↓ 서버 스폰
[픽업]    바닥에 놓인 아이템 액터           ← 이번 단계
    ↕ 상호작용(F) / 버리기(G)              ← 이번 단계 (양방향)
[인벤토리] 서버 권한 보관 + UI              ← 이번 단계
    ↓ 장착
[무기]    AEPWeapon 스폰·부착              ← 이번 단계 (기존 흐름 이관)
    ─────────────────────────────
[컨테이너] 공구상자/구급상자/가방 + 검색시간  ← 추후 (§7)
[자판기]   돈 투입 → 5초 → 배출            ← 추후 (§7)
```

**이번 단계에서 하지 않는 것:** 월드 컨테이너(§7-1), 자판기, 무기 부착물(§7-3), 드래그앤드롭 UI, 2D 격자(테트리스) 인벤토리, 소모품 사용, 재장전의 탄약 소비, 사망 시 드랍, 무기 2정 이상 슬롯.

> **인벤토리 용량은 아이템별 칸 수의 합산**이다(§4-6) — 무게 시스템과 동형이고 2D 배치 문제가 아니다.
>
> **배낭은 이번 범위에 포함된다.** 착용 컨테이너마다 칸 수가 **통합되지 않고 각자 독립**이며(**본체 자체는 0칸이다** — 13차), 배낭을 버리면 안의 아이템이 같이 나간다(GAME.md). 이 때문에 엔트리가 `ParentEntryId`를 갖고, 그 구조가 §7-3 부착물과 동일하다.

---

## 2. 현재 상태 진단 — 데이터 계층이 전부 데드코드다

`CLAUDE.md`와 `DOCS/Mine/Item.md`에 아이템 3-tier가 설계되어 있으나, **2026-07-26 기준 어느 것도 실행되지 않는다.**

| 심볼 | 상태 |
|---|---|
| `FEPItemData` (DataTable Row) | 구조체 선언만 존재. **참조하는 코드 0** |
| `DT_Items.uasset` | 에셋 존재. **읽는 코드 0** |
| `UEPItemDefinition` | 클래스 존재. `DA_AK74_*` 3종 에셋 존재 |
| `UEPItemInstance::CreateInstance()` | **호출처 0** |
| `UEPWeaponInstance::CreateWeaponInstance()` | **호출처 0** |
| `UEPGameInstance` | `Init()` 오버라이드만 있는 빈 껍데기 |

실제 무기는 `EPGameMode.cpp:81`에서 `DefaultWeaponClass`(액터 클래스)를 직접 스폰하는 것으로 끝난다. 즉 `ItemId → Definition` 조회 경로가 프로젝트에 **존재하지 않는다.**

> 이 단계의 숨은 목표: 설계만 되어 있고 안 돌아가는 3-tier를 실제로 가동시키는 것. 포트폴리오 리뷰에서 코드를 열어봤을 때 "설계는 있는데 안 쓴다"로 읽히는 상태를 해소한다.

---

## 3. 순서 결정 — 토대(Step 00) 먼저, 그 다음 스포너

**결정: 아이템 계층 정비 → 스포너(+픽업) → 상호작용 → 인벤토리 → UI → 장비**
(= §5의 Step 00 → 01 → 02 → 03 → 04 → 05)

### 왜 Step 00이 앞에 오는가

§2가 진단한 대로 아이템 데이터 계층은 **전부 데드코드**다. 스포너가 `ItemId`로 아이템을 뽑으려면 `ItemId → DataTable → Definition` 조회 경로와 상태 초기화가 **먼저** 있어야 한다. 이걸 Step 01 안에서 같이 만들면 "스포너 작업"이 사실상 "데이터 계층 재설계 + 스포너"가 되어, 확률이 안 맞을 때 원인이 테이블인지 조회 경로인지 구분이 안 된다.

Step 00은 눈에 보이는 결과가 없는 대신 **콘솔 커맨드 하나로 독립 검증된다** — `ItemId`를 주면 올바른 초기 `FEPItemState`가 나오는가. 그게 확인된 뒤에 스포너를 붙인다.

### 왜 인벤토리가 아니라 스포너가 그 다음인가

| 근거 | 설명 |
|---|---|
| 데이터 흐름의 상류 | 인벤토리는 아이템을 **받는** 쪽이다. 상류가 먼저 있으면 인벤토리를 실제 데이터로 검증할 수 있다 |
| 임시 코드가 안 생긴다 | 인벤토리를 먼저 만들면 "테스트용 아이템 지급" 코드를 쓰고 버려야 한다 |
| 독립 검증이 된다 | 스포너는 인벤토리 없이도 완결된다 — 확률대로 나오는지 로그 집계로 확인 가능 |
| 3-tier의 첫 **게임플레이** 소비자 | Step 00이 조회 경로를 만들었다면, `Definition → WorldMesh → 월드에 보임`까지 실제로 도는 건 여기가 처음이다 |
| 결과가 빨리 보인다 | 맵에 아이템이 깔리는 건 눈에 보이는 진전이다 |

**반론과 처리:** 스포너만 있으면 주울 수 없다 → Step 01은 "스폰되어 보인다"까지가 완료 조건이다. 줍는 건 Step 02에서 붙인다.

---

## 4. 아키텍처 결정사항

### 4-1. 아이템 인프라 — 조회 서브시스템 **하나**, 개체 상태는 **값 타입**

| 계층 | 형태 | 역할 |
|---|---|---|
| `UEPItemDefinition` (+서브클래스) | `UPrimaryDataAsset` — **UObject 계층 유지** | 타입 데이터와 다형성. `ItemId`별 1개, 불변 |
| `FEPItemState` | **`USTRUCT` (순수 값 타입)** | 개체별 런타임 상태. 엔트리·픽업에 **내장** |
| `UEPItemDefinitionSubsystem` | `UGameInstanceSubsystem` | `ItemId → FEPItemData / UEPItemDefinition` 조회 |

#### ★ 개체 상태를 `UObject`로 두지 않는다

기존 설계는 `UEPItemInstance`(UObject) + `UEPItemInstanceSubsystem`(소유) + `int32` 핸들이었다. **폐기한다.**

| 항목 | 실제 내용 |
|---|---|
| `UEPWeaponInstance`가 담는 것 | `CurrentAmmo`(int) + `Durability`(float) — **숫자 둘** |
| 타입별 다형성 | **이미 `UEPItemDefinition`에 있다.** 인스턴스에 남는 건 값뿐 |
| 진짜 개체 행동 (`CurrentSpread` / `LastFireTime` / `ConsecutiveShots`) | **이미 `AEPWeapon` 액터에 있고 복제도 하지 않는다** |
| 부착물·모듈 트리 (Lyra가 UObject를 쓰는 제1 이유) | **계획에 있다.** 그런데 트리는 UObject의 비용이 **최대**가 되는 지점이다 — 근거는 §7-3 |
| 중첩 컨테이너 (배낭) | **계획에 있다.** 부착물과 **같은 구조**(부모를 갖는 엔트리)로 풀린다 — §4-6 |

```cpp
// 개체 상태 — 순수 값 타입. Outer도, 핸들도, 소유 서브시스템도 없다
USTRUCT()
struct FEPItemState
{
    GENERATED_BODY()

    // 이 개체가 담고 있는 소모 단위
    //   무기      : 장전된 발수        탄약상자 : 남은 발수
    //   현금뭉치  : 금액               소모품   : 남은 사용 횟수
    UPROPERTY() int32 Charges    = 0;

    UPROPERTY() float Durability = 100.f;
};
```

**이관이 값 대입으로 끝난다.** `Entry.State = Pickup->State;` / `Pickup->State = Entry.State;` — 소유권 공백이 없고, 이관 프로토콜도 `EndPlay` 정리도 고아 누수도 성립하지 않는다. 버린 무기의 잔탄이 보존되는 이유가 **"규칙을 지켜서"가 아니라 "값을 복사했으니까"** 가 된다.

부수 효과로 **가방 속 두 번째 소총의 잔탄을 UI에 표시할 수 있게 된다.** 핸들 방식에서는 서버 전용 인스턴스라 클라가 읽을 방법이 없었고, GAS `Ammo` 어트리뷰트는 장착 무기 하나만 커버했다.

> **비대화의 전환 기준:** `FEPItemState`는 지금 8바이트다. 대역폭이 아니라 **"모든 아이템이 다른 타입의 필드값을 지불한다"** 가 진짜 비용이고, 필드 4개에서는 보이지 않다가 타입 전용 필드가 8~10개쯤 되면 추해진다. **세 번째 아이템 카테고리가 자기 전용 필드를 요구하면 그때 `FInstancedStruct`로 간다.** 지금 도입하면 프로퍼티 델타 상실 + 엔트리당 `UScriptStruct*` 전송 + BP/DB 복잡도를 사놓고 아무것도 못 받는다.

#### ★ 아이템은 스택되지 않는다 (확정)

붕대 3개는 겹치지 않고 **엔트리 3개**다. 대신 아이템마다 **차지하는 칸 수**가 있고, 인벤토리는 **수용 가능한 총 칸 수**를 가진다. 무게 시스템과 같은 합산 방식이다 (§4-6).

| 개념 | 표현 |
|---|---|
| 아이템의 칸 수 | `FEPItemData::SlotSize` — **이미 있는 필드**. 지금까지 안 읽었을 뿐 |
| 인벤토리 용량 | `UEPInventoryComponent::MaxSlots` |
| 점유량 | `UsedSlots = Σ SlotSize` — 파생값, 복제하지 않는다 |

탄약은 **탄약상자**라는 아이템이 발수를 `Charges`로 들고 있고, 그것으로 탄창을 채운다. 돈도 같은 방식(현금뭉치 하나가 금액을 `Charges`로 보유)이라 **엔트리가 폭발하지 않는다.**

> **`FEPItemData::MaxStack`은 남겨두되 읽지 않는다.** 나중에 스택을 되살릴 여지를 위한 예약 필드다. 되살릴 때는 `FEPInventoryEntry`에 `Quantity`를 추가하고 병합 로직을 넣으면 되며, **복제 구조(FastArray + POD)는 그대로 유효하다.**

#### `UEPItemDefinitionSubsystem` — 조회

- `Initialize()`에서 `DT_Items`를 1회 순회해 `TMap<FName, FEPItemData>`를 캐시. 매 조회마다 `FindRow`를 돌지 않는다.
- **★ 행 포인터(`FEPItemData*`)를 캐시하지 않는다.** `FindRow`가 돌려주는 포인터는 DataTable의 `RowMap` 내부를 가리키므로, 에디터에서 DT를 리임포트하거나 핫리로드하면 재할당되어 **댕글링**한다. 패키지 빌드에서는 재현되지 않고 에디터에서만 나는 종류라 원인 추적이 오래 걸린다. `FEPItemData`는 작으므로 **값으로 복사**해 담는다.
- DataTable 경로는 하드코딩하지 않고 `UEPCombatDeveloperSettings`처럼 `UDeveloperSettings`에 노출한다. 타입은 `FName` 문자열 경로가 아니라 **`TSoftObjectPtr<UDataTable>`** — 문자열 경로는 에셋 이동·리네임에 침묵으로 깨진다 (§9).
- 데디케이티드 서버에서도 동작해야 한다(스폰 판정이 서버).

##### ★ Definition은 상주시킨다 — 소프트 참조는 시각 에셋에만

`FEPItemData::ItemDefinition`이 `TSoftObjectPtr`이라 "Definition도 비동기 로드"로 읽히기 쉬우나, **그러면 아이템을 만들 수 없다.** 픽업 획득은 RPC 응답 안에서 성패가 결정돼야 하는 동기 경로인데, §4-9의 `Definition->InitState()`도 `SlotSize` 조회(§4-6)도 Definition이 메모리에 있어야 한다. 로드를 기다리는 사이에 "줍기 성공/실패"를 유보할 수 없다.

| 대상 | 정책 |
|---|---|
| `UEPItemDefinition` / `UEPWeaponDefinition` 에셋 | **매치 시작 전 전량 상주.** `UEPItemDefinitionSubsystem::Initialize()`에서 AssetManager로 `EPItemDefinition` PrimaryAssetType을 일괄 로드 |
| `WorldMesh` / `Icon` / `WeaponMesh` | **소프트 유지.** 픽업 메시 표시·UI 아이콘처럼 지연이 허용되는 지점에서 `FStreamableManager` 비동기 로드, 콜백에서 세팅 |

- Definition은 수치·참조만 담은 메타데이터라 개당 수 KB다. 수백 개여도 상주 비용이 문제되지 않는다
- 데디케이티드 서버는 Definition만 로드하고 시각 에셋 로드는 건너뛴다 — 이 분리가 성립하는 이유가 위 표다
- `AssetManager` 설정(Project Settings → Asset Manager)에 `EPItemDefinition` PrimaryAssetType을 등록해야 한다. Step 00의 완료 조건에 포함

#### 개체 식별자: `FEPInventoryEntry::EntryId` (`int32`)

**배열 인덱스**를 식별자로 쓰면 안 된다 — 제거·정렬로 인덱스가 밀리는 사이에 클라 요청이 도착하면 엉뚱한 아이템을 버린다. 그리고 `FFastArraySerializer`는 **클라이언트 배열의 순서가 서버와 같다는 보장을 하지 않는다**(`FastArraySerializer.h:54`). 그래서 명시적 키가 필요하다.

```
UEPInventoryComponent
├─ int32 NextEntryId = 1        ← 서버가 단조 증가로 발급
└─ 재번호하지 않는다. 매치 내 유일하고, 제거해도 다른 엔트리의 값이 바뀌지 않는다
```

| 쓰이는 곳 | |
|---|---|
| 드랍 RPC | `Server_DropItem(int32 EntryId)` |
| 장착 RPC / 슬롯 표현(`SlotId`) | §4-8 |
| UI 정렬 키 | **`SortKey`(엔트리 필드, 서버 발급)** — 11차. `InsertEntry`가 "형제 맨 뒤"로 발급하므로 **`EntryId` 오름차순이 기본값으로 남는다.** 정렬은 `GetSortedContents`가 하고 **클라·서버가 같은 함수를 쓴다.** `EntryId`는 식별자이지 순서가 아니다 (§4-6 · `05_Loot_03_Inventory.md` 03-1) |

> **`FGuid InstanceId`와 `SchemaVersion`은 제거한다** (`EPItemInstance.h`). 전자는 읽는 코드가 없고, 후자는 **아이템의 속성이 아니라 세이브 포맷의 속성**이다 — 한 세이브 안의 모든 아이템을 같은 빌드가 쓰므로 `USaveGame`/DB 행 봉투에 하나만 둔다. DB 스태시의 영구 식별자는 **저장 시점에 발급**하고, 복사 탐지 안티치트가 실제로 필요해지면 그때 POD에 필드를 추가한다 — 세이브 포맷 범프이지 아키텍처 변경이 아니다.

### 4-2. 루트 테이블 — 가중치 + **중첩**

```
UEPLootTable : UPrimaryDataAsset
├─ TArray<FEPLootEntry> Entries
│    ├─ float Weight                    (상대 가중치)
│    ├─ FName ItemId                    ┐ 둘 중 하나만 채운다
│    └─ TObjectPtr<UEPLootTable> SubTable ┘ (유효하면 재귀 롤. 하드 참조)
└─ float EmptyWeight                    (아무것도 안 나올 가중치)
```

> **수량 필드가 없다.** 스택이 없으므로 롤 결과는 **아이템 하나**다. "탄약 20~60발"은 수량이 아니라 **탄약상자 하나의 `Charges`** 이고, 그 초기값은 `Definition->InitState()`가 정한다 (§4-9). 스포너에서 굴릴 때마다 발수를 흔들고 싶다는 요구가 생기면 그때 `UEPItemDefinition`에 범위를 넣는다 — 루트 테이블이 아이템 타입별 상태를 알게 하지 않는다.

**퍼센트가 아니라 가중치를 쓰는 이유:** 항목을 추가·삭제할 때마다 합계 100을 다시 맞출 필요가 없다.

**중첩이 반드시 필요한 이유 (★):** GAME.md의 자판기 표는 **아이템별이 아니라 등급별** 확률이다.

| 등급 | 확률 |
|---|---|
| 일반 (탄약, 붕대, 잡템) | 50% |
| 고급 (회복키트, 좋은 탄약) | 30% |
| 희귀 (무기, 고가 아이템) | 15% |
| 전설 (고급 무기, 최고가) | 5% |

이걸 평면(flat) 가중치로 풀어쓰면 **일반 등급에 아이템을 하나 추가할 때마다 다른 등급의 비율이 깎인다.** 기획 의도가 데이터 구조에 의해 침식되는 것이다. 중첩 구조면 등급 노드에서 50/30/15/5를 고정하고, 하위 테이블에서 아이템을 자유롭게 늘려도 등급 비율이 불변이다.

```
LT_VendingMachine
├─ SubTable: LT_Rarity_Common     Weight 50
├─ SubTable: LT_Rarity_Uncommon   Weight 30
├─ SubTable: LT_Rarity_Rare       Weight 15
└─ SubTable: LT_Rarity_Legendary  Weight  5
      └─ LT_Rarity_Common
           ├─ ItemId: AmmoBox_545  Weight 1
           ├─ ItemId: Bandage      Weight 1
           └─ ItemId: Scrap        Weight 1   ← 여기에 추가해도 "일반 50%"는 그대로
```

등급 테이블은 자판기·컨테이너·바닥 스포너가 **공유**한다. 오브젝트별로 다른 건 "어느 등급을 몇 %로 뽑느냐"이고, 등급 안의 아이템 풀은 대개 같기 때문이다. 구급상자면 `LT_Rarity_Medical_*`처럼 갈래를 나눈다.

> 구현 주의: 재귀 롤이므로 **순환 참조 방어**가 필요하다. 롤 깊이 상한(예: 8)을 두고 초과 시 경고 로그 후 중단한다. 에디터에서 A→B→A로 엮는 실수는 반드시 나온다.

**`EmptyWeight`가 필요한 이유:** 타르코프식 파밍은 "빈 스폰 지점"이 있어야 긴장감이 산다. 모든 스포너가 100% 무언가를 뱉으면 맵을 도는 재미가 없다.

**★ `EmptyWeight`는 루트 테이블에서만 유효하다.** 하위 등급 테이블에도 적용되면 "일반 등급 50%"를 뽑고 **그 안에서 또 빈 결과**가 나와, 실제 일반 확률이 50% 미만이 된다. 기획표가 조용히 침식되고 원인을 찾기 어렵다.

```
Roll(Table, Depth):
    if Depth > 0 && Table.EmptyWeight > 0:
        경고 로그 ("하위 테이블의 EmptyWeight는 무시됨") 후 0으로 취급
```

빈 결과가 필요하면 루트에서만 지정한다. 등급별로 빈 확률을 다르게 주고 싶다는 요구가 생기면, 그건 `EmptyWeight`가 아니라 **하위 테이블에 "아무것도 아님" 엔트리**를 명시적으로 넣는 방식으로 표현해야 한다 — 의도가 데이터에 드러난다.

#### ★ 로딩 정책 — Definition과 같이 AssetManager에 등록한다

`UEPLootTable`도 `UPrimaryDataAsset`이다. 실사용 경로만 보면 스포너가 하드 포인터로 들고 있어 레벨과 함께 로드되므로 문제가 없어 보이지만, **`EP.Loot.RollTable <이름> <횟수>`(§9)는 이름으로 찾는다.**

- 어떤 스포너도 참조하지 않는 테이블(신규 작성 중, 자판기 전용, 등급 테이블만 단독 검증)은 **메모리에 없어서 커맨드가 못 찾는다**
- 확률 검증은 이 커맨드가 유일한 수단인데(§10 "확률은 눈으로 못 믿는다"), 정작 검증하고 싶은 새 테이블에서 안 먹는다

| 대상 | 정책 |
|---|---|
| `UEPLootTable` | `EPLootTable` PrimaryAssetType으로 AssetManager에 등록. 커맨드는 `UAssetManager::GetPrimaryAssetObject`로 이름 조회 |
| `SubTable` | `TObjectPtr` 하드 참조 유지 — 루트가 로드되면 등급 테이블도 따라 로드된다 |

루트 테이블 전량 상주가 부담되면 커맨드에서만 `LoadPrimaryAsset`으로 온디맨드 로드해도 된다. 어느 쪽이든 **"이름 → 에셋" 해석 경로를 Step 01에서 확보해야** 한다는 점이 핵심이다.

**이 DataAsset을 지금 만드는 것의 핵심 가치:** 컨테이너(§7-1)와 자판기(§7-2)가 **같은 타입을 그대로 재사용**한다. 에셋만 늘리면 되고 코드는 안 늘어난다. GAME.md의 "자판기 타입별로 다른 아이템 테이블(DataAsset)" 확장 요구가 여기서 이미 충족된다.

### 4-3. 스포너 — 맵에 놓는 마커

```
AEPItemSpawner : AActor
├─ UEPLootTable* LootTable
├─ int32 RollCount = 1          (몇 번 굴릴지)
├─ float SpawnRadius            (0이면 정확히 스포너 위치)
└─ bool bAlignToGround
```

- **서버 전용 판정.** `HasAuthority()` 확인 후 굴리고 `AEPPickup`을 스폰한다. 픽업 액터가 복제되므로 클라 동기화는 자동으로 따라온다 — 스포너 자신은 복제할 필요가 없다.
- 스포너는 **런타임에 보이지 않는다.** 에디터 빌보드 아이콘만 둔다.
- 바닥 정렬은 스포너 아래로 짧은 라인 트레이스를 쏴서 접지시킨다. 레벨 디자이너가 높이를 정밀하게 안 맞춰도 되게 하기 위함.

**스폰 시점 — `BeginPlay`가 아니라 GameMode가 지시한다.**

이 프로젝트의 GameMode는 `MatchState`(Waiting → Playing → Ended)를 관리한다. 스포너가 각자 `BeginPlay`에서 굴리면 매치 시작 전에 아이템이 이미 깔려 있고, 라운드 재시작 시 재스폰 경로가 없다.

```
AEPGameMode::HandleMatchHasStarted()
    → 월드의 모든 AEPItemSpawner 순회 → SpawnLoot() 호출
    → Super::HandleMatchHasStarted()   ← 플레이어 리스타트는 그 다음
```

- 스포너는 `SpawnLoot()` / `ClearLoot()` 두 개의 서버 함수만 노출하고, 호출 시점은 GameMode가 결정한다. 라운드 재시작·테스트용 재굴림이 공짜로 따라온다.
- **★ 호출 순서: `Super::` 앞에서 뿌린다.** `AGameMode::HandleMatchHasStarted()`는 같은 전이에서 대기 중인 플레이어들을 리스타트시킨다(→ `HandleStartingNewPlayer` → 기본 지급, §4-8). 루트를 나중에 뿌리면 플레이어가 이미 서 있는 자리에 아이템이 스폰되어 겹칠 수 있고, "시작 직후 잠깐 맵이 비어 있는" 창이 생긴다. 이 프로젝트의 GameMode는 `AGameMode` 파생이고 `HandleMatchHasStarted()`를 이미 오버라이드하고 있다(`EPGameMode.h:70`).
- 스폰한 픽업의 핸들을 스포너가 약참조로 들고 있으면 `ClearLoot()`에서 자기가 뿌린 것만 정리할 수 있다. 플레이어가 버린 아이템까지 지우지 않기 위함.

### 4-4. 픽업 액터 — 무엇을 복제할 것인가

```
AEPPickup : AActor
├─ FName ItemId          (Replicated, OnRep로 메시 적용)   ← 클라가 알아야 하는 것
├─ FEPItemState State    ★ 서버 전용. 복제하지 않는다      ← 소유자만 알아야 하는 것
└─ UStaticMeshComponent* Mesh
```

**★ 개체 상태를 복제하지 않는 이유는 비용이 아니라 정보 은폐다.**

`FEPItemState`는 8바이트라 대역폭은 이유가 못 된다. 문제는 **바닥 무기의 잔탄이 복제되면 치트 클라이언트가 릴러번시 범위 내 모든 픽업의 잔탄을 읽어 "어디서 얼마 전에 교전이 있었는지"를 추론한다**는 것이다. `12/30`짜리 라이플이 바닥에 있다 = **여기서 누가 죽었다.** GAME.md가 두 번 명시한 정보 은폐 기둥(플레이어 수 비공개, 사망 여부 미노출, 킬 피드백은 킬러에게만)을 **사고로** 뒤집는다.

쪼개는 축이 "무엇인가 / 어떤 상태인가"가 아니라 **"클라가 알아도 되는가"** 이고, 양쪽 다 값 타입이라 값 복사 이관의 이점은 하나도 잃지 않는다. 누출이 *규율*이 아니라 *구조*로 불가능해진다.

> **기각한 대안:** ① 상태를 복제하고 노출을 수용 → 정보 은폐와 정면 충돌. ② 픽업에 서버 전용 `ItemId`를 하나 더 → **"정체성의 진실이 두 곳"**, 이 설계가 죽이려던 바로 그 버그류. ③ 커스텀 `NetSerialize`로 필드별 조건 → 8바이트 아끼자고 직렬화기를 손으로 쓴다.

| 픽업의 출처 | `State` | 획득 시 서버 동작 |
|---|---|---|
| 스포너가 뿌린 것 | `Definition->InitState()`로 초기화 (스폰 시점) | `Entry.State = Pickup->State` |
| 플레이어가 버린 것 (§4-7) | 버릴 때 엔트리에서 **값 복사** | 동일 |

**두 경로가 같다.** 핸들 유무로 갈리던 분기가 사라진다 — 픽업은 언제나 유효한 `State`를 들고 있고, 획득은 언제나 값 대입이다. 버린 무기의 잔탄이 보존되는 것은 규칙을 지켜서가 아니라 값을 복사했기 때문이다.

- 메시: `OnRep_ItemId`에서 레지스트리 조회 → `Definition->WorldMesh` 비동기 로드 → 도착하면 세팅. 로드 전에는 공용 플레이스홀더 메시를 쓴다. **`WorldMesh`가 비어 있는 아이템이 대부분일 것이므로**(현재 무기 외 메시 없음) 플레이스홀더 박스는 선택이 아니라 필수다.

**동시 획득 경쟁 (★ 멀티 필수 처리)**

두 플레이어가 같은 픽업에 동시에 상호작용하는 상황은 반드시 발생한다. 서버에서 다음 순서로 처리한다.

```
UEPGA_Interact::ActivateAbility  [서버]        ← 호출자. 대상이 무엇인지 모른다
  1. Target이 유효한가 (IsValid && !IsActorBeingDestroyed)
  2. Target이 IEPInteractable을 구현하는가        ← 아무 액터나 보낼 수 있다
  3. ★ 거리 재검증: DistSq(Character, Target) <= (InteractRange + 여유)^2
       클라의 트레이스 결과를 신뢰하지 않는다 (§4-5)
  4. ★ IEPInteractable::CanInteract(Interactor, OutReason) == true 인가
       bClaimed, DropCooldown, 컨테이너 "이미 검색됨", 자판기 돈 부족이 전부 여기서
  5. IEPInteractable::OnInteract(Interactor, OutReason) 이 true를 돌려줬는가

  하나라도 실패 → Client_OnInteractFailed(OutReason). 조용히 return 하지 않는다

AEPPickup::OnInteract  [대상]                   ← 선점과 트랜잭션은 여기 안에서만
  a. bClaimed = true                            ← 첫 줄. 인벤토리 삽입보다 먼저
  b. ① EntryId = TryAutoEquip(Payload)          ← 슬롯이 비어 있으면 거기로 (§4-6)
        아이템의 SlotPriority를 훑어 첫 빈 슬롯. AddSubtree(INDEX_NONE, S, In) 한 번으로 들어간다
        ★ 칸 검사를 지나지 않는다 — 착용은 칸을 안 먹는다
     ② 실패했으면 컨테이너                       ← GetInsertionOrder()대로
        for (C : GetInsertionOrder()) if ((EntryId = AddSubtree(C, NAME_None, Payload)) != NONE) break;
        본체(0칸) → 외투 → 상의 → 하의 → 배낭 → 팔목.  ★ 본체가 먼저다 — 뒤집으면 배낭부터 찬다
  c. EntryId != INDEX_NONE → Destroy(); return true
     EntryId == INDEX_NONE → bClaimed = false; OutReason 채우고 return false

  ★★ AddItem이 아니라 AddSubtree다. 픽업은 서브트리를 들고 있고(배낭+내용물),
     AddItem으로 받으면 안의 아이템이 전부 증발한다 (함정 3c)
```

> **`bClaimed`는 호출자의 절차에 없다.** `AEPPickup`의 `private`이고(`EPPickup.h:45`), `IEPInteractable`에는 그 개념이 아예 없다 — 자판기에는 `bClaimed`가 없다. **호출자가 알아야 하는 건 "선점됐는가"가 아니라 "된다/안 된다"뿐이다.** 확인은 `CanInteract`가, 마킹과 되돌리기는 `OnInteract`이 한다 — **되돌릴 값을 세운 함수가 되돌린다.**

- **★ 3·4단계를 빠뜨리면 서버 검증이 사실상 없어진다.** §4-5가 "서버가 거리와 대상 유효성을 재검증한다", "`CanInteract()`는 서버가 다시 호출해 판정한다"고 선언해놓고 이 절차에 호출이 없으면, **클라이언트가 프롬프트를 안 그릴 뿐 RPC는 그대로 통과한다.** 특히 §4-7의 `DropCooldown`(버린 직후 0.5초 재획득 금지)이 `CanInteract()`로 구현되므로, 4단계가 없으면 쿨다운이 서버에서 강제되지 않는다. 이 절차는 구현 체크리스트로 읽히는 자리라 누락이 그대로 코드가 된다.
- **획득은 전부 아니면 전무다.** 스택이 없으므로 픽업 하나 = 아이템 하나이고, 칸이 모자라면 아무것도 들어가지 않는다. 부분 획득 경로가 없어져 `bClaimed`를 되돌리는 갈래도 실패 하나뿐이다.
- `bClaimed`는 **복제하지 않는다.** 서버 내부 상태이고, 결과는 액터 파괴로 클라에 전달된다.
- 늦은 요청은 조용히 무시하지 않고 요청자에게 실패 사유를 회신한다(`Client_OnInteractFailed`). 아무 반응이 없으면 플레이어는 입력이 씹혔다고 느낀다.

**네트워크 예산**

맵에 픽업이 수십~수백 개 상주하므로 관련성 설정을 반드시 잡는다.

| 설정 | 값 | 이유 |
|---|---|---|
| `bReplicates` | true | 상태 복제 필요 |
| `PrimaryActorTick.bCanEverTick` | **false** | 픽업은 스스로 할 일이 없다. 수백 개면 유의미하다 |
| `bAlwaysRelevant` | **false** | 기본값이지만 명시. true면 전 맵 픽업이 모든 클라에 복제된다 |
| `SetNetCullDistanceSquared()` | **제곱값**. 5000cm를 원하면 `25000000.f` | 멀리 있는 픽업은 복제 대상에서 제외 |
| `SetReplicateMovement` | **false** | 정적으로 놓인 아이템이라 이동 복제 불필요 |
| `NetDormancy` | **`DORM_Initial`** | ★ 아래 참조. 스택이 없어 복제 상태가 아예 불변이다 |

> **★ 5.5부터 직접 대입은 deprecated다.** `NetCullDistanceSquared` 필드는 `UE_DEPRECATED(5.5, "Public access to NetCullDistanceSquared has been deprecated...")`가 붙어 있다(`Actor.h:869`). 생성자에서 `NetCullDistanceSquared = ...`로 쓰면 5.7에서 경고가 난다. **`SetNetCullDistanceSquared(25000000.f)`** 를 쓴다. 같은 이유로 `NetUpdateFrequency`도 `SetNetUpdateFrequency()`다(`Actor.h:874`). `bReplicates` / `bAlwaysRelevant` / `NetDormancy`는 deprecated가 아니라 생성자 대입 그대로 둔다.

> **단위 함정:** 기본값은 `225000000`(= 15000cm의 제곱)이다. 여기에 `5000`을 그대로 넣으면 컬링 거리가 √5000 ≈ **70cm**가 되어 픽업이 코앞에서만 보인다. "왜 아이템이 안 보이지"로 한참 헤매는 대표적인 실수다.

**★ Net Dormancy가 이 액터에 정확히 맞는 이유**

스택이 없어진 지금 픽업의 **복제 상태는 완전히 불변이다.** 클라에 나가는 값은 `ItemId` 하나뿐이고 그것은 스폰 시점에 정해져 파괴될 때까지 바뀌지 않는다(`State`는 서버 전용). `DORM_Initial`로 두면 초기 복제 후 관련성·복제 검사에서 아예 빠진다. 맵에 200~300개가 깔리는 액터에서 이 차이는 크다.

```
스폰       → DORM_Initial (초기 1회 복제 후 휴면)
획득 완료  → Destroy()    ← 파괴는 휴면과 무관하게 전달된다
```

> **`FlushNetDormancy()`를 부를 일이 없다.** 스택이 있던 설계에서는 부분 획득 시 `Quantity`를 낮추고 반드시 이걸 불러야 했고, 빠뜨리면 "서버는 정상인데 클라 화면의 개수만 옛날 값"이라는 재현 까다로운 버그가 났다. 그 함정 자체가 성립하지 않는다.

> 물리로 튕기는 드랍(사망 시 드랍 등)이 생기면 그때만 이동 복제를 켜고 `DORM_Awake`로 둔다. 정지 후 `SetReplicateMovement(false)` + 다시 휴면시키는 게 정석이다.

### 4-5. 상호작용 — 클라 탐지, 서버 검증

```
UEPInteractionComponent (Character에 부착)
├─ Tick: 로컬 컨트롤러만 카메라 전방 트레이스 → 대상 갱신 → HUD 프롬프트
│         SetComponentTickInterval(0.1f)  ← 매 프레임 트레이스 금지
└─ Input(F) → ASC->HandleGameplayEvent(TAG_Ability_Interact, Payload{Target=FocusedActor})
                 → UEPGA_Interact (서버에서 판정)
```

- **트레이스 주기는 0.1초로 낮춘다.** 프롬프트 표시는 100ms 지연이 체감되지 않는데, 매 프레임 트레이스는 그대로 낭비다. 로컬 컨트롤러가 아니면 아예 틱을 끈다(`SetComponentTickEnabled(false)`).
- **클라이언트의 트레이스 결과를 신뢰하지 않는다.** 서버는 `UEPGA_Interact::ActivateAbility`에서 거리(`InteractRange` + 여유)와 대상 유효성을 재검증한다. 사격 경로에서 이미 확립한 원칙(클라는 "요청", 서버가 "결정")과 동일하다.
- 대상 추상화는 **인터페이스**(`IEPInteractable`)로 둔다. 픽업·컨테이너·자판기·탈출지점이 전부 같은 진입점을 쓴다. 이 컴포넌트를 지금 제대로 만들어두면 로드맵 7·11·12가 전부 이걸 재사용한다.

**인터페이스는 처음부터 4개를 갖춘다.** 나중에 인터페이스를 넓히면 이미 구현한 모든 클래스를 건드려야 하므로, 확장 지점을 지금 확보한다.

| 함수 | 이번 단계 픽업의 구현 | 나중에 쓰는 곳 |
|---|---|---|
| `GetInteractText()` | "줍기 — 붕대" | 컨테이너 "검색", 자판기 "1000원 투입" |
| `CanInteract(Interactor, OutReason)` → `bool` | 인벤토리 여유 확인 | 자판기 돈 부족, 컨테이너 이미 검색됨, 탈출 조건 미달 |
| `GetInteractDuration()` | 0 | 컨테이너 검색 N초, 자판기 5초, 탈출 대기 |
| `OnInteract(Interactor, OutReason)` → `bool` | 인벤토리 삽입 후 파괴 | 각 오브젝트의 실제 동작 |

> **인자 이름은 `Instigator`가 아니라 `Interactor`다.** `AActor::Instigator`가 이미 있어서 구현체 멤버 함수 안에서 멤버를 가린다. 그리고 **두 함수 모두 `bool` + `FText& OutReason`을 돌려준다** — 구현체가 넷으로 예고돼 있어서 반환 규약은 지금 정해야 한다. 근거는 `05_Loot_02_Interaction.md` 02-1.

- `CanInteract()`는 **HUD 프롬프트에도 쓴다.** false면 회색 + 사유 표시. 눌러보고 나서 실패하는 것보다 낫다.
- 클라이언트에서도 `CanInteract()`를 호출해 프롬프트를 그리되, **서버가 다시 호출해 판정한다.** 클라 결과는 표시용일 뿐이다.
- **상호작용은 전부 GAS 어빌리티 `UEPGA_Interact` 하나로 간다.** 즉시든 채널링(`GetInteractDuration() > 0`, §7-1)이든 같은 어빌리티다. 컴포넌트는 대상을 고르고 이벤트를 쏘는 데까지만 하고, 판정·채널링·쿨다운은 어빌리티가 한다.
  - 이 프로젝트의 **모든 게임플레이 입력이 이미 어빌리티 태그로 간다**(`EPCharacter.cpp:388-435`). 상호작용만 직접 서버 RPC로 두면 죽음 확인·쿨다운(§4-7 `DropCooldown`)·채널링을 손으로 다시 만들게 되고, 채널링이 붙는 순간 같은 F키가 **대상의 duration 값에 따라 다른 네트워크 경로**를 탄다.
  - 대상 전달에 `FGameplayAbilityTargetData`나 서버 재트레이스는 필요 없다. `FGameplayEventData::Target`이 서버 RPC 파라미터다 — 근거는 `05_Loot_02_Interaction.md` 02-2.
- **상호작용 채널을 여는 곳은 두 번째 소비자부터 콜리전 프리셋이다.** Step 02 시점에는 소비자가 `AEPPickup` 하나라 `SetCollisionResponseToChannel(EP_TraceChannel_Interact, ECR_Block)` 한 줄이 더 싸다. **§7-1 컨테이너 / §7-2 자판기 / 로드맵 12 탈출 지점이 들어올 때** `.ini`에 `EP_Interactable` 프리셋을 만들고 셋이 그것을 쓴다.
  - Lyra가 `Interactable_OverlapDynamic`(`LyraStarterGame/Config/DefaultEngine.ini:213`)으로 하는 것과 같다. **액터마다 한 줄씩 늘리지 않는다** — 네 번째 액터에서 반드시 하나 빠뜨린다. 공통 C++ 베이스도 만들지 않는다(계층이 늘어난다).
  - 우리는 `LineTraceSingleByChannel`이므로 프리셋의 응답은 **`Block`**이다. Lyra는 `LineTraceMulti`라 `Overlap`을 쓴다 — 섞으면 아무것도 안 잡힌다.

### 4-6. 인벤토리 — 컨테이너별 칸 합산

> 구현 스펙(자료구조·코드·함정표)은 `05_Loot_03_Inventory.md`. 여기에는 **결정과 근거만** 둔다.

**칸은 합산이고 격자 위치가 아니다.** 아이템마다 `FEPItemData::SlotSize`가 있고, UI에서는 전부 한 줄로 보이되 소비하는 칸 수가 다르다 — 붕대 1칸, 구급상자 3칸, 소총 5칸. 무게 시스템과 동형이라 **단편화 처리가 필요 없다**("3칸이 연속으로 비었는가"가 아니라 "총 3칸이 남았는가").

**용량은 스칼라 하나가 아니라 컨테이너마다 독립된 풀이다.** 착용한 컨테이너마다 자기 칸 수가 있고 **서로 통합되지 않는다.** 배낭을 버리면 안의 아이템이 같이 나간다.

```
EntryId=1  Parent=NONE  Shirt_Basic  SlotId=Torso   ← 입고 있는 상의 (10칸 제공)
EntryId=2  Parent=NONE  Backpack_B   SlotId=Back    ← 매고 있는 배낭  (8칸 제공)
EntryId=3  Parent=1     Bandage        1칸           ← 상의 내부
EntryId=4  Parent=2     Weapon_AK74    5칸           ← 배낭 내부
EntryId=5  Parent=4     Scope_4x                     ← 배낭 속 총의 부착물 (§7-3)
```

> ### ★★ 본체 인벤토리는 **0칸**이다 (2026-08-25 확정)
>
> **수납 용량이 전부 착용 컨테이너에서 나온다.** 아무것도 안 입으면 들 수 있는 것이 없고, 그래서 **스폰 시 기본 상의·하의를 입고 시작한다**(`UEPLootDeveloperSettings::StartingEquipment`, 03-6). 로비가 생기면 *"로비에서 고른 옷을 입고 나온다"* 로 대체된다 — 바뀌는 것은 **그 배열을 채우는 쪽**이다.
>
> | 아이템 | `SlotSize` (차지) | `ContainerCapacity` (제공) |
> |---|---|---|
> | **본체** (`INDEX_NONE`) | — | **0** ｜ `MaxSlots`. **테스트 중에만 10**이고 곧 0으로 내린다 |
> | 기본 상의 | 11 | 10 |
> | 기본 하의 | 6 | 5 |
> | 배낭 A | 15 | 12 ｜ 어디에도 안 들어간다(15 > 12, 15 > 10) — **의도** |
> | 배낭 B | 10 | 8 ｜ 상의(10)·배낭A(12)에 들어간다 |
>
> **`MaxSlots` 필드는 남긴다.** 0을 넣으면 `CanFit`이 항상 거짓이라 나머지 코드가 그대로 돌고, 없애면 `GetInsertionOrder`의 맨 앞 `INDEX_NONE`까지 같이 빼야 한다. **밸런싱 값으로만 다룬다.**
>
> **★ 상의·하의는 컨테이너를 잃을 수 있다 — 가방과 외투만 남는 안** (§8 미정). 그 이행은 **데이터 둘**이다: `DT_Items`의 `ContainerCapacity`를 0으로 내리고(위 정의: 컨테이너 = `Capacity > 0`), `ContainerOrder`에서 뺀다. **코드 변경 0** — `GetCapacity`가 0을 돌려주고 `CanFit`이 항상 거짓이 되며, `BodySlots`·정합 검사는 무관하다(슬롯은 남고 착용 효과만 남는다). **§4-9의 *"컨테이너 여부를 타입 계층이 아니라 값으로"* 가 값을 하는 자리다** — `UEPClothingDefinition : UEPContainerDefinition` 같은 계층이었으면 클래스를 갈아엎어야 했다.

| 결정 | 근거 |
|---|---|
| **`ParentEntryId`로 컨테이너를 표현한다** | 자기 타입 재귀는 `Class.cpp:974`에서 **Fatal**이라 무제한 깊이를 중첩으로 못 만든다. 무엇보다 **배낭·부착물·상자가 같은 표현**을 쓰게 되어 위 `EntryId=5`가 특수 케이스 없이 성립한다 |
| **`SlotId`가 장착의 유일한 진실이다** (9차) | 용량 합산이 "수납된 자식"과 "슬롯에 든 자식"을 구분해야 한다(슬롯에 든 것은 칸을 안 먹는다). **별도 장비 슬롯 배열을 만들지 않는다** — `USceneComponent`가 `AttachSocketName`을 자식에 두고 `AttachChildren`은 `Transient` 파생 색인인 것과 같다(`SceneComponent.h:108-119`). Step 03에서 실제로 쓰는 값은 `"Back"`·`"Torso"`·`"Legs"` (`DOCS/Mine/EquipmentSlots.md` §1·§3) |
| **`UsedSlots`를 캐시하지 않는다** | 추가·제거·복제 수신 세 경로를 전부 갱신해야 하고 하나만 빠지면 "안 찼는데 가득 찼다"가 된다. 엔트리 30개 × `TMap` 룩업은 수백 나노초다 |
| **`UsedSlots`를 복제하지 않는다** | 클라는 `COND_OwnerOnly`로 엔트리 전부를 갖고 `SlotSize`는 Definition 조회로 나온다. 복제하면 진실이 두 곳이 된다 |
| **~~`EntryId`로 정렬한다~~ → `SortKey`(서버 발급 희소 키)** (11차) | FastArray는 클라 배열 순서를 보장하지 않는다(`FastArraySerializer.h:54`). 삭제가 `RemoveAtSwap`이라(`:1191`) 실제로 뒤섞인다. **`EntryId`는 식별자이지 순서가 아니다** — 재배치를 넣는 순간 식별자를 바꿔야 한다 (§4-8 · `05_Loot_03_Inventory.md` 03-1) |
| **복제 조건은 `COND_OwnerOnly`** | 조건 없이 복제하면 패킷만 봐도 상대 소지품을 아는 치트가 되고 8인 매치면 대역폭도 8배다. `PlayerState::Kills`/`Extracted`가 이미 같은 조건이다 |

> **시체 루팅이 생겨도 조건을 풀지 않는다.** 시체 액터가 자기 인벤토리를 별도로 노출하는 방식으로 간다. 살아있는 플레이어의 가방은 끝까지 소유자 전용이다.

#### ★★ 컨테이너의 정의 — 그리고 중첩 깊이는 데이터가 막는다

**컨테이너 = `FEPItemData::ContainerCapacity > 0`.** 별도 플래그도 타입 계층도 없다. 배낭·상자·파우치가 전부 같다.

그런데 이 정의만으로는 **컨테이너를 컨테이너에 넣는 것**에 아무 제약이 없다. `MoveEntry` 검사 6은 **사이클(자기 자손)만** 막고 `CanFit`은 종류를 안 본다. 즉 초안대로면 **배낭 안 배낭 안 배낭**이 무한히 된다 — 가방 7겹으로 수백 칸을 드는 익스플로잇이다.

**결정: 모든 컨테이너가 `ContainerCapacity < SlotSize`를 지킨다.** (2026-08-24)

```
A를 B에 넣으려면   SlotSize(A) ≤ Capacity(B) < SlotSize(B)
                   ⇒ SlotSize(A) < SlotSize(B)
```

**깊어질수록 `SlotSize`가 반드시 작아진다.** 양의 정수라 유한하고, 깊이 상한은 **서로 다른 컨테이너 크기의 개수**다.

```
배낭 A   SlotSize 15, Cap 12     A 안에 B  → 10 ≤ 12   ✅ 된다
배낭 B   SlotSize 10, Cap  8     B 안에 들어갈 컨테이너 없음 → 깊이 2에서 끝
                                 A 안에 A  → 15 ≤ 12   ❌ 당연히 안 된다
```

##### ★★ 부등호가 둘이고 서로 다른 식이다 (2026-08-25)

**헷갈리면 방금 "당연히 안 된다"고 한 것이 가능해진다.**

```
넣기 판정   :  SlotSize(넣을 것)  ≤  Capacity(담을 것)     ← ≤ 다. B(10)를 상의(10)에 넣는다
데이터 규칙 :  Capacity(X)        <  SlotSize(X)           ← < 다. 등호를 허용하면 안 된다
```

```
SlotSize(A) ≤ Capacity(B) < SlotSize(B)   ⇒  SlotSize(A) <  SlotSize(B)   깊이 유한 ✅
SlotSize(A) ≤ Capacity(B) ≤ SlotSize(B)   ⇒  SlotSize(A) ≤  SlotSize(B)   깊이 무한 ❌
```

`SlotSize 10 / Cap 10`짜리 행 하나면 **그 가방이 자기 안에 들어간다**(10 ≤ 10).

**★ 근거는 익스플로잇이 아니다 (13차 답변).** N겹으로 쌓아도 **총 용량이 그대로**다 — 바깥 10칸 중 10칸을 안쪽이 먹으므로 순증 0이고, 가방 N개를 소모해서 얻는 것이 없다. 진짜 근거 셋은 전부 **비용** 쪽이다.

| | |
|---|---|
| ① **비용이 0이다** | `IsDataValid()`가 이미 `<`로 짜여 있고 확정 수치가 전부 만족한다. **쓸 코드도 고칠 값도 없다** |
| ② **깊이 상한이 이 규칙 하나에만 걸려 있다** | `RemoveEntry` 재귀 · UI 중첩 · 세이브가 전부 *"깊이는 유한하다"* 를 전제하는데 증명이 위 부등식 **하나**다. 되돌리면 **세 곳이 동시에 근거를 잃는다** |
| ③ **되돌릴 손잡이가 이미 이름으로 있다** | 아래 *"`MaxContainerDepth`는 만들지 않는다 … 그때 연다"*. **`=`를 원하는 날의 답은 `=`를 여는 게 아니라 깊이 상한을 여는 것이다** |

**즉 질문이 *"막을 가치가 있나"* 가 아니라 *"이미 공짜로 막혀 있는 것을 굳이 풀 이유가 있나"* 다.**

##### ★★ 그리고 증명이 쓰는 `SlotSize ≥ 1`을 검증하는 곳이 없었다 (13차 답변)

`FEPItemData::SlotSize = 1`은 **기본값일 뿐 하한이 아니다**(`EPItemData.h:39`, `ClampMin` 없음). **본체가 0칸이 되면 구멍이 된다.**

```cpp
CanFit(본체, X) = GetUsedSlots(-1) + SlotSize <= GetCapacity(-1)
                =        0        +    0      <=        0          →  참
```

**`SlotSize = 0`인 아이템은 0칸 본체에 무한히 들어가고 영원히 안 찬다.** `GetInsertionOrder`의 맨 앞이 본체라 **그 아이템만 컨테이너에 절대 안 들어간다.** `IsDataValid()`에 `SlotSize >= 1` 한 검사를 더한다(`05_Loot_00_ItemCore.md`).

> **"자리를 안 먹는 아이템"이 기획으로 오면 답은 `SlotSize`가 아니라 슬롯이다** — 몸 슬롯(`SlotId != None`)이 이미 그 표현이고, **두 번째 표현을 만들지 않는다.**

**런타임 코드가 0줄이다.** `MoveEntry`에 검사 8번을 붙이지 않는다 — 데이터가 이미 답한다.

##### ★ 규칙이 아니라 형태로 — `IsDataValid()`

`ContainerCapacity`는 **DT 컬럼**이다. 규칙을 문서에만 적으면 반년 뒤 새 배낭 행에 `Cap 20 / SlotSize 8`을 넣는 순간 **아무 에러 없이 무한 중첩이 열린다.** 증상은 크래시가 아니라 *"어떤 유저가 배낭 7겹으로 500칸을 들고 다닌다"* 라 발견이 늦다.

```cpp
// Step 00의 IsDataValid()에 한 줄 — 에디터에서 DT 저장 시 걸린다
if (Row.ContainerCapacity > 0 && Row.ContainerCapacity >= Row.SlotSize)
    → Error: "컨테이너 용량이 자기 크기 이상이면 중첩 깊이가 무한해진다"
```

`IsDataValid()`가 이미 DT↔Definition 정합을 검사하는 자리다(§4-9). **새 패턴이 아니다.**

##### ★★ 그래서 배낭 벗기는 바닥 드랍이다

이 규칙의 실제 비용이 여기 있고, 미리 못 박지 않으면 밸런싱이 조용히 막힌다.

**착용 중인 배낭은 칸을 안 먹는다** — `SlotId != None`이면 `GetUsedSlots`가 건너뛴다(`05_Loot_03_Inventory.md` 03-3). 즉 `SlotSize`는 **벗었을 때만** 의미가 있다. 그런데 벗기를 `MoveEntry(배낭, INDEX_NONE, NAME_None)`으로 하면 검사 5를 지난다.

```
GetUsedSlots(본체) + SlotSize(배낭) ≤ MaxSlots
```

| `MaxSlots` = 10(테스트값)일 때 `SlotSize` | 결과 |
|---|---|
| 8 | 본체가 **2칸 이하**로 차 있을 때만 벗어진다 |
| 11 | **영원히 못 벗는다** |

**그리고 위 규칙상 용량을 키우려면 `SlotSize`를 키워야 하므로, 키울수록 못 벗게 된다.** 증상도 나쁘다 — `MoveEntry`가 조용히 `false`라 **"벗기가 가끔 안 먹는다"** 로 나타난다.

> **★★ 본체 0칸 확정(2026-08-25)으로 "가끔"이 "항상"이 됐다.** `GetUsedSlots + SlotSize ≤ 0`은 **어떤 배낭에도 거짓**이고, 확정한 A(15)·B(10)는 **테스트값 10에서도 이미 못 벗는다.** 아래 결정이 없으면 벗기 경로가 **아예 존재하지 않는다.**

**결정: 벗기는 `Server_DropItem`이다.** 발밑에 픽업으로 나가므로 칸 검사가 없고, `RemoveEntry` → 스폰 경로라 **내용물도 그대로 따라간다**(03-5). 코드가 이미 있다. 타르코프도 넣을 자리가 없으면 바닥이다. **배낭 `SlotSize`를 15까지 잡을 수 있는 근거가 이것이다.**

> **★ 착용도 같은 이유로 칸 검사를 안 지난다 (2026-08-25).** `AddSubtree(Parent, **SlotId**, In)`이 `SlotId`가 있으면 `CanFit`을 건너뛴다 — 본체를 경유해 넣고 옮기면 *"등이 비었는데 배낭을 못 맨다"* 가 되고, **본체 0칸에서는 영구히 아무것도 자동 착용되지 않는다** (`05_Loot_03_Inventory.md` 함정 4y).

> **명시적 깊이 상한(`MaxContainerDepth` + `MoveEntry` 검사 7)은 만들지 않는다.** 검사 6이 이미 부모 사슬을 도니 3줄이면 되지만, 위 둘로 목적이 달성되고 **확장점이 문서에 이름으로 있지도 않다**(CLAUDE.md §2). 밸런싱이 `SlotSize`/`Capacity` 커플링을 못 견디게 되면 그때 연다 — 그 신호는 *"용량은 키우고 싶은데 `SlotSize`를 못 키운다"* 이다.

#### 삽입 — 전부 아니면 전무

칸이 모자라면 아무것도 들어가지 않고 픽업은 그대로 남는다. 픽업 하나 = 아이템 하나이므로 "몇 개 들어갔나"라는 질문이 성립하지 않는다. 그래서 `AddItem`은 개수가 아니라 **발급된 `EntryId`** 를 돌려준다 — 삽입 직후 그 엔트리를 바로 가리킬 수 있어 "기본 지급 후 자동 장착"에 재검색이 필요 없다.

#### ★ 균질(fungible) 아이템은 합친다 — 스택이 아니다

현금뭉치 두 개를 주웠을 때 엔트리가 둘로 늘면 칸만 낭비된다. `FEPItemData::bFungible`이면 **`Charges`를 더한다.**

> **균질 = 같은 `ItemId`면 개체 간 구별할 근거가 없다는 뜻.** 현금뭉치·탄약상자는 ✅(탄종은 `ItemId`가 이미 가른다), 무기·탄창은 ❌(`Durability`와 부착물 자식을 갖는다).

**스택의 부담이 하나도 안 따라오는 이유는 칸 수가 `Charges`와 무관하기 때문이다.**

| 스택이 데려오던 것 | `Charges` 합치기 |
|---|---|
| `SlotSize × Quantity` 계산 | **없음.** 엔트리당 고정 |
| 부분 획득 (60발 중 40발) | **없음.** 칸이 안 늘어 실패가 성립 불가 |
| 병합 순서 규칙 | **없음.** 합칠 대상이 최대 하나다 |
| 분할 | 자료구조 요구가 아니라 UI 기능이다 |

**부수 이득 둘.** 가방이 꽉 차도 **돈과 탄약은 항상 들어가** 부분 획득을 없애며 잃은 완충 장치가 복원된다. 그리고 `bMergeable`이 아니라 `bFungible`이라 **"균질 아이템은 `Durability`를 쓰지 않는다"** 가 불변식으로 따라와, *합칠 때 내구도는 어떻게 하나*라는 질문이 아예 발생하지 않는다.

#### ★★ 불변식은 문서가 아니라 함수가 강제한다

이 설계의 위험은 셋이다 — **엔트리를 지운 뒤 잔탄을 write-back하면 소실**, **수정 후 `MarkItemDirty` 누락**, **컨테이너 제거 시 자식이 고아로 남음**. 셋 다 증상이 엉뚱하고 재현이 어렵다.

**규칙으로 남기면 안 된다.** 여러 문서에 적히고 한쪽만 고쳐지는 사고가 반드시 난다. 형태로 막는다.

| 위험 | 막는 형태 |
|---|---|
| write-back 순서 | **`RemoveEntry()`가 제거된 서브트리를 반환한다.** 스냅샷을 얻는 유일한 방법이 제거하는 것이므로 순서를 뒤집는 게 문법적으로 불가능해진다 |
| 자식 고아 | **`RemoveEntry()`가 유일한 제거 지점.** 캐스케이드가 자기 자신을 재귀 호출하므로 장착 검사·write-back이 노드마다 자동으로 돈다 |
| `MarkItemDirty` 누락 | **원시 엔트리를 밖으로 내보내지 않는다.** 수정은 `AddEntryCharges()` 같은 API로만 |
| 중간 Broadcast | **스코프 가드.** 순회 중에는 알림이 나가지 않는다 |

> 지금 남은 위험은 성능도 확장성도 아니라 **코드로 강제 가능한 불변식을 규율에 맡기는 것**이다. 1인 프로젝트에서도 3개월 뒤에는 깨진다.

#### 부착 위치: Character (확정)

타르코프식은 **사망 시 소지품 손실**이 규칙이다(GAME.md 코어 루프). Character와 수명을 같이하는 것이 규칙과 일치한다 — PlayerState에 두면 사망 시 명시적으로 비워야 하고 "비우는 걸 깜빡하는" 버그의 자리가 생긴다.

> ASC를 PlayerState에 둔 것과 반대지만 이유가 다르다. ASC는 리스폰 후에도 **보존되어야** 해서 PlayerState였다. 인벤토리는 반대로 **소실되어야** 한다.

### 4-7. 아이템 버리기 — 픽업의 역방향

> 구현 스펙은 `05_Loot_03_Inventory.md` 03-5.

```
G키(장착 무기) 또는 인벤토리 UI에서 선택
    → Server_DropItem(int32 EntryId)
    → RemoveEntry가 서브트리를 반환 → 픽업에 값 복사 → DropCooldown 시작
```

- **RPC 파라미터는 `EntryId`다.** 배열 인덱스였다면 제거·정렬로 밀리는 사이에 요청이 도착해 엉뚱한 아이템을 버린다. `EntryId`는 재번호되지 않으므로 그 경쟁이 없다
- **배낭을 버리면 안의 아이템이 같이 나간다**(GAME.md). 픽업이 엔트리 하나가 아니라 **서브트리**를 든다
- 잔탄·내구도가 보존되는 이유는 **값을 복사했기 때문**이지 규칙을 지켰기 때문이 아니다. 인스턴스를 재생성하는 실수 자체가 성립하지 않는다

| 항목 | 처리 |
|---|---|
| 스폰 위치 | 캐릭터 전방 약 100cm, 아래로 라인 트레이스해 접지. 실패 시 발밑 |
| 즉시 재획득 | `DropCooldown`(약 0.5초) 동안 `CanInteract()`가 false. 없으면 G를 누른 순간 F 프롬프트가 바로 떠서 실수로 다시 줍는다 |
| 부분 버리기 | **없다.** 엔트리 하나가 최소 단위다 |
| 클라 예측 | **하지 않는다.** 결과가 늦게 보여도 무해하고, 예측하면 롤백 처리가 필요해진다 |

> **사망 시 전체 드랍**(§8 미정 #4)은 이 경로의 확장이다. 시체 위치에 픽업 하나를 스폰하고 **전 엔트리**를 서브트리로 넘긴다 — `ParentEntryId == INDEX_NONE`인 것들이 그 픽업의 루트가 된다. §7-1 컨테이너(시체 루팅)의 특수 케이스다.

### 4-8. 무기 장착 흐름 이관 (★ 탄약 소유권 충돌)

> 구현 스펙은 `05_Loot_05_Equipment.md`.

**현재 흐름:** `EPGameMode::HandleStartingNewPlayer` → `DefaultWeaponClass` 액터 직접 스폰 → `CombatComponent::EquipWeapon()`. 아이템 계층을 전혀 거치지 않는다.

**목표:** 인벤토리 UI/숫자키 → `Server_Equip(EntryId)` → `CombatComponent::EquipFromInventory(EntryId)`.

**인벤토리 컴포넌트가 `AEPWeapon`을 스폰하지 않는다.** 무기 액터의 수명·부착·애님 레이어·어빌리티 부여를 이미 `UEPCombatComponent`가 전부 쥐고 있다(`EPCombatComponent.cpp:161`). 인벤토리는 `EntryId`만 넘긴다.

#### 문제: 탄약의 진실이 두 곳에 있다

| 위치 | 성격 |
|---|---|
| `UEPAttributeSet::Ammo` / `MaxAmmo` | 캐릭터(ASC) 소유. **1인당 하나뿐.** 복제되고 HUD가 구독 |
| `FEPInventoryEntry::State.Charges` | 개체별. 무기마다 다름 |

무기 2정을 넣고 교체하려면 **정별로** 잔탄이 보존돼야 하는데 어트리뷰트는 캐릭터에 하나뿐이다.

#### 결정: 어트리뷰트는 "현재 장착 무기의 뷰", 엔트리가 진실

```
Equip   : Entry.State.Charges  →  SetAmmo()
Unequip : GetAmmo()            →  Entry.State.Charges
```

- 발사·재장전은 지금처럼 GAS 어트리뷰트만 건드린다. **`GA_Item_PrimaryUse` / `GA_Item_Reload`는 수정하지 않는다**
- write-back이 필요한 지점은 교체·버리기·사망 셋이고 **전부 `UnequipWeapon()`을 거치게** 만들어 한 곳에만 둔다
- **버리기 경로의 순서는 `RemoveEntry()`가 보장한다**(§4-6). 여기에 순서 규칙을 다시 적지 않는다

#### 즉시 고쳐야 할 것: `EquipWeapon`의 만탄 리셋

`EPCombatComponent.cpp:177-178`이 장착할 때마다 `InitAmmo(MaxAmmo)`로 만탄 리셋을 한다. **버리기가 들어오는 순간 익스플로잇이 된다** — 12/30 무기를 버렸다 줍기만 하면 30/30이다.

#### 장착 슬롯 표현

별도 장비 슬롯 배열도 별도 필드도 만들지 않는다 — **`FEPInventoryEntry::SlotId`가 장착의 유일한 진실이다**(9차 확정, 2026-08-22). `SlotId == "Hotbar1"`이 곧 *"이 총은 1번 칸에 있다"* 이고, `SlotId == "Back"`이 곧 *"이 배낭은 등에 있다"* 다.

**`SlotId`로 표현되지 않는 상태가 하나 남는다** — *"1번과 2번 중 지금 어느 쪽을 손에 들었나."* 그것만 `ActiveHotbarIndex`(int32, `COND_OwnerOnly`)로 둔다.

> **엔진 자신이 같은 형태다.** `USceneComponent`는 `AttachParent` ＋ `AttachSocketName`(`FName`)을 **자식**에 두고, 자식 목록(`AttachChildren`)은 `Transient` **파생 색인**이다(`SceneComponent.h:108-119`). 진실은 자식에, 색인은 파생으로. 슬롯 12개(핫바 4 + 착용 8) + 부착 4를 별도 배열로 표현하면 *"슬롯 이름 ↔ 배열 인덱스"* 표가 어딘가에 필요해지고, 그 표가 `SlotPriority`가 없애려는 하드코딩이다. 상세: `DOCS/Mine/EquipmentSlots.md`

**`COND_OwnerOnly`로 충분하다.** 다른 클라이언트는 `AEPWeapon` 액터가 복제되고(`EPWeapon.cpp:19`) 소켓에 부착되므로 이미 보인다. 이 값은 순전히 소유자 UI를 위한 것이다.

- **핸들이 두 개가 아니다.** 이전 설계는 인벤토리의 복제 핸들과 CombatComponent의 서버 전용 핸들을 별개로 뒀는데, 그건 "인벤토리가 먼저 비워져도 write-back 대상을 잃지 않게" 하려는 자료구조적 방어였다. `RemoveEntry()`의 반환값이 그 자리를 대신한다
- **슬롯이 12개가 돼도 `TMap<EEPEquipSlot, int32>`로 가지 않는다** (9차 확정). 맵으로 가면 복제 대상이 하나 늘고 엔트리와의 정합을 지키는 코드가 생긴다. `SlotId`는 **이미 엔트리 안에 있고 이미 복제된다** — 추가 대역폭이 0이다
- **`SlotId`로 표현되지 않는 것은 하나뿐이다** — *"1번과 2번 중 지금 어느 쪽을 들었나."* 그것만 `ActiveHotbarIndex`로 둔다. **가리키는 것이 엔트리가 아니라 슬롯이라 죽은 번호가 생길 문법이 없다**

### 4-9. 확장 지점 — 아이템 종류를 늘려도 인벤토리를 안 고치게

#### ★ 다형성은 Definition의 virtual 초기화 함수에 남긴다

기존 static 팩토리 2종(`UEPItemInstance::CreateInstance` / `UEPWeaponInstance::CreateWeaponInstance`)은 시그니처가 서로 달라서, 인벤토리가 무기를 만들 때 `EEPItemType`으로 분기하고 `Cast<UEPWeaponDefinition>`을 해야 했다. **방어구·소모품·퀘스트 아이템을 추가할 때마다 그 분기가 자란다.**

```
UEPItemDefinition::InitState(const FEPItemData& Data, FEPItemState& State) const
    기본  → State.Charges = Data.InitialCharges
    무기  → State.Charges = MaxAmmo
```

호출부는 한 줄이고 **인벤토리·픽업·스포너·자판기가 아이템 타입을 전혀 모른다.** 새 아이템 종류 = Definition 서브클래스 하나, 기존 코드 수정 0.

#### ★ 데이터를 DataTable에 둘지 DataAsset에 둘지

> **① 여러 아이템을 표로 나란히 놓고 조정하는 값은 `FEPItemData`(DataTable).**
> **② 그 아이템 한 종류에만 의미 있는 것 — 에셋 참조, `virtual` 동작, 타입 전용 필드 — 은 `UEPItemDefinition`(DataAsset).**

판정선은 **"모든 아이템이 값을 갖는가"** 다.

| | DT | DA |
|---|---|---|
| `SlotSize` / `SellPrice` / `Rarity` / `bFungible` / `InitialCharges` / `ContainerCapacity` | ✅ 전부 값을 가짐(대부분 0이어도) | |
| `WorldMesh` / `Icon` / `GrantedAbility` | | ✅ 에셋 참조 |
| `MaxAmmo` / `InitState()` | | ✅ 무기 전용 / virtual |

`MaxAmmo`를 DT에 넣으면 무기 아닌 행이 전부 빈칸이 된다. 그게 판정선이다.

> **두 계층을 유지하는 이유:** 합치면 양방향 참조 동기화·`IsDataValid` 오버라이드·캐시 2개·`FindData` null 무증상 버그가 전부 사라진다. 그럼에도 유지하는 것은 **아이템이 수십 종이 되면 밸런싱 표(CSV·일괄 수정·diff)가 확실히 낫기 때문**이다. DataAsset은 그중 아무것도 안 된다.

#### 소모품이 들어갈 자리를 지금 판다

루트 테이블에 붕대·회복키트가 들어가는데 **사용할 방법이 없다.** 구현하지는 않되 자리는 잡는다 — `UEPItemDefinition::GrantedAbility`.

- 장착/선택 시 `GiveAbility`, 해제 시 `ClearAbility`. 사용 입력은 그 어빌리티를 활성화만 한다
- **힐 스킬이 이미 GAS로 있다.** `UEPGA_Skill_Base`의 `CastTime` + `State.Casting` 구조를 상속하면 "붕대 3초 시전, 피격 시 취소, 중앙 게이지"가 코드 추가 없이 된다

#### 데이터 정합성 — 양방향 참조를 검증한다

`FEPItemData::ItemDefinition`(소프트 참조)과 `UEPItemDefinition::ItemDataRow`가 서로를 가리켜 손으로 동기화해야 한다. 아이템이 수십 개를 넘으면 반드시 어긋나고, 어긋나도 컴파일·로드는 통과한다.

- **DataTable → Definition을 정본으로 정한다.** 조회는 항상 `ItemId → Row → ItemDefinition` 방향으로만
- `IsDataValid()`로 에디터에서 검증하고, 서브시스템 초기화 때 같은 검사를 런타임에서도 돌린다

##### Step 00에서 지울 것

| 심볼 | 이유 |
|---|---|
| `UEPItemInstance` / `UEPWeaponInstance` 클래스 전체 | 개체 상태가 `FEPItemState`로 대체됐다. 호출처 0이라 지금이 지울 적기다 |
| `InstanceId` (FGuid) | 읽는 코드가 없다. DB 영구 식별자는 저장 시점에 발급한다 |
| `SchemaVersion` | 아이템이 아니라 **세이브 포맷의 속성**이다. `USaveGame`/DB 행 봉투에 하나만 둔다 |

## 5. 단계 계획

| Step | 문서 | 내용 | 완료 조건 |
|---|---|---|---|
| 00 | `05_Loot_00_ItemCore.md` | **기존 아이템 계층 정비** — `FEPItemState` 도입, `UEPItemInstance`/`UEPWeaponInstance` 삭제, Definition의 virtual `InitState()`, `GrantedAbility`/`InitialCharges` 필드, `MaxAmmo` → `int32`, `UEPItemDefinitionSubsystem`, AssetManager에 `ItemDef` 등록 | 매치 시작 시 Definition이 전량 상주하고, `ItemId`를 주면 올바른 초기 `State`가 나온다 (무기 → `Charges = MaxAmmo`). `IsDataValid()`가 DT↔Definition 불일치를 잡아낸다 |
| 01 | `05_Loot_01_Spawner.md` | `UEPLootTable`(중첩), `AEPItemSpawner`, `AEPPickup`(`ItemId` 복제 + `State` 서버 전용, Dormancy), `EP.Loot.RollTable` | 맵에 스포너를 놓고 PIE 2인 → 서버·클라 양쪽에서 같은 아이템이 같은 위치에 보인다. `RollTable` 1000회로 등급 비율이 기획표와 일치 |
| 02 | `05_Loot_02_Interaction.md` | `IEPInteractable`(4함수), `UEPInteractionComponent`(틱 0.1s), **서버 거리 재검증 + `CanInteract()` 재호출**, `bClaimed` 경쟁 처리, HUD 프롬프트 | F키를 누르면 서버가 거리·`CanInteract()`를 재검증하고 픽업이 파괴된다. 사거리 밖 요청 거부. 2인이 동시에 눌러도 한 명만 성공 |
| 03 | `05_Loot_03_Inventory.md` | `UEPInventoryComponent`(Character 부착), `FEPInventoryList`(FastArray + `EntryId`/`ParentEntryId` + 내장 `State`), **컨테이너별 칸 합산 + 배낭**, `bFungible` 합치기, `RemoveEntry()` 불변식(write-back + 자식 캐스케이드), **버리기(G) + `DropCooldown`** | 주운 아이템이 인벤토리에 쌓이고 델타 복제된다. 칸이 모자라면 **아무것도 안 들어가고 픽업이 그대로 남는다.** **배낭을 매면 별도 칸이 열리고, 배낭을 버리면 안의 아이템이 같이 나간다.** 현금뭉치 둘을 주우면 **금액이 합쳐진다.** 무기를 버렸다 다시 주우면 **잔탄이 그대로**다 |
| 04 | `05_Loot_04_InventoryUI.md` | **04-A 표시** — 정사각형 격자 + 분절 게이지 + `＋ 남은 용량` 블록, 착용 12슬롯, FastArray 콜백 기반 갱신 / **04-B 드래그** — 이동·교환·착용, `Server_MoveEntry`/`Server_SwapEntries`, 로컬 순서(`ULocalPlayerSaveGame`에 지속) | 상의를 입으면 그 구획이 뜬다. **칸 3개인데 게이지는 `7/20`**(AK4+권총2+붕대1). **A의 아이템과 B의 아이템을 교환할 수 있고, 실패하면 아무 일도 일어나지 않는다** |
| 05 | `05_Loot_05_Equipment.md` | `ActiveHotbarIndex` 세팅 경로, `CombatComponent::EquipFromInventory(EntryId)`, 탄약 주입/write-back + `MarkItemDirty`, `EquipWeapon` 만탄 리셋 제거, `DefaultLoadout` 이관 | 무기를 12/30까지 쏘고 버렸다 다시 주워 장착하면 **12/30 그대로**. 다른 클라에서도 장착 무기가 보인다 |

각 Step 완료 시 `LOOT_STATUS.md`와 해당 `05_Loot_0X_XXX_STATUS.md`를 코드 기준으로 갱신한다.

> **★ Step 02와 03의 경계:** Step 02 시점에는 인벤토리가 없다. `CanInteract()`의 "인벤토리 여유 확인"과 `OnInteract()`의 "인벤토리 삽입"은 **Step 03에서 채운다.**
> - Step 02의 범위 = 인터페이스 4함수 + 트레이스/프롬프트 + **서버 거리·대상 재검증** + `bClaimed` 경쟁 처리
> - Step 02의 `OnInteract()`는 `Destroy()`만 하고 로그를 남긴다. §3이 내세운 "임시 코드가 안 생긴다"를 지키려면, 이 한 줄이 **Step 03에서 `AddItem()` 호출로 대체되는 유일한 지점**이어야 한다
> - 칸 여유 확인 없이 파괴하므로 Step 02 단독으로는 "가방이 가득 찼을 때 픽업이 남는가"를 검증할 수 없다. 그건 Step 03 완료 조건이다

> **UI 범위 통제 (2026-08-23 갱신):** ~~Step 04는 표시 전용이다~~ — **9차 기획 확대로 뒤집혔다.** 착용·해제·핫바 배정이 전부 드래그이므로 **드래그가 Step 04의 본체**다. 대신 **자동 정렬·툴팁·핫바 5~0 UI·부착물 UI를 뺀다.**
>
> **레이아웃은 정사각형 격자다.** 칸 하나 = 아이템 하나이고, 부피(`SlotSize`)는 **배지 + 분절 게이지**로 보인다. **2D 배치(타르코프식)로 가지 않는다** — `FEPItemData`에 2D 크기가 없고, 넣으면 `FEPInventoryEntry`·`CanFit`·`MoveEntry`·UI가 전부 바뀌는 **Step 03 재작성**이다.
>
> **아이템 순서는 서버에 두지 않는다.** 서버 로직 중 순서를 보는 곳이 0곳이고 `COND_OwnerOnly`라 남이 볼 수도 없다. **지속은 `ULocalPlayerSaveGame`이 맡는다**(10차) — Lyra가 플레이어별 UI 상태를 두는 자리와 같다. 상세: `05_Loot_04_InventoryUI.md` 04-0 · 04-7 · 04-8.
>
> **★ Step 04를 04-A(표시) / 04-B(드래그)로 나눈다 (10차).** 완료 조건이 **14개**로 8차가 Step 03을 셋으로 쪼갠 숫자(13개)보다도 많고, **의존성이 실제로 다르다** — 04-A는 Step 03만 있으면 검증되지만 04-B는 `Server_MoveEntry`·`SwapEntries`가 있어야 한다. 분할선은 **RPC 표면이 처음 열리기 직전**이다.

---

## 6. 기존 코드와의 접점

| 기존 자산 | 이번 단계에서의 역할 |
|---|---|
| `FEPItemData` / `DT_Items` | `UEPItemDefinitionSubsystem`이 처음으로 읽는다. **`SlotSize`가 인벤토리 용량 판정의 핵심 필드가 된다** (§4-6). `MaxStack`은 읽지 않는다 — 스택 부활용 예약 필드 |
| `UEPItemDefinition` / `WorldMesh` / `Icon` | 픽업 메시와 인벤토리 아이콘의 출처. **Step 00에서 `virtual InitState()` / `GrantedAbility` / `InitialCharges`가 추가된다** |
| `UEPItemInstance` / `UEPWeaponInstance` **클래스 전체** | **Step 00에서 삭제.** 개체 상태는 `FEPItemState`(USTRUCT)로 대체 (§4-1) |
| `UEPItemInstance::CreateInstance()` (static) | 위 삭제에 포함. 다형성은 Definition의 `InitState()`로 (§4-9) |
| `UEPWeaponInstance::CreateWeaponInstance()` (static) | 동상 |
| `UEPItemInstance::InstanceId` (FGuid) / `SchemaVersion` | 위 삭제에 포함. 각각 "읽는 코드 없음" / "세이브 포맷의 속성" (§4-1) |
| `UEPWeaponDefinition::MaxAmmo` (`uint8`) | **Step 00에서 `int32`로.** `FEPItemState::Charges`·어트리뷰트와의 3중 캐스팅 정리 (§4-8) |
| `EEPItemType` / `EEPItemRarity` | 루트 테이블 필터링과 UI 색상에 사용 |
| `UEPWeaponInstance::CurrentAmmo` / `Durability` | `FEPItemState::Charges` / `Durability`로 이름을 바꿔 살아남는다. Step 05에서 처음으로 실제 값이 된다 (§4-8) |
| `AEPWeapon` 액터 / `UEPCombatComponent` 발사 로직 | **건드리지 않는다.** 바뀌는 건 "누가 언제 무기 액터를 만드는가"뿐 |
| `EPGameMode::HandleStartingNewPlayer` (`DefaultWeaponClass`) | Step 05에서 `DefaultLoadout : TArray<FName>` 기반 인벤토리 지급으로 교체 |
| `EPCombatComponent.cpp:177` `InitAmmo(MaxAmmo)` | Step 05에서 **반드시 제거.** 버리기가 들어오면 만탄 익스플로잇이 된다 (§4-8) |

---

## 7. 추후 반영 — 확정 기획, 구현은 나중

### 7-1. 타르코프식 컨테이너 (검색 시간)

맵 곳곳에 **공구상자 / 구급상자 / 가방 / 서류함** 등을 배치하고, 오브젝트 종류마다 다른 아이템이 나오게 한다.

| 항목 | 기획 |
|---|---|
| 배치 | 레벨에 직접 배치하는 액터. 스포너와 달리 **오브젝트 자체가 월드에 남는다** |
| 아이템 테이블 | 오브젝트 종류별로 다른 `UEPLootTable`. 공구상자→부품/탄약, 구급상자→회복 아이템, 가방→잡템/현금, 서류함→퀘스트 아이템(이력서·자격증) |
| **검색 시간** | 상호작용 시작 후 N초간 채널링. 완료해야 내용물이 공개된다 |
| 검색 중 | 이동하면 취소. 진행도 게이지 표시. **소리 발생 → 위치 노출** (자판기 5초 대기와 같은 긴장 장치) |
| 검색 후 | 컨테이너는 "이미 검색됨" 상태로 복제되어, 다른 플레이어가 헛수고하지 않게 한다 |
| 내용물 | 컨테이너 내부 인벤토리 UI를 열어 원하는 것만 가져간다 (바닥 스폰이 아니라 UI 방식) |

**지금 설계에서 미리 확보해 두는 것:**
- `UEPLootTable`을 그대로 재사용한다 → 코드 추가 없이 에셋만 늘리면 된다
- `IEPInteractable::GetInteractDuration()`을 처음부터 넣어둔다 → 검색 시간이 인터페이스 확장 없이 들어온다
- **★ 이 절이 §7-6(컴포넌트 3분할)의 트리거다** (2026-08-26). 상자는 *"가진 것"* 만 있고 *"입은 것"* 이 없는 **첫 소유자**다 — 지금 구조로는 상자가 `UEPInventoryComponent`를 통째로 달아 **몸 슬롯과 핫바를 갖는다.** §8 미정 #10이 같은 지점을 데이터 쪽에서 짚었다
- 검색 채널링은 **GAS 어빌리티로 구현**한다. `UEPGA_Skill_Base`의 `CastTime` + `State.Casting` 구조가 이미 있으므로, 검색 중 스킬 잠금·피격 중단·진행도 게이지(`WBP_CastGauge`)가 **전부 공짜로 재사용된다.** 이게 GAS를 먼저 한 배당금이다

### 7-2. 자판기

GAME.md §자판기 기획 유지 (1000원 → 5초 진동 + 소리 전파 → 배출). 컨테이너와 같은 상호작용·루트 테이블 인프라를 쓰되, **돈 차감**과 **배출 연출**이 추가된다.

배출물 크기 문제(라이플이 일반 자판기 배출구에 안 맞음)는 **상자 배출**로 해결하기로 결정. 자판기는 등급별 상자를 뱉고, 플레이어가 상자를 열면 내용물이 공개된다. 장점:
- 자판기 메시 크기와 배출 아이템 크기가 완전히 분리된다
- 아이템별 월드 메시가 없어도 된다 (현재 무기 외 메시 없음)
- "열기 전까지 모른다"는 뽑기 긴장감이 오히려 강화된다
- 상자 색을 등급별로 두면 원거리 정보 공개 수준을 튜닝할 수 있다 (교전 유도 노브)

상자는 §7-1 컨테이너의 특수 케이스로 구현한다 — 검색 시간 0, 1회용, 파괴됨.

> **★ 상자는 `PickupClass`로 스폰되지 않는다.** 상자는 컨테이너이므로 **자판기가 자기 필드**(등급 → 상자 클래스)로 들고 있다. `UEPLootDeveloperSettings::PickupClass`가 "프로젝트 전체 하나"인 것과 충돌하지 않는다 — **축이 다르다.** `PickupClass`는 **바닥에 떨어지는 아이템 픽업**의 클래스이고, 상자 클래스는 **스폰 지점(자판기)의 속성**이다.
>
> 안 적어두면 이 절을 구현할 때 *"픽업 클래스는 전역 하나로 정했잖아"* 와 충돌하고, 그 시점엔 왜 그렇게 정했는지 아무도 기억하지 못한다.

> **상자를 안 열고 가방에 넣어 탈출하는 것도 가능하다.** 초안에서는 이걸 "값 타입 설계가 아픈 유일한 시나리오"로 보고 금지하려 했으나, **배낭(§4-6)이 들어오면서 중첩 컨테이너를 어차피 만들게 되어 제약이 소멸했다.** 상자는 자기 용량을 가진 컨테이너 아이템이고, 내용물은 `ParentEntryId`로 매달린 엔트리다 — 배낭과 완전히 같은 구조다. 다른 점은 "열기 전까지 내용물을 복제하지 않는다"뿐이다.

> **★ 단 무조건 가능한 것은 아니다 (2026-08-24).** §4-6의 `Capacity < SlotSize` 규칙을 **상자도 컨테이너이므로 함께 지킨다.** 즉 *"상자를 가방에 넣어 탈출"* 은 **`SlotSize(상자) ≤ Capacity(가방)`일 때만** 성립한다 — 큰 상자는 못 넣고, 그건 버그가 아니라 **의도**다(그렇지 않으면 상자로 용량을 무한 증식할 수 있다). 위 문장의 *"제약이 소멸했다"* 는 **표현의 제약**이 사라졌다는 뜻이지 게임 규칙이 없다는 뜻이 아니다. 상자 크기를 정할 때 이 판정이 밸런싱 손잡이가 된다.

### 7-3. 무기 부착물 (배그식 — 깊이 1)

**결정: 무기에 고정 슬롯 N개, 부착물은 자기 슬롯을 갖지 않는다.**

```
AK-74
├─ [Optic]  4배율 스코프     ← 여기서 끝
├─ [Muzzle] 소음기
├─ [Grip]   수직손잡이
└─ [Mag]    탄창             (State.Charges = 장전된 발수)
```

타르코프처럼 부착물 위에 또 부착물을 다는 무제한 중첩은 채용하지 않는다. **부착물 자체는 인벤토리 아이템**이라 따로 줍고 버리고 팔 수 있다.

#### 값 타입 설계가 부착물을 막지 않는다

먼저 엔진 제약 두 개를 **정확히** 못박는다. 대충 알면 "안 된다"고 포기하거나, 반대로 해보고 "되는데?"가 되어 문서 신뢰를 잃는다.

```
Class.cpp:974   Fatal    "Struct recursion via arrays is unsupported for properties."
                         ★ 조건: StructProp->Struct == this — 즉 자기 타입 재귀일 때만
Class.cpp:5512  Warning  "nested NetDeltaSerialize struct ... not supported."
                         + "Struct will replicate using non-delta serialization."
```

| 시도 | 결과 |
|---|---|
| `FEPItemState` 안에 `TArray<FEPItemState>` (자기 재귀) | **Fatal.** 무제한 깊이를 중첩으로 표현하는 길이 막힌다 |
| `FEPInventoryEntry` 안에 `TArray<FEPAttachmentEntry>` (이종, 깊이 1) | **동작한다.** 다만 그 페이로드가 **프로퍼티 델타를 잃는다** |
| FastArray를 struct 안에 중첩 | **동작한다.** 비델타로 강등 + 경고 |

즉 **금지가 아니라 "무제한 깊이는 불가, 깊이 1은 델타 손실"** 이다. 그럼에도 중첩을 쓰지 않는다 — 델타를 잃고, 깊이 1에 갇히며, 배낭(§4-6)과 부착물이 서로 다른 표현을 갖게 되기 때문이다.

**평면 배열 + 부모 참조**로 통일한다. 이 표현은 **깊이와 무관**하다.

```cpp
FEPInventoryEntry {
    int32 EntryId;
    int32 ParentEntryId = INDEX_NONE;   // ← 부착물일 때만 유효. 나중에 추가
    FName SlotId;                        // "Optic" / "Muzzle" / "Grip" / "Mag"
    FName ItemId;
    FEPItemState State;
}
```

```
EntryId=7   Parent=NONE  Weapon_AK74   Charges=0
EntryId=12  Parent=7     Optic   Scope_4x
EntryId=13  Parent=7     Muzzle  Suppressor
EntryId=14  Parent=7     Mag     Mag_STD   Charges=30     ← 장전된 30발
EntryId=20  Parent=NONE  AmmoBox_545       Charges=100    ← 인벤토리에 따로
```

- **기존 FastArray가 그대로 동작한다.** 노드마다 델타 복제되고, 복제 구조가 바뀌지 않는다
- **DB에 그대로 들어간다.** 이건 관계형 모델 그 자체다 (로드맵 5단계)
- 깊이 1이므로 순회는 `Parent == 무기EntryId` 한 번 훑기다. **재귀도, 순환 참조 방어도 필요 없다**

#### ★ 왜 UObject 트리가 아닌가 — 부착물은 오히려 값 타입에 유리하다

"부착물이 생기면 UObject가 필요해지지 않나"가 자연스러운 반론이고, **반대다.**

| | UObject 트리 | 평면 + 부모 참조 |
|---|---|---|
| 순회 | 자연스러움 (포인터 추적) | 한 번 훑기. **가독성만 손해** |
| 소유권 | 노드마다 Outer·수명·핸들 — **§4-1이 제거한 부담이 노드 수만큼 곱해진다** | 없음 |
| 복제 | **서브오브젝트 트리 복제는 UObject 복제 중 가장 아픈 경우.** 노드마다 등록, 클라가 트리 재구성, 부착·탈착 순서에 널 참조 | 기존 FastArray 무변경 |
| 이관 (버리기·거래) | 서브트리 전체의 소유권 이동 | 값 복사 |
| DB 직렬화 | 트리 → 행 변환 필요 | 이미 행이다 |

UObject의 약점은 애초에 "숫자 둘을 담는 게 무겁다"가 아니라 **소유권과 복제**였고, 트리는 그 둘을 정확히 증폭시킨다.

**UObject가 진짜로 이기는 지점은 노드별 *행동*이다** — 조준경이 ADS 로직을 직접 갖는 식. 이 프로젝트에서 부착물이 하는 일은 **수치 합산**(`Recoil = Base + Σ Modifier`)이고, 무기 속성은 이미 Definition 레벨, 전투 행동은 `AEPWeapon`/GAS에 있다. 시각 효과는 `AEPWeapon`이 소켓에 메시를 붙이면 된다.

> Lyra가 `UItemInstance`를 UObject로 두는 이유는 **런타임에 붙였다 떼는 Fragment**인데, 부착물은 Fragment가 아니라 **자기도 아이템**이다. 인벤토리에 따로 들어가고 팔 수 있으므로 어차피 인벤토리 엔트리와 같은 표현이어야 한다.

#### 지금 확보돼 있는 것

부착물의 전제 조건은 **개체를 안정적으로 가리킬 수 있는 식별자** 하나다. `EntryId`(서버 발급, 단조 증가, **재번호 없음** — §4-1)가 정확히 그것이다.

배열 인덱스를 썼거나 번호를 재사용했다면 부모 참조가 성립하지 않아 부착물이 원천 봉쇄됐을 것이다. FastArray 순서 미보장 때문에 넣은 필드인데 결과적으로 이 문이 열려 있다. **Step 00~05에서 이 성질을 깨지 않는 것이 부착물을 위해 지켜야 할 유일한 것이다.**

> **배낭(§4-6)이 Step 03에 들어오면서 `ParentEntryId`·서브트리 픽업·자식 캐스케이드가 전부 먼저 만들어진다.** 부착물은 그 위에 슬롯 제약(이름 있는 자리, 1개, 타입 필터)만 얹는 것이 된다.

#### 실제로 구현할 때 드는 비용

| 항목 | 비용 | Step 03에서 이미? |
|---|---|---|
| 엔트리에 `ParentEntryId` / `SlotId` | — | **✅ 배낭 때문에 이미 들어간다** (§4-6) |
| `AEPPickup`이 서브트리를 든다 | — | **✅ 배낭 버리기 때문에 이미 필요하다** (§4-7) |
| `AddSubtree()` + `EntryId` 재매핑 | — | **✅ 배낭 되줍기 때문에 이미 필요하다** (§4-6) |
| `RemoveEntry()`의 자식 캐스케이드 | — | **✅ 이미 필요하다** (§4-7) |
| `UsedSlots`가 부착 자식을 제외 | 판정식 한 줄 | ✅ `SlotId` 유무로 이미 구분 |
| Definition에 슬롯 스키마 (`SlotId` + 허용 `ItemId` 목록) | 신규 | ❌ |
| 스탯 합산 (`Recoil`/`Spread`/`MaxAmmo`) | 신규 — 아래 | ❌ |
| `AEPWeapon`이 부착물 메시를 소켓에 부착 | 신규 | ❌ |
| 부착/탈착 RPC + 허용 타입 검증 | 신규 | ❌ |

**배낭이 Step 03에 들어오면서 부착물의 구조적 비용이 거의 전부 선불된다.** 남는 것은 부착물 고유의 것(슬롯 스키마·스탯 합산·메시·부착 RPC)뿐이다.

##### ★ 지금 미리 해둘 수 있는 유일한 준비 — 스탯 읽는 지점 일원화

현재 `Recoil` / `Spread` / `MaxAmmo`는 GAS 어트리뷰트가 아니라 **`WeaponDef->` 직접 읽기**다. 그래서 부착물 보정을 GE로 줄 수가 없다.

```
지금 : UEPCombatComponent → WeaponDef->Recoil        (여러 곳에서 직접)
이후 : UEPCombatComponent → AEPWeapon의 캐시된 합산값  (한 곳)
```

**합산 결과를 `AEPWeapon`에 캐시하고 읽는 지점을 한 곳으로 모으는 것**이 부착물 전에 할 수 있는 준비다. 읽는 곳이 흩어져 있으면 부착물이 올 때 전부 찾아 고쳐야 한다. Step 05에서 무기 장착 흐름을 손댈 때 같이 정리하면 추가 비용이 거의 없다.

##### 안심해도 되는 것 — `AddItem`의 원자성

부착물 달린 무기 획득 = 엔트리 4개 삽입이지만, **칸은 부모만 먹으므로 부모가 들어가면 자식은 반드시 들어간다.** 부분 실패가 성립하지 않아 롤백 로직이 필요 없다.

#### 무제한 중첩으로 넓히고 싶어지면

**필드 추가가 없다.** 자료구조가 이미 무제한 깊이를 표현할 수 있다. 붙는 것은 둘뿐이다.

- 재귀 순회 (부모를 따라 올라가며 스탯 누적)
- **순환 참조 방어** — 부착 시 "대상이 내 조상인가" 검사. A에 B를 달고 B에 A를 다는 실수는 반드시 나온다 (§4-2의 루트 테이블 순환과 같은 부류)

---

### 7-4. 탄창 아이템화 (미정 #1의 ⓐ) — 구현 기획

**Step 05 직후에 한다.** 기술적으로는 싸지만(자료구조 변경 0) **검증 순서 때문에 비싸다** — ⓐ 작업의 대부분이 *"이미 도는 코드를 한 칸 옮기는 것"* 이라, `Entry.State.Charges ↔ Ammo` 왕복이 ⓑ로 한 번 검증된 뒤에 해야 한다. 순서를 바꾸면 무기 장착이 한 번도 안 돌아본 상태에서 *"탄창 자식 엔트리의 잔탄이 이상하다"* 를 디버깅하게 되고, **주입이 틀린 건지 탄창 참조가 틀린 건지 가를 방법이 없다.**

#### 결론 — 새 API가 거의 필요 없다

재장전이 **`SwapEntries` 하나**로 떨어지고 나머지는 기존 함수의 **인자만 바뀐다.** 신규는 `FindReloadMag` 하나다.

#### 데이터 — `Caliber`는 Definition, 탄종은 아직 아니다

미정 #1은 *"`FEPItemState`에 `FName AmmoType`이 필요할 수 있다"* 고만 적었는데 **둘로 갈라야 한다.**

| | 어디 | 왜 |
|---|---|---|
| **`Caliber`** (규격) | `UEPMagazineDefinition` / `UEPWeaponDefinition` | `Mag_STD_545`는 **언제나** 5.45다. 개체마다 다를 수 없다 |
| **`LoadedAmmoId`** (안에 든 탄종) | `FEPItemState` | 같은 5.45라도 BT/PS가 다르다. **개체 상태가 맞다** |

```cpp
UEPMagazineDefinition : UEPItemDefinition
{
    FName Caliber;      // "545x39"
    int32 Capacity;     // 30 — MaxAmmo가 여기로 이사한다
};
```

호환 판정은 **`Mag->Caliber == Weapon->Caliber` 한 줄**이다. 매트릭스도 태그도 만들지 않는다.

> **`LoadedAmmoId`는 ⓐ와 같이 오지 않는다.** 소비자가 없다 — 관통력 시스템이 없으니 탄종을 구분해도 아무것도 안 달라진다. **미정 #6(탄약상자로 탄창 채우기)이 열릴 때 처음 의미를 갖고, 그때가 넣을 때다.** §2의 *"상상한 확장점을 만들지 않는다"* 에 해당한다.

#### ★★ 재장전 = `SwapEntries` — 여기가 핵심

빈 탄창을 **"어디로 보낼까"** 가 이 기능의 유일한 난제다. 가방이 꽉 찼으면? 바닥에 버리나? **교환이면 그 질문 자체가 없어진다.**

```
전:  [Mag]#14(15발)         가방·3번칸 #22(30발)
후:  [Mag]#22(30발)         가방·3번칸 #14(15발)
```

새 탄창이 **방금 비운 자리**에 빈 탄창이 들어가므로 **칸 검사가 필요 없다.** 총량이 안 변한다.

그리고 `SwapEntries`는 이미 계약이 있다 — 부모·`SlotId`·**`SortKey`까지 교환**(`05_Loot_03_Inventory.md` 03-2 단일 쓰기 지점 표). 빈 탄창이 **새 탄창이 있던 화면 자리를 그대로 물려받는다.** 눈으로는 "탄창이 제자리에서 바뀐" 것이고, 타르코프의 동작과 같다.

```cpp
void UEPCombatComponent::PerformReload()
{
    const int32 WeaponId = Inv->GetEquippedEntryId();
    const int32 OldMag   = Inv->GetEntryInSlot(WeaponId, "Mag");   // 없을 수 있다
    const int32 NewMag   = FindReloadMag(WeaponId);
    if (NewMag == INDEX_NONE) return;

    WriteBackAmmo();                                     // ★ 4번째 write-back 경로 (아래)

    if (OldMag != INDEX_NONE) Inv->SwapEntries(OldMag, NewMag);
    else                      Inv->MoveEntry(NewMag, WeaponId, "Mag");

    InjectAmmoFromMag(NewMag);                           // MaxAmmo 먼저 (05-5 함정 5)
}
```

#### 어느 탄창을 고르나 — `SortKey`가 답이다

```cpp
int32 UEPCombatComponent::FindReloadMag(int32 WeaponId) const
{
    for (int32 C : Inv->GetInsertionOrder())              // 본체 → 배낭 → 파우치
        for (int32 Id : Inv->GetSortedContents(C))        // ★ 화면에 보이는 순서
            if (IsCompatible(Id, WeaponId)) return Id;
    return INDEX_NONE;
}
```

**사용자가 드래그로 정렬한 순서가 곧 재장전 우선순위다.** 새 정책 필드가 0개고 설명이 필요 없다 — *"위에 둔 게 먼저 들어간다."*

**`SortKey`가 서버로 온 것의 두 번째 소비자**이기도 하다(첫째는 Step 04 UI). 11차 검수가 지적한 *"지금 소비자가 하나뿐"* 이 여기서 닫힌다.

> 정책은 바뀔 수 있다(*"탄 많은 것 먼저"*). 그래서 **함수 하나에 가둔다** — 바꿀 곳이 한 곳이다.

#### 경계 — 어빌리티는 인벤토리를 모른다

```
GA_Item_Reload          "재장전해" 만 말한다
   ├─ CanActivate  →  Combat->HasCompatibleMag()      (클라도 계산 — COND_OwnerOnly로 복제돼 있다)
   ├─ 예측: 애님 · ReloadTime
   └─ 서버      →  Combat->PerformReload()            ★ 한 줄
                      └─ Inv->SwapEntries / MoveEntry
```

`RemoveEntry` → `UnequipWeapon()`이 한 줄인 것과 **정확히 같은 모양**이고, **어빌리티 예측 경로에 서버 권위 자료구조가 들어오지 않는다.**

Step 05의 완료 조건(*"`GA_Item_PrimaryUse` / `GA_Item_Reload`는 한 줄도 수정하지 않았다"*)은 이때 깨지지만, **깨지는 폭이 `PerformReload()` 호출 한 줄**이다.

#### ★ `MaxAmmo`의 출처 — 05-5의 준비는 ⓑ의 답이다

`05_Loot_05_Equipment.md:73`이 `AEPWeapon::GetMaxAmmo()`를 *"부착물 준비의 전부"* 라고 적었다. **그것은 ⓑ 세계(확장탄창 = `MaxAmmo`를 올리는 스탯 부착물)의 답이고, ⓐ에서는 다른 답이 된다.**

ⓐ에서 `MaxAmmo`는 **꽂힌 탄창의 `Capacity`** 이고, 그건 무기 스탯이 아니라 **인벤토리 엔트리**다. 무기 액터가 그걸 읽게 하려면 액터에 `EntryId`를 심고 인벤토리를 뒤지게 해야 한다 — **§4-8이 여태 피해온 방향**이다.

**읽는 주체는 `UEPCombatComponent`다.** `SetMaxAmmo`가 불리는 곳은 장착·재장전 둘뿐이고 **둘 다 이미 `EntryId`를 손에 쥐고 있다.** `GetMaxAmmo()`는 Spread/Recoil 계열 합산용으로 남고 **탄약에서는 손을 뗀다.**

#### UI — 파라미터 하나가 갈린다

Step 04는 장비 슬롯을 이렇게 그린다.

```cpp
const int32 Id = Inventory->GetEntryInSlot(INDEX_NONE, SlotId);   // 04-4 :331 — 본체 고정
```

무기 부착물 슬롯(`Optic`/`Muzzle`/`Grip`/`Mag`)은 **부모가 `INDEX_NONE` 대신 무기 `EntryId`일 뿐 같은 위젯**이다. → **`UEPEquipSlotWidget`이 `(ParentEntryId, SlotId)` 쌍을 받게 해 두면 무기 검사창이 사실상 공짜다.** 위젯 하나, 드래그 경로 하나를 그대로 쓴다.

#### ★★ Step 05에서 미리 해두면 공짜인 것 — 셋

셋 다 **§7-3·미정 #1이 이름으로 예고한 확장점**이고(§2 판단 기준 통과), 합쳐서 20줄 안쪽이다.

| | 지금 비용 | 안 하면 나중에 |
|---|---|---|
| **`WriteBackAmmo()`로 함수 추출** | `UnequipWeapon` 안 두 줄을 빼는 것 | 재장전이 **4번째 write-back 경로**가 되는데 *"전부 `UnequipWeapon()`을 거친다"* 계약(05-5 함정 6)이 깨진 채 발견된다 |
| **`UEPEquipSlotWidget`에 `ParentEntryId`** | 필드 하나 + `INDEX_NONE` 기본값 | 위젯을 복제하거나 Step 04를 다시 연다 |
| **`MaxAmmo` 주입을 `InjectAmmoFromMag(int32)`로** | 이름만 미리 | 장착과 재장전이 **같은 순서 규칙**(`MaxAmmo` 먼저 — 05-5 함정 5)을 각자 구현한다 |

#### 하지 않는 것

- **`LoadedAmmoId`** — 소비자 없음. 미정 #6과 함께 온다
- **탄약상자로 탄창 채우기** — **다른 상호작용**이다(전투 중이 아니고, 시간이 걸리고, 인벤토리 화면 안이다). `AddEntryCharges` 두 번이면 되지만 재장전과 섞지 않는다
- **탄종 호환 매트릭스·태그** — `Caliber` 비교 한 줄로 충분하다
- **무기 액터가 인벤토리를 아는 경로**

#### 안 바뀌는 것

`FEPInventoryEntry` 구조 · `RemoveEntry` 캐스케이드(탄창은 무기 자식이라 **자동으로 딸려 나가고 딸려 들어온다**) · `SortKey` · `MoveEntry` · `AddSubtree` · 복제 · 칸 계산. **자료구조 작업은 0이다** — §7-3이 그렇게 설계해 뒀다.

---

### 7-5. 장비 효과 — 장착이 **어빌리티를 준다** (2026-08-26 신설)

**방어구·헬멧·특수 장비가 능력치나 어빌리티를 주는 자리다.** 지금은 아이템이 **인벤토리 안에서만** 의미를 갖고, 몸에 걸쳤을 때 캐릭터가 달라지는 경로가 **하나도 없다** — 무기의 잔탄이 유일한 예외다(§4-8).

**실무 표준은 "저장은 FastArray, 효과만 GAS"다.** Lyra가 그 형태이고 두 시스템이 정확히 한 지점에서 만난다.

```cpp
// LyraEquipmentDefinition.h:49-51
// Gameplay ability sets to grant when this is equipped
UPROPERTY(EditDefaultsOnly, Category=Equipment)
TArray<TObjectPtr<const ULyraAbilitySet>> AbilitySetsToGrant;

// LyraEquipmentManagerComponent.cpp:91-93   장착
for (const TObjectPtr<const ULyraAbilitySet>& AbilitySet : EquipmentCDO->AbilitySetsToGrant)
    AbilitySet->GiveToAbilitySystem(ASC, /*inout*/ &NewEntry.GrantedHandles, Result);

// LyraEquipmentManagerComponent.cpp:118     해제
Entry.GrantedHandles.TakeFromAbilitySystem(ASC);
```

**인벤토리 자체는 ASC가 아니다** — `ULyraInventoryManagerComponent : UActorComponent`다. **어트리뷰트로는 아이템을 표현할 수 없기 때문이다**: 스칼라가 `float` 하나뿐이라 `ItemId(FName)`·`ParentEntryId`·`SlotId(FName)`·트리 구조가 들어갈 자리가 없고, 어트리뷰트 개수는 `UPROPERTY`로 **컴파일 타임에 고정**인데 인벤토리는 가변 배열이다. 태그도 답이 아니다 — **태그는 상태이고, *"3번 가방 안 두 번째 칸"* 을 말하지 못한다.**

#### 우리 구조에 붙는 모양

```cpp
// UEPItemDefinition — 착용형 아이템만 채운다
UPROPERTY(EditDefaultsOnly, Category = "Equipment")
TArray<TObjectPtr<const UEPAbilitySet>> AbilitySetsToGrant;
```

| 시점 | 무엇 |
|---|---|
| **몸 슬롯에 들어갔다** | `GiveToAbilitySystem` — 핸들을 보관 |
| **몸 슬롯에서 나갔다** | `TakeFromAbilitySystem` |
| **엔트리가 사라졌다** | 같은 회수. `RemoveEntry`가 자동으로 |

#### ★ 지금 확보돼 있는 것 — 훅이 **한 곳**이다

- **`MoveEntry`가 `SlotId`를 고치는 유일한 지점이다**(9차 확정, `05_Loot_03_Inventory.md` 03-2). 부여/회수가 **그 함수 끝의 분기 하나**로 끝난다. `SlotId`가 여러 곳에서 바뀌는 설계였다면 훅이 그 수만큼 늘고 하나를 빠뜨리면 **벗었는데 효과가 남는다.**
- **`RemoveEntry`의 4단계 순서가 회수 자리를 이미 갖고 있다.** ① write-back → ② 스냅샷 → ③ `RemoveSelf` → ④ 캐스케이드에서 **①과 같은 칸**이다 — `RemoveSelf` 뒤에는 `GetEquippedEntryId()`가 `INDEX_NONE`이라 회수 대상을 잃는다(Step 05 05-3의 write-back과 **같은 함정**).
- **`SwapEntries`(04-7)도 이 훅을 지난다** — 두 엔트리가 슬롯을 맞바꾸므로 회수 둘 + 부여 둘이거나, 같은 슬롯이면 0이다.

#### ★ 핸들을 어디 두나 — **엔트리에 넣으면 안 된다**

`FGameplayAbilitySpecHandle`은 **서버 전용 식별자**이고 `FEPInventoryEntry`는 **복제된다**. Lyra는 핸들을 인벤토리가 아니라 **장비 매니저의 별도 배열**(`FLyraAppliedEquipmentEntry::GrantedHandles`)에 두어 이 문제를 피한다.

**우리는 컴포넌트에 서버 전용 맵을 둔다.**

```cpp
// UEPInventoryComponent — 복제하지 않는다. 서버에만 산다
UPROPERTY(NotReplicated) TMap<int32, FEPGrantedHandles> GrantedByEntry;   // EntryId → 핸들
```

> **이게 §7-6(컴포넌트 3분할)의 첫 번째 실질적 근거가 된다.** *"서버 전용 핸들 맵"* 은 인벤토리(가진 것)의 관심사가 아니라 **장비(입은 것)의 관심사**다. 지금은 한 컴포넌트라 그냥 필드가 하나 느는 것이지만, 나눌 때 **통째로 따라가는 덩어리**로 미리 알아둔다.

#### 하지 않는 것 — 그리고 결정 신호

**지금 만들지 않는다. 소비자가 0개다** — `DT_Items`에 방어구 행이 하나도 없고 `UEPAbilitySet`도 없다(GAS 단계는 어빌리티를 직접 부여한다). CLAUDE.md §2 기준으로 *"두 번째 구현자가 없는 인터페이스"* 다.

**결정 신호:** **방어구·헬멧이 기획에 들어올 때.** Step 05가 *"하지 않는 것"* 에 이미 이름으로 적어뒀다. 그때 §4-9의 *"아이템 종류를 늘려도 인벤토리를 안 고치게"* 가 이 필드 하나로 확장된다.

> **잔탄과 같은 모양이지만 방향이 반대다.** 잔탄은 **값을 왕복**시킨다(`Charges` ↔ `Ammo`, §4-8). AbilitySet은 **핸들을 들고 있다가 되돌려준다** — 값이 오가지 않는다. 그래서 write-back 순서 함정은 공유하되 *"어느 쪽이 진실인가"* 라는 질문은 없다.

---

### 7-6. 컴포넌트 3분할 — **가진 것 / 입은 것 / 손에 든 것** (2026-08-26 신설)

**`UEPInventoryComponent` 하나가 지금 셋을 든다.**

| 관심사 | 지금 드는 것 |
|---|---|
| **가진 것** | `Entries`(FastArray) · `AddItem` · `RemoveEntry` · `CanFit` · `SortKey` 일습 |
| **입은 것** | `SlotId` 판정 — `CanPlaceInSlot` · `GetEntryInSlot` · `BodySlots` · (§7-5의 핸들 맵) |
| **손에 든 것** | `ActiveHotbarIndex` · `HotbarRefs` · `GetEquippedEntryId` · `Server_SetActiveHotbarIndex` |

**실무 표준은 셋으로 나누는 것이고, 핵심은 "클래스 이름"이 아니라 소유자다** (Lyra 직독).

```cpp
ULyraInventoryManagerComponent : public UActorComponent        // 아무나 들 수 있다
ULyraEquipmentManagerComponent : public UPawnComponent         // 몸에 붙는다
ULyraQuickBarComponent         : public UControllerComponent   // 폰이 죽어도 산다
```

**소유자가 다른 것이 분할의 이유 전체다.** 상자·시체·자판기는 *"가진 것"* 만 있으면 되고 *"입은 것"* 이 없다. 핫바는 **폰보다 오래 살아야** 리스폰 후에도 유지된다.

#### ★ 우리의 분할 신호는 §7-1이다

**월드 컨테이너(나무 상자)가 `UEPInventoryComponent`를 통째로 달아야 하는 첫 소비자다.** 그러면 상자가 **몸 슬롯과 핫바를 갖는다.**

> **§8 미정 #10이 이미 같은 지점을 짚었다** — *"`BodySlots`가 답해야 할 질문은 **이 소유자에게 그 슬롯이 있는가**인데 전역 싱글턴은 **그 이름이 몸 슬롯인가**만 답한다. §7-1에서 나무 상자가 상의를 입는다(검사 3·5를 다 통과한다)."*
>
> **미정 #10은 그것을 데이터의 자리 문제로 봤고(`UEPPawnInventoryData`), §7-6은 같은 것을 컴포넌트의 자리 문제로 본다.** 둘 다 맞고 **같이 온다** — 상자가 `Equipment` 컴포넌트를 안 달면 `BodySlots`를 읽는 코드에 애초에 도달하지 않는다.

#### ★ 지금 확보돼 있는 것 — **데이터가 안 움직인다**

**장착의 진실이 `FEPInventoryEntry::SlotId`, 즉 엔트리 안에 있다**(9차 확정). 그래서 장비를 별도 컴포넌트로 빼도 **배열은 인벤토리에 그대로 남고 함수만 옮긴다.**

```
Lyra :  Slots 배열이 QuickBar 컴포넌트에 있다   →  분리가 자료구조에 박혀 있다
우리 :  SlotId 가 엔트리에 있다                 →  분리가 함수 배치의 문제일 뿐이다
```

**나갈 것은 `ActiveHotbarIndex` · `HotbarRefs` · `GetEquippedEntryId` · `SetActiveHotbarIndex` · `Server_SetActiveHotbarIndex` ＋ §7-5의 핸들 맵이다.** `Entries` · `SortKey` · `CanFit` · `RemoveEntry`는 **한 줄도 안 움직인다.**

> **`GetEquippedEntryId()`가 셋을 관통한다** — `ActiveHotbarIndex`(손) → 슬롯 이름(입은 것) → `Entries` 조회(가진 것). **분할의 실제 비용은 이 함수 하나**이고, 컴포넌트 간 참조를 어느 방향으로 둘지가 그때의 유일한 설계 질문이다. Lyra는 `QuickBar → EquipmentManager`를 **찾아가는 함수**로 푼다(`FindEquipmentManager()`, `LyraQuickBarComponent.h:56`).

#### 하지 않는 것 — 그리고 결정 신호

**지금 나누지 않는다.** 소비자가 하나(플레이어 폰)이고, 세 상태가 서로를 읽는다. 나누면 **오늘 얻는 것 없이 컴포넌트 간 참조 배선이 생긴다** — CLAUDE.md §2의 *"소비자가 하나인 계층"* 이다.

**결정 신호 둘:**
- **ⓐ §7-1 월드 컨테이너** — *"가진 것"* 만 있는 소유자가 처음 생긴다. **이쪽이 먼저다**
- **ⓑ 리스폰 후 핫바 유지** — 핫바가 폰보다 오래 살아야 하면 `UControllerComponent`로 나간다. 지금은 사망 시 전부 드랍(미정 #4)이라 **살릴 것이 없다**

---

## 8. 열린 결정사항

### 확정된 것

| 항목 | 결정 | 근거 |
|---|---|---|
| 개체 상태의 형태 | **`FEPItemState` (USTRUCT, 값 타입).** `UEPItemInstance`/`UEPWeaponInstance`/인스턴스 서브시스템 **전부 삭제** | 담는 게 숫자 둘이고 다형성은 이미 Definition에 있다. UObject성이 Outer·핸들·이관 프로토콜을 강제했다 (§4-1) |
| 스택 | **없다.** 붕대 3개 = 엔트리 3개 | 타르코프식. 탄약은 탄약상자의 `Charges`, 돈은 현금뭉치의 `Charges` (§4-1) |
| 균질 아이템 합치기 | **한다** (`FEPItemData::bFungible` — 현금뭉치·탄약상자) | 칸 수가 `Charges`와 무관해 스택의 부담이 하나도 안 따라온다. 가방이 차도 돈·탄약은 들어간다 (§4-6) |
| 인벤토리 용량 | **컨테이너별 칸 수 합산. 본체는 0칸이고 수납은 착용 컨테이너에서만 나온다** (2026-08-25) | 무게 시스템과 동형. 격자 배치가 아니라 스칼라 판정. `MaxSlots`는 테스트 중에만 10 (§4-6) |
| 시작 장비 | **`UEPLootDeveloperSettings::StartingEquipment`** → `BeginPlay`에서 `TryAutoEquip` (2026-08-25) | 본체가 0칸이라 아무것도 안 입으면 첫 아이템도 못 줍는다. 로비가 생기면 **배열을 채우는 쪽만** 바뀐다 (§4-6) |
| 서브트리 삽입 | **`AddSubtree(Parent, SlotId, In)`** — 목적지는 `(Parent, SlotId)` 쌍, **기본값 없음** (2026-08-25) | `InsertEntry`·`MoveEntry`와 같은 어휘. 반쪽만 받으니 슬롯 정보가 스냅샷에서 새거나 아예 표현이 안 됐다 (`05_Loot_03_Inventory.md` 함정 4y·4z) |
| 컨테이너 중첩 | **`ParentEntryId`.** 배낭·부착물·상자가 전부 같은 표현 | 자기 타입 재귀는 `Fatal`이고, 평면 표현은 깊이와 무관하다 (§4-6, §7-3) |
| 불변식 강제 | **`RemoveEntry()`의 반환값 + `AddEntryCharges()` 내부**에서. 문서 규칙 금지 | write-back 순서·`MarkItemDirty`를 규율에 맡기면 3개월 뒤 깨진다 (§4-7) |
| 인벤토리 부착 위치 | **Character** | 사망 시 소실이 규칙과 일치 (§4-6) |
| 픽업이 복제하는 것 | **`ItemId`만.** `FEPItemState`는 서버 전용 | 바닥 무기의 잔탄이 보이면 "여기서 누가 죽었다"가 추론된다 — GAME.md 정보 은폐 (§4-4) |
| 인벤토리 복제 | **FastArray + POD**, `State`를 엔트리에 **내장** | 내부 struct 델타가 기본 활성이라 변경 필드만 나간다 (§4-6) |
| 루트 테이블 | **가중치 + 중첩.** 수량 필드 없음 | GAME.md 등급 확률 보존 (§4-2) |
| 부분 획득 | **없다** — 전부 아니면 전무 | 픽업 하나 = 아이템 하나 (§4-4, §4-6) |
| 아이템 버리기 | **포함** (Step 03) | `Pickup->State = Entry.State` 값 복사 (§4-7) |
| 무기 장착 흐름 이관 | **포함** (Step 05) | 탄약은 엔트리가 진실, 어트리뷰트는 뷰 (§4-8) |
| 장비 슬롯 | **별도 배열도 필드도 없음** — **`FEPInventoryEntry::SlotId`가 진실** (9차) | 슬롯 12개 + 부착 4를 필드로 표현하면 필드가 12개가 되거나 `TMap`을 따로 복제해야 한다. `SlotId`는 이미 엔트리 안에 있고 이미 복제된다. 엔진 선례: `SceneComponent.h:108-119` (§4-8) |
| 장비 슬롯 — 남는 상태 | **`ActiveHotbarIndex`(int32) 하나** | 가리키는 것이 엔트리가 아니라 **슬롯**이라 죽은 번호가 생길 문법이 없다 (§4-8) |
| 엔트리 이동 | **`MoveEntry(EntryId, NewParent, NewSlotId)`** 가 유일한 지점 | `InsertEntry`·`SetEntryCharges`·`RemoveSelf`와 같은 규칙의 네 번째 적용. `MarkItemDirty`를 빠뜨릴 문법이 없어진다 |
| 슬롯 자격 | **`FEPItemData::SlotPriority`(`TArray<FName>`)** — 순서가 우선순위 | 태그 컨테이너는 순서를 표현하지 못한다. 열거형은 슬롯 추가가 코드 변경이 된다 |
| 슬롯 종류 판정 | **`UEPLootDeveloperSettings::BodySlots`** | 몸 슬롯인지 부착 슬롯인지를 아는 유일한 곳. 없으면 *"가방 안에 든 상의를 입는다"* 가 표현된다 |
| 인벤토리 복제 조건 | **`COND_OwnerOnly`** | 남의 가방이 보이면 치트 + 대역폭 (§4-6) |
| 식별자 | **`FEPInventoryEntry::EntryId` (`int32`, 서버 발급, 재번호 없음)** | FastArray는 클라 배열 순서를 보장하지 않는다. `FGuid`/`SchemaVersion`은 제거 (§4-1) |
| 상태 초기화 | **Definition의 virtual `InitState()`** + 기본 클래스 `InitialCharges` | static 팩토리로는 타입 분기가 계속 자란다 (§4-9) |
| 픽업 복제 | **`DORM_Initial`** + Tick off | 복제 상태가 아예 불변이라 `FlushNetDormancy()`조차 필요 없다 (§4-4) |
| Definition 보유 대상 | **모든 아이템.** 예외 없음 | 바닥 픽업은 `WorldMesh`가, UI는 `Icon`이, 생성은 `InitState()`가 필요하다 (§4-9) |
| Definition 로딩 | **매치 전 전량 상주** (AssetManager). 소프트는 `WorldMesh`/`Icon`/`WeaponMesh`만 | 획득은 동기 경로라 로드를 기다릴 수 없다 (§4-1) |
| 무기 액터 스폰 책임 | **`UEPCombatComponent`** 유지. 인벤토리는 `EntryId`만 넘긴다 | 무기 수명·부착·애님·어빌리티를 이미 쥐고 있다 (§4-8) |
| 드랍 / 장착 RPC 파라미터 | **`EntryId`** | 재번호되지 않는 고정 번호라 경쟁이 없다 (§4-7) |
| `FInstancedStruct` 전환 기준 | **세 번째 아이템 카테고리가 자기 전용 필드를 요구할 때** | 지금 도입하면 프로퍼티 델타를 잃고 아무것도 못 받는다 (§4-1) |

### 미정

| # | 항목 | 선택지 | 비고 |
|---|---|---|---|
| 1 | **탄창이 별도 아이템인가** | ⓐ `[Mag]` 슬롯의 부착물 (탄창마다 `Charges`, 미리 채워 여러 개 휴대 — 타르코프) / ⓑ 무기 자신의 `Charges` + 확장탄창은 `MaxAmmo`를 올리는 스탯 부착물 (배그) | **깊이는 어느 쪽이든 1이고 자료구조가 같다** (§7-3). **Step 00~05 범위 밖** — Step 05는 무기 자신의 `Charges`(=ⓑ)로 만들고, ⓐ로 갈 때 장착 시 읽는 대상만 자식 엔트리로 바꾼다. **★ ⓐ의 구현 기획은 §7-4에 있다** (2026-08-24) — 재장전이 `SwapEntries` 하나로 떨어지고 신규 API는 `FindReloadMag` 하나다. **`AmmoType`은 둘로 갈렸다**: 규격(`Caliber`)은 Definition, 안에 든 탄종(`LoadedAmmoId`)은 `FEPItemState`이되 **#6과 함께 온다** |
| 2 | 스폰 시드 | 매치마다 랜덤 / 시드 고정 | 재현 가능한 테스트가 필요하면 `FRandomStream` + 서버 시드 복제 |
| 3 | 배낭 종류·용량 | ~~소형/중형/대형 몇 칸씩인가~~ | **✅ 테스트값 확정 (2026-08-25, §4-6).** A `SlotSize 15 / Cap 12`, B `10 / 8`. 상의 `11 / 10`, 하의 `6 / 5`, 본체 **0**. 종류는 DT 행 추가로 계속 는다. **규칙은 `ContainerCapacity < SlotSize`**(등호 없음 — 넣기 판정의 `≤`와 다른 식이다). 벗기·착용이 둘 다 칸 검사를 안 지나므로 `SlotSize`가 본체 용량에 **묶이지 않는다.** `IsDataValid()`가 강제한다 |
| 4 | 사망 시 드랍 — **구현 시점만 미정** | ~~인벤토리 통째 / 배낭 하나로~~ | **선택지가 아니다.** GAME.md 인벤토리 절이 **"사망 시 전부 드랍"** 으로 이미 정했다. 시체 위치에 픽업 하나 + **전 엔트리**를 서브트리로 (§4-7). **이번 단계 범위 밖** |
| 5 | ~~무기 2정 이상~~ | ~~주무기/보조 슬롯~~ | **✅ 확정으로 이동 (9차, 2026-08-22).** 핫바 1~4로 확정 — `Hotbar1`/`Hotbar2`는 무기 전부, `Hotbar3` 보조, `Hotbar4` 근접. `TMap`이 아니라 **엔트리의 `SlotId`** 로 표현한다. `DOCS/Mine/EquipmentSlots.md` §1 |
| 6 | **재장전의 탄약 소비** | 무한 재장전 유지 / 탄약상자 `Charges` 차감 | 루트 테이블에 `AmmoBox_545`가 들어가는데 `GA_Item_Reload`는 인벤토리를 보지 않는다 — §4-9의 소모품과 같은 "있는데 쓸 수 없는" 상태. **이번 단계 범위 밖**이지만, Step 05의 `Entry.State.Charges` 주입 구조가 들어오면 `AddEntryCharges(Id, -N)`이 차감 지점이 된다. #1의 답에 직접 걸린다. **★ #1의 ⓐ와 같이 오지 않는다** (§7-4) — 재장전은 전투 중 어빌리티, 탄창 채우기는 인벤토리 화면 안의 시간 걸리는 조작이라 **섞지 않는다.** `FEPItemState::LoadedAmmoId`가 처음 의미를 갖는 지점이 여기다 |
| 7 | DB 영속화 대상 | 어떤 필드를 저장할 것인가 | `FEPItemState`는 `FJsonObjectConverter` 한 줄로 직렬화된다. 영구 식별자와 스키마 버전은 **행 봉투**에 둔다 (§4-1). **★ 둘을 같이 저장한다 — `NextEntryId` / `ActiveHotbarIndex`.** 장착·배낭은 엔트리의 `SlotId`에 들어 있으므로 배열만 저장하면 따라온다(9차). `NextEntryId`를 빠뜨리면 로드 후 1부터 재발급해 기존 엔트리와 충돌하고 `ParentEntryId`를 오염시킨다 |
| **8** | **상의·하의가 컨테이너를 유지하는가** | 유지 / **가방과 외투만 컨테이너** | 사용자 예고 (2026-08-25). 이행이 **데이터 둘**이다 — `DT_Items`의 `ContainerCapacity`를 0으로, `ContainerOrder`에서 제거. **코드 변경 0**(§4-6). 그때 `SlotSize` 11/6의 하한도 사라져(`0 < SlotSize`) *"접으면 2칸"* 같은 값으로 내려간다. **결정 신호:** 옷이 방어구 역할만 하고 수납은 조끼·배낭이 전담하게 될 때 |
| **9** | **본체 0칸 전환 시점** | 지금 / Step 03 검증 후 | `MaxSlots = 10`은 **테스트값**이다(2026-08-25). 0으로 내리면 `StartingEquipment`가 **필수**가 되고 완료 조건 2~6의 검증 경로가 상의 컨테이너로 옮겨간다. **필드는 지운다가 아니라 0을 넣는다** — `GetInsertionOrder`의 맨 앞 `INDEX_NONE`이 그대로 남아도 `CanFit`이 걸러낸다. `GAME.md` 인벤토리 절도 같이 고친다. **★ 전환 시점은 03-B(줍기·버리기) 완료 이후다** (13차 답변) — `StartingEquipment`가 돌아야 컨테이너가 생기고, 그 전에는 `EP.Inv.Add`가 본체로만 넣는다. **그리고 전환 후 03-A 완료 조건 아홉 개를 다시 돌리려면 `EP.Inv.Add`에 `[Container]` 인자가 있어야 한다** — 없으면 회귀 테스트를 잃는다 |
| **10** | **장비 구성 셋을 드는 곳** (14차에 `StartingEquipment` 하나에서 **셋**으로 넓어졌다 — ＋`BodySlots`·`ContainerOrder`) | `UEPLootDeveloperSettings` (지금) / **`UEPPawnInventoryData`**(`UPrimaryDataAsset`) | 지금은 **전역 기본값**이고 `UGameMapsSettings::GlobalDefaultGameMode`(`GameMapsSettings.h:216`)와 같은 모양이다 — *"다른 방법으로 지정되지 않았을 때 쓰는 값"*. **로비가 오면 드는 곳이 `UPrimaryDataAsset`으로 옮겨간다**(Lyra `ULyraPawnData`가 `AbilitySets`·`InputConfig`를 그렇게 든다 — `LyraPawnData.h:25-53`)이고 이 필드는 폴백이 된다. 결정 신호: *"모드나 캐릭터마다 시작 장비가 달라져야 한다"*<br>**★ 6차의 `UDeveloperSettings` 확정을 근거로 삼지 않는다** (13차 답변) — 6차 근거는 *"인스턴스 없이 읽어야 하는 소비자가 둘 이상"*(`BodySlots`는 `MoveEntry`와 UI가 읽는다)이었고, 이 필드는 **소비자가 하나이고 캐릭터 액터 문맥에서 읽는다.** 근거를 잘못 적으면 **다음에 오는 콘텐츠성 전역 필드가 6차를 인용해서 들어온다**<br>**★★ 14차 — 셋이 같은 배를 탄다.** `BodySlots`·`ContainerOrder`도 *"캐릭터가 무엇으로 이루어졌는가"* 라 같은 종류다. **`BodySlots`가 특히 그렇다** — 답해야 할 질문이 *"이 소유자에게 그 슬롯이 있는가"* 인데 전역 싱글턴은 *"그 이름이 몸 슬롯인가"* 만 답한다. **트리거가 둘로 늘었다:** ⓐ 로비(위) ⓑ **§7-1 월드 컨테이너** — 슬롯이 소유자별이 되는 첫 지점이고, 그때까지 나무 상자가 상의를 입는 것이 *"우연히 도달 불가"* 로만 막혀 있다. **이전 비용은 읽는 곳 둘**(`CanPlaceInSlot` · Step 04 UI)**이라 지금 만들지 않는다** — §9 참조 |

---

## 9. 개발·디버그 도구

확률 시스템은 **눈으로 검증할 수 없다.** 처음부터 도구를 같이 만든다. 기존 `UEPCombatDeveloperSettings` 패턴을 그대로 따른다.

```
UEPLootDeveloperSettings : UDeveloperSettings
├─ bool                    bEnableLootDebugLog      스폰 판정 결과를 로그로
├─ bool                    bEnableSpawnerDebugDraw  에디터/PIE에서 스포너 위치·반경 시각화
├─ TSoftObjectPtr<UDataTable> ItemDataTable         DT_Items (하드코딩 금지)
├─ TArray<FName>           BodySlots                몸 슬롯 12 (핫바 4 + 착용 8)      — 9차
├─ TArray<FName>           ContainerOrder           획득 시 볼 컨테이너 순서          — 10차
└─ TArray<FName>           StartingEquipment        스폰 시 입힐 아이템 ["Shirt_Basic","Pants_Basic"]  — 13차
```

> **★★ 아래 셋(`BodySlots`·`ContainerOrder`·`StartingEquipment`)은 이 클래스의 나머지와 종류가 다르다 — 임시 자리다 (14차).**
> 위 셋은 *"어디에 있는 무엇을 쓰나"* ＋ *"디버그"* 인데, 아래 셋은 **캐릭터가 무엇으로 이루어졌는가**다. **§8 미정 #10이 셋 전부를 관장한다** — 최종 자리는 `UEPPawnInventoryData`(DataAsset)다.
>
> **6차의 *"전역 데이터 참조는 `UDeveloperSettings`"* 를 근거로 인용하지 않는다.** 그 문장은 *"전역 **데이터 참조**"* 이지 *"전역 데이터"* 가 아니다 — `ItemDataTable`·`PickupClass`처럼 **가리키는 것**을 말한 것이다. 13차가 `StartingEquipment`를 붙이며 그 구분을 흘렸고, 검수에서 근거가 교체되며 미정 #10이 생긴 것이 그 흔적이다.
>
> **`BodySlots`가 지금 답하는 질문이 반쪽이다.** *"`Torso`가 몸 슬롯인가"*(전역, 설정이 답할 수 있다)와 *"이 인벤토리 소유자에게 `Torso`가 있는가"*(소유자별, 전역 싱글턴은 못 답한다)가 섞여 있다. 검사 3은 뒤를 물어야 하는데 앞을 묻는다 — **§7-1 월드 컨테이너에서 나무 상자가 상의를 입는다**(검사 3·5를 다 통과한다). 지금은 상자의 `MoveEntry`를 부르는 표면이 없어 도달 불가이고, 13차가 잡은 `GetOwner<AEPCharacter>()` 무보호 역참조와 **같은 종류의 우연**이다.
>
> **그래도 지금 옮기지 않는다** — 읽는 곳이 둘(`CanPlaceInSlot` 검사 3 · Step 04 슬롯 그리기)이라 이전 비용이 낮고, 지금 만들면 **소비자가 하나인 계층**이 는다(CLAUDE.md §2). 옮길 때의 모양은 `ItemDataTable`과 같다 — **설정이 가리키고, 내용은 에셋에 있다.**
>
> ```
> TSoftObjectPtr<UEPPawnInventoryData> PawnInventoryData;   // ← 참조는 설정, 데이터는 DataAsset
> ```

> **★ 이 클래스의 규칙: 에셋 참조와 디버그만 둔다 (14차 명문화).** 위 다섯 필드가 이미 그 규칙을 지키고 있는데 적혀 있지 않아서 13차가 콘텐츠성 필드를 붙였다. **Lyra도 같다** — `ULyraDeveloperSettings`·`ULyraCosmeticDeveloperSettings`·`ULyraWeaponDebugSettings`가 **전부 `config=EditorPerProjectUserSettings`** 이고(`LyraDeveloperSettings.h:39` · `LyraCosmeticDeveloperSettings.h:25` · `LyraWeaponDebugSettings.h:14`) 내용은 `ExperienceOverride`·봇 수 오버라이드·`bTestFullGameFlowInPIE`·자주 쓰는 맵 목록이다. **게임플레이 구성은 DataAsset이 든다**(`ULyraPawnData : UPrimaryDataAsset` — `PawnClass`·`AbilitySets`·`InputConfig`·`DefaultCameraMode`, `LyraPawnData.h:24-53`). **슬롯 개수조차 컴포넌트 필드다** — `ULyraQuickBarComponent::NumSlots`(`LyraQuickBarComponent.h:63-64`).

> **`FName` 경로가 아니라 `TSoftObjectPtr<UDataTable>`을 쓴다.** 다만 이유는 *"리다이렉터가 알아서 따라가서"* 가 **아니다** — `.ini`에 적힌 경로는 에셋 리네임 시 **자동으로 고쳐지지 않는다.** 엔진이 리네임 다이얼로그에 그렇게 적어 놓았다.
>
> ```
> AssetRenameManager.cpp:463
> "Source code, config INI, and text files may need Find/Replace for: {0}
>  Otherwise assets can be missing from cooked builds. Continue with rename?"
> ```
>
> `TSoftObjectPtr`의 진짜 이득은 **깨질 때 시끄럽다**는 것이다.
>
> | | `FName`/`FString` | `TSoftObjectPtr` |
> |---|---|---|
> | Project Settings UI | 텍스트 입력 (오타 가능) | 에셋 피커 + `AllowedClasses` |
> | 리네임 탐지 | ❌ **침묵** | ✅ `FindCDOReferences`(`:708`)가 CDO를 통째로 직렬화해 잡는다 |
> | 리다이렉터 강제 생성 | ❌ | ✅ (`:796` `bCreateRedirector \|= bSetRedirectorFlags`) |
> | 경고 다이얼로그 | ❌ | ✅ (`:463`) |
>
> **그러므로 `.ini` 경로를 가진 에셋을 리네임할 때 뜨는 다이얼로그를 무시하지 않는다.** 무시하면 리다이렉터가 유일한 생명줄이 되고, `Fix Up Redirectors`가 그것마저 지운다.
>
> config 프로퍼티는 **애셋 레지스트리 의존 간선을 만들지 않는다.** 네이티브 CDO는 디스크 패키지가 아니라 인덱싱될 자리가 없다 — 쿠커가 맵·게임모드용 config 키 5개를 **이름으로 하드코딩**해 읽는 것이 그 증거다(`CookOnTheFlyServer.cpp:8840-8867`).
>
> 그래도 `DT_Items`는 쿡이 보장된다 — 각 `UEPItemDefinition`의 `ItemDataRow`(`FDataTableRowHandle::DataTable`은 **하드 참조**)가 끌고 간다. `.ini`가 깨지면 쿡 누락이 아니라 **런타임 null**이고, `BuildDataCache`의 Error 로그가 잡는다.

콘솔 커맨드 2개면 충분하다.

| 커맨드 | 용도 |
|---|---|
| `EP.Loot.RollTable <LootTableName> <Count>` | 지정 테이블을 N회 굴려 아이템별 집계를 출력. **등급 비율이 기획표와 맞는지 확인하는 유일한 수단.** 이름 해석은 `UAssetManager::GetPrimaryAssetObject`로 — 스포너가 참조하지 않는 테이블도 찾을 수 있어야 한다 (§4-2) |
| `EP.Loot.Respawn` | 월드의 모든 스포너를 Clear 후 재굴림 (서버 전용) |

`#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)` 가드는 SSR 디버그와 동일하게 적용한다.

---

## 10. 검증 원칙

- **모든 스폰·획득 판정은 서버에서만.** 클라이언트는 결과를 복제받을 뿐이다
- PIE 2인 멀티로 검증한다. **리슨서버 호스트가 아니라 클라이언트 쪽에서** 확인한다 — 호스트는 권한과 예측이 같아 동기화 버그가 드러나지 않는다 (GAS 08단계에서 겪은 교훈)
- 확률은 눈으로 못 믿는다. 스포너를 N회 굴리는 콘솔 커맨드나 로그 집계로 분포를 확인한다
