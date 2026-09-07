# Step 03-B — 줍기 · 버리기 · 자동 착용

> 마스터 기획: `05_Loot_DOCS.md` (§4-6 · §4-7)
> **선행 구간: `05_Loot_03A_Core.md`** — 이 문서의 모든 함수가 그 위에 얹힌다
> **★ 15차에 통합 문서 `05_Loot_03_Inventory.md`(2692줄)를 둘로 쪼갠 것이다.**

---

## 목표

**주운 아이템이 인벤토리에 들어가고, G로 버리면 픽업이 되돌아 나온다.** 배낭을 주우면 자동으로 매지고, 버리면 안의 내용물이 통째로 따라 나간다.

> **★ 03-A가 끝나 있어야 한다.** 이 구간의 함수는 전부 03-A의 단일 쓰기 지점(`InsertEntry` · `RemoveSelf` · `AssignSortKey` · `MoveEntry` · `SetEntryCharges`)과 판정 함수(`CanFit` · `CanPlaceInSlot` · `GetEntryInSlot`) 위에 얹힌다.

**완료 조건 — 03-B 몫 (번호는 통합 문서의 것을 유지한다)**

- [ ] **1.** 주운 아이템이 인벤토리에 들어가고 클라이언트에 복제된다
- [ ] **7의 전반.** **배낭을 주우면 자동으로 매진다**
  > **★ 본체가 꽉 찬 상태에서도 매져야 한다** (13차, 함정 4y). 착용은 칸을 안 먹으므로 **자동 착용이 `CanFit`을 지나면 안 된다.** 본체를 경유하는 구현이면 여기서 실패한다 — `AddSubtree(INDEX_NONE, "Back", In)` 한 번으로 들어가는지 확인한다
- [ ] **4의 후반.** 칸이 모자라면 **픽업이 그대로 남는다** (`bClaimed` 되돌림)
- [ ] **8.** 배낭을 버리면 안의 아이템이 같이 나가고, **고아 엔트리가 남지 않는다** (`Dump`의 `Parent` 열)
- [ ] **9.** 배낭을 버렸다 다시 주우면 안의 아이템이 그대로 돌아온다 — `EntryId`는 새로 발급되되 **`Parent` 관계가 유지**된다
- [ ] **10.** 무기의 `Charges`가 버렸다 주워도 보존된다
  > **★ 12/30은 여기서 확인할 수 없다.** `Charges`를 12로 만들려면 발사가 필요하고 장착 경로는 Step 05다. **여기서는 *"`EP.Inv.Drop` 후 `EP.Loot.List`의 `Charges`가 보존되는가"* 까지만 본다** — 값 복사가 도는지는 그걸로 증명된다
- [ ] **11.** 버린 직후 0.5초 동안 그 픽업이 회색 프롬프트로 표시되고 서버가 거부한다
- [ ] **12.** 스폰 직후 **상의·하의를 입고 시작한다** (`StartingEquipment`)
- [ ] **★ 배낭에 아이템 4개 이상을 넣고 순서를 섞은 뒤** 버렸다 다시 주우면 **그 순서 그대로다** — `Parent` 관계뿐 아니라 **`SortKey`도** 살아난다 (11차, 완료 조건 20)
  > **개수와 "섞기"를 조건에 적는 이유:** 두세 개면 우연히 맞아서 **통과했는데 안 고쳐진 상태**가 된다

**★ Step 02에서 이월된 검증 2건** — `Status/LOOT_STATUS.md`가 *"Step 03의 `DropCooldown`이 같은 경로를 쓰므로 그때 함께 검증"* 으로 넘긴 것이다. **번호를 매기지 않는다.**

- [ ] **사거리 밖 거부** (02 완료 조건 3) — `EPGA_Interact.cpp:69-75`가 한 번도 돌은 적이 없다. `EP.Inv.Drop`으로 버리고 뒤로 물러나면 정상 플레이로 재현된다
- [ ] **동시 F 경쟁** (02 완료 조건 4) — `bClaimed` 선점이 도는가. 둘째가 `Client_OnInteractFailed`를 받고, **거부당한 쪽의 인벤토리가 비어 있어야** 한다(`EP.Inv.Dump`)

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

    // ★ 2단계다. ①은 "슬롯이 비었는가", ②는 "칸이 남았는가" — 판정 기준이 달라 섞지 않는다
    // ① 장비 슬롯 시도 (아이템의 SlotPriority 순서대로 빈 곳)
    int32 NewId = Inv->TryAutoEquip(Payload);

    // ② 실패했으면 컨테이너 (본체 → 매고 있는 배낭. 순서를 뒤집으면 배낭부터 차서 본체가 빈다)
    if (NewId == INDEX_NONE)
        for (int32 C : Inv->GetInsertionOrder())        // 본체(0칸) → 외투 → 상의 → 하의 → 배낭 …
            if ((NewId = Inv->AddSubtree(C, NAME_None, Payload)) != INDEX_NONE) break;
    // Payload는 Step 01의 `FEPItemState State`가 이 단계에서 바뀐 것 (아래)
    //
    // ★★ GetInsertionOrder()는 UEPLootDeveloperSettings::ContainerOrder를 그대로 반환하지 않는다.
    //     반환형이 TArray<int32>(컨테이너 EntryId)이고 ContainerOrder는 TArray<FName>(슬롯 이름)이며,
    //     무엇보다 ContainerOrder에는 본체가 없다 — 본체는 슬롯이 아니기 때문이다.
    //
    //       TArray<int32> Out;  Out.Add(INDEX_NONE);      // ★ 본체. 빠뜨리면 테스트 중(MaxSlots=10) 완료 조건 2~6이 전부 걸린다
    //       for (const FName& S : GetDefault<UEPLootDeveloperSettings>()->ContainerOrder)
    //           if (const int32 Id = GetEntryInSlot(INDEX_NONE, S); Id != INDEX_NONE) Out.Add(Id);
    //
    //     완료 조건 2~6이 전부 본체 위에 서 있어 빠지면 03-B에서 즉시 걸린다.

    if (NewId == INDEX_NONE)                           // ★ if(NewId)로 쓰면 안 된다 — 아래
    {
        bClaimed = false;
        OutReason = NSLOCTEXT("EP", "PickupNoRoom", "가방에 자리가 없습니다.");
        return false;
    }

    // ★ TryAutoEquip은 위 ①에서 이미 끝났다 — 여기서 다시 부르지 않는다 (03-6)
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
// 전제(출처가 아니라 **모양**이다 — 13차): In은 전위 순회 배열이고 In[0]이 루트이며
//       루트가 Parent = INDEX_NONE · SlotId = NAME_None · SortKey = 0 으로 정규화돼 있다.
//       RemoveEntry가 그 형태를 만든다(03-2 계약). StartingEquipment처럼 손으로 만든
//       원소 1개짜리 배열도 같은 모양이면 유효하다 — 출처로 적으면 계약 위반처럼 읽힌다
int32 UEPInventoryComponent::AddSubtree(int32 Parent, FName SlotId,
                                        const TArray<FEPInventoryEntry>& In)
{
    if (In.Num() == 0) return INDEX_NONE;

    // 자식이 없으면 단일 아이템이다 → AddItem이 bFungible을 본다
    // ★ 슬롯으로 가는 것은 합칠 수 없다 — 합치기는 "이 개체를 없앤다"이고 슬롯은 자리다
    if (In.Num() == 1 && SlotId.IsNone())
        return AddItem(Parent, In[0].ItemId, In[0].State);

    ensureMsgf(!IsFungible(In[0].ItemId),
               TEXT("[Inventory] 자식을 가진 fungible 아이템: %s"), *In[0].ItemId.ToString());

    // ★★ 칸 검사는 "수납일 때만" 돈다 — MoveEntry 검사 5와 같은 조건이다 (13차)
    //   착용 중인 것은 칸을 안 먹는데(GetUsedSlots가 건너뛴다) 착용하러 가는 길에만
    //   칸을 요구하면 "등이 비었는데 배낭을 못 맨다"가 된다. 본체가 0칸이 되면 영구히 못 맨다
    if (SlotId.IsNone() && !CanFit(Parent, In[0].ItemId)) return INDEX_NONE;   // 루트만 — 아래
    // ★★ 슬롯이면 배치 자격을 본다 — MoveEntry 검사 2·3·4와 같은 함수다 (13차, 03-2)
    //   호출자(TryAutoEquip)가 미리 확인하지만, §7-3 부착이 두 번째 호출자로 예고돼 있다
    if (!CanPlaceInSlot(Parent, SlotId, In[0].ItemId)) return INDEX_NONE;

    FScopedInventoryNotify Guard(this);
    TMap<int32, int32> OldToNew;
    for (const FEPInventoryEntry& Src : In)
    {
        const bool  bRoot     = (Src.ParentEntryId == INDEX_NONE);
        const int32 NewParent = bRoot ? Parent
                                      : OldToNew.FindRef(Src.ParentEntryId);   // ★ 부모가 먼저 들어와 있다
        const FName NewSlot   = bRoot ? SlotId : Src.SlotId;   // ★ 루트의 자리는 호출자가 정한다

        const int32 NewId = InsertEntry(NewParent, Src.ItemId, Src.State, NewSlot);

        // ★★ 루트가 아니면 스냅샷의 키를 그대로 복원한다 (11차)
        //   부모가 방금 만들어진 빈 컨테이너라 형제 충돌이 없다 —
        //   그래서 버린 배낭을 되주우면 내용물 순서가 그대로 살아난다.
        //   루트는 InsertEntry가 발급한 "이 컨테이너 맨 뒤"를 쓴다 (Src.SortKey는 0)
        if (Src.ParentEntryId != INDEX_NONE) AssignSortKey(NewId, Src.SortKey);

        OldToNew.Add(Src.EntryId, NewId);
    }
    return OldToNew[In[0].EntryId];
}
```

> **★ 자식 키 복원이 `InsertEntry`의 "맨 뒤 발급"을 덮어쓴다.** 덮어쓰지 않고 `InsertEntry`가 발급한 값을 그냥 두면 **순서는 `In` 배열 순서로 정해지는데, `In`은 `RemoveChildrenRecursive`가 `Entries.Items`를 순회해 만든 것이라 FastArray 내부 순서다.** 화면 순서가 아니다. 증상은 *"배낭을 버렸다 주우면 내용물이 뒤섞인다"* 이고, **아이템이 두세 개면 안 보인다.**

> **`InsertEntry`를 쓰는 것이지 `AddItem`을 쓰는 게 아니다.** 자식에 `AddItem`을 부르면 둘이 동시에 깨진다 —
> ① **`bFungible` 합치기**가 돌아 배낭 속 현금이 다른 컨테이너 현금과 합쳐진다
> ② **`CanFit`이 넘겨받은 `Parent` 기준**으로 검사하는데, 자식은 루트 **안**으로 들어간다
> 그래서 03-2가 발급·삽입·`MarkItemDirty`만 하는 `InsertEntry`를 따로 뒀다.

#### ★★ 목적지는 `(Parent, SlotId)` 쌍이다 — 기본값을 주지 않는다 (13차 신설)

**초안은 `AddSubtree(Container, In)`으로 위치를 반쪽만 받았다.** 인접한 두 함수는 이미 둘 다 받는다.

```cpp
int32 InsertEntry(int32 Parent,  FName ItemId, const FEPItemState&, FName SlotId);   // 둘 다
bool  MoveEntry  (int32 EntryId, int32 NewParent, FName NewSlotId);                  // 둘 다
int32 AddSubtree (int32 Container, const TArray<FEPInventoryEntry>& In);             // ← 하나뿐이었다
```

**결함 둘이 같은 구멍에서 나왔다.** *"슬롯 정보를 어디서 얻나"* 에 답이 없으니 한쪽은 **스냅샷에서 몰래 새어 들어오고**(위 절), 다른 한쪽은 **아예 표현이 안 됐다**(자동 착용).

| | 뒤에 붙이고 기본값 (`FName RootSlotId = NAME_None`) | **`(Parent, SlotId)`를 앞에 나란히** |
|---|---|---|
| 읽힘 | *"옵션이 하나 붙었다"* | *"목적지를 받는다"* — `MoveEntry`와 같은 어휘 |
| 빠뜨리면 | **조용히 수납이 된다** (같은 버그가 재발) | **컴파일이 안 된다** |

**기본값을 주면 안 된다.** 이 버그가 난 원인이 정확히 *"아무도 슬롯을 안 정해서 스냅샷 값이 흘러 들어간 것"* 인데, 기본값은 그 상태를 **다시 문법으로 허용한다.**

**호출부는 셋뿐이다.**

```cpp
AddSubtree(C,          NAME_None, Payload);   // OnInteract ② — 컨테이너 수납
AddSubtree(INDEX_NONE, S,         Payload);   // TryAutoEquip — 몸 슬롯 (03-6)
AddSubtree(INDEX_NONE, NAME_None, Sub);       // §7-1 월드 컨테이너에서 꺼내기
```

**칸 검사 분기가 `MoveEntry` 검사 5와 한 글자도 다르지 않다** — 새 개념이 아니라 **같은 규칙의 두 번째 적용**이다. §7-3 부착물도 `AddSubtree(총Id, "Optic", Payload)`로 그냥 성립한다.

> **`TryAutoEquip`이 `InsertEntry`를 직접 쓰는 대안은 쓰지 않는다** — `OldToNew` 재매핑을 복제하게 된다.

#### ★★ 칸 검사가 루트만인 것은 축소가 아니라 **정확한** 것이다 — 늘리지 마라

재매핑 규칙상 `Src.ParentEntryId == INDEX_NONE`인 원소(루트)만 `Parent`를 부모로 받고, 나머지는 `OldToNew`가 준 부모 — 즉 **서브트리 내부** — 를 받는다. **자식이 `Parent`의 칸을 먹는 경우가 문법적으로 없다.**

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
  > **★ 13차에 `&& SlotId.IsNone()`이 붙었다.** 슬롯으로 가는 것은 합칠 수 없다 — 합치기는 *"이 개체를 없애고 기존 개체의 숫자를 올린다"* 인데 **슬롯은 자리이지 숫자가 아니다.** 그리고 `bFungible`이면서 슬롯에 들어가는 아이템은 지금 없지만, 조건을 안 걸면 **그런 DT 행이 생기는 날 조용히 슬롯이 안 채워진다**
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

AEPPlayerController* PC = Cast<AEPPlayerController>(Owner->GetController());
if (!PC) return;

PC->SetInteractPrompt(
    Interactable ? (bCan ? Interactable->GetInteractText() : Reason) : FText::GetEmpty(),
    bCan);
```

**기존 `:84-95`를 지우고 위 블록의 뒤 절반으로 대체한다.** 교체가 아니라 추가로 읽으면 `CanInteract`이 한 번 더 불린다 — 이번 구현의 전제가 *"판정을 조기 반환 앞으로 올린다"* 이므로 아래에 같은 호출이 남아 있으면 안 된다.

- 기존의 `if (!FocusedActor) { 빈 프롬프트; return; }` 갈래가 삼항으로 흡수된다. `UpdateFocus`는 `Implements<UEPInteractable>()`일 때만 `NewFocus`를 채우므로(`EPInteractionComponent.cpp:74`) **`Interactable == nullptr`과 `NewFocus == nullptr`은 같은 조건이다**
- 기존의 `if (!Interactable) return;`도 같은 이유로 사라진다. 남겨두면 도달 불가 분기다

> `TickInterval = 0.1f`이라 초당 10회 `CanInteract` 호출이고 그 함수는 비교 두 번이다. **비용이 문제가 아니라 "포커스 불변 = 상태 불변"이라는 전제가 틀렸다는 게 문제다.** `bLastCanInteract`는 `UEPInteractionComponent`의 private `bool` 하나.

> **이걸 안 짚고 가면:** 서버 거부는 도는데 프롬프트만 안 변한다. 증상이 *"가끔 못 줍는데 UI는 줍을 수 있다고 한다"* 이고 **`DropCooldown` 로직이 아니라 HUD를 파게 된다.** Step 02가 남긴 두 개의 "지금은 도달 불가" 갈래가 여기서 동시에 첫 소비자를 만난다.

---

## 03-6. 배낭 장착 — 줍고 빈 슬롯이면 자동 착용

**GAME.md 장비 절에 "배낭 슬롯 1"이 있는데 조작 방법이 어느 문서에도 없었다.** Step 03의 완료 조건("배낭을 매면 별도 칸이 열린다")이 이것에 의존하므로 여기서 정의한다.

```cpp
// 별도 필드가 없다. SlotId == "Back"이 곧 "이 배낭은 등에 있다"이다 (03-2)
// ★★ RPC가 없다 (14차). 착용 표면은 Server_MoveEntry(04-B) 하나이고, Step 03의 착용은
//    전부 서버 내부(TryAutoEquip)이거나 커맨드(EP.Inv.Move)다 — 03-2 별도 절

// 획득 ①단계. SlotPriority를 순회해 첫 빈 슬롯에 넣는다. 배낭 전용이 아니다
int32 TryAutoEquip(const TArray<FEPInventoryEntry>& In);
```

```
줍는다 → TryAutoEquip
    아이템의 SlotPriority를 순서대로 훑는다        (배낭이면 ["Back"] 하나)
        GetEntryInSlot(INDEX_NONE, S) == INDEX_NONE  → AddSubtree(INDEX_NONE, S, In)
    전부 차 있으면 INDEX_NONE 반환
        → ②단계로 넘어가 GetInsertionOrder()대로 컨테이너에 들어간다
          (이미 매고 있으면 그냥 아이템으로 들어간다 — 교체는 벗고 다시 줍기)
```

> ### ★★ 본체를 경유하지 않는다 (13차)
>
> **초안은 *"`AddSubtree` 후 루트에 `MoveEntry(id, -1, S)`"* 였다.** 슬롯에 넣으려면 검사 3(몸 슬롯은 `ParentEntryId == INDEX_NONE`) 때문에 **본체를 한 번 거쳐야 하고**, 그 삽입이 `CanFit(본체)`을 지난다.
>
> ```
> 본체 8/10 사용, Back 비어 있음, 배낭 A(SlotSize 15)를 줍는다
>   ① TryAutoEquip → 본체에 넣기 시도 → 8 + 15 > 10  →  실패
>   ② 컨테이너      → 전부 실패
>   ⇒ "가방에 자리가 없습니다"      ← 등이 비었는데 못 맨다
> ```
>
> **매고 나면 칸을 안 먹는데(`GetUsedSlots`가 슬롯을 건너뛴다) 매러 가는 길에만 칸을 요구한다.** 그리고 **본체가 0칸이 되면 영구히 아무것도 자동 착용되지 않는다.**
>
> **`AddSubtree(Parent, SlotId, In)`이 한 번에 넣는다** — `SlotId`가 있으면 `CanFit`을 건너뛴다(03-4). `MoveEntry` 검사 5와 같은 조건이고, **중간 상태가 아예 없어져 되돌릴 것도 없다.**

> **배낭 전용 함수를 만들지 않는 이유:** 무기·상의·헬멧이 들어올 때 같은 함수가 넷이 된다. 일반형은 *"`SlotPriority`를 순회하며 빈 첫 슬롯에 `MoveEntry`"* 한 줄짜리 루프이고, **Step 03에서는 배낭 행의 `["Back"]` 하나만 돌아 배낭 전용 함수와 동작이 정확히 같다.** 배낭 자동 착용을 Step 03에 두는 결정(3차 확정)은 그대로다.

- **자동 착용을 고른 이유:** Step 04(UI)가 없어도 Step 03만으로 검증된다. 수동 착용은 조작 UI가 있어야 하는데 그건 다음 단계다
- **교체는 지금 넣지 않는다.** "벗고 다시 줍기"로 충분하고, 교체를 넣으면 *현재 배낭 내용물을 어디로 옮기나*가 따라온다. **★ 그 "벗기"의 경로는 아래에서 확정한다**
- **`RemoveEntry()`가 이 번호도 정리한다**(03-2). 배낭을 버렸는데 죽은 번호가 남으면 `GetCapacity`가 0을 돌려주고 Step 04에 **유령 구획**이 남는다

#### ★★ 벗기는 `MoveEntry`가 아니라 `Server_DropItem`이다

*"벗고 다시 줍기"* 의 **벗기**가 그동안 경로 없이 쓰였다. 자연스러운 구현은 본체로 옮기는 것인데, **그러면 벗기가 본체 적재량에 걸린다.**

```
벗기 = MoveEntry(배낭, INDEX_NONE, NAME_None)
        → 검사 5:  GetUsedSlots(본체) + SlotSize(배낭) ≤ MaxSlots
```

**착용 중인 배낭은 칸을 안 먹지만**(`SlotId != None`이라 `GetUsedSlots`가 건너뛴다 — 03-3) **벗는 순간 먹는다.** 그리고 `05_Loot_DOCS.md` §4-6이 정한 `ContainerCapacity < SlotSize` 규칙 때문에 **용량을 키우려면 `SlotSize`를 키워야 하므로, 키울수록 벗기 어려워진다.**

| `MaxSlots` = 10(테스트값)일 때 `SlotSize` | 결과 |
|---|---|
| 8 | 본체가 **2칸 이하**로 차 있을 때만 벗어진다 |
| 11 | **영원히 못 벗는다** — 용량 상한이 9칸으로 묶인다 |

> **★★ 13차 확정으로 이게 "가끔"이 아니라 "항상"이 됐다.** 본체가 **0칸**이 되므로(03-3 용량표) `GetUsedSlots(본체) + SlotSize ≤ 0`은 **어떤 배낭에도 거짓**이다. 확정한 배낭 A(`SlotSize 15`)·B(`10`)는 **테스트값 10에서도 이미 못 벗는다.** 아래 결정이 없으면 벗기 경로가 **아예 존재하지 않는다.**

**증상이 나쁘다.** `MoveEntry`가 조용히 `false`를 돌려주므로 *"벗기가 안 먹는다"* 로 나타나고, **재현 조건이 본체 적재량**이라 함정 4l(제자리 이동)과 구분이 안 된다.

**결정: 벗기는 `Server_DropItem(배낭EntryId)`이다** (2026-08-24).

- **칸 검사가 없다** — 발밑 픽업으로 나가므로 `SlotSize`가 본체 용량에서 완전히 풀린다. 배낭 `SlotSize`를 15까지 잡을 수 있는 근거가 이것이다
- **내용물이 따라간다** — `RemoveEntry` → 스폰 경로라 서브트리 전체가 픽업에 실린다(03-5). 본체로 옮겼다면 *"배낭 안 아이템은 어디로 가나"* 가 따라왔다
- **코드가 이미 있다** — 03-5의 `Server_DropItem` 그대로다. 새 RPC도 새 경로도 없다
- 타르코프도 넣을 자리가 없으면 바닥이다

> **★ 이 결정이 `Server_EquipBackpack`을 없애는 첫 번째 조각이었다 (14차).** 벗기가 드랍이면 그 RPC는 **매기 전용**이 되고, 매기는 Step 03에서 서버 내부(`TryAutoEquip`)라 클라 호출자가 사라진다. 나머지 조각은 03-2에 있다 — 04-B가 `Server_MoveEntry`를 여는 이상 좁은 래퍼가 좁히는 것이 없다.

> **슬롯이 셋(주무기/보조/배낭)이 되면 `TMap<EEPEquipSlot, int32>`로 간다** — §8 미정 #5. 지금은 **필드 둘**이다. 원소가 둘인 맵을 만들 이유가 없다.

---

### ★★ 시작 장비 — 스폰 시 기본 상의·하의를 입고 시작한다 (13차 신설)

**본체가 0칸이므로 아무것도 안 입고 스폰하면 첫 아이템조차 못 줍는다.** 수납 용량이 전부 착용 컨테이너에서 나오기 때문이다(03-3 용량표).

```cpp
// UEPLootDeveloperSettings — ContainerOrder / BodySlots 옆 (셋 다 임시 자리 — §8 미정 #10)
UPROPERTY(config, EditAnywhere, Category = "Inventory")
TArray<FName> StartingEquipment;      // ["Shirt_Basic", "Pants_Basic"]
```

캐릭터 `BeginPlay`(서버)에서 각 `ItemId`를 원소 1개짜리 배열로 만들어 **`TryAutoEquip`을 그대로 부른다.** 새 경로가 아니다 — `AddSubtree(INDEX_NONE, SlotId, In)`가 이미 그 모양이고, 슬롯 배정은 아이템의 `SlotPriority`(`["Torso"]` / `["Legs"]`)가 답한다.

- **`UEPLootDeveloperSettings`인 이유:** **테스트 환경의 임시 형태**라서다. 로비가 생기면 *"로비에서 고른 옷을 입고 나온다"* 로 대체되고, 그때 바뀌는 것은 **이 배열을 채우는 쪽**이지 `TryAutoEquip`이 아니다.
  > **★ 6차를 근거로 인용하지 않는다** (13차 답변 ＋ 14차). 6차의 *"전역 데이터 참조는 `UDeveloperSettings`"* 는 `ItemDataTable`처럼 **가리키는 것**을 말한 것이고, 이 필드는 **콘텐츠**다. 지금 모양은 `UGameMapsSettings::GlobalDefaultGameMode`(`GameMapsSettings.h:216`) — *"달리 지정되지 않았을 때 쓰는 값"* — 에 가깝고, **최종 자리는 `BodySlots`·`ContainerOrder`와 함께 `UEPPawnInventoryData`(DataAsset)다**(§8 미정 #10)
- **본체가 10칸인 동안은 급하지 않다.** 0칸으로 내리는 순간 없으면 아무것도 못 줍는다
- **03-B 범위다** — `TryAutoEquip`과 같은 함수를 쓰고, `TryAutoEquip`은 `AddSubtree`(03-B)에 의존한다 (체크포인트 절)

---

## 함정 — 03-B 몫


| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| **3** | **스냅샷을 `RemoveEntry`보다 먼저 뜸** | 장착 무기를 버렸다 주우면 **만탄.** 증상이 Step 05의 `InitAmmo` 문제와 똑같아 오진한다 | `RemoveEntry(Id, &Out)`이 반환 (03-5) ★★ |
| **3c** | **`AddItem`으로 픽업을 받음** | 배낭을 버렸다 주우면 **안의 아이템이 전부 증발** | `AddSubtree` + `EntryId` 재매핑 (03-4) ★★ |
| **3f** | **`RemoveEntry`를 스폰보다 먼저** | 스폰이 실패하면 **서브트리가 통째로 증발한다.** 배낭이면 내용물까지 | 스폰 → 제거 → `InitPickup` (03-5) ★★ |
| 4b | **`if (AddItem(...))`로 성공 검사** | `INDEX_NONE`(-1)이 truthy라 **실패를 성공으로 읽는다** | `!= INDEX_NONE` (03-4) |
| **4n** | **스냅샷에서 자식 `SortKey`까지 버림** (루트와 같이 0으로) | 배낭을 버렸다 주우면 **내용물 순서가 뒤섞인다.** `AddSubtree`가 `In` 배열 순서를 따르는데 그건 `Entries.Items` 순회 결과라 **화면 순서가 아니다.** 아이템이 두세 개면 안 보인다 | `bIsRoot`일 때만 `SortKey = 0` (03-2 ②) ＋ `AddSubtree`가 자식 키 복원 (03-4) ★★ |
| **4y** | **★★ 자동 착용이 `CanFit`을 지남** | 본체를 경유해 슬롯에 넣으면 삽입이 칸 검사를 지난다. **매고 나면 칸을 안 먹는데 매러 가는 길에만 칸을 요구한다** — *"등이 비었는데 배낭을 못 맨다"*. 본체가 0칸이 되면 **영구히 아무것도 자동 착용되지 않는다** | `AddSubtree(Parent, SlotId, In)`이 슬롯이면 `CanFit`을 건너뛴다 (03-4·03-6) ★★ |
| 5 | 획득 실패에서 `bClaimed` 안 되돌림 | 그 픽업을 아무도 못 줌 | 03-4 |
| 6c | 획득 시 **배낭부터** 채움 | 본체가 늘 비어 있고 배낭을 벗으면 다 나간다 | 본체 → 배낭 순 (03-4) |
| 9 | `DropCooldown`을 클라에서만 검사 | 버리자마자 재획득 가능 | Step 02의 4단계 필수 |
| **9b** | **`DropCooldownEndTime`을 복제 안 함 / 타이머 핸들로 둠** | 서버 거부는 도는데 **회색 프롬프트가 영영 안 뜬다.** 증상이 UI 문제로 보여 HUD를 판다 | 복제되는 `float` + 서버 월드 시간 (03-5) |
| **9c** | **`UpdateFocus`가 포커스 변화에만 갱신** | 버린 픽업을 계속 보면 쿨다운이 끝나도 회색이 안 풀린다 | 조기 반환을 **결과 기준**으로 (03-5) |
| **9d** | **드랍 실패를 조용히 return** | Step 02가 세운 "실패를 삼키지 않는다"가 깨진다. RPC에는 GAS 회신이 없다 | `Client_OnInventoryActionFailed` (03-5) |
| **9e** | **인벤토리 조작을 GAS 어빌리티로** | `FGameplayEventData`에 `int32`가 없어 `EventMagnitude` 인코딩이나 `TargetData` 서브클래스를 산다. 실제로 쓰는 GAS 기능은 `ActivationBlockedTags` 하나 | 서버 RPC + `CanMutateInventory()` (03-5) |
| 10 | `ClearLoot`이 버린 아이템까지 삭제 | `EP.Loot.Respawn` 시 플레이어 소지품이 사라짐 | Step 01의 `SpawnedPickups` 약참조 — **여기서 처음 검증 가능** |
| **10c** | **`AddSubtree`가 서브트리 전체 칸을 검사하도록 "고침"** | 배낭을 되주울 때 자기 내용물이 본체 칸을 먹는 것으로 계산돼 **완료 조건 9가 깨진다** | 루트만 검사가 정확하다 (03-4) |
| **10e** | **★ `SlotSize`에 하한이 없다** | 기본값이 1일 뿐이고 `ClampMin`도 `IsDataValid` 검사도 없다. **본체가 0칸이 되면** `CanFit`이 `0 + 0 <= 0` → **참**이라 `SlotSize = 0`인 아이템만 0칸 본체에 무한히 들어가고, `GetInsertionOrder`의 맨 앞이 본체라 **컨테이너에는 절대 안 들어간다.** 크래시도 경고도 없이 *"이 아이템만 가방에 안 들어간다"* | `IsDataValid`에 `SlotSize >= 1` (`05_Loot_00_ItemCore.md`). **컨테이너 깊이 증명이 쓰는 전제이기도 하다** |

> **★★ 표시가 붙은 것 중 넷은 정상 플레이에서 반드시 나오고 증상이 원인을 가린다 — 4k(잔탄이 조용히 사라짐) · 4q(배치가 조금씩 무너짐) · 4v(장착이 무동작인데 `true`) · 4y(등이 비었는데 못 맨다).**
>
> **3b는 시나리오가 아니라 계약으로 남아 있다** — 9차 확정으로 *"배낭 속 무기 장착"* 은 표현 불가능해졌지만(장착 ＝ `ParentEntryId == INDEX_NONE`), ① 착용 컨테이너 안에 또 컨테이너가 들어가는 구조가 그대로 있고 ② 핫바 5~0이 컨테이너 안 아이템을 들 수 있게 되면 같은 모양이 되살아난다 (`EquipmentSlots.md` §10 미정 #7).

---

## 변경 이력

| 날짜 | 무엇 |
|---|---|
| 2026-08-26 (15차 — **파일 분할**) | **통합 문서 `05_Loot_03_Inventory.md`(2692줄)를 03-A / 03-B 둘로 쪼갰다.** 8차의 *"파일을 쪼개지 않는다"* 를 뒤집는다 — 근거였던 *"`RemoveEntry`가 경계에서 갈린다"* 가 두 번 무너졌다(13차가 가운데 구간 삭제 / 제거 경로 셋이 전부 03-B). **직접적 계기는 열 함수의 본문이 통째로 빠진 것을 아무도 못 본 것**이다. **03-A-부록(기본 함수 열)은 A 문서에 있다** — `FindEntry`·`ContainsEntry`·`FindFungibleEntryId`·`GetEntryInSlot`·`GetEquippedEntryId`·`RemoveSelf`·`AssignSortKey`·`KeySpace_Min`·`KeySpace_NextAbove`·`KeyOf` 본문. 함정표는 대응 열로 라우팅(A 44 / B 16, 겹치는 4는 양쪽). **완료 조건 번호는 통합 문서의 1~20을 유지한다** |
| 2026-08-25 (14차 — 사용자 지적) | **★★ `Server_EquipBackpack`을 없앴다.** 13차가 *"Step 03에 호출자 0개"* 까지 찾고 **04-A로 이동**을 골랐는데, 근거였던 `EP.Inv.Equip`이 **콘솔 커맨드**라 이 문서 자신의 규칙(*"커맨드는 내부 함수를 직접 부른다"*)대로면 **옮긴 자리에도 호출자가 0개**다. 그리고 9차의 근거 *"좁은 RPC가 넓은 RPC보다 낫다"* 는 **04-B가 `Server_MoveEntry`를 여는 이상 성립하지 않는다** — 넓은 문이 열린 뒤의 좁은 문은 표면을 안 줄인다. 배낭이 특별할 근거도 없다(`SlotPriority` ＋ `BodySlots`가 상의·하의·외투·배낭을 같은 모양으로 만든다 — **`TryAutoEquip`에 이미 적용한 규칙이 RPC 이름에만 안 적용돼 있었다**). 03-2 별도 절 재작성, 03-6 RPC 삭제, 함정 9f를 `Server_*` 일반으로, `EP.Inv.Equip`(04-A)도 `EP.Inv.Move`의 별칭이라 폐기 |
| 2026-08-26 (15차) | **03-9에 커맨드 구현 형태를 적었다** — 여태 시그니처만 있었다. 자리는 **`Private/Inventory/EPInventoryDebugCommands.cpp` 전용 파일**(Step 01의 `EPLootDebugCommands.cpp`와 같은 형태). `FindInv(World, Args, ArgIdx)` 헬퍼, `ECVF_Cheat` ＋ `#if !(UE_BUILD_SHIPPING \|\| UE_BUILD_TEST)`, **반환값 로그 필수**(안 찍으면 결함 A-1의 검증이 성립하지 않는다), 서버 전용 가드의 예외는 `Dump` 둘. **`Owner` 타입을 코드에 맞춘다** — `TObjectPtr<UEPInventoryComponent>`이므로 `PostReplicatedReceive`에 `Cast`가 필요 없다(11차의 `UActorComponent` 판정을 뒤집는다 — Lyra는 리스트를 여러 컴포넌트가 쓸 여지를 둔 형태이고 우리는 소유자가 하나로 고정이다). **정의는 `.cpp`** — `FEPInventoryList`가 컴포넌트보다 위에 있어 인라인 본문은 불완전 타입이다 |
| 2026-08-26 (15차 — 04 대조) | **`CanPlaceInSlot`의 네 번째 인자 `IgnoreEntryId`를 예고한다** — 04-7의 `SwapEntries`가 **상대가 차지한 슬롯**으로 들어가 검사 4가 언제나 거짓이 된다. 붙이는 것은 Step 04(검증 표면은 소비자를 따라간다). 그리고 **UI가 `SlotPriority`·`BodySlots`를 직독하지 않는다** — 04-1 ②의 판정식 목록을 `CanFit`/`CanPlaceInSlot`으로 교체했다 |
| 2026-08-25 (13차 **답변 반영** — `Review/05_Loot_REVIEW13_Answer.md`) | **★★ `MoveEntry`도 키를 늦게 구하고 있었다** — `InsertEntry`와 같은 결함(함정 4x가 두 함수로 넓어졌다). 재부모 **전에** 구한다. **`FScopedInventoryNotify` 정의가 소스에 없다** — `.cpp` 상단으로 위치 확정(**지금 코드가 컴파일 안 된다**). **`CanPlaceInSlot(Parent, SlotId, ItemId)` 추출** — 검사 2·3·4를 `AddSubtree`·§7-3과 공유. *"슬롯 진입 경로가 둘"* 이 API에서 거짓이었다. **`IsDataValid`에 `SlotSize >= 1`** — 본체 0칸에서 `0+0<=0`이 참이라 0칸 아이템이 무한히 들어간다. **`EP.Inv.Add`에 `[Container]`** — 없으면 `MaxSlots=0` 전환이 03-A 완료 조건 9개를 죽인다. **★★ 옛 03-B(배낭) 구간 삭제** — `Server_EquipBackpack`이 Step 03에 호출자 0개라 **04-A로**. 구간이 셋에서 **둘**로. **래퍼 기각 근거 교체** — *"엔진 관례와 싸운다"* 는 **거짓이다**(`MarkAbilitySpecDirty`가 감싼다). 근거는 *"안에 할 일이 없다"*. **`Cap == SlotSize` 근거 교체** — 익스플로잇이 아니라 *"비용 0 ＋ 세 곳의 전제 ＋ 되돌릴 손잡이"*. 함정 4v 증상에 `MarkArrayDirty`·`IDCounter` 한 줄. 완료 조건 18의 **32,764회**(답변의 32,763도 틀렸다 — 직접 계산), 완료 조건 4 문구, `AddSubtree` 전제를 **출처가 아니라 모양**으로 |
| 2026-08-25 (13차 검수 — `05_Loot_REVIEW_Inventory.md`) | **★★ `AddSubtree`가 위치를 반쪽만 받고 있었다.** `AddSubtree(Parent, **SlotId**, In)`으로 목적지를 나란히 받고 **기본값을 주지 않는다** — `InsertEntry`·`MoveEntry`와 같은 어휘. 슬롯이면 `CanFit`을 건너뛴다(함정 4y: *"등이 비었는데 배낭을 못 맨다"*, 본체 0칸이면 영구). **루트 스냅샷이 `SlotId`도 버린다**(함정 4z) — 안 버리면 `AddSubtree`가 검사 3·4를 우회해 슬롯을 채웠다. **`MoveEntry`가 `FindEntry` 복사본에 쓰고 있었다**(함정 4v — 무동작인데 `true`). **`SortKey` 함수의 `INDEX_NONE` 센티널이 키 −1과 충돌**(함정 4w) → `bool` ＋ out 파라미터. **`InsertEntry`가 키를 배열에 넣기 전에 발급**(함정 4x). **`PostReplicatedReceive`가 컴파일 안 됐다** — `Owner`가 `TObjectPtr<UActorComponent>`(11차)인데 캐스트가 없었다. **가드 목록을 없애고 "단일 쓰기 지점 다섯"에 건다** — 별도 목록이라 두 번 낡았다(사설 래퍼는 기각 — 근거는 *"관례"* 가 아니라 *"안에 할 일이 없다"* 다. **엔진은 감싼다**: `MarkAbilitySpecDirty`). **`EP.Inv.Move` 신설** — `MoveEntry`가 Step 04까지 한 줄도 안 돌고 있었다(완료 조건 15·17). `Server_EquipBackpack`에 `UFUNCTION` ＋ 게이트(함정 9f). `GetOwner<AEPCharacter>()` 무보호 역참조(§7-1). **완료 조건 18 ②의 관찰 문구 교정** — 맨 앞 20회로는 재정규화가 안 돈다(32,764회). **용량표 확정** — 본체 **0칸**(테스트 중 10), 상의 `11-10` / 하의 `6-5` / 배낭A `15-12` / 배낭B `10-8`, **넣기 `≤` 와 데이터 `<` 는 다른 식이다**. **`StartingEquipment` 신설**(03-B). 함정표 번호순 재정렬 ＋ 꼬리 주석 교정 |
| 2026-08-23 (12차 검수) | **★★ 무한 재귀 제거 (함정 4t).** 재정규화 가드에서 `PrevEntryId != INDEX_NONE`이 빠져 **맨 앞 드래그가 서버를 죽였다** — 경계(`bOutOfRange`)와 고갈(`bNoGap`)을 분리하고 `ReorderEntryInternal(bRetry)` ＋ `ensure`로 종료를 문법으로 보장. **제자리 드롭 조기 반환 (4u).** 키 공간 헬퍼에 **`KeySpace_` 접두어**(합치는 것으로는 스코프 혼동을 못 막는다). **★ *"UE에 선례가 없다"* 가 거짓이었다** — `FUIFrameworkStackBoxSlot::Index`(주석까지 같다) · `FUIFrameworkGameLayerSlot::ZOrder`. 조밀/희소 조건 대비로 근거를 바꿨다. **슬롯 아이템이 자리를 지키는 것(A) 확정** — 대안 B는 같은 동률이 재현되고 `NewSlotId` 예외가 돌아온다. 03-2 ★ 노트의 증상 서술이 **틀렸던 것**(*"AK가 X보다 앞에"* → 실제로는 뒤)도 교정. 완료 조건 19개(＋1) |
| 2026-08-23 (11차 검수) | **키 공간을 `GetSortedContents`에서 떼어냈다 (함정 4q).** `KeySpace_NextAtEnd`·`RenormalizeSortKeys`가 **부모가 같은 것 전부**를 본다 — 초안은 슬롯을 거른 목록에서 최대 키를 구해 *"꽂고·줍고·뺀다"* 라는 정상 조작에서 동률이 났다. `InsertEntry`의 삼항과 `MoveEntry`의 `NewSlotId` 조건이 **사라지고 코드가 줄었다**(꽂았다 빼면 원래 자리로 돌아온다). **재정규화를 세 분기 공통 가드로 (함정 4r)** — 맨 앞/맨 뒤가 무한 증감이라 `int32` 경계에서 순서가 영구히 깨졌다. **`Server_ReorderEntry`(RPC)를 04-B로** — 9차의 `Server_MoveEntry` 규칙. `Owner`를 `TObjectPtr<UActorComponent>`로 + **파일 앞부분 블록 신설**(전방선언 3개). 조밀 기각 사유 교정(*"재정규화 코드가 사라진다"* 는 거짓 — 그게 곧 조밀 재번호 루프다). `double` 기각에 고갈 판정 근거 추가, 컨테이너 배열 기각에 9차 일관성 근거 추가. 완료 조건 19개(＋2), 단일 쓰기 지점 표에 `SwapEntries` 한 줄 |
| 2026-08-23 (11차) | **★★ 화면 순서를 서버가 든다.** `FEPInventoryEntry::SortKey`(`int32` 희소, `Step = 1<<16`) 신설 — 형제 스코프. `AssignSortKey`가 유일한 쓰기 지점(패턴의 5번째), `InsertEntry`가 발급 · `MoveEntry`가 부모 변경 시 재발급 · `Server_ReorderEntry(EntryId, PrevEntryId)`가 자리 바꾸기(인덱스 아님) · `RenormalizeSortKeys`가 이분 고갈 처리. `GetSortedContents`는 **클라·서버 공용** 정렬. **`RemoveEntryInternal` ②의 `bIsRoot`가 `ParentEntryId`와 `SortKey`를 동시에 관장** — 루트는 버리고 자식은 보존해 **배낭을 되주워도 내용물 순서가 산다.** `EP.Inv.Reorder` 커맨드 + `Dump`에 `SortKey` 열. 함정 4m~4p, 완료 조건 17개(＋4). 근거: `05_Loot_04_InventoryUI.md` 04-8이 10차까지 클라 로컬이었고 그 설계는 **지속을 줄 수 없다** |