# 검수 요청 3차 — 실무성 / 확장성 / 코드 수준

> 작성일: 2026-07-26
> 1차: `05_Loot_REVIEW_StructMigration.md` — struct 전환 (승인, 반영 완료)
> 2차: `05_Loot_REVIEW2_Request.md` / `_Answer.md` — 스택 폐지 + 부착물 (반영 완료, §11)
> 시점: Step 00 착수 직전. **구현 코드는 여전히 0줄**

---

## 0. 이번 검수의 성격

1·2차는 **설계 판단**(struct vs UObject, 스택 폐지)을 다뤘고 둘 다 결론이 났다.

이번은 다르다. **이 문서로 실제 코드를 쓸 수 있는가**를 본다. 사용자가 Step 00부터 직접 구현에 들어가므로 문서가 곧 구현 스펙이다.

> "설계가 옳은가"가 아니라 **"이대로 짜면 컴파일되고 동작하는가"** 를 봐달라.

---

## 1. REVIEW2 이후 바뀐 것

2차 검증 답변을 반영하던 중 사용자가 **배낭(중첩 컨테이너)** 을 확정해서 Step 03이 예상보다 크게 바뀌었다.

| 변경 | 영향 |
|---|---|
| 인벤토리 용량이 스칼라 하나 → **컨테이너별 독립 풀** | 본체 10칸 + 배낭 별도. 통합 안 함. 배낭 버리면 내용물 같이 나감 |
| 엔트리에 `ParentEntryId` / `SlotId` 추가 | 배낭·부착물·상자가 같은 표현이 됨 |
| `bFungible` 합치기 도입 | 현금뭉치·탄약상자. 2차 §3 수용 |
| 순서 규칙을 문서에서 걷어냄 | `RemoveEntry()` / `SetEntryCharges()` **내부 불변식**으로. 2차 §7-④ 수용 |
| GAME.md 인벤토리 절 **전면 개정** | 6슬롯 → 칸 합산 + 배낭 + 스택 없음 + 내구도 신설. 2차 ★2 수용 |

**2차 §9 결정 3건 중 2번(`Durability`/`MaxStack` 제거)은 사용자 판단으로 기각**했다. 대신 GAME.md에 내구도를 정식 편입해 "기획엔 없는데 필드만 있다"는 지적의 근본을 없앴다.

> 부수 정정: 열쇠·붕대의 "사용 횟수"는 `Durability`가 아니라 `Charges`다. `Durability`의 실사용자는 무기 하나다.

---

## 2. ★ 내가 이미 아는 결함 — 이것부터 확인해달라

한 세션에 변경이 몰려서, **1차 검증 때 지적받았던 결함 유형이 재발했다.**

### 2-1. 선언 없이 호출되는 함수 3개

```
GetEquippedBackpack()      05_Loot_03_Inventory.md:329,330 / 05_Loot_04_InventoryUI.md:97
FindFungibleEntry()        05_Loot_03_Inventory.md:234
RemoveChildrenRecursive()  05_Loot_03_Inventory.md:173 / 05_Loot_05_Equipment.md:139
```

03-2의 클래스 선언에 없다. 1차 때 `GetEntries()` / `PlaceholderIcon`이 정확히 같은 형태였다.

→ **이 유형이 더 있는지 전수로 훑어달라.** 시그니처가 문서마다 어긋난 곳도 함께.

### 2-2. 자기 규칙 위반 — `FindFungibleEntry`가 포인터를 돌려준다

03-2가 **"엔트리 포인터를 밖으로 내보내지 않는다"** 를 명시해놓고(`FindEntry`를 값 복사로 만든 이유가 그것이다), 03-3의 `AddItem`은 `FEPInventoryEntry*`를 받아 쓴다.

private이라 "밖"은 아니지만 **같은 위험 클래스**다. 규칙을 좁힐지, 함수 형태를 바꿀지 판단해달라.

### 2-3. 필드를 `FEPItemData`에 둘지 `UEPItemDefinition`에 둘지 규칙이 없다

```
FEPItemData (DataTable)       : SlotSize / MaxStack / bFungible / SellPrice / Rarity
UEPItemDefinition (DataAsset) : InitialCharges / ContainerCapacity / GrantedAbility
                                WorldMesh / Icon
```

**둘을 가르는 기준을 문서 어디에도 적지 않았다.** `bFungible`(DataTable)과 `ContainerCapacity`(DataAsset)가 갈린 데 원칙이 있는지 **나조차 설명하지 못한다.**

→ 원칙을 세워달라. 아니면 지금 한쪽으로 몰아야 하는지. 아이템 종류가 늘면 반드시 어긋나는 자리다.

### 2-4. `RemoveEntry()` ↔ `UnequipWeapon()` 상호 호출

2차 §7-④(불변식을 코드로)를 반영한 결과다.

```
InventoryComponent::RemoveEntry(id)
  → CombatComponent::UnequipWeapon()
      → InventoryComponent::SetEntryCharges(id, N)
          → MarkItemDirty + OnInventoryChanged.Broadcast()   ← ★
  → RemoveChildrenRecursive(id)
  → 엔트리 제거
```

- 두 컴포넌트가 **서로를 직접 부른다.** 계층상 허용되는 결합인가?
- **중간의 `Broadcast()`가 문제 아닌가** — 곧 삭제될 엔트리를 들고 UI가 갱신된다. Step 04가 그 콜백에서 목록을 통째로 다시 만든다
- 재진입 위험은 없다고 봤는데(종료한다) 확인해달라

### 2-5. 스탯 합산 준비가 서술만 있고 스펙이 없다

§7-3에 "부착물 전에 할 수 있는 유일한 준비 = `WeaponDef->` 직접 읽기를 `AEPWeapon` 캐시 한 곳으로 모으기"라고 적고 **Step 05에서 같이 하면 비용이 거의 없다**고 했는데, Step 05 문서에는 그 항목이 없다.

→ 넣어야 하나, 부착물 단계로 미뤄야 하나?

---

## 3. 검수 요청

### 3-1. 실무성 — 이대로 짜면 되는가

- Step 00~05를 순서대로 구현했을 때 **각 단계가 단독으로 컴파일되는가**
  > 1차 때 Step 01의 `AEPPickup : public IEPInteractable`이 Step 02의 인터페이스를 미리 참조해 컴파일 불가였다. **배낭이 Step 03에 들어오면서 앞뒤 의존이 다시 얽혔을 가능성이 높다.**
- **각 단계의 완료 조건을 그 단계만으로 검증할 수 있는가.** 특히 Step 03에 배낭이 들어왔는데 UI는 Step 04다 — 배낭 칸이 열렸는지를 Step 03만으로 확인할 방법이 있나?
- 코드 스니펫의 **null 체크·권한 체크 누락**
  > `UnequipWeapon()`이 `GetOwner<AEPCharacter>()->GetInventoryComponent()`를 가드 없이 체이닝한다
- `AddDefaulted_GetRef()` 참조를 `MarkItemDirty`까지 들고 있는데 안전한가
- **복제 순서**: `ParentEntryId`가 가리키는 부모보다 자식 엔트리가 먼저 도착하면 UI가 어떻게 되나. FastArray는 순서를 보장하지 않는다(`FastArraySerializer.h:54`)

### 3-2. 확장성

- 미정 #1(탄창) / #4(사망 드랍) / #6(재장전 소비)이 **현재 구조 위에서 정말 저렴하게 되는가**
  > 특히 사망 드랍은 "배낭 하나만 떨구면 서브트리가 통째로 나간다"고 적었는데, **본체 10칸에 있던 것들은?**
- §7-1 컨테이너 / §7-2 자판기 / §7-3 부착물이 지금 shape로 들어오는가
- 로드맵 5단계 DB — 평면 엔트리 배열이 정말 그대로 행이 되는가 (`NextEntryId` 저장은 반영했다)
- **경제 시스템**: 부착물 달린 무기의 판매가를 계산하려면 서브트리를 훑어야 한다. `SellPrice` + `Durability` + 자식 합산을 이 구조가 감당하나?

### 3-3. 코드 수준

- 문서의 C++ 스니펫이 **UE 관례에 맞는가** (명명, `const`, 참조/값, `TObjectPtr`)
- 프로젝트 기존 관례(`CLAUDE.md` §Conventions)와 어긋나는 곳
- 서버 권한 검사 위치가 일관된가 (`check(HasAuthority())` / `checkf` / 무검사가 섞여 있다)
- **과설계 지점 — 50줄로 될 걸 200줄로 쓴 곳이 있나.** 특히 §4-6이 이번에 크게 불어났다

### 3-4. 문서 자체

- `05_Loot_DOCS.md`가 **1007줄**이다. 스펙으로 쓰기에 여전히 읽히는가, 아니면 §4를 쪼개야 하나
- 마스터와 Step 문서에 **중복 서술**된 곳 (2차 §7-④로 순서 규칙은 걷어냈지만 다른 게 남았을 수 있다)
- `05_Loot_REVIEW*.md` 4개가 스펙 폴더에 섞여 있다. 분리해야 하나
- 배낭이 들어오면서 **stale해진 서술 전수 확인**
  > 1·2차 모두 반영 후 stale 참조가 남았고 매번 지적받았다. **이번이 세 번째다.** 패턴이면 프로세스 문제로 봐달라

---

## 4. 재론하지 않았으면 하는 것

| | 근거 |
|---|---|
| struct vs UObject | 1차 확정 |
| 스택 폐지 · 칸 합산 · `bFungible` | 2차 확정 |
| 부착물 깊이 1 / 상호작용 키 F / 본체 10칸 / 배낭 독립 풀 | **사용자 기획 결정** |
| `Durability` / `MaxStack` 존치 | 사용자가 2차 권고를 기각. **구현상 문제는 지적하되 존치 여부는 논의 대상이 아니다** |

---

## 5. 대상 파일

| 파일 | 비고 |
|---|---|
| `DOCS/GAME.md` | 인벤토리·장비 절 (이번에 전면 개정) |
| `DOCS/DOCS.md` | `:45` `:65` `:179` |
| `DOCS/Notes/05/05_Loot_DOCS.md` | 마스터 (1007줄) |
| `DOCS/Notes/05/LOOT_STATUS.md` | 확정 결정표 + 손대야 할 것 |
| `DOCS/Notes/05/05_Loot_00_ItemCore.md` ~ `05_Loot_05_Equipment.md` | 구현 스펙 6개 |
| `DOCS/Notes/05/05_Loot_REVIEW*.md` | 검증 기록 4개 (참고용) |

**구현 코드는 여전히 0줄이다. 지금 고치는 게 가장 싸다.**
