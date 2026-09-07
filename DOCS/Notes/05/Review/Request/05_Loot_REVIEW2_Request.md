# 검증 요청 2차 — 스택 폐지 + 칸 수 합산 + 부착물 계획

> 작성일: 2026-07-26
> 1차 검증: `05_Loot_REVIEW_StructMigration.md` (struct 전환 — **승인, 반영 완료**)
> 대상: 1차 검증 **이후** 들어온 두 결정과, 그것이 1차 결론에 미친 영향
> 시점: Step 00 착수 직전. **구현 코드는 여전히 0줄**

---

## 0. 무엇을 봐달라는 요청인가

1차에서 승인된 struct 전환을 문서에 반영하던 중, **설계를 다시 흔드는 결정 두 개**가 들어왔다.

| # | 결정 | 출처 |
|---|---|---|
| A | **아이템은 스택되지 않는다.** 인벤토리 용량은 아이템별 칸 수의 **합산** | 사용자 (타르코프식) |
| B | **무기 부착물을 추후 도입한다** (배그식, 깊이 1) | 사용자 |

A는 1차 검증의 미해결 항목 2개를 없앴고, B는 **1차 검증의 핵심 논거 하나를 무효화했다.**

두 결정을 8개 문서에 반영한 상태다. **반영 결과가 맞는지, 그리고 내가 못 푼 문제(§4)에 대한 판단**을 요청한다.

**반대 논거를 환영한다.** 아래는 전부 내가 결론을 내린 뒤 검증을 요청하는 형태라, 명시하지 않으면 확증 쪽으로 흐르기 쉽다.

---

## 1. 결정 A — 스택 폐지 + 칸 수 합산

### 기획

- 붕대 3개는 겹치지 않고 **엔트리 3개**다
- 아이템마다 **차지하는 칸 수**(`FEPItemData::SlotSize` — 이미 있던 미사용 필드)가 있고, 인벤토리는 **수용 가능한 총 칸 수**(`MaxSlots`)를 가진다
- **무게 시스템과 동형인 스칼라 합산.** 2D 격자 배치가 아니고, 1차원 연속 배치(단편화 처리)도 아니다
- 탄약은 **탄약상자**라는 아이템이 발수를 들고 있고, 그것으로 탄창을 채운다

```
UsedSlots      = Σ FindData(E.ItemId)->SlotSize      ← 파생값. 복제하지 않는다
CanFit(ItemId) = UsedSlots + SlotSize <= MaxSlots
```

### 사라진 것

`Quantity`(엔트리·픽업·루트테이블) / 스택 병합 순서 규칙 / 부분 획득 / `AddItem`의 개수 반환 /
`FlushNetDormancy()` 경로와 그 누락 함정 / 부분 획득에서의 `bClaimed` 되돌리기 / `MaxStack` 분기

> `FEPItemData::MaxStack`은 **읽지 않되 예약 필드로 남겼다** — 스택을 되살릴 여지. 되살릴 때는 엔트리에 `Quantity`를 추가하고 병합 로직을 넣으면 되고, 복제 구조는 그대로다.

### 1차 검증의 미해결 항목이 어떻게 닫혔는가

| 1차 항목 | 결과 |
|---|---|
| **F-2 / E-1** "스택 아이템도 Definition을 가지는가" | **질문이 증발.** 스택이 없어 "상태 없는 아이템" 범주가 사라졌고, 애초에 바닥 픽업의 `WorldMesh`와 UI의 `Icon`이 Definition에만 있어 예외가 성립하지 않았다 → **모든 아이템이 Definition을 가진다.** `Definitions=4` → **`7`** |
| **F-1 / B-3** 픽업 잔탄 노출 | **채택.** 단 2-struct가 아니다 — `Quantity`가 빠져 `FEPItemStack`이 `FName` 하나를 감싼 껍데기가 되므로, **픽업이 `FName ItemId`만 복제하고 `FEPItemState`는 서버 전용**으로 단순화. 정보 은폐 경계는 동일 |
| **C-2** `InstanceId`(FGuid) 삭제 | **결론은 채택, 논거 하나는 무효.** "병합·분할에서 의미가 정의되지 않는다"는 **스택이 없어 성립하지 않는다.** "읽는 코드가 없다"만으로 유지 |
| **C-3** 같은 ItemId 구분 | `SlotIndex` → **`EntryId`로 개명.** 칸 수가 아이템마다 달라 "몇 번 칸"이라는 의미가 없어졌으나, 넣은 **이유**(FastArray 순서 미보장)는 그대로 |
| **D-2** 새로 생기는 유일한 함정 | **유효.** `EquippedEntryId`로 이름만 바뀌고 위험 동일 → **write-back → `MarkItemDirty` → 엔트리 제거** 순서 규칙으로 명시 |

---

## 2. 결정 B — 부착물. ★ 1차 검증의 핵심 논거가 무효다

### 1차 A-1 원문

> "**부착물(attachment) 시스템이 `GAME.md` 어디에도 없다**는 것이 결정적이다.
> Lyra/타르코프가 `UItemInstance`를 UObject로 두는 제1 이유가 부착물·모듈 트리인데 (...)
> **제외 목록에조차 올라와 있지 않을 만큼** 고려 밖이다."

**사실이 아니다.** 부착물은 추후 도입 예정이다. 배그식 — 무기에 고정 슬롯 N개, 부착물은 자기 슬롯을 갖지 않는 **깊이 1**.

### 그래도 결론은 유지된다고 판단했다 (← 여기를 봐달라)

논거가 "부재"에서 "존재해도 낫다"로 **교체**됐고, 후자가 더 강하다고 본다.

**근거 1 — 엔진이 중첩을 막는다.** 어떤 설계든 평면 배열로 가야 한다.

```
Class.cpp:974   Fatal   "Struct recursion via arrays is unsupported for properties."
Class.cpp:5512  Warning "nested NetDeltaSerialize struct ... not supported.
                         Only use NetDeltaSerialize structs at class level."
```

**근거 2 — UObject의 비용은 트리에서 최대가 된다.**

| | UObject 트리 | 평면 + 부모 참조 |
|---|---|---|
| 순회 | 자연스러움 | 한 번 훑기. **가독성만 손해** |
| 소유권 | 노드마다 Outer·수명·핸들 — 1차에서 제거한 부담이 **노드 수만큼 곱해진다** | 없음 |
| 복제 | **서브오브젝트 트리 복제**. 노드마다 등록, 클라가 트리 재구성, 부착·탈착 순서에 널 참조 | 기존 FastArray 무변경 |
| 이관(버리기·거래) | 서브트리 소유권 이동 | 값 복사 |
| DB 직렬화 | 트리 → 행 변환 | 이미 행이다 |

**근거 3 — 부착물은 Fragment가 아니라 자기도 아이템이다.** 따로 줍고 팔 수 있으므로 어차피 인벤토리 엔트리와 같은 표현이어야 한다. Lyra의 UObject 논거(런타임 부착 Fragment)가 이 프로젝트엔 적용되지 않는다.

### 표현

```cpp
FEPInventoryEntry {
    int32 EntryId;
    int32 ParentEntryId = INDEX_NONE;   // ← 부착물일 때만. 지금은 넣지 않는다
    FName SlotId;                        // "Optic" / "Muzzle" / "Grip" / "Mag"
    FName ItemId;
    FEPItemState State;
}
```

```
EntryId=7   Parent=NONE  Weapon_AK74
EntryId=12  Parent=7     Optic   Scope_4x
EntryId=14  Parent=7     Mag     Mag_STD   Charges=30    ← 장전된 30발
EntryId=20  Parent=NONE  AmmoBox_545       Charges=100   ← 인벤토리에 따로
```

**깊이가 1로 유지되는 이유:** 타르코프의 깊이는 "아이템 안에 아이템"에서 나오지 "탄창 안에 총알"에서 나오지 않는다. 총알은 아이템이 아니라 숫자(`Charges`)다.

### 지금 확보돼 있는 것

부착물의 전제 조건은 **안정적인 개체 식별자** 하나이고, `EntryId`(서버 발급 / 단조 증가 / **재번호 없음**)가 그것이다. 배열 인덱스를 골랐다면 부모 참조가 불가능해 원천 봉쇄됐다. 다른 이유(FastArray 순서)로 내린 결정이 문을 열어뒀다.

→ **Step 00~05에서 부착물을 위해 지켜야 할 유일한 것 = `EntryId` 안정성.** STATUS에 못박았다.

---

## 3. 현재 설계 (압축)

```cpp
// 타입 데이터 — UObject 계층 유지. 다형성은 전부 여기
UEPItemDefinition : UPrimaryDataAsset
├─ ItemId / ItemDataRow / WorldMesh / Icon / GrantedAbility
├─ int32 InitialCharges = 0
└─ virtual void InitState(FEPItemState&) const;    → State.Charges = InitialCharges
UEPWeaponDefinition : UEPItemDefinition
└─ InitState() override                             → State.Charges = MaxAmmo

// 개체 상태 — 순수 값 타입
USTRUCT() FEPItemState {
    int32 Charges    = 0;      // 무기·탄약상자: 발수 / 현금뭉치: 금액 / 소모품: 사용 횟수
    float Durability = 100.f;
};

USTRUCT() FEPInventoryEntry : FFastArraySerializerItem {
    int32 EntryId = INDEX_NONE;
    FName ItemId;
    FEPItemState State;
};

UEPInventoryComponent          // Character 부착
├─ FEPInventoryList Entries    (COND_OwnerOnly)
├─ int32 MaxSlots = 30         (COND_OwnerOnly)
├─ int32 EquippedEntryId       (COND_OwnerOnly)
└─ int32 NextEntryId = 1       (서버 전용, 복제 X)

AEPPickup
├─ FName ItemId                (Replicated)      ← 클라가 알아야 하는 것
└─ FEPItemState State          (UPROPERTY, 복제 X) ← 소유자만 알아야 하는 것

// 삭제: UEPItemInstance / UEPWeaponInstance / UEPItemInstanceSubsystem / int32 핸들 전부
// 유지: UEPItemDefinitionSubsystem
```

**핵심 흐름**

```
생성  : Def->InitState(State)         → AddItem(ItemId, State) → int32 EntryId
획득  : Entry.State = Pickup->State                     (값 복사)
버리기: Pickup->State = Entry.State   → 엔트리 제거      (값 복사)
장착  : Entry.State.Charges → SetAmmo()
해제  : GetAmmo() → Entry.State.Charges → MarkItemDirty(Entry)
```

---

## 4. ★ 내가 못 푼 문제 — 여기가 제일 중요하다

### 4-1. **`Charges` 합치기 — "스택 없음"이 실제로 성립하는가**

스택을 없앴는데, 다음이 정의되지 않았다.

- **현금뭉치 두 개를 주우면?** 합쳐지지 않으면 10,000원짜리 3장이 칸 3개를 먹는다. 합쳐지면 **그건 `Charges` 위의 병합이고, 내가 없앴다고 선언한 병합 로직이 이름만 바꿔 돌아온 것 아닌가**
- **반쯤 쓴 탄약상자 두 개는?** 같은 문제
- 합치기를 허용하면 **분할**도 따라온다("50발만 덜어서 버리기")

내 잠정 입장은 "합치지 않는다"(타르코프도 반쯤 쓴 탄창을 자동으로 합치지 않는다)인데, **돈에서는 그게 명백히 불편하다.** GAME.md는 "돈 = 인벤토리 아이템"이다.

**질문:** ① 합치기를 허용해야 하나? ② 허용한다면 그것이 스택과 실질적으로 무엇이 다른가 — `Quantity`를 되살리는 것과 비교해 정말 더 단순한가? ③ 돈만 예외로 두는 것(`bMergeable` 플래그)은 특수 케이스의 시작인가, 정당한 구분인가?

### 4-2. **`Charges`라는 이름이 네 가지 의미를 진다**

무기 잔탄 / 탄약상자 잔량 / 현금 금액 / 소모품 사용 횟수. 지금은 "이 개체가 담은 소모 단위"로 일관되게 읽히지만, **다형성을 필드 이름으로 뭉갠 것**일 수 있다.

`FInstancedStruct` 전환 기준을 "세 번째 카테고리가 자기 전용 필드를 요구할 때"로 문서화했는데(1차 B-2 결론 유지), **그 기준이 이 상황에 맞는지** 봐달라. 이미 네 개가 한 필드를 공유 중이면 기준선을 이미 넘은 것 아닌가?

### 4-3. **가방이 꽉 차면 아무것도 못 줍는다**

부분 획득이 사라져 결과가 전부 아니면 전무다. 타르코프 정확도로는 맞지만, **탄약 60발 중 40발만 들어가던 완충 장치가 없어졌다.** 게임 필 문제이지 구조 문제는 아니라고 봤는데, 동의하는지.

### 4-4. **`GetUsedSlots()`를 캐시하지 않기로 했다**

매 호출마다 엔트리 전체를 순회하며 `TMap` 조회를 한다(엔트리 20~30개). `CanFit`은 줍기 시도마다, UI 갱신마다 불린다.

캐시를 안 하는 이유는 **"추가·제거·복제 수신 세 경로를 전부 갱신해야 하고 하나만 빠지면 '안 찼는데 가득 찼다'가 된다"** 는 것이다. 상호작용 프롬프트가 용량을 표시하게 되면 틱 주기(0.1s)로 불릴 수 있는데, 그래도 괜찮은가.

### 4-5. **부착물이 올 때 `AEPPickup`만 모양이 바뀐다고 단정했다**

`FEPItemState` 하나 → `TArray<FEPInventoryEntry>` 서브트리. **정말 그것뿐인가?** 내가 안 따져본 후보:

- 루트 테이블이 **부착물이 미리 달린 무기**를 뱉을 수 있어야 하나
- 상호작용 프롬프트가 부착물을 표시해야 하나
- 사망 시 드랍이 서브트리를 어떻게 다루나
- `AEPWeapon` 액터가 부착물 메시를 소켓에 붙이는 것 외에, 스탯 합산 주입 지점이 `UEPCombatComponent` 어디여야 하나

### 4-6. **깊이 1이 안정적인가**

"총알은 숫자라 깊이가 늘지 않는다"고 했는데, **탄창에 서로 다른 탄종을 섞어 넣는 것**(타르코프는 가능)을 원하면 깨진다. 그 경우 탄창이 `TArray<...>`를 들어야 하고 깊이 2가 된다. 지금 기획엔 없지만, 이게 실질적 위험인지.

---

## 5. 그 외 검증 요청

### 5-1. 사실 확인

- `Class.cpp:974` / `:5512` 인용이 내가 읽은 대로인지 (struct 재귀 Fatal / 중첩 NetDeltaSerialize 불가)
- `EntryId`가 `int32` 단조 증가로 매치 내 충분한지 (오버플로는 비현실적이라 봤다)
- `AEPPickup::State`를 `UPROPERTY()`로 두되 `GetLifetimeReplicatedProps`에 등록하지 않는 방식이 의도대로 동작하는지 (GC·직렬화 대상이면서 복제는 안 됨)

### 5-2. 설계 반론

- **`AddItem`이 `bool`이 아니라 `int32 EntryId`를 돌려준다.** 삽입 직후 그 엔트리를 가리킬 수 있어 "기본 지급 후 자동 장착"에 재검색이 없다는 이유인데, 실패를 `INDEX_NONE`으로 표현하는 게 `bool` + out 파라미터보다 나은지
- **Step 04를 고정 슬롯 격자에서 "목록 + 칸 게이지"로 바꿨다.** 칸 수가 아이템마다 다르면 격자에 아이콘을 꽂는 방식이 성립하지 않아서인데, 익스트랙션 슈터 UI로서 후퇴는 아닌지
- **Step 04가 갱신 때마다 행 위젯을 전부 재생성한다.** 위젯 풀링/`UListView`를 지금 안 넣는 게 맞는지
- **write-back 순서 규칙이 두 문서(Step 03·05)에 중복 서술돼 있다.** 한쪽만 고쳐지는 사고를 막을 방법이 있는지

---

## 6. 이미 확인된 사실 — 재검증 불필요

1차 검증에서 확인했고 그대로 반영했다.

| | 근거 |
|---|---|
| FastArray 내부 struct 델타가 **기본 활성** | `FastArraySerializer.h:218` |
| `MarkItemDirty`는 **수동** | `FastArraySerializer.h:441` |
| FastArray 제약 4개(최상위 UPROPERTY / RepSkip 금지 / 배열 하나 / 정적 배열 중첩 금지) | `FastArraySerializer.h:721-728` |
| 클라 배열 순서 미보장 | `FastArraySerializer.h:54` |
| `FInstancedStruct` NetSerialize 지원, 단 프로퍼티 델타 상실 | `InstancedStruct.h:286` / `InstancedStruct.cpp:518,536` / `UnrealEngine.cpp:337` |
| 5.5 deprecated 세터 | `Actor.h:869,874` |
| `IsDataValid` 시그니처 | `Object.h:1100,1110` |

**이번 세션에서 새로 확인한 것**

- `UEPItemInstance` / `UEPWeaponInstance` **참조 0** — C++ 4개 파일 자신 외 전무, `Content/` uasset도 0. 삭제 비용 없음
- 이 클래스들의 출처: `git show 74bd94a:DOCS/Mine/Item.md` 3~4행 — **Epic 학습 경로 Coder-05의 공식(`FTableRowBase` + `UItemDefinition` + `UItemInstance(UObject)`)을 그대로 채택**한 것. 프로젝트 요구에서 도출된 게 아니다. `InstanceId`·`SchemaVersion`·`IsSupportedForNetworking`은 UObject라서 담을 수 있게 된 것들이지 그것들 때문에 UObject를 고른 게 아니다

---

## 7. 수정 범위 (완료)

| 파일 | 주요 변경 |
|---|---|
| `05_Loot_DOCS.md` | §4-1 값 타입 + 스택 폐지, §4-2 수량 필드 제거, §4-4 픽업 2분할·부분 획득 삭제, §4-6 칸 수 합산·`EntryId`·`MarkItemDirty` 함정, §4-7 순서 규칙, §4-8 `EquippedEntryId`, §4-9 `InitState()`, **§7-3 부착물 신규**, §8 확정표 전면 |
| `LOOT_STATUS.md` | 확정 결정표 전면, "손대야 할 것" 갱신, 설계 변경 이력, **`EntryId` 안정성 경고** |
| `05_Loot_00_ItemCore.md` | `FEPItemState` 신설, 인스턴스 클래스 **파일째 삭제** 절, `InitState()`, `MakeItemState()`, DT 7행 + DA 7종, `EP.Item.Make` → `EP.Item.State` |
| `05_Loot_01_Spawner.md` | 루트테이블 수량 제거, `InitPickup()` 단일 진입점, `State` 서버 전용, `EndPlay` 불필요, 부착물 시 모양 변경 표시 |
| `05_Loot_02_Interaction.md` | 상호작용 키 **E → F**, 개수 표기 제거 |
| `05_Loot_03_Inventory.md` | 엔트리 구조, 칸 수 합산 `AddItem`, `EntryId`, 수명 관리 불필요, 함정표 전면 |
| `05_Loot_04_InventoryUI.md` | 고정 슬롯 격자 → **목록 + 칸 게이지**, `EntryId` 정렬, `Charges` 표시 |
| `05_Loot_05_Equipment.md` | `EquippedEntryId`, `SetEntryCharges` + `MarkItemDirty`, **순서 규칙**, `DefaultLoadout` |
| `DOCS/DOCS.md` | `:45` `:65` `:179` `ItemInstance` → `FItemState(USTRUCT)` |
| `05_Loot_REVIEW_StructMigration.md` | §H 반영 결과 + **A-1 논거 무효 기록** |

> `DOCS.md:125` `:132`(Track A/B 운영 전략)에도 `ItemInstance`가 남아 있으나 **과거 작업 방식의 기록**이라 손대지 않았다.

---

## 8. 재론하지 않았으면 하는 것

- **struct vs UObject 자체** — 1차에서 결론 났고, §2가 유일한 새 변수(부착물)를 다뤘다. §2의 판단이 틀렸다면 그건 재론 대상이다
- **1차의 A-2 / A-3 / B-1 / B-2 / C-1** — 전부 그대로 유효하고 반영 완료
- 상호작용 키(F), 스택 폐지 자체, 부착물 깊이(1) — **사용자 기획 결정**이다. 구현상 문제가 있으면 지적하되, 기획 선호는 논의 대상이 아니다
