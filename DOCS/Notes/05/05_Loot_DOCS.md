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
    ↕ 상호작용(E) / 버리기(G)              ← 이번 단계 (양방향)
[인벤토리] 서버 권한 보관 + UI              ← 이번 단계
    ↓ 장착
[무기]    AEPWeapon 스폰·부착              ← 이번 단계 (기존 흐름 이관)
    ─────────────────────────────
[컨테이너] 공구상자/구급상자/가방 + 검색시간  ← 추후 (§7)
[자판기]   돈 투입 → 5초 → 배출            ← 추후 (§7)
```

**이번 단계에서 하지 않는 것:** 컨테이너, 자판기, 드래그앤드롭 UI, 격자(그리드) 인벤토리, 무게, 소모품 사용, 사망 시 드랍, 무기 2정 이상 슬롯.

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

§2가 진단한 대로 아이템 데이터 계층은 **전부 데드코드**다. 스포너가 `ItemId`로 아이템을 뽑으려면 `ItemId → DataTable → Definition` 조회 경로와 인스턴스 팩토리가 **먼저** 있어야 한다. 이걸 Step 01 안에서 같이 만들면 "스포너 작업"이 사실상 "데이터 계층 재설계 + 스포너"가 되어, 확률이 안 맞을 때 원인이 테이블인지 조회 경로인지 구분이 안 된다.

Step 00은 눈에 보이는 결과가 없는 대신 **콘솔 커맨드 하나로 독립 검증된다** — `ItemId`를 주면 올바른 타입의 인스턴스가 나오는가. 그게 확인된 뒤에 스포너를 붙인다.

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

### 4-1. 아이템 인프라 — 서브시스템 **2개**로 분리한다

수명이 다르므로 하나로 합치지 않는다.

| 서브시스템 | 종류 | 역할 | 수명 |
|---|---|---|---|
| `UEPItemDefinitionSubsystem` | `UGameInstanceSubsystem` | `ItemId → FEPItemData / UEPItemDefinition` 조회 (정적 데이터) | 앱 전체. 레벨 전환에도 유지 |
| `UEPItemInstanceSubsystem` | `UWorldSubsystem` (**서버 전용**) | 런타임 `UEPItemInstance` **소유**와 핸들 조회 | 월드(매치) 단위. 종료 시 일괄 정리 |

#### `UEPItemDefinitionSubsystem` — 조회

- `Initialize()`에서 `DT_Items`를 1회 순회해 `TMap<FName, FEPItemData>`를 캐시. 매 조회마다 `FindRow`를 돌지 않는다.
- **★ 행 포인터(`FEPItemData*`)를 캐시하지 않는다.** `FindRow`가 돌려주는 포인터는 DataTable의 `RowMap` 내부를 가리키므로, 에디터에서 DT를 리임포트하거나 핫리로드하면 재할당되어 **댕글링**한다. 패키지 빌드에서는 재현되지 않고 에디터에서만 나는 종류라 원인 추적이 오래 걸린다. `FEPItemData`는 작으므로 **값으로 복사**해 담는다.
- DataTable 경로는 하드코딩하지 않고 `UEPCombatDeveloperSettings`처럼 `UDeveloperSettings`에 노출한다. 타입은 `FName` 문자열 경로가 아니라 **`TSoftObjectPtr<UDataTable>`** — 문자열 경로는 에셋 이동·리네임에 침묵으로 깨진다 (§9).
- 데디케이티드 서버에서도 동작해야 한다(스폰 판정이 서버).

##### ★ Definition은 상주시킨다 — 소프트 참조는 시각 에셋에만

`FEPItemData::ItemDefinition`이 `TSoftObjectPtr`이라 "Definition도 비동기 로드"로 읽히기 쉬우나, **그러면 인스턴스를 만들 수 없다.** 픽업 획득은 RPC 응답 안에서 성패가 결정돼야 하는 동기 경로인데, §4-9의 `Definition->CreateInstance()`는 Definition이 메모리에 있어야 호출된다. 로드를 기다리는 사이에 "줍기 성공/실패"를 유보할 수 없다.

| 대상 | 정책 |
|---|---|
| `UEPItemDefinition` / `UEPWeaponDefinition` 에셋 | **매치 시작 전 전량 상주.** `UEPItemDefinitionSubsystem::Initialize()`에서 AssetManager로 `EPItemDefinition` PrimaryAssetType을 일괄 로드 |
| `WorldMesh` / `Icon` / `WeaponMesh` | **소프트 유지.** 픽업 메시 표시·UI 아이콘처럼 지연이 허용되는 지점에서 `FStreamableManager` 비동기 로드, 콜백에서 세팅 |

- Definition은 수치·참조만 담은 메타데이터라 개당 수 KB다. 수백 개여도 상주 비용이 문제되지 않는다
- 데디케이티드 서버는 Definition만 로드하고 시각 에셋 로드는 건너뛴다 — 이 분리가 성립하는 이유가 위 표다
- `AssetManager` 설정(Project Settings → Asset Manager)에 `EPItemDefinition` PrimaryAssetType을 등록해야 한다. Step 00의 완료 조건에 포함

#### `UEPItemInstanceSubsystem` — 소유 (★ Outer 문제 해결)

인스턴스의 `Outer`를 인벤토리 컴포넌트로 두면, 버리기(§4-7)에서 픽업으로 넘길 때 `Rename()`으로 Outer를 옮겨야 하고 컴포넌트 파괴 시 GC 위험이 생긴다. **인스턴스는 서브시스템이 소유하고, 인벤토리·픽업은 참조만 갖는다.**

```
UEPItemInstanceSubsystem (서버 전용)
├─ TMap<int32, TObjectPtr<UEPItemInstance>> Instances   ← 유일한 강참조
├─ int32 NextHandle = 1                                  ← 단조 증가 발급
├─ CreateInstance(FName ItemId) → int32                  ← 상태 보유 아이템만 (아래 참조)
├─ Find(int32 Handle) → UEPItemInstance*
└─ Destroy(int32 Handle)
```

- **이관이 핸들 대입으로 끝난다.** 인벤토리 → 픽업 → 인벤토리를 오가도 `Rename()`이 없고, 인스턴스는 처음부터 끝까지 서브시스템 소유다
- 핸들 조회가 O(1)이다
- 인벤토리·픽업은 **`int32` 핸들만** 들고 있는다. `UEPItemInstance*`를 멤버로 두면 강참조가 둘이 되어 "유일한 강참조" 전제가 깨진다

#### ★ 인스턴스를 언제 만드는가 — 상태 없는 아이템은 만들지 않는다

`FEPInventoryEntry`는 핸들 1개 + `Quantity` N개인데 `UEPItemInstance`에도 `Quantity`가 있다(`EPItemInstance.h:27`). 그대로 두면 **§4-8에서 지적한 "탄약의 진실이 두 곳" 문제가 스택에서 재발한다.** 게다가 스택 병합·분할이 인스턴스를 어떻게 처리하는지 정의되지 않으면 구현자마다 갈린다.

| 조건 | 인스턴스 | 핸들 |
|---|---|---|
| `MaxStack > 1` (탄약·붕대·잡템 — 개체 상태 없음) | **만들지 않는다** | `INDEX_NONE` |
| `MaxStack == 1` 이고 개체 상태 보유 (무기·방어구) | 만든다 | 유효 |

- 스택 아이템의 엔트리는 순수 `(ItemId, Quantity)`다. **병합은 정수 덧셈, 분할은 정수 뺄셈으로 끝난다** — 인스턴스를 지우거나 새로 만들 일이 없다
- 비스택 아이템은 병합 자체가 불가능하므로 분할 문제가 생기지 않는다. 잔탄·내구도가 온전히 보존되는 것도 이 경우뿐이고, 실제로 보존이 필요한 것도 이 경우뿐이다
- 판정은 `MaxStack`으로 충분하다. "스택 가능하면서 개체 상태도 갖는" 아이템이 실제로 생기면 그때 `UEPItemDefinition`에 `bHasInstanceState`를 추가한다
- **`UEPItemInstance::Quantity` 필드는 제거한다.** 수량의 진실은 `FEPInventoryEntry::Quantity`(인벤토리) 또는 `AEPPickup::Quantity`(월드) 하나뿐이다

#### 인스턴스 수명 — 해제 시점을 명시한다

"매치 종료 시 서브시스템 파괴로 일괄 정리"만으로는 부족하다. 30분 매치에서 사망·리스폰이 반복되면 그때까지 누적된다.

| 시점 | 처리 |
|---|---|
| `UEPInventoryComponent::EndPlay` | 자기 엔트리의 유효 핸들을 전부 `Destroy(Handle)` |
| `AEPPickup::EndPlay` (`ClearLoot`, 월드 밖 낙하 등) | 보유 핸들이 유효하면 `Destroy(Handle)` |
| 획득/버리기로 **이관 중** | **호출 금지** — 아래 순서를 지킨다 |
| 매치 종료 | 서브시스템 `Deinitialize()`가 최종 안전망 |

```
이관 프로토콜 (소유권 공백이 없도록 이 순서로)
  인벤토리 → 픽업 : 픽업에 핸들 대입 → 인벤토리 엔트리 제거
  픽업 → 인벤토리 : 인벤토리 삽입 성공 확인 → 픽업 핸들을 INDEX_NONE으로 → Destroy()
```

> 픽업 핸들을 비우기 전에 `Destroy()`하면 `AEPPickup::EndPlay`가 방금 인벤토리로 넘긴 인스턴스를 지운다. 획득 직후 무기 잔탄이 사라지는 형태로 나타난다.

> **★ 사망 시 드랍(§8 미정 #4)을 넣을 때의 함정:** 드랍은 **반드시 `UEPInventoryComponent::EndPlay`보다 먼저** 돌아야 한다. 순서가 뒤집히면 `EndPlay`가 핸들을 전부 `Destroy()`한 뒤 드랍이 빈 인벤토리를 뿌려, **"죽으면 아이템이 그냥 사라진다"** 로 나타난다. 원인이 인벤토리 로직이 아니라 수명 관리에 있어 추적이 오래 걸린다. 사망 처리(`AEPCharacter`의 사망 진입점)에서 드랍을 먼저 호출하고, `EndPlay` 정리는 "그때까지 남아 있으면 지운다"는 안전망으로만 남긴다.

#### 식별자: `FGuid`가 아니라 `int32` 핸들

`UEPItemInstance::InstanceId`(FGuid)는 **DB 영속용으로 남긴다.** 복제와 RPC에는 서버가 발급한 `int32` 핸들을 쓴다.

| | `FGuid` | `int32` 핸들 |
|---|---|---|
| 크기 | 16바이트 × 슬롯 수 | 4바이트 |
| 유일성 범위 | 전역·영구 | 매치 내 |
| 위조 방지 | 서버 검증 필요 | 서버 검증 필요 (동일) |

**배열 인덱스**를 식별자로 쓰면 안 된다 — 제거·정렬로 인덱스가 밀리는 사이에 클라 요청이 도착하면 엉뚱한 아이템을 버린다. 핸들은 그 경쟁을 원천 차단한다.

> 단, §4-6에서 도입하는 `FEPInventoryEntry::SlotIndex`는 **배열 인덱스가 아니라 화면 칸 번호**이고, 엔트리를 제거해도 다른 엔트리의 값이 재배치되지 않는다. 그래서 드랍 RPC의 파라미터로는 안전하며, 스택 아이템(핸들 없음)까지 하나의 경로로 다룰 수 있다 (§4-7).

### 4-2. 루트 테이블 — 가중치 + **중첩**

```
UEPLootTable : UPrimaryDataAsset
├─ TArray<FEPLootEntry> Entries
│    ├─ float Weight                    (상대 가중치)
│    ├─ FName ItemId                    ┐ 둘 중 하나만 채운다
│    ├─ TObjectPtr<UEPLootTable> SubTable ┘ (유효하면 재귀 롤. 하드 참조)
│    └─ int32 MinQuantity / MaxQuantity (ItemId일 때만)
└─ float EmptyWeight                    (아무것도 안 나올 가중치)
```

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
           ├─ ItemId: Ammo_762    Weight 1, Qty 20~60
           ├─ ItemId: Bandage     Weight 1, Qty 1~2
           └─ ItemId: Scrap_Paper Weight 1, Qty 1~3   ← 여기에 추가해도 "일반 50%"는 그대로
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
├─ FName ItemId                        (Replicated, OnRep로 메시 적용)
├─ int32 Quantity                      (Replicated)
├─ int32 InstanceHandle = INDEX_NONE   ← 서버 전용. 복제하지 않음. 포인터가 아니라 핸들 (§4-1)
└─ UStaticMeshComponent* Mesh
```

**결정: 인스턴스는 서버에만 존재하고, 클라이언트에는 `ItemId + Quantity`만 복제한다.** 인벤토리(§4-6)와 정확히 같은 원칙이다.

| 픽업의 출처 / 종류 | `InstanceHandle` | 획득 시 서버 동작 |
|---|---|---|
| 스포너가 뿌린 스택 아이템 (`MaxStack > 1`) | `INDEX_NONE` | 인스턴스 없이 수량만 인벤토리에 병합 (§4-1) |
| 스포너가 뿌린 비스택 아이템 | `INDEX_NONE` | 획득 시점에 `CreateInstance()` → 인벤토리 삽입 |
| 플레이어가 버린 비스택 아이템 (§4-7) | **유효** | 핸들을 **그대로** 인벤토리로 이관 (재생성 금지) |
| 플레이어가 버린 스택 아이템 | `INDEX_NONE` | 수량만 병합 |

> 스포너가 뿌리는 시점에는 비스택 아이템도 인스턴스를 만들지 않는다. 맵에 200~300개가 깔리는데 그중 대부분은 아무도 줍지 않는다 — 주울 때 만드는 편이 싸고, 버려진 픽업만 이관 대상이라 규칙도 단순하다.

- 인스턴스를 복제하지 않는 이유: `UObject` 서브오브젝트 복제를 픽업 액터마다 붙이면 맵에 깔린 수십~수백 개가 전부 복제 대상이 된다. **클라이언트가 바닥 아이템에 대해 알아야 할 건 "무엇이 몇 개인지"뿐이다** — 아이콘·이름은 `ItemId`로 조회되고, 잔탄 같은 인스턴스 상태는 줍기 전에는 볼 필요가 없다.
- 버려진 무기의 잔탄·내구도가 보존되는 것은 **인스턴스를 재생성하지 않고 핸들만 이관**하기 때문이다. 이 규칙을 어기면 버렸다 줍기만 해도 만탄이 되는 익스플로잇이 생긴다 (§4-8 참조 — 현재 코드가 정확히 그 상태다).
- 클라가 바닥 무기의 잔탄을 표시해야 하는 요구가 생기면, 액터를 서브오브젝트 복제로 바꾸지 말고 `FEPPickupPayload` 구조체를 하나 더 복제하는 쪽으로 확장한다.
- 메시: `OnRep_ItemId`에서 레지스트리 조회 → `Definition->WorldMesh` 비동기 로드 → 도착하면 세팅. 로드 전에는 공용 플레이스홀더 메시를 쓴다. **`WorldMesh`가 비어 있는 아이템이 대부분일 것이므로**(현재 무기 외 메시 없음) 플레이스홀더 박스는 선택이 아니라 필수다.

**동시 획득 경쟁 (★ 멀티 필수 처리)**

두 플레이어가 같은 픽업에 동시에 상호작용하는 상황은 반드시 발생한다. 서버에서 다음 순서로 처리한다.

```
Server_Interact(Target)
  1. Target이 유효한가 (IsValid && !IsActorBeingDestroyed)
  2. Target이 IEPInteractable을 구현하는가        ← 아무 액터나 보낼 수 있다
  3. ★ 거리 재검증: DistSq(Character, Target) <= (InteractRange + 여유)^2
       클라의 트레이스 결과를 신뢰하지 않는다 (§4-5)
  4. ★ IEPInteractable::CanInteract(Instigator) == true 인가
       DropCooldown, 컨테이너 "이미 검색됨", 자판기 돈 부족이 전부 여기서 걸린다
  5. bClaimed == false 인가        ← 이 프레임에 이미 다른 요청이 선점했는지
  6. bClaimed = true 로 즉시 마킹  ← 인벤토리 삽입보다 먼저
  7. Added = AddItem(ItemId, Quantity)      (§4-6, 실제 삽입된 개수)
  8. Added == Quantity  → 핸들 이관 후 Destroy()            (전량 획득)
     0 < Added < Quantity → Quantity -= Added
                            FlushNetDormancy()
                            bClaimed = false                (부분 획득 — 픽업은 남는다)
     Added == 0        → bClaimed = false
                         Client_OnInteractFailed(사유)      (인벤 가득 참 등)

  1~4 중 하나라도 실패 → Client_OnInteractFailed(사유). 조용히 return 하지 않는다
```

- **★ 3·4단계를 빠뜨리면 서버 검증이 사실상 없어진다.** §4-5가 "서버가 거리와 대상 유효성을 재검증한다", "`CanInteract()`는 서버가 다시 호출해 판정한다"고 선언해놓고 이 절차에 호출이 없으면, **클라이언트가 프롬프트를 안 그릴 뿐 RPC는 그대로 통과한다.** 특히 §4-7의 `DropCooldown`(버린 직후 0.5초 재획득 금지)이 `CanInteract()`로 구현되므로, 4단계가 없으면 쿨다운이 서버에서 강제되지 않는다. 이 절차는 구현 체크리스트로 읽히는 자리라 누락이 그대로 코드가 된다.
- **★ 부분 획득에서 `bClaimed`를 반드시 되돌린다.** "성공→파괴 / 실패→해제" 두 갈래로만 쓰면, 픽업이 살아남는 부분 획득 경로에서 `bClaimed`가 true로 굳어 **그 아이템을 아무도 다시 줍지 못한다.** 부분 획득은 §4-6에서 정식 지원하는 경로라 반드시 발생한다.
- `bClaimed`는 **복제하지 않는다.** 서버 내부 상태이고, 결과는 액터 파괴(또는 `Quantity` 갱신)로 클라에 전달된다.
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
| `NetDormancy` | **`DORM_Initial`** | ★ 아래 참조 |

> **★ 5.5부터 직접 대입은 deprecated다.** `NetCullDistanceSquared` 필드는 `UE_DEPRECATED(5.5, "Public access to NetCullDistanceSquared has been deprecated...")`가 붙어 있다(`Actor.h:869`). 생성자에서 `NetCullDistanceSquared = ...`로 쓰면 5.7에서 경고가 난다. **`SetNetCullDistanceSquared(25000000.f)`** 를 쓴다. 같은 이유로 `NetUpdateFrequency`도 `SetNetUpdateFrequency()`다(`Actor.h:874`). `bReplicates` / `bAlwaysRelevant` / `NetDormancy`는 deprecated가 아니라 생성자 대입 그대로 둔다.

> **단위 함정:** 기본값은 `225000000`(= 15000cm의 제곱)이다. 여기에 `5000`을 그대로 넣으면 컬링 거리가 √5000 ≈ **70cm**가 되어 픽업이 코앞에서만 보인다. "왜 아이템이 안 보이지"로 한참 헤매는 대표적인 실수다.

**★ Net Dormancy가 이 액터에 정확히 맞는 이유**

픽업은 스폰된 뒤 파괴될 때까지 상태가 **거의 변하지 않는다**(`ItemId`는 불변, `Quantity`는 부분 획득 때만). `DORM_Initial`로 두면 초기 복제 후 관련성·복제 검사에서 아예 빠진다. 맵에 200~300개가 깔리는 액터에서 이 차이는 크다.

```
스폰            → DORM_Initial (초기 1회 복제 후 휴면)
Quantity 변경   → FlushNetDormancy()  ← 부분 획득 시 반드시 호출
획득 완료       → Destroy()           ← 파괴는 휴면과 무관하게 전달된다
```

> `FlushNetDormancy()` 호출을 빠뜨리면 **클라이언트 화면의 개수만 옛날 값으로 남는다.** 서버는 정상이라 재현이 까다로운 종류의 버그다. 부분 획득 경로에 반드시 넣는다.

> 물리로 튕기는 드랍(사망 시 드랍 등)이 생기면 그때만 이동 복제를 켜고 `DORM_Awake`로 둔다. 정지 후 `SetReplicateMovement(false)` + 다시 휴면시키는 게 정석이다.

### 4-5. 상호작용 — 클라 탐지, 서버 검증

```
UEPInteractionComponent (Character에 부착)
├─ Tick: 로컬 컨트롤러만 카메라 전방 트레이스 → 대상 갱신 → HUD 프롬프트
│         SetComponentTickInterval(0.1f)  ← 매 프레임 트레이스 금지
└─ Input(E) → Server_Interact(AActor* Target)
```

- **트레이스 주기는 0.1초로 낮춘다.** 프롬프트 표시는 100ms 지연이 체감되지 않는데, 매 프레임 트레이스는 그대로 낭비다. 로컬 컨트롤러가 아니면 아예 틱을 끈다(`SetComponentTickEnabled(false)`).
- **클라이언트의 트레이스 결과를 신뢰하지 않는다.** 서버는 `Server_Interact`에서 거리(`InteractRange` + 여유)와 대상 유효성을 재검증한다. 사격 경로에서 이미 확립한 원칙(클라는 "요청", 서버가 "결정")과 동일하다.
- 대상 추상화는 **인터페이스**(`IEPInteractable`)로 둔다. 픽업·컨테이너·자판기·탈출지점이 전부 같은 진입점을 쓴다. 이 컴포넌트를 지금 제대로 만들어두면 로드맵 7·11·12가 전부 이걸 재사용한다.

**인터페이스는 처음부터 4개를 갖춘다.** 나중에 인터페이스를 넓히면 이미 구현한 모든 클래스를 건드려야 하므로, 확장 지점을 지금 확보한다.

| 함수 | 이번 단계 픽업의 구현 | 나중에 쓰는 곳 |
|---|---|---|
| `GetInteractText()` | "줍기 — 붕대 x2" | 컨테이너 "검색", 자판기 "1000원 투입" |
| `CanInteract(Instigator)` | 인벤토리 여유 확인 | 자판기 돈 부족, 컨테이너 이미 검색됨, 탈출 조건 미달 |
| `GetInteractDuration()` | 0 | 컨테이너 검색 N초, 자판기 5초, 탈출 대기 |
| `OnInteract(Instigator)` | 인벤토리 삽입 후 파괴 | 각 오브젝트의 실제 동작 |

- `CanInteract()`는 **HUD 프롬프트에도 쓴다.** false면 회색 + 사유 표시. 눌러보고 나서 실패하는 것보다 낫다.
- 클라이언트에서도 `CanInteract()`를 호출해 프롬프트를 그리되, **서버가 다시 호출해 판정한다.** 클라 결과는 표시용일 뿐이다.
- `GetInteractDuration() > 0`인 경우의 채널링은 **GAS 어빌리티로 구현**한다 (§7-1 참조). 상호작용 컴포넌트는 채널링을 직접 구현하지 않고 어빌리티를 활성화만 한다.

### 4-6. 인벤토리 복제 — FastArray + 서버 권한 인스턴스

```
UEPInventoryComponent (Character에 부착)
├─ FEPInventoryList Entries          : FFastArraySerializer   ← COND_OwnerOnly
│    └─ FEPInventoryEntry            : FFastArraySerializerItem
│         ├─ int32 SlotIndex         ★ 화면상 몇 번 칸인가 (배열 순서와 무관)
│         ├─ int32 Handle            (§4-1. 스택 아이템은 INDEX_NONE)
│         ├─ FName ItemId
│         └─ int32 Quantity
├─ int32 MaxSlots                    (Replicated, COND_OwnerOnly — UI가 빈 칸 수를 알아야 함)
└─ int32 EquippedHandle              (§4-8)
```

**★ `SlotIndex`가 반드시 필요한 이유 — FastArray는 순서를 보장하지 않는다**

`FFastArraySerializer`는 항목을 **ReplicationID로 식별**할 뿐, 클라이언트 배열의 **순서가 서버와 같다는 보장이 없다.** 추가·제거가 섞이면 클라마다 배열 순서가 달라진다.

Step 04는 "1x1 고정 슬롯"을 그리므로, 배열 인덱스를 그대로 칸 번호로 쓰면 **아이템을 하나 줍거나 버릴 때마다 기존 아이콘들이 다른 칸으로 튄다.** 산발적으로 나고 서버는 멀쩡해서 재현이 어려운 부류다.

- `AddItem()`이 **가장 낮은 빈 `SlotIndex`** 를 배정한다. 제거해도 다른 엔트리의 `SlotIndex`는 건드리지 않는다
- UI는 배열을 순회하지 않고 `SlotIndex`로 그린다 (칸 N개를 먼저 만들고 엔트리를 꽂는 방식)
- 이 필드가 나중에 그리드 인벤토리의 `GridX/GridY`로 자연스럽게 확장된다

**★ 복제 조건은 `COND_OwnerOnly`다.**

조건 없이 복제하면 **모든 클라이언트가 모든 플레이어의 인벤토리를 받는다.** 패킷만 봐도 상대 소지품을 아는 치트가 되고, 8인 매치면 대역폭도 8배가 된다. GAME.md의 "정보 은폐" 기조와도 정면으로 어긋난다.

```cpp
DOREPLIFETIME_CONDITION(UEPInventoryComponent, Entries, COND_OwnerOnly);
```

프로젝트 관례와도 일치한다 — `PlayerState::Kills` / `Extracted`가 이미 `COND_OwnerOnly`다.

> 나중에 "시체 루팅"이 생기면 남의 인벤토리를 봐야 하는데, 그때 조건을 푸는 게 아니라 **시체 액터가 자기 인벤토리를 별도로 노출**하는 방식으로 간다. 살아있는 플레이어의 가방은 끝까지 소유자 전용이다.

**결정: `UEPItemInstance`를 복제하지 않는다.** 인스턴스는 서버에만 존재하고, 클라이언트에는 위 POD 구조체만 델타 복제한다.

- **왜 FastArray인가:** 일반 `TArray` UPROPERTY는 원소 하나만 바뀌어도 배열 전체를 다시 보낸다. `FFastArraySerializer`는 변경된 항목만 보내고 클라이언트에 `PostReplicatedAdd` / `PostReplicatedChange` / `PreReplicatedRemove` 콜백을 준다 — UI 갱신을 폴링 없이 이벤트로 처리할 수 있다.
- **왜 UObject 서브오브젝트 복제를 안 쓰는가:** `bReplicateUsingRegisteredSubObjectList` + `AddReplicatedSubObject` 경로는 동작하지만, 인스턴스 수명(생성/파괴 타이밍)과 복제 등록을 손으로 맞춰야 하고 실수하면 클라에서 널 참조가 난다. **클라이언트 UI가 실제로 필요로 하는 건 아이콘·이름·개수뿐**이고 그건 `ItemId`로 전부 조회된다. 복잡도 대비 이득이 없다.
- **인스턴스는 왜 남기는가:** 서버 권한 상태(무기 잔탄, 내구도, 퀘스트 아이템 식별자)와 향후 DB 직렬화(`SchemaVersion` 필드가 이미 있음)의 자리다. `int32 Handle`이 복제 항목과 서버 인스턴스를 잇는 키다 — **단 비스택 아이템에 한한다.** 탄약·붕대는 개체 상태가 없어 인스턴스 자체를 만들지 않고 `Handle`이 `INDEX_NONE`이다 (§4-1).
- 확장 여지: 클라가 인스턴스별 상태를 알아야 하는 순간이 오면 `FEPInventoryEntry`에 필드를 추가한다. 구조 전체를 바꿀 필요가 없다.

**슬롯 형태: 1x1 고정 슬롯 (확정)**

아이템마다 크기를 갖고 격자에 테트리스처럼 채우는 방식은 **채용하지 않는다.**

근거: `FEPItemData`의 크기 관련 필드는 `MaxStack`과 `SlotSize` 둘뿐이고, `SlotSize`는 **1차원 스칼라**라 "2x2"와 "1x4"를 구분할 수 없다. 즉 현재 데이터 모델에 2D 아이템 크기가 존재하지 않는다. 없는 것을 지금 만들어 붙이는 대신, 고정 슬롯 N칸으로 시작한다.

- 인벤토리는 `MaxSlots`(예: 20)를 가지며, 한 슬롯 = 한 스택이다
- **`SlotSize`는 이번 단계에서 읽지 않는다.** 테트리스로 전환할 때를 위해 남겨둔다

**스택 병합 순서 (반드시 이 순서로)**

정의해두지 않으면 구현자마다 달라지고 부분 획득 반환값이 흔들린다.

```
AddItem(ItemId, Count, InHandle = INDEX_NONE) → 실제로 들어간 개수
  1. 같은 ItemId이면서 여유가 있는 기존 스택을 SlotIndex 오름차순으로 채운다
  2. 남은 수량을 빈 SlotIndex(가장 낮은 것부터)에 MaxStack 단위로 새 엔트리로 만든다
  3. 빈 슬롯이 떨어지면 거기서 멈추고, 그때까지 들어간 개수를 반환한다

  비스택 아이템(MaxStack == 1)의 핸들 처리:
    InHandle 유효   → 그대로 엔트리에 대입 (버린 것을 다시 주움 — 잔탄 보존)
    InHandle 없음   → 빈 슬롯 확보에 성공한 뒤에만 CreateInstance()
                      ★ 순서를 뒤집으면 삽입 실패 시 고아 인스턴스가 남는다
```

예: 탄약 60발 획득, `MaxStack` 30, 현재 [20발 스택 1개, 빈 슬롯 1개]
→ 기존 스택을 30으로 채우고(10 소모) → 빈 슬롯에 30 → **40 삽입, 20 반환 잔여**
→ 픽업의 `Quantity`를 20으로 낮추고 `FlushNetDormancy()` (§4-4)

**기존 스택을 먼저 채우는 이유:** 반대로 하면 빈 슬롯이 먼저 소모되어, 합칠 수 있었던 상황에서 "가방이 가득 찼다"가 나온다. 플레이어가 납득하지 못한다.

> 테트리스로 가는 이행 경로: `FEPItemData`에 `SlotWidth`/`SlotHeight`를 추가하고 `SlotSize`를 파생값으로 두거나 폐기한다. `FEPInventoryEntry`에 `int32 GridX, GridY, Rotation`을 추가하고, 배치/충돌 판정 로직만 교체하면 된다. **복제 구조(FastArray + POD)는 그대로 유효하다** — 그래서 지금 이 선택이 나중을 막지 않는다.

**부분 획득 정책: 부분 획득을 허용한다.**

탄약 60발을 주웠는데 인벤토리에 40발만 들어가는 경우, 40발을 넣고 픽업의 `Quantity`를 20으로 줄여 월드에 남긴다. 전량 거부는 플레이어 입장에서 납득이 안 된다.

- `AddItem()`은 `bool`이 아니라 **실제로 들어간 개수(`int32`)** 를 반환한다. 0이면 완전 실패
- 0이 반환되면 §4-4의 `bClaimed`를 되돌리고 `Client_OnInteractFailed`로 사유를 회신한다

**부착 위치: Character (확정)**

`UEPInventoryComponent`는 Character에 붙인다.

- 타르코프식은 **사망 시 소지품 손실**이 규칙이다(GAME.md 코어 루프). Character와 수명을 같이하는 것이 규칙과 일치한다 — PlayerState에 두면 사망 시 명시적으로 비워야 하고, "비우는 걸 깜빡하는" 버그의 자리가 생긴다
- ASC를 PlayerState에 둔 것과 대비되지만 이유가 다르다. ASC는 리스폰 후에도 **보존되어야** 해서 PlayerState였다. 인벤토리는 반대로 **소실되어야** 한다
- 탈출 성공 시의 정산(가방 내용물 → 계정 잔고)은 탈출 처리 시점에 Character의 인벤토리를 읽어 PlayerState로 옮기면 된다. 탈출은 매치당 1회 이벤트라 이 방향이 비용이 낮다

### 4-7. 아이템 버리기 — 픽업의 역방향

```
Client: G키(장착 무기) 또는 인벤토리 UI에서 선택
    → Server_DropItem(int32 SlotIndex, int32 Quantity)
Server:
    1. 그 SlotIndex에 엔트리가 실제로 있는가 (클라 요청은 신뢰하지 않는다)
    2. 장착 중이면 먼저 해제 (§4-8 — 어트리뷰트 → 인스턴스 write-back 포함)
    3. 캐릭터 전방에 AEPPickup 스폰 + 바닥 트레이스로 접지
    4. 픽업에 핸들 대입 → 그 다음 인벤토리 엔트리 제거   ← §4-1 이관 프로토콜 순서
       (스택 아이템이면 핸들 없이 수량만 분할)
```

- **RPC 파라미터는 `FGuid`가 아니라 `int32`다** (§4-1). 여기서는 `SlotIndex`를 쓴다 — UI가 클릭한 칸을 그대로 보내면 되고, 서버는 그 칸의 엔트리를 확인만 하면 된다. 슬롯 인덱스가 요청 중에 바뀌는 경쟁이 문제였지만, `SlotIndex`는 §4-6에서 **제거해도 재배치되지 않는 고정 번호**가 되었으므로 그 문제가 없다.
- 핸들을 보내도 되지만, 스택 아이템은 핸들이 `INDEX_NONE`이라 식별자가 되지 못한다. 두 경로를 하나로 유지하려면 `SlotIndex`가 맞다.

**핵심 규칙: 비스택 아이템은 인스턴스를 파괴하지 않고 핸들만 픽업으로 넘긴다.** `CreateInstance`를 다시 부르면 잔탄·내구도·`InstanceId`가 전부 초기화된다. **인스턴스의 `Outer`는 끝까지 `UEPItemInstanceSubsystem`이며 바뀌지 않는다** — 바뀌는 것은 "누가 그 핸들을 들고 있는가"뿐이다 (§4-1).

> 스택 아이템(탄약 등)은 애초에 인스턴스가 없으므로 버리기가 정수 뺄셈 + 픽업 스폰으로 끝난다. "버렸다 주우면 같은 개체인가"라는 질문 자체가 성립하지 않는다.

| 항목 | 처리 |
|---|---|
| 스폰 위치 | 캐릭터 전방 약 100cm, 아래로 라인 트레이스해 접지. 트레이스 실패 시 발밑 |
| 벽 끼임 | 스폰 위치가 막혀 있으면 발밑으로 폴백. `AlwaysSpawn`으로 두되 위치를 보정한다 |
| 즉시 재획득 | 버린 직후 `DropCooldown`(약 0.5초) 동안 그 픽업은 `CanInteract()`가 false. 없으면 G를 누른 순간 E 프롬프트가 바로 떠서 실수로 다시 줍는다 |
| 부분 버리기 | 스택 분할. 남은 수량은 인벤토리에 유지 |
| 클라 예측 | **하지 않는다.** 버리기는 결과가 늦게 보여도 무해하고, 예측하면 롤백 처리가 필요해진다 |

> 사망 시 전체 드랍은 이 경로를 N회 호출하면 되지만, **이번 단계 범위 밖이다.** 인벤토리를 통째로 뿌리면 액터가 한 번에 20개 생기므로 별도 설계(가방 컨테이너 하나로 드랍 등)가 필요하다.

### 4-8. 무기 장착 흐름 이관 (★ 탄약 소유권 충돌)

**현재 흐름:** `EPGameMode::HandleStartingNewPlayer` → `DefaultWeaponClass` 액터 직접 스폰 → `CombatComponent::EquipWeapon()`. 아이템 계층을 전혀 거치지 않는다.

**목표 흐름:**

```
인벤토리 UI / 숫자키
    → Inventory: EquippedHandle 갱신
    → CombatComponent::EquipFromInventory(int32 Handle)      ← 무기 액터의 주인은 그대로 CombatComponent
         → WeaponDef로 AEPWeapon 액터 스폰 + 소켓 부착 + LinkAnimClassLayers
         → Instance.CurrentAmmo → GAS Ammo 어트리뷰트로 주입
```

- **인벤토리 컴포넌트가 `AEPWeapon`을 스폰하지 않는다.** 무기 액터의 수명·부착·애님 레이어·어빌리티 부여는 이미 `UEPCombatComponent`가 전부 쥐고 있다(`EPCombatComponent.cpp:161` `EquipWeapon(AEPWeapon*)`). 인벤토리는 **핸들만 넘긴다.**
- 기존 `EquipWeapon(AEPWeapon*)`은 남겨두고 `EquipFromInventory(int32 Handle)`를 추가해 그쪽으로 위임한다. 기존 호출부(테스트 지급 등)를 한 번에 갈아엎지 않아도 된다.

#### 문제: 탄약의 진실이 두 곳에 있다

| 위치 | 성격 |
|---|---|
| `UEPAttributeSet::Ammo` / `MaxAmmo` | 캐릭터(ASC) 소유. **1인당 하나뿐.** 복제되고 HUD가 구독. GAS 발사·재장전이 읽고 쓰는 값 |
| `UEPWeaponInstance::CurrentAmmo` | 아이템 인스턴스 소유. 무기마다 다름. 현재 데드코드 |

인벤토리에 무기 2정을 넣고 교체하려면 **정별로** 잔탄이 보존돼야 하는데, GAS 어트리뷰트는 캐릭터에 하나뿐이다.

#### 결정: 어트리뷰트는 "현재 장착 무기의 뷰", 인스턴스가 진실

```
Equip   : Instance.CurrentAmmo  →  SetAmmo()        (주입)
Unequip : GetAmmo()             →  Instance.CurrentAmmo   (write-back)
```

- 발사·재장전은 지금처럼 GAS 어트리뷰트만 건드린다. **`GA_Item_PrimaryUse` / `GA_Item_Reload`는 수정하지 않는다**
- 해제 시점의 write-back을 빠뜨리면 무기를 바꿨다 돌아올 때 잔탄이 되돌아간다. **`UnequipWeapon()`에 반드시 넣는다**
- 사망 시에도 write-back이 필요하다 — 시체/드랍의 잔탄이 맞아야 한다

#### 즉시 고쳐야 할 것: `EquipWeapon`의 만탄 리셋

`EPCombatComponent.cpp:177-178`이 장착할 때마다 이렇게 한다.

```cpp
AS->InitAmmo(static_cast<float>(NewWeapon->WeaponDef->MaxAmmo));      // ← 만탄 리셋
AS->InitMaxAmmo(static_cast<float>(NewWeapon->WeaponDef->MaxAmmo));
```

버리기(§4-7)가 들어오는 순간 이건 **익스플로잇이 된다** — 12/30 무기를 버렸다 줍기만 하면 30/30이 된다. 다음으로 바꾼다.

```
MaxAmmo ← WeaponDef->MaxAmmo      (무기 스펙, 유지)
Ammo    ← Instance->CurrentAmmo   (인스턴스 상태)
```

> `Init*` 계열은 어트리뷰트 변경 델리게이트를 발생시키지 않는다. HUD가 장착 즉시 갱신되지 않으면 `SetAmmo()`를 쓰거나 `RefreshAmmo()`를 명시 호출해야 한다. 현재 코드가 `Init*`을 쓰고 있으니 이관 시 확인할 것.

> **타입 정리:** `UEPWeaponDefinition::MaxAmmo`가 `uint8`(상한 255), `UEPWeaponInstance::CurrentAmmo`가 `int32`, GAS 어트리뷰트가 `float`이라 주입/write-back 경로마다 캐스팅이 낀다. `MaxAmmo`를 `int32`로 바꿔 최소한 정수 쪽은 통일한다. 데이터 에셋 값은 그대로 유지된다(폭 확대라 손실 없음).

#### 장착 슬롯 표현

별도 장비 슬롯 배열을 만들지 않는다. 인벤토리 안의 인스턴스를 가리키기만 한다.

```
UEPInventoryComponent
└─ int32 EquippedHandle   (Replicated, COND_OwnerOnly)
```

- **`COND_OwnerOnly`로 충분하다.** 다른 클라이언트는 `AEPWeapon` 액터가 복제되고(`EPWeapon.cpp:19` `bReplicates = true`) 캐릭터 소켓에 부착되므로 **이미 보인다.** 이 값은 순전히 소유자 UI("어느 슬롯이 장착 중인가")를 위한 것이다
- 무기는 인벤토리 슬롯을 차지한 채로 장착된다. 1x1 고정 슬롯 단계에서 별도 장비 슬롯은 과하다
- 확장 시 `TMap<EEPEquipSlot, int32>`로 바꾼다 (헬멧·가방·보조무기)
- 장착된 인스턴스는 버리기 전에 자동 해제한다 (§4-7 2단계)

#### GameMode 기본 지급의 이관

`HandleStartingNewPlayer`에서 `AEPWeapon`을 직접 스폰하던 것을 다음으로 바꾼다.

```
DefaultWeaponClass (액터 클래스)  →  DefaultLoadout : TArray<FName ItemId>
    → 인벤토리에 인스턴스 생성해 삽입
    → 무기 타입 첫 항목을 자동 장착
```

`AEPWeapon` 액터 자체와 `UEPCombatComponent`의 발사 로직은 **건드리지 않는다.** 바뀌는 것은 "누가 언제 무기 액터를 만드는가"뿐이다.

### 4-9. 확장 지점 — 아이템 종류를 늘려도 인벤토리를 안 고치게

#### ★ 팩토리를 Definition의 **virtual 함수**로 옮긴다

현재 팩토리는 static이고 시그니처가 서로 다르다.

```cpp
static UEPItemInstance*   UEPItemInstance::CreateInstance(UObject*, FName, UEPItemDefinition*);
static UEPWeaponInstance* UEPWeaponInstance::CreateWeaponInstance(UObject*, FName, int32 InMaxAmmo, ...);
```

이 상태로는 인벤토리가 무기를 만들 때 `EEPItemType`으로 분기하고 `Cast<UEPWeaponDefinition>`을 해야 한다. **방어구·소모품·퀘스트 아이템을 추가할 때마다 그 분기가 자란다.** 확장이 여기서 막힌다.

```
UEPItemDefinition
└─ virtual UEPItemInstance* CreateInstance(UObject* Outer) const;    ← 기본: UEPItemInstance

UEPWeaponDefinition : UEPItemDefinition
└─ virtual UEPItemInstance* CreateInstance(UObject* Outer) const override;
      → UEPWeaponInstance 생성 + CurrentAmmo = MaxAmmo 초기화
```

- 호출부는 `Definition->CreateInstance(Outer)` 한 줄이다. **인벤토리·픽업·자판기가 아이템 타입을 전혀 모른다**
- 새 아이템 종류 = Definition 서브클래스 하나 추가. 기존 코드 수정 0
- 기존 static 팩토리 2개(`UEPItemInstance::CreateInstance`, `UEPWeaponInstance::CreateWeaponInstance`)는 **제거한다.** 어차피 호출처가 0이라 지금이 바꿀 적기다
- 실제 `NewObject`는 `UEPItemInstanceSubsystem`이 호출하고 Definition의 virtual에 위임한다 (Outer 소유권은 §4-1 규칙 유지)

##### Step 00에서 같이 지울 것

| 심볼 | 이유 |
|---|---|
| `UEPItemInstance::Quantity` (`EPItemInstance.h:27`) | 수량의 진실은 `FEPInventoryEntry::Quantity` / `AEPPickup::Quantity` 하나뿐 (§4-1) |
| `UEPItemInstance::IsSupportedForNetworking()` (`EPItemInstance.h:41`) | 인스턴스를 복제하지 않기로 확정했다(§4-6). `return true`가 남아 있으면 "복제되는 줄 알았다"는 오해의 씨앗이 된다 |

#### 데이터 정합성 — 양방향 참조를 검증한다

`FEPItemData::ItemDefinition`(소프트 참조)과 `UEPItemDefinition::ItemDataRow`(`FDataTableRowHandle`)가 **서로를 가리키는 양방향 참조**라 손으로 동기화해야 한다. 아이템이 수십 개를 넘어가면 반드시 어긋나고, 어긋나도 컴파일·로드는 통과한다.

- **DataTable → Definition을 정본으로 정한다.** 조회는 항상 `ItemId → Row → ItemDefinition` 방향으로만 한다
- `UEPItemDefinition::IsDataValid()`를 오버라이드해 에디터에서 검증한다
  - `ItemId`가 비어 있지 않은가
  - `ItemDataRow`가 가리키는 Row Name == 자기 `ItemId`인가
  - 그 Row의 `ItemDefinition`이 자기 자신을 가리키는가
- `UEPItemDefinitionSubsystem::Initialize()`에서도 캐시를 만들며 같은 검사를 돌리고, 불일치는 경고 로그를 남긴다 (패키지 빌드에서 조용히 null이 되는 것보다 낫다)

#### 소모품이 들어갈 자리를 지금 판다

루트 테이블에 붕대·회복키트가 들어가는데 **사용할 방법이 없다.** 이번 단계에서 구현하지는 않되, 자리는 지금 잡는다.

```
UEPItemDefinition
└─ TSubclassOf<UGameplayAbility> GrantedAbility;   ← 사용 시 발동할 어빌리티
```

- 장착/선택 시 `GiveAbility`, 해제 시 `ClearAbility`. 사용 입력은 그 어빌리티를 활성화만 한다
- **힐 스킬이 이미 GAS로 있다.** `UEPGA_Skill_Base`의 `CastTime` + `State.Casting` 구조를 그대로 상속하면 "붕대 3초 시전, 피격 시 취소, 중앙 게이지 표시"가 코드 추가 없이 된다
- 필드 하나를 지금 넣어두는 비용은 0에 가깝고, 나중에 `UEPItemDefinition`을 다시 열지 않아도 된다

---

## 5. 단계 계획

| Step | 문서 | 내용 | 완료 조건 |
|---|---|---|---|
| 00 | `05_Loot_00_ItemCore.md` | **기존 아이템 계층 정비** — Definition의 virtual 팩토리, static 팩토리 2종 제거, `Quantity`/`IsSupportedForNetworking` 제거, `GrantedAbility` 필드, `MaxAmmo` → `int32`, 서브시스템 2종, AssetManager에 `EPItemDefinition` 등록 | 매치 시작 시 Definition이 전량 상주하고, `DT_Items`에서 조회한 Definition으로 인스턴스가 생성되며, 무기면 `UEPWeaponInstance`가 나온다 (콘솔 커맨드로 확인). `IsDataValid()`가 DT↔Definition 불일치를 잡아낸다 |
| 01 | `05_Loot_01_Spawner.md` | `UEPLootTable`(중첩), `AEPItemSpawner`, `AEPPickup`(Dormancy), `EP.Loot.RollTable` | 맵에 스포너를 놓고 PIE 2인 → 서버·클라 양쪽에서 같은 아이템이 같은 위치에 보인다. `RollTable` 1000회로 등급 비율이 기획표와 일치 |
| 02 | `05_Loot_02_Interaction.md` | `IEPInteractable`(4함수), `UEPInteractionComponent`(틱 0.1s), **서버 거리 재검증 + `CanInteract()` 재호출**, `bClaimed` 경쟁 처리, HUD 프롬프트 | E키를 누르면 서버가 거리·`CanInteract()`를 재검증하고 픽업이 파괴된다. 사거리 밖 요청 거부. 2인이 동시에 눌러도 한 명만 성공 |
| 03 | `05_Loot_03_Inventory.md` | `UEPInventoryComponent`(Character 부착), `FEPInventoryList`(FastArray + `SlotIndex`), 스택 병합, 부분 획득 + `bClaimed` 되돌리기, 인스턴스 수명, **버리기(G) + `DropCooldown`** | 주운 아이템이 인벤토리에 쌓이고 델타 복제된다. 탄약을 인벤 여유보다 많이 주우면 **픽업이 줄어든 수량으로 남고 다시 주울 수 있다.** 무기를 버렸다 다시 주우면 **같은 `Handle`** 이 돌아온다 |
| 04 | `05_Loot_04_InventoryUI.md` | 1x1 고정 슬롯 위젯(`SlotIndex` 기준 배치), FastArray 콜백 기반 갱신, 슬롯에서 버리기 | 인벤토리 화면(Tab)에 아이콘·개수가 표시되고, 획득 즉시 갱신된다 (폴링 없음). **아이템을 줍고 버려도 기존 아이콘의 칸 위치가 바뀌지 않는다** |
| 05 | `05_Loot_05_Equipment.md` | `EquippedHandle` 복제, `CombatComponent::EquipFromInventory(Handle)`, 탄약 주입/write-back, `EquipWeapon` 만탄 리셋 제거, `DefaultLoadout` 이관 | 무기를 12/30까지 쏘고 버렸다 다시 주워 장착하면 **12/30 그대로**. 다른 클라에서도 장착 무기가 보인다 |

각 Step 완료 시 `LOOT_STATUS.md`와 해당 `05_Loot_0X_XXX_STATUS.md`를 코드 기준으로 갱신한다.

> **★ Step 02와 03의 경계:** Step 02 시점에는 인벤토리가 없다. `CanInteract()`의 "인벤토리 여유 확인"과 `OnInteract()`의 "인벤토리 삽입"은 **Step 03에서 채운다.**
> - Step 02의 범위 = 인터페이스 4함수 + 트레이스/프롬프트 + **서버 거리·대상 재검증** + `bClaimed` 경쟁 처리
> - Step 02의 `OnInteract()`는 `Destroy()`만 하고 로그를 남긴다. §3이 내세운 "임시 코드가 안 생긴다"를 지키려면, 이 한 줄이 **Step 03에서 `AddItem()` 호출로 대체되는 유일한 지점**이어야 한다
> - 여유 확인 없이 파괴하므로 Step 02 단독으로는 부분 획득을 검증할 수 없다. 그건 Step 03 완료 조건이다

> **UI 범위 통제:** Step 04는 표시 전용이다. 드래그앤드롭·정렬·아이템 이동은 넣지 않는다. 조작이 필요하면 숫자키/컨텍스트 메뉴로 처리한다.

---

## 6. 기존 코드와의 접점

| 기존 자산 | 이번 단계에서의 역할 |
|---|---|
| `FEPItemData` / `DT_Items` | `UEPItemDefinitionSubsystem`이 처음으로 읽는다. `MaxStack`은 스택 병합에, `SlotSize`는 추후 그리드 인벤토리에 쓰인다 |
| `UEPItemDefinition` / `WorldMesh` / `Icon` | 픽업 메시와 인벤토리 아이콘의 출처. **Step 00에서 `virtual CreateInstance()`와 `GrantedAbility`가 추가된다** |
| `UEPItemInstance::CreateInstance()` (static) | **Step 00에서 제거.** Definition의 virtual 팩토리로 대체 (§4-9) |
| `UEPWeaponInstance::CreateWeaponInstance()` (static) | **Step 00에서 제거.** 동상 |
| `UEPItemInstance::InstanceId` (FGuid) | 유지하되 **DB 영속용**. 복제·RPC에는 `int32` 핸들을 쓴다 (§4-1) |
| `UEPItemInstance::Quantity` | **Step 00에서 제거.** 수량의 진실은 엔트리/픽업 하나뿐 (§4-1) |
| `UEPItemInstance::IsSupportedForNetworking()` | **Step 00에서 제거.** 인스턴스는 복제하지 않는다 (§4-6) |
| `UEPWeaponDefinition::MaxAmmo` (`uint8`) | **Step 00에서 `int32`로.** 인스턴스·어트리뷰트와의 3중 캐스팅 정리 (§4-8) |
| `EEPItemType` / `EEPItemRarity` | 루트 테이블 필터링과 UI 색상에 사용 |
| `UEPWeaponInstance::CurrentAmmo` / `Durability` | Step 05에서 처음으로 실제 값이 된다. 장착 시 GAS `Ammo`로 주입, 해제 시 write-back (§4-8) |
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
- 검색 채널링은 **GAS 어빌리티로 구현**한다. `UEPGA_Skill_Base`의 `CastTime` + `State.Casting` 구조가 이미 있으므로, 검색 중 스킬 잠금·피격 중단·진행도 게이지(`WBP_CastGauge`)가 **전부 공짜로 재사용된다.** 이게 GAS를 먼저 한 배당금이다

### 7-2. 자판기

GAME.md §자판기 기획 유지 (1000원 → 5초 진동 + 소리 전파 → 배출). 컨테이너와 같은 상호작용·루트 테이블 인프라를 쓰되, **돈 차감**과 **배출 연출**이 추가된다.

배출물 크기 문제(라이플이 일반 자판기 배출구에 안 맞음)는 **상자 배출**로 해결하기로 결정. 자판기는 등급별 상자를 뱉고, 플레이어가 상자를 열면 내용물이 공개된다. 장점:
- 자판기 메시 크기와 배출 아이템 크기가 완전히 분리된다
- 아이템별 월드 메시가 없어도 된다 (현재 무기 외 메시 없음)
- "열기 전까지 모른다"는 뽑기 긴장감이 오히려 강화된다
- 상자 색을 등급별로 두면 원거리 정보 공개 수준을 튜닝할 수 있다 (교전 유도 노브)

상자는 §7-1 컨테이너의 특수 케이스로 구현한다 — 검색 시간 0, 1회용, 파괴됨.

---

## 8. 열린 결정사항

### 확정된 것

| 항목 | 결정 | 근거 |
|---|---|---|
| 인벤토리 부착 위치 | **Character** | 사망 시 소실이 규칙과 일치 (§4-6) |
| 슬롯 형태 | **1x1 고정 슬롯** | 데이터 모델에 2D 크기가 없음 (§4-6) |
| 픽업의 인스턴스 **복제** | **하지 않음** — 클라에는 `ItemId` + `Quantity`만 | 복제 비용 (§4-4) |
| 픽업의 인스턴스 **참조** | 서버 전용 `int32 InstanceHandle` 보유 (버려진 비스택 아이템일 때만 유효) | 버린 무기의 잔탄·내구도 보존이 여기 걸려 있다 (§4-4, §4-7) |
| 인벤토리 복제 | **FastArray + POD**, 인스턴스는 서버 전용 | 서브오브젝트 수명 관리 회피 (§4-6) |
| 루트 테이블 | **가중치 + 중첩** | GAME.md 등급 확률 보존 (§4-2) |
| 부분 획득 | **허용** | `AddItem()`이 삽입된 개수를 반환 (§4-6) |
| 아이템 버리기 | **포함** (Step 03) | 인스턴스를 재생성하지 않고 픽업으로 이관 (§4-7) |
| 무기 장착 흐름 이관 | **포함** (Step 05) | 탄약은 인스턴스가 진실, 어트리뷰트는 뷰 (§4-8) |
| 장비 슬롯 | **별도 배열 없음** — `EquippedHandle` 하나 | 1x1 단계에서 별도 슬롯은 과함 (§4-8) |
| 인벤토리 복제 조건 | **`COND_OwnerOnly`** | 남의 가방이 보이면 치트 + 대역폭 (§4-6) |
| 식별자 | **`int32` 핸들** (FGuid는 DB 영속용) | 4바이트, 슬롯 인덱스의 경쟁 문제 회피 (§4-1) |
| 인스턴스 소유 | **`UEPItemInstanceSubsystem`** (World, 서버 전용) | Outer `Rename()` 회피, O(1) 조회, 매치 종료 시 일괄 정리 (§4-1) |
| 인스턴스 생성 | **Definition의 virtual 팩토리** | static 팩토리로는 타입 분기가 계속 자란다 (§4-9) |
| 픽업 복제 | **`DORM_Initial`** + Tick off | 수백 개 액터의 관련성 검사 비용 (§4-4) |
| 인스턴스 생성 대상 | **`MaxStack == 1`인 아이템만.** 스택 아이템은 `Handle = INDEX_NONE` | 스택 병합·분할이 정수 연산으로 끝난다. "수량의 진실이 두 곳" 회피 (§4-1) |
| 슬롯 번호 | **`FEPInventoryEntry::SlotIndex`** 명시 필드 | FastArray는 클라이언트 배열 순서를 보장하지 않는다 (§4-6) |
| Definition 로딩 | **매치 전 전량 상주** (AssetManager). 소프트는 `WorldMesh`/`Icon`/`WeaponMesh`만 | 획득은 동기 경로라 로드를 기다릴 수 없다 (§4-1) |
| 무기 액터 스폰 책임 | **`UEPCombatComponent`** 유지. 인벤토리는 핸들만 넘긴다 | 무기 수명·부착·애님·어빌리티를 이미 쥐고 있다 (§4-8) |
| 드랍 RPC 파라미터 | **`SlotIndex`** (핸들 아님) | 스택 아이템은 핸들이 없어 식별자가 못 된다. `SlotIndex`는 고정 번호라 경쟁 없음 (§4-7) |

### 미정

| # | 항목 | 선택지 | 비고 |
|---|---|---|---|
| 1 | 돈 아이템화 | GAME.md는 "돈 = 인벤토리 아이템" | 자판기 단계에서 결정해도 늦지 않음 |
| 2 | 스폰 시드 | 매치마다 랜덤 / 시드 고정 | 재현 가능한 테스트가 필요하면 `FRandomStream` + 서버 시드 복제 |
| 3 | 인벤토리 슬롯 수 | `MaxSlots` 기본값 | 밸런싱 사항. 20 전후에서 시작 |
| 4 | 사망 시 드랍 | 인벤토리 통째 / 가방 컨테이너 1개 | 액터 20개가 한 번에 생기는 문제. **이번 단계 범위 밖** (§4-7) |
| 5 | 무기 2정 이상 | 주무기/보조 슬롯 | `TMap<EEPEquipSlot, int32>`로 확장 가능하게만 설계 (§4-8) |
| 6 | **재장전의 탄약 소비** | 무한 재장전 유지 / 인벤토리 탄약 차감 | 루트 테이블에 `Ammo_762`가 들어가는데 `GA_Item_Reload`는 인벤토리를 보지 않는다 — §4-9의 소모품과 같은 "있는데 쓸 수 없는" 상태. **이번 단계 범위 밖**이지만, Step 05의 `Instance.CurrentAmmo` 주입 구조가 들어오면 차감 지점이 자연스럽게 생긴다 |
| 7 | DB 영속화 대상 | 어떤 필드를 저장할 것인가 | `InstanceId`(FGuid) + `SchemaVersion` 자리는 이미 있으나 직렬화 스키마가 없다. 탈출 정산·계정 창고가 생길 때 결정 |

---

## 9. 개발·디버그 도구

확률 시스템은 **눈으로 검증할 수 없다.** 처음부터 도구를 같이 만든다. 기존 `UEPCombatDeveloperSettings` 패턴을 그대로 따른다.

```
UEPLootDeveloperSettings : UDeveloperSettings
├─ bool                    bEnableLootDebugLog      스폰 판정 결과를 로그로
├─ bool                    bEnableSpawnerDebugDraw  에디터/PIE에서 스포너 위치·반경 시각화
└─ TSoftObjectPtr<UDataTable> ItemDataTable         DT_Items (하드코딩 금지)
```

> **`FName` 경로가 아니라 `TSoftObjectPtr<UDataTable>`을 쓴다.** 문자열 경로는 에셋을 옮기거나 이름을 바꿔도 컴파일·저장이 통과하고, 런타임에 조용히 null이 된다. 소프트 포인터는 에디터가 참조를 추적해 리다이렉터를 따라간다. 기존 `UEPCombatDeveloperSettings` 관례와도 맞춘다.

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
