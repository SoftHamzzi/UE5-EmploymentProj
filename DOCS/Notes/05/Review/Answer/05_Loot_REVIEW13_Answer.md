# 검수 답변 13차 — 자체 검수 21건의 판정과 결정 5건

> 작성일: 2026-08-25
> 요청: `05_Loot_REVIEW13_Request.md` ＋ 본체 `Review/05_Loot_REVIEW_Inventory.md`
> 대조: `05_Loot_03_Inventory.md`(2343줄) · `_STATUS.md`(217줄) · `05_Loot_DOCS.md` · `05_Loot_00_ItemCore.md` · `05_Loot_04_InventoryUI.md` · `GAME.md` · **소스 3종 직독** · 엔진 5.7 · Lyra
> 성격: 이번 요청은 *"결함을 찾아달라"* 가 아니라 **"자체 검수 21건의 판정과 그 안의 결정 5건이 맞나"** 다. 그래서 §1·§2가 본론이고 §4는 부록이 아니라 **같은 무게**로 본다

---

## 0. 판정 요약

| | 무엇 | 판정 | 한 줄 |
|---|---|---|---|
| **§1-①** | A-1 `MoveEntry`가 복사본에 쓴다 | ✅ **결함이 맞다** | 그리고 *"무해한 임시 객체"* 가 아니다 — `MarkItemDirty`가 **`MarkArrayDirty`를 부르고 `IDCounter`를 영구히 소비한다** |
| **§1-②** | A-2 루트 스냅샷의 `SlotId` | ✅ **결함이 맞다** | 세 줄 표 전부 확인. 처방(A안)도 맞다 |
| **§1-③** | B-1 `INDEX_NONE` 센티널 | ✅ **결함이 맞고 "계약"이다** | −1 도달 경로 재계산 확인(**3개·16회면 난다**). 도달 불가 방어가 아니다 |
| **§2-①** | `AddSubtree(Parent, SlotId, In)` | ✅ **맞다** | 단 **구조체 걱정은 근거가 없다**(§7-1이 인자를 안 늘린다) ＋ **구멍이 남았다**(슬롯 인자에 검증이 0) |
| **§2-②** | 루트가 `SlotId`를 버린다 (A안) | ✅ **맞다** | "지금 안 만든다"도 맞다. 단 **§2 규칙 해석을 한 줄 고쳐야 한다** |
| **§2-③** | 가드를 단일 쓰기 지점 표에 / 래퍼 기각 | ✅ **결론 맞다, ❌ 근거 틀렸다** | **엔진이 `MarkItemDirty`를 감싼다** — `UAbilitySystemComponent::MarkAbilitySpecDirty` |
| **§2-④** | 용량표 · 부등호 둘 | ✅ **유지** | 단 근거가 *"익스플로잇"* 이 아니다. 그리고 **`SlotSize >= 1` 검증이 없다 — 본체 0칸이 그걸 구멍으로 만든다** |
| **§2-⑤** | `StartingEquipment` in DeveloperSettings | ✅ **맞다** | 단 **6차와 같은 자리가 아니다.** 근거가 다르고, 그 차이가 다음 필드를 좌우한다 |
| **§3** | ㉯ `TryAutoEquip`을 03-C로 | ✅ **맞다** | 그리고 **03-B는 합치는 게 아니라 없애는 게 맞다** — `Server_EquipBackpack`은 Step 03에 호출자가 **0개**다 |
| **§4** | 놓친 것 | **12건** | 아래 |

> **한 줄 결론:** 21건의 판정은 전부 맞다. **결정 5건도 전부 맞다.** 그런데 그중 셋(③④⑤)은 **결론이 맞고 근거가 틀렸거나 자리를 잘못 짚었고**, 근거가 틀린 채로 남으면 **다음 결정이 그걸 인용해서 틀린다.** 그리고 §4에서 **같은 종류(B-2)가 다른 함수에 하나 더 있었다.**

---

## 1. 자체 검수 21건 — 판정

### 1-①. A-1 — 결함이 맞다. 그리고 요청서가 증상을 **좋게** 적었다

**질문:** *"문서만 보고 구현하면 이 코드가 나오나, 아니면 '당연히 참조를 잡지'라고 볼 사안인가."*

**나온다.** 근거 셋이고, 셋이 서로를 강화한다.

| | 왜 그 코드가 나오나 |
|---|---|
| ① | 초안이 `E`를 **선언 없이** 썼고, 바로 위 검사 6이 `FEPInventoryEntry E;`를 **같은 이름으로** 만들었다. 이어 붙이면 컴파일된다 — 컴파일러가 물어보지 않는다 |
| ② | 이 문서가 **두 곳에서** *"`FindEntry`는 값 복사"*, *"엔트리 포인터를 밖으로 내보내지 않는다"* 고 못박았다. 그러면 구현자가 아는 조회 수단은 `FindEntry` 하나다 — **가변 참조를 얻는 유일한 합법 경로(`Entries.Items` 직접 순회)가 이 함수의 절에 안 적혀 있었다** |
| ③ | `SetEntryCharges`가 그 형태를 쓰지만 **03-3에 있다.** 구현자는 03-2를 보며 `MoveEntry`를 쓴다 |

**②가 결정적이다.** *"당연히 참조를 잡지"* 가 성립하려면 참조를 잡는 것이 이 문서에서 **허용된 동작**이어야 하는데, 이 문서는 정확히 그 반대를 세 번 말한다. **금지 규칙이 실수를 유도한 것**이라 구현자 탓이 아니다.

#### ★ 증상 서술을 고쳐야 한다 — 임시 객체에 쓰는 것이 무해하지 않다

요청서와 검토 문서가 *"`MarkItemDirty`는 배열 밖 임시 객체의 `ReplicationID`를 건드린다"* 로 적었다. **그것보다 나쁘다.**

```cpp
// FastArraySerializer.h:441-454
void MarkItemDirty(FFastArraySerializerItem & Item)
{
    if (Item.ReplicationID == INDEX_NONE) { Item.ReplicationID = ++IDCounter; ... }
    Item.ReplicationKey++;
    MarkArrayDirty();            // ★ 여기
}

// FastArraySerializer.h:457-465
void MarkArrayDirty()
{
    ItemMap.Reset();
    IncrementArrayReplicationKey();
    CachedNumItems = INDEX_NONE;
    CachedNumItemsToConsiderForWriting = INDEX_NONE;
}
```

그리고 **복사본의 `ReplicationID`는 `INDEX_NONE`이다** — `FFastArraySerializerItem`의 복사 생성자와 대입 연산자가 셋을 전부 리셋한다(`FastArraySerializer.h:307-322`). 이 문서가 03-1에서 이미 적어둔 성질(*"복사는 복제 ID를 승계하지 않는다"*)이 **여기서 나쁜 쪽으로 작동한다.**

| 호출마다 실제로 일어나는 일 | 결과 |
|---|---|
| `IDCounter`가 **하나 소진된다** (복사본은 항상 `INDEX_NONE`이므로 매번) | 영구 누수. 매치 안에서 무해하지만 *"아무 일도 안 일어났다"* 는 거짓 |
| `ItemMap.Reset()` ＋ `ArrayReplicationKey++` | **다음 쓰기에서 배열 전체 델타 재스캔**이 돈다 |
| 배열 원소는 안 바뀜 | 재스캔 결과 **보낼 것이 없다** |

**즉 "무동작인데 `true`"에 더해 "무동작인데 네트워크 일은 한다"** 가 된다. 진단이 더 어려워지는 방향이다 — 프로파일러에 FastArray 직렬화가 잡히므로 *"복제는 도는데 값이 안 온다"* 로 읽혀 **복제 쪽을 판다.** 함정 4v의 증상 칸에 이 한 줄을 넣는 것을 권한다.

> **처방은 그대로 맞다.** `for (FEPInventoryEntry& E : Entries.Items)` ＋ 검사 6의 변수를 `Cur`로. **문서에 이미 반영돼 있다**(`:876-905`). 추가로 03-2의 *"엔트리 포인터를 밖으로 내보내지 않는다"* 옆에 **"안에서는 참조로 쓴다"** 를 한 줄 붙이면 ②가 닫힌다.

---

### 1-②. A-2 — 결함이 맞다. 세 줄 다 확인했다

| 목적지 | 확인 |
|---|---|
| 본체 | ✅ `Parent=-1, SlotId="Back"` → `GetEntryInSlot(-1,"Back")`이 잡는다. **완료 조건 9가 통과해 버린다**는 지적이 정확하다 |
| 이미 다른 배낭 | ✅ `GetEntryInSlot`이 **첫 번째만** 돌려주므로 둘째는 존재하되 보이지 않는다. `GetUsedSlots`도 건너뛴다(`SlotId != None`) → **칸도 안 먹고 안 보인다** |
| 배낭 안 | ✅ 함정 4i의 정의 그대로 |

**그리고 *"내용물 유무로 갈린다"* 도 맞다.** `In.Num() == 1 → AddItem(Container, ItemId, State)`은 `SlotId` 인자가 없어 `NAME_None`으로 들어간다. **빈 배낭은 안 매지고 내용물이 든 배낭만 매지는** 상태였다.

**우회 경로가 문서 안에 있었다는 진단이 이 건의 핵심이고, 그게 맞다.** 9차가 검사 3·4를 `MoveEntry`에 넣으면서 *"슬롯을 채우는 유일한 지점"* 을 만들었다고 적었는데, `AddSubtree`가 `InsertEntry`를 직접 부르므로 **그 문장이 그때부터 거짓이었다.**

> **한 가지 더 확인했다 — 자식은 `SlotId`를 보존해도 안전하다.** 검사 3이 몸 슬롯에 `ParentEntryId == INDEX_NONE`을 요구하므로 **자식이 들 수 있는 `SlotId`는 부착 슬롯뿐**이고, 그건 보존이 맞다(조준경 달린 총). 루트만 버리는 비대칭이 우연이 아니라 **검사 3에서 유도된다.** 이 근거를 03-2의 `bIsRoot` 주석에 한 줄 넣으면 *"왜 자식은 안 버리나"* 가 다시 안 올라온다.

---

### 1-③. B-1 — 결함이 맞다. **계약이지 도달 불가 방어가 아니다**

**질문 둘 다 답한다.**

#### ⓐ −1 도달 경로 — 맞다. 그리고 **아이템 3개면 난다**

요청서는 *"(−65536, 0) 구간에 16회 꽂으면"* 이라고만 적었는데, **16개의 아이템이 필요한 것으로 읽힌다.** 아니다.

```
초기:  A(0)  B(65536)
① B를 맨 앞으로       →  B = KeySpace_Min − Step = −65536
② C를 B 뒤로          →  −65536 + (0−(−65536))/2 = −32768
③ B를 C 뒤로          →  −32768 + (0−(−32768))/2 = −16384
④ C를 B 뒤로          →  −8192
   …  B와 C를 번갈아 서로 뒤로 보낸다
⑯                     →  −1
```

**아이템 3개(A·B·C)와 커맨드 16회다.** `EP.Inv.Reorder`로 손으로 재현되고, **완료 조건 18 ①(*"같은 틈에 16회"*)을 음수 구간에서 돌리면 그대로 나온다.** 요청서의 계산은 맞고, **재현 난이도가 요청서가 생각한 것보다 훨씬 낮다.**

**그 다음도 확인했다.** 키가 −1인 형제를 `KeySpace_NextAbove`가 찾아 −1을 돌려주면 호출자는 `bTail = true`로 읽고 `NewKey = PrevKey + SortKeyStep`을 쓴다. `PrevKey < −1`이므로 **−1을 뛰어넘어 0 이상으로 착지하고**, 거기 A(0)가 있으면 **동률**이다. 그리고 `bNoGap`은 `bTail`일 때 꺼지므로 재정규화도 안 걸린다 — **요청서 서술 그대로다.**

#### ⓑ 계약이다. CLAUDE.md §2가 금지한 것과 **반대편**이다

§2가 금지한 것은 *"도달 불가한 분기의 에러 처리"* 다. 여기는 셋 다 다르다.

| §2가 금지한 것 | B-1 |
|---|---|
| 분기를 **추가**한다 | **분기가 안 는다** — `if (x == INDEX_NONE)`이 `if (!Func(...))`로 **바뀔 뿐**이다 |
| 도달 불가한 상태를 방어한다 | **16회로 도달한다.** 완료 조건이 그 경로를 직접 돌린다 |
| 나중에 넣어도 싸다 | **비싸다** — 반환 타입이 바뀌면 **읽기 지점 전부**가 바뀐다. §2의 *"나중에 넣기 비싼 것 — 계약(반환 규약)"* 예시 그대로다 |

**그리고 이건 사이클 검사(검사 6)와 같은 부류가 아니라 더 강하다.** 검사 6은 *"데이터가 트리가 아니면"* 이라는 **가정 위반**을 막지만, B-1은 **정상 데이터에서 함수가 거짓말한다.**

> **`KeyOf`의 *"없으면 0"* 도 같이 고치는 것이 맞다.** 0은 재정규화 직후 첫 형제의 키다. 지금 도달하지 않는 이유가 *"모든 호출부가 앞에서 `FindEntry`로 확인한다"* 인데, **그건 규율이고 이 문서가 규율을 신뢰하지 않기로 세 번 결정했다.**

#### ⓒ 처방의 형태 — `bool` ＋ out이 맞다

엔진에 선례가 둘 다 있다.

| 형태 | 선례 |
|---|---|
| **`bool F(..., int32& Out)`** | `FParse::Value(const TCHAR*, const TCHAR*, int32& Value)` — `Parse.h:60`. **`int32` 값과 "없음"을 분리하는 Core의 표준형** |
| `TOptional<int32>` | `AGameSession::MaxPlayersOptionOverride` / `MaxSpectatorsOptionOverride` — `GameSession.h:265-268` |

**`bool` ＋ out을 권한다.** `TOptional`은 `.IsSet()`과 `.GetValue()`가 **두 단계**라 *"값을 먼저 읽는다"* 는 같은 실수를 다시 허용하고, 여기 호출부는 이미 `if (!KeyOf(Prev, PrevKey)) return;` 형태라 **한 단계로 끝난다.** 문서에 반영된 시그니처(`:534-536`)가 그대로 맞다.

> **`KeySpace_Min`만 `int32`로 남는 것도 확인했다.** 호출부에 **자기 자신이 반드시 있어서** 비지 않는다 — 문서가 그 이유를 선언 옆에 적어뒀다(`:531`). **이유가 적혀 있으므로 비일관이 아니다.** 이유를 지우면 다음 사람이 셋 다 `bool`로 바꾸거나 셋 다 되돌린다.

#### ⓓ 수치 하나 — 완료 조건 18의 "32,767회"는 **32,763회**다

C-2가 맞게 잡은 지적인데 숫자가 4 어긋난다. `SortKeyGuard = 4 × Step`이므로 경계는 `MIN_int32 + 262,144`이고,

```
65,536 × 32,762 = 2,147,213,312   ← 아직 안 걸린다
65,536 × 32,763 = 2,147,278,848   ← 여기서 bOutOfRange
```

**결론은 하나도 안 바뀐다**(20회로는 못 간다). 문서에 숫자를 적을 거면 맞는 값을 적는 게 낫다.

---

### 1-2. 나머지 18건 — 전부 확인함

| # | 판정 | 비고 |
|---|---|---|
| A-3 | ✅ | 본체 경유가 `CanFit`을 지나는 것이 맞고, **본체 0칸에서 영구 실패**도 맞다 |
| A-4 | ✅ | **단 지금 코드는 컴파일된다** — 헤더가 아직 `TObjectPtr<UEPInventoryComponent> Owner`다(`EPInventoryComponent.h:22`). 결함은 **11차 결정을 적용하는 순간** 나타난다. 순서가 중요하다: `Owner` 타입과 `Cast`는 **같은 커밋**이어야 한다 |
| A-5 | ✅ | 목록이 규칙을 안 지킨 것이 맞다 |
| B-2 | ✅ | 그리고 **같은 모양이 `MoveEntry`에 하나 더 있다** → §4 N-1 |
| B-3 | ✅ | `CanMutateInventory()`를 *"모든 `Server_*`의 첫 줄"* 로 다시 쓴 것이 맞다 — `Server_ReorderEntry`(04-8 `:801`)가 이미 그 형태다 |
| B-4 | ✅ | 소스 `EPInventoryComponent.cpp:109`에 그대로 있다. §7-1이 오기 전에 고치는 게 맞다 |
| C-1 | ✅ | **소스·문서 전부 확인 — `MoveEntry` 호출자가 0개다.** `EP.Inv.Equip`은 04-A 소속이 맞다(`05_Loot_04_InventoryUI.md:45`) |
| C-2 | ✅ | 위 ⓓ의 수치만 |
| C-3 | ✅ | STATUS가 **19행으로 갱신됐다** — 확인함(`_STATUS.md:109-129`) |
| C-4 | ✅ | §3에서 다룬다 |
| D-1 | ✅ | 헤더 `:76`에 매크로 없음 확인. **단 처방이 하나가 아니다** → §4 N-3 |
| D-2 | ✅ | 반영 확인 |
| D-3 | ✅ | 꼬리 주석이 3b 행과 일치하게 교체됨(`:2258` 부근) |
| D-4 | ✅ | `:880`이 *"(`05_Loot_04_InventoryUI.md` 함정 11b)"* 로 출처를 밝히게 바뀜 |
| D-5 | ✅ | STATUS 결함 #2가 11차 결정으로 교체됨(`_STATUS.md:24`) |
| D-6 | ✅ | **번호순 재정렬 확인** — `1·2·3·3b~3g·4·4b~4z·5·6·6b~6d·7·8·8b·9·9b~9f·10·10b·10c`. `4a`는 여전히 없는데 **번호 결번은 색인 기능을 안 해친다** (`3e`도 비어 있다가 채워졌다). 두지 마라 |
| D-7 | ✅ | *"부모별로 묶고 그 안에서 `SortKey` 순"* 이 맞다 |
| D-8 | ✅ | 문서 3종 갱신 확인. **에셋만 남았다** |

---

## 2. ★ 사용자 결정 5건

### 2-①. `AddSubtree(int32 Parent, FName SlotId, const TArray&)` — **맞다**

#### 근거 교정도 맞다

*"인자를 하나 늘린다"* 에서 *"빠진 걸 채운다"* 로 바꾼 것이 옳다. 셋 중 둘이 이미 쌍을 받고 있었고, **결함 둘(A-2·A-3)이 같은 구멍에서 나왔다**는 관찰이 그 근거를 증명한다. 한쪽은 값이 **몰래 새어 들어왔고**(스냅샷) 다른 쪽은 **표현이 안 됐다** — 그건 *"인자가 하나 모자란다"* 의 정확한 두 증상이다.

**기본값 금지도 맞다.** 이 문서의 기준(*"규율이 아니라 형태로 막는다"*)이 여기서는 명시 강제다. 호출부가 셋뿐이라 비용이 없다.

#### ★ 구조체 걱정은 **근거가 없다** — 문서 자신의 §7-1이 인자를 안 늘린다

> *"§7-1의 `AActor* Source`가 넷째로 붙을 날을 생각하면 지금 구조체로 가야 하나."*

**그 날은 안 온다.** 이 문서가 §7-1을 어떻게 적었는지 보면 된다.

```
Container->RemoveEntry(Id, &Sub)   →   MyInv->AddSubtree(INDEX_NONE, NAME_None, Sub)
                                                          ← 03-2 :1227-1229
```

**출처는 `AddSubtree`의 인자가 아니라 "어느 컴포넌트에서 `RemoveEntry`를 부르나"다.** 월드 컨테이너는 **자기 인벤토리 컴포넌트를 갖고**, 꺼내기는 그 컴포넌트에서 빼서 내 컴포넌트에 넣는 **두 함수 호출**이다. `Source`가 인자로 들어올 자리가 없다.

**즉 넷째 인자는 문서에 이름이 없다.** CLAUDE.md §2 기준으로 **상상한 확장점**이고, 구조체는 만들지 않는 것이 맞다 — *"호출부 셋이 전부 리터럴을 넘긴다"* 는 이유보다 이쪽이 강하다.

#### ★★ 그런데 구멍이 하나 남았다 — **슬롯 인자에 검증이 0이다**

이 결정이 세운 문장은 이것이다.

> *"슬롯을 채우는 경로가 `MoveEntry`와 `TryAutoEquip` **둘로 고정되고 둘 다 빈자리를 먼저 확인한다**."*

**API 수준에서는 거짓이다.** `AddSubtree`는 **public**이고(`EPInventoryComponent.h:51`), `SlotId`를 받아 `InsertEntry`에 그대로 넘기며, **검사 2(SlotPriority)·3(정합)·4(중복)를 하나도 안 한다.** 빈자리 확인은 `TryAutoEquip`이라는 **호출자**에 있다.

**그리고 문서가 이미 세 번째 호출자를 예고했다.**

```cpp
// 03-4 :1700 — §7-3 부착물
AddSubtree(총Id, "Optic", Payload);      // ← TryAutoEquip이 아니다
```

이 호출은 *"그 무기에 Optic 슬롯이 있는가"*(검사 3)도 *"이미 조준경이 달렸는가"*(검사 4)도 안 본다. **A-2가 막은 우회 경로가 §7-3에서 문법 그대로 되살아난다.**

**처방 — 검사 2·3·4를 판정 하나로 뽑는다.**

```cpp
// 세 검사는 전부 (Parent, SlotId, ItemId)의 함수다. 옮기는 엔트리를 안 본다
bool CanPlaceInSlot(int32 Parent, FName SlotId, FName ItemId) const;
```

| 부르는 곳 | 지금 | 바뀌면 |
|---|---|---|
| `MoveEntry` | 검사 2·3·4를 인라인 | `if (!NewSlotId.IsNone() && !CanPlaceInSlot(NewParent, NewSlotId, E.ItemId)) return false;` |
| `AddSubtree` | **없다** | `if (!SlotId.IsNone() && !CanPlaceInSlot(Parent, SlotId, In[0].ItemId)) return INDEX_NONE;` |
| §7-3 부착물 | — | 자동으로 옳다 |

**이건 새 계층이 아니라 이 문서가 이미 쓴 패턴의 세 번째 적용이다.** 문서가 `CanFit`에 대해 정확히 같은 말을 했다.

> *"칸 판정을 `AddItem`에 인라인으로 다시 쓰지 않는다. `CanFit`은 `AddSubtree`와 상호작용 프롬프트도 쓰므로 **판정식이 세 곳에 흩어지면 반드시 어긋난다.**"* (03-3 `:1364`)

**슬롯 판정도 소비자가 셋이고, 지금 세 곳에 흩어질 참이다.** 그리고 `AddSubtree`가 이미 *"칸 검사 분기가 `MoveEntry` 검사 5와 한 글자도 다르지 않다"* 고 적었는데(`:1702`), **한 글자도 다르지 않은 것을 두 번 쓰는 것이 바로 이 문서가 금지한 것이다.**

> **`CanFit`을 왜 안 합치나 — 합치면 안 된다.** `CanFit`은 *"수납일 때"*, `CanPlaceInSlot`은 *"슬롯일 때"* 로 **배타적**이다. 하나로 묶으면 `SlotId`의 `None` 여부로 갈리는 분기가 판정 함수 안으로 들어가고, 그건 12차가 `KeySpace_` 접두어로 막은 것과 같은 종류의 혼동을 만든다.

---

### 2-②. 루트 스냅샷이 `SlotId`를 버린다 (A안) — **맞다**

#### A안이 맞는 이유는 "동작이 자연스럽다"가 아니다

사용자 판정(*"Back이 비어 있으면 매지고, 차 있으면 아이템으로 들어가고, 아이템 자리도 없으면 못 줍는다"*)이 **이미 있는 흐름의 세 갈래를 정확히 열거한 것**이 결정적이다. 세 갈래가 03-4의 ①`TryAutoEquip` → ②`GetInsertionOrder` **두 단계에서 그냥 나온다** — 새 분기가 0개다.

**B안이 무엇을 치러야 하는지도 확인했다.** `AddSubtree`에 검사 2·3·4를 복제해야 하는데, **그 복제가 §2-①에서 방금 문제로 지적된 바로 그것이다.** 즉 B안은 *"검증을 두 곳에 두는 대신 착용 상태를 복원한다"* 는 거래인데, **A안 ＋ `CanPlaceInSlot`이면 검증이 한 곳이면서 §7-3까지 덮는다.** 거래가 성립하지 않는다.

#### ★ "원래 슬롯 우선"을 지금 안 만드는 것 — **맞다. 단 §2 해석을 한 줄 고쳐야 한다**

CLAUDE.md §2의 판단 기준은 이렇게 적혀 있다.

> *"이 확장점이 `DOCS/` 어딘가에 이름으로 적혀 있는가? **적혀 있으면 만든다.**"*

**글자 그대로 읽으면 `TryAutoEquip(In, FName PreferredSlot)`은 만들어야 한다** — 검토 문서가 방금 그 이름을 `DOCS/`에 적었기 때문이다. **그게 이 규칙의 구멍이다.** 자기가 이름을 적고 그 이름을 근거로 만들면, 규칙이 스스로 부푼다.

**§2의 진짜 판정선은 바로 아래 줄에 있다.**

> *"지금 소비자가 하나여도, 문서에 **두 번째 소비자가 예고돼 있으면** 그 자리를 만든다"*
> *"나중에 넣기 **비싼 것**은 지금 넣는다 — 식별자 안정성, 복제 조건, 계약"*

`PreferredSlot`은 **둘 다 아니다.** 소비자가 0이고(*"Hotbar2로 돌아온다"* 는 기획서에 없다), 나중에 넣는 비용이 **인자 하나 ＋ 호출부 하나**다. **안 만드는 것이 맞다.**

> **문서에 한 줄 권한다.** *"이름을 적는 것은 **기록**이지 승인이 아니다. §2의 '적혀 있으면 만든다'는 **기획서·상위 설계에 예고된 것**을 뜻하고, 검수 문서가 방금 적은 이름은 여기 해당하지 않는다."* — 이 구분이 없으면 검수를 할수록 만들 것이 늘어난다.

---

### 2-③. 가드를 "단일 쓰기 지점 표"에 — **결론 맞다. 그런데 기각 근거가 사실이 아니다**

#### 표에 거는 것은 맞다

*"낡은 원인은 목록이 낡은 게 아니라 목록이 **둘**이었다는 것"* 이라는 진단이 정확하다. 그리고 **새 규칙이 0개**라는 것이 이 처방의 값이다. `ReorderEntry`는 가드가 필요 없고(`AssignSortKey`가 갖는다), `MoveEntry`는 필요하다(직접 부른다) — 둘 다 맞다.

**가드의 크기를 다시 잰 것도 맞다.** 클라 UI가 `PostReplicatedReceive`로 받으므로 ㉠(배칭)의 소비자는 리슨서버/PIE뿐이고, ㉡(재진입)이 진짜 이유라는 판정이 옳다.

#### ❌ 그런데 *"엔진 관례와 싸운다"* 가 사실이 아니다 — **엔진이 감싼다**

```cpp
// AbilitySystemComponent_Abilities.cpp:980-996
void UAbilitySystemComponent::MarkAbilitySpecDirty(FGameplayAbilitySpec& Spec, bool WasAddOrRemove)
{
    if (IsOwnerActorAuthoritative())
    {
        if (!(Spec.Ability && Spec.Ability->NetExecutionPolicy == ...ServerOnly && !WasAddOrRemove))
        {
            ActivatableAbilities.MarkItemDirty(Spec);          // ← 감싼다
        }
        AbilitySpecDirtiedCallbacks.Broadcast(Spec);
    }
    else
    {
        ActivatableAbilities.MarkArrayDirty();
    }
}
```

그리고 헤더가 **호출자에게 래퍼를 쓰라고 지시한다.**

> *"Returns an ability spec from a handle. **If modifying call MarkAbilitySpecDirty.**"* — `AbilitySystemComponent.h:1112`

**GAS는 엔진에서 가장 큰 게임플레이 시스템이고, 그 FastArray는 `MarkItemDirty`를 직접 부르지 않는다.** *"UE 표준 이름이라 문서·샘플·자동완성이 전부 그것을 가리킨다"* 는 문장은 이 선례 앞에서 무너진다. **호출 지점이 6곳이고(`:305, 1264, 1341, 1969, 2088` 등) 전부 래퍼를 지난다.**

#### ✅ 그래도 기각이 맞다 — **근거를 바꾼다**

ASC의 래퍼가 존재하는 이유는 **안에 할 일이 있어서다.**

| ASC 래퍼가 하는 일 | 우리 `DirtyItem`이 할 일 |
|---|---|
| 권한 분기 | 없음 |
| `ServerOnly` 어빌리티는 dirty 스킵 | 없음 |
| `AbilitySpecDirtiedCallbacks.Broadcast` | 없음 (알림은 스코프 가드가 낸다) |
| 클라 예측 시 `MarkArrayDirty` | 없음 (아직) |

**할 일이 하나도 없다.** 그러면 남는 것은 이름 바꾸기이고, 그건 *"없어도 되는 어휘를 하나 만드는 안"* 이라는 원래 판정 그대로다. **강제할 문법이 없다는 지적도 여전히 맞다** — ASC도 강제하지 못해서 **헤더 주석으로 부탁하고 있다**(`:1112`).

**바꿔 쓸 문장:**

> ~~엔진 관례와 싸운다 — `MarkItemDirty`가 UE 표준 이름이다~~
> → **엔진도 감싼다**(`UAbilitySystemComponent::MarkAbilitySpecDirty`, `AbilitySystemComponent_Abilities.cpp:980`). **감싸는 이유는 안에 할 일이 있을 때다** — 권한 분기·조건부 스킵·콜백·클라 예측. 우리 래퍼에는 그중 하나도 없어서 **이름만 바뀐다.** 강제할 문법이 없는 것은 엔진도 같아서, ASC는 헤더 주석으로 부탁한다(`AbilitySystemComponent.h:1112`).

> **★ 이 정정이 값을 하는 날이 예고돼 있다.** 04-8의 **낙관적 클라 적용**을 넣으면 클라가 로컬 `SortKey`를 덮는데, ASC의 `else` 분기가 정확히 그 상황이다 — *"Clients predicting should call MarkArrayDirty to force the internal replication map to be rebuilt"*(`FastArraySerializer.h:993-994`). **그때는 래퍼 안에 할 일이 생긴다.** 지금 만들지는 않되, 기각 사유를 *"관례"* 로 적어두면 **그날 이 선례를 못 찾는다.** → §4 N-6

---

### 2-④. 용량표 — 확정 유지. 단 근거 하나를 바꾸고 **빠진 검증 하나를 넣는다**

#### 판정 요청 1 — `Cap == SlotSize`를 막아야 하나: **막는다. 근거는 익스플로잇이 아니다**

**익스플로잇으로는 약하다.** 정직하게 재보면:

```
SlotSize 10 / Cap 10 짜리 가방 X
X 안에 X 하나 → 바깥 X의 10칸 중 10칸을 안쪽 X가 먹는다 → 순증 0
N겹으로 쌓아도 총 용량은 10 그대로. 가방 N개를 소모해서 얻는 것이 0이다
```

**얻는 게 없으니 하는 사람도 없다.** 이 방향으로 밀면 *"과잉이다"* 가 맞는 말이 된다.

**진짜 근거는 셋이고 전부 값이 아니라 비용 쪽에 있다.**

| | |
|---|---|
| ① **비용이 0이다** | `IsDataValid`가 **이미 `<`로 짜여 있고**(`05_Loot_00_ItemCore.md:224`) 확정 수치가 전부 만족한다(15>12, 11>10, 6>5, 10>8). **쓸 코드도 고칠 값도 없다** |
| ② **깊이 상한이 이 규칙 하나에만 걸려 있다** | `RemoveEntry` 재귀·UI 중첩·세이브가 전부 *"깊이는 유한하다"* 를 전제한다. 그 전제의 증명이 §4-6의 부등식 **하나**다. **되돌리면 세 곳의 전제가 동시에 근거를 잃는다** |
| ③ **되돌릴 손잡이가 이미 이름으로 있다** | *"명시적 깊이 상한(`MaxContainerDepth` ＋ 검사 7)은 만들지 않는다. 밸런싱이 커플링을 못 견디면 그때 연다"*(`05_Loot_DOCS.md:510`). **`=`를 원하는 날의 답이 이미 적혀 있다** — `=`를 여는 게 아니라 깊이 상한을 여는 것이다 |

**즉 질문이 *"막을 가치가 있나"* 가 아니라 *"이미 공짜로 막혀 있는 것을 굳이 풀 이유가 있나"* 다.** 없다.

> **부등식 증명 자체는 맞다.** `SlotSize(A) ≤ Cap(B) < SlotSize(B) ⇒ SlotSize(A) < SlotSize(B)`이고 `SlotSize`가 양의 정수라 사슬이 유한하다. 깊이 상한은 **서로 다른 `SlotSize` 값의 개수**다. ✅

#### ★★ 그런데 증명이 `SlotSize ≥ 1`을 쓰는데 **그걸 검증하는 곳이 없다** — 본체 0칸이 이걸 구멍으로 만든다

`IsDataValid`가 보는 것은 컨테이너 규칙 하나뿐이고(`05_Loot_00_ItemCore.md:224`), `FEPItemData::SlotSize = 1`은 **기본값일 뿐 하한이 아니다.** DT에 0을 넣을 수 있다.

**본체가 10칸인 동안은 무해했다.** 0이 되면 다르다.

```cpp
bool CanFit(Container, ItemId) { return GetUsedSlots(C) + SlotSize <= GetCapacity(C); }

본체:  0 + 0 <= 0   →  참
```

**`SlotSize = 0`인 아이템은 0칸짜리 본체에 무한히 들어간다.** 그리고 `GetUsedSlots`가 그것들을 0으로 세므로 **영원히 안 찬다.** `GetInsertionOrder`의 맨 앞이 본체라 **그 아이템은 컨테이너에 절대 안 들어가고 항상 본체로 간다** — 동작이 조용히 갈린다.

- **`SlotSize = 0`은 실수하기 쉬운 값이다.** *"열쇠·퀘스트 토큰은 자리를 안 먹었으면"* 이라는 기획이 오면 DT에 0을 넣는 것이 가장 자연스러운 표현이다
- **증상이 없다.** 크래시도 경고도 없고 *"이 아이템만 가방에 안 들어간다"* 로 나타난다
- **`ContainerCapacity`를 지킨 것과 정확히 같은 이유로 지켜야 한다** — 규칙을 문서에만 두면 반년 뒤 깨진다

**처방 — 이미 있는 함수에 한 줄이다.**

```cpp
// UEPItemDefinition::IsDataValid — 컨테이너 검사 옆
if (Row->SlotSize < 1)
{
    Context.AddError(FText::Format(
        NSLOCTEXT("EP","SlotSizeMin","SlotSize({0})가 1 미만입니다 — 칸을 안 먹는 아이템은 용량 판정을 무력화합니다."),
        Row->SlotSize));
    Result = EDataValidationResult::Invalid;
}
```

> **"자리를 안 먹는 아이템"이 기획으로 오면 그때는 `SlotSize`가 아니라 슬롯이다** — 몸 슬롯(`SlotId != None`)이 이미 *"칸을 안 먹는다"* 의 유일한 표현이고(`GetUsedSlots`의 `continue` 한 줄), 그게 이 설계의 정의다. **두 번째 표현을 만들지 않는다.**

#### 판정 요청 2 — 상의·하의의 컨테이너 제거가 **데이터 둘, 코드 0줄**인가: **맞다. 확인했다**

| 손잡이 | 무엇이 자동으로 옳아지나 | 확인 |
|---|---|---|
| `DT_Items`의 `ContainerCapacity` → 0 | `GetCapacity`가 0 → `CanFit` 항상 거짓 | ✅ `05_Loot_03_Inventory.md:1272` |
| `ContainerOrder`에서 제거 | 헛도는 판정이 없어진다 | ✅ **안 해도 동작이 맞다** |

**그리고 UI도 안 고쳐도 된다 — 이게 요청서가 확인 못 한 부분이다.**

```cpp
// 05_Loot_04_InventoryUI.md 04-3 :334
if (Inventory->GetCapacity(Id) <= 0)     continue;   // 신발·귀·얼굴
```

**04-A가 이미 `Capacity <= 0`으로 구획을 거른다.** `ContainerOrder`에 상의가 남아 있어도 **구획이 안 그려진다.** 즉 손잡이 둘 중 **첫째 하나만으로도 UI까지 전부 옳다.**

> **§4-9의 *"컨테이너 여부를 타입 계층이 아니라 값으로"* 가 값을 하는 자리**라는 서술이 정확하다. 여기에 위 인용을 근거로 붙이면 *"코드 0줄"* 이 주장이 아니라 **확인된 사실**이 된다.

- **공짜가 아닌 것 하나(`StartingEquipment`에 외투나 배낭이 들어가야 한다)** 도 맞다. 그리고 그건 **데이터 셋째**다
- **`SlotSize` 11/6이 "컨테이너인 동안의 최소값"** 이라는 것도 맞다. 단 위 검증이 들어가면 하한이 `0`이 아니라 **`1`** 이다

---

### 2-⑤. `StartingEquipment` in `UEPLootDeveloperSettings` — **맞다. 단 6차와 같은 자리가 아니다**

#### 결론: 지금은 여기가 맞다

**엔진에 정확히 이 모양의 선례가 있다.**

```cpp
// GameMapsSettings.h:216-217   (UCLASS(config=Engine, defaultconfig), :100)
/** GameMode to use if not specified in any other way. (e.g. per-map DefaultGameMode or on the URL). */
UPROPERTY(config, noclear, EditAnywhere, ...)
FSoftClassPath GlobalDefaultGameMode;
```

**주석이 사용자의 계획을 그대로 적고 있다** — *"다른 방법으로 지정되지 않았을 때 쓰는 값. (예: 맵별 설정이나 URL이 덮는다)"*. **전역 기본값을 config에 두고 더 구체적인 소스가 나중에 덮는 것**이 엔진의 표준 모양이고, 로비가 그 *"더 구체적인 소스"* 다.

**`UDeveloperSettings`가 런타임 콘텐츠 참조를 드는 것도 선례가 있다.**

```cpp
// UserInterfaceSettings.h:117, 133
class UUserInterfaceSettings : public UDeveloperSettings
    TMap<TEnumAsByte<EMouseCursor::Type>, FSoftClassPath> SoftwareCursors;
```

> **정확히 적어둔다:** `UGameMapsSettings`는 `UDeveloperSettings` 파생이 아니라 `config=Engine`인 `UObject`다(`:100-102`). 인용하는 것은 **클래스 계층이 아니라 "전역 기본값 ＋ 나중에 덮기"라는 모양**이다. `UDeveloperSettings` 쪽 선례는 `UUserInterfaceSettings`다.

#### ❌ 그런데 6차 확정과 **같은 자리가 아니다**

6차의 근거는 이것이었다(03-2 `:864` 인용).

> *"소비자가 둘이다 — 이 검사와 **Step 04 UI의 슬롯 그리기.** **UI에는 물어볼 인벤토리 인스턴스가 없을 수도 있다.**"*

**`BodySlots`·`ContainerOrder`가 전역인 이유는 "인스턴스 없이 읽어야 하는 소비자가 있다"** 이고, 둘 다 **아이템 시스템의 규칙**이다.

**`StartingEquipment`는 셋 다 다르다.**

| | `BodySlots` / `ContainerOrder` | `StartingEquipment` |
|---|---|---|
| 소비자 | **둘 이상**, 하나는 인스턴스가 없다 | **하나** — 서버 `BeginPlay` |
| 부르는 곳의 문맥 | 없을 수 있다 | **캐릭터 액터가 있다.** 못 읽을 이유가 없다 |
| 성격 | **규칙** (표현 가능한 상태의 정의) | **콘텐츠·밸런싱** (누가 무엇을 들고 시작하나) |

**즉 6차 근거로는 이 필드가 정당화되지 않는다.** 정당화하는 것은 **위의 `GlobalDefaultGameMode` 모양 ＋ "지금은 모드가 하나"** 다. **결론은 같고 근거가 다른데, 근거를 잘못 적으면 다음에 오는 콘텐츠성 전역 필드가 6차를 인용해서 들어온다** — 그때는 막을 문장이 없다.

#### 최종적으로 갈 자리는 어디인가 — Lyra가 그걸 갖고 있다

```cpp
// LyraPawnData.h:25-53
class ULyraPawnData : public UPrimaryDataAsset
    TSubclassOf<APawn> PawnClass;
    TArray<TObjectPtr<ULyraAbilitySet>> AbilitySets;      // 스폰 시 부여할 것
    TObjectPtr<ULyraInputConfig> InputConfig;
    TSubclassOf<ULyraCameraMode> DefaultCameraMode;
```

**"이 폰이 무엇을 들고 태어나나"는 Lyra에서 `UPrimaryDataAsset`이고, 모드/Experience가 그걸 고른다.** 로비가 생기면 `StartingEquipment`는 **그쪽으로 간다** — 배열을 채우는 쪽만 바뀌는 게 아니라 **드는 곳이 바뀐다.**

- **`GameMode`는 지금 자리가 아니다.** 모드가 하나뿐이라 `UDeveloperSettings`와 구별이 안 되고, 로비가 오면 어차피 PawnData를 고르는 쪽이 된다
- **`PlayerStart`는 틀린 자리다.** 그건 공간이고 이건 장비다
- **`DataAsset`을 지금 만드는 것은 이르다.** 소비자가 하나이고 값이 `TArray<FName>` 둘이다. **CLAUDE.md §2의 "두 번째 소비자가 예고돼 있으면"** 에 걸리지 않는다 — 로비는 **기획에 이름이 있지만 언제 오는지가 없다**

**권고: 필드는 그대로 두고, `05_Loot_DOCS.md` §8 미정에 한 줄 올린다.**

> **미정 #10 — 시작 장비를 드는 곳.** 지금은 `UEPLootDeveloperSettings::StartingEquipment`(전역 기본값, `UGameMapsSettings::GlobalDefaultGameMode`와 같은 모양). **로비가 오면 드는 곳이 `UPrimaryDataAsset`(Lyra `ULyraPawnData` 형태)로 옮겨가고 이 필드는 폴백이 된다.** 결정 신호: *"모드나 캐릭터마다 시작 장비가 달라져야 한다."* **6차의 `UDeveloperSettings` 확정을 근거로 삼지 않는다** — 그건 *"인스턴스 없이 읽는 소비자가 있다"* 였고 이 필드는 소비자가 하나다

---

## 3. 구간 재조정 — ㉯가 맞다. 그리고 **03-B는 남길 이유가 없다**

### 3-1. ㉯(`TryAutoEquip`을 03-C로) — 맞다

**㉮가 아무것도 고치지 못한다**는 판단이 결정적이고 그게 맞다. `AddSubtree`를 03-A로 올려도 `TryAutoEquip`의 진짜 호출자(`OnInteract`)가 03-C에 있으므로 **03-B에서 돌리려면 `EP.Inv.AutoEquip`을 또 만들어야 하고**, 그건 C-1이 방금 지적한 *"호출자 없는 코드"* 를 **한 구간 규모로 재생산한다.**

**9차의 규칙을 반대 방향으로 쓴 것도 맞다.** 규칙은 *"호출자와 같은 구간에"* 이고, 9차는 쓰는 쪽이 앞이라 올렸고 여기는 쓰는 쪽이 뒤라 내린다. **같은 규칙이다.**

> **`GetCapacity`가 통째로 03-A로 온 것도 맞다.** `EP.Inv.Move`가 컨테이너로 옮기므로 컨테이너 갈래가 03-A에서 실행된다 — *"함수를 반만 만든다"* 가 없어진 것이 이 재조정의 부수 이득이고, 그게 8차가 03-7에서 배운 것과 같다.

### 3-2. ★★ 판정 요청에 대한 답 — **합치는 게 아니라 03-B를 없앤다**

> *"03-B가 이 정도로 얇아지면 03-A에 합쳐야 하나. 다만 '두 번째 용량 풀이 처음 생기는 지점'이라는 검증 가치는 남는다고 봤다."*

**검증 가치는 남는다. 그런데 그 검증이 03-A의 도구로 이미 된다.** 그리고 03-B에 남은 **유일한 새 코드가 Step 03에 호출자가 0개다.**

#### `Server_EquipBackpack`은 Step 03에서 **한 번도 안 불린다**

문서 전체를 훑었다. 호출자가 없다.

| 경로 | 무엇을 부르나 |
|---|---|
| 줍기 자동 착용 | `OnInteract` → `TryAutoEquip` → `AddSubtree` — **서버 내부. RPC를 안 지난다** |
| 벗기 | **`Server_DropItem`** (03-6 `:2023` 확정) |
| 03-B 검증 | **`EP.Inv.Move <id> -1 Back`** — 문서가 직접 그렇게 적었다(`:58`) |
| 수동 착용 UI | **Step 04** (드래그) / `EP.Inv.Equip`도 **04-A** (`05_Loot_04_InventoryUI.md:45`) |

**이 문서가 같은 상황에 같은 규칙을 이미 두 번 적용했다.**

> *"Step 03에는 `NewParent`와 `NewSlotId`를 정당하게 만들어낼 UI가 없다"* → `Server_MoveEntry`를 **안 만든다** (`:1205`)
> *"Step 03에는 정당한 클라 호출자가 없다"* → `Server_ReorderEntry`를 **04-B로** (`:1085`)

**`Server_EquipBackpack`도 정당한 클라 호출자가 없다.** 규칙이 여기만 적용되지 않았고, 그 이유는 *"03-6이 원래 그 RPC로 쓰여 있었다"* 는 역사뿐이다.

> **`Server_EquipBackpack`의 *모양*이 옳다는 판정은 그대로다.** *"좁은 RPC가 넓은 RPC보다 낫다"* 는 `Server_MoveEntry`와의 비교이지 **언제 여느냐의 답이 아니다.** 9차가 `MoveEntry`에 대해 세운 문장 그대로다 — *"내부 계약은 지금, 외부 표면은 소비자와 함께."*

#### 그래서 03-B에 남는 것이 없다

| 03-B의 내용물 | 어디로 |
|---|---|
| `Server_EquipBackpack` (3줄 래퍼) | **04-A** — `EP.Inv.Equip`과 같은 자리. 거기가 첫 호출자다 |
| 다중 컨테이너 검증 (완료 조건 7 후반) | **03-A의 마지막 검증** — `EP.Inv.Add` ＋ `EP.Inv.Move`로 하고, 그 둘은 03-A 소속이다 |

**권고:**

```
03-A  코어 ＋ SortKey 일습 ＋ MoveEntry ＋ GetCapacity(통째로) ＋ EP.Inv.Move
      마지막 검증에 "두 번째 용량 풀" 포함 (완료 조건 7 후반)
03-B  줍기·버리기 ＋ AddSubtree ＋ TryAutoEquip ＋ StartingEquipment    ← 지금의 03-C
```

**구간이 셋에서 둘로 준다.** 8차가 셋으로 나눈 이유가 *"완료 조건 19개는 다른 단계 두 개 분량"* 이었는데, 지금 배분은 **03-A 10개 / 03-B 1개 / 03-C 8개**다. 가운데 구간이 **완료 조건 하나짜리 코드 3줄**이고 그 3줄에 호출자가 없다.

#### 그리고 03-B를 남기면 **없어질 전제 위에서 검증하게 된다**

03-B의 검증 절차는 이렇다.

```
EP.Inv.Add Backpack_B  →  본체에 넣는다   (SlotSize 10, MaxSlots 10 → 딱 맞는다)
EP.Inv.Move <id> -1 Back
```

- **`Backpack_A`(SlotSize 15)는 본체(10)에 못 들어간다.** 03-B에서 쓸 수 없다 — 의도된 값이라 버그는 아니지만, **검증에 쓸 수 있는 배낭이 B 하나뿐**이다
- **`Shirt_Basic`(11)도 본체에 못 들어간다.** 03-B에서 상의를 만들 방법이 아예 없다
- **`MaxSlots`가 0이 되면 이 절차가 통째로 죽는다** — §8 미정 #9가 확정되는 순간 03-B의 유일한 검증 경로가 사라진다

**A-3이 *"본체 경유는 '본체에 칸이 좀 남아 있다'를 깔고 있고 그 전제가 곧 사라진다"* 고 진단한 바로 그 전제 위에, 03-B의 검증 절차가 그대로 서 있다.** 생산 경로는 고쳤는데 검증 경로는 안 고쳤다. → §4 N-5

---

## 4. 놓친 것 — 12건

> **요청서가 지정한 네 축을 그대로 따라간다.** 축마다 최소 하나씩 나왔다.

### 축 1 — 11·12차가 바꾼 것의 파급

#### ★★ N-1. **B-2와 같은 모양이 `MoveEntry`에 하나 더 있다** (최우선)

B-2는 *"`InsertEntry`가 `AddDefaulted` **뒤에** 키를 발급해서 자기가 키 공간의 형제로 잡힌다"* 였다. **`MoveEntry`가 같은 짓을 한다.**

```cpp
// 03-2 :887-897 — 반영된 코드
const int32 OldParent = E.ParentEntryId;
E.ParentEntryId = NewParent;                 // ★ 여기서 이미 목적지의 형제가 된다
E.SlotId        = NewSlotId;
Entries.MarkItemDirty(E);

if (NewParent != OldParent)
    AssignSortKey(EntryId, KeySpace_NextAtEnd(NewParent));   // ★ 자기(옛 키)를 센다
```

`KeySpace_NextAtEnd`는 *"부모가 같은 것 전부"* 를 돌므로 **방금 재부모된 자기 자신을 옛 컨테이너의 키와 함께 포함한다.**

| | 명세대로(자기 제외) | 지금 코드대로(자기 포함) |
|---|---|---|
| 목적지가 **빈** 컨테이너 | `bAny == false` → **0** | `Max = 옛 키` → **옛 키 + Step** |
| 옛 키가 목적지 최대보다 **작다** | 정상 | **같다** (최대가 안 바뀐다) |
| 옛 키가 목적지 최대보다 **크다** | 목적지 최대 + Step | **옛 키 + Step** — 목적지와 무관한 큰 값 |
| `if (!bAny) return 0;` | 도는 분기 | **여기서도 죽는다** |

**순서는 안 깨진다** — 어느 경우든 목적지의 최대보다 크므로 맨 뒤에 붙는다(함정 4m은 지켜진다). **깨지는 것은 키 공간이다.**

```
본체:  … 붕대 1,000,000        (오래 쓴 컨테이너)
배낭:  칫솔 0   붕대 65536      (새 컨테이너)

붕대를 배낭으로 옮긴다  →  배낭 안에서 1,065,536
그 배낭을 정리하다 보면 배낭의 키가 본체의 이력을 따라 올라간다
```

**컨테이너 사이를 오갈 때마다 큰 쪽의 키 크기가 작은 쪽으로 전염된다.** 경계에 닿으면 `KeySpace_NextAtEnd`의 가드가 재정규화하므로 **틀린 순서로는 안 가지만**, 재정규화 빈도가 이유 없이 올라가고 **B-2가 살려낸 *"빈 컨테이너면 0"* 분기가 이 경로에서 다시 죽는다.**

**처방 — B-2와 **글자 그대로 같은** 한 줄 순서 바꾸기다.**

```cpp
// 재부모 전에 목적지의 키를 구한다 — InsertEntry가 AddDefaulted 전에 구하는 것과 같다
const int32 NewKey = (NewParent != OldParent) ? KeySpace_NextAtEnd(NewParent) : INDEX_NONE;

E.ParentEntryId = NewParent;
E.SlotId        = NewSlotId;
Entries.MarkItemDirty(E);

if (NewParent != OldParent) AssignSortKey(EntryId, NewKey);
```

> **이게 요청서 §4가 물은 것("같은 혼동이 다른 함수에도 있나")의 답이다.** 그리고 혼동의 이름이 이미 있다 — 12차가 붙인 `KeySpace_` 접두어는 *"어느 목록을 보나"* 를 이름에 실었는데, **여기서 틀린 것은 목록이 아니라 시점**이다. 함정 4x의 제목을 *"키는 배열 상태를 바꾸기 **전에** 구한다"* 로 넓히고 두 함수를 같이 적는 것을 권한다.

#### ★ N-6. 04-8 낙관적 적용 × 12차 제자리 조기 반환(4u) — **넣는 날 안 맞는다**

04-8이 *"클라 전용이라 서버 계약을 안 건드린다 — 나중에 넣어도 비싸지 않다"* 로 미뤄뒀다. **12차가 그 문장을 반쯤 거짓으로 만들었다.**

```
드롭 → 클라가 로컬 SortKey를 추정값으로 덮는다
     → Server_ReorderEntry
     → 서버: 제자리다  →  4u의 조기 반환  →  AssignSortKey를 안 부른다
     → 아무것도 복제되지 않는다
     ⇒ 클라의 추정 키가 영영 안 덮인다
```

**표시 순서는 맞아서 안 보인다.** 문제는 **다음 드래그의 추정이 오염된 키에서 출발한다**는 것이다. 04-8이 *"키 값이 아니라 순서만 맞으면 되므로 도착한 값으로 덮는 것으로 충분하다"* 고 적었는데, **도착하지 않는 경우가 12차에 생겼다.**

**그리고 두 번째 문제가 더 크다.** 클라가 추정 키를 만들려면 **서버와 같은 키 공간**(부모 전체)을 봐야 하는데, `KeySpace_*` 넷이 전부 **private**이다(`:530-536`). 표시 목록(`GetSortedContents`)으로 추정하면 **함정 4q·4s가 클라에서 그대로 재현된다** — 슬롯 형제와 동률이 나서 잠깐 엉뚱한 자리에 그려진다.

**즉 낙관적 적용은 "클라 전용"이 아니다.** 서버 표면(private 헬퍼의 가시성)을 건드린다.

**지금 할 일은 없다.** 04-8에 두 줄 적는 것으로 충분하다.

> **낙관적 적용을 넣을 때 둘을 같이 본다 (13차 검수).**
> ① **제자리면 클라도 먼저 조기 반환한다** — 서버의 4u와 같은 판정을 `GetSortedContents`만으로 할 수 있다. 안 그러면 추정 키가 영영 안 덮인다
> ② **추정 키는 `GetSortedContents`로 만들면 안 된다** — 서버는 부모 전체를 본다(함정 4q). 클라가 같은 계산을 하려면 `KeySpace_*`가 공개돼야 하므로 **"클라 전용이라 서버 계약을 안 건드린다"가 그때는 성립하지 않는다.** 아니면 **키를 추정하지 말고 표시 순서만 로컬로 뒤집는다** — 그쪽이 싸다

#### N-7. `FScopedInventoryNotify`가 **정의 없이 쓰이고 있다** — 03-A 첫 빌드 컴파일 에러

```
Private/Inventory/EPInventoryComponent.cpp:104   FScopedInventoryNotify Guard(this);
Public/Inventory/EPInventoryComponent.h:130      friend struct FScopedInventoryNotify;   ← 선언이 아니다
```

**소스 전체에 정의가 없다.** `friend` 선언은 타입을 도입하지만 **불완전 타입**이라 인스턴스를 만들 수 없다. A-4(`PostReplicatedReceive`)와 **같은 부류인데 A-4는 아직 잠재적이고 이건 이미 코드에 있다.**

**STATUS의 "남은 골격 결함 4건"에 없다.** 03-7이 정의를 어디에 두는지(헤더 `.h`인지 `.cpp` 익명 네임스페이스인지)도 문서에 없다 — `MoveEntry`·`InsertEntry`가 `.cpp`에서 쓰므로 **`.cpp` 상단이면 충분하고**, 그러면 `friend` 선언만 헤더에 남는 지금 형태가 맞다.

#### N-11. `TArray<class FEPInventoryEntry>`의 `class` 키워드가 남았다

골격 결함 #1의 처방(`#include`)은 **적용됐다**(`EPInventoryComponent.h:8`). 그런데 `:20`은 그대로다.

```cpp
UPROPERTY() TArray<class FEPInventoryEntry> Items;   // FEPInventoryEntry는 struct다
```

**MSVC C4099**(`type name first seen using 'struct' now seen using 'class'`). include가 생긴 지금은 **전방선언 흉내를 낼 이유도 없어졌다.** 결함 #1의 처방에 *"＋ `class` 키워드 제거"* 를 붙이는 것을 권한다.

---

### 축 2 — `SortKey`가 `int32`인 데서 오는 것

#### N-1이 이 축의 답이다 (위)

**같은 혼동의 세 번째 인스턴스다.** 8차 `FindFungibleEntryId`(컨테이너 인자 누락) → 9차 `GetEntryInSlot`(부모 인자 누락) → 11차 `KeySpace_NextAtEnd`(목록 혼동) → 12차 `ReorderEntry`(목록 혼동) → **13차 `MoveEntry`(시점 혼동)**. 축이 *"어느 스코프를 보나"* 에서 *"언제 보나"* 로 한 칸 옮겨갔을 뿐이다.

#### N-12(낮음). 이분 중간값의 뺄셈이 넘칠 수 있다 — **안 고쳐도 된다. 판단만 적어둔다**

```cpp
NewKey = PrevKey + (NextKey - PrevKey) / 2;
```

`PrevKey + (Next−Prev)/2` 형태는 **합의 오버플로는 피하지만 차의 오버플로는 못 피한다.** `PrevKey ≈ MIN`이고 `NextKey ≈ MAX`면 `NextKey − PrevKey`가 `int32`를 넘는다(부호 있는 오버플로 = UB).

**도달 가능성을 정직하게 재봤다.**

```
한 컨테이너가 양끝에 동시에 가려면
  맨 앞 이동 ~32,700회  ＋  키를 유지한 채 추가/제거 ~32,700회   (양쪽 다 가드 직전까지)
  그 전에 어느 한쪽이 경계에 닿으면 재정규화가 전부 0..N*Step으로 되돌린다
```

**정상 플레이로는 도달하지 않고, 스크립트로도 6만 회 이상이 필요하다.** CLAUDE.md §2 기준으로 *"도달 불가한 분기"* 에 가깝다 — **다만 분기가 아니라 캐스트 하나다.**

```cpp
const int32 NewKey = static_cast<int32>((static_cast<int64>(PrevKey) + NextKey) / 2);
```

**권고: 넣어도 되고 안 넣어도 된다.** 넣으면 한 줄이고 분기가 안 늘며 *"희소 키의 이분은 int64로 계산한다"* 가 계약이 된다. **안 넣기로 정하면 그 판단을 함정표에 적어둘 필요는 없다** — 근거 없는 방어를 안 넣는 것이 이 문서의 일관된 태도이고, 여기서도 그게 틀리지 않다.

---

### 축 3 — 본체 0칸의 파급

#### ★★ N-4. `SlotSize ≥ 1` 검증이 없다 → §2-④에서 다뤘다 (여기가 이 축의 최대 건이다)

#### ★ N-5. **검증 도구 전부가 `MaxSlots = 10`에 서 있다**

§8 미정 #9가 *"0으로 내리면 완료 조건 2~6의 검증 경로가 상의 컨테이너로 옮겨간다"* 고 적었다. **지금 도구로는 옮겨갈 수 없다.**

```
EP.Inv.Add <ItemId> [PlayerIndex]      # 03-9 :2185 — 컨테이너 인자가 없다
```

**컨테이너를 지정할 방법이 없으므로 본체로 갈 수밖에 없고, 본체가 0이면 전부 실패한다.** 상의로 넣으려면 `GetInsertionOrder`(03-C)나 `StartingEquipment`(03-C)가 필요하다.

| | 지금 | `MaxSlots = 0`이 되면 |
|---|---|---|
| 03-A 완료 조건 2~6 | `EP.Inv.Add` | **전부 실패. 검증 불가** |
| 03-A 완료 조건 14·15·17~19 | `EP.Inv.Add`로 아이템을 만들어야 한다 | **아이템을 만들 수 없다 → 전부 검증 불가** |
| 03-B(현재) 완료 조건 7 후반 | `EP.Inv.Add Backpack_B` | **배낭을 인벤토리에 넣을 방법이 없다** |

**즉 §8 미정 #9의 전환은 03-C 완료 이후에만 가능하고, 그 뒤에는 03-A 완료 조건 9개를 다시 돌릴 수 없다.** 회귀 테스트를 잃는다.

**처방 — 인자 하나다.**

```
> EP.Inv.Add <ItemId> [Container] [PlayerIndex]    # Container 기본 -1(본체)
```

- **`EP.Inv.Move`가 이미 `<NewParent>`를 받는다.** 어휘가 늘지 않는다
- **`MaxSlots`를 0으로 내려도 03-A 완료 조건이 전부 살아 있다** — 상의를 `EP.Inv.Add Shirt_Basic`... 은 본체 11칸이 필요하니 안 되지만, **`EP.Inv.Move`로 슬롯에 넣을 수 있다.** 정확히는 `EP.Inv.Add Shirt_Basic -1` 자체가 막히므로, **`MaxSlots`를 내리는 것은 `StartingEquipment`(03-C)와 같은 시점**이라는 결론이 나온다 — 그것도 §8 #9에 적어야 한다
- **`GetInsertionOrder`(03-C)를 03-A로 당기는 대안은 하지 않는다.** 소비자가 `OnInteract` 하나뿐이고 `AddSubtree`에 의존한다(9차·13차 판정 그대로)

> **§8 미정 #9에 한 줄 추가를 권한다:** *"전환 시점은 **03-C 완료 이후**다. `StartingEquipment`가 돌아야 컨테이너가 생기고, 그 전에는 `EP.Inv.Add`가 본체로만 넣는다. 그리고 전환 후 03-A 완료 조건을 다시 돌리려면 `EP.Inv.Add`에 컨테이너 인자가 있어야 한다."*

#### N-9. **마스터 기획서의 획득 절차가 `TryAutoEquip`을 모른다**

```
// 05_Loot_DOCS.md:316-317   AEPPickup::OnInteract [대상]
  b. EntryId = AddItem(본체, ItemId, State)      (§4-6)
       실패하고 배낭을 매고 있으면 배낭에 재시도. 순서를 뒤집으면 본체가 늘 빈다
```

**세 겹으로 낡았다.**

| | 지금 절차 | 실제 (03-4) |
|---|---|---|
| ① 자동 착용 | **없다** | `TryAutoEquip`이 **①단계**다 |
| ② 컨테이너 순회 | *"본체 → 배낭"* 2단계 폴백 | `GetInsertionOrder()` (본체 → 외투 → 상의 → 하의 → 배낭 → 팔목) |
| ③ 서브트리 | `AddItem` | **`AddSubtree`** (함정 3c: *"안의 아이템이 전부 증발"*) |
| ④ 본체 0칸 | *"순서를 뒤집으면 본체가 늘 빈다"* | **첫 줄이 항상 실패한다.** 그 문장의 전제가 사라졌다 |

**그리고 이 절이 자기가 위험한 자리라고 직접 적어놨다.**

> *"이 절차는 **구현 체크리스트로 읽히는 자리라 누락이 그대로 코드가 된다**."* (`05_Loot_DOCS.md:320`)

**마스터 기획서가 단계 문서보다 상위이므로**, 여기가 낡으면 다음 사람이 `AddItem`으로 구현하고 함정 3c에 그대로 빠진다. **03 문서 21건을 고치는 동안 이 절이 안 딸려왔다** — D-2가 *"파급이 이 문서 밖으로 나간다"* 고 짚었던 그 방향이다.

---

### 축 4 — `EP.Inv.Move` 신설의 파급 (검사 0~6이 서로 모순 없나)

**일곱 개를 서로 대조했다. 모순은 없다.** 다만 셋을 확인해 둔다.

| | 확인 |
|---|---|
| **검사 0 vs 검사 4** | 0이 *"목적지가 지금 자리와 같으면 `false`"*, 4가 *"목적지 슬롯이 차 있으면 `false`"*. **자기가 그 슬롯에 있으면 0이 먼저 잡는다** — 0이 4보다 앞이라 *"자기 자신 때문에 실패"* 가 안 난다. ✅ **순서가 계약이다.** 문서에 순서 이유가 없으므로 한 줄 권한다 |
| **검사 3 vs 검사 5** | 3이 몸 슬롯에 `Parent == INDEX_NONE`을 요구하고, 5는 `NewSlotId == None`일 때만 돈다. **둘이 배타적**이라 겹치지 않는다. ✅ |
| **검사 6 vs `RemoveEntry` ④** | 6이 없으면 `RemoveEntry`의 *"부모가 이미 배열에서 빠져 사이클이 성립하지 않는다"* 가 근거를 잃는다는 문서의 설명이 맞다. **`MoveEntry`가 트리 전제를 깰 수 있는 유일한 함수**라는 것도 맞다(`InsertEntry`는 새 노드, `RemoveEntry`는 삭제). ✅ |

#### N-2 (§2-①에서 다룬 것) — 검사 2·3·4가 `AddSubtree`에는 **없다**

`EP.Inv.Move`가 검사 일곱을 처음 돌리는 것은 맞다. **그런데 같은 상태를 만드는 두 번째 경로(`AddSubtree`)는 그 일곱 중 셋을 안 돈다.** 완료 조건 15·17이 `EP.Inv.Move`만 검증하므로 **`AddSubtree`의 슬롯 경로는 03-C에서도 검증되지 않는다** — `TryAutoEquip`이 미리 확인해서 통과하기 때문이다. **호출자가 보증하는 계약은 호출자가 늘어나면 깨진다.**

---

### 잔여 정합 3건 (낮음)

| # | 무엇 | 어디 |
|---|---|---|
| **N-8** | **STATUS의 소스 스냅샷이 낡았다.** *"`EPInventoryComponent.h`는 엔진 템플릿 그대로(ctor/`BeginPlay`/`TickComponent`만)"*, *"`.cpp` 엔진 템플릿 그대로. `bCanEverTick = true`"* — **셋 다 아니다.** 헤더에 함수 40여 개가 선언돼 있고, `.cpp`에 `SetIsReplicatedByDefault(true)`·`bCanEverTick = false`·`Entries.Owner = this`가 들어가 있다. **골격 결함 #1·#3·#4가 이미 고쳐졌다.** 헤더가 *"최종 확인: 2026-08-25 (소스 직접 대조)"* 라고 적혀 있어 더 나쁘다 — **CLAUDE.md가 STATUS를 진실의 원천으로 못박았는데 그 표가 소스보다 뒤에 있다.** 남은 결함은 **#2 하나 ＋ N-7(`FScopedInventoryNotify`) ＋ N-11(`class` 키워드)** 이다 | `_STATUS.md:9-26` |
| **N-10** | **변경 이력이 `StartingEquipment`를 03-B라고 적는다.** 본문(`:2070`)·체크포인트 표(`:60`)·STATUS(`:95`)는 전부 03-C다. 13차 변경 이력 한 줄만 옛 배분을 들고 있다 | `:2341` |
| **N-13** | **완료 조건 4의 문구가 03-A 도구로 검증 불가다.** *"칸이 모자란 상태로 **줍기**를 시도하면 아무것도 안 들어가고 **픽업이 그대로 남는다**"* — `bClaimed` 되돌림(03-4)이 있어야 뒷절이 성립한다. **완료 조건 1을 03-C로 보낸 것과 같은 이유**인데 4는 03-A에 남았다. 문구를 *"`EP.Inv.Add`가 `INDEX_NONE`을 돌려주고 `Dump`가 안 바뀐다"* 로 바꾸거나, 뒷절을 03-C로 나눈다. 조건 2의 *"주우면"* 도 같은 종류(문구만) | `:14`, `_STATUS.md:114` |

#### 그리고 계약 문장 하나 (N-14, 낮음)

`AddSubtree`의 전제가 이렇게 적혀 있다.

```cpp
// 전제: In은 RemoveEntry가 만든 전위 순회 배열. In[0]이 루트이고 ...
```

**`StartingEquipment`는 `RemoveEntry`가 만들지 않는다** — `BeginPlay`가 `ItemId`로 원소 1개를 손으로 만든다(03-6 `:2066`). 지금은 우연히 성립한다(기본 생성자가 `ParentEntryId = INDEX_NONE`, `SlotId = NAME_None`, `SortKey = 0`). **전제를 출처가 아니라 모양으로 다시 쓰는 것을 권한다** — *"`In[0]`이 루트이고 `Parent = INDEX_NONE` · `SlotId = NAME_None` · `SortKey = 0`으로 정규화돼 있다. `RemoveEntry`가 그 형태를 만든다"*. 출처로 적어두면 **손으로 만든 배열이 계약 위반처럼 읽힌다.**

---

## 5. 작업 목록 — 우선순위

> **아래는 제안이다. 적용 여부는 사용자가 결정한다.**

### 지금 (03-A 착수 전)

| # | 무엇 | 어디 | 왜 |
|---|---|---|---|
| **1** | **`MoveEntry`가 재부모 **전에** 키를 구한다** (N-1) | 03-2 `:887-897` ＋ 함정 4x | B-2와 같은 결함. 키가 컨테이너 사이로 전염되고 *"빈 컨테이너면 0"* 이 다시 죽는다 |
| **2** | **`FScopedInventoryNotify` 정의 위치를 문서에 명시** (N-7) | 03-7 ＋ STATUS 결함표 | **지금 코드가 컴파일되지 않는다** |
| **3** | **`CanPlaceInSlot(Parent, SlotId, ItemId)` 추출** (N-2) | 03-2 검사 2·3·4 ＋ 03-4 `AddSubtree` | *"슬롯 진입 경로가 둘"* 이 API에서 거짓이고, §7-3이 셋째다 |
| **4** | **STATUS 소스 스냅샷 갱신 ＋ 결함표 재작성** (N-8·N-11) | `_STATUS.md:9-26` | STATUS가 진실의 원천인데 소스보다 뒤에 있다 |
| **5** | **`IsDataValid`에 `SlotSize >= 1`** (N-4) | `05_Loot_00_ItemCore.md` | 본체 0칸에서 `0 + 0 <= 0`이 참이다 |
| **6** | **`EP.Inv.Add`에 `[Container]` 인자** (N-5) | 03-9 | 없으면 `MaxSlots = 0` 전환이 03-A 완료 조건 9개를 죽인다 |

### 구간을 정할 때

| # | 무엇 | 왜 |
|---|---|---|
| **7** | **03-B를 없애고 둘로 나눈다.** `Server_EquipBackpack` → **04-A**, 완료 조건 7 후반 → **03-A 마지막 검증** | Step 03에 호출자가 0개. 9차·11차가 두 번 적용한 규칙이 여기만 빠졌다 |
| **8** | **§8 미정 #9에 "전환은 03-C 완료 이후"를 명시** | `StartingEquipment`가 돌아야 컨테이너가 생긴다 |

### 문서 정합

| # | 무엇 |
|---|---|
| **9** | **`05_Loot_DOCS.md:316` 획득 절차 재작성** (N-9) — `TryAutoEquip` ①단계 ＋ `GetInsertionOrder` ＋ `AddSubtree`. **마스터 기획서라 우선순위가 03 문서보다 높다** |
| **10** | **§2-③의 래퍼 기각 근거 교체** — *"엔진 관례와 싸운다"* → *"엔진도 감싼다. 감싸는 이유는 안에 할 일이 있을 때다"* (`AbilitySystemComponent_Abilities.cpp:980`) |
| **11** | **§2-⑤의 근거 분리 ＋ §8 미정 #10 신설** — 6차와 같은 자리가 아니다. `GlobalDefaultGameMode` 모양이고 최종 자리는 `ULyraPawnData` 형태다 |
| **12** | **§2-④의 근거 교체** — *"익스플로잇"* → *"비용 0 ＋ 세 곳의 전제 ＋ 되돌릴 손잡이가 이미 이름으로 있다"* |
| **13** | **함정 4v의 증상에 한 줄** (§1-①) — `MarkItemDirty`가 `MarkArrayDirty`를 부르고 `IDCounter`를 소비한다. *"아무 일도 안 일어난다"* 가 아니다 |
| **14** | **04-8에 낙관적 적용 주의 두 줄** (N-6) |
| **15** | **CLAUDE.md §2 해석 한 줄** (§2-②) — *"이름을 적는 것은 기록이지 승인이 아니다"* |
| **16** | 완료 조건 18의 32,767 → **32,763** / 변경 이력의 `StartingEquipment` 03-B → 03-C / 완료 조건 4 문구 (N-10·N-13) / `AddSubtree` 전제를 출처가 아니라 모양으로 (N-14) / 검사 0이 4보다 앞이라는 이유 한 줄 |

### 하지 않기로 한 것

- **`AddSubtree`를 구조체로** — §7-1이 인자를 안 늘린다. 상상한 확장점이다
- **`TryAutoEquip(In, PreferredSlot)`** — 소비자 0, 나중에 인자 하나
- **B안(슬롯째 복원)** — 검증을 두 곳으로 가르는 대가가 A안 ＋ `CanPlaceInSlot`보다 크다
- **`DirtyItem`/`DirtyArray` 래퍼** — 안에 넣을 일이 없다 (근거만 교체)
- **`Cap == SlotSize` 허용** — 이미 공짜로 막혀 있다
- **`MaxContainerDepth` ＋ 검사 7** — §4-6이 이미 조건부로 이름을 남겼다
- **이분 중간값 `int64` 캐스트** — 넣어도 되고 안 넣어도 된다. 6만 회가 필요하다
- **함정표의 `4a` 결번 채우기** — 색인 기능을 안 해친다
- **`GetInsertionOrder`를 03-A로** — 소비자가 03-C다 (9차·13차 판정 유지)

---

## 6. 인용

| 무엇 | 어디 | 확인한 것 |
|---|---|---|
| `MarkItemDirty`가 `MarkArrayDirty`를 부른다 | `FastArraySerializer.h:441-454` | A-1의 증상이 *"무해한 임시 객체"* 가 아니다 |
| `MarkArrayDirty`가 `ItemMap.Reset()` ＋ 키 증가 | `FastArraySerializer.h:457-465` | 매 호출 전체 델타 재스캔 |
| 복사 생성자·대입이 `ReplicationID`를 리셋 | `FastArraySerializer.h:307-322` | 복사본은 항상 `INDEX_NONE` → `IDCounter` 소비 |
| 클라 예측 시 `MarkArrayDirty` | `FastArraySerializer.h:459`, `993-994` | N-6의 근거 |
| **엔진이 `MarkItemDirty`를 감싼다** | `AbilitySystemComponent_Abilities.cpp:980-996` | §2-③의 기각 근거가 사실이 아니다 |
| 래퍼를 쓰라는 헤더 지시 | `AbilitySystemComponent.h:1112` | 강제 문법이 없는 것은 엔진도 같다 |
| `bool F(..., int32& Out)` 표준형 | `Parse.h:60` | B-1 처방의 형태 |
| `TOptional<int32>` 대안 | `GameSession.h:265-268` | 선례는 있으나 두 단계다 |
| 전역 기본값 ＋ 나중에 덮기 | `GameMapsSettings.h:100-102, 216-217` | §2-⑤ — *"다른 방법으로 지정되지 않았을 때"* |
| `UDeveloperSettings`가 콘텐츠 참조를 든다 | `UserInterfaceSettings.h:117, 133` | §2-⑤ |
| **"이 폰이 무엇을 들고 태어나나"의 자리** | `LyraPawnData.h:25-53` | §2-⑤의 최종 자리 |
| UI가 `Capacity <= 0`으로 구획을 거른다 | `05_Loot_04_InventoryUI.md:334` | §2-④ *"코드 0줄"* 확인 |
| `IsDataValid`가 컨테이너 규칙만 본다 | `05_Loot_00_ItemCore.md:224` | N-4 — `SlotSize` 하한이 없다 |
| 판정식을 흩뜨리지 않는다 (`CanFit` 선례) | `05_Loot_03_Inventory.md:1364` | N-2의 근거 |
| `Server_MoveEntry`를 안 여는 규칙 | `05_Loot_03_Inventory.md:1205` | §3-2 |
| `Server_ReorderEntry`를 04-B로 여는 규칙 | `05_Loot_03_Inventory.md:1085` | §3-2 — 같은 규칙이 세 번째에 적용되지 않았다 |
| `EP.Inv.Equip`은 04-A 소속 | `05_Loot_04_InventoryUI.md:45` | §3-2 |
| 획득 절차가 체크리스트로 읽힌다 | `05_Loot_DOCS.md:320` | N-9 |
| 깊이 상한을 조건부로 남긴 문장 | `05_Loot_DOCS.md:510` | §2-④의 세 번째 근거 |
| 소스 3종 직독 | `EPInventoryTypes.h` · `EPInventoryComponent.h`(131줄) · `.cpp`(189줄) | N-7 · N-8 · N-11 |
