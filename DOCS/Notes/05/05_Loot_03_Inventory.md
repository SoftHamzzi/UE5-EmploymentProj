# Step 03 — Inventory ⟶ **둘로 쪼갰다 (2026-08-26, 15차)**

> **이 파일에는 내용이 없다.** 2692줄짜리 통합 문서가 아래 둘로 갈라졌다.
>
> | 문서 | 무엇 |
> |---|---|
> | **`05_Loot_03A_Core.md`** | 자료구조 · 컴포넌트 · 칸 합산 · 슬롯 정합 · 표시 순서 · 알림 · 검증 커맨드 |
> | **`05_Loot_03B_PickupDrop.md`** | 줍기(`OnInteract`) · 버리기(`Server_DropItem`) · 자동 착용 · 시작 장비 |

**진행 상태의 진실의 원천은 `LOOT_STATUS.md`와 `05_Loot_03_Inventory_STATUS.md`다.**

---

## 어디로 갔나

| 옛 절 | 간 곳 |
|---|---|
| 목표 · 완료 조건 | **둘 다.** 번호(1~20)는 **유지**했고 각자 자기 몫만 든다 |
| 체크포인트 (03-A/03-B 구간) | **03A** — 분할 근거(★★ 15차 절)가 거기 붙어 있다 |
| **03-1** 자료구조 | **03A** |
| **03-2** `UEPInventoryComponent` | **03A** |
| **03-3** `AddItem` — 칸 합산 | **03A** |
| **★ 03-A-부록** 기본 함수 열 | **03A — 신설.** 통합 문서에 본문이 없던 열 개다 |
| **03-4** `OnInteract` | **03B** |
| **03-5** 버리기 | **03B** |
| **03-6** 배낭 자동 착용 · `StartingEquipment` | **03B** |
| **03-7** 알림 | **03A** (8차 판정 — `FScopedInventoryNotify`를 03-3이 쓴다) |
| **03-8** 수명 | **03A** |
| **03-9** 검증 커맨드 | **03A** (`EP.Inv.Drop`만 03B에서 처음 돈다) |
| 함정표 | **대응 열로 라우팅.** A 44개 / B 16개, 양쪽에 걸리는 4개는 둘 다 |
| 변경 이력 | **둘 다** — 15차 이전은 공통 역사다 |

---

## 왜 쪼갰나 — 8차 결정을 뒤집는다

8차의 규칙은 *"파일을 쪼개지 않는다. 작업만 나눈다"* 였고 근거는 **경계에서 `RemoveEntry`가 갈려 stale이 세 번 났다**는 것이었다. **그 근거가 두 번 무너졌다.**

- **13차가 가운데 구간을 없앴다** — 셋에서 둘로 줄며 경계가 **하나**가 됐다
- **`RemoveEntry`가 더 이상 경계에 없다** — 제거 경로 셋(`RemoveEntry` · `RemoveEntryInternal` · `RemoveSelf`)이 **전부 03-B**다. 03-A는 `RemoveSelf`를 호출조차 하지 않는다

**그리고 쪼개지 않은 비용이 실측됐다.** 2692줄 안에서 **열 함수의 본문이 통째로 빠진 것을 아무도 못 봤다** — `FindEntry` · `ContainsEntry` · `RemoveSelf` · `AssignSortKey` · `KeySpace_Min` · `KeySpace_NextAbove` · `KeyOf` · `GetEntryInSlot` · `FindFungibleEntryId` · `GetEquippedEntryId`. 검수를 여덟 번 받는 동안 **가장 기본적인 것이 비어 있었다.**

> **★ `Review/` 하위 문서는 고치지 않았다.** 검수 요청·답변은 **그 시점의 기록**이고, 거기 적힌 `05_Loot_03_Inventory.md` 참조는 당시에 맞았다. 위 표가 그 참조들의 안내판 역할을 한다.

> **★ 옛 줄 번호 참조는 이제 맞지 않는다.** 살아 있는 문서(`05_Loot_04_InventoryUI.md` · `05_Loot_05_Equipment.md`)의 줄 참조는 새 파일로 옮겼다. `Mine/EquipmentSlots.md`의 것들은 9차 시점의 인용이라 그대로 두었다.
