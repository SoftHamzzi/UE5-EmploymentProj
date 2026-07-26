# Step 04 — InventoryUI (아이템 목록 + 칸 수 게이지)

> 마스터 기획: `05_Loot_DOCS.md` (§4-6)
> 선행: `05_Loot_03_Inventory.md` — `OnInventoryChanged` 델리게이트가 있어야 한다

---

## 목표

Tab으로 인벤토리 화면을 열어 소지품 목록과 칸 사용량을 본다. **표시 전용이다.**

**완료 조건**

- [ ] Tab → 인벤토리 화면이 열리고 닫힌다. 마우스 커서 모드 전환
- [ ] 아이템을 주우면 **폴링 없이** 즉시 갱신된다
- [ ] **목록 맨 위 항목을 버려도 나머지 항목의 순서가 바뀌지 않는다** (`EntryId` 검증의 핵심 케이스)
- [ ] `Icon`이 없는 아이템도 플레이스홀더로 표시되고 이름은 보인다
- [ ] **`7 / 10 칸` 게이지가 아이템의 `SlotSize`대로 증감한다** — 무기 하나를 넣으면 5가 오른다
- [ ] **붕대 3개가 3줄로 보인다** (스택 안 됨). **현금뭉치 둘은 1줄에 합산 금액**
- [ ] **배낭을 매면 두 번째 구획이 나타나고, 각자 게이지를 갖는다.** 안 매면 Collapsed
- [ ] **배낭을 버리면 두 번째 구획이 통째로 사라진다**
- [ ] 항목 우클릭(또는 선택 + G) → 버리기가 동작한다
- [ ] 인벤토리를 연 채로 이동·사격이 된다 (`GameAndUI` 확인)

> **1x1 고정 슬롯 격자가 아니다.** 아이템마다 차지하는 칸 수가 다르고 배치가 아니라 **합산**이므로(§4-6), 격자에 아이콘을 꽂는 대신 **목록 + 사용량 게이지**로 그린다. UMG 작업량도 이쪽이 적다.

---

## 04-1. 범위 통제 — 먼저 읽을 것

**이번 단계는 표시 전용이다.** 다음은 넣지 않는다.

| 넣지 않는 것 | 이유 |
|---|---|
| 드래그앤드롭 | `UDragDropOperation` + 드롭 대상 판정 + 서버 검증까지 붙으면 이 단계가 Step 03보다 커진다 |
| 정렬 / 아이템 이동 | 서버 권한 재배치 RPC가 필요하다. 배치가 아니라 합산이므로 이득도 작다 |
| 툴팁 / 상세 스펙 패널 | `FEPItemData`에 `Description`이 있으니 나중에 붙이면 된다 |
| 장비 슬롯 UI | Step 05에서 `EquippedEntryId` 강조만 추가 |

조작이 필요하면 **우클릭 / 숫자키 / 선택 + G**로 처리한다. 방금 HUD 작업(04_GAS_08)을 크게 했으므로, 여기서 UMG 작업량을 다시 늘리면 Step 05가 밀린다.

---

## 04-2. 위젯 구성

```
WBP_Inventory (UEPInventoryWidget)          ← 화면 전체. 평소 Collapsed
├─ BodySection (UEPContainerPanel)          ← 본체 (10칸)
│    ├─ CapacityText / CapacityBar          "7 / 10 칸"
│    └─ ItemList (VerticalBox)
└─ BackpackSection (UEPContainerPanel)      ← 매고 있는 배낭. 없으면 Collapsed
     ├─ CapacityText / CapacityBar          "3 / 12 칸"
     └─ ItemList (VerticalBox)

WBP_InventoryRow × N (UEPInventoryRowWidget)
          ├─ ItemIcon    (Image)
          ├─ NameText    (TextBlock)
          ├─ SlotText    (TextBlock)        ← "5칸"
          ├─ ChargesText (TextBlock)        ← "12 / 30" 또는 "100발". 0이면 숨김
          └─ RarityBorder (Image)           ← EEPItemRarity로 색상
```

```cpp
// 컨테이너 한 구획. 본체와 배낭이 같은 클래스를 쓴다
UCLASS()
class EMPLOYMENTPROJ_API UEPContainerPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    void Rebuild(UEPInventoryComponent* Inv, int32 Container);

protected:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UPanelWidget> ItemList;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock>   CapacityText;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> CapacityBar;

    // ★ 행을 만드는 쪽이 여기이므로 클래스 참조도 여기 있어야 한다
    UPROPERTY(EditAnywhere, Category = "Inventory")
    TSubclassOf<UEPInventoryRowWidget> RowWidgetClass;
};

UCLASS()
class EMPLOYMENTPROJ_API UEPInventoryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitWithInventory(UEPInventoryComponent* InInventory);
    void ToggleVisible();

protected:
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget)) TObjectPtr<UEPContainerPanel> BodySection;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UEPContainerPanel> BackpackSection;

private:
    void RefreshEntries();

    TWeakObjectPtr<UEPInventoryComponent> Inventory;
    FDelegateHandle ChangedHandle;
};
```

> **★ 선언·위젯 트리·구현 셋이 일치해야 한다.** `BindWidget`은 WBP에 같은 이름의 위젯이 없으면 **위젯 블루프린트 컴파일에서 실패**한다. 초안은 `UEPInventoryWidget`이 `ItemList`/`CapacityText`/`CapacityBar`를 직접 `BindWidget`으로 갖는데 트리에서는 그 셋이 구획 **안**에 있고 구현은 구획을 쓰는, 3중 불일치 상태였다. `RowWidgetClass`도 쓰는 쪽(`UEPContainerPanel::Rebuild`)으로 옮겼다.

### ★ 컨테이너별로 나눠 그리고, 각 구획은 `EntryId` 오름차순

```cpp
void UEPInventoryWidget::RefreshEntries()
{
    BodySection->Rebuild(Inventory.Get(), INDEX_NONE);          // 본체

    const int32 Backpack = Inventory->GetEquippedBackpack();
    BackpackSection->SetVisibility(Backpack != INDEX_NONE
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Backpack != INDEX_NONE)
        BackpackSection->Rebuild(Inventory.Get(), Backpack);
}

void UEPContainerPanel::Rebuild(UEPInventoryComponent* Inv, int32 Container)
{
    ItemList->ClearChildren();

    TArray<FEPInventoryEntry> Rows;
    for (const FEPInventoryEntry& E : Inv->GetEntries())
        if (E.ParentEntryId == Container && E.SlotId.IsNone())   // 부착물 제외 (§7-3)
            Rows.Add(E);                                          // ★ 값 복사

    Rows.Sort([](const FEPInventoryEntry& A, const FEPInventoryEntry& B)
              { return A.EntryId < B.EntryId; });                 // ★

    for (const FEPInventoryEntry& E : Rows)
    {
        UEPInventoryRowWidget* Row = CreateWidget<UEPInventoryRowWidget>(this, RowWidgetClass);
        Row->SetEntry(E);
        ItemList->AddChild(Row);
    }

    const int32 Used = Inv->GetUsedSlots(Container);
    const int32 Max  = Inv->GetCapacity(Container);
    CapacityText->SetText(FText::Format(NSLOCTEXT("EP", "Cap", "{0} / {1} 칸"), Used, Max));
    CapacityBar->SetPercent(Max > 0 ? float(Used) / float(Max) : 0.f);
}
```

**게이지가 두 개인 것이 GAME.md의 "본체와 배낭의 칸 수는 통합되지 않는다"를 화면으로 드러낸다.** 하나로 합쳐 그리면 플레이어가 왜 못 넣는지 이해하지 못한다.

**정렬을 빠뜨리면 안 된다.** `FFastArraySerializer`는 클라이언트 배열의 순서를 보장하지 않는다 (`FastArraySerializer.h:54`). 그대로 순회하면 **아이템을 하나 줍거나 버릴 때마다 기존 항목들이 자리를 바꾼다.** 산발적으로 나고 서버는 멀쩡해서 재현이 어렵다.

`EntryId`는 재번호되지 않으므로(Step 03), 오름차순 정렬은 **새 아이템이 항상 끝에 붙고 기존 항목은 움직이지 않는다**는 뜻이다.

> **매번 위젯을 다시 만드는 것이 낭비 아닌가:** 엔트리가 20~30개고 갱신은 획득·버리기 때마다(초당 수십 번이 아니다) 일어난다. 위젯 풀링은 지금 넣으면 순수 복잡도다.
>
> **★ 나중에도 `UListView`로 가지 않는다.** 아이템 타입이 `UObject*`로 고정돼 있다.
> ```cpp
> // UMG/Public/Components/ListView.h:38
> class UListView : public UListViewBase, public ITypedUMGListView<UObject*>
> ```
> 도입하면 엔트리마다 UObject 래퍼를 만들어 넘겨야 한다 — **방금 지운 `UEPItemInstance`를 UI 전용으로 부활시키는 것**이고 수명 관리까지 딸려온다. 즉 "지금은 30행이라 괜찮다"가 아니라 **값 타입 설계에서는 수동 재생성이 오히려 정합적**이다. 목록이 커지면 `UListView`가 아니라 **풀링을 직접** 넣는다.

> `GetEntries()`는 Step 03에서 추가하는 읽기 전용 뷰다. **반환 참조는 그 프레임 안에서만 유효하다** — 위에서 값으로 복사해 정렬하는 이유이기도 하다.

---

## 04-3. 갱신은 이벤트로 — 폴링 금지

**항목별 콜백 3종을 쓰지 않는다.** `PostReplicatedAdd`는 **항목마다** 불려(`FastArraySerializer.h:1163`) 한 번의 수신에 목록 재생성이 항목 수만큼 돈다. 대신 직렬화기 쪽 콜백 하나를 쓴다 (Step 03 03-7).

```cpp
// FEPInventoryList
void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters&)
{
    if (Owner) Owner->OnInventoryChanged.Broadcast();     // 수신 1회당 1회
}
```

**Add/Change/Remove를 구분할 필요가 없다** — 어차피 전체 재생성이고, 빠뜨릴 콜백 자체가 없어진다.

> **`Owner`가 세팅돼 있어야 한다.** `UEPInventoryComponent` 생성자의 `Entries.Owner = this;`(Step 03 03-2)가 없으면 콜백이 델리게이트에 닿지 못하고, 증상은 **"서버는 정상인데 클라 UI가 영원히 갱신 안 됨"** 이다. 원인이 UI나 복제로 보여서 엉뚱한 데를 판다.

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

    RefreshEntries();          // ★ 구독 직후 현재 상태를 1회 반영
}
```

**`NativeTick`에서 인벤토리를 읽지 않는다.** 인벤토리는 초당 수십 번 바뀌는 값이 아니다. 이건 `UEPHUDWidget`이 어트리뷰트를 구독하는 방식과 같고, `UEPSkillSlotWidget`이 `NativeTick`을 쓰는 것과는 다르다 — 저쪽은 GE 잔여 시간이 매 프레임 변하기 때문이다.

> **구독 직후 1회 반영을 빠뜨리면** 이미 아이템을 들고 있는 상태에서 창을 처음 열었을 때 빈 화면이 나온다. 다음 획득이 일어나야 채워진다. `InitWithASC` 계열에서 반복적으로 나오는 실수다.

> `UnbindAll` 패턴은 `UEPHUDWidget`에 이미 있다. 같은 형태로 `NativeDestruct`에서 해제한다.

---

## 04-4. 행 위젯

```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPInventoryRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetEntry(const FEPInventoryEntry& Entry);

protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;

    UPROPERTY(meta = (BindWidget))         TObjectPtr<UImage>     ItemIcon;
    UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock> NameText;
    UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock> SlotText;
    UPROPERTY(meta = (BindWidget))         TObjectPtr<UTextBlock> ChargesText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage>     RarityBorder;

    UPROPERTY(EditAnywhere, Category = "Style")
    TMap<EEPItemRarity, FLinearColor> RarityColors;

    // 아이콘이 없거나 로드 전에 쓰는 대체 텍스처
    UPROPERTY(EditAnywhere, Category = "Style")
    TObjectPtr<UTexture2D> PlaceholderIcon;

private:
    int32 EntryId = INDEX_NONE;
    TSharedPtr<FStreamableHandle> IconHandle;
};
```

### 아이콘은 비동기 로드 + 플레이스홀더

```cpp
void UEPInventoryRowWidget::SetEntry(const FEPInventoryEntry& Entry)
{
    EntryId = Entry.EntryId;

    const FEPItemData* Data = /* DefinitionSubsystem->FindData(Entry.ItemId) */;
    if (!Data) return;

    NameText->SetText(Data->DisplayName);
    SlotText->SetText(FText::Format(NSLOCTEXT("EP", "Slots", "{0}칸"), Data->SlotSize));

    // Charges가 0인 아이템(잡템·퀘스트)은 숫자를 숨긴다
    ChargesText->SetVisibility(Entry.State.Charges > 0
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    ChargesText->SetText(FText::AsNumber(Entry.State.Charges));

    if (RarityBorder)
        if (const FLinearColor* C = RarityColors.Find(Data->Rarity))
            RarityBorder->SetColorAndOpacity(*C);

    ItemIcon->SetBrushFromTexture(PlaceholderIcon);      // 먼저 플레이스홀더
    /* Definition->Icon 비동기 로드 후 교체 — CreateWeakLambda */
}
```

**`Charges`를 표시할 수 있는 것이 이 설계 전환의 직접적 이득이다.** 서버 전용 인스턴스 방식에서는 **가방 속 두 번째 소총의 잔탄을 UI에 표시할 방법이 아예 없었다**(핸들 → 서버 전용 객체 → 클라가 못 읽음). GAS `Ammo` 어트리뷰트는 장착 무기 하나만 커버한다.

**현재 `Icon`이 설정된 아이템은 하나도 없다.** Step 00에서 만든 `DA_AmmoBox_545` / `DA_Bandage` 등도 `Icon`을 비워뒀다. 플레이스홀더 + 이름 표시가 **이번 단계의 실질적 표시 수단**이다.

> 비동기 로드 콜백은 `CreateWeakLambda`를 쓴다. 로드 도착 전에 창을 닫거나 목록이 다시 만들어지면 일반 람다는 죽은 위젯을 건드린다. **매번 행을 재생성하므로(04-2) 이 경로가 자주 돈다** — 고정 슬롯 방식보다 더 중요하다.

---

## 04-5. 버리기 연결

```cpp
FReply UEPInventoryRowWidget::NativeOnMouseButtonDown(
    const FGeometry& Geo, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() == EKeys::RightMouseButton && EntryId != INDEX_NONE)
    {
        /* PlayerController → Server_DropItem(EntryId) */
        return FReply::Handled();
    }
    return FReply::Unhandled();
}
```

- **UI는 `EntryId`만 보낸다.** 서버가 그 엔트리를 확인하고 판정한다 (Step 03 03-5)
- 스택이 없으므로 "몇 개 버릴까"라는 질문이 없다. 우클릭 = 그 아이템 하나
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
| 1 | `EntryId` 정렬 없이 배열 순서대로 그림 | 줍고 버릴 때마다 항목 순서가 튐 | `Sort` (04-2) |
| 2 | 구독 직후 1회 반영 누락 | 창을 처음 열면 빈 화면. 다음 획득 때 채워짐 | `InitWithInventory` 끝에서 `RefreshEntries()` |
| 3 | **`Entries.Owner = this` 누락** (Step 03 03-2) | 클라 UI가 **영원히** 갱신 안 됨. 원인이 UI/복제로 보인다 | 04-3 ★ |
| 3b | 항목별 콜백 3종을 씀 | 수신 1회에 목록 재생성이 항목 수만큼. `PostReplicatedChange` 누락 시 잔탄만 옛날 값 | `PostReplicatedReceive` 하나 (04-3) |
| 4 | `NativeTick`에서 폴링 | 불필요한 비용, FastArray를 쓴 의미가 없어짐 | 콜백 구독 |
| 5 | 재바인딩 시 기존 핸들 미해제 | 리스폰 후 `RefreshEntries`가 2번씩 불림 | `Remove(ChangedHandle)` 선행 |
| 6 | 아이콘 로드에 일반 람다 | 목록 재생성/창 닫기 후 콜백 도착 시 크래시 | `CreateWeakLambda` |
| 7 | 플레이스홀더 없음 | 아이콘이 없어 목록이 전부 비어 보임 → 검증 불가 | 04-4 |
| 8 | 게이지를 엔트리 **개수**로 계산 | 무기(5칸)를 넣어도 1만 오름 → 합산 로직 버그가 무증상 | `GetUsedSlots(Container)` (04-2) |
| 8b | 본체와 배낭을 **한 목록에** 그림 | 왜 못 넣는지 플레이어가 이해 못 함. GAME.md의 "통합되지 않는다"가 화면에 안 드러남 | 구획 2개 + 게이지 2개 (04-2) |
| 8c | 부착물 자식을 목록에 섞어 그림 | 조준경이 가방 목록에 따로 뜬다 (§7-3 도입 후) | `SlotId.IsNone()` 필터 (04-2) |
| 9 | `FInputModeUIOnly` | 인벤토리 중 이동 불가. 버그처럼 느껴짐 | `GameAndUI` |
| 10 | 클라에서 엔트리 선삭제 | 서버가 거부하면 아이템이 사라진 채로 남음 | 예측 금지 |

---

## 이 단계에서 하지 않는 것

- 드래그앤드롭 / 정렬 / 항목 간 이동
- 툴팁 / 아이템 상세 패널
- 장착 강조 표시 → **Step 05** (`EquippedEntryId`)
- 컨테이너 내용물 UI → §7-1
- 2D 격자(테트리스) 배치 → 데이터 모델에 2D 크기가 생기면 (§4-6)
