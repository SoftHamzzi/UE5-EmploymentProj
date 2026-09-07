# 검수 답변 8차 — 드랍은 RPC로 남긴다, 그리고 7차의 규칙 문장을 고쳐 쓴다

> 작성일: 2026-08-04
> 요청: `05_Loot_REVIEW8_Request.md` (2026-08-03)
> 근거: UE 5.7 엔진 소스 · GameplayAbilities 플러그인 · LyraStarterGame 직독. **웹 인용 없음**
> 대상: `05_Loot_03_Inventory.md` (886줄, 미수정) / Step 02 구현 실물

---

## 0. 판정 요약

| # | 항목 | 판정 |
|---|---|---|
| §1-2 | *"이 프로젝트에 서버 RPC가 여전히 하나도 없다"* | ⚠️ **7차와 같은 반쪽 사실.** 손으로 쓴 것이 0개일 뿐, `ServerTryActivateAbilityWithEventData`가 F키마다 나간다 |
| **§2** | **A(직접 RPC) / B(`UEPGA_DropItem`)** | ✅ **A. `Server_DropItem`을 유지한다.** 아래 §2가 근거 |
| §2-4 ① | 7차 판정의 사정거리 | **7차 판정은 안 뒤집힌다. 뒤집히는 것은 7차가 거기 붙인 일반화 문장이다** |
| §2-4 ② | `EventMagnitude` 인코딩(ⓐ) | ❌ **관용구가 아니다.** 엔진 1건·Lyra 1건 모두 *크기*이지 식별자가 아니다 |
| §2-4 ③ | `UEPGA_InventoryAction` 베이스 | ❌ **A라서 질문이 소멸한다.** 뽑지 않는다 |
| §2-4 ④ | A의 정당화 문안 | 📄 §10-1에 작성 |
| §2-4 ⑤ | GE 쿨다운 / GE 칸계산 / 태그 알림 부정 | ✅ **셋 다 쓰지 않는 판단이 맞다** |
| §3-1 | `OnInteract` 시그니처 | ✅ 문서가 stale. `bClaimed` 되돌림은 **`OnInteract` 내부** (`bClaimed`가 private이라 형태가 이미 정해져 있다) |
| §3-2 | `InitPickup` 파급 3곳 | ✅ 확인. 박아야 한다. `FEPInventoryEntry`는 **전용 헤더**로 (EPTypes.h 아님) |
| §3-3 | `DropCooldown` 미구현 | ✅ 지정해야 한다 — **그리고 지금 설계로는 회색 프롬프트가 구조적으로 못 뜬다** (§3-3 ★) |
| §4-1 | 콜백 셋 선언 | ✅ **지운다. 그냥 무의미한 게 아니라 링크 에러다.** Lyra도 아이템에 안 둔다 |
| §4-2 | `bIsRoot` public 노출 | ✅ **진단이 맞다.** public/private 분리가 정답 |
| §4-3 | `MarkItemDirty` 두 지점 | ✅ private `InsertEntry`로 모은다. `AddSubtree`→`AddItem` 금지 판단도 맞다 |
| §5-1 | `EquippedEntryId` 비대칭 | ✅ **맞다.** `RemoveEntry`가 비운다 + `UnequipWeapon`은 write-back만 |
| §5-2 | FastArray 복사 시맨틱 | 🔀 **기전은 확정. 그런데 결과가 과장됐다** — 우리 식별자는 `ReplicationID`가 아니라 `EntryId`다 |
| §5-3 | 커맨드 구멍 둘 | ✅ 둘 다 맞다. **해법은 이미 프로젝트 안에 있다** (`EP.Loot.List`의 `[server-only]` 패턴) |
| §6-1 | 컨테이너가 두 함수로 성립 | ✅ 적는다. `Server_MoveItem` 일반형은 만들지 않는다 |
| §6-2 | `GetCapacity`와 `SlotId` | ✅ 함정표 한 줄 |
| §6-3 | `AddSubtree` 루트만 칸 검사 | ❌ **걱정이 근거 없다.** 루트만 검사하는 것이 축소가 아니라 **정확한** 것이다 |
| §7-1 | `MaxStack` 데드 필드 | 🚫 **재론 대상이다** — `LOOT_STATUS.md` 확정표가 "사용자 지시로 남김"이라 적었다 |
| §7-2 | 03-A/B/C 분할 | ⚠️ **분할선이 두 군데 어긋나 있다.** 03-7과 완료조건 1 |

**한 줄:** 이번 답의 핵심은 **"드랍은 RPC로 남기고, 대신 7차가 세운 규칙 문장을 정확히 다시 쓴다"** 이고, 결정타는 검수 요청이 요구한 바로 그것 — **Lyra 전체에서 손으로 쓴 서버 RPC는 정확히 하나이고, 그 하나가 인벤토리 슬롯 변경(`int32`)이다.**

---

## 1. 전제 정정 — §1-2는 7차와 같은 자리에서 다시 반쪽이다

```
$ grep -rn "UFUNCTION(Server" EmploymentProj/Source/EmploymentProj/
(결과 없음)
```

이 결과는 사실이다. 결론이 반쪽이다.

**7차 §1에서 이미 한 번 정정한 내용이 그대로 재발했다.** `UEPGA_Interact`는 `LocalPredicted`이고, F를 누를 때마다 이 줄이 실행된다.

```cpp
// AbilitySystemComponent_Abilities.cpp:1921-1923
if (TriggerEventData)
{
    ServerTryActivateAbilityWithEventData(Handle, Spec->InputPressed, ScopedPredictionKey, *TriggerEventData);
}
```

`ServerTryActivateAbilityWithEventData`는 `UFUNCTION(Server, Reliable, WithValidation)`이다. **F키 한 번 = 서버 RPC 한 번**이고, `FGameplayEventData` 구조체가 통째로 파라미터로 실린다.

즉 §1-2를 정확히 쓰면 이렇다.

> **손으로 선언한 서버 RPC가 0개다. 서버 RPC 호출은 게임플레이 입력마다 나간다.**

이 정정이 §2에서 중요한 이유: **"`Server_DropItem`이 들어오면 이 프로젝트 최초의 서버 RPC가 된다"는 문장이 성립하지 않는다.** 최초의 *손수 만든* RPC일 뿐이고, 그건 아키텍처 사건이 아니라 코드 스타일 사건이다. §2-2(a)의 무게가 여기서 한 단계 내려간다.

> **부수 확인:** 트리거 경로의 게이트는 7차가 인용한 `:1755` 하나가 아니다. `TriggerAbilityFromGameplayEvent`가 `:2482`에서 `HasNetworkAuthorityToActivateTriggeredAbility`로 한 번 더 거른다. 그 함수(`:2664-2680`)는 `LocalPredicted`면 `IsLocallyControlled()`, `ServerOnly`면 `IsOwnerActorAuthoritative()`를 반환한다 — **02 STATUS의 "`LocalPredicted`가 아니면 동작하지 않는다"가 트리거 경로에서도 같은 이유로 성립한다.** 근거 줄만 추가하면 된다.

---

## 2. ★ 최대 주제 — **A. `Server_DropItem`을 유지한다**

### 2-1. 결정타 — Lyra의 손수 만든 서버 RPC는 **정확히 하나**이고 그게 이것이다

```
$ grep -rn "UFUNCTION(Server" LyraStarterGame/Source/LyraGame/
Equipment/LyraQuickBarComponent.h:30:  UFUNCTION(Server, Reliable, BlueprintCallable, Category="Lyra")
```

**1건.** 게임 모듈 전체에서 하나다. 그리고 그 하나가 이것이다.

```cpp
// LyraQuickBarComponent.h:18,30-31
class ULyraQuickBarComponent : public UControllerComponent
...
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Lyra")
    void SetActiveSlotIndex(int32 NewIndex);
```

```cpp
// LyraQuickBarComponent.cpp:135-147
void ULyraQuickBarComponent::SetActiveSlotIndex_Implementation(int32 NewIndex)
{
    if (Slots.IsValidIndex(NewIndex) && (ActiveSlotIndex != NewIndex))
    {
        UnequipItemInSlot();
        ActiveSlotIndex = NewIndex;
        EquipItemInSlot();
        OnRep_ActiveSlotIndex();
    }
}
```

**우리 §2-3이 우려한 조건이 전부 맞아떨어진다.** GAS를 뼈대로 쓰는 프로젝트다. 입력은 어빌리티로 간다. 그런데 **파라미터가 액터가 아닌 스칼라(`int32` 슬롯 인덱스)인 인벤토리/장비 상태 변경 하나만** 손수 만든 서버 RPC로 남겼다. `EventMagnitude`에 싣지도, `FGameplayAbilityTargetData` 서브클래스를 만들지도 않았다.

호출자도 게임플레이 입력이다 — `CycleActiveSlotForward/Backward`(`.cpp:46-84`)가 곧바로 이 RPC를 부른다. 즉 **"휠로 무기 바꾸기"라는 입력이 어빌리티를 거치지 않는다.**

> 이 grep 한 줄이 §2 전체보다 무겁다. 7차에서 Lyra의 `Interaction/` 모듈을 근거로 "우리는 안 가져온다"를 판정했던 것과 **같은 종류의 직독 증거**이고, 이번엔 방향이 반대다.

### 2-2. Lyra가 어디서 선을 긋는가 — 그리고 그 선이 우리 것과 같다

| Lyra | 형태 | 근거 |
|---|---|---|
| 줍기 | **어빌리티** | `UPickupableStatics::AddPickupToInventory`가 `BlueprintAuthorityOnly` + `meta = (WorldContext = "Ability")` (`IPickupable.h`). 어빌리티에서 부르라고 시그니처가 말한다 |
| 상호작용 | **어빌리티** | `LyraGameplayAbility_Interact` (7차에서 확인) |
| 장비 착용/해제 | **서버 함수** (RPC 아님) | `LyraEquipmentManagerComponent.h:122,125` `BlueprintAuthorityOnly` — 이미 서버에 있는 코드가 부른다 |
| **활성 슬롯 변경** | **서버 RPC(`int32`)** | 위 |

**선은 "어빌리티 대 RPC"가 아니라 "월드 상호작용이냐, 서버가 이미 가진 상태의 변경 요청이냐"다.**

- 줍기·상호작용은 **클라가 월드를 조회해서 대상을 고른다.** 대상 액터가 있고, 서버가 그 선택을 세계 상태로 재검증해야 하고(거리·가시성), 클라/서버 불일치가 생길 수 있다 → 어빌리티. **7차가 Step 02를 B로 판정한 이유가 정확히 이것이다.**
- 슬롯 변경·드랍은 **클라가 이미 자기가 받은 서버 데이터 중 하나를 지목한다.** 월드 조회가 없다. 서버는 `Slots.IsValidIndex(NewIndex)` / `ContainsEntry(EntryId)` 하나로 완전히 검증된다. 불일치가 생길 자리가 없다 → 상태 변경 요청.

**드랍은 후자다.** `EntryId`는 서버가 발급해서 `COND_OwnerOnly`로 그 클라에게만 보낸 번호다. 클라가 그걸 되돌려주는 것은 **월드에 대한 주장이 아니라 자기 인벤토리 행에 대한 지목**이다.

### 2-3. 세 갈래의 실제 가격

**ⓐ `EventMagnitude`에 싣는다 — 관용구가 아니다.** (§2-4 ②, §8-1 답)

전수 조사했다.

| 위치 | 대입값 | 성격 |
|---|---|---|
| `AbilitySystemComponent_Abilities.cpp:2643` | `EventData.EventMagnitude = NewCount;` | **태그 개수.** 크기다 |
| Lyra `LyraHealthComponent.cpp:163` | `Payload.EventMagnitude = DamageMagnitude;` | 데미지. 크기다 |
| `AbilitySystemComponent.cpp:2022-2024` | 디버그 출력 `%.3f` | 소수로 찍는다 |

**엔진과 Lyra를 통틀어 식별자로 쓴 곳이 0건이다.** 그리고 `:2643`은 그나마 **RPC를 안 탄다** — 바로 아래 `InternalTryActivateAbility`(`:2645`)를 직접 부르는 로컬 경로다. 즉 "정수를 float 필드에 넣어 네트워크로 보낸 선례"는 **한 건도 없다.**

디버거에 `EventMagnitude: 17.000`이 찍히고 그게 "17번 엔트리"라는 걸 아는 사람이 자기 자신뿐인 코드다. 값 손실은 없지만(2²⁴까지 무손실) **의미론 손실이 전부다.** 기각.

**ⓑ `FGameplayAbilityTargetData` 서브클래스 — 기술적으로는 되지만 int32 하나에 붙일 의식이 아니다.**

최소 형태가 이렇다 (`GameplayAbilityTargetTypes.h:560-652`의 `FGameplayAbilityTargetData_SingleTargetHit`이 그 형태다).

```cpp
USTRUCT()
struct FEPTargetData_EntryId : public FGameplayAbilityTargetData
{
    GENERATED_USTRUCT_BODY()
    UPROPERTY() int32 EntryId = INDEX_NONE;

    virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FEPTargetData_EntryId>
    : public TStructOpsTypeTraitsBase2<FEPTargetData_EntryId>
{ enum { WithNetSerializer = true }; };   // ← 주석이 "For now this is REQUIRED" (:650)
```

여기에 `FGameplayAbilityTargetDataHandle`은 원소를 **`TSharedPtr` 힙 할당 + 스크립트 struct 폴리모픽 직렬화**로 나른다. `int32` 하나를 보내려고 치르는 값으로는 비싸다. **그리고 결정적으로, Lyra가 정확히 같은 상황에서 이걸 안 썼다.**

**ⓒ 직접 RPC — 선택.**

### 2-4. B의 진짜 가격 — GAS 기능을 하나 쓰자고 배관을 하나 더 만든다

요청의 §2-3이 이미 정확히 셌다. 다시 확인만 한다.

| GAS 기능 | 드랍이 쓰는가 |
|---|---|
| 예측 | ❌ 03-5가 "하지 않는다"로 확정 |
| 코스트 | ❌ |
| 쿨다운(GE) | ❌ `DropCooldown`은 **픽업**에 붙는다. 플레이어 ASC와 무관 |
| 애님/몽타주 | ❌ |
| `TargetData` | ❌ 대상이 액터가 아니다 |
| **`ActivationBlockedTags`** | ✅ **하나** |

`ActivationBlockedTags` 하나를 얻으려고 치르는 값은 이렇다.

- 어빌리티 클래스 1 + 태그 1 + `DefaultAbilities` 항목 1 (여기까진 7차 B의 가격과 같다)
- **그런데 파라미터 전달 수단이 없어 ⓐ 또는 ⓑ를 추가로 산다** — 7차에는 이게 0원이었다. `FGameplayEventData::Target`이 공짜였기 때문이다
- Step 04 UI에서 "이 엔트리를 버려"를 보낼 때 **UI → 이벤트 페이로드 → 어빌리티 → 컴포넌트** 네 단계를 거친다. RPC면 **UI → 컴포넌트** 두 단계다

반대편, A가 사는 것: `if (ASC->HasMatchingGameplayTag(TAG_State_Dead)) return;` **한 줄**. 그리고 그 한 줄은 §10-1이 지정하는 대로 `CanMutateInventory()` **한 곳**에만 놓인다.

### 2-5. CLAUDE.md §2가 이번엔 A 편이다

> *"판단 기준: 이 확장점이 `DOCS/` 어딘가에 이름으로 적혀 있는가?"*

| | 문서에 이름이 있는가 |
|---|---|
| `Server_DropItem(int32)` | ✅ `05_Loot_03_Inventory.md` 03-2, 03-5 |
| `Server_Equip(EntryId)` | ✅ `05_Loot_DOCS.md` §4-8 / `05_Loot_05_Equipment.md` |
| `Server_EquipBackpack(int32)` | ✅ 03-2, 03-6 |
| **`UEPGA_DropItem`** | ❌ **어느 문서에도 없다** |

**7차와 정확히 대칭이다.** 7차에서 B를 고른 결정적 문서 근거는 *"채널링은 GAS로"가 02-1에 이름이 있다*였다. 이번엔 이름이 있는 쪽이 A다. 그리고 03-5가 "클라 예측은 하지 않는다"를 **명시적으로 확정**했으므로, GAS로 가는 근거가 될 만한 미래가 문서에 하나도 예고돼 있지 않다.

### 2-6. ★ 7차 답변은 뒤집히지 않는다 — 뒤집히는 것은 거기 붙인 문장이다

요청 §2-4 ④가 "7차 답변을 뒤집는 근거"를 요구했다. **뒤집을 것이 없다.**

7차가 Step 02를 B로 판정한 실제 근거 셋은 지금도 전부 유효하다.

1. 대상이 **액터**라 `FGameplayEventData::Target`이 공짜였다 → 드랍에는 성립 안 함
2. 02-1에 **채널링(`GetInteractDuration`)** 이 이름으로 예고돼 있었다 → 드랍에는 없음
3. 상태 태그·차단·실패 회신 등 **여러 기능**이 걸려 있었다 → 드랍은 하나

뒤집히는 것은 7차가 그 판정에 붙인 **일반화된 한 문장**이다.

```diff
- 게임플레이 입력의 진입점은 어빌리티 하나다
+ 클라이언트가 서버에 요청하는 경로는 둘뿐이다.
+   ① 월드 상호작용, 그리고 시간·비용·애님이 붙는 행동  →  어빌리티
+   ② 서버가 이미 소유한 상태에 대한 변경 요청           →  UEPInventoryComponent의 서버 RPC
```

**이 문장이 Lyra의 선과 같고, 이미 짜여 있는 우리 코드와도 같다.** 그리고 §2-2(b)가 걱정한 "넷으로 는다"가 여기서 풀린다.

| 앞으로 올 것 | 새 문장이 배정하는 곳 | 늘어나는 것 |
|---|---|---|
| Step 05 무기 장착 | ② — `Server_Equip(EntryId)` | 같은 클래스에 RPC 하나 |
| §4-9 소모품 사용 | ① — `GrantedAbility` (이미 어빌리티) | 0 |
| §7-1 컨테이너 **열기** | ① — 이미 `UEPGA_Interact`가 한다 | 0 |
| §7-1 컨테이너 **가져가기** | ② — 컨테이너 UI에서의 상태 변경 | 같은 클래스에 RPC 하나 |
| §7-2 자판기 구매 | ② | 같은 클래스에 RPC 하나 |

**"넷으로 는다"는 사실이지만, 넷이 전부 한 클래스의 상태 변경 요청이라 배관이 늘어나는 게 아니라 같은 배관의 함수가 느는 것이다.** 어빌리티로 갔으면 어빌리티 클래스가 넷이 되거나, `EntryId` 인코딩 규약을 넷이 공유하게 된다.

> **되돌아갈 신호를 명시한다.** 다음 중 하나가 참이 되면 그 항목만 어빌리티로 올린다 —
> ⓐ 버리기/장착에 **시전 시간이나 몽타주**가 붙는다 (`UEPGA_Skill_Base::CastTime` 구조가 이미 있다)
> ⓑ 클라 **예측**이 필요해진다
> ⓒ RPC가 `UEPInventoryComponent` **바깥 클래스**에 생기려 한다 ← 이게 진짜 경계선이다

### 2-7. §2-4 ⑤ 부정 확인 — 셋 다 쓰지 않는 판단이 맞다

| | 판정 | 이유 |
|---|---|---|
| `DropCooldown` → GE 쿨다운 | ❌ 안 쓴다 | 쿨다운의 소유자가 **픽업 액터**다. 플레이어 ASC의 GE는 "이 플레이어가 X초간 못 한다"를 표현하는 것이고, 우리가 원하는 건 "이 픽업을 X초간 아무도 못 줍는다"다. **주체가 다르다.** GE로 하면 버린 사람만 못 줍고 옆 사람은 즉시 줍는다 — 완료 조건과 반대 |
| 칸 계산·`bFungible` → GE/어트리뷰트 | ❌ 안 쓴다 | 어트리뷰트는 스칼라 하나에 클램프·집계·예측이 붙은 물건이다. `UsedSlots`는 **컨테이너별로 다른 값**이고 `SlotSize`는 DT에서 나온다. 어트리뷰트로 만들면 컨테이너 수만큼 어트리뷰트가 필요하거나(배낭은 런타임에 생긴다) 매 변경마다 GE를 쏴야 한다. 03-3이 "캐시하지 않고 매번 계산"으로 간 이유가 그대로 여기 적용된다 |
| `OnInventoryChanged` → 태그 이벤트 | ❌ 안 쓴다 | **태그는 상태이고 알림은 사건이다.** "인벤토리가 바뀌었다"는 지속 상태가 아니라 에지다. 태그로 하면 켰다 끄는 코드가 필요하고 그게 곧 델리게이트를 태그로 흉내 내는 것이다. 게다가 복제 경로가 ASC로 갈라져 `COND_OwnerOnly` 하나로 통제하던 규칙이 둘이 된다 |

> 셋 다 **"GAS에 비슷한 이름의 기능이 있다"와 "그 기능이 이 문제를 푼다"를 구분하는** 문제다. 이 구분을 §10-1 문안에 한 줄로 남긴다.

---

## 3. Step 02 실물과 어긋난 곳 3건

### 3-1. `OnInteract` 시그니처 — 확인, 그리고 `bClaimed` 되돌림은 이미 형태로 정해져 있다

```cpp
// Public/Interaction/EPInteractable.h:23,27  (실물)
virtual bool CanInteract(AEPCharacter* Interactor, FText& OutReason) const = 0;
virtual bool OnInteract (AEPCharacter* Interactor, FText& OutReason) = 0;
```

문서 03-4의 `void AEPPickup::OnInteract(AEPCharacter* Instigator)`는 stale이 맞다. 문안은 §10-2.

**되돌림 책임 — `OnInteract` 내부다. 선택지가 아니라 접근 권한 문제다.**

```cpp
// Public/Loot/EPPickup.h:24,50
bool IsClaimed() const { return bClaimed; }   // 게터만 있다
...
private:
    bool bClaimed = false;                     // private, setter 없음
```

`UEPGA_Interact`가 되돌리려면 **세터를 새로 뚫어야 한다.** 그 세터의 소비자는 영원히 하나(어빌리티)이고, 뚫는 순간 *"`bClaimed`를 밖에서 켤 수도 있다"* 가 성립해서 함정 #3(선점)이 약해진다. 반면 `OnInteract` 안이면 진입에서 켜고 각 실패 반환 앞에서 끄는 **국소 불변식**으로 끝난다.

> **다만 지금 코드에는 그 실패 경로가 하나도 없다** — `OnInteract`이 무조건 `true`다(`EPPickup.cpp:66-73`). Step 03에서 갈래가 처음 생기므로, 그때 **`return false` 하는 모든 줄 앞에 `bClaimed = false;`가 있는지**가 검수 항목이다. 갈래가 셋(Inv 없음 / 본체+배낭 둘 다 실패 / 데이터 오류)이라 한 줄만 빠져도 그 픽업은 **아무도 못 줍는 채로 바닥에 남는다**(함정 #5).

**`FText` 대 `FGameplayTag` — `FText`를 유지한다.** 7차 3-4의 판정이 그대로 산다. 이유가 하나 더 붙는데, §2가 A라서 실패 사유가 GAS를 타지 않는다. `FGameplayTag`로 바꾸면 클라에 태그→문자열 매핑 테이블을 따로 만들어야 하고, "가방에 자리가 없습니다"는 **지역화 문자열이지 게임 상태가 아니다.** (`FText`가 RPC에서 네임스페이스+키+원문을 통째로 실어 보내는 비용은 7차 3-4에 적힌 그대로다 — 실패 1회당이라 허용한다.)

### 3-2. `InitPickup` 파급 3곳 — 확인. 그리고 헤더 위치는 EPTypes.h가 아니다

세 곳 전부 확인했다.

| 위치 | 실물 |
|---|---|
| `EPItemSpawner.cpp:91` | `Pickup->InitPickup(RolledId, NewState);` — 유일한 호출부 |
| `EPLootDebugCommands.cpp:133` | `P->GetState().Charges, P->IsClaimed() ? ...` (서버 분기) |
| `EPLootDebugCommands.cpp:137` | `... L.Z, P->GetState().Charges);` (클라 분기) |

**파일:줄로 박는 게 맞다.** 03-4는 "스포너 경로도 같은 함수를 쓴다"만 적어서, `GetState()`를 읽는 두 곳이 **`EP.Loot.List` 안에 있다는 것**이 안 보인다. 그 커맨드는 Step 01 문서 소속이라 Step 03만 보고 고치면 컴파일이 깨진다.

> **`GetState()`의 대체형도 지정해야 한다.** `Payload`가 배열이므로 `GetState()`는 성립하지 않는다. `EP.Loot.List`가 필요한 것은 **루트의 `Charges`와 서브트리 크기**다 →
> `int32 GetPayloadNum() const` + `const FEPItemState& GetRootState() const`(빈 배열 가드 포함) 둘로 나누면 커맨드 두 줄만 바뀐다. LOOT_STATUS 확정표가 이미 *"`DropCooldown`과 버려진 배낭의 `Payload` 개수를 그쪽에서 본다"*(03-9)고 적었으므로 **개수 게터는 이미 이름이 있다.**

**`FEPInventoryEntry`의 위치 — `Public/Inventory/EPInventoryTypes.h` 신규 헤더.**

세 후보를 비교했다.

| 후보 | 문제 |
|---|---|
| `Types/EPTypes.h`로 내림 | **거의 모든 파일이 이걸 include한다**(트레이스 채널 상수·SSR 스냅샷이 여기 있다). `FFastArraySerializerItem` 상속이라 `Net/Serialization/FastArraySerializer.h`가 따라 들어가고, 그건 전투·애님까지 전부 끌고 온다. 그리고 `FEPInventoryEntry`는 **복제 계층 타입**이지 공용 스칼라가 아니다 |
| `EPInventoryComponent.h`에 둠 (Lyra 방식) | Lyra는 `FLyraInventoryEntry`/`FLyraInventoryList`를 컴포넌트 헤더에 둔다. 그런데 **Lyra의 픽업은 엔트리를 안 든다**(`FInventoryPickup`은 `ULyraInventoryItemInstance*` 배열이다). 우리는 `AEPPickup`이 엔트리 배열을 들어야 하므로 **`Loot/` → 컴포넌트 헤더 전체** 의존이 생긴다 |
| **전용 헤더** ✅ | `AEPPickup`이 필요한 것은 **엔트리뿐**이다. `FEPInventoryList`(직렬화기)와 컴포넌트는 그대로 컴포넌트 헤더에 남는다 |

> **Build.cs는 손댈 필요가 없다.** `NetCore`가 `Engine`의 **Public** 의존이라(`Engine.Build.cs:85`) 전이로 들어온다. Lyra가 `LyraGame.Build.cs:57`에서 `NetCore`를 Private에 명시한 것은 관례이지 요구가 아니다. — *없는 작업을 문서에 넣지 않기 위해 확인했다.*

### 3-3. `DropCooldown` — 지정해야 한다. ★ **그리고 지금 설계로는 회색 프롬프트가 못 뜬다**

현재 구현은 `bClaimed`만 본다(`EPPickup.cpp:56-64`). 03-5가 추가할 것을 문서가 지정해야 한다는 판단은 맞다. **그런데 필드 모양보다 먼저 걸리는 게 있다.**

완료 조건: *"버린 직후 0.5초 동안 그 픽업이 **회색 프롬프트로 표시**되고 서버가 거부한다"*

**앞의 절반이 지금 구조에서 성립하지 않는다.** 이유가 둘이고 둘 다 코드로 확인된다.

1. **클라가 쿨다운 값을 모른다.** 프롬프트 판정은 `UEPInteractionComponent::UpdateFocus()`(`EPInteractionComponent.cpp:94`)가 **클라이언트에서** `CanInteract`을 불러서 낸다. `bClaimed`가 복제되지 않아 회색 갈래가 이미 도달 불가라는 것은 02 STATUS가 적어뒀다. `DropCooldownEndTime`을 복제하지 않으면 **똑같이 도달 불가다.**
2. **같은 대상을 보고 있으면 프롬프트가 갱신되지 않는다.** `if (NewFocus == FocusedActor) return;`(`:78`). 버린 픽업을 계속 보고 있으면 0.5초가 지나도 **회색이 안 풀린다.** 02 STATUS가 "대응은 Step 03에서 정한다"로 넘긴 항목이다.

**둘 다 푸는 최소 형태:**

```cpp
// AEPPickup — 복제한다. 값은 서버 월드 시간
UPROPERTY(Replicated) float DropCooldownEndTime = 0.f;   // 0이면 쿨다운 없음

bool AEPPickup::CanInteract(AEPCharacter*, FText& OutReason) const
{
    if (bClaimed) { OutReason = ...; return false; }

    const AEPGameState* GS = GetWorld()->GetGameState<AEPGameState>();
    if (GS && GS->GetServerWorldTimeSeconds() < DropCooldownEndTime)
    {
        OutReason = NSLOCTEXT("EP", "PickupCooling", "방금 버린 아이템입니다");
        return false;
    }
    return true;
}
```

- **타이머 핸들이 아니라 `float`이다.** 콜백으로 할 일이 없다 — 판정은 비교 한 번이고, 타이머를 쓰면 서버에만 있는 상태가 되어 1번 문제가 그대로 남는다
- **`GetServerWorldTimeSeconds()`를 쓴다.** 로컬 시간을 쓰면 클라/서버 판정이 갈린다. SSR이 이미 이 시계를 쓴다(`GS->GetServerWorldTimeSeconds()`, 프로젝트 관례)
- **Dormancy와 충돌하지 않는다.** `InitPickup` + `StartDropCooldown`을 `SpawnActor`와 **같은 프레임**에 부르면(Step 01의 `InitPickup` 규칙과 동일) 초기 복제에 실려 나가고, 이후 값이 안 바뀌므로 `DORM_Initial`이 유지된다. **`FlushNetDormancy()`가 필요 없다**
- 2번(프롬프트 갱신)은 `UpdateFocus`의 조기 반환을 **결과 기준**으로 바꾼다 — §10-3

> **이걸 안 짚고 가면 어떻게 되는가:** 서버 거부는 동작하는데 프롬프트만 안 변한다. 증상이 "가끔 못 줍는데 UI는 줍을 수 있다고 한다"이고, **`DropCooldown` 로직이 아니라 HUD를 파게 된다.** Step 02가 남긴 두 개의 "지금은 도달 불가" 갈래가 여기서 동시에 첫 소비자를 만난다.

---

## 4. 문서 내부 모순 3건

### 4-1. 콜백 셋 — 지운다. **무의미한 게 아니라 링크 에러다**

```cpp
// 03-1 (현재 문서)
void PostReplicatedAdd(const struct FEPInventoryList& Serializer);
void PostReplicatedChange(const struct FEPInventoryList& Serializer);
void PreReplicatedRemove(const struct FEPInventoryList& Serializer);
```

**엔진은 셋을 요구하지 않는다.** 기반 struct가 인라인 빈 구현을 갖고 있다.

```cpp
// FastArraySerializer.h:341,349,356
inline void PreReplicatedRemove (const struct FFastArraySerializer& InArraySerializer) { }
inline void PostReplicatedAdd   (const struct FFastArraySerializer& InArraySerializer) { }
inline void PostReplicatedChange(const struct FFastArraySerializer& InArraySerializer) { }
```

호출은 무조건 나간다.

```
FastArraySerializer.h:1139   Items[idx].PreReplicatedRemove(ArraySerializer);
FastArraySerializer.h:1163   Items[idx].PostReplicatedAdd(ArraySerializer);
FastArraySerializer.h:1174   Items[idx].PostReplicatedChange(ArraySerializer);
```

**여기가 요청이 묻지 않은 부분이다.** 파생 struct가 같은 이름을 **선언만** 하면 기반의 인라인 정의를 **가리고**(name hiding), 위 호출이 파생 쪽으로 붙는다. 정의가 없으면 컴파일은 통과하고 **링크 시점에 unresolved external**이 난다. 즉 03-1을 그대로 구현하면 셋을 **빈 함수로라도 .cpp에 써야** 빌드가 된다 — 요청이 예측한 *"구현자가 빈 함수 셋을 만든다"* 가 실수가 아니라 **강제**가 된다.

- **셋 중 일부만 정의해도 같다** — 정의 안 한 것만 링크 에러다. "일부만 정의"라는 안전한 중간 상태가 없다
- 아무것도 선언하지 않으면 기반의 no-op이 불려서 **아무 일도 안 일어난다.** 이게 우리가 원하는 것이다

**Lyra도 아이템에 안 둔다.** `FLyraInventoryEntry`가 정의하는 것은 `GetDebugString()` 하나뿐이고, 셋은 **직렬화기 쪽**에 배열 단위 시그니처로 있다.

```cpp
// LyraInventoryManagerComponent.h — FLyraInventoryList
void PreReplicatedRemove (const TArrayView<int32> RemovedIndices, int32 FinalSize);
void PostReplicatedAdd   (const TArrayView<int32> AddedIndices,   int32 FinalSize);
void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
```

(직렬화기 쪽 셋도 기반에 no-op이 있다 — `FastArraySerializer.h:501,508,515`.)

**Lyra가 그걸 정의하는 이유는 우리에게 없다.** Lyra는 항목마다 `FLyraInventoryChangeMessage`(개수 델타 포함)를 쏘려고 `LastObservedCount`라는 `NotReplicated` 필드까지 뒀다. 우리는 03-7이 **수신 1회당 1회 전체 갱신**으로 확정했으므로 델타가 필요 없다.

> **03-7의 `PostReplicatedReceive`는 안전하다.** 그쪽만 진짜 컨셉 검사다 — `CPostReplicatedReceiveFuncable`(`:533-537`)로 존재 여부를 보고 `CallPostReplicatedReceiveOrNot`(`:701-710`)이 갈린다. **정의하면 불리고 안 하면 안 불린다.** 이름 가림 문제가 없다.

### 4-2. `bIsRoot` — 진단이 맞다. 분리한다

문서가 `RemoveEntry ↔ AddSubtree` 계약을 세 곳에서 *"규율이 아니라 형태로 막는다"*고 강조해 놓고, **그 형태를 깰 수 있는 파라미터를 public 시그니처에 뒀다.** 자기 논리와 어긋난다는 진단은 정확하다.

```cpp
RemoveEntry(Id, &Out, /*bIsRoot=*/false)   // → 루트 정규화 생략 → AddSubtree가 루트를 못 찾는다
```

그리고 그 실패는 03-4가 **"흔한 경로"**라고 직접 경고한 것(배낭 속 무기 버리기)과 **증상이 같다.**

**정답은 public/private 분리다.**

```cpp
public:
    bool RemoveEntry(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved = nullptr);
private:
    bool RemoveEntryInternal(int32 EntryId, TArray<FEPInventoryEntry>* OutRemoved, bool bIsRoot);
```

`RemoveEntry`는 `RemoveEntryInternal(EntryId, OutRemoved, /*bIsRoot=*/true)` 한 줄이고, `RemoveChildrenRecursive`가 `RemoveEntryInternal(..., false)`를 부른다. **밖에서 `false`를 넘길 문법이 사라진다** — 문서가 세 번 말한 그 원칙 그대로다.

> 대안(별도 함수 `RemoveSubtreeAsRoot` / `RemoveSubtreeAsChild`)도 되지만, 재귀가 자기 자신을 부르는 지금 구조가 3b(자식마다 장착 검사)를 보장하는 핵심이므로 **재귀 대상을 하나로 유지하는 형태**가 낫다.

### 4-3. `MarkItemDirty` 두 지점 — private `InsertEntry`로 모은다

03-2가 *"`MarkItemDirty`도 여기 한 곳"* 이라 적었는데, `AddItem`(03-3)과 `AddSubtree`(03-4)가 각자 삽입한다. 모순이 맞다.

```cpp
private:
    // 발급·삽입·MarkItemDirty의 유일한 지점. 칸 검사도 합치기도 하지 않는다
    int32 InsertEntry(int32 Parent, FName ItemId, const FEPItemState& State, FName SlotId);
```

- `AddItem` = `bFungible` 합치기 → `CanFit` → `InsertEntry`
- `AddSubtree` = 루트 `CanFit` → 루프마다 `InsertEntry` + `OldToNew` 기록

**`AddSubtree`가 자식에 `AddItem`을 부르면 안 된다는 판단도 맞다.** 두 가지가 동시에 깨진다.

1. **`bFungible` 합치기** — 배낭 안의 현금뭉치가 본체의 현금뭉치와 합쳐져 **컨테이너를 탈출한다.** `AddItem`은 `FindFungibleEntryId(ItemId)`가 컨테이너를 안 보기 때문에(03-2 시그니처에 `Container` 인자가 없다) 어디에 있든 첫 번째를 찾는다
2. **`CanFit`** — 자식은 **루트 자신 안**으로 들어가는데 `AddItem`은 넘겨받은 `Container` 기준으로 검사한다. 재매핑된 부모를 넘기면 그 시점에 아직 용량 계산이 성립하지 않는다

> **①은 문서가 아직 모르는 버그의 씨앗이기도 하다.** `FindFungibleEntryId(FName)`에 컨테이너 인자가 없으면 **정상 획득 경로에서도** "배낭 속 현금이 본체 현금으로 합쳐진다"가 난다. 칸이 안 늘어서 증상이 거의 없고, 배낭을 벗는 순간 돈이 딸려 나온다. **`FindFungibleEntryId(int32 Container, FName ItemId)`로 시그니처를 고치는 것을 03-2에 넣어야 한다.**

---

## 5. 실무 위험 3건

### 5-1. `EquippedEntryId` — 비대칭이 맞다. `RemoveEntry`가 비운다

```cpp
if (EntryId == EquippedEntryId)
    C->UnequipWeapon();                       // 번호는 안 비운다
if (EntryId == EquippedBackpackEntryId)
    EquippedBackpackEntryId = INDEX_NONE;     // 비운다
```

**대칭으로 맞추는 게 맞다.** 근거가 취향이 아니라 셋이다.

1. **책임이 다르다.** `UnequipWeapon`은 **다른 컴포넌트**(`UEPCombatComponent`)의 함수다. 인벤토리 필드를 비우는 책임이 거기 있으면 헤더 결합은 없어도 **의미 결합**이 생긴다 — 03-2가 헤더 순환을 피하려고 만든 구조가 무의미해진다
2. **`UnequipWeapon`은 제거 없이도 불린다.** Step 05의 교체·사망 경로가 그렇다(LOOT_STATUS: "교체·버리기·사망 세 경로가 전부 여기를 거치게"). 거기서 `EquippedEntryId`를 비우는 것은 맞지만, **`RemoveEntry`가 그걸 믿을 근거는 없다**
3. **`RemoveEntry`가 캐스케이드로 자식을 지울 때 `UnequipWeapon`이 여러 번 불릴 수 있다.** 배낭 안의 무기가 장착 중이면 자식 노드에서 불린다. 그때 번호를 비우는 주체가 하나여야 순서 논쟁이 안 생긴다

→ **`RemoveEntry` 내부에서 `UnequipWeapon()` 호출 직후 `EquippedEntryId = INDEX_NONE;`.** `UnequipWeapon`은 write-back만 한다.

**"두 단계 뒤에 처음 실행된다"는 구조적 위험 — 문서가 이미 절반은 표시했다.** 03의 "이 단계에서 하지 않는 것"에 ★로 적혀 있다. 부족한 것은 **그 사실이 Step 05 문서에도 있어야** 한다는 점이다.

```
Step 05 완료 조건에 추가:
  [ ] 배낭 속 무기를 장착한 상태로 배낭을 버린다 → 잔탄 보존 + EquippedEntryId == INDEX_NONE
      + 손에 든 AEPWeapon 액터가 사라진다
      ★ 이 경로는 Step 03에서 작성됐지만 Step 05에서 처음 실행된다
```

> `ensureMsgf`를 `RemoveEntry`에 넣는 방법도 있으나 **Step 03 내내 항상 거짓인 분기라 ensure가 울릴 일이 없다.** 방어를 코드로 넣을 자리가 아니라 **완료 조건으로 넘길 자리**다.

### 5-2. ★ FastArray 복사 시맨틱 — **기전은 확정. 결과는 과장됐다**

**좋은 쪽 해석: 맞다.** 스냅샷·`Payload`·`FindEntry` 값 복사가 전부 안전하다. 복사 생성자(`:308-312`)와 `operator=`(`:314-323`)가 셋을 `INDEX_NONE`으로 리셋하므로 **복제 ID가 따라붙지 않는다.** 03-1에 한 줄 적을 값어치가 있다.

**나쁜 쪽 해석: 기전은 정확히 그렇게 돌아간다.** 엔진 소스로 사슬을 확인했다.

```cpp
// ① 살아있는 원소에 통째 대입 → ID가 날아간다
// FastArraySerializer.h:314-323
FFastArraySerializerItem& operator=(const FFastArraySerializerItem& In)
{ if (&In != this) { ReplicationID = INDEX_NONE; ReplicationKey = INDEX_NONE; ... } return *this; }

// ② 다음 송신에서 새 ID를 발급한다
// :925-928
if (Item.ReplicationID == INDEX_NONE) { ArraySerializer.MarkItemDirty(Item); }
// :443-445  →  Item.ReplicationID = ++IDCounter;

// ③ 옛 맵에 그 ID가 없으므로 "New!"로 분류되고 삭제 카운트가 는다
// :950-957
else { /* New */ ChangedElements.Add(...); ++DeleteCount; }

// ④ 옛 ID는 새 맵에 없으므로 삭제 목록에 들어간다
// :961-976
if (!NewIDToKeyMap.Contains(It.Key())) { DeletedElements.Add(It.Key()); ... }
```

**수신 측은 remove + add로 본다.** 요청의 두 번째 주장이 그대로 맞다. `PreReplicatedRemove` → `PostReplicatedAdd`가 각각 불린다(`:1139`, `:1163`).

**그런데 결과가 과장됐다. 우리 UI의 식별자는 `ReplicationID`가 아니다.**

03-1이 이미 결정한 것이 이 문제를 먼저 막고 있다.

> *"`AddItem()`이 `NextEntryId++`로 발급한다 … UI는 `EntryId` 오름차순으로 그린다"*

`EntryId`는 우리가 만든 **복제되는 `UPROPERTY`**다. remove+add가 나도 `EntryId` 값은 그대로 실려 온다. 그러므로 —

| 요청이 예측한 피해 | 실제 |
|---|---|
| Step 04 슬롯 위젯이 재생성된다 | **어차피 재생성된다.** 03-7이 "수신 1회당 전체 갱신"으로 확정했다. remove+add든 change든 결과가 같다 |
| 선택/드래그가 끊긴다 | **`EntryId`로 선택을 들고 있으면 안 끊긴다.** 03-1이 이미 그렇게 정해뒀다 |
| — | ✅ **대역폭:** 변경된 필드만이 아니라 **엔트리 전체가 다시 나간다.** 내부 struct 델타(03-1이 인용한 `:218`)의 이득이 그 엔트리에서만 사라진다 |
| — | ✅ **`ReplicationID`가 단조 증가로 소진된다.** `int32`라 실질 문제는 아니지만 로그(`LogNetFastTArray`)가 읽기 어려워진다 |

**즉 규칙("필드 단위 대입")은 유지하되 이유를 바꿔 적는다.** "UI가 깨진다"가 아니라 **"델타가 사라지고, 클라가 삭제→추가로 보므로 항목 단위 콜백을 나중에 붙이면 그때 거짓말한다"** 가 정확하다.

> **지금 코드에 이 실수가 들어갈 자리가 없다는 것도 적어둔다.** `SetEntryCharges`가 `E.State.Charges = ...` 필드 대입이고(03-3), 그 외에 살아있는 원소를 건드리는 함수가 없다. **위험은 "나중에 `MoveItem`이나 write-back을 짜는 사람"이고, 그래서 지금 문서에 적는 것이 맞다.** 03-1의 한 줄로 충분하다 — 함정표에 올릴 만큼 현재 위험은 아니다.

### 5-3. 검증 커맨드 구멍 둘 — 둘 다 맞다. 해법이 이미 프로젝트 안에 있다

**① `NextEntryId`가 클라에서 거짓말한다 — 맞다.** `NextEntryId`는 서버 전용(03-2가 명시)이라 클라는 초기값 `1`을 본다. `EP.Inv.Dump`는 클라 허용이다.

**그런데 프로젝트가 이 문제를 이미 한 번 풀었다.**

```cpp
// EPLootDebugCommands.cpp:120-124  (EP.Loot.List)
const bool bAuthority = World->GetNetMode() != NM_Client;
UE_LOG(LogTemp, Log, TEXT("Idx, ItemId, Location, Charges%s, Claimed"),
    bAuthority ? TEXT("") : TEXT("[server-only]"));
```

**같은 형태를 쓴다.** 클라에서는 `NextEntryId = [server-only]`로 찍고 값을 안 낸다.

> **그리고 완료 조건은 그 열 없이도 닫힌다.** *"재번호되지 않는다"*는 **`EntryId` 열만으로 증명된다** — 셋 넣고 2번을 버린 뒤 하나 더 넣어 `1, 3, 4`가 나오면 끝이다. `NextEntryId`는 확인의 **편의**이지 증거가 아니다. 03-9 표에서 그 열의 역할을 "재번호 없음을 증명"에서 **"서버에서 한눈에 확인"**으로 낮춰 적는 게 정확하다.

**② `EP.Inv.Add`의 대상 지정 — `[PlayerIndex]`가 맞다.** §10이 "클라이언트 쪽에서 확인"을 원칙으로 세웠는데 `Add`가 서버 전용이면, 리슨서버 호스트의 인벤토리만 채우게 되어 `COND_OwnerOnly` 검증(`DumpAll`의 `Entries=0`)이 **애초에 성립하지 않는다** — 남의 가방이 비어 있어야 하는데 남의 가방이 항상 비어 있다.

```
EP.Inv.Add <ItemId> [PlayerIndex]     # 기본 0 = 첫 번째 PlayerController
EP.Inv.Drop <EntryId> [PlayerIndex]
```

`Drop`도 같이 받아야 한다. 안 그러면 "PIE 2번 창의 아이템을 버리는" 시나리오를 못 만든다.

> 순회는 `TActorRange<AEPPickup>`을 쓴 기존 커맨드와 같은 결로 `GEngine->GetWorldContexts()` 대신 **`World->GetPlayerControllerIterator()` 인덱스**면 충분하다. `ECVF_Cheat` + `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)` 가드도 기존과 동일하게 유지한다.

---

## 6. 확장성

### 6-1. §7-1 컨테이너가 두 함수로 이미 성립한다 — 맞다. 적되 함수는 만들지 않는다

```
Container->RemoveEntry(Id, &Sub)   →   MyInv->AddSubtree(INDEX_NONE, Sub)
```

**성립한다.** 그리고 `RemoveEntry`의 계약(전위 순회 / `In[0]`이 루트 / 루트 `Parent=INDEX_NONE`)이 **컴포넌트 경계를 넘어서도 그대로 유효하다** — 계약이 `EntryId` 공간에 의존하지 않고 배열 모양에만 의존하기 때문이다. 이건 우연이 아니라 `AddSubtree`가 `OldToNew` 재매핑을 하기 때문이고, **재매핑은 원래 "번호 공간이 다르다"를 전제한 설계**다. 버린 배낭을 되줍는 것이 이미 그 경우다.

**03-2에 한 줄 적는 게 맞다.** 안 적으면 §7-1에서 `TransferItem`이라는 세 번째 경로가 생기고, 그건 `RemoveEntry`의 4단계 순서를 우회할 자리를 새로 만든다.

**`Server_MoveItem(EntryId, TargetInv, TargetContainer)` 일반형을 지금 만들지 않는다는 판단도 맞다.** CLAUDE.md §2 그대로다 — 지금 소비자가 없다. **사실만 적고 함수는 만들지 않는 것**이 옳은 선이다.

> §2가 A이므로 §7-1이 올 때 생기는 것은 `UEPInventoryComponent`의 RPC 하나이고, 그 시그니처가 `Server_TakeFromContainer(AActor* Container, int32 EntryId)`가 된다. **그때가 "RPC가 인벤토리 컴포넌트 밖으로 나가려 하는가"(§2-6 신호 ⓒ)를 점검할 시점이다** — 대상 액터가 파라미터에 들어오는 순간 상호작용 쪽 성격이 섞이기 시작한다.

### 6-2. `GetCapacity`와 `SlotId` — 함정표 한 줄이면 된다

`GetCapacity(Container)`는 **용량**을 답하고, §7-3 부착물은 **슬롯 이름 집합**으로 제한된다. 서로 다른 질문이다.

**지금 넣을 것이 없다.** `SlotId` 필드가 이미 있고(03-1), `GetUsedSlots`가 `!E.SlotId.IsNone()`이면 건너뛴다(03-3). 즉 **부착물은 이미 용량 계산 바깥에 있다.** §7-3이 추가할 것은 `CanAttach(SlotId)` 계열의 별도 판정이지 `GetCapacity`의 확장이 아니다.

→ 함정표에 한 줄: *"`SlotId != None`인 자식은 `GetCapacity`가 답하는 대상이 아니다. §7-3은 용량이 아니라 슬롯 스키마로 제한한다."*

### 6-3. `AddSubtree`의 루트만 칸 검사 — ❌ **걱정이 근거 없다**

```cpp
if (!CanFit(Container, In[0].ItemId)) return INDEX_NONE;   // 칸 검사는 루트만
```

**이건 축소가 아니라 정확한 것이다.** 이유가 구조적이다.

> **서브트리의 자식은 언제나 루트(또는 루트의 자손) *안*으로 들어간다.** `AddSubtree`의 재매핑이 그렇게 만든다 — `Src.ParentEntryId == INDEX_NONE`인 원소(루트)만 `Container`를 부모로 받고, 나머지는 `OldToNew.FindRef(Src.ParentEntryId)`를 받는다. **자식이 `Container`의 칸을 먹는 경우가 문법적으로 없다.**

요청이 든 예 — *"용량 12 상자의 내용물을 용량 8 배낭으로"* — 를 두 경우로 나눠 보면 둘 다 안 깨진다.

| 실제 조작 | `AddSubtree` 호출 모양 | 검사 |
|---|---|---|
| 상자 **안의 아이템 하나**를 배낭으로 | 그 아이템이 루트 (`In.Num()==1`) | 루트 검사 = 그 아이템 검사 ✅ 정확 |
| **상자 자체**를 배낭에 | 상자가 루트. 내용물은 상자 안에 그대로 | 배낭에 대해선 상자 1개분만 든다 ✅ 정확 (상자의 용량은 상자가 어디 있든 상자의 성질이다) |

**문서에 적을 것은 "검사를 늘려라"가 아니라 "왜 루트만으로 충분한가"다.** 안 적으면 §7-1에서 누군가 서브트리 전체 합을 검사하는 코드를 넣고, 그러면 **배낭을 되주울 때 자기 내용물이 본체 칸을 먹는 것으로 계산돼 되줍기가 실패한다** — 완료 조건 9번이 깨진다. **지금 상태가 맞고, 잘못 "고치는" 쪽이 위험하다.**

> 다만 값싼 방어 하나는 있다: **자식이 루트의 용량을 넘는 배열**(데이터 오류 / 미래에 용량을 줄이는 기능)은 지금 아무도 안 본다. `ensureMsgf`로 드러내는 것은 03-4의 `OldToNew.FindRef` 방어와 같은 결이다. **필수는 아니다.**

---

## 7. 미결

### 7-1. `MaxStack` — 재론 대상이다

`EPItemData.h:36`에 선언 1줄, **읽는 코드 0곳**(전체 grep 확인). 사실은 맞다.

**그런데 이건 이미 결정된 항목이다.** `LOOT_STATUS.md` 확정표:

> `FEPItemData::MaxStack` — **읽지 않는다.** 스택 부활용 예약 필드로 남김 **(사용자 지시)**

검수가 뒤집을 자리가 아니다. 다만 §7-1이 붙인 조건 하나에는 답할 수 있다 — **§7-3 탄창으로 되살아날 여지는 없다.** 탄창의 잔탄은 `FEPItemState::Charges`이고(§8 확정표: "열쇠·붕대의 사용 횟수는 `Durability`가 아니라 `Charges`다"), `MaxStack`은 **엔트리 개수**의 상한이지 개체 내부 수량이 아니다. 두 축이 다르다.

→ **그대로 둔다.** 지울지는 사용자 결정이고, 지운다면 이유는 "§7-3에서 안 쓸 것이기 때문"이 아니라 "예약 필드를 두는 관례를 접기 때문"이다.

### 7-2. 03-A/B/C 3분할 — ⚠️ 분할선이 **두 군데** 어긋나 있다

**① `FScopedInventoryNotify`가 03-7 소속인데 03-A가 그걸 쓴다.** 요청의 지적이 맞다.

```cpp
// 03-3 AddItem — 03-A 범위
FScopedInventoryNotify Guard(this);          // ← 정의는 03-7
```

`SetEntryCharges`(03-3)도 쓴다. **03-7을 03-A로 옮긴다.** 옮기는 것이 자연스러운 이유가 하나 더 있다 — `PostReplicatedReceive`(03-7)가 없으면 03-A의 완료 조건 *"클라이언트에 복제된다"* 를 **`EP.Inv.Dump`로만** 봐야 하고, 델리게이트가 안 도는 상태로 Step 04에 넘어간다.

**② 완료 조건 1이 03-A에서 닫히지 않는다.** *"주운 아이템이 인벤토리에 들어가고"* — 줍기는 `AEPPickup::OnInteract`(03-4)이고 03-4는 03-C 소속이다. 03-A에는 `EP.Inv.Add`밖에 없다.

| 고친 분할 | 범위 | 완료 조건 |
|---|---|---|
| **03-A 코어** | 03-1 · 03-2 · 03-3 · **03-7** · 03-9 | **2~6** (전부 `EP.Inv.Add`로) |
| **03-B 배낭** | 03-6 + `GetCapacity` | 7 |
| **03-C 버리기·줍기** | 03-4 · 03-5 | **1, 8~13** |

- **`RemoveEntry`/`AddSubtree` 없이 03-A가 컴파일·실행되는가 → 된다.** 선언만 있으면 되고, `AddItem`이 부르지 않는다. 단 `RemoveEntry`를 **선언만 하고 정의를 안 하면 링크 에러**이므로(§4-1과 같은 함정) 03-A에서는 **빈 정의 대신 아예 선언도 미루는 편**이 낫다 — 헤더에 안 적으면 컴파일러가 잊어준다
- **§2가 A이므로 `Server_DropItem`은 03-C 소속이다.** ③번 질문(`UEPGA_DropItem`이 어디 소속인가)은 소멸한다

---

## 8. 실무 조사 결과 (§8 답변)

### 8-1. `EventMagnitude`를 식별자로 쓰는 선례 — **없다**

§2-3에 표로 정리했다. 엔진 1건(태그 개수), Lyra 1건(데미지), 둘 다 크기다. 그리고 엔진의 그 1건은 RPC를 타지 않는 로컬 활성화다(`_Abilities.cpp:2643` → `:2645` `InternalTryActivateAbility`).

**"관용구가 아니다"로 답한다.**

### 8-2. Lyra의 인벤토리 조작 — **어빌리티와 RPC를 성격으로 나눈다**

§2-1·2-2에 정리했다. 요약:

- 게임 모듈 전체 `UFUNCTION(Server` = **1건**, `ULyraQuickBarComponent::SetActiveSlotIndex(int32)`
- 그 컴포넌트는 `UControllerComponent` — **PlayerController에 붙어 있어** RPC 소유권이 자명하다
- 검증은 `Slots.IsValidIndex(NewIndex)` 하나 (`.cpp:137`)
- 줍기는 어빌리티(`AddPickupToInventory`가 `BlueprintAuthorityOnly` + `WorldContext="Ability"`)
- 장착/해제는 RPC가 아니라 **서버 전용 함수**(`LyraEquipmentManagerComponent.h:122,125`)
- **Lyra에는 "버리기"가 없다** (`grep -rn "Drop"` → 무관한 2건뿐). 드랍의 직접 선례는 없고, 가장 가까운 것이 위 슬롯 RPC다

> **우리와 다른 점 하나는 짚어둔다.** Lyra의 RPC는 **Controller** 컴포넌트에 있고 우리 것은 **Character** 컴포넌트에 놓인다(인벤토리 부착 위치 = Character, §8 확정). 둘 다 소유 클라에서 서버로 갈 수 있다 — Character는 possess된 폰이라 그 커넥션이 소유자다. **다만 사망 후 폰이 파괴된 상태에서 UI가 늦게 RPC를 쏘면 그냥 드랍된다**(채널이 없다). 조용한 실패이므로 Step 04에서 "인벤토리 UI는 사망 시 닫는다"를 한 줄 적어두는 편이 낫다.

### 8-3. `ReplicationID`가 `INDEX_NONE`인 원소를 `MarkItemDirty`가 어떻게 처리하는가 — **새 ID 발급, 수신 측은 remove+add**

§5-2에 소스 사슬을 붙였다. `:925-928` → `:443-445`(`++IDCounter`) → `:950-957`(New!) → `:961-976`(옛 ID 삭제 목록). 수신 측에서 `PreReplicatedRemove`(`:1139`)와 `PostReplicatedAdd`(`:1163`)가 각각 불린다.

### 8-4. 항목 콜백을 정의하지 않아도 되는가 — **된다. 오히려 선언만 하면 링크가 깨진다**

§4-1에 정리했다. 기반의 인라인 no-op(`:341,349,356`)이 받는다. 컨셉 검사가 걸린 것은 **직렬화기 쪽 `PostReplicatedReceive` 하나뿐**이다(`:533-537`, `:701-710`).

---

## 9. 작업 목록 (우선순위)

| # | 무엇 | 어디 | 왜 지금 |
|---|---|---|---|
| **1** | **§2 판정 반영** — `Server_DropItem` 유지 + 규칙 문장 교체 | `05_Loot_03_Inventory.md` 03-5 서두 / `05_Loot_02_Interaction.md` 함정 #12 / `LOOT_STATUS.md` 확정표 | 이게 안 정해지면 03-C를 못 쓴다. 그리고 7차의 문장이 **세 문서에** 퍼지기 전에 고쳐야 한다 |
| **2** | **§4-1** 콜백 셋 삭제 | 03-1 | **링크 에러다.** 03-A 첫날에 걸린다 |
| **3** | **§4-2** `RemoveEntryInternal` 분리 | 03-2 | 03-C 착수 전 |
| **4** | **§3-3** `DropCooldownEndTime` 복제 + 프롬프트 갱신 | 03-5 / 02-3(`UpdateFocus`) | 완료 조건 11의 **앞 절반이 지금 설계로 불가능**하다 |
| **5** | **§4-3** `InsertEntry` + `FindFungibleEntryId(Container, ItemId)` | 03-2 · 03-3 · 03-4 | 후자는 문서가 아직 모르는 버그다 |
| **6** | **§7-2** 분할선 수정 (03-7 → 03-A, 완료조건 1 → 03-C) | 체크포인트 표 | 03-A 착수 직전 |
| **7** | **§5-1** `EquippedEntryId = INDEX_NONE` + Step 05 완료 조건 | 03-2 / `05_Loot_05_Equipment.md` | Step 05 문서를 지금 같이 고쳐야 잊지 않는다 |
| **8** | **§3-2** 파급 3곳 파일:줄 + `FEPInventoryEntry` 헤더 위치 | 03-4 | 03-C |
| **9** | **§5-3** `[server-only]` 표기 + `[PlayerIndex]` 인자 | 03-9 | 03-A 검증 직전 |
| **10** | **§6-1 / §6-3 / §6-2** 한 줄씩 | 03-2 / 03-4 / 함정표 | 싸다. 안 적으면 §7-1에서 셋 다 재발명한다 |
| **11** | **§5-2** 복사 시맨틱 한 줄 (이유를 정정해서) | 03-1 | 함정표까지 올릴 필요는 없다 |

**하지 않는 것:** `UEPGA_DropItem` / `UEPGA_InventoryAction` 베이스 / `FEPTargetData_EntryId` / `EventMagnitude` 인코딩 / `Server_MoveItem` 일반형 / `AddSubtree` 서브트리 전체 칸 검사 / `MaxStack` 삭제.

---

## 10. 제안 문안

> 아래는 **제안 문안**이다. 적용 여부는 사용자가 결정한다.

### 10-1. `05_Loot_03_Inventory.md` 03-5 서두 — RPC를 남기는 근거

```markdown
### ★ 왜 어빌리티가 아니라 서버 RPC인가 (8차 확정)

7차가 Step 02를 `UEPGA_Interact`로 확정하면서 세운 문장은 이랬다.

> ~~게임플레이 입력의 진입점은 어빌리티 하나다~~

**이 문장을 다음으로 대체한다.**

> **클라이언트가 서버에 요청하는 경로는 둘뿐이다.**
> ① **월드 상호작용**, 그리고 **시간·비용·애님이 붙는 행동** → **어빌리티**
> ② **서버가 이미 소유한 상태에 대한 변경 요청** → **`UEPInventoryComponent`의 서버 RPC**

**드랍은 ②다.** 클라는 월드를 조회하지 않는다. `EntryId`는 서버가 발급해 `COND_OwnerOnly`로
그 클라에게만 보낸 번호이고, 클라는 그중 하나를 지목할 뿐이다. 서버 검증은
`ContainsEntry(EntryId)` 하나로 **완전하다** — 거리도 가시성도 클라/서버 불일치도 없다.

**이 선은 Lyra의 것과 같다.** Lyra 게임 모듈 전체에서 손으로 쓴 서버 RPC는 정확히 하나이고,
그것이 인벤토리/장비의 활성 슬롯 변경이다.

    LyraQuickBarComponent.h:30   UFUNCTION(Server, Reliable, BlueprintCallable, Category="Lyra")
                                 void SetActiveSlotIndex(int32 NewIndex);

줍기·상호작용은 어빌리티로 가면서(`IPickupable.h`의 `AddPickupToInventory`가
`BlueprintAuthorityOnly` + `meta=(WorldContext="Ability")`), 슬롯 변경만 RPC로 남겼다.
`EventMagnitude`에 싣지도, `FGameplayAbilityTargetData` 서브클래스를 만들지도 않았다.

**GAS로 갔을 때의 대가.** `FGameplayEventData`에 `int32`가 없다(`GameplayAbilityTypes.h:246-284`,
스칼라는 `float EventMagnitude` 하나). 7차에서 대상 전달이 공짜였던 것은 대상이 **액터**여서
`FGameplayEventData::Target`이 있었기 때문이고, `EntryId`에는 그 자리가 없다.
`EventMagnitude`에 정수를 싣는 선례는 **엔진과 Lyra를 통틀어 0건**이다
(엔진의 유일한 대입 `_Abilities.cpp:2643`은 태그 **개수**이고 RPC를 타지 않는다).

**그리고 이 어빌리티가 GAS에서 실제로 쓸 기능은 `ActivationBlockedTags` 하나다** —
예측 없음(아래 표), 코스트 없음, 애님 없음, 쿨다운은 플레이어가 아니라 **픽업** 소유.
그 하나는 RPC에서 한 줄이다.

    if (!CanMutateInventory()) return;   // State.Dead / 향후 State.Casting을 여기 한 곳에서 본다

**되돌아갈 신호 (이 중 하나가 참이 되면 그 항목만 어빌리티로 올린다):**
  ⓐ 버리기·장착에 **시전 시간이나 몽타주**가 붙는다 (`UEPGA_Skill_Base::CastTime` 구조 재사용)
  ⓑ 클라 **예측**이 필요해진다
  ⓒ **RPC가 `UEPInventoryComponent` 바깥 클래스에 생기려 한다** ← 진짜 경계선

> **주의 — GAS에 비슷한 이름이 있다는 것과 그 기능이 이 문제를 푼다는 것은 다르다.**
> `DropCooldown`은 GE 쿨다운이 아니다(주체가 픽업이지 플레이어가 아니다 — GE로 하면
> 버린 사람만 못 줍고 옆 사람은 즉시 줍는다). 칸 계산은 어트리뷰트가 아니다(컨테이너별
> 파생값이고 `SlotSize`는 DT에서 나온다). `OnInventoryChanged`는 태그가 아니다(태그는
> 상태이고 알림은 에지다).
```

### 10-2. 03-4 `OnInteract` 교체

```cpp
bool AEPPickup::OnInteract(AEPCharacter* Interactor, FText& OutReason)
{
    bClaimed = true;                                   // ★ 무엇보다 먼저 (함정 #3)

    UEPInventoryComponent* Inv = Interactor ? Interactor->GetInventoryComponent() : nullptr;
    if (!Inv)
    {
        bClaimed = false;                              // ★ 실패 반환 앞마다 되돌린다
        OutReason = NSLOCTEXT("EP", "PickupNoInv", "인벤토리가 없습니다.");
        return false;
    }

    // 본체 → 실패하면 매고 있는 배낭. 순서를 뒤집으면 배낭부터 차서 본체가 빈다
    int32 NewId = Inv->AddSubtree(INDEX_NONE, Payload);
    if (NewId == INDEX_NONE && Inv->GetEquippedBackpack() != INDEX_NONE)
        NewId = Inv->AddSubtree(Inv->GetEquippedBackpack(), Payload);

    if (NewId == INDEX_NONE)                           // ★ if(NewId)로 쓰면 안 된다 (함정 4b)
    {
        bClaimed = false;
        OutReason = NSLOCTEXT("EP", "PickupNoRoom", "가방에 자리가 없습니다.");
        return false;                                  // → UEPGA_Interact가 Client_OnInteractFailed로 회신
    }

    Inv->TryAutoEquipBackpack(NewId);
    Destroy();                                         // 성공 — 값은 이미 복사됐다
    return true;
}
```

> **`bClaimed` 되돌림은 이 함수 안이다.** `bClaimed`는 `EPPickup.h:50`의 private이고 세터가
> 없다(`IsClaimed()` 게터만, `:24`). 어빌리티가 되돌리게 하려면 세터를 뚫어야 하는데,
> 그 순간 *"밖에서 `bClaimed`를 켤 수 있다"* 가 성립해 선점(함정 #3)이 약해진다.
> **`return false` 하는 모든 줄 앞에 `bClaimed = false;`가 있는지가 이 함수의 검수 항목이다.**
```

### 10-3. 02-3 `UpdateFocus` — 같은 대상을 보는 동안의 재평가

```cpp
// 03-5의 DropCooldown이 이 갈래의 첫 소비자다.
// 포커스가 안 바뀌어도 CanInteract 결과가 바뀌면 프롬프트를 갱신해야 한다.
IEPInteractable* Interactable = Cast<IEPInteractable>(NewFocus);
FText   Reason;
const bool bCan = (Interactable && Interactable->CanInteract(Owner, Reason));

if (NewFocus == FocusedActor && bCan == bLastCanInteract) return;   // ★ 결과 기준
FocusedActor      = NewFocus;
bLastCanInteract  = bCan;
```

> `TickInterval = 0.1f`이므로 초당 10회 `CanInteract` 호출이고, 그 함수는 비교 두 번이다.
> **비용이 문제가 아니라 "포커스 불변 = 상태 불변"이라는 전제가 틀렸다는 게 문제다.**
> `bLastCanInteract`는 `UEPInteractionComponent`의 private `bool` 하나.

### 10-4. 03-1 콜백 셋 삭제 + 대체 주석

```markdown
> **★ 항목 단위 콜백(`PostReplicatedAdd`/`Change`/`PreReplicatedRemove`)을 선언하지 않는다.**
> 기반 `FFastArraySerializerItem`이 인라인 no-op을 갖고 있고(`FastArraySerializer.h:341,349,356`),
> 호출은 무조건 나간다(`:1139`, `:1163`, `:1174`).
> **파생 struct가 같은 이름을 선언만 하면 기반 정의를 가려서 링크 에러가 난다** —
> "선언해두고 나중에 채운다"가 성립하지 않는 자리다.
> 알림은 03-7의 `FEPInventoryList::PostReplicatedReceive` 하나로 간다. 그쪽만 진짜
> 컨셉 검사라(`:533-537` → `:701-710`) **정의하면 불리고 안 하면 안 불린다.**
>
> Lyra도 아이템에는 안 둔다 — `FLyraInventoryEntry`는 `GetDebugString()` 하나뿐이고,
> 셋은 직렬화기 쪽에 배열 단위 시그니처로 있다. Lyra가 그걸 정의하는 이유(항목마다
> 개수 델타 메시지를 쏜다, `LastObservedCount` 필드까지 둔다)는 우리에게 없다.

> **★ `FFastArraySerializerItem`의 복사는 복제 ID를 승계하지 않는다.**
> 복사 생성자와 `operator=`가 `ReplicationID`/`ReplicationKey`/`MostRecentArrayReplicationKey`를
> 전부 `INDEX_NONE`으로 리셋한다(`FastArraySerializer.h:302-323`).
> **좋은 쪽:** 스냅샷·`Payload`·`FindEntry` 값 복사가 전부 안전하다.
> **나쁜 쪽:** **살아있는 배열 원소에 `E = Other;`로 통째 대입하면 안 된다.**
> 송신 시 `:925-928`이 새 ID를 발급하고, 그 결과 수신 측은 **삭제 + 추가**로 본다
> (`:950-957` New! / `:961-976` 옛 ID 삭제 / `:1139`·`:1163` 콜백).
> UI는 `EntryId`로 식별하므로 지금은 깨지지 않지만, **그 엔트리에서 내부 struct 델타가
> 사라져 전체가 다시 나가고**, 나중에 항목 단위 콜백을 붙이면 그때 거짓말한다.
> → 수정은 언제나 **필드 단위**로. `SetEntryCharges`의 `E.State.Charges = ...`가 그 형태다.
```

### 10-5. 03-2 — 컨테이너 간 이동이 이미 성립한다는 한 줄

```markdown
> **★ §7-1 월드 컨테이너가 이 두 함수로 이미 성립한다.**
>
>     Container->RemoveEntry(Id, &Sub)   →   MyInv->AddSubtree(INDEX_NONE, Sub)
>
> `RemoveEntry`의 반환 계약(전위 순회 / `In[0]`이 루트 / 루트 `Parent=INDEX_NONE`)은
> **`EntryId` 공간이 아니라 배열 모양에만 의존하므로 컴포넌트 경계를 넘어서도 유효하다.**
> `AddSubtree`의 `OldToNew` 재매핑이 원래 "번호 공간이 다르다"를 전제로 만들어졌고,
> 버린 배낭을 되줍는 것이 이미 그 경우다.
>
> **`TransferItem` / `Server_MoveItem` 일반형을 지금 만들지 않는다** — 소비자가 없다
> (CLAUDE.md §2). **사실만 적고 함수는 만들지 않는다.** 안 적으면 §7-1에서 세 번째 경로가
> 생기고, 그건 `RemoveEntry` 4단계 순서를 우회할 자리를 새로 만든다.
```

### 10-6. 03-4 — 루트만 검사하는 것이 왜 정확한가

```markdown
> **★ 칸 검사가 루트만인 것은 축소가 아니라 정확한 것이다. 늘리지 마라.**
>
> 재매핑 규칙상 `Src.ParentEntryId == INDEX_NONE`인 원소(루트)만 `Container`를 부모로 받고,
> 나머지는 `OldToNew`가 준 부모 — 즉 **서브트리 내부** — 를 받는다.
> **자식이 `Container`의 칸을 먹는 경우가 문법적으로 없다.**
>
> - 상자 **안의 아이템 하나**를 옮긴다 → 그게 루트다. 루트 검사 = 그 아이템 검사
> - **상자 자체**를 옮긴다 → 배낭이 드는 것은 상자 1개분이다. 상자의 용량은 상자가 어디
>   있든 상자의 성질이다
>
> 서브트리 전체 합을 검사하도록 "고치면" **배낭을 되주울 때 자기 내용물이 본체 칸을 먹는
> 것으로 계산돼 완료 조건 9(되줍기)가 깨진다.**
```

---

## 11. 인용 (전부 로컬 직독)

| 주장 | 출처 |
|---|---|
| `FGameplayEventData`에 `int32` 없음, 스칼라는 `float EventMagnitude` | `GameplayAbilityTypes.h:246-284`, `:280` |
| 이벤트 데이터가 통째로 서버 RPC 파라미터 | `AbilitySystemComponent_Abilities.cpp:1921-1923` |
| 트리거 경로의 정책 게이트 | 같은 파일 `:2482`, `:2664-2680` |
| 엔진의 유일한 `EventMagnitude` 대입 = 태그 개수, RPC 미경유 | 같은 파일 `:2643` → `:2645` |
| `EventMagnitude` 디버그 출력이 `%.3f` | `AbilitySystemComponent.cpp:2022-2024` |
| `FGameplayAbilityTargetData` 서브클래스 최소 형태 / `WithNetSerializer` 필수 주석 | `GameplayAbilityTargetTypes.h:560-652`, `:650` |
| **Lyra 게임 모듈 전체의 `UFUNCTION(Server` = 1건** | `grep -rn "UFUNCTION(Server" LyraGame/` |
| 그 1건 = 활성 슬롯 변경(`int32`), `UControllerComponent` | `LyraQuickBarComponent.h:18,30-31` |
| 구현·검증 / 게임플레이 입력이 직접 부름 | `LyraQuickBarComponent.cpp:135-147`, `:46-84` |
| Lyra 줍기는 어빌리티에서 부르는 서버 함수 | `Inventory/IPickupable.h` (`AddPickupToInventory`) |
| Lyra 장착/해제는 RPC가 아니라 서버 전용 함수 | `LyraEquipmentManagerComponent.h:122,125` |
| Lyra의 `EventMagnitude` 유일 사용 = 데미지 | `LyraHealthComponent.cpp:163` |
| Lyra에 드랍 경로 없음 | `grep -rn "Drop" LyraGame/` (무관 2건) |
| `FLyraInventoryEntry`는 항목 콜백을 정의하지 않음 / 셋은 직렬화기에 | `LyraInventoryManagerComponent.h` |
| 항목 콜백의 기반 인라인 no-op | `FastArraySerializer.h:341,349,356` |
| 항목 콜백 호출 지점 (무조건) | 같은 파일 `:1139`, `:1163`, `:1174` |
| 직렬화기 레벨 콜백의 기반 no-op | 같은 파일 `:501,508,515` |
| `PostReplicatedReceive`만 컨셉 검사 | 같은 파일 `:517-537`, `:701-710` |
| 복사 생성자/`operator=`가 ID 셋을 리셋 | 같은 파일 `:302-323` |
| `MarkItemDirty`의 새 ID 발급 | 같은 파일 `:441-454`, `:445` |
| `INDEX_NONE`이면 송신 중 자동 `MarkItemDirty` | 같은 파일 `:925-928` |
| 새 ID → "New!" + `++DeleteCount` | 같은 파일 `:950-957` |
| 옛 ID → 삭제 목록 | 같은 파일 `:961-976` |
| `NetCore`가 `Engine`의 **Public** 의존 (Build.cs 수정 불필요) | `Engine.Build.cs:85` |
| Lyra는 `NetCore`를 Private에 명시 (관례) | `LyraGame.Build.cs:57` |
| `IEPInteractable` 실제 시그니처 | `EPInteractable.h:23,27` |
| `bClaimed` private · 세터 없음 · 게터만 | `EPPickup.h:24,50` |
| `OnInteract`이 현재 무조건 `true` | `EPPickup.cpp:66-73` |
| `CanInteract`이 `bClaimed`만 봄 | `EPPickup.cpp:56-64` |
| `InitPickup` 유일 호출부 | `EPItemSpawner.cpp:91` |
| `GetState()` 읽는 두 곳 | `EPLootDebugCommands.cpp:133,137` |
| **`[server-only]` 표기 선례** | `EPLootDebugCommands.cpp:120-124` |
| 프롬프트가 포커스 변화에만 갱신 / 클라에서 `CanInteract` 호출 | `EPInteractionComponent.cpp:78`, `:94` |
| 사거리 값과 게터 | `EPInteractionComponent.h:23-24,30,33` |
| `Client_OnInteractFailed` | `EPPlayerController.h:47` |
| `MaxStack` 선언 1줄 / 읽는 코드 0곳 | `EPItemData.h:36` + 전체 grep |
