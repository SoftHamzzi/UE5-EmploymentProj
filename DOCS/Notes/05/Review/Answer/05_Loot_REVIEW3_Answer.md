# 검증 답변 3차 — 실무성 / 확장성 / 코드 수준

> 작성일: 2026-07-26
> 대상: `05_Loot_REVIEW3_Request.md`
> 1차: `05_Loot_REVIEW_StructMigration.md` / 2차: `05_Loot_REVIEW2_Request.md`·`_Answer.md`
> 시점: 구현 코드 0줄. 지금 고치는 게 가장 싸다는 요청서의 판단에 동의한다.

> 이 문서는 **검증 기록**이다. 확정 결정은 `LOOT_STATUS.md`, 설계는 `05_Loot_DOCS.md`, 구현 스펙은 Step 문서에 반영한다.

---

## 0. 총평

| 축 | 판정 | 요약 |
|---|---|---|
| **확장성** | **상** | 배낭이 들어오면서 부착물·컨테이너·사망드랍의 구조적 비용이 대부분 선불됐다. 방향 유지 |
| **실무성** | **중** | 이대로 짜면 Step 03에서 막힌다. 컴파일 불가 3곳, **설계 결함 3건**, 정의되지 않은 동작 1건 |
| **코드 간결함** | **중상** | 과설계는 못 찾았다. 문제는 **중복**이다 — stale이 세 번 연속 난 원인이기도 하다 |

요청서 §2가 "내가 이미 아는 결함"이라며 3개를 댔는데, **같은 유형이 9개**다(§3). 그리고 요청서가 인지하지 못한 **설계 결함 3건**이 §1·§2·§8에 있다. 그중 §1은 이 문서 전체가 막으려 했던 바로 그 버그가 다른 자리로 옮겨간 것이다.

### 판정표

| 요청 항목 | 판정 |
|---|---|
| 2-1 선언 없는 함수 3개 | ❌ **9개다** (함수 6 / 멤버 2 / 클래스 1) |
| 2-2 `FindFungibleEntry` 포인터 | ⚠️ 규칙을 좁히지 마라. **포인터를 안 쓰면 된다** |
| 2-3 DT / DA 분리 원칙 | ✅ 원칙 두 줄로 세울 수 있다. **필드 2개 이동** |
| 2-4 `RemoveEntry` ↔ `UnequipWeapon` | ⚠️ 결합은 OK. **Broadcast 지적이 맞고**, 재진입은 지금만 없다 |
| 2-5 스탯 합산 스펙 | ❌ **미뤄라.** §7-3의 근거가 사실과 다르다 |
| 3-1 단계별 단독 컴파일 | ❌ Step 04·05 불가, Step 03은 미검증 통과 |
| 3-1 단계별 검증 가능성 | ❌ **Step 03에 검증 수단이 전무하다** |
| 3-1 null·권한 체크 | ❌ 6곳 |
| 3-1 `AddDefaulted_GetRef` | ⚠️ 참조는 안전. **그 다음 줄이 위험** |
| 3-1 복제 순서 | ✅ 방어 불필요. **그 이유를 문서에 적어라** |
| 3-2 미정 #1/#4/#6 | ⚠️ #4는 GAME.md와 어긋남. #1은 선행 조건이 빠짐 |
| 3-2 §7-1/2/3 | ✅ 들어온다 |
| 3-2 DB 5단계 | ⚠️ 저장 대상 2개 추가 |
| 3-2 경제 | ⚠️ 구조는 감당. **규칙 2개가 없다** |
| 3-3 과설계 | ✅ 없다 |
| 3-3 관례 | ⚠️ 3건 + **치명적 누락 1건** |
| 3-4 1007줄 | ⚠️ 길이가 아니라 **중복**이 문제 |
| 3-4 stale 전수 | ❌ **8건.** ★2건은 STATUS 파일에 있다 |
| 3-4 프로세스 문제인가 | ✅ **그렇다.** 원인이 특정된다 |

---

## 1. ★★ 최우선 — 버리기 경로가 잔탄을 잃는다

**요청서가 인지하지 못한 설계 결함이고, 이 문서 전체가 막으려던 바로 그 버그다.**

`05_Loot_03_Inventory.md` 03-5의 절차:

```
3. Pickup->InitPickup(엔트리 + 자식 서브트리)   ← 여기서 Entry.State를 값 복사
4. RemoveEntry(EntryId)                          ← 여기서 UnequipWeapon() → write-back
```

**스냅샷이 write-back보다 먼저다.** 18발 쏘고 장착 무기를 G로 버리면 픽업이 들고 나가는 `Charges`는 **30**이다. 다시 주워 장착하면 30/30.

`05_Loot_05_Equipment.md`의 함정 #2가 정확히 이 증상("증상이 #1과 똑같다")을 적어놓고, 대응란에 "05-3 ★ = `RemoveEntry()`가 보장한다"고 썼다. 그런데 **`RemoveEntry()`는 자기가 보장한 값을 아무도 못 읽는 시점에 쓴다.**

불변식을 옳은 함수에 넣었는데, 그 결과 **"스냅샷은 제거보다 나중"** 이라는 새 순서 규칙이 생겼다. 2차 §7-④로 걷어낸 순서 규칙이 한 칸 옆으로 옮겨간 것이지 사라진 게 아니다.

### 수정 — 제거하는 함수가 스냅샷을 돌려준다

```cpp
// UEPInventoryComponent
// 제거된 서브트리를 write-back이 끝난 상태로 돌려준다. 실패 시 false
bool RemoveEntry(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved = nullptr);
```

```cpp
void UEPInventoryComponent::Server_DropItem_Implementation(int32 EntryId)
{
    TArray<FEPInventoryEntry> Removed;
    if (!RemoveEntry(EntryId, &Removed)) return;      // 없는 엔트리 = 조작된 요청

    AEPPickup* P = SpawnPickupInFront();
    P->InitPickup(MoveTemp(Removed));                 // ★ write-back이 끝난 값
    P->StartDropCooldown();
}
```

**순서를 뒤집는 것이 문법적으로 불가능해진다** — 스냅샷을 얻는 유일한 방법이 제거하는 것이기 때문이다. 이 문서가 write-back 순서에 대해 내린 결론("규율이 아니라 형태로 막는다")을 여기에도 그대로 적용한 것이다.

> 부수: 03-2의 `bool DropItem(int32)` public 선언은 정의도 호출도 없다. `Server_DropItem`과 중복이므로 지운다.

---

## 2. ★★ 자식 캐스케이드가 장착 무기를 건너뛴다

```cpp
void UEPInventoryComponent::RemoveEntry(int32 EntryId)
{
    if (EntryId == EquippedEntryId) CombatComponent->UnequipWeapon();
    RemoveChildrenRecursive(EntryId);
    // ... 자신 제거
}
```

장착 검사가 **자기 자신에만** 걸린다. 배낭에 든 무기를 장착한 채 배낭을 버리면:

- `EntryId`(배낭) != `EquippedEntryId`(무기) → **unequip이 안 불린다**
- `RemoveChildrenRecursive`가 무기 엔트리를 그냥 지운다 → write-back 소실
- `EquippedEntryId`가 **죽은 번호**를 가리킨 채 남는다
- `AEPWeapon` 액터는 손에 그대로 붙어 있다

**정상 플레이에서 나오는 경로다.** 본체가 10칸이고 무기가 5칸이므로(00-8), 03-4의 본체→배낭 폴백으로 무기가 배낭에 들어가는 일이 흔하다. 그리고 05-4는 "무기는 자기 `SlotSize`만큼 칸을 차지한 채로 장착된다" — 즉 장착해도 엔트리는 배낭에 남는다.

### 수정 — 제거 경로를 하나로 만든다

```cpp
void UEPInventoryComponent::RemoveChildrenRecursive(int32 ParentId,
                                                    TArray<FEPInventoryEntry>* OutRemoved)
{
    // ★ 자식 목록을 먼저 뜬다 — 순회 중 배열이 바뀐다
    TArray<int32> Children;
    for (const FEPInventoryEntry& E : Entries.Items)
        if (E.ParentEntryId == ParentId) Children.Add(E.EntryId);

    for (int32 Id : Children)
        RemoveEntry(Id, OutRemoved);        // ★ 같은 함수로 되돌아온다
}
```

`RemoveEntry`가 **유일한 제거 지점**이 되면 장착 검사·write-back·`MarkItemDirty`가 노드마다 자동으로 돈다. 지금 문서는 제거 경로가 둘(`RemoveEntry` / `RemoveChildrenRecursive`)이고 불변식은 한쪽에만 있다 — **불변식을 함수로 강제한다는 원칙 자체가 반쪽만 지켜졌다.**

> **순회 중 수정 금지가 여기서 실제 문제다.** FastArray는 삭제에 `RemoveAtSwap`을 쓴다(`FastArraySerializer.h:1191`). 원본 배열을 순회하면서 지우면 인덱스가 뒤에서 앞으로 튀어 **일부 자식을 건너뛴다.** 증상은 "가끔 고아 엔트리가 남는다"이고 재현이 어렵다.

---

## 3. 2-1 답 — 전수 결과: **9개**

| # | 심볼 | 호출 위치 | 종류 |
|---|---|---|---|
| 1 | `GetEquippedBackpack()` | 03-4:329,330 / 04-2:97 | 함수 |
| 2 | `FindFungibleEntry()` | 03-3:234 | 함수 |
| 3 | `RemoveChildrenRecursive()` | 03-2:173 / 05-3:139 | 함수 |
| 4 | **`GetEquippedEntryId()`** | 05-3:100 | 함수 — 요청서 미인지 |
| 5 | **`SetEquippedEntryId()`** | 05-3:105 | 함수 — 요청서 미인지 |
| 6 | **`ConsumeCharges()`** | 마스터 §4-6:514 / 03-3:312 "어차피 필요하다" | 함수 — 선언 없음 |
| 7 | **`Defs`** | 03-3:229,264,278 / 01-4:257 / 05-5:198 | **멤버** |
| 8 | **`CombatComponent`** | 03-2:171 / 05-3:138 | **멤버** |
| 9 | **`UEPContainerPanel`** | 04-2:95,101,104 | **클래스 전체** |

### 7·8이 더 나쁘다 — 함수가 아니라 **설계 결정**이 빠진 것

함수는 없으면 "추가하면 되네"지만, 멤버는 **어떻게 얻을 것인가**를 정해야 한다.

**`Defs`** — `UGameInstanceSubsystem`이다. 멤버로 캐시할지 매번 조회할지 정해야 하는데, **매번 조회를 권한다.**

```cpp
// UEPInventoryComponent — private
const UEPItemDefinitionSubsystem* Defs() const;   // GetGameInstance()->GetSubsystem<...>()
```

00-5가 `FindData()` 반환 포인터의 장기 보관을 금지한 이유("`BuildDataCache()`가 다시 돌면 전부 댕글링")와 같은 종류의 질문을 서브시스템 포인터에도 만들지 않기 위해서다. 조회는 `TMap` 룩업 한 번이라 비용이 없다.

**`CombatComponent`** — 멤버로 두면 `UEPInventoryComponent` 헤더가 `UEPCombatComponent`를 알아야 하고, 반대 방향(05-3의 `Inv->SetEntryCharges`)도 있어 **헤더 순환**이다.

```cpp
// .cpp 안에서만
if (UEPCombatComponent* Combat = GetOwner<AEPCharacter>()->GetCombatComponent())
    Combat->UnequipWeapon();
```

기존 관례와도 같다 — `UEPCombatComponent`가 `GetOwnerCharacter()`를 거쳐 `AEPPlayerState`에 닿는 방식(`EPCombatComponent.cpp:171-172`)이 이미 그 모양이다. **이렇게 하면 2-4의 "두 컴포넌트가 서로를 직접 부른다"가 헤더 결합 없이 끝난다** (§6).

### 9는 Step 04가 배낭 이전 버전에 멈춰 있다는 신호다

`UEPInventoryWidget` 선언(04-2:75-77)은 `ItemList` / `CapacityText` / `CapacityBar`를 `BindWidget`으로 갖는데,

- 04-2의 **위젯 트리**는 그 셋을 `BodySection` / `BackpackSection` **안**에 둔다
- 04-2의 **구현** `RefreshEntries()`는 셋 중 아무것도 쓰지 않고, 선언되지 않은 `BodySection`·`BackpackSection`을 쓴다

선언·트리·구현이 셋 다 다르다. `BindWidget`은 WBP에 같은 이름이 없으면 **위젯 블루프린트 컴파일에서 실패**하므로, 이 상태로 만들면 에디터에서 막힌다.

`UEPContainerPanel`을 선언하고, `RowWidgetClass`를 그쪽으로 옮겨야 한다(현재 `UEPInventoryWidget`에 있는데 쓰는 쪽은 `UEPContainerPanel::Rebuild`다).

### 접근 지정자 — 컴파일 에러 1건

03-2에서 `SetEntryCharges` / `RemoveEntry`가 **protected**인데, 05-3의 `UEPCombatComponent::UnequipWeapon()`이 `Inv->SetEntryCharges(...)`를 부른다.

03-2의 "밖으로 내보내지 않는다"는 **원시 엔트리**에 대한 규칙이지 수정 API에 대한 것이 아니다. 오히려 그 규칙의 목적이 "수정은 API로만"이므로 **`SetEntryCharges`는 public이 맞다.** `RemoveEntry`는 `Server_DropItem`이 내부에서 부르므로 protected로 둬도 된다.

### 시그니처 어긋남

| 위치 | 내용 |
|---|---|
| `LOOT_STATUS.md:91` | `EquipFromInventory(int32 **Handle**)` — 05-3은 `EntryId`. `Handle`은 폐기된 설계의 잔재 |
| 03-2:126 `bool DropItem(int32)` | 정의도 호출도 없음. 03-5는 `Server_DropItem` |
| 03-2:133 `CanFit(Container, ItemId)` | 선언만. `AddItem`이 같은 판정을 인라인으로 다시 쓴다 — 하나로 합쳐라 |

---

## 4. 2-2 답 — 규칙을 좁히지 마라. **포인터를 안 쓰면 된다**

`AddItem`이 `Found`에서 읽는 것은 `EntryId`와 `Charges` 둘뿐이고, 둘 다 포인터 없이 얻을 수 있다.

```cpp
int32 FindFungibleEntryId(FName ItemId) const;      // 없으면 INDEX_NONE
void  AddEntryCharges(int32 EntryId, int32 Delta);  // SetEntryCharges의 상대
```

```cpp
if (Data->bFungible)
{
    const int32 Id = FindFungibleEntryId(ItemId);
    if (Id != INDEX_NONE)
    {
        AddEntryCharges(Id, InState.Charges);
        return Id;
    }
}
```

**이득이 하나 더 따라온다.** 지금 코드는 `Found->State.Charges + InState.Charges`를 **호출부에서** 계산해 `SetEntryCharges`에 넘긴다 — 읽고·더하고·쓰는 것이 함수 경계를 넘나든다. `AddEntryCharges`는 셋을 한 함수 안에 가둔다. 규칙이 아니라 **형태로** 막는 것이 이 문서의 원칙이다.

그리고 `ConsumeCharges(Id, N)`는 `AddEntryCharges(Id, -N)`이다. 자판기 1000원 투입·소모품 사용·재장전 소비가 전부 이걸 쓴다(마스터 §4-6:514). **함수 하나로 합쳐라** — 부호만 다른 두 함수를 따로 두면 어느 쪽이 클램프를 하는지가 갈린다.

> **규칙은 그대로 둬라.** "private이니 예외"를 한 번 허용하면 다음 사람이 그 예외를 근거로 쓴다. 예외를 만들 필요 자체가 없는 형태가 있으므로 더욱 그렇다.

---

## 5. 2-3 답 — 원칙 두 줄. 필드 2개 이동

**DataTable이 DataAsset보다 나은 점은 정확히 하나다** — 여러 아이템을 한 화면에서 비교·수정하고 CSV로 오간다.
**DataAsset이 나은 점도 하나다** — 에셋 참조를 갖고, `virtual`로 오버라이드된다.

여기서 원칙이 나온다.

> **① 여러 아이템을 표로 나란히 놓고 조정하는 값은 `FEPItemData`(DataTable).**
> **② 그 아이템 한 종류에만 의미 있는 것 — 에셋 참조, `virtual` 동작, 타입 전용 필드 — 은 `UEPItemDefinition`(DataAsset).**

현재 배치를 이 기준으로 검사하면:

| 필드 | 현재 | 기준상 | |
|---|---|---|---|
| `SlotSize` / `SellPrice` / `Rarity` / `MaxStack` / `bIsQuestItem` / `DisplayName` / `Description` | DT | DT | ✅ |
| **`bFungible`** | DT | DT | ✅ |
| `WorldMesh` / `Icon` / `GrantedAbility` | DA | DA | ✅ 에셋 참조 |
| `InitState()` | DA | DA | ✅ virtual |
| `MaxAmmo` | DA(Weapon) | DA | ✅ 무기 전용 |
| **`InitialCharges`** | DA | **DT** | ❌ 탄약상자 100 / 현금뭉치 10000 / 붕대 1 — 전형적인 밸런싱 열 |
| **`ContainerCapacity`** | DA | **DT** | ❌ 소형 12 / 중형 20 / 대형 30 — 전형적인 밸런싱 열 |

**두 필드만 옮기면 원칙에 예외가 없어진다.** `InitState`가 `InitialCharges`를 읽으므로 인자가 하나 는다:

```cpp
virtual void InitState(const FEPItemData& Data, FEPItemState& State) const;
// 기본 : State.Charges = Data.InitialCharges;
// 무기 : State.Charges = MaxAmmo;      ← MaxAmmo는 무기 전용이라 DA에 남는다
```

호출부는 `MakeItemState` 하나뿐이고(00-6), 거기서 Row와 Definition을 이미 둘 다 들고 있다. **비용 = 인자 하나.**

> **`MaxAmmo`가 왜 DA에 남는가 — 이게 원칙 ②의 판정선이다.** DT는 모든 아이템이 공유하는 표다. 무기 전용 열을 넣으면 나머지 행이 전부 빈칸이 된다. **"모든 아이템이 값을 갖는가"** 를 물으면 `InitialCharges`(전부 가짐, 대부분 0) / `ContainerCapacity`(전부 가짐, 대부분 0) / `MaxAmmo`(무기만)가 정확히 갈린다.

### 참고 — 더 근본적인 선택지 (권하지는 않는다)

`FEPItemData`를 없애고 Definition 하나로 합칠 수 있다. 지금 계층이 둘이라서 무는 비용은:

① 양방향 참조를 손으로 맞춤 → ② 그것 때문에 `IsDataValid` 오버라이드 → ③ 캐시 2개·조회 2개(`FindData`/`FindDefinition`) → ④ `FindData` null → "가방 무한대" 무증상 버그(03-3)

전부 계층이 둘이라서 생긴 것이고, **지금이 합치기 가장 싼 시점이다**(코드 0줄, DT 3행, DA 3개).

그럼에도 권하지 않는다 — 아이템이 수십 종이 되면 밸런싱 표가 있는 편이 확실히 낫고, DataAsset은 일괄 수정도 diff도 안 된다. **원칙 두 줄이면 충분하다.**

---

## 6. 2-4 답 — 결합은 OK, Broadcast 지적이 맞다, 재진입은 "지금만" 없다

### 계층 — 허용된다. 단 헤더로 서로를 알게 하지 마라

두 컴포넌트가 서로를 부르는 것 자체는 문제가 아니다. `UEPCombatComponent`가 이미 `AEPPlayerState`·`UEPAttributeSet`·`AEPWeapon`을 가로질러 부른다. 문제가 되는 건 **헤더 순환**이고, §3의 처방(양쪽 다 `.cpp`에서 캐릭터를 경유)으로 끝난다.

### Broadcast — 지적이 맞다. 그리고 이유가 하나 더 있다

지금 구조에서 **엔트리 하나 제거에 브로드캐스트가 여러 번 나간다.**

```
RemoveEntry(배낭)
  → UnequipWeapon → SetEntryCharges → Broadcast   (1)
  → 자식 N개 각각                    → Broadcast   (N)
  → 자신 제거                        → Broadcast   (1)
```

Step 04는 매번 목록을 **통째로 다시 만든다**(04-2). 배낭 하나 버리면 위젯 재생성이 N+2회 돈다.

**클라이언트 쪽은 더 나쁘다.** `PostReplicatedAdd`는 **항목마다** 불린다(`FastArraySerializer.h:1163`). 한 번의 수신에 갱신이 항목 수만큼 돈다.

엔진이 정확히 이걸 위한 콜백을 준다.

```cpp
// FEPInventoryList에 정의한다 — 엔트리가 아니라 직렬화기 쪽이다
void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Params);
```

> *"If a function with the signature `void PostReplicatedReceive(...)` is defined in the derived struct, **it will be called after each call to NetDeltaSerialize on the receiving end**, including if we have mapped some unmapped objects"*
> — `FastArraySerializer.h:517-519` (호출부 `:1386, :1619, :1736, :1864`)

**항목별 콜백 3종을 전부 버리고 이 하나만 쓴다.** 수신 한 번당 정확히 한 번 불리고, Add/Change/Remove를 구분할 필요가 없다 — Step 04는 어차피 전체 재생성이다.

**Step 04 함정 #3("`PostReplicatedChange`를 브로드캐스트 안 함 → 잔탄 숫자만 옛날 값")이 통째로 소멸한다.** 빠뜨릴 콜백이 없기 때문이다.

서버 쪽도 같은 모양으로 맞춘다.

```cpp
// UEPInventoryComponent — private
int32 NotifyDepth   = 0;
bool  bNotifyPending = false;
struct FScopedInventoryNotify;   // 진입 ++ / 이탈 --. 0이 되고 pending이면 그때 한 번 Broadcast
```

`AddItem` / `RemoveEntry` / `SetEntryCharges` 선두에 가드를 하나 놓으면 **"중간 Broadcast"라는 상태가 존재할 수 없게 된다.** 10줄이고, 이 문서가 write-back 순서에 대해 한 처리와 같은 종류다.

### 재진입 — 지금은 없다. 그런데 근거가 하나뿐이다

`RefreshEntries()`는 읽기 전용이므로 지금은 종료한다. 맞다.

문제는 **그것이 유일한 근거**라는 점이다. 구독자가 하나 더 붙어 인벤토리를 건드리는 순간(예: "가방이 꽉 차면 자동으로 뭔가 버린다") 재진입한다. 위 스코프 가드를 넣으면 **`Entries.Items`를 순회하는 도중에는 브로드캐스트가 나가지 않으므로** 구독자가 무엇을 하든 성립하지 않는다.

---

## 7. 2-5 답 — **부착물 단계로 미뤄라.** §7-3의 근거가 사실과 다르다

`05_Loot_DOCS.md` §7-3:920이 *"Step 05에서 무기 장착 흐름을 손댈 때 같이 정리하면 추가 비용이 거의 없다"* 고 했는데, 실제 `WeaponDef->` 직접 읽기는 **5개 파일**이다.

| 파일 | 읽는 것 | 부착물이 바꾸나 |
|---|---|---|
| `EPWeapon.cpp` :42,66,78,79,83,116,121,123,126 | Spread 6종 / Damage / PelletCount | ✅ |
| `EPCombatComponent.cpp` :177,178 | `MaxAmmo` | ✅ |
| `EPCombatComponent.cpp` :70,102,105,155,187,201,218,368 | BallisticType / ProjectileClass / AnimLayer / Abilities / TagDamage | ❌ |
| `EPServerSideRewindComponent.cpp` :282,399 | `TraceDistanceCm` | ❌ |
| `EPGA_Item_PrimaryUse.cpp` :111 | `FireRate` | ⚠️ |
| `EPGA_Item_Reload.cpp` :46 | `ReloadTime` | ⚠️ |

두 가지가 나온다.

**① Step 05가 이걸 하면 자기 완료 조건을 깬다.**
05의 완료 조건 마지막 줄이 *"`GA_Item_PrimaryUse` / `GA_Item_Reload`는 **한 줄도 수정하지 않았다**"* 이다. `FireRate`/`ReloadTime`을 캐시로 돌리려면 그 두 파일을 건드려야 한다. **같은 문서 안에서 정면 충돌한다.**

**② 부착물이 실제로 바꾸는 스탯은 이미 한 곳에 모여 있다.**
Spread/Recoil 계열은 전부 `AEPWeapon` 안이고(`EPWeapon.cpp`), 그게 §7-3이 목표로 지목한 바로 그 자리다. *"읽는 곳이 흩어져 있으면 부착물이 올 때 전부 찾아 고쳐야 한다"* 는 전제가 Spread에 대해서는 이미 거짓이다.

### 남는 준비는 **한 줄**이다

```cpp
// EPCombatComponent.cpp:177-178 — Step 05가 어차피 고치는 줄
AS->SetMaxAmmo(static_cast<float>(NewWeapon->GetMaxAmmo()));   // WeaponDef->MaxAmmo 대신
```

`AEPWeapon::GetMaxAmmo()`는 지금 `WeaponDef->MaxAmmo`를 그대로 돌려주고, 부착물이 오면 그 함수 안에서 합산한다.

**새 패턴이 아니라 기존 패턴을 한 번 더 쓰는 것이다** — `AEPWeapon::GetDamage()`가 이미 정확히 이 모양이다(`EPWeapon.cpp:66`).

> **Step 05 문서에 넣을 것은 이 한 줄뿐이고, 스탯 합산 스펙은 쓰지 마라.** 지금 쓰면 슬롯 스키마(§7-3 비용표에서 "신규 ❌")가 없는 상태에서 쓰는 것이라 부착물 단계에서 다시 쓴다. §7-3:911-920을 사실에 맞게 줄여라 — "Spread/Recoil은 이미 `AEPWeapon`에 모여 있고, 남은 건 `MaxAmmo` 한 줄이다".

---

## 8. 3-1 실무성

### 8-1. 단계별 단독 컴파일

| Step | 판정 | |
|---|---|---|
| 00 | ✅ | |
| 01 | ✅ | `IEPInteractable` 상속을 명시적으로 미룬 것(01-4 ★)이 정확하다. 1차 지적이 제대로 반영됐다 |
| 02 | ✅ | |
| 03 | ⚠️ | 컴파일은 된다. **동작이 미검증으로 통과한다** — 아래 |
| 04 | ❌ | `UEPContainerPanel` 미선언 (§3) |
| 05 | ❌ | `GetEquippedEntryId`/`SetEquippedEntryId` 미선언 + `SetEntryCharges` protected (§3) |

**Step 03의 ⚠️:** `RemoveEntry`가 `EquippedEntryId`를 보는데, Step 03에는 그 값을 세팅하는 경로가 없다(Step 05다). 즉 **그 분기는 Step 03 내내 항상 거짓**이고, 장착 관련 불변식은 한 번도 실행되지 않는다.

컴파일도 되고 완료 조건도 전부 통과하는데, **§1과 §2의 버그는 Step 05에 가서야 처음 드러난다.** 문서에 "이 분기는 Step 05부터 실제로 돈다"를 명시하지 않으면, Step 03을 "통과"시켜 놓고 Step 05에서 원인이 두 단계 뒤에 있는 버그를 만난다.

### 8-2. ★ 배낭을 **어떻게 매는지가 어느 문서에도 없다**

가장 큰 구멍이다.

- Step 03 완료 조건: *"**배낭을 매면** 본체 10칸과 별개로 배낭 칸이 열린다"*
- `GetEquippedBackpack()`이 03-4·04-2에서 불린다
- **그런데 배낭을 매는 동작이 Step 00~05 어디에도 없다.** RPC도, UI 조작도, 자동 장착도 없다
- `EquippedEntryId`는 무기 하나짜리 필드다(05-4). **배낭용 필드는 선언조차 없다**
- `DOCS/GAME.md` 장비 절은 "배낭 슬롯 1"을 명시한다 — **기획에는 있고 구현 스펙에만 없다**

**Step 03의 완료 조건이 정의되지 않은 동작에 의존한다.** 최소한 이것이 필요하다.

```cpp
UPROPERTY(Replicated) int32 EquippedBackpackEntryId = INDEX_NONE;   // COND_OwnerOnly
UFUNCTION(Server, Reliable) void Server_EquipBackpack(int32 EntryId);
int32 GetEquippedBackpack() const { return EquippedBackpackEntryId; }
```

그리고 **`RemoveEntry`가 이 번호도 정리해야 한다** — 무기와 완전히 같은 이유다. 배낭을 버렸는데 `EquippedBackpackEntryId`가 죽은 번호로 남으면 `GetCapacity`가 0을 돌려주고, Step 04에 **유령 구획**이 남는다.

> **Step 03에 넣는 것을 권한다** — Step 03의 완료 조건이 요구하기 때문이다.
> 05-4의 *"확장 시 `TMap<EEPEquipSlot, int32>`로 바꾼다"* 가 지금 필요해진 셈인데, **필드 둘로 가라.** 슬롯이 셋(주무기/보조/배낭)이 되는 건 미정 #5이고, 그때 한 번에 TMap으로 바꾸는 편이 싸다. 지금 TMap을 넣으면 원소가 둘인 맵이 된다.

### 8-3. ★ 배낭을 다시 주우면 **내용물이 전부 사라진다**

03-5는 서브트리를 픽업에 넘긴다. 그런데 03-4의 획득 경로는:

```cpp
int32 NewId = Inv->AddItem(INDEX_NONE, ItemId, State);   // ← 엔트리 하나만
```

**픽업이 든 자식들을 읽는 코드가 없다.** 배낭을 버렸다 다시 주우면 안의 아이템이 전부 증발한다.

Step 03 완료 조건은 *"배낭을 버리면 안의 아이템이 같이 나가고, 고아 엔트리가 남지 않는다"* 만 확인하고 **되줍기를 확인하지 않는다** — 검증에서도 새어나간다.

필요한 것은 **서브트리 삽입 API**이고, 여기서 `EntryId` **재매핑**이 필수다. 픽업이 든 엔트리들의 `EntryId`는 버린 사람의 번호이고, 새 인벤토리에서는 `NextEntryId`로 다시 발급된다. `ParentEntryId`를 옛 번호 → 새 번호로 옮기지 않으면 자식이 고아가 되거나 **남의 엔트리에 매달린다.**

```cpp
// 반환: 루트의 새 EntryId. 실패 시 INDEX_NONE
int32 AddSubtree(int32 Container, const TArray<FEPInventoryEntry>& In);
//  1. 루트(ParentEntryId == INDEX_NONE인 것) 기준으로만 칸 검사 — 자식은 칸을 안 먹는다
//  2. TMap<int32,int32> OldToNew를 만들며 순서대로 삽입
//  3. 각 엔트리의 ParentEntryId를 OldToNew로 치환. 루트는 Container
```

> `bFungible` 합치기는 이 경로에 **적용하지 않는다.** 컨테이너·무기는 균질이 아니라 루트가 fungible일 일이 없고, 넣으면 자식 처리가 정의되지 않는다.

**이건 부착물(§7-3)에도 그대로 필요하다.** §7-3:898의 비용표에 `AEPPickup이 서브트리를 든다 ✅`는 있는데 **"서브트리를 다시 넣는다"가 없다.** 배낭이 선불한 항목 목록에 이 줄을 추가해야 한다.

### 8-4. null·권한 체크

| 위치 | 문제 |
|---|---|
| 05-3 `UnequipWeapon` | `GetOwner<AEPCharacter>()->GetInventoryComponent()` 무가드 — 지적대로. 기존 코드가 `if (!GetOwner()->HasAuthority() \|\| !EquippedWeapon) return;`으로 시작하니 그 옆에 붙인다 |
| 03-4 `OnInteract` | `Instigator->GetInventoryComponent()` 무가드 |
| 03-3 `AddItem` | `check(GetOwner()->HasAuthority())` — **프로젝트 관례와 다르다.** 기존 코드는 전부 early return이고 `CLAUDE.md §Conventions`도 그 형태다. `check`는 Shipping에서도 크래시한다 |
| 04-4 `SetEntry` | `Data`가 null이면 **조용히 return** → 행이 빈 채 남는다. 03-3은 같은 상황에 `ensureMsgf`를 넣었는데 여기만 없다 |
| 05-5 `HandleStartingNewPlayer` | `const UEPItemDefinitionSubsystem* Defs = /* GameInstance에서 획득 */;` 가 주석이다. null이면 **전원이 빈손으로 시작**하고 원인이 GameMode로 안 보인다 |
| 01-4 `OnRep_ItemId` | 람다가 `Def`(원시 포인터)를 캡처한다. Definition은 상주하므로 안전하지만, **그 안전이 `DefinitionHandle`이 살아 있는 동안만 성립**한다는 걸 적어두는 게 좋다 |

### 8-5. `AddDefaulted_GetRef` — 참조는 안전하다. **그 다음 줄이 위험하다**

```cpp
FEPInventoryEntry& E = Entries.Items.AddDefaulted_GetRef();
// ...
Entries.MarkItemDirty(E);
OnInventoryChanged.Broadcast();     // ← 구독자가 AddItem을 부르면 재할당
return E.EntryId;                   // ← 댕글링
```

`MarkItemDirty`까지는 배열을 건드리는 것이 없으므로 **안전하다.** 깨지는 건 브로드캐스트 다음 줄이다.

```cpp
const int32 NewId = E.EntryId;      // ★ Broadcast 앞에서 값을 뜬다
OnInventoryChanged.Broadcast();
return NewId;
```

§6의 스코프 가드를 넣으면 이 문제 자체가 사라진다 — 브로드캐스트가 함수를 벗어난 뒤에 나가기 때문이다.

### 8-6. 복제 순서 — 방어가 필요 없다. **그 이유를 문서에 적어라**

자식이 부모보다 먼저 도착하면:

- `GetUsedSlots(INDEX_NONE)`은 그 자식을 안 센다 (`ParentEntryId != INDEX_NONE`)
- `Rebuild(Backpack)`은 아예 안 불린다 (`GetEquippedBackpack()`이 아직 `INDEX_NONE`)
- 자식은 **잠깐 안 보인다.** 다음 수신에서 저절로 맞는다

**깨지지 않는 이유는 모든 파생값을 매 갱신마다 처음부터 다시 계산하기 때문이다.** `UsedSlots`를 캐시하지 않기로 한 결정(03-3)이 여기서 두 번째 배당을 낸다 — 캐시했다면 "부모 없는 자식이 도착했을 때 캐시를 어떻게 하나"가 진짜 문제가 됐다.

**이걸 03-3에 적어라.** 안 적으면 다음 사람이 불필요한 순서 방어 코드를 넣는다.

---

## 9. 3-2 확장성

### 사망 드랍 — 미정 #4가 GAME.md와 어긋난다

미정 #4의 ⓑ("배낭 하나로")는 `GAME.md` 인벤토리 절의 **"사망 시 전부 드랍"** 과 다르다.

질문("본체 10칸에 있던 것들은?")의 답은 **기획에 이미 있다 — 전부 나간다.** 그리고 구조상으로도 이쪽이 더 싸다: 시체 위치에 픽업 하나를 스폰하고 §8-3의 `AddSubtree` 역방향으로 **전 엔트리**를 넘기면 된다. `ParentEntryId == INDEX_NONE`인 것들이 그 픽업의 루트가 된다.

§7-1 컨테이너(시체 루팅)의 특수 케이스이고, 03-2의 *"시체 액터가 자기 인벤토리를 별도로 노출"* 과도 맞는다. **미정 #4는 선택지가 아니라 이미 정해진 것으로 고쳐라.**

### 미정 #1(탄창) / #6(재장전 소비)

싸다. #6은 `ConsumeCharges` 하나이고 그 함수는 §4에서 어차피 만든다. #1은 ⓐ든 ⓑ든 `EquipFromInventory`가 읽는 대상만 바뀐다(자기 `State.Charges` → `SlotId=="Mag"`인 자식의 `State.Charges`).

다만 **ⓐ면 §8-3의 `AddSubtree`가 전제다** — 탄창 달린 무기를 줍는 것이 곧 서브트리 삽입이다. 미정 #1의 비고에 이 선행 조건을 적어라.

### §7-1 / 7-2 / 7-3

들어온다. 배낭이 `ParentEntryId` + 서브트리 픽업 + 자식 캐스케이드 + `AddSubtree`를 전부 선불한다. §7-2의 "상자를 안 열고 가방에 넣기"도 배낭과 같은 구조라 성립한다.

유일하게 새로 드는 것은 §7-1의 **"열기 전까지 내용물을 복제하지 않는다"** 다. `COND_OwnerOnly` 인벤토리와 달리 **컨테이너는 누구의 소유도 아니므로** 복제 조건을 새로 정해야 한다. 지금 결정할 필요는 없지만 §7-1에 한 줄 남겨라.

### 로드맵 5단계 DB

평면 엔트리가 그대로 행이 된다. `NextEntryId` 저장은 반영됐다. **두 개를 더 저장해야 한다.**

- `EquippedEntryId` / `EquippedBackpackEntryId` — 안 하면 로드 후 장착이 풀린다
- 특히 `EquippedEntryId`만 저장하고 `NextEntryId`를 안 저장하면 **죽은 번호를 가리킨다**

### 경제 — 구조는 감당한다. **규칙 두 개가 없다**

서브트리 순회는 깊이 3(배낭 > 무기 > 부착물)이고 평면 배열 한 번 훑기다. 비용 문제 없음.

없는 건 계산 규칙이다.

1. **`Durability`가 판매가에 어떻게 들어가나** — `GAME.md`는 "감소하며 판매가에 반영된다"만 적었다. `SellPrice * (Durability/100)`인지 구간별인지 정해야 한다. Step 범위 밖이지만 `GAME.md`에 한 줄 넣는 편이 낫다
2. **현금뭉치의 판매가는 `SellPrice`가 아니라 `Charges` 자체다.** `bFungible` 아이템은 판매가 계산에서 예외라는 규칙이 필요하다.
   > **지금 상태로 만들면 기본값이 이미 틀린다.** `FEPItemData::SellPrice`의 기본값이 `100`이고(`EPItemData.h:43`), 00-8의 신규 6행 표에 `SellPrice` 열이 **없다.** `Cash_10000`을 표대로 만들면 **10,000원짜리 현금뭉치를 100원에 파는 상태**가 된다.

---

## 10. 3-3 코드 수준

### 과설계 — **없다**

50줄로 될 걸 200줄로 쓴 곳을 못 찾았다. §4-6이 이번에 크게 불어난 건 배낭·합치기·부착물 세 결정이 실제로 거기서 만나기 때문이고, **코드량이 아니라 설명량**이 늘었다. 코드 자체는 `AddItem` 25줄 / `GetUsedSlots` 12줄 / `RemoveEntry` 6줄로 적정하다.

### 관례

| 항목 | 지적 |
|---|---|
| `check(HasAuthority())` | 프로젝트는 전부 `if (!GetOwner()->HasAuthority()) return;`. 03-3만 다르고 `CLAUDE.md §Conventions`와도 어긋난다 |
| `Defs` | 약어 멤버명. 프로젝트에 이런 축약이 없다. `Defs()` private 헬퍼로 (§3) |
| `TObjectPtr` | 03-1 `Owner` ✅ / 04-2 위젯 ✅ / 01-2 `TWeakObjectPtr<AEPPickup>` ✅ (의도적) |
| `const` | `FindEntry` / `GetUsedSlots` / `GetCapacity` / `CanFit` / `GetEntries` ✅ |
| 참조·값 | `FindEntry` 값 반환의 근거가 명확 ✅. `InitPickup`이 `TArray`를 받게 되면 `MoveTemp` |

### ★ 치명적 누락 — `FEPInventoryList::Owner`를 세팅하는 코드가 없다

03-1이 `UPROPERTY(NotReplicated) TObjectPtr<UEPInventoryComponent> Owner;`를 선언하고, 04-3이 `PostReplicated*` → `Owner->OnInventoryChanged.Broadcast()`를 전제한다.

**그런데 `Entries.Owner = this;`가 어느 문서에도 없다.**

빠지면 클라이언트에서 콜백이 델리게이트에 닿지 못한다. 증상은 **"서버는 정상인데 클라 인벤토리 UI가 영원히 갱신되지 않는다"** — 원인이 UI나 복제로 보여서 엉뚱한 데를 파게 된다. **Step 04 전체가 이 한 줄에 걸려 있다.**

```cpp
UEPInventoryComponent::UEPInventoryComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
    Entries.Owner = this;            // ★ 이것
}
```

---

## 11. 3-4 문서

### 1007줄은 문제가 아니다. **중복이 문제다**

§4-6/§4-7/§4-8이 Step 03/05 문서와 거의 같은 코드·같은 함정표를 갖는다. 확인한 중복:

| 사실 | 적힌 곳 |
|---|---|
| FastArray 제약 4개 (`:721-728`) | 마스터 §4-6:450, Step 03:108 |
| `MarkItemDirty` 수동 (`:441`) | 마스터 §4-6:444, Step 03:104, Step 05:125 |
| `EntryId` 재번호 없음 + `:54` | 마스터 §4-1:158, §4-6:436, Step 03:66, Step 04:132 |
| `INDEX_NONE` truthy | 마스터 §4-6:485, Step 03:343, Step 05:201 |
| `RemoveEntry` 코드 블록 | 마스터 §4-7:546, Step 03:168, Step 05:136 |
| 자식 캐스케이드 표 | 마스터 §4-7:569, Step 03:391 |
| `COND_OwnerOnly` 근거 + 시체 루팅 각주 | 마스터 §4-6:452, Step 03:183 |
| `bFungible` 표 + 스택 대조표 | 마스터 §4-6:489, Step 03:295 |
| `AddItem` 절차 | 마스터 §4-6:468, Step 03:223 |

**요청서 §3-4의 마지막 질문("이번이 세 번째다. 패턴이면 프로세스 문제로 봐달라")의 답이 이 표다.**

결정 하나를 바꾸면 3~4곳을 고쳐야 하고, 매번 하나가 남는다. 이번에도 남았다(§12).

**이 문서가 write-back 순서에 대해 내린 결론을 문서 자체에 적용하면 답이 나온다 — 사실은 한 곳에만 둔다.**

| 문서 | 담는 것 | 담지 않는 것 |
|---|---|---|
| `05_Loot_DOCS.md` §4 | **결정과 근거.** 왜 이렇게 했나 | **코드 블록·함정표·엔진 인용** |
| `05_Loot_0X_*.md` | **구현 스펙.** 코드·함정표·완료 조건·엔진 인용 | 결정의 근거 (마스터로 링크) |
| `LOOT_STATUS.md` | **진행 상태 + 확정표 한 줄 요약** | 코드·순서 규칙 |

마스터에서 코드 블록만 걷어내도 1007 → 700줄 안쪽이고, 무엇보다 **고칠 곳이 절반으로 준다.**

### `05_Loot_REVIEW*.md` — 분리해라

4개 합쳐 1,288줄로 스펙 폴더의 **27%** 다. `DOCS/Notes/05/Review/`로 옮기고 `LOOT_STATUS.md`에서 링크한다. 구현 중에 파일 목록을 볼 때 검증 기록이 시야를 먹는다.

---

## 12. stale 전수 — 이번에 남은 것 8건

| # | 위치 | 내용 |
|---|---|---|
| **★1** | `LOOT_STATUS.md:59` | 확정표에 **"write-back 순서: write-back → `MarkItemDirty` → 엔트리 제거"** 가 그대로 있다. **같은 표 :37이 "문서 규칙으로 두지 않는다"고 적어놓고 22줄 아래에서 그 규칙을 확정표에 싣는다** |
| **★2** | `LOOT_STATUS.md:88` | `Server_DropItem` 행이 "write-back → 엔트리 제거 순서"를 **Step 05** 조치로 적었다. `RemoveEntry`는 **Step 03**이다 |
| 3 | `LOOT_STATUS.md:91` | `EquipFromInventory(int32 **Handle**)` — 폐기된 핸들 설계의 잔재 |
| 4 | `LOOT_STATUS.md:21` | 부착물 항목이 "엔트리에 `ParentEntryId`/`SlotId` **2필드 추가**"라고 적었다. 배낭 때문에 Step 03에서 이미 들어간다 |
| 5 | `DOCS/GAME.md` 장비 절 | "무기 슬롯은 1정부터 만든다 (§8 **미정 #6**)" — 무기 2정은 **#5**다. #6은 재장전 탄약 소비 |
| 6 | `05_Loot_DOCS.md:972` | 미정 #4 사망 드랍 선택지 ⓑ가 `GAME.md` "사망 시 전부 드랍"과 어긋남 (§9) |
| 7 | `05_Loot_01_Spawner.md:274` | "**Step 03에서 확장한다**"(`AEPPickup::State` → 서브트리) — 맞지만 **Step 03 문서에 `AEPPickup` 확장이 한 줄도 없다** (§8-3) |
| 8 | `05_Loot_04_InventoryUI.md:75-77` | `UEPInventoryWidget`의 `BindWidget` 3개가 배낭 이전 구조 (§3) |

**★1·★2가 특히 나쁘다.** `CLAUDE.md`가 *"진행 상태의 진실의 원천은 STATUS 파일이다"* 라고 못박은 파일에 **폐기된 규칙이 확정표로 남아 있다.** 세션 시작 템플릿(`LOOT_STATUS.md`:118-123)이 이 파일부터 읽으라고 지시하므로, 구현 중에 STATUS만 보고 짜면 **§1의 버그를 정확히 그대로 만든다.**

---

## 13. 착수 전 결정 4가지

| # | 항목 | 권고 |
|---|---|---|
| 1 | `RemoveEntry`가 제거된 서브트리를 반환하게 할 것인가 | **그렇다.** §1·§2가 한 번에 닫히고, 순서를 뒤집는 것이 문법적으로 불가능해진다 |
| 2 | 배낭 장착을 Step 03에 넣을 것인가 | **넣는다.** Step 03 완료 조건이 요구한다. **필드 둘**(`EquippedEntryId` / `EquippedBackpackEntryId`), TMap은 미정 #5에서 |
| 3 | `InitialCharges` / `ContainerCapacity`를 DT로 옮길 것인가 | **옮긴다.** `InitState(const FEPItemData&, FEPItemState&)`. 원칙에 예외가 없어진다 |
| 4 | 갱신 알림을 `PostReplicatedReceive` + 스코프 가드로 통일할 것인가 | **한다.** Step 04 함정 #3이 소멸하고, 2-4의 중간 Broadcast가 성립 불가가 된다 |

### 그리고 — Step 03에 검증 수단이 전무하다

Step 00에는 `EP.Item.State`/`EP.Item.Dump`가, Step 01에는 `EP.Loot.RollTable`/`EP.Loot.Respawn`이 있는데, **배낭·서브트리·칸 합산·`bFungible`이 전부 몰려 있는 Step 03에만 커맨드가 없다.** UI는 Step 04다.

Step 03의 완료 조건 12개 중 **7개를 확인할 방법이 지금 없다.**

```
EP.Inv.Dump
  EntryId  Parent  SlotId  ItemId          Charges  SlotSize
  1        -1      -       Bandage         1        1
  2        -1      -       Backpack_Small  0        2
  3        2       -       Weapon_AK74     12       5
  ---
  Body     : 3 / 10       Backpack(2) : 5 / 12
```

`EP.Inv.Add <ItemId>` 도 함께 두면 픽업을 찾아다니지 않고 칸 부족·합치기·배낭 폴백을 바로 재현할 수 있다. 서버 전용으로 가드한다.

---

## 14. 재확인한 엔진 사실

| 사실 | 출처 | 이 검증에서의 쓰임 |
|---|---|---|
| `PostReplicatedReceive(const FPostReplicatedReceiveParameters&)`는 직렬화기에 정의하며 **수신 1회당 1회** 불린다 | `FastArraySerializer.h:517-519`, 호출부 `:1386 :1619 :1736 :1864` | §6 — 항목별 콜백 3종을 대체 |
| `PostReplicatedAdd`는 **항목마다** 불린다 | `FastArraySerializer.h:1163` | §6 — 수신 1회에 갱신 N회 |
| 삭제는 `RemoveAtSwap` — 클라 배열 순서가 실제로 뒤섞인다 | `FastArraySerializer.h:1191` | §2 — 순회 중 제거 금지 / `EntryId` 정렬 근거 보강 |
| `FEPItemData::SellPrice` 기본값 = `100` | `EPItemData.h:43` | §9 — 현금뭉치 판매가 |
| `PreAttributeChange`가 `Ammo`를 `[0, MaxAmmo]`로 클램프 | `EPAttributeSet.cpp:16-17` | Step 05의 "`MaxAmmo` 먼저" 근거 확인 ✅ |
| `AEPWeapon::GetDamage()`가 `WeaponDef->Damage`를 감싸는 기존 패턴 | `EPWeapon.cpp:66` | §7 — `GetMaxAmmo()`가 새 패턴이 아님 |
| `WeaponDef->` 직접 읽기가 **5개 파일** | `EPWeapon` / `EPCombatComponent` / `EPServerSideRewindComponent` / `EPGA_Item_PrimaryUse` / `EPGA_Item_Reload` | §7 — §7-3:920의 "비용이 거의 없다" 반증 |

2차에서 확인해 재검증하지 않은 것: `Class.cpp:974`(자기 재귀만 Fatal) / `:5512`(Warning + 비델타 강등) / `FastArraySerializer.h:218`(내부 struct 델타 기본 활성) / `:441`(`MarkItemDirty` 수동) / `:721-728`(제약 4개) / `:54`(순서 미보장).
