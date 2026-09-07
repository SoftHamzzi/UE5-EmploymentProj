# 검수 요청 4차 — 3차 반영분 확인 + 남은 판단 5건

> 작성일: 2026-07-26
> 3차: `05_Loot_REVIEW3_Request.md` / `_Answer.md` (설계 결함 3건 + 미선언 9개 + 중복 9건)
> 시점: Step 00 착수 직전. **구현 코드 0줄**
> 대상: **3차 반영으로 바뀐 부분만.** 특히 `05_Loot_03_Inventory.md`(720줄)

---

## 0. 이번 요청의 성격

3차 답변을 전부 수용해 반영했다. 착수 전 결정 4건과 문서 재구조화까지 했다.

**두 가지를 봐달라.**

1. **§1·§2 수정이 실제로 구멍을 닫았는가** — 고치면서 새 구멍을 만들지 않았는지
2. **§3의 판단 5건** — 반영 중에 답이 안 나온 것들. 그중 하나는 **3차 권고 자체를 다시 묻는 것**이다

---

## 1. 3차 반영 요약

### 설계 결함 3건

| 3차 지적 | 반영 |
|---|---|
| §1 버리기가 잔탄을 잃는다 | `RemoveEntry(Id, &Out)`가 제거된 서브트리를 반환. **스냅샷을 얻는 유일한 방법이 제거하는 것**이 됐다 |
| §2 캐스케이드가 장착 무기를 건너뛴다 | `RemoveChildrenRecursive`가 `RemoveEntry`를 **재귀 호출**. 장착 검사·write-back이 노드마다 돈다 |
| §8-3 배낭 되줍기 시 내용물 증발 | `AddSubtree()` + `EntryId` 재매핑. `AEPPickup`이 `TArray<FEPInventoryEntry> Payload`를 든다 |

### 그 외

- 미선언 9개 전부 선언. `Defs`/`CombatComponent`는 멤버가 아니라 **매번 조회**(후자는 헤더 순환이라 `.cpp`에서 캐릭터 경유)
- `Entries.Owner = this;` 생성자에 명시
- `PostReplicatedReceive` 하나로 통일 + 서버 측 `FScopedInventoryNotify` → **Step 04 함정 #3 소멸**
- `InitialCharges`/`ContainerCapacity` → **DT**. `InitState(const FEPItemData&, FEPItemState&)`
- 배낭 자동 착용(03-6), `EquippedBackpackEntryId` 별도 필드
- `AddEntryCharges(Id, Delta)` 단일 델타 API (`ConsumeCharges` 폐기)
- `EP.Inv.Dump` / `Add` / `Drop` 신설
- `check()` → early return, `Cash_10000` `SellPrice=0`
- **문서 재구조화**: 마스터 1007 → 839줄. §4는 결정·근거만, Step 문서가 코드·함정표·엔진 인용을 단독 보유
- `Review/` 폴더 분리
- 미정 #4(사망 드랍) 선택지 → 확정("전부 드랍", GAME.md 기준), 미정 재번호

### 3차 권고 중 유일하게 기각한 것

**`Durability`/`MaxStack` 제거 — 사용자 판단.** 무기는 내구도를, 열쇠·붕대는 사용 횟수를 갖는 것이 기획 의도다. 대신 **`GAME.md`에 내구도를 정식 편입**해 "기획엔 없는데 필드만 있다"는 지적의 근본을 없앴다.

> 3차의 부수 지적은 맞았다 — 열쇠·붕대는 `Durability`가 아니라 `Charges`이고, `Durability`의 실사용자는 무기 하나다. 그렇게 구분해 적었다.

---

## 2. ★ 자체 발견 — 같은 결함을 또 만들었다

3차 §3이 미선언 심볼 9개를 지적했는데, **그것을 고치는 과정에서 4개를 새로 만들었다.** 착수 전에 자체 grep으로 찾아 수정했다.

| 심볼 | 처리 |
|---|---|
| `GetEntryById()` | `FindEntry()` 값 복사로 대체 |
| `SpawnPickupInFront()` | private 선언 추가 |
| `StartDropCooldown()` | `AEPPickup`에 선언 추가 |
| `Payload` | Step 01의 `State`와 **이름이 어긋나 있었다.** "Step 03에서 교체"로 양쪽 명시 |
| `InitPickup()` | Step 01은 `(FName, const FEPItemState&)`, Step 03은 `(TArray&&)` — **시그니처 불일치** 해소 |

**세 차수 연속 같은 유형이다.** 3차 §11이 "중복이 원인"이라 했고 그건 맞았지만, 이번 건은 중복이 아니라 **"코드 조각을 쓸 때 선언부를 같이 안 고치는 것"** 이다. 프로세스 문제로 봐야 하는지, 아니면 착수하면 컴파일러가 잡아주니 넘어가도 되는지 판단해달라.

---

## 3. 판단 요청 5건

### 3-1. ★ `UnequipWeapon`의 write-back이 델타 API에 절대값을 억지로 끼운다

3차 §4가 `AddEntryCharges(Id, Delta)` 하나로 합치라고 했고 수용했다(`ConsumeCharges`는 그 음수다). 그런데 **write-back만은 본질적으로 "덮어쓰기"** 다.

```cpp
// 지금 이렇게 됐다 — 읽고, 빼고, 델타로 넘긴다
FEPInventoryEntry E;
if (Inv->FindEntry(EquippedId, E))
    Inv->AddEntryCharges(EquippedId,
        FMath::RoundToInt(AS->GetAmmo()) - E.State.Charges);
```

**3차 §4가 없애려던 "읽고·더하고·쓰는 것이 함수 경계를 넘나드는 것"이 여기서 그대로 재현된다.** 게다가 그 사이에 값이 바뀌면 어긋난다(지금은 서버 단일 스레드라 안 바뀌지만, 근거가 그것뿐이다).

**질문:** `SetEntryCharges(Id, Value)`를 되살려야 하나? 3차는 "부호만 다른 두 함수를 두면 어느 쪽이 클램프를 하는지 갈린다"고 했는데, `Set`과 `Add`는 부호 차이가 아니라 **의미가 다르다**. 둘 다 두되 `Add`가 `Set`을 호출하게 하면 클램프 지점은 하나로 유지된다.

### 3-2. `AddSubtree`와 `bFungible`의 관계를 흐릿하게 적었다

문서에 이렇게 썼다.

> `bFungible` 합치기는 이 경로에 **적용하지 않는다.** (…) 단일 아이템 획득도 `AddSubtree`로 통일하면 경로가 하나가 된다 — 그 안에서 원소가 1개면 `AddItem`으로 위임한다.

**두 문장이 서로 어긋난다.** 원소 1개인 탄약상자가 `AddSubtree` → `AddItem`으로 위임되면 **합쳐진다.** 그게 맞는 동작이라고 보지만(단일 fungible 아이템은 합쳐야 한다), 그러면 "이 경로에 적용하지 않는다"는 서술이 틀렸다.

정확히는 **"루트가 자식을 가지면 합치지 않는다"** 여야 할 것 같은데, 자식 없는 fungible 루트와 자식 있는 루트를 무엇으로 가를지 — `In.Num() == 1`인지 `bFungible`인지 — 판단해달라.

### 3-3. `RemoveEntry` 재귀 + 스코프 가드가 겹친다

```
RemoveEntry(배낭)                    Guard 진입 (depth 1)
  → UnequipWeapon
      → AddEntryCharges              Guard 진입 (depth 2) → 이탈 (depth 1)
  → RemoveChildrenRecursive
      → RemoveEntry(자식)            Guard 진입 (depth 2) → 이탈 (depth 1)
  → 자신 제거                        Guard 이탈 (depth 0) → 여기서 1회 Broadcast
```

의도대로면 배낭 하나 버릴 때 알림이 **정확히 1회** 나간다. 확인해달라 — 특히 **재귀 중 `Entries.Items` 재할당**이 상위 프레임의 지역 상태를 깨지 않는지. `RemoveChildrenRecursive`가 자식 목록을 `TArray<int32>`로 먼저 뜨므로 안전하다고 봤는데, 상위 `RemoveEntry`가 들고 있는 `Snapshot`은 값이라 무관하고, `OutRemoved` 포인터는 호출자 스택이라 무관하다는 판단이다.

### 3-4. Step 03이 720줄이 됐다

Step 00(524) / 01(398) / 02(279) / **03(720)** / 04(333) / 05(272).

배낭 + 서브트리 + `bFungible` + 불변식 + 커맨드가 전부 여기 있다. 3차 §11이 "길이가 아니라 중복이 문제"라 했으니 길이 자체는 지적이 아닐 수 있는데, **한 단계에 담기에 너무 많은 것 아닌지** 판단해달라.

쪼갠다면 경계가 어디인가 — `03a 인벤토리 코어` / `03b 배낭·서브트리`? 아니면 배낭을 Step 04 뒤로 미루는 게 나은가? (완료 조건이 서로 얽혀 있어 후자는 어려워 보인다)

### 3-5. Step 03의 완료 조건 12개를 콘솔 커맨드만으로 전부 확인할 수 있나

`EP.Inv.Dump` / `Add` / `Drop`을 신설했지만, UI가 없는 상태에서:

- "다른 클라이언트에 내 인벤토리가 복제되지 않는다"(`COND_OwnerOnly`) — 어떻게 확인하나. `Dump`를 클라에서 돌리면 되나?
- "`EntryId`가 재번호되지 않는다" — `Dump`로 되지만 **여러 번 줍고 버려야** 드러난다. 커맨드 하나로 시나리오를 돌리는 `EP.Inv.Stress` 같은 게 필요한가?
- "픽업이 그대로 남는다" — `Dump`는 인벤토리만 본다. 월드 픽업을 보는 수단이 없다

---

## 4. 재론하지 않았으면 하는 것

| | 근거 |
|---|---|
| struct vs UObject / 스택 폐지 / 칸 합산 / `bFungible` 도입 | 1·2차 확정 |
| 배낭 독립 풀 / 자동 착용 / 부착물 깊이 1 / 상호작용 F / 본체 10칸 | 사용자 기획 결정 |
| `Durability` / `MaxStack` 존치 | 사용자가 3차 권고를 기각. **구현상 문제는 지적하되 존치 여부는 논의 대상이 아니다** |
| DT/DA 두 계층 유지 | 3차 §5 권고대로 확정. 원칙 두 줄로 예외 없음 |

---

## 5. 대상 파일

| 파일 | 줄 | 이번 변경 |
|---|---|---|
| `05_Loot_03_Inventory.md` | 720 | **전면.** §1·§2·§8-3 수정 + 03-6/03-7/03-9 신설 |
| `05_Loot_DOCS.md` | 839 | §4-6~4-9 재구조화(코드 제거), 미정 재번호 |
| `LOOT_STATUS.md` | 136 | 확정표 갱신, ★1·★2 stale 제거, 변경 이력 |
| `05_Loot_00_ItemCore.md` | 524 | DT/DA 원칙, `InitState` 시그니처, DT 6행 + `SellPrice` |
| `05_Loot_04_InventoryUI.md` | 333 | `UEPContainerPanel` 신설, `PostReplicatedReceive` |
| `05_Loot_05_Equipment.md` | 272 | 순서 규칙 제거(위임), `GetMaxAmmo()`, null 가드 |
| `05_Loot_01_Spawner.md` | 398 | `Payload` 교체 예고 |
| `DOCS/GAME.md` | — | 인벤토리·장비 절 (3차에서 개정) |
