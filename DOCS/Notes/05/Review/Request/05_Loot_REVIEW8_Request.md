# 검수 요청 8차 — Step 03 인벤토리: 드랍/장착을 GAS로 올릴 것인가, 그리고 문서 내부 모순 6건

> 작성일: 2026-08-03
> 7차: `05_Loot_REVIEW7_Request.md` / `_Answer.md` (Step 02 상호작용 — **`UEPGA_Interact`로 확정**)
> 시점: **Step 02 구현 완료·PIE 검증 완료(`step5-2`), Step 03 코드 0줄.** 문서만 있다
> 성격: **① 아키텍처 판단 하나(§2) + ② 문서-코드 어긋남 및 내부 모순 6건의 검증(§3~§4).** 7차와 구조가 같다 — §2가 뒤집히면 §3-1이 무의미해진다

---

## 0. 사용자 입장 (먼저 밝힌다)

**7차에서 "게임플레이 입력의 진입점은 어빌리티 하나다"라는 규칙을 세우고 Step 02를 GAS로 확정했다.** 그 결과 `UEPGA_Interact`가 구현됐고 PIE 2인에서 동작한다.

**그런데 Step 03 문서(`05_Loot_03_Inventory.md`)는 그 규칙보다 먼저 쓰였다.** 03-5가 `UFUNCTION(Server, Reliable) void Server_DropItem(int32 EntryId)`를 선언한다. 7차 답변이 기각한 바로 그 형태다.

**세 가지를 요청한다.**

1. **§2를 먼저 판정해달라.** 7차의 판정이 드랍·장착에도 그대로 적용되는가, 아니면 상호작용과 사정이 다른가. Claude는 "적용된다(GAS로 가라)"고 판단했으나 **그 판단에는 대가가 있다** — `FGameplayEventData`에 `int32` 필드가 없다(§2-3). 그 대가를 치를 만한지가 핵심이다
2. **§3의 어긋남 3건은 사실 확인이다.** Step 02 구현 실물과 문서가 다르다. 이건 판단이 아니라 대조이므로 빠르게 확인만 해주면 된다
3. **§4의 내부 모순 3건과 §5의 실무 위험 3건은 근거까지 검증해달라.** 특히 4-2와 5-2는 "문서가 자기 논리를 어긴다"는 강한 주장이라, 틀렸으면 없는 문제를 새기는 것이다

---

## 1. 현재 상태 (사실만)

### 1-1. 진행

| | 상태 |
|---|---|
| Step 00 (ItemCore) | 완료 |
| Step 01 (Spawner) | 완료. `EP.Loot.RollTable` 1000회 등급 비율 검증만 미실행 |
| Step 02 (Interaction) | **완료.** `UEPGA_Interact` + `IEPInteractable` + HUD 프롬프트. PIE 2인 검증 완료, 태그 `step5-2` |
| **Step 03 (Inventory)** | **코드 0줄.** 문서만 있다 |

### 1-2. ★ 이 프로젝트에 서버 RPC가 여전히 하나도 없다

```
$ grep -rn "UFUNCTION(Server" EmploymentProj/Source/EmploymentProj/
(결과 없음)
```

7차 이후에도 유지되고 있다. Step 02는 `ASC->HandleGameplayEvent(TAG_Ability_Interact, &Payload)` 한 줄로 갔고, 대상은 `FGameplayEventData::Target`(액터 포인터)에 실려서 서버로 갔다.

**`Server_DropItem`이 들어오면 이 프로젝트 최초의 손수 만든 서버 RPC가 된다.** 7차 §2-2(a)와 글자 그대로 같은 상황이다.

### 1-3. 확인된 사실 (다시 파지 말 것 — 전부 이번 세션에 직독 확인함)

| 사실 | 출처 |
|---|---|
| `UFUNCTION(Server` 선언 0개 | `Source/EmploymentProj/` 전체 grep |
| `FGameplayEventData`의 스칼라 필드는 **`float EventMagnitude` 하나뿐.** `int32` 없음 | `GameplayAbilityTypes.h:280` |
| 같은 구조체에 `Target`/`Instigator`/`OptionalObject`/`OptionalObject2`(전부 `UObject*` 계열), `TargetData`, 태그 컨테이너 둘 | `GameplayAbilityTypes.h:246-284` |
| **`FFastArraySerializerItem`의 복사 생성자와 `operator=`가 `ReplicationID`/`ReplicationKey`/`MostRecentArrayReplicationKey`를 전부 `INDEX_NONE`으로 리셋한다** | `FastArraySerializer.h:302-323` |
| `PostReplicatedReceive`는 파생 struct에 **정의돼 있을 때만** 호출된다 (`CPostReplicatedReceiveFuncable` 컨셉) | `FastArraySerializer.h:517-537` |
| `IEPInteractable::OnInteract`의 실제 시그니처는 `bool OnInteract(AEPCharacter*, FText& OutReason)` | `EPInteractable.h:27` |
| `IEPInteractable::CanInteract`도 `bool(AEPCharacter*, FText&) const` | `EPInteractable.h:23` |
| `AEPPickup::InitPickup`의 실제 시그니처는 `void InitPickup(FName, const FEPItemState&)` | `EPPickup.h:22` |
| `InitPickup` 호출부는 **1곳**, `GetState()` 읽는 곳은 **2곳** | `EPItemSpawner.cpp:91` / `EPLootDebugCommands.cpp:133,137` |
| `FEPItemState`는 `int32 Charges` + `float Durability` 둘뿐 | `EPTypes.h:77-86` |
| `FEPItemData`에 `MaxStack`(:36) 필드가 **아직 남아 있다** — §8이 "스택 없음"으로 확정한 뒤 | `EPItemData.h:36` |
| `UEPGA_Interact`는 `LocalPredicted` + `InstancedPerActor`, `AbilityTriggers`에 `GameplayEvent`/`TAG_Ability_Interact` 1개 | `EPGA_Interact.cpp:14-21` |
| `TAG_Ability_Interact`는 `TAG_Ability_Item_Reload` 아래에 선언됨 | `EPNativeGameplayTags.h:30` |
| `AEPPickup::CanInteract`의 현재 구현은 **`bClaimed`만** 본다 (`DropCooldown` 개념 없음) | `EPPickup.cpp:55-60` |

### 1-4. 발견된 코드 버그 1건 (이미 사용자에게 보고, 아직 미수정)

```cpp
// EPGA_Interact.cpp:40 — ! 이 빠졌다
if (TryInteract(Char, TriggerEventData, Reason) && PC)
    PC->Client_OnInteractFailed(Reason);
```

**성공했을 때** 실패 회신을 보낸다. 지금은 성공 시 `Reason`이 비어 `Client_OnInteractFailed_Implementation`이 `IsEmpty()`로 조기 반환하므로 증상이 없지만, **실패 사유가 영원히 표시되지 않는다.** Step 03에서 "가방에 자리가 없습니다"가 이 경로를 타므로 지금 고쳐야 한다.

> 검수자에게: 이건 판정 요청이 아니라 보고다. **다만 §3-1(반환 계약)이 이 경로에 얹히므로 맥락으로 알고 있어야 한다.**

---

## 2. ★ 최대 주제 — `Server_DropItem`인가 `UEPGA_DropItem`인가

### 2-1. 문서의 현재 설계 (03-5)

```cpp
UFUNCTION(Server, Reliable)
void Server_DropItem(int32 EntryId);

void UEPInventoryComponent::Server_DropItem_Implementation(int32 EntryId)
{
    TArray<FEPInventoryEntry> Removed;
    if (!RemoveEntry(EntryId, &Removed)) return;
    AEPPickup* P = SpawnPickupInFront();
    if (!P) return;
    P->InitPickup(MoveTemp(Removed));
    P->StartDropCooldown();
}
```

진입점은 G키(장착 무기) 또는 Step 04의 인벤토리 UI 선택이다.

### 2-2. 반대 근거 — 7차 판정이 여기도 적용된다는 주장

**(a) 7차가 세운 규칙을 한 단계 만에 깬다.** `05_Loot_02_Interaction.md` 함정 #12가 이렇게 적혀 있다.

> `Server_Interact` 직접 RPC로 감 → **이 프로젝트 유일의 손수 만든 서버 RPC가 된다.** 깨지는 규칙은 "서버 RPC를 안 쓴다"가 아니라 **"게임플레이 입력의 진입점은 어빌리티 하나다"** 이다.

**(b) 하나로 끝나지 않는다.** 인벤토리를 서버에서 바꾸는 경로가 앞으로 넷이다.

| 앞으로 올 것 | 문서에 이름이 있는가 | 직접 RPC면 |
|---|---|---|
| Step 05 무기 장착 | §4-8 `Server_Equip(EntryId)` | 두 번째 RPC |
| §4-9 소모품 사용 | `UEPItemDefinition::GrantedAbility` — **이미 어빌리티다** | 인벤토리 변경 경로가 두 배관 |
| §7-1 컨테이너 "가져가기" | §7-1 | 세 번째 |
| §7-2 자판기 구매 | §7-2 | 네 번째 |

**(c) 태그 차단이 공짜다.** `ActivationBlockedTags.AddTag(TAG_State_Dead)`는 이미 형제 넷(`Item_PrimaryUse`, `Item_Reload`, `Skill_Base`, `Interact`)에 같은 줄로 있다. 그리고 **§7-1이 "검색 중 이동하면 취소"를 명시**했으므로 검색 채널링 중 버리기 금지가 곧 필요해진다 — 어빌리티면 `TAG_State_Casting` 한 줄, 직접 RPC면 그 RPC를 다시 열어야 한다.

### 2-3. ★ 찬성 근거 — 그런데도 RPC일 수 있는 이유 (여기가 7차와 다르다)

**대상 전달이 상호작용과 성격이 다르다.** 7차에서 GAS를 고른 결정적 이유는 *"`FGameplayEventData`에 `Target`이 있고 그 구조체가 통째로 서버 RPC 파라미터다"* 였다. 상호작용의 대상은 **액터**라 자연스럽게 실렸다.

**드랍의 파라미터는 `int32 EntryId`이고, `FGameplayEventData`에 `int32` 필드가 없다** (`GameplayAbilityTypes.h:246-284`). 스칼라는 `float EventMagnitude` 하나뿐이다.

| 갈래 | 내용 | 비용 |
|---|---|---|
| **ⓐ `EventMagnitude`에 싣는다** | `FMath::RoundToInt(Data->EventMagnitude)`. `EntryId < 2^24`(16,777,216)면 float 왕복이 무손실 — 한 매치 인벤토리에서 도달 불가 | 코드 0줄. **의미론적으로 지저분** |
| **ⓑ `FGameplayAbilityTargetData` 서브클래스** | `NetSerialize` 포함 ~20줄 | 깨끗하지만 지금 소비자가 하나 |
| **ⓒ 직접 RPC 유지** | 7차 규칙에 예외를 하나 만들고, 그 근거를 명시 | 규칙이 죽는다 |

**그 외 GAS 기능을 실제로 안 쓴다는 점도 밝힌다.**
- 03-5가 **"클라 예측은 하지 않는다"** 고 확정했다. `LocalPredicted`는 순전히 라우팅 스위치로만 쓰인다 (7차에서 확립된 사실)
- `DropCooldown`은 **플레이어가 아니라 픽업**에 붙으므로 GE 쿨다운이 아니다
- 코스트·애님·데미지 전부 없다

즉 **이 어빌리티가 GAS에서 실제로 쓰는 기능은 `ActivationBlockedTags` 하나다.** 7차 §2-3의 "상호작용은 어빌리티가 아니다"가 여기서는 더 강하게 성립할 수 있다.

### 2-4. 판정 요청

1. **A(직접 RPC) / B(`UEPGA_DropItem`) 중 무엇인가.** 7차 판정의 사정거리가 여기까지인가, 아니면 "대상이 액터가 아니면 다르다"가 성립하는가
2. **B라면 파라미터 전달은 ⓐ/ⓑ 중 무엇인가.** ⓐ의 `EventMagnitude` float 인코딩이 실무에서 통용되는가, 아니면 눈살을 찌푸릴 형태인가. **Lyra나 엔진 샘플에 `EventMagnitude`를 식별자로 쓰는 선례가 있는가**
3. **B라면 Step 05 `Equip`까지 묶어 `UEPGA_InventoryAction` 베이스를 뽑는가.** CLAUDE.md §2가 "두 번째 구현자가 없는 베이스 클래스"를 금지하므로, Step 03 시점에는 구현자가 하나다 — **Step 05에서 뽑는 게 맞는가, 아니면 처음부터 둘을 같이 만드는가**
4. **A로 남길 경우**, "GAS 프로젝트에 서버 RPC가 하나(곧 넷) 있다"를 어떻게 정당화해 문서에 적는가. 7차가 이미 한 번 기각한 형태이므로 **7차 답변을 뒤집는 근거**가 필요하다
5. **부정 확인:** 다음 셋에 GAS를 쓰지 말라는 판단이 맞는가 — `DropCooldown`→GE 쿨다운(픽업 소유), 칸 계산/`bFungible`→GE(어트리뷰트가 아님), `OnInventoryChanged`→태그 이벤트(태그는 상태이지 알림이 아님)

---

## 3. Step 02 실물과 어긋난 곳 3건 — 사실 확인

**전부 Step 02를 구현하면서 시그니처가 바뀌었는데 Step 03 문서가 옛것을 들고 있다.**

### 3-1. `OnInteract` 시그니처 (03-4)

```cpp
문서:  void AEPPickup::OnInteract(AEPCharacter* Instigator)
실물:  bool AEPPickup::OnInteract(AEPCharacter* Interactor, FText& OutReason)   // EPInteractable.h:27
```

문서의 마지막 두 줄이 죽은 지시가 된다.

```cpp
bClaimed = false;                       // 실패 — 다른 사람이 주울 수 있게 되돌린다
/* Client_OnInteractFailed("가방에 자리가 없습니다") */
```

이제 `OutReason`을 채우고 `false`를 반환하면 `UEPGA_Interact`가 회신한다(§1-4 버그 수정 후).

- **`bClaimed` 되돌림 책임이 누구에게 있는가.** 현재 구현은 `OnInteract` 진입 직후 `bClaimed = true`를 하고, `TryInteract`가 `CanInteract` → `OnInteract` 순으로 부른다. **실패 시 되돌리는 것이 `OnInteract` 내부인가, `UEPGA_Interact`인가**
- 7차 3-4가 `bool` + `FText&`를 확정했는데, **실행 중 실패(칸 부족)에 `FText`가 맞는가.** §2가 B로 가면 `FGameplayTag` 실패 사유가 더 자연스러워지는가

### 3-2. `InitPickup` 교체의 파급 3곳이 문서에 없다 (03-4)

문서는 `AEPPickup`의 `FEPItemState State` → `TArray<FEPInventoryEntry> Payload` **교체**를 지시한다. 실제로 깨지는 곳은 셋이다.

```
EPItemSpawner.cpp:91          Pickup->InitPickup(RolledId, NewState);   ← 유일한 호출부
EPLootDebugCommands.cpp:133   P->GetState().Charges                     ← 읽기
EPLootDebugCommands.cpp:137   P->GetState().Charges                     ← 읽기
```

- 문서에 이 셋을 파일:줄로 박는 것이 맞는가 (03-4가 "스포너 경로도 같은 함수를 쓴다"고만 적혀 있다)
- **`AEPPickup`이 `FEPInventoryEntry`를 알아야 한다.** 인벤토리 컴포넌트 헤더를 `Loot/`가 include하게 된다 — **`FEPInventoryEntry`를 `EPTypes.h`로 내리는 게 맞는가, 아니면 별도 헤더인가.** 지금 문서는 정하지 않았다

### 3-3. `DropCooldown`이 `CanInteract`에 없다 (03-5)

03-5 표는 *"`DropCooldown`(0.5초) 동안 `CanInteract()`가 false"* 인데, 현재 구현(`EPPickup.cpp:55-60`)은 `bClaimed`만 본다. 03-5가 추가할 정확한 줄과 필드(`float DropCooldownEndTime`? 타이머 핸들?)를 문서가 지정해야 하는가.

---

## 4. 문서 내부 모순 3건 — 검증 요청

### 4-1. 03-1이 쓰지 않을 콜백 셋을 선언한다

```cpp
// 03-1
void PostReplicatedAdd(const struct FEPInventoryList& Serializer);
void PostReplicatedChange(const struct FEPInventoryList& Serializer);
void PreReplicatedRemove(const struct FEPInventoryList& Serializer);
```

그런데 03-7은 **"항목별 콜백을 쓰지 않는다"** 며 `FEPInventoryList::PostReplicatedReceive` 하나로 간다.

- **03-1에서 셋을 지우는 게 맞는가.** 남겨두면 구현자가 빈 함수 셋을 만들고 Step 04에서 "왜 알림이 두 번 오지?"를 판다
- **엔진이 셋 다 요구하지는 않는다**는 것이 확실한가 (`FastArraySerializer.h:517-537`의 `CPostReplicatedReceiveFuncable`는 "정의돼 있을 때만" 호출로 읽힌다). 셋 중 하나라도 없으면 컴파일이 깨지는 경로가 있는가

### 4-2. ★ `bIsRoot`가 public API에 노출돼 계약을 깰 수 있다

```cpp
bool RemoveEntry(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved = nullptr, bool bIsRoot = true);
```

이 문서는 `RemoveEntry ↔ AddSubtree` 계약을 **"규율이 아니라 형태로 막는다"** 고 세 곳(03-2 두 번, 03-5 한 번)에서 강조한다. 그런데 **호출자가 `bIsRoot=false`를 넘기면 루트의 `ParentEntryId = INDEX_NONE` 정규화가 안 돼 `AddSubtree`가 루트를 못 찾는다** — 문서가 03-4에서 "흔한 경로"라고 경고한 바로 그 실패다.

- **문서 자신의 논리와 어긋난다는 진단이 맞는가**
- 맞다면 public `RemoveEntry(Id, OutRemoved)` / private `RemoveEntryInternal(Id, OutRemoved, bIsRoot)` 분리가 정답인가, 다른 형태가 있는가

### 4-3. `MarkItemDirty` 호출 지점이 결국 둘이 된다

03-2는 *"`MarkItemDirty`도 여기 한 곳"* 이라 적었는데, `AddItem`(03-3)과 `AddSubtree`(03-4)가 각자 엔트리를 삽입한다.

- private `int32 InsertEntry(Parent, ItemId, State, SlotId)` 하나로 모으고 둘 다 그걸 부르게 하는 게 맞는가
- 아니면 `AddSubtree`가 루프 안에서 `AddItem`을 부르는 형태가 가능한가 — **`AddItem`이 `bFungible` 합치기와 `CanFit`을 하므로 자식에는 부르면 안 된다**는 것이 지금 구조다. 이 판단이 맞는가

---

## 5. 실무 위험 3건 — 검증 요청

### 5-1. ★ `RemoveEntry`가 `EquippedEntryId`를 비우지 않는다

```cpp
if (EntryId == EquippedEntryId)
    C->UnequipWeapon();                       // ← 번호를 비우는 건 누구 책임인가?
if (EntryId == EquippedBackpackEntryId)
    EquippedBackpackEntryId = INDEX_NONE;     // ← 여긴 명시돼 있다
```

배낭은 명시했는데 무기는 Step 05의 `UnequipWeapon`에 암묵적으로 맡겼다. **함정표 3b가 말하는 "`EquippedEntryId`가 죽은 번호"가 정확히 이 비대칭에서 나온다.**

- 배낭 줄과 대칭으로 `EquippedEntryId = INDEX_NONE;`을 `RemoveEntry`에 명시하고 `UnequipWeapon`은 write-back만 하게 하는 게 맞는가
- **더 나쁜 점:** 03-9가 이 분기는 *"Step 03 내내 항상 거짓"* 이라 적었다. 버그가 있어도 **두 단계 뒤에 처음 실행된다.** 이 구조적 위험을 문서에 어떻게 표시해야 하는가

### 5-2. ★ `FFastArraySerializerItem`의 복사 시맨틱을 문서가 모른다

```cpp
// FastArraySerializer.h:308-323
FFastArraySerializerItem(const FFastArraySerializerItem& InItem)
    : ReplicationID(INDEX_NONE), ReplicationKey(INDEX_NONE), MostRecentArrayReplicationKey(INDEX_NONE) {}

FFastArraySerializerItem& operator=(const FFastArraySerializerItem& In)
{ if (&In != this) { ReplicationID = INDEX_NONE; ReplicationKey = INDEX_NONE; MostRecentArrayReplicationKey = INDEX_NONE; } return *this; }
```

Claude의 해석은 둘이다. **두 해석 다 검증해달라.**

- **좋은 쪽:** 스냅샷 · `Payload` · `FindEntry` 값 복사가 전부 안전하다. 복제 ID가 따라붙지 않는다 → 03-1에 한 줄 적으면 리뷰어 질문 하나가 사라진다
- **★ 나쁜 쪽:** **살아있는 배열 원소에 `E = SomeEntry;`로 통째 대입하면 그 원소의 `ReplicationID`가 날아간다.** 이후 `MarkItemDirty`가 새 ID를 발급하고 **클라는 그걸 remove + add로 본다** → Step 04에서 슬롯 위젯이 재생성되어 선택/드래그가 끊긴다. 그래서 `SetEntryCharges`가 `E.State.Charges = ...` **필드 단위 대입**인 것이 우연이 아니라 필수다

두 번째 주장이 특히 자신 없다. **`ReplicationID`가 `INDEX_NONE`이 된 기존 원소를 `MarkItemDirty`가 어떻게 처리하는가** — 새 ID 발급인가, 그리고 수신 측이 그걸 remove+add로 보는가. 엔진 소스로 확정해달라.

### 5-3. 검증 커맨드가 완료 조건을 못 닫는 구멍 둘 (03-9)

- **`EP.Inv.Dump`는 클라 허용인데 `NextEntryId`는 서버 전용(복제 안 함)이다.** 클라에서는 `1`이 찍힌다. 그런데 03-9 표는 *"`NextEntryId`로 재번호 없음을 증명"* 한다고 적었다 — **클라 Dump에서 그 열은 거짓말한다**
- **`EP.Inv.Add`가 서버 전용인데 §10은 "리슨서버 호스트가 아니라 클라이언트 쪽에서 확인"이다.** 대상 지정 인자가 없으면 완료 조건 대부분을 호스트에서만 검증하게 되어 §10 원칙과 충돌한다 → `EP.Inv.Add <ItemId> [PlayerIndex]`가 맞는가

---

## 6. 확장성 — 문서에 이름이 있는데 자리가 없는 것

### 6-1. ★ §7-1 월드 컨테이너를 이 구조가 이미 지원한다

`ParentEntryId`는 **한 인벤토리 컴포넌트 안**에서만 유효한 번호다. §7-1의 공구상자·구급상자는 자기 `UEPInventoryComponent`를 갖는 액터가 되고, "가져가기"는 **두 컴포넌트 간 서브트리 이동**이다.

```
Container->RemoveEntry(Id, &Sub)   →   MyInv->AddSubtree(INDEX_NONE, Sub)
```

**두 함수 조합으로 이미 성립한다.** 드랍은 "대상 인벤토리가 새로 스폰한 픽업"인 특수 케이스일 뿐이다.

- **이 사실을 03-2에 한 줄 적는 것이 맞는가.** 적으면 §7-1의 코드 추가가 거의 0이 되고, 안 적으면 그때 `TransferItem` 같은 세 번째 경로가 생긴다
- **`Server_MoveItem(EntryId, TargetInv, TargetContainer)` 일반형을 지금 만들면 안 된다**는 판단이 맞는가 (소비자가 하나뿐 — CLAUDE.md §2 위반). **사실만 적고 함수는 만들지 않는 것**이 옳은 선인가

### 6-2. `GetCapacity`가 §7-3에서 두 종류를 답해야 한다

```cpp
int32 GetCapacity(int32 Container) const { /* ContainerCapacity를 본다 */ }
```

§7-3 부착물은 **용량이 아니라 슬롯 이름 집합**(`SlotId`)으로 제한된다. 지금 넣지는 않되 함정표에 한 줄이 필요한가, 아니면 §7-3 진입 시 자연스럽게 드러나는가.

### 6-3. `AddSubtree`의 칸 검사가 루트만이다

```cpp
if (!CanFit(Container, In[0].ItemId)) return INDEX_NONE;   // 칸 검사는 루트만
```

버린 배낭을 되주울 때는 자식이 그 배낭 자신에게 들어가므로 용량이 자동으로 맞는다. **그런데 §7-1 컨테이너 간 이동이 들어오면 "용량 12 상자의 내용물을 용량 8 배낭으로"가 성립한다.** 지금 깨지지 않는 이유를 문서에 적어야 하는가, 아니면 `AddSubtree`가 서브트리 전체 칸을 검사해야 하는가.

---

## 7. 미결 — 사용자가 결정해야 한다고 남겨둔 것

### 7-1. `FEPItemData::MaxStack`이 데드 필드로 남아 있다

`EPItemData.h:36`. §8이 "스택 없음"을 확정한 뒤에도 남았다. 읽는 코드가 있는지 확인 후 지우는 것이 맞는가 — 아니면 §7-3 탄창(미정 #1 ⓐ)에서 되살아날 여지가 있는가.

### 7-2. 03-A/B/C 3분할이 실제로 유효한가

문서 자신이 "완료 조건 13개는 다른 단계 두 개 분량"이라며 셋으로 나눴다.

| | 범위 | 완료 조건 |
|---|---|---|
| 03-A 코어 | 03-1·2·3·9 | 1~6 |
| 03-B 배낭 | 03-6 + `GetCapacity` | 7 |
| 03-C 버리기 | 03-4·5·7 | 8~13 |

- **03-A가 `RemoveEntry`/`AddSubtree` 없이 정말 컴파일·실행되는가.** 03-2가 `RemoveEntry`를 선언하고 03-3의 `AddItem`이 `FScopedInventoryNotify`를 쓴다 — 가드는 03-7 소속인데 03-A 범위에 없다. **분할선이 어긋나 있는 것 아닌가**
- §2가 B로 판정되면 `UEPGA_DropItem`은 03-C 소속인가

---

## 8. ★ 실무 조사 요청

우리 판단만으로 결정하지 않겠다. **가능하면 실제 소스를 근거로.**

1. **`FGameplayEventData::EventMagnitude`를 식별자·인덱스로 쓰는 선례가 Lyra나 엔진 샘플에 있는가.** 있으면 인용, 없으면 "관용구가 아니다"로 답해달라 (§2-4의 2번)
2. **Lyra가 인벤토리 조작(장착·드랍·이동)을 무엇으로 하는가.** 어빌리티인가 직접 RPC인가. `LyraInventoryManagerComponent` / `LyraQuickBarComponent` / `LyraEquipmentManagerComponent` 주변을 봐달라. **특히 파라미터가 액터가 아닌 경우(슬롯 인덱스 등)를 어떻게 넘기는가**
3. **`FFastArraySerializerItem`의 `ReplicationID`가 `INDEX_NONE`으로 리셋된 기존 원소를 `MarkItemDirty`가 어떻게 처리하는가** (§5-2). 수신 측에서 remove+add로 보이는가
4. **FastArray 엔트리에 `PostReplicatedAdd/Change/PreReplicatedRemove`를 정의하지 않아도 되는가** (§4-1). 셋 중 일부만 정의했을 때 컴파일/런타임이 어떻게 되는가

> 로컬 경로: 엔진 `C:\Program Files\Epic Games\UE_5.7\Engine`, Lyra `C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame`, GAS 문서 `C:\Github\GASDocumentation`. **기억으로 Lyra API를 단정하지 말 것** — 6·7차에서 인용 정확도가 유용했다.

---

## 9. 재론하지 않았으면 하는 것

| | 근거 |
|---|---|
| `FEPItemState` 값 타입 / 스택 폐지 / `bFungible` 합치기 | 1·2차 확정 |
| DT/DA 두 계층 유지 | 3차 §5 확정 |
| 전역 에셋 참조를 `UEPLootDeveloperSettings`에 두는 것 | 6차 확정 |
| 픽업의 전 채널 `Ignore` + Dormancy + `Interact` 채널만 열기 | 5·7차 확정, Step 01·02 구현 완료 |
| **Step 02가 `UEPGA_Interact`로 가는 것** | **7차 확정, 구현·검증 완료.** 재론 불필요 — §2는 *드랍/장착*에 대한 질문이다 |
| `EntryId`(int32, 서버 발급, 재번호 없음) / `ParentEntryId` 평면 표현 | 1·2차 확정 (§8 확정표) |
| `COND_OwnerOnly` | §8 확정표 |
| 인벤토리 부착 위치 = Character | §8 확정표 |
| `UsedSlots` 캐시 안 함 / 복제 안 함 | §4-6 확정 |
| 본체 10칸, 배낭 별도 풀 | GAME.md 확정 |

---

## 10. 대상 파일

| 파일 | 관계 |
|---|---|
| **`05_Loot_03_Inventory.md`** | **검수 대상 본체.** 885줄, 미수정 |
| `05_Loot_02_Interaction.md` 함정 #12, 02-2 | §2의 근거 — 7차가 세운 규칙 |
| `05_Loot_DOCS.md` §4-6 / §4-7 / §4-8 / §4-9 / §7-1 / §7-2 / §8 | 상위 결정. §2·§6의 근거 |
| `05_Loot_05_Equipment.md` | Step 05 `Server_Equip` — §2-2(b)의 두 번째 RPC |
| `05_Loot_04_InventoryUI.md` | 드랍 진입점 둘 중 하나(UI 선택) |
| `Public/Interaction/EPInteractable.h:23,27` | §3-1 — 실제 시그니처 |
| `Private/GAS/EPGA_Interact.cpp:14-21,40` | §1-4 버그 / §2의 형제 패턴 |
| `Public/Loot/EPPickup.h:22,30` · `Private/Loot/EPPickup.cpp:55-60` | §3-2, §3-3 |
| `Private/Loot/EPItemSpawner.cpp:91` · `EPLootDebugCommands.cpp:133,137` | §3-2 파급 |
| `Public/Types/EPTypes.h:77-86` | `FEPItemState` |
| `Public/Data/EPItemData.h:36,39,42,45,56` | `MaxStack` 데드 필드 / `SlotSize` / `ContainerCapacity` / `bFungible` |
| `Public/GAS/EPNativeGameplayTags.h:30` | §2가 B면 `TAG_Ability_Inventory_Drop`이 여기 붙는다 |
| `FastArraySerializer.h:302-323, 517-537` | §4-1, §5-2 |
| `GameplayAbilityTypes.h:246-284` | §2-3 — `int32` 필드 없음 |
