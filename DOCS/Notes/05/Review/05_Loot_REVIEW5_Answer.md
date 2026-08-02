# 검수 답변 5차 — `05_Loot_01_Spawner.md` (Step 01 착수 직전)

> 작성일: 2026-07-30
> 요청: `05_Loot_REVIEW5_Request.md`
> 대상: `05_Loot_01_Spawner.md` (513줄). 상호 참조: `05_Loot_02_Interaction.md`, `LOOT_STATUS.md`, Step 00 실제 코드
> 엔진: UE 5.7 (`C:\Program Files\Epic Games\UE_5.7`)

---

## 0. 결론

| 요청 | 판정 | 요지 |
|---|---|---|
| **§1-3** 이식 누락이 있나 | ⚠️ **하나 있다** | `PrimaryAssetType`은 Step 01이 마지막이다. 그런데 **같은 함정이 Step 02의 콜리전 채널로 형태만 바꿔 남아 있다** (§7). `RequestAsyncLoad`는 다른 경로 — 재발 안 한다 (§1-2) |
| **§2** 자체 수정 3건 진단 | ✅ **셋 다 맞다** | 근거·수정 형태 모두 정확. `2-1`은 특히 정확하다 |
| **§2** 새 구멍을 만들었나 | ❌ **만들었다 1건** | 이번에 세운 **"반환 규약 두 겹"에 구멍이 있다** — 하위 테이블의 빈 `ItemId`가 "정상적인 빈 결과"로 위장한다 (§2-2) |
| **★ 문서 전체** | ❌ **`SpawnLoot()` 본문이 없다** | Step 01의 **유일한 새 게임플레이 로직**이 문서에 안 적혀 있다. §3-5의 세 질문은 전부 이 하나의 부재가 만든 증상이다 (§3) |
| **3-1 (a)** Deferred 필요한가 | ❌ **불필요** | "우연히 된다"가 아니다. **복제는 상태 기반이고 프레임 끝에 일어난다**(`LevelTick.cpp:1899`). 문서에 근거 한 줄만 넣으면 끝 (§4-1) |
| **3-1 (b)** 파괴 전달 근거 | ✅ **맞다** | `NetDriver.cpp:4345-4356`. 단 "무관하게"가 아니라 **별도 경로(destruction info)** 다 (§4-2) |
| **3-1 (c)** 완료 조건 6 양립 | ❌ **양립하지 않는다** | 추측 두 개 모두 아니고 세 번째다 — **한 번 본 픽업은 멀어져도 사라지지 않는다.** 결함이 아니라 문구가 틀렸다 (§4-3) |
| **3-2** 클라 `List`가 정직한가 | ⚠️ **(ㄱ) + 완료 조건 문구** | (ㄴ)은 안 된다. 진짜 약점은 커맨드가 아니라 **절차**다. 그리고 **예시 출력의 3열 중 2열은 Step 01에 존재하지 않는다** (§5) |
| **3-3** `FRand()` 대응 과잉인가 | ✅ **과잉 아니다. 근거는 오히려 약하게 적혔다** | 닫힌 구간만이 아니라 **부동소수 잔차**로도 도달한다. 다만 권고 형태(마지막 엔트리)는 바꿔야 한다 (§6) |
| **3-4** 프로파일을 Step 01에서 | ✅ **Step 01에서 정해야 한다** | 단 Step 02의 트레이스 때문이 **아니고**, Step 01 자체로 관측되는 것 때문이다. **채널을 앞당길 필요는 없다** (§7) |
| **3-5** 스폰 겹침·접지 | 🔀 **셋 중 하나만 실재** | `SpawnCollisionHandlingOverride` 걱정은 **근거 없다**(기본값이 `AlwaysSpawn`). 접지 채널은 실재하고 §7과 **한 줄로 같이 닫힌다** (§8) |
| **3-6** `InitPickup` 시점 | ✅ **지금 진행. "절반 다시 쓴다"도 아니다** | §4-1로 Deferred가 빠지면 고칠 횟수가 **셋에서 둘로** 줄고, 그 둘도 시그니처 1곳 + 호출부 1곳이다 (§9) |

**총평.** 오늘 고친 3건은 진단·수정 모두 정확하고, 특히 `2-1`(`OnRep_`은 받는 쪽만)은 실제로 리슨서버에서 하루를 태울 수 있는 것을 미리 잡았다. **한 파일 검수로 좁힌 판단이 옳았다.** 다만 이 문서는 지금 **"픽업 액터 설계서"로는 완성됐고 "스포너 구현서"로는 미완성**이다 — 클래스 4개 중 `AEPItemSpawner`만 본문이 없고, §3-5의 세 질문이 그 자리에서 나왔다.

---

## 1. §1-3 — 이식 누락 확인

### 1-1. `PrimaryAssetType`이 새로 생기는 곳은 Step 01이 마지막이다

`05_Loot_02`~`05`를 훑었다. Step 02는 인터페이스 + 컴포넌트, 03은 컴포넌트 + FastArray, 04는 위젯, 05는 기존 컴포넌트 수정이다. **새 `UPrimaryDataAsset`도 새 `PrimaryAssetType`도 없다.** §7-1 컨테이너·§7-2 자판기가 루트 테이블 에셋을 더 만들지만 타입은 `LootTable` 재사용이다.

**그런데 한 단어로 재발을 원천 봉쇄할 수 있다.** Step 00 사건의 실제 원인은 "에셋을 먼저 만든 것"이 아니라 **하위 클래스가 `GetPrimaryAssetId()`를 다시 오버라이드한 것**이었다(`LOOT_STATUS.md:87`). 순서 규칙은 사람이 지켜야 하는 반면, 이건 컴파일러가 지킬 수 있다.

```cpp
    // 하위 클래스가 다시 오버라이드하면 타입이 갈린다 — Step 00의 WeaponDef 사건
    virtual FPrimaryAssetId GetPrimaryAssetId() const override final
    { return FPrimaryAssetId(TEXT("LootTable"), GetFName()); }
```

`final` 한 단어다. 순서 규칙(01-1의 3단계)은 그대로 두고, **이걸 함께 넣는 것이 이식의 완성**이다. 순서를 지키는 것은 이번 한 번을 막고, `final`은 앞으로 전부를 막는다.

> **Step 00 코드에도 같은 한 단어가 유효하다** — `UEPItemDefinition::GetPrimaryAssetId()`에 `final`. `UEPWeaponDefinition`의 오버라이드를 제거한 것이 이번 수정이었으니, `final`은 그 수정이 되돌아오지 않게 못을 박는다. Step 00은 완료됐으므로 판단은 사용자 몫이다.

### 1-2. `RequestAsyncLoad`의 null은 §1-1과 같은 모호함이 **아니다**

확인했다. **다른 함수, 다른 이유, 다른 의미다.**

`§1-1`의 모호함은 `ChangeBundleStateForPrimaryAssets`가 "이미 같은 번들 상태면 `continue`"(`AssetManager.cpp:2195-2199`)한 결과 **합칠 핸들이 없어서** `CreateCombinedHandle`이 nullptr을 주는 것이었다(`:2298`). 즉 "성공했는데 null".

`FStreamableManager::RequestAsyncLoad`가 null을 반환하는 경로는 **딱 둘이고 둘 다 진짜 오류다.**

```cpp
// StreamableManager.cpp:1852-1858 — 짧은 패키지 이름 (Error 로그 + null)
// StreamableManager.cpp:1862-1869 — 요청 대상이 비었거나 전부 null (Display 로그 + null)
```

**"이미 로드됨"은 null이 아니다.** 이미 메모리에 있으면 유효한 핸들이 나오고 완료 델리게이트가 호출된다 — 그것도 **`RequestAsyncLoad`가 반환하기 전에 동기로**:

```cpp
// StreamableManager.cpp:1940  RequestAsyncLoad 내부에서 StartHandleRequests(NewRequest)
// StreamableManager.cpp:2009  "Go through and complete loading anything that's already
//                              in memory, this may call the callback right away"
// StreamableManager.cpp:2018  → CheckCompletedRequests → :2261 Handle->CompleteLoad()
```

**결론: 01-4의 코드는 안전하다.** 하지만 문서에 한 줄이 필요하다.

> **`MeshHandle`에 대입되기 전에 람다가 실행될 수 있다.** 메시가 이미 메모리에 있으면 완료 델리게이트가 `RequestAsyncLoad` 안에서 동기 호출된다(`StreamableManager.cpp:2009`의 주석이 그렇게 적혀 있다). 지금 람다는 `MeshHandle`을 건드리지 않아 무해하지만, **람다 안에서 `MeshHandle`을 만지는 수정이 들어오면 그 순간 null이다.**

그리고 §1-1의 교훈이 여기서는 **반대로** 적용된다 — `RequestAsyncLoad`의 null은 판정에 **써도 된다.** Step 00에서 "핸들로 판정하지 말라"를 배웠기 때문에 여기서도 안 쓰는 것이 습관이 되면, 정작 잡아야 할 오타(`WorldMesh`에 잘못된 경로)를 놓친다. 함정 #14 옆에 한 줄로 구분해 두는 편이 낫다.

---

## 2. §2 자체 수정 3건 — 진단 검증

### 2-1. 세 건 모두 맞다

| # | 판정 | 확인 내용 |
|---|---|---|
| **2-1** `OnRep_`만 두면 완료 조건 1 실패 | ✅ | 정확하다. `OnRep_`은 수신측 전용이고 서버는 대입할 뿐이다. **"판정 기준을 서버냐가 아니라 화면이 있느냐로 다시 적었다"가 이 수정의 핵심**이고, 그 문장이 문서에 남아 있는 것이 옳다 — 나중에 `IsNetMode(NM_DedicatedServer)`를 `HasAuthority()`로 "정리"하려는 유혹을 막는다 |
| **2-2** 등록만으로는 `RollTable`이 못 찾는다 | ✅ | `AssetManager.cpp:1910-1920`이 그대로다. 문서가 **못 찾는 이유를 정확히 서술해 놓고 등록이 해결한다고 결론냈다**는 자기 진단이 맞다. `LoadPrimaryAsset` + `WaitUntilComplete` + 결과 포인터 판정이 정답 |
| **2-3** `WorldMesh` 조회부 주석 | ✅ | `GetGameInstance()`는 `AActor`에 있다(`Actor.h:3771`). `FindDefinition(FName)`(`EPItemDefinitionSubsystem.h:23`)·`MakeItemState(FName, FEPItemState&) → bool`(`:25`) 시그니처 일치. **컴파일된다** |

보완 4건도 확인했다. `FRand()` 닫힌 구간(`UnrealMathUtility.h:313-316` — `FRandRange(0,T) = 0 + T*FRand()`), `PlaceholderPickupMesh`의 `Category` 이동, `EngineUtils.h`, `RollLootTable`의 소유자·반환 규약 — 전부 타당하다.

**"검증 후 문제 없었던 것" 목록도 다시 확인했다.** `Actor.h:869,874` deprecated, 세터 존재, `bAlwaysRelevant`(`:300`)/`NetDormancy`(`:832`) deprecated 아님 — 맞다. 이 목록을 요청서에 적어 둔 것 자체가 유용하다. 같은 것을 다시 파지 않게 된다.

### 2-2. ★ 새 구멍 — "반환 규약 두 겹"에 하위 테이블 구멍이 있다

이번에 세운 규약이다(01-1:108).

> `false` = 테이블이 잘못됐다 → 호출부가 에러 로그.
> `true` + `OutItemId == NAME_None` = **정상적인 빈 결과** → 조용히 스폰 생략.
> 스포너가 이 둘을 구분하지 않으면 데이터 오류가 "그냥 안 나온 것"에 묻힌다.

**규약의 목적은 맞는데, 그 목적을 스스로 뚫는 경로가 있다.**

`true + NAME_None`은 **루트의 `EmptyWeight`가 뽑혔을 때만** 나와야 한다. 그런데:

```cpp
        if (E.SubTable) return RollLootTable(E.SubTable, OutItemId, Depth + 1);

        OutItemId = E.ItemId;      // ★ E.ItemId가 비어 있으면? 그대로 NAME_None을 담아 true를 반환한다
        return true;
```

디자이너가 등급 테이블에 엔트리를 추가하고 `ItemId`도 `SubTable`도 안 채우면 — 01-1이 *"둘 중 하나만 채운다"* 고 **주석으로만** 적어 놓은 그 실수 — 롤은 `true + NAME_None`을 반환하고 스포너는 **"정상적인 빈 결과"로 읽어 조용히 넘긴다.** 규약이 막으려던 상황이 정확히 그대로 일어난다. 게다가 증상이 "가끔 아이템이 안 나온다"라서 `EmptyWeight`를 의심하며 엉뚱한 데를 파게 된다.

`Depth > 0`에서는 `EmptyWeight`가 무시되므로 **하위 테이블이 `true + NAME_None`을 반환하는 것은 정의상 오류**다. 그걸 코드가 알고 있어야 한다. §6의 재구성에 한 줄로 함께 들어간다.

> **이건 "미선언 심볼" 부류가 아니라 4차 §1과 같은 부류다** — 두 함수가 값을 주고받는데 그 사이 계약이 한쪽에만 적혀 있다. `RollLootTable`이 규약을 알고 `NAME_None`을 걸러야 하는데, 규약이 **호출자 쪽 문서 문장**으로만 존재한다. 양쪽 다 컴파일되므로 아무도 안 걸린다.

---

## 3. ★ 문서 전체에서 가장 큰 것 — `SpawnLoot()` 본문이 없다

이 문서는 클래스 4개를 다룬다.

| | 선언 | 본문 |
|---|---|---|
| `UEPLootTable` | ✅ 01-1 | ✅ `RollLootTable` 전문 |
| `AEPPickup` | ✅ 01-4 | ✅ 생성자 / `InitPickup` / `OnRep_` / `ApplyVisual` 전문 |
| `UEPLootDeveloperSettings` | ✅ 01-5 | ✅ (필드뿐) |
| **`AEPItemSpawner`** | ✅ 01-2 | ❌ **없다** |

`SpawnLoot()`는 **Step 01에서 새로 생기는 유일한 게임플레이 로직**이다. 롤 결과를 받아 규약 두 겹을 구분하고, `MakeItemState` 실패를 처리하고(함정 #11), 반경 안에 흩고, 접지시키고, `SpawnActor` 후 **같은 프레임에 `InitPickup`을 부르고**(§4-1), `SpawnedPickups`에 담는다(함정 #6). 문서에는 이 중 어느 것도 순서로 적혀 있지 않다 — 01-4에 `InitPickup` 호출 2줄 조각만 있고, 그 조각의 `RolledItemId`는 어디서도 선언되지 않는다.

**§3-5의 세 질문(겹침·접지 채널·`SpawnRadius=0`)은 전부 이 부재의 증상이다.** 본문을 적으면 세 질문이 각자 한 줄로 답해진다. 아래를 문서에 넣는 것을 권한다.

```cpp
void AEPItemSpawner::SpawnLoot()
{
    if (!HasAuthority()) return;
    if (!LootTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[Loot] %s: LootTable 미지정"), *GetName());
        return;
    }

    UGameInstance* GI = GetGameInstance();
    UEPItemDefinitionSubsystem* Defs = GI ? GI->GetSubsystem<UEPItemDefinitionSubsystem>() : nullptr;
    if (!Defs) return;

    for (int32 i = 0; i < RollCount; ++i)
    {
        FName RolledId = NAME_None;

        if (!RollLootTable(LootTable, RolledId))
        {
            // false = 테이블이 잘못됐다. 조용히 넘기면 안 된다 (01-1 반환 규약)
            UE_LOG(LogTemp, Error, TEXT("[Loot] %s: 롤 실패 — %s 데이터 오류"),
                   *GetName(), *GetNameSafe(LootTable));
            continue;
        }
        if (RolledId.IsNone()) continue;          // true + None = 정상적인 빈 결과

        FEPItemState NewState;
        if (!Defs->MakeItemState(RolledId, NewState))
        {
            // 함정 #11 — Definition 없는 아이템을 기본값으로 깔면 Step 04에서 원인이 멀어진다
            UE_LOG(LogTemp, Error, TEXT("[Loot] %s: Definition 없음 — 스폰 생략"),
                   *RolledId.ToString());
            continue;
        }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;   // 기본값이지만 명시 (§3-5)

        AEPPickup* Pickup = GetWorld()->SpawnActor<AEPPickup>(
            AEPPickup::StaticClass(), GetSpawnPoint(), FRotator::ZeroRotator, Params);
        if (!Pickup) continue;

        // ★ SpawnActor와 같은 프레임에 부른다. 프레임을 넘기면 초기 복제가 NAME_None으로 나간다
        Pickup->InitPickup(RolledId, NewState);
        SpawnedPickups.Add(Pickup);
    }
}

FVector AEPItemSpawner::GetSpawnPoint() const
{
    FVector Point = GetActorLocation();

    if (SpawnRadius > 0.f)
    {
        const FVector2D Offset = FMath::RandPointInCircle(SpawnRadius);
        Point += FVector(Offset.X, Offset.Y, 0.f);
    }

    if (bAlignToGround)
    {
        FHitResult Hit;
        FCollisionQueryParams Params;
        Params.AddIgnoredActor(this);

        // ECC_Visibility로 충분하다 — 픽업은 전 채널 Ignore이므로(01-4)
        // 먼저 뿌린 픽업 위에 다음 픽업이 얹히지 않는다
        if (GetWorld()->LineTraceSingleByChannel(
                Hit, Point + FVector(0, 0, 200.f), Point - FVector(0, 0, 500.f),
                ECC_Visibility, Params))
        {
            Point = Hit.ImpactPoint + FVector(0, 0, 5.f);
        }
    }
    return Point;
}
```

```cpp
void AEPItemSpawner::ClearLoot()
{
    if (!HasAuthority()) return;

    for (TWeakObjectPtr<AEPPickup>& Weak : SpawnedPickups)
        if (AEPPickup* P = Weak.Get())
            P->Destroy();

    SpawnedPickups.Reset();       // 이미 주워져 죽은 약참조도 여기서 비워진다
}
```

- `FMath::RandPointInCircle(float) → FVector2D` (`UnrealMathUtility.h:348`)
- `HasAuthority()` 가드는 **실제로 동작한다.** 스포너는 `bReplicates = false`라 `RemoteRole = ROLE_None`(`Actor.cpp:286`)인데, 레벨 배치 액터는 클라에서 `ExchangeNetRoles(true)`(`Level.cpp:3683`)를 거쳐 `Role`이 `ROLE_None`이 된다 → 클라에서 `HasAuthority()`는 false다. 컴포넌트가 아니므로 `GetOwner()->`를 붙이지 않는다(`CLAUDE.md` 관례)
- 헤더에 `FVector GetSpawnPoint() const;` 추가

---

## 4. 판단 3-1 — Dormancy × 스폰 타이밍 × 릴러번시

### 4-1. (a) `SpawnActorDeferred`는 필요 없다. "우연히 된다"도 아니다

**근거가 없다고 본 것은 맞다. 하지만 결론은 안전하고, 근거는 문서에 적힌 것과 다른 데 있다.**

두 가지가 각각 독립적으로 보장한다.

**① 복제는 상태 기반이고, 프레임 끝에 일어난다.**

UE 프로퍼티 복제는 "값이 바뀔 때 이벤트를 보내는" 것이 아니라 **채널을 채우는 시점의 현재 값을 보내는** 것이다. 그리고 그 시점은 액터 틱이 전부 끝난 뒤다.

```cpp
// LevelTick.cpp:1848  RunTickGroup(TG_PostUpdateWork);
// LevelTick.cpp:1854  RunTickGroup(TG_LastDemotable);
// LevelTick.cpp:1900  BroadcastTickFlush(...);     ← NetDriver::TickFlush → ServerReplicateActors
```

`HandleMatchHasStarted`는 매치 상태 전이(액터 틱 단계) 안에서 불린다. 즉 `SpawnActor`와 `InitPickup` 사이에 **복제가 끼어들 지점이 물리적으로 없다.** "같은 프레임이라 우연히 되는 것"이 아니라 **복제 모델이 그렇게 정의돼 있다.**

**② 게다가 동적 스폰 액터에서 `DORM_Initial`은 "즉시 휴면"이 아니다.**

이것이 문서에 적힌 근거("`ItemId`가 스폰 시점에 정해져 바뀌지 않는다")보다 훨씬 강한 안전판이고, **문서에 한 줄도 없다.**

```cpp
// NetDriver.cpp:8349
bool UNetDriver::IsDormInitialStartupActor(AActor* Actor)
{
    return Actor && Actor->IsNetStartupActor() && (Actor->NetDormancy == DORM_Initial);
}
```

**`IsNetStartupActor()`가 조건에 있다** — 즉 `DORM_Initial`의 특별 취급은 **맵에 배치된 액터에만** 적용된다. 열거형 주석도 그렇게 적혀 있다: *"This actor is initially dormant for all connection **if it was placed in map**"* (`EngineTypes.h:3370`).

스폰된 픽업은 여기 걸리지 않으므로:

- `NetworkObjectList.cpp:55` — 정상 네트워크 액터 리스트에 들어간다 (맵 배치 액터라면 배제된다)
- `NetDriver.cpp:5177` — "리스트에서 제거"(`ActorsToRemove.Add`) 경로를 **타지 않는다**
- `ShouldActorGoDormant`(`NetDriver.cpp:5316`)는 **`!Channel`이면 false를 반환한다** → 채널이 열려 한 번 복제된 **뒤에야** `StartBecomingDormant()`(`:5440`)가 불린다

**즉 동적 스폰 픽업에서 `DORM_Initial`은 사실상 `DORM_DormantAll`과 같다.** 문서의 서술("초기 1회 복제 후 휴면")은 결과적으로 맞다. 다만 **그것이 맞는 이유가 문서에 적힌 이유와 다르다.**

> 참고로 `SpawnActorDeferred`를 쓰더라도 안전하다 — `NetDriver.cpp:5162-5167`이 `!Actor->IsActorInitialized()`면 스킵하고, 주석이 *"it might have been intentionally spawn deferred until a later frame"* 라고 그 경우를 명시한다. 하지만 필요 없는 복잡도다.

**문서에 넣을 것은 한 줄이다.**

> `InitPickup()`은 `SpawnActor()`와 **같은 프레임**에서 불러야 한다. 복제는 프레임 끝(`LevelTick.cpp:1900` → `ServerReplicateActors`)에 **그 순간의 현재 값**으로 나가므로, 그 사이 어디서 대입하든 초기 번들에 실린다. 프레임을 넘기면(타이머·`Latent`·다음 틱) 클라가 `NAME_None`을 먼저 받는다. `SpawnActorDeferred`는 필요 없다.

이 한 줄이 §3-6의 걱정도 같이 없앤다 — 지켜야 할 것이 "같은 프레임"뿐이라, `InitPickup`의 시그니처가 Step 03에서 바뀌어도 **호출 지점은 안 움직인다.**

### 4-2. (b) 맞다. 다만 "무관하게"가 아니라 "별도 경로"다

`UNetDriver::NotifyActorDestroyed`(`NetDriver.cpp:4331-4360`)가 연결마다 갈린다.

```cpp
// :4336  UActorChannel* Channel = Connection->FindActorChannelRef(ThisActor);
// :4337  if (Channel) { ... Channel->Close(CloseReason); }
// :4345  else {
//            const bool bDormantOrRecentlyDormant = NetworkObjectInfo &&
//                (NetworkObjectInfo->DormantConnections.Contains(Connection) ||
//                 NetworkObjectInfo->RecentlyDormantConnections.Contains(Connection));
// :4347        if (bShouldCreateDestructionInfo || bDormantOrRecentlyDormant) {
// :4351            DestructionInfo = CreateDestructionInfo(ThisActor, DestructionInfo);
// :4354            Connection->AddDestructionInfo(DestructionInfo);
```

엔진 주석이 그대로 답이다(`:4349-4350`):

> *"Make a new destruction info if necessary. It is necessary if the actor is dormant or recently dormant because **even though the client knew about the actor at some point, it doesn't have a channel to handle destruction.**"*

즉 **휴면 중인 연결에는 채널이 없으므로 채널로 못 보내고, 대신 파괴 정보(destruction info)라는 별도 경로로 보낸다.** 무관해서 통과하는 게 아니라 엔진이 그 경우를 따로 처리한다.

그리고 **채널이 한 번도 열린 적 없는 클라**(멀어서 릴러번시 밖)에 대한 사용자의 이해도 정확하다 — `bDormantOrRecentlyDormant`가 false이고, 동적 액터라 `bShouldCreateDestructionInfo`도 false다(`:4319` `!GetNetGuidCache()->IsDynamicObject(ThisActor)`). **아무것도 안 보낸다.** 클라가 애초에 모르므로 무해하다.

**문서 수정:**

```
획득 완료  → Destroy()    ← 휴면 중인 연결에도 전달된다 (채널이 아니라 destruction info로)
```

### 4-3. ★ (c) 완료 조건 6은 `DORM_Initial`과 양립하지 않는다 — 추측 두 개 모두 아니다

사용자의 추측: ① 채널이 닫혔다 다시 들어오면 재생성된다(원문 이해) ② `DORM_Initial`이 "한 번 열렸으면 다시 안 연다"로 동작하면 실패한다(우려).

**실제로는 셋째다: 채널이 릴러번시로 닫히는 일 자체가 일어나지 않는다.**

순서를 따라가면 이렇게 된다.

| 패스 | 일어나는 일 | 근거 |
|---|---|---|
| N | 채널 없음 + 릴러번시 안 → **채널 생성, `ItemId` 복제** | `NetDriver.cpp:5396` (릴러번시 밖이면 `continue`, 채널 안 만듦) |
| N+1 | 채널 있음 → `ShouldActorGoDormant` true → `StartBecomingDormant()` | `:5435-5440` |
| N+k | 프로퍼티 다 보냈으면 `Close(EChannelCloseReason::Dormancy)` | `DataChannel.cpp:4578` |
| — | 클라는 **액터를 파괴하지 않고** `ClientSetActorDormant()`로 유지 | `DataChannel.cpp:2317-2321`, `:2691` |
| 이후 전부 | `DormantConnections`에 들어갔으므로 `IsActorDormant` → `continue` | `:5310`, `:5429` |

마지막 줄이 결정적이다. **휴면 진입 후에는 릴러번시 검사(`:5396`)에 도달조차 하지 않는다.** 그래서:

- **아직 못 본 픽업**: `SetNetCullDistanceSquared(25000000.f)`가 작동한다. 5000cm 밖이면 채널이 안 열리고 클라에 존재하지 않는다. **가까이 가면 나타난다 ✔**
- **한 번 본 픽업**: 멀어져도 **클라에서 사라지지 않는다.** 채널은 닫혔지만 액터는 살아 있고 대역폭은 0이다

**그리고 이게 원하던 동작이다.** 픽업이 거리에 따라 사라지고 나타나면 팝핑이 보이고, 다시 볼 때마다 채널 재생성 + 초기 번들을 다시 보낸다. 휴면은 정확히 그걸 없애기 위한 것이다. **결함이 아니라 완료 조건 문구가 틀렸다.**

**문서 수정 — 완료 조건 6을 둘로 가른다.**

```
- [ ] 스폰 지점에서 5000cm 밖에 서서 클라 창 `EP.Loot.List` → **목록에 없다.**
      가까이 가면 나타난다 (아직 채널이 열린 적 없는 픽업에만 컬링이 걸린다)
- [ ] 한 번 본 픽업은 멀어져도 목록에 남는다 — **이게 정상이다.**
      `DORM_Initial`은 휴면 진입 시 클라 액터를 유지하고(`ClientSetActorDormant`),
      그 연결은 이후 릴러번시 검사에 도달하지 않는다
```

`EP.Loot.List`가 이미 있으므로 검증 수단은 추가로 필요 없다. **이 두 줄이 §3-2의 커맨드가 만드는 두 번째 값**이다.

> Dormancy 규칙 블록에 한 줄 추가:
> **동적 스폰 액터에서 `DORM_Initial`은 `DORM_DormantAll`과 같다** — `IsDormInitialStartupActor`(`NetDriver.cpp:8349`)가 `IsNetStartupActor()`를 요구하므로 맵 배치 액터만 특별 취급된다. 스폰된 픽업은 정상적으로 한 번 복제된 뒤 휴면한다.

---

## 5. 판단 3-2 — 클라 `EP.Loot.List`

### 결론: 커맨드는 유지한다. (ㄱ)을 적용하고, **완료 조건 문구를 같이 고쳐야 한다.**

**(ㄴ)은 안 된다.** 코드 리뷰로 갈음하면 나중에 누가 `DOREPLIFETIME(AEPPickup, State)`를 추가할 때 **아무도 안 걸린다.** 함정 #10이 존재하는 이유가 정확히 그것이고, 관측 수단을 없애면 함정표가 경고문으로만 남는다. Step 03에서 `State`가 `Payload` 배열로 **교체**되면(01-4 각주) 그때 복제 등록을 다시 판단하게 되는데, 그 시점에 되돌아올 위험이 가장 크다.

**진짜 약점은 사용자가 지적한 그대로이고, 그건 커맨드의 한계가 아니라 절차의 한계다.** `Charges == 0`이 "복제 안 됨"과 "진짜 0"을 구분하지 못하는 건 맞다. 고칠 것은 출력 형식이 아니라 **완료 조건이 요구하는 순서**다.

```
- [ ] 서버 창 `EP.Loot.List`에서 `Charges > 0`인 픽업의 `Idx`를 먼저 확인한다.
      **같은 Idx가 클라 창에서 0**이면 통과. 12가 찍히면 `DOREPLIFETIME`에 `State`를 넣은 것이다.
      서버에 `Charges > 0`인 픽업이 하나도 없으면 이 조건은 **검증되지 않은 것**이다.
```

**두 창 대조가 검증이고, 클라 출력 단독은 아니다.** 그리고 Step 01에서 이 절차가 실제로 성립한다 — `AmmoBox_545`와 `Cash_10000`이 `InitialCharges`를 가지므로(Step 00 `MakeItemState`) 서버 목록에 `Charges > 0`이 반드시 나온다. 절차만 적으면 된다.

### 그리고 예시 출력에 Step 01에 없는 열이 둘 있다

01-5:459-467의 표에 `Charges` `Claimed` `Cooldown` `Payload` 4열이 있다. 그런데 Step 01의 `AEPPickup`(01-4)에는:

| 열 | Step 01에 있나 |
|---|---|
| `Charges` | ✅ `State.Charges` |
| `Claimed` | ✅ `bClaimed` (01-4에 필드 선언 있음, 사용은 Step 02) |
| `Cooldown` | ❌ **없다** — `DropCooldown`은 Step 03 |
| `Payload` | ❌ **없다** — `State`가 `Payload`로 교체되는 것이 Step 03 |

**즉 이 예시 출력은 Step 01 코드로 만들 수 없다.** 그리고 하필 클라 출력의 `Payload 0`이 "복제 안 됨의 증거"처럼 보이는데, `Payload`는 서버에서조차 Step 01에는 없고 표(`:479`)는 `1` 고정으로 그려 놓았다 — **아무것도 증명하지 않는 숫자가 증명하는 것처럼 놓여 있다.**

**(ㄱ)을 적용하되 이유는 "이상하지 않다고 설명하기 위해"가 아니다.** 관측 대상이 아닌 값을 숫자로 찍으면 읽는 사람은 그것이 검증에 쓰인다고 읽는다. Step 01 출력은 이렇게 좁힌다.

```
> EP.Loot.List                                        (서버 창)
  Idx  ItemId          Location            Charges  Claimed
  0    Bandage         (1200, 340, 92)     1        false
  1    AmmoBox_545     (880, -20, 90)      45       false     ← 대조 대상

> EP.Loot.List                                        (클라 창 — [server-only] State)
  Idx  ItemId          Location            Charges  Claimed
  0    Bandage         (1200, 340, 92)     0        -
  1    AmmoBox_545     (880, -20, 90)      0        -         ← 서버의 45가 0이면 통과
```

`Cooldown`/`Payload` 열은 **Step 03에서 추가한다**고 명시한다. 지금 표(`:476-480`)의 "증명하는 것" 행 중 두 줄은 Step 03 문서로 옮긴다 — 3차 §11이 잡은 중복 문제와 같은 부류다(사실 하나가 두 문서에).

---

## 6. 판단 3-3 — `FRand()` 닫힌 구간

### 결론: 과잉이 아니다. 오히려 **근거를 약하게 적었다.** 다만 권고 형태는 바꿔야 한다.

**근거가 성립하는가: 성립한다.** `CLAUDE.md`의 "불가능한 시나리오"는 *논리적으로 도달 불가한 분기*를 말한다. 여기는 도달 가능하고, 도달했을 때 **틀린 정보를 출력한다**(데이터가 정상인데 "테이블이 잘못됐다"는 Error 로그). 확률이 낮은 것과 무해한 것은 다르다. 특히 이 로그는 §2-2의 규약이 걸고 있는 신뢰의 근거다 — 거짓 양성이 한 번 나오면 그 다음부터 아무도 그 로그를 안 믿는다.

**그런데 2⁻²⁴가 아니다.** 닫힌 구간은 두 원인 중 하나일 뿐이다.

`TotalWeight`는 **더하면서** 만들고 `Pick`은 **빼면서** 소모한다. float에서 `((T - w₁) - w₂) - ... - wₙ`은 `T = w₁ + w₂ + ... + wₙ`이었어도 정확히 0이 되지 않는다 — 양의 잔차가 남을 수 있다. 그러면 `Pick`이 `TotalWeight`와 **정확히 같지 않아도** 루프가 끝까지 음수가 안 되어 fall-through한다. 발생 확률은 상대 오차 규모(float ≈ 1e-7)에 비례하고, 엔트리가 많고 가중치 편차가 크면 커진다. **2⁻²⁴(≈6e-8)보다 오히려 흔하다.**

### 권고 형태는 바꾼다 — "마지막 엔트리를 집어 준다"에 문제가 둘 있다

① `Weight == 0`인 엔트리가 마지막이면 **뽑히지 않아야 할 아이템이 뽑힌다**(디자이너가 "일단 꺼둔" 엔트리). ② `SubTable` 엔트리면 재귀 호출 3줄을 fall-through 자리에 복제해야 한다.

**한 루프로 재구성하면 fall-through 자체가 없어진다.** 줄 수는 거의 같고 상태는 하나 줄어든다.

```cpp
    // 뽑힌 엔트리를 가리킨다. 루프가 끝나도 nullptr이 아니면 "마지막 유효 엔트리"가 담긴다
    const FEPLootEntry* Chosen = nullptr;

    for (const FEPLootEntry& E : Table->Entries)
    {
        if (E.Weight <= 0.f) continue;          // 가중치 0은 뽑히지 않는다 — 명시적으로
        Chosen = &E;
        if ((Pick -= E.Weight) < 0.f) break;    // 여기서 확정
    }

    // nullptr = 유효 엔트리가 하나도 없다 = 진짜 데이터 오류
    if (!Chosen) return false;

    if (Chosen->SubTable) return RollLootTable(Chosen->SubTable, OutItemId, Depth + 1);

    if (Chosen->ItemId.IsNone())
    {
        // ★ §2-2 — ItemId도 SubTable도 없는 엔트리. true + None으로 내보내면
        //    스포너가 "정상적인 빈 결과"로 읽어 데이터 오류가 묻힌다
        UE_LOG(LogTemp, Error, TEXT("[Loot] %s: ItemId도 SubTable도 없는 엔트리"),
               *GetNameSafe(Table));
        return false;
    }

    OutItemId = Chosen->ItemId;
    return true;
```

이 한 덩어리가 셋을 동시에 닫는다.

| | |
|---|---|
| 닫힌 구간 + 부동소수 잔차 | `Chosen`이 마지막 **유효** 엔트리를 이미 들고 있으므로 fall-through가 없다 |
| `Weight <= 0` 엔트리 | 명시적으로 건너뛴다 (원래 코드는 `Pick -= 0 >= 0`으로 **우연히** 건너뛰었다) |
| **§2-2의 빈 `ItemId`** | 규약 위반을 `false`로 정직하게 반환 |

그리고 `false`의 의미가 하나로 좁혀진다: **"이 테이블에서는 아무것도 뽑을 수 없다."** 널·깊이 초과·`TotalWeight <= 0`·유효 엔트리 0개·엔트리 미기입이 전부 그 뜻이고, `true + NAME_None`은 **루트 `EmptyWeight`뿐**이다. 규약 두 겹이 이제 코드에 있다.

### 두 가지 더

**① `OutItemId`를 함수가 초기화해야 한다.** 지금은 빈 결과 경로(`:91`)에서 `OutItemId`를 건드리지 않고 `true`를 반환한다. 호출자가 `FName RolledId = NAME_None;`으로 선언하기를 **믿고 있는 것**이고, 그 규약은 어디에도 안 적혀 있다. 함수 진입부에 `OutItemId = NAME_None;` 한 줄. **불변식을 코드로** — 이 프로젝트가 3차·4차에서 계속 적용한 규칙이다.

**② `UEPLootTable::IsDataValid()`.** `UEPItemDefinition`이 Step 00에서 이미 갖고 있는 패턴이다(`EPItemDefinition.h:47`). 런타임 `Error` 로그는 그 테이블이 실제로 뽑힐 때까지 안 뜨는데, 에디터 검증은 저장하는 순간 뜬다. 넣을 값이 있는 항목만:

- `ItemId`와 `SubTable`이 **둘 다 비었거나 둘 다 채워진** 엔트리
- `SubTable == this` (깊이 1 자기 참조 — 깊이 상한 8은 이걸 못 막는 게 아니라 **뒤늦게** 막는다)
- `Entries`가 비어 있고 `EmptyWeight == 0`

`ItemId`가 DT에 있는지 검사는 넣지 않는다 — `IsDataValid` 시점에 DT 로드를 강제하게 되고, 그건 `MakeItemState` 실패(함정 #11)가 이미 잡는다.

**`static`은 헤더에 붙이지 않는다.** 01-1:106이 *"`EPLootTable.h`에 정적 자유 함수로 선언"* 이라고 적었는데, 헤더의 `static` 자유 함수는 TU마다 사본이 생기고 안 쓰는 TU에서 `-Wunused-function`이 난다. **헤더에는 `EMPLOYMENTPROJ_API bool RollLootTable(const UEPLootTable*, FName&, int32 Depth = 0);` 선언만, 정의는 `EPLootTable.cpp`에.** 기본 인자는 선언 쪽에만 쓴다.

---

## 7. 판단 3-4 — 픽업 콜리전 프로파일

### 결론: **Step 01에서 정해야 한다.** 단 Step 02의 트레이스 때문이 아니다. 그리고 **채널을 앞당기지 않는다.**

### 7-1. 지금 문서대로 짜면 Step 01 안에서 문제가 보인다

C++로 만든 `UStaticMeshComponent`의 기본 상태를 확인했다.

```cpp
// BodyInstance.cpp:363  , ObjectType(ECC_WorldStatic)
// BodyInstance.cpp:365  , CollisionEnabled(ECollisionEnabled::QueryAndPhysics)
// BodyInstance.cpp:396  , CollisionProfileName(UCollisionProfile::CustomCollisionProfileName)
// BodyInstance.cpp:299  ResponseToChannels = FCollisionResponseContainer::GetDefaultResponseContainer();

// CollisionProfile.cpp:373
FCollisionResponseContainer::DefaultResponseContainer.SetAllChannels(ECR_Block);
```

**프로파일은 `Custom`이고 반응은 전 채널 `Block`이다.** 01-4 생성자의 `SetCollisionEnabled(QueryOnly)` 한 줄은 물리 시뮬레이션만 뺀 것이고, **쿼리는 그대로 막는다** — `UCharacterMovementComponent`의 이동 스윕은 쿼리다.

그래서 Step 01만 돌려도 이렇게 된다.

| 채널 | 픽업의 기본 반응 | 결과 |
|---|---|---|
| `Pawn` | **Block** | **플레이어가 플레이스홀더 Cube에 걸리고 올라탄다.** 바닥 아이템 5개가 장애물이 된다 |
| `Visibility` | **Block** | `bAlignToGround` 라인트레이스가 **먼저 뿌린 픽업**에 걸려 다음 픽업이 그 위에 얹힌다 (§3-5의 우려가 여기서 실재) |
| `WeaponTrace` (`ECC_GameTraceChannel1`) | **Ignore** | 총알은 안 막힌다 — `DefaultEngine.ini:306`이 `DefaultResponse=ECR_Ignore`라서 |

완료 조건에 "맞는다"가 없어서 Step 01만으로는 검증이 안 된다고 봤는데, **"걸린다"는 PIE에서 즉시 보인다.** 프로파일 미정의 비용은 Step 02로 미뤄지지 않는다.

### 7-2. 프로파일 이름을 예고하지 말고, 반응을 코드로 두 줄 적는다

```cpp
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;

    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionObjectType(ECC_WorldDynamic);        // 런타임 스폰물 — 기본값 WorldStatic이 아니다
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);   // ★ 아무것도 막지 않는다
```

Step 02가 채널을 만들면 **한 줄만 추가한다.**

```cpp
    Mesh->SetCollisionResponseToChannel(EP_TraceChannel_Interact, ECR_Block);
```

이게 이음매다. Step 01은 픽업을 **모든 채널에 투명하게** 만들고(그래서 아무것도 망가뜨리지 않는다), Step 02가 정확히 하나를 연다. **존재하지 않는 채널을 Step 01이 참조하지 않으므로 채널을 앞당길 필요가 없다.** 그리고 Step 01이 검증 가능한 완료 조건을 하나 얻는다.

```
- [ ] 픽업을 향해 사격해도 총알이 막히지 않고, 픽업 위를 걸어도 걸리지 않는다
```

프로파일 에셋(`.ini`의 `+Profiles=`)을 만들지 않는 이유: 픽업 하나뿐이고, 프로파일은 에디터에서 눈에 안 보이는 곳(`DefaultEngine.ini`)에 값이 흩어진다. 반응이 세 줄이면 코드에 두는 편이 읽기 쉽다. 상호작용 대상이 셋(픽업·컨테이너·자판기)이 되면 그때 `"EPInteractable"` 프로파일로 묶는다 — `EquippedEntryId`가 셋이 되면 `TMap`으로 간다는 판단(`LOOT_STATUS.md:60`)과 같은 기준이다.

### 7-3. ★ 곁가지로 Step 02 함정 #5의 진단이 불완전하다

Step 02 함정 #5는 *"`ECC_Visibility` 재사용 → 픽업 앞 잡동사니가 상호작용을 막음 → 대응: 전용 채널 `EP_TraceChannel_Interact`"* 라고 적었다.

**전용 채널을 만드는 것만으로는 안 풀린다.** 새 채널을 `DefaultResponse = ECR_Block`으로 만들면 그 값이 **기본 응답 컨테이너에 들어간다.**

```cpp
// CollisionProfile.cpp:470
FCollisionResponseContainer::DefaultResponseContainer.SetResponse(
    (ECollisionChannel)EnumIndex, CustomChannel.DefaultResponse);
```

그러면 반응을 따로 지정하지 않은 **모든** 프리미티브(벽·바닥·소품)가 새 채널도 막는다 — `ECC_Visibility` 재사용과 결과가 똑같다. 함정을 피했다고 생각하며 그대로 밟는다.

**해결책은 "새 채널"이 아니라 "기본 응답 `Ignore` + 픽업만 `Block`"이다.** 그리고 이 프로젝트에는 이미 관례가 있다.

```ini
; DefaultEngine.ini:306
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="WeaponTrace")
```

`EP_TraceChannel_Interact`도 **`DefaultResponse=ECR_Ignore`, `bTraceType=True`** 로 만든다. 그러면 7-2의 두 줄과 정확히 맞물린다. Step 02 문서의 함정 #5 대응란을 그렇게 고치고, `LOOT_STATUS.md:93`("`EP_TraceChannel_Interact` 신규 채널")에도 `DefaultResponse=Ignore`를 붙이는 것을 권한다 — 등록 설정이 코드보다 잊기 쉽고, Step 00의 `Is Editor Only` 사건이 정확히 그 부류였다.

---

## 8. 판단 3-5 — 스폰 겹침과 접지

세 항목이 성격이 다르다.

### 8-1. `SpawnCollisionHandlingOverride` — 걱정할 근거가 없다

```cpp
// Actor.cpp:340 (AActor::AActor)
SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

// LevelActor.cpp:623-629
ESpawnActorCollisionHandlingMethod CollisionHandlingMethod = Template->SpawnCollisionHandlingMethod;
if (SpawnParameters.SpawnCollisionHandlingOverride != ESpawnActorCollisionHandlingMethod::Undefined)
    CollisionHandlingMethod = SpawnParameters.SpawnCollisionHandlingOverride;
```

`Undefined`(기본값)는 **클래스 기본값으로 폴백**하고, `AActor`의 그 값은 `AlwaysSpawn`이다. **조용히 실패하지 않는다.**

"조용히 실패한다"는 기억은 `APawn`에서 온 것으로 보인다 — `Pawn.cpp:91`이 `AdjustIfPossibleButDontSpawnIfColliding`으로 덮는다. 캐릭터 스폰이 겹쳐서 안 되던 경험이 `SpawnActor` 전반의 인상으로 남은 것이다. `AActor` 직계 상속인 픽업에는 해당 없다.

§3에 넣은 본문에 `AlwaysSpawn`을 **명시적으로** 쓴 것은 방어가 아니라 문서화다 — 스폰이 실패할 수 없다는 것을 읽는 사람이 확인 없이 알게 된다. 없어도 동작은 같다.

### 8-2. 접지 채널 — 적어야 하고, §7과 한 줄로 같이 닫힌다

이건 실재한다. 그런데 §7-2의 `SetCollisionResponseToAllChannels(ECR_Ignore)`가 원인을 제거한다 — 픽업이 `Visibility`를 안 막으므로 **픽업끼리 걸리지 않는다.** 그래서 `ECC_Visibility`로 충분하고, 문서에 채널명과 **"왜 충분한지"** 를 함께 적어야 한다. 채널명만 적으면 나중에 §7-2를 "정리"할 때 이 의존이 안 보인다. §3의 `GetSpawnPoint()` 주석이 그 형태다.

`Params.AddIgnoredActor(this)`도 필요하다 — 스포너 자신의 빌보드는 `Visibility`를 막지 않지만(`UBillboardComponent`), 나중에 스포너에 콜리전이 붙으면 자기 발밑에 걸린다.

### 8-3. `SpawnRadius = 0`으로 전부 한 점 — 미룬다. 단 명시적으로

미뤄도 안전하다. §7-2 이후에는 픽업이 서로를 안 막으므로 **겹쳐도 아무것도 고장나지 않는다** — 겹쳐 보일 뿐이고, 그건 레벨 디자인 문제다. `SpawnRadius`의 `meta = (ClampMin = "0.0")`도 그대로 둔다(단일 스폰 지점이 정당한 용례다).

**"완료 조건에 없으니 무시한다"가 아니라 "고장나지 않으므로 미룬다"로 적는다.** 이 단계에서 하지 않는 것 절에 한 줄:

```
- 스폰 지점 겹침 방지 / 분산 품질 → 레벨 디자인.
  픽업이 서로의 콜리전을 막지 않으므로(01-4) 겹쳐도 기능은 정상이다
```

이유가 적혀 있으면 나중에 "겹치는데 왜 방치했나"를 다시 판단하지 않는다.

---

## 9. 판단 3-6 — `InitPickup` 시그니처와 완료 조건

### 결론: 지금 진행이 맞다. 그리고 **"절반 다시 쓴다"도 아니다.**

세 번 고친다는 계산이 §4-1로 두 번이 된다 — **Deferred 전환이 없다.** 남는 것은 01 작성과 Step 03 배열 전환뿐이고, Step 03에서 실제로 바뀌는 것은 **두 곳**이다.

| Step 03에서 바뀌는 것 | 바뀌지 않는 것 |
|---|---|
| `InitPickup`의 두 번째 인자 (`const FEPItemState&` → `TArray<FEPInventoryEntry>&&`) | 호출 **지점** (`SpawnActor` 직후, 같은 프레임) |
| `State` 필드 → `Payload` 필드 | 생성자 / `ApplyVisual` / `OnRep_ItemId` / `EndPlay` 없음 / Dormancy |
| `GetState()` → `GetPayload()` | 콜리전 (§7-2) / 복제 등록 없음 / 함정 #10 |

**`AEPPickup`의 5개 함수 중 하나의 인자와 필드 하나다.** "절반 다시 쓴다"가 아니라 **"한 줄 서명과 한 필드"** 다. 그렇게 받아들이고 진행하는 것이 맞다.

### 완료 조건을 어떻게 잡는가 — 묻는 질문이 정확하다

**Step 03에서 바뀔 것을 아는 상태의 완료 조건은 "구조"가 아니라 "관측"으로 적는다.** 구조로 적으면 Step 03에서 그 문장이 거짓이 되고, 관측으로 적으면 그대로 살아 있다.

| 지금 문서 | 이렇게 |
|---|---|
| (완료 조건 7) *"클라이언트 패킷에 픽업의 `Charges`가 나가지 않는다"* | 그대로 좋다 — `State`든 `Payload`든 관측이 같다 |
| 01-4의 *"`FEPItemState` 8바이트라 대역폭은 이유가 못 된다"* | Step 03에서 `Payload`가 배열이 되면 **거짓이 된다.** 근거를 "8바이트"가 아니라 **"정보 은폐"** 단독으로 남긴다. 정보 은폐는 배열이 되어도 그대로다 |
| (`InitPickup` 계약) 문서에 없음 | **"`SpawnActor`와 같은 프레임"** 을 §4-1대로 넣는다. 시그니처가 바뀌어도 이 계약은 안 바뀐다 |

즉 Step 01의 완료 조건은 **지금 그대로 두고**, `AEPPickup` 각주(01-4:294)에 "Step 03에서 바뀌는 것" 표(위 3열 중 왼쪽 두 칸)를 넣는 것으로 충분하다. 그러면 Step 03에서 고칠 곳이 문서에 열거돼 있어 찾지 않는다.

---

## 10. 그 외 — 구현 직전에 걸릴 것

셋 다 컴파일 단계에서 5분에 걸리는 부류지만, 지금 적어 두면 0분이다.

### 10-1. 헤더 전방 선언 / include

| 파일 | 필요한 것 |
|---|---|
| `EPPickup.h` | `#include "Types/EPTypes.h"` — `FEPItemState`가 **값 멤버**라 완전 타입이 필요하다 (전방 선언으로 안 된다). `struct FStreamableHandle;` / `class UStaticMeshComponent;` 전방 선언 |
| `EPItemSpawner.h` | `class AEPPickup;` — `TWeakObjectPtr<AEPPickup>`에 필요. `class UEPLootTable;` |
| `EPLootTable.h` | `UEPLootTable`이 `FEPLootEntry`보다 **뒤에** 선언되는데 `FEPLootEntry::SubTable`이 `TObjectPtr<UEPLootTable>`을 쓴다 → 파일 상단에 `class UEPLootTable;` 전방 선언 필요 |
| `EPPickup.cpp` | `Engine/AssetManager.h`(`UAssetManager::GetStreamableManager`), `Engine/StreamableManager.h`, `EPLootDeveloperSettings.h`, `EPItemDefinitionSubsystem.h` |

### 10-2. 01-4:278의 `RolledItemId`가 선언되지 않았다

```cpp
// 스포너 (01-2)
FEPItemState NewState;
if (Defs->MakeItemState(RolledItemId, NewState))     // ← RolledItemId / Defs 둘 다 없다
    Pickup->InitPickup(RolledItemId, NewState);
```

§3의 `SpawnLoot()` 본문이 들어가면 이 조각은 삭제한다 — 같은 코드가 두 곳에 있으면 3차 §11이 잡은 중복 문제가 반복된다. **조각을 남길 이유가 "버리기 경로와 나란히 보여주기"라면**, 나란히 놓을 것은 코드가 아니라 문장이다: *"두 경로 모두 `InitPickup`으로 들어오고, 잔탄은 규칙이 아니라 값 복사로 보존된다."*

### 10-3. `EP.Loot.Respawn`의 권한

문서가 "(서버 전용)"이라고만 적었다. 스포너의 `HasAuthority()`가 클라에서 false이므로(§3) 클라에서 실행하면 **조용히 아무 일도 안 일어난다.** 커맨드 쪽에서 막고 이유를 찍는 편이 낫다.

```cpp
if (World->GetNetMode() == NM_Client)
{
    UE_LOG(LogTemp, Warning, TEXT("[Loot] Respawn은 서버 전용입니다."));
    return;
}
```

`EP.Loot.List`는 반대로 **클라에서 돌아야 한다**(§5) — Step 00이 이미 "순수 조회는 클라 허용"으로 구분한 그 기준이다.

---

## 11. 사용자 결정이 필요한 것

**즉시 반영 권장 (Step 01 착수 전)**

| # | 내용 | 근거 |
|---|---|---|
| 1 | **`SpawnLoot()` / `GetSpawnPoint()` / `ClearLoot()` 본문**을 01-2에 추가 | §3 — Step 01의 유일한 새 로직이 문서에 없다 |
| 2 | `RollLootTable`을 **단일 루프 + `Chosen`** 으로 재구성 (+ `OutItemId = NAME_None` 초기화) | §6 — fall-through·`Weight<=0`·§2-2 구멍이 한 번에 닫힌다 |
| 3 | 생성자에 **`SetCollisionObjectType(ECC_WorldDynamic)` + `SetCollisionResponseToAllChannels(ECR_Ignore)`** | §7 — 지금대로면 플레이어가 픽업에 걸리고 접지가 어긋난다 |
| 4 | `GetPrimaryAssetId()`에 **`final`** | §1-1 — 순서 규칙은 이번만, `final`은 앞으로 전부 |
| 5 | **`InitPickup`은 `SpawnActor`와 같은 프레임** 계약 한 줄 | §4-1 — 유일하게 필요한 계약이고 문서에 없다 |
| 6 | **완료 조건 6을 둘로 가른다** (못 본 픽업만 컬링 / 본 픽업은 남는 게 정상) | §4-3 — 현재 문구는 검증하면 실패한다 |
| 7 | **완료 조건 7에 두 창 대조 절차** 명시 | §5 — 클라 출력 단독으로는 증명이 안 된다 |
| 8 | 예시 출력에서 **`Cooldown`/`Payload` 열 제거** (Step 03으로) | §5 — Step 01 코드로 만들 수 없는 출력 |
| 9 | Dormancy 규칙에 **"동적 스폰에서 `DORM_Initial` = `DORM_DormantAll`"** + 파괴는 destruction info 경로 | §4-1·4-2 |
| 10 | 10-1 전방 선언 / 10-2 조각 삭제 / 10-3 `Respawn` 권한 | §10 |

**Step 02 문서에 반영 (지금 적어야 잊지 않는다)**

| # | 내용 | 근거 |
|---|---|---|
| 11 | 함정 #5 대응을 **"전용 채널"에서 "`DefaultResponse=ECR_Ignore` + 픽업만 `Block`"** 으로 | §7-3 — 채널만 만들면 같은 함정을 그대로 밟는다 |
| 12 | `LOOT_STATUS.md:93`에 **`DefaultResponse=Ignore`, `bTraceType=True`** 명시 | §7-3 — `.ini` 설정은 코드보다 잊기 쉽다 (`Is Editor Only` 사건) |

**판단 보류 / 사용자 몫**

| # | 내용 |
|---|---|
| 13 | `UEPLootTable::IsDataValid()` 3항목 — 넣으면 §6의 런타임 검사가 에디터로 앞당겨진다. 미루면 §6만으로도 안전하다 (§6) |
| 14 | Step 00의 `UEPItemDefinition::GetPrimaryAssetId()`에 `final` — 완료된 단계 코드 (§1-1) |
| 15 | 01-4의 "8바이트라 대역폭은 이유가 못 된다"를 지금 지울지, Step 03에서 지울지 (§9) |
