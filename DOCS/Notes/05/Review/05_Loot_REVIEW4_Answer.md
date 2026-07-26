# 검증 답변 4차 — 3차 반영분 확인 + 판단 5건

> 작성일: 2026-07-26
> 대상: `05_Loot_REVIEW4_Request.md`
> 3차: `05_Loot_REVIEW3_Request.md` / `_Answer.md`
> 시점: Step 00 착수 직전. 구현 코드 0줄

> 이 문서는 **검증 기록**이다. 확정 결정은 `LOOT_STATUS.md`, 설계는 `05_Loot_DOCS.md`, 구현 스펙은 Step 문서에 반영한다.

---

## 0. 총평

3차 지적은 **전부 제대로 닫혔다.** §1(스냅샷 순서) / §2(캐스케이드 장착 검사) / §8-3(서브트리 되줍기) 세 결함의 처방이 의도대로 들어갔고, 미선언 9개·`Entries.Owner`·`PostReplicatedReceive`·DT/DA 원칙·문서 재구조화까지 반영됐다. `Review/` 분리와 마스터 1007→839도 확인했다.

**다만 요청서 §0의 첫 질문 — "고치면서 새 구멍을 만들지 않았는가" — 의 답은 "만들었다"다.** 하나는 설계 수준이고 둘은 컴파일 에러다.

| 축 | 판정 |
|---|---|
| 3차 결함 3건 닫혔는가 | ✅ 방향은 전부 맞다. **단 §1의 처방이 §8-3의 처방과 어긋난다** (아래 §1) |
| 새 구멍 | ❌ **3건** — 설계 1(§1), 컴파일 2(§2) |
| 판단 5건 | 3-1 ✅ 당신 말이 맞다(3차가 과했다) / 3-2 ✅ 규칙을 정정 / 3-3 ⚠️ 분석은 맞으나 둘을 놓쳤다 / 3-4 ❌ 쪼개지 마라 / 3-5 ⚠️ 커맨드 1개 + 출력 1줄이면 된다 |

### 판정표

| 항목 | 판정 |
|---|---|
| §1 `RemoveEntry(Id,&Out)` 반환 | ✅ 처방 정확. **단 반환 배열의 순서·루트 표현이 `AddSubtree`와 안 맞는다** |
| §2 캐스케이드 재귀 | ✅ 정확. **단 자기 부모 사이클에 무한 재귀** |
| §8-3 `AddSubtree` | ✅ 필요성·재매핑 정확. 위 두 건에 걸려 현재 스펙으로는 동작 안 함 |
| 미선언 9개 | ✅ 전부 선언됨 |
| `Entries.Owner = this` | ✅ 생성자 + ★★ 경고 |
| `PostReplicatedReceive` 통일 | ✅ Step 03·04 양쪽 반영. 함정 #3 소멸 확인 |
| DT/DA 원칙 | ✅ 두 필드 이동 + `InitState(const FEPItemData&, ...)` |
| `check()` → early return | ✅ |
| `Cash_10000 SellPrice=0` + 규칙 | ✅ |
| stale ★1·★2 | ✅ 제거됨. `GAME.md` #6→#5도 수정 확인 |
| 문서 재구조화 | ✅ 마스터에서 코드 제거 확인 |
| **신규 결함** | ❌ §1(순서/루트) · §2(`NewId`·`AS` 미선언) |
| **잔여 미세 결함** | ⚠️ `bNotifyPending` 죽은 필드 / `Dump` 서버 전용 / 마크다운 깨짐 / 미정 #7 저장 목록 |

---

## 1. ★★ 새 구멍 — `RemoveEntry`가 만든 배열을 `AddSubtree`가 읽을 수 없다

**§1과 §8-3을 각각 옳게 고쳤는데, 둘이 만나는 지점을 안 봤다.**

### 1-1. 순서가 뒤집혀 있다

`05_Loot_03_Inventory.md` 03-2:

```cpp
RemoveChildrenRecursive(EntryId, OutRemoved);   // ← 자식이 먼저 들어간다
if (OutRemoved)
{
    FEPInventoryEntry Snapshot;
    if (FindEntry(EntryId, Snapshot))
        OutRemoved->Add(Snapshot);              // ← 루트가 마지막
}
```

결과 배열은 **깊이 우선 후위(자식 → … → 루트)** 다. 그런데 03-4의 `AddSubtree`는:

> `TMap<int32,int32> OldToNew`를 만들며 **순서대로 삽입**

첫 원소(자식)를 삽입할 때 그 `ParentEntryId`가 가리키는 루트는 **아직 `OldToNew`에 없다.** 재매핑이 첫 줄에서 실패한다.

### 1-2. 루트의 `ParentEntryId`가 `INDEX_NONE`이 아니다

`AddSubtree`는 루트를 이렇게 찾는다.

> 루트(`ParentEntryId == INDEX_NONE`인 것) 기준으로만 칸 검사

배낭이 **본체**에 있었으면 우연히 맞다. 그런데 **배낭 안에 있던 무기를 버리면** 그 엔트리의 `ParentEntryId`는 배낭 번호(예: 2)다. `INDEX_NONE`인 원소가 하나도 없어 **루트를 못 찾는다.**

본체 10칸 / 무기 5칸이라 무기가 배낭에 들어가는 건 흔한 경로다(같은 문서가 03-2에서 그 근거로 3b 함정을 정당화했다). **이 경로가 그대로 깨진다.**

### 1-3. 자기 부모 사이클에 무한 재귀

```cpp
RemoveEntry(X)                       // X.ParentEntryId == X (데이터 오류)
  → RemoveChildrenRecursive(X)       // Children에 X가 들어온다
      → RemoveEntry(X)               // X는 아직 안 지워졌다 (제거가 함수 끝에 있다)
          → RemoveChildrenRecursive(X) → …   스택 오버플로
```

자기 제거가 **캐스케이드보다 뒤**에 있어서 생긴다. §7-3이 "무제한 중첩으로 넓히면 순환 참조 방어가 필요하다"고 적어뒀는데, **재귀 제거를 도입한 지금 이미 필요해졌다.**

### ★ 세 개를 한 번의 재배치로 닫는다

```cpp
bool UEPInventoryComponent::RemoveEntry(int32 EntryId,
                                        TArray<FEPInventoryEntry>* OutRemoved,
                                        bool bIsRoot /* = true */)
{
    if (!GetOwner()->HasAuthority()) return false;
    if (!Entries.Items.ContainsByPredicate(...)) return false;   // 존재 확인만

    FScopedInventoryNotify Guard(this);

    // ① write-back 먼저 — 스냅샷은 반드시 이 뒤에 뜬다 (3차 §1)
    if (EntryId == EquippedEntryId)
        if (UEPCombatComponent* C = GetOwner<AEPCharacter>()->GetCombatComponent())
            C->UnequipWeapon();
    if (EntryId == EquippedBackpackEntryId)
        EquippedBackpackEntryId = INDEX_NONE;

    // ② 스냅샷을 자식보다 **먼저** 담는다. 루트는 컨테이너 소속을 끊는다
    if (OutRemoved)
    {
        FEPInventoryEntry Snapshot;
        if (FindEntry(EntryId, Snapshot))
        {
            if (bIsRoot) Snapshot.ParentEntryId = INDEX_NONE;   // ★ 1-2 해결
            OutRemoved->Add(Snapshot);                          // ★ 1-1 해결
        }
    }

    // ③ 자신을 먼저 제거한다 — 그래야 캐스케이드가 자기를 다시 못 찾는다
    RemoveSelf(EntryId);                                        // ★ 1-3 해결

    // ④ 자식은 그 다음. 부모가 이미 배열에서 빠졌으므로 사이클이 성립하지 않는다
    RemoveChildrenRecursive(EntryId, OutRemoved);
    return true;
}
```

```cpp
void UEPInventoryComponent::RemoveChildrenRecursive(int32 ParentId,
                                                    TArray<FEPInventoryEntry>* OutRemoved)
{
    TArray<int32> Children;
    for (const FEPInventoryEntry& E : Entries.Items)
        if (E.ParentEntryId == ParentId) Children.Add(E.EntryId);

    for (int32 Id : Children)
        RemoveEntry(Id, OutRemoved, /*bIsRoot=*/false);   // ★ 자식은 Parent를 보존한다
}
```

**결과 배열이 전위 순회(부모 먼저)가 되고, `In[0]`이 항상 루트이며, `Parent`는 `INDEX_NONE`이다.** `AddSubtree`가 이렇게 단순해진다.

```cpp
int32 UEPInventoryComponent::AddSubtree(int32 Container, const TArray<FEPInventoryEntry>& In)
{
    if (In.Num() == 0) return INDEX_NONE;

    // In[0]이 루트다 — RemoveEntry가 그렇게 만든다. 탐색이 필요 없다
    if (In.Num() == 1) return AddItem(Container, In[0].ItemId, In[0].State);   // §4

    // 칸 검사는 루트만 (자식은 칸을 안 먹는다)
    if (!CanFit(Container, In[0].ItemId)) return INDEX_NONE;

    FScopedInventoryNotify Guard(this);
    TMap<int32, int32> OldToNew;
    for (const FEPInventoryEntry& Src : In)
    {
        const int32 NewParent = (Src.ParentEntryId == INDEX_NONE)
            ? Container
            : OldToNew.FindRef(Src.ParentEntryId);      // ★ 부모가 먼저 들어와 있다
        ...
        OldToNew.Add(Src.EntryId, NewId);
    }
    return OldToNew[In[0].EntryId];
}
```

> **`OldToNew.FindRef`가 0을 돌려주면 데이터 오류다** — 부모가 배열에 없다는 뜻이고, 지금 구조에서는 발생할 수 없다. `ensureMsgf` + 그 엔트리를 건너뛴다. 조용히 `Parent=0`으로 넣으면 **`EntryId=0`이 없으므로 고아**가 되어 §8-3이 막으려던 상태로 돌아간다.

> **"루트는 `In[0]`이다"를 03-4에 명시하라.** 지금은 "`ParentEntryId == INDEX_NONE`인 것"이라 적혀 있어, 배열 순서라는 계약이 문서에 없다. `RemoveEntry`가 그 순서를 **보장하는 쪽**이라는 것도 같이 적어야 두 함수가 계약으로 묶인다.

---

## 2. ★ 컴파일 에러 2건 — 요청서 §2와 같은 유형이 남았다

### 2-1. `NewId`가 선언되지 않았다 (`05_Loot_03_Inventory.md` 03-3)

```cpp
    Entries.MarkItemDirty(E);
    return NewId;                            // ★ E가 아니라 미리 뜬 값
```

**주석은 고쳤는데 코드를 안 고쳤다.** `NewId`가 어디에도 없다. 3차 §8-5의 처방("`const int32 NewId = E.EntryId;`를 알림 앞에서 뜬다")이 주석으로만 반영됐다.

```cpp
FEPInventoryEntry& E = Entries.Items.AddDefaulted_GetRef();
const int32 NewId = NextEntryId++;       // ★ 여기서 뜬다
E.EntryId       = NewId;
E.ParentEntryId = Container;
E.ItemId        = ItemId;
E.State         = InState;

Entries.MarkItemDirty(E);
return NewId;                            // 가드 소멸 → Broadcast는 이 뒤다
```

> 스코프 가드를 넣은 지금은 브로드캐스트가 `return` **뒤**에 나가므로 댕글링 위험 자체가 줄었지만, 값을 미리 뜨는 형태는 유지하는 게 맞다 — 가드를 나중에 누가 빼도 안전하다.

### 2-2. `AS`가 선언되지 않았다 (`05_Loot_05_Equipment.md` 05-3)

```cpp
void UEPCombatComponent::UnequipWeapon()
{
    ...
    Inv->AddEntryCharges(EquippedId, FMath::RoundToInt(AS->GetAmmo()) - E.State.Charges);
}
```

`AS`가 없다. 기존 코드에서 `UEPAttributeSet`은 `PlayerState`를 거쳐 얻는다(`EPCombatComponent.cpp:171-174`).

```cpp
AEPPlayerState* PS = Char ? Char->GetPlayerState<AEPPlayerState>() : nullptr;
UEPAttributeSet* AS = PS ? PS->GetAttributeSet() : nullptr;
if (Inv && AS) { ... }
```

05-2의 `AS->InitAmmo(...)` / `AS->SetMaxAmmo(...)`는 기존 코드 **발췌**라 `AS`가 스코프에 있지만, 05-3은 **완결된 함수 본문**이다. 이 구분이 문서에 드러나야 한다 — 발췌에는 `// EPCombatComponent.cpp:177-178 (현재)` 같은 표시가 이미 있으니, 완결 본문 쪽만 채우면 된다.

**이 두 건이 요청서 §2가 자체 발견한 4개와 정확히 같은 유형이다.** 자체 grep이 함수·클래스 이름은 잡았지만 **지역 변수**는 못 잡았다 — 판단은 §8에서.

---

## 3. 3-1 답 — 당신이 맞다. **3차 §4가 과했다**

`SetEntryCharges`를 되살려라.

3차 §4의 논거는 *"부호만 다른 두 함수를 두면 어느 쪽이 클램프를 하는지 갈린다"* 였고, 그건 **`Add` vs `Consume`**(의미가 같고 부호만 반대)에 대한 말이었다. `Set`과 `Add`는 그 관계가 아니다 — **의미가 다르고, 한쪽이 다른 쪽으로 정의된다.** 지적대로다.

그리고 지금 문서 상태가 3차 §4가 없애려던 것을 정확히 재현한다.

```cpp
FEPInventoryEntry E;
if (Inv->FindEntry(EquippedId, E))
    Inv->AddEntryCharges(EquippedId, FMath::RoundToInt(AS->GetAmmo()) - E.State.Charges);
//                                   └──────── 읽고 · 빼고 · 넘긴다. 전부 호출부에서 ────┘
```

**write-back은 본질적으로 대입이다.** 델타로 표현하려면 현재값을 먼저 읽어야 하고, 그 순간 3차 §4가 지운 패턴이 그대로 돌아온다. 게다가 이건 `UEPCombatComponent`, 즉 **다른 컴포넌트**에서 벌어진다 — 3차 §4가 특히 막으려던 자리다.

### 살아남는 규칙은 "부호"가 아니라 **"쓰기 지점이 하나"** 다

```cpp
// UEPInventoryComponent — public
void SetEntryCharges(int32 EntryId, int32 NewCharges);   // ★ 유일한 쓰기 지점
void AddEntryCharges(int32 EntryId, int32 Delta);        //   Set에 위임
```

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

- **클램프·`MarkItemDirty`·알림이 여전히 한 함수에만 있다.** 3차 §4의 목적은 그것이었고 유지된다
- `AddEntryCharges`가 읽고-쓰지만 **컴포넌트 내부, 같은 호출 스택, 중간에 알림 없음**이다. 3차가 문제 삼은 건 그게 **호출부**에서 벌어지는 것이었다
- `ConsumeCharges`는 여전히 만들지 않는다. `AddEntryCharges(Id, -N)`이다 — 여기까지는 3차가 맞다

**함수는 3개가 아니라 2개다.** `UnequipWeapon`은 이렇게 줄어든다.

```cpp
Inv->SetEntryCharges(EquippedId, FMath::RoundToInt(AS->GetAmmo()));   // 끝
```

> **`SetEntryCharges`가 public인 것이 규칙 위반이 아니다.** 03-2가 금지한 것은 **원시 엔트리(포인터·비-const 참조)를 내보내는 것**이지 수정 API 자체가 아니다. 오히려 그 금지의 목적이 "수정은 API로만"이므로 API가 있어야 성립한다.

> `AddItem`의 `bFungible` 분기도 `AddEntryCharges(Id, InState.Charges)` 그대로 두면 된다 — 거기는 진짜 델타다.

---

## 4. 3-2 답 — 규칙은 `In.Num()`도 `bFungible`도 아니다. **"자식이 있는가"** 다

지적대로 두 문장이 어긋난다. 정확한 규칙은 이것이다.

> **합치기는 "이 개체를 없애고 기존 개체의 숫자를 올린다"이다. 없애면 안 되는 것(자식)이 매달려 있으면 성립하지 않는다.**

그래서 판정은 **"루트에 자식이 있는가"** 이고, `In.Num() == 1`이 그 판정의 정확한 형태다(자식이 있으면 원소가 2 이상이다).

`bFungible`은 판정이 아니라 **그 아래 계층**이다 — 원소 1개일 때 합칠지 말지는 `AddItem`이 이미 `bFungible`로 결정한다. 두 조건이 겹치는 게 아니라 **직렬**이다.

```cpp
int32 AddSubtree(int32 Container, const TArray<FEPInventoryEntry>& In)
{
    if (In.Num() == 0) return INDEX_NONE;

    // 자식이 없으면 단일 아이템이다 → AddItem이 bFungible을 본다
    if (In.Num() == 1) return AddItem(Container, In[0].ItemId, In[0].State);

    // 자식이 있다 → 합치기가 성립하지 않는다
    ensureMsgf(!IsFungible(In[0].ItemId),
               TEXT("[Inventory] 자식을 가진 fungible 아이템: %s"), *In[0].ItemId.ToString());
    ...
}
```

- **경로가 하나가 된다.** 스포너·되줍기·기본 지급이 전부 `AddSubtree`를 부르고, 그 안에서 원소 1개면 `AddItem`으로 내려간다
- **탄약상자는 합쳐진다.** 원소 1개이므로 `AddItem` → `bFungible` → 합산. 당신 판단이 맞다
- **`ensure`가 데이터 오류를 드러낸다.** 자식 있는 fungible은 논리적으로 불가능하다 — `bFungible`은 "구별할 근거가 없다"는 뜻인데 자식이 있으면 그 자체가 구별 근거다. 발생하면 DT 설정 실수이고, 조용히 넘기면 자식이 증발한다

### 문서 서술 정정

```diff
- bFungible 합치기는 이 경로에 적용하지 않는다. (…) 원소가 1개면 AddItem으로 위임한다.
+ 자식이 있으면(원소 2개 이상) 합치지 않는다 — 합치기는 개체를 없애는 것이라 매달린 자식이 갈 곳이 없다.
+ 원소 1개면 AddItem으로 위임하고, 합칠지 말지는 거기서 bFungible이 정한다.
+ 자식을 가진 fungible 아이템은 데이터 오류이므로 ensure로 드러낸다.
```

---

## 5. 3-3 답 — 분석은 맞다. **둘을 놓쳤다**

### 맞는 부분

브로드캐스트 **1회** 맞다. 가드 소멸자가 `NotifyDepth == 0`일 때만 쏘고, 재귀가 아무리 깊어도 최외곽 이탈 한 번뿐이다.

지역 상태 분석도 맞다. 재귀 중 `Entries.Items`가 재할당돼도:

| 지역 | 안전한 이유 |
|---|---|
| `Entry` / `Snapshot` | **값 복사** (03-2가 `FindEntry`를 값 반환으로 만든 이유가 여기서 배당을 낸다) |
| `Children` | `TArray<int32>` — 배열과 무관 |
| `OutRemoved` | 호출자 스택의 `TArray`. 재할당돼도 포인터가 아니라 배열 자체를 다룬다 |
| 가드 | `UEPInventoryComponent*` 하나 |

**참조를 배열 안으로 들이대는 지역이 하나도 없다.** 3차 §8-5가 지적한 `AddDefaulted_GetRef`의 `E&`가 유일한 예외인데 그건 재귀 경로에 없다.

### 놓친 것 ① — 무한 재귀

자기 부모 사이클에서 스택 오버플로가 난다. §1-3에 상세를 적었고, 자기 제거를 캐스케이드 앞으로 옮기면 사라진다.

### 놓친 것 ② — `bNotifyPending`이 죽은 필드다

03-2가 `NotifyDepth`와 `bNotifyPending` 둘을 선언했는데, **누가 `bNotifyPending`을 세우는지 어느 문서에도 없다.**

세울 수 있는 후보는 둘뿐이다.

| 누가 세우나 | 결과 |
|---|---|
| 가드 **생성자** | 가드는 변형 함수에만 놓이므로 `NotifyDepth > 0` ⟺ `bNotifyPending == true`. **완전히 중복** |
| `MarkItemDirty` 호출부마다 손으로 | 빠뜨릴 자리가 생긴다 — 가드가 없애려던 바로 그것 |

**필드를 지워라.** 소멸자에서 `if (--NotifyDepth == 0) Owner->OnInventoryChanged.Broadcast();`면 끝이다. 가드가 놓인 함수는 전부 성공 경로에서만 진입하므로(모든 실패 검사가 가드보다 앞에 있다) 헛 브로드캐스트도 안 난다.

> 예외는 `AddEntryCharges(Id, 0)` / `SetEntryCharges`가 같은 값을 쓰는 경우인데, UI 재생성 한 번이라 무해하다. 이걸 막으려고 필드를 유지하면 **10줄짜리 가드가 값 비교 로직을 갖게 된다.**

---

## 6. 3-4 답 — **쪼개지 마라.** 길이가 아니라 **작업량**이 문제고, 처방이 다르다

### 쪼개면 안 되는 이유

**이음매가 없다.** 제안한 `03a 코어 / 03b 배낭·서브트리` 경계에서 `RemoveEntry`가 정확히 갈라진다 — 장착 검사·`MarkItemDirty`는 코어이고, 자식 캐스케이드·`OutRemoved`는 배낭이다. **한 함수를 두 문서가 나눠 갖게 된다.**

3차 §11이 지목한 원인이 그것이었다. 파일을 쪼개면 `RemoveEntry` 코드 블록이 03a·03b 양쪽에 적히고, **9건짜리 중복표에 열 번째 줄이 추가된다.**

배낭을 Step 04 뒤로 미루는 것도 안 된다. 지적대로 완료 조건이 얽혀 있고, 더 근본적으로 **`RemoveEntry`의 캐스케이드가 배낭 없이는 설계 동기를 잃는다** — 자식이 없으면 캐스케이드가 죽은 코드다.

### 진짜 문제는 다른 데 있다

720줄이 아니라 **완료 조건 13개**가 신호다.

| Step | 완료 조건 | 줄 |
|---|---|---|
| 00 | 7 | 524 |
| 01 | 7 | 398 |
| 02 | 5 | 279 |
| **03** | **13** | **720** |
| 04 | 10 | 333 |
| 05 | 8 | 272 |
| 06 | 8 | 272 |

**Step 03은 다른 단계 두 개 분량의 작업이다.** 그건 문서 문제가 아니라 일정 문제고, 문서를 쪼개도 작업량은 그대로다.

### 처방 — 파일이 아니라 **체크포인트**를 나눈다

문서는 하나로 두고, 각 절에 체크포인트 표시만 붙인다.

```
03-A 코어    03-1 · 03-2 · 03-3 · 03-9        완료 조건 1~6
             → EP.Inv.Add로 칸 합산·bFungible·COND_OwnerOnly까지 검증하고 멈춘다

03-B 배낭    03-6 + GetCapacity(컨테이너)      완료 조건 7
             → 배낭을 주우면 두 번째 풀이 열리는지. 아직 못 버린다

03-C 버리기  03-4 · 03-5 · 03-7               완료 조건 8~13
             → RemoveEntry / AddSubtree / 캐스케이드. 여기가 §1·§2의 무대다
```

- **03-A만으로 컴파일·실행·검증이 된다.** `RemoveEntry`를 아직 안 쓰므로 `Server_DropItem`·`AddSubtree`가 없어도 성립한다
- **03-C가 위험이 몰린 구간이라는 게 드러난다.** 함정표의 ★★ 4건 중 3건(3, 3b, 3c)이 전부 여기다
- 파일이 하나이므로 `RemoveEntry`가 한 번만 적힌다

`LOOT_STATUS.md` 진행 표에 03을 세 줄로 적으면 중단·재개 지점도 명확해진다.

---

## 7. 3-5 답 — 커맨드 1개 추가 + 출력 1줄. `Stress`는 필요 없다

### ① `COND_OwnerOnly` — `Dump`를 클라에서 돌리면 된다. **단 지금은 못 돈다**

03-9가 `EP.Inv.*`를 통째로 **"서버 전용"** 으로 적었다. 그러면 이 검증이 불가능하다.

**읽기 커맨드는 클라에서도 돌아야 한다.** Step 00이 이미 같은 구분을 했다 — *"순수 조회라 클라이언트에서 실행해도 된다"*(00-9). 03-9도 같게 나눠라.

| 커맨드 | 권한 |
|---|---|
| `EP.Inv.Dump` | **클라 허용** — 로컬 폰의 인벤토리를 읽는다 |
| `EP.Inv.Add` / `EP.Inv.Drop` | 서버 전용 |

그리고 소유 여부를 보려면 **남의 컴포넌트**를 봐야 한다.

```
> EP.Inv.DumpAll        # 클라에서 실행. 월드의 모든 UEPInventoryComponent를 순회
  Pawn_0 (내 폰)   Entries=4
  Pawn_1           Entries=0      ← 0이면 통과. 0이 아니면 COND_OwnerOnly 누락
```

`COND_OwnerOnly`가 빠지면 `Entries=4`가 그대로 찍힌다. **이 한 줄이 완료 조건을 직접 증명한다** — 패킷 캡처도, UI도 필요 없다.

### ② `EntryId` 재번호 — 커맨드가 아니라 **출력 한 줄**

`EP.Inv.Stress`는 과하다. `Dump` 꼬리에 `NextEntryId`를 찍으면 끝이다.

```
> EP.Inv.Dump
  EntryId  Parent  SlotId  ItemId          Charges  SlotSize
  1        -1      -       Bandage         1        1
  3        -1      -       AmmoBox_545     100      1
  4        -1      -       Weapon_AK74     30       5
  ---
  Body : 7 / 10        NextEntryId = 5        ← ★
```

셋 넣고 가운데(2번)를 버린 뒤 하나 더 넣었을 때 **`1, 3, 4` + `NextEntryId=5`** 면 재번호가 없다는 게 증명된다. `2`가 재사용됐거나 목록이 `1,2,3`으로 밀렸으면 즉시 보인다.

시나리오 자동화는 지금 넣지 마라 — 검증할 대상보다 검증 도구가 커진다.

### ③ 월드 픽업 — **Step 01 소관이다**

`Dump`는 인벤토리만 본다. 맞다. 그런데 픽업 목록은 Step 03이 아니라 **Step 01**의 도구다.

```
> EP.Loot.List
  Idx  ItemId          Location            Claimed  Cooldown  Payload
  0    Bandage         (1200, 340, 92)     false    -         1
  1    Backpack_Small  (880, -20, 90)      false    0.31      4      ← 방금 버린 것
```

- Step 01의 완료 조건이 이미 이걸 요구한다 — *"`ClearLoot`이 자기 것만 지우는지는 **Step 03에서 재확인**"* (01, 완료 조건 3번). 재확인할 수단이 없으면 그 줄이 공수표다
- `Cooldown` 열이 Step 03의 "버린 직후 0.5초 회색" 조건을, `Payload` 열이 "배낭 안의 것이 같이 나갔는가"를 직접 보여준다
- **Step 01 문서에 넣어라.** Step 03에 넣으면 픽업 도구가 두 문서에 갈린다

### 정리 — Step 03 완료 조건 13개 중 검증 불가는 **0개**가 된다

| 조건 | 수단 |
|---|---|
| 1~6, 12(재번호) | `EP.Inv.Dump` (+ `NextEntryId` 줄) |
| 7(배낭 자동 착용) | `EP.Inv.Dump`의 `Backpack(N) : x / y` 줄 |
| 8(고아 없음) / 9(되줍기) | `EP.Inv.Dump`의 `Parent` 열 |
| 10(잔탄 보존) | `EP.Inv.Dump`의 `Charges` 열 — **단 실제 검증은 Step 05** (장착 경로가 없다) |
| 11(DropCooldown) | `EP.Loot.List`의 `Cooldown` 열 |
| 13(`COND_OwnerOnly`) | `EP.Inv.DumpAll` (클라에서) |

> **10번은 Step 03 단독으로 확인할 수 없다.** `Charges`를 12로 만들려면 발사가 필요하고 그건 장착 상태여야 한다. `EP.Inv.Add`로 넣은 무기의 `Charges`는 항상 만탄이다. **완료 조건에서 빼거나 "`EP.Inv.Drop` → `EP.Loot.List`의 `Charges`가 보존되는지"로 바꿔라** — 값 복사가 도는지는 그걸로 충분히 증명된다.

---

## 8. §2 답 — 프로세스 문제 **아니다.** 단 세 종류는 예외다

**대체로 넘어가도 된다.** 근거는 이번 데이터 자체다.

3차가 9개를 지적했고, 반영하며 4개가 새로 생겼고, 자체 grep이 그 4개를 잡았고, **남은 것이 2개(`NewId` / `AS`)** 다. 그 2개도 착수 5분 안에 컴파일러가 잡는다. 즉 **자체 grep은 실제로 효과가 있었고, 나머지는 컴파일러가 처리한다.** 여기에 프로세스를 더 얹으면 비용이 이득을 넘는다.

### 다만 컴파일러가 **안** 잡는 세 종류는 다르다

| 종류 | 이번 사례 |
|---|---|
| **① 계약이 어긋난 것** — 양쪽 다 컴파일된다 | **§1.** `RemoveEntry`가 만드는 배열 순서와 `AddSubtree`가 기대하는 순서가 다르다. 컴파일도 되고 배낭 하나짜리 테스트도 통과한다 |
| **② 실행되지만 아무 일도 안 하는 것** | `Entries.Owner` 누락(3차 §10), `bNotifyPending`(§5) |
| **③ 선언은 있는데 부르는 사람이 없는 것** | `CanFit` — 3차에서 지적했고 지금도 `AddItem`이 판정을 인라인으로 다시 쓴다 (§9) |

**이번에 실제로 아팠던 것은 전부 이 셋이다.** 미선언 심볼은 한 번도 아프지 않았다 — 지적받자마자 5분에 고쳐졌고 안 고쳤어도 컴파일러가 잡았을 것이다.

### 규칙 하나만 값이 있다

> **함수를 두 개 이상이 주고받으면, 그 사이의 계약(순서·널 의미·소유권)을 한쪽 문서에 명시한다.**

§1이 정확히 그 누락이다. `RemoveEntry`는 "제거된 서브트리를 반환한다"만 적었고 **"전위 순회, `In[0]`이 루트, `Parent`는 `INDEX_NONE`"** 이라는 계약을 안 적었다. 반대편(`AddSubtree`)은 자기가 편한 계약을 가정했다. 둘 다 문법적으로 완전해서 아무도 안 걸린다.

선언부 표를 먼저 고치고 본문을 쓰는 습관은 이미 효과를 봤으니(자체 grep 4건) 유지하되, **거기서 한 칸 더 나가 "반환값이 어떤 모양인가"를 적으면 §1류가 걸린다.**

---

## 9. 그 외 확인·잔여

### 확인된 것

| 항목 | |
|---|---|
| 3차 §1 처방 | ✅ `RemoveEntry(Id,&Out)` + 03-5의 근거 서술까지 정확 |
| 3차 §2 처방 | ✅ 재귀 + `Children` 선복사 + `RemoveAtSwap` 근거 |
| `EquippedBackpackEntryId` 정리 | ✅ `RemoveEntry` 안 (03-2:248) |
| Step 04 3중 불일치 | ✅ `UEPContainerPanel` 신설 + `RowWidgetClass` 이동 + 근거 |
| `UListView` 각주 | ✅ 3차 §7-③의 "이유가 성능이 아니다"가 정확히 반영됨 |
| DT/DA 원칙 | ✅ 판정선("모든 아이템이 값을 갖는가")까지 |
| `SellPrice` 열 + `bFungible` 예외 규칙 | ✅ 00-8 |
| Step 03 분기가 Step 05에서 처음 돈다 | ✅ 양쪽 문서에 명시 |
| `GAME.md` 미정 #6 → #5 | ✅ |
| ★1·★2 stale | ✅ `LOOT_STATUS.md`에서 제거 |

### 잔여 (경미)

| # | 위치 | 내용 |
|---|---|---|
| 1 | `05_Loot_03_Inventory.md:290` | 스트레이 코드펜스(```` ``` ````). 292~295줄이 코드 블록으로 렌더링된다 |
| 2 | 03-2 `CanFit` | 여전히 선언만이고 `AddItem`이 판정을 인라인으로 다시 쓴다. §1의 `AddSubtree`가 `CanFit`을 쓰게 되니 **이제 실사용자가 생긴다** — `AddItem`도 같이 `CanFit`을 부르게 해서 판정을 한 곳으로 |
| 3 | 마스터 §8 미정 #7 | 저장 대상에 `NextEntryId`만 적혀 있다. 03-2:203은 `EquippedEntryId` / `EquippedBackpackEntryId`까지 셋을 적는다 — **미정 #7을 03-2에 맞춰라** |
| 4 | 03-2 `SetEquippedEntryId` | 가드·`MarkItemDirty` 대상이 아닌 단순 복제 필드인데, 알림을 쏘는지 안 쏘는지 안 적혀 있다. Step 04가 장착 강조를 그리므로(05-4) **쏴야 한다** |
| 5 | 03-9 | `EP.Inv.Dump`가 서버 전용으로 적혀 있다 → §7-① |

---

## 10. 착수 전 결정 3가지

| # | 항목 | 권고 |
|---|---|---|
| 1 | `RemoveEntry`의 순서 재배치 (write-back → 스냅샷(루트 정규화) → 자기 제거 → 캐스케이드) | **한다.** §1의 세 결함이 한 번에 닫히고 `AddSubtree`가 단순해진다 |
| 2 | `SetEntryCharges` 부활 + `AddEntryCharges`가 위임 | **한다.** 3-1 지적이 맞다. 함수는 2개로 유지 |
| 3 | Step 03을 체크포인트 3개(A 코어 / B 배낭 / C 버리기)로 | **한다.** 파일은 쪼개지 않는다 |

여기에 기계적인 것 넷 — `NewId` / `AS` 선언, `bNotifyPending` 삭제, `EP.Inv.Dump` 클라 허용 + `NextEntryId` 출력, `EP.Loot.List` 신설(Step 01).

> **완료 조건 10번(잔탄 12/30 보존)은 Step 03 단독으로 확인할 수 없다.** 문구를 `EP.Inv.Drop` → `EP.Loot.List`의 `Charges` 보존으로 바꾸거나 Step 05로 옮겨라 (§7).

---

## 11. 이번에 확인한 사실

| 사실 | 출처 |
|---|---|
| `PostReplicatedReceive`는 수신 1회당 1회 (3차에서 확인, 반영 확인) | `FastArraySerializer.h:517-519` |
| `PostReplicatedAdd`는 항목마다 (동상) | `FastArraySerializer.h:1163` |
| 삭제는 `RemoveAtSwap` — 순회 중 제거 금지 근거 (동상) | `FastArraySerializer.h:1191` |
| `UEPAttributeSet`은 `AEPPlayerState::GetAttributeSet()`로 얻는다 — `UEPCombatComponent`에 멤버가 없다 | `EPCombatComponent.cpp:171-174`, `EPPlayerState.h:25` |
| `AEPWeapon::GetDamage()`가 `WeaponDef->`를 감싸는 기존 패턴 (`GetMaxAmmo()`의 선례) | `EPWeapon.h:36`, `EPWeapon.cpp:66` |
| `FEPItemData::SellPrice` 기본값 `100` (00-8에 반영 확인) | `EPItemData.h:43` |
