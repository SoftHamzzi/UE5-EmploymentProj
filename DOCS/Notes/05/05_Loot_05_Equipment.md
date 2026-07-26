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
- [ ] 장착 중인 무기를 버리면 자동 해제되고 잔탄이 인스턴스에 기록된다
- [ ] 사망 시에도 잔탄이 write-back된다
- [ ] `GA_Item_PrimaryUse` / `GA_Item_Reload`는 **한 줄도 수정하지 않았다**

---

## 05-1. 문제 — 탄약의 진실이 두 곳에 있다

| 위치 | 성격 | 현재 |
|---|---|---|
| `UEPAttributeSet::Ammo` / `MaxAmmo` | 캐릭터(ASC) 소유. **1인당 하나뿐.** 복제되고 HUD가 구독. GAS 발사·재장전이 읽고 쓴다 | 실제로 쓰임 |
| `UEPWeaponInstance::CurrentAmmo` | 아이템 인스턴스 소유. 무기마다 다름 | 데드코드 |

인벤토리에 무기 2정을 넣고 교체하려면 **정별로** 잔탄이 보존돼야 하는데, GAS 어트리뷰트는 캐릭터에 하나뿐이다.

### 결정: 어트리뷰트는 "현재 장착 무기의 뷰", 인스턴스가 진실

```
Equip   : Instance.CurrentAmmo  →  SetAmmo()              (주입)
Unequip : GetAmmo()             →  Instance.CurrentAmmo   (write-back)
```

- 발사·재장전은 지금처럼 GAS 어트리뷰트만 건드린다. **어빌리티는 수정하지 않는다**
- **해제 시점의 write-back을 빠뜨리면** 무기를 바꿨다 돌아올 때 잔탄이 되돌아간다
- **사망 시에도 write-back이 필요하다** — 시체/드랍의 잔탄이 맞아야 한다

> 이 구조의 대안은 "무기마다 ASC를 둔다"인데, ASC는 무겁고 복제 비용이 크며 GAS의 어트리뷰트 모델과 맞지 않는다. Lyra도 장착 무기 하나를 어트리뷰트로 표현하고 나머지는 인스턴스 상태로 둔다.

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
AS->SetMaxAmmo(static_cast<float>(WeaponDef->MaxAmmo));    // 무기 스펙
AS->SetAmmo(static_cast<float>(Instance->CurrentAmmo));    // 인스턴스 상태
```

> **`Init*` → `Set*`로 바꾸는 이유:** `Init*` 계열은 어트리뷰트 변경 델리게이트를 **발생시키지 않는다.** `UEPHUDWidget`이 `GetGameplayAttributeValueChangeDelegate`로 탄약을 구독하고 있으므로(`EPHUDWidget.cpp:26`), `Init*`으로 두면 장착 즉시 HUD가 갱신되지 않는다. 지금은 `RefreshAmmo()`가 다른 경로로 불려 가려져 있을 수 있으니, 무기 교체를 반복하며 확인할 것.

> `MaxAmmo`를 먼저 세팅한다. `UEPAttributeSet::PreAttributeChange`가 `Ammo`를 `[0, MaxAmmo]`로 클램프하므로(`EPAttributeSet.cpp:17-18`), 순서를 뒤집으면 이전 무기의 낮은 `MaxAmmo`에 잘린다.

---

## 05-3. `EquipFromInventory` — 무기 액터 스폰 책임은 그대로 둔다

```cpp
// UEPCombatComponent
void EquipWeapon(AEPWeapon* NewWeapon);              // 기존 — 유지
void EquipFromInventory(int32 Handle);               // 신규
void UnequipWeapon();                                // 기존 — write-back 추가
```

```
인벤토리 UI / 숫자키
    → Server_Equip(int32 SlotIndex)
    → Inventory: SlotIndex → Handle 확인, EquippedHandle 갱신
    → CombatComponent::EquipFromInventory(Handle)
         → Instance→CachedDefinition을 UEPWeaponDefinition으로 확인
         → AEPWeapon 액터 스폰 + 소켓 부착 + LinkAnimClassLayers   ← 기존 EquipWeapon 경로 재사용
         → SetMaxAmmo / SetAmmo 주입 (05-2)
```

**★ 인벤토리 컴포넌트가 `AEPWeapon`을 스폰하지 않는다.** 무기 액터의 수명·부착·애님 레이어·어빌리티 부여를 이미 `UEPCombatComponent`가 전부 쥐고 있다(`EPCombatComponent.cpp:162`). 인벤토리는 **핸들만 넘긴다.**

기존 `EquipWeapon(AEPWeapon*)`을 남겨두고 `EquipFromInventory`가 그쪽으로 위임하면, 기존 호출부를 한 번에 갈아엎지 않아도 되고 이관 중 두 경로를 비교 검증할 수 있다.

### `UnequipWeapon`에 write-back 추가

```cpp
// UEPCombatComponent에 필드 하나를 추가한다 (서버 전용, 복제하지 않음)
//   int32 EquippedInstanceHandle = INDEX_NONE;
// 인벤토리의 EquippedHandle(복제, UI용)과는 별개다 — 아래 참조

void UEPCombatComponent::UnequipWeapon()
{
    if (EquippedInstanceHandle != INDEX_NONE)
        if (UEPWeaponInstance* Inst = Cast<UEPWeaponInstance>(
                InstanceSubsystem->Find(EquippedInstanceHandle)))
            Inst->CurrentAmmo = FMath::RoundToInt(AS->GetAmmo());   // ★ write-back

    EquippedInstanceHandle = INDEX_NONE;
    /* 기존 해제 로직: 액터 파괴, 애님 레이어 해제 등 */
}
```

> **핸들이 두 곳에 있는 이유:** `UEPInventoryComponent::EquippedHandle`은 **복제되는 UI용 값**(어느 슬롯이 장착 중인가)이고, `UEPCombatComponent::EquippedInstanceHandle`은 **서버 전용 작업 값**이다. write-back은 서버에서만 일어나므로 CombatComponent가 자기 값을 들고 있는 편이 낫다 — 매번 인벤토리를 거치면 "인벤토리가 먼저 비워진 뒤 해제되는" 순서에서 핸들을 잃는다(버리기 경로가 정확히 그렇다).
>
> 두 값은 `EquipFromInventory` / `UnequipWeapon`에서 **함께 갱신**한다. 어긋나면 UI가 장착 표시를 잘못 그린다.

**write-back이 필요한 지점은 세 곳이다.**

| 시점 | 경로 |
|---|---|
| 무기 교체 | `EquipFromInventory` → 기존 무기 `UnequipWeapon()` |
| 버리기 | Step 03 `Server_DropItem` 3단계 → `UnequipWeapon()` |
| 사망 | 사망 처리 → `UnequipWeapon()`. **`EndPlay`보다 먼저** |

셋 다 `UnequipWeapon()`을 거치게 만들면 write-back은 한 곳에만 있으면 된다. **분기마다 따로 쓰지 않는다** — 하나를 빠뜨리면 그 경로에서만 잔탄이 되돌아가고, 재현 조건이 좁아 찾기 어렵다.

---

## 05-4. 장착 슬롯 표현

```cpp
// UEPInventoryComponent
UPROPERTY(Replicated)
int32 EquippedHandle = INDEX_NONE;      // DOREPLIFETIME_CONDITION(..., COND_OwnerOnly)
```

**`COND_OwnerOnly`로 충분하다.** 다른 클라이언트는 `AEPWeapon` 액터가 복제되고(`EPWeapon.cpp:19` `bReplicates = true`) 캐릭터 소켓에 부착되므로 **이미 보인다.** 이 값은 순전히 소유자 UI("어느 슬롯이 장착 중인가")를 위한 것이다.

- 무기는 인벤토리 슬롯을 차지한 채로 장착된다. 1x1 고정 슬롯 단계에서 별도 장비 슬롯 배열은 과하다
- 확장 시 `TMap<EEPEquipSlot, int32>`로 바꾼다 (헬멧·가방·보조무기)
- **장착된 인스턴스는 버리기 전에 자동 해제한다** (Step 03 05-3단계)

Step 04의 슬롯 위젯에 장착 표시(테두리 강조)만 추가한다.

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

    UEPInventoryComponent* Inv = Char->GetInventoryComponent();
    for (const FName& ItemId : DefaultLoadout)
        Inv->AddItem(ItemId, /* 수량은 MaxStack 기준 */ 1);

    /* 무기 타입 첫 항목을 자동 장착 */
}
```

`AEPWeapon` 액터 자체와 `UEPCombatComponent`의 발사 로직은 **건드리지 않는다.** 바뀌는 것은 "누가 언제 무기 액터를 만드는가"뿐이다.

> 기존 `DefaultWeaponClass`는 `BP_EPGameMode` 에셋에 값이 들어 있다. 제거하면 그 값이 사라지므로, **`DefaultLoadout`에 `Weapon_AK74_HitScan`을 먼저 채워 넣고 검증한 뒤** 옛 필드를 지운다. 순서를 반대로 하면 무기 없이 스폰되어 다른 버그로 오인한다.

> `DefaultLoadout`에는 탄약도 넣을 수 있다(`Ammo_762` 등). 다만 재장전이 인벤토리 탄약을 소비하는 건 §8 미정 #6이라, 지금은 넣어도 소비되지 않는다.

---

## 05-6. 타입 정리 (Step 00에서 이미 처리)

`UEPWeaponDefinition::MaxAmmo`는 Step 00에서 `uint8` → `int32`로 바뀌었다. 이 단계의 주입/write-back 경로가 그 위를 왕복한다.

```
WeaponDefinition::MaxAmmo   int32     ← Step 00에서 통일
WeaponInstance::CurrentAmmo int32
AttributeSet::Ammo          float     ← GAS 규격상 float 고정
```

정수 ↔ float 경계는 `FMath::RoundToInt()`로 한 곳에서만 넘는다 (write-back). 주입 방향은 `static_cast<float>`로 충분하다 — 정보 손실이 없다.

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | `InitAmmo(MaxAmmo)` 잔존 | **버렸다 줍기만 해도 만탄** (익스플로잇) | 05-2 |
| 2 | `Init*` 유지 | 장착해도 HUD 탄약이 안 바뀜 | `Set*` |
| 3 | `Ammo`를 `MaxAmmo`보다 먼저 세팅 | 이전 무기의 낮은 `MaxAmmo`에 클램프됨 | `MaxAmmo` 먼저 |
| 4 | write-back을 경로별로 분산 | 특정 경로에서만 잔탄 되돌아감. 재현 조건이 좁음 | `UnequipWeapon()` 한 곳에만 |
| 5 | 사망 시 write-back 누락 | 시체/드랍의 잔탄이 만탄 | 사망 처리에서 `UnequipWeapon()` |
| 6 | 사망 드랍이 `EndPlay` 뒤 | 죽으면 아이템이 그냥 사라짐 | 드랍 → `EndPlay` 순서 |
| 7 | `EquippedHandle`을 무조건 복제 | 남이 뭘 들었는지 패킷으로 노출 | `COND_OwnerOnly` |
| 8 | 인벤토리가 `AEPWeapon`을 직접 스폰 | 무기 수명·애님·어빌리티 배선이 두 곳으로 갈림 | `CombatComponent`에 위임 |
| 9 | `DefaultWeaponClass`를 먼저 제거 | 무기 없이 스폰. 다른 버그로 오인 | `DefaultLoadout` 검증 후 제거 |

---

## 검증 시나리오 (PIE 2인, **클라이언트 쪽에서** 확인)

```
1. 클라이언트로 접속 → DefaultLoadout대로 무기 장착 확인
2. 18발 발사 → HUD 12/30
3. G로 버리기 → 픽업 생성. 0.5초간 프롬프트 회색
4. 다시 줍기 → 인벤토리에 들어감
5. 장착 → ★ HUD가 12/30 이어야 한다 (30/30이면 05-2 미적용)
6. EP.Item.Dump → LiveInstances가 3~5 사이에 변하지 않았어야 한다 (재생성 없음)
7. 두 번째 무기를 주워 번갈아 장착 → 각각의 잔탄 보존
8. 사망 → 리스폰 후 EP.Item.Dump로 인스턴스 정리 확인
```

> **리슨서버 호스트가 아니라 클라이언트에서** 확인한다. 호스트는 권한과 예측이 같아 동기화 버그가 드러나지 않는다 (GAS 08단계에서 겪은 교훈).

---

## 이 단계에서 하지 않는 것

- 재장전 시 인벤토리 탄약 차감 → §8 미정 #6
- 무기 2정 이상 슬롯(주무기/보조) → §8 미정 #5
- 방어구·헬멧 장착
- 사망 시 전체 드랍 → §8 미정 #4
- 내구도(`Durability`) 실제 감소 로직
