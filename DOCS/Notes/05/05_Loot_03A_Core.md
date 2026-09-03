# Step 03-A — Inventory 코어 (자료구조 · 칸 합산 · 슬롯 · 표시 순서)

> 마스터 기획: `05_Loot_DOCS.md` (§4-6)
> **다음 구간: `05_Loot_03B_PickupDrop.md`** — 줍기 · 버리기 · 자동 착용
> 선행: Step 00(ItemCore) · Step 01(Spawner)
> **★ 15차에 통합 문서 `05_Loot_03_Inventory.md`(2692줄)를 둘로 쪼갠 것이다.** 쪼갠 이유와 경계 규칙은 아래 체크포인트 절.

---

## 목표

**아이템이 서버 권한으로 보관되고, 소유 클라이언트에만 델타 복제된다.** 칸 합산·슬롯 정합·표시 순서까지가 이 구간이다. **줍기와 버리기는 없다** — 아이템은 `EP.Inv.Add`로 만든다.

> **★ 이 구간은 `RemoveEntry`/`AddSubtree` 없이 컴파일되고 실행된다.** 헤더에 선언조차 하지 않는다 — 선언만 하고 정의를 안 하면 링크 에러다.

**완료 조건 — 03-A 몫 (번호는 통합 문서의 것을 유지한다)**

- [ ] **2.** 붕대 3개를 넣으면 엔트리가 3개다 (스택되지 않음)
- [ ] **3.** 현금뭉치 둘을 넣으면 엔트리 1개, `Charges`가 합산된다 (`bFungible`)
- [ ] **4.** 칸이 모자라면 **아무것도 안 들어간다** — `EP.Inv.Add`가 `INDEX_NONE`을 돌려주고 `Dump`가 안 바뀐다
  > **★ *"픽업이 그대로 남는다"* 는 03-B다** (13차). `bClaimed` 되돌림(03-4)이 있어야 성립한다
- [ ] **5.** 가방이 꽉 차도 현금뭉치·탄약상자는 들어간다 (칸이 안 늘어나므로)
- [ ] **6.** 무기(`SlotSize=5`)를 넣으면 `UsedSlots`가 5 증가한다 — 엔트리 개수가 아니라 **칸 수**로 찬다
- [ ] **7의 후반.** 배낭 칸이 본체·상의와 **독립된 풀**이다. 본체가 꽉 차도 배낭에는 들어간다
  > 앞 절(*"주우면 자동으로 매진다"*)은 03-B다 — 줍기 경로가 없다
- [ ] **14.** 아이템을 넣고 빼도 **기존 항목의 `EntryId`가 재번호되지 않는다** (`Dump` 꼬리의 `NextEntryId`)
- [ ] **15.** **다른 컨테이너로 옮기면 목적지 맨 뒤에 붙는다** — 옛 컨테이너의 키를 들고 가지 않는다 (함정 4m)
- [ ] **17.** **무기를 핫바에 꽂았다 빼면 원래 자리로 돌아온다** — 그 사이에 다른 아이템을 넣어도 **동률이 나지 않는다** (함정 4q)
- [ ] **18.** **재정규화와 경계 가드를 본다** (함정 4r·4t)
  > ① `EP.Inv.Reorder`로 **같은 틈에 16회** 삽입 → **이분 고갈 → 재정규화가 돈다.** `RenormalizeSortKeys`가 죽은 코드가 아님을 증명하는 것은 **이 경로 하나다**
  > ② **맨 앞으로 20회 반복 이동**(`PrevEntryId = -1`) → **`ensure`가 울리지 않고 순서가 매번 실제로 바뀐다.** 키가 `−Step`씩 내려가는 것을 `Dump`의 `SortKey` 열로 본다
  > **★ ②에서는 재정규화가 돌지 않는다 (13차 정정).** 경계까지 **32,764회**가 필요하고 맨 앞 분기는 `bNoGap`이 항상 거짓이다. 이건 **함정 4t의 회귀 테스트**다 — 가드를 잘못 쓰면 `ensure`가 울리거나(`bRetry` 있음) **서버가 멈춘다**(`bRetry` 없음)
  > **경계 재정규화는 커맨드로 도달 불가다.** 확인하려면 `SortKeyGuard`를 일시적으로 크게 잡는다
- [ ] **19.** 아이템을 원래 자리에 도로 놓으면 `SortKey`가 안 바뀐다 (`Dump`로 확인, 함정 4u)
- [ ] **13.** 다른 클라이언트에 내 인벤토리가 복제되지 않는다 (`COND_OwnerOnly`)
- [ ] **16.** `EP.Inv.Reorder`로 자리를 바꾸면 `Dump` 순서가 바뀌고 **클라와 서버가 같은 순서를 찍는다**

> **★ 이 구간의 검증은 전부 `EP.Inv.*` 커맨드로 한다** (03-9). UI는 Step 04이고 줍기는 03-B다.
>
> **★ `EP.Inv.Add`에 `[Container]` 인자가 필요하다** — 본체가 0칸으로 전환되면 인자 없는 형태로는 **아무것도 만들 수 없다**(§8 미정 #9). 전환은 03-B 완료 이후지만, **인자는 지금 넣는다** — 없으면 전환 후 이 아홉 개를 다시 못 돌린다.

---

## 체크포인트 — 이 단계는 둘로 나눠 진행한다 (13차에 셋에서 줄었다)

**파일을 쪼개지 않는다.** 경계에서 `RemoveEntry`가 정확히 갈라져 한 함수가 두 문서에 적히고, 그게 stale이 세 번 연속 난 원인이다. 대신 **작업을 나눈다** — 완료 조건 19개는 다른 단계 두 개 분량이다.

> **★★ 13차에 가운데 구간(옛 03-B 배낭)이 없어졌다.** 거기 남은 새 코드가 `Server_EquipBackpack`(3줄) 하나였는데 **Step 03에 호출자가 0개**였다 — 아래 별도 절.
>
> **★★ 14차에 그 함수 자체가 없어졌다.** 옮겨간 04-A에도 호출자가 없었다 — 아래 ④.

| | 범위 | 완료 조건 | 멈춰서 검증 |
|---|---|---|---|
| **03-A 코어** | 03-1 · 03-2 · 03-3 · **03-7** · 03-9<br>**＋ `GetEntryInSlot(Parent, SlotId)`**<br>**＋ `MoveEntry` (정합·사이클 검사 포함)**<br>**＋ `ActiveHotbarIndex` 필드**<br>**＋ `FEPItemData::SlotPriority` (DT 필드 + 배낭 행)**<br>**＋ `UEPLootDeveloperSettings::BodySlots`**<br>**＋ `SortKey` 일습 — `AssignSortKey` · `GetSortedContents` · `ReorderEntry` · `RenormalizeSortKeys`** (**RPC는 아니다** — 아래)<br>**＋ `EP.Inv.Move` · `EP.Inv.Add [Container]` (13차)** | **2~6, 7의 후반(독립 풀), 14·15·17~19** | `EP.Inv.Add`로 칸 합산·`bFungible`·`COND_OwnerOnly`, **`EP.Inv.Reorder`로 순서·재정규화, `EP.Inv.Move`로 검사 0~6·완료 조건 15·17.** **`RemoveEntry`/`AddSubtree` 없이 컴파일·실행된다** |
| **03-B 줍기·버리기** | 03-4 · 03-5 · 03-6<br>**＋ `GetInsertionOrder()`**<br>**＋ 스냅샷의 루트/자식 `SortKey`·`SlotId` 규칙**<br>**＋ `AddSubtree` · `TryAutoEquip` · `StartingEquipment`** (13차) | **1, 7의 전반(자동 착용), 8~13, 16** + 이월 2건 | `RemoveEntry` / `AddSubtree` / 캐스케이드 / `Server_DropItem` / 스폰 직후 상의·하의 착용 |

> **★ 함정표의 ★★ 대부분이 03-B에 몰려 있다.** 위험이 어디 있는지가 이 구분으로 드러난다.

> ### ★★ 13차 — `TryAutoEquip`이 내려가고, 가운데 구간이 없어졌다
>
> **①`TryAutoEquip`이 `AddSubtree`를 부른다.** 13차가 `AddSubtree(Parent, SlotId, In)`로 슬롯 경로를 명시하면서 **시그니처에 드러났을 뿐**, 초안의 *"`AddSubtree` 후 루트에 `MoveEntry`"* 때도 있던 의존이다. 완료 조건 7의 앞 절(*"배낭을 **주우면** 자동으로 매진다"*)은 줍기 경로(`OnInteract`)를 요구한다.
>
> **`AddSubtree`를 03-A로 올리는 안은 아무것도 고치지 못한다.** `TryAutoEquip`의 진짜 호출자도 줍기 쪽이므로, 둘을 가운데 구간에서 돌리려면 **`EP.Inv.AutoEquip` 같은 커맨드를 하나 더** 만들어야 한다 — **한 구간 뒤에 진짜 호출자가 오는 함수를 위해서.** `EP.Inv.Move`를 만든 이유(*"Step 04까지 한 줄도 안 돈다"*)와 정반대다. **내려보내면 새 커맨드가 0개다.**
>
> **②그러고 나니 가운데 구간에 남는 새 코드가 `Server_EquipBackpack`(3줄) 하나인데, Step 03에 호출자가 0개다.**
>
> | 경로 | 무엇을 부르나 |
> |---|---|
> | 줍기 자동 착용 | `OnInteract` → `TryAutoEquip` → `AddSubtree` — **서버 내부. RPC를 안 지난다** |
> | 벗기 | **`Server_DropItem`** (03-6 확정) |
> | 다중 컨테이너 검증 | **`EP.Inv.Add` ＋ `EP.Inv.Move <id> -1 Back`** — 둘 다 03-A |
> | 수동 착용 UI | **04-B** 드래그 → `Server_MoveEntry` / **04-A** 검증은 커맨드 (`05_Loot_04_InventoryUI.md:45`) |
>
> **이 문서가 같은 상황에 같은 규칙을 이미 두 번 적용했다.**
>
> > *"Step 03에는 `NewParent`와 `NewSlotId`를 정당하게 만들어낼 UI가 없다"* → `Server_MoveEntry`를 **안 만든다**
> > *"Step 03에는 정당한 클라 호출자가 없다"* → `Server_ReorderEntry`를 **04-B로**
>
> **`Server_EquipBackpack`도 정당한 클라 호출자가 없다.** 규칙이 여기만 적용되지 않았고 이유는 *"03-6이 원래 그 RPC로 쓰여 있었다"* 는 역사뿐이다. → 13차는 **04-A로 보냈다.**
>
> **④그런데 04-A에도 호출자가 없었다 — 14차에 함수를 없앤다.** 13차의 근거는 *"`EP.Inv.Equip`이 첫 호출자다"* 였는데 **`EP.Inv.Equip`은 콘솔 커맨드**이고, 이 문서가 두 번 확립한 규칙(*"커맨드가 내부 함수를 직접 부른다. RPC 표면을 열지 않는다"*)이 그대로 적용된다. **옮긴 자리에서도 0개다.**
>
> > ***"좁은 RPC가 넓은 RPC보다 낫다"* 도 14차에 무너졌다.** 그건 `Server_MoveEntry`와의 비교인데 **04-B가 그것을 연다**(`05_Loot_04_InventoryUI.md:610`). 넓은 문이 열린 뒤의 좁은 문은 **공격 표면을 하나도 줄이지 않는다.** 상세는 03-2의 별도 절.
>
> **③그리고 남겼다면 없어질 전제 위에서 검증하게 된다.** 옛 03-B의 검증 절차는 `EP.Inv.Add Backpack_B`(SlotSize 10)로 본체(10칸)에 넣는 것이었는데, **`Backpack_A`(15)도 `Shirt_Basic`(11)도 본체에 안 들어가고**, `MaxSlots`가 0이 되면 **절차가 통째로 죽는다.** A-3이 *"본체 경유는 곧 사라질 전제 위에 있다"* 고 진단한 그 전제 위에 **검증 경로가 그대로 서 있었다** — 생산 경로는 고치고 검증 경로는 안 고친 것이다.
>
> > **9차의 규칙을 반대 방향으로 쓴 것이다.** 규칙은 *"호출자와 같은 구간에"* 이고, 9차는 쓰는 쪽이 **앞**이라 다섯 개를 올렸고 13차는 쓰는 쪽이 **뒤**라 셋을 내렸다. **같은 규칙이다.**
>
> > **`GetCapacity`는 통째로 03-A다.** `EP.Inv.Move`가 컨테이너로 옮기므로 컨테이너 갈래도 03-A에서 실행된다 — *"함수를 반만 만든다"* 가 없어진 것이 이 재조정의 부수 이득이고, 8차가 03-7에서 배운 것과 같다.

### ★ 분할선이 8차 검수로 두 군데 바뀌었다

- **03-7(알림)이 03-A로 왔다.** `FScopedInventoryNotify`를 `AddItem`·`SetEntryCharges`(둘 다 03-3)가 쓴다 — 정의가 03-B에 있으면 03-A가 컴파일되지 않는다. 그리고 `PostReplicatedReceive`가 없으면 03-A의 "클라이언트에 복제된다"를 `EP.Inv.Dump`로만 보게 되어, **델리게이트가 한 번도 안 돈 채로 Step 04에 넘어간다**
- **완료 조건 1("주운 아이템이 인벤토리에 들어가고")이 03-B로 갔다.** 줍기는 `AEPPickup::OnInteract`(03-4)이고 03-A에는 `EP.Inv.Add`밖에 없다

> **03-A에서는 `RemoveEntry`를 헤더에 선언조차 하지 않는다.** 선언만 하고 정의를 안 하면 링크 에러다 — 03-1의 콜백 함정과 같은 종류다. 헤더에 안 적으면 컴파일러가 잊어준다.

---


### ★★ 15차 — 파일을 쪼갰다. 8차 결정을 뒤집는다

**8차가 세운 규칙은 *"파일을 쪼개지 않는다. 작업만 나눈다"* 였고, 근거는 이랬다.**

> *"경계에서 `RemoveEntry`가 정확히 갈라져 한 함수가 두 문서에 적히고, 그게 stale이 세 번 연속 난 원인이다."*

**그 근거가 두 번 무너졌다.**

- **① 13차가 가운데 구간을 없앴다.** 셋에서 둘로 줄면서 **경계가 둘에서 하나**가 됐다.
- **② 문제의 `RemoveEntry`가 더 이상 경계에 없다.** 9차·13차를 거치며 제거 경로가 `RemoveEntry` → `RemoveEntryInternal` → `RemoveSelf`로 갈렸고, **셋 다 03-B다.** 03-A는 `RemoveSelf`를 **호출조차 하지 않는다** — 03-7의 가드 표가 이름만 언급할 뿐이다.

**그리고 쪼개지 않아서 생긴 비용이 실측됐다.** 2692줄 한 파일에서 **열 함수의 본문이 아예 빠진 것을 아무도 못 봤다** — `FindEntry`·`ContainsEntry`·`RemoveSelf`·`AssignSortKey`·`KeySpace_Min`·`KeySpace_NextAbove`·`KeyOf`·`GetEntryInSlot`·`FindFungibleEntryId`·`GetEquippedEntryId`. 설계 근거·함정·검수 이력이 층층이 쌓이는 동안 **가장 기본적인 것이 비어 있었다.**

#### 쪼개면서 지키는 경계 규칙 셋

| | 규칙 |
|---|---|
| **한 함수는 한 문서에** | 선언은 03-A(헤더 전체가 A에 있다), **본문은 그 함수를 부르는 구간에** |
| **함정표를 나눈다** | 대응 열이 가리키는 절로 라우팅한다. **양쪽에 걸리는 것만 둘 다에 둔다**(현재 극소수) |
| **완료 조건은 번호를 유지한다** | 통합 문서의 1~20번을 그대로 쓴다 — 다시 매기면 STATUS·검수 이력의 참조가 전부 끊긴다 |

> **★ 앞으로 stale이 나면 여기를 의심한다.** 8차의 우려가 사라진 것이 아니라 **비용이 반대쪽으로 커진 것**이다.


---

## 03-1. 자료구조

```cpp
// ── EPInventoryComponent.h 파일 앞부분 ──────────────────────
// ★★ 세 라운드 연속 여기서 걸렸다(9·10·11차). 한 번만 적어두고 이후 블록은 클래스 본문만 보여준다
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Inventory/EPInventoryTypes.h"      // ★ FEPInventoryEntry — 완전 타입이 필요하다
#include "EPInventoryComponent.generated.h"  //   UPROPERTY() TArray<T>는 T의 정의를 요구한다

class AEPPickup;                   // SpawnPickupInFront
class UEPItemDefinitionSubsystem;  // Defs()
class UEPCombatComponent;          // RemoveEntryInternal ①
// ───────────────────────────────────────────────────────────

// (아래 FEPInventoryEntry는 EPInventoryTypes.h에 있다 — 파일 위치 절 참조)
USTRUCT()
struct FEPInventoryEntry : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY() int32 EntryId       = INDEX_NONE;   // ★ 서버 발급 개체 식별자
    UPROPERTY() int32 ParentEntryId = INDEX_NONE;   // ★ 담고 있는 컨테이너. NONE이면 본체
    UPROPERTY() FName SlotId;                        // 장비·부착 슬롯 이름. None이면 컨테이너 수납
    UPROPERTY() FName ItemId;
    UPROPERTY() FEPItemState State;                  // 개체 상태를 값으로 내장 (Step 00)

    // ★★ 화면 순서. 형제(같은 Parent) 안에서만 의미가 있다 — 아래 별도 절 (11차)
    //   ★ SlotId가 있어도 키를 갖는다. 표시에서만 빠지고 자리는 잡아둔다 —
    //     그래서 무기를 핫바에 꽂았다 빼면 원래 자리로 돌아온다 (11차 검수, 함정 4q)
    UPROPERTY() int32 SortKey = 0;

    // ★ 항목 단위 콜백을 선언하지 않는다 — 아래 참조. 선언만 하면 링크 에러다
};

USTRUCT()
struct FEPInventoryList : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY() TArray<FEPInventoryEntry> Items;

    // ★ 구체 타입으로 둔다 (15차에 11차를 뒤집음 — 아래).
    //   아직 선언되지 않았지만 .generated.h가 전방선언을 넣어준다. 캐스트가 필요 없다.
    UPROPERTY(NotReplicated) TObjectPtr<UEPInventoryComponent> Owner;

    // ★ 정의는 둘 다 .cpp다 — 이 구조체 안에서 컴포넌트는 불완전 타입이다 (15차)
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);
    void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters&);
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

> ### ★★ `Build.cs`에 `NetCore`를 추가해야 한다 — 안 하면 **링크 에러**다
>
> ```csharp
> // EmploymentProj.Build.cs
> PublicDependencyModuleNames.AddRange(new string[] { "NetCore", "Core", /* ... */ });
> ```
>
> `Engine.Build.cs:86`에 `NetCore`가 `PublicDependencyModuleNames`로 들어 있긴 하다. 그러나 **모듈러(DLL) 빌드에서 UBT는 상위 모듈의 public include 경로만 전파하고, import 라이브러리는 직접 의존한 모듈 것만 링크 줄에 넣는다.** 그래서 증상이 갈라진다:
>
> | 단계 | 결과 | 이유 |
> |---|---|---|
> | 컴파일 | ✅ 성공 | `FastArraySerializer.h`를 찾는다 (include 경로는 전파됨) |
> | 링크 | ❌ 실패 | `UnrealEditor-NetCore.lib`가 링크 줄에 없다 |
>
> ```
> LNK2019: "Z_Construct_UScriptStruct_FFastArraySerializerItem" 외부 기호를 확인할 수 없음
>          └─ Z_Construct_UScriptStruct_FEPInventoryEntry_Statics::StructParams 에서 참조
> ```
>
> 기반 struct의 **리플렉션 등록 함수를 불러야 하는 건 상속했을 때뿐**이다. 그래서 `FEPInventoryEntry`가 프로젝트 최초의 NetCore USTRUCT 상속이 되는 이 지점에서야 드러난다.
>
> Lyra가 `LyraGame.Build.cs:57`에 `NetCore`를 명시한 것은 **관례가 아니라 요구**다. Public에 넣든 Private에 넣든 링크는 통한다 — Lyra는 Private을 썼다.

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
- UI는 `EntryId`가 아니라 **`SortKey` 오름차순**으로 그린다 (아래 별도 절). `EntryId`는 **식별자이지 순서가 아니다** — 둘을 겸하게 두면 재배치를 넣는 순간 식별자를 바꿔야 한다
- **`ParentEntryId`가 이 번호를 참조한다.** 재번호하면 배낭 속 아이템이 엉뚱한 곳에 매달린다

> **로드맵 5단계 세이브 주의:** `NextEntryId`도 함께 저장해야 한다. 안 하면 로드 후 1부터 재발급해 기존 엔트리와 충돌하고, 하필 `ParentEntryId`를 오염시켜 **아이템이 엉뚱한 컨테이너에 들어간다.**

### ★ `ParentEntryId` — 배낭과 부착물이 같은 구조다

```
EntryId=1  Parent=NONE  Shirt_Basic   SlotId=Torso   ← 입고 있는 상의 (10칸 제공)
EntryId=2  Parent=NONE  Backpack_B    SlotId=Back    ← 매고 있는 배낭  (8칸 제공)
EntryId=3  Parent=1     Bandage         1칸           ← 상의 내부
EntryId=4  Parent=2     Weapon_AK74     5칸           ← 배낭 내부
EntryId=5  Parent=4     Scope_4x   SlotId=Optic       ← 배낭 속 총의 부착물 (§7-3, 추후)
```

- **중첩 struct가 아니라 부모 참조인 이유:** 자기 타입 재귀는 `Class.cpp:974`에서 **Fatal**이고, 이종 중첩은 프로퍼티 델타를 잃는다. 무엇보다 **배낭과 부착물이 같은 표현을 쓰게 되는 것**이 크다 — 위 `EntryId=5`가 특수 케이스 없이 성립한다
- **`SlotId`가 장착의 유일한 진실이다.** 용량 합산이 "수납된 자식"과 "슬롯에 든 자식"을 구분해야 하므로(슬롯에 든 것은 칸을 안 먹는다) 필드가 없으면 판정식을 다시 써야 한다. **값 목록은 `DOCS/Mine/EquipmentSlots.md` §1** — 핫바 `Hotbar1`~`Hotbar4`, 착용 8종(`Torso`/`Legs`/`Coat`/`Back`/`Wrist`/`Ears`/`Face`/`Feet`), 부착 4종(`Optic`/`Muzzle`/`Grip`/`Mag`). **Step 03에서 실제로 쓰이는 값은 `"Back"` 하나다**
  > **★ 9차 확정 — 별도 장비 슬롯 배열을 만들지 않는다.** 엔진 자신이 같은 형태다: `USceneComponent`가 `AttachParent` + `AttachSocketName`을 **자식**에 두고, 자식 목록(`AttachChildren`)은 `Transient` **파생 색인**이다(`SceneComponent.h:108-119`). 진실은 자식에, 색인은 파생으로. 근거 전문은 `EquipmentSlots.md` §3·§11

### ★★ `SortKey` — 화면 순서를 서버가 든다 (11차 결정)

**형제 안에서의 상대 순서다.** 같은 `ParentEntryId`를 가진 것들끼리만 비교하고, 다른 컨테이너의 값과 겹쳐도 아무 문제가 없다. `SlotId`가 있는 엔트리는 **슬롯이 곧 자리**라 이 값을 보지 않는다.

```
Parent=NONE   SortKey=      0   Shirt_Basic  SlotId=Torso   ← 슬롯. 키는 갖되 표시에서 빠진다
Parent=NONE   SortKey=  65536   Backpack_B   SlotId=Back
Parent=1      SortKey=      0   MedKit          ← 값이 겹쳐도 무관하다. 부모가 다르다
Parent=1      SortKey=  65536   Bandage
Parent=4      SortKey=      0   Scope_4x     SlotId=Optic   ← 슬롯. 표시 목록에서 빠진다
```

#### 왜 서버가 드는가

**10차까지는 클라이언트 로컬이었다.** 근거는 *"서버 로직 중 순서를 보는 곳이 0곳"* 이었고 그건 지금도 사실이다. 뒤집은 이유는 **지속**이다.

로드맵 5단계 세이브(`DOCS.md §5` 14번)는 **엔트리 배열을 저장한다.** 순서가 엔트리의 필드면 **저장 코드를 한 줄도 안 쓰고 따라간다.** 클라 로컬로 두면 별도 세이브(`ULocalPlayerSaveGame`)가 필요하고, 그쪽은 `EntryId`가 매치마다 1부터 재발급되는 것과 **충돌한다** — 지난 매치의 `Order[7]`이 이번 매치의 7번(전혀 다른 아이템)에 적용된다. 세션 도장을 찍어 무효화하면 지속이 한 번도 발휘되지 않아 **애초에 클라 인메모리와 같아진다.**

> **그리고 이행 비용이 여기서 갈린다.** §7-1의 2D 격자(테트리스)로 가게 되면 `SortKey`가 `FIntPoint Location`으로 바뀌는데, **호출 지점이 완전히 동일하다** — `InsertEntry`(빈 자리) · `MoveEntry`(새 컨테이너면 새 자리) · 재배치 RPC · 스냅샷에 실려 감 · 루트/자식 분기. **필드 교체이지 구조 변경이 아니다.** 클라 로컬로 두면 같은 이행이 *"04-8을 버리고 03을 다시 연다"* 가 된다.

#### ★★ 선례 — 엔진도 같은 이유로 복제되는 정렬 키를 둔다 (12차 검수에서 뒤집힘)

**11차까지 이 문서와 검수 답변은 *"UE에 선례가 없다"* 를 전제로 원칙을 세웠다. 틀렸다.**

```cpp
// UIFStackBox.h:45-47 — FUIFrameworkStackBoxSlot : FUIFrameworkSlotBase : FFastArraySerializerItem
/** Index in the array the Slot is. The position in the array can change when replicated. */
UPROPERTY()
int32 Index = INDEX_NONE;
```

**주석이 우리 문제를 그대로 적고 있다** — *"복제되면 배열 안 위치가 바뀔 수 있다."* `FastArraySerializer.h:54`가 말하는 그것이고, **`SortKey`를 도입한 이유와 같다.** 같은 플러그인에 두 번째 형태도 있다 — `FUIFrameworkGameLayerSlot::ZOrder`(`UIFPlayerComponent.h:56-57`).

> **왜 못 찾았나:** `FFastArraySerializerItem` **직계 파생**만 훑었다. `FUIFrameworkSlotBase`에는 순서 필드가 없고, **그것을 상속한 슬롯 타입**에 있다. 전수는 14건(테스트 3 · 문서 예제 1 제외 시 10)이다.

**엔진은 조밀을 골랐다. 조건이 우리와 다르다.**

```cpp
// UIFStackBox.cpp:42-46 — 추가는 배열 인덱스를 그대로
int32 NewEntryIndex = Slots.Add(MoveTemp(Entry));
Slots[NewEntryIndex].Index = NewEntryIndex;

// UIFStackBox.cpp:53-60 — 제거는 뒤를 전부 재번호
Slots.RemoveAt(Index);
for (; Index < Slots.Num(); ++Index) Slots[Index].Index = Index;
```

| | UIFramework StackBox | 우리 |
|---|---|---|
| N | 위젯 몇 개 | 컨테이너 20~30, **스태시 280**(로드맵 14) |
| 순서를 누가 바꾸나 | **코드** (추가·제거) | **사용자** (드래그가 주 조작) |
| 중간 삽입 | 사실상 없다 | **정리의 본체** |
| 재번호 1회 비용 | 위젯 몇 개 dirty | **형제 N개 dirty** |

**조밀은 "순서 변경이 곧 구조 변경(추가/제거)일 때" 맞다.** 그때는 어차피 배열이 바뀌므로 재번호가 추가 비용이 아니다. **우리는 순서만 바꾸는 조작이 따로 있고 그게 제일 잦다.**

> **그리고 위 `RemoveEntry`의 `for` 루프가 곧 `RenormalizeSortKeys`다.** 아래 *"조밀로 하면 재정규화 코드가 사라진다"* 가 거짓이라는 판정을 **엔진 소스가 직접 보여준다.**

#### 왜 조밀한 정수(0,1,2,…)가 아니라 **희소 배치**인가

조밀하면 드래그 한 번에 **형제 전부를 재번호**해야 한다. 그 비용은 실재한다 — 다만 흔히 생각하는 것보다 작으니 정확히 적어둔다.

FastArray는 경로가 **둘**이다.

| 경로 | 언제 | 바뀐 항목당 보내는 것 |
|---|---|---|
| `FastArrayDeltaSerialize` (`:1474-1485`) | 델타 미지원/미요청 시 **폴백** | `NetSerializeStruct` — **구조체 전체** (≈45B) |
| **`FastArrayDeltaSerialize_DeltaSerializeStructs`** (`:1398-1401` → `:1645`) | **기본값** | 바뀐 **프로퍼티만** (`SortKey` 하나 ≈12B) |

> *"Delta Serialization for inner structs is now enabled by default. That means that when a ReplicationKey changes, we will compare the current state of the struct to the last sent state, tracking changelists and only sending properties that changed exactly like the standard replication path."*
> — `FastArraySerializer.h:218-219`

**가장 직접적인 근거는 기반 생성자다(11차 검수).**

```cpp
// FastArraySerializer.cpp:24-36 — 아무도 안 불러도 켜져 있다
FFastArraySerializer::FFastArraySerializer()
    : ... , DeltaFlags(EFastArraySerializerDeltaFlags::None)
{ SetDeltaSerializationEnabled(true); }
```

> **단 불변 보장은 아니다.** 델타는 **연결이 협상**한다 — `bSupportsFastArrayDeltaStructSerialization`이 `FEngineNetworkCustomVersion::FastArrayDeltaStruct` 이상일 때만 켜지고(`RepLayout.cpp:4590-4597`), `net.SupportFastArrayDelta 0`으로 끌 수 있다. 같은 빌드끼리는 항상 참이지만 **설계를 여기 걸지 않는다** — 위 판정(O(N) vs O(1))은 델타 여부와 무관하게 성립한다.

그래서 조밀 재번호의 실제 비용은 이렇다.

| 컨테이너 | 조밀 재번호 | 희소 |
|---|---|---|
| 가방 20칸 | ~240B + 20개 changelist 비교 | ~12B + 1개 |
| **스태시 280칸** (5단계) | **~3.4KB + 280개 changelist 비교** / 드래그 | ~12B + 1개 |

**★★ 그런데 기각 사유는 이 숫자가 아니다(11차 검수).** 3.4KB는 UI 조작 빈도에서 문제가 안 된다. **진짜 사유는 "조밀로 하면 재정규화 코드를 아낀다"는 이득이 존재하지 않는다는 것이다.**

```cpp
// 희소의 RenormalizeSortKeys — 고갈 때만 부른다
int32 K = 0;  for (int32 Id : 형제) { AssignSortKey(Id, K); K += SortKeyStep; }

// 조밀 재번호 — 재배치마다 부른다
int32 K = 0;  for (int32 Id : 형제) { AssignSortKey(Id, K); K += 1; }
```

**같은 루프다. 이름만 다르다.** 희소가 추가로 쓰는 것은 **중점 계산 + 경계 가드 ≈ 5줄**이고, 그 5줄이 O(N)을 O(1)로 만든다. **없는 이득과 실재하는 O(N)을 맞바꿀 이유가 없다.**

> 델타 경로는 바뀐 항목마다 이전 전송 상태와 비교해 changelist를 만들므로 **대역폭보다 서버 CPU가 먼저 보인다.** 스태시 정렬은 플레이어가 가장 많이 하는 조작이다.

```cpp
static constexpr int32 SortKeyStep = 1 << 16;   // 65536

// 맨 뒤에 붙이기 :  MaxSiblingKey + SortKeyStep
// 사이에 꽂기   :  (PrevKey + NextKey) / 2
// 맨 앞         :  FirstKey - SortKeyStep
```

- **상한:** 65536 간격으로 32767개까지 `int32`에 들어간다. 스태시 280개는 여유가 압도적이다
- **고갈:** 같은 틈에 연속으로 꽂으면 ~16회에서 `(Prev+Next)/2 == Prev`가 된다 → 그 컨테이너만 `0, Step, 2*Step…`으로 **재정규화**한다. 이건 도달 불가 분기가 아니라 **이 방식의 정의상 반드시 도달하는 지점**이라 완료 조건으로 증명한다

> **`double`을 쓰지 않는다. 결정적인 것은 고갈 판정이다(11차 검수에서 근거가 바뀌었다).**
>
> ```cpp
> // int32 : 정확하다. 경계 가드도 상수 비교로 끝난다
> if (NewKey <= PrevKey) 재정규화;
>
> // double : 부동소수 동등 비교가 유일한 판정이다
> if (Mid == Prev || Mid == Next) 재정규화;   // 맞는데, 보는 사람마다 버그로 신고한다
> ```
>
> **정답인데 영원히 의심받는 코드다.** 5단계 2차의 외부 DB(REST) 왕복과 `EP.Inv.Dump`의 `0.37500000000000006`도 사실이지만 **둘 다 부차적이다** — DB는 마감 밖이고 Dump는 포맷으로 가릴 수 있다. 그리고 **이분 여유 16회 vs 52회는 이 선택의 축이 아니다** — 재정규화가 있으면 둘 다 "사실상 안 남"이고, 없으면 둘 다 언젠가 깨진다.

> **컨테이너가 `TArray<int32> ContentOrder`를 드는 안도 검토했다.** dirty가 1개인 건 같지만 ① **본체(`INDEX_NONE`)는 엔트리가 없어** 컴포넌트에 별도 배열이 필요하고(같은 개념이 두 집에 산다 — CLAUDE.md §2), ② 아이템이 죽을 때 **부모 배열에서 빼는 청소**가 생긴다. `SortKey`는 **엔트리와 함께 죽어 자가 청소된다** — `ActiveHotbarIndex`(슬롯을 가리켜 죽은 번호가 없음)를 `HotbarRefs`보다 낫다고 본 것과 같은 기준이다.
>
> **★ 그리고 이것은 9차가 이미 기각한 모양이다(11차 검수).** 9차는 *"별도 배열(부모가 자식 참조 목록을 듦)"* 대신 *"자식이 자기 자리를 듦(`SlotId`)"* 을 택했고, 근거가 `USceneComponent`가 `AttachSocketName`을 **자식**에 두고 `AttachChildren`은 `Transient` 파생 색인이라는 것이었다(`SceneComponent.h:108-119`). **`SortKey`는 그 결정의 연장선이다** — 같은 기준이 세 번째로 같은 답을 낸다.

---

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
    // ★★ 목적지는 (Parent, SlotId) 쌍이다 — InsertEntry·MoveEntry와 같은 어휘. 기본값을 주지 않는다 (13차)
    //   SlotId가 None이 아니면 CanFit을 건너뛰고 루트를 그 슬롯에 넣는다 — 착용은 칸을 안 먹는다
    int32 AddSubtree(int32 Parent, FName SlotId, const TArray<FEPInventoryEntry>& In);

    // ── 조회 ────────────────────────────────────────────────
    // ★ 포인터가 아니라 값으로 돌려준다 (아래 참조)
    bool  FindEntry(int32 EntryId, FEPInventoryEntry& Out) const;

    // ★★ 컨테이너 안에서만 찾는다. 인자를 빼면 배낭 속 현금이 본체 현금과 합쳐진다 (아래)
    int32 FindFungibleEntryId(int32 Container, FName ItemId) const;   // 없으면 INDEX_NONE

    int32 GetUsedSlots(int32 Container) const;            // 그 컨테이너의 Σ SlotSize
    int32 GetCapacity(int32 Container) const;             // 본체면 MaxSlots, 아니면 DT
    bool  CanFit(int32 Container, FName ItemId) const;    // 삽입 판정의 유일한 지점
    bool  IsFungible(FName ItemId) const;                 // DT의 bFungible

    // ★★ 슬롯 배치 가능 판정의 유일한 지점 — MoveEntry 검사 2·3·4가 이 함수다 (13차)
    //   셋 다 (Parent, SlotId, ItemId)의 함수이고 옮기는 엔트리를 보지 않는다.
    //   소비자가 셋이다 — MoveEntry · AddSubtree · §7-3 부착. CanFit과 같은 이유로 뽑는다
    bool CanPlaceInSlot(int32 Parent, FName SlotId, FName ItemId) const;

    // ★ 슬롯 조회는 전부 이것 하나로. 슬롯이 12개라 슬롯마다 게터를 만들지 않는다
    //   부모 인자는 선택이 아니다 — Optic은 무기마다 하나씩 있는 슬롯이다 (아래)
    int32 GetEntryInSlot(int32 Parent, FName SlotId) const;      // 없으면 INDEX_NONE

    int32 GetEquippedBackpack() const { return GetEntryInSlot(INDEX_NONE, TEXT("Back")); }
    int32 GetEquippedEntryId()  const;    // ActiveHotbarIndex → 슬롯 이름 → 엔트리

    // UI가 순회할 읽기 전용 뷰. 반환 참조는 그 프레임 안에서만 유효하다
    const TArray<FEPInventoryEntry>& GetEntries() const { return Entries.Items; }

    // ★★ 그 컨테이너의 수납 아이템을 SortKey 오름차순으로. 슬롯에 든 것은 빠진다 (11차)
    //   클라(그리기)와 서버("전부 옮기기")가 같은 함수를 쓴다 — 순서 해석이 한 곳이다
    //   동률은 EntryId로 깬다. 정상 데이터에서는 안 나지만 결정성을 계약으로 둔다
    TArray<int32> GetSortedContents(int32 Container) const;

    // ── 수정 (원시 엔트리를 내보내지 않으므로 이것들이 유일한 통로) ──
    void SetEntryCharges(int32 EntryId, int32 NewCharges);  // ★ 유일한 쓰기 지점
    void AddEntryCharges(int32 EntryId, int32 Delta);       //   Set에 위임. 음수면 차감

    // ★ ParentEntryId + SlotId를 고치는 유일한 지점. 장착·해제·드래그·부착이 전부 이것 하나다
    bool MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId);   // 아래 별도 절

    // ★★ Server_EquipBackpack은 **없다** (14차 삭제). 착용 RPC는 Server_MoveEntry(04-B) 하나다
    //    — 아래 "Server_MoveEntry는 만들지 않는다" 절

    // ★ 제거된 서브트리를 전위 순회로 돌려준다. In[0]이 루트 (03-5 계약)
    //   재귀 파라미터를 노출하지 않는다 — 아래 참조
    bool RemoveEntry(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved = nullptr);

    // 획득 시 ①단계. 아이템의 SlotPriority를 순회해 첫 빈 슬롯에 서브트리를 넣는다 (03-6)
    //   AddSubtree와 반환 규약이 같다 — 루트의 새 EntryId, 실패는 INDEX_NONE
    //   ★ 루트에만 SlotId를 세팅한다. 조준경이 달린 총을 통째로 장착하는 경우가 있다
    //   Step 03에서는 배낭 행의 ["Back"] 하나만 돌아 "배낭 자동 착용"과 정확히 같다
    int32 TryAutoEquip(const TArray<FEPInventoryEntry>& In);

    // 획득 시 ②단계. 어느 컨테이너부터 볼 것인가. [INDEX_NONE, 외투, 상의, 하의, 배낭 …] (03-4)
    //   ★ 맨 앞의 INDEX_NONE(본체)은 반드시 직접 붙인다 — ContainerOrder에는 본체가 없다
    //   ★★ 본체가 0칸이 되어도 이 함수는 안 바뀐다 (13차). CanFit이 항상 거짓이라
    //      그냥 지나간다 — 맨 앞 원소를 빼는 것은 헛도는 판정을 없애는 것뿐이다
    TArray<int32> GetInsertionOrder() const;

    // ★ 상태 변경 RPC의 유일한 게이트. 죽음·시전 확인이 여기 한 곳에 있다 (03-5)
    //   ★★ 규칙은 "모든 Server_*의 첫 줄"이다 (13차). "게이트가 한 곳에 있다"만 적으면
    //      "모든 RPC가 그것을 지난다"가 안 따라온다 — 실제로 빠뜨린 전례가 있다(옛 Server_EquipBackpack).
    //      Step 03의 대상은 Server_DropItem 하나, 04-B에서 Move·Swap·Reorder 셋이 는다
    bool CanMutateInventory() const;

    UFUNCTION(Server, Reliable)
    void Server_DropItem(int32 EntryId);                  // 03-5

    // ★★ 같은 컨테이너 안에서 자리만 바꾼다. 부모도 슬롯도 용량도 안 건드린다 (11차)
    //   PrevEntryId 바로 뒤에 놓는다. INDEX_NONE이면 맨 앞
    //   ★ 인덱스가 아니라 이웃을 받는 이유는 아래 별도 절
    //   ★★ RPC가 아니다. Server_ReorderEntry(외부 표면)는 Step 04-B에서 연다 —
    //      9차가 MoveEntry / Server_MoveEntry에 적용한 규칙과 같다. 여기 호출자는 EP.Inv.Reorder뿐
    void ReorderEntry(int32 EntryId, int32 PrevEntryId);

    DECLARE_MULTICAST_DELEGATE(FOnInventoryChanged);
    FOnInventoryChanged OnInventoryChanged;               // UI가 구독 (Step 04)

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;

    UPROPERTY(Replicated) FEPInventoryList Entries;
    // ★★ 본체 용량. **최종값은 0이다** — 수납은 착용 컨테이너에서만 나온다 (13차)
    //   테스트 중에만 10으로 둔다. 필드는 남긴다 — 0을 넣으면 CanFit이 항상 거짓이라
    //   나머지 코드가 그대로 돌고, 없애면 GetInsertionOrder의 맨 앞 INDEX_NONE까지 빼야 한다
    UPROPERTY(Replicated) int32 MaxSlots = 10;
    // ★ SlotId로 표현되지 않는 유일한 상태 — "1번과 2번 중 지금 어느 쪽을 들었나"
    //   가리키는 것이 엔트리가 아니라 슬롯이라 죽은 번호가 생길 문법이 없다 (아래)
    UPROPERTY(Replicated) int32 ActiveHotbarIndex = INDEX_NONE;   // 0~3. 세팅은 Step 05

private:
    // ★ 재귀 본체. bIsRoot를 밖에서 넘길 문법이 없어야 계약이 지켜진다 (아래)
    bool RemoveEntryInternal(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved, bool bIsRoot);
    void RemoveChildrenRecursive(int32 ParentId, TArray<FEPInventoryEntry>* OutRemoved);
    void RemoveSelf(int32 EntryId);                  // 배열에서 빼고 MarkArrayDirty
    bool ContainsEntry(int32 EntryId) const;

    // ★ 번호 발급 · 삽입 · MarkItemDirty의 유일한 지점.
    //   칸 검사도 bFungible 합치기도 하지 않는다 — 그건 호출자 몫이다
    //   ★ SortKey도 여기서 발급한다 (형제 맨 뒤). 호출자가 정하지 않는다
    int32 InsertEntry(int32 Parent, FName ItemId, const FEPItemState& State, FName SlotId);

    // ★ SortKey를 고치는 유일한 지점. MarkItemDirty가 여기 한 곳에 있다 (11차)
    void AssignSortKey(int32 EntryId, int32 NewKey);

    // 형제 키를 0, Step, 2*Step... 으로 다시 깐다. 이분 고갈 시에만 돈다
    void RenormalizeSortKeys(int32 Container);

    // ★★ KeySpace_ 접두어는 장식이 아니다 (12차 검수). 이 셋은 **부모 전체**를 본다 —
    //   GetSortedContents(표시 목록)를 부르면 안 된다. 같은 혼동이 두 번 났다 (함정 4q·4s)
    int32 KeySpace_NextAtEnd(int32 Container);        // 최대 + Step. ★ 유일한 발급 지점이라 const가 아니다
    int32 KeySpace_Min      (int32 Container) const;  // 최소. 비면 0 (호출부에 자기 자신이 있어 안 빈다)

    // ★★ 실패를 반환값으로 돌려주지 않는다 — SortKey는 음수도 0도 유효한 값이다 (13차, 함정 4w)
    bool KeySpace_NextAbove(int32 Container, int32 Key, int32 Exclude, int32& OutKey) const;
    bool KeyOf(int32 EntryId, int32& OutKey) const;

    // ★ 재귀 본체. bRetry를 밖에서 넘길 문법이 없어야 한다 (8차 bIsRoot와 같은 형태)
    void ReorderEntryInternal(int32 EntryId, int32 PrevEntryId, bool bRetry);

    static constexpr int32 SortKeyStep  = 1 << 16;
    static constexpr int32 SortKeyGuard = SortKeyStep * 4;   // 경계까지 남길 여유

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

> **★★ `Owner`는 `TObjectPtr<UEPInventoryComponent>`다 — 11차를 뒤집는다 (15차).**
>
> **11차의 근거가 사실이 아니었다.** 그 문장은 *"구체 타입으로 두면 **선언 순서상 전방선언이 필요해진다**"* 였는데, **`.generated.h`가 그 전방선언을 자동으로 넣어준다.**
>
> ```cpp
> // Intermediate/.../UHT/EPInventoryComponent.generated.h:54   ← 헤더 6번째 줄에서 include된다
> class UEPInventoryComponent;
> ```
>
> **그리고 `UActorComponent`로 둬도 얻는 것이 없다.** 캐스트를 하려면 `Cast<UEPInventoryComponent>`가 `StaticClass()`를 부르므로 **어차피 완전 타입이 필요하고**, 그래서 정의가 `.cpp`로 가는 것은 **양쪽이 똑같다.** 남는 차이는 **캐스트 한 줄뿐**이다.
>
> **Lyra 선례는 유효하지만 우리 조건과 다르다.** `FLyraInventoryList::OwnerComponent`가 `TObjectPtr<UActorComponent>`인 것은 맞다(`LyraInventoryManagerComponent.h:113`). 다만 **우리는 소유자가 `UEPInventoryComponent` 하나로 고정**이고, 리스트를 다른 컴포넌트가 쓸 계획이 문서 어디에도 없다.
>
> **★ 어느 쪽이든 정의는 `.cpp`다.** `FEPInventoryList`가 컴포넌트 **위**에 선언되므로 그 안에서 컴포넌트는 **불완전 타입**이다 — `TObjectPtr<T>` 선언은 전방선언으로 되지만 `Owner->OnInventoryChanged`는 정의를 요구한다. **`FScopedInventoryNotify`를 `.cpp` 상단에 둔 것과 같은 이유다**(13차). `NetDeltaSerialize`도 같이 내린다 — 헤더에 남겨도 컴파일은 되지만 **한 구조체의 정의가 두 파일에 갈리는 것**을 피한다.

> **★★ `Entries.Owner = this;`** — 03-1이 `FEPInventoryList::Owner`를 선언하고 Step 04가 `PostReplicated*` → `Owner->OnInventoryChanged`를 전제하는데, 이 한 줄이 없으면 **콜백이 델리게이트에 닿지 못한다.** 증상은 "서버는 정상인데 클라 인벤토리 UI가 영원히 갱신되지 않는다"이고 원인이 UI나 복제로 보여서 엉뚱한 데를 판다. **Step 04 전체가 이 줄에 걸려 있다.**

> **`EndPlay` 오버라이드가 없다.** 이전 설계는 여기서 엔트리의 핸들을 전부 `Destroy()`해야 했고, 그 때문에 "사망 시 드랍은 반드시 `EndPlay`보다 먼저"라는 순서 규칙이 따라붙었다. 상태가 값이므로 컴포넌트가 파괴되면 같이 사라진다 — **정리할 것도, 지킬 순서도 없다.**

> **`NextEntryId`를 복제하지 않는다.** 클라는 발급하지 않고 받은 `EntryId`를 읽기만 한다. 복제하면 서버 내부 상태가 밖으로 샌다.
> **★ 로드맵 5단계 세이브에는 반드시 넣는다** — `NextEntryId` / `ActiveHotbarIndex` 둘. 장착·배낭은 엔트리의 `SlotId`에 들어 있으므로 배열만 저장하면 따라온다. `NextEntryId`를 빠뜨리면 로드 후 1부터 재발급해 기존 엔트리와 충돌하고, 하필 `ParentEntryId`를 오염시켜 **아이템이 엉뚱한 컨테이너에 들어간다.**

#### `Defs`를 멤버로 캐시하지 않는다

`UEPItemDefinitionSubsystem`은 `UGameInstanceSubsystem`이다. 멤버 포인터로 들고 있으면 00-5가 `FindData()` 반환 포인터의 장기 보관을 금지한 것과 **같은 종류의 질문**("언제까지 유효한가")이 생긴다. 조회는 `TMap` 룩업 한 번이라 비용이 없다.

```cpp
const UEPItemDefinitionSubsystem* UEPInventoryComponent::Defs() const
{
    // ★ 손으로 GameInstance를 거치지 않는다 — 정적 접근자가 이미 있다 (15차)
    //   EPItemDefinitionSubsystem.h:21 — GEngine 널 가드까지 그 안에 들어 있다
    return UEPItemDefinitionSubsystem::Get(this);
}
```

#### `UEPCombatComponent`를 멤버로 두지 않는다 — 헤더 순환

인벤토리가 전투를 부르고(`RemoveEntry` → `UnequipWeapon`) 전투가 인벤토리를 부른다(`UnequipWeapon` → `AddEntryCharges`). 멤버로 두면 **헤더가 서로를 알아야 한다.**

```cpp
// .cpp 안에서만 캐릭터를 경유한다
if (UEPCombatComponent* Combat = GetOwner<AEPCharacter>()->GetCombatComponent())
    Combat->UnequipWeapon();
```

기존 관례와 같다 — `UEPCombatComponent`가 `GetOwnerCharacter()`를 거쳐 `AEPPlayerState`에 닿는 방식(`EPCombatComponent.cpp:173-174`)이 이미 그 모양이다. **두 컴포넌트가 서로를 부르는 것 자체는 문제가 아니다**(전투는 이미 `AEPPlayerState`·`UEPAttributeSet`·`AEPWeapon`을 가로질러 부른다). 문제는 헤더 결합이고, 이 방식이면 생기지 않는다.

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

    // ★★ 순서를 바꾸지 말 것. ①은 반드시 ③보다 앞이다 (함정 4k).
    //     GetEquippedEntryId()는 필드가 아니라 배열을 읽는 파생 게터라, RemoveSelf 뒤에
    //     부르면 INDEX_NONE을 돌려주고 아래 if가 통째로 안 돈다 — write-back이 "덮이는"
    //     것이 아니라 "안 불린다". 잔탄이 버리기 직전 값 그대로 픽업에 실려 대부분의
    //     경우 정답과 구분되지 않고, 틀린 값이 보이는 것은 발사 후 write-back이 밀린
    //     그 한 발뿐이다. 순서를 바꿔도 컴파일된다 — 그래서 주석이 여기 있다.
    //
    // ① write-back 먼저 — 스냅샷도 제거도 반드시 이 뒤다 (아래 ★)
    if (EntryId == GetEquippedEntryId())                // ← 파생값. 배열을 읽는다
    {
        // ★★ 소유자가 캐릭터가 아닐 수 있다 — §7-1 월드 컨테이너가 이 함수를 쓴다 (13차)
        //   지금은 컨테이너의 ActiveHotbarIndex가 INDEX_NONE이라 이 분기에 안 들어와
        //   "우연히" 안 죽는다. §7-1이 오는 날 그 우연을 아무도 기억하지 못한다
        if (AEPCharacter* Ch = GetOwner<AEPCharacter>())
            if (UEPCombatComponent* C = Ch->GetCombatComponent())
                C->UnequipWeapon();                     // write-back만 한다
        // 비울 번호가 없다 — ③의 RemoveSelf가 슬롯을 비운다
    }

    // ② 스냅샷을 자식보다 **먼저** 담는다. 루트는 컨테이너 소속을 끊는다
    if (OutRemoved)
    {
        FEPInventoryEntry Snapshot;
        if (FindEntry(EntryId, Snapshot))
        {
            // ★ bIsRoot가 세 필드를 동시에 관장한다 (11차 → 13차에서 SlotId 추가)
            //   루트는 목적지 컨테이너의 키 체계로 들어가므로 여기서 값을 버린다.
            //   자식은 "새로 만들어질 빈 부모" 안으로 들어가므로 원래 키를 들고 간다
            //   — 그래야 버린 배낭을 되주울 때 내용물 순서가 그대로 살아난다
            //   ★★ SlotId도 버린다. 안 버리면 AddSubtree가 MoveEntry의 정합 검사를
            //      우회해 슬롯을 채운다 — 아래 별도 절 (13차)
            if (bIsRoot)
            {
                Snapshot.ParentEntryId = INDEX_NONE;
                Snapshot.SlotId        = NAME_None;
                Snapshot.SortKey       = 0;
            }
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

#### ★★ 비울 번호가 없다 — 그 대신 ①→③ 순서가 더 강하게 걸린다

**8차까지는 `EquippedEntryId` / `EquippedBackpackEntryId`를 여기서 손으로 비웠다.** 9차 확정으로 두 필드가 사라졌으므로 **비우는 코드 자체가 없다.** 엔트리가 배열에서 빠지면 파생 게터가 자동으로 `INDEX_NONE`을 돌려준다 — **죽은 번호를 남길 문법이 없다.**

**대신 순서가 새 의미를 얻는다.**

| | 8차까지 (저장된 필드) | 9차 이후 (파생 게터) |
|---|---|---|
| `RemoveSelf` 뒤에 장착 번호를 읽으면 | 값이 **살아 있다** | **`INDEX_NONE`** |
| ①과 ③을 뒤바꾸면 | write-back이 `INDEX_NONE`을 향한다 | **write-back이 아예 안 불린다** |
| 증상 | 눈에 보이는 오작동 | **잔탄이 조용히 사라진다** |

**그래서 ①이 ②·③보다 앞이라는 것은 스타일이 아니라 계약이다.** `05_Loot_05_Equipment.md`의 순서 규칙(`:141`)도 근거 문장이 바뀐다 — *"`RemoveEntry`가 먼저 비우면"* 이 아니라 *"`RemoveSelf`가 먼저 돌면 대상을 못 찾는다"* 다.

> **`UnequipWeapon`에 맡기지 않는 이유는 그대로다.** `UnequipWeapon`은 **다른 컴포넌트**(`UEPCombatComponent`)의 함수이고, 교체·사망 경로에서 제거 없이도 불린다. `RemoveEntry`가 그것을 믿을 근거는 없다.

> **★ 이 분기는 Step 03 내내 항상 거짓이다** — `ActiveHotbarIndex`를 세팅하는 경로가 Step 05에 있다. `ensureMsgf`를 넣어도 울릴 일이 없으므로 **방어를 코드로 넣을 자리가 아니라 완료 조건으로 넘길 자리다.** `05_Loot_05_Equipment.md` 완료 조건에 관련 항목을 넣어뒀다.

#### ★★ 네 단계의 순서가 각각 하나씩 막는다

| 단계 | 막는 것 |
|---|---|
| ① write-back → ② 스냅샷 | **잔탄 소실.** 선두에서 뜬 값을 쓰면 write-back 전이라 만탄이 나간다 |
| ② 루트를 먼저, `Parent = INDEX_NONE` **＋ `SlotId = NAME_None` ＋ `SortKey = 0`** | **`AddSubtree`가 못 읽는 배열** ＋ **옛 컨테이너의 키를 들고 가는 것** ＋ **검사 3을 우회한 슬롯 점유.** 아래 계약 |
| ③ 자기 제거를 캐스케이드 **앞**에 | **자기 부모 사이클의 무한 재귀.** `X.Parent == X`인 데이터 오류에서 스택 오버플로 |
| ④ 캐스케이드 마지막 | 자식이 부모의 write-back 결과에 영향받지 않는다 |

#### ★ `RemoveEntry` ↔ `AddSubtree` 계약

**두 함수를 각각 옳게 고쳐도 계약이 어긋나면 둘 다 컴파일되고 배낭 하나짜리 테스트도 통과한다.** 명시한다.

> **`OutRemoved`는 전위 순회다.** `In[0]`이 항상 루트이고, 루트의 `ParentEntryId`는 **`INDEX_NONE`으로 정규화**되며 `SlotId`는 **`NAME_None`으로**, `SortKey`는 **0으로 버려진다**. 자식은 `ParentEntryId` · `SlotId` · `SortKey`를 **전부 원본 그대로 보존**한다.

이 계약이 없으면 `AddSubtree`가 이렇게 깨진다.

- **후위 순회면** 첫 원소(자식)의 부모가 아직 `OldToNew`에 없어 재매핑이 첫 줄에서 실패한다
- **루트 정규화가 없으면** 배낭 **안**에 있던 무기를 버릴 때 `ParentEntryId`가 배낭 번호라 `INDEX_NONE`인 원소가 하나도 없다 → 루트를 못 찾는다. 수납이 착용 컨테이너에서만 나오므로 **거의 모든 경로가 여기다**
- **자식 `SortKey`를 같이 버리면** 배낭을 버렸다 주울 때마다 **내용물 순서가 초기화된다.** 순서를 서버로 옮긴 이유의 절반이 여기다 (11차)
- **루트 `SlotId`를 안 버리면** 매고 있던 배낭을 버렸다 주울 때 `SlotId = "Back"`이 그대로 실려 들어간다 — **`MoveEntry`의 정합 검사(검사 3)와 중복 검사(검사 4)를 통째로 우회한다.** 아래 별도 절 (13차)

#### ★★ 루트 `SlotId`를 버리는 것이 A-2다 (13차 확정)

**버리지 않으면 `AddSubtree`가 슬롯을 채우는 두 번째 경로가 된다.** 그런데 그 경로에는 검사가 없다.

| `AddSubtree`의 목적지 | `SlotId="Back"`을 보존했을 때 | 판정 |
|---|---|---|
| 본체 | `Parent=-1, SlotId="Back"` → 자동으로 매진다 | **우연히 맞다.** 그래서 완료 조건 9가 통과해 버린다 |
| 이미 다른 배낭을 맴 | `"Back"` 슬롯에 **엔트리가 둘** | `GetEntryInSlot`이 먼저 찾은 하나만 돌려준다 → **유령 배낭** |
| 배낭 안 | `Parent=배낭, SlotId="Back"` | **정확히 함정 4i의 상태** — 칸도 안 먹고 착용으로 잡힌다 |

**그리고 동작이 내용물 유무로 갈렸다.** `In.Num() == 1`이면 `AddItem`으로 빠지는데 그쪽은 `SlotId`를 버린다 — **빈 배낭은 안 매지고 내용물이 든 배낭은 매지는** 상태였다.

**결정: 루트는 `SlotId`도 버린다.** 되주울 때 착용 여부는 **그 시점에 슬롯이 비었는가**로 정해지고, 그게 `TryAutoEquip`(03-4 ①단계)의 정의다. 슬롯을 채우는 경로가 `MoveEntry`와 `TryAutoEquip` **둘로 고정되고 둘 다 빈자리를 먼저 확인한다.**

> **안 만드는 확장 하나 — 이름만 남긴다.** *"원래 있던 슬롯을 우선 시도"*(Hotbar2에서 버린 무기가 Hotbar2로 돌아온다)는 `TryAutoEquip(In, FName PreferredSlot)` 한 인자다. **이 결정이 그 확장을 막지 않는다** — 힌트일 뿐 판정은 여전히 `TryAutoEquip`이 한다.
>
> **★ CLAUDE.md §2를 글자 그대로 읽으면 "만들어야 한다"가 된다 — 방금 `DOCS/`에 이름을 적었기 때문이다 (13차 답변).** 그게 그 규칙의 구멍이고, **자기가 이름을 적고 그 이름을 근거로 만들면 규칙이 스스로 부푼다.**
>
> **진짜 판정선은 §2의 바로 아래 두 줄이다** — *"두 번째 **소비자가 예고돼** 있으면"*, *"나중에 넣기 **비싼 것**"*. `PreferredSlot`은 **둘 다 아니다**(소비자 0, 나중 비용 = 인자 하나 ＋ 호출부 하나). **이름을 적는 것은 기록이지 승인이 아니다** — §2의 *"적혀 있으면 만든다"* 는 **기획서·상위 설계에 예고된 것**을 뜻하고, 검수 문서가 방금 적은 이름은 여기 해당하지 않는다.

| 위험 | 막는 형태 |
|---|---|
| write-back 순서 | **`RemoveEntry()`가 제거된 서브트리를 반환한다.** 스냅샷을 얻는 유일한 방법이 제거하는 것이므로 **순서를 뒤집는 게 문법적으로 불가능**해진다 |
| 자식 고아 | **`RemoveEntry()`가 유일한 제거 지점.** 캐스케이드가 자기 자신을 재귀 호출하므로 장착 검사·write-back이 **노드마다** 돈다 |
| 계약 우회 | **`bIsRoot`가 private에만 있다.** 루트 정규화를 건너뛸 문법이 없다 |
| `MarkItemDirty` 누락 | **원시 엔트리를 밖으로 내보내지 않는다.** 수정은 `AddEntryCharges()`로만, 삽입은 `InsertEntry()`로만 |
| 중간 Broadcast | **스코프 가드** (03-7) |

> **★ 장착 검사를 재귀의 각 노드에 두는 이유 (9차에서 근거가 바뀌었다):** 8차까지의 근거는 *"무기가 배낭에 들어가는 일이 흔하다"* 였다. **9차 확정으로 그 시나리오는 표현 불가능해졌다** — 장착은 `SlotId == "HotbarN"` **＋** `ParentEntryId == INDEX_NONE`이므로 장착된 무기는 배낭 안에 있을 수 없다(`EquipmentSlots.md` §6).
>
> **그래도 검사는 각 노드에 둔다.** 사라진 것은 시나리오지 계약이 아니다. ① 착용 컨테이너(상의·외투) 안에 또 컨테이너가 들어가는 구조는 그대로 남고, ② 핫바 5~0으로 **컨테이너 안 아이템을 손에 드는 것**이 허용되면 같은 모양이 그대로 되살아난다(`EquipmentSlots.md` §10 미정 #7). **재귀가 자기 자신을 부르는 구조는 그 결정과 무관하게 옳다.**

> **★ 순회 중 수정 금지가 여기서 실제 문제다.** FastArray 삭제는 `RemoveAtSwap`을 쓴다(`FastArraySerializer.h:1191`). 원본을 순회하며 지우면 인덱스가 뒤에서 앞으로 튀어 **일부 자식을 건너뛴다.** 증상은 "가끔 고아 엔트리가 남는다"이고 재현이 어렵다.

- **원시 엔트리를 밖으로 내보내지 않는다.** 수정은 `SetEntryCharges()` / `AddEntryCharges()`로만 — 그러면 호출자가 `MarkItemDirty`를 잊을 방법이 없다
- Step 05는 "순서를 지켜라"가 아니라 **"`RemoveEntry()`가 보장한다"** 한 줄만 적는다

### ★★ `MoveEntry` — `ParentEntryId` + `SlotId`를 고치는 유일한 지점 (9차 신설)

이 문서는 이미 같은 규칙을 **두 번** 세웠다.

| 무엇 | 유일한 변경 지점 |
|---|---|
| 엔트리 추가 | `InsertEntry` — *"번호 발급·삽입·`MarkItemDirty`의 유일한 지점"* |
| `Charges` | `SetEntryCharges` — *"★ 유일한 쓰기 지점"* |
| 배열에서 빼기 | `RemoveSelf` — *"배열에서 빼고 `MarkArrayDirty`"* |
| `SortKey` | `AssignSortKey` — *"화면 순서를 고치는 유일한 지점"* (11차)<br>★ **`SwapEntries`(04-B)도 이걸 두 번 부른다** — `E.SortKey = Other.SortKey` 직접 대입 금지 |
| **`ParentEntryId` + `SlotId`** | **← 여기가 비어 있었다** |

`SlotId`가 장착의 진실이 되면서 **장착·해제·드래그·부착·컨테이너 간 이동이 전부 같은 연산**이 됐다. 바뀌는 것이 두 필드뿐이기 때문이다.

```cpp
bool MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId);
```

| 동작 | 호출 |
|---|---|
| 배낭 매기 | `MoveEntry(id, INDEX_NONE, "Back")` |
| 무기 장착 | `MoveEntry(id, INDEX_NONE, "Hotbar1")` |
| 해제 | `MoveEntry(id, 상의Id, NAME_None)` |
| 컨테이너 간 이동 | `MoveEntry(id, 배낭Id, NAME_None)` |
| 부착물 달기 | `MoveEntry(id, 총Id, "Optic")` |

**계약이지 편의 함수가 아니다.** 없으면 Step 04의 드래그, Step 05의 장착, §7-3의 부착이 각자 함수를 만들고 **`MarkItemDirty` 호출을 각자 빠뜨린다.** 그리고 8차가 확인한 대로 **살아 있는 원소에 통째로 대입하면** `ReplicationID`가 리셋되어(`FastArraySerializer.h:302-323`) 수신 측에 **삭제＋추가**로 보인다. `MoveEntry`는 **반드시 필드 둘만 고치고 `MarkItemDirty(Item)`** 을 부른다.

> **이 규칙이 대역폭에서 두 번째 값을 한다.** FastArray는 내부 struct 델타가 기본 켜져 있어(`FastArraySerializer.cpp:35`, `.h:218-221`) `SlotId`만 바뀌면 그 프로퍼티만 나간다(커스텀 `FName`은 문자열 경로라 ~16바이트, `CoreNet.cpp:344-360`). **그런데 통째 대입을 하면 삭제＋추가가 되어 전 프로퍼티가 다시 나간다.**

#### 안에 들어가는 검사 일곱

| # | 검사 | 이유 |
|---|---|---|
| **0** | **목적지가 지금 자리와 같은가** (`ParentEntryId == NewParent && SlotId == NewSlotId`) → **`false`** | ★ 아래 |
| 1 | `ContainsEntry(EntryId)` | 조작된 요청 |
| 2 | `NewSlotId`가 그 아이템의 `SlotPriority`에 있는가 | `NAME_None`이면 통과 |
| 3 | **`SlotId` ↔ `ParentEntryId` 정합** | ★ 아래 |
| 4 | `GetEntryInSlot(NewParent, NewSlotId)`가 비었는가 | 차 있으면 교체가 아니라 **실패** |
| | ↑ **2·3·4는 `CanPlaceInSlot(NewParent, NewSlotId, ItemId)` 하나다** (13차) | 아래 별도 절 |
| 5 | `NewSlotId == NAME_None`이면 `CanFit(NewParent, ItemId)` | 옮겨갈 컨테이너에 자리가 있나 |
| 6 | `NewParent`가 자기 자손이 아닌가 | **사이클 금지** ★★ |

**검사 0 — 제자리 이동은 검사 5에서 자기 크기를 두 번 센다.** 엔트리가 이미 `NewParent` 안에 있고 `SlotId`도 `None`이면, 검사 5의 `CanFit(NewParent, ItemId)` = `GetUsedSlots(NewParent) + SlotSize <= Capacity`에서 **`GetUsedSlots`가 이미 그 크기를 포함하고 있다.**

```
배낭 18/20의 AK(4)에 대해 MoveEntry(AK, 배낭, None)  →  18 + 4 = 22 > 20  →  거절
```

**증상은 *"가방이 좀 차면 제자리 이동이 실패한다"*, 그리고 널널할 때는 성공한다.** Step 04에서 이걸 부르는 경로는 없다(같은 컨테이너 안 자리 바꾸기는 `Server_ReorderEntry`로 간다 — `05_Loot_04_InventoryUI.md` 04-7·04-8), **`Server_MoveEntry`는 열려 있고 조작된 클라이언트는 같은 요청을 만들 수 있다.** 한 줄로 닫는다. 그리고 이 검사는 *"목적지가 같으면 할 일이 없다"* 라는 **계약 자체로도 맞다** — 무의미한 `MarkItemDirty`가 나가지 않는다.

**검사 3 — 정합 불변식.** `SlotId`가 진실이 되면 *표현할 수 있지만 무의미한 상태*가 생긴다.

```
상의   ParentEntryId = 배낭Id,  SlotId = "Torso"     ← "가방 안에 든 상의를 입고 있다"
```

이 상태는 `GetUsedSlots`에서 **칸을 안 먹고**(`SlotId != None`) `GetEntryInSlot`에서 **입은 것으로 잡힌다.** 배낭에 상의를 넣어두면 칸도 안 먹고 착용 효과도 받는다. 규칙은 두 줄이다.

```
장비 슬롯 (핫바 1~4 + 착용 8)      →  ParentEntryId == INDEX_NONE
부착 슬롯 (Optic/Muzzle/Grip/Mag)  →  ParentEntryId == 그 무기 엔트리
```

*"이 슬롯 이름이 장비인가 부착인가"* 를 아는 곳이 없으므로 **목록을 하나 만든다.** `SlotPriority`는 답하지 못한다 — 아이템이 *들어갈 수 있는* 곳의 목록일 뿐이다.

```cpp
// UEPLootDeveloperSettings (Step 00에서 생성됨) — 여기서 확장. ★ 임시 자리다 (§8 미정 #10)
UPROPERTY(config, EditAnywhere, Category = "Inventory")
TArray<FName> BodySlots;      // 핫바 1~4 + 착용 8 = 12개
```

> **★★ 아래는 규칙의 모양이지 칠 코드가 아니다 (14차).** 13차가 검사 2·3·4를 **`CanPlaceInSlot`으로 뽑았으므로** 실제 코드는 그 함수 안에 있다 — 아래 별도 절. **`MoveEntry`에 들어가는 것은 한 줄뿐이다.**
>
> ```cpp
> if (!CanPlaceInSlot(NewParent, NewSlotId, Cur.ItemId)) return false;   // 검사 2·3·4
> ```
>
> 여기 그대로 인라인으로 치면 **검사 3이 두 번 돌고**(무해) **검사 2·4가 빠진다** — `AddSubtree`·§7-3의 우회 경로(A-2)가 되살아난다. 변수 이름으로 구별한다: 아래는 `NewParent`/`NewSlotId`(**`MoveEntry`의 파라미터**), `CanPlaceInSlot`은 `Parent`/`SlotId`다.

```cpp
// ⚠ 설명용 — 실제 코드는 CanPlaceInSlot 안에 있다 (아래 별도 절)
const bool bIsBodySlot = GetDefault<UEPLootDeveloperSettings>()->BodySlots.Contains(NewSlotId);

if (bIsBodySlot && NewParent != INDEX_NONE)
    return false;                                   // 가방 안에서 입을 수 없다

if (!bIsBodySlot && !NewSlotId.IsNone())
{
    // 부착이다 — 부모 무기가 그 슬롯을 갖고 있어야 한다 (§7-3에서 활성화)
    const UEPWeaponDefinition* W = GetWeaponDefOf(NewParent);
    if (!W || !W->AttachmentSlots.Contains(NewSlotId)) return false;
}
```

> **전역에서 읽어야 하는 이유:** 소비자가 둘이다 — 이 검사와 **Step 04 UI의 슬롯 그리기.** UI에는 물어볼 인벤토리 인스턴스가 없을 수도 있다. `AttachmentSlots`(무기 쪽)는 소비자가 위 갈래 하나뿐이고 Step 03·04에서 도달 불가라 **§7-3으로 미룬다.**
>
> **★★ `UEPLootDeveloperSettings`는 임시 자리다 (14차). 6차를 근거로 인용하지 않는다.** 위 근거는 *"전역에서 닿아야 한다"* 까지만 말하고 **`UDeveloperSettings`를 특정하지 않는다** — DataAsset도 태그 레지스트리도 전역에서 닿는다. 그리고 6차의 문장은 *"전역 **데이터 참조**"* 이지 *"전역 데이터"* 가 아니다(`ItemDataTable`·`PickupClass`처럼 **가리키는 것**). **최종 자리는 `UEPPawnInventoryData`(DataAsset)** — `05_Loot_DOCS.md` §8 미정 #10이 `ContainerOrder`·`StartingEquipment`와 함께 관장한다.
>
> **그리고 이 검사가 묻는 질문이 반쪽이다.**
>
> ```
> "Torso"라는 이름이 몸 슬롯인가          ← 전역. BodySlots가 답한다
> 이 소유자에게 Torso가 있는가            ← 소유자별. 전역 싱글턴은 못 답한다
> ```
>
> **검사 3은 뒤를 물어야 하는데 앞을 묻는다.** 어긋남이 §7-1에서 드러난다 — 나무 상자의 `UEPInventoryComponent`에 `MoveEntry(상의, INDEX_NONE, "Torso")`를 보내면 **검사 3(`bIsBodySlot` ✓ `Parent == INDEX_NONE` ✓)도 검사 5(슬롯이라 건너뜀)도 통과해 상자가 상의를 입는다.** 지금은 상자의 `MoveEntry`를 부르는 표면이 없어 **우연히** 도달 불가이고, 13차가 잡은 `GetOwner<AEPCharacter>()` 무보호 역참조와 같은 종류다.
>
> **그래도 지금 옮기지 않는다** — 읽는 곳이 이 검사와 04 UI 둘뿐이라 이전이 두 줄이고, 지금 DataAsset을 만들면 **소비자가 하나인 계층**이 는다(CLAUDE.md §2). **옮길 트리거는 로비 아니면 §7-1이다.**
>
> > **Lyra는 슬롯 목록을 `UDeveloperSettings`에 넣지 않는다.** `ULyraDeveloperSettings`·`ULyraCosmeticDeveloperSettings`·`ULyraWeaponDebugSettings`가 **전부 `config=EditorPerProjectUserSettings`**(`LyraDeveloperSettings.h:39` 등)로 개발·디버그 전용이고, 게임플레이 구성은 `ULyraPawnData : UPrimaryDataAsset`(`LyraPawnData.h:24-53`)이 든다. **슬롯 개수조차 컴포넌트 필드다** — `ULyraQuickBarComponent::NumSlots`(`LyraQuickBarComponent.h:63-64`).

**검사 6 — 사이클 검사는 "도달 불가 분기의 에러 처리"가 아니다.** `RemoveEntry`의 재귀에는 무한 재귀를 막는 장치가 없고, 안전한 이유가 이 문서에 명시돼 있다 — *"부모가 이미 배열에서 빠져 사이클이 성립하지 않는다"*(④). **그 문장은 "입력이 트리다"라는 전제 위에 서 있고, `MoveEntry`는 그 전제를 깰 수 있는 유일한 함수다.** `InsertEntry`는 새 노드만 만들고 `RemoveEntry`는 노드를 없앤다.

```cpp
// 5줄. NewParent에서 위로 걸어 올라가 자기를 만나면 사이클이다
// ★ 지역 변수 이름을 E로 쓰지 않는다 — 아래 쓰기 블록의 E와 뜻이 다르다 (13차, 함정 4v)
for (int32 P = NewParent; P != INDEX_NONE; )
{
    if (P == EntryId) return false;
    FEPInventoryEntry Cur;
    P = FindEntry(P, Cur) ? Cur.ParentEntryId : INDEX_NONE;
}
```

> **안 넣었을 때의 증상은 예외도 로그도 아니라 전용 서버 프로세스가 멈추는 것이다.** 5줄과 그것을 맞바꾸지 않는다. CLAUDE.md §2의 *"나중에 넣기 비싼 것 — 계약"* 에 해당한다.

#### ★★ 검사 2·3·4는 `CanPlaceInSlot` 하나다 — `AddSubtree`가 그 셋을 안 돈다 (13차 신설)

13차가 세운 문장은 이것이었다.

> *"슬롯을 채우는 경로가 `MoveEntry`와 `TryAutoEquip` **둘로 고정되고 둘 다 빈자리를 먼저 확인한다**."*

**API 수준에서는 거짓이다.** `AddSubtree`는 **public**이고 `SlotId`를 받아 `InsertEntry`에 그대로 넘기며, **검사 2·3·4를 하나도 안 한다.** 빈자리 확인은 `TryAutoEquip`이라는 **호출자**에 있다.

**그리고 이 문서가 이미 세 번째 호출자를 예고했다.**

```cpp
AddSubtree(총Id, "Optic", Payload);      // §7-3 부착물 — TryAutoEquip이 아니다
```

이 호출은 *"그 무기에 `Optic` 슬롯이 있는가"*(검사 3)도 *"이미 조준경이 달렸는가"*(검사 4)도 안 본다. **A-2가 막은 우회 경로가 §7-3에서 문법 그대로 되살아난다.** 호출자가 보증하는 계약은 **호출자가 늘어나면 깨진다.**

```cpp
// 셋 다 (Parent, SlotId, ItemId)의 함수다 — 옮기는 엔트리를 보지 않는다
bool UEPInventoryComponent::CanPlaceInSlot(int32 Parent, FName SlotId, FName ItemId) const
{
    if (SlotId.IsNone()) return true;                       // 수납은 이 함수의 일이 아니다

    // 검사 2 — 그 아이템이 이 슬롯에 갈 자격이 있나
    const FEPItemData* Data = Defs() ? Defs()->FindData(ItemId) : nullptr;
    if (!Data || !Data->SlotPriority.Contains(SlotId)) return false;

    // 검사 3 — 몸 슬롯이면 부모가 본체
    // ★★ 지금 치는 것은 두 줄이다. else 갈래는 §7-3의 것이다 — 아래
    if (!GetDefault<UEPLootDeveloperSettings>()->BodySlots.Contains(SlotId)) return false;
    if (Parent != INDEX_NONE) return false;

    // 검사 4 — 그 자리가 비어 있나
    return GetEntryInSlot(Parent, SlotId) == INDEX_NONE;
}
```

> ### ★★ 부착 슬롯 갈래는 **지금 치지 않는다** (15차 — 실제로 컴파일 에러가 났다)
>
> 13차 초안은 검사 3을 `bIsBodySlot`으로 갈라 `else`에 부착 슬롯을 적고 `// §7-3에서 활성화`를 달았다. **그 주석이 *"지금은 안 친다"* 라는 뜻인 것이 안 읽혔고, 그대로 치면 컴파일되지 않는다.**
>
> ```
> C2039 'SlotPriority': 'FEPItemData'의 멤버가 아닙니다      ← 03-A가 아직 안 넣었다 (체크포인트 표)
> C3861 'GetWeaponDefOf': 식별자를 찾을 수 없습니다           ← 그런 함수가 없다
> C2027 정의되지 않은 형식 'UEPWeaponDefinition'              ← AttachmentSlots 필드도 없다
> ```
>
> **`AttachmentSlots`도 `GetWeaponDefOf`도 존재하지 않고, `DT_Items`에 부착 슬롯을 가진 행이 0개다.** 만들면 **소비자가 없는 필드 둘 ＋ 함수 하나**가 생긴다 — CLAUDE.md §2가 금지하는 *"상상한 확장점"* 이다.
>
> **그리고 동작이 같다.** 지금은 부착 슬롯이 존재하지 않으므로 *"`BodySlots`에 없으면 거절"* 이 **정확한 판정**이다. §7-3이 오면 `return false` 한 줄을 `else { … }`로 벌리는 것이 전부이고, **`CanPlaceInSlot`이 유일한 진입점이라 그 자리도 하나다.**
>
> **함정 3e와 같은 부류다** — 설명용·미래용 블록을 그대로 인라인으로 치는 것. 이번에는 컴파일이 막아줬지만 3e는 **컴파일된다**는 점만 다르다.

| 부르는 곳 | 어떻게 |
|---|---|
| `MoveEntry` | `if (!CanPlaceInSlot(NewParent, NewSlotId, Cur.ItemId)) return false;` |
| **`AddSubtree`** | `if (!CanPlaceInSlot(Parent, SlotId, In[0].ItemId)) return INDEX_NONE;` |
| §7-3 부착물 | **자동으로 옳다** |

**새 계층이 아니라 이 문서가 이미 쓴 패턴의 세 번째 적용이다.** `CanFit`에 대해 정확히 같은 말을 했다 — *"칸 판정을 `AddItem`에 인라인으로 다시 쓰지 않는다. 판정식이 세 곳에 흩어지면 **반드시 어긋난다**"*(03-3). 그리고 `AddSubtree`가 *"칸 검사 분기가 `MoveEntry` 검사 5와 **한 글자도 다르지 않다**"* 고 적었는데, **한 글자도 다르지 않은 것을 두 번 쓰는 것이 바로 그 금지 대상이다.**

> **★ 네 번째 인자가 Step 04에서 붙는다 — `IgnoreEntryId` (15차).** `SwapEntries`(04-7)는 **상대가 차지한 슬롯**으로 들어가므로 검사 4의 `GetEntryInSlot(Parent, SlotId) == INDEX_NONE`이 **언제나 거짓**이다. 그대로 두면 슬롯이 걸린 교환이 하나도 성립하지 않는다. 해법은 *"지금 나가는 중인 엔트리는 세지 않는다"* 이고, **검사 0이 막은 것과 같은 종류**다.
>
> ```cpp
> bool CanPlaceInSlot(int32 Parent, FName SlotId, FName ItemId, int32 IgnoreEntryId = INDEX_NONE) const;
> ```
>
> **여기서 미리 붙이지 않는다** — 기본값이 `INDEX_NONE`이라 지금 호출자 둘은 한 글자도 안 바뀌고, `Server_MoveEntry`(9차)·`Server_ReorderEntry`(11차)·`Server_EquipBackpack`(14차)에 세 번 적용한 규칙(*"검증 표면은 소비자를 따라간다"*)이 그대로다. 설계는 `05_Loot_04_InventoryUI.md` 04-7.

> **`CanFit`과 합치지 않는다.** `CanFit`은 *"수납일 때"*, `CanPlaceInSlot`은 *"슬롯일 때"* 로 **배타적**이다. 하나로 묶으면 `SlotId`의 `None` 여부로 갈리는 분기가 판정 함수 **안으로** 들어가고, 그건 12차가 `KeySpace_` 접두어로 막은 것과 같은 종류의 혼동을 만든다.

> **★ 검사 0이 검사 4보다 앞인 것은 계약이다 (13차).** 엔트리가 **이미 그 슬롯에 있으면** 검사 4의 `GetEntryInSlot`이 **자기 자신**을 찾아 *"차 있다"* 로 거절한다. 검사 0(제자리면 `false`)이 먼저라 그 갈래에 도달하지 않는다. 순서를 바꾸면 컴파일되고 증상은 *"제자리 이동이 실패한다"* 로 함정 4l과 구분되지 않는다.

#### ★★ 검사를 다 통과한 뒤 — 부모가 바뀌었으면 `SortKey`를 재발급한다 (11차)

```cpp
// 전부 통과한 뒤에 쓴다 (검사 도중 쓰기 금지 — 05_Loot_04_InventoryUI.md 함정 11b)
// ★★ 배열 원소의 **참조**를 잡는다. FindEntry는 값 복사라 그 결과에 쓰면
//    컴파일되고 true를 반환하는데 배열은 그대로다 (13차, 함정 4v)
//    SetEntryCharges(03-3)가 이미 이 형태다 — 여기만 빠져 있었다
FEPInventoryEntry Cur;
if (!FindEntry(EntryId, Cur)) return false;   // 검사 1에서 이미 봤다
const int32 OldParent = Cur.ParentEntryId;

// ★★ 키를 **재부모 전에** 구한다 (13차, 함정 4x)
//   부모를 먼저 바꾸면 자기 자신이 **옛 키를 든 채** 목적지의 형제로 잡힌다 —
//   InsertEntry가 AddDefaulted 뒤에 발급하던 것과 **글자 그대로 같은 결함**이다
const bool  bReparent = (NewParent != OldParent);
const int32 NewKey    = bReparent ? KeySpace_NextAtEnd(NewParent) : 0;

FScopedInventoryNotify Guard(this);           // ★ MarkItemDirty를 직접 부르므로 가드가 필요하다 (03-7)

for (FEPInventoryEntry& E : Entries.Items)
{
    if (E.EntryId != EntryId) continue;

    E.ParentEntryId = NewParent;
    E.SlotId        = NewSlotId;
    Entries.MarkItemDirty(E);

    if (bReparent) AssignSortKey(EntryId, NewKey);   // 새 컨테이너 맨 뒤
    return true;
}
return false;
```

> **★★ 순서를 바꾸면 키가 컨테이너 사이로 전염된다 (13차 검수).** `KeySpace_NextAtEnd`는 *"부모가 같은 것 전부"* 를 돌므로, 재부모를 먼저 하면 **방금 옮겨온 자기 자신을 옛 컨테이너의 키와 함께 센다.**
>
> ```
> 본체:  … 붕대 1,000,000        (오래 쓴 컨테이너)
> 배낭:  칫솔 0   가위 65536      (새 컨테이너)
>
> 붕대를 배낭으로 옮긴다  →  배낭 안에서 1,065,536      ← 목적지와 무관한 값
> ```
>
> **순서(맨 뒤)는 안 깨진다** — 어느 경우든 목적지 최대보다 크다(함정 4m은 지켜진다). **깨지는 것은 키 공간이다.** 컨테이너를 오갈 때마다 큰 쪽의 키 크기가 작은 쪽으로 옮고, 재정규화 빈도가 이유 없이 올라가며 **함정 4x가 살려낸 *"빈 컨테이너면 0"* 분기가 이 경로에서 다시 죽는다.**

> **★★ `E`가 어디서 오는지가 이 함수의 검수 항목이다 (13차).** 초안은 `E`를 선언 없이 썼고, 바로 위 검사 6이 `FEPInventoryEntry E;`로 **지역 복사본**을 같은 이름으로 만들고 있었다. 그대로 이어 붙이면 `E`는 **부모 사슬을 걷다 마지막으로 방문한 조상의 복사본**이고, `MarkItemDirty(E)`는 **배열 밖 임시 객체**를 건드린다.
>
> **증상:** 배낭을 매도 아무 일이 없고 무기를 꽂아도 안 꽂히는데 **반환값은 `true`다.** `Dump`의 `Parent`/`SlotId` 열이 안 바뀌어서 *"`EP.Inv.Move`가 안 먹는다"* 로 오진하고 커맨드 파싱 쪽을 판다.

**빠뜨리면 옛 컨테이너의 키를 들고 새 컨테이너에 들어간다.** 본체가 `65536, 131072`를 쓰고 배낭이 `65536, 131072, 196608`을 쓰는 중이라면, 본체의 첫 아이템을 배낭으로 옮겼을 때 **배낭 맨 앞에 꽂힌다.** 플레이어는 맨 뒤에 붙기를 기대한다.

- **컴파일된다.** 그리고 두 컨테이너의 키 범위가 겹칠 때만 보인다 — 겹치는 게 정상이므로 거의 항상 보이지만, *"가끔 이상한 자리에 들어간다"* 로 인지된다
- **★★ `NewSlotId`는 보지 않는다 (11차 검수).** 초안은 *"슬롯으로 갈 때는 재발급하지 않는다"* 는 예외를 뒀는데, **그게 동률의 원인이었다**(함정 4q). 조건은 **부모가 바뀌었는가** 하나다
- **부모가 그대로면 건드리지 않는다.** 같은 컨테이너 안 자리 바꾸기는 `MoveEntry`가 아니라 `ReorderEntry`의 일이다 (아래)
  > **부수 효과가 좋다** — 무기를 핫바에 꽂았다 빼면 `MoveEntry(id, -1, "Hotbar1")` → `MoveEntry(id, -1, None)` 둘 다 부모가 안 바뀌므로 **키를 계속 들고 있다.** 그래서 **원래 자리로 돌아온다.** 예외를 두면 맨 뒤로 간다

---

#### ★★ 조립된 전체 본문 (15차 신설)

**위까지가 조각이다.** 검사 0·1·5는 표에만 있고 코드가 없었고, 검사 6과 쓰기 블록은 각자 다른 절에 있었다 — **구현자가 다섯 군데를 꿰매야 했다.** 03-A-부록을 만든 것과 같은 이유로 여기 한 벌을 둔다.

```cpp
bool UEPInventoryComponent::MoveEntry(int32 EntryId, int32 NewParent, FName NewSlotId)
{
    if (!GetOwner()->HasAuthority()) return false;

    // ── 검사 1 — 조작된 요청 ───────────────────────────────────────
    if (!ContainsEntry(EntryId)) return false;

    FEPInventoryEntry Cur;
    if (!FindEntry(EntryId, Cur)) return false;         // 검사 1에서 이미 봤다

    // ── 검사 0 — 제자리면 할 일이 없다 ★ 검사 4보다 반드시 앞이다 ──
    //   뒤에 두면 검사 4의 GetEntryInSlot이 **자기 자신**을 찾아 거절한다 (13차)
    //   그리고 검사 5에서 자기 크기를 두 번 센다 — "가방이 좀 차면 제자리 이동이 실패한다"
    if (Cur.ParentEntryId == NewParent && Cur.SlotId == NewSlotId) return false;

    // ── 검사 2·3·4 — 슬롯 배치 가능한가 (한 줄이다) ────────────────
    //   ★ 이 아래를 인라인으로 풀어 쓰지 않는다 — 검사 2·4가 빠진다 (함정 3e)
    if (!CanPlaceInSlot(NewParent, NewSlotId, Cur.ItemId)) return false;

    // ── 검사 5 — 수납이면 목적지에 자리가 있나 ─────────────────────
    //   슬롯이면 건너뛴다. 슬롯에 든 것은 칸을 안 먹는다 (함정 4y)
    if (NewSlotId.IsNone() && !CanFit(NewParent, Cur.ItemId)) return false;

    // ── 검사 6 — 사이클 금지 ───────────────────────────────────────
    //   ★ 지역 변수를 E로 쓰지 않는다. 아래 쓰기 블록의 E와 뜻이 다르다 (함정 4v)
    for (int32 P = NewParent; P != INDEX_NONE; )
    {
        if (P == EntryId) return false;
        FEPInventoryEntry Up;
        P = FindEntry(P, Up) ? Up.ParentEntryId : INDEX_NONE;
    }

    // ── 여기부터 쓰기. 검사 도중에는 한 필드도 안 썼다 ─────────────
    //   ★★ 키를 **재부모 전에** 구한다 (함정 4x). 부모를 먼저 바꾸면
    //      자기 자신이 옛 키를 든 채 목적지의 형제로 잡힌다
    const bool  bReparent = (NewParent != Cur.ParentEntryId);
    const int32 NewKey    = bReparent ? KeySpace_NextAtEnd(NewParent) : 0;

    FScopedInventoryNotify Guard(this);      // ★ MarkItemDirty를 직접 부른다 (03-7)

    for (FEPInventoryEntry& E : Entries.Items)   // ★★ 참조다. Cur은 복사본이라 못 쓴다 (함정 4v)
    {
        if (E.EntryId != EntryId) continue;

        E.ParentEntryId = NewParent;
        E.SlotId        = NewSlotId;             // ★ 필드 둘만. 통째 대입 금지 (함정 4g)
        Entries.MarkItemDirty(E);

        if (bReparent) AssignSortKey(EntryId, NewKey);   // 새 컨테이너 맨 뒤
        return true;
    }
    return false;
}
```

**순서가 전부 계약이다.** 어느 것도 스타일이 아니다.

| 순서 | 어기면 |
|---|---|
| **검사 0이 검사 4보다 앞** | 제자리 이동에서 검사 4가 **자기 자신**을 찾아 거절한다. 증상이 함정 4l과 구별되지 않는다 |
| **검사 0이 검사 5보다 앞** | `CanFit`이 자기 크기를 두 번 센다 → *"가방이 좀 차면 제자리 이동이 실패한다"* |
| **검사 6이 쓰기보다 앞** | 사이클이 생기면 `RemoveEntry` 재귀가 **전용 서버 프로세스를 멈춘다** |
| **키 계산이 재부모보다 앞** | 옛 컨테이너의 키가 목적지로 전염된다 (함정 4x) |
| **쓰기가 검사 전부보다 뒤** | 한쪽만 바뀐 중간 상태가 남는다 (`05_Loot_04_InventoryUI.md` 함정 11b) |

> **★ `Cur`을 두 번 쓰는 것에 주의한다.** 검사에서는 `Cur.ItemId` · `Cur.ParentEntryId`를 읽지만, **쓰기는 반드시 `Entries.Items`의 참조 `E`에 한다.** `Cur`에 쓰면 컴파일되고 `true`를 반환하는데 **배열은 그대로다** — 13차가 잡은 함정 4v가 정확히 이 함수에서 났다.

> **★ 반환값을 버리지 않는다.** `EP.Inv.Move`가 이 `bool`을 로그로 찍어야 검사 0~6 중 무엇에 걸렸는지는 고사하고 **걸렸다는 사실**이 보인다 (03-9).

---

### ★★ `ReorderEntry` — 같은 컨테이너 안 자리 바꾸기 (11차 신설)

`MoveEntry`와 **의도적으로 다른 함수다.** 바꾸는 필드가 다르고(`SortKey` vs `ParentEntryId`+`SlotId`), 무엇보다 **실패할 수 있는 조건이 다르다.**

| | `MoveEntry` | `ReorderEntry` |
|---|---|---|
| 용량 판정 | **한다** (검사 5) | **하지 않는다** — 컨테이너를 안 떠난다 |
| 슬롯 정합 | 한다 (검사 2·3·4) | 해당 없음 |
| 사이클 | 한다 (검사 6) | 해당 없음 |
| **정상 클라에서 실패하나** | **한다** (가방이 차면) | **안 한다** |

마지막 줄이 설계를 가른다 — **재배치는 실패할 수 없는 연산**이라 클라가 낙관적으로 먼저 그려도 되돌릴 일이 없다(04-7).

```cpp
// ★ 일반 함수다. RPC 표면(Server_ReorderEntry)은 Step 04-B에서 연다 — 아래 ★★
void UEPInventoryComponent::ReorderEntry(int32 EntryId, int32 PrevEntryId)
{
    ReorderEntryInternal(EntryId, PrevEntryId, /*bRetry=*/false);
}

// ★★ 재귀 본체. bRetry를 밖에서 넘길 문법이 없어야 "한 번만 재귀한다"가 지켜진다 (12차 검수)
//   8차가 RemoveEntry/RemoveEntryInternal에 bIsRoot로 한 것과 같은 형태다
void UEPInventoryComponent::ReorderEntryInternal(int32 EntryId, int32 PrevEntryId, bool bRetry)
{
    if (!GetOwner()->HasAuthority()) return;

    FEPInventoryEntry E;
    if (!FindEntry(EntryId, E)) return;             // 조작된 요청
    if (!E.SlotId.IsNone())     return;             // 슬롯에 든 것은 순서가 없다

    const int32 Container = E.ParentEntryId;

    // ★ Prev는 "같은 컨테이너의 수납 형제"여야 한다. 이 검사가 전부다
    if (PrevEntryId != INDEX_NONE)
    {
        FEPInventoryEntry P;
        if (!FindEntry(PrevEntryId, P))                        return;
        if (P.ParentEntryId != Container || !P.SlotId.IsNone()) return;
        if (PrevEntryId == EntryId)                            return;   // 자기 뒤로
    }

    // ★★ Prev는 "보이는 목록"에서 오지만(사용자 의도), 틈은 "부모 전체"에서 구한다 (12차 대기)
    //   GetSortedContents로 다음 키를 구하면 그 사이에 낀 슬롯 형제와 동률이 난다 — 아래 ★★
    int32 PrevKey = KeySpace_Min(Container) - SortKeyStep;   // 맨 앞: 부모 전체의 최소보다 한 칸 앞
    if (PrevEntryId != INDEX_NONE && !KeyOf(PrevEntryId, PrevKey)) return;   // 위에서 존재는 확인했다

    // PrevKey보다 큰 것 중 가장 작은 키 — 자기 자신은 제외한다
    // ★★ 성패를 bool로 받는다. INDEX_NONE을 센티널로 쓰면 키 −1과 충돌한다 (13차, 함정 4w)
    int32 NextKey = 0;
    const bool bTail = !KeySpace_NextAbove(Container, PrevKey, /*Exclude=*/EntryId, NextKey);

    int32 NewKey;
    if (PrevEntryId == INDEX_NONE) NewKey = PrevKey;                     // 맨 앞
    else if (bTail)                NewKey = PrevKey + SortKeyStep;       // 맨 뒤
    else                           NewKey = PrevKey + (NextKey - PrevKey) / 2;   // 사이

    // ★★ 두 판정을 한 식에 묶지 않는다 (12차 검수). 성격이 다르다
    //   경계 = "자릿수가 없다"  /  고갈 = "틈이 없다"
    const bool bOutOfRange = (NewKey <= MIN_int32 + SortKeyGuard)
                          || (NewKey >= MAX_int32 - SortKeyGuard);

    // ★★★ PrevEntryId != INDEX_NONE 을 빠뜨리면 맨 앞 드래그가 무한 재귀한다.
    //   맨 앞 분기는 설계상 NewKey == PrevKey 라서 고갈 판정을 적용하면 안 된다 (함정 4t)
    const bool bNoGap = (PrevEntryId != INDEX_NONE) && !bTail && (NewKey <= PrevKey);

    if (bOutOfRange || bNoGap)
    {
        // 재정규화 뒤에도 걸리면 종료 조건이 깨진 것이다 — 조용히 도는 것보다 낫다
        if (!ensureMsgf(!bRetry, TEXT("[Inventory] ReorderEntry: 재정규화 후에도 자리가 없다")))
            return;

        RenormalizeSortKeys(Container);
        ReorderEntryInternal(EntryId, PrevEntryId, /*bRetry=*/true);
        return;
    }

    // ★ 결과가 지금 자리와 같으면 할 일이 없다 — MoveEntry 검사 0과 대칭 (12차 검수)
    //   드래그 취소는 "도로 놓기"라 가장 흔하고, 그때마다 틈이 반으로 준다
    {
        const TArray<int32> Cur = GetSortedContents(Container);
        const int32 MyIdx   = Cur.Find(EntryId);
        const int32 CurPrev = (MyIdx <= 0) ? INDEX_NONE : Cur[MyIdx - 1];
        if (CurPrev == PrevEntryId) return;
    }

    AssignSortKey(EntryId, NewKey);
}
```

#### ★★ 실패 센티널로 `INDEX_NONE`을 쓰지 않는다 — `SortKey`는 −1도 유효하다 (13차 신설)

**`EntryId`와 `SortKey`가 다른 종류의 정수라는 것이 여기서 처음 문제가 된다.**

| | 값 범위 | `INDEX_NONE`을 센티널로 쓸 수 있나 |
|---|---|---|
| `EntryId` | **1부터** 단조 증가 | ✅ 0도 −1도 절대 안 나온다 |
| `SortKey` | **음수를 포함한 `int32` 전체** | ❌ **−1이 도달 가능한 값이다** |

맨 앞 이동이 `KeySpace_Min - SortKeyStep`이라 키가 음수로 내려가고(함정 4r가 인정한 성질), **`(−65536, 0)` 구간에 연속으로 꽂으면 정확히 −1에 닿는다.**

```
−32768, −16384, −8192, −4096, −2048, −1024, −512, −256, −128, −64, −32, −16, −8, −4, −2, −1
                                                                                      ↑ 16회째
```

**완료 조건 18의 ①(*"같은 틈에 16회"*)이 그 구간에서 돌면 만들어진다.**

키가 −1인 형제가 생기면 `KeySpace_NextAbove`가 그를 **찾아도** 호출자는 *"다음이 없다"* 로 읽는다 → `bTail = true` → `NewKey = PrevKey + SortKeyStep`. **바로 뒤에 놓으라고 했는데 한 칸 건너뛴 자리에 놓이고**, 거기 이미 키가 있으면 **동률**이 난다. 그리고 `bNoGap`이 `bTail`일 때 꺼지므로 **재정규화도 안 걸린다.**

`KeyOf`의 *"없으면 0"* 도 같은 모양이다 — 0은 재정규화 직후 **첫 형제의 키**다. 지금은 모든 호출부가 앞에서 `FindEntry`로 존재를 확인해 도달하지 않지만, **네 번째 읽기 지점이 생기면 조용히 틀린다.**

> **`INDEX_NONE`은 truthy다**(03-4)와 같은 종류인데, 그쪽은 *"−1을 참으로 읽는다"* 이고 이쪽은 *"−1을 없음으로 읽는다"* 다. **둘 다 값과 실패를 한 채널에 실은 대가다.** `EntryId`를 돌려주는 함수들은 그래도 되고, **`SortKey`를 돌려주는 함수들은 안 된다.**

#### ★★ 틈은 "보이는 목록"이 아니라 "부모 전체"에서 구한다 (12차 검수 대기)

**11차 검수가 `KeySpace_NextAtEnd`·`RenormalizeSortKeys`의 스코프를 고쳤는데, `ReorderEntry` 자신은 그대로였다.** 같은 결함이 남는다.

```
본체:  붕대 0       AK 65536 (Hotbar1 슬롯)      구급상자 131072
       GetSortedContents(-1) = [붕대, 구급상자]     ← AK는 안 보인다

"붕대 뒤"에 X를 놓는다
  초안:  PrevKey=0, 다음은 구급상자(131072)  →  0 + (131072-0)/2 = 65536  ← ★ AK와 동률
  수정:  PrevKey=0, 부모 전체의 다음은 AK(65536)  →  0 + 65536/2 = 32768   ← 안전
```

**세 분기 전부 같은 문제였다** — 맨 앞도 `Sorted[0] - Step`이 그 아래 슬롯 형제와 겹칠 수 있고, 맨 뒤도 `PrevKey + Step`이 위쪽 슬롯 형제와 겹칠 수 있다.

**규칙 한 줄:** `Prev`는 **사용자가 본 목록**에서 오고(그래서 슬롯이 아닌 것만 유효하다 — 위 검사), **틈은 부모 전체에서** 구한다.

#### ★★ 두 값이 서로 다른 질문에 답한다 — 그래서 스코프가 갈린다 (12차 검수)

```
Prev  →  "사용자가 무엇 뒤에 놓으려 했나"      — 의도.  사용자가 본 것에서만 나온다
틈    →  "그 자리에 어떤 숫자를 줘야 하나"     — 표현.  실제로 존재하는 키 전부를 봐야 한다
```

**한쪽으로 통일하면 둘 중 하나가 깨진다.**

| 통일 방향 | 무엇이 깨지나 |
|---|---|
| 둘 다 표시 목록 | **키 공간에 구멍이 있는데 없다고 계산한다** → 함정 4q·4s |
| 둘 다 부모 전체 | `Prev`로 **화면에 없는 슬롯 형제**를 받게 된다. 클라는 그 `EntryId`를 가리킬 수도 없다 — **사용자가 표현할 수 없는 요청**을 API가 받는 꼴이고 조작된 클라만 쓸 수 있다 |

> **네 번째 읽기 지점이 생기면 이 질문 하나로 스코프를 고른다.** 그래서 헬퍼에 `KeySpace_` 접두어를 붙였다 — 합치는 것으로는 못 막고, **이름으로 막는다.**

#### ★★ 장착한 아이템은 자리를 지킨다 (12차 확정)

> **장착은 아이템을 가방에서 꺼내는 것이 아니다. 칸만 돌려주고 순서 자리는 남긴다.**
> 그래서 뺐다 꽂아도 원래 자리로 돌아오고, **그 사이에 넣은 아이템은 그 앞에 선다.**

**뒷문장이 계산으로 확인된 실제 동작이다.**

```
붕대 0    X 32768    AK 65536(슬롯)    구급상자 131072
AK 해제 후 화면:  [붕대, X, AK, 구급상자]
                        ↑ X는 놓은 자리(두 번째)를 지킨다. AK는 그 뒤다
```

**틈을 부모 전체에서 구하면 새 키는 반드시 `Prev`와 바로 다음 실제 키 사이에 들어가므로, 보이지 않는 이웃을 뛰어넘을 수 없다.** 위 §1의 수정이 이 성질을 만든다.

> **초안은 여기에 *"AK가 X보다 앞에 나타난다"* 고 적었는데 사실이 아니었다(12차 검수).** 그 잘못된 증상 때문에 *"슬롯에 들어가면 자리를 포기한다"* 는 대안을 판정 대기로 올렸었다. **대안은 기각됐다** — 키를 맨 뒤로 밀어도 표시 목록 기준 맨 뒤 발급이 그 구간을 침범해 **같은 동률이 재현되고**, 키 공간에서 아예 빼면 11차가 없앤 `NewSlotId` 예외가 모양만 바꿔 돌아온다.

#### ★★ RPC 표면은 여기서 열지 않는다 (11차 검수)

**9차가 `MoveEntry`에 적용한 규칙 그대로다** — *"내부 계약은 지금, 외부 표면은 소비자와 함께. **검증 표면을 소비자보다 먼저 열지 않는다.**"*

| | 어디 | 이유 |
|---|---|---|
| `AssignSortKey` · `KeySpace_NextAtEnd` · `KeyOf` | **03-A** | `InsertEntry`가 부른다. 없으면 컴파일 안 됨 |
| `GetSortedContents` · `RenormalizeSortKeys` | **03-A** | 위가 부른다 |
| **`ReorderEntry`** (일반 함수) | **03-A** | `EP.Inv.Reorder`가 부른다 |
| **`Server_ReorderEntry`** (RPC) | **04-B** | **드래그가 첫 호출자다** |

**03-9의 검증 목표는 RPC 없이 달성된다** — *"순서 계약을 UI보다 먼저 닫는다"* 는 **커맨드가 있으면** 성립하고 RPC 유무와 무관하다. 03-A에서 `UFUNCTION(Server, Reliable)` 하나와 그 검증(조작된 `PrevEntryId` 처리)이 빠진다.

> **`CanMutateInventory()` 게이트는 RPC 쪽에 붙는다.** *"상태 변경 **RPC**의 유일한 게이트"*(03-5)이고, `EP.Inv.Reorder`는 서버 전용 커맨드라 게이트 대상이 아니다. `ReorderEntry` 본문은 `HasAuthority()`만 본다.

#### ★ 인덱스가 아니라 **앞 이웃**을 받는다

```cpp
void ReorderEntry(int32 EntryId, int32 PrevEntryId);   // ← 이것
void ReorderEntry(int32 EntryId, int32 NewIndex);      // ← 아니다
```

**인덱스는 클라와 서버의 목록이 한 칸이라도 어긋나면 틀린 자리에 놓는다.** 이 게임에서 그건 이론이 아니다 — 현금·탄약이 `bFungible`로 조용히 합쳐지고(03-3), 자동 획득이 드래그 도중에 목록을 밀 수 있다. **이웃은 그 상황에서도 정확하다.**

부수 이득 둘:
- **검증이 한 줄이다** — *"Prev가 같은 컨테이너의 수납 형제인가."* 인덱스면 범위 클램프에 더해 *"클램프한 결과가 클라가 의도한 자리인가"* 가 답이 없다
- **다중 선택 드래그로 넓어진다.** `TArray<int32> EntryIds`를 받아 Prev 뒤에 순서대로 꽂으면 된다. 인덱스 방식은 넓힐 방법이 없다 (지금 만들지는 않는다 — CLAUDE.md §2)

#### ★ `GetSortedContents` — 클라와 서버가 같은 함수를 쓴다

```cpp
TArray<int32> UEPInventoryComponent::GetSortedContents(int32 Container) const
{
    TArray<const FEPInventoryEntry*> Out;
    for (const FEPInventoryEntry& E : Entries.Items)
        if (E.ParentEntryId == Container && E.SlotId.IsNone())    // ★ 슬롯은 빠진다
            Out.Add(&E);

    Out.Sort([](const FEPInventoryEntry& A, const FEPInventoryEntry& B)
    {
        return A.SortKey != B.SortKey ? A.SortKey < B.SortKey
                                      : A.EntryId < B.EntryId;    // ★ 동률 결정성
    });

    // ★ 밖으로는 EntryId만 나간다 — 엔트리 포인터를 내보내지 않는다 (03-2)
    TArray<int32> Ids;
    Ids.Reserve(Out.Num());
    for (const FEPInventoryEntry* E : Out) Ids.Add(E->EntryId);
    return Ids;
}
```

**클라가 이걸로 그리고, 서버도 이걸로 읽는다.** 10차까지 순서가 클라 로컬이라 *"서버가 순서를 봐야 하는 유일한 연산"* 인 **"전부 옮기기"(Shift+클릭)** 에 우회로(클라가 자기 순서대로 `Server_MoveEntry`를 N번)를 만들어 뒀는데, **그 우회로가 사라진다** — 서버가 `GetSortedContents`를 직접 부른다.

> **★ 서버 쪽 소비자가 하나 더 예고돼 있다 — ⓐ의 재장전 탄창 선택** (2026-08-24, `05_Loot_DOCS.md` §7-4). `FindReloadMag`가 `GetInsertionOrder()` × `GetSortedContents()`로 훑어 **첫 호환 탄창**을 고른다. 즉 **사용자가 드래그로 정렬한 순서가 곧 재장전 우선순위**가 되고, *"어느 탄창을 먼저 쓸까"* 라는 정책 필드가 **0개**로 끝난다. 11차 검수의 지적(*"서버로 옮겼는데 서버 소비자가 사실상 하나뿐"*)이 여기서 닫힌다.

- **★★ 동률 타이브레이크는 "만약을 위한 것"이 아니다 (11차 검수에서 근거가 바뀌었다).** 초안은 *"`AssignSortKey`가 유일한 쓰기 지점이라 겹칠 문법이 없다"* 고 적었는데 **거짓이었다** — 아래 "키 공간" 절. 스코프를 고친 뒤에도 타이브레이크는 남긴다: **복제 지연 중 클라가 잠깐 옛 키를 들고 있을 수 있고**, `FastArraySerializer.h:54`가 배열 순서를 보장하지 않으므로 그때 클라와 서버가 다른 순서를 그린다

#### ★★ 키 공간은 `GetSortedContents`가 아니다 (11차 검수)

**초안은 `KeySpace_NextAtEnd`가 `GetSortedContents`로 최대 키를 구했다. 거기서 동률이 났다.**

```
본체(Parent = -1)에 셋:   붕대 0    구급상자 65536    AK 131072

① AK를 1번 핫바에 장착   MoveEntry(AK, -1, "Hotbar1")
     부모가 안 바뀐다(-1 → -1)         →  재발급 안 함
     초안은 "슬롯으로 가면 재발급 안 함" 예외도 뒀다
     ⇒ AK.SortKey = 131072 그대로. 그런데 GetSortedContents(-1)에서는 빠진다

② 아무거나 하나 줍는다    InsertEntry(Parent = -1, SlotId = None)
     KeySpace_NextAtEnd(-1) = KeyOf(구급상자) + 65536 = 131072
     ⇒ 새 아이템.SortKey = 131072                 ← ★ AK와 동률

③ AK를 해제              MoveEntry(AK, -1, None)
     부모가 안 바뀐다  →  재발급 안 함
     ⇒ 본체에 SortKey 131072가 둘
```

**세 단계 전부 정상 플레이이고, 가장 흔한 조작 순서다** — 총을 꽂고, 뭘 줍고, 총을 뺀다.

**증상은 크래시가 아니라 조용한 흐트러짐이다.** 타이브레이크가 있어 클라·서버 불일치까지는 안 가지만, **동률 그룹 안 순서가 `EntryId` 순(= 획득 순)으로 고정된다.** 손으로 맞춰둔 배치가 **무기를 뺐다 꽂을 때마다 조금씩 무너진다.**

**원인은 두 함수의 역할 혼동이다.**

| 함수 | 무엇인가 | 슬롯을 거르나 |
|---|---|---|
| `GetSortedContents` | **그릴 것을 고른다** | ✅ 거른다 (`SlotId.IsNone()`) |
| `KeySpace_NextAtEnd` · `RenormalizeSortKeys` | **키 공간을 정의한다** | ❌ **거르면 안 된다** |

**03-1의 스펙은 원래 옳았다** — *"형제(같은 `Parent`) 스코프."* 구현 힌트가 그걸 어겼다. 고치면 **예외 세 개가 전부 사라지고 코드가 줄어든다** — `InsertEntry`의 삼항, `MoveEntry`의 `NewSlotId` 조건, `RenormalizeSortKeys`의 필터.
- **`SlotId.IsNone()` 필터가 여기 있다.** 04-2의 함정 8c(부착물 자식이 가방 목록에 섞임)가 이 한 줄로 닫힌다

#### ★ `RenormalizeSortKeys` — 죽은 코드가 아니다

```cpp
void UEPInventoryComponent::RenormalizeSortKeys(int32 Container)
{
    // ★★ 부모가 같은 것 전부를 모은다 — 슬롯에 든 것도 포함 (11차 검수)
    //   GetSortedContents로 돌면 슬롯 형제의 키만 옛 값으로 남아 동률이 새로 생긴다
    TArray<const FEPInventoryEntry*> All;
    for (const FEPInventoryEntry& E : Entries.Items)
        if (E.ParentEntryId == Container) All.Add(&E);

    All.Sort([](const FEPInventoryEntry& A, const FEPInventoryEntry& B)
    { return A.SortKey != B.SortKey ? A.SortKey < B.SortKey : A.EntryId < B.EntryId; });

    int32 K = 0;
    for (const FEPInventoryEntry* E : All)
    {
        AssignSortKey(E->EntryId, K);   // 현재 순서를 유지한 채 간격만 다시 벌린다
        K += SortKeyStep;
    }
}
```

이 방식의 **정의상 반드시 도달하는 지점**이다 — 같은 틈에 연속으로 꽂으면 ~16회에서 `(Prev+Next)/2 == Prev`가 된다. 사이클 검사(검사 6)와 달리 *"데이터 오류에서만 도는 방어"* 가 아니라 **정상 조작으로 도달한다.**

- **완료 조건으로 증명한다.** 안 넣으면 영원히 안 돌아 죽은 코드가 되고, 그때 처음 도는 날 순서가 무너진다
- **비용은 여기서만 N개 dirty다.** 재정규화가 조밀 재번호와 같은 비용을 치르는 대신, **~16회에 한 번**만 치른다
- **★ 재귀는 한 번만 돈다.** 재정규화 후 간격이 정확히 `Step`이라 이분이 반드시 성공하고 맨 앞/맨 뒤도 경계에서 멀어진다. **형제가 32767개를 넘으면 재정규화 자체가 넘치는데**(`Step × N > MAX_int32`) `ContainerCapacity`가 그보다 훨씬 작아 도달 불가다 — **스태시 용량을 정할 때 이것이 상한이다**(11차 검수)

---

#### ★ `Server_MoveEntry`는 만들지 않는다 — 그리고 좁은 대체품도 만들지 않는다 (14차)

`MoveEntry`는 **내부 계약**이고, RPC 표면은 별개다. 8차의 문장 *"서버가 이미 소유한 상태에 대한 변경 요청 → 컴포넌트의 서버 RPC"* 는 **RPC를 열어도 된다**는 허가지 **얼마나 넓게 열지**의 답이 아니다.

**Step 03에는 `NewParent`와 `NewSlotId`를 정당하게 만들어낼 UI가 없다.** 그런데 RPC를 열면 조작된 클라이언트는 만들 수 있다. **소비자보다 검증 표면을 먼저 여는 것**이고, 이건 8차가 `Server_DropItem`을 고른 이유(*"서버가 이미 소유한 상태"*)와 무관한 순수한 공격 표면 확대다.

##### ★★ 9차가 대신 세워둔 `Server_EquipBackpack`도 없앤다 (14차)

9차는 *"넓은 RPC를 안 여는 대신 좁은 것 하나를 연다"* 로 `Server_EquipBackpack(int32)`을 남겼고, 근거는 **좁은 RPC가 넓은 RPC보다 낫다**였다. **그 근거는 `Server_MoveEntry`가 끝까지 안 열릴 때만 성립한다.**

```
04-B가 Server_MoveEntry(id, NewParent, NewSlotId)를 연다      05_Loot_04_InventoryUI.md:610, 619
   → 착용 = Server_MoveEntry(id, INDEX_NONE, "Torso")
   → 배낭  = Server_MoveEntry(id, INDEX_NONE, "Back")          ← 래퍼가 하던 일 그대로
```

**넓은 문이 열리는 순간 좁은 문은 공격 표면을 하나도 줄이지 않는다** — 조작된 클라는 그냥 넓은 쪽을 부른다. **좁은 래퍼는 그게 유일한 문일 때만 좁힌다.**

**그리고 호출자가 계획 전체에 0개다.**

| 시점 | 착용 경로 | 무엇을 부르나 |
|---|---|---|
| Step 03 자동 착용 | `OnInteract` → `TryAutoEquip` → `AddSubtree` | **서버 내부. RPC를 안 지난다** |
| Step 03 검증 | `EP.Inv.Move <id> -1 Back` | **내부 `MoveEntry` 직접** |
| 04-A 검증 | `EP.Inv.Move <id> -1 Torso` | **내부 `MoveEntry` 직접** — 커맨드는 RPC를 안 지난다(03-9) |
| 04-B 드래그 | `Server_MoveEntry(id, -1, "Torso")` | 일반 RPC |
| 벗기 | `Server_DropItem(id)` | 03-6 |

> **13차는 여기까지 갔다가 멈췄다.** *"Step 03에 호출자가 0개"* 를 찾아내고 **삭제가 아니라 04-A로 이동**을 골랐는데, 근거가 *"`EP.Inv.Equip`이 첫 호출자다"* 였다. **`EP.Inv.Equip`은 콘솔 커맨드다.** 이 문서가 두 번 확립한 규칙 — *"커맨드가 내부 함수를 직접 부른다. RPC 표면을 열지 않는다"*(`EP.Inv.Reorder`→`ReorderEntry`, `EP.Inv.Move`→`MoveEntry`) — 이 그대로 적용되므로 **옮긴 자리에도 호출자가 없다.**

> **배낭이 특별할 근거도 이미 없다.** 같은 절이 `TryAutoEquip`에 대해 *"배낭 전용 함수를 만들지 않는 이유: 무기·상의·헬멧이 들어올 때 같은 함수가 넷이 된다"* 고 적었다. **자동 착용 경로에는 그 규칙을 적용하고 RPC 이름에는 적용하지 않았던 것이다** — 슬롯 배정은 `SlotPriority`(아이템)와 `BodySlots`(설정)가 답하고, 상의·하의·외투·배낭이 전부 같은 모양인데 `"Back"`만 함수 이름에 하드코딩돼 있었다.

#### 교체(swap)는 Step 04의 `SwapEntries`로

검사 4는 *"차 있으면 실패"* 다. **Lyra도 같다** — `AddItemToSlot`이 `Slots[i] == nullptr`일 때만 대입하고 조용히 실패한다(`LyraQuickBarComponent.cpp:169-179`). 우리는 `Client_OnInventoryActionFailed`가 있으니(03-5) 조용하지 않게 할 수 있다.

**교체는 `MoveEntry`를 두 번 부르는 것으로 만들 수 없다.** 중간 상태에 상대가 아직 안 빠져 있어서 **성립하는 교환이 거절된다** — 외투가 `7/10`이고 AK(4) ↔ 붕대(1)면 교환 후는 `7-1+4 = 10`으로 딱 맞는데, 순차로 하면 `7+4 = 11`로 실패한다. 그래서 별도 함수가 된다.

```cpp
bool SwapEntries(int32 A, int32 B);   // Step 04. 판정은 교환 후 상태로 한 번에
```

**Step 03에는 넣지 않는다** — 소비자가 Step 04의 드래그뿐이고, `MoveEntry`와 달리 여기서 래퍼가 될 호출자도 없다. 설계는 `05_Loot_04_InventoryUI.md` 04-7에 있다.

> **★ 두 번째 소비자가 예고돼 있다 — ⓐ의 재장전** (2026-08-24, `05_Loot_DOCS.md` §7-4). 탄창이 별도 아이템이 되면 **재장전이 `SwapEntries(꽂힌탄창, 새탄창)` 한 줄**이다. 교환이면 *"빈 탄창을 어디로 보내나"* 가 사라지고(방금 비운 자리로 간다) **칸 검사가 필요 없다** — 총량이 안 변하기 때문이다. **부모·`SlotId`·`SortKey`를 함께 교환한다는 계약이 거기서 그대로 값을 한다**: 빈 탄창이 새 탄창의 화면 자리를 물려받아 *"제자리에서 탄창이 바뀐"* 것으로 보인다. **지금 만들 이유는 아니고**(소비자는 여전히 04-B가 처음이다), 04-7의 교환 계약을 좁힐 때 이쪽을 같이 볼 근거다.

#### 반환 규약

`bool`. 실패 사유는 필요해지면 `FText` 아웃 파라미터로 늘린다.

---

> ### ★ `MoveEntry`가 유일한 지점인 것의 두 번째 배당금 — 장비 효과 (2026-08-26, `05_Loot_DOCS.md` §7-5)
>
> 방어구가 오면 *"장착하면 `AbilitySet`을 부여하고 벗으면 회수한다"* 가 붙는데(Lyra `AbilitySetsToGrant`), **`SlotId`를 고치는 곳이 여기 하나라 훅도 하나다** — 이 함수 끝의 분기 한 줄이다. `SlotId`가 여러 곳에서 바뀌는 설계였다면 훅이 그 수만큼 늘고, 하나를 빠뜨리면 **벗었는데 효과가 남는다.**
>
> **회수 쪽 자리도 이미 있다** — `RemoveEntry`의 ① write-back과 **같은 칸**이다. `RemoveSelf` 뒤에는 `GetEquippedEntryId()`가 `INDEX_NONE`이라 회수 대상을 잃는다(`05_Loot_05_Equipment.md` 05-3의 함정과 같은 것).
>
> **핸들은 엔트리에 넣지 않는다** — `FGameplayAbilitySpecHandle`은 서버 전용인데 `FEPInventoryEntry`는 복제된다. 컴포넌트의 `UPROPERTY(NotReplicated) TMap<int32, ...>`이다. **지금 만들지 않는다** — 방어구 행이 `DT_Items`에 하나도 없다.

### ★ §7-1 월드 컨테이너가 이 두 함수로 **이미** 성립한다

```
Container->RemoveEntry(Id, &Sub)   →   MyInv->AddSubtree(INDEX_NONE, NAME_None, Sub)
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
    DOREPLIFETIME_CONDITION(UEPInventoryComponent, ActiveHotbarIndex,       COND_OwnerOnly);
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

    // ★★ 키를 **배열에 넣기 전에** 구한다 (13차, 함정 4x)
    //   먼저 AddDefaulted하면 그 원소(SortKey=0)가 자기 키 공간의 형제로 잡혀
    //   "빈 컨테이너면 0" 분기가 죽고, 형제 키가 전부 음수인 컨테이너에서 최대가 0이 된다
    // ★ SortKey는 여기서 발급한다 — 형제 맨 뒤. 호출자가 정하지 않는다 (11차)
    //   ★★ 슬롯 여부로 갈리지 않는다. 슬롯에 든 것도 키를 갖는다 — 아래 "키 공간" 절
    //   AddSubtree만 예외다. 거기서는 스냅샷의 키를 그대로 쓴다 (03-4)
    const int32 NewKey = KeySpace_NextAtEnd(Parent);

    FEPInventoryEntry& E = Entries.Items.AddDefaulted_GetRef();
    E.EntryId       = NewId;
    E.ParentEntryId = Parent;
    E.SlotId        = SlotId;
    E.ItemId        = ItemId;
    E.State         = State;                 // ★ 값 복사. 잔탄이 여기서 보존된다
    E.SortKey       = NewKey;

    Entries.MarkItemDirty(E);
    return NewId;                            // ★ E.EntryId가 아니라 미리 뜬 값
}

// ★★ 키 공간은 "부모가 같은 것 전부"다 — 슬롯 여부를 보지 않는다 (11차 검수)
//   GetSortedContents를 쓰면 안 된다. 그건 "그릴 것"을 고르는 함수이지 키 공간이 아니다
int32 UEPInventoryComponent::KeySpace_NextAtEnd(int32 Container)
{
    int32 Max = 0; bool bAny = false;
    for (const FEPInventoryEntry& E : Entries.Items)
        if (E.ParentEntryId == Container)
        { Max = bAny ? FMath::Max(Max, E.SortKey) : E.SortKey; bAny = true; }

    if (!bAny) return 0;

    // ★ 유일한 발급 지점이므로 상한 가드도 여기 하나면 된다 (함정 4q)
    if (Max > MAX_int32 - SortKeyGuard)
    {
        RenormalizeSortKeys(Container);          // 0, Step, 2*Step... 으로 다시 깐다
        return KeySpace_NextAtEnd(Container);        // 한 번만 재귀한다 — 아래 ★
    }
    return Max + SortKeyStep;
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
        if (!E.SlotId.IsNone())             continue;   // ★ 슬롯에 든 것은 칸을 안 먹는다
                                                        //   이 한 줄이 "칸을 먹는다"의 정의 전체다

        const FEPItemData* Row = D->FindData(E.ItemId);
        if (!ensureMsgf(Row, TEXT("[Inventory] DT에 없는 ItemId: %s"), *E.ItemId.ToString()))
            continue;                                   // ★ 아래 참조
        Sum += Row->SlotSize;
    }
    return Sum;
}

int32 UEPInventoryComponent::GetCapacity(int32 Container) const
{
    if (Container == INDEX_NONE) return MaxSlots;       // ★ 최종 0. 테스트 중에만 10 (아래)

    FEPInventoryEntry E;
    if (!FindEntry(Container, E)) return 0;

    const UEPItemDefinitionSubsystem* D = Defs();
    const FEPItemData* Row = D ? D->FindData(E.ItemId) : nullptr;
    return Row ? Row->ContainerCapacity : 0;           // ★ DT다 (§4-9 원칙)
}
```

### ★★ 용량표 — 본체는 **0칸**이 된다 (13차 확정)

| 아이템 | `SlotSize` (차지) | `ContainerCapacity` (제공) | 비고 |
|---|---|---|---|
| **본체** (`INDEX_NONE`) | — | **0** | `MaxSlots`. **테스트 중에만 10**이고 곧 0으로 내린다 |
| 기본 상의 | 11 | 10 | 스폰 시 착용 |
| 기본 하의 | 6 | 5 | 스폰 시 착용 |
| 배낭 A | 15 | 12 | **어디에도 안 들어간다**(15 > 12, 15 > 10) — 의도 |
| 배낭 B | 10 | 8 | 상의(10)·배낭A(12)에 들어간다 |

**수납 용량이 전부 착용 컨테이너에서 나온다.** 아무것도 안 입으면 0칸이고, 그래서 **스폰 시 기본 상의·하의를 입고 시작한다**(03-6 `StartingEquipment`).

#### 부등호가 둘이고 서로 다른 식이다

```
넣기 판정   :  SlotSize(넣을 것)  ≤  Capacity(담을 것)     ← ≤ 다. B(10)를 상의(10)에 넣는다
데이터 규칙 :  Capacity(X)        <  SlotSize(X)           ← < 다. 등호를 허용하면 안 된다
```

**아래를 `≤`로 풀면 §4-6의 깊이 증명이 무너진다.**

```
SlotSize(A) ≤ Capacity(B) < SlotSize(B)   ⇒  SlotSize(A) <  SlotSize(B)   깊이 유한 ✅
SlotSize(A) ≤ Capacity(B) ≤ SlotSize(B)   ⇒  SlotSize(A) ≤  SlotSize(B)   깊이 무한 ❌
```

`SlotSize 10 / Cap 10`짜리 행 하나면 **그 가방이 자기 안에 들어간다**(10 ≤ 10).

**★ 이걸 막는 근거가 *"익스플로잇"* 은 아니다 (13차 검수).** 정직하게 재보면 N겹으로 쌓아도 **총 용량은 그대로**다 — 바깥 10칸 중 10칸을 안쪽이 먹으므로 순증 0이고, 가방 N개를 소모해서 얻는 것이 없다. **얻는 게 없으니 하는 사람도 없다.**

**진짜 근거 셋은 전부 비용 쪽에 있다.**

| | |
|---|---|
| ① **비용이 0이다** | `IsDataValid`가 **이미 `<`로 짜여 있고**(`05_Loot_00_ItemCore.md`) 확정 수치가 전부 만족한다. **쓸 코드도 고칠 값도 없다** |
| ② **깊이 상한이 이 규칙 하나에만 걸려 있다** | `RemoveEntry` 재귀 · UI 중첩 · 세이브가 전부 *"깊이는 유한하다"* 를 전제하는데, 그 전제의 증명이 §4-6의 부등식 **하나**다. 되돌리면 **세 곳의 전제가 동시에 근거를 잃는다** |
| ③ **되돌릴 손잡이가 이미 이름으로 있다** | §4-6이 *"`MaxContainerDepth`는 만들지 않는다. 밸런싱이 커플링을 못 견디면 그때 연다"* 고 적어뒀다. **`=`를 원하는 날의 답은 `=`를 여는 게 아니라 깊이 상한을 여는 것이다** |

**즉 질문이 *"막을 가치가 있나"* 가 아니라 *"이미 공짜로 막혀 있는 것을 굳이 풀 이유가 있나"* 다.**

#### ★★ 그런데 `SlotSize ≥ 1`을 검증하는 곳이 없다 — 본체 0칸이 이걸 구멍으로 만든다 (13차)

위 증명이 *"`SlotSize`가 양의 정수라 사슬이 유한하다"* 를 쓰는데, **`FEPItemData::SlotSize = 1`은 기본값일 뿐 하한이 아니다**(`EPItemData.h:39` — `ClampMin`도 없다). `IsDataValid`가 보는 것도 컨테이너 규칙 하나뿐이다.

**본체가 10칸인 동안은 무해했다. 0이 되면 다르다.**

```cpp
CanFit(본체, X) = GetUsedSlots(-1) + SlotSize <= GetCapacity(-1)
                =        0        +    0      <=        0          →  참
```

**`SlotSize = 0`인 아이템은 0칸짜리 본체에 무한히 들어가고, `GetUsedSlots`가 그것들을 0으로 세므로 영원히 안 찬다.** `GetInsertionOrder`의 맨 앞이 본체라 **그 아이템만 컨테이너에 절대 안 들어가고 항상 본체로 간다** — 동작이 조용히 갈린다.

- **`SlotSize = 0`은 실수하기 쉬운 값이다.** *"열쇠·퀘스트 토큰은 자리를 안 먹었으면"* 이라는 기획이 오면 DT에 0을 넣는 것이 가장 자연스러운 표현이다
- **증상이 없다.** 크래시도 경고도 없고 *"이 아이템만 가방에 안 들어간다"* 로 나타난다
- **`ContainerCapacity`를 지킨 것과 정확히 같은 이유로 지켜야 한다** — 규칙을 문서에만 두면 반년 뒤 깨진다

> **"자리를 안 먹는 아이템"이 기획으로 오면 그때는 `SlotSize`가 아니라 슬롯이다.** 몸 슬롯(`SlotId != None`)이 이미 *"칸을 안 먹는다"* 의 유일한 표현이고(`GetUsedSlots`의 `continue` 한 줄), **두 번째 표현을 만들지 않는다.**

> **★ 상의·하의는 컨테이너를 잃을 수 있다 — 이행이 데이터 둘이다** (§8 미정). `DT_Items`의 `ContainerCapacity`를 0으로 내리고(§4-6: 컨테이너 = `Capacity > 0`), `ContainerOrder`에서 빼면 끝이다. **코드 변경 0** — `GetCapacity`가 0을 돌려주고 `CanFit`이 항상 거짓이 되며 `BodySlots`·검사 3은 무관하다. 그때 `SlotSize` 11/6의 하한도 사라져(`0 < SlotSize`) *"접으면 2칸"* 같은 값으로 내려간다.

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

> **★ 복제 순서 방어가 필요 없는 이유도 여기 있다.** 자식이 부모보다 먼저 도착하면 `GetUsedSlots(INDEX_NONE)`이 그 자식을 안 세고(`ParentEntryId != INDEX_NONE`), 배낭 구획은 아예 안 그려진다(`GetEntryInSlot(INDEX_NONE, "Back")`이 아직 `INDEX_NONE`). **잠깐 안 보이다가 다음 수신에서 저절로 맞는다.** 모든 파생값을 매 갱신마다 처음부터 다시 계산하기 때문이고, 캐시했다면 "부모 없는 자식이 도착했을 때 캐시를 어떻게 하나"가 진짜 문제가 됐다. **순서 방어 코드를 넣지 마라.**

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


## 03-A-부록. ★★ 조회·쓰기 기본 함수 열 — 통합 문서에 본문이 없었다 (15차 신설)

**2692줄짜리 통합 문서에 이 열 개의 본문이 하나도 없었다.** 선언과 한 줄 주석(`// 없으면 INDEX_NONE`)만 있고, 설계 근거·함정·검수 이력이 층층이 쌓이는 동안 **가장 기본적인 것이 비어 있었다.** 파일을 쪼갠 직접적 계기가 이것이다.

**전부 03-A 필수다** — 아래 표의 "부르는 곳"이 이미 03-A 안에 있다.

| 함수 | 부르는 곳 | 스텁으로 두면 |
|---|---|---|
| `FindEntry` | `AddEntryCharges` · `ReorderEntryInternal` · `MoveEntry` | **거짓 실패** — 아무것도 안 되는데 조용하다 |
| `ContainsEntry` | `MoveEntry` 검사 1 · `ReorderEntry` | `return true`면 **조작된 요청이 전부 통과한다** |
| `FindFungibleEntryId` | `AddItem` | 현금이 합쳐지지 않고 엔트리가 계속 는다 |
| `GetEntryInSlot` | `CanPlaceInSlot` 검사 4 · `GetEquippedBackpack` | `return 0`이면 **0번 엔트리가 모든 슬롯을 채운 것으로 보인다** |
| `GetEquippedEntryId` | Step 05 · `RemoveEntryInternal` ① | — |
| `RemoveSelf` | `RemoveEntryInternal` ③ | **아무것도 안 지워진다** (03-B) |
| `AssignSortKey` | `InsertEntry`(간접) · `RenormalizeSortKeys` · `ReorderEntryInternal` | **순서가 안 바뀐다.** 완료 조건 16~19가 전부 죽는다 |
| `KeySpace_Min` · `KeySpace_NextAbove` · `KeyOf` | `ReorderEntryInternal` | 같은 위 |

---

### 조회 셋 — `Entries.Items`를 한 번 돈다

```cpp
// 값 복사로 돌려준다. 배열 원소의 포인터를 밖으로 내보내지 않는다 (03-2)
bool UEPInventoryComponent::FindEntry(int32 EntryId, FEPInventoryEntry& Out) const
{
    for (const FEPInventoryEntry& E : Entries.Items)
    {
        if (E.EntryId != EntryId) continue;
        Out = E;
        return true;
    }
    return false;
}

// 존재만 묻는다. 조작된 요청을 거르는 검사 1의 전부다
bool UEPInventoryComponent::ContainsEntry(int32 EntryId) const
{
    for (const FEPInventoryEntry& E : Entries.Items)
        if (E.EntryId == EntryId) return true;
    return false;
}

// 같은 컨테이너 안의 같은 ItemId. ★★ Container 인자가 이 함수의 핵심이다
int32 UEPInventoryComponent::FindFungibleEntryId(int32 Container, FName ItemId) const
{
    for (const FEPInventoryEntry& E : Entries.Items)
        if (E.ParentEntryId == Container && E.ItemId == ItemId)
            return E.EntryId;
    return INDEX_NONE;
}
```

> **★ `FindEntry`가 `bool` ＋ out 파라미터인 이유.** 엔트리를 포인터로 돌려주면 **호출자가 그것을 들고 있다가 배열이 재할당된 뒤에 쓴다.** `RemoveSelf`가 `RemoveAtSwap`을 하므로 그 순간이 실재한다. 그리고 **13차의 함정 4v가 정확히 이 함수 때문에 났다** — `FindEntry`는 **복사본**을 주므로 그 결과에 쓰면 컴파일되고 `true`를 반환하는데 배열은 안 바뀐다. **쓰려면 `for (FEPInventoryEntry& E : Entries.Items)`로 참조를 잡아야 한다.**

> **★ `ContainsEntry`를 `FindEntry`로 대신하지 않는다.** `FEPInventoryEntry` 한 벌을 복사하는 비용이 붙고, 무엇보다 **버려질 out 변수를 호출부마다 선언하게 된다** — 검사 1이 그 모양이면 읽는 사람이 *"저 값을 쓰나?"* 를 매번 확인한다.

> **★ `FindFungibleEntryId`에 `SlotId` 조건을 넣지 않는다.** *"슬롯에 든 것과는 안 합쳐야 하지 않나"* 가 자연스러운 의심인데, **`bFungible`인 아이템은 `SlotPriority`가 비어 있어 슬롯에 들어갈 수 없다**(검사 2). 조건을 넣으면 **도달 불가 분기의 방어**이고 CLAUDE.md §2가 금지한다. 다만 *"`bFungible`이면서 슬롯에 들어가는 아이템"* 이 생기면 이 문장이 깨진다 — 함정 4o와 같은 자리다.

---

### 슬롯 조회 둘

```cpp
// (Parent, SlotId) 쌍으로 정확히 하나를 찾는다. 없으면 INDEX_NONE
// ★ Parent 인자가 있어야 부착 슬롯에서 안 깨진다 — 무기 둘이 각자 Optic을 갖는다
int32 UEPInventoryComponent::GetEntryInSlot(int32 Parent, FName SlotId) const
{
    if (SlotId.IsNone()) return INDEX_NONE;      // ★ 수납은 슬롯이 아니다 — 첫 수납품을 돌려주면 안 된다

    for (const FEPInventoryEntry& E : Entries.Items)
        if (E.ParentEntryId == Parent && E.SlotId == SlotId)
            return E.EntryId;
    return INDEX_NONE;
}

// 활성 핫바 인덱스 → 슬롯 이름 → 엔트리. 저장된 필드가 아니라 파생 게터다 (9차)
int32 UEPInventoryComponent::GetEquippedEntryId() const
{
    if (ActiveHotbarIndex < 0 || ActiveHotbarIndex > 3) return INDEX_NONE;

    const FName SlotId(*FString::Printf(TEXT("Hotbar%d"), ActiveHotbarIndex + 1));
    return GetEntryInSlot(INDEX_NONE, SlotId);   // 핫바 1~4는 몸 슬롯이라 부모가 본체다
}
```

> **★★ `SlotId.IsNone()` 조기 반환이 없으면 조용히 무너진다.** 없으면 `GetEntryInSlot(배낭, NAME_None)`이 **배낭 속 첫 수납품**을 돌려준다. 그러면 `CanPlaceInSlot`의 검사 4가 *"자리가 차 있다"* 로 읽고, `GetEquippedBackpack()`은 **아무 아이템이나 배낭으로 잡는다.** 지금 모든 호출부가 `SlotId != None`을 보장하지만 **그건 호출자의 계약이고, 호출자는 는다**(13차가 `AddSubtree`에서 배운 것).

> **★ `ActiveHotbarIndex`의 범위 검사가 `0~3`인 것이 계약이다.** 5~0은 `SlotId`가 아니라 `HotbarRefs` 참조라 *"손에 든다"* 가 성립하지 않는다(`EquipmentSlots.md` §4). 열어두면 `"Hotbar7"` 같은 **없는 슬롯**을 조회해 조용히 `INDEX_NONE`이 나오고, 증상은 *"7번을 누르면 무기가 사라진다"* 다.

> **`FString::Printf`가 매 호출마다 도는 것이 걸리면** `BodySlots`처럼 배열로 뺀다. **지금은 하지 않는다** — 호출 빈도가 UI 갱신 수준이고, 배열로 빼면 *"인덱스 ↔ 이름"* 이라는 **두 번째 진실**이 생긴다(§8 미정 #10이 관장할 자리다).

---

### 쓰기 둘 — 가드가 여기 붙는다

```cpp
// 배열에서 빼고 MarkArrayDirty. 제거의 유일한 물리적 지점이다
void UEPInventoryComponent::RemoveSelf(int32 EntryId)
{
    FScopedInventoryNotify Guard(this);           // ★ 단일 쓰기 지점 (03-7)

    for (int32 i = 0; i < Entries.Items.Num(); ++i)
    {
        if (Entries.Items[i].EntryId != EntryId) continue;

        Entries.Items.RemoveAtSwap(i);            // ★ 순서는 SortKey가 들므로 Swap이어도 된다
        Entries.MarkArrayDirty();                 // ★ MarkItemDirty가 아니다 — 항목이 사라졌다
        return;
    }
}

// SortKey를 고치는 유일한 지점. 배열 원소의 참조를 잡는다
void UEPInventoryComponent::AssignSortKey(int32 EntryId, int32 NewKey)
{
    FScopedInventoryNotify Guard(this);           // ★ 단일 쓰기 지점 (03-7)

    for (FEPInventoryEntry& E : Entries.Items)    // ★★ 참조다. FindEntry는 복사본이라 안 된다 (함정 4v)
    {
        if (E.EntryId != EntryId) continue;
        if (E.SortKey == NewKey) return;          // ★ 값이 같으면 델타를 안 보낸다 (함정 4u와 같은 취지)

        E.SortKey = NewKey;
        Entries.MarkItemDirty(E);
        return;
    }
}
```

> **★★ `RemoveSelf`가 `MarkArrayDirty`인 것이 `MarkItemDirty`와 갈리는 유일한 자리다.** 나머지 넷은 **살아 있는 항목의 필드**를 고치므로 `MarkItemDirty(E)`이고, 여기만 **항목이 배열에서 사라진다.** 바꿔 쓰면 **삭제가 클라에 안 간다** — 서버에서는 없어졌는데 클라 화면에는 남아 있고, 그 항목을 클릭하면 `ContainsEntry`가 거짓이라 조용히 실패한다.

> **★ `RemoveAtSwap`을 써도 되는 이유는 11차가 만든 것이다.** 배열 순서에 의미가 없어졌기 때문이다 — 표시 순서는 `SortKey`가 들고 `GetSortedContents`가 정렬한다. **10차까지는 이 줄이 `RemoveAt`이어야 했다.** 그리고 `FastArraySerializer.h:54`가 애초에 수신 측 배열 순서를 보장하지 않으므로, **`RemoveAt`으로 바꿔도 얻는 것이 없다.**

> **★ `AssignSortKey`의 `E.SortKey == NewKey` 조기 반환.** `RenormalizeSortKeys`가 N개를 전부 다시 깔 때 **대부분이 이미 같은 값**인 경우가 흔하고(재정규화 직후 또 부르면 전부 같다), 그때 무의미한 `MarkItemDirty` N개가 나간다. 12차가 `ReorderEntry`에 넣은 조기 반환(함정 4u)과 같은 취지다.

> **★ 가드가 둘 다에 있어야 한다.** `RemoveEntry` 하나가 배낭과 그 안 아이템 다섯을 지우면 `RemoveSelf`가 여섯 번 돌고, 가드가 없으면 **`Broadcast`가 여섯 번 나간다.** 03-7이 *"수신 1회당 1회"* 를 세운 것과 같은 이유를 서버 쪽에서 지키는 장치다.

---

### 키 공간 셋 — **부모 전체**를 본다

**`GetSortedContents`가 아니다** (함정 4q). 표시 목록은 슬롯을 거르지만 **키 공간은 슬롯에 든 형제의 키도 세야 한다** — 안 그러면 꽂았다 뺀 아이템과 동률이 난다.

```cpp
// 부모 전체의 최소 키. 비면 0
// ★ 실패 센티널이 필요 없다 — 호출부(ReorderEntryInternal)에 자기 자신이 있어 절대 안 빈다
int32 UEPInventoryComponent::KeySpace_Min(int32 Container) const
{
    int32 Min = 0; bool bAny = false;
    for (const FEPInventoryEntry& E : Entries.Items)
    {
        if (E.ParentEntryId != Container) continue;   // ★ SlotId를 보지 않는다 (함정 4q)
        Min  = bAny ? FMath::Min(Min, E.SortKey) : E.SortKey;
        bAny = true;
    }
    return bAny ? Min : 0;
}

// Key보다 큰 키 중 최소. Exclude는 세지 않는다(옮기는 중인 자기 자신)
// 반환: 찾았으면 true. ★ INDEX_NONE을 센티널로 쓰지 않는다 — SortKey는 −1도 유효하다 (13차, 함정 4w)
bool UEPInventoryComponent::KeySpace_NextAbove(
    int32 Container, int32 Key, int32 Exclude, int32& OutKey) const
{
    bool bAny = false;
    for (const FEPInventoryEntry& E : Entries.Items)
    {
        if (E.ParentEntryId != Container) continue;   // ★ 여기도 SlotId를 안 본다
        if (E.EntryId == Exclude)          continue;
        if (E.SortKey <= Key)              continue;

        OutKey = bAny ? FMath::Min(OutKey, E.SortKey) : E.SortKey;
        bAny   = true;
    }
    return bAny;
}

// 그 엔트리의 키. 없으면 false — 0을 돌려주지 않는다 (0은 재정규화 직후 첫 형제의 키다)
bool UEPInventoryComponent::KeyOf(int32 EntryId, int32& OutKey) const
{
    for (const FEPInventoryEntry& E : Entries.Items)
    {
        if (E.EntryId != EntryId) continue;
        OutKey = E.SortKey;
        return true;
    }
    return false;
}
```

> **★★ 셋 다 `SlotId`를 보지 않는다.** `GetSortedContents`(`E.SlotId.IsNone()`으로 거른다)와 **여기가 갈리는 지점**이고, 11차 검수가 잡은 함정 4q의 전부다. 이름에 `KeySpace_` 접두어를 붙인 것이 12차이고, 이유가 이 구분을 **읽는 순간 보이게** 하기 위해서다.
>
> ```
> 무기를 핫바에 꽂는다  →  SlotId="Hotbar1". 표시 목록에서 사라지지만 SortKey는 그대로 든다
> 그 사이에 붕대를 줍는다  →  GetSortedContents로 최대를 구하면 무기의 키를 못 본다
>                          →  붕대가 무기와 같은 키를 받는다  →  뺐을 때 동률
> ```

> **★ `KeySpace_Min`만 `bool`이 아닌 이유.** 유일한 호출부가 `ReorderEntryInternal`이고 거기서는 **옮기는 엔트리 자신이 그 컨테이너에 있다** — 배열이 비는 경우가 없다. 나머지 둘은 *"없음"* 이 **정상 결과**다(맨 뒤로 옮기면 위에 아무도 없다). **`KeyOf`가 `bool`인 것은 다르다** — 거기서 *"없음"* 은 조작된 요청이고, `0`을 돌려주면 **재정규화 직후 첫 형제의 자리**로 조용히 간다(13차).

> **★ 넷 다 O(N) 순회다.** 엔트리가 수십 개 수준이고 호출 빈도가 조작 단위라 캐시하지 않는다 — `GetUsedSlots`를 캐시하지 않는 것과 같은 판정(03-3)이고, 캐시하면 *"부모 없는 자식이 도착했을 때 캐시를 어떻게 하나"* 가 진짜 문제가 된다.

---

## 03-7. 알림 — 수신 1회당 1회

**항목별 콜백을 쓰지 않는다.** `PostReplicatedAdd`는 **항목마다** 불리므로(`FastArraySerializer.h:1163`) 한 번의 수신에 UI 재생성이 항목 수만큼 돈다. 서버 쪽도 배낭 하나 버리면 `Broadcast`가 N+2회 나간다.

> **"안 쓴다"가 "선언만 해둔다"가 아니다.** 03-1이 셋을 **아예 선언하지 않는** 이유는 이름 가림 때문에 선언만 하면 링크 에러가 나기 때문이다. 기반 no-op이 받게 두는 것이 유일하게 성립하는 형태다.

```cpp
// ★ 헤더에는 선언만. 정의는 EPInventoryComponent.cpp다 (15차) —
//   FEPInventoryList가 컴포넌트보다 위에 있어 여기서는 불완전 타입이다
// 엔트리가 아니라 직렬화기 쪽에 둔다
void FEPInventoryList::PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters&)
{
    // ★★ Owner는 TObjectPtr<UEPInventoryComponent>다 (15차, 03-2). 캐스트가 없다.
    if (Owner) Owner->OnInventoryChanged.Broadcast();
}
```

> *"If a function with the signature `void PostReplicatedReceive(...)` is defined in the derived struct, **it will be called after each call to NetDeltaSerialize on the receiving end**"*
> — `FastArraySerializer.h:517-519`

**수신 한 번당 정확히 한 번** 불리고 Add/Change/Remove를 구분할 필요가 없다 — Step 04는 어차피 전체 재생성이다. **Step 04의 "`PostReplicatedChange` 브로드캐스트 누락" 함정이 통째로 소멸한다.** 빠뜨릴 콜백이 없기 때문이다.

서버 쪽도 같은 모양으로 맞춘다.

```cpp
// ★★ 정의는 EPInventoryComponent.cpp 상단이다 — 헤더가 아니다 (13차)
//   쓰는 곳이 전부 이 .cpp 안이고, 헤더에는 friend 선언만 남는다
//   ★ 지금 소스는 friend 선언만 갖고 있어 컴파일되지 않는다 — 03-A 첫 빌드에서 막힌다
struct FScopedInventoryNotify        // 진입 ++ / 이탈 --
{                                    // 0이 되면 그때 한 번 Broadcast
    explicit FScopedInventoryNotify(UEPInventoryComponent* In) : C(In) { ++C->NotifyDepth; }
    ~FScopedInventoryNotify()
    {
        if (--C->NotifyDepth == 0) C->OnInventoryChanged.Broadcast();
    }
    UEPInventoryComponent* C;
};
```

> **`friend struct FScopedInventoryNotify;`는 선언이 아니다.** 타입 이름을 도입하지만 **불완전 타입**이라 인스턴스를 만들 수 없다. `.cpp` 상단에 정의를 두면 헤더는 `friend` 한 줄로 끝나고 **`NotifyDepth`·`OnInventoryChanged`가 private으로 남는다** — 그게 `friend`를 쓴 이유다.

#### ★★ 가드를 어디에 두는가 — **이미 있는 표를 쓴다** (13차 확정)

**초안은 별도 목록이었다** — *"`AddItem` / `AddSubtree` / `RemoveEntry` / `SetEntryCharges` 선두"*. **그 목록이 두 번 연속 낡았다:** 9차의 `MoveEntry`도 11차의 `ReorderEntry`/`AssignSortKey`도 올라오지 않았다.

**원인은 그게 이 문서의 다른 표와 따로 노는 두 번째 목록이라는 것이다.**

```
초안의 가드 목록 :  AddItem · AddSubtree · RemoveEntry · SetEntryCharges
단일 쓰기 지점   :  InsertEntry · SetEntryCharges · RemoveSelf · AssignSortKey · MoveEntry
```

**아래가 정확히 "`MarkItemDirty` / `MarkArrayDirty`를 부르는 함수 전부"다.** 그리고 그 표는 이미 성실히 관리된다 — 9차가 `MoveEntry`를, 11차가 `AssignSortKey`를 올렸고 *"`SwapEntries`(04-B)도 이걸 두 번 부른다"* 까지 적혀 있다.

**그래서 목록을 새로 쓰지 않는다. 가드를 그 표에 건다.**

| 층 | 어디 | 빠뜨리면 |
|---|---|---|
| **㉡ 재진입·필수** | **단일 쓰기 지점 다섯** — `InsertEntry` · `SetEntryCharges` · `RemoveSelf` · `AssignSortKey` · `MoveEntry` | **알림이 안 간다** |
| ㉠ 배칭·선택 | 공개 진입점 — `AddItem` · `AddSubtree` · `RemoveEntry` · `ReorderEntry` | 알림이 여러 번 간다 (**무해**) |

- **`ReorderEntry`는 가드가 없어도 된다** — `AssignSortKey`가 갖는다
- **`MoveEntry`는 필요하다** — `MarkItemDirty`를 직접 부른다
- **`SwapEntries`(04-B) · ⓐ의 재장전(§7-4) · §7-3 부착물은 자동으로 옳다** — 전부 다섯을 경유한다
- **새 규칙이 0개다.** 지킬 표가 하나로 합쳐진다

> **가드가 지키는 것의 크기를 정확히 적어둔다.** 클라 UI는 `PostReplicatedReceive`로 받으므로(가드와 무관) **서버 쪽 `Broadcast`의 소비자는 리슨서버 호스트 / PIE 화면뿐**이다. 배낭 하나 버릴 때 12회 `Broadcast` = 20칸 그리드 재생성 12회라 **㉠은 실측하면 비용이 아니다.** 가드를 유지하는 이유는 ㉠이 아니라 **㉡** 이다 — `RenormalizeSortKeys`가 `Entries.Items` 포인터를 들고 도는 자리가 실재한다.

> ### 기각 — `MarkItemDirty`를 감싼 사설 래퍼(`DirtyItem`/`DirtyArray`)
>
> **★ *"엔진 관례와 싸운다"* 를 근거로 쓰지 않는다 — 사실이 아니다 (13차 검수). 엔진이 감싼다.**
>
> ```cpp
> // AbilitySystemComponent_Abilities.cpp:980-996
> void UAbilitySystemComponent::MarkAbilitySpecDirty(FGameplayAbilitySpec& Spec, bool WasAddOrRemove)
> {
>     if (IsOwnerActorAuthoritative())
>     {
>         if (!(Spec.Ability && ...ServerOnly && !WasAddOrRemove))
>             ActivatableAbilities.MarkItemDirty(Spec);          // ← 감싼다
>         AbilitySpecDirtiedCallbacks.Broadcast(Spec);
>     }
>     else
>     {
>         ActivatableAbilities.MarkArrayDirty();                 // 클라 예측
>     }
> }
> ```
>
> 헤더가 **호출자에게 래퍼를 쓰라고 지시한다** — *"If modifying call MarkAbilitySpecDirty."*(`AbilitySystemComponent.h:1112`, 같은 문장이 네 곳). 호출자 다섯 곳이 전부 래퍼를 지나고 **`MarkItemDirty` 직접 호출은 래퍼 안 한 곳뿐이다.**
>
> **그래도 기각이 맞다. 근거는 이것이다 — 감싸는 이유는 안에 할 일이 있을 때다.**
>
> | ASC 래퍼가 하는 일 | 우리 `DirtyItem`이 할 일 |
> |---|---|
> | 권한 분기 | 없음 |
> | `ServerOnly` 어빌리티는 dirty 스킵 | 없음 |
> | `AbilitySpecDirtiedCallbacks.Broadcast` | 없음 — 알림은 스코프 가드가 낸다 |
> | 클라 예측 시 `MarkArrayDirty` | 없음 (**아직**) |
>
> **하나도 없다.** 그러면 남는 것은 이름 바꾸기다. 강제할 문법이 없다는 점도 **엔진이 같아서** ASC는 헤더 주석으로 부탁한다.
>
> **★ 마지막 줄이 예고다.** 04-8의 낙관적 클라 적용을 넣으면 ASC의 `else` 분기가 정확히 그 상황이 된다 — *"Clients predicting should call MarkArrayDirty to force the internal replication map to be rebuilt"*(`FastArraySerializer.h:993-994`). **그때는 래퍼 안에 할 일이 생긴다.** 기각 사유를 *"관례"* 로 적어두면 **그날 이 선례를 못 찾는다.**

- **`bNotifyPending` 같은 플래그를 두지 않는다.** 가드는 변형 함수에만 놓이고 모든 실패 검사가 가드보다 앞에 있으므로 `NotifyDepth > 0` 자체가 "보낼 것이 있다"는 뜻이다. 플래그를 두면 완전히 중복이거나, `MarkItemDirty` 호출부마다 손으로 세워야 해서 **가드가 없애려던 문제가 돌아온다**
- **`MoveEntry`도 알림을 쏜다.** Step 04가 장비 슬롯 12칸과 장착 강조를 그리므로(05-4) 안 쏘면 배낭을 매도 구획이 안 열리고 무기를 바꿔도 테두리가 안 옮겨간다
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

Step 00에는 `EP.Item.State`/`EP.Item.Dump`가, Step 01에는 `EP.Loot.RollTable`/`EP.Loot.Respawn`이 있는데 **배낭·서브트리·칸 합산·`bFungible`이 전부 몰린 Step 03에만 커맨드가 없었다.** UI는 Step 04라 완료 조건 19개 중 13개를 확인할 방법이 없다.

```
> EPInvDump                                # ★ 명령을 친 창에서 로컬 실행된다 (아래)
  EntryId  Parent  SlotId  ItemId          Charges  SlotSize  SortKey
  1        -1      Torso   Shirt_Basic     0        11              0   ← 슬롯. 칸을 안 먹는다
  2        -1      Legs    Pants_Basic     0         6          65536
  3        -1      Back    Backpack_B      0        10         131072
  4         1      -       Bandage         1         1              0
  5         3      -       Weapon_AK74    30         5              0   ← 부모가 달라 겹쳐도 된다
  ---
  Body : 0 / 10(테스트값, 최종 0)   Torso(1) : 1 / 10   Legs(2) : 0 / 5   Back(3) : 5 / 8
  NextEntryId = 6                                                        ← ★
  ※ 부모별로 묶고 그 안에서 SortKey 순으로 찍는다 (13차)
    SortKey는 형제 스코프라 전역 정렬은 뜻이 없다. 배열 순서로 찍으면 순서 버그가 Dump에 안 보인다

> EPInvDumpAll                             # 명령을 친 창의 월드에 있는 모든 인벤토리 컴포넌트
  Pawn_0 (내 폰)   Entries=5
  Pawn_1           Entries=0                ← 0이면 COND_OwnerOnly 통과

> EPInvAdd <ItemId> [Container]             # 서버로 라우팅. Container 기본 -1(본체) (13차)
> EPInvDrop <EntryId>                       # 서버로 라우팅
> EPInvReorder <EntryId> <PrevEntryId>      # 서버로 라우팅. -1이면 맨 앞 (11차)
> EPInvMove <EntryId> <NewParent> <SlotId>  # 서버로 라우팅. SlotId "-" 이면 NAME_None (13차)
> ※ 16차: 이름에 점이 없고 [PlayerIndex]가 없다 — 아래 CheatManagerExtension 절
```

### ★ 어디에 어떻게 적나 — 전용 파일, Step 01과 같은 형태 (15차 신설)

**`Private/Inventory/EPInventoryDebugCommands.cpp` 하나를 새로 만든다.** 헤더는 없다.

| | 어디에 | 왜 |
|---|---|---|
| Step 00 | `EPItemDefinitionSubsystem.cpp` 하단 (`:175`, `:206`) | 커맨드 둘이고 대상이 서브시스템 자신 |
| **Step 01** | **`Private/Loot/EPLootDebugCommands.cpp`** | 셋으로 늘고 대상이 여럿(스포너·픽업·테이블) |
| **Step 03** | **`Private/Inventory/EPInventoryDebugCommands.cpp`** | **여섯이고 대상이 폰의 컴포넌트다** |

**`UEPInventoryComponent.cpp`에 넣지 않는다.** 게임플레이 클래스에 `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)` 블록이 200줄 붙고, **커맨드가 private을 못 봐서 `friend`를 하나 더 만들고 싶어진다.** 커맨드는 public API만 써야 한다 — 그게 *"03-A의 계약이 닫혔다"* 를 증명하는 방식이기 때문이다.

#### 뼈대 — `UCheatManagerExtension` 하나에 여섯 (16차 개정)

**13차까지 여기 있던 `FAutoConsoleCommandWithWorldAndArgs` 뼈대는 폐기됐다.** `Play As Client` PIE에서 권한을 못 얻기 때문이다 — 근거·대체 형태·**여섯 개의 전체 본문**이 전부 아래 「그런데 `Play As Client`에서 서버 권한을 못 얻는다」 절에 한 벌로 있다.

폐기된 것: `FindInv` 헬퍼 · `[PlayerIndex]` 인자 · `NM_Client` 조기 반환 · `ECVF_Cheat` · `FCString::Atoi` 인자 파싱. 남는 것은 **반환값 로그**와 **`Dump` 둘을 클라 로컬로 두는 계약**뿐이다.

> **★ 가드는 `UE_WITH_CHEAT_MANAGER` 하나다** (`CheatManagerDefines.h:8` — `1 && !UE_BUILD_SHIPPING`). `ECVF_Cheat`는 콘솔 오브젝트의 플래그라 exec 함수에는 없고, `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)`도 쓰지 않는다 — **Test 빌드에서 검증 도구가 살아 있는 편이 낫다.** Step 00·01의 `EP.Item.*`·`EP.Loot.*` 다섯은 콘솔 오브젝트 그대로이므로 `ECVF_Cheat`를 계속 갖는다.

> **★ 반환값을 반드시 로그로 찍는다.** `MoveEntry`가 `bool`을 돌려주는데(03-2 반환 규약) 커맨드가 그걸 버리면 **검사 0~6 중 어디서 걸렸는지는 고사하고 걸렸다는 사실조차 안 보인다.** 13차가 잡은 결함 A-1(*"무동작인데 `true`를 반환한다"*)이 **바로 이 커맨드로 검증돼야 하는 것**이라, 여기서 반환값을 안 찍으면 그 검증이 성립하지 않는다.
>
> **★ 단 `ReorderEntry`는 `void`다** (`EPInventoryComponent.h:202`). *"정상 클라에서는 실패할 수 없는 연산"* 이라 반환값이 없는 것이 설계다 — 16차에 확인했고, 그 전 판본이 `bool`이라고 적었던 것은 틀렸다. `EPInvReorder`는 호출 사실만 찍고 **결과는 `EPInvDump`의 `SortKey` 열로 본다.**

> **★ `BlueprintAuthorityOnly`를 붙이지 않는 것은 `Dump` 둘뿐이다.** `EPInvDump`/`EPInvDumpAll`은 **명령을 친 창에서 로컬로 돌아야 의미가 있다** — `COND_OwnerOnly`가 제대로 걸렸는지(다른 폰의 `Entries`가 0인지)는 클라에서만 확인된다. 나머지 넷(`Add`/`Drop`/`Reorder`/`Move`)은 상태를 바꾸므로 지정자를 붙여 서버로 보낸다. **`NM_Client` 조기 반환은 이제 필요 없다** — 엔진이 로컬 실행 자체를 막는다(`CheatManager.cpp:130`).

> **★ 커맨드는 RPC를 안 지난다.** `EP.Inv.Move`가 `Server_MoveEntry`가 아니라 **내부 `MoveEntry`를 직접** 부른다 — `EP.Inv.Reorder`→`ReorderEntry`와 같은 형태이고, **14차가 `Server_EquipBackpack`을 없앤 근거가 이것**이다(03-2). 커맨드를 첫 호출자로 세면 RPC 표면을 소비자 없이 여는 셈이 된다.

> ### ★ `EP.Inv.Add`의 `[Container]` — 없으면 본체 0칸 전환이 03-A를 통째로 죽인다 (13차 신설)
>
> 지금 커맨드는 컨테이너를 지정할 방법이 없어 **본체로만 넣는다.** `MaxSlots`가 0이 되면(§8 미정 #9):
>
> | | `MaxSlots = 0` 이후 |
> |---|---|
> | 완료 조건 2~6 | **전부 실패. 검증 불가** |
> | 완료 조건 14·15·17~19 | `EP.Inv.Add`로 **아이템을 만들 수 없다** → 전부 검증 불가 |
>
> **즉 전환 후에는 03-A 완료 조건 아홉 개를 다시 돌릴 수 없다 — 회귀 테스트를 잃는다.** `EP.Inv.Move`가 이미 `<NewParent>`를 받으므로 **어휘가 늘지 않는다.**
>
> **그리고 전환 시점의 답이 여기서 나온다:** 상의를 인벤토리에 넣으려면 `SlotSize 11`이라 본체 10칸으로도 안 되므로, **`MaxSlots`를 내리는 것은 `StartingEquipment`(03-B)가 도는 시점과 같다.** §8 미정 #9에 적었다.

> ### ★★ `EP.Inv.Move`가 없으면 `MoveEntry`가 **Step 04까지 한 줄도 안 돈다** (13차 신설)
>
> 03-A가 만드는 것 중 **호출자가 없는 코드가 통째로 하나 있었다.**
>
> | 완료 조건 | 필요한 호출 | 이 커맨드가 없으면 |
> |---|---|---|
> | 15. 다른 컨테이너로 옮기면 목적지 맨 뒤 (함정 4m) | `MoveEntry(id, 배낭, None)` | **검증 불가** |
> | 17. 핫바에 꽂았다 빼면 원래 자리 (함정 4q) | `MoveEntry(id, -1, "Hotbar1")` → `(id, -1, None)` | **검증 불가** |
>
> `Server_MoveEntry`는 Step 03에서 안 열고(03-2), `Server_EquipBackpack`은 **아예 없다**(14차). **이 커맨드가 Step 03의 유일한 착용·이동 표면이다.** 04-A의 `EP.Inv.Equip`도 이것의 얇은 별칭이라 없앤다 — `EP.Inv.Move <id> -1 Torso`가 정확히 그 일이다(`05_Loot_04_InventoryUI.md:45`).
>
> **커맨드가 내부 함수 `MoveEntry`를 직접 부른다** — `EP.Inv.Reorder`가 `ReorderEntry`를 직접 부르는 것과 **같은 형태이고 같은 이유**다. RPC 표면을 열지 않는다.
>
> **11·12차가 가장 공들인 두 함정(4q 동률 · 4m 키 미재발급)과 검사 0~6 일곱 개, 정합(4i)·사이클(4j)이 전부 여기서 처음 실행된다.** 안 넣으면 Step 04에서 터지고, 4q의 증상은 *"손으로 맞춰둔 배치가 조금씩 무너진다"* 라 **UI 버그로 오진한다.**

> **★ `EP.Inv.Reorder`가 없으면 순서 계약을 UI보다 먼저 닫을 수 없다.** `ReorderEntry`·`GetSortedContents`·재정규화는 전부 03-A인데 **RPC 표면과 드래그는 04-B다.** 커맨드가 **일반 함수 `ReorderEntry`를 직접 부르므로** RPC 없이 계약이 닫힌다 — 04-A가 `EP.Inv.Add`/`EP.Inv.Move`를 요구하는 것과 같은 이유다.

| 커맨드 | 권한 | 이유 |
|---|---|---|
| `Dump` / `DumpAll` | **클라 로컬** | 순수 조회. Step 00의 `EP.Item.State`와 같은 구분 |
| `Add` / `Drop` / `Reorder` / `Move` | 서버 | 상태를 바꾼다 |

### ★★ 그런데 `Play As Client`에서 **서버 권한을 못 얻는다** — 형태를 바꾼다 (16차 신설)

**위 표는 "서버 전용"이라고만 적었지 어떻게 서버에서 실행하는지를 안 적었다.** 데디케이티드 서버 PIE(`Play As Client`)에서 클라 창의 콘솔에 친 커맨드는 **클라 월드에서 돈다.** `GetOwner()->HasAuthority()`가 거짓이라 넷은 즉시 반환하고 끝난다.

**`ServerExec`로 넘기는 것은 안 된다.** 엔진 소스가 답을 준다 — 콘솔 오브젝트(`FAutoConsoleCommand*`)는 `UEngine::Exec` 안의 한 줄에서만 처리되는데,

```cpp
// UnrealEngine.cpp:5415
else if (IConsoleManager::Get().ProcessUserConsoleInput(Cmd, Ar, InWorld))
```

`ServerExecRPC_Implementation`(`PlayerController.cpp:1879`)이 서버에서 타는 경로는 `UPlayer::Exec`(`Player.cpp:98`)이고, **그 안에는 `ProcessConsoleExec` 체인만 있다** (`Player.cpp:126-157` — PlayerInput · PC · Pawn · HUD · GameMode · **CheatManager** · GameState · CameraManager). **콘솔 매니저가 없다.**

> **즉 `ServerExec`로 서버에 보낼 수 있는 것은 `UFUNCTION(exec)` 뿐이고, `FAutoConsoleCommand`는 `Command not recognized`가 뜬다.**

그래서 **커맨드를 `UCheatManagerExtension`의 exec 함수로 바꾼다.** Lyra가 쓰는 형태이고, 엔진이 클라→서버 라우팅을 대신 해준다.

```cpp
// CheatManager.cpp:95-131
// If on the client and calling a cheat function marked as BlueprintAuthorityOnly,
// automatically route it through the ServerExec() RPC to the server
if ((Function != nullptr) && Function->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly))
{
    MyPC->ServerExec(Cmd);
    return true;
}
```

#### 자리 — `EPInventoryDebugCommands.cpp`를 대체한다

| | 파일 | 비고 |
|---|---|---|
| 신설 | `Public/Inventory/EPInventoryCheats.h` | `UEPInventoryCheats : public UCheatManagerExtension` |
| 신설 | `Private/Inventory/EPInventoryCheats.cpp` | |
| 수정 | `Public/Core/EPPlayerController.h` · `.cpp` | `AddCheats` 오버라이드 **한 개** |
| 폐기 | `Private/Inventory/EPInventoryDebugCommands.cpp` | 여섯이 전부 옮겨간다 |

**Step 00·01의 `EP.Item.*`·`EP.Loot.*`는 그대로 둔다.** 그것들은 서브시스템·월드 조회라 권한이 필요 없다. 바뀌는 것은 **폰의 컴포넌트를 건드리는 Step 03의 여섯뿐**이다.

#### 헤더

```cpp
#pragma once

#include "GameFramework/CheatManager.h"
#include "EPInventoryCheats.generated.h"

class UEPInventoryComponent;

/** Step 03 인벤토리 검증 커맨드. 클라 콘솔에 쳐도 서버에서 돈다 */
UCLASS()
class UEPInventoryCheats : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	UEPInventoryCheats();

	// ── 조회. 로컬에서 돈다 (클라의 복제본을 본다) ──
	UFUNCTION(Exec) void EPInvDump();
	UFUNCTION(Exec) void EPInvDumpAll();

	// ── 변경. BlueprintAuthorityOnly가 서버로 보낸다 ──
	UFUNCTION(Exec, BlueprintAuthorityOnly) void EPInvAdd(const FString& ItemId, int32 Container = -1);
	UFUNCTION(Exec, BlueprintAuthorityOnly) void EPInvDrop(int32 EntryId);
	UFUNCTION(Exec, BlueprintAuthorityOnly) void EPInvReorder(int32 EntryId, int32 PrevEntryId);
	UFUNCTION(Exec, BlueprintAuthorityOnly) void EPInvMove(int32 EntryId, int32 NewParent, const FString& SlotId);

private:
	UEPInventoryComponent* GetInv() const;
};
```

**이름에 점을 못 쓴다.** exec 함수 이름은 식별자라 `EP.Inv.Move`가 안 된다 → **`EPInvMove`.** 문서 전체의 `EP.Inv.*` 표기는 이 규칙으로 읽는다(`05_Loot_04_InventoryUI.md:45` 포함).

**인자 파싱이 사라진다.** `FCString::Atoi`도 `Args.Num() < 2` 검사도 필요 없다 — UHT가 `int32`로 받아준다. `SlotId`만 `FString`인데, exec 인자는 `FName`을 못 받기 때문이다.

#### 등록 — CDO 생성자에서 (`LyraBotCheats.cpp:14-24`와 같은 형태)

```cpp
UEPInventoryCheats::UEPInventoryCheats()
{
#if UE_WITH_CHEAT_MANAGER
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda(
			[](UCheatManager* CheatManager)
			{
				CheatManager->AddCheatManagerExtension(NewObject<ThisClass>(CheatManager));
			}));
	}
#endif
}
```

> **★★ Lyra가 씌운 `WITH_SERVER_CODE` 가드를 따라 하지 않는다.** `LyraBotCheats`는 서버에서만 등록하는데, **우리는 클라에도 붙어 있어야 한다.** 위 라우팅 코드가 `FindFunction`으로 **클라의 확장 목록**을 뒤져 `BlueprintAuthorityOnly`인지 보고 나서야 `ServerExec`를 부르기 때문이다. 서버에만 등록하면 클라 콘솔에서 그냥 *"not recognized"* 가 뜨고, **증상이 "커맨드를 안 만들었다"와 구별되지 않는다.** `ULyraTeamCheats`는 가드가 없다 — 그쪽이 우리 경우다.

#### 전제 — 클라에는 CheatManager가 **기본으로 안 생긴다**

```cpp
// PlayerController.cpp:1166
if ( (World->GetAuthGameMode() && World->GetAuthGameMode()->AllowCheats(this)) || bForce)
{
    CheatManager = NewObject<UCheatManager>(this, CheatClass);
```

**클라에는 `GetAuthGameMode()`가 없다.** `bForce` 없이는 `CheatManager`가 `nullptr`이고, 그러면 위 라우팅이 아예 실행되지 않는다. Lyra가 강제하는 이유가 이것이다(`LyraPlayerController.cpp:323`):

```cpp
void AEPPlayerController::AddCheats(bool bForce)
{
#if UE_WITH_CHEAT_MANAGER          // CheatManagerDefines.h:8 — (1 && !UE_BUILD_SHIPPING)
	Super::AddCheats(true);
#else
	Super::AddCheats(bForce);
#endif
}
```

**`CheatClass`가 비어 있어도 안 생긴다**(`:1160`). `APlayerController::CheatClass`의 기본값이 `UCheatManager`라 보통은 문제없지만, BP에서 비웠다면 여기서 조용히 끝난다.

#### 본문 — 여섯 전부 (권한 분기가 사라진다)

```cpp
// Private/Inventory/EPInventoryCheats.cpp
#include "Inventory/EPInventoryCheats.h"
#include "Inventory/EPInventoryComponent.h"
#include "Inventory/EPInventoryTypes.h"
#include "Core/EPCharacter.h"
#include "Data/EPItemDefinitionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"                    // TActorIterator — DumpAll

UEPInventoryCheats::UEPInventoryCheats()
{
#if UE_WITH_CHEAT_MANAGER
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UCheatManager::RegisterForOnCheatManagerCreated(FOnCheatManagerCreated::FDelegate::CreateLambda(
			[](UCheatManager* CheatManager)
			{
				CheatManager->AddCheatManagerExtension(NewObject<ThisClass>(CheatManager));
			}));
	}
#endif
}

UEPInventoryComponent* UEPInventoryCheats::GetInv() const
{
	// 서버에서는 '명령을 친 그 클라'의 PC다 — ServerExec이 그 PC의 RPC이므로
	const APlayerController* PC = GetPlayerController();        // CheatManager.h:80
	const AEPCharacter* C = PC ? Cast<AEPCharacter>(PC->GetPawn()) : nullptr;
	if (!C) { UE_LOG(LogTemp, Error, TEXT("[Inv] 폰이 없습니다.")); return nullptr; }
	return C->GetInventoryComponent();
}

// ── 조회. 친 창에서 로컬로 돈다 ──────────────────────────────────────

void UEPInventoryCheats::EPInvDump()
{
	const UEPInventoryComponent* Inv = GetInv();
	if (!Inv) return;

	// ★ 배열 순서로 찍지 않는다 — 그러면 순서 버그가 Dump에 안 보인다 (13차)
	//   부모로 묶고, 슬롯을 먼저, 그 안에서 SortKey 오름차순
	TArray<FEPInventoryEntry> Rows = Inv->GetEntries();
	Rows.StableSort([](const FEPInventoryEntry& A, const FEPInventoryEntry& B)
	{
		if (A.ParentEntryId != B.ParentEntryId) return A.ParentEntryId < B.ParentEntryId;
		const bool bASlot = !A.SlotId.IsNone(), bBSlot = !B.SlotId.IsNone();
		if (bASlot != bBSlot) return bASlot;                    // 슬롯이 위
		return A.SortKey < B.SortKey;
	});

	UE_LOG(LogTemp, Log, TEXT("EntryId  Parent  SlotId  ItemId  Charges  SortKey"));
	for (const FEPInventoryEntry& E : Rows)
	{
		UE_LOG(LogTemp, Log, TEXT("%7d %7d  %-8s %-16s %7d %8d"),
			E.EntryId, E.ParentEntryId,
			E.SlotId.IsNone() ? TEXT("-") : *E.SlotId.ToString(),
			*E.ItemId.ToString(), E.State.Charges, E.SortKey);
	}

	// 컨테이너마다 독립 풀이 열렸는지 — 본체 + 용량이 있는 엔트리 전부
	UE_LOG(LogTemp, Log, TEXT("Body : %d / %d"),
		Inv->GetUsedSlots(INDEX_NONE), Inv->GetCapacity(INDEX_NONE));
	for (const FEPInventoryEntry& E : Rows)
	{
		const int32 Cap = Inv->GetCapacity(E.EntryId);
		if (Cap <= 0) continue;                                  // 컨테이너가 아니다
		UE_LOG(LogTemp, Log, TEXT("%s(%d) : %d / %d"),
			*E.ItemId.ToString(), E.EntryId, Inv->GetUsedSlots(E.EntryId), Cap);
	}
	// ★ NextEntryId 열은 없다 — private(서버 전용)이고 public 게터가 없다.
	//   완료 조건 '재번호 없음'은 EntryId 열만으로 닫힌다 (셋 넣고 2번을 버린 뒤 하나 더 → 1,3,4)
}

void UEPInventoryCheats::EPInvDumpAll()
{
	UWorld* World = GetWorld();                                  // CheatManager.h:77
	if (!World) return;

	// ★ 반드시 '친 창'의 월드다. 서버로 보내면 다 보이므로 COND_OwnerOnly 검증이 무의미해진다
	for (TActorIterator<AEPCharacter> It(World); It; ++It)
	{
		const UEPInventoryComponent* Inv = It->GetInventoryComponent();
		UE_LOG(LogTemp, Log, TEXT("%-24s Entries=%d"),
			*It->GetName(), Inv ? Inv->GetEntries().Num() : -1);
	}
}

// ── 변경. BlueprintAuthorityOnly가 서버로 보낸다 ─────────────────────

void UEPInventoryCheats::EPInvAdd(const FString& ItemId, int32 Container)
{
	UEPInventoryComponent* Inv = GetInv();
	if (!Inv) return;

	const UEPItemDefinitionSubsystem* Defs = UEPItemDefinitionSubsystem::Get(Inv);
	FEPItemState State;
	if (!Defs || !Defs->MakeItemState(FName(*ItemId), State)) return;   // 실패 로그는 그쪽이 찍는다

	const int32 NewId = Inv->AddItem(Container, FName(*ItemId), State);
	UE_LOG(LogTemp, Log, TEXT("[Inv] Add(%s -> %d) = %d"),
		*ItemId, Container, NewId);                              // INDEX_NONE이면 자리가 없었다
}

void UEPInventoryCheats::EPInvDrop(int32 EntryId)
{
	UEPInventoryComponent* Inv = GetInv();
	if (!Inv) return;

	// ★ 서버에서 Server_ RPC를 부르면 구현이 그대로 로컬 실행된다 — 03-B의 정식 경로를 그대로 탄다
	Inv->Server_DropItem(EntryId);
	UE_LOG(LogTemp, Log, TEXT("[Inv] Drop(%d) 요청. 결과는 EPInvDump / EP.Loot.List로 본다"), EntryId);
}

void UEPInventoryCheats::EPInvReorder(int32 EntryId, int32 PrevEntryId)
{
	UEPInventoryComponent* Inv = GetInv();
	if (!Inv) return;

	Inv->ReorderEntry(EntryId, PrevEntryId);                     // ★ void다 (헤더 :202)
	UE_LOG(LogTemp, Log, TEXT("[Inv] Reorder(%d, prev=%d). SortKey는 EPInvDump로 본다"),
		EntryId, PrevEntryId);
}

void UEPInventoryCheats::EPInvMove(int32 EntryId, int32 NewParent, const FString& SlotId)
{
	UEPInventoryComponent* Inv = GetInv();
	if (!Inv) return;

	const FName Slot = (SlotId == TEXT("-")) ? NAME_None : FName(*SlotId);
	const bool bOk = Inv->MoveEntry(EntryId, NewParent, Slot);   // ★ 내부 함수 직접. RPC 안 지난다
	UE_LOG(LogTemp, Log, TEXT("[Inv] Move(%d -> %d/%s) = %s"),
		EntryId, NewParent, *SlotId, bOk ? TEXT("OK") : TEXT("거절"));
}
```

**`if (World->GetNetMode() == NM_Client) return;` 가드가 전부 없어진다.** 변경 넷은 **서버에서만 실행된다** — 클라에서 친 것은 `ProcessConsoleExec`이 가로채 `ServerExec`로 보내고 로컬 실행은 하지 않는다(`CheatManager.cpp:130` `return true`). 반대로 `EPInvDump`/`EPInvDumpAll`은 지정자가 없으므로 **친 창에서 그대로 돈다.**

**여섯 다 public API만 쓴다.** `friend`를 하나도 더 만들지 않는 것이 *"03-A의 계약이 닫혔다"* 를 증명하는 방식이다 — 그래서 `NextEntryId`(private)와 `MaxSlots`(protected)는 직접 못 읽고, 각각 **`EntryId` 열**과 **`GetCapacity(INDEX_NONE)`** 로 대신한다.

> **★ 반환값 로그는 그대로 필수다.** 오히려 더 중요해진다 — 클라에서 쳤을 때 `UE_LOG`는 **서버 쪽에 찍힌다.** PIE 단일 프로세스면 같은 Output Log에 섞여 보이지만, 별도 프로세스로 띄우면 서버 창을 봐야 한다. `ClientMessage`로 돌려받고 싶으면 `ServerExecRPC_Implementation`(`PlayerController.cpp:1882`)이 `ConsoleCommand`의 **반환 문자열**만 보내므로 로그 대신 `Ar.Logf` 형태로 바꿔야 하는데 — **지금은 필요 없다.**

#### 제약 셋

| | |
|---|---|
| **128자** | `APlayerController::ServerExec`이 `Msg.Left(128)`로 자른다(`PlayerController.cpp:1890-1895`). 우리 커맨드는 최장 `EPInvMove 12 -1 Hotbar1` 정도라 여유가 크다 |
| **셰이핑 없음** | `UE_WITH_CHEAT_MANAGER`가 `!UE_BUILD_SHIPPING`. 기존 `#if !(UE_BUILD_SHIPPING \|\| UE_BUILD_TEST)`보다 **약하다**(Test 빌드에서 살아 있다). 검증 도구로는 그게 낫다 |
| **`ECVF_Cheat` 개념이 없다** | 콘솔 오브젝트가 아니므로 해당 플래그가 없다. 대신 위 컴파일 가드가 그 역할을 한다 |

### ★ `COND_OwnerOnly` 검증은 **어느 창에서 치느냐**로 한다 — `[PlayerIndex]`가 사라진다 (16차 개정)

§10이 *"리슨서버 호스트가 아니라 클라이언트 쪽에서 확인"* 을 원칙으로 세웠다. **13차까지의 답은 `[PlayerIndex]` 인자였다** — 서버 커맨드가 호스트 인벤토리만 채우면 `DumpAll`의 `Entries=0`이 *"복제가 막혔다"* 가 아니라 *"애초에 아무것도 안 넣었다"* 를 보여줄 뿐이라, **검증이 통째로 위양성이 되기** 때문이다.

**CheatManager 라우팅이 이 문제를 원인 단에서 없앤다.** 서버에서 `GetPlayerController()`는 **명령을 친 바로 그 클라의 PC**다(`ServerExec`이 그 PC의 RPC이므로). PIE 2번 창에서 `EPInvAdd Bandage`를 치면 2번 플레이어에게 들어간다.

| | 13차 (콘솔 오브젝트) | **16차 (CheatManager)** |
|---|---|---|
| 대상 지정 | `[PlayerIndex]` 인자 + `GetPlayerControllerIterator()` 순회 | **명령을 친 창** |
| `FindInv` 헬퍼 | 필요 | **불필요** — `GetInv()` 세 줄 |
| 인자 개수 | 넷 다 하나씩 더 | **원래 인자만** |
| 위양성 위험 | 인덱스를 잘못 세면 조용히 다른 플레이어를 건드린다 | **없다** |

**검증 절차가 이렇게 된다.**

```
PIE 창 2 (클라 A) 콘솔> EPInvAdd Bandage       # 서버로 라우팅 → 클라 A에게 들어간다
PIE 창 2 콘솔>          EPInvDump              # 로컬. Bandage가 보인다
PIE 창 3 (클라 B) 콘솔> EPInvDumpAll           # 로컬. A의 폰이 Entries=0 이면 COND_OwnerOnly 통과
```

> **`DumpAll`을 서버로 보내면 안 되는 이유가 여기서 분명해진다.** `BlueprintAuthorityOnly`를 붙이는 순간 서버에서 돌고, 서버는 **원래 다 보이므로** `Entries=0`이 영영 안 나온다. **검증이 조용히 무의미해진다** — `Dump` 둘에 그 지정자를 붙이지 않는 것이 계약이다.

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
| `Torso(N) : x / y` · `Back(N) : x / y` | 착용 컨테이너마다 **독립 풀**이 열린다 (배낭 자동 착용 포함) |
| `EP.Inv.Move` 후의 `Parent` · `SlotId` · `SortKey` | **완료 조건 15·17.** 다른 컨테이너로 옮기면 맨 뒤(4m), 핫바에 꽂았다 빼면 원래 자리(4q) |
| `DumpAll`의 `Entries=0` | **`COND_OwnerOnly`.** 패킷 캡처도 UI도 필요 없다 |

> **시나리오 자동화(`EP.Inv.Stress` 같은 것)는 넣지 않는다.** 검증할 대상보다 검증 도구가 커진다. 위 출력이면 손으로 세 번 눌러 확인된다.

> **월드 픽업 목록은 여기가 아니라 Step 01의 `EP.Loot.List`다** — 픽업 도구가 두 문서로 갈리지 않게. `DropCooldown`과 버려진 배낭의 `Payload` 개수를 그쪽에서 본다.

---

## 함정

| # | 함정 | 증상 | 대응 |
|---|---|---|---|
| 1 | `COND_OwnerOnly` 누락 | 남의 가방이 전부 복제됨 (치트) | 03-2 |
| 2 | `EntryId` 없이 배열 인덱스 사용 | 줍고 버릴 때마다 목록 순서가 튀고, 경쟁 시 엉뚱한 아이템을 버림 | 03-1 |
| **3b** | **캐스케이드가 자기 자신만 장착/컨테이너 검사** | 착용 컨테이너 안에 든 것들이 write-back·정리 없이 사라진다. **9차 확정으로 "배낭 속 무기 장착"은 표현 불가능해졌지만**(장착＝`ParentEntryId == INDEX_NONE`) 계약은 그대로다 — 핫바 5~0이 컨테이너 안 아이템을 들 수 있게 되면 원래 모양이 되살아난다 (`EquipmentSlots.md` §10 미정 #7) | 캐스케이드가 `RemoveEntry` 재귀 (03-2) ★★ |
| **3d** | **`Entries.Owner = this` 누락** | 서버는 정상인데 **클라 UI가 영원히 갱신 안 됨.** 원인이 UI/복제로 보인다 | 생성자 (03-2) ★★ |
| 3e | 순회하며 자식 제거 | `RemoveAtSwap`이라 인덱스가 튀어 **일부 자식을 건너뛴다.** "가끔 고아가 남는다" | 자식 목록을 먼저 뜬다 (03-2) |
| **3g** | **`bIsRoot`를 public 시그니처에 둠** | 밖에서 `false`를 넘기면 루트 정규화가 생략돼 `AddSubtree`가 루트를 못 찾는다. **증상이 3c와 같다** | `RemoveEntryInternal` private (03-2) |
| 4 | `MarkItemDirty` 누락 | 서버만 맞고 클라 갱신 안 됨 | 원시 엔트리 비노출 + `AddEntryCharges()` / `InsertEntry()` (03-2) |
| 4c | **`FindData()` null을 조용히 넘김** | 그 아이템이 칸을 0으로 먹어 **가방이 무한대**가 된다. 무증상 | `ensure` + 경고 (03-3) |
| 4d | `check(HasAuthority())` | Shipping에서도 크래시. 프로젝트 관례와 다름 | early return (03-3) |
| **4e** | **항목 콜백 3종을 선언만 함** | **링크 에러**(unresolved external). 이름 가림이라 "나중에 채운다"가 성립하지 않는다 | 아예 선언하지 않는다 (03-1) |
| **4f** | **`FindFungibleEntryId`에 컨테이너 인자 없음** | 배낭 속 현금이 본체 현금과 합쳐진다. **무증상이다가 배낭을 벗으면 돈이 딸려 나간다** | `(Container, ItemId)` (03-3) |
| **4g** | **살아있는 엔트리에 통째 대입(`E = Other`)** | 복제 ID가 리셋돼 수신 측이 **삭제+추가**로 본다. 내부 struct 델타가 사라진다 | 필드 단위 대입 (03-1) |
| **4h** | **`GetEntryInSlot`에 컨테이너(부모) 인자 없음** | `Optic`은 무기마다 하나씩 있는 슬롯이다. AK에 조준경을 달면 **M4에는 영원히 못 단다.** 4f와 같은 모양이고 **부착 슬롯이 생기는 날 처음 나타난다** | `(Parent, SlotId)` (03-2) ★★ |
| **4i** | **`SlotId`↔`ParentEntryId` 정합 검사 없음** | *"가방 안에 든 상의를 입고 있다"* 가 표현된다. 그 상의는 **칸도 안 먹고 착용 효과도 받는다** | `BodySlots` 검사 (03-2 `MoveEntry`) ★★ |
| **4j** | **`MoveEntry`에 사이클 검사 없음** | 배낭을 자기 안에 넣으면 `RemoveEntry` 재귀가 무한이 된다. 증상이 예외도 로그도 아니라 **전용 서버 프로세스가 멈추는 것** | 부모 사슬 5줄 (03-2) ★★ |
| **4k** | **`RemoveEntryInternal`에서 write-back을 `RemoveSelf` 뒤로** | 장착 진실이 파생 게터가 되면서 `RemoveSelf` 이후 조회가 `INDEX_NONE`이다. **write-back이 아예 안 불려 잔탄이 조용히 사라진다.** 증상이 *"0으로 덮인다"* 에서 *"안 불린다"* 로 바뀌어 **재현이 더 어려워졌다** — 틀린 값이 보이는 것은 발사 후 write-back이 밀린 그 한 발뿐이다 | ①→②→③→④ 순서 (03-2) ★★ **＋ 코드 주석에도 적는다** (순서를 바꿔도 컴파일된다) |
| **4l** | **`MoveEntry`에 제자리 거절(검사 0)이 없음** | 목적지가 지금 자리와 같으면 검사 5의 `CanFit`이 **자기 크기를 두 번 센다.** *"가방이 좀 차면 제자리 이동이 실패한다"* — 널널하면 되므로 **재현 조건이 용량**이다 | 검사 0 (03-2). Step 04가 안 부르게 됐어도 `Server_MoveEntry`는 열려 있다 |
| **4m** | **`MoveEntry`가 부모 변경 시 `SortKey`를 재발급하지 않음** | 옛 컨테이너의 키를 들고 가서 **목적지 맨 뒤가 아니라 엉뚱한 자리에 꽂힌다.** 두 컨테이너의 키 범위가 겹치는 것이 정상이라 거의 항상 나지만 *"가끔 이상한 자리"* 로 인지된다 | 검사 통과 후 `AssignSortKey(Id, KeySpace_NextAtEnd(NewParent))` (03-2) ★★ |
| **4n** | **스냅샷에서 자식 `SortKey`까지 버림** (루트와 같이 0으로) | 배낭을 버렸다 주우면 **내용물 순서가 뒤섞인다.** `AddSubtree`가 `In` 배열 순서를 따르는데 그건 `Entries.Items` 순회 결과라 **화면 순서가 아니다.** 아이템이 두세 개면 안 보인다 | `bIsRoot`일 때만 `SortKey = 0` (03-2 ②) ＋ `AddSubtree`가 자식 키 복원 (03-4) ★★ |
| **4o** | **재정규화를 안 넣음** | 같은 틈에 ~16회 꽂으면 `(Prev+Next)/2 == Prev`가 되어 **두 아이템이 같은 키를 갖는다.** 그 뒤로는 클라·서버가 다른 순서를 그린다(`FastArraySerializer.h:54`). **정상 조작으로 도달하는 지점**이라 사이클 검사와 달리 "도달 불가 방어"가 아니다 | `RenormalizeSortKeys` (03-2) ＋ **완료 조건 16** ★★ |
| **4p** | **슬롯에 든 엔트리의 키를 0으로 밀어둠** | 초안이 그랬다. 빼는 순간 **맨 앞에 꽂히고**, 그 전에 발급된 키와 **동률**이 난다(4q와 같은 뿌리) | 슬롯 여부로 갈리지 않는다 — `InsertEntry`도 `MoveEntry`도 **부모만** 본다 (03-2, 11차 검수) |
| **4q** | **★ 키 공간을 `GetSortedContents`로 구함** (슬롯을 거른 채) | **정상 플레이에서 동률이 난다** — 무기를 핫바에 꽂고(키가 표시에서 빠짐) 뭘 줍고(같은 키 발급) 무기를 뺀다. 크래시는 아니고 **손으로 맞춰둔 배치가 조금씩 무너진다.** 그리고 *"겹칠 문법이 없다"* 는 서술 때문에 `AssignSortKey`부터 뒤지게 된다 | `KeySpace_NextAtEnd`·`RenormalizeSortKeys`가 **부모만** 본다 (03-2) ★★ |
| **4r** | **★ 재정규화를 이분 고갈에만 검** | 맨 앞(`-Step`)·맨 뒤(`+Step`)가 **무한 증감**이다. `int32` 오버플로가 나면 부호가 뒤집혀 **맨 앞으로 보낸 것이 맨 뒤에 나타나고, 재정규화가 안 걸리므로 영구적**이다. `KeySpace_NextAtEnd`는 **줍기마다** 최대 키를 Step만큼 올려 스태시(5단계)에서 누적된다 | 세 분기 공통 경계 가드 ＋ `KeySpace_NextAtEnd` 안의 가드 (03-2) ★★ |
| **4s** | **★ `ReorderEntry`가 틈을 `GetSortedContents`에서 구함** | **4q와 같은 뿌리인데 11차 검수가 놓쳤다.** 보이는 두 이웃 사이에 **슬롯 형제가 끼어 있으면** 중점이 그 키와 겹친다 — 붕대 0 / AK 65536(슬롯) / 구급상자 131072에서 *"붕대 뒤"* 가 정확히 65536이다. 세 분기 전부 해당 | `Prev`는 보이는 목록, **틈은 부모 전체** (03-2) ★★ |
| **4t** | **★★ 재정규화 가드에서 `PrevEntryId != INDEX_NONE`을 뺌** | 맨 앞 분기는 설계상 `NewKey == PrevKey`라 고갈 조건이 **항상 참**이다. 재정규화해도 같은 조건이 다시 참 → **무한 재귀 → 전용 서버 프로세스 종료.** **아이템 2개 이상인 컨테이너에서 하나를 맨 앞으로 끌면 즉사한다** — 가장 흔한 정리 동작이다 | 경계(`bOutOfRange`)와 고갈(`bNoGap`)을 **분리** ＋ `ReorderEntryInternal(bRetry)` ＋ `ensure` (03-2) ★★ |
| **4u** | **제자리 드롭에 조기 반환 없음** | 드래그 취소는 *"도로 놓기"* 라 가장 흔한데, 매번 새 중점이 계산돼 `MarkItemDirty`가 나간다. **틈이 매번 반으로 줄어** 16번이면 재정규화가 돈다 — 사용자는 아무것도 안 바꿨는데 | `CurPrev == PrevEntryId`면 `return` (03-2). `MoveEntry` 검사 0과 대칭 |
| **4v** | **★★ `MoveEntry`가 `FindEntry`의 복사본에 씀** | `FindEntry`는 값 복사다(03-2). 바로 위 검사 6이 `FEPInventoryEntry E;`로 같은 이름의 지역 복사본을 만들어 놓아 **그대로 이어 붙이면 마지막으로 방문한 조상의 복사본에 쓴다.** 컴파일되고 **`true`를 반환하는데 배열은 그대로다** — 장착·배낭 매기가 통째로 무동작. **그리고 무해하지도 않다**: `MarkItemDirty`가 `MarkArrayDirty`를 부르고(`FastArraySerializer.h:441-454`) 복사본은 `ReplicationID == INDEX_NONE`이라 **매 호출 `IDCounter`를 소진하며 전체 델타 재스캔이 돈다.** 프로파일러에 직렬화가 잡혀 *"복제는 도는데 값이 안 온다"* 로 읽히고 **복제 쪽을 판다** | 배열 원소의 **참조**를 잡는다. `SetEntryCharges`가 이미 그 형태다 (03-2) ★★ |
| **4w** | **★★ `SortKey` 함수가 `INDEX_NONE`을 실패 센티널로 씀** | `EntryId`는 1부터라 안전하지만 **`SortKey`는 −1도 0도 유효한 값이다.** 맨 앞 이동이 키를 음수로 내리고 `(−65536, 0)`에 16회 꽂으면 **정확히 −1**이 나온다(완료 조건 18 ①의 경로). 그 형제를 `KeySpace_NextAbove`가 찾아도 호출자가 *"없다"* 로 읽어 **한 칸 건너뛴 자리에 놓이고 재정규화도 안 걸린다** | 성패를 `bool`로, 값을 out 파라미터로 (03-2) ★★ |
| **4x** | **★★ 키를 배열 상태를 바꾼 *뒤에* 구한다 — `InsertEntry`와 `MoveEntry` 둘 다** | **`InsertEntry`:** 아직 값이 안 들어간 자기 자신(`SortKey=0`)이 키 공간의 형제로 잡혀 *"빈 컨테이너면 0"* 분기가 **죽고**, 키가 전부 음수인 컨테이너에서 최대가 0이 된다. **`MoveEntry`(13차 답변):** 재부모를 먼저 하면 **옛 컨테이너의 키를 든 자기 자신**을 목적지의 형제로 센다 — 순서(맨 뒤)는 지켜지지만 **키가 컨테이너 사이로 전염되고**(본체 1,000,000 → 배낭에서 1,065,536) 재정규화 빈도가 이유 없이 오른다. 둘 다 **명세와 코드가 다른 동작을 말하게 된다** | 키를 **배열 상태를 바꾸기 전에** 구한다 — `InsertEntry`는 `AddDefaulted` 전(03-3), `MoveEntry`는 재부모 전(03-2) ★★ |
| **4z** | **★★ 루트 스냅샷이 `SlotId`를 보존** | `AddSubtree`가 `MoveEntry`의 정합(검사 3)·중복(검사 4) 검사를 **우회해 슬롯을 채운다.** 배낭 안에 되주우면 함정 4i 상태가 되고, 이미 배낭을 매고 있으면 `"Back"` 슬롯에 엔트리가 둘이 되어 **유령 배낭**이 남는다. 게다가 `In.Num()==1`이면 `AddItem`으로 빠져 **빈 배낭은 안 매지고 내용물이 든 배낭만 매진다** | `bIsRoot`일 때 `SlotId = NAME_None` (03-2 ②) ★★ |
| 6 | `UsedSlots`를 필드로 캐시 | 추가·제거·복제 수신 중 하나만 빠져도 "안 찼는데 가득 찼다" | 매번 계산 (03-3) |
| 6b | 본체와 배낭의 칸을 **합산** | GAME.md는 "통합되지 않는다" | 컨테이너별 (03-3) |
| 6d | 복제 순서 방어 코드를 넣음 | 불필요한 복잡도. 파생값을 매번 재계산하므로 저절로 맞는다 | 03-3 |
| 7 | 칸 여유 판정에 엔트리 **개수**를 씀 | `SlotSize`가 큰 무기를 무제한으로 넣게 됨 | `Σ SlotSize` (03-3) |
| 8 | 엔트리 포인터를 밖으로 반환 | 배열 재할당 후 댕글링. Step 00의 `FEPItemData*`와 같은 문제 | `FindEntry`는 값 복사 (03-2) |
| 8b | 알림 뒤에 `E.EntryId`를 읽음 | 구독자가 배열을 재할당하면 댕글링 | 값을 미리 뜬다 (03-3) |
| 9 | `DropCooldown`을 클라에서만 검사 | 버리자마자 재획득 가능 | Step 02의 4단계 필수 |
| **3e** | **`MoveEntry` 검사 3의 설명 블록을 그대로 인라인으로 침** | 검사 3이 두 번 돌아 무해해 보이는데, `CanPlaceInSlot`을 안 부르게 되어 **검사 2·4가 통째로 빠진다.** 증상은 *"조준경이 달린 총에 조준경이 또 달린다"*(§7-3에서 처음 보인다) — A-2가 되살아난 것이다. 변수 이름이 `NewParent`/`NewSlotId`면 설명용이다 | `MoveEntry`는 한 줄(`CanPlaceInSlot`)만 (03-2) ★★ |
| **9f** | **`Server_*`에 `UFUNCTION` 매크로 없음 / `CanMutateInventory()` 없음** | 매크로가 없으면 **RPC가 아니라 로컬 함수**라 클라에서 불러도 서버는 모른다. 게이트가 없으면 **죽은 뒤에도 인벤토리가 바뀐다** — 사망 시 드랍(§8 미정 #4)이 오면 경쟁이 된다. **옛 `Server_EquipBackpack`이 실제로 둘 다 빠뜨린 채 코드에 들어갔다**(14차에 함수 자체가 없어졌다) | Step 03의 대상은 `Server_DropItem` 하나. 04-B에서 Move·Swap·Reorder 셋이 는다 (03-2) |
| 10 | `ClearLoot`이 버린 아이템까지 삭제 | `EP.Loot.Respawn` 시 플레이어 소지품이 사라짐 | Step 01의 `SpawnedPickups` 약참조 — **여기서 처음 검증 가능** |
| **10b** | **`SlotId != None`인 자식을 용량으로 셈** | 부착물이 칸을 먹는다 | `GetUsedSlots`가 건너뛴다. **§7-3은 용량이 아니라 슬롯 스키마로 제한한다** (03-3) |
| **3g** | **`CanPlaceInSlot`의 부착 슬롯 갈래(`GetWeaponDefOf`·`AttachmentSlots`)를 지금 침** | **컴파일되지 않는다** — 함수도 필드도 없고 `DT_Items`에 부착 슬롯 행이 0개다. 만들면 소비자 없는 필드 둘 ＋ 함수 하나가 생긴다(CLAUDE.md §2 *"상상한 확장점"*). **지금은 *"`BodySlots`에 없으면 거절"* 이 정확한 판정이다** | else 갈래는 §7-3에서. `return false` 한 줄을 벌리는 것이 전부 (03-2) |
| **10d** | **★★ `AddSubtree`가 슬롯 배치 검증을 안 한다** | `MoveEntry`의 검사 2(자격)·3(정합)·4(중복)를 하나도 안 지나면서 **public이고 `SlotId`를 받는다.** 지금은 유일한 호출자(`TryAutoEquip`)가 미리 확인해 통과하지만, **§7-3 부착이 두 번째 호출자로 예고돼 있고**(`AddSubtree(총Id, "Optic", …)`) 그쪽은 확인하지 않는다 — **함정 4z가 막은 우회 경로가 문법 그대로 되살아난다.** *"호출자가 보증하는 계약은 호출자가 늘면 깨진다"* | `CanPlaceInSlot(Parent, SlotId, ItemId)`을 뽑아 `MoveEntry`·`AddSubtree`가 같이 부른다 (03-2) ★★ |
| **10e** | **★ `SlotSize`에 하한이 없다** | 기본값이 1일 뿐이고 `ClampMin`도 `IsDataValid` 검사도 없다. **본체가 0칸이 되면** `CanFit`이 `0 + 0 <= 0` → **참**이라 `SlotSize = 0`인 아이템만 0칸 본체에 무한히 들어가고, `GetInsertionOrder`의 맨 앞이 본체라 **컨테이너에는 절대 안 들어간다.** 크래시도 경고도 없이 *"이 아이템만 가방에 안 들어간다"* | `IsDataValid`에 `SlotSize >= 1` (`05_Loot_00_ItemCore.md`). **컨테이너 깊이 증명이 쓰는 전제이기도 하다** |

> **★★ 표시가 붙은 것 중 넷은 정상 플레이에서 반드시 나오고 증상이 원인을 가린다 — 4k(잔탄이 조용히 사라짐) · 4q(배치가 조금씩 무너짐) · 4v(장착이 무동작인데 `true`) · 4y(등이 비었는데 못 맨다).**
>
> **3b는 시나리오가 아니라 계약으로 남아 있다** — 9차 확정으로 *"배낭 속 무기 장착"* 은 표현 불가능해졌지만(장착 ＝ `ParentEntryId == INDEX_NONE`), ① 착용 컨테이너 안에 또 컨테이너가 들어가는 구조가 그대로 있고 ② 핫바 5~0이 컨테이너 안 아이템을 들 수 있게 되면 같은 모양이 되살아난다 (`EquipmentSlots.md` §10 미정 #7).

---

## 이 단계에서 하지 않는 것

- 인벤토리 화면 UI → **Step 04** (이번엔 `OnInventoryChanged` 델리게이트 + `EP.Inv.*` 커맨드만)
- 무기 장착/해제, `ActiveHotbarIndex` **세팅** → **Step 05**
  > **★ `RemoveEntry`의 장착 분기는 Step 03 내내 항상 거짓이다.** `ActiveHotbarIndex`를 세팅하는 경로가 Step 05에 있기 때문이다. 컴파일도 되고 완료 조건도 통과하지만 **장착 관련 불변식은 한 번도 실행되지 않는다.** Step 05에서 처음 도는 코드라는 걸 알고 넘어가야, 거기서 버그가 나도 원인을 두 단계 뒤에서 찾지 않는다
  >
  > **방어를 코드로 넣을 자리가 아니다** — 항상 거짓인 분기라 `ensure`가 울릴 일이 없다. **완료 조건으로 넘겼다:** `05_Loot_05_Equipment.md` 참조
- **핫바 5~0 (`HotbarRefs`) → Step 04 / 05.** 필드도 청소도 여기서 넣지 않는다
  > **9차 확정.** 제거 경로가 셋(버리기·사용·캐스케이드)이라 나중이 비싸 보이지만 **셋 다 `RemoveSelf` 하나로 모인다.** 청소를 붙일 곳은 나중에도 정확히 한 줄이고, 지금 넣으면 Step 03 내내 **길이 0인 배열을 도는 루프**가 남는다 (`EquipmentSlots.md` §4)
- **`Server_MoveEntry` → Step 04.** `MoveEntry`(내부 계약)는 여기서 만들지만 RPC 표면은 드래그 UI와 함께 연다
  > **★ `Server_ReorderEntry`도 Step 04다(11차 검수).** 초안은 여기서 열려 했으나 **9차가 `Server_MoveEntry`에 적용한 규칙이 그대로 적용된다** — 소비자(드래그)가 04-B다. 내부 함수 `ReorderEntry`만 여기서 만들고 `EP.Inv.Reorder`가 그것을 직접 부른다
- **`UEPWeaponDefinition::AttachmentSlots` → §7-3.** `MoveEntry`의 부착 갈래가 Step 03·04에서 도달 불가다
- **슬롯 조회 캐시 / `PostReplicatedReceive`의 `TMap`** — 만들지 않는다. 필요하면 **읽는 쪽(위젯)** 이 알림 1회당 1회 만든다
- 배낭 교체 / 수동 착용 UI → **Step 04 이후** (03-6은 자동 착용까지)
- 사망 시 드랍 → §8 미정 #4
- **자동 정렬 버튼 / 다중 선택 드래그** → 확장점 이름만 남긴다. 정렬은 `RenormalizeSortKeys`의 비교 함수 교체, 다중 선택은 `ReorderEntry`가 `TArray<int32>`를 받는 형태다 (11차)
- **`SortKey`의 낙관적 클라 적용** → **Step 04.** 클라 전용이라 서버 계약을 안 건드린다 (04-8)
- 드래그앤드롭·아이템 이동 UI

---

## 변경 이력

| 날짜 | 무엇 |
|---|---|
| 2026-08-28 (16차-b) | **옛 `FAutoConsoleCommand` 뼈대가 새 절과 함께 남아 모순이었다** — 폐기하고 포인터로 바꿨다. 그리고 **여섯 중 `EPInvMove` 하나만 본문이 있었다**(15차의 열 함수 누락과 같은 실수) — 여섯 전부를 한 벌로 넣었다. 문서가 `ReorderEntry`를 `bool`이라고 적었던 것도 고쳤다 — **`void`다**(`EPInventoryComponent.h:202`, *"정상 클라에서는 실패할 수 없는 연산"*). `Dump`는 `NextEntryId`(private)·`MaxSlots`(protected)를 못 읽으므로 `EntryId` 열과 `GetCapacity(INDEX_NONE)`으로 대신한다 — **public API만 쓴다는 계약을 깨지 않는다** |
| 2026-08-28 (16차) | **03-9의 커맨드를 `UCheatManagerExtension`으로 바꿨다.** `Play As Client` PIE에서 콘솔 오브젝트(`FAutoConsoleCommand`)는 **클라 월드에서 돌아 권한이 없고**, `ServerExec`로도 못 넘긴다 — 서버 쪽 exec 체인(`Player.cpp:126-157`)에 콘솔 매니저가 없다(`UnrealEngine.cpp:5415`가 유일한 처리 지점). `UFUNCTION(Exec, BlueprintAuthorityOnly)`는 엔진이 `ServerExec`로 자동 라우팅한다(`CheatManager.cpp:95-131`). **부수 효과로 `[PlayerIndex]` 인자 넷과 `FindInv` 헬퍼가 사라진다** — 서버의 `GetPlayerController()`가 명령을 친 창의 PC다. 전제 둘: `AEPPlayerController::AddCheats`에서 `Super::AddCheats(true)` 강제(클라엔 `GetAuthGameMode()`가 없어 기본으론 CheatManager가 안 생긴다, `PlayerController.cpp:1166`), 등록은 CDO 생성자의 `RegisterForOnCheatManagerCreated`이되 **Lyra의 `WITH_SERVER_CODE` 가드는 뺀다**(클라에 확장이 없으면 라우팅 검사가 실패). 이름에 점을 못 써 `EP.Inv.Move` → `EPInvMove`. `Dump` 둘에 `BlueprintAuthorityOnly`를 붙이면 `COND_OwnerOnly` 검증이 조용히 무의미해진다 |
| 2026-08-26 (15차) | **`MoveEntry`의 조립된 전체 본문을 넣었다** — 검사 0·1·5는 표에만 있고 코드가 없었고, 검사 6과 쓰기 블록은 각자 다른 절에 있어 **구현자가 다섯 군데를 꿰매야 했다.** 순서 계약 다섯을 표로 명시(검사 0 < 4 · 검사 0 < 5 · 검사 6 < 쓰기 · 키 계산 < 재부모 · 쓰기 < 모든 검사) |
| 2026-08-26 (15차 — **파일 분할**) | **통합 문서 `05_Loot_03_Inventory.md`(2692줄)를 03-A / 03-B 둘로 쪼갰다.** 8차의 *"파일을 쪼개지 않는다"* 를 뒤집는다 — 근거였던 *"`RemoveEntry`가 경계에서 갈린다"* 가 두 번 무너졌다(13차가 가운데 구간 삭제 / 제거 경로 셋이 전부 03-B). **직접적 계기는 열 함수의 본문이 통째로 빠진 것을 아무도 못 본 것**이다. **★ 03-A-부록 신설** — `FindEntry`·`ContainsEntry`·`FindFungibleEntryId`·`GetEntryInSlot`·`GetEquippedEntryId`·`RemoveSelf`·`AssignSortKey`·`KeySpace_Min`·`KeySpace_NextAbove`·`KeyOf` 본문. 함정표는 대응 열로 라우팅(A 44 / B 16, 겹치는 4는 양쪽). **완료 조건 번호는 통합 문서의 1~20을 유지한다** |
| 2026-08-25 (14차 — 사용자 지적) | **★★ `Server_EquipBackpack`을 없앴다.** 13차가 *"Step 03에 호출자 0개"* 까지 찾고 **04-A로 이동**을 골랐는데, 근거였던 `EP.Inv.Equip`이 **콘솔 커맨드**라 이 문서 자신의 규칙(*"커맨드는 내부 함수를 직접 부른다"*)대로면 **옮긴 자리에도 호출자가 0개**다. 그리고 9차의 근거 *"좁은 RPC가 넓은 RPC보다 낫다"* 는 **04-B가 `Server_MoveEntry`를 여는 이상 성립하지 않는다** — 넓은 문이 열린 뒤의 좁은 문은 표면을 안 줄인다. 배낭이 특별할 근거도 없다(`SlotPriority` ＋ `BodySlots`가 상의·하의·외투·배낭을 같은 모양으로 만든다 — **`TryAutoEquip`에 이미 적용한 규칙이 RPC 이름에만 안 적용돼 있었다**). 03-2 별도 절 재작성, 03-6 RPC 삭제, 함정 9f를 `Server_*` 일반으로, `EP.Inv.Equip`(04-A)도 `EP.Inv.Move`의 별칭이라 폐기 |
| 2026-08-26 (15차) | **03-9에 커맨드 구현 형태를 적었다** — 여태 시그니처만 있었다. 자리는 **`Private/Inventory/EPInventoryDebugCommands.cpp` 전용 파일**(Step 01의 `EPLootDebugCommands.cpp`와 같은 형태). `FindInv(World, Args, ArgIdx)` 헬퍼, `ECVF_Cheat` ＋ `#if !(UE_BUILD_SHIPPING \|\| UE_BUILD_TEST)`, **반환값 로그 필수**(안 찍으면 결함 A-1의 검증이 성립하지 않는다), 서버 전용 가드의 예외는 `Dump` 둘. **`Owner` 타입을 코드에 맞춘다** — `TObjectPtr<UEPInventoryComponent>`이므로 `PostReplicatedReceive`에 `Cast`가 필요 없다(11차의 `UActorComponent` 판정을 뒤집는다 — Lyra는 리스트를 여러 컴포넌트가 쓸 여지를 둔 형태이고 우리는 소유자가 하나로 고정이다). **정의는 `.cpp`** — `FEPInventoryList`가 컴포넌트보다 위에 있어 인라인 본문은 불완전 타입이다 |
| 2026-08-26 (15차 — 04 대조) | **`CanPlaceInSlot`의 네 번째 인자 `IgnoreEntryId`를 예고한다** — 04-7의 `SwapEntries`가 **상대가 차지한 슬롯**으로 들어가 검사 4가 언제나 거짓이 된다. 붙이는 것은 Step 04(검증 표면은 소비자를 따라간다). 그리고 **UI가 `SlotPriority`·`BodySlots`를 직독하지 않는다** — 04-1 ②의 판정식 목록을 `CanFit`/`CanPlaceInSlot`으로 교체했다 |
| 2026-08-25 (13차 **답변 반영** — `Review/05_Loot_REVIEW13_Answer.md`) | **★★ `MoveEntry`도 키를 늦게 구하고 있었다** — `InsertEntry`와 같은 결함(함정 4x가 두 함수로 넓어졌다). 재부모 **전에** 구한다. **`FScopedInventoryNotify` 정의가 소스에 없다** — `.cpp` 상단으로 위치 확정(**지금 코드가 컴파일 안 된다**). **`CanPlaceInSlot(Parent, SlotId, ItemId)` 추출** — 검사 2·3·4를 `AddSubtree`·§7-3과 공유. *"슬롯 진입 경로가 둘"* 이 API에서 거짓이었다. **`IsDataValid`에 `SlotSize >= 1`** — 본체 0칸에서 `0+0<=0`이 참이라 0칸 아이템이 무한히 들어간다. **`EP.Inv.Add`에 `[Container]`** — 없으면 `MaxSlots=0` 전환이 03-A 완료 조건 9개를 죽인다. **★★ 옛 03-B(배낭) 구간 삭제** — `Server_EquipBackpack`이 Step 03에 호출자 0개라 **04-A로**. 구간이 셋에서 **둘**로. **래퍼 기각 근거 교체** — *"엔진 관례와 싸운다"* 는 **거짓이다**(`MarkAbilitySpecDirty`가 감싼다). 근거는 *"안에 할 일이 없다"*. **`Cap == SlotSize` 근거 교체** — 익스플로잇이 아니라 *"비용 0 ＋ 세 곳의 전제 ＋ 되돌릴 손잡이"*. 함정 4v 증상에 `MarkArrayDirty`·`IDCounter` 한 줄. 완료 조건 18의 **32,764회**(답변의 32,763도 틀렸다 — 직접 계산), 완료 조건 4 문구, `AddSubtree` 전제를 **출처가 아니라 모양**으로 |
| 2026-08-25 (13차 검수 — `05_Loot_REVIEW_Inventory.md`) | **★★ `AddSubtree`가 위치를 반쪽만 받고 있었다.** `AddSubtree(Parent, **SlotId**, In)`으로 목적지를 나란히 받고 **기본값을 주지 않는다** — `InsertEntry`·`MoveEntry`와 같은 어휘. 슬롯이면 `CanFit`을 건너뛴다(함정 4y: *"등이 비었는데 배낭을 못 맨다"*, 본체 0칸이면 영구). **루트 스냅샷이 `SlotId`도 버린다**(함정 4z) — 안 버리면 `AddSubtree`가 검사 3·4를 우회해 슬롯을 채웠다. **`MoveEntry`가 `FindEntry` 복사본에 쓰고 있었다**(함정 4v — 무동작인데 `true`). **`SortKey` 함수의 `INDEX_NONE` 센티널이 키 −1과 충돌**(함정 4w) → `bool` ＋ out 파라미터. **`InsertEntry`가 키를 배열에 넣기 전에 발급**(함정 4x). **`PostReplicatedReceive`가 컴파일 안 됐다** — `Owner`가 `TObjectPtr<UActorComponent>`(11차)인데 캐스트가 없었다. **가드 목록을 없애고 "단일 쓰기 지점 다섯"에 건다** — 별도 목록이라 두 번 낡았다(사설 래퍼는 기각 — 근거는 *"관례"* 가 아니라 *"안에 할 일이 없다"* 다. **엔진은 감싼다**: `MarkAbilitySpecDirty`). **`EP.Inv.Move` 신설** — `MoveEntry`가 Step 04까지 한 줄도 안 돌고 있었다(완료 조건 15·17). `Server_EquipBackpack`에 `UFUNCTION` ＋ 게이트(함정 9f). `GetOwner<AEPCharacter>()` 무보호 역참조(§7-1). **완료 조건 18 ②의 관찰 문구 교정** — 맨 앞 20회로는 재정규화가 안 돈다(32,764회). **용량표 확정** — 본체 **0칸**(테스트 중 10), 상의 `11-10` / 하의 `6-5` / 배낭A `15-12` / 배낭B `10-8`, **넣기 `≤` 와 데이터 `<` 는 다른 식이다**. **`StartingEquipment` 신설**(03-B). 함정표 번호순 재정렬 ＋ 꼬리 주석 교정 |
| 2026-08-23 (12차 검수) | **★★ 무한 재귀 제거 (함정 4t).** 재정규화 가드에서 `PrevEntryId != INDEX_NONE`이 빠져 **맨 앞 드래그가 서버를 죽였다** — 경계(`bOutOfRange`)와 고갈(`bNoGap`)을 분리하고 `ReorderEntryInternal(bRetry)` ＋ `ensure`로 종료를 문법으로 보장. **제자리 드롭 조기 반환 (4u).** 키 공간 헬퍼에 **`KeySpace_` 접두어**(합치는 것으로는 스코프 혼동을 못 막는다). **★ *"UE에 선례가 없다"* 가 거짓이었다** — `FUIFrameworkStackBoxSlot::Index`(주석까지 같다) · `FUIFrameworkGameLayerSlot::ZOrder`. 조밀/희소 조건 대비로 근거를 바꿨다. **슬롯 아이템이 자리를 지키는 것(A) 확정** — 대안 B는 같은 동률이 재현되고 `NewSlotId` 예외가 돌아온다. 03-2 ★ 노트의 증상 서술이 **틀렸던 것**(*"AK가 X보다 앞에"* → 실제로는 뒤)도 교정. 완료 조건 19개(＋1) |
| 2026-08-23 (11차 검수) | **키 공간을 `GetSortedContents`에서 떼어냈다 (함정 4q).** `KeySpace_NextAtEnd`·`RenormalizeSortKeys`가 **부모가 같은 것 전부**를 본다 — 초안은 슬롯을 거른 목록에서 최대 키를 구해 *"꽂고·줍고·뺀다"* 라는 정상 조작에서 동률이 났다. `InsertEntry`의 삼항과 `MoveEntry`의 `NewSlotId` 조건이 **사라지고 코드가 줄었다**(꽂았다 빼면 원래 자리로 돌아온다). **재정규화를 세 분기 공통 가드로 (함정 4r)** — 맨 앞/맨 뒤가 무한 증감이라 `int32` 경계에서 순서가 영구히 깨졌다. **`Server_ReorderEntry`(RPC)를 04-B로** — 9차의 `Server_MoveEntry` 규칙. `Owner`를 `TObjectPtr<UActorComponent>`로 + **파일 앞부분 블록 신설**(전방선언 3개). 조밀 기각 사유 교정(*"재정규화 코드가 사라진다"* 는 거짓 — 그게 곧 조밀 재번호 루프다). `double` 기각에 고갈 판정 근거 추가, 컨테이너 배열 기각에 9차 일관성 근거 추가. 완료 조건 19개(＋2), 단일 쓰기 지점 표에 `SwapEntries` 한 줄 |
| 2026-08-23 (11차) | **★★ 화면 순서를 서버가 든다.** `FEPInventoryEntry::SortKey`(`int32` 희소, `Step = 1<<16`) 신설 — 형제 스코프. `AssignSortKey`가 유일한 쓰기 지점(패턴의 5번째), `InsertEntry`가 발급 · `MoveEntry`가 부모 변경 시 재발급 · `Server_ReorderEntry(EntryId, PrevEntryId)`가 자리 바꾸기(인덱스 아님) · `RenormalizeSortKeys`가 이분 고갈 처리. `GetSortedContents`는 **클라·서버 공용** 정렬. **`RemoveEntryInternal` ②의 `bIsRoot`가 `ParentEntryId`와 `SortKey`를 동시에 관장** — 루트는 버리고 자식은 보존해 **배낭을 되주워도 내용물 순서가 산다.** `EP.Inv.Reorder` 커맨드 + `Dump`에 `SortKey` 열. 함정 4m~4p, 완료 조건 17개(＋4). 근거: `05_Loot_04_InventoryUI.md` 04-8이 10차까지 클라 로컬이었고 그 설계는 **지속을 줄 수 없다** |