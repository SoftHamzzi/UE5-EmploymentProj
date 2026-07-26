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
인벤토리 UI / 숫자키
    → Server_Equip(int32 EntryId)
    → Inventory: 그 EntryId가 실제로 있는가 확인, EquippedEntryId 갱신
    → CombatComponent::EquipFromInventory(EntryId)
         → FindDefinition(Entry.ItemId)를 UEPWeaponDefinition으로 확인
         → AEPWeapon 액터 스폰 + 소켓 부착 + LinkAnimClassLayers   ← 기존 EquipWeapon 경로 재사용
         → SetMaxAmmo / SetAmmo 주입 (05-2)
```

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
    if (Inv) Inv->SetEquippedEntryId(INDEX_NONE);

    /* 기존 해제 로직: 액터 파괴, 애님 레이어 해제 등 */
}
```

> **`AddEntryCharges`가 아니라 `SetEntryCharges`다.** write-back은 본질적으로 **대입**이라 델타 API에 넘기려면 현재값을 먼저 읽어야 하고, 그러면 읽고·빼고·넘기는 것이 **다른 컴포넌트에서** 벌어진다 — 원시 엔트리를 감춘 의미가 사라진다. 클램프와 `MarkItemDirty`는 여전히 `SetEntryCharges` 한 곳에만 있다 (Step 03 03-3).

> **05-2의 코드는 기존 함수 발췌**라 `AS`가 이미 스코프에 있다. 위는 **완결된 함수 본문**이므로 직접 얻어야 한다.

> **★ `MarkItemDirty`는 `AddEntryCharges()` 안에서 보장된다** (Step 03 03-3). 획득·버리기는 Add/Remove라 FastArray가 알아서 보내지만 **잔탄 write-back은 기존 항목의 필드 변경**이라, 빠뜨리면 서버만 맞고 소유 클라의 UI는 옛 값을 계속 보여준다. **원시 엔트리를 밖으로 내보내지 않으므로 호출자가 잊을 방법이 없다.**

> **null 가드가 필요하다.** 기존 코드가 `if (!GetOwner()->HasAuthority() || !EquippedWeapon) return;`로 시작하므로 그 옆에 붙인다. `GetOwner<AEPCharacter>()->GetInventoryComponent()`를 무가드로 체이닝하면 사망·리스폰 타이밍에서 터진다.

> **핸들을 이중화하지 않는다.** 이전 설계는 `UEPCombatComponent`에 서버 전용 `EquippedInstanceHandle`을 따로 뒀는데, 이유가 "버리기에서 인벤토리가 먼저 비워져도 write-back 대상을 잃지 않게" 였다 — **자료구조로 순서 실수를 막은 것.** 값 타입에서는 대상이 엔트리 자신이므로 그 방어가 성립하지 않는다.

### ★ 순서 규칙을 여기 적지 않는다 — `RemoveEntry()`가 보장한다

위험 자체는 실재한다: **엔트리를 지운 뒤 잔탄을 write-back하면 조용히 소실된다.** 증상은 "장착 무기를 버렸다 주우면 만탄"이고, **05-2의 `InitAmmo` 문제와 증상이 똑같아서 오진하기 쉽다.**

하지만 이걸 "순서를 지켜라"라는 문서 규칙으로 두면 이 문서와 Step 03 양쪽에 적히고, **한쪽만 고쳐지는 사고가 반드시 난다.** **`RemoveEntry(Id, &Out)`이 제거된 서브트리를 write-back이 끝난 상태로 반환한다** — 스냅샷을 얻는 유일한 방법이 제거하는 것이므로 순서를 뒤집는 게 문법적으로 불가능해진다. 구현은 Step 03 03-2·03-5에만 둔다.

**write-back이 필요한 지점은 세 곳이고, 전부 `UnequipWeapon()`을 거친다.**

| 시점 | 경로 |
|---|---|
| 무기 교체 | `EquipFromInventory` → 기존 무기 `UnequipWeapon()` |
| 버리기 | `RemoveEntry()`가 자동 호출 — **호출자가 신경 쓸 것이 없다** |
| 사망 | 사망 처리 → `UnequipWeapon()` |

**분기마다 따로 쓰지 않는다** — 하나를 빠뜨리면 그 경로에서만 잔탄이 되돌아가고, 재현 조건이 좁아 찾기 어렵다.

---

## 05-4. 장착 슬롯 표현

필드는 Step 03에서 이미 선언돼 있다(03-2) — `EquippedEntryId`(무기) / `EquippedBackpackEntryId`(배낭), 둘 다 `COND_OwnerOnly`. 이 단계는 **무기 쪽을 실제로 세팅하는 경로**를 만든다.

**`COND_OwnerOnly`로 충분하다.** 다른 클라이언트는 `AEPWeapon` 액터가 복제되고(`EPWeapon.cpp:19` `bReplicates = true`) 캐릭터 소켓에 부착되므로 **이미 보인다.** 이 값은 순전히 소유자 UI("어느 아이템이 장착 중인가")를 위한 것이다.

- 무기는 자기 `SlotSize`만큼 칸을 차지한 채로 장착된다. 별도 장비 슬롯 배열은 과하다
- **슬롯이 셋(주무기/보조/배낭)이 되면 `TMap<EEPEquipSlot, int32>`로 간다** — §8 미정 #5. 지금은 필드 둘이라 맵이 과하다
- **장착된 아이템은 `RemoveEntry()`가 알아서 해제한다** (Step 03 03-2)

> **★ `RemoveEntry`의 `EquippedEntryId` 분기는 이 단계에서 처음 실제로 돈다.** Step 03에는 그 값을 세팅하는 경로가 없어 분기가 내내 거짓이었다. **Step 03을 "통과"시켰다고 그 코드가 검증된 것이 아니다** — 장착 관련 버그는 여기서 처음 드러난다.

Step 04의 행 위젯에 장착 표시(테두리 강조)만 추가한다.

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

        const int32 EntryId = Inv->AddItem(INDEX_NONE, ItemId, State);   // 본체에
        if (EntryId == INDEX_NONE) continue;                    // 칸 부족. ★ if(EntryId) 금지

        if (FirstWeaponEntryId == INDEX_NONE && /* 무기 타입인가 */)
            FirstWeaponEntryId = EntryId;
    }

    if (FirstWeaponEntryId != INDEX_NONE)
        Char->GetCombatComponent()->EquipFromInventory(FirstWeaponEntryId);
}
```

> **`AddItem`이 `EntryId`를 돌려주는 것이 여기서 값을 한다.** 삽입 직후 그 엔트리를 바로 가리킬 수 있어 "방금 넣은 무기를 찾아 장착"에 재검색이 필요 없다.

> **★ 본체가 10칸뿐이다** (GAME.md). 무기 `SlotSize`가 5면 무기 + 탄약상자 + 붕대만 해도 7칸이고, 기본 지급이 조금만 늘어도 넘친다. 조용히 `continue`로 빠지면 **"가끔 탄약이 없다"** 로 나타나 원인을 못 찾는다 — 실패 시 반드시 경고 로그를 남긴다. 기본 지급에서는 **배낭 폴백을 쓰지 않는다**(시작 배낭이 없으므로).

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
| 6 | write-back을 경로별로 분산 | 특정 경로에서만 잔탄 되돌아감. 재현 조건이 좁음 | `UnequipWeapon()` 한 곳에만 |
| 7 | 사망 시 write-back 누락 | 시체/드랍의 잔탄이 만탄 | 사망 처리에서 `UnequipWeapon()` |
| 8 | `EquippedEntryId`를 무조건 복제 | 남이 뭘 들었는지 패킷으로 노출 | `COND_OwnerOnly` |
| 9 | 인벤토리가 `AEPWeapon`을 직접 스폰 | 무기 수명·애님·어빌리티 배선이 두 곳으로 갈림 | `CombatComponent`에 위임 |
| 10 | `DefaultWeaponClass`를 먼저 제거 | 무기 없이 스폰. 다른 버그로 오인 | `DefaultLoadout` 검증 후 제거 |
| 11 | `MaxSlots`가 `DefaultLoadout`보다 작음 | 뒤쪽 아이템이 조용히 누락 | 실패 시 경고 로그 (05-5) |

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

## 이 단계에서 하지 않는 것

- 재장전 시 탄약상자 `Charges` 차감 → §8 미정 #6
- 탄창을 별도 아이템으로 두기 → §8 미정 #1 (`FEPItemState`에 `AmmoType`이 필요해진다)
- 무기 2정 이상 슬롯(주무기/보조) → §8 미정 #5
- 방어구·헬멧 장착
- 사망 시 전체 드랍 → §8 미정 #4
- 내구도(`Durability`) 실제 감소 로직
