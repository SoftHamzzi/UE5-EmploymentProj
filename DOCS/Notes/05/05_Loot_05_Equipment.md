# Step 05 — Equipment (무기 장착 흐름 이관 + 탄약 소유권 정리)

> 마스터 기획: `05_Loot_DOCS.md` (§4-8)
> 선행: `05_Loot_04_InventoryUI.md` — 인벤토리에서 무기를 고를 수 있어야 한다

---

## 목표

`EPGameMode`가 무기 액터를 직접 스폰하던 흐름을 **인벤토리 기반**으로 바꾸고, 탄약의 소유권을 정리한다.

**완료 조건**

- [ ] 무기를 **12/30까지 쏘고 버렸다 다시 주워 장착하면 12/30 그대로** (핵심 검증)
- [ ] 무기 2정을 들고 번갈아 장착해도 각각의 잔탄이 보존된다
- [ ] 다른 클라이언트에서도 장착 무기가 보인다 (`AEPWeapon` 액터 복제)
- [ ] 매치 시작 시 `DefaultLoadout`대로 인벤토리가 채워지고 무기가 자동 장착된다
- [ ] 장착 중인 무기를 버리면 자동 해제되고 잔탄이 엔트리에 기록된다
- [ ] **착용 컨테이너(상의·외투·배낭)를 장착 상태에서 버린다** → 안의 내용물이 통째로 나가고 잔탄이 보존된다
  > **★ 이 경로는 Step 03에서 작성됐지만 여기서 처음 실행된다.** `RemoveEntry`의 장착 분기는 `ActiveHotbarIndex` 세팅 경로가 Step 05에 있어 **Step 03 내내 항상 거짓**이다. 컴파일도 되고 Step 03 완료 조건도 전부 통과하지만 장착 관련 불변식은 한 번도 안 돈다. 여기서 버그가 나면 **원인은 두 단계 전 코드에 있다** (`05_Loot_03_Inventory.md` 03-2)
  >
  > **9차에서 시나리오가 바뀌었다.** 이전 문장은 *"배낭 속 무기를 장착한 채 배낭을 버린다"* 였다. 장착이 `SlotId == "HotbarN"` **＋** `ParentEntryId == INDEX_NONE`이 되면서 **장착된 무기는 배낭 안에 있을 수 없다.** 검증해야 할 것은 캐스케이드 자체다
- [ ] **★ write-back이 `RemoveSelf` 앞에서 도는가** → 장착 무기를 12/30으로 버렸다 주우면 **12/30 그대로**
  > **9차에서 성격이 바뀐 항목이다.** `EquippedEntryId`가 저장된 필드일 때는 순서를 어겨도 *"`INDEX_NONE`을 향한 write-back"* 이라 눈에 보였다. 파생 게터가 되면서 **`RemoveSelf` 이후에는 write-back이 아예 안 불린다** — 증상이 "잔탄이 조용히 사라짐"이다 (`EquipmentSlots.md` §3)
- [ ] **★ `HotbarRefs` 청소가 `RemoveSelf`에 들어갔는가** → 5번에 걸어둔 붕대를 다 쓰면 5번 칸이 빈 칸으로 그려진다
  > **★ Step 03·04에서 의도적으로 미룬 항목이다.** 필드는 Step 04(드래그 배정)에서 선언되고, **청소는 여기가 유일한 자리다.** `HotbarRefs`는 `EntryId`를 **직접** 들어 `ActiveHotbarIndex`가 받는 보호(슬롯을 가리키므로 죽은 번호가 없음)를 못 받는다. 붙일 곳은 `RemoveSelf` **한 줄** — 버리기·사용·캐스케이드 세 경로가 전부 거기로 모인다 (`EquipmentSlots.md` §4)
- [ ] **★ 핫바 5~0이 화면에 그려지고 1~4와 시각적으로 구별된다**
  > **★ 04가 이 단계로 미뤄둔 항목이다** (`05_Loot_04_InventoryUI.md` "이 단계에서 하지 않는 것"). 5~0은 **참조**라 아이템이 컨테이너 칸에도 **그대로 보인다** — 테두리만/반투명 + 구획 칸에 `⑤` 배지를 붙이지 않으면 *"5번에 걸었는데 왜 가방에 그대로 있지?"* 가 난다. `HotbarRefs` 청소(위 항목)와 같은 자리에서 검증된다
- [ ] 사망 시에도 잔탄이 write-back된다
- [ ] **가방 속(장착 안 한) 무기의 잔탄이 인벤토리 UI에 보인다** — Step 04의 `ChargesText`
- [ ] `GA_Item_PrimaryUse` / `GA_Item_Reload`는 **한 줄도 수정하지 않았다**

---

## 05-1. 문제 — 탄약의 진실이 두 곳에 있다

| 위치 | 성격 | 현재 |
|---|---|---|
| `UEPAttributeSet::Ammo` / `MaxAmmo` | 캐릭터(ASC) 소유. **1인당 하나뿐.** 복제되고 HUD가 구독. GAS 발사·재장전이 읽고 쓴다 | 실제로 쓰임 |
| `FEPInventoryEntry::State.Charges` | 개체별. 무기마다 다름 | Step 03에서 만들어짐 |

인벤토리에 무기 2정을 넣고 교체하려면 **정별로** 잔탄이 보존돼야 하는데, GAS 어트리뷰트는 캐릭터에 하나뿐이다.

### 결정: 어트리뷰트는 "현재 장착 무기의 뷰", 엔트리가 진실

```
Equip   : Entry.State.Charges  →  SetAmmo()                            (주입)
Unequip : GetAmmo()            →  Entry.State.Charges + MarkItemDirty  (write-back)
```

- 발사·재장전은 지금처럼 GAS 어트리뷰트만 건드린다. **어빌리티는 수정하지 않는다**
- **해제 시점의 write-back을 빠뜨리면** 무기를 바꿨다 돌아올 때 잔탄이 되돌아간다
- **사망 시에도 write-back이 필요하다** — 시체/드랍의 잔탄이 맞아야 한다

> 이 구조의 대안은 "무기마다 ASC를 둔다"인데, ASC는 무겁고 복제 비용이 크며 GAS의 어트리뷰트 모델과 맞지 않는다. 장착 무기 하나를 어트리뷰트로 표현하고 나머지는 개체 상태로 두는 것이 정석이다.

---

## 05-2. ★ 즉시 고쳐야 할 것 — `EquipWeapon`의 만탄 리셋

```cpp
// EPCombatComponent.cpp:177-178  (현재)
AS->InitAmmo(static_cast<float>(NewWeapon->WeaponDef->MaxAmmo));      // ← 만탄 리셋
AS->InitMaxAmmo(static_cast<float>(NewWeapon->WeaponDef->MaxAmmo));
```

지금은 게임모드가 스폰 시 1회만 장착하므로 드러나지 않는다. **버리기(Step 03)가 들어온 지금 이건 익스플로잇이다** — 12/30 무기를 버렸다 줍기만 하면 30/30이 된다.

```cpp
// 변경 후
AS->SetMaxAmmo(static_cast<float>(NewWeapon->GetMaxAmmo()));  // ★ WeaponDef-> 가 아니다
AS->SetAmmo(static_cast<float>(Entry.State.Charges));         // 개체 상태
```

> **★ `GetMaxAmmo()`를 거치는 것이 부착물(§7-3) 준비의 전부다.** 지금은 `WeaponDef->MaxAmmo`를 그대로 돌려주고, 부착물이 오면 그 함수 안에서 합산한다. **새 패턴이 아니라 기존 패턴을 한 번 더 쓰는 것**이다 — `AEPWeapon::GetDamage()`가 이미 정확히 이 모양이다(`EPWeapon.cpp:66`).
>
> **스탯 합산 스펙은 여기 쓰지 않는다.** §7-3이 "Step 05에서 같이 정리하면 비용이 거의 없다"고 했지만 실제 `WeaponDef->` 직접 읽기는 **5개 파일**이고, 그중 `GA_Item_PrimaryUse`(`FireRate`)·`GA_Item_Reload`(`ReloadTime`)를 건드리면 **이 단계의 완료 조건("어빌리티는 한 줄도 수정하지 않았다")과 정면 충돌**한다. 그리고 부착물이 실제로 바꾸는 Spread/Recoil 계열은 **이미 `AEPWeapon` 안에 모여 있다**(`EPWeapon.cpp`). 남는 준비는 위 한 줄뿐이다.

> **★★ 정정 — 위 문단은 ⓑ(확장탄창 = 스탯 부착물)의 답이다** (2026-08-24, `05_Loot_DOCS.md` §7-4).
> **미정 #1이 ⓐ(탄창이 별도 아이템)로 가면 `MaxAmmo`에 대해서는 다른 답이 된다.** ⓐ의 `MaxAmmo`는 **꽂힌 탄창의 `Capacity`** 이고, 그건 무기 스탯이 아니라 **인벤토리 엔트리**다. 무기 액터가 그걸 읽으려면 액터에 `EntryId`를 심고 인벤토리를 뒤지게 해야 하는데, **§4-8이 여태 피해온 방향**이다.
>
> **ⓐ에서 읽는 주체는 `UEPCombatComponent`다.** `SetMaxAmmo`가 불리는 곳은 장착·재장전 둘뿐이고 **둘 다 이미 `EntryId`를 쥐고 있다.** `GetMaxAmmo()`는 Spread/Recoil 합산용으로 남고 **탄약에서는 손을 뗀다.**
>
> **그래도 위 한 줄은 지금 그대로 넣는다.** ⓐ로 갈지 미정이고, ⓑ면 정확한 준비이며, ⓐ여도 이 줄이 만드는 부채는 **호출부 한 곳을 옮기는 것**뿐이다.

> **`Init*` → `Set*`로 바꾸는 이유:** `Init*` 계열은 어트리뷰트 변경 델리게이트를 **발생시키지 않는다.** `UEPHUDWidget`이 `GetGameplayAttributeValueChangeDelegate`로 탄약을 구독하고 있으므로(`EPHUDWidget.cpp:26`), `Init*`으로 두면 장착 즉시 HUD가 갱신되지 않는다. 지금은 `RefreshAmmo()`가 다른 경로로 불려 가려져 있을 수 있으니, 무기 교체를 반복하며 확인할 것.

> `MaxAmmo`를 먼저 세팅한다. `UEPAttributeSet::PreAttributeChange`가 `Ammo`를 `[0, MaxAmmo]`로 클램프하므로(`EPAttributeSet.cpp:17-18`), 순서를 뒤집으면 이전 무기의 낮은 `MaxAmmo`에 잘린다.

---

## 05-3. `EquipFromInventory` — 무기 액터 스폰 책임은 그대로 둔다

```cpp
// UEPCombatComponent
void EquipWeapon(AEPWeapon* NewWeapon);              // 기존 — 유지
void EquipFromInventory(int32 EntryId);              // 신규
void UnequipWeapon();                                // 기존 — write-back 추가
```

```
숫자키 1~4
    → Server_SetActiveHotbarIndex(int32 Index)      ★ 15차. Server_Equip이 아니다 — 아래
    → Inventory: ActiveHotbarIndex = Index          (배정이 아니라 활성. MoveEntry를 안 부른다)
    → CombatComponent::EquipFromInventory(GetEquippedEntryId())
         → 빈 슬롯이면 손만 비운다 (INDEX_NONE)
         → FindDefinition(Entry.ItemId)를 UEPWeaponDefinition으로 확인
         → AEPWeapon 액터 스폰 + 소켓 부착 + LinkAnimClassLayers   ← 기존 EquipWeapon 경로 재사용
         → SetMaxAmmo / SetAmmo 주입 (05-2)

드래그로 무기를 Hotbar1에 넣는 것은 **배정**이고 이미 04-B에 있다
    → Server_MoveEntry(id, INDEX_NONE, "Hotbar1")   (05_Loot_04_InventoryUI.md 04-7)
```

### ★★ `Server_Equip(EntryId)`를 만들지 않는다 — 배정과 활성은 다른 연산이다 (15차)

**초안의 흐름은 `Server_Equip(EntryId)` 하나가 `MoveEntry` ＋ `ActiveHotbarIndex` ＋ 액터 스폰을 전부 했다.** 세 가지가 어긋난다.

**① 13차의 검사 0 때문에 실제로 안 돈다.** 드래그로 이미 `Hotbar1`에 넣어둔 무기 — **정상 경로다** — 에 대해 1번 키를 누르면:

```
MoveEntry(id, INDEX_NONE, "Hotbar1")
   → 검사 0: ParentEntryId == INDEX_NONE && SlotId == "Hotbar1"   → 제자리
   → false                                    (05_Loot_03_Inventory.md 03-2)
```

**반환값을 보고 빠져나가면 1번 키가 아무 일도 안 한다.** 무시하고 진행하면 이번엔 반환값에 의미가 없어져, 진짜 실패(다른 사람 아이템·삭제된 번호)와 구별이 사라진다.

**② `EntryId`만으로는 몇 번 슬롯인지 정할 수 없다.** `SlotPriority`가 `["Hotbar1","Hotbar2"]`처럼 여럿을 허용한다(`EquipmentSlots.md` §5). *"HotbarN"* 의 N을 서버가 추측해야 하는데, **플레이어가 누른 것은 애초에 번호다.**

**③ 14차가 죽인 것과 같은 모양이다.** 04-B가 `Server_MoveEntry`를 여는데(`05_Loot_04_InventoryUI.md` 04-7) `Server_Equip`은 **그것으로 표현되는 일을 하는 네 번째 착용 표면**이다. *"넓은 문이 열린 뒤의 좁은 문은 공격 표면을 하나도 줄이지 않는다"*(14차, 03-2).

**갈라야 하는 이유는 두 연산이 실제로 다르기 때문이다.**

| | 배정 — 어느 칸에 넣나 | 활성 — 어느 칸을 손에 드나 |
|---|---|---|
| 상태 | `SlotId = "HotbarN"` (엔트리 필드, 복제) | `ActiveHotbarIndex` (컴포넌트 필드, `COND_OwnerOnly`) |
| 연산 | `MoveEntry` | 대입 |
| 표면 | **`Server_MoveEntry`** — 04-B 드래그 | **`Server_SetActiveHotbarIndex`** — 이 단계 |
| 빈도 | 가끔 | **전투 중 수시로** |

**`EquipmentSlots.md` §3·§4가 이미 이 구분을 세워뒀다** — *"`SlotId`로 표현되지 않는 것이 하나 있다: 1번과 2번 중 지금 어느 쪽을 손에 들고 있는가"*.

> **★ Lyra가 정확히 이 모양이다** (직독). 배정은 **서버 내부**, 활성만 **RPC**다.
>
> ```cpp
> // LyraQuickBarComponent.h:29-30
> UFUNCTION(Server, Reliable, BlueprintCallable, Category="Lyra")
> void SetActiveSlotIndex(int32 NewIndex);
>
> // LyraQuickBarComponent.h:47 — 배정은 RPC가 아니다
> UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
> void AddItemToSlot(int32 SlotIndex, ULyraInventoryItemInstance* Item);
> ```
>
> **그리고 구현이 우리 검사 0과 같은 조기 반환을 갖는다.**
>
> ```cpp
> // LyraQuickBarComponent.cpp:137-146
> if (Slots.IsValidIndex(NewIndex) && (ActiveSlotIndex != NewIndex))
> {
>     UnequipItemInSlot();
>     ActiveSlotIndex = NewIndex;
>     EquipItemInSlot();
>     OnRep_ActiveSlotIndex();
> }
> ```
>
> **`Unequip` → 대입 → `Equip` 순서가 그대로 우리 write-back 순서다**(05-3). `SetActiveSlotIndex`는 Lyra가 손으로 만든 **유일한** 서버 RPC이기도 하다 — 그만큼 이 한 칸이 좁다.

```cpp
// UEPInventoryComponent — 활성 핫바. 세팅 경로가 이 단계에서 처음 생긴다 (03-2)
void SetActiveHotbarIndex(int32 Index);                         // 서버 내부. 대입 + MarkArrayDirty 아님
UFUNCTION(Server, Reliable) void Server_SetActiveHotbarIndex(int32 Index);
```

```cpp
void UEPInventoryComponent::Server_SetActiveHotbarIndex_Implementation(int32 Index)
{
    if (!CanMutateInventory()) return;              // ★ 상태 변경 RPC의 유일한 게이트 (03-5)
    if (Index != INDEX_NONE && (Index < 0 || Index > 3)) return;   // 1~4만. 5~0은 HotbarRefs다
    if (Index == ActiveHotbarIndex) return;         // Lyra와 같은 조기 반환

    // ★ 컴포넌트는 GetOwner<AEPCharacter>()를 무가드로 체이닝하지 않는다 (13차, 03-2)
    AEPCharacter* Char = GetOwner<AEPCharacter>();
    UEPCombatComponent* Combat = Char ? Char->GetCombatComponent() : nullptr;
    if (!Combat) return;

    Combat->UnequipWeapon();                        // ★ write-back이 여기서 돈다 (05-3)
    SetActiveHotbarIndex(Index);
    Combat->EquipFromInventory(GetEquippedEntryId());
}
```

> **★ 범위 검사가 `0~3`인 것이 계약이다.** `ActiveHotbarIndex`는 **1~4만** 가리킨다 — 5~0은 `SlotId`가 아니라 `HotbarRefs` 참조라 *"손에 든다"* 가 성립하지 않는다(`EquipmentSlots.md` §4). 열어두면 `GetEquippedEntryId()`가 `"Hotbar7"` 같은 없는 슬롯을 조회해 조용히 `INDEX_NONE`을 돌려주고, 증상은 *"7번을 누르면 무기가 사라진다"* 다.

**★ 인벤토리 컴포넌트가 `AEPWeapon`을 스폰하지 않는다.** 무기 액터의 수명·부착·애님 레이어·어빌리티 부여를 이미 `UEPCombatComponent`가 전부 쥐고 있다(`EPCombatComponent.cpp:162`). 인벤토리는 **`EntryId`만 넘긴다.**

기존 `EquipWeapon(AEPWeapon*)`을 남겨두고 `EquipFromInventory`가 그쪽으로 위임하면, 기존 호출부를 한 번에 갈아엎지 않아도 되고 이관 중 두 경로를 비교 검증할 수 있다.

### `UnequipWeapon`에 write-back 추가

```cpp
void UEPCombatComponent::UnequipWeapon()
{
    if (!GetOwner()->HasAuthority() || !EquippedWeapon) return;   // 기존 가드 옆에

    AEPCharacter*          Char = GetOwner<AEPCharacter>();
    UEPInventoryComponent* Inv  = Char ? Char->GetInventoryComponent() : nullptr;

    // UEPCombatComponent에 AttributeSet 멤버가 없다 — PlayerState를 거친다
    // (기존 코드와 같은 경로: EPCombatComponent.cpp:171-174)
    AEPPlayerState*  PS = Char ? Char->GetPlayerState<AEPPlayerState>() : nullptr;
    UEPAttributeSet* AS = PS   ? PS->GetAttributeSet() : nullptr;

    if (Inv && AS)
    {
        const int32 EquippedId = Inv->GetEquippedEntryId();
        if (EquippedId != INDEX_NONE)
            Inv->SetEntryCharges(EquippedId,
                FMath::RoundToInt(AS->GetAmmo()));      // ★ write-back — 대입 한 줄
    }
    if (Inv) Inv->SetActiveHotbarIndex(INDEX_NONE);   // 손을 비운다. 슬롯은 그대로다
                                                      // ★ 내부 함수다. RPC는 Server_SetActiveHotbarIndex (위)

    /* 기존 해제 로직: 액터 파괴, 애님 레이어 해제 등 */
}
```

> **`AddEntryCharges`가 아니라 `SetEntryCharges`다.** write-back은 본질적으로 **대입**이라 델타 API에 넘기려면 현재값을 먼저 읽어야 하고, 그러면 읽고·빼고·넘기는 것이 **다른 컴포넌트에서** 벌어진다 — 원시 엔트리를 감춘 의미가 사라진다. 클램프와 `MarkItemDirty`는 여전히 `SetEntryCharges` 한 곳에만 있다 (Step 03 03-3).

### ★★ 순서 규칙 — 9차에서 근거 문장이 바뀌었다

**`RemoveEntry`는 더 이상 아무 번호도 비우지 않는다.** 장착의 진실이 `SlotId`가 되면서 `EquippedEntryId` / `EquippedBackpackEntryId` 필드가 사라졌기 때문이다(03-2). 엔트리가 배열에서 빠지면 `GetEquippedEntryId()`가 자동으로 `INDEX_NONE`을 돌려준다.

**그래서 순서 규칙 자체는 남지만 위반했을 때의 결과가 바뀐다.**

| | 8차까지 (저장된 필드) | 9차 이후 (파생 게터) |
|---|---|---|
| `RemoveSelf` 뒤에 장착 번호를 읽으면 | 값이 **살아 있다** | **`INDEX_NONE`** |
| write-back을 `RemoveSelf` 뒤로 옮기면 | `INDEX_NONE`을 향한 write-back | **write-back이 아예 안 불린다** |
| 증상 | 눈에 보이는 오작동 | **잔탄이 조용히 사라진다** |

**규칙:** `RemoveEntryInternal`의 ① write-back → ② 스냅샷 → ③ `RemoveSelf` → ④ 캐스케이드 순서는 **스타일이 아니라 계약이다.**

`:124`의 `SetActiveHotbarIndex(INDEX_NONE)`은 **남긴다.** 홀스터·교체·사망은 **제거 없이** `UnequipWeapon`을 부르는 경로이고, 거기서 안 비우면 무기 액터는 없는데 활성 인덱스는 살아 있는 상태가 된다. 대입이라 idempotent이고, **`RemoveEntry`가 이것에 의존하지 않는다**는 8차 결론은 그대로다.

> **05-2의 코드는 기존 함수 발췌**라 `AS`가 이미 스코프에 있다. 위는 **완결된 함수 본문**이므로 직접 얻어야 한다.

> **★ `MarkItemDirty`는 `SetEntryCharges()` 안에서 보장된다** (Step 03 03-3). **13차가 *"쓰기 지점을 하나로 — `Set`이 진짜고 `Add`는 위임이다"* 로 확정했다**(03-3) — 위 코드가 부르는 것도 `SetEntryCharges`다. 획득·버리기는 Add/Remove라 FastArray가 알아서 보내지만 **잔탄 write-back은 기존 항목의 필드 변경**이라, 빠뜨리면 서버만 맞고 소유 클라의 UI는 옛 값을 계속 보여준다. **원시 엔트리를 밖으로 내보내지 않으므로 호출자가 잊을 방법이 없다.**

> **null 가드가 필요하다.** 기존 코드가 `if (!GetOwner()->HasAuthority() || !EquippedWeapon) return;`로 시작하므로 그 옆에 붙인다. `GetOwner<AEPCharacter>()->GetInventoryComponent()`를 무가드로 체이닝하면 사망·리스폰 타이밍에서 터진다.

> **핸들을 이중화하지 않는다.** 이전 설계는 `UEPCombatComponent`에 서버 전용 `EquippedInstanceHandle`을 따로 뒀는데, 이유가 "버리기에서 인벤토리가 먼저 비워져도 write-back 대상을 잃지 않게" 였다 — **자료구조로 순서 실수를 막은 것.** 값 타입에서는 대상이 엔트리 자신이므로 그 방어가 성립하지 않는다.

### ★ 순서 규칙을 여기 적지 않는다 — `RemoveEntry()`가 보장한다

위험 자체는 실재한다: **엔트리를 지운 뒤 잔탄을 write-back하면 조용히 소실된다.** 증상은 "장착 무기를 버렸다 주우면 만탄"이고, **05-2의 `InitAmmo` 문제와 증상이 똑같아서 오진하기 쉽다.**

하지만 이걸 "순서를 지켜라"라는 문서 규칙으로 두면 이 문서와 Step 03 양쪽에 적히고, **한쪽만 고쳐지는 사고가 반드시 난다.** **`RemoveEntry(Id, &Out)`이 제거된 서브트리를 write-back이 끝난 상태로 반환한다** — 스냅샷을 얻는 유일한 방법이 제거하는 것이므로 순서를 뒤집는 게 문법적으로 불가능해진다. 구현은 Step 03 03-2·03-5에만 둔다.

**write-back이 필요한 지점은 세 곳이고, 전부 `UnequipWeapon()`을 거친다.** (★ 미정 #1이 ⓐ로 가면 **재장전이 넷째가 된다** — 아래 ⓐ 대비 절)

| 시점 | 경로 |
|---|---|
| 무기 교체 | `EquipFromInventory` → 기존 무기 `UnequipWeapon()` |
| 버리기 | `RemoveEntry()`가 자동 호출 — **호출자가 신경 쓸 것이 없다** |
| 사망 | 사망 처리 → `UnequipWeapon()` |

**분기마다 따로 쓰지 않는다** — 하나를 빠뜨리면 그 경로에서만 잔탄이 되돌아가고, 재현 조건이 좁아 찾기 어렵다.

---

### ★★ ⓐ(탄창 아이템화) 대비 — 지금 해두면 공짜인 것 셋

**미정 #1이 ⓐ로 가면 Step 05 직후에 붙인다**(`05_Loot_DOCS.md` §7-4). 그때 붙는 것 대부분은 이 단계 코드의 **인자만 바꾸는 일**인데, **셋만은 지금 형태를 잡아둬야 값이 싸다.** 셋 다 §7-3·미정 #1이 **이름으로 예고한 확장점**이라 §2의 판단 기준을 통과한다. 합쳐서 20줄 안쪽이다.

| | 지금 비용 | 안 하면 나중에 |
|---|---|---|
| **`WriteBackAmmo()`로 함수 추출** — `UnequipWeapon()` 안의 write-back 두 줄을 이름 있는 함수로 | 두 줄을 빼는 것 | ⓐ의 재장전이 **4번째 write-back 경로**가 된다(재장착이 아니므로 `UnequipWeapon()`을 안 거친다). 위 표의 *"전부 `UnequipWeapon()`을 거친다"* 가 **깨진 채로 발견된다** — 함정 6이 막으려던 바로 그 상태다 |
| **`UEPEquipSlotWidget`에 `ParentEntryId`** (04-4) — `GetEntryInSlot(INDEX_NONE, SlotId)`의 `INDEX_NONE`을 필드로 | 필드 하나 + 기본값 `INDEX_NONE` | 무기 부착물 슬롯(`Optic`/`Muzzle`/`Grip`/`Mag`)은 **부모가 무기 `EntryId`일 뿐 같은 위젯**이다. 안 빼두면 위젯을 복제하거나 Step 04를 다시 연다 |
| **`MaxAmmo` 주입을 `InjectAmmoFromMag(int32)`로** — 05-2의 두 줄에 이름을 붙임 | 이름만 미리 | 장착과 재장전이 **같은 순서 규칙**(`MaxAmmo` 먼저 — 함정 5)을 각자 구현한다. 한쪽만 틀리면 *"재장전할 때만 탄이 잘린다"* 가 된다 |

> **셋 다 ⓑ로 확정돼도 손해가 아니다.** 함수 추출 둘은 호출부가 하나여도 이름이 의미를 갖고(순서 규칙이 한 곳에 산다), 위젯 필드는 기본값이 지금 동작 그대로다. **ⓐ가 안 와도 되돌릴 일이 없다** — 이게 §2가 "지금 넣는다"로 판정하는 조건이다.

---

## 05-4. 장착 슬롯 표현

**표현은 Step 03에서 이미 확정돼 있다(03-2)** — `FEPInventoryEntry::SlotId`가 장착의 유일한 진실이고, 남는 상태는 `ActiveHotbarIndex`(`COND_OwnerOnly`) 하나다. 이 단계는 **그 인덱스를 실제로 세팅하는 경로**를 만든다.

**`COND_OwnerOnly`로 충분하다.** 다른 클라이언트는 `AEPWeapon` 액터가 복제되고(`EPWeapon.cpp:19` `bReplicates = true`) 캐릭터 소켓에 부착되므로 **이미 보인다.** 이 값은 순전히 소유자 UI("어느 칸을 들고 있나")를 위한 것이다.

- **장착된 무기는 칸을 먹지 않는다** — `SlotId != None`이면 `GetUsedSlots`가 건너뛴다(03-3). 8차까지의 *"자기 `SlotSize`만큼 칸을 차지한 채로 장착된다"* 는 9차 기획 확대로 뒤집혔다
- **`TMap<EEPEquipSlot, int32>`로 가지 않는다** — 슬롯이 12개가 돼도 마찬가지다. `SlotId`가 이미 엔트리 안에 있고 이미 복제된다. §8 미정 #5는 **확정으로 이동**했다
- **장착된 아이템은 `RemoveEntry()`가 알아서 해제한다** (Step 03 03-2)
- **슬롯 배정**은 `MoveEntry(EntryId, INDEX_NONE, "HotbarN")` 한 줄이고 **04-B의 드래그가 그 소비자다**. 이 단계가 만드는 것은 **활성**(`Server_SetActiveHotbarIndex`)이다 — 둘은 다른 연산이다 (05-3 ★★)

> **★ `RemoveEntry`의 장착 분기는 이 단계에서 처음 실제로 돈다.** Step 03에는 `ActiveHotbarIndex`를 세팅하는 경로가 없어 분기가 내내 거짓이었다. **Step 03을 "통과"시켰다고 그 코드가 검증된 것이 아니다** — 장착 관련 버그는 여기서 처음 드러난다.

Step 04의 **칸 위젯**(`UEPItemCellWidget` / `UEPEquipSlotWidget` — 04-2에서 행 위젯이 칸 위젯으로 바뀌었다)에 장착 표시(테두리 강조)만 추가한다. 읽는 값은 `GetEquippedEntryId()` 하나다.

---

## 05-5. `DefaultLoadout` 이관

```cpp
// EPGameMode.h
UPROPERTY(EditDefaultsOnly, Category = "Combat")
TArray<FName> DefaultLoadout;        // was: TSubclassOf<AEPWeapon> DefaultWeaponClass
```

```cpp
// EPGameMode.cpp — HandleStartingNewPlayer
void AEPGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    Super::HandleStartingNewPlayer_Implementation(NewPlayer);

    AEPCharacter* Char = Cast<AEPCharacter>(NewPlayer->GetPawn());
    if (!Char) return;

    UEPInventoryComponent* Inv  = Char->GetInventoryComponent();
    const UEPItemDefinitionSubsystem* Defs = /* GameInstance에서 획득 */;

    int32 FirstWeaponEntryId = INDEX_NONE;
    for (const FName& ItemId : DefaultLoadout)
    {
        FEPItemState State;
        if (!Defs->MakeItemState(ItemId, State)) continue;      // Step 00 00-6

        // ★★ 본체가 아니다 (15차). 본체는 0칸으로 확정됐다 — 아래
        int32 EntryId = INDEX_NONE;
        for (int32 C : Inv->GetInsertionOrder())                // 본체(0칸) → 외투 → 상의 → 하의 → 배낭
        {
            EntryId = Inv->AddItem(C, ItemId, State);
            if (EntryId != INDEX_NONE) break;
        }
        if (EntryId == INDEX_NONE) continue;                    // 어느 컨테이너에도 안 들어갔다. ★ if(EntryId) 금지

        if (FirstWeaponEntryId == INDEX_NONE && /* 무기 타입인가 */)
            FirstWeaponEntryId = EntryId;
    }

    if (FirstWeaponEntryId != INDEX_NONE)
        Char->GetCombatComponent()->EquipFromInventory(FirstWeaponEntryId);
}
```

> **`AddItem`이 `EntryId`를 돌려주는 것이 여기서 값을 한다.** 삽입 직후 그 엔트리를 바로 가리킬 수 있어 "방금 넣은 무기를 찾아 장착"에 재검색이 필요 없다.

> ### ★★ 본체는 **0칸**이다 — `AddItem(INDEX_NONE, …)`은 전부 실패한다 (15차, 13차 확정 반영)
>
> **초안은 `AddItem(INDEX_NONE, ItemId, State)`로 본체에 넣었다.** 13차가 본체 용량을 **0칸**으로 확정하면서(`05_Loot_03_Inventory.md` 03-3 용량표 · `05_Loot_DOCS.md` §8 미정 #9) 그 호출은 **`CanFit`에서 `0 + SlotSize <= 0`이 되어 언제나 거짓**이다.
>
> ```
> DefaultLoadout = ["Weapon_AK74", "AmmoBox_545", "Bandage"]
>    → 셋 다 AddItem 실패 → FirstWeaponEntryId == INDEX_NONE → 무기 없이 스폰
>    → 증상은 함정 10(DefaultWeaponClass 먼저 제거)과 **똑같다**
> ```
>
> **`GetInsertionOrder()`를 쓴다** — 03-4의 `OnInteract`가 이미 정확히 이 모양이다(`05_Loot_03B_PickupDrop.md:62`). 획득 경로에 두 번째 구현을 만들지 않는다.
>
> **★ 그러려면 옷을 먼저 입고 있어야 한다.** 13차가 그 문제를 이미 풀었다 — **`StartingEquipment`**(03-B)가 캐릭터 `BeginPlay`(서버)에서 상의·하의를 입힌다. **순서가 계약이 된다:**
>
> ```
> Pawn BeginPlay  →  StartingEquipment  →  상의(Cap 10) · 하의(Cap 5) 착용     (03-B)
> HandleStartingNewPlayer (Super:: 뒤)  →  DefaultLoadout                     (여기)
> ```
>
> **`Super::HandleStartingNewPlayer_Implementation`이 `RestartPlayer`를 거쳐 폰을 스폰하므로, 이 함수가 `GetPawn()`을 읽는 시점에는 `BeginPlay`가 이미 돌았다.** 뒤집히면 **수납 용량이 0인 상태에서 `DefaultLoadout`이 돌아 전부 실패한다** — 그리고 증상은 위와 같다. 검증은 `EP.Inv.Dump`로 상의·하의 두 줄이 로드아웃보다 먼저 있는지 보는 것이다.
>
> **여전히 넘칠 수 있다.** 상의 10 + 하의 5 = 15칸인데 무기 `SlotSize`가 5면 무기 + 탄약상자 + 붕대로 7칸이고, 기본 지급이 늘면 넘친다. 조용히 `continue`로 빠지면 **"가끔 탄약이 없다"** 로 나타나 원인을 못 찾는다 — **실패 시 반드시 경고 로그를 남긴다.**

`AEPWeapon` 액터 자체와 `UEPCombatComponent`의 발사 로직은 **건드리지 않는다.** 바뀌는 것은 "누가 언제 무기 액터를 만드는가"뿐이다.

> 기존 `DefaultWeaponClass`는 `BP_EPGameMode` 에셋에 값이 들어 있다. 제거하면 그 값이 사라지므로, **`DefaultLoadout`에 `Weapon_AK74_HitScan`을 먼저 채워 넣고 검증한 뒤** 옛 필드를 지운다. 순서를 반대로 하면 무기 없이 스폰되어 다른 버그로 오인한다.

> `DefaultLoadout`에는 탄약상자도 넣을 수 있다(`AmmoBox_545`). 다만 재장전이 그 `Charges`를 소비하는 건 §8 미정 #6이라, 지금은 넣어도 소비되지 않는다.

---

## 05-6. 타입 정리 (Step 00에서 이미 처리)

`UEPWeaponDefinition::MaxAmmo`는 Step 00에서 `uint8` → `int32`로 바뀌었다. 이 단계의 주입/write-back 경로가 그 위를 왕복한다.

```
WeaponDefinition::MaxAmmo   int32     ← Step 00에서 통일
FEPItemState::Charges       int32
AttributeSet::Ammo          float     ← GAS 규격상 float 고정
```

정수 ↔ float 경계는 `FMath::RoundToInt()`로 한 곳에서만 넘는다 (write-back). 주입 방향은 `static_cast<float>`로 충분하다 — 정보 손실이 없다.

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | `InitAmmo(MaxAmmo)` 잔존 | **버렸다 줍기만 해도 만탄** (익스플로잇) | 05-2 |
| 2 | **write-back보다 엔트리 제거가 먼저** | **증상이 #1과 똑같다.** 05-2를 고쳤는데도 만탄이면 여기 | 05-3 ★ |
| 3 | `MarkItemDirty` 누락 | 서버는 맞는데 **인벤토리 UI의 잔탄만** 옛 값 | 05-3 |
| 4 | `Init*` 유지 | 장착해도 HUD 탄약이 안 바뀜 | `Set*` |
| 5 | `Ammo`를 `MaxAmmo`보다 먼저 세팅 | 이전 무기의 낮은 `MaxAmmo`에 클램프됨 | `MaxAmmo` 먼저 |
| 6 | write-back을 경로별로 분산 | 특정 경로에서만 잔탄 되돌아감. 재현 조건이 좁음 | `UnequipWeapon()` 한 곳에만. **★ ⓐ에서 재장전이 `UnequipWeapon()`을 안 거치므로 `WriteBackAmmo()` 추출이 이 함정의 연장선이다** (ⓐ 대비 절) |
| 7 | 사망 시 write-back 누락 | 시체/드랍의 잔탄이 만탄 | 사망 처리에서 `UnequipWeapon()` |
| 8 | `ActiveHotbarIndex` / `Entries`를 무조건 복제 | 남이 뭘 들었는지 패킷으로 노출 | `COND_OwnerOnly` |
| 9 | 인벤토리가 `AEPWeapon`을 직접 스폰 | 무기 수명·애님·어빌리티 배선이 두 곳으로 갈림 | `CombatComponent`에 위임 |
| 10 | `DefaultWeaponClass`를 먼저 제거 | 무기 없이 스폰. 다른 버그로 오인 | `DefaultLoadout` 검증 후 제거 |
| 11 | 착용 컨테이너 용량이 `DefaultLoadout`보다 작음 | 뒤쪽 아이템이 조용히 누락 | 실패 시 경고 로그 (05-5) |
| **12** | **`DefaultLoadout`을 `AddItem(INDEX_NONE, …)`으로 본체에 넣음** | **본체가 0칸이라 전부 실패한다**(13차 확정). `FirstWeaponEntryId`가 `INDEX_NONE`이 되어 **무기 없이 스폰** — 증상이 함정 10과 **똑같다** | `GetInsertionOrder()` 순회 (05-5). 03-4의 `OnInteract`와 같은 모양 ★★ |
| **12b** | **`StartingEquipment`보다 `DefaultLoadout`이 먼저 돎** | 옷이 없으면 **수납 용량이 0**이라 12와 같은 증상. `EP.Inv.Dump`에 상의·하의 줄이 없다 | Pawn `BeginPlay`(03-B) → `HandleStartingNewPlayer`(05-5). `Super::` 뒤에서 읽으면 보장된다 |
| **13** | **`Server_Equip(EntryId)` 하나로 배정＋활성을 처리** | 이미 그 슬롯에 있는 무기에 대해 **검사 0이 `false`를 돌려준다** → 숫자키가 아무 일도 안 한다. 무시하면 반환값이 무의미해져 진짜 실패와 구별이 사라진다 | 배정 = `Server_MoveEntry`(04-B) / 활성 = **`Server_SetActiveHotbarIndex`**(05-3). Lyra도 같다 ★★ |
| **13b** | **`Server_SetActiveHotbarIndex`가 5~0을 받음** | `GetEquippedEntryId()`가 `"Hotbar7"` 같은 없는 슬롯을 조회해 조용히 `INDEX_NONE` → *"7번을 누르면 무기가 사라진다"* | `0~3`만. 5~0은 `SlotId`가 아니라 `HotbarRefs` 참조다 (`EquipmentSlots.md` §4) |

---

## 검증 시나리오 (PIE 2인, **클라이언트 쪽에서** 확인)

```
1. 클라이언트로 접속 → DefaultLoadout대로 무기 장착 확인
2. 18발 발사 → HUD 12/30
3. G로 버리기 → 픽업 생성. 0.5초간 프롬프트 회색
4. 다시 줍기 → 인벤토리에 들어감
5. 장착 → ★ HUD가 12/30 이어야 한다
     30/30이면 05-2(InitAmmo) 또는 05-3(순서) 미적용 — 증상이 같으니 둘 다 확인
6. 두 번째 무기를 주워 번갈아 장착 → 각각의 잔탄 보존
7. ★ 가방을 열어 장착 안 한 무기의 잔탄이 UI에 맞게 보이는지 확인
     여기가 틀리면 MarkItemDirty 또는 Step 04의 PostReplicatedChange 브로드캐스트
8. 사망 → 리스폰 후 인벤토리가 비어 있는지 확인
```

> **리슨서버 호스트가 아니라 클라이언트에서** 확인한다. 호스트는 권한과 예측이 같아 동기화 버그가 드러나지 않는다 (GAS 08단계에서 겪은 교훈).

---

## 변경 이력

| 날짜 | 무엇 |
|---|---|
| 2026-08-26 | **§7-5(장비 효과 = `AbilitySet` 부여) 예고를 "하지 않는 것"에 연결.** 이 단계의 write-back 순서 규칙이 그쪽 회수에도 그대로 걸린다 |
| 2026-08-26 (15차 — 03의 13·14차 · 04의 15차 대조) | **★★ 하드 결함 둘.** ① **`Server_Equip(EntryId)` 폐기** — 배정과 활성을 뭉갰고, **13차의 검사 0 때문에 정상 경로에서 `false`가 난다.** `Server_SetActiveHotbarIndex(int32)`로 가른다(Lyra `SetActiveSlotIndex` 직독 — `LyraQuickBarComponent.h:29-30`, 배정 `AddItemToSlot`은 RPC가 아니다). ② **`DefaultLoadout`이 본체에 넣고 있었다** — 13차가 본체를 **0칸**으로 확정해 `AddItem(INDEX_NONE, …)`이 전부 실패한다. `GetInsertionOrder()` 순회로, 그리고 `StartingEquipment`(03-B) → `DefaultLoadout` 순서를 **계약으로 명시**. 함정 **12·12b·13·13b** 신설, 11 문구 교정. `MarkItemDirty` 보장 주체를 `AddEntryCharges` → **`SetEntryCharges`**(13차 *"Set이 진짜"*). 완료 조건에 **핫바 5~0 UI**(04가 이 단계로 미뤄둔 항목). "행 위젯" → **칸 위젯** |

---

## 이 단계에서 하지 않는 것

- 재장전 시 탄약상자 `Charges` 차감 → §8 미정 #6
- 탄창을 별도 아이템으로 두기 → §8 미정 #1 (`FEPItemState`에 `AmmoType`이 필요해진다)
- 무기 2정 이상 슬롯(주무기/보조) → §8 미정 #5
- **방어구·헬멧 장착** → **§7-5.** 그때 붙는 것은 *"장착하면 `AbilitySet`을 부여한다"* 이고 **훅은 `MoveEntry` 한 곳**이다(Lyra `AbilitySetsToGrant` — `LyraEquipmentDefinition.h:49-51`). **회수 자리는 이 단계가 이미 만든다** — `RemoveEntry`의 ① write-back과 **같은 칸**이라, 05-3의 순서 규칙이 그대로 그쪽에도 걸린다
- 사망 시 전체 드랍 → §8 미정 #4
- 내구도(`Durability`) 실제 감소 로직
