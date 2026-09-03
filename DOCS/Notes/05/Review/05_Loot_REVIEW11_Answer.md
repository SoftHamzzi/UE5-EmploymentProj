# 검수 답변 11차 — 아이템 순서를 서버로 옮긴 것

> 작성일: 2026-08-23
> 요청서: `05_Loot_REVIEW11_Request.md` / 본체: `05_Loot_03_Inventory.md`(1763줄) · `05_Loot_04_InventoryUI.md`(934줄) · `EquipmentSlots.md` §13
> 근거: UE 5.7 엔진 직독 · Lyra 직독 · 프로젝트 문서·소스 직독
> **기억으로 단정한 API는 없다.** 인용은 §11 표에 파일·줄로 있다

---

## 0. 판정 요약

| 항목 | 판정 | 한 줄 근거 |
|---|---|---|
| **§2 세션 충돌** | **실재한다** | `NextEntryId = 1`(`EPInventoryComponent.h:107`, 컴포넌트 필드) vs 디스크에 남는 세이브. **키가 저장소보다 짧게 산다** |
| **§2 10차 §5 판정** | **틀렸다. 내 잘못이다** | `ULocalPlayerSaveGame`을 제시하면서 **키의 수명을 확인하지 않았다.** 서버로 옮기는 것이 맞다 |
| §2-6-3 제3의 안 | **기각이 맞다. 근거를 강화한다** | 서버가 세션 GUID를 복제하는 순간 **비용은 다 내고 이득만 못 받는다** (§2-4) |
| **★ 새 결함 1** | **`SortKey`가 형제 안에서 겹칠 수 있다** | 장착 → 획득 → 해제로 **정상 플레이에서 동률이 난다.** 03-1의 *"겹칠 문법이 없다"* 가 거짓 (§3) |
| **★ 새 결함 2** | **재정규화가 이분 고갈에만 걸려 있다** | 맨 앞/맨 뒤 분기는 **무한 증감**이고 `int32` 경계에서 순서가 영구히 깨진다 (§4) |
| **§3 `int32` 희소** | **맞다. 그런데 요청서의 근거 하나가 틀렸다** | *"조밀이면 재정규화 코드가 사라진다"* 는 거짓 — **`RenormalizeSortKeys`가 곧 조밀 재번호 함수다** (§5-1) |
| §3 `double` 기각 | **맞다. 근거를 바꾼다** | DB/Dump보다 **고갈 판정이 부동소수 동등 비교**가 되는 쪽이 결정적 |
| §3 컨테이너 배열 기각 | **맞다. 근거를 하나 더 준다** | 그것은 **9차가 기각한 바로 그 모양**이다 (부모가 자식 참조 배열을 듦) |
| §3-4 경로 정정 | **정정이 맞다** | 기본 생성자가 `SetDeltaSerializationEnabled(true)`를 직접 부른다(`FastArraySerializer.cpp:35`) |
| §4 `Server_ReorderEntry` | **함수는 맞다. RPC는 04-B다** | 9차의 `MoveEntry` / `Server_MoveEntry` 분리와 **같은 기준** (§6-2) |
| §4 앞 이웃 vs 인덱스 | **앞 이웃이 맞다** | 근거가 요청서보다 강하다 — 인덱스는 **낙관적 적용과 상극**이다 (§6-3) |
| §5 `bIsRoot` 이중 관장 | **맞다. 안전하다** | 두 필드가 아니라 **한 개념**이다. 다른 인벤토리에서 와도 참 (§7) |
| §6 범위 | **03-D로 쪼개지 않는다. 대신 03-A에서 RPC를 뺀다** | 경계에서 stale이 나는 비용이 더 크다(8차). 1주는 **03-A 하나 분량**이다 (§8) |
| §7-4-1 `SwapEntries` 키 교환 | **미룬 것은 맞다. 단 계약 한 줄은 지금** | `AssignSortKey`가 유일 쓰기 지점이라는 규칙이 그 함수에 걸린다 (§9-1) |
| §7-4-2 전방선언 | **문서에 적는다 — 한 번만** | 세 라운드 연속 헤더 앞부분에서 걸렸다 (§9-2) |
| §8 조사 | **선례 없음이 정직한 답** | 엔진 전체 `FFastArraySerializerItem` 파생 **6개, 순서 필드 0개** (§10) |

**한 줄 결론:** 뒤집은 판단이 맞고 **내 10차 판정이 틀렸다.** 뒤집은 뒤의 설계도 대체로 맞는데, **`SortKey`가 겹칠 수 있고(§3) 경계에서 깨진다(§4).** 둘 다 지금 고치면 각각 두세 줄이다.

---

## 1. 먼저 — 10차 §5 판정을 철회한다

10차 답변 §2-4는 이렇게 적었다.

> *"★ 이게 §2-5의 첫 두 줄을 지운다. **재접속·사망 시 순서 소실**과 **다른 기기에서 순서 다름**을 **받아들일 필요가 없다.** `ULocalPlayerSaveGame`은 저장이고, Lyra의 주석이 'safe to store in the cloud'라고 직접 말한다. **대가를 안 치르고 이득만 남는다.**"*

**틀렸다.** `ULocalPlayerSaveGame`이 저장한다는 것은 맞지만, **저장한 내용이 다음 세션에 의미를 잃는다는 것을 확인하지 않았다.** 저장소를 검증하고 **키를 검증하지 않았다.**

`ULyraSettingsShared`가 저장하는 것은 키바인드·자막 크기 같은 **자기완결적 값**이다. 우리가 저장하려던 것은 **서버가 런타임에 발급한 번호의 목록**이다. 같은 저장소가 두 경우에 같은 값을 하지 않는다.

> **일반화하면 이렇다 — 저장되는 키는 저장소만큼 오래 살아야 한다.** `EntryId`의 수명은 `UEPInventoryComponent`이고(매치), `ULocalPlayerSaveGame`의 수명은 디스크다. **짧은 것으로 긴 것을 색인했다.**

**§2-5가 *"Claude의 첫 권고가 틀렸다"* 로 적은 것도 정확하다.** 다만 틀린 것이 하나 더 있다 — **10차 답변 자체**가 먼저 틀렸고, "지금은 옮기지 마라"는 그 틀린 전제 위에서 나온 두 번째 오류다.

---

## 2. ★ §2 — 세션 충돌은 실재한다

### 2-1. 세 사실을 대조했다. 조합이 맞다

| 사실 | 확인 |
|---|---|
| `NextEntryId`가 컴포넌트 필드, 초기값 **1** | `EPInventoryComponent.h:107` — `int32 NextEntryId = 1;` **직독 확인** |
| 복제하지 않는 서버 전용 필드 | 03-2. 즉 **클라는 이 값을 알 수도 없다** |
| 컴포넌트가 `AEPCharacter`에 붙는다 | §8 확정표. 매치가 끝나면 캐릭터와 함께 죽는다 |
| `ULocalPlayerSaveGame`은 디스크 슬롯 | `SaveGame.h:40-47` |

**따라서 다음 매치의 3번은 지난 매치의 3번과 아무 관계가 없다.** `Resolve`가 `Live.Contains(3)`만 보므로 통과시킨다. **요청서 §2-2의 시나리오가 그대로 성립한다.**

### 2-2. 그리고 증상이 요청서가 적은 것보다 나쁘다

요청서는 *"가끔 순서가 이상하다"* 로 적었다. **한 겹 더 있다.**

`Order[Container]`의 키도 `EntryId`다(배낭의 번호). 다음 매치에서 **7번이 배낭이 아니라 조끼**면, 조끼의 내용물이 **지난 매치 배낭의 순서표**로 정렬된다. 즉 잘못된 순서가 **컨테이너 단위로 통째로** 적용되고, `Resolve`의 "목록에 없으면 뒤에 붙인다" 규칙이 그걸 그럴듯하게 만든다.

**그리고 이건 사용자 눈에 "인벤토리가 가끔 자기 마음대로 정렬된다"로 보인다** — 순서 기능 자체에 대한 신뢰를 깨는 부류다.

### 2-3. §2-3 세션 도장 — 맞다. 저장이 한 번도 안 쓰인다

`FGuid SessionId` 불일치면 비우는 방식은 **정확히 안전하지만**, 로드맵 14번(서버 세이브)이 마감 범위 밖이라 **매치 시작마다 항상 불일치**다. 즉 디스크에 쓰고, 다음에 읽고, 버린다. **`ULocalPlayerSaveGame`을 쓰는 코드 전체가 no-op이 된다.**

**그러면 남는 것은 10차 이전 설계(서브시스템 인메모리)와 같고, 그건 요청서 §2-3의 진단 그대로다.**

### 2-4. §2-6-3 제3의 안 — 기각이 맞다. 근거를 강화한다

*"서버가 세션 GUID를 복제하고 클라가 키에 섞는다"* 를 검토했고 기각한 것이 맞다. 요청서의 근거(*"서버가 이미 개입하는데 순서만 클라에 두는 이유가 없다"*)는 맞지만 조금 약하다. **정확히는 이렇다.**

| | 세션 GUID 혼합안 | 서버 `SortKey` |
|---|---|---|
| 서버가 복제하는 것 | 세션 GUID (1개) | `SortKey` (엔트리당 int32, **델타라 바뀔 때만**) |
| 서브트리 순서 보존 | **불가능** — 되주우면 `EntryId`가 전부 새 번호다 | 된다 (§7) |
| "전부 옮기기" | 여전히 클라가 N번 호출 (10차 §2-3의 우회로) | 서버가 `GetSortedContents` 직접 호출 |
| 로드맵 14번이 오면 | **클라 세이브와 서버 세이브를 맞춰야** 한다 | `UPROPERTY` 하나가 따라간다 |
| 클라 코드 | `Resolve`/`MoveTo`/세이브 클래스 2개 | **0** |

**세션 GUID 혼합안은 복제를 시작하면서 그 대가로 얻는 것이 "충돌 안 남" 하나뿐이다.** 비용을 내기 시작한 이상 제대로 내는 편이 낫다.

### 2-5. ★ 오늘 실제로 얻는 것을 정직하게 적어둔다

**요청서 §2-5의 *"매치 안에서는 이득 0"* 은 절반만 맞다.** 서버 세이브가 없으므로 **매치를 넘는 지속은 서버로 옮겨도 오늘은 안 생긴다.** 그건 사실이다. **그런데 오늘 생기는 이득이 셋 있다.**

1. **버린 배낭을 되주우면 내용물 순서가 산다** (§7). 클라 로컬로는 원리적으로 불가능했다 — `AddSubtree`가 `EntryId`를 전부 재발급하므로 저장된 목록이 전부 미아가 된다
2. **"전부 옮기기"의 우회로가 사라진다.** 10차 §2-3이 *"클라가 자기 순서대로 `Server_MoveEntry`를 N번"* 이라는 설계를 남겨뒀는데, 그게 **N번의 부분 실패**를 다뤄야 하는 코드였다. `GetSortedContents`가 서버에 있으면 한 번에 판정한다
3. **클라·서버 순서 불일치가 원천 봉쇄된다.** `FastArraySerializer.h:54`가 배열 순서를 보장하지 않으므로, 순서의 진실이 양쪽에 따로 있으면 **재현 안 되는 불일치**가 남는다

**그리고 코드량이 준다** — §2-5가 적은 대로 `Resolve`·`MoveTo`·세이브 클래스 2개·`FEPContainerOrder`·함정 5건이 사라진다. **"지금 하면 지금도 싸다"가 맞다.**

> **§2 판정: 뒤집은 것이 맞다. 11차는 되돌아가지 않는다.**

---

## 3. ★ 새 결함 1 — `SortKey`가 형제 안에서 겹친다

03-2가 이렇게 단언한다.

> *"동률 타이브레이크는 정상 데이터에서 안 쓰인다. `AssignSortKey`가 유일한 쓰기 지점이라 **형제 키가 겹칠 문법이 없다**."* (`05_Loot_03_Inventory.md:820`)

**겹친다. 문법이 있다.**

### 3-1. 재현 — 장착 → 획득 → 해제

세 함수가 맞물린다.

```cpp
// :976-979 — 형제 "최대 키"를 GetSortedContents로 구한다
int32 UEPInventoryComponent::NextKeyAtEndOf(int32 Container) const
{
    const TArray<int32> Sorted = GetSortedContents(Container);   // ★ 슬롯에 든 것은 빠진다 (:332)
    return Sorted.Num() ? KeyOf(Sorted.Last()) + SortKeyStep : 0;
}

// :720 — 슬롯으로 갈 때는 재발급하지 않는다
// "NewSlotId가 있으면 재발급하지 않는다. 슬롯에 든 것은 SortKey를 안 본다"

// :714 — 재발급 조건은 "부모가 바뀌었을 때"뿐
```

```
본체(Parent = -1)에 셋:   붕대 0    구급상자 65536    AK 131072

① AK를 1번 핫바에 장착   MoveEntry(AK, -1, "Hotbar1")
     부모가 안 바뀐다(-1 → -1)  →  :714 재발급 안 함
     NewSlotId가 있다           →  :720 재발급 안 함
     ⇒ AK.SortKey = 131072 그대로. 그런데 GetSortedContents(-1)에서는 빠진다

② 아무거나 하나 줍는다    InsertEntry(Parent = -1, SlotId = None)
     NextKeyAtEndOf(-1) = KeyOf(구급상자) + 65536 = 131072
     ⇒ 새 아이템.SortKey = 131072            ← ★ AK와 동률

③ AK를 해제              MoveEntry(AK, -1, None)
     부모가 안 바뀐다(-1 → -1)  →  재발급 안 함
     ⇒ 본체에 SortKey 131072가 둘
```

**세 단계 전부 정상 플레이다.** 그리고 이건 **가장 흔한 조작 순서**다 — 총을 꽂고, 뭘 줍고, 총을 뺀다.

### 3-2. 증상 — 타이브레이크가 있어서 크래시는 아니고, 정리해둔 순서가 흐트러진다

`GetSortedContents`에 `EntryId` 타이브레이크가 있으므로(`:811`) 클라·서버 불일치까지는 안 간다. **그러나 동률이 쌓이면 그 그룹 안 순서가 `EntryId` 순 — 즉 획득 순 — 으로 고정된다.** 플레이어가 손으로 맞춰둔 배치가 **무기를 뺐다 꽂을 때마다 조금씩 무너진다.**

**그리고 `:820`의 서술 때문에 이 버그를 찾을 때 `AssignSortKey`를 먼저 보게 된다** — 거기엔 아무 문제가 없다.

### 3-3. 처방 — 키의 스코프를 스펙대로 되돌린다. 코드가 줄어든다

**03-1의 스펙은 이미 옳게 적혀 있다** — *"형제(같은 `Parent`) 스코프"*(`:259` 부근). 구현 힌트가 그걸 어겼다. **`GetSortedContents`는 "그릴 것"을 고르는 함수이지 "키 공간"을 정의하는 함수가 아닌데, `NextKeyAtEndOf`가 그걸 빌려 썼다.**

```cpp
// ① 키 공간은 부모만 본다. 슬롯 여부를 보지 않는다
int32 UEPInventoryComponent::NextKeyAtEndOf(int32 Container) const
{
    int32 Max = INDEX_NONE;
    bool  bAny = false;
    for (const FEPInventoryEntry& E : Entries.Items)
        if (E.ParentEntryId == Container) { Max = bAny ? FMath::Max(Max, E.SortKey) : E.SortKey; bAny = true; }
    return bAny ? Max + SortKeyStep : 0;
}

// ② 삽입도 슬롯 여부로 갈리지 않는다 — :969의 삼항을 없앤다
E.SortKey = NextKeyAtEndOf(Parent);       // 전: SlotId.IsNone() ? NextKeyAtEndOf(Parent) : 0

// ③ :720의 "슬롯으로 갈 때 재발급 안 함" 예외도 없앤다 — 부모가 안 바뀌면 원래 안 바뀐다
```

**세 곳 다 예외가 사라진다.** 그리고 부수 효과가 좋다 — **무기를 뺐다 꽂으면 원래 자리로 돌아온다.** 키를 계속 들고 있었기 때문이다. 지금 설계에서는 맨 뒤로 간다.

> **`RenormalizeSortKeys`도 같은 이유로 부모 전체를 돌아야 한다.** 지금은 `GetSortedContents`로 돌아서(`:829`) **슬롯에 든 형제의 키만 옛 값으로 남는다** — 재정규화 직후에 동률이 새로 생긴다.

> **타이브레이크(`:811`)는 그래도 남긴다.** 이유가 바뀐다 — *"겹칠 수 없지만 만약을 위해"* 가 아니라 **"복제 지연 중 클라가 잠깐 옛 키를 들고 있을 수 있어서"** 다. `:820`의 문장을 그렇게 고치는 것을 권한다.

---

## 4. ★ 새 결함 2 — 재정규화가 이분 고갈에만 걸려 있다

```cpp
// :775-781 — 재정규화 트리거
if (!bTail && PrevEntryId != INDEX_NONE && NewKey <= PrevKey)
{
    RenormalizeSortKeys(Container);
    ...
}
```

**"사이에 끼우기"에만 걸려 있다.** 나머지 두 분기는 무한히 증감한다.

| 분기 | 식 | 경계 |
|---|---|---|
| 맨 앞 (`:768`) | `KeyOf(Sorted[0]) - SortKeyStep` | **하한 없음.** 반복하면 `INT32_MIN` 아래로 |
| 맨 뒤 (`:770`) · `NextKeyAtEndOf` (`:979`) | `PrevKey + SortKeyStep` | **상한 없음.** 반복하면 `INT32_MAX` 위로 |

`int32` 오버플로가 나면 부호가 뒤집혀 **맨 앞으로 보낸 아이템이 맨 뒤에 나타난다.** 그리고 그 상태는 재정규화가 안 걸리므로 **영구적**이다.

**도달 가능한가.** 65536 간격이면 32768번이다.

- **맨 앞 반복**: 같은 아이템을 계속 맨 앞으로 끌면 매번 65536씩 내려간다. 사람이 32768번은 안 하지만 **매크로로는 한다** — 그리고 이건 서버 상태를 영구히 망가뜨리는 조작이다
- **맨 뒤 (`NextKeyAtEndOf`)** 가 더 현실적이다. **줍기 경로가 매번 이걸 부른다.** "줍고 버리고"를 반복하면 남은 아이템의 최대 키가 래칫처럼 올라간다 — 키 0과 65536이 있을 때 0을 버리고 하나 주우면 131072, 65536을 버리고 주우면 196608… **한 사이클에 65536.** 로드맵 14번의 스태시(280칸, 세션을 넘어 삶)에서는 **누적된다**

**처방은 세 분기 공통 가드 한 줄이다.**

```cpp
// 세 분기 어디서든, 경계에 가까우면 먼저 재정규화하고 다시 계산한다
constexpr int32 KeyGuard = SortKeyStep * 4;
if (NewKey <= INT32_MIN + KeyGuard || NewKey >= INT32_MAX - KeyGuard || (!bTail && NewKey <= PrevKey))
{
    RenormalizeSortKeys(Container);
    /* 다시 계산 */
}
```

> **`InsertEntry`에도 같은 가드가 필요하다.** 지금 `NextKeyAtEndOf`는 실패를 표현할 방법이 없고, `InsertEntry`는 **줍기 경로라 실패하면 안 된다.** 가드를 `NextKeyAtEndOf` 안에 두면(넘칠 것 같으면 재정규화 후 반환) 호출부 둘이 다 덮인다 — **`AssignSortKey`가 유일 쓰기 지점인 것과 같은 형태로 `NextKeyAtEndOf`를 유일 발급 지점으로 만든다.**

> **재귀는 안전하다.** 재정규화 후 간격이 정확히 `Step`이라 이분이 반드시 성공하고, 맨 앞/맨 뒤도 경계에서 멀어진다. 한 번만 돈다. **다만 형제가 32767개를 넘으면 재정규화 자체가 넘치는데, `ContainerCapacity`가 그보다 훨씬 작아 도달 불가다** — 03-1에 한 줄로 적어두면 나중에 스태시 용량을 정할 때 상한이 보인다.

---

## 5. §3 키 배치 — `int32` 희소가 맞다

### 5-1. ★ §3-5-1의 전제가 틀렸다 — 조밀이 재정규화 코드를 없애지 않는다

요청서는 *"[조밀이면] 재정규화 코드가 통째로 사라진다"* 로 적었다. **사라지지 않는다. 이름만 바뀐다.**

```cpp
// 희소의 RenormalizeSortKeys (:826-833)
int32 K = 0;
for (int32 Id : 형제) { AssignSortKey(Id, K); K += SortKeyStep; }

// 조밀 재번호
int32 K = 0;
for (int32 Id : 형제) { AssignSortKey(Id, K); K += 1; }
```

**같은 루프다.** 조밀 방식은 이 함수를 **재배치마다** 부르고, 희소는 **고갈 때만** 부른다.

따라서 실제 비교는 이렇다.

| | 조밀 재번호 | `int32` 희소 |
|---|---|---|
| 공통 코드 | 순차 배정 루프 | 같은 루프 |
| 추가 코드 | 없음 | **중점 계산 + 고갈 가드 ≈ 5줄** |
| 재배치 1회 비용 | **형제 N개 `MarkItemDirty`** | **1개** |
| 스태시 280칸 | 드래그마다 280개 changelist 비교 + ~3.4KB | 1개 · 12B |

**희소가 5줄을 더 쓰고 O(N)을 O(1)로 만든다.** 그리고 그 5줄은 §4의 가드를 넣으면 어차피 필요하다.

> **§3-5-1의 질문(*"3.4KB가 조밀을 기각할 만큼인가"*)에 대한 답: 3.4KB로 기각하는 게 아니다.** 대역폭은 UI 조작 빈도에서 문제가 안 된다. **기각 사유는 "재정규화 코드를 아낀다"는 이득이 존재하지 않는다는 것**이다. 없는 이득과 실재하는 O(N)을 맞바꿀 이유가 없다.

### 5-2. `double` 기각 — 맞다. 근거를 바꾼다

요청서의 두 근거(DB/REST 부동소수, `Dump`에 `0.37500000000000006`)는 사실이지만 **둘 다 부차적이다.** 로드맵 15번은 마감 밖이고, Dump는 포맷으로 가릴 수 있다.

**결정적인 것은 고갈 판정이다.**

```cpp
// int32 : 정확하다
if (NewKey <= PrevKey) 재정규화;

// double : 부동소수 동등 비교가 유일한 판정이다
const double Mid = Prev + (Next - Prev) * 0.5;
if (Mid == Prev || Mid == Next) 재정규화;   // 맞지만, 보는 사람마다 버그로 신고한다
```

**정답인데 영원히 의심받는 코드**다. 그리고 `int32`는 §4의 경계 가드도 상수 비교로 끝난다.

> **52회 vs 16회는 실질 차이가 없다.** 재정규화가 있으면 둘 다 "사실상 안 남"이고, 없으면 둘 다 언젠가 깨진다. **분기 횟수는 이 선택의 축이 아니다.**

### 5-3. 컨테이너 배열 기각 — 맞다. 세 번째 근거가 있다

요청서의 둘(① 본체는 엔트리가 없다 ② 부모 배열 청소가 생긴다)이 맞다. **하나 더 있고, 이게 제일 무겁다.**

**`TArray<int32> ContentOrder`를 부모가 드는 것은 9차가 기각한 바로 그 모양이다.** 9차 §2는 *"별도 배열(부모가 자식 참조 목록을 듦)"* 대신 *"자식이 자기 자리를 듦(`SlotId`)"* 을 택했고, 근거는 `USceneComponent`가 `AttachSocketName`을 자식에 두고 `AttachChildren`은 `Transient` 파생 색인이라는 것이었다.

**`SortKey`는 그 결정의 연장선이다.** 자식이 자기 자리를 든다. 컨테이너 배열은 **같은 논쟁을 다른 필드로 다시 하는 것**이고, 9차에서 확인한 결함(부모 배열이 죽은 참조를 들 수 있다)이 그대로 재현된다 — 요청서 ②가 그것이다.

> **`ActiveHotbarIndex` vs `HotbarRefs` 대비를 요청서가 정확히 인용했다**(`:271`). 같은 기준이 세 번째로 같은 답을 낸다.

### 5-4. §3-5-4 경로 판정 — **정정이 맞다**

기본이 델타 경로인 것이 맞다. **근거는 요청서가 든 것(`:218-219` 주석, `:1398-1401` 분기)보다 더 직접적인 것이 있다.**

```cpp
// FastArraySerializer.cpp:24-36 — 기반 생성자가 스스로 켠다
FFastArraySerializer::FFastArraySerializer()
    : ... , DeltaFlags(EFastArraySerializerDeltaFlags::None)
{
    SetDeltaSerializationEnabled(true);       // ★ 아무도 안 불러도 켜져 있다
}
```

즉 `HasDeltaBeenRequested`가 **기본으로 서 있고**, `:1398-1401`의 분기가 그래서 델타 쪽으로 간다. `:1474-1485`의 `NetSerializeStruct`는 요청서 말대로 폴백이다.

> **한 가지 단서를 덧붙인다.** 델타는 연결이 협상해야 한다 — `bSupportsFastArrayDeltaStructSerialization`이 `FEngineNetworkCustomVersion::FastArrayDeltaStruct` 이상일 때만 켜진다(`RepLayout.cpp:4590-4597`). **같은 빌드끼리는 항상 참이고**, `net.SupportFastArrayDelta 0`으로 끌 수 있다. 즉 **"바뀐 프로퍼티만"은 기본값이지 불변 보장이 아니다** — 설계를 거기 걸지 않는 편이 좋고, §5-1의 판정은 그것과 무관하게 성립한다(O(N) vs O(1)).

---

## 6. §4 `Server_ReorderEntry`

### 6-1. 별도 연산인 것 — 맞다. 10차 §3과 같은 기준이다

10차가 `SwapEntries`를 승인한 기준은 *"`MoveEntry` 두 번으로 안 되는 이유가 실재한다"* 였다. **여기서도 실재하고, 성격이 더 강하다.**

`MoveEntry`에 4번째 인자를 붙이면 **검사 7개 중 5개가 항상 건너뛰는 분기**가 되고, 무엇보다 **함수의 실패 계약이 갈린다** — 같은 함수가 "실패할 수 있음"과 "실패할 수 없음"을 동시에 가진다. 10차 §5-1이 `MoveEntry(..., DisplacedEntryId)`를 기각한 것과 **같은 형태의 문제**다(한 함수에 불변식 둘).

**그리고 "실패할 수 없다"는 계약 자체가 산출물이다.** 04-7의 낙관적 적용이 그것 위에 서 있다. 함수를 합치면 그 계약을 말할 자리가 없어진다.

### 6-2. ★ 그런데 RPC는 03-A가 아니라 04-B다

**9차가 세운 규칙을 그대로 적용하면 이 결론이 나온다.**

> 9차 §4-2: *"`MoveEntry`(내부 계약)는 지금. `Server_MoveEntry`(외부 표면)는 Step 04의 드래그와 함께. **검증 표면을 소비자보다 먼저 열지 않는다.**"*

`Server_ReorderEntry`의 소비자는 04-B의 드래그다. Step 03에는 **정당한 클라 호출자가 없다.**

| | 어디 | 이유 |
|---|---|---|
| `AssignSortKey` · `NextKeyAtEndOf` · `KeyOf` | **03-A** | `InsertEntry`가 부른다. 없으면 컴파일 안 됨 |
| `GetSortedContents` · `RenormalizeSortKeys` | **03-A** | 위가 부른다 |
| **`ReorderEntry(EntryId, PrevEntryId)`** (일반 함수) | **03-A** | `EP.Inv.Reorder`가 부른다 |
| **`Server_ReorderEntry`** (RPC) | **04-B** | **드래그가 첫 호출자다** |

**`EP.Inv.Reorder`가 RPC가 아니라 일반 함수를 부르면 03-A의 검증 목표가 그대로 달성된다.** 03-9의 *"순서 계약을 UI보다 먼저 닫는다"*(`:1639`)는 커맨드가 있으면 성립하고, **RPC 유무와 무관하다.**

> **이게 §6-3의 질문에 대한 답이기도 하다.** *"`Server_ReorderEntry`·`RenormalizeSortKeys`·`EP.Inv.Reorder`까지 03-A여야 하는가"* → **뒤의 둘은 그렇고, RPC는 아니다.** 03-A에서 `UFUNCTION(Server, Reliable)` 하나와 그 검증(조작된 `PrevEntryId` 처리)이 빠진다.

### 6-3. 앞 이웃 vs 인덱스 — 앞 이웃. 근거가 요청서보다 강하다

요청서의 두 근거(목록 어긋남, 검증 한 줄)가 맞다. **결정적인 것은 04-7의 낙관적 적용과의 관계다.**

**인덱스는 낙관적 적용과 상극이다.** 클라가 먼저 그리고 서버가 나중에 확정하는데, 인덱스를 보내면 **서버가 확정한 자리가 클라가 그린 자리와 다를 수 있다** — 그러면 화면이 한 번 튄다. 그리고 그 튐은 *"가끔 드래그가 한 칸 밀린다"* 로 보여서 UI 버그처럼 재현된다.

**앞 이웃은 그 튐이 구조적으로 없다.** 클라가 "붕대 뒤"라고 말하고 서버도 "붕대 뒤"에 놓기 때문에, 목록이 그 사이에 바뀌었어도 **둘의 해석이 같다.**

> 다중 선택 확장(`TArray<int32>`)은 지금 만들지 않는 것이 맞다 — CLAUDE.md §2. 요청서가 스스로 그렇게 적었다.

### 6-4. §4-4-2 셋이 많은가 — 아니다. 단 게이트는 하나여야 한다

`Server_MoveEntry` / `Server_SwapEntries` / `Server_ReorderEntry` 셋은 **검증 표면이 서로 다르고 실패 계약도 다르다.** 8차가 세운 *"서버가 이미 소유한 상태의 변경 요청 → 컴포넌트의 서버 RPC"* 안에 셋 다 들어간다.

**합칠 형태는 없다.** 합치면 §6-1의 문제(한 함수에 불변식 여럿)가 세 배가 된다.

> **확인한 것 하나:** 셋 다 `CanMutateInventory()`를 첫 줄에서 부른다(`:739`). **8차가 만든 "게이트는 한 곳" 규칙이 셋으로 늘어난 뒤에도 지켜지고 있다.** 이건 잘 됐다.

---

## 7. §5 `bIsRoot`가 두 필드를 관장하는 것 — 맞다

### 7-1. 두 필드가 아니라 한 개념이다

`bIsRoot`가 뜻하는 것은 *"이 엔트리는 옛 컨테이너 소속을 잃는다"* 하나다. 그 소속이 두 필드로 표현될 뿐이다 — **어느 컨테이너인가(`ParentEntryId`)와 그 안 몇 번째인가(`SortKey`).** 별도 플래그로 가르면 **둘이 어긋난 스냅샷**(부모는 정규화됐는데 키는 옛 컨테이너 것)이 표현 가능해진다.

**CLAUDE.md §2의 *"한 값을 두 경로가 봐야 하면 둘 다 볼 수 있는 곳에 둔다"* 와 같은 방향이다.**

### 7-2. §5-3 다른 인벤토리에서 오는 경우 — 안전하다

`Container->RemoveEntry` → `MyInv->AddSubtree`(03-4)에서도 참이다. 근거를 정확히 하면 요청서가 적은 것(*"부모가 방금 만들어진 빈 컨테이너"*)보다 조금 넓다.

```
AddSubtree(Dest, In)
  In[0] (루트)      →  InsertEntry(Dest, ...)  →  Dest 안의 "맨 뒤" 키를 새로 받는다
  In[1..] (자식)    →  InsertEntry(NewRootId, ...) → AssignSortKey(NewId, Src.SortKey)
                       ↑ 부모가 방금 만든 NewRootId다. 그 부모의 자식은 In[1..]뿐이다
```

**자식의 형제 집합은 `In` 안에서 닫혀 있다.** `In`은 한 서브트리에서 나온 것이므로 그 안의 키는 이미 유일하고, 새 부모에는 다른 자식이 없다. **어느 인벤토리에서 왔는지는 무관하다** — 키가 원본 인벤토리의 다른 컨테이너와 겹쳐도 부모가 다르므로 상관없다(§3-1의 형제 스코프).

> **★ 단 §3의 수정이 여기에도 걸린다.** 자식 중에 **슬롯에 든 것**(총에 달린 조준경)이 있으면 현행 `InsertEntry`가 키 0을 주고 그 다음 줄에서 `AssignSortKey(NewId, Src.SortKey)`가 덮는다 — 결과는 맞다. §3-3의 수정(삽입 시 슬롯 여부로 안 가름)을 적용하면 **덮어쓰기가 아니라 그냥 일관된다.**

### 7-3. §5-2의 진단이 정확하다

*"`In`은 `Entries.Items` 순회 결과라 FastArray 내부 순서다"* 가 맞다. 그리고 이게 **왜 자식 키 보존이 선택이 아니라 필수인지**를 말한다 — 보존을 안 하면 순서가 *"화면 순서"* 도 *"획득 순서"* 도 아닌 **복제 구현 세부**로 정해진다. `FastArraySerializer.h:54`가 그 순서를 보장하지도 않는다.

> **완료 조건 *"배낭을 버렸다 다시 주우면 안의 아이템 순서가 그대로다"*(`:29`)가 이걸 정확히 잡는다.** 아이템 두세 개로는 안 보이므로, **검증할 때 배낭에 최소 4개를 넣고 손으로 섞어둔 뒤** 버리라고 조건에 적어두는 게 좋다. 안 적으면 통과했는데 안 고쳐진 상태가 된다.

---

## 8. §6 범위

### 8-1. 03-D로 쪼개지 않는다

**쪼개는 비용이 8차가 이미 측정한 것이다** — 경계에서 `RemoveEntry`가 갈려 stale이 세 번 연속 났고, 그래서 *"파일은 쪼개지 않는다"*(03 STATUS)가 나왔다. 03-A를 또 나누면 `SortKey` 발급(`InsertEntry`)과 재배치(`ReorderEntry`)가 갈리는데, **둘은 같은 불변식(형제 안 유일)을 공유한다.** §3의 결함이 정확히 그 불변식이 깨진 것이고, **갈라 두면 한쪽만 고친다.**

### 8-2. 대신 03-A에서 둘을 뺀다

| 뺄 것 | 어디로 | 근거 |
|---|---|---|
| **`Server_ReorderEntry` RPC** | 04-B | §6-2. 소비자가 거기다 |
| `SwapEntries`의 `SortKey` 교환 | 04-B | 함수 자체가 Step 04 (10차 확정) |

**그러면 03-A의 `SortKey` 일습은 이렇게 남는다** — `SortKey` 필드 · `AssignSortKey` · `NextKeyAtEndOf` · `KeyOf` · `GetSortedContents` · `RenormalizeSortKeys` · `ReorderEntry` · `EP.Inv.Reorder` · `Dump` 열. **함수 여섯과 커맨드 하나이고, 넷은 열 줄 미만이다.**

### 8-3. ★ 1주는 03-A 하나 분량이다 — 정직하게 적는다

**§6-3의 질문에 그렇다고 답할 수 없다.** 03-A에 현재 들어 있는 것을 세면:

```
FastArray(트레이트·NetDeltaSerialize·PostReplicatedReceive)   ← 처음 쓰는 기능
InsertEntry / AddItem / bFungible 합치기 / GetUsedSlots / GetCapacity / CanFit
GetEntryInSlot / MoveEntry(검사 7개) / ActiveHotbarIndex
SortKey 일습 6개 + 재정규화
FScopedInventoryNotify / COND_OwnerOnly
SlotPriority(DT 마이그레이션) / BodySlots
EP.Inv.Add / Dump / Reorder
```

**이게 1주다. 03-B·03-C가 그 위에 얹힌다.** Step 03 전체를 1주로 잡았다면 **추정이 어긋나 있고, 어긋난 쪽은 범위가 아니라 추정이다** — 범위 항목은 하나하나 근거가 있고 9차·10차·11차가 각각 그 근거를 확인했다.

> **권고: 일정 단위를 Step이 아니라 구간(03-A / 03-B / 03-C)으로 바꾼다.** 그러면 "1주"가 03-A에 붙고, 나머지 둘은 각각 며칠로 따로 선다. **STATUS 파일이 이미 구간 단위라 표기만 맞추면 된다.**

> **줄일 수 있는 것이 하나 있긴 하다** — `SlotPriority`의 DT 마이그레이션은 배낭 행 하나(`["Back"]`)면 03-A가 돌아간다. 나머지 행은 Step 05에서 채워도 9차 §6-4의 논거(*"미루면 전 행을 다시 채운다"*)가 깨지지 않는다 — **행이 아니라 필드가 있으면 되기 때문이다.** 03-A는 필드 추가 + 한 행만.

---

## 9. §7-4 아직 안 한 것 둘

### 9-1. `SwapEntries`의 `SortKey` 교환을 미룬 것 — 맞다. 단 계약 한 줄은 지금

**함수 본문이 04-B이므로 본문을 미룬 판단은 맞다.** 10차가 `SwapEntries`를 Step 04로 확정했고 경계를 넘지 않는 것이 옳다.

**그런데 03-2에는 한 줄이 지금 들어가야 한다.** 03-2의 단일 쓰기 지점 표(`:612`)가 *"`SortKey` → `AssignSortKey`가 유일한 지점"* 이라고 선언했으므로, **`SwapEntries`가 `E.SortKey = Other.SortKey`로 쓰면 그 규칙이 깨진다.** 그리고 10차 §5-4가 이미 경고한 것과 같은 자리다 — 살아 있는 엔트리에 통째 대입 금지.

```
03-2 단일 쓰기 지점 표에 한 줄:
  SwapEntries도 AssignSortKey를 두 번 부른다 — 직접 대입하지 않는다 (04-B에서 구현)
```

**계약은 소유자 문서에, 구현은 소비자 문서에.** 8차가 `RemoveEntry`↔`AddSubtree` 계약에 쓴 방식과 같다.

### 9-2. 전방선언 — 문서에 적는다. 한 번만

**적어야 한다.** 근거 둘.

1. **CLAUDE.md가 프로젝트 관례로 명시한다** — *"Forward declarations in headers; `#include` only in .cpp"*. 관례를 문서가 어기면 관례가 아니라 문서가 이긴다
2. **세 라운드 연속 헤더 앞부분에서 걸렸다.** 9차 §11-13(include 누락·전방선언 없음·`SetIsReplicatedByDefault`), 10차, 이번 §1-2. **반복되는 것은 문서 구멍이다**

**모든 코드블록에 적을 필요는 없다.** 03-1이나 03-2 앞에 **파일 앞부분 블록 하나**를 두고, 이후 블록은 클래스 본문만 보여주면 된다.

```cpp
// EPInventoryComponent.h — 파일 앞부분
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Inventory/EPInventoryTypes.h"      // FEPInventoryEntry (완전 타입 필요)
#include "EPInventoryComponent.generated.h"

class AEPPickup;
class UEPItemDefinitionSubsystem;
class UEPCombatComponent;
```

> **`class UEPInventoryComponent;` 전방선언은 필요 없게 만드는 편이 낫다.** `FEPInventoryList::Owner`를 `TObjectPtr<UActorComponent>`로 두면 그 문제가 사라진다 — **Lyra가 그렇게 한다**(`LyraInventoryManagerComponent.h:112-113`). 9차 §11-13에서 권고했는데 아직 구체 타입이다(`EPInventoryComponent.h:18`). **`class`/`struct` 불일치(C4099)도 `TArray<FEPInventoryEntry>`로 쓰면 같이 사라진다** — 전방선언 흉내를 낼 이유가 include로 이미 없어졌다.

---

## 10. §8 실무 조사

### 10-1. 상대 정렬 키를 쓰는 게임 인벤토리 선례 — **로컬 소스로 확인 가능한 범위에는 없다**

Lyra: 순서 필드 0건(요청서 §1-3의 grep 재확인). 엔진: §10-2. **§3-3이 든 절대 위치 사례(타르코프 `location:{x,y,r}` / 마인크래프트 슬롯 인덱스 / WoW bag+slot)는 내가 로컬 소스로 확인할 수 없다** — 웹 접근 없이 확인한 것처럼 적지 않는다.

**그러나 §3-3의 논증은 인용 없이도 선다.**

```
절대 위치는 "자리의 개수가 고정"일 때만 정의된다
우리 컨테이너는 SlotSize 합산이라 자리 개수가 아이템 구성에 따라 변한다 (04-0)
⇒ "3번 자리"가 무엇인지 말할 수 없다
```

**그래서 이건 선례를 못 찾은 문제가 아니라, 조건이 다른 문제다.** 마인크래프트·WoW가 절대 인덱스를 쓸 수 있는 것은 **칸이 고정이고 아이템 하나가 정확히 한 칸**이기 때문이다. 타르코프는 2D 좌표를 쓰는데 그건 아이템이 2D 크기를 갖기 때문이고, 우리는 그 길을 **사용자 확정으로 배제**했다.

> **§3-3이 스스로 던진 질문 — *"그 전제 자체를 다시 봐야 하는가"* — 에 대한 답: 아니다.** 전제(가변 칸)는 `SlotSize` 합산에서 나오고, 그건 2차 확정이다. **가변 칸을 유지하는 한 상대 키가 강제된다.**

### 10-2. UE에서 "복제되는 정렬 키"의 관례 — **없다. 전수 확인**

```
$ grep -rn "public FFastArraySerializerItem" Engine/Source/Runtime --include=*.h
  WorldPartition/HLOD/DestructibleHLODComponent.h:21   FWorldPartitionDestructibleHLODDamagedActorState
  Net/Serialization/FastArraySerializer.h:64           FExampleItemEntry      ← 문서용 예제

$ grep -rn "public FFastArraySerializerItem" Engine/Plugins/Runtime/GameplayAbilities --include=*.h
  GameplayAbilitySpec.h:167     FGameplayAbilitySpec
  GameplayCueInterface.h:101    FActiveGameplayCue
  GameplayEffect.h:1334         FActiveGameplayEffect
  GameplayPrediction.h:570      FReplicatedPredictionKeyItem
```

**엔진 전체에서 `FFastArraySerializerItem` 파생이 여섯이고, 그중 순서 필드를 가진 것이 하나도 없다.** GAS의 넷은 전부 "집합"이다 — 활성 이펙트·부여된 어빌리티·활성 큐·예측 키. **정렬해서 보여줄 대상이 아니다.**

**즉 선례가 없는 것이 아니라 이 문제를 가진 엔진 시스템이 없다.** 9차 §8-1(용량 있는 인벤토리)과 같은 상황이고, 답도 같다 — **원칙으로 정한다.**

> 인접한 UE 관용구를 굳이 대면 **Slate/UMG의 `ZOrder`** 가 "간격을 두고 쓰는 정수 순서 키"다(사람이 0/10/100을 쓴다). **복제되지 않으므로 약한 근거이고, 근거로 쓰지 않는 편이 낫다.**

### 10-3. 재정규화가 실제로 도는가 — **정상 플레이에서는 거의 안 돈다. 그런데 §4 때문에 그게 위험이다**

이분 고갈은 **같은 두 이웃 사이에 16번 연속** 끼워야 한다. 가방을 정리하는 사람은 여러 틈에 흩어 놓으므로 사실상 도달하지 않는다.

**그래서 §4가 중요해진다.** 재정규화가 이분 고갈에만 걸려 있으면 **그 함수는 실질적으로 절대 안 불린다** — 즉 03-9의 완료 조건 *"재정규화가 도는 것을 증명한다"* 를 만족시키려면 `EP.Inv.Reorder`로 **16번 같은 틈에 밀어 넣는 시나리오**를 손으로 만들어야 한다.

**§4의 경계 가드를 넣으면 사정이 달라진다.** 맨 앞/맨 뒤 반복은 커맨드로 쉽게 만들 수 있고(65536씩 이동), **테스트 가능한 트리거가 생긴다.** 즉 §4의 수정은 결함을 고치면서 **완료 조건을 검증 가능하게 만든다.**

> **완료 조건 문안 제안:** *"`EP.Inv.Reorder`로 같은 틈에 16회 삽입 → 재정규화가 돌고 순서가 유지된다"* 와 *"맨 앞으로 반복 이동 → 키가 경계에 닿기 전에 재정규화가 돈다"* 둘. 후자가 현실적으로 훨씬 자주 도는 경로다.

### 10-4. `ULocalPlayerSaveGame` + 서버 발급 ID — **엔진에 방어 장치가 없다**

`ULocalPlayerSaveGame`이 제공하는 것은 **스키마 버전**뿐이다 — `GetLatestDataVersion()`(Lyra가 오버라이드한다, `LyraSettingsShared.h:77`). **"이 데이터가 어느 세션 것인가"를 표현하는 장치가 없다.** 세션 도장을 붙이려면 §2-3처럼 직접 만들어야 하고, 그러면 §2-3의 결론(저장이 no-op)에 도달한다.

**일반 원칙으로 적으면 이것이다.**

> **저장되는 색인의 키는 저장소만큼 오래 살아야 한다.** 짧은 키로 긴 저장소를 색인하면, 실패가 "못 읽는다"가 아니라 **"엉뚱한 것을 읽는다"** 로 나타난다.

**후자가 이 사건의 핵심이다.** 키가 사라졌으면 `Resolve`가 버렸을 텐데, **번호가 재사용되므로 살아 있는 것처럼 보인다.** 10차 답변이 이걸 못 본 것도 같은 이유다 — 저장소의 수명만 보고 키의 수명을 안 봤다.

---

## 11. 권장 작업 순서

**아래는 제안이다. 적용 여부는 사용자가 결정한다.**

| # | 작업 | 대상 | 왜 이 순서인가 |
|---|---|---|---|
| **1** | **★ `SortKey` 스코프 교정** — `NextKeyAtEndOf`가 부모만 보게, `InsertEntry`의 삼항 제거, `:720` 예외 제거, `RenormalizeSortKeys`도 부모 전체 (§3-3) | 03-2 · 03-3 | **정상 플레이에서 동률이 난다.** 그리고 코드가 **줄어든다** |
| **2** | **★ `:820`의 *"겹칠 문법이 없다"* 문장 교체** (§3-3) | 03-2 | 그 문장 때문에 버그를 `AssignSortKey`에서 찾게 된다 |
| **3** | **★ 재정규화 트리거를 세 분기 공통으로** + `NextKeyAtEndOf` 안에 가드 (§4) | 03-2 | 맨 앞/맨 뒤가 **무한 증감**이고 오버플로 후 상태가 영구적이다 |
| **4** | **`Server_ReorderEntry` RPC를 04-B로.** `EP.Inv.Reorder`는 일반 함수 `ReorderEntry`를 부른다 (§6-2) | 03-2 · 03-9 · 04-8 · STATUS | 9차가 `Server_MoveEntry`에 적용한 규칙과 **같은 기준.** 03-A가 가벼워진다 |
| **5** | **03-2 단일 쓰기 지점 표에 `SwapEntries` 한 줄** (§9-1) | 03-2 | 계약은 소유자 문서에. 본문은 04-B 그대로 |
| **6** | **파일 앞부분 블록(전방선언 3개 + include) 한 번** + `Owner`를 `TObjectPtr<UActorComponent>`로 (§9-2) | 03-1 또는 03-2 | **세 라운드 연속 같은 자리에서 걸렸다** |
| **7** | **10차 §2-4 철회를 `LOOT_STATUS.md`에 기록** (§1) | `LOOT_STATUS.md` | *"저장되는 키는 저장소만큼 오래 살아야 한다"* 를 규칙으로 남긴다 |
| **8** | **§3-5-1 근거 교체** — *"조밀이면 재정규화가 사라진다"* → **`RenormalizeSortKeys`가 곧 조밀 재번호다** (§5-1) | 03-1 · `EquipmentSlots.md` §13 | 기각 사유가 틀린 채로 남으면 다음에 다시 올라온다 |
| **9** | **`double` 기각 근거를 고갈 판정으로** (§5-2) | 03-1 | DB/Dump는 부차적이다 |
| **10** | **컨테이너 배열 기각에 9차 일관성 근거 추가** (§5-3) | 03-1 | 같은 기준이 세 번째로 같은 답을 낸다 |
| **11** | **완료 조건 두 개 문안 보강** — 배낭 순서(최소 4개 + 손으로 섞기), 재정규화(맨 앞 반복) (§7-3, §10-3) | 03 완료 조건 | 지금 문안은 **통과했는데 안 고쳐진 상태**를 허용한다 |
| **12** | **일정 단위를 Step → 구간(03-A/B/C)으로** (§8-3) | `LOOT_STATUS.md` · STATUS | 1주는 03-A 분량이다 |
| **13** | `SlotPriority` DT는 **필드 + 배낭 행 하나**만 03-A (§8-3) | 03-1 | 나머지 행은 Step 05에서 채워도 논거가 안 깨진다 |

**하지 않는 것:**

- 클라 로컬 순서로 되돌리기 (§2)
- 세션 GUID 혼합안 (§2-4)
- 조밀 재번호 / `double` / 컨테이너 배열 (§5)
- `MoveEntry`에 4번째 인자 (§6-1)
- 인덱스 기반 재배치 (§6-3)
- `bIsRoot` 대신 별도 플래그 (§7-1)
- **03-D 신설** (§8-1)
- 다중 선택 재배치 (`TArray<int32>`) — 이름만 남긴다
- `SwapEntries` 본문을 03으로 당기기 (§9-1)

---

## 12. 인용 목록

**엔진** — `C:\Program Files\Epic Games\UE_5.7\Engine`

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `Runtime/Net/Core/Private/Net/Serialization/FastArraySerializer.cpp:24-36` | 기반 생성자가 **`SetDeltaSerializationEnabled(true)`를 직접 부른다** — 델타가 기본인 가장 직접적 근거 | §5-4 |
| `Runtime/Net/Core/Classes/Net/Serialization/FastArraySerializer.h:218-221` | *"Delta Serialization for inner structs is now enabled by default"* | §5-4 |
| `…FastArraySerializer.h:1395-1401` | `HasDeltaBeenRequested` ＋ 연결 지원 시 델타 경로 | §5-4 |
| `…FastArraySerializer.h:54` | 클라 배열 순서 **비보장** | §2-5, §7-3 |
| `…FastArraySerializer.h:64` | `FExampleItemEntry` — 문서용 예제 | §10-2 |
| `Runtime/Engine/Private/RepLayout.cpp:4590-4597` | 델타는 **연결이 협상**한다 (`FEngineNetworkCustomVersion::FastArrayDeltaStruct`) | §5-4 |
| `Runtime/Engine/Public/WorldPartition/HLOD/DestructibleHLODComponent.h:21` | Runtime의 유일한 실사용 파생 — **순서 필드 없음** | §10-2 |
| `Plugins/Runtime/GameplayAbilities/…/GameplayAbilitySpec.h:167` · `GameplayCueInterface.h:101` · `GameplayEffect.h:1334` · `GameplayPrediction.h:570` | GAS의 네 파생 — **넷 다 순서 필드 없음** | §10-2 |
| `Runtime/Engine/Classes/GameFramework/SaveGame.h:40-47` | `ULocalPlayerSaveGame` — 로컬 플레이어 연관. **세션 개념 없음** | §2-1, §10-4 |

**Lyra** — `C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame\Source\LyraGame`

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `grep -rn "SortOrder\|SortIndex\|SortKey\|DisplayOrder\|SortPriority"` | **0건** | §10-1, §10-2 |
| `Settings/LyraSettingsShared.h:77` | `GetLatestDataVersion()` — 제공되는 것은 **스키마 버전뿐** | §10-4 |
| `Inventory/LyraInventoryManagerComponent.h:112-113` | `TObjectPtr<UActorComponent> OwnerComponent` — 구체 타입을 안 쓴다 | §9-2 |

**프로젝트**

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `Public/Inventory/EPInventoryComponent.h:107` | `int32 NextEntryId = 1;` — **직독 확인.** 컴포넌트 필드, 비복제 | §2-1 |
| `Public/Inventory/EPInventoryComponent.h:16-18` | `TArray<class FEPInventoryEntry>` ＋ `TObjectPtr<UEPInventoryComponent> Owner` (전방선언 없음) | §9-2 |
| `05_Loot_03_Inventory.md:332` | `GetSortedContents` — *"슬롯에 든 것은 빠진다"* | §3-1 |
| `05_Loot_03_Inventory.md:714, 720` | 재발급은 **부모 변경 시에만**, 슬롯行은 예외 | §3-1 |
| `05_Loot_03_Inventory.md:768, 770, 775-781` | 맨 앞/맨 뒤/이분 — **재정규화가 이분에만** | §4 |
| `05_Loot_03_Inventory.md:811, 820` | `EntryId` 타이브레이크 ＋ *"겹칠 문법이 없다"* | §3-2 |
| `05_Loot_03_Inventory.md:826-833` | `RenormalizeSortKeys` = **순차 배정 루프** | §5-1 |
| `05_Loot_03_Inventory.md:969, 976-979` | `InsertEntry`의 삼항 ＋ `NextKeyAtEndOf` | §3-1, §4 |
| `05_Loot_03_Inventory.md:1249-1250` | `AddSubtree` 자식 키 복원 | §7-2 |
| `05_Loot_03_Inventory.md:739` | 세 RPC 모두 `CanMutateInventory()` 통과 | §6-4 |
| `05_Loot_03_Inventory.md:1639` | `EP.Inv.Reorder` — *"순서 계약을 UI보다 먼저 닫는다"* | §6-2 |
| `05_Loot_03_Inventory.md:29` | 완료 조건 — 배낭 되줍기 순서 보존 | §7-3 |
| `05_Loot_REVIEW10_Answer.md` §2-4 | **철회 대상** — *"대가를 안 치르고 이득만 남는다"* | §1 |
