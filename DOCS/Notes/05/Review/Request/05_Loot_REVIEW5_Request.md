# 검수 요청 5차 — Step 01 착수 직전 단일 문서 검수

> 작성일: 2026-07-30
> 4차: `05_Loot_REVIEW4_Request.md` / `_Answer.md` (3차 반영 확인 + 판단 5건)
> 대상: **`05_Loot_01_Spawner.md` 한 파일** (513줄)
> 시점: **Step 00 구현 완료. Step 01 착수 직전**

---

## 0. 이번 요청이 이전 네 차수와 다른 점

**코드가 생겼다.** 1~4차는 전부 "구현 코드 0줄" 상태의 설계 검수였다. 이번엔 Step 00이 실제로 돌아가고, **문서가 맞았는지 틀렸는지가 처음으로 관측됐다.** §1에 그 결과를 적었다 — 검수 결론을 뒤집는 것은 아니고, 문서가 예측하지 못한 실패 유형이 하나 드러났다.

**대상이 한 파일이다.** 4차처럼 전 문서를 훑지 말고 `05_Loot_01_Spawner.md`만 봐달라. 다른 문서는 상호 참조 확인 용도로만 필요하다.

**봐줄 것 세 가지.**

1. **§1의 관측 결과가 Step 01에 제대로 이식됐는가** — 같은 함정이 `UEPLootTable`에서 재현될 자리가 있는데 막았다고 보는데, 놓친 데가 있는지
2. **§2의 자체 수정 3건** — 오늘 발견해 고쳤다. 진단이 맞는지, 고치면서 새 구멍을 만들지 않았는지
3. **§3의 판단 6건** — 그중 3-1은 **문서가 단언만 하고 근거를 안 적은 것**이라 특히 봐달라

---

## 1. Step 00 구현에서 관측된 것 — 문서가 예측 못 한 실패 유형

Step 00은 완료됐다. `EP.Item.Dump` → `DataCache = 5, Definitions = 5`. 가는 길에 **두 번 막혔고 둘 다 설계 결함이 아니라 "언리얼이 이 API를 어떻게 대하는가"였다.**

### 1-1. `Definitions = 0` — 핸들 null을 실패로 판정했다

문서(Claude 작성)가 이렇게 적었다.

```cpp
DefinitionHandle = Manager.LoadPrimaryAssetsWithType(FPrimaryAssetType(TEXT("ItemDef")));
if (!DefinitionHandle.IsValid())
{
    UE_LOG(..., TEXT("ItemDef 프라이머리 애셋이 하나도 없습니다."));
    return;      // ← 여기서 항상 빠져나갔다
}
```

**틀렸다.** `ChangeBundleStateForPrimaryAssets`(`AssetManager.cpp:2195-2199`)는 `CurrentState.IsSame(NewBundleState)`이면 `continue`하고, 결과적으로 `AllHandles`가 비면 `CreateCombinedHandle`이 nullptr을 반환한다(`:2298`).

> **핸들 null = "새로 로드할 게 없다"이지 "에셋이 없다"가 아니다.** 에디터 세션에서는 DA가 이미 상주 중인 경우가 정상이라 거의 항상 이쪽으로 빠진다.

수정: 등록 여부는 `GetPrimaryAssetIdList()`의 **개수**로, 결과는 `GetPrimaryAssetObjectList()`의 **내용**으로 판정. 핸들은 대기용으로만 쓴다.

### 1-2. `Definitions = 3` (5여야 함) — `PrimaryAssetType`이 `.uasset`에 구워져 있었다

무기 DA 3종이 스캔에서 빠졌다. 원인은 **코드가 아니라 에셋이었다.**

`UEPWeaponDefinition::GetPrimaryAssetId()`가 `"WeaponDef"`를 반환하던 시절에 저장된 DA들이라, 그 문자열이 애셋 레지스트리 태그로 박혀 있었다. `FAssetData::GetPrimaryAssetId()`(`AssetData.cpp:692-703`)는 **저장된 태그만 읽고 클래스에 묻지 않는다.** `AssetManager.cpp:1396-1426`이 타입 불일치로 `continue`.

**LOOT_STATUS의 "오버라이드 제거"(Step 00 항목)만으로는 안 고쳐진다.** 무기 DA를 전부 다시 저장해야 했다.

> **증상이 들쭉날쭉했던 이유:** `ARFilter.bIncludeOnlyOnDiskAssets = !GIsEditor || IsRunningCookCommandlet();`(`:1089`). 에디터에서는 메모리에 올라온 애셋의 `FAssetData`를 살아 있는 객체로 다시 만들어 **stale 태그를 우회한다.** 그래서 해당 DA를 에디터에서 열어두면 되살아났다. **패키지 빌드에서는 예외 없이 전부 빠진다.**
>
> `IssuedWarnings`(`:1414`)가 `static TSet`이라 **타입 쌍당 경고가 딱 한 줄** 나온다. 5개 중 3개가 빠졌는데 로그는 한 줄이었다.

### 1-3. Step 01에 이식한 것 — 이게 §0의 질문 1번이다

`UEPLootTable`도 `GetPrimaryAssetId()`를 새로 정의한다. **같은 함정의 자리가 그대로 있다.**

| 이식한 곳 | 내용 |
|---|---|
| 01-1 | **"코드 먼저 컴파일 → 등록 → 재시작 → 에셋 생성"** 순서 명시. 순서만 지키면 안 겪는다 |
| 01-1 | "등록하면 `RollTable`이 찾는다"는 서술이 **틀렸다.** 등록은 ID 성립 조건일 뿐 — 로드는 별개 (아래 §2-2) |
| 01-5 | 커맨드가 **결과 포인터로** 판정. 핸들로 판정하면 1-1이 재발 |
| 함정 #14 | 표에 추가 |

**질문:** 다른 자리는 없나. 특히 `05_Loot_02`~`05`에서 `PrimaryAssetType`이 새로 생기는 데가 있는지, 그리고 **1-1의 "핸들 null"이 Step 01의 비동기 메시 로드(`RequestAsyncLoad`)에서 다시 나타나는지** — 그쪽은 `FStreamableManager`의 다른 경로라 같은 논리가 아닐 것 같은데 확신이 없다.

---

## 2. 자체 수정 3건 — 진단이 맞는지 확인해달라

오늘 01 문서를 엔진 소스와 현재 코드에 대조하며 찾은 것들이다. 셋 다 고쳤다.

### 2-1. `OnRep_ItemId()`에만 메시 코드를 두면 완료 조건 1이 실패한다

**진단:** `OnRep_`은 복제를 **받는** 쪽에서만 불린다. 서버는 값을 직접 대입하므로 호출되지 않는다. PIE 기본 넷 모드가 *Play As Listen Server*이므로 **서버 창에서만 픽업이 안 보인다.**

**수정:** `ApplyVisual()`로 분리해 `InitPickup()`(서버 경로)과 `OnRep_ItemId()`(클라 경로) 양쪽에서 호출. `IsNetMode(NM_DedicatedServer)` 가드는 유지 — 판정 기준을 "서버냐"가 아니라 **"화면이 있느냐"** 로 다시 적었다.

### 2-2. AssetManager 등록만으로는 `RollTable`이 테이블을 못 찾는다

```cpp
// AssetManager.h:218 — "returning nullptr if it's not in memory"
// AssetManager.cpp:1910-1920
UObject* UAssetManager::GetPrimaryAssetObject(const FPrimaryAssetId& Id) const
{
    const FPrimaryAssetData* NameData = GetNameData(Id);
    if (NameData) { return NameData->GetAssetPtr().Get(); }   // 메모리에 없으면 nullptr
    return nullptr;
}
```

**문서가 스스로를 무너뜨리고 있었다.** 원문:

> 아직 어떤 스포너도 참조하지 않는 새 테이블 — 정확히 검증하고 싶은 그 테이블 — 이 메모리에 없어서 커맨드가 못 찾는다.

등록을 근거로 들면서 **못 찾는 이유를 정확히 서술**해 놓고, 등록이 그걸 해결한다고 결론냈다.

**수정:** `LoadPrimaryAsset` + `WaitUntilComplete` 후 **결과 포인터로** 판정하는 코드로 교체. 핸들 null은 "이미 상주"와 "그런 ID 없음"을 둘 다 의미하므로 판정에 못 쓴다(§1-1).

### 2-3. `WorldMesh` 조회부가 주석으로 비어 있어 컴파일 불가

원문이 `UEPItemDefinition* Def = /* DefinitionSubsystem->FindDefinition(ItemId) */;`였다. Step 00이 끝났으므로 `GetGameInstance()->GetSubsystem<UEPItemDefinitionSubsystem>()->FindDefinition(ItemId)`로 채웠다.

### 그 외 보완 4건

- `RollLootTable`의 **소유자**(자유 함수 — `Depth`를 공개 API로 노출하지 않기 위함)와 **반환 규약 두 겹**(`false` = 데이터 오류 / `true + NAME_None` = 정상 빈 결과)
- `FMath::FRand()`가 **`[0, 1]` 닫힌 구간**임을 명시 (§3-3에서 판단 요청)
- `PlaceholderPickupMesh`의 `Category`를 `"Debug"` → `"Visual"` (문서 자신이 "유일한 표시 수단"이라 적은 필드)
- `EngineUtils.h` include (`TActorIterator`)

**검증 후 문제 없었던 것:** `NetCullDistanceSquared`/`NetUpdateFrequency`의 5.5 deprecated(`Actor.h:869,874`)와 세터 존재(`:4622,4646`), `bAlwaysRelevant`(`:300`)/`NetDormancy`(`:832`)는 deprecated 아님, `EPGameMode.cpp:137`의 `HandleMatchHasStarted` 위치, `FEPItemState` 8바이트, `UEPItemDefinition::WorldMesh` 존재.

---

## 3. 판단 요청 6건

### 3-1. ★ Dormancy × 스폰 타이밍 × 릴러번시 — 문서가 근거 없이 단언한다

01-4가 이렇게 적어 놓았다.

```
스폰       → DORM_Initial (초기 1회 복제 후 휴면)
획득 완료  → Destroy()    ← 파괴는 휴면과 무관하게 전달된다
```

> **복제되는 값이 `ItemId` 하나뿐이고 그것은 스폰 시점에 정해져 바뀌지 않는다.** (…) **그 호출도 그 함정도 이제 없다.**

**세 가지가 근거 없이 단언돼 있다.**

**(a) `ItemId`가 초기 복제 번들에 들어가는가.** 스포너는 `SpawnActor` → `InitPickup(Id, State)`를 부른다. `DORM_Initial`은 채널이 열릴 때 초기 상태를 보내고 곧바로 휴면하는데, **그 시점이 `InitPickup` 이전이면 클라에는 `ItemId = NAME_None`이 박히고 휴면해 영원히 안 고쳐진다.** 같은 프레임 안이면 안전한지, 아니면 `SpawnActorDeferred` + `FinishSpawning`으로 감싸야 하는지. 문서에 이 얘기가 한 줄도 없다.

> 안전하더라도 **"같은 프레임이라 우연히 된다"에 기대는 것**이면 `SpawnActorDeferred`를 쓰는 편이 낫다고 본다. `InitPickup`이 Step 03에서 시그니처가 바뀌므로(§3-6) 그때 순서가 흔들릴 여지가 있다.

**(b) "파괴는 휴면과 무관하게 전달된다"의 근거.** 맞다고 알고는 있으나 엔진 근거를 못 찾았다. `DORM_Initial`이라 **채널이 아직 열린 적 없는** 클라(멀리 있어 릴러번시 밖)에게는 어차피 보낼 것도 없으니 무해하다는 이해인데, 채널이 열린 뒤 휴면 중인 클라에게 `Destroy()`가 어떤 경로로 나가는지 확인해달라.

**(c) 완료 조건 6번("멀리 있는 픽업이 컬링되고, 가까이 가면 나타난다")이 `DORM_Initial`과 양립하는가.** 릴러번시 밖으로 나가 채널이 닫혔다가 다시 들어오면 채널이 재생성되며 다시 복제된다고 봤는데, **`DORM_Initial`이 "한 번 열렸으면 다시 안 연다"로 동작하면 완료 조건 6이 그대로 실패한다.** `SetNetCullDistanceSquared(25000000.f)`(5000cm)와 겹쳐서 봐달라.

### 3-2. `EP.Loot.List`를 클라에서도 돌리는 게 정직한 검증인가

완료 조건 7번(**"클라이언트 패킷에 `Charges`가 나가지 않는다"**)에 검증 수단이 없어서 오늘 만들었다. 클라에서 커맨드를 돌려 `Charges`가 전부 `0`이면 통과다.

**문제:** `Claimed`/`Cooldown`/`Payload`도 서버 전용이라 클라에서는 같이 기본값으로 찍힌다. 문서에 *"이상한 게 아니라 같은 이유의 같은 결과다"* 라고 적어 뒀지만, **"복제 안 됨"과 "값이 진짜 0임"을 출력만으로 구분할 수 없다**는 약점은 남는다.

**질문:** 이대로 두는 게 나은가, 아니면
- (ㄱ) 클라에서는 복제 안 되는 열을 `-`로 찍고 헤더에 `[server-only]`를 표시하나
- (ㄴ) 애초에 관측으로 증명할 게 아니라 `GetLifetimeReplicatedProps`에 `State`가 없다는 **코드 리뷰로 갈음**해야 하나 (그러면 완료 조건 문구를 바꿔야 한다)

### 3-3. `FRand()` 닫힌 구간 대응이 과잉인가

```cpp
// GenericPlatformMath.h:635 — "Returns a random float between 0 and 1, inclusive."
static inline float FRand()
{
    constexpr int32 RandMax = 0x00ffffff < RAND_MAX ? 0x00ffffff : RAND_MAX;
    return (Rand() & RandMax) / (float)RandMax;
}
```

`FRandRange(0, TotalWeight)`가 **정확히 `TotalWeight`를 반환할 수 있다.** 그러면 롤 루프가 끝까지 음수가 안 되어 마지막 `return false`에 도달한다. 확률은 2⁻²⁴ 수준.

`CLAUDE.md`가 *"불가능한 시나리오에 에러 처리를 넣지 마라"* 고 하는데, 이건 불가능하지 않고 다만 희박하다. 문서에는 **"`false`가 '테이블이 잘못됐다'는 뜻이라 그때 엉뚱한 에러 로그가 뜬다"** 를 근거로 마지막 줄을 고치라고 적었다.

**질문:** 근거가 성립하나, 아니면 2⁻²⁴에 한 줄이라도 쓰는 게 과잉인가. (고친다면 마지막 엔트리를 집어 주는 것 말고 더 나은 형태가 있는지도)

### 3-4. 픽업의 콜리전 프로파일을 Step 01에서 정할 것인가

생성자가 `Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly)` 한 줄뿐이다. **오브젝트 채널도 반응 설정도 없다.**

Step 02가 `EP_TraceChannel_Interact`로 픽업을 트레이스한다(LOOT_STATUS). Step 01에서 프로파일을 안 정해두면 **Step 02에서 "왜 트레이스가 안 맞지"** 로 시간을 쓸 것 같은데, 반대로 지금 정하면 Step 02가 채널을 만들기 전이라 참조할 대상이 없다.

**질문:** 여기서 프로파일 이름만 예고하고 Step 02에서 채우는 게 맞나, 아니면 Step 01에서 채널 생성까지 앞당기나. (완료 조건에 "보인다"만 있고 "맞는다"는 없어서 Step 01만으로는 검증이 안 된다)

### 3-5. 스폰 겹침과 접지 — `SpawnRadius`가 있는데 충돌 처리가 없다

`AEPItemSpawner`가 `RollCount`회 굴려 `SpawnRadius` 안에 뿌리는데,

- `FActorSpawnParameters::SpawnCollisionHandlingOverride`가 언급조차 안 된다. 기본값이면 겹칠 때 스폰이 **조용히 실패**할 수 있다
- `bAlignToGround`의 라인트레이스가 **어떤 채널을 쓰는지** 안 적혀 있다. `ECC_Visibility`면 방금 뿌린 다른 픽업에 걸려 공중에 뜬다
- `RollCount = 5`인데 `SpawnRadius = 0`이면 전부 한 점

**질문:** Step 01 범위에서 어디까지 다뤄야 하나. 완료 조건에 "겹치지 않는다"가 없으니 지금은 무시하고 레벨 디자인 문제로 미뤄도 되는지, 아니면 `SpawnCollisionHandlingOverride` 한 줄은 지금 못 박아야 하는지.

### 3-6. `InitPickup` 시그니처 — Step 03 교체 예고를 지금 앞당기나

4차에서 **"지금 배열로 만들지 않는다"** 로 확정했고 그 근거(원소가 항상 1개인 배열은 읽는 쪽에 군더더기)에 동의한다. 문서에도 남아 있다.

다만 **이제 Step 01을 실제로 구현할 시점**이고, §3-1(a)에서 `SpawnActorDeferred`를 도입한다면 `InitPickup` 호출 지점이 한 번 더 바뀐다. **같은 함수를 세 번 고치게 된다**(01 작성 → Deferred 전환 → 03 배열 전환).

**질문:** 확정을 뒤집자는 게 아니라, **Step 03에서 바뀔 것을 아는 상태에서 Step 01의 완료 조건을 어떻게 잡아야 하는가**를 묻는 것이다. 지금 `AEPPickup`을 "Step 03에서 절반 다시 쓴다"고 받아들이고 진행하는 게 맞나?

---

## 4. 재론하지 않았으면 하는 것

| | 근거 |
|---|---|
| struct vs UObject / 스택 폐지 / 칸 합산 / `bFungible` | 1·2차 확정 |
| `Durability` / `MaxStack` 존치 | 3차 권고를 사용자가 기각. **구현상 문제는 지적하되 존치 여부는 논의 대상이 아니다** |
| DT/DA 두 계층 유지 | 3차 §5 권고대로 확정 |
| `State`를 복제하지 않는 결정 | 2차 확정 (정보 은폐). **검증 방법**은 논의 대상이다(§3-2) |
| 수량 필드 없는 루트 테이블 / `EmptyWeight` 루트 한정 | 2·3차 확정 |
| `AEPPickup`이 `TArray<FEPInventoryEntry> Payload`로 가는 방향 | 4차 확정. **시점**만 §3-6에서 묻는다 |
| Step 00 구현 내용 | 완료. §1은 배경 설명이지 검수 대상이 아니다 |

---

## 5. 대상 파일

| 파일 | 줄 | 비고 |
|---|---|---|
| **`05_Loot_01_Spawner.md`** | **513** | **이번 검수 대상.** 오늘 §2의 7건 반영 |
| `05_Loot_00_ItemCore.md` | 887 | 상호 참조용 (특히 **함정 #1** — §1-2의 상세) |
| `05_Loot_02_Interaction.md` | 279 | §3-4 판단에 필요 |
| `LOOT_STATUS.md` | 142 | 확정 결정 대조용 |
| `05_Loot_DOCS.md` | 840 | §4-2·4-3·4-4가 Step 01의 마스터 기획 |

### 실제 코드 (Step 00 산출물 — 배경 참고용)

```
Public/Data/EPItemDefinitionSubsystem.h
Private/Data/EPItemDefinitionSubsystem.cpp     ← §1-1·1-2의 최종 형태
Public/Data/EPItemData.h
Public/Data/EPItemDefinition.h
Public/Data/EPLootDeveloperSettings.h          ← Step 01에서 3필드 확장
Config/DefaultGame.ini                         ← :14 ItemDef 등록, :33 ItemDataTable
Private/Core/EPGameMode.cpp:137                ← HandleMatchHasStarted
```
