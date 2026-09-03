# 검수 답변 10차 — Step 04 격자 UI와 "순서를 서버에 두지 않는다"

> 작성일: 2026-08-23
> 요청서: `05_Loot_REVIEW10_Request.md` / 제안 본체: `05_Loot_04_InventoryUI.md` (715줄)
> 근거: UE 5.7 엔진 직독(`C:\Program Files\Epic Games\UE_5.7\Engine`) · Lyra 직독 · 프로젝트 문서·소스 직독
> **기억으로 단정한 API는 없다.** 인용은 전부 §12 표에 파일·줄로 있다

---

## 0. 판정 요약

| 항목 | 판정 | 한 줄 근거 |
|---|---|---|
| **§2 순서를 로컬에** | **맞다 — 단 저장소를 나눈다** | 로컬이 옳다. `ULocalPlayerSubsystem`은 **런타임 홀더로만** 쓰인다(엔진 용례 4건 전부). 지속은 **`ULocalPlayerSaveGame`** 이다(`LyraSettingsShared.h:57-64`) |
| §2-4 Lyra 해석 | **맞다. 반례가 아니다** | Lyra의 `Slots`는 우리 `SlotId`에 대응한다 — 9차에서 이미 같은 결론이 나왔다 |
| §2-2(a) "0곳" | **현재는 0곳이 맞다. 한 곳 정정** | `FindFungibleEntryId`는 **배열 순서**에 의존한다(표시 순서는 아니다) (§2-2) |
| §2-6-2 미래 소비자 | **하나 있다 — "모두 옮기기"** | 목적지가 부분적으로 차면 **무엇이 들어갔는가가 순서로 갈린다.** 유일한 실질 위험 (§2-3) |
| **★ 새 결함 1** | **`SwapEntries`의 용량식이 슬롯 항목에서 틀린다** | 본체 10/10 + 핫바의 AK(4) ↔ 본체 구급상자(3) → **11/10을 통과시킨다** (§3) |
| **★ 새 결함 2** | **드롭 핸들러의 `Hit`이 두 의미로 쓰인다** | 같은 줄에서 한 번은 **표시 인덱스**, 한 번은 **`EntryId`** (§4-1) |
| **★ 새 결함 3** | **같은 컨테이너의 빈 영역에 놓으면 서버로 간다** | 자기 컨테이너에 `Server_MoveEntry` → `CanFit`이 **자기 크기를 두 번 센다** → 가방이 찰수록 "맨 뒤로 보내기"가 거절된다 (§4-2) |
| §3 `SwapEntries` | **C가 맞다. Step 04도 맞다** | 9차의 `MoveEntry` 판정과 같은 논리. B는 호출부 전부에 기본값을 흘린다 |
| §3-4-3 검사 5 | **안전하다** | 장착 교체는 한쪽 `SlotId`가 비지 않아 통과한다. **단 §3의 용량식이 바로 그 경로에서 틀린다** |
| §3-4-4 원자성 | **원자적이다 — 확인함** | 변경 원소 전부가 **한 프로퍼티 페이로드**에 실리고, 분할된 번치는 **`bPartialFinal`까지 모아서** 올린다(`DataChannel.cpp:867-911`) |
| §5 드롭 판정 | **`HitTestCell`을 만들지 않는다** | Slate의 `OnDrop`은 **버블 라우팅**이다(`SlateApplication.cpp:5523`). 히트 테스트를 직접 짜는 것은 엔진이 이미 하는 일의 재구현 |
| §4 격자 UX | **대체로 맞다. 한 가지를 바꾼다** | **빈 칸을 N개 그리는 것 자체가 거짓말이다.** 개수를 포기했으면 칸도 포기한다 (§7-2) |
| §5-2-3 재생성 비용 | **`FUserWidgetPool`을 쓴다** | 엔진이 *"dynamic entries"* 를 위해 만든 것이고 `UDynamicEntryBox`가 그 완성품이다(`UserWidgetPool.h:14`) |
| §6 범위 | **쪼갠다. 04-A / 04-B가 맞다** | 완료 조건 13개는 8차가 03을 쪼갠 바로 그 숫자다 |
| §7 파급 | **3건 누락** | `LOOT_STATUS.md:25` / `DOCS/DOCS.md:43` / `GAME.md:157` (§10) |

**한 줄 결론:** §2·§3·§4의 판단은 셋 다 유지된다. **바꿔야 할 것은 저장소 하나(`ULocalPlayerSaveGame`)와, 04-7·04-8 코드에 실재하는 결함 셋이다.**

---

## 1. 전제 — 9차 반영 확인

**반영 상태를 대조했다. 9차 §11의 13개 중 문서 항목이 전부 들어갔다.** 확정표 4행(`LOOT_STATUS.md:76` 외), `GetEntryInSlot(Parent, SlotId)`, `BodySlots`, `SlotPriority`, `MoveEntry`/`Server_MoveEntry` 분리, `HotbarRefs` 연기, 03-A/B/C 재조정, `BACKLOG.md` B-5, `StudyPath.md:926`, `Build.cs` 서술 정정까지 확인했다.

**§1-4의 두 가지를 판정한다.**

### 1-1. 함정 3b를 미정으로 옮긴 것 — **맞다. 9차 판정을 정정한다**

9차는 3b를 *"남는다 — write-back 소실은 번호가 아니라 값의 문제"* 로 판정했다. **그 판정이 틀렸다.**

3b의 전제는 *"배낭 속 무기를 **장착한 채**"* 다. 새 설계에서 장착은 `SlotId == "HotbarN"` **＋** `ParentEntryId == INDEX_NONE`이고, `MoveEntry`의 `BodySlots` 정합 검사(9차 §3-2)가 **`ParentEntryId != INDEX_NONE`인 장비 슬롯을 거절한다.** 배낭 속에 있으면서 장착된 엔트리는 **표현 자체가 불가능**하다.

9차가 3b를 "값의 문제"라고 부른 것은 맞지만, **그 값에 도달하는 상태가 사라진 것을 보지 못했다.** 미정 #7로 옮기고 *"핫바 5~0이 컨테이너 안 아이템을 들 수 있게 되면 되살아난다"* 로 조건을 붙인 것이 정확한 처리다.

### 1-2. 함정 4k(write-back 순서 계약의 성격 변화) — **맞다. 그리고 더 나쁘다**

지적이 정확하다. 그리고 한 겹 더 있다.

```
필드였을 때   순서 위반 → EquippedEntryId가 INDEX_NONE인 채 write-back → 잔탄이 0으로 덮인다
파생 게터     순서 위반 → GetEquippedEntryId()가 INDEX_NONE → if 분기가 거짓 → write-back이 아예 안 불린다
```

**증상이 "덮인다"에서 "안 불린다"로 바뀌면 재현이 어려워진다.** 덮이는 쪽은 값이 0이라 눈에 띄지만, 안 불리는 쪽은 **버리기 직전 값이 그대로 픽업에 실려** 대부분의 경우 정답과 구분되지 않는다. 틀린 값이 보이는 것은 *발사 후 write-back이 밀린 그 한 발*뿐이다.

> **그래서 4k는 함정표가 아니라 `RemoveEntryInternal`의 코드 주석에도 있어야 한다.** ①이 ③보다 앞이라는 것은 지금 문서에만 있고, 순서를 바꿔도 **컴파일된다.**

---

## 2. ★ 최대 주제 — 순서를 로컬에 두는 것. **맞다. 단 저장소가 틀렸다**

### 2-1. §2-4의 해석 — 맞다. Lyra는 반례가 아니다

이 대비는 9차에서 이미 결론이 나왔다. `ULyraQuickBarComponent::Slots`는 **무엇이 장착되는가**를 정하고(`GetActiveSlotItem()` → `EquipItemInSlot()`), 그건 우리 `SlotId`에 대응한다. **우리 `SlotId`는 이미 서버 권위이고 이미 복제된다.**

요청서 §2-4 표의 세 줄이 그대로 맞다. 한 줄을 덧붙인다.

**Lyra에는 "어느 칸에 그릴까"라는 축이 아예 없다.** `NumSlots = 3`이고 인덱스가 곧 배치다(`LyraQuickBarComponent.h:66`). 축이 없는 코드베이스는 그 축에 대한 선례를 줄 수 없다. **§2-3의 세 불릿(재접속 소실 / 자동 정렬 / 상용 게임)은 Lyra 인용이 아니라 실무 논거이고, 그 셋은 §2-4가 아니라 §2-6-4에서 다뤄야 한다** — 그리고 §2-4가 답한다: **저장소를 바꾸면 셋 다 대부분 해소된다**(§2-4).

### 2-2. §2-2(a) "서버 로직 중 순서를 보는 곳 0곳" — 현재는 맞다. 한 곳 정정

표의 다섯 항목을 전부 대조했다. **표시 순서를 보는 곳은 0곳이 맞다.** 다만 한 줄의 서술이 정확하지 않다.

| 서버가 하는 일 | 요청서 | 정정 |
|---|---|---|
| `FindFungibleEntryId` | *"✗ 첫 번째를 찾는다"* | **배열 순서에 의존한다.** 다만 그 순서는 **서버 `Entries.Items`의 물리적 순서**지 표시 순서가 아니다 |

**결과는 같지만 이유가 다르고, 그 차이가 중요하다.** `bFungible` 병합 대상이 여럿일 때 어느 것에 합쳐질지는 배열 순서가 정한다. 그 배열은 `RemoveSelf`가 `RemoveAtSwap`을 쓰면 순서가 뒤섞이고, 수신 측에서도 순서가 보장되지 않는다(`FastArraySerializer.h:54`). **현금뭉치 둘이 서로 구분 불가능하므로 무해하다** — 이것이 무해한 진짜 이유이고, *"첫 번째를 찾으니까 순서와 무관하다"* 가 아니다.

> **그래서 §2-2(a)의 결론은 유지되되 근거가 하나 좁아진다.** *"서버는 순서를 안 본다"* 가 아니라 **"서버가 보는 순서는 표시 순서와 다른 축이고, 그 축에서는 원소가 교환 가능하다"** 가 정확하다. 교환 불가능한 원소가 생기면(예: `bFungible`인데 `Durability`가 다른 아이템) 이 문장이 깨진다.

### 2-3. §2-6-2 미래 소비자 — **하나 있다. "모두 옮기기"**

전수로 훑은 결과 실질 위험은 하나다.

**quick-transfer(Shift+클릭 "전부 옮기기")** 는 표준 인벤토리 기능이고, `05_Loot_04_InventoryUI.md`의 드래그가 있으면 다음에 요청될 가능성이 높다. 그리고 이것은 **순서를 서버가 봐야 하는 유일한 연산**이다.

```
배낭(20/20 가득) → 외투(빈 칸 7)로 "전부 옮기기"
  → 7칸어치만 들어간다
  → 무엇이 들어갔는가?  플레이어는 "위에서부터"를 기대한다
```

**서버가 순서를 모르면 "위에서부터"를 지킬 수 없다.** 그리고 이건 UI가 대신 할 수도 있다 — 클라가 자기 순서대로 `Server_MoveEntry`를 N번 부르면 된다. **N번 RPC지만 사람이 한 번 누르는 동작이라 문제가 되지 않는다.**

> **판정: 위험이 실재하지만 로컬 순서를 뒤집지 않는다.** 해법이 "클라가 순서대로 N번 호출"이고, 그건 §2-1이 이미 채택한 구조(*"다른 드래그는 전부 서버를 탄다"*)의 반복이다. **문서에 이름을 적어두면 된다** — 안 적어두면 그때 `SortIndex`를 서버에 넣는 판단이 다시 올라온다.

**나머지 후보는 위험이 아니다.**

| 후보 | 판정 |
|---|---|
| *"가방에서 자동으로 꺼낸다"*(자동 재장전 등) | 원소가 교환 가능하다(탄약상자 아무거나). §2-2와 같은 이유로 무해 |
| 사망 시 전부 떨어뜨리기 | 순서가 의미 없다 |
| 자동 정렬 | **클라 연산이다.** 로컬 순서 위에 얹힌다 — §6의 판단이 맞다 |
| 탈출 정산 UI | 표시일 뿐 |

### 2-4. ★ §2-6-4 — `ULocalPlayerSubsystem`이 아니다. **`ULocalPlayerSaveGame`이다**

**`ULocalPlayerSubsystem`은 자리가 맞다. 그런데 그 자리에 두는 것은 "지속되는 값"이 아니다.**

```
$ grep -rln "ULocalPlayerSubsystem" LyraStarterGame/Source/                           →  0건
$ grep -rn  "public ULocalPlayerSubsystem" Engine/Source/Runtime Engine/Source/Editor  →  0건
$ grep -rln "public ULocalPlayerSubsystem" Engine/Plugins/                            →  4건
     EnhancedInput/…/EnhancedInputSubsystems.h
     CommonUI/…/CommonInputSubsystem.h
     CommonUI/…/CommonUIVisibilitySubsystem.h
     CommonUI/…/Input/CommonUIActionRouterBase.h
```

**용례는 있다 — 전부 플러그인이고, 전부 "지금 이 순간의 런타임 상태"다.** `UCommonInputSubsystem`(`CommonInputSubsystem.h:23-24`)은 **현재 입력 방식**을, `UCommonUIVisibilitySubsystem`(`CommonUIVisibilitySubsystem.h:22`)은 *"the visibility tags currently in play … **These can change over time**"*(`:40-43`)를 든다. **저장되는 값을 든 것은 하나도 없다.**

**그리고 저장되는 값의 자리는 Lyra가 따로 갖고 있다.**

**Lyra가 "플레이어별 UI 상태"를 두는 곳은 명확하다.**

```cpp
// LyraSettingsShared.h:57-64
/**
 * ULyraSettingsShared - The "Shared" settings are stored as part of the USaveGame system, these settings are not machine
 * specific like the local settings, and are safe to store in the cloud - and 'share' them.  Using the save game system
 * we can also store settings per player, so things like controller keybind preferences should go here, because if those
 * are stored in the local settings all users would get them.
 */
UCLASS()
class ULyraSettingsShared : public ULocalPlayerSaveGame
```

그리고 기기 종속 설정은 따로 간다 — `ULyraSettingsLocal : UGameUserSettings`(`LyraSettingsLocal.h:36`). **Lyra는 이 축을 둘로 갈랐고, "플레이어별 · 기기 무관 · 클라우드 가능"이 정확히 우리 인벤토리 순서의 성격이다.**

`ULocalPlayerSaveGame`은 엔진이 이 용도로 만든 클래스다:

```
// SaveGame.h:40-45
Abstract subclass of USaveGame that provides utility functions that let you associate a Save Game object
with a specific local player.
```

**권장 형태 — 둘을 쓴다. 겹치지 않는다.**

```cpp
// 지속 — 무엇을 저장하는가
UCLASS()
class UEPInventoryLayoutSave : public ULocalPlayerSaveGame
{
    UPROPERTY() TMap<int32, FEPContainerOrder> Order;   // TMap<int32, TArray<int32>>는 USTRUCT 래핑 필요
    virtual int32 GetLatestDataVersion() const override { return 1; }
};

// 런타임 — 누가 들고 있는가 (수명이 LocalPlayer라 맞다)
UCLASS()
class UEPInventoryLayout : public ULocalPlayerSubsystem
{
    UPROPERTY() TObjectPtr<UEPInventoryLayoutSave> Save;   // LoadOrCreateSaveGameForLocalPlayer
    TArray<int32> Resolve(...) const;
    bool          MoveTo (...);
};
```

`ULyraSettingsShared::LoadOrCreateSettings`(`.cpp:62-70`)가 `LoadOrCreateSaveGameForLocalPlayer(클래스, LocalPlayer, 슬롯이름)` 한 줄이다. **`CreateTemporarySettings`(`.cpp:52-60`)가 로그인 전 임시본을 만드는 패턴까지 그대로 쓸 수 있다** — 우리는 매치 중 접속이라 로그인 타이밍 문제가 Lyra보다 단순하다.

> **★ 이게 §2-5의 첫 두 줄을 지운다.** *"재접속·사망 시 순서 소실 — 받아들인다"* 와 *"다른 기기에서 순서 다름 — 받아들인다"* 를 **받아들일 필요가 없다.** `ULocalPlayerSaveGame`은 저장이고, Lyra의 주석이 *"safe to store in the cloud"* 라고 직접 말한다. 대가를 안 치르고 이득만 남는다.

> **`TMap<int32, TArray<int32>>`는 `UPROPERTY`로 직렬화되지 않는다** — 중첩 컨테이너다. `USTRUCT { TArray<int32> Ids; }`로 한 겹 싸야 한다. 문서에 안 적으면 구현에서 걸린다.

### 2-5. §2-6-3 나중에 서버로 옮기는 비용 — **낙관적이지 않다. 단 조건이 하나 있다**

§2-5 마지막 줄(*"서버는 불투명한 `TArray<int32>`로 저장만 하지 해석하지 않는다"*)은 맞다. **그러나 그 형태로 옮기면 §2-3의 세 불릿 중 무엇도 해결되지 않는다** — 서버가 해석하지 않으면 quick-transfer도 못 하고, 그러면 옮길 이유가 없다.

**즉 "나중에 서버로"는 두 가지 다른 일이다.**

| | 무엇 | 비용 |
|---|---|---|
| ⓐ **보관만** 서버로 (계정 이전용) | 불투명 `TArray<int32>` 복제 | 싸다. 그런데 **`ULocalPlayerSaveGame`이 이미 해준다** (§2-4) |
| ⓑ **서버가 순서를 **읽는다** (quick-transfer 등) | 순서가 게임 상태가 된다 | **비싸다.** 검증·복제 조건·`RemoveSelf` 유지 코드가 전부 생긴다 |

**ⓐ는 필요 없어지고 ⓑ는 §2-3이 답한 대로 클라가 N번 호출로 대신한다.** 그래서 "나중에 옮긴다"는 시나리오 자체가 거의 사라진다 — **이것이 로컬 선택의 진짜 강점이고, §2-5는 그걸 방어적으로만 적었다.**

### 2-6. §2-2(d)의 논거는 유지된다 — 그리고 9차와 대비가 정확하다

*"`Resolve`가 매번 대조하므로 죽은 `EntryId`가 남아도 무해하다"* 는 9차가 `HotbarRefs`에 **청소를 요구한 것과 대비된다.** 차이가 어디서 오는지 적어두는 것이 좋다.

```
HotbarRefs   서버가 읽는다 (Step 05의 장착)  →  죽은 번호가 게임 상태를 틀리게 한다  →  청소 필요
Order        클라만 읽고 매 갱신 재계산       →  죽은 번호가 다음 Resolve에서 사라진다  →  청소 불필요
```

**"청소가 필요한가"는 자료구조가 아니라 소비자가 정한다.** 이 문장이 두 판정을 하나로 묶는다.

---

## 3. ★ 새 결함 1 — `SwapEntries`의 용량식이 슬롯 항목에서 틀린다

**04-7의 이 두 줄이 용량을 초과시킨다.**

```cpp
const int32 UsedPA = GetUsedSlots(PA) - SizeA + SizeB;
const int32 UsedPB = GetUsedSlots(PB) - SizeB + SizeA;
```

### 3-1. 반례 — 본체가 11/10이 된다

```
본체 MaxSlots 10, 현재 10/10 (전부 SlotId == None)
AK(SlotSize 4)      ParentEntryId = -1,  SlotId = "Hotbar1"   ← 칸을 안 먹는다
구급상자(SlotSize 3) ParentEntryId = -1,  SlotId = None        ← 칸을 먹는다

구급상자를 AK 위로 드래그  →  Server_SwapEntries(구급상자, AK)
```

검사 5(*"같은 부모 ＋ 둘 다 `SlotId == None`"*)는 AK가 `"Hotbar1"`이라 **통과한다.** 그다음:

| | 값 |
|---|---|
| 식이 계산한 것 | `10 - 4 + 3 = 9` ≤ 10 → **통과** |
| **실제** | 구급상자(3)가 슬롯으로 빠지고 AK(4)가 컨테이너로 들어온다 → `10 - 3 + 4 = 11` |

**`SizeA`(=4)를 빼는데, AK는 애초에 `GetUsedSlots`에 들어 있지 않았다.** `GetUsedSlots`가 `if (!E.SlotId.IsNone()) continue;`로 슬롯에 든 것을 건너뛰기 때문이다.

**결과: 본체가 11/10이 되고, 그다음 `CanFit`은 전부 거절하며, 플레이어는 아이템을 넣을 수도 뺄 수도 없는 상태를 스스로 만들 수 있다.**

### 3-2. 올바른 식 — 크기 항은 **그쪽이 칸을 먹을 때만** 붙는다

교환은 `(ParentEntryId, SlotId)`를 통째로 맞바꾼다. 그래서 **B는 A의 `SlotId`를 물려받는다.** 즉 A의 자리가 칸을 먹는 자리였다면 A가 빠지고 B가 들어오며, 슬롯이었다면 둘 다 칸과 무관하다. **두 항의 조건이 같다.**

```cpp
// A의 옛 부모 PA — A의 SlotId가 None일 때만 칸 회계가 움직인다
const int32 DeltaPA = A.SlotId.IsNone() ? (SizeB - SizeA) : 0;
const int32 DeltaPB = B.SlotId.IsNone() ? (SizeA - SizeB) : 0;
```

**반례 재검증:** A=구급상자(`SlotId==None`) → `DeltaPA = 4 - 3 = +1`. B=AK(`"Hotbar1"`) → `DeltaPB = 0`. 본체 `10 + 1 = 11 > 10` → **거절.** 맞다.

### 3-3. ★ 그리고 `PA == PB`일 때 두 수를 따로 내면 안 된다

현행 식은 `UsedPA`와 `UsedPB`를 **독립된 두 수**로 만들어 `||`로 본다. `PA == PB`면 같은 컨테이너에 대해 서로 다른 두 답이 나온다(반례에서 `9`와 `11`). 지금은 `||` 덕에 우연히 안전한 쪽으로 기울지만, **§3-2의 델타 형태로 고치면 반드시 합산해야 한다.**

```cpp
if (PA == PB)
{
    if (GetUsedSlots(PA) + DeltaPA + DeltaPB > GetCapacity(PA)) return false;
}
else
{
    if (GetUsedSlots(PA) + DeltaPA > GetCapacity(PA)) return false;
    if (GetUsedSlots(PB) + DeltaPB > GetCapacity(PB)) return false;
}
```

> **`PA == PB`는 드문 경우가 아니다.** 장착 교체가 전부 여기다 — 핫바에 든 무기와 본체의 아이템은 **둘 다 `ParentEntryId == INDEX_NONE`** 이다(9차 §3-2의 정합 규칙이 그렇게 강제한다). **§3-4-3이 "부모가 둘 다 `INDEX_NONE`이라 같은 부모로 잡힌다"고 걱정한 바로 그 경로가, 검사 5는 통과하지만 용량식에서 틀린다.**

> **함정표에 넣을 것:** *"교환의 용량 판정에 `SlotSize`를 무조건 더하고 뺀다 → 슬롯에 든 쪽이 이중 계산돼 용량을 넘긴다. 증상은 '가방이 11/10이고 아무것도 안 들어간다'"*

---

## 4. ★ 새 결함 2·3 — 04-7 드롭 핸들러

```cpp
const int32 Hit = HitTestCell(Geo, Ev);              // 빈 칸이면 INDEX_NONE

if (P->SourceContainer == ContainerId && Hit != INDEX_NONE)
    return GetLayout()->MoveTo(Inventory, ContainerId, P->EntryId, Hit);   // ← Hit = 표시 인덱스

if (Hit == INDEX_NONE) Inventory->Server_MoveEntry(P->EntryId, ContainerId, NAME_None);
else                   Inventory->Server_SwapEntries(P->EntryId, Hit);     // ← Hit = EntryId
```

### 4-1. `Hit`이 한 함수 안에서 두 의미로 쓰인다

`MoveTo`의 시그니처는 `MoveTo(Inv, Container, EntryId, int32 **NewIndex**)`(04-8/§2-1)이고, `Server_SwapEntries(int32 A, int32 B)`의 둘째 인자는 **`EntryId`** 다(04-7). **같은 변수가 한 줄 건너 다른 것을 뜻한다.** 둘 중 하나는 반드시 틀린다.

**둘 다 컴파일된다** — 양쪽 다 `int32`다. 이게 이 결함의 성격이다.

**해법은 반환을 갈라 이름을 붙이는 것이다.**

```cpp
struct FEPCellHit
{
    int32 DisplayIndex = INDEX_NONE;   // 격자에서 몇 번째 자리인가 (빈 자리 포함)
    int32 EntryId      = INDEX_NONE;   // 그 자리에 아이템이 있으면 그 번호
};
```

`DisplayIndex`는 "어디에 놓았나"이고 `EntryId`는 "무엇 위에 놓았나"다. **둘은 독립이다** — 빈 자리에 놓으면 `DisplayIndex`만 유효하다.

### 4-2. ★ 같은 컨테이너의 빈 영역에 놓으면 서버로 가고, 거기서 거절된다

`SourceContainer == ContainerId`인데 `Hit == INDEX_NONE`(빈 영역)이면 첫 조건이 거짓이 되어 **아래로 흘러 `Server_MoveEntry(EntryId, ContainerId, NAME_None)`을 부른다.** 자기가 이미 들어 있는 컨테이너로 옮기라는 요청이다.

**그리고 `MoveEntry`가 이걸 거절한다.** 9차가 확정한 검사 목록에 `NewSlotId == None`이면 `CanFit(NewParent, ItemId)`가 있고, `CanFit`은 `GetUsedSlots(NewParent) + SlotSize <= Capacity`다. **엔트리가 이미 그 컨테이너 안에 있으므로 자기 크기가 두 번 세어진다.**

```
배낭 18/20에 든 AK(4)를 배낭의 빈 영역으로 드래그  →  18 + 4 = 22 > 20  →  거절
```

**증상:** *"가방이 좀 차면 아이템을 맨 뒤로 못 보낸다."* 그리고 **가방이 널널할 때는 된다** — 서버 왕복만 낭비하면서. 재현 조건이 "용량"이라 UI 버그로 보이지 않는다.

**해법은 조건을 뒤집는 것이다. 같은 컨테이너면 무조건 로컬이다.**

```cpp
if (P->SourceContainer == ContainerId)
    return GetLayout()->MoveTo(Inventory, ContainerId, P->EntryId,
                               Hit.DisplayIndex);   // 빈 영역이면 맨 뒤 (MoveTo가 클램프)

if (Hit.EntryId == INDEX_NONE) Inventory->Server_MoveEntry(P->EntryId, ContainerId, NAME_None);
else                           Inventory->Server_SwapEntries(P->EntryId, Hit.EntryId);
return true;
```

**"같은 컨테이너 안에서는 서버가 볼 것이 없다"** 가 §2의 전제이므로, 이 형태가 §2와도 일관된다. 지금 코드는 **§2가 세운 규칙을 자기 구현에서 어긴다.**

> **`MoveTo`의 계약에 클램프를 명시한다** — `NewIndex == INDEX_NONE`이거나 범위를 넘으면 맨 뒤. 안 적으면 호출부마다 다르게 처리한다.

---

## 5. §3 `SwapEntries` 판정

### 5-1. §3-4-1 C가 맞다 — 9차 논리가 그대로 적용된다

**CLAUDE.md §2의 *"두 번째 구현자가 없는 인터페이스"* 는 여기도 안 걸린다.** 9차와 같은 이유다 — 인터페이스도 베이스 클래스도 아닌 단일 클래스의 단일 함수라 다형성이 없다.

**B(기본 인자)를 거부하는 근거는 요청서보다 강하다.**

`MoveEntry(EntryId, NewParent, NewSlotId, DisplacedEntryId = INDEX_NONE)`로 가면 **§3-2의 용량식이 함수 안에서 갈린다.** `Displaced`가 없으면 `Used + Size`, 있으면 §3-2의 델타식이다. **한 함수가 두 개의 용량 규칙을 갖게 되고, 그중 하나는 호출부 전부가 지나가면서 절대 안 쓴다.** 이건 인자 개수 문제가 아니라 **불변식이 둘인 함수**를 만드는 문제다.

`SwapEntries`는 *"둘의 자리를 맞바꾼다"* 라는 다른 계약이고, 그래서 다른 용량 규칙을 갖는 것이 자연스럽다. **C.**

### 5-2. §3-4-2 Step 04에 두는 것 — 맞다. 9차 기준과 일관된다

9차가 `MoveEntry`를 03-A로 보낸 이유는 **03-B의 `Server_EquipBackpack`이 그것의 래퍼**여서였다. `SwapEntries`에는 Step 03 호출자가 없다. **판정 기준이 "다음 단계에 소비자가 있는가"가 아니라 "이번 단계에 소비자가 있는가"이므로 일관된다.**

`Server_MoveEntry`를 여기서 여는 것도 9차 §4-2 그대로다 — *"검증 표면은 소비자를 따라간다."*

### 5-3. §3-4-3 검사 5의 판정식 — **안전하다.** 단 §3이 그 경로에서 터진다

걱정한 시나리오를 정리하면 이렇다.

| 상황 | A의 `SlotId` | B의 `SlotId` | 부모 | 검사 5 |
|---|---|---|---|---|
| 순수 표시 순서 (본체 안 두 아이템) | None | None | 둘 다 `-1` | **거절** ✅ |
| 장착 교체 (핫바 무기 ↔ 본체 아이템) | `"Hotbar1"` | None | 둘 다 `-1` | 통과 ✅ |
| 부착물 교체 (두 무기의 조준경) | `"Optic"` | `"Optic"` | 다름 | 통과 ✅ |

**"둘 다 `SlotId == None`" 이 정확한 판정식이다.** 부모만 봤으면 장착 교체가 거절됐을 것이고, `SlotId`만 봤으면 서로 다른 컨테이너의 두 아이템(둘 다 None)이 잘못 거절됐을 것이다. **둘을 AND로 묶은 것이 맞다.**

> **다만 검사 5는 "부모가 같을 때"만 본다.** 서로 다른 컨테이너의 두 일반 아이템(둘 다 `SlotId == None`, 부모 다름)은 통과하고 서버를 탄다 — **맞다.** 그건 표시 순서가 아니라 실제 이동이다.

> **★ 그리고 통과한 그 경로가 §3의 결함을 만난다.** 장착 교체는 검사 5를 통과해 §3-1의 용량식에 도달한다. **검사 5가 옳게 통과시킨 것을 용량식이 잘못 계산한다** — 두 절을 같이 읽어야 하는 이유다.

### 5-4. §3-4-4 원자성 — **원자적이다. 확인함**

**결론: 같은 프레임에 `MarkItemDirty`한 원소 둘은 반드시 같은 수신에 함께 도착한다.** 근거가 셋이다.

**① 변경 원소 전부가 하나의 헤더와 하나의 페이로드로 나간다.**

```cpp
// FastArraySerializer.h:985-1001 (WriteDeltaHeader)
Writer << Header.BaseReplicationKey;
Writer << NumDeletes;
Writer << Header.NumChanged;          // ★ 이번에 바뀐 것 전부의 개수
```

`NumChanged`가 상한(`MaxNumberOfAllowedChangesPerUpdate = 2048`, `FastArraySerializer.cpp:10`)을 넘어도 **경고 로그만 찍고 전부 쓴다**(`UE_CLOG`, `:1000-1001`). **일부만 보내는 경로가 없다.**

**② 그 페이로드는 `FEPInventoryList` 프로퍼티 하나의 커스텀 델타 쓰기다.** `FRepLayout::SendCustomDeltaProperty`가 프로퍼티 단위로 `NetDeltaSerialize`를 부르므로(`RepLayout.cpp:4582-4583`), 두 원소가 서로 다른 번치로 갈라질 방법이 없다 — **애초에 하나의 비트 스트림이다.**

**③ 그 번치가 커서 분할돼도 수신 측이 다시 모은다.**

```cpp
// DataChannel.cpp:867-911
if ( InPartialBunch && !InPartialBunch->bPartialFinal && bSequenceMatches && ... )
{
    ...
    if (Bunch.bPartialFinal) { InPartialBunch->bPartialFinal = true; }
}
```

**`bPartialFinal`이 올 때까지 `InPartialBunch`에 모아두고, 완성된 뒤에야 위로 올린다.** 반쪽짜리 프로퍼티가 `NetDeltaSerialize`에 들어가는 경로가 없다.

> **따라서 `PostReplicatedReceive`도 교환 후 상태에서 한 번만 불린다.** 04-7이 *"필드 넷만 고치고 `MarkItemDirty` 두 번"* 이라고 적은 것이 그대로 안전하다. **한 프레임 이상한 상태는 보이지 않는다.**

> **★ 단 조건이 하나다 — 두 `MarkItemDirty`가 같은 서버 프레임 안이어야 한다.** `SwapEntries`가 통째로 서버 함수 하나라 자동으로 만족된다. **검사와 쓰기를 갈라 두 프레임에 걸치면 이 보장이 사라진다** — §3의 *"전부 통과한 뒤에 쓴다"* 가 원자성까지 지키고 있다는 것을 문서에 적어두는 게 좋다.

---

## 6. §5 드롭 판정 — `HitTestCell`을 만들지 않는다

### 6-1. §5-2-1·2 — Slate가 이미 히트 테스트를 하고, 결과를 **버블링**한다

```cpp
// SlateApplication.cpp:5523-5533
Reply = FEventRouter::Route<FReply>(this, FEventRouter::FBubblePolicy(LocalWidgetsUnderPointer), ...
{
    if (bIsDragDropping)
    {
        FDragDropEvent LocalDropEvent(Event, LocalDragDropContent);
        const FReply TempDropReply = CurWidget.Widget->OnDrop(CurWidget.Geometry, LocalDropEvent);
        return TempDropReply;
    }
    ...
```

**`FBubblePolicy`** — 커서 아래 **가장 깊은 위젯부터** 위로 올라가며 `OnDrop`을 부르고, **처음 `Handled()`를 반환한 곳에서 멈춘다.** `OnDragOver`도 같다(`:5799`).

**즉 `HitTestCell(Geo, Ev)`은 엔진이 이미 하는 일을 지오메트리 산술로 다시 짜는 것이다.** 그리고 그 산술은 패딩·스크롤 오프셋·DPI 스케일·`UniformGridPanel`의 `SlotPadding`을 전부 스스로 처리해야 한다 — 하나라도 틀리면 **가장자리에서만 어긋나는 버그**가 된다.

### 6-2. 04-1의 두 근거는 다른 방법으로 해소된다

요청서가 격자 수신을 고른 근거는 둘이다. **둘 다 "칸이 검증을 갖는다"를 전제하는데, 그럴 필요가 없다.**

| 근거 | 해소 |
|---|---|
| *"같은 검증이 N개 위젯에 흩어진다"* | **흩어지지 않는다.** 칸은 **자기 정체만 넘긴다** — 검증은 패널에 한 곳 |
| *"빈 칸에 떨어뜨린 경우를 아무도 안 받는다"* | **빈 칸도 위젯이다**(04-1이 이미 그렇게 적었다). 그리고 안 받으면 **버블링으로 패널이 받는다** |

```cpp
// 칸 — 한 줄. 검증 없음
bool UEPItemCellWidget::NativeOnDrop(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* Op)
{
    return OwnerPanel->HandleDrop(FEPCellHit{ DisplayIndex, EntryId }, Op);
}

// 패널 — 칸 사이 여백·바깥 여백에 놓은 경우. 버블링으로 여기 온다
bool UEPContainerPanel::NativeOnDrop(const FGeometry& G, const FDragDropEvent& E, UDragDropOperation* Op)
{
    return HandleDrop(FEPCellHit{ INDEX_NONE, INDEX_NONE }, Op);   // 맨 뒤로
}

// 검증·분기는 여기 한 곳 (§4-2의 수정된 형태)
bool UEPContainerPanel::HandleDrop(const FEPCellHit& Hit, UDragDropOperation* Op);
```

**`HandleDrop`이 유일한 판정 지점이라는 04-1의 목표는 그대로 지켜지고, 지오메트리 산술이 0줄이 된다.** 그리고 §4-1의 타입 혼동도 `FEPCellHit`이 구조적으로 막는다.

> **덤:** 칸 위젯이 `OnDrop`을 받으면 `NativeOnDragEnter`/`Leave`로 **그 칸만** 하이라이트하는 것이 공짜가 된다. 격자가 받으면 하이라이트 대상도 좌표로 다시 계산해야 한다.

### 6-3. §5-2-3 재생성 비용 — **엔진이 이 문제에 이름과 도구를 갖고 있다**

```
// UMG/Public/Blueprint/UserWidgetPool.h:14
Pools UUserWidget instances to minimize UObject and SWidget allocations for UMG elements with dynamic entries.
@see UListView
@see UDynamicEntryBox
```

**`FUserWidgetPool`이 정확히 이 문제를 위해 있다.** 그리고 완성품이 있다 — **`UDynamicEntryBox`**:

```
// UMG/Public/Components/DynamicEntryBoxBase.h:23-27
Base for widgets that support a dynamic number of auto-generated entries at both design- and run-time.
```

- 내부에 `FUserWidgetPool EntryWidgetPool`을 든다(`DynamicEntryBoxBase.h:179`)
- `Reset(bDeleteWidgets = false)` — **기본값이 파괴가 아니라 회수**다(`DynamicEntryBox.h:51`)
- `EDynamicBoxType`에 **`Wrap`** 이 있다(`DynamicEntryBoxBase.h:14-22`) — 고정 크기 정사각형 칸이면 그게 곧 격자다
- **UMG 코어다.** CommonUI 의존이 늘지 않는다

**판정: 구획 안의 칸 격자를 `UUniformGridPanel` + 수동 재생성 대신 `UDynamicEntryBox(Wrap)`으로 만든다.**

| | 현행 제안 | `UDynamicEntryBox` |
|---|---|---|
| 위젯 재사용 | 없음 (매번 `NewObject`) | **풀링** |
| 행·열 계산 | `AddChildToUniformGrid(w, Row, Col)` 수동 | 없음 (Wrap이 흘린다) |
| 창 크기 대응 | 열 수 고정 5 | 자동 |
| 칸 하이라이트 | 좌표 재계산 | 칸 위젯이 직접 |

> **`UUniformGridPanel`을 유지해도 된다** — 그때는 `FUserWidgetPool`을 직접 들고 `Reset` 대신 `GetOrCreateInstance`를 쓴다. **하지 않으면 안 되는 것은 "매 갱신 `NewObject` N개"** 다. 엔트리 30 × 구획 6 × 초당 수 회면 GC 압력이 실측 가능해진다.

### 6-4. §5-2-4 `NativeOnDragOver`에서 매 프레임 `GetUsedSlots` — **하지 않는다. 드래그 시작에 한 번 캔다**

`GetUsedSlots`는 **드래그 중에 바뀌지 않는다.** 서버 상태가 바뀌는 유일한 계기는 `PostReplicatedReceive`이고, 그건 알림을 쏜다(03-7).

```cpp
// NativeOnDragDetected에서 한 번
Payload->CachedUsed = Inventory->GetUsedSlots(ContainerId);   // 구획마다
// PostReplicatedReceive 알림이 오면 무효화
```

**§2-5의 O(N) 논의와 같은 결론이다** — 캐시가 필요한 게 아니라 **부르는 횟수가 틀렸다.** 9차가 12칸 UI에 대해 내린 판정(*"순회 한 번"*)과 같은 형태다.

---

## 7. §4 격자 UX 판정

**소스로 검증되지 않는 판단이라는 것을 인정하고 답한다.** 아래는 근거가 인용이 아니라 설계 일관성이다.

### 7-1. 방향은 맞다 — 자료구조를 지킨 것이 가장 큰 값이다

*"칸 하나 = 아이템 하나, 부피는 숫자"* 는 **`FEPItemData`에 2D 크기가 없다는 사실과 정합적인 유일한 형태**다. 04-0이 대안(2D 배치)의 비용을 *"Step 03 재작성 규모"* 로 정확히 산정했다. **여기서 되돌릴 것이 없다.**

### 7-2. ★ 한 가지를 바꾼다 — **빈 칸을 N개 그리는 것 자체가 거짓말이다**

§4-3-3이 *"빈 칸의 개수를 포기한 것이 옳은가"* 를 물었다. **포기한 것은 맞는데, 그림이 여전히 개수를 말하고 있다.**

```
┌─────┬─────┬─────┬─────┬─────┐
│ AK  │ 권총 │ 붕대 │     │     │      ← 빈 칸 2개가 "2개 더 들어간다"로 읽힌다
└─────┴─────┴─────┴─────┴─────┘        실제로는 붕대라면 13개, AK라면 3개
```

**"고정 5열 × 가변 행, 최소 2행"이면 빈 칸 개수가 레이아웃 부산물인데, 플레이어는 그걸 정보로 읽는다.** 분절 게이지를 옆에 둬도 두 장치가 서로 다른 숫자를 말한다 — §4-1이 지적한 바로 그 어긋남이 형태만 바꿔 남는다.

**대안: 빈 자리를 세지 말고 한 덩어리로 만든다.**

```
┌─────┬─────┬─────┬───────────────────┐
│ AK  │ 권총 │ 붕대 │   ＋ 남은 용량 13   │   ← 하나의 드롭 영역. 개수를 말하지 않는다
│   ④│   ②│   ①│                   │
└─────┴─────┴─────┴───────────────────┘
```

| | 효과 |
|---|---|
| **개수를 말하지 않는다** | 셀 수 있는 것이 없으니 오해할 것도 없다 |
| **드롭 대상이 명시적이다** | *"여기 놓으면 맨 뒤"* 가 그림으로 보인다 — §4-2의 드래그 아웃 존과 같은 원리 |
| **드래그 중 색이 여기 하나에만 붙는다** | 지금은 빈 칸 N개가 동시에 빨개진다 |
| **위젯 수가 준다** | §6-3의 재생성 비용이 아이템 수에만 비례한다 |

**"몇 개 더 들어가나"에 대한 답은 원래 없다** — 아이템마다 다르기 때문이다. 그러니 **답이 없는 질문을 그림이 유도하지 않게 하는 것**이 최선이고, 남은 용량 숫자가 진짜 답에 가장 가깝다.

### 7-3. §4-3-2 분절 게이지 — 충분하다. 하나를 덧댄다

분절 게이지는 *"어느 아이템이 얼마나 먹는가"* 를 답한다. **`④` 배지가 이미 같은 정보를 칸에서 답하고 있어 중복이지만, 중복이 나쁘지 않다** — 하나는 절대량(배지), 하나는 상대 비중(게이지)이다.

**덧댈 것 하나: 드래그 중 "넘치는 만큼 미리보기"를 게이지에만 두지 말고 숫자로도 보인다.** `7 / 20` → `7 → 11 / 20` (초록) 또는 `7 → 24 / 20` (빨강). **§4-1이 진단한 *"17칸 남았는데 왜 안 들어가지"* 를 정확히 그 순간에 답하는 것은 색이 아니라 숫자다.**

### 7-4. §4-3-1 고려하지 못한 대안 — 없다. 배제 근거가 각각 다르다

| 대안 | 배제 근거 |
|---|---|
| 칸 크기를 `SlotSize`에 비례 | 정사각형이 깨지고, 2열 차지하는 아이템이 생기면 **줄바꿈에서 2D 배치 문제가 그대로 재발**한다 |
| 실제 여러 칸 점유 | 2D. 사용자 확정으로 배제 |
| 목록 유지 | 기획이 격자를 확정했고, **착용 12칸이 목록으로 안 그려진다** |
| **부피순 자동 정렬 고정** | 순서를 로컬에 두는 §2와 충돌하지 않지만, *"직접 정리할 수 있게"* 라는 기획과 충돌 |

---

## 8. §6 범위 통제 — **쪼갠다. 04-A / 04-B가 맞다**

### 8-1. §6-1 — 쪼개야 한다

**완료 조건 13개는 8차가 Step 03을 셋으로 쪼갠 바로 그 숫자다.** 그리고 이번엔 근거가 하나 더 있다 — **04-A는 Step 03이 없어도 검증되지만 04-B는 `Server_MoveEntry`·`SwapEntries`가 필요하다.** 즉 **의존성이 실제로 다르다.**

| | 범위 | 완료 조건 | 검증 |
|---|---|---|---|
| **04-A 표시** | 04-0 · 04-1 · 04-2 · 04-3 · 04-4 · 04-6 | 1, 2, 3, 4, 5, 6, 13, 14 (8개) | Tab / 폴링 없는 갱신 / 구획 생성·소멸 / **칸 3개인데 `7/20`** |
| **04-B 드래그** | 04-5 · 04-7 · 04-8 | 7, 8, 9, 10, 11, 12 (6개) | 이동 / **교환** / 착용 / 넘침 피드백 / **로컬 순서** |

**경계선의 근거:** 04-A가 끝나는 지점은 **`Server_MoveEntry`가 처음 열리기 직전**이다. 8차·9차가 쓴 기준(*"검증 표면은 소비자를 따라간다"*)이 그대로 분할선이 된다.

> **★ 8차의 03-7 사고와 같은 것이 하나 있다 — 04-A가 칸을 `UUserWidget`으로 만들어야 한다.** 04-B의 드롭이 칸 위젯의 `NativeOnDrop`에 걸리기 때문이다(§6-2). 04-A에서 칸을 `UImage`+`UTextBlock` 조합으로 만들면 04-B에서 **전부 다시 만든다.** 04-2가 이미 `WBP_ItemCell`로 적었으므로 지금은 맞고, **분할할 때 이 이유를 적어둬야 04-A를 줄이려는 유혹에서 지켜진다.**

> **버리기(04-5)를 04-B에 두는 이유:** 우클릭 버리기만 보면 04-A여도 되지만, **드래그 아웃 존**이 04-5에 있고 그건 드래그다. 우클릭 경로만 04-A로 당기면 04-5가 두 구간에 걸린다 — **문서 절을 쪼개지 않는다**는 8차 원칙(경계에서 stale이 세 번 났다)을 지킨다.

### 8-2. §6-2 드래그를 더 뺄 수 있는가 — **뺄 수 없다. 판단이 맞다**

**착용 자체가 드래그다**(9차 기획). 드래그를 빼면 착용·해제 경로가 없어지고, 그러면 04-A의 완료 조건 4(*"상의를 입으면 구획이 뜬다"*)를 **검증할 수단이 없다.**

> **04-A는 `EP.Inv.Add` + `EP.Inv.Equip` 류 디버그 커맨드로 검증한다.** Step 01·03이 쓴 방식과 같다. **이걸 04-A 범위에 명시하지 않으면 04-A가 "눈으로만 확인"이 된다.**

### 8-3. §6-3 자동 정렬을 뺀 것 — **맞다. 순서도 맞다**

*"자동 정렬이 더 자주 쓰일 수 있다"* 는 사실일 수 있지만, **자동 정렬은 로컬 순서(04-8) 위에 얹히는 순수 추가분**이다 — `Order`를 다른 기준으로 채우는 함수 하나다. **드래그가 없으면 자동 정렬이 만들어낸 순서를 사람이 고칠 수 없고, 그러면 정렬 기준이 마음에 안 들 때 할 수 있는 게 없다.**

**의존 방향이 한쪽이므로 순서가 정해진다.** 04-8 → 자동 정렬.

---

## 9. 실무 조사 5건

### 9-1. UMG 인벤토리 드래그앤드롭의 관용구 — **엔진에도 없다. 그러나 라우팅 정책이 답이다**

**엔진 `Source/` 전체를 훑었다. 이 훅을 구현한 소스가 UMG 자신 말고는 하나도 없다.**

```
$ grep -rln "NativeOnDrop"        Engine/Source/ Engine/Plugins/
Source/Runtime/UMG/Public/Blueprint/UserWidget.h      ← 선언
Source/Runtime/UMG/Private/UserWidget.cpp             ← 기본 구현
Source/Runtime/UMG/Private/Slate/SObjectWidget.cpp    ← Slate 연결
(나머지는 전부 .dll / .obj — 에디터 전용 플러그인의 빌드 산출물)

$ grep -rln "NativeOnDragDetected" …                   ← 같은 셋
```

`CommonUI`도 마찬가지다 — `DragDrop` 문자열은 `CommonButtonBase`(버튼의 드래그 감지)에만 나오고 인벤토리류 위젯은 없다. Lyra에도 없다(요청서 §1-3, 확인함).

> **즉 "UMG 드래그앤드롭 인벤토리의 엔진 선례"는 존재하지 않는다.** 엔진은 훅 셋을 열어두고 구현은 게임에 맡긴다. **이 절의 답은 선례가 아니라 라우팅 규칙에서 나온다.**

**그래서 "이 게임은 이렇게 했다"는 선례는 못 준다. 대신 엔진이 정한 라우팅이 §6-1의 답이다** — `OnDrop`/`OnDragOver`가 `FBubblePolicy`로 깊은 곳부터 위로 올라간다(`SlateApplication.cpp:5523, 5799`). **UMG에서 "누가 드롭을 받는가"는 설계 선택이 아니라 위젯 트리가 정한다.** 격자가 좌표로 판정하는 것은 그 위에 두 번째 히트 테스트를 얹는 것이다.

### 9-2. `ULocalPlayerSubsystem`의 관용성 — **자리는 맞고, 지속은 `ULocalPlayerSaveGame`이다**

§2-4에 폈다. 요약:

- `grep -rln "ULocalPlayerSubsystem" LyraStarterGame/Source/` → **0건**
- `grep -rn "public ULocalPlayerSubsystem" Engine/Source/Runtime Engine/Source/Editor` → **0건**
- `grep -rln "public ULocalPlayerSubsystem" Engine/Plugins/` → **4건** — EnhancedInput / CommonInput / CommonUIVisibility / CommonUIActionRouter. **전부 런타임 상태이고 저장되는 값을 든 것은 없다** (`CommonUIVisibilitySubsystem.h:40-43` — *"These can change over time"*)
- Lyra의 플레이어별 UI 상태 = `ULyraSettingsShared : ULocalPlayerSaveGame`(`LyraSettingsShared.h:64`), 주석이 *"per player … safe to store in the cloud"*(`:57-60`)
- 기기 종속은 따로 — `ULyraSettingsLocal : UGameUserSettings`(`LyraSettingsLocal.h:36`)
- 로드는 한 줄 — `LoadOrCreateSaveGameForLocalPlayer(클래스, LocalPlayer, 슬롯)`(`LyraSettingsShared.cpp:65`)

### 9-3. ★ 원소 둘의 도착 원자성 — **함께 도착한다**

§5-4에 폈다. 근거 셋: `NumChanged`가 변경 전부를 한 헤더에 싣고 상한 초과도 경고뿐(`FastArraySerializer.h:985-1001`, `.cpp:10`), 커스텀 델타는 프로퍼티 단위 한 덩어리(`RepLayout.cpp:4582-4583`), 분할 번치는 `bPartialFinal`까지 모아서 올린다(`DataChannel.cpp:867-911`).

### 9-4. 위젯 매 갱신 재생성 — **엔진이 도구를 갖고 있다**

§6-3에 폈다. `FUserWidgetPool`(`UserWidgetPool.h:14`)과 그 완성품 `UDynamicEntryBox`(`DynamicEntryBoxBase.h:23-27`, 풀 보유 `:179`, `Reset` 기본값이 회수 `DynamicEntryBox.h:51`, `EDynamicBoxType::Wrap` `DynamicEntryBoxBase.h:14-22`).

**"경고"의 형태로 있는 것은 아니다** — 엔진은 대신 **대안을 제공하는 방식**으로 말한다. `UListView`도 `FUserWidgetPool`을 쓴다(`ListViewBase.h`). 8차가 `UListView`를 배제한 것은 여전히 맞지만(`UObject*` 고정), **그때 함께 배제된 것이 아이템 타입이지 풀링이 아니다.**

### 9-5. 스칼라 부피를 정사각형 격자로 — **소스 근거 없음. 판단으로 답한다**

§7에 폈다. **인용할 수 있는 것이 없어서 인용하지 않는다.** 제안한 변경(§7-2, 빈 칸을 한 덩어리로)의 근거는 선례가 아니라 *"그림이 답할 수 없는 질문을 유도하지 않는다"* 는 원칙이다.

---

## 10. §7-4 파급 — **3건 누락**

전 문서를 훑었다. 9차보다 훨씬 촘촘하다(9차는 7건이었다). **3건이 남았고, 셋 다 성격이 다르다.**

| # | 위치 | 무엇 | 위험도 |
|---|---|---|---|
| **1** | **`LOOT_STATUS.md:25`** | `- [ ] 05_Loot_04 InventoryUI **(아이템 목록 + 칸 수 게이지)**` — **옛 이름 그대로다.** 같은 파일 아래쪽에 이번에 추가한 확정표 5행이 정확히 반대를 말한다. **진실의 원천 문서 안에서 자기모순** | ★★ |
| **2** | **`DOCS/DOCS.md:43`** | `UI/HUD \| ... 인벤토리 UI (UMG, **최소한으로**)` — 로드맵의 **범위 단어**가 뒤집혔다. §6이 통제하려는 바로 그 축인데 최상위 문서가 아직 "최소한으로"다 | ★★ |
| **3** | **`GAME.md:157`** | *"**목록에서는** 전부 한 줄로 보이되 소비하는 칸 수가 다르다"* — **이번 라운드가 직접 반증한 문장이다.** 격자로 바뀌면서 "한 줄"이 "한 칸"이 된다. **9차가 미룬 `:158`·`:178-180`과 달리 이건 10차가 새로 깬 것** | ★★ |

> **1·3은 9차와 같은 종류의 누락이다** — 요청서 §7이 *"뒤집히는 결정"* 만 훑고 **설명 문장**을 안 봤다. 9차의 `StudyPath.md` 건과 같다.

> **2는 새로운 종류다.** 뒤집힌 것이 사실이 아니라 **범위 서술**이다. `DOCS.md`는 단계 문서가 아니라서 파급 조사의 시야에 잘 안 들어온다.

**참고 — 파급은 아니지만 눈에 띄는 것 둘**

| | 위치 | 내용 |
|---|---|---|
| a | `LOOT_STATUS.md:194-195` | 검수 이력표에 **9차 행이 8차 행보다 위에** 있다. 순서만 |
| b | `05_Loot_03_Inventory_STATUS.md:28,34` | `ContainerOrder`가 *"9차(2026-08-22)에서 추가된 항목"* 절에 들어 있는데 **10차 항목**이다 |

### 10-1. §7-3 `ContainerOrder`를 `GetInsertionOrder()`가 반환하는 것 — **맞다**

*"표시 순서와 삽입 순서가 지금은 같고, 갈리면 함수라 쪼개는 비용이 0"* 이 정확하다. **한 줄 덧붙일 것:** `GetInsertionOrder()`는 배열 앞에 `INDEX_NONE`(본체)을 붙여야 한다 — `ContainerOrder`는 착용 슬롯 이름 5개뿐이고 본체는 슬롯이 아니다. **9차 §7-1이 `[INDEX_NONE, Back]`으로 적었던 것이 이번에 `["Coat","Torso",...]`로 바뀌면서 본체가 빠졌다.** 안 붙이면 **본체 10칸에 아무것도 안 들어간다** — Step 03 완료 조건 2~6이 전부 본체 위에 서 있으므로 03-C에서 즉시 걸린다.

---

## 11. 권장 작업 순서

**아래는 제안이다. 적용 여부는 사용자가 결정한다.**

| # | 작업 | 대상 | 왜 이 순서인가 |
|---|---|---|---|
| **1** | **★ `SwapEntries` 용량식 교정** — `SlotId.IsNone()` 조건 + `PA == PB` 합산 (§3-2·3-3) | 04-7 | **문서에 틀린 식이 박힌 채로 구현되면 용량 초과 상태를 플레이어가 만든다.** 재현 조건이 "가방이 꽉 참"이라 QA에서 늦게 나온다 |
| **2** | **★ 드롭 핸들러 재작성** — `FEPCellHit` + *"같은 컨테이너면 무조건 로컬"* (§4) | 04-7 | 결함 둘이 한 함수에 있다. `Hit` 타입 혼동은 **컴파일된다** |
| **3** | **★ `UEPInventoryLayout`의 지속을 `ULocalPlayerSaveGame`으로** (§2-4) | 04-8, `LOOT_STATUS.md` 확정표 | §2-5가 받아들이기로 한 대가 둘이 **안 치러도 되는 것**이다. Lyra 선례가 있다 |
| **4** | **드롭을 칸이 받게** — `HitTestCell` 삭제 (§6-1·6-2) | 04-1, 04-7 | 지오메트리 산술 0줄. 2번 수정과 같은 함수라 **함께 한다** |
| **5** | **Step 04를 04-A / 04-B로 분할** (§8-1) | `05_Loot_04_InventoryUI.md`, `LOOT_STATUS.md` | 완료 조건 13개. **04-A가 칸을 `UUserWidget`으로 만들어야 하는 이유를 함께 적는다** |
| **6** | **`FUserWidgetPool` / `UDynamicEntryBox(Wrap)`** (§6-3) | 04-2 | 매 갱신 `NewObject` N개를 문서가 확정하기 전에 |
| **7** | **빈 칸 N개 → "＋ 남은 용량 13" 한 덩어리** (§7-2) | 04-0, 04-2 | 6번과 같은 절. 위젯 수도 준다 |
| **8** | **`GetInsertionOrder()`에 본체(`INDEX_NONE`) 선두 명시** (§10-1) | 03-4, `05_Loot_03_Inventory_STATUS.md` | 빠지면 **03-C에서 아무것도 안 주워진다** |
| **9** | **§10 누락 3건** — `LOOT_STATUS.md:25` / `DOCS/DOCS.md:43` / `GAME.md:157` | 3개 파일 | 1번은 자기모순, 2번은 범위 통제의 최상위 근거 |
| **10** | **함정 4k를 코드 주석 지시로 승격** (§1-2) | 03-2 | 순서를 바꿔도 컴파일되고, 증상이 "안 불린다"로 바뀌어 더 조용하다 |
| **11** | **quick-transfer를 "순서를 서버가 볼 뻔한 경로"로 명시** (§2-3) | 04-8 또는 `LOOT_STATUS.md` | 안 적으면 그때 `SortIndex`를 서버에 넣는 판단이 다시 올라온다 |
| **12** | **`TMap<int32, TArray<int32>>`의 USTRUCT 래핑 명시** (§2-4) | 04-8 | 중첩 컨테이너는 `UPROPERTY`가 안 된다. 구현에서 걸린다 |
| **13** | 드래그 중 `GetUsedSlots` 캐시 + 넘침을 **숫자로** (§6-4, §7-3) | 04-7 | 매 프레임 O(N)을 문서가 확정하기 전에 |
| a | `LOOT_STATUS.md` 이력표 9차/8차 순서, 03_STATUS의 `ContainerOrder` 라벨 | 2개 파일 | 표기만 |

**하지 않는 것:**

- 엔트리에 `SortIndex` 추가 (§2)
- 순서를 서버 복제로 (§2-5 ⓑ)
- `MoveEntry`에 `DisplacedEntryId` 기본 인자 (§5-1)
- `SwapEntries`를 Step 03으로 (§5-2)
- `HitTestCell` 지오메트리 계산 (§6-1)
- 2D 배치 / 칸 크기 비례 (§7-4)
- 자동 정렬을 드래그보다 먼저 (§8-3)
- `UListView` (8차 확정)
- 드래그를 Step 05로 미루기 (§8-2)

---

## 12. 인용 목록

**엔진** — `C:\Program Files\Epic Games\UE_5.7\Engine`

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `Runtime/Slate/Private/Framework/Application/SlateApplication.cpp:5523-5533` | `OnDrop`이 **`FBubblePolicy`** 로 라우팅 — 깊은 위젯부터 위로, 첫 `Handled`에서 멈춤 | §6-1, §9-1 |
| `…SlateApplication.cpp:5799` | `OnDragOver`도 `FBubblePolicy` | §6-1 |
| `Runtime/Engine/Private/DataChannel.cpp:867-911` | 분할 번치를 `InPartialBunch`에 모으고 **`bPartialFinal`까지** 기다린다 | §5-4, §9-3 |
| `Runtime/Net/Core/Classes/Net/Serialization/FastArraySerializer.h:985-1001` | `NumChanged`가 변경 전부를 한 헤더에. 상한 초과는 **경고뿐**(`UE_CLOG`) | §5-4, §9-3 |
| `Runtime/Net/Core/Private/…/FastArraySerializer.cpp:10` | `MaxNumberOfAllowedChangesPerUpdate = 2048` | §5-4 |
| `…FastArraySerializer.h:54` | 클라 배열 **순서 비보장** | §2-2 |
| `…FastArraySerializer.h:302-323` | 복사 시맨틱이 `ReplicationID` 리셋 | §3, §5-4 |
| `Runtime/Engine/Private/RepLayout.cpp:4582-4583` | 커스텀 델타는 **프로퍼티 단위** `NetDeltaSerialize` 한 번 | §5-4, §9-3 |
| `Runtime/UMG/Public/Blueprint/UserWidgetPool.h:14-23` | *"Pools UUserWidget instances … for UMG elements with **dynamic entries**"* + `@see UDynamicEntryBox` | §6-3, §9-4 |
| `Runtime/UMG/Public/Components/DynamicEntryBoxBase.h:14-22` | `EDynamicBoxType` — **`Wrap`** 포함 | §6-3 |
| `…DynamicEntryBoxBase.h:23-27` | *"dynamic number of auto-generated entries at both design- and run-time"* | §6-3, §9-4 |
| `…DynamicEntryBoxBase.h:179` | `FUserWidgetPool EntryWidgetPool` | §6-3 |
| `Runtime/UMG/Public/Components/DynamicEntryBox.h:51` | `Reset(bool bDeleteWidgets = **false**)` — 기본이 회수 | §6-3 |
| `Runtime/UMG/Public/Components/ListViewBase.h` | `UListView`도 `FUserWidgetPool`을 쓴다 | §9-4 |
| `Runtime/Engine/Classes/GameFramework/SaveGame.h:40-47` | `ULocalPlayerSaveGame` — *"associate a Save Game object with a specific local player"* | §2-4, §9-2 |
| `Runtime/Engine/Public/Subsystems/LocalPlayerSubsystem.h` | `UCLASS(Abstract, Within = LocalPlayer)`. Runtime/Editor 내 서브클래스 0건, **플러그인에 4건** | §2-4, §9-2 |
| `Plugins/Runtime/CommonUI/…/CommonInputSubsystem.h:23-24` · `CommonUIVisibilitySubsystem.h:22,40-43` | `ULocalPlayerSubsystem` 용례 — **런타임 상태만**(*"These can change over time"*) | §2-4, §9-2 |
| `grep -rln "NativeOnDrop\|NativeOnDragDetected" Engine/Source Engine/Plugins` | 소스 히트가 **UMG 자신 셋뿐**(`UserWidget.h/.cpp`, `SObjectWidget.cpp`). 구현 선례 0건 | §9-1 |
| `Plugins/Runtime/CommonUI/…/Public/` (전수) | **드래그앤드롭 클래스 없음** | §9-1 |

**Lyra** — `C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame\Source\LyraGame`

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `Settings/LyraSettingsShared.h:57-60` | *"per player … **safe to store in the cloud**"* | §2-4, §9-2 |
| `Settings/LyraSettingsShared.h:64` | `class ULyraSettingsShared : public **ULocalPlayerSaveGame**` | §2-4, §9-2 |
| `Settings/LyraSettingsShared.cpp:52-70` | `CreateTemporarySettings` / `LoadOrCreateSettings` — 로드가 한 줄 | §2-4 |
| `Settings/LyraSettingsLocal.h:36` | `ULyraSettingsLocal : UGameUserSettings` — 기기 종속은 따로 | §2-4, §9-2 |
| `grep -rln "ULocalPlayerSubsystem" LyraGame/` | **0건** | §2-4, §9-2 |
| `Equipment/LyraQuickBarComponent.h:66` | `int32 NumSlots = 3` — 인덱스가 곧 배치. "그릴 칸" 축이 없다 | §2-1 |
| `grep -rln "DragDrop" LyraGame/` | 인벤토리 드래그 UI **없음** (요청서 §1-3 확인) | §9-1 |

**프로젝트**

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `05_Loot_04_InventoryUI.md` 04-7 | `SwapEntries` 용량식 두 줄 | §3 |
| `05_Loot_04_InventoryUI.md` 04-7 | `NativeOnDrop` — `Hit`이 두 의미 | §4 |
| `05_Loot_04_InventoryUI.md` 04-8 / 요청서 §2-1 | `MoveTo(Inv, Container, EntryId, **NewIndex**)` | §4-1 |
| `05_Loot_03_Inventory.md` 03-3 | `GetUsedSlots`의 `if (!E.SlotId.IsNone()) continue;` | §3-1 |
| `LOOT_STATUS.md:25` | 04행이 *"아이템 목록 + 칸 수 게이지"* — **stale** | §10-1 |
| `LOOT_STATUS.md:69` | `FindFungibleEntryId(Container, ItemId)` (8차 확정) | §2-2 |
| `LOOT_STATUS.md:194-195` | 이력표 9차/8차 순서 뒤바뀜 | §10-a |
| `DOCS/DOCS.md:43` | *"인벤토리 UI (UMG, **최소한으로**)"* — **stale** | §10-2 |
| `GAME.md:157` | *"**목록에서는** 전부 한 줄로 보이되"* — **이번 라운드가 반증** | §10-3 |
| `GAME.md:180` | *"무기에 고정 슬롯 N개, 부착물은 자기 슬롯을 갖지 않는다(배그식)"* — 9차 §3-3의 `AttachmentSlots`가 **기획 문서에 이미 이름이 있다** | §10 참고 |
| `05_Loot_03_Inventory_STATUS.md:28,34` | `ContainerOrder`가 "9차 항목" 절에 있다 (10차 항목) | §10-b |
