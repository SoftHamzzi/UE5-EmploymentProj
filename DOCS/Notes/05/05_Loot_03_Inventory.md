# Step 03 — Inventory (FastArray 복제 + 컨테이너별 칸 합산 + 배낭 + 버리기)

> 마스터 기획: `05_Loot_DOCS.md` (§4-1, §4-6, §4-7)
> 선행: `05_Loot_02_Interaction.md` — `OnInteract()`가 이 단계에서 실제 삽입으로 바뀐다

---

## 목표

주운 아이템이 서버 권한으로 보관되고, 소유 클라이언트에만 델타 복제된다. G로 버리면 픽업이 되돌아 나온다. **배낭을 매면 별도 칸이 열린다.**

**완료 조건**

- [ ] 주운 아이템이 인벤토리에 들어가고 클라이언트에 복제된다
- [ ] **붕대 3개를 주우면 엔트리가 3개**다 (스택되지 않음)
- [ ] **현금뭉치 둘을 주우면 엔트리 1개, `Charges`가 합산**된다 (`bFungible`)
- [ ] 칸이 모자란 상태로 줍기를 시도하면 **아무것도 안 들어가고 픽업이 그대로 남는다**
- [ ] **가방이 꽉 차도 현금뭉치·탄약상자는 들어간다** (칸이 안 늘어나므로)
- [ ] 무기(`SlotSize=5`)를 넣으면 `UsedSlots`가 5 증가한다 — 엔트리 개수가 아니라 **칸 수**로 찬다
- [ ] **배낭을 주우면 자동으로 매지고**, 본체 10칸과 별개로 배낭 칸이 열린다. 본체가 꽉 차도 배낭에는 들어간다
- [ ] **배낭을 버리면 안의 아이템이 같이 나가고, 고아 엔트리가 남지 않는다** (`EP.Inv.Dump`의 `Parent` 열)
- [ ] **배낭을 버렸다 다시 주우면 안의 아이템이 그대로 돌아온다** — `EntryId`는 새로 발급되되 `Parent` 관계가 유지된다
- [ ] 무기를 12/30까지 쏘고 버렸다 다시 주우면 **`Charges`가 12 그대로**다
- [ ] 버린 직후 0.5초 동안 그 픽업이 회색 프롬프트로 표시되고 서버가 거부한다
- [ ] **다른 클라이언트에 내 인벤토리가 복제되지 않는다** (`COND_OwnerOnly` 확인)
- [ ] 아이템을 줍고 버려도 **기존 항목의 `EntryId`가 재번호되지 않는다** (`Dump` 꼬리의 `NextEntryId`)

> **★ 잔탄 12/30 보존은 여기서 확인할 수 없다.** `Charges`를 12로 만들려면 발사가 필요하고 그건 장착 상태여야 하는데, 장착 경로가 Step 05다. `EP.Inv.Add`로 넣은 무기는 항상 만탄이다. **이 단계에서는 "`EP.Inv.Drop` 후 `EP.Loot.List`의 `Charges`가 보존되는가"까지만 본다** — 값 복사가 도는지는 그걸로 증명된다. 12/30은 Step 05 완료 조건이다.

---

## 체크포인트 — 이 단계는 셋으로 나눠 진행한다

**파일을 쪼개지 않는다.** 경계에서 `RemoveEntry`가 정확히 갈라져 한 함수가 두 문서에 적히고, 그게 stale이 세 번 연속 난 원인이다. 대신 **작업을 나눈다** — 완료 조건 13개는 다른 단계 두 개 분량이다.

| | 범위 | 완료 조건 | 멈춰서 검증 |
|---|---|---|---|
| **03-A 코어** | 03-1 · 03-2 · 03-3 · **03-7** · 03-9 | **2~6** | `EP.Inv.Add`로 칸 합산·`bFungible`·`COND_OwnerOnly`. **`RemoveEntry`/`AddSubtree` 없이 컴파일·실행된다** |
| **03-B 배낭** | 03-6 + `GetCapacity(컨테이너)` | 7 | 배낭을 주우면 두 번째 풀이 열리는가. 아직 못 버린다 |
| **03-C 줍기·버리기** | 03-4 · 03-5 | **1, 8~13** | `RemoveEntry` / `AddSubtree` / 캐스케이드 / `Server_DropItem` |

> **★ 함정표의 ★★ 4건 중 3건이 03-C에 몰려 있다.** 위험이 어디 있는지가 이 구분으로 드러난다.

### ★ 분할선이 8차 검수로 두 군데 바뀌었다

- **03-7(알림)이 03-A로 왔다.** `FScopedInventoryNotify`를 `AddItem`·`SetEntryCharges`(둘 다 03-3)가 쓴다 — 정의가 03-C에 있으면 03-A가 컴파일되지 않는다. 그리고 `PostReplicatedReceive`가 없으면 03-A의 "클라이언트에 복제된다"를 `EP.Inv.Dump`로만 보게 되어, **델리게이트가 한 번도 안 돈 채로 Step 04에 넘어간다**
- **완료 조건 1("주운 아이템이 인벤토리에 들어가고")이 03-C로 갔다.** 줍기는 `AEPPickup::OnInteract`(03-4)이고 03-A에는 `EP.Inv.Add`밖에 없다

> **03-A에서는 `RemoveEntry`를 헤더에 선언조차 하지 않는다.** 선언만 하고 정의를 안 하면 링크 에러다 — 03-1의 콜백 함정과 같은 종류다. 헤더에 안 적으면 컴파일러가 잊어준다.

---

## 03-1. 자료구조

```cpp
USTRUCT()
struct FEPInventoryEntry : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY() int32 EntryId       = INDEX_NONE;   // ★ 서버 발급 개체 식별자
    UPROPERTY() int32 ParentEntryId = INDEX_NONE;   // ★ 담고 있는 컨테이너. NONE이면 본체
    UPROPERTY() FName SlotId;                        // 부착물일 때만. Step 03에선 항상 None
    UPROPERTY() FName ItemId;
    UPROPERTY() FEPItemState State;                  // 개체 상태를 값으로 내장 (Step 00)

    // ★ 항목 단위 콜백을 선언하지 않는다 — 아래 참조. 선언만 하면 링크 에러다
};

USTRUCT()
struct FEPInventoryList : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY() TArray<FEPInventoryEntry> Items;

    UPROPERTY(NotReplicated) TObjectPtr<UEPInventoryComponent> Owner;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
    { return FFastArraySerializer::FastArrayDeltaSerialize<FEPInventoryEntry, FEPInventoryList>(Items, DeltaParms, *this); }
};

template<> struct TStructOpsTypeTraits<FEPInventoryList>
    : public TStructOpsTypeTraitsBase2<FEPInventoryList>
{ enum { WithNetDeltaSerializer = true }; };
```

### ★ 파일 위치 — `FEPInventoryEntry`만 전용 헤더로 뺀다

**`Public/Inventory/EPInventoryTypes.h` (신규).** `FEPInventoryList`와 컴포넌트는 `EPInventoryComponent.h`에 남는다.

이유는 **`AEPPickup`이 엔트리 배열을 들어야 하기 때문**이다(03-4의 `Payload`).

| 후보 | 문제 |
|---|---|
| `Types/EPTypes.h`로 내림 | 거의 모든 파일이 이걸 include한다(트레이스 채널 상수·SSR 스냅샷). `FFastArraySerializerItem` 상속이라 `Net/Serialization/FastArraySerializer.h`가 전투·애님까지 전부 따라 들어간다. 그리고 이건 **복제 계층 타입**이지 공용 스칼라가 아니다 |
| `EPInventoryComponent.h`에 둠 (Lyra 방식) | Lyra의 픽업은 엔트리를 안 든다(`FInventoryPickup`은 인스턴스 포인터 배열이다). 우리는 `Loot/` → 컴포넌트 헤더 **전체** 의존이 생긴다 |
| **전용 헤더** ✅ | `AEPPickup`이 필요한 것은 엔트리뿐이다 |

> **`Build.cs`는 손댈 필요가 없다.** `NetCore`가 `Engine`의 **Public** 의존이라(`Engine.Build.cs:85`) 전이로 들어온다. Lyra가 `LyraGame.Build.cs:57`에서 Private에 명시한 것은 관례이지 요구가 아니다.

### ★★ 항목 단위 콜백을 선언하지 않는다 — 선언만 하면 **링크 에러**다

기반 `FFastArraySerializerItem`이 인라인 no-op을 갖고 있고, 호출은 무조건 나간다.

```cpp
// FastArraySerializer.h:341, 349, 356 — 기반의 빈 정의
inline void PreReplicatedRemove (const struct FFastArraySerializer&) { }
inline void PostReplicatedAdd   (const struct FFastArraySerializer&) { }
inline void PostReplicatedChange(const struct FFastArraySerializer&) { }

// :1139, :1163, :1174 — 조건 없이 불린다
Items[idx].PreReplicatedRemove(ArraySerializer);
Items[idx].PostReplicatedAdd(ArraySerializer);
Items[idx].PostReplicatedChange(ArraySerializer);
```

**파생 struct가 같은 이름을 선언하면 기반 정의를 가린다(name hiding).** 위 호출이 파생 쪽으로 붙고, 정의가 없으면 컴파일은 통과한 뒤 **링크 시점에 unresolved external**이 난다. 셋 중 일부만 정의해도 같다 — **"선언해두고 나중에 채운다"가 성립하지 않는 자리다.**

아무것도 선언하지 않으면 기반의 no-op이 불려서 아무 일도 안 일어난다. **그게 우리가 원하는 것이다** — 알림은 03-7의 `PostReplicatedReceive` 하나로 간다. 그쪽만 진짜 컨셉 검사라(`:533-537` → `:701-710`) **정의하면 불리고 안 하면 안 불린다.**

> **Lyra도 아이템에는 안 둔다.** `FLyraInventoryEntry`가 정의하는 것은 `GetDebugString()` 하나뿐이고, 셋은 직렬화기 쪽에 배열 단위 시그니처로 있다. Lyra가 그걸 정의하는 이유(항목마다 개수 델타 메시지를 쏜다 — `LastObservedCount` 필드까지 둔다)는 **우리에게 없다.** 03-7이 수신 1회당 전체 갱신으로 확정했다.

### ★ `FFastArraySerializerItem`의 복사는 복제 ID를 승계하지 않는다

```cpp
// FastArraySerializer.h:302-323 — 복사 생성자와 operator= 둘 다
ReplicationID = ReplicationKey = MostRecentArrayReplicationKey = INDEX_NONE;
```

**좋은 쪽:** 스냅샷(`RemoveEntry`의 `OutRemoved`)·픽업 `Payload`·`FindEntry` 값 복사가 **전부 안전하다.** 복제 ID가 따라붙지 않는다.

**나쁜 쪽: 살아있는 배열 원소에 `E = Other;`로 통째 대입하면 안 된다.** 송신 시 `:925-928`이 `INDEX_NONE`을 보고 새 ID를 발급하고(`:445` `++IDCounter`), 그 결과 **수신 측은 삭제 + 추가로 본다**(`:950-957` New! / `:961-976` 옛 ID 삭제 / `:1139`·`:1163` 콜백).

- **UI는 안 깨진다** — 식별자가 `ReplicationID`가 아니라 `EntryId`이고(아래), 03-7이 어차피 수신 1회당 전체 갱신이다
- **실제 손실은 델타다.** 그 엔트리에서 내부 struct 델타(`:218`)의 이득이 사라져 **엔트리 전체가 다시 나간다**
- 나중에 항목 단위 콜백을 붙이면 그때 거짓말한다

→ **수정은 언제나 필드 단위로.** `SetEntryCharges`의 `E.State.Charges = ...`(03-3)가 그 형태다. **위험한 사람은 나중에 write-back이나 이동을 짜는 사람이므로 지금 적어둔다.**

### ★ `EntryId`가 반드시 필요한 이유

`FFastArraySerializer`는 항목을 **ReplicationID로 식별**할 뿐, 클라이언트 배열의 **순서가 서버와 같다는 보장이 없다.** 엔진 주석 원문:

> *"the \*order\* of the list is not guaranteed to be identical between client and server in all cases."*
> — `FastArraySerializer.h:54`

배열 인덱스로 UI를 그리거나 RPC를 보내면 **아이템을 하나 줍거나 버릴 때마다 기존 항목이 자리를 바꾸고**, 제거·정렬 중에 도착한 요청이 엉뚱한 아이템을 버린다. 산발적으로 나고 서버는 멀쩡해서 재현이 어려운 부류다.

- `AddItem()`이 `NextEntryId++`로 발급한다
- 제거해도 **다른 엔트리의 `EntryId`는 재번호되지 않는다** (그래서 드랍·장착 RPC 파라미터로 안전하다)
- UI는 `EntryId` 오름차순으로 그린다 — 새 항목은 항상 끝에 붙고 기존 항목은 움직이지 않는다
- **`ParentEntryId`가 이 번호를 참조한다.** 재번호하면 배낭 속 아이템이 엉뚱한 곳에 매달린다

> **로드맵 5단계 세이브 주의:** `NextEntryId`도 함께 저장해야 한다. 안 하면 로드 후 1부터 재발급해 기존 엔트리와 충돌하고, 하필 `ParentEntryId`를 오염시켜 **아이템이 엉뚱한 컨테이너에 들어간다.**

### ★ `ParentEntryId` — 배낭과 부착물이 같은 구조다

```
EntryId=1  Parent=NONE  Bandage         1칸    ← 본체 (10칸)
EntryId=2  Parent=NONE  Backpack_Small         ← 매고 있는 배낭
EntryId=3  Parent=2     MedKit          3칸    ← 배낭 내부 (배낭 용량)
EntryId=4  Parent=2     Weapon_AK74     5칸
EntryId=5  Parent=4     Scope_4x   SlotId=Optic  ← 배낭 속 총의 부착물 (§7-3, 추후)
```

- **중첩 struct가 아니라 부모 참조인 이유:** 자기 타입 재귀는 `Class.cpp:974`에서 **Fatal**이고, 이종 중첩은 프로퍼티 델타를 잃는다. 무엇보다 **배낭과 부착물이 같은 표현을 쓰게 되는 것**이 크다 — 위 `EntryId=5`가 특수 케이스 없이 성립한다
- **`SlotId`를 지금 넣는 이유:** 용량 합산이 "수납된 자식"과 "부착된 자식"을 구분해야 하므로(부착물은 칸을 안 먹는다), 필드가 없으면 판정식을 §7-3에서 다시 써야 한다. `ParentEntryId`가 어차피 들어오므로 함께 넣는 편이 싸다. **Step 03에서는 항상 `NAME_None`이다**

### ★ 상태를 엔트리에 넣었을 때의 이득과 함정

**이득 — 내부 struct 델타가 기본으로 켜져 있다.**

> *"Delta Serialization for inner structs is now enabled by default. ... only sending properties that changed exactly like the standard replication path."*
> — `FastArraySerializer.h:218`

`Entry.State.Charges`가 30 → 29로 바뀌면 엔트리 전체가 아니라 **`Charges`만** 나간다. 상태를 내장한 선택이 대역폭 면에서 손해가 아니다.

**함정 — `MarkItemDirty`는 여전히 수동이다** (`FastArraySerializer.h:441`).

상태가 배열 밖(서버 전용 인스턴스)에 있던 설계에서는 이 질문 자체가 없었다. 이제 `Entry.State`를 직접 쓰고 `MarkItemDirty(Entry)`를 안 부르면 **조용히 복제가 안 된다.** Step 05의 write-back 경로(`UnequipWeapon`)가 정확히 여기에 해당한다.

**제약** (`FastArraySerializer.h:721-728`) — 아이템 배열은 직렬화기 안의 **최상위 `UPROPERTY`** 여야 하고, `RepSkip` 금지, **복제되는 아이템 배열은 하나뿐**, 직렬화기와 배열 모두 정적 배열 안에 중첩 금지. 전부 자명하게 충족되지만 명시해 둔다.

---

## 03-2. `UEPInventoryComponent`

```cpp
UCLASS(meta = (BlueprintSpawnableComponent))
class EMPLOYMENTPROJ_API UEPInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEPInventoryComponent();

    // ── 삽입 ────────────────────────────────────────────────
    // 반환: 발급된 EntryId. INDEX_NONE이면 실패 (칸 부족 / 데이터 없음)
    // ★ 반환값을 if()로 검사하지 말 것 — INDEX_NONE(-1)은 truthy다
    int32 AddItem(int32 Container, FName ItemId, const FEPItemState& InState);

    // 픽업이 든 서브트리를 통째로 넣는다. 반환은 루트의 새 EntryId (03-4)
    int32 AddSubtree(int32 Container, const TArray<FEPInventoryEntry>& In);

    // ── 조회 ────────────────────────────────────────────────
    // ★ 포인터가 아니라 값으로 돌려준다 (아래 참조)
    bool  FindEntry(int32 EntryId, FEPInventoryEntry& Out) const;

    // ★★ 컨테이너 안에서만 찾는다. 인자를 빼면 배낭 속 현금이 본체 현금과 합쳐진다 (아래)
    int32 FindFungibleEntryId(int32 Container, FName ItemId) const;   // 없으면 INDEX_NONE

    int32 GetUsedSlots(int32 Container) const;            // 그 컨테이너의 Σ SlotSize
    int32 GetCapacity(int32 Container) const;             // 본체면 MaxSlots, 아니면 DT
    bool  CanFit(int32 Container, FName ItemId) const;    // 삽입 판정의 유일한 지점
    bool  IsFungible(FName ItemId) const;                 // DT의 bFungible

    int32 GetEquippedEntryId()   const { return EquippedEntryId; }
    int32 GetEquippedBackpack()  const { return EquippedBackpackEntryId; }

    // UI가 순회할 읽기 전용 뷰. 반환 참조는 그 프레임 안에서만 유효하다
    const TArray<FEPInventoryEntry>& GetEntries() const { return Entries.Items; }

    // ── 수정 (원시 엔트리를 내보내지 않으므로 이것들이 유일한 통로) ──
    void SetEntryCharges(int32 EntryId, int32 NewCharges);  // ★ 유일한 쓰기 지점
    void AddEntryCharges(int32 EntryId, int32 Delta);       //   Set에 위임. 음수면 차감
    void SetEquippedEntryId(int32 EntryId);                 // Step 05. 알림을 쏜다 (아래)
    void Server_EquipBackpack(int32 EntryId);               // 03-6

    // ★ 제거된 서브트리를 전위 순회로 돌려준다. In[0]이 루트 (03-5 계약)
    //   재귀 파라미터를 노출하지 않는다 — 아래 참조
    bool RemoveEntry(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved = nullptr);

    // 획득 직후 호출. 컨테이너이고 배낭 슬롯이 비어 있을 때만 맨다 (03-6)
    void TryAutoEquipBackpack(int32 EntryId);

    // ★ 상태 변경 RPC의 유일한 게이트. 죽음·시전 확인이 여기 한 곳에 있다 (03-5)
    bool CanMutateInventory() const;

    UFUNCTION(Server, Reliable)
    void Server_DropItem(int32 EntryId);                  // 03-5

    DECLARE_MULTICAST_DELEGATE(FOnInventoryChanged);
    FOnInventoryChanged OnInventoryChanged;               // UI가 구독 (Step 04)

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;

    UPROPERTY(Replicated) FEPInventoryList Entries;
    UPROPERTY(Replicated) int32 MaxSlots = 10;                        // 본체 용량 (GAME.md)
    UPROPERTY(Replicated) int32 EquippedEntryId          = INDEX_NONE; // Step 05
    UPROPERTY(Replicated) int32 EquippedBackpackEntryId  = INDEX_NONE; // 03-6

private:
    // ★ 재귀 본체. bIsRoot를 밖에서 넘길 문법이 없어야 계약이 지켜진다 (아래)
    bool RemoveEntryInternal(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved, bool bIsRoot);
    void RemoveChildrenRecursive(int32 ParentId, TArray<FEPInventoryEntry>* OutRemoved);
    void RemoveSelf(int32 EntryId);                  // 배열에서 빼고 MarkArrayDirty
    bool ContainsEntry(int32 EntryId) const;

    // ★ 번호 발급 · 삽입 · MarkItemDirty의 유일한 지점.
    //   칸 검사도 bFungible 합치기도 하지 않는다 — 그건 호출자 몫이다
    int32 InsertEntry(int32 Parent, FName ItemId, const FEPItemState& State, FName SlotId);

    // 캐릭터 전방 100cm + 바닥 트레이스. 실패 시 발밑 (03-5)
    AEPPickup* SpawnPickupInFront() const;

    // 서브시스템은 캐시하지 않고 매번 조회한다 (아래)
    const UEPItemDefinitionSubsystem* Defs() const;

    int32 NextEntryId = 1;        // 서버 전용. 복제하지 않는다

    int32 NotifyDepth = 0;        // 중간 알림을 막는 스코프 가드 (03-7)
    friend struct FScopedInventoryNotify;
};
```

```cpp
UEPInventoryComponent::UEPInventoryComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
    Entries.Owner = this;        // ★★ 빠뜨리면 클라 UI가 영원히 갱신되지 않는다
}
```

> **★★ `Entries.Owner = this;`** — 03-1이 `FEPInventoryList::Owner`를 선언하고 Step 04가 `PostReplicated*` → `Owner->OnInventoryChanged`를 전제하는데, 이 한 줄이 없으면 **콜백이 델리게이트에 닿지 못한다.** 증상은 "서버는 정상인데 클라 인벤토리 UI가 영원히 갱신되지 않는다"이고 원인이 UI나 복제로 보여서 엉뚱한 데를 판다. **Step 04 전체가 이 줄에 걸려 있다.**

> **`EndPlay` 오버라이드가 없다.** 이전 설계는 여기서 엔트리의 핸들을 전부 `Destroy()`해야 했고, 그 때문에 "사망 시 드랍은 반드시 `EndPlay`보다 먼저"라는 순서 규칙이 따라붙었다. 상태가 값이므로 컴포넌트가 파괴되면 같이 사라진다 — **정리할 것도, 지킬 순서도 없다.**

> **`NextEntryId`를 복제하지 않는다.** 클라는 발급하지 않고 받은 `EntryId`를 읽기만 한다. 복제하면 서버 내부 상태가 밖으로 샌다.
> **★ 로드맵 5단계 세이브에는 반드시 넣는다** — `NextEntryId` / `EquippedEntryId` / `EquippedBackpackEntryId` 셋 다. `NextEntryId`를 빠뜨리면 로드 후 1부터 재발급해 기존 엔트리와 충돌하고, 하필 `ParentEntryId`를 오염시켜 **아이템이 엉뚱한 컨테이너에 들어간다.**

#### `Defs`를 멤버로 캐시하지 않는다

`UEPItemDefinitionSubsystem`은 `UGameInstanceSubsystem`이다. 멤버 포인터로 들고 있으면 00-5가 `FindData()` 반환 포인터의 장기 보관을 금지한 것과 **같은 종류의 질문**("언제까지 유효한가")이 생긴다. 조회는 `TMap` 룩업 한 번이라 비용이 없다.

```cpp
const UEPItemDefinitionSubsystem* UEPInventoryComponent::Defs() const
{
    const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    return GI ? GI->GetSubsystem<UEPItemDefinitionSubsystem>() : nullptr;
}
```

#### `UEPCombatComponent`를 멤버로 두지 않는다 — 헤더 순환

인벤토리가 전투를 부르고(`RemoveEntry` → `UnequipWeapon`) 전투가 인벤토리를 부른다(`UnequipWeapon` → `AddEntryCharges`). 멤버로 두면 **헤더가 서로를 알아야 한다.**

```cpp
// .cpp 안에서만 캐릭터를 경유한다
if (UEPCombatComponent* Combat = GetOwner<AEPCharacter>()->GetCombatComponent())
    Combat->UnequipWeapon();
```

기존 관례와 같다 — `UEPCombatComponent`가 `GetOwnerCharacter()`를 거쳐 `AEPPlayerState`에 닿는 방식(`EPCombatComponent.cpp:171-172`)이 이미 그 모양이다. **두 컴포넌트가 서로를 부르는 것 자체는 문제가 아니다**(전투는 이미 `AEPPlayerState`·`UEPAttributeSet`·`AEPWeapon`을 가로질러 부른다). 문제는 헤더 결합이고, 이 방식이면 생기지 않는다.

### ★★ 불변식을 문서가 아니라 함수로 강제한다

이 단계의 위험은 셋이다 — **엔트리를 지운 뒤 잔탄을 write-back하면 소실**, **수정 후 `MarkItemDirty` 누락**, **컨테이너 제거 시 자식이 고아로 남음**. 셋 다 증상이 엉뚱하고 재현이 어렵다.

**규칙으로 남기면 안 된다.** 여러 문서에 적히고 한쪽만 고쳐지는 사고가 반드시 난다. **형태로 막는다.**

```cpp
// public — 진입점. 재귀 파라미터가 없다
bool UEPInventoryComponent::RemoveEntry(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved)
{
    return RemoveEntryInternal(EntryId, OutRemoved, /*bIsRoot=*/true);
}

// private — 재귀 본체
bool UEPInventoryComponent::RemoveEntryInternal(int32 EntryId,
                                                TArray<FEPInventoryEntry>* OutRemoved,
                                                bool bIsRoot)
{
    if (!GetOwner()->HasAuthority()) return false;
    if (!ContainsEntry(EntryId))     return false;      // 존재 확인만. 값은 아래서 뜬다

    FScopedInventoryNotify Guard(this);                 // 중간 알림 차단 (03-7)

    // ① write-back 먼저 — 스냅샷은 반드시 이 뒤에 뜬다
    if (EntryId == EquippedEntryId)
    {
        if (UEPCombatComponent* C = GetOwner<AEPCharacter>()->GetCombatComponent())
            C->UnequipWeapon();                         // write-back만 한다
        EquippedEntryId = INDEX_NONE;                   // ★ 번호는 여기서 비운다 (아래)
    }
    if (EntryId == EquippedBackpackEntryId)
        EquippedBackpackEntryId = INDEX_NONE;           // 죽은 번호를 남기지 않는다

    // ② 스냅샷을 자식보다 **먼저** 담는다. 루트는 컨테이너 소속을 끊는다
    if (OutRemoved)
    {
        FEPInventoryEntry Snapshot;
        if (FindEntry(EntryId, Snapshot))
        {
            if (bIsRoot) Snapshot.ParentEntryId = INDEX_NONE;
            OutRemoved->Add(Snapshot);
        }
    }

    // ③ 자신을 먼저 제거한다 — 그래야 캐스케이드가 자기를 다시 못 찾는다
    RemoveSelf(EntryId);

    // ④ 자식은 그 다음. 부모가 이미 배열에서 빠져 사이클이 성립하지 않는다
    RemoveChildrenRecursive(EntryId, OutRemoved);
    return true;
}

void UEPInventoryComponent::RemoveChildrenRecursive(int32 ParentId,
                                                    TArray<FEPInventoryEntry>* OutRemoved)
{
    // ★ 자식 목록을 먼저 뜬다 — 순회 중 배열이 바뀐다
    TArray<int32> Children;
    for (const FEPInventoryEntry& E : Entries.Items)
        if (E.ParentEntryId == ParentId) Children.Add(E.EntryId);

    for (int32 Id : Children)
        RemoveEntryInternal(Id, OutRemoved, /*bIsRoot=*/false);  // ★ 자식은 Parent를 보존한다
}
```

#### ★★ `bIsRoot`를 public에 두면 문서가 세 번 말한 계약이 깨진다

초안은 `RemoveEntry(Id, Out, bool bIsRoot = true)` 하나였다. **그러면 밖에서 이렇게 부를 수 있다.**

```cpp
RemoveEntry(Id, &Out, /*bIsRoot=*/false);   // 루트 정규화 생략 → AddSubtree가 루트를 못 찾는다
```

증상이 03-4가 **"흔한 경로"** 라고 직접 경고한 것(배낭 속 무기 버리기)과 **똑같다.** 이 문서는 계약을 *"규율이 아니라 형태로 막는다"* 고 세 곳에서 강조해 놓고 **그 형태를 깰 파라미터를 시그니처에 열어뒀던 것**이다.

**public/private 분리로 `false`를 넘길 문법이 사라진다.** 별도 함수 둘(`RemoveSubtreeAsRoot`/`AsChild`)로 나누는 대안도 되지만, **재귀가 자기 자신을 부르는 구조가 함정 3b(자식마다 장착 검사)를 보장하는 핵심**이므로 재귀 대상을 하나로 유지하는 이 형태가 낫다.

#### ★★ `EquippedEntryId`도 `RemoveEntry`가 비운다 — `UnequipWeapon`에 맡기지 않는다

배낭 번호만 비우고 무기 번호는 Step 05의 `UnequipWeapon`에 암묵적으로 맡겼던 것이 초안이다. **대칭으로 맞춘다.** 근거가 취향이 아니라 셋이다.

1. **책임이 다르다.** `UnequipWeapon`은 **다른 컴포넌트**(`UEPCombatComponent`)의 함수다. 인벤토리 필드를 비우는 책임이 거기 있으면 헤더 결합은 없어도 **의미 결합**이 생겨, 03-2가 헤더 순환을 피하려고 만든 구조가 무의미해진다
2. **`UnequipWeapon`은 제거 없이도 불린다.** Step 05의 교체·사망 경로가 그렇다. 거기서 번호를 비우는 것은 맞지만 **`RemoveEntry`가 그걸 믿을 근거는 없다**
3. **캐스케이드에서 여러 번 불릴 수 있다.** 배낭 속 무기가 장착 중이면 자식 노드에서 불린다. 번호를 비우는 주체가 하나여야 순서 논쟁이 안 생긴다

> **★ 이 분기는 Step 03 내내 항상 거짓이다** — `EquippedEntryId`를 세팅하는 경로가 Step 05에 있다. `ensureMsgf`를 넣어도 울릴 일이 없으므로 **방어를 코드로 넣을 자리가 아니라 완료 조건으로 넘길 자리다.** `05_Loot_05_Equipment.md` 완료 조건에 "배낭 속 무기를 장착한 채 배낭 버리기"를 추가했다.

#### ★★ 네 단계의 순서가 각각 하나씩 막는다

| 단계 | 막는 것 |
|---|---|
| ① write-back → ② 스냅샷 | **잔탄 소실.** 선두에서 뜬 값을 쓰면 write-back 전이라 만탄이 나간다 |
| ② 루트를 먼저, `Parent = INDEX_NONE` | **`AddSubtree`가 못 읽는 배열.** 아래 계약 |
| ③ 자기 제거를 캐스케이드 **앞**에 | **자기 부모 사이클의 무한 재귀.** `X.Parent == X`인 데이터 오류에서 스택 오버플로 |
| ④ 캐스케이드 마지막 | 자식이 부모의 write-back 결과에 영향받지 않는다 |

#### ★ `RemoveEntry` ↔ `AddSubtree` 계약

**두 함수를 각각 옳게 고쳐도 계약이 어긋나면 둘 다 컴파일되고 배낭 하나짜리 테스트도 통과한다.** 명시한다.

> **`OutRemoved`는 전위 순회다.** `In[0]`이 항상 루트이고, 루트의 `ParentEntryId`는 **`INDEX_NONE`으로 정규화**된다. 자식의 `ParentEntryId`는 **원본 번호를 보존**한다.

이 계약이 없으면 `AddSubtree`가 이렇게 깨진다.

- **후위 순회면** 첫 원소(자식)의 부모가 아직 `OldToNew`에 없어 재매핑이 첫 줄에서 실패한다
- **루트 정규화가 없으면** 배낭 **안**에 있던 무기를 버릴 때 `ParentEntryId`가 배낭 번호라 `INDEX_NONE`인 원소가 하나도 없다 → 루트를 못 찾는다. 본체 10칸/무기 5칸이라 **흔한 경로다**

| 위험 | 막는 형태 |
|---|---|
| write-back 순서 | **`RemoveEntry()`가 제거된 서브트리를 반환한다.** 스냅샷을 얻는 유일한 방법이 제거하는 것이므로 **순서를 뒤집는 게 문법적으로 불가능**해진다 |
| 자식 고아 | **`RemoveEntry()`가 유일한 제거 지점.** 캐스케이드가 자기 자신을 재귀 호출하므로 장착 검사·write-back이 **노드마다** 돈다 |
| 계약 우회 | **`bIsRoot`가 private에만 있다.** 루트 정규화를 건너뛸 문법이 없다 |
| `MarkItemDirty` 누락 | **원시 엔트리를 밖으로 내보내지 않는다.** 수정은 `AddEntryCharges()`로만, 삽입은 `InsertEntry()`로만 |
| 중간 Broadcast | **스코프 가드** (03-7) |

> **★ 장착 검사가 자기 자신에만 걸리면 안 되는 이유:** 본체가 10칸이고 무기가 5칸이라(00-8) 무기가 배낭에 들어가는 일이 흔하다. 그 상태로 배낭을 버리면 `EntryId`(배낭) ≠ `EquippedEntryId`(무기)라 **unequip이 안 불리고**, 자식으로 지워지면서 write-back이 소실되며, `EquippedEntryId`가 **죽은 번호**를 가리킨 채 `AEPWeapon` 액터만 손에 남는다. 위 재귀 구조가 이걸 원천 차단한다.

> **★ 순회 중 수정 금지가 여기서 실제 문제다.** FastArray 삭제는 `RemoveAtSwap`을 쓴다(`FastArraySerializer.h:1191`). 원본을 순회하며 지우면 인덱스가 뒤에서 앞으로 튀어 **일부 자식을 건너뛴다.** 증상은 "가끔 고아 엔트리가 남는다"이고 재현이 어렵다.

- **원시 엔트리를 밖으로 내보내지 않는다.** 수정은 `SetEntryCharges()` / `AddEntryCharges()`로만 — 그러면 호출자가 `MarkItemDirty`를 잊을 방법이 없다
- Step 05는 "순서를 지켜라"가 아니라 **"`RemoveEntry()`가 보장한다"** 한 줄만 적는다

### ★ §7-1 월드 컨테이너가 이 두 함수로 **이미** 성립한다

```
Container->RemoveEntry(Id, &Sub)   →   MyInv->AddSubtree(INDEX_NONE, Sub)
```

`RemoveEntry`의 반환 계약(전위 순회 / `In[0]`이 루트 / 루트 `Parent = INDEX_NONE`)은 **`EntryId` 공간이 아니라 배열 모양에만 의존하므로 컴포넌트 경계를 넘어서도 유효하다.** `AddSubtree`의 `OldToNew` 재매핑이 원래 *"번호 공간이 다르다"* 를 전제로 만들어졌고, **버린 배낭을 되줍는 것이 이미 그 경우다.**

- **`TransferItem` / `Server_MoveItem` 일반형을 지금 만들지 않는다** — 소비자가 없다 (CLAUDE.md §2). **사실만 적고 함수는 만들지 않는다**
- 안 적으면 §7-1에서 세 번째 경로가 생기고, 그건 **`RemoveEntry` 4단계 순서를 우회할 자리를 새로 만든다**

> §7-1이 실제로 올 때 생기는 것은 `UEPInventoryComponent`의 RPC 하나이고 시그니처가 `Server_TakeFromContainer(AActor* Container, int32 EntryId)`가 된다. **그때가 03-5의 "되돌아갈 신호 ⓒ"를 점검할 시점이다** — 대상 액터가 파라미터에 들어오는 순간 상호작용 쪽 성격이 섞이기 시작한다.

> 지금 남은 위험은 성능도 확장성도 아니라 **코드로 강제 가능한 불변식을 규율에 맡기는 것**이다. 1인 프로젝트에서도 3개월 뒤에는 깨진다.

### ★ 복제 조건은 `COND_OwnerOnly`

```cpp
void UEPInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME_CONDITION(UEPInventoryComponent, Entries,                 COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UEPInventoryComponent, MaxSlots,                COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UEPInventoryComponent, EquippedEntryId,         COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UEPInventoryComponent, EquippedBackpackEntryId, COND_OwnerOnly);
}
```

조건 없이 복제하면 **모든 클라이언트가 모든 플레이어의 인벤토리를 받는다.** 패킷만 봐도 상대 소지품을 아는 치트가 되고, 8인 매치면 대역폭도 8배다. GAME.md의 "정보 은폐" 기조와 정면으로 어긋난다. 프로젝트 관례와도 일치한다 — `PlayerState::Kills` / `Extracted`가 이미 `COND_OwnerOnly`다.

> 나중에 "시체 루팅"이 생기면 남의 인벤토리를 봐야 하는데, 그때 **조건을 푸는 게 아니라** 시체 액터가 자기 인벤토리를 별도로 노출하는 방식으로 간다. 살아있는 플레이어의 가방은 끝까지 소유자 전용이다.

`MaxSlots`도 복제한다 — UI가 `UsedSlots / MaxSlots` 게이지를 그려야 한다.

> **`State`를 소유 클라에 복제해도 되는 이유:** `COND_OwnerOnly`라 자기 가방뿐이다. 남의 잔탄이 새어나가는 지점은 **바닥 픽업**이고 거기서 막았다 (Step 01). 그리고 이 복제 덕분에 **가방 속 두 번째 소총의 잔탄을 UI에 표시할 수 있다** — 서버 전용 인스턴스 방식에서는 불가능했던 것이다.

### ★ 엔트리 포인터를 밖으로 내보내지 않는다

`FindEntry`가 `const FEPInventoryEntry*`를 돌려주면 **`Entries.Items` 내부를 가리킨다.** `AddItem` / `DropItem` / 복제 수신(`PostReplicatedAdd`)이 배열을 재할당하는 순간 그 포인터는 댕글링한다. Step 00에서 `FEPItemData*` 행 포인터를 금지한 것과 **정확히 같은 문제**다.

- `FindEntry`는 **값으로 복사해 돌려준다.** 엔트리는 20바이트 남짓이라 복사 비용이 없다
- `GetEntries()`는 UI 순회용으로 참조를 노출하되, **그 프레임 안에서만 유효**하다. 순회 중에 `AddItem`을 부르지 않는다
- 엔트리를 수정할 때는 컴포넌트 내부에서 인덱스로 접근하고 `MarkItemDirty()`를 호출한다. 밖으로 비-const 포인터를 내보내면 `MarkItemDirty` 호출을 강제할 수 없다

### 부착 위치는 Character

`AEPCharacter` 생성자에 `CombatComponent`/`RewindComponent` 옆으로 추가한다.

타르코프식은 **사망 시 소지품 손실**이 규칙이므로(GAME.md 코어 루프) Character와 수명을 같이하는 것이 규칙과 일치한다. PlayerState에 두면 사망 시 명시적으로 비워야 하고 "비우는 걸 깜빡하는" 버그의 자리가 생긴다.

> ASC를 PlayerState에 둔 것과 반대지만 이유가 다르다. ASC는 리스폰 후에도 **보존되어야** 해서 PlayerState였다. 인벤토리는 반대로 **소실되어야** 한다.

---

## 03-3. `AddItem` — 컨테이너별 칸 합산

```cpp
int32 UEPInventoryComponent::AddItem(int32 Container, FName ItemId,
                                     const FEPItemState& InState)
{
    if (!GetOwner()->HasAuthority()) return INDEX_NONE;   // ★ check()가 아니다 — 아래

    const FEPItemData* Data = Defs() ? Defs()->FindData(ItemId) : nullptr;
    if (!Data) return INDEX_NONE;

    // ★ 균질 아이템은 합친다. 칸이 안 늘어나므로 칸 검사보다 앞에 온다
    if (Data->bFungible)
    {
        const int32 Id = FindFungibleEntryId(Container, ItemId);   // ★★ 컨테이너 안에서만
        if (Id != INDEX_NONE)
        {
            AddEntryCharges(Id, InState.Charges);
            return Id;
        }
    }

    if (!CanFit(Container, ItemId)) return INDEX_NONE;    // 전부 아니면 전무

    return InsertEntry(Container, ItemId, InState, NAME_None);
}

// ★ 번호 발급 · 삽입 · MarkItemDirty의 유일한 지점 (03-2)
int32 UEPInventoryComponent::InsertEntry(int32 Parent, FName ItemId,
                                         const FEPItemState& State, FName SlotId)
{
    FScopedInventoryNotify Guard(this);

    const int32 NewId = NextEntryId++;       // ★ 참조보다 먼저 뜬다
    FEPInventoryEntry& E = Entries.Items.AddDefaulted_GetRef();
    E.EntryId       = NewId;
    E.ParentEntryId = Parent;
    E.SlotId        = SlotId;
    E.ItemId        = ItemId;
    E.State         = State;                 // ★ 값 복사. 잔탄이 여기서 보존된다

    Entries.MarkItemDirty(E);
    return NewId;                            // ★ E.EntryId가 아니라 미리 뜬 값
}

bool UEPInventoryComponent::CanFit(int32 Container, FName ItemId) const
{
    const FEPItemData* Data = Defs() ? Defs()->FindData(ItemId) : nullptr;
    if (!Data) return false;
    return GetUsedSlots(Container) + Data->SlotSize <= GetCapacity(Container);
}
```

> **칸 판정을 `AddItem`에 인라인으로 다시 쓰지 않는다.** `CanFit`은 `AddSubtree`(03-4)와 상호작용 프롬프트도 쓰므로 판정식이 세 곳에 흩어지면 반드시 어긋난다.

> **가드가 있으면 알림이 `return` 뒤에 나가므로 `E`가 댕글링할 창이 없다.** 그래도 값을 미리 뜨는 형태는 유지한다 — 나중에 누가 가드를 빼도 안전하다.

### ★★ `FindFungibleEntryId`에 컨테이너 인자가 없으면 돈이 컨테이너를 탈출한다

초안은 `FindFungibleEntryId(FName ItemId)`였다. 그러면 **배낭 속 현금뭉치가 본체 현금과 합쳐진다** — 어디에 있든 첫 번째를 찾기 때문이다.

**증상이 거의 없는 것이 이 버그의 성질이다.** `Charges` 합산은 칸을 늘리지 않으므로 UI에서 총액이 맞고, 아무 에러도 안 난다. **배낭을 벗거나 버리는 순간 돈이 딸려 나간다.**

`AddSubtree`가 자식마다 `AddItem`을 부르면 안 되는 이유도 여기 있다(03-4).

> **완료 조건 5("가방이 꽉 차도 현금·탄약은 들어간다")는 그대로 성립한다.** 03-4의 본체 → 배낭 2단계가 커버한다 — 본체 조회 실패 → 본체 `CanFit` 실패 → 배낭 조회 성공 → 합산.

```cpp
int32 UEPInventoryComponent::GetUsedSlots(int32 Container) const
{
    const UEPItemDefinitionSubsystem* D = Defs();
    if (!D) return 0;

    int32 Sum = 0;
    for (const FEPInventoryEntry& E : Entries.Items)
    {
        if (E.ParentEntryId != Container) continue;
        if (!E.SlotId.IsNone())             continue;   // 부착물은 칸을 안 먹는다 (§7-3)

        const FEPItemData* Row = D->FindData(E.ItemId);
        if (!ensureMsgf(Row, TEXT("[Inventory] DT에 없는 ItemId: %s"), *E.ItemId.ToString()))
            continue;                                   // ★ 아래 참조
        Sum += Row->SlotSize;
    }
    return Sum;
}

int32 UEPInventoryComponent::GetCapacity(int32 Container) const
{
    if (Container == INDEX_NONE) return MaxSlots;       // 본체 10칸

    FEPInventoryEntry E;
    if (!FindEntry(Container, E)) return 0;

    const UEPItemDefinitionSubsystem* D = Defs();
    const FEPItemData* Row = D ? D->FindData(E.ItemId) : nullptr;
    return Row ? Row->ContainerCapacity : 0;           // ★ DT다 (§4-9 원칙)
}
```

### ★ `FindData()` null이 무증상 버그를 만든다

DT에 없는 `ItemId`가 섞이면 그 아이템은 **칸을 0으로 먹는다.** 결과가 "가방이 무한대"이고 **아무 에러도 안 난다.** `ensure` + 경고 로그가 반드시 필요하다 — 조용히 `continue`하면 밸런싱이 무너진 채로 몇 주가 간다.

### 권한 검사는 `check()`가 아니라 early return

```cpp
if (!GetOwner()->HasAuthority()) return INDEX_NONE;   // ✅ 프로젝트 관례
check(GetOwner()->HasAuthority());                    // ❌ Shipping에서도 크래시한다
```

`CLAUDE.md §Conventions`와 기존 코드가 전부 early return이다. 인벤토리만 다른 방식을 쓸 이유가 없다.

### `UsedSlots`를 캐시하지 않는다

엔트리가 20~30개고 조회가 `TMap` 룩업이라 **수백 나노초**다. 캐시하면 **추가·제거·복제 수신 세 경로를 전부 갱신해야 하고**, 하나만 빠져도 "가방이 안 찼는데 가득 찼다"가 된다. 그 버그가 압도적으로 비싸다.

**클라이언트도 같은 함수를 쓴다.** `COND_OwnerOnly`로 엔트리 전부를 받고 `SlotSize`는 DT 조회로 나오므로 복제할 이유가 없다.

> **성능이 걱정되면 캐시가 아니라 폴링을 없앤다.** UI는 수신 1회당 1회 오는 콜백으로 갱신하므로(03-7) 애초에 매 틱 부르지 않는다.

> **★ 복제 순서 방어가 필요 없는 이유도 여기 있다.** 자식이 부모보다 먼저 도착하면 `GetUsedSlots(INDEX_NONE)`이 그 자식을 안 세고(`ParentEntryId != INDEX_NONE`), 배낭 구획은 아예 안 그려진다(`GetEquippedBackpack()`이 아직 `INDEX_NONE`). **잠깐 안 보이다가 다음 수신에서 저절로 맞는다.** 모든 파생값을 매 갱신마다 처음부터 다시 계산하기 때문이고, 캐시했다면 "부모 없는 자식이 도착했을 때 캐시를 어떻게 하나"가 진짜 문제가 됐다. **순서 방어 코드를 넣지 마라.**

### ★ 균질 아이템 합치기 — 스택이 아니다

현금뭉치 두 개를 주웠을 때 엔트리가 둘로 늘면 칸만 낭비된다. `FEPItemData::bFungible`이면 **`Charges`를 더한다.**

| 아이템 | 균질? | 근거 |
|---|---|---|
| 현금뭉치 | ✅ | 10,000원 두 뭉치를 구별할 이유가 없다 |
| 탄약상자 | ✅ | 탄종은 `ItemId`가 이미 가른다 (`AmmoBox_545` / `_762`) |
| 무기 | ❌ | `Durability`와 부착물 자식을 갖는다 |
| 탄창 | ❌ | 미정 #1이 `AmmoType`을 붙일 수 있다 |

**스택의 부담이 하나도 안 따라오는 이유는 칸 수가 `Charges`와 무관하기 때문이다.** `SlotSize × Quantity` 계산도, 부분 획득도, 병합 순서 규칙도 성립하지 않는다. 합칠 대상은 최대 하나다.

**부수 이득:** 가방이 꽉 차도 **돈과 탄약은 항상 들어간다** — 부분 획득을 없애며 잃은 완충 장치가 여기서 복원된다.

### ★ 쓰기 지점을 하나로 — `Set`이 진짜고 `Add`는 위임이다

```cpp
void UEPInventoryComponent::SetEntryCharges(int32 EntryId, int32 NewCharges)
{
    if (!GetOwner()->HasAuthority()) return;

    for (FEPInventoryEntry& E : Entries.Items)
        if (E.EntryId == EntryId)
        {
            FScopedInventoryNotify Guard(this);
            E.State.Charges = FMath::Max(0, NewCharges);   // ★ 클램프는 여기 한 곳
            Entries.MarkItemDirty(E);                      // ★ MarkItemDirty도 여기 한 곳
            return;
        }
}

void UEPInventoryComponent::AddEntryCharges(int32 EntryId, int32 Delta)
{
    FEPInventoryEntry E;
    if (FindEntry(EntryId, E))
        SetEntryCharges(EntryId, E.State.Charges + Delta);
}
```

**둘 다 두는 이유는 `Set`과 `Add`가 부호 차이가 아니라 의미가 다르기 때문이다.**

| | 쓰이는 곳 |
|---|---|
| `SetEntryCharges` | **잔탄 write-back**(Step 05) — 본질적으로 대입이다 |
| `AddEntryCharges` | `bFungible` 합치기, 자판기 1000원 투입, 소모품 사용, 재장전 소비(§8 미정 #6) |

`Set` 하나만 두면 `UnequipWeapon`이 **다른 컴포넌트에서** 읽고-빼고-넘기게 되어, 원시 엔트리를 감춘 의미가 사라진다. `Add` 하나만 둬도 같은 문제가 write-back에서 생긴다.

- **`ConsumeCharges(Id, N)`는 만들지 않는다.** `AddEntryCharges(Id, -N)`이다 — 이쪽은 진짜로 부호만 다르다
- **`SetEntryCharges`가 public인 것이 03-2의 규칙 위반이 아니다.** 금지한 것은 **원시 엔트리를 내보내는 것**이고, 그 금지의 목적이 "수정은 API로만"이므로 API가 있어야 성립한다

> **`bMergeable`이 아니라 `bFungible`인 이유:** "합쳐도 되는가"는 결과고 "구별할 근거가 없다"가 원인이다. 이 이름을 쓰면 **"균질 아이템은 `Durability`를 쓰지 않는다"** 가 불변식으로 따라와서, *합칠 때 내구도는 어떻게 하나*라는 질문이 아예 발생하지 않는다.

> **스택이 없어 사라진 것:** 병합 순서 규칙, `MaxStack` 단위 분할, 부분 삽입 반환값, 고아 인스턴스 방지를 위한 "슬롯 확보 후 생성" 순서.

---

## 03-4. `OnInteract` 완성

Step 02의 로그 한 줄을 여기서 대체한다.

**★ 시그니처는 Step 02가 확정한 것이다** — `bool` + `FText& OutReason` (`EPInteractable.h:23,27`). 실패 회신은 `UEPGA_Interact`가 `Client_OnInteractFailed`로 보낸다. 이 함수는 사유만 채우고 `false`를 반환한다.

```cpp
bool AEPPickup::OnInteract(AEPCharacter* Interactor, FText& OutReason)
{
    bClaimed = true;                                   // ★ 무엇보다 먼저 (Step 02 함정 #3)

    UEPInventoryComponent* Inv = Interactor ? Interactor->GetInventoryComponent() : nullptr;
    if (!Inv)
    {
        bClaimed = false;                              // ★ 실패 반환 앞마다 되돌린다
        OutReason = NSLOCTEXT("EP", "PickupNoInv", "인벤토리가 없습니다.");
        return false;
    }

    // 본체 → 실패하면 매고 있는 배낭. 순서를 뒤집으면 배낭부터 차서 본체가 빈다
    int32 NewId = Inv->AddSubtree(INDEX_NONE, Payload);
    if (NewId == INDEX_NONE && Inv->GetEquippedBackpack() != INDEX_NONE)
        NewId = Inv->AddSubtree(Inv->GetEquippedBackpack(), Payload);
    // Payload는 Step 01의 `FEPItemState State`가 이 단계에서 바뀐 것 (아래)

    if (NewId == INDEX_NONE)                           // ★ if(NewId)로 쓰면 안 된다 — 아래
    {
        bClaimed = false;
        OutReason = NSLOCTEXT("EP", "PickupNoRoom", "가방에 자리가 없습니다.");
        return false;
    }

    // 빈 슬롯이면 배낭은 매고, 그 외 컨테이너는 그냥 들어간다 (03-6)
    Inv->TryAutoEquipBackpack(NewId);
    Destroy();                                         // 성공 — 값은 이미 복사됐다
    return true;
}
```

### ★★ `bClaimed` 되돌림은 이 함수 안이다 — 선택이 아니라 접근 권한이다

```cpp
// EPPickup.h:24, 50 — 게터만 있고 세터가 없다
bool IsClaimed() const { return bClaimed; }
private:
    bool bClaimed = false;
```

`UEPGA_Interact`가 되돌리게 하려면 **세터를 새로 뚫어야 한다.** 그 세터의 소비자는 영원히 하나이고, 뚫는 순간 *"밖에서 `bClaimed`를 켤 수도 있다"* 가 성립해 선점(Step 02 함정 #3)이 약해진다.

**`return false` 하는 모든 줄 앞에 `bClaimed = false;`가 있는지가 이 함수의 검수 항목이다.** 갈래가 셋이라 한 줄만 빠지면 그 픽업은 **아무도 못 줍는 채로 바닥에 남는다**(함정 #5).

> **실패 사유는 `FText`를 유지한다.** `FGameplayTag`로 바꾸면 클라에 태그→문자열 매핑을 따로 만들어야 하고, "가방에 자리가 없습니다"는 **지역화 문자열이지 게임 상태가 아니다.** 03-5가 A(직접 RPC)로 확정돼 실패 사유가 GAS를 타지도 않는다.

### ★ `AEPPickup`을 여기서 확장한다

Step 01은 아이템 하나만 뿌리므로 `FEPItemState State` 하나였다. 배낭 버리기가 들어오면서 **서브트리**를 들어야 한다.

```cpp
// AEPPickup — Step 01의 State를 대체한다 (필드를 추가하는 게 아니라 교체)
UPROPERTY() TArray<FEPInventoryEntry> Payload;      // 서버 전용. 복제하지 않음
UPROPERTY(ReplicatedUsing = OnRep_ItemId) FName ItemId;   // 그대로 — 클라 표시용

void InitPickup(TArray<FEPInventoryEntry>&& InPayload);   // 서버. ItemId는 루트에서 뽑는다
void StartDropCooldown();                                 // 03-5

// GetState()의 대체형 — EP.Loot.List가 쓴다 (아래)
int32 GetPayloadNum() const { return Payload.Num(); }
const FEPItemState& GetRootState() const;                 // 빈 배열이면 기본값 참조
```

#### ★ 교체가 깨뜨리는 곳이 셋이다 — 전부 Step 01 소속이다

| 위치 | 현재 | 조치 |
|---|---|---|
| `EPItemSpawner.cpp:91` | `Pickup->InitPickup(RolledId, NewState);` | **유일한 호출부.** 원소 1개짜리 배열로 바꾼다 |
| `EPLootDebugCommands.cpp:133` | `P->GetState().Charges, P->IsClaimed() ? ...` | `GetRootState().Charges` + `GetPayloadNum()` |
| `EPLootDebugCommands.cpp:137` | `... L.Z, P->GetState().Charges);` | 같음 (클라 분기 — `[server-only]`) |

**뒤 둘은 `EP.Loot.List` 안에 있고 그 커맨드는 `05_Loot_01_Spawner.md` 소속이다.** Step 03 문서만 보고 고치면 컴파일이 깨진다. 03-9가 *"버려진 배낭의 `Payload` 개수를 그쪽에서 본다"* 고 이미 적었으므로 **개수 게터는 이름이 먼저 있었다.**

- **스포너 경로도 같은 함수를 쓴다** — 원소 1개짜리 배열을 만들어 넘긴다. 진입점이 둘이면 한쪽만 고쳐지는 사고가 난다
- `ItemId`는 **루트 엔트리에서 뽑아 세팅**한다. 클라는 여전히 "무엇이 놓여 있는가"만 알고, 내용물·잔탄은 서버 전용이다(정보 은폐 — Step 01)
- 배낭을 버리면 바닥에는 **배낭 하나로 보인다.** 안에 뭐가 있는지는 주워야 안다 — 타르코프와 같고, 별도 처리가 필요 없다

### ★★ `AddItem`이 아니라 `AddSubtree`다 — 안 그러면 배낭 내용물이 증발한다

03-5가 픽업에 **서브트리**를 넘기는데 획득 경로가 엔트리 하나만 읽으면, **배낭을 버렸다 다시 주울 때 안의 아이템이 전부 사라진다.** 완료 조건이 "버리면 같이 나간다"만 확인하고 되줍기를 확인하지 않으면 검증에서도 새어나간다.

```cpp
// 반환: 루트의 새 EntryId. 실패 시 INDEX_NONE
// 전제: In은 RemoveEntry가 만든 전위 순회 배열. In[0]이 루트이고 Parent는 INDEX_NONE (03-2 계약)
int32 UEPInventoryComponent::AddSubtree(int32 Container, const TArray<FEPInventoryEntry>& In)
{
    if (In.Num() == 0) return INDEX_NONE;

    // 자식이 없으면 단일 아이템이다 → AddItem이 bFungible을 본다
    if (In.Num() == 1) return AddItem(Container, In[0].ItemId, In[0].State);

    ensureMsgf(!IsFungible(In[0].ItemId),
               TEXT("[Inventory] 자식을 가진 fungible 아이템: %s"), *In[0].ItemId.ToString());

    if (!CanFit(Container, In[0].ItemId)) return INDEX_NONE;   // ★ 칸 검사는 루트만 — 아래

    FScopedInventoryNotify Guard(this);
    TMap<int32, int32> OldToNew;
    for (const FEPInventoryEntry& Src : In)
    {
        const int32 NewParent = (Src.ParentEntryId == INDEX_NONE)
            ? Container
            : OldToNew.FindRef(Src.ParentEntryId);   // ★ 부모가 먼저 들어와 있다

        const int32 NewId = InsertEntry(NewParent, Src.ItemId, Src.State, Src.SlotId);
        OldToNew.Add(Src.EntryId, NewId);
    }
    return OldToNew[In[0].EntryId];
}
```

> **`InsertEntry`를 쓰는 것이지 `AddItem`을 쓰는 게 아니다.** 자식에 `AddItem`을 부르면 둘이 동시에 깨진다 —
> ① **`bFungible` 합치기**가 돌아 배낭 속 현금이 다른 컨테이너 현금과 합쳐진다
> ② **`CanFit`이 넘겨받은 `Container` 기준**으로 검사하는데, 자식은 루트 **안**으로 들어간다
> 그래서 03-2가 발급·삽입·`MarkItemDirty`만 하는 `InsertEntry`를 따로 뒀다.

#### ★★ 칸 검사가 루트만인 것은 축소가 아니라 **정확한** 것이다 — 늘리지 마라

재매핑 규칙상 `Src.ParentEntryId == INDEX_NONE`인 원소(루트)만 `Container`를 부모로 받고, 나머지는 `OldToNew`가 준 부모 — 즉 **서브트리 내부** — 를 받는다. **자식이 `Container`의 칸을 먹는 경우가 문법적으로 없다.**

| 실제 조작 | 호출 모양 | 검사 |
|---|---|---|
| 상자 **안의 아이템 하나**를 배낭으로 | 그게 루트다 (`In.Num() == 1`) | 루트 검사 = 그 아이템 검사 ✅ |
| **상자 자체**를 배낭에 | 상자가 루트, 내용물은 상자 안에 그대로 | 배낭이 드는 것은 상자 1개분 ✅ 상자의 용량은 상자가 어디 있든 상자의 성질이다 |

서브트리 전체 합을 검사하도록 **"고치면" 배낭을 되주울 때 자기 내용물이 본체 칸을 먹는 것으로 계산돼 완료 조건 9(되줍기)가 깨진다.**

**★ `EntryId` 재매핑이 필수다.** 픽업이 든 번호는 **버린 사람의 것**이고 새 인벤토리에서는 `NextEntryId`로 다시 발급된다. `ParentEntryId`를 옛 번호 그대로 두면 자식이 고아가 되거나 **남의 엔트리에 매달린다.**

> **`In[0]`이 루트라는 것은 탐색 결과가 아니라 계약이다** — `RemoveEntry`가 그렇게 만든다(03-2). `ParentEntryId == INDEX_NONE`인 것을 찾는 방식으로 쓰면 **배낭 안에 있던 무기를 버릴 때 루트를 못 찾는다.**

> **`OldToNew.FindRef`가 0을 돌려주면 데이터 오류다.** 부모가 배열에 없다는 뜻이고 지금 구조에서는 발생할 수 없다. `ensureMsgf` + 그 엔트리를 건너뛴다 — 조용히 `Parent=0`으로 넣으면 `EntryId=0`이 없으므로 **고아**가 되어 이 함수가 막으려던 상태로 돌아간다.

#### `bFungible`과의 관계 — 판정은 "자식이 있는가"다

합치기는 **"이 개체를 없애고 기존 개체의 숫자를 올린다"** 이므로, 없애면 안 되는 것(자식)이 매달려 있으면 성립하지 않는다.

- **`In.Num() == 1`이 그 판정의 정확한 형태다** (자식이 있으면 원소가 2 이상)
- `bFungible`은 판정이 아니라 **그 아래 계층**이다 — 원소 1개일 때 합칠지는 `AddItem`이 정한다. 두 조건은 겹치는 게 아니라 **직렬**이다
- **탄약상자는 합쳐진다.** 원소 1개 → `AddItem` → `bFungible` → 합산
- 자식을 가진 fungible은 **논리적으로 불가능하다** — `bFungible`은 "구별할 근거가 없다"는 뜻인데 자식이 있으면 그 자체가 구별 근거다. DT 설정 실수이므로 `ensure`로 드러낸다

> **부착물(§7-3)도 이 함수를 그대로 쓴다.**

### ★ `INDEX_NONE`은 truthy다

```cpp
if (Inv->AddItem(...))       // ← INDEX_NONE == -1 이라 실패를 성공으로 읽는다
```

`bool` 반환이었으면 불가능했던 오독이고, `EntryId`가 1부터 시작해 0이 안 나오는 것도 여기서는 도움이 안 된다. **`!= INDEX_NONE`을 강제한다.** 개수 대신 `EntryId`를 돌려받는 이득의 대가로 붙는 함정이다.

**갈래가 둘뿐이다.** 스택이 있던 설계에서는 전량/부분/실패 셋이었고, 각각에 빠뜨리기 쉬운 필수 호출이 붙어 있었다.

| 이전 설계의 함정 | 지금 |
|---|---|
| `InstanceHandle = INDEX_NONE`을 `Destroy()` **전에** — 안 하면 획득 직후 잔탄 소실 | **없다.** `State`는 이미 복사됐고 픽업 파괴가 원본을 건드리지 않는다 |
| 부분 획득 시 `FlushNetDormancy()` — 안 하면 클라 개수만 옛날 값 | **없다.** 부분 획득 경로가 없다 |
| 부분 획득 시 `bClaimed = false` — 안 하면 아무도 못 줍는다 | 실패 갈래 하나에만 남는다 |

---

## 03-5. 버리기

### ★★ 왜 어빌리티가 아니라 서버 RPC인가 (8차 확정)

7차가 Step 02를 `UEPGA_Interact`로 확정하면서 세운 문장은 이랬다.

> ~~게임플레이 입력의 진입점은 어빌리티 하나다~~

**이 문장을 다음으로 대체한다.**

> **클라이언트가 서버에 요청하는 경로는 둘뿐이다.**
> ① **월드 상호작용**, 그리고 **시간·비용·애님이 붙는 행동** → **어빌리티**
> ② **서버가 이미 소유한 상태에 대한 변경 요청** → **`UEPInventoryComponent`의 서버 RPC**

**드랍은 ②다.** 클라는 월드를 조회하지 않는다. `EntryId`는 서버가 발급해 `COND_OwnerOnly`로 그 클라에게만 보낸 번호이고, 클라는 그중 하나를 **지목**할 뿐이다. 서버 검증은 `ContainsEntry(EntryId)` 하나로 **완전하다** — 거리도 가시성도 클라/서버 불일치도 없다.

**이 선은 Lyra의 것과 같다.** Lyra 게임 모듈 전체에서 손으로 쓴 서버 RPC는 **정확히 하나**이고, 그것이 인벤토리/장비의 활성 슬롯 변경이다.

```cpp
// LyraQuickBarComponent.h:30-31   (ULyraQuickBarComponent : UControllerComponent)
UFUNCTION(Server, Reliable, BlueprintCallable, Category="Lyra")
void SetActiveSlotIndex(int32 NewIndex);
// .cpp:137 — 검증이 이 한 줄이다
if (Slots.IsValidIndex(NewIndex) && (ActiveSlotIndex != NewIndex)) { ... }
```

줍기·상호작용은 어빌리티로 가면서(`IPickupable.h`의 `AddPickupToInventory`가 `BlueprintAuthorityOnly` + `meta=(WorldContext="Ability")`), **슬롯 변경만 RPC로 남겼다.** `EventMagnitude`에 싣지도, `FGameplayAbilityTargetData` 서브클래스를 만들지도 않았다.

**GAS로 갔을 때의 대가.** `FGameplayEventData`에 `int32`가 없다(`GameplayAbilityTypes.h:246-284` — 스칼라는 `float EventMagnitude` 하나). 7차에서 대상 전달이 공짜였던 것은 대상이 **액터**라 `FGameplayEventData::Target`이 있었기 때문이고, `EntryId`에는 그 자리가 없다. **정수를 `EventMagnitude`에 싣는 선례는 엔진과 Lyra를 통틀어 0건이다**(엔진의 유일한 대입 `AbilitySystemComponent_Abilities.cpp:2643`은 태그 **개수**이고 RPC를 타지도 않는다).

**그리고 이 어빌리티가 GAS에서 실제로 쓸 기능은 `ActivationBlockedTags` 하나다.**

| GAS 기능 | 드랍이 쓰는가 |
|---|---|
| 예측 | ❌ 아래에서 "하지 않는다"로 확정 |
| 코스트 / 애님 / 몽타주 | ❌ |
| 쿨다운(GE) | ❌ `DropCooldown`은 **픽업**에 붙는다. 플레이어 ASC와 무관 |
| `TargetData` | ❌ 대상이 액터가 아니다 |
| **`ActivationBlockedTags`** | ✅ **하나** — RPC에서는 한 줄이다 |

```cpp
if (!CanMutateInventory()) return;   // State.Dead / 향후 State.Casting을 여기 한 곳에서 본다
```

**되돌아갈 신호 — 이 중 하나가 참이 되면 그 항목만 어빌리티로 올린다.**
- ⓐ 버리기·장착에 **시전 시간이나 몽타주**가 붙는다 (`UEPGA_Skill_Base::CastTime` 구조 재사용)
- ⓑ 클라 **예측**이 필요해진다
- ⓒ **RPC가 `UEPInventoryComponent` 바깥 클래스에 생기려 한다** ← 진짜 경계선

> **★ GAS에 비슷한 이름이 있다는 것과 그 기능이 이 문제를 푼다는 것은 다르다.**
> `DropCooldown`은 GE 쿨다운이 아니다 — **주체가 픽업이지 플레이어가 아니다.** GE로 하면 버린 사람만 못 줍고 옆 사람은 즉시 줍는다. 칸 계산은 어트리뷰트가 아니다 — 컨테이너별 파생값이고 `SlotSize`는 DT에서 나온다. `OnInventoryChanged`는 태그가 아니다 — **태그는 상태이고 알림은 에지다.**

> **§2-2(b)가 걱정한 "RPC가 넷으로 는다"는 사실이지만**, 넷이 전부 **한 클래스의 상태 변경 요청**이라 배관이 느는 게 아니라 같은 배관의 함수가 느는 것이다. 어빌리티로 갔으면 어빌리티 클래스가 넷이 되거나 `EntryId` 인코딩 규약을 넷이 공유했다.

### 구현

```cpp
UFUNCTION(Server, Reliable)
void Server_DropItem(int32 EntryId);
```

```cpp
void UEPInventoryComponent::Server_DropItem_Implementation(int32 EntryId)
{
    if (!CanMutateInventory()) return;             // 죽음·시전 게이트 (위)

    // ★★ 스폰이 제거보다 먼저다 — 아래 참조
    AEPPickup* P = SpawnPickupInFront();
    if (!P)
    {
        Client_OnInventoryActionFailed(NSLOCTEXT("EP", "DropNoSpace", "버릴 공간이 없습니다."));
        return;
    }

    TArray<FEPInventoryEntry> Removed;
    if (!RemoveEntry(EntryId, &Removed))           // 없는 엔트리 = 조작된 요청
    {
        P->Destroy();                              // 빈 픽업을 남기지 않는다
        return;                                    // ★ 조작 요청에는 회신하지 않는다
    }

    P->InitPickup(MoveTemp(Removed));              // ★ write-back이 끝난 값
    P->StartDropCooldown();
}
```

### ★★ 스폰이 제거보다 먼저다 — 안 그러면 아이템이 증발한다

초안의 순서가 이랬다.

```cpp
if (!RemoveEntry(EntryId, &Removed)) return;   // ← 인벤토리에서 이미 사라졌다
AEPPickup* P = SpawnPickupInFront();
if (!P) return;                                // ★ Removed가 여기서 통째로 증발한다
```

**배낭을 버리다 스폰이 실패하면 내용물까지 같이 사라진다.** 세부표가 `AlwaysSpawn` + 발밑 폴백을 지정했으므로 실제로 null이 잘 안 나오겠지만, **코드가 `if (!P) return;`으로 그 갈래를 인정하고 있다.** 인정한 갈래에 소실 경로가 있으면 안 된다.

**순서를 뒤집어도 write-back 불변식은 안 깨진다.** 이 문서가 막으려던 것은 *"스냅샷을 제거보다 먼저 뜨는 것"* 이지 *"스폰을 제거보다 먼저 하는 것"* 이 아니다. `RemoveEntry`가 write-back을 자기 안에서 하므로(03-2) **스냅샷은 여전히 제거의 반환값으로만 얻는다.**

빈 픽업을 먼저 띄우고 실패 시 `Destroy()`하는 형태는 **"실패해도 아무것도 안 잃는다"를 형태로 만든 것**이라 §4-6의 기조와 같다.

### ★ 실패를 조용히 삼키지 않는다 — RPC에는 GAS 회신이 없다

Step 02가 *"실패를 조용히 삼키지 않는다"* 를 원칙으로 세우고 `AEPPlayerController::Client_OnInteractFailed`(`EPPlayerController.h:47`)를 만들었다. **A(직접 RPC)로 가면 GAS의 실패 라우팅이 없으므로 그 경로를 직접 재사용해야 한다.**

- **`Client_OnInteractFailed` → `Client_OnInventoryActionFailed`로 이름을 일반화한다.** 상호작용·드랍·장착이 같은 프롬프트를 쓴다. 구현부(`EPPlayerController.cpp:38-49`)는 그대로 — 온스크린 메시지 + 로그
- **`UEPGA_Interact`의 호출부도 같이 바뀐다** (`EPGA_Interact.cpp:42`). 한 줄이다
- **조작된 요청(`ContainsEntry` 실패)에는 회신하지 않는다.** 정상 클라에서 도달 불가한 갈래이고, 회신하면 치터에게 인벤토리 상태를 알려준다

### ★★ 스냅샷을 제거보다 먼저 뜨면 잔탄을 잃는다

**이 문서군 전체가 막으려던 바로 그 버그가 여기로 옮겨왔었다.** 초안의 절차가 이랬다.

```
3. Pickup->InitPickup(엔트리 + 자식)   ← Charges=30 (아직 write-back 전)
4. RemoveEntry(EntryId)                 ← 여기서 12를 쓴다. 아무도 안 읽는다
```

18발 쏘고 장착 무기를 버리면 픽업이 들고 나가는 값이 **30**이다. 다시 주워 장착하면 30/30. Step 05의 `InitAmmo` 만탄 리셋과 **증상이 똑같아** 오진한다.

**원인은 불변식을 옳은 함수에 넣고도 "스냅샷은 제거보다 나중"이라는 새 순서 규칙을 만든 것이다.** 규칙을 걷어낸 게 아니라 한 칸 옆으로 옮긴 것이었다.

→ **`RemoveEntry()`가 제거된 서브트리를 반환한다.** 스냅샷을 얻는 유일한 방법이 제거하는 것이므로 **순서를 뒤집는 게 문법적으로 불가능**해진다. 이 문서가 write-back에 대해 내린 결론("규율이 아니라 형태로 막는다")을 그대로 적용한 것이다.

> `RemoveEntry()`는 장착 중이면 스스로 `UnequipWeapon()`을 부르고 자식을 캐스케이드한다(03-2). **호출자가 신경 쓸 순서가 없다.**

### ★ 자식 캐스케이드 — 값 타입에서 유일하게 살아남는 "고아"

배낭 엔트리를 지울 때 자식을 같이 처리하지 않으면 **고아 엔트리**가 남는다. 부모가 없으니 어느 컨테이너에도 안 보이고, 칸도 안 먹고, **UI에도 안 뜨는데 세이브에는 실린다.**

하필 §4-1이 UObject를 버린 이유가 고아 인스턴스 누수였다. **표현만 바뀌어 같은 개념이 돌아온 것**이므로 `RemoveEntry()` 안에 반드시 넣는다.

| 상황 | 자식 처리 |
|---|---|
| 배낭을 **버린다** | 서브트리째 픽업으로 이관 |
| 배낭이 **파괴된다** (사망·매치 종료) | 서브트리째 제거 |
| 무기를 버린다 | 부착물이 딸려 나간다 (§7-3) |

### RPC 파라미터가 `EntryId`인 이유

배열 인덱스였다면 위험했다 — 제거·정렬로 인덱스가 밀리는 사이에 요청이 도착하면 엉뚱한 아이템을 버린다. `EntryId`는 03-1에서 **재번호되지 않는 서버 발급 번호**로 정의했으므로 그 경쟁이 없다.

### 세부

| 항목 | 처리 |
|---|---|
| 스폰 위치 | 캐릭터 전방 약 100cm, 아래로 라인 트레이스해 접지. 실패 시 발밑 |
| 벽 끼임 | 스폰 위치가 막혀 있으면 발밑으로 폴백. `AlwaysSpawn`으로 두되 위치를 보정 |
| 즉시 재획득 | `DropCooldown`(0.5초) 동안 `CanInteract()`가 false. 없으면 G를 누른 순간 F 프롬프트가 바로 떠서 실수로 다시 줍는다 |
| 부분 버리기 | **없다.** 스택이 없으므로 엔트리 하나가 최소 단위다 |
| 클라 예측 | **하지 않는다.** 결과가 늦게 보여도 무해하고, 예측하면 롤백 처리가 필요해진다 |

`DropCooldown`이 서버에서 강제되려면 **Step 02의 4단계(`CanInteract()` 서버 재호출)가 반드시 있어야 한다.** 없으면 클라가 프롬프트만 회색으로 그리고 RPC는 통과한다. 그 단계는 구현돼 있다 — `EPGA_Interact.cpp:77`.

### ★★ `DropCooldown` — 지금 설계로는 **회색 프롬프트가 뜰 수 없다**

완료 조건: *"버린 직후 0.5초 동안 그 픽업이 **회색 프롬프트로 표시**되고 서버가 거부한다"*

**앞의 절반이 성립하지 않는다.** 이유가 둘이고 둘 다 Step 02 코드로 확인된다.

1. **클라가 쿨다운 값을 모른다.** 프롬프트 판정은 `UEPInteractionComponent::UpdateFocus()`가 **클라이언트에서** `CanInteract`을 불러 낸다(`EPInteractionComponent.cpp:94`). `bClaimed`가 비복제라 회색 갈래가 이미 도달 불가인데(02 STATUS), 쿨다운도 복제 안 하면 **똑같이 도달 불가다**
2. **같은 대상을 보고 있으면 프롬프트가 갱신되지 않는다.** `if (NewFocus == FocusedActor) return;`(`:78`). 버린 픽업을 계속 보고 있으면 0.5초가 지나도 **회색이 안 풀린다**

**둘 다 푸는 최소 형태.**

```cpp
// AEPPickup — 복제한다. 값은 서버 월드 시간
UPROPERTY(Replicated) float DropCooldownEndTime = 0.f;   // 0이면 쿨다운 없음

bool AEPPickup::CanInteract(AEPCharacter*, FText& OutReason) const
{
    if (bClaimed) { OutReason = NSLOCTEXT("EP", "PickupClaimed", "이미 획득됨"); return false; }

    const AEPGameState* GS = GetWorld()->GetGameState<AEPGameState>();
    if (GS && GS->GetServerWorldTimeSeconds() < DropCooldownEndTime)
    {
        OutReason = NSLOCTEXT("EP", "PickupCooling", "방금 버린 아이템입니다");
        return false;
    }
    return true;
}
```

- **타이머 핸들이 아니라 `float`이다.** 콜백으로 할 일이 없다 — 판정은 비교 한 번이고, 타이머는 서버에만 있는 상태가 되어 1번 문제가 그대로 남는다
- **`GetServerWorldTimeSeconds()`를 쓴다.** 로컬 시간이면 클라/서버 판정이 갈린다. SSR이 이미 이 시계를 쓰는 프로젝트 관례다
- **Dormancy와 충돌하지 않는다.** `InitPickup` + `StartDropCooldown`을 `SpawnActor`와 **같은 프레임**에 부르면(Step 01의 `InitPickup` 규칙과 동일) 초기 복제에 실려 나가고, 이후 값이 안 바뀌므로 `DORM_Initial`이 유지된다 — **`FlushNetDormancy()`가 필요 없다**

**2번은 `UpdateFocus`의 조기 반환을 결과 기준으로 바꾼다** (`UEPInteractionComponent`, Step 02 구현물 수정).

```cpp
// 포커스가 안 바뀌어도 CanInteract 결과가 바뀌면 프롬프트를 갱신해야 한다
IEPInteractable* Interactable = Cast<IEPInteractable>(NewFocus);
FText Reason;
const bool bCan = (Interactable && Interactable->CanInteract(Owner, Reason));

if (NewFocus == FocusedActor && bCan == bLastCanInteract) return;   // ★ 결과 기준
FocusedActor     = NewFocus;
bLastCanInteract = bCan;
```

> `TickInterval = 0.1f`이라 초당 10회 `CanInteract` 호출이고 그 함수는 비교 두 번이다. **비용이 문제가 아니라 "포커스 불변 = 상태 불변"이라는 전제가 틀렸다는 게 문제다.** `bLastCanInteract`는 `UEPInteractionComponent`의 private `bool` 하나.

> **이걸 안 짚고 가면:** 서버 거부는 도는데 프롬프트만 안 변한다. 증상이 *"가끔 못 줍는데 UI는 줍을 수 있다고 한다"* 이고 **`DropCooldown` 로직이 아니라 HUD를 파게 된다.** Step 02가 남긴 두 개의 "지금은 도달 불가" 갈래가 여기서 동시에 첫 소비자를 만난다.

---

## 03-6. 배낭 장착 — 줍고 빈 슬롯이면 자동 착용

**GAME.md 장비 절에 "배낭 슬롯 1"이 있는데 조작 방법이 어느 문서에도 없었다.** Step 03의 완료 조건("배낭을 매면 별도 칸이 열린다")이 이것에 의존하므로 여기서 정의한다.

```cpp
UPROPERTY(Replicated) int32 EquippedBackpackEntryId = INDEX_NONE;   // COND_OwnerOnly

UFUNCTION(Server, Reliable) void Server_EquipBackpack(int32 EntryId);

// 획득 직후 호출. 컨테이너이고 슬롯이 비어 있을 때만 맨다
void TryAutoEquipBackpack(int32 EntryId);
```

```
줍는다 → 본체(또는 배낭)에 들어감 → TryAutoEquipBackpack
    ContainerCapacity > 0  이고  EquippedBackpackEntryId == INDEX_NONE
        → 맨다
    이미 매고 있다 → 그냥 아이템으로 들어가 있는다 (교체는 벗고 다시 줍기)
```

- **자동 착용을 고른 이유:** Step 04(UI)가 없어도 Step 03만으로 검증된다. 수동 착용은 조작 UI가 있어야 하는데 그건 다음 단계다
- **교체는 지금 넣지 않는다.** "벗고 다시 줍기"로 충분하고, 교체를 넣으면 *현재 배낭 내용물을 어디로 옮기나*가 따라온다
- **`RemoveEntry()`가 이 번호도 정리한다**(03-2). 배낭을 버렸는데 죽은 번호가 남으면 `GetCapacity`가 0을 돌려주고 Step 04에 **유령 구획**이 남는다

> **슬롯이 셋(주무기/보조/배낭)이 되면 `TMap<EEPEquipSlot, int32>`로 간다** — §8 미정 #5. 지금은 **필드 둘**이다. 원소가 둘인 맵을 만들 이유가 없다.

---

## 03-7. 알림 — 수신 1회당 1회

**항목별 콜백을 쓰지 않는다.** `PostReplicatedAdd`는 **항목마다** 불리므로(`FastArraySerializer.h:1163`) 한 번의 수신에 UI 재생성이 항목 수만큼 돈다. 서버 쪽도 배낭 하나 버리면 `Broadcast`가 N+2회 나간다.

> **"안 쓴다"가 "선언만 해둔다"가 아니다.** 03-1이 셋을 **아예 선언하지 않는** 이유는 이름 가림 때문에 선언만 하면 링크 에러가 나기 때문이다. 기반 no-op이 받게 두는 것이 유일하게 성립하는 형태다.

```cpp
// FEPInventoryList에 정의한다 — 엔트리가 아니라 직렬화기 쪽이다
void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters&)
{
    if (Owner) Owner->OnInventoryChanged.Broadcast();
}
```

> *"If a function with the signature `void PostReplicatedReceive(...)` is defined in the derived struct, **it will be called after each call to NetDeltaSerialize on the receiving end**"*
> — `FastArraySerializer.h:517-519`

**수신 한 번당 정확히 한 번** 불리고 Add/Change/Remove를 구분할 필요가 없다 — Step 04는 어차피 전체 재생성이다. **Step 04의 "`PostReplicatedChange` 브로드캐스트 누락" 함정이 통째로 소멸한다.** 빠뜨릴 콜백이 없기 때문이다.

서버 쪽도 같은 모양으로 맞춘다.

```cpp
struct FScopedInventoryNotify        // 진입 ++ / 이탈 --
{                                    // 0이 되고 pending이면 그때 한 번 Broadcast
    explicit FScopedInventoryNotify(UEPInventoryComponent* In);
    ~FScopedInventoryNotify();
};
```

`AddItem` / `AddSubtree` / `RemoveEntry` / `SetEntryCharges` 선두에 가드를 놓으면 **"중간 Broadcast"라는 상태가 존재할 수 없게 된다.** 10줄이고, 이 문서가 write-back 순서에 대해 한 처리와 같은 종류다.

- **`bNotifyPending` 같은 플래그를 두지 않는다.** 가드는 변형 함수에만 놓이고 모든 실패 검사가 가드보다 앞에 있으므로 `NotifyDepth > 0` 자체가 "보낼 것이 있다"는 뜻이다. 플래그를 두면 완전히 중복이거나, `MarkItemDirty` 호출부마다 손으로 세워야 해서 **가드가 없애려던 문제가 돌아온다**
- **`SetEquippedEntryId`도 알림을 쏜다.** Step 04가 장착 강조를 그리므로(05-4) 안 쏘면 무기를 바꿔도 UI 테두리가 안 옮겨간다
- 값이 안 바뀌어도 한 번 더 쏘는 경우(`AddEntryCharges(Id, 0)`)가 있지만 UI 재생성 한 번이라 무해하다. 이걸 막으려고 값 비교를 넣으면 **10줄짜리 가드가 로직을 갖게 된다**

**재진입도 여기서 막힌다.** `RefreshEntries()`가 읽기 전용이라 지금은 종료하지만, 그게 유일한 근거다. 구독자가 하나 더 붙어 인벤토리를 건드리는 순간(예: "가방이 꽉 차면 자동으로 뭔가 버린다") 재진입한다. 가드가 있으면 **`Entries.Items`를 순회하는 도중에는 알림이 나가지 않으므로** 구독자가 무엇을 하든 성립하지 않는다.

---

## 03-8. 수명 — 관리할 것이 없다

`FEPItemState`가 값 타입이므로 소유자(엔트리·픽업)가 사라지면 같이 사라진다. 이전 설계에서 이 자리에 있던 것들:

| 이전 설계의 규칙 | 지금 |
|---|---|
| `UEPInventoryComponent::EndPlay`에서 핸들 전부 `Destroy()` | 불필요 |
| `AEPPickup::EndPlay`에서 핸들 `Destroy()` | 불필요 |
| 이관 중에는 `Destroy()` 호출 금지 (이관 프로토콜) | 이관이 값 대입이라 프로토콜 자체가 없다 |
| 매치 종료 시 서브시스템 `Deinitialize()` 안전망 | 서브시스템이 없다 |
| **사망 시 드랍은 반드시 `EndPlay`보다 먼저** | 남는 요구는 "죽기 전에 인벤토리를 읽어야 한다"는 자명한 것뿐 |

**대신 새로 생긴 것이 하나 있다** — 부모-자식 관계의 정합성(고아 엔트리). 자료구조가 막아주던 것을 **`RemoveEntry()`의 내부 불변식**이 대신한다.

---

## 03-9. ★ 검증 커맨드 — 이 단계에만 없었다

Step 00에는 `EP.Item.State`/`EP.Item.Dump`가, Step 01에는 `EP.Loot.RollTable`/`EP.Loot.Respawn`이 있는데 **배낭·서브트리·칸 합산·`bFungible`이 전부 몰린 Step 03에만 커맨드가 없었다.** UI는 Step 04라 완료 조건 12개 중 7개를 확인할 방법이 없다.

```
> EP.Inv.Dump                              # ★ 클라에서도 실행된다 (아래)
  EntryId  Parent  SlotId  ItemId          Charges  SlotSize
  1        -1      -       Bandage         1        1
  3        -1      -       Backpack_Small  0        2
  4        3       -       Weapon_AK74     30       5
  ---
  Body : 3 / 10     Backpack(3) : 5 / 12     NextEntryId = 5     ← ★

> EP.Inv.DumpAll                            # 월드의 모든 인벤토리 컴포넌트
  Pawn_0 (내 폰)   Entries=4
  Pawn_1           Entries=0                ← 0이면 COND_OwnerOnly 통과

> EP.Inv.Add <ItemId> [PlayerIndex]     # 서버 전용. 기본 0 = 첫 번째 PlayerController
> EP.Inv.Drop <EntryId> [PlayerIndex]   # 서버 전용
```

| 커맨드 | 권한 | 이유 |
|---|---|---|
| `Dump` / `DumpAll` | **클라 허용** | 순수 조회. Step 00의 `EP.Item.State`와 같은 구분 |
| `Add` / `Drop` | 서버 전용 | 상태를 바꾼다 |

### ★ `[PlayerIndex]`가 없으면 `COND_OwnerOnly`를 검증할 수 없다

§10이 *"리슨서버 호스트가 아니라 클라이언트 쪽에서 확인"* 을 원칙으로 세웠는데 `Add`가 서버 전용이면 **호스트 인벤토리만 채우게 된다.** 그러면 `DumpAll`의 `Entries=0`이 *"복제가 막혔다"* 가 아니라 *"애초에 아무것도 안 넣었다"* 를 보여줄 뿐이다 — **검증이 통째로 위양성이 된다.**

`Drop`도 같이 받아야 한다. 안 그러면 "PIE 2번 창의 아이템을 버리는" 시나리오를 못 만든다. 순회는 `World->GetPlayerControllerIterator()` 인덱스면 충분하다.

### ★ `NextEntryId`는 클라에서 거짓말한다 — `[server-only]`로 찍는다

`NextEntryId`는 서버 전용(03-2)이라 클라는 초기값 `1`을 본다. 그런데 `Dump`는 클라 허용이다.

**프로젝트가 이 문제를 이미 한 번 풀었다.**

```cpp
// EPLootDebugCommands.cpp:120-124  (EP.Loot.List)
const bool bAuthority = World->GetNetMode() != NM_Client;
UE_LOG(LogTemp, Log, TEXT("Idx, ItemId, Location, Charges%s, Claimed"),
    bAuthority ? TEXT("") : TEXT("[server-only]"));
```

같은 형태로 클라에서는 `NextEntryId = [server-only]`를 찍고 값을 내지 않는다.

> **완료 조건은 그 열 없이도 닫힌다.** *"재번호되지 않는다"* 는 **`EntryId` 열만으로 증명된다** — 셋 넣고 2번을 버린 뒤 하나 더 넣어 `1, 3, 4`가 나오면 끝이다. `NextEntryId`는 확인의 **편의**이지 증거가 아니다.

`#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)` 가드. SSR 디버그와 동일한 패턴이다.

**각 열이 완료 조건을 직접 증명한다.**

| 열 / 커맨드 | 증명하는 것 |
|---|---|
| `Parent` | 되줍기 후 재매핑(03-4), 고아 없음(03-5) |
| `Charges` | 값 복사가 도는가 |
| `EntryId` | **재번호 없음.** 셋 넣고 2번을 버린 뒤 하나 더 넣었을 때 `1, 3, 4`면 통과. `2`가 재사용됐거나 목록이 `1,2,3`으로 밀렸으면 즉시 보인다 |
| `NextEntryId` | 위를 **서버에서 한눈에** 확인 (클라에서는 `[server-only]`) |
| `Backpack(N) : x / y` | 배낭 자동 착용 + 독립 풀 |
| `DumpAll`의 `Entries=0` | **`COND_OwnerOnly`.** 패킷 캡처도 UI도 필요 없다 |

> **시나리오 자동화(`EP.Inv.Stress` 같은 것)는 넣지 않는다.** 검증할 대상보다 검증 도구가 커진다. 위 출력이면 손으로 세 번 눌러 확인된다.

> **월드 픽업 목록은 여기가 아니라 Step 01의 `EP.Loot.List`다** — 픽업 도구가 두 문서로 갈리지 않게. `DropCooldown`과 버려진 배낭의 `Payload` 개수를 그쪽에서 본다.

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | `COND_OwnerOnly` 누락 | 남의 가방이 전부 복제됨 (치트) | 03-2 |
| 2 | `EntryId` 없이 배열 인덱스 사용 | 줍고 버릴 때마다 목록 순서가 튀고, 경쟁 시 엉뚱한 아이템을 버림 | 03-1 |
| **3** | **스냅샷을 `RemoveEntry`보다 먼저 뜸** | 장착 무기를 버렸다 주우면 **만탄.** 증상이 Step 05의 `InitAmmo` 문제와 똑같아 오진한다 | `RemoveEntry(Id, &Out)`이 반환 (03-5) ★★ |
| **3b** | **캐스케이드가 자기 자신만 장착 검사** | 배낭 속 무기를 장착한 채 배낭을 버리면 write-back 소실 + `EquippedEntryId`가 죽은 번호 + 무기는 손에 남음 | 캐스케이드가 `RemoveEntry` 재귀 (03-2) ★★ |
| **3c** | **`AddItem`으로 픽업을 받음** | 배낭을 버렸다 주우면 **안의 아이템이 전부 증발** | `AddSubtree` + `EntryId` 재매핑 (03-4) ★★ |
| **3d** | **`Entries.Owner = this` 누락** | 서버는 정상인데 **클라 UI가 영원히 갱신 안 됨.** 원인이 UI/복제로 보인다 | 생성자 (03-2) ★★ |
| 3e | 순회하며 자식 제거 | `RemoveAtSwap`이라 인덱스가 튀어 **일부 자식을 건너뛴다.** "가끔 고아가 남는다" | 자식 목록을 먼저 뜬다 (03-2) |
| **3f** | **`RemoveEntry`를 스폰보다 먼저** | 스폰이 실패하면 **서브트리가 통째로 증발한다.** 배낭이면 내용물까지 | 스폰 → 제거 → `InitPickup` (03-5) ★★ |
| **3g** | **`bIsRoot`를 public 시그니처에 둠** | 밖에서 `false`를 넘기면 루트 정규화가 생략돼 `AddSubtree`가 루트를 못 찾는다. **증상이 3c와 같다** | `RemoveEntryInternal` private (03-2) |
| **3h** | **`EquippedEntryId`를 `UnequipWeapon`에 맡김** | 죽은 번호가 남는다. 배낭 버리기 캐스케이드에서 여러 번 불려 순서 논쟁이 생긴다 | `RemoveEntry`가 비운다 (03-2) |
| 4 | `MarkItemDirty` 누락 | 서버만 맞고 클라 갱신 안 됨 | 원시 엔트리 비노출 + `AddEntryCharges()` / `InsertEntry()` (03-2) |
| **4e** | **항목 콜백 3종을 선언만 함** | **링크 에러**(unresolved external). 이름 가림이라 "나중에 채운다"가 성립하지 않는다 | 아예 선언하지 않는다 (03-1) |
| **4f** | **`FindFungibleEntryId`에 컨테이너 인자 없음** | 배낭 속 현금이 본체 현금과 합쳐진다. **무증상이다가 배낭을 벗으면 돈이 딸려 나간다** | `(Container, ItemId)` (03-3) |
| **4g** | **살아있는 엔트리에 통째 대입(`E = Other`)** | 복제 ID가 리셋돼 수신 측이 **삭제+추가**로 본다. 내부 struct 델타가 사라진다 | 필드 단위 대입 (03-1) |
| 4b | **`if (AddItem(...))`로 성공 검사** | `INDEX_NONE`(-1)이 truthy라 **실패를 성공으로 읽는다** | `!= INDEX_NONE` (03-4) |
| 4c | **`FindData()` null을 조용히 넘김** | 그 아이템이 칸을 0으로 먹어 **가방이 무한대**가 된다. 무증상 | `ensure` + 경고 (03-3) |
| 4d | `check(HasAuthority())` | Shipping에서도 크래시. 프로젝트 관례와 다름 | early return (03-3) |
| 5 | 획득 실패에서 `bClaimed` 안 되돌림 | 그 픽업을 아무도 못 줌 | 03-4 |
| 5b | `EquippedBackpackEntryId`를 안 비움 | 배낭을 버려도 **유령 구획**이 UI에 남고 `GetCapacity`가 0 | `RemoveEntry` 내부 (03-6) |
| 6 | `UsedSlots`를 필드로 캐시 | 추가·제거·복제 수신 중 하나만 빠져도 "안 찼는데 가득 찼다" | 매번 계산 (03-3) |
| 6b | 본체와 배낭의 칸을 **합산** | GAME.md는 "통합되지 않는다" | 컨테이너별 (03-3) |
| 6c | 획득 시 **배낭부터** 채움 | 본체가 늘 비어 있고 배낭을 벗으면 다 나간다 | 본체 → 배낭 순 (03-4) |
| 6d | 복제 순서 방어 코드를 넣음 | 불필요한 복잡도. 파생값을 매번 재계산하므로 저절로 맞는다 | 03-3 |
| 7 | 칸 여유 판정에 엔트리 **개수**를 씀 | `SlotSize`가 큰 무기를 무제한으로 넣게 됨 | `Σ SlotSize` (03-3) |
| 8 | 엔트리 포인터를 밖으로 반환 | 배열 재할당 후 댕글링. Step 00의 `FEPItemData*`와 같은 문제 | `FindEntry`는 값 복사 (03-2) |
| 8b | 알림 뒤에 `E.EntryId`를 읽음 | 구독자가 배열을 재할당하면 댕글링 | 값을 미리 뜬다 (03-3) |
| 9 | `DropCooldown`을 클라에서만 검사 | 버리자마자 재획득 가능 | Step 02의 4단계 필수 |
| **9b** | **`DropCooldownEndTime`을 복제 안 함 / 타이머 핸들로 둠** | 서버 거부는 도는데 **회색 프롬프트가 영영 안 뜬다.** 증상이 UI 문제로 보여 HUD를 판다 | 복제되는 `float` + 서버 월드 시간 (03-5) |
| **9c** | **`UpdateFocus`가 포커스 변화에만 갱신** | 버린 픽업을 계속 보면 쿨다운이 끝나도 회색이 안 풀린다 | 조기 반환을 **결과 기준**으로 (03-5) |
| **9d** | **드랍 실패를 조용히 return** | Step 02가 세운 "실패를 삼키지 않는다"가 깨진다. RPC에는 GAS 회신이 없다 | `Client_OnInventoryActionFailed` (03-5) |
| **9e** | **인벤토리 조작을 GAS 어빌리티로** | `FGameplayEventData`에 `int32`가 없어 `EventMagnitude` 인코딩이나 `TargetData` 서브클래스를 산다. 실제로 쓰는 GAS 기능은 `ActivationBlockedTags` 하나 | 서버 RPC + `CanMutateInventory()` (03-5) |
| 10 | `ClearLoot`이 버린 아이템까지 삭제 | `EP.Loot.Respawn` 시 플레이어 소지품이 사라짐 | Step 01의 `SpawnedPickups` 약참조 — **여기서 처음 검증 가능** |
| **10b** | **`SlotId != None`인 자식을 용량으로 셈** | 부착물이 칸을 먹는다 | `GetUsedSlots`가 건너뛴다. **§7-3은 용량이 아니라 슬롯 스키마로 제한한다** (03-3) |
| **10c** | **`AddSubtree`가 서브트리 전체 칸을 검사하도록 "고침"** | 배낭을 되주울 때 자기 내용물이 본체 칸을 먹는 것으로 계산돼 **완료 조건 9가 깨진다** | 루트만 검사가 정확하다 (03-4) |

> **★★ 4건은 정상 플레이에서 반드시 나오고 증상이 원인을 가린다.** 본체 10칸 / 무기 5칸이라 무기가 배낭에 들어가는 일이 흔하므로 3b는 이론이 아니다.

---

## 이 단계에서 하지 않는 것

- 인벤토리 화면 UI → **Step 04** (이번엔 `OnInventoryChanged` 델리게이트 + `EP.Inv.*` 커맨드만)
- 무기 장착/해제, `EquippedEntryId` **세팅** → **Step 05**
  > **★ `RemoveEntry`의 `EquippedEntryId` 분기는 Step 03 내내 항상 거짓이다.** 세팅 경로가 Step 05에 있기 때문이다. 컴파일도 되고 완료 조건도 통과하지만 **장착 관련 불변식은 한 번도 실행되지 않는다.** Step 05에서 처음 도는 코드라는 걸 알고 넘어가야, 거기서 버그가 나도 원인을 두 단계 뒤에서 찾지 않는다
  >
  > **방어를 코드로 넣을 자리가 아니다** — 항상 거짓인 분기라 `ensure`가 울릴 일이 없다. **완료 조건으로 넘겼다:** `05_Loot_05_Equipment.md`에 *"배낭 속 무기를 장착한 채 배낭을 버린다"* 한 줄을 추가했다
- 배낭 교체 / 수동 착용 UI → **Step 04 이후** (03-6은 자동 착용까지)
- 사망 시 드랍 → §8 미정 #4
- 드래그앤드롭·정렬·아이템 이동
