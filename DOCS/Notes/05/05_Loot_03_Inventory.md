# Step 03 — Inventory (FastArray 복제 + 스택 병합 + 버리기)

> 마스터 기획: `05_Loot_DOCS.md` (§4-1, §4-6, §4-7)
> 선행: `05_Loot_02_Interaction.md` — `OnInteract()`가 이 단계에서 실제 삽입으로 바뀐다

---

## 목표

주운 아이템이 서버 권한으로 보관되고, 소유 클라이언트에만 델타 복제된다. G로 버리면 픽업이 되돌아 나온다.

**완료 조건**

- [ ] 주운 아이템이 인벤토리에 쌓이고 클라이언트에 복제된다
- [ ] 탄약을 인벤 여유보다 많이 주우면 **픽업이 줄어든 수량으로 남고 다시 주울 수 있다** (`bClaimed` 되돌리기 확인)
- [ ] 무기를 버렸다 다시 주우면 **같은 `Handle`** 이 돌아온다 (`EP.Item.Dump`로 인스턴스 수 불변 확인)
- [ ] 탄약(스택)을 버렸다 주우면 인스턴스가 **하나도 생기지 않는다**
- [ ] 버린 직후 0.5초 동안 그 픽업이 회색 프롬프트로 표시되고 서버가 거부한다
- [ ] **다른 클라이언트에 내 인벤토리가 복제되지 않는다** (`COND_OwnerOnly` 확인)
- [ ] 캐릭터 사망 → 인스턴스가 정리된다 (`EP.Item.Dump`의 `LiveInstances`가 0으로)

---

## 03-1. 자료구조

```cpp
USTRUCT()
struct FEPInventoryEntry : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY() int32 SlotIndex = INDEX_NONE;   // ★ 화면상 몇 번 칸인가
    UPROPERTY() int32 Handle    = INDEX_NONE;   // 비스택 아이템만 유효 (§4-1)
    UPROPERTY() FName ItemId;
    UPROPERTY() int32 Quantity  = 0;

    void PostReplicatedAdd(const struct FEPInventoryList& Serializer);
    void PostReplicatedChange(const struct FEPInventoryList& Serializer);
    void PreReplicatedRemove(const struct FEPInventoryList& Serializer);
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

### ★ `SlotIndex`가 반드시 필요한 이유

`FFastArraySerializer`는 항목을 **ReplicationID로 식별**할 뿐, 클라이언트 배열의 **순서가 서버와 같다는 보장이 없다.** 엔진 주석 원문:

> *"the \*order\* of the list is not guaranteed to be identical between client and server in all cases."*
> — `FastArraySerializer.h:54`

Step 04는 1x1 고정 슬롯을 그리므로, 배열 인덱스를 그대로 칸 번호로 쓰면 **아이템을 하나 줍거나 버릴 때마다 기존 아이콘들이 다른 칸으로 튄다.** 산발적으로 나고 서버는 멀쩡해서 재현이 어려운 부류다.

- `AddItem()`이 **가장 낮은 빈 `SlotIndex`** 를 배정한다
- 제거해도 **다른 엔트리의 `SlotIndex`는 건드리지 않는다** (그래서 드랍 RPC 파라미터로 안전하다)
- 이 필드가 나중에 그리드 인벤토리의 `GridX/GridY`로 자연스럽게 확장된다

---

## 03-2. `UEPInventoryComponent`

```cpp
UCLASS(meta = (BlueprintSpawnableComponent))
class EMPLOYMENTPROJ_API UEPInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEPInventoryComponent();

    // 반환: 실제로 들어간 개수. 0이면 완전 실패
    int32 AddItem(FName ItemId, int32 Count, int32 InHandle = INDEX_NONE);

    bool DropItem(int32 SlotIndex, int32 Count);          // 서버

    // ★ 포인터가 아니라 값으로 돌려준다 (아래 참조)
    bool FindBySlot(int32 SlotIndex, FEPInventoryEntry& Out) const;

    // UI가 순회할 읽기 전용 뷰. 반환 참조는 그 프레임 안에서만 유효하다
    const TArray<FEPInventoryEntry>& GetEntries() const { return Entries.Items; }

    DECLARE_MULTICAST_DELEGATE(FOnInventoryChanged);
    FOnInventoryChanged OnInventoryChanged;               // UI가 구독 (Step 04)

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

    UPROPERTY(Replicated) FEPInventoryList Entries;
    UPROPERTY(Replicated) int32 MaxSlots = 20;
    UPROPERTY(Replicated) int32 EquippedHandle = INDEX_NONE;   // Step 05
};
```

### ★ 복제 조건은 `COND_OwnerOnly`

```cpp
void UEPInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME_CONDITION(UEPInventoryComponent, Entries,        COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UEPInventoryComponent, MaxSlots,       COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UEPInventoryComponent, EquippedHandle, COND_OwnerOnly);
}
```

조건 없이 복제하면 **모든 클라이언트가 모든 플레이어의 인벤토리를 받는다.** 패킷만 봐도 상대 소지품을 아는 치트가 되고, 8인 매치면 대역폭도 8배다. GAME.md의 "정보 은폐" 기조와 정면으로 어긋난다. 프로젝트 관례와도 일치한다 — `PlayerState::Kills` / `Extracted`가 이미 `COND_OwnerOnly`다.

> 나중에 "시체 루팅"이 생기면 남의 인벤토리를 봐야 하는데, 그때 **조건을 푸는 게 아니라** 시체 액터가 자기 인벤토리를 별도로 노출하는 방식으로 간다. 살아있는 플레이어의 가방은 끝까지 소유자 전용이다.

`MaxSlots`도 복제한다 — UI가 빈 칸 수를 알아야 한다.

### ★ 엔트리 포인터를 밖으로 내보내지 않는다

`FindBySlot`이 `const FEPInventoryEntry*`를 돌려주면 **`Entries.Items` 내부를 가리킨다.** `AddItem` / `DropItem` / 복제 수신(`PostReplicatedAdd`)이 배열을 재할당하는 순간 그 포인터는 댕글링한다. Step 00에서 `FEPItemData*` 행 포인터를 금지한 것과 **정확히 같은 문제**다.

- `FindBySlot`은 **값으로 복사해 돌려준다.** 엔트리는 16바이트 남짓이라 복사 비용이 없다
- `GetEntries()`는 UI 순회용으로 참조를 노출하되, **그 프레임 안에서만 유효**하다. 순회 중에 `AddItem`을 부르지 않는다
- 엔트리를 수정할 때는 컴포넌트 내부에서 인덱스로 접근하고 `MarkItemDirty()`를 호출한다. 밖으로 비-const 포인터를 내보내면 `MarkItemDirty` 호출을 강제할 수 없다

### 부착 위치는 Character

`AEPCharacter` 생성자에 `CombatComponent`/`RewindComponent` 옆으로 추가한다.

타르코프식은 **사망 시 소지품 손실**이 규칙이므로(GAME.md 코어 루프) Character와 수명을 같이하는 것이 규칙과 일치한다. PlayerState에 두면 사망 시 명시적으로 비워야 하고 "비우는 걸 깜빡하는" 버그의 자리가 생긴다.

> ASC를 PlayerState에 둔 것과 반대지만 이유가 다르다. ASC는 리스폰 후에도 **보존되어야** 해서 PlayerState였다. 인벤토리는 반대로 **소실되어야** 한다.

---

## 03-3. `AddItem` — 스택 병합 순서

```
AddItem(ItemId, Count, InHandle = INDEX_NONE) → 실제로 들어간 개수
  1. 같은 ItemId이면서 여유가 있는 기존 스택을 SlotIndex 오름차순으로 채운다
  2. 남은 수량을 빈 SlotIndex(가장 낮은 것부터)에 MaxStack 단위로 새 엔트리로 만든다
  3. 빈 슬롯이 떨어지면 거기서 멈추고, 그때까지 들어간 개수를 반환한다

  비스택 아이템(MaxStack == 1)의 핸들 처리:
    InHandle 유효   → 그대로 엔트리에 대입 (버린 것을 다시 주움 — 잔탄 보존)
    InHandle 없음   → ★ 빈 슬롯 확보에 성공한 뒤에만 CreateInstance()
```

**★ 인스턴스 생성은 슬롯 확보 이후다.** 순서를 뒤집으면 삽입 실패 시 **고아 인스턴스**가 서브시스템에 영원히 남는다. `EP.Item.Dump`의 `LiveInstances`가 슬금슬금 오르는 형태로 나타나서, 원인을 찾기 전까지는 누수인 줄도 모른다.

**기존 스택을 먼저 채우는 이유:** 반대로 하면 빈 슬롯이 먼저 소모되어, 합칠 수 있었던 상황에서 "가방이 가득 찼다"가 나온다. 플레이어가 납득하지 못한다.

**예시** — 탄약 60발 획득, `MaxStack` 30, 현재 `[슬롯0: 20발, 슬롯1: 비어있음]`, `MaxSlots` 2

```
1) 슬롯0을 30으로 채움          (10 소모, 잔여 50)
2) 슬롯1에 30 배치              (30 소모, 잔여 20)
3) 빈 슬롯 없음 → 중단
→ 40 반환. 픽업의 Quantity를 20으로 낮추고 FlushNetDormancy()
```

`MarkItemDirty(Entry)` / `MarkArrayDirty()`를 변경마다 호출해야 델타 복제가 나간다. 빠뜨리면 서버만 맞고 클라는 갱신되지 않는다.

---

## 03-4. `OnInteract` 완성 — 부분 획득과 `bClaimed`

Step 02의 로그 한 줄을 여기서 대체한다.

```cpp
void AEPPickup::OnInteract(AEPCharacter* Instigator)
{
    UEPInventoryComponent* Inv = Instigator->GetInventoryComponent();
    const int32 Added = Inv->AddItem(ItemId, Quantity, InstanceHandle);

    if (Added >= Quantity)                      // 전량 획득
    {
        InstanceHandle = INDEX_NONE;            // ★ Destroy 전에 비운다
        Destroy();
        return;
    }

    if (Added > 0)                              // 부분 획득 — 픽업은 남는다
    {
        Quantity -= Added;
        FlushNetDormancy();                     // ★ 없으면 클라 개수가 옛날 값
        bClaimed = false;                       // ★ 되돌리지 않으면 아무도 못 줍는다
        return;
    }

    bClaimed = false;
    /* Client_OnInteractFailed("가방이 가득 찼습니다") */
}
```

세 개의 ★가 전부 **빠뜨리기 쉽고 증상이 엉뚱한** 종류다.

| 누락 | 증상 |
|---|---|
| `InstanceHandle = INDEX_NONE` 먼저 | `AEPPickup::EndPlay`가 방금 인벤토리로 넘긴 인스턴스를 지운다 → **획득 직후 무기 잔탄이 사라진다** |
| `FlushNetDormancy()` | 서버는 정상인데 **클라 화면의 개수만 옛날 값**으로 남는다 |
| `bClaimed = false` | 부분 획득된 픽업을 **아무도 다시 못 줍는다** |

> 부분 획득은 스택 아이템에서만 발생하고, 스택 아이템은 `InstanceHandle`이 `INDEX_NONE`이라 첫 항목과 겹치지 않는다. 그래도 세 분기를 명시적으로 쓰는 편이 낫다 — 나중에 "스택 가능한 비스택 아이템"이 생기면 조용히 깨진다.

---

## 03-5. 버리기

```cpp
UFUNCTION(Server, Reliable)
void Server_DropItem(int32 SlotIndex, int32 Count);
```

```
Server_DropItem(SlotIndex, Count)
  1. 그 SlotIndex에 엔트리가 실제로 있는가 (클라 요청은 신뢰하지 않는다)
  2. Count가 엔트리 Quantity 이하인가
  3. 장착 중이면 먼저 해제 (Step 05 — 어트리뷰트 → 인스턴스 write-back 포함)
  4. 캐릭터 전방에 AEPPickup 스폰 + 바닥 트레이스로 접지
  5. ★ 픽업에 핸들 대입 → 그 다음 인벤토리 엔트리 제거   (이관 프로토콜 순서)
  6. 픽업의 DropCooldown 타이머 시작
```

### RPC 파라미터가 `Handle`이 아니라 `SlotIndex`인 이유

**스택 아이템은 `Handle`이 `INDEX_NONE`이라 식별자가 되지 못한다.** 두 경로(스택/비스택)를 하나로 유지하려면 `SlotIndex`가 맞다. UI가 클릭한 칸을 그대로 보내면 되고, 서버는 그 칸의 엔트리를 확인만 하면 된다.

배열 인덱스였다면 위험했겠지만 — 제거·정렬로 인덱스가 밀리는 사이에 요청이 도착하면 엉뚱한 아이템을 버린다 — `SlotIndex`는 03-1에서 **제거해도 재배치되지 않는 고정 번호**로 정의했으므로 그 경쟁이 없다.

### ★ 인스턴스를 재생성하지 않는다

`CreateInstance`를 다시 부르면 잔탄·내구도·`InstanceId`가 전부 초기화된다. **인스턴스의 `Outer`는 끝까지 `UEPItemInstanceSubsystem`이며 바뀌지 않는다** — 바뀌는 것은 "누가 그 핸들을 들고 있는가"뿐이다.

> 스택 아이템은 애초에 인스턴스가 없으므로 버리기가 정수 뺄셈 + 픽업 스폰으로 끝난다. "버렸다 주우면 같은 개체인가"라는 질문 자체가 성립하지 않는다.

### 세부

| 항목 | 처리 |
|---|---|
| 스폰 위치 | 캐릭터 전방 약 100cm, 아래로 라인 트레이스해 접지. 실패 시 발밑 |
| 벽 끼임 | 스폰 위치가 막혀 있으면 발밑으로 폴백. `AlwaysSpawn`으로 두되 위치를 보정 |
| 즉시 재획득 | `DropCooldown`(0.5초) 동안 `CanInteract()`가 false. 없으면 G를 누른 순간 E 프롬프트가 바로 떠서 실수로 다시 줍는다 |
| 클라 예측 | **하지 않는다.** 결과가 늦게 보여도 무해하고, 예측하면 롤백 처리가 필요해진다 |

`DropCooldown`이 서버에서 강제되려면 **Step 02의 4단계(`CanInteract()` 서버 재호출)가 반드시 있어야 한다.** 없으면 클라가 프롬프트만 회색으로 그리고 RPC는 통과한다.

---

## 03-6. 인스턴스 수명

| 시점 | 처리 |
|---|---|
| `UEPInventoryComponent::EndPlay` | 자기 엔트리의 유효 핸들을 전부 `Destroy(Handle)` |
| `AEPPickup::EndPlay` | 보유 핸들이 유효하면 `Destroy(Handle)` (01-4에서 구현됨) |
| 획득/버리기로 **이관 중** | **호출 금지** — 이관 프로토콜 순서를 지킨다 |
| 매치 종료 | 서브시스템 `Deinitialize()`가 최종 안전망 |

> **★ 사망 시 드랍(§8 미정 #4)을 넣을 때의 함정:** 드랍은 **반드시 `EndPlay`보다 먼저** 돌아야 한다. 순서가 뒤집히면 `EndPlay`가 핸들을 전부 `Destroy()`한 뒤 드랍이 빈 인벤토리를 뿌려 **"죽으면 아이템이 그냥 사라진다"** 로 나타난다. 원인이 인벤토리 로직이 아니라 수명 관리에 있어 추적이 오래 걸린다. 사망 처리에서 드랍을 먼저 호출하고, `EndPlay` 정리는 "그때까지 남아 있으면 지운다"는 안전망으로만 남긴다.

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | `COND_OwnerOnly` 누락 | 남의 가방이 전부 복제됨 (치트) | 03-2 |
| 2 | `SlotIndex` 없이 배열 인덱스 사용 | 줍고 버릴 때마다 아이콘이 다른 칸으로 튐 | 03-1 |
| 3 | 인스턴스를 슬롯 확보 **전에** 생성 | 고아 인스턴스 누수 | 03-3 |
| 4 | 부분 획득에서 `bClaimed` 안 되돌림 | 그 픽업을 아무도 못 줌 | 03-4 |
| 5 | `FlushNetDormancy()` 누락 | 클라 화면 개수만 옛날 값 | 03-4 |
| 6 | `Destroy()` 전에 핸들 안 비움 | 획득 직후 무기 잔탄 소실 | 03-4 |
| 7 | 버리기에서 `CreateInstance` 재호출 | 버렸다 주우면 만탄 | 03-5 |
| 8 | `MarkItemDirty` 누락 | 서버만 맞고 클라 갱신 안 됨 | 03-3 |
| 9 | 기존 스택보다 빈 슬롯을 먼저 채움 | 합칠 수 있는데 "가방 가득" | 03-3 |
| 10 | `DropCooldown`을 클라에서만 검사 | 버리자마자 재획득 가능 | Step 02의 4단계 필수 |
| 11 | 엔트리 포인터를 밖으로 반환 | 배열 재할당 후 댕글링. Step 00의 `FEPItemData*`와 같은 문제 | `FindBySlot`은 값 복사 (03-2) |
| 12 | `ClearLoot`이 버린 아이템까지 삭제 | `EP.Loot.Respawn` 시 플레이어 소지품이 사라짐 | Step 01의 `SpawnedPickups` 약참조 — **여기서 처음 검증 가능** |

---

## 이 단계에서 하지 않는 것

- 인벤토리 화면 UI → **Step 04** (이번엔 `OnInventoryChanged` 델리게이트만 노출)
- 장착/해제, `EquippedHandle` 사용 → **Step 05** (필드는 선언만)
- 사망 시 드랍 → §8 미정 #4
- 드래그앤드롭·정렬·아이템 이동
