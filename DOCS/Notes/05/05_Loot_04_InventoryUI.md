# Step 04 — InventoryUI (1x1 고정 슬롯 표시)

> 마스터 기획: `05_Loot_DOCS.md` (§4-6)
> 선행: `05_Loot_03_Inventory.md` — `OnInventoryChanged` 델리게이트가 있어야 한다

---

## 목표

Tab으로 인벤토리 화면을 열어 아이콘·개수를 본다. **표시 전용이다.**

**완료 조건**

- [ ] Tab → 인벤토리 화면이 열리고 닫힌다. 마우스 커서 모드 전환
- [ ] 아이템을 주우면 **폴링 없이** 즉시 갱신된다
- [ ] **아이템을 줍고 버려도 기존 아이콘의 칸 위치가 바뀌지 않는다** (`SlotIndex` 검증)
- [ ] `Icon`이 없는 아이템도 플레이스홀더로 표시되고 이름은 보인다
- [ ] 슬롯 우클릭(또는 선택 + G) → 버리기가 동작한다
- [ ] 인벤토리를 연 채로 이동·사격이 된다 (`GameAndUI` 확인)
- [ ] **슬롯 0의 아이템을 버려도 슬롯 1·2의 아이콘이 제자리에 있다** (`SlotIndex` 검증의 핵심 케이스 — 0번을 비우는 것이 배열 순서 의존을 가장 잘 드러낸다)

---

## 04-1. 범위 통제 — 먼저 읽을 것

**이번 단계는 표시 전용이다.** 다음은 넣지 않는다.

| 넣지 않는 것 | 이유 |
|---|---|
| 드래그앤드롭 | `UDragDropOperation` + 드롭 대상 판정 + 서버 검증까지 붙으면 이 단계가 Step 03보다 커진다 |
| 정렬 / 아이템 이동 | 서버 권한 슬롯 재배치 RPC가 필요하다. 1x1 고정 슬롯에서는 이득도 작다 |
| 툴팁 / 상세 스펙 패널 | `FEPItemData`에 `Description`이 있으니 나중에 붙이면 된다 |
| 장비 슬롯 UI | Step 05에서 `EquippedHandle` 표시만 추가 |

조작이 필요하면 **우클릭 / 숫자키 / 선택 + G**로 처리한다. 방금 HUD 작업(04_GAS_08)을 크게 했으므로, 여기서 UMG 작업량을 다시 늘리면 Step 05가 밀린다.

---

## 04-2. 위젯 구성

```
WBP_Inventory (UEPInventoryWidget)          ← 화면 전체. 평소 Collapsed
└─ SlotGrid (UniformGridPanel)              ← MaxSlots개를 미리 채운다
     └─ WBP_InventorySlot × N (UEPInventorySlotWidget)
          ├─ SlotBackground (Image)
          ├─ ItemIcon       (Image)
          ├─ QuantityText   (TextBlock)     ← 1이면 숨김
          └─ RarityBorder   (Image)         ← EEPItemRarity로 색상
```

```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPInventoryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitWithInventory(UEPInventoryComponent* InInventory);
    void ToggleVisible();

protected:
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUniformGridPanel> SlotGrid;

    UPROPERTY(EditAnywhere, Category = "Inventory")
    TSubclassOf<UEPInventorySlotWidget> SlotWidgetClass;

    UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ClampMin = "1"))
    int32 Columns = 5;

private:
    void RebuildSlots();      // MaxSlots만큼 빈 칸 생성 (1회)
    void RefreshEntries();    // 엔트리를 SlotIndex 자리에 꽂는다

    UPROPERTY()
    TArray<TObjectPtr<UEPInventorySlotWidget>> Slots;   // 인덱스 == SlotIndex

    TWeakObjectPtr<UEPInventoryComponent> Inventory;
    FDelegateHandle ChangedHandle;
};
```

### ★ 칸을 먼저 만들고 엔트리를 꽂는다

```cpp
void UEPInventoryWidget::RefreshEntries()
{
    for (UEPInventorySlotWidget* Slot : Slots)      // 1) 전부 비우고
        Slot->Clear();

    for (const FEPInventoryEntry& E : Inventory->GetEntries())   // 2) SlotIndex 자리에 꽂는다
        if (Slots.IsValidIndex(E.SlotIndex))
            Slots[E.SlotIndex]->SetEntry(E);
}
```

> `GetEntries()`는 Step 03에서 `UEPInventoryComponent`에 추가하는 읽기 전용 뷰다. **반환 참조는 이 순회 안에서만 유효하다** — 순회 중에 `AddItem`/`DropItem`을 부르면 배열이 재할당되어 반복자가 깨진다.

**배열을 순회하며 순서대로 그리면 안 된다.** `FFastArraySerializer`는 클라이언트 배열의 순서를 보장하지 않는다 (`FastArraySerializer.h:54`). 순서대로 그리면 **아이템을 하나 줍거나 버릴 때마다 기존 아이콘들이 다른 칸으로 튄다.** 산발적으로 나고 서버는 멀쩡해서 재현이 어렵다.

`Slots` 배열의 인덱스가 곧 `SlotIndex`다. Step 03이 `SlotIndex`를 "제거해도 재배치되지 않는 고정 번호"로 정의한 것이 여기서 값을 한다.

---

## 04-3. 갱신은 이벤트로 — 폴링 금지

`FFastArraySerializer`가 클라이언트에 주는 콜백을 쓴다.

```
FEPInventoryEntry::PostReplicatedAdd     → Owner->OnInventoryChanged.Broadcast()
FEPInventoryEntry::PostReplicatedChange  → 동상
FEPInventoryEntry::PreReplicatedRemove   → 동상
```

위젯은 `OnInventoryChanged`를 구독해 `RefreshEntries()`를 부른다.

```cpp
void UEPInventoryWidget::InitWithInventory(UEPInventoryComponent* InInventory)
{
    if (Inventory.IsValid() && ChangedHandle.IsValid())
        Inventory->OnInventoryChanged.Remove(ChangedHandle);      // ★ 리스폰 재바인딩 안전

    Inventory = InInventory;
    if (!Inventory.IsValid()) return;

    ChangedHandle = Inventory->OnInventoryChanged.AddUObject(
        this, &UEPInventoryWidget::RefreshEntries);

    RebuildSlots();
    RefreshEntries();          // ★ 구독 직후 현재 상태를 1회 반영
}
```

**`NativeTick`에서 인벤토리를 읽지 않는다.** 인벤토리는 초당 수십 번 바뀌는 값이 아니다. 이건 `UEPHUDWidget`이 어트리뷰트를 구독하는 방식과 같고, `UEPSkillSlotWidget`이 `NativeTick`을 쓰는 것과는 다르다 — 저쪽은 GE 잔여 시간이 매 프레임 변하기 때문이다.

> **구독 직후 1회 반영을 빠뜨리면** 이미 아이템을 들고 있는 상태에서 창을 처음 열었을 때 빈 화면이 나온다. 다음 획득이 일어나야 채워진다. `InitWithASC` 계열에서 반복적으로 나오는 실수다.

> `UnbindAll` 패턴은 `UEPHUDWidget`에 이미 있다. 같은 형태로 `NativeDestruct`에서 해제한다.

---

## 04-4. 슬롯 위젯

```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetEntry(const FEPInventoryEntry& Entry);
    void Clear();

protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;

    UPROPERTY(meta = (BindWidget))       TObjectPtr<UImage>     ItemIcon;
    UPROPERTY(meta = (BindWidget))       TObjectPtr<UTextBlock> QuantityText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>   RarityBorder;

    UPROPERTY(EditAnywhere, Category = "Style")
    TMap<EEPItemRarity, FLinearColor> RarityColors;

    // 아이콘이 없거나 로드 전에 쓰는 대체 텍스처
    UPROPERTY(EditAnywhere, Category = "Style")
    TObjectPtr<UTexture2D> PlaceholderIcon;

private:
    int32 SlotIndex = INDEX_NONE;
    TSoftObjectPtr<UTexture2D> PendingIcon;
};
```

### 아이콘은 비동기 로드 + 플레이스홀더

```cpp
void UEPInventorySlotWidget::SetEntry(const FEPInventoryEntry& Entry)
{
    SlotIndex = Entry.SlotIndex;

    const FEPItemData* Data = /* DefinitionSubsystem->FindData(Entry.ItemId) */;
    if (!Data) return;

    QuantityText->SetText(FText::AsNumber(Entry.Quantity));
    QuantityText->SetVisibility(Entry.Quantity > 1
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

    if (RarityBorder)
        if (const FLinearColor* C = RarityColors.Find(Data->Rarity))
            RarityBorder->SetColorAndOpacity(*C);

    ItemIcon->SetBrushFromTexture(PlaceholderIcon);      // 먼저 플레이스홀더
    /* Definition->Icon 비동기 로드 후 교체 — CreateWeakLambda */
}
```

**현재 `Icon`이 설정된 아이템은 하나도 없다.** `Ammo_762` / `Bandage` 같은 신규 행은 Definition 에셋조차 없다(스택 아이템이라 필요 없음). 플레이스홀더 + 이름 표시가 **이번 단계의 실질적 표시 수단**이다. 아이콘이 없다고 슬롯이 비어 보이면 검증 자체가 안 된다.

> 비동기 로드 콜백은 `CreateWeakLambda`를 쓴다. 로드 도착 전에 창을 닫거나 슬롯이 `Clear()`되면 일반 람다는 죽은 상태를 건드린다.

`Quantity == 1`이면 개수를 숨긴다 — 무기마다 "x1"이 붙으면 지저분하다.

---

## 04-5. 버리기 연결

```cpp
FReply UEPInventorySlotWidget::NativeOnMouseButtonDown(
    const FGeometry& Geo, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() == EKeys::RightMouseButton && SlotIndex != INDEX_NONE)
    {
        /* PlayerController → Server_DropItem(SlotIndex, 전량) */
        return FReply::Handled();
    }
    return FReply::Unhandled();
}
```

- **UI는 `SlotIndex`만 보낸다.** 서버가 그 칸의 엔트리를 확인하고 판정한다 (Step 03 03-5)
- 부분 버리기(수량 지정)는 이번 범위 밖이다. 우클릭 = 전량
- **클라이언트에서 엔트리를 미리 지우지 않는다.** 서버 응답으로 `PreReplicatedRemove`가 오면 그때 사라진다. 예측하면 거부됐을 때 되돌려야 한다

---

## 04-6. 입력과 모드 전환

- `IA_ToggleInventory` (Tab) → `AEPPlayerController`에 UPROPERTY + 게터 (기존 스킬 입력과 동일 패턴)
- 열 때: `SetInputMode(FInputModeGameAndUI)` + `bShowMouseCursor = true`
- 닫을 때: `FInputModeGameOnly` + 커서 숨김

> **`FInputModeUIOnly`를 쓰지 않는다.** 인벤토리를 연 상태에서도 주변 상황이 보이고 이동할 수 있어야 한다 — 익스트랙션 슈터에서 가방을 여는 동안 완전히 무방비가 되는 건 의도된 리스크지만, **입력이 통째로 막히면 리스크가 아니라 버그처럼 느껴진다.** 타르코프도 인벤토리 중 이동이 된다.

`UEPHUDWidget`과 별개 위젯으로 두고 `AEPPlayerController::InitHUD`에서 같이 생성·바인딩한다.

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | 배열 순서대로 그림 | 줍고 버릴 때마다 아이콘이 다른 칸으로 튐 | `SlotIndex`로 꽂기 (04-2) |
| 2 | 구독 직후 1회 반영 누락 | 창을 처음 열면 빈 화면. 다음 획득 때 채워짐 | `InitWithInventory` 끝에서 `RefreshEntries()` |
| 3 | `NativeTick`에서 폴링 | 불필요한 비용, FastArray를 쓴 의미가 없어짐 | 콜백 구독 |
| 4 | 재바인딩 시 기존 핸들 미해제 | 리스폰 후 `RefreshEntries`가 2번씩 불림 | `Remove(ChangedHandle)` 선행 |
| 5 | 아이콘 로드에 일반 람다 | 창 닫은 뒤 콜백 도착 시 크래시 | `CreateWeakLambda` |
| 6 | 플레이스홀더 없음 | 아이콘이 없어 슬롯이 전부 비어 보임 → 검증 불가 | 04-4 |
| 7 | `FInputModeUIOnly` | 인벤토리 중 이동 불가. 버그처럼 느껴짐 | `GameAndUI` |
| 8 | 클라에서 엔트리 선삭제 | 서버가 거부하면 아이템이 사라진 채로 남음 | 예측 금지 |

---

## 이 단계에서 하지 않는 것

- 드래그앤드롭 / 정렬 / 슬롯 간 이동
- 부분 버리기(수량 지정 UI)
- 툴팁 / 아이템 상세 패널
- 장비 슬롯 표시 → **Step 05**
- 컨테이너 내용물 UI → §7-1
