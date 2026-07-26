# 검증 답변 — `UEPItemInstance`(UObject) 폐기, struct 전환 제안

> 작성일: 2026-07-26
> 대상 제안: 아이템 런타임 상태를 `UObject`(`UEPItemInstance`/`UEPWeaponInstance`) + `UEPItemInstanceSubsystem` + `int32` 핸들 구조에서
> 순수 `USTRUCT`(`FEPItemStack`) + 값 복사 구조로 전환
> 시점: Step 00 착수 직전, 05_Loot 문서 7개 작성 완료, 구현 코드 0줄
> 이 문서는 **검증 기록**이다. 확정 결정은 `LOOT_STATUS.md`, 설계는 `05_Loot_DOCS.md`에 반영한다.

---

## 결론 요약

**동의한다.** 단, 제안된 논거 5개 중 4번("함정이 전부 성립 불가가 된다")은 과장이며,
파급 범위 목록에서 **4건이 빠졌다.** 착수 전에 **결정해야 할 항목이 2개** 있다.

| 구분 | 판정 |
|---|---|
| A-1 보존 상태가 Ammo/Durability뿐인가 | ✅ 그렇다 (조건 1개 — 자판기 상자) |
| A-2 `FInstancedStruct` NetSerialize 지원 | ✅ 사실. 단 **주장보다 비싸다** |
| A-3 FastArray 중첩 USTRUCT 델타 | ✅ 정상 동작. **의도된 사용법**이며 공짜 이득 |
| B-1 UObject를 유지해야 할 시나리오 | ❌ 프로젝트 범위 안에 없다 |
| B-2 처음부터 `FInstancedStruct`를 쓸 것인가 | ❌ 아니다. YAGNI |
| B-3 픽업 잔탄 노출 | ⚠️ **실제 후퇴. 수용하면 안 된다** |
| 전환 자체 | ✅ 지금이 가장 싼 시점 (근거는 제안보다 더 강함) |

---

## A. 사실 확인

### A-1. 보존해야 할 상태가 정말 Ammo/Durability뿐인가 — **그렇다** (조건 1개)

`GAME.md` · `DOCS.md`를 훑은 결과, per-instance 상태를 요구하는 미래 기능은 사실상 없다.

| 기능 | per-instance 상태 필요? | 근거 |
|---|---|---|
| 돈 (인벤토리 아이템) | ✗ | 순수 스택. `Quantity`가 전부 |
| 퀘스트 수집 (이력서·자격증·USB) | ✗ | **ItemId 집합 판정.** "모두 모으면 취업 성공" — 개체 구분 불필요 |
| 아이템 판매 / 경제 | ✗ | 가격은 Definition, 상태 보정은 `Durability` (이미 있음) |
| 탈출 정산 → DB (로드맵 5단계) | ✗ | struct가 **압도적으로** 유리 (`FJsonObjectConverter` 한 줄) |
| 시체 루팅 | ✗ | 값 복사. UObject일 때가 오히려 "캐릭터 파괴 시 핸들 주인" 문제를 만들었음 |
| 컨테이너 §7-1 | ✗ | 내용물은 **액터**가 소유. 액터의 `TArray<FEPItemStack>`이면 끝 |
| 자판기 상자 §7-2 | **조건부** | 아래 |

**부착물(attachment) 시스템이 `GAME.md` 어디에도 없다**는 것이 결정적이다.
Lyra/타르코프가 `UItemInstance`를 UObject로 두는 제1 이유가 부착물·모듈 트리인데,
이 프로젝트는 무기 속성이 전부 Definition 레벨(Damage / FireRate / Recoil / Spread / MaxAmmo / FireMode)이고
초기 무기 2종이다. **제외 목록에조차 올라와 있지 않을 만큼** 고려 밖이다.

#### 조건 — 자판기 상자

`05_Loot_DOCS.md` §7-2는 "상자 배출 → 플레이어가 열면 내용물 공개"이고
"§7-1 컨테이너의 특수 케이스(검색 시간 0, 1회용, 파괴됨)"라고 못박았다.

- 상자가 **월드 액터로만 존재**하면 → struct로 충분하다.
- 그러나 익스트랙션 슈터에서 **"상자를 안 열고 가방에 넣어 탈출한다"** 는 자연스러운 확장이고,
  그 순간 인벤토리 엔트리가 **가변 길이 컨테이너를 중첩**해야 한다.
  이것이 struct가 실제로 아픈 유일한 시나리오다.

**조치:** 지금 결정할 필요는 없다. 다만 §7-2에 **"상자는 인벤토리에 들어가지 않는다"** 를 명시해 두면
나중에 이 지점이 조용히 뚫리는 것을 막는다.

---

### A-2. `FInstancedStruct` NetSerialize — **사실이지만, 주장보다 비싸다**

```
CoreUObject/Public/StructUtils/InstancedStruct.h:286     WithNetSerializer = true            ✓ 확인
Engine/Private/UnrealEngine.cpp:324                      NetSerializeScriptStructDelegate 바인딩  ✓ 확인
```

네이티브 직렬화가 없는 내부 struct도 **엔진 시작 시 델리게이트가 바인딩되므로 그냥 동작한다.**
여기까지는 제안이 맞다. 그런데 구현을 보면:

```cpp
// InstancedStruct.cpp:518 / 536
Ar << SerializedScriptStruct;    // UScriptStruct* 를 PackageMap 통해 매번 전송

// UnrealEngine.cpp:337 (델리게이트 람다 내부)
RepLayout->SerializePropertiesForStruct(...);   // ★ 델타가 아니라 전량 직렬화
```

세 가지가 따라온다.

1. 엔트리마다 `UScriptStruct*` **오브젝트 참조**가 실려 나간다.
2. **`FInstancedStruct`를 FastArray 아이템 안에 넣는 순간 그 페이로드는 프로퍼티 델타를 잃는다.**
   → A-3에서 공짜로 얻는 이득을 정확히 반납한다.
3. 클라에서 타입이 바뀔 때마다 `InitializeAs()` → **힙 할당**.

**결론:** "이행이 저렴하다"는 하향 조정이 필요하다.
**UObject 대비** 싸다는 뜻이지 **절대적으로** 싸지 않다.
이행 시 소스 변경(인벤토리·픽업·장비·UI 전 접점) + **복제 특성 변화** + **세이브 포맷 파괴**가 동시에 온다.

---

### A-3. `FFastArraySerializerItem` 안에 중첩 USTRUCT — **정상 동작. 오히려 의도된 사용법**

```
Net/Core/Classes/Net/Serialization/FastArraySerializer.h:218

  "Delta Serialization for inner structs is now enabled by default. That means that
   when a ReplicationKey changes, we will compare the current state of the struct to
   the last sent state, tracking changelists and only sending properties that changed
   exactly like the standard replication path."
```

즉 `Entry.Stack.Ammo`가 30 → 29일 때 **`Ammo`만** 나간다. 엔트리 전체가 아니다.
제안이 **자동으로 이득을 본다.**

#### 제약 (FastArraySerializer.h:721-728) — 문서에 박아 둘 값어치가 있다

- 아이템 배열은 `FFastArraySerializer` 안의 **최상위 UPROPERTY**여야 한다
- `RepSkip` 금지
- **복제되는 아이템 배열은 하나뿐**이어야 한다 (여러 개 두려면 나머지는 전부 `RepSkip`)
- 직렬화기도, 아이템 배열도 **정적 배열 안에 중첩 금지**

전부 자명하게 충족되지만 명시해 둔다.

#### ★ 새로 생기는 함정 — `MarkItemDirty` 수동 호출

`MarkItemDirty(Item)`은 여전히 수동이다(`FastArraySerializer.h:441`).

UObject 설계에서는 **탄약이 배열 밖에 있어서 이 질문 자체가 없었다.**
이제 `Entry.Stack.Ammo`를 직접 쓰고 `MarkItemDirty(Entry)`를 안 부르면 **조용히 복제가 안 된다.**
write-back 경로(`UnequipWeapon`)가 정확히 여기에 해당한다.

→ **Step 03 · Step 05 함정 표에 추가할 것.**

---

## B. 설계 반론

### B-1. UObject를 유지해야만 하는 시나리오 — **범위 안에 없다**

§7-1 컨테이너 / §7-2 자판기 / 퀘스트 수집 / 탈출 정산 / 시체 루팅 전부 struct로 된다.
그중 **정산과 시체 루팅은 struct가 명백히 낫다** — 전자는 DB 직렬화, 후자는 소유권 이관이 값 복사가 되므로.

제안 측에서 세우지 않은 반론 3개와 그 결말:

| 반론 | 결말 |
|---|---|
| 아이템이 **행동**을 가져야 한다면? (인벤토리 안에서 타는 수류탄 도화선, 델리게이트 바인딩, GA의 타깃) | struct는 못 한다. 하지만 이 프로젝트의 그런 것들은 전부 이미 ASC나 스폰된 액터에 있다. **이론적** |
| BP에서 아이템 타입별 `OnUsed` 오버라이드 | 다형성을 Definition(`UPrimaryDataAsset`)에 남기는 제안 그대로면 **커버된다** |
| 디버깅 — UObject는 `obj list`에 뜬다 | struct는 프로퍼티 창으로만 보인다. **사소** |

**차단 요인 없음.**

---

### B-2. struct 비대화 / 처음부터 `FInstancedStruct`를 써야 하나 — **아니오**

`FInstancedStruct`의 가치는 **이종(異種) 페이로드**다.
지금 페이로드 형태는 정확히 하나고, 초기 범위 안에 두 번째가 없다.
지금 도입하면 A-2의 비용(프로퍼티 델타 상실 + 엔트리당 struct 참조 + BP/DB 복잡도)을 **사놓고 아무것도 못 받는다.**

#### 언제 비대화가 실제로 아픈가 — 바이트가 아니다

`FEPItemStack`은 현재 24바이트, 6슬롯, `COND_OwnerOnly`. **대역폭 문제가 될 일이 없다.**
진짜 비용은 **모든 아이템이 다른 타입의 필드값을 지불한다**는 것이고,
필드 4개에서는 보이지 않다가 **타입 전용 필드가 8~10개쯤** 되면 추해진다.

> **전환 기준 (문서에 남길 것):**
> **세 번째 아이템 카테고리가 자기 전용 필드를 요구하면, 그때 `FInstancedStruct`로 간다.**
> 가장 유력한 후보는 A-1의 **자판기 상자 내용물**이다.

---

### B-3. 픽업이 Stack 전체를 복제하면 잔탄이 노출된다 — **실제 후퇴다. 수용하면 안 된다**

`GAME.md`는 **정보 은폐를 두 번 명시적으로 못박는다.**

- 플레이어 수 비공개 (타르코프 방식)
- 사망 여부를 PlayerState에 저장하지 않음
- 킬 피드백은 킬러에게만 Client RPC

그런데 바닥 무기 잔탄이 복제되면,
치트 클라이언트가 **릴러번시 범위 내 모든 픽업의 잔탄을 읽어 "어디서 얼마 전에 교전이 있었는지"를 추론**한다.
`12/30`짜리 라이플이 바닥에 있다 = **여기서 누가 죽었다.**
명시된 설계 기둥을 **사고로** 뒤집는 것이다.

#### 채택안 — 2-struct 분리

```cpp
// 정체성 + 개수 — 클라가 알아야 하는 것
USTRUCT()
struct FEPItemStack
{
    GENERATED_BODY()
    UPROPERTY() FName ItemId;
    UPROPERTY() int32 Quantity = 1;
};

// 상태 — 소유자만 알아야 하는 것
USTRUCT()
struct FEPItemState
{
    GENERATED_BODY()
    UPROPERTY() int32 Ammo       = 0;
    UPROPERTY() float Durability = 100.f;
};

// AEPPickup
UPROPERTY(Replicated) FEPItemStack Stack;    // 복제
UPROPERTY()           FEPItemState State;    // 서버 전용

// FEPInventoryEntry — COND_OwnerOnly라 둘 다 복제해도 안전
int32 SlotIndex;
FEPItemStack Stack;
FEPItemState State;
```

방금 통합한 것을 다시 쪼개는 것처럼 보이지만, **쪼개는 축이 다르다.**
"클라가 알아도 되는 것 / 아닌 것"이라는 **실재하는 경계**이고,
양쪽 다 순수 값 타입이라 제안의 이점(값 복사 이관)은 **하나도 잃지 않는다.**
그리고 누출이 *규율*이 아니라 *구조*로 불가능해진다.

#### 기각한 대안

| 대안 | 기각 사유 |
|---|---|
| 픽업이 Stack 전체 복제, 노출 수용 | 정보 은폐 기둥과 정면 충돌 |
| 픽업에 서버 전용 Stack을 하나 더 | **"수량의 진실이 두 곳"** — 제안이 죽이려던 바로 그 버그류를 되살린다 |
| struct에 커스텀 `NetSerialize`로 필드별 조건 | 과잉. 4바이트 아끼자고 직렬화기를 손으로 쓴다 |

---

## C. 열린 문제 — 답

### C-1. `SchemaVersion` → **삭제**

아이템의 속성이 아니라 **세이브 포맷의 속성**이다.
아이템별 버저닝은 개체가 독립적으로 저작될 때(모드, UGC) 하는 것이고,
여기서는 한 세이브 안의 모든 아이템을 **같은 빌드가 쓴다.**
`USaveGame` / DB 행 **봉투에 하나만** 둔다.

> 현재 `EPItemInstance.h:31`에 `UPROPERTY() int32 SchemaVersion = 1;`로 존재
> → **Step 00 "지울 것" 행**이 된다.

---

### C-2. `InstanceId (FGuid)` → **런타임에서 삭제. DB 저장 시점 발급**

**대역폭은 이유가 아니다.** 16바이트 × 6슬롯 = 96바이트, `COND_OwnerOnly`. 아무것도 아니다.

**진짜 이유: GUID는 병합·분할에서 의미가 정의되지 않는다.**

- 탄약 30 + 20을 합치면 **누구의 GUID가 살아남는가?**
- 50을 25/25로 쪼개면 **어느 쪽이 원본인가?**

답이 없다는 것이 이 개념이 **스택에 속하지 않는다**는 신호다.
비스택 아이템에서는 정합하지만, **읽는 코드가 없다.**

| 용도 | 실제 식별자 |
|---|---|
| 인벤토리 UI / 드랍 · 장착 RPC | `SlotIndex` |
| 복제 | FastArray `ReplicationID` |
| DB 스태시 | 행 PK (저장 시 발급) |
| 거래 기록 (`DOCS.md` 5단계) | 감사 로그 PK. 런타임 GUID 불필요 |
| 복사 탐지 안티치트 | 유일하게 진짜 필요. **범위 밖** |

필요해지면 POD struct에 필드 하나 추가 = **세이브 포맷 범프**이지 아키텍처 변경이 아니다.

---

### C-3. 같은 ItemId 스택 두 개 구분 — **이미 해결됐고, 생겨도 감당된다**

- **한 인벤토리 안**: `SlotIndex`가 이미 구분한다.
  (FastArray가 클라 배열 순서를 보장하지 않아 **이미 넣기로 확정된 필드**다.) → 해결됨
- **월드 / 시간 축**: 퀘스트는 ItemId 집합 판정이라 불필요.
  필요해지는 것은 고유 명명 아이템("Bob의 소총")이나 아이템 귀속 상태("이 USB는 3/5") —
  **둘 다 `GAME.md`에 없다.**
- **생긴다면**: `int32 UniqueId`(서버 발급, 필요한 아이템만 nonzero) 4바이트면 끝.

> struct가 못 하는 것은 **행동**이지 **정체성**이 아니다.

---

## D. 판단 — 동의. 다만 논거 4번은 과장

### D-1. 시점 근거는 제안보다 더 강하다

"코드 0줄이라서 싸다"가 아니다.
**현재 문서가 UObject 설계의 복잡도를 "구현자가 어기면 안 되는 규칙"으로 인코딩해 두었기 때문에** 싸다.

- 이관 프로토콜 (소유권 공백 없는 순서)
- `EndPlay` 2곳의 `Destroy(Handle)`
- 핸들 비우고 `Destroy()` (안 하면 잔탄 소실)

규칙은 **컴파일러가 아니라 규율이 강제한다.**
아무도 아직 지킬 필요가 없을 때 규칙을 지우는 것이 가장 싸다.

### D-2. ★ 논거 4번 "함정이 전부 성립 불가가 된다"는 과장이다

**하나가 살아남고, 형태만 바뀐다.**

> `LOOT_STATUS.md:76` — `EquippedInstanceHandle`을 인벤토리 `EquippedHandle`과 **별개**로 둔 이유는
> "버리기에서 인벤토리가 먼저 비워져도 write-back 대상을 잃지 않게" 였다.

`EquippedSlotIndex` 하나로 합치면 **이 위험이 돌아온다.**
드랍 경로가 슬롯 N을 비운 뒤 `UnequipWeapon()`이 N에 잔탄을 쓰면 **소실.**

- 이전: **자료구조가 막아줬다** (핸들이 두 개라 인벤토리가 비어도 대상이 살아있음)
- 이후: **명시적 순서 규칙이 필요하다** — "write-back을 슬롯 비우기보다 **먼저**"

→ **Step 05에 들어가야 하며, struct 전환으로 *새로 생기는* 유일한 함정이다.**

### D-3. 비용도 0은 아니다

문서 7개 편집 + STATUS 확정표 **~8행 삭제 / ~6행 추가.**
이 문서군에서 최근에만 **stale 참조가 두 번** 남았으므로,
마무리에 **일관성 grep 패스**를 예산에 넣는다.

---

## E. 빠뜨린 파급 4건

### ★ E-1. `05_Loot_00_ItemCore.md` — 스택 아이템도 Definition이 필요해질 수 있다 (제일 중요)

현재 §00-8의 논리는 **"스택 아이템은 인스턴스를 만들지 않으므로 Definition 에셋이 필요 없다"** 이다.

그런데 `virtual InitStack(FEPItemStack&) const`는 **스택/비스택 구분 없이 모든 아이템에 대해 불린다.**

| 선택지 | 결과 |
|---|---|
| (a) 모든 아이템이 Definition을 가진다 | `Ammo_762` / `Bandage` / `Scrap_Paper`도 DA 필요 → `Definitions=4` → **`7`** |
| (b) **Definition이 없으면 `InitStack`을 건너뛴다** ← 추천 | 필드가 기본값으로 남고, 스택 아이템에는 그것이 정답. `Definitions=4` 유지 |

**Step 00에서 명시적으로 결정해야 한다.** 안 하면 `Definitions=?` 검증 기준이 붕 뜬다.

**부수 — `DA_Resume`은 살아남는다.** 사유만 바뀐다.

| | 사유 |
|---|---|
| 이전 | "MaxStack == 1 → 인스턴스 생성" 분기 검증용 |
| 이후 | **베이스 `InitStack` + AssetManager 타입 통일** 검증용 (무기 이외 Definition이 로드되는지 확인하는 유일한 케이스) |

→ 직전 세션의 ★1 수정(`DA_Resume` 요구, `Definitions=4`)은 **유효하다.**

---

### E-2. `LOOT_STATUS.md` — 확정표에서 무효화되는 것이 5행이 아니라 **7행**

조건부로 만들 인스턴스가 없어지므로 **결정 자체가 증발**하는 행이 있다.

| 행 | 현재 내용 | 조치 |
|---|---|---|
| `:30` | 인벤토리 복제 — "`UEPItemInstance`는 서버 전용" | 수정 |
| `:32` | 픽업 액터 (참조) — 서버 전용 `int32 InstanceHandle` | **삭제** |
| `:38` | 장비 슬롯 — `EquippedHandle`(int32) | 수정 → `EquippedSlotIndex` |
| `:40` | 식별자 — `int32` 핸들, `FGuid InstanceId`는 DB 영속용 | **삭제** (C-2) |
| `:41` | 인스턴스 소유 — `UEPItemInstanceSubsystem`이 유일한 강참조 | **삭제** |
| `:42` | 인스턴스 생성 — Definition의 virtual `CreateInstance()` | 수정 → `virtual InitStack()` |
| `:45` | 인스턴스 생성 대상 — `MaxStack == 1`인 아이템만 | **삭제** (결정 자체가 증발) |

추가로 `:50` (인스턴스 수명 — `EndPlay`에서 `Destroy(Handle)`) 도 **삭제**된다.

---

### E-3. `05_Loot_04_InventoryUI.md` — "거의 무변경"이 아니다. 2곳

- **`:33`** `| 장비 슬롯 UI | Step 05에서 EquippedHandle 표시만 추가 |` → `EquippedSlotIndex`
- **`:197`** "`Ammo_762` / `Bandage` 같은 신규 행은 **Definition 에셋조차 없다(스택 아이템이라 필요 없음)**"
  → **E-1의 결정에 직접 걸린다**

#### 그리고 이것은 제안에 유리한 발견이다

현재 설계에서는 **가방 속 두 번째 소총의 잔탄을 UI에 표시할 방법이 없다.**
(핸들 → 서버 전용 인스턴스 → 클라가 못 읽음.)
GAS `Ammo` 어트리뷰트는 **장착 무기 하나만** 커버한다.
→ **struct 전환이 이 구멍을 공짜로 메운다.**

---

### E-4. `DOCS/DOCS.md` 3곳 — 포트폴리오 문서다

| 위치 | 내용 |
|---|---|
| `:45` | `FTableRowBase` + `UItemDefinition(UPrimaryDataAsset)` + `ItemInstance` |
| `:65` | "Data-driven: `DataTable Row + ItemDefinition + ItemInstance` 구조로 무기/아이템 정의" |
| `:179` | "Data Driven \| DataTable(Row) + ItemDefinition(DataAsset) + ItemInstance 분리" |

코드가 `FEPItemStack`인데 문서가 `ItemInstance`라고 하면 안 된다.

**다만 면접에서는 오히려 유리한 소재다.**
"왜 UObject를 안 썼는가 — 값 타입 + FastArray 내부 델타 + DB 직렬화"는
**A-3의 엔진 소스까지 인용 가능한** 답변이다. 표현만 갱신한다.

> 참고: `DOCS.md:184`가 가리키는 `DOCS/Mine/Item.md`는 **존재하지 않는다** (`Mine/`에 `TickGroup.md`뿐).
> 이번 건과 무관한 기존 문제이므로 손대지 않았다.

---

### E-5. `05_Loot_02_Interaction.md` — 확인 결과 **무변경 맞음**

`Handle` / `핸들` / `Instance` 검색 결과 **0건.**
핸들 이관은 `05_Loot_DOCS.md` §4-4의 `Server_Interact` 8단계 절차에만 있고,
그 문서는 이미 수정 범위에 포함돼 있다.

---

## F. 착수 전 결정해야 할 2가지

나머지는 이 두 결정에서 **기계적으로 따라 나온다.**

| # | 결정 사항 | 추천 | 파급 |
|---|---|---|---|
| 1 | **B-3** — 픽업 잔탄 노출을 막을 것인가 | **막는다.** `FEPItemStack`(정체성) + `FEPItemState`(상태) 2-struct 분리 | `05_Loot_DOCS.md` §4-4/§4-6/§4-7, `01_Spawner`, `03_Inventory` |
| 2 | **E-1** — 스택 아이템도 Definition을 가지는가 | **아니다.** Definition이 없으면 `InitStack` 건너뜀 | `00_ItemCore` §00-8/§00-9, `04_InventoryUI:197` |

---

## G. 확정 시 수정 범위 (전체)

| 파일 | 조치 |
|---|---|
| `05_Loot_DOCS.md` | §4-1 인스턴스 레지스트리 절 **삭제**, §4-4 / §4-6 / §4-7 / §4-8 핸들 서술 → 값 복사, §7-2에 "상자는 인벤토리에 안 들어감" 명시, §8 확정표 |
| `05_Loot_00_ItemCore.md` | 인스턴스 서브시스템 · 핸들 절 삭제, 클래스 계층 → `FEPItemStack` / `FEPItemState`, `virtual InitStack()`, **E-1 결정 반영**, `SchemaVersion` / `InstanceId` 제거 행 추가 |
| `05_Loot_01_Spawner.md` | `AEPPickup`: `InstanceHandle` → `Stack`(복제) + `State`(서버 전용) |
| `05_Loot_02_Interaction.md` | **무변경** (E-5) |
| `05_Loot_03_Inventory.md` | 엔트리에 `Stack`/`State` 내장, `AddItem` 시그니처, 함정 3건 삭제, **`MarkItemDirty` 함정 추가** (A-3) |
| `05_Loot_04_InventoryUI.md` | `:33` `EquippedSlotIndex`, `:197` E-1 결정 반영 |
| `05_Loot_05_Equipment.md` | 핸들 2종 → `EquippedSlotIndex`, **★ write-back 순서 규칙 추가** (D-2) |
| `LOOT_STATUS.md` | 확정 결정표 **7~8행 삭제 / 신규 행 추가**, "손대야 할 것" 표 갱신 |
| `DOCS/DOCS.md` | `:45` / `:65` / `:179` `ItemInstance` 표현 갱신 |

마무리에 **일관성 grep 패스** 필수 — `InstanceHandle` / `CreateInstance` / `ItemInstanceSubsystem` / `EquippedHandle` / `SchemaVersion` / `InstanceId` 잔재 확인.

---

## H. 반영 결과 (2026-07-26) — **완료**

전환은 승인됐고, **같은 시점에 두 번째 결정이 겹쳤다: 스택 폐지(타르코프식) + 칸 수 합산.**
두 변경을 함께 문서에 반영했다. 아래는 이 문서의 미결 항목이 어떻게 닫혔는지다.

### F의 착수 전 결정 2가지

| # | 결정 | 결과 |
|---|---|---|
| 1 | **B-3** 픽업 잔탄 노출 | **채택.** 다만 2-struct가 아니다 — `Quantity`가 없어져 `FEPItemStack`이 `FName` 하나를 감싼 껍데기가 되므로, **픽업이 `FName ItemId`만 복제하고 `FEPItemState`는 서버 전용**으로 단순화했다. 경계는 그대로다 |
| 2 | **E-1** 스택 아이템도 Definition을 가지는가 | **질문이 증발했다.** 스택이 없어 "상태 없는 아이템" 범주가 사라졌고, 애초에 바닥 픽업의 `WorldMesh`와 UI의 `Icon`이 Definition에만 있어 예외가 성립하지 않았다 → **모든 아이템이 Definition을 가진다.** `Definitions=4` → **`7`** |

### C의 열린 문제

| # | 답변 | 상태 |
|---|---|---|
| C-1 `SchemaVersion` | 삭제 | 채택 |
| C-2 `InstanceId`(FGuid) | 런타임 삭제 | **결론은 채택, 논거 하나는 무효.** "병합·분할에서 의미가 정의되지 않는다"는 **스택이 없어 성립하지 않는다.** "읽는 코드가 없다"만으로 유지한다 |
| C-3 같은 ItemId 구분 | `SlotIndex`가 이미 구분 | **`EntryId`로 개명.** 칸 수가 아이템마다 달라 "몇 번 칸"이라는 의미가 없어졌으나, 넣은 **이유**(FastArray 순서 미보장)는 그대로다 |

### D-2 (새로 생기는 유일한 함정)

**유효하다.** `EquippedSlotIndex` → `EquippedEntryId`로 이름만 바뀌고 위험은 동일하다.
→ **write-back → `MarkItemDirty` → 엔트리 제거** 순서 규칙으로 `05_Loot_03_Inventory.md` 03-5 / `05_Loot_05_Equipment.md` 05-3에 명시. 함정 표에도 넣었고, **증상이 `InitAmmo` 문제와 동일**하다는 오진 주의를 붙였다.

### 스택 폐지로 **추가로** 사라진 것

A-2/A-3의 판단과 무관하게 아래가 전부 증발했다.

`Quantity`(엔트리·픽업·루트테이블) / 스택 병합 순서 / 부분 획득 / `AddItem`의 개수 반환 /
`FlushNetDormancy()` 경로와 그 누락 함정 / 부분 획득에서의 `bClaimed` 되돌리기 /
`MaxStack` 분기(필드는 스택 부활용 **예약**으로 남김)

### 새로 생긴 것

- `FEPItemState::Charges` — `Ammo`가 아니다. **스택이 없으면 돈을 인벤토리 아이템으로 두는 유일한 방법이 "현금뭉치 하나가 금액을 보유"** 이고, 탄약상자·소모품도 같은 필드를 쓴다
- `UEPItemDefinition::InitialCharges` + `virtual InitState()` — `CreateInstance()`/`GetInstanceClass()`를 대체
- `UsedSlots = Σ SlotSize` — **캐시하지 않는다.** 캐시하면 추가·제거·복제 수신 셋을 전부 갱신해야 하고 하나만 빠져도 "안 찼는데 가득 찼다"가 된다

### ★ A-1의 핵심 논거가 무효다 — 부착물은 **계획에 있다**

> A-1: "**부착물(attachment) 시스템이 `GAME.md` 어디에도 없다**는 것이 결정적이다.
> Lyra/타르코프가 `UItemInstance`를 UObject로 두는 제1 이유가 부착물·모듈 트리인데 (...)
> **제외 목록에조차 올라와 있지 않을 만큼** 고려 밖이다."

**사실이 아니다.** 부착물은 추후 도입 예정이고(배그식, 깊이 1), 문서화했다 → **§7-3**.

다만 **결론은 바뀌지 않고 논거가 더 강해진다.**

| | 검증 시점의 논거 | 실제 |
|---|---|---|
| A-1 | "부착물이 없으니 UObject가 필요 없다" — **부재에 기댄 논거** | 무효 |
| 대체 | "부착물이 **있어도** 평면+부모참조가 UObject 트리보다 낫다" | 유효, 더 강함 |

근거는 셋이다.

1. **엔진이 중첩을 막는다.** `Class.cpp:974`가 `TArray<FEPItemState>` 재귀를 **Fatal**로 죽이고, `:5512`가 FastArray 중첩을 막는다 → 어떤 설계든 **평면 배열 + 부모 참조**로 가야 한다
2. **UObject의 비용은 트리에서 최대가 된다.** 소유권·수명·핸들이 노드 수만큼 곱해지고, **서브오브젝트 트리 복제**는 UObject 복제 중 가장 아픈 경우다. 제거한 부담이 그대로 돌아온다
3. **부착물은 Fragment가 아니라 자기도 아이템이다.** 따로 줍고 팔 수 있으므로 어차피 인벤토리 엔트리와 같은 표현이어야 한다. Lyra의 UObject 논거가 이 프로젝트에는 적용되지 않는다

**부수 확인:** C-3에서 "`SlotIndex`가 이미 구분한다"고 답한 개체 식별자(→ `EntryId`)가 **부착물의 전제 조건**이었다. 배열 인덱스를 골랐다면 부모 참조가 불가능해 부착물이 원천 봉쇄됐다. 다른 이유(FastArray 순서 미보장)로 내린 결정이 여기서 문을 열어뒀다.

### A-2 / A-3 / B-1 / B-2

전부 유효하며 그대로 문서에 반영했다 — `FInstancedStruct` 전환 기준("세 번째 카테고리가 전용 필드를 요구할 때"), 내부 struct 델타 이득, `MarkItemDirty` 함정, FastArray 제약 4개, §7-2 "상자는 인벤토리에 들어가지 않는다".
