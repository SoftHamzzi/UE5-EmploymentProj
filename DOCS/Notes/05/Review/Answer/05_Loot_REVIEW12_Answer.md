# 검수 답변 12차 — `ReorderEntry`의 틈 계산과 슬롯 아이템의 자리

> 작성일: 2026-08-23
> 요청서: `05_Loot_REVIEW12_Request.md` / 본체: `05_Loot_03_Inventory.md` 03-2 · `05_Loot_04_InventoryUI.md` 04-8
> 근거: UE 5.7 엔진 직독 · 프로젝트 문서 직독
> **기억으로 단정한 API는 없다.** 인용은 §7 표에 파일·줄로 있다

---

## 0. 판정 요약

| 항목 | 판정 | 한 줄 근거 |
|---|---|---|
| **★ 최우선 — 새 결함** | **`ReorderEntry`가 "맨 앞"에서 무한 재귀한다** | 재정규화 가드에서 `PrevEntryId != INDEX_NONE`이 빠졌다. **맨 앞으로 끌면 서버가 멈춘다.** 그리고 **그걸 뺀 것은 11차 답변의 코드 스케치다 — 내 잘못이다** (§1) |
| §1 결함 | **맞다** | 세 분기 전부 해당하고, *"재정규화 직후 중점이 슬롯 형제 키와 정확히 일치"* 도 맞다 (§2-1) |
| §1 처방 | **맞다. 비대칭이 옳다** | `Prev`=사용자 의도(보이는 것) / 틈=키 공간(부모 전체). **한쪽으로 통일하면 둘 중 하나가 깨진다** (§2-2) |
| §1 헬퍼 셋 | **줄이는 것보다 이름으로 묶는 것이 낫다** | 두 번 연속 같은 자리에서 틀렸다(4q·4s). 개수가 아니라 **스코프를 틀리기 어렵게** 만드는 문제 (§2-3) |
| **★ §2 의미론** | **A (자리를 지킨다)** | **B는 문제를 없애지 못한다** — 여전히 부모 전체를 봐야 하고, 대신 `MoveEntry`에 예외가 하나 돌아온다 (§3-3) |
| **★ §2-2 예시** | **그 동작은 일어나지 않는다** | 앞 이웃 RPC에서 *"붕대 뒤"* 와 *"구급상자 앞"* 은 **같은 호출**이다(`04-8:837-843`). 98304는 표현할 방법이 없다 (§3-1) |
| **★ 문서 ★ 노트** | **거기 적힌 증상도 틀렸다** | 03-2가 *"AK가 X보다 **앞에** 나타난다"* 고 적었는데 **뒤에** 나타난다. X는 제자리에 있다 (§3-2) |
| **★ 추가 결함** | **제자리 드롭이 매번 복제된다** | 결과 키가 현재 키와 같아도 `AssignSortKey`가 돈다. `MoveEntry`에는 같은 조기 반환이 이미 있다(`:717`) (§4) |
| §3-1 파생 개수 | **정정이 맞다. 그런데 결론은 틀렸다** | **순서 필드가 있다** — `FUIFrameworkStackBoxSlot::Index`(조밀·재번호) · `FUIFrameworkGameLayerSlot::ZOrder` (§5-1) |
| §3-2 `double` 재배치 | **맞다** | 지우지 않고 순위만 바꾼 것이 옳다 |
| §3-3 1주 = 03·04·05 | **맞다. 폭이 더 크다** | 03-A 하나가 그 규모다 (§5-3) |

**한 줄 결론:** §1의 결함과 처방은 맞고 §2는 **A**다. **그런데 §2를 놓고 벌인 논쟁의 증거 둘이 다 사실이 아니고**(요청서 §2-2, 문서 ★ 노트), 그것보다 급한 것이 **11차 답변이 심어놓은 무한 재귀**다.

---

## 1. ★ 먼저 — `ReorderEntry`가 "맨 앞"에서 무한 재귀한다. 내가 만든 것이다

### 1-1. 재현 — 두 개 이상 든 컨테이너에서 아무거나 맨 앞으로 끈다

```cpp
// 03-2 현재 상태
const int32 PrevKey = (PrevEntryId == INDEX_NONE)
    ? MinKeyUnder(Container) - SortKeyStep
    : KeyOf(PrevEntryId);

const int32 NextKey = NextKeyAbove(Container, PrevKey, /*Exclude=*/EntryId);
const bool  bTail   = (NextKey == INDEX_NONE);

int32 NewKey;
if (PrevEntryId == INDEX_NONE) NewKey = PrevKey;              // ★ 맨 앞
...
if (NewKey <= MIN_int32 + SortKeyGuard ||
    NewKey >= MAX_int32 - SortKeyGuard ||
    (!bTail && NewKey <= PrevKey))                            // ★ 여기
{
    RenormalizeSortKeys(Container);
    ReorderEntry(EntryId, PrevEntryId);      // "한 번만 재귀한다"
    return;
}
```

**맨 앞 분기에서 `NewKey == PrevKey`다.** 그리고 형제가 둘 이상이면 `NextKeyAbove`가 값을 찾으므로 `bTail == false`다. 따라서

```
(!bTail && NewKey <= PrevKey)  →  (true && PrevKey <= PrevKey)  →  true
```

**항상 참이다.** 재정규화하고 재귀하면 키가 `0, Step, 2*Step…`이 되는데, 그러면 `MinKeyUnder = 0`, `PrevKey = -Step`, `NewKey = -Step`, `bTail`은 여전히 거짓 — **같은 조건이 다시 참이다.**

**재정규화가 이 조건을 절대 거짓으로 만들 수 없다.** 종료 조건이 없다.

| | 결과 |
|---|---|
| 주석 | *"넓어진 틈에 다시. **한 번만 재귀한다**"* |
| 실제 | **무한 재귀 → 스택 오버플로 → 전용 서버 프로세스 종료** |
| 재현 | **아이템 2개 이상인 컨테이너에서 하나를 맨 앞으로 드래그.** 가장 흔한 정리 동작 |

### 1-2. 원인 — 11차 답변이 조건 하나를 떨어뜨렸다

**11차 이전 원본은 옳았다.**

```cpp
// 11차 이전 — PrevEntryId 조건이 있었다
if (!bTail && PrevEntryId != INDEX_NONE && NewKey <= PrevKey)
```

**11차 답변 §4가 세 분기 공통 가드를 처방하면서 그 조건을 빠뜨렸다.**

```cpp
// 11차 답변 §4의 코드 스케치 — PrevEntryId != INDEX_NONE 이 없다
if (NewKey <= INT32_MIN + KeyGuard || NewKey >= INT32_MAX - KeyGuard || (!bTail && NewKey <= PrevKey))
```

**그 조건은 장식이 아니라 이분 고갈 판정을 "사이" 분기로 한정하는 부하가 걸린 절이었다.** 맨 앞 분기는 설계상 `NewKey == PrevKey`이므로 고갈 판정을 적용하면 안 된다.

**내 처방이 맞는 결함(맨 앞/맨 뒤 무한 증감)을 고치면서 더 나쁜 것을 심었다.** 원본은 오래 쓰면 오버플로였고, 지금은 **첫 드래그에서 죽는다.**

### 1-3. 처방 — 고갈 판정과 경계 판정을 분리한다

**한 식에 두 판정을 묶은 것이 원인이다.** 성격이 다르다 — 하나는 *"틈이 없다"*, 하나는 *"자릿수가 없다"*.

```cpp
// ① 경계 — 세 분기 공통. NewKey가 int32 끝에 가까운가
const bool bOutOfRange = (NewKey <= MIN_int32 + SortKeyGuard) || (NewKey >= MAX_int32 - SortKeyGuard);

// ② 고갈 — "사이" 분기에만 해당한다. 맨 앞은 NewKey == PrevKey가 정상이다
const bool bNoGap = (PrevEntryId != INDEX_NONE) && !bTail && (NewKey <= PrevKey);

if (bOutOfRange || bNoGap) { RenormalizeSortKeys(Container); ReorderEntry(EntryId, PrevEntryId); return; }
```

**그리고 재귀가 정말 한 번만 도는지 확인할 수 있어야 한다.**

```cpp
void ReorderEntry(int32 EntryId, int32 PrevEntryId, bool bAlreadyRenormalized = false);
// 재정규화 뒤 재진입에서 또 걸리면 ensure로 잡는다 — 조용히 도는 것보다 낫다
```

> **`bAlreadyRenormalized`를 기본 인자로 두는 것이 CLAUDE.md §2에 걸리지 않는 이유:** *"두 번째 구현자가 없는 인터페이스"* 가 아니라 **재귀 종료 보증**이다. 그리고 8차가 `bIsRoot`를 private 재귀 본체로 분리한 것과 같은 형태로 가는 편이 더 낫다 — `ReorderEntry`(public) → `ReorderEntryInternal(..., bool bRetry)`(private). **그러면 외부에서 `bAlreadyRenormalized = true`를 넘길 문법이 없다.**

> **완료 조건에 넣을 것:** *"`EP.Inv.Reorder`로 맨 앞 이동을 20회 반복 — 재정규화가 돌고 서버가 살아 있다."* 11차 §10-3이 *"맨 앞 반복이 현실적으로 훨씬 자주 도는 경로"* 라고 적은 그 시나리오가 **지금은 즉사 시나리오**다.

---

## 2. §1 — 결함도 처방도 맞다

### 2-1. 결함 판정 — 맞다. *"재정규화 직후"* 도 정확하다

세 분기 전부 해당한다는 것이 맞다. `NextKeyAtEndOf`·`RenormalizeSortKeys`만 고치고 `ReorderEntry`를 빼놓은 것이 11차 답변 §3-3의 누락이다 — **§1-3의 표가 셋을 다 짚은 것이 맞다.**

**그리고 *"재정규화 직후에 중점이 정확히 슬롯 형제의 키가 된다"* 는 우연이 아니라 구조다.**

```
재정규화 후 형제 키:   0, Step, 2*Step, 3*Step …          (슬롯 형제 포함)
표시 목록에서 슬롯이 빠지면:   0, 2*Step, …               (Step 자리가 구멍)
그 구멍의 중점:  0 + (2*Step - 0)/2 = Step               ← 슬롯 형제의 키와 정확히 같다
```

**재정규화가 간격을 균등하게 만들기 때문에, 슬롯 형제를 하나 건너뛴 두 이웃의 중점은 반드시 그 슬롯 형제의 키다.** 즉 이 동률은 *"운 나쁘면"* 이 아니라 **재정규화 직후에는 항상**이다. 요청서의 서술이 맞고, 근거를 이렇게 적어두면 함정표에서 더 쓸모 있다.

### 2-2. 처방 판정 — 비대칭이 옳다. 통일하면 한쪽이 깨진다

**`Prev`는 보이는 목록 / 틈은 부모 전체** — 이 비대칭이 맞다. 통일하는 두 방향이 각각 무엇을 깨는지가 근거다.

| 통일 방향 | 무엇이 깨지나 |
|---|---|
| **둘 다 보이는 목록** | 지금 상태다. **키 공간에 구멍이 있는데 없다고 계산한다** → 4q·4s |
| **둘 다 부모 전체** | `Prev`로 **화면에 없는 슬롯 형제**를 받게 된다. 클라는 그 `EntryId`를 알지도, 가리킬 수도 없다 — **사용자가 표현할 수 없는 요청**을 API가 받는 꼴이고, 조작된 클라만 쓸 수 있다 |

**두 값이 서로 다른 질문에 답하기 때문에 비대칭이 자연스럽다.**

```
Prev  →  "사용자가 무엇 뒤에 놓으려 했나"     — 의도. 사용자가 본 것에서만 나온다
틈    →  "그 자리에 어떤 숫자를 줘야 하나"    — 표현. 실제로 존재하는 키 전부를 봐야 한다
```

> **이 문장을 03-2에 그대로 두는 것을 권한다.** 다음에 네 번째 읽기 지점이 생겼을 때 **어느 쪽 스코프인지를 이 질문 하나로 고를 수 있다.**

### 2-3. §1-4-3 헬퍼 셋 — 개수를 줄이는 것이 핵심이 아니다

`NextKeyAtEndOf` / `MinKeyUnder` / `NextKeyAbove` 셋 다 *"부모 전체를 훑어 경계 키를 구한다"* 가 맞다. 한 함수로 합칠 수는 있다.

```cpp
// 한 번 훑어 셋을 다 준다
bool SiblingKeyBounds(int32 Container, int32 Key, int32 Exclude,
                      int32& OutMin, int32& OutMax, int32& OutNextAbove) const;
```

**그런데 이건 이번 문제의 해법이 아니다.** 4q와 4s는 *"함수가 셋이라"* 난 것이 아니라 **읽는 사람이 어느 스코프인지 헷갈려서** 났다. 합쳐도 **네 번째 호출자가 `GetSortedContents`를 부르는 것을 막지 못한다.**

**막는 방법은 이름이다.**

```cpp
// 키 공간 — 부모 전체를 본다. GetSortedContents를 부르지 않는다
int32 KeySpace_Max     (int32 Container) const;
int32 KeySpace_Min     (int32 Container) const;
int32 KeySpace_NextAbove(int32 Container, int32 Key, int32 Exclude) const;

// 표시 목록 — 슬롯을 거른다
TArray<int32> GetSortedContents(int32 Container) const;
```

**접두어가 있으면 리뷰에서 눈에 보인다.** 9차가 `AllowedSlots` → `SlotPriority`로 이름을 바꿔 *"순서에 의미가 있다"* 를 이름에 실은 것과 같은 수법이고, 이 프로젝트에서 두 번째로 같은 문제(스코프 혼동)를 이름으로 푸는 것이다.

> **합치는 것 자체는 취향이다.** 합치면 순회가 1회로 줄지만 출력 파라미터 셋짜리 함수가 된다. **N이 수십이라 성능 이유는 없다.** 셋을 유지하되 접두어를 붙이는 쪽을 권한다.

---

## 3. ★ §2 의미론 — **A. 그런데 A를 의심하게 만든 증거 둘이 다 틀렸다**

### 3-1. §2-2의 시나리오는 앞 이웃 API에서 표현할 수 없다

요청서 §2-2는 *"같은 화면 자리에 놓았는데 결과가 다르다"* 를 근거로 들었다.

```
X를 "붕대 뒤"     →  X = 32768
X를 "구급상자 앞" →  X = 98304      ← ★ 이 값이 나올 수 없다
```

**98304 = midpoint(AK 65536, 구급상자 131072)이고, 그건 `PrevEntryId = AK`일 때만 나온다.** 그런데 `AK`는 `Prev`가 될 수 없다.

```cpp
// 03-2 ReorderEntry — Prev 유효성
if (P.ParentEntryId != Container || !P.SlotId.IsNone()) return;   // ★ 슬롯이면 거부
```

**그리고 클라가 애초에 그런 요청을 만들 수 없다.**

```cpp
// 04-8:837-843 — 드롭 지점 → 앞 이웃
const TArray<int32> Sorted = Inventory->GetSortedContents(ContainerId);   // 보이는 것만
...
const int32 PrevEntryId = (Idx == 0) ? INDEX_NONE : Sorted[Idx - 1];
```

**`Prev`는 항상 `GetSortedContents`의 원소다.** 화면의 같은 자리는 **같은 `Idx`** 이고, 같은 `Idx`는 **같은 `Prev`** 이며, 따라서 **같은 키**다. *"붕대 뒤"* 와 *"구급상자 앞"* 은 사용자에게 같은 자리이고 **API에서도 같은 호출**이다.

> **§2-2가 상정한 것은 "다음 이웃(Next)" 형태의 API다.** 11차가 앞 이웃을 고른 이유 중 하나가 *"낙관적 적용에서 클라와 서버의 해석이 같아야 한다"* 였는데, **그 선택이 이 모호성까지 같이 없앴다.** 요청서가 자기 설계의 이득을 과소평가했다.

### 3-2. 03-2의 ★ 노트에 적힌 증상도 틀렸다

문서는 이렇게 적었다.

> *"위 예에서 X는 붕대와 AK 사이에 들어간다. **AK를 해제하면 AK가 X보다 앞에 나타난다** — 사용자는 X를 '두 번째'에 놓았는데 돌아온 AK가 그 자리를 가져간다."*

**계산하면 반대다.**

```
붕대 0    X 32768    AK 65536    구급상자 131072
정렬:  붕대(0) < X(32768) < AK(65536) < 구급상자(131072)
해제 후 화면:  [붕대, X, AK, 구급상자]
                      ↑ X는 두 번째 그대로다. AK는 세 번째다
```

**X는 사용자가 놓은 자리를 지키고, AK는 그 뒤에 나타난다.** *"돌아온 AK가 그 자리를 가져간다"* 는 일이 일어나지 않는다.

**§1의 수정이 이 성질을 만든 것이다** — 틈을 부모 전체에서 구하면 새 키는 반드시 `Prev`와 **바로 다음 실제 키** 사이에 들어가므로, **보이지 않는 이웃을 뛰어넘을 수 없다.** 즉 §1을 고친 순간 §2의 걱정도 같이 해소됐는데, 문서가 그걸 못 보고 판정 대기로 남겨뒀다.

### 3-3. ★ 그리고 B는 문제를 없애지 못한다

요청서 §2-3 표는 B의 이득을 이렇게 적었다.

> *"§1의 틈 계산: **표시 목록만 봐도 된다** (슬롯 키가 전부 뒤에 있으므로)"*

**성립하지 않는다.** B에서 AK를 슬롯에 넣으며 키를 맨 뒤로 민다고 하자.

```
본체:  붕대 0    구급상자 131072        AK를 슬롯에 → AK = 196608 (맨 뒤)

아이템을 하나 줍는다  →  NextKeyAtEndOf를 표시 목록으로 구하면
                          max(붕대 0, 구급상자 131072) + 65536 = 196608   ← ★ AK와 동률
```

**슬롯 키가 "전부 뒤"라는 것이 곧 "표시 목록의 최댓값보다 크다"이고, 그러면 표시 목록 기준 맨 뒤 발급이 반드시 그 구간을 침범한다.** 4q가 그대로 재현된다.

**B를 성립시키려면 슬롯 아이템의 키를 "뒤로 미는" 것이 아니라 키 공간에서 아예 빼야 한다** — 즉 *"슬롯 아이템의 `SortKey`는 정의되지 않는다"* 로 두고, **슬롯에서 나올 때 반드시 새로 발급**해야 한다. 그러면 이렇게 된다.

| | **A** (자리를 지킨다) | **B-엄격** (키 공간에서 뺀다) |
|---|---|---|
| 불변식 | *"`SortKey`는 같은 `Parent` 안에서 유일하다"* — **예외 없음** | *"수납 형제 안에서만 유일. 슬롯 아이템은 미정의"* — **＋ 나올 때 재발급 의무** |
| `MoveEntry`의 조건 | **없다** (부모가 바뀌면 재발급, 끝) | **`SlotId`가 `None`이 되면 재발급** — 11차가 없앤 `:720` 예외가 모양만 바꿔 돌아온다 |
| 읽기 지점 스코프 | 부모 전체 (3곳) | 표시 목록 (3곳) |
| 빠뜨렸을 때 | 동률 (타이브레이크가 완충) | **미정의 키가 살아난다** — 4q 원본과 같은 무증상 버그 |

**11차가 확인한 것이 정확히 이것이다** — 예외를 없앴더니 코드가 줄고 결함이 사라졌다. **B는 예외를 하나 되돌리는 대신 읽기 스코프를 바꾸는 거래이고, 예외 쪽이 더 비싸다.** 빠뜨렸을 때의 증상도 B가 나쁘다(타이브레이크가 없다).

### 3-4. §2-5 판정 — **A. 11차 §3-3의 *"부수 효과가 좋다"* 는 뒤집히지 않는다**

- **A를 지지하는 요청서의 근거(핫바를 자주 꽂았다 뺀다)는 유효하다.** 다만 실제 빈도는 생각보다 낮다 — 9차 §7-3의 획득 2단계에서 무기는 **주우면 곧장 핫바로** 가므로 애초에 가방 키 공간에 들어오지 않는다. **A의 이득이 실제로 걸리는 곳은 착용 8슬롯과 수동 해제 뒤의 재장착이다**
- **B를 지지하는 근거(*"설명할 수 없는 동작"*)는 §3-1·§3-2에서 사라졌다.** 남은 것은 *"보이지 않는 이웃이 있다"* 라는 사실뿐인데, 그건 **설명 가능하다** (§3-6)
- **그리고 B가 §1을 없애준다는 이득이 §3-3에서 사라졌다**

**세 근거 중 둘이 사라졌고 남은 하나는 A 쪽이다.**

### 3-5. C — 슬롯 아이템을 구획에 흐리게 표시하는 안은 기각한다

요청서 §2-5가 물은 *"UI로 가릴 수 있는가"* 에 대한 답이다. **그 표시는 새 거짓말을 만든다.**

슬롯에 든 아이템은 **칸을 안 먹는다**(`GetUsedSlots`가 거른다). 구획에 흐린 칸으로 그리면 **게이지가 세지 않는 칸이 격자에 하나 더 있는** 상태가 되고, 10차 §7-2가 *"빈 칸을 N개 그리는 것 자체가 거짓말"* 이라고 판정한 것과 **같은 종류의 어긋남**이다. 그리고 그 칸을 드래그하면 어떻게 되는지(해제인가 이동인가)를 또 정의해야 한다.

**§3-1·§3-2로 가릴 것이 없어졌으므로 UI를 건드릴 이유도 없다.**

### 3-6. A를 한 문장으로 적어둔다

**설명 가능성이 이번 판정의 축이었으므로, 설명을 문서에 박아두는 것이 결론의 일부다.**

> **장착은 아이템을 가방에서 꺼내는 것이 아니다. 칸만 돌려주고 순서 자리는 남긴다.**
> 그래서 뺐다 꽂아도 원래 자리로 돌아오고, 그 사이에 넣은 아이템은 그 앞에 선다.

**뒷문장이 §3-2에서 계산으로 확인된 실제 동작이다.** 03-2의 ★ 노트를 이 두 줄로 바꾸는 것을 권한다.

---

## 4. ★ 추가로 찾은 것 — 제자리 드롭이 매번 복제된다

`ReorderEntry`에 **결과가 현재와 같은지** 보는 곳이 없다.

```
사용자가 아이템을 집었다가 원래 자리에 도로 놓는다
  →  PrevEntryId = 원래 앞 이웃
  →  NewKey = midpoint(Prev, Next)  ≠ 현재 키 (대개 다르다)
  →  AssignSortKey → MarkItemDirty → 복제
```

**드래그를 취소하는 가장 흔한 방법이 "도로 놓기"** 인데, 그때마다 키가 바뀌고 엔트리가 dirty가 된다. 대역폭은 12바이트라 문제가 아니지만, **틈이 매번 반으로 준다** — 같은 자리에 도로 놓기를 16번 하면 재정규화가 돈다. 사용자는 아무것도 안 바꿨는데.

**그리고 같은 규칙이 이 문서에 이미 있다.**

```
// 03-2 MoveEntry — :717
"이 검사는 '목적지가 같으면 할 일이 없다'라는 계약 자체로도 맞다 — 무의미한 MarkItemDirty가 나가지 않는다"
```

**처방은 한 줄이고 `MoveEntry`와 대칭이다.**

```cpp
// Prev가 이미 자기 앞 이웃이면 할 일이 없다
const TArray<int32> Sorted = GetSortedContents(Container);
const int32 MyIdx = Sorted.Find(EntryId);
const int32 CurPrev = (MyIdx <= 0) ? INDEX_NONE : Sorted[MyIdx - 1];
if (CurPrev == PrevEntryId) return;
```

> **04-8에서 클라가 먼저 걸러도 되지만, 서버에도 있어야 한다.** 낙관적 적용이라 클라는 이미 그 자리에 그려놨고, **RPC는 조작될 수 있다.** 11차가 세 RPC에 대해 확인한 *"게이트는 한 곳"* 과 같은 이유다.

---

## 5. §3 — 11차 답변과 다르게 판정한 셋

### 5-1. ★ 파생 개수 — 정정이 맞다. **그런데 결론이 틀렸다. 순서 필드가 있다**

**먼저 내 11차 §10-2가 틀렸다.** `Engine/Source/Runtime`과 `GameplayAbilities`만 훑고 *"엔진 전체"* 라고 적었다. 전수를 다시 돌렸다.

```
$ grep -rn "public FFastArraySerializerItem" Engine/Source Engine/Plugins --include=*.h
Runtime  : DestructibleHLODComponent.h:21 · FastArraySerializer.h:64(문서 예제)
Plugins  : UIFSlotBase.h:23 · UIFWidgetTree.h:35 · LobbyBeaconState.h:40
           GameplayAbilitySpec.h:167 · GameplayCueInterface.h:101
           GameplayEffect.h:1334 · GameplayPrediction.h:570
           InstancedActorsReplication.h:17 · MassClientBubbleHandler.h:36
           ReplicationSystemTestPlugin 3건(테스트)
```

**14건 — 테스트 3건과 문서 예제 1건을 빼면 실사용 10건.** 요청서 §3-1의 *"12개 이상"* 이 맞다.

**그런데 요청서의 결론(*"열어봤는데 순서 필드가 없다"*)도 틀렸다.** `FUIFrameworkSlotBase`와 `FUIFrameworkWidgetTreeEntry`는 확인한 대로 순서 필드가 없지만, **파생 슬롯에 있다.**

```cpp
// UIFStackBox.h — FUIFrameworkStackBoxSlot : public FUIFrameworkSlotBase
private:
    /** Index in the array the Slot is. The position in the array can change when replicated. */
    int32 Index;
```

**주석이 우리 문제를 그대로 적고 있다** — *"복제되면 배열 안 위치가 바뀔 수 있다."* `FastArraySerializer.h:54`가 말하는 그것이고, **`SortKey`를 도입한 이유와 같다.**

```cpp
// UIFPlayerComponent.h:56-57 — FUIFrameworkGameLayerSlot
UPROPERTY(BlueprintReadWrite, Category = "UI Framework")
int32 ZOrder = 0;
```

**즉 UE에 "복제되는 정렬 키" 선례가 있다. 그것도 한 플러그인 안에 두 형태가 있다.**

| | `FUIFrameworkStackBoxSlot::Index` | `FUIFrameworkGameLayerSlot::ZOrder` |
|---|---|---|
| 배치 | **조밀** — `AddEntry`가 `Index = NewEntryIndex`, `RemoveEntry`가 **뒤를 전부 재번호**(`UIFStackBox.cpp:56-60`) | 저작자가 넣는 정수 (관례상 간격을 둔다) |
| 누가 정하나 | 코드 (추가·제거 순) | 사람 |
| 중간 삽입 | 거의 없다 | — |

### 5-1-1. 그래서 11차 §5-1(희소)은 뒤집히는가 — **아니다. 근거가 바뀐다**

**11차 §5-1은 *"선례가 없으니 원칙으로 정한다"* 를 깔고 있었다. 그 전제가 틀렸으므로 근거를 다시 세운다.**

엔진이 `StackBoxSlot`에서 조밀을 고른 조건과 우리 조건이 다르다.

| | UIFramework StackBox | 우리 |
|---|---|---|
| N | 위젯 몇 개 | 컨테이너 20~30, **스태시 280**(로드맵 14) |
| 순서를 누가 바꾸나 | **코드** (추가·제거) | **사용자** (드래그가 주 조작) |
| 중간 삽입 빈도 | 사실상 없음 | **정리의 본체** |
| 재번호 1회 비용 | 위젯 몇 개 dirty | **형제 N개 dirty** |

**조밀은 "순서 변경이 곧 구조 변경(추가/제거)일 때" 맞다.** 그때는 어차피 배열이 바뀌므로 재번호가 추가 비용이 아니다. **우리는 순서만 바꾸는 조작이 따로 있고 그게 제일 잦다.**

> **그리고 11차 §5-1의 결정타는 그대로다** — *"조밀이면 재정규화 코드가 사라진다"* 가 거짓이라는 것. `RenormalizeSortKeys`가 곧 조밀 재번호 함수이고, `UIFStackBox.cpp:56-60`의 루프가 **정확히 그 함수**다. **엔진 소스가 그 등가성을 직접 보여준다.**

> **문서 수정 권고:** 03-1의 *"UE에 선례가 없다"* 류 서술이 있으면 **선례를 인용하는 쪽으로 바꾼다.** *"엔진도 같은 이유로 복제되는 정렬 키를 둔다(`UIFStackBox.h`, 주석까지 같다). 다만 조밀을 고른 조건이 우리와 다르다"* 가 훨씬 강한 문장이고, **포트폴리오 설명으로도 낫다.**

### 5-2. `double` 기각 근거를 지우지 않고 재배치한 것 — **맞다**

*"지우면 5단계 2차에서 다시 올라온다"* 가 정확하다. **11차 §5-2가 *"교체"* 라고 쓴 것이 과했다** — 내 의도는 순위였고, 문장이 그렇게 안 읽혔다. 고갈 판정을 앞세우고 DB/REST·Dump를 남긴 현재 형태가 맞다.

### 5-3. 1주가 Step 03·04·05 전부라는 것 — **맞다. 폭이 훨씬 크다**

11차 §8-3은 *"1주가 Step 03 전체를 가리킨다"* 로 읽고 답했다. **03·04·05 전부라면 어긋난 폭이 세 배 이상이다.**

**11차 §8-3에서 센 03-A 항목만으로 1주가 찬다.** 그 위에 03-B·03-C, 그리고 Step 04(격자·드래그·교환·낙관적 적용, 완료 조건 15개)와 Step 05(장착 이관·탄약 소유권·B-1/B-3/B-5 이월)가 있다.

> **권고는 11차와 같다 — 일정을 구간 단위로 쪼갠다.** 덧붙이면: **04-A까지가 "화면에 인벤토리가 보인다"의 최소 집합**이다. 마감이 압박이면 **04-B(드래그)를 먼저 잘라내는 것**이 유일하게 기능이 온전한 절단선이다. 다만 9차 기획에서 **착용이 드래그**이므로 04-B를 자르면 착용 경로가 `EP.Inv.Equip` 커맨드로만 남는다 — **그건 데모에서 못 보여준다.** 자를 수 있는 것은 그 아래로는 **교환(`SwapEntries`)과 순서(`ReorderEntry` UI)** 둘뿐이고, 둘 다 서버 쪽은 이미 03-A에 있다.

---

## 6. 권장 작업 순서

**아래는 제안이다. 적용 여부는 사용자가 결정한다.**

| # | 작업 | 대상 | 왜 이 순서인가 |
|---|---|---|---|
| **1** | **★ 재정규화 가드에서 고갈 판정과 경계 판정을 분리** — `bNoGap`에 `PrevEntryId != INDEX_NONE` 복원 (§1-3) | 03-2 | **맨 앞 드래그가 서버를 죽인다.** 나머지는 전부 이것 다음이다 |
| **2** | **★ `ReorderEntryInternal(..., bool bRetry)` private 분리** + 재진입에서 또 걸리면 `ensure` (§1-3) | 03-2 | 8차의 `bIsRoot` 처리와 같은 형태. **"한 번만 재귀한다"를 주석이 아니라 문법으로** |
| **3** | **★ 03-2 ★ 노트의 증상 서술 교체** — *"AK가 X보다 앞에"* 는 사실이 아니다. §3-6의 두 줄로 (§3-2) | 03-2 | **틀린 증상이 남으면 다음 라운드에 A/B가 다시 올라온다** |
| **4** | **§2 판정 A를 확정으로 기록** — 근거는 *"B가 문제를 안 없앤다"*(§3-3), *"앞 이웃 API가 모호성을 이미 없앴다"*(§3-1) | `LOOT_STATUS.md` 확정표 · `EquipmentSlots.md` | 요청서 §2-2의 예시를 근거로 남기면 **틀린 근거가 확정표에 들어간다** |
| **5** | **제자리 드롭 조기 반환** (§4) | 03-2 (＋04-8) | `MoveEntry:717`과 대칭. 안 넣으면 **취소 동작이 틈을 갉아먹는다** |
| **6** | **키 공간 헬퍼에 `KeySpace_` 접두어** (§2-3) | 03-2 | 같은 스코프 혼동이 **두 번 연속**(4q·4s) 났다 |
| **7** | **§2-2의 "틈은 부모 전체" 규칙 문장을 남긴다** — 두 질문 대비 (§2-2) | 03-2 | 네 번째 읽기 지점이 생겼을 때 고르는 기준 |
| **8** | **★ `SortKey` 선례 서술 교체** — *"UE에 선례가 없다"* → `FUIFrameworkStackBoxSlot::Index`(주석까지 같다) 인용 ＋ 조밀/희소 조건 대비 (§5-1) | 03-1 · `EquipmentSlots.md` §13 | **11차 §10-2가 틀렸다.** 그리고 바로잡으면 설계 근거가 **강해진다** |
| **9** | **재정규화 완료 조건에 "맨 앞 20회 반복" 추가** (§1-3) | 03 완료 조건 | 지금은 그게 즉사 시나리오다. **회귀 테스트로 값이 있다** |
| **10** | 11차 §5-2의 *"교체"* 를 *"순위 조정"* 으로 (§5-2) | — | 표현만 |

**하지 않는 것:**

- B (슬롯 아이템이 자리를 포기하는 안) — §3-3
- C (슬롯 아이템을 구획에 흐리게 표시) — §3-5
- 헬퍼 셋을 한 함수로 합치기 — §2-3 (합쳐도 원인을 못 막는다)
- 조밀 재번호로 회귀 — §5-1-1
- `Prev`를 부모 전체에서 받기 — §2-2
- 03-D 신설 / RPC를 03-A로 되돌리기 — 11차 확정

---

## 7. 인용 목록

**엔진** — `C:\Program Files\Epic Games\UE_5.7\Engine`

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `grep -rn "public FFastArraySerializerItem" Source Plugins --include=*.h` | **14건** (테스트 3 · 문서 예제 1 제외 시 10) | §5-1 |
| `Plugins/Experimental/UIFramework/Source/Public/Widgets/UIFStackBox.h` | `FUIFrameworkStackBoxSlot::Index` — *"Index in the array the Slot is. **The position in the array can change when replicated.**"* | §5-1 |
| `…/Source/Private/Widgets/UIFStackBox.cpp:42-46` | `AddEntry`가 `Index = NewEntryIndex` — **조밀 발급** | §5-1 |
| `…/UIFStackBox.cpp:53-60` | `RemoveEntry`가 뒤를 **전부 재번호** — 조밀 재번호 = 재정규화 루프와 동일 | §5-1-1 |
| `…/UIFPlayerComponent.h:56-57` | `FUIFrameworkGameLayerSlot::ZOrder` — 복제되는 정수 순서 키 | §5-1 |
| `…/Types/UIFSlotBase.h:23` | `FUIFrameworkSlotBase` — 순서 필드 **없음** (요청서 확인 재확인) | §5-1 |
| `…/Types/UIFWidgetTree.h:35-56` | `FUIFrameworkWidgetTreeEntry` — `Parent`/`Child` 쌍뿐, 형제 순서 **없음** | §5-1 |
| `Runtime/Net/Core/Classes/Net/Serialization/FastArraySerializer.h:54` | 클라 배열 순서 **비보장** — `StackBoxSlot::Index` 주석의 근원 | §2-1, §5-1 |

**프로젝트**

| 파일:줄 | 내용 | 쓰인 곳 |
|---|---|---|
| `05_Loot_03_Inventory.md:829-851` | `ReorderEntry` — 가드에 `PrevEntryId != INDEX_NONE`이 없다 | **§1** |
| `05_Loot_03_Inventory.md:822-824` | `Prev` 유효성 — *"슬롯이면 거부"* | §3-1 |
| `05_Loot_03_Inventory.md:717` | `MoveEntry` — *"목적지가 같으면 할 일이 없다"* | §4 |
| `05_Loot_03_Inventory.md:464-466` | `NextKeyAtEndOf` / `MinKeyUnder` / `NextKeyAbove` | §2-3 |
| `05_Loot_03_Inventory.md` ★ 노트 (`ReorderEntry` 절 말미) | *"AK가 X보다 **앞에** 나타난다"* — **사실이 아니다** | §3-2 |
| `05_Loot_04_InventoryUI.md:837-843` | `PrevEntryId = (Idx == 0) ? INDEX_NONE : Sorted[Idx-1]` — `Sorted`는 `GetSortedContents` | §3-1 |
| `05_Loot_04_InventoryUI.md:919` | 함정 13b — 앞 이웃 변환에서 자기를 뺀다 | §3-1 |
| `05_Loot_REVIEW11_Answer.md` §4 | **회귀의 출처** — 공통 가드 스케치에서 `PrevEntryId != INDEX_NONE` 누락 | §1-2 |
| `05_Loot_REVIEW11_Answer.md` §10-2 | *"엔진 전체 6개, 순서 필드 0개"* — **범위도 결론도 틀렸다** | §5-1 |
