# 검수 응답 6차 — 전역 데이터 참조를 어디에 둘 것인가

> 작성일: 2026-07-30
> 대상: `05_Loot_REVIEW6_Request.md`
> 성격: 아키텍처 판단 1건 + 엔진/Lyra 소스 직독 근거
> **Lyra 소스는 로컬에 있었다** — `C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame`. 이 문서의 Lyra 인용은 전부 그 파일들을 직독한 것이다. 기억으로 쓴 것은 하나도 없다

---

## 0. 결론 요약

| 질문 | 판정 |
|---|---|
| §2 문서 vs Claude 진단 | 🔀 **둘 다 부분적으로 틀렸다.** 문서의 **결론(=`TSoftObjectPtr`를 써라)은 맞고 이유가 틀렸다.** Claude의 진단은 사실관계는 맞았으나 **"에디터가 경고 다이얼로그를 띄우고 리다이렉터를 강제로 남긴다"는 것을 몰랐다** |
| §2-1 config `TSoftObjectPtr`가 레지스트리 간선을 만드는가 | ❌ **안 만든다.** 쿠커가 config 키 5개를 **이름으로 하드코딩**해 읽는 것이 증거 |
| §2-2 리네임 시 `.ini`가 다시 쓰이는가 | ❌ **안 쓰인다.** 대신 **탐지해서 사람에게 고치라고 말한다** |
| §2-3 `Fix Up Redirectors` 후 살아남는가 | ❌ 끊긴다 |
| §2-4 패키지 빌드 | ⚠️ **엔진이 직접 "cooked build에서 빠질 수 있다"고 말한다.** 단 우리 `DT_Items`는 DA가 하드 참조해서 **쿡은 안전**하다 |
| 4-1 "곧 바꿀 거면 지금" 전제 | ⚠️ **전제는 옳지만 이 사안에 적용되지 않는다.** 미루는 비용이 참조 개수에 비례하지 않는다 |
| 4-2 A~E | ✅ **A 유지.** B는 이점의 근거가 §2 판정으로 **무너졌고**, Lyra 직독 결과 **Lyra 자신이 A를 한다** |
| 4-3 `PlaceholderPickupMesh` | ✅ 옮길 대상이 아니다. `UDeveloperSettings`가 그 필드의 **정확한 자리**다 |
| 4-4 `PickupClass` 전역 | ✅ §7까지 유지된다. **단 §7-2가 두 번째 축을 만든다** — 아이템별이 아니라 **스폰 지점별**. 지금 결정과 충돌하지 않지만 문서에 한 줄이 필요하다 |
| 4-5 `TSubclassOf` vs `TSoftClassPtr` | ❌ **`TSubclassOf`는 틀렸다. 이번 요청에서 실제로 위험한 유일한 항목이다** — 지금 작성 중인 코드에 들어간다 |

**한 줄 요약:** 이번 요청의 답은 *"구조를 바꿀 필요는 없다"* 이고, 대신 **바꿔야 할 것 하나가 §4-5에 숨어 있었다.** `UPROPERTY(Config) TSubclassOf<AEPPickup>`는 `.ini`에 BP 경로가 들어가는 순간 **CDO 생성 시점(= 엔진 초기화 중)에 동기 로드**를 일으키고, 실패해도 **조용히 null**이 된다. Lyra는 같은 자리에서 `TSoftClassPtr`(config) + `TSubclassOf`(런타임 해석본) **쌍**을 쓴다.

---

## 1. ★ §2 판정 — 확정

### 1-1. 엔진이 직접 답을 적어 놓았다

에셋을 리네임하면 에디터는 **모든 네이티브 CDO를 훑어** 그 에셋을 가리키는 소프트 참조를 찾는다.

```
AssetRenameManager.cpp:437   // Warn the user if they are about to rename an asset that is referenced by a CDO
AssetRenameManager.cpp:708   FAssetRenameManager::FindCDOReferences(...)
              :724             for (TObjectIterator<UClass> ClassDefaultObjectIt; ...)
              :759             FSoftObjectPathRenameSerializer SoftRefCheckSerializer(...)
              :763             CDO->Serialize(SoftRefCheckSerializer);      ← CDO를 통째로 직렬화
              :796             AssetToRename->bCreateRedirector |= bSetRedirectorFlags;
```

찾으면 이 다이얼로그를 띄운다 — **문구가 그대로 답이다.**

> `AssetRenameManager.cpp:463`
> *"**Source code, config INI, and text files may need Find/Replace** for: {0}
> Otherwise **assets can be missing from cooked builds.** Continue with rename?"*

정리하면 **네 단계 중 셋까지만 해 준다.**

| | 해 주는가 |
|---|---|
| ① 리네임을 **탐지**한다 (CDO 직렬화) | ✅ |
| ② 리다이렉터를 **강제로 남긴다** (`:796`) | ✅ |
| ③ 사람에게 **경고**한다 (`:463`) | ✅ |
| ④ `.ini`를 **고쳐 준다** | ❌ **"may need Find/Replace" — 직접 하라는 뜻** |

### 1-2. 그래서 누가 틀렸나

**문서(`05_Loot_DOCS.md:823` / `00_ItemCore.md:610`)** — *"소프트 포인터는 에디터가 참조를 추적해 리다이렉터를 따라간다."*

- "에디터가 참조를 추적해" → **맞다.** `FindCDOReferences`가 정확히 그것이다
- "리다이렉터를 따라간다" → **맞다.** 게다가 `:796`이 리다이렉터를 **강제로** 남긴다
- **틀린 것은 생략된 결론이다.** 이 문장은 "그러니 안전하다"로 읽히는데, 엔진은 같은 자리에서 *"cooked build에서 빠질 수 있다"* 고 경고한다. **탐지는 하지만 수리는 안 한다**

**Claude의 진단** — *"`.ini`의 경로는 그냥 문자열이라 애셋 레지스트리 참조가 아니다. 이름을 바꾸면 `.ini` 문자열은 안 고친다. `Fix Up Redirectors`를 돌리면 끊긴다."*

- 사실관계 세 개는 **전부 맞다**
- 빠진 것: **에디터가 침묵하지 않는다.** 다이얼로그가 뜨고, 리다이렉터가 강제로 남는다. "조용히 깨진다"가 아니라 **"경고를 무시하면 깨진다"**

**따라서 문서의 *결정*(`FName`이 아니라 `TSoftObjectPtr`)은 유지한다.** 다만 이유가 바뀐다 — 이게 중요한 이유는 §1-4에 있다.

### 1-3. 질문 4개에 대한 답

**Q1. config 프로퍼티의 `TSoftObjectPtr`는 애셋 레지스트리 의존 간선을 만드는가 → ❌**

애셋 레지스트리는 **디스크의 패키지**를 인덱싱한다. 네이티브 CDO는 패키지에 없다(`/Script/EmploymentProj`는 디스크 에셋이 아니다). 그러니 간선이 생길 자리가 없다.

**결정적 증거 — 쿠커가 config 키를 다섯 개 손으로 적어 놓았다.**

```cpp
// CookOnTheFlyServer.cpp:8840-8867
auto AddDefaultObject = [&](FName PropertyName) {
    const FConfigValue* PairString = MapSettingsSection->Find(PropertyName);   // ← .ini 텍스트를 직접 읽는다
    ...
};
AddDefaultObject(FName(TEXT("GameDefaultMap")));
AddDefaultObject(FName(TEXT("ServerDefaultMap")));
AddDefaultObject(FName(TEXT("GlobalDefaultGameMode")));
AddDefaultObject(FName(TEXT("GlobalDefaultServerGameMode")));
AddDefaultObject(FName(TEXT("GameInstanceClass")));
```

**일반 메커니즘이 있었다면 이 다섯 줄이 존재할 이유가 없다.** 그리고 이것들조차 리다이렉터를 만나면 에러다:

> `CookOnTheFlyServer.cpp:8648-8652` — *"{0} contains a redirected reference '{1}'. **The intended asset will fail to load in a packaged build.** Select the intended asset again in Project Settings to fix this issue."*

→ **`ModifyCook` 논의는 달라지지 않는다.** 요청 §1-3의 "쿡 강제는 `AlwaysCook`만"이 그대로 유효하다.

**Q2. 리네임 시 에디터가 `.ini`를 다시 쓰는가 → ❌** (§1-1)

**Q3. `Fix Up Redirectors` 후 살아남는가 → ❌**

`:796`이 남긴 리다이렉터가 **유일한 생명줄**이다. `Fix Up Redirectors`는 참조하는 **패키지**를 고쳐 쓰고 리다이렉터를 지운다 — `.ini`는 패키지가 아니라서 고쳐지지 않은 채 리다이렉터만 사라진다. 정확히 요청에 적힌 대로다.

**Q4. 패키지 빌드 → ⚠️ 엔진 문구는 "빠질 수 있다"지만, 우리 경우는 쿡이 아니라 로드가 깨진다**

리다이렉터 패키지는 **아무도 하드 참조하지 않으면 쿡되지 않는다.** 그래서 일반론으로는 "쿡 누락 + 런타임 null" 둘 다 가능하다.

**그런데 `DT_Items`는 이미 쿡이 보장돼 있다.** 요청 §1-3이 스스로 적은 대로, 각 `UEPItemDefinition`의 `ItemDataRow`(`FDataTableRowHandle::DataTable`)가 **하드 참조**이고 `ItemDef`는 `CookRule=AlwaysCook`이다(`DefaultGame.ini:14`). 즉 `.ini` 경로가 깨져도:

- 쿡 누락 → **안 일어난다**
- 런타임 null → 일어난다. 그러면 `BuildDataCache`가 `UE_LOG(Error, "ItemDataTable이 설정되지 않았습니다.")`를 찍고(`EPItemDefinitionSubsystem.cpp:57` 직후), 아이템이 0개가 된다

**즉 실패 모드가 "조용한 침묵"이 아니라 "에러 로그 + 전부 없음"이다.** 리네임 다이얼로그 → PIE 에러 로그 → 아이템 0개, 이 세 신호를 다 무시해야 배포까지 간다.

### 1-4. ★ 이유를 고쳐야 하는 진짜 이유

지금 config 에셋 경로는 하나다. 위험은 낮다. **문제는 문서가 "안전하다"고 적혀 있는 한, 다음에 경로를 추가할 때 또 안전하다고 믿는다는 것이다.** §2의 모순을 사용자가 발견한 것 자체가 이 문서 문장이 이미 한 번 판단을 흐렸다는 증거다.

그리고 여기서 `FName` 대신 `TSoftObjectPtr`를 쓰는 **진짜 이득**이 나온다 — 리다이렉터가 아니다:

| | `FString`/`FName` 경로 | `TSoftObjectPtr` |
|---|---|---|
| Project Settings UI | 텍스트 입력 (오타 가능) | **에셋 피커** + `AllowedClasses` 필터 |
| 리네임 시 탐지 | ❌ `FSoftObjectPathRenameSerializer`가 못 본다 | ✅ `operator<<(FSoftObjectPath&)`(`AssetRenameManager.cpp:629`)가 잡는다 |
| 리다이렉터 강제 생성 | ❌ | ✅ (`:796`) |
| 경고 다이얼로그 | ❌ **침묵** | ✅ (`:463`) |

**`FName` 경로였다면 진짜로 조용히 깨진다.** 그 차이가 결정의 근거가 되어야 하고, 문서는 그걸 적어야 한다.

---

## 2. B가 약속한 이점 중 무엇이 남는가

> 요청 §3-B: *"끊길 수 있는 문자열이 프로젝트 전체에 하나로 줄어든다. DataAsset 안쪽은 전부 실제 에셋 참조라 쿡·리네임이 자동으로 따라온다 — §2가 '내 진단이 맞다'로 판정되면 이 이점이 성립한다."*

판정은 "Claude 쪽이 대체로 맞다"였으므로, 요청의 논리대로면 이점이 성립한다. **그런데 성립하는 크기가 예상과 다르다.**

| B가 준다는 것 | 실제 |
|---|---|
| 끊길 문자열이 **1개**로 준다 | 지금 3개 → 1개. **0개가 아니다.** 그 1개는 여전히 같은 방식으로 깨진다 |
| 안쪽 참조는 쿡이 자동 | ✅ 맞다. **그런데 `ItemDataTable`은 이미 DA 하드 참조로 쿡된다**(§1-3 Q4). `PlaceholderPickupMesh`는 임시(§6), `PickupClass`는 클래스라 문제 축이 다르다(§8) |
| 리네임이 자동으로 따라옴 | ✅ 맞다. **그런데 지금 그 위험에 노출된 에셋이 `DT_Items` 하나다** |

**즉 B는 3개를 1개로 줄이는데, 그 3개 중 실제로 지켜야 할 것이 1개다.** 1 → 1이다.

그리고 §3에서 보겠지만, **Lyra의 `DA_EPGameData` 대응물조차 `.ini` 경로를 1개로 줄이지 못했다.** Lyra는 20개 넘게 갖고 있다.

---

## 3. ★ §5 실무 조사 — Lyra 직독

### 3-1. `ULyraGameData`는 "전역 에셋 통"이 아니다

```cpp
// LyraGameData.h:19-43 — 전문(全文)이다. 필드가 3개뿐이다
UCLASS(MinimalAPI, BlueprintType, Const, Meta = (DisplayName = "Lyra Game Data", ...))
class ULyraGameData : public UPrimaryDataAsset
{
    // Gameplay effect used to apply damage.  Uses SetByCaller for the damage magnitude.
    UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects", ...)
    TSoftClassPtr<UGameplayEffect> DamageGameplayEffect_SetByCaller;

    UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects", ...)
    TSoftClassPtr<UGameplayEffect> HealGameplayEffect_SetByCaller;

    UPROPERTY(EditDefaultsOnly, Category = "Default Gameplay Effects")
    TSoftClassPtr<UGameplayEffect> DynamicTagGameplayEffect;
};
```

메시도, 아이콘도, 데이터테이블도, 액터 클래스도 없다. **GAS 전역 GE 3개를 담는 통이다.** 요청 §3-B가 그린 그림(`ItemDataTable` + `PlaceholderPickupMesh` + `PickupClass` + 사운드 + 아이콘)과 성격이 다르다.

### 3-2. Lyra는 config 에셋 경로를 하나로 줄이지 않았다 — 20개가 넘는다

`ULyraAssetManager`부터 **하나가 아니라 둘**이다.

```cpp
// LyraAssetManager.h:87-97
// Global game data asset to use.
UPROPERTY(Config)
TSoftObjectPtr<ULyraGameData> LyraGameDataPath;
...
// Pawn data used when spawning player pawns if there isn't one set on the player state.
UPROPERTY(Config)
TSoftObjectPtr<ULyraPawnData> DefaultPawnData;
```

그리고 `Lyra/Config/DefaultGame.ini`에 흩어진 config 에셋 경로:

| 줄 | 섹션 | 경로 |
|---|---|---|
| `:55-56` | `LyraAssetManager` | `LyraGameDataPath`, `DefaultPawnData` |
| `:77` | `LyraUIManagerSubsystem` | `DefaultUIPolicyClass` |
| `:80-81` | `LyraUIMessaging` | `ConfirmationDialogClass`, `ErrorDialogClass` |
| `:84` | `CommonLoadingScreenSettings` | `LoadingScreenWidget` |
| `:88` | `CommonInputSettings` | `InputData` |
| `:96-97` | `CommonUISettings` | `DefaultThrobberMaterial`, `DefaultRichTextDataClass` |
| `:228-237` | **`LyraAudioSettings`** | **9개 이상** (`DefaultControlBusMix`, `OverallVolumeControlBus`, 서브믹스 체인 …) |
| `:241` | `LyraReplicationGraphSettings` | `DefaultReplicationGraphClass` |

`DefaultEngine.ini`에도 `:66-69`(GameMode·GameInstance·맵), `:257-262`(사운드 클래스·서브믹스)가 더 있다.

### 3-3. ★ 결정적 — Epic이 `UDeveloperSettings`에 에셋 참조를 넣는다

```cpp
// LyraAudioSettings.h:14-25, 30-59  (발췌)
USTRUCT()
struct FLyraSubmixEffectChainMap
{
    UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/Engine.SoundSubmix"))
    TSoftObjectPtr<USoundSubmix> Submix = nullptr;

    UPROPERTY(EditAnywhere, meta = (AllowedClasses = "/Script/Engine.SoundEffectSubmixPreset"))
    TArray<TSoftObjectPtr<USoundEffectSubmixPreset>> SubmixEffectChain;
};

UCLASS(MinimalAPI, config = Game, defaultconfig, meta = (DisplayName = "LyraAudioSettings"))
class ULyraAudioSettings : public UDeveloperSettings          // ★
{
    UPROPERTY(config, EditAnywhere, Category = MixSettings, meta = (AllowedClasses = "/Script/AudioModulation.SoundControlBusMix"))
    FSoftObjectPath DefaultControlBusMix;
    ...   // 이런 필드가 9개 이상
};
```

**후보 A가 Lyra 관례다.** 그리고 `meta = (AllowedClasses = ...)`를 쓰는 것도 관례다 — 우리 `ItemDataTable`에는 없다(§9-3).

엔진 본체도 같다:

- `RendererSettings.h:1523-1539` — `FSoftObjectPath` 캘리브레이션 머티리얼 3개, `meta = (AllowedClasses = "/Script/Engine.Material")`
- `UserInterfaceSettings.h:133, 180` — `TMap<..., FSoftClassPath> SoftwareCursors`, `FSoftClassPath CustomScalingRuleClass`

### 3-4. 쿡 안전은 indirection이 아니라 **AssetManager 등록**에서 온다

`ULyraGameData`가 쿡되는 이유는 DataAsset이어서가 아니다. `.ini`에 이렇게 등록돼 있어서다.

```ini
; Lyra/Config/DefaultGame.ini:62
+PrimaryAssetTypesToScan=(PrimaryAssetType="LyraGameData",
    AssetBaseClass="/Script/LyraGame.LyraGameData", bHasBlueprintClasses=False, bIsEditorOnly=False,
    Directories=, SpecificAssets=("/Game/DefaultGameData.DefaultGameData"),
    Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))
```

**즉 경로가 `.ini`에 두 번 적혀 있다** — `:55`의 `LyraGameDataPath`와 `:62`의 `SpecificAssets`. 리네임하면 **두 곳**을 고쳐야 한다. B는 문자열을 줄이는 구조가 아니다.

### 3-5. ★ 더 결정적 — 쿡된 빌드에서 Lyra는 config 경로로 로드하지 않는다

```cpp
// LyraAssetManager.cpp:144-189
UPrimaryDataAsset* ULyraAssetManager::LoadGameDataOfClass(TSubclassOf<UPrimaryDataAsset> DataClass,
        const TSoftObjectPtr<UPrimaryDataAsset>& DataClassPath, FPrimaryAssetType PrimaryAssetType)
{
    if (!DataClassPath.IsNull())                      // :149  ← config 경로는 "게이트"로만 쓰인다
    {
        if (GIsEditor)
        {
            Asset = DataClassPath.LoadSynchronous();  // :163  ← 에디터에서만 경로로 로드
            LoadPrimaryAssetsWithType(PrimaryAssetType);
        }
        else
        {
            TSharedPtr<FStreamableHandle> Handle = LoadPrimaryAssetsWithType(PrimaryAssetType);  // :168
            Handle->WaitUntilComplete(0.0f, false);
            Asset = Cast<UPrimaryDataAsset>(Handle->GetLoadedAsset());                            // :174
        }
    }
    if (!Asset)
    {
        // It is not acceptable to fail to load any GameData asset. It will result in soft failures that are hard to diagnose.
        UE_LOG(LogLyra, Fatal, ...);                  // :186
    }
}
```

**패키지 빌드의 실제 로드 경로는 `PrimaryAssetType`이다.** config 경로는 에디터 편의 + null 게이트다. 즉 Lyra의 구조는 후보 B가 아니라 **후보 D를 `UPrimaryDataAsset`에 적용한 것**에 가깝고, 타입에 에셋이 정확히 하나여서(`SpecificAssets` 1개) `GetLoadedAsset()`이 성립한다.

그리고 `:185-186`의 주석은 우리 §1-4와 같은 이야기다 — *"soft failures that are hard to diagnose"*. Lyra의 대응은 구조가 아니라 **`Fatal`** 이다.

### 3-6. 커뮤니티 관례

검색 결과의 공통 진단은 §1과 일치한다 — *"soft references in ini files don't trigger cook — add to AssetManager primary asset rules"*, *"흔한 원인은 primary asset rule 누락, cook trace가 못 따라가는 소프트 참조, 미해결 리다이렉터"*. 즉 **처방이 "DataAsset으로 옮겨라"가 아니라 "AssetManager에 등록해라"** 다. Tom Looman의 `UDeveloperSettings` 글도 *"C++ 클래스에 액터/에셋을 연결하는 데 매우 편리하다"* 로 A를 전제한다.

> ⚠️ 이 §3-6만 웹 검색 기반이고 §3-1~3-5는 로컬 소스 직독이다. 인용 강도가 다르다는 걸 명시한다.

---

## 4. 4-1 — 사용자 전제 판정

> *"머지않은 미래에 바꿀 것이고 오래 걸리지 않는다면, 지금 하는 게 맞다."*

**원칙은 옳다.** 프로젝트 원칙 "나중에 넣기 비싼 것은 지금 넣는다"(CLAUDE.md §2)와 같은 말이다. 문제는 **이 사안이 그 조건에 안 맞는다**는 것이다.

### 4-1-a. 옮기는 작업량은 참조 개수에 비례하지 않는다

| | 비용 |
|---|---|
| `DA_EPGameData` 클래스 + `.ini` 1줄 + AssetManager 등록 + 접근자 | **고정.** 참조 개수와 무관 |
| 필드 하나 추가 | `UDeveloperSettings`든 DataAsset이든 **한 줄로 같다** |
| 소비자 수정 | `GetDefault<Settings>()->X` → `GameData.X`. **호출 지점당 한 줄** |

지금 소비자는 2곳이다(`EPItemDefinitionSubsystem.cpp:57`, `EPItemSpawner.cpp:32`). Step 03·04에서 늘어난다 해도 **그때 새로 쓰는 코드는 처음부터 새 경로를 쓴다.** 미루는 비용은 "그 사이에 옛 경로로 쓰인 코드"뿐인데, **Step 02는 상호작용이라 전역 에셋 참조를 안 쓴다.** 사이에 낀 것이 거의 없다.

### 4-1-b. Step 03·04에서 전역 에셋 참조가 늘어난다는 예상 — 대체로 틀렸다

요청이 예로 든 셋을 그대로 검토하면:

| 예상 | 실제 갈 자리 |
|---|---|
| 획득/버리기 사운드 | **`UEPItemDefinition`.** 총 줍는 소리와 붕대 줍는 소리가 같으면 안 된다. 아이템별이어야 자연스럽다 |
| 기본 아이콘 | `UEPItemDefinition::Icon`이 **이미 있다**(`EPItemDefinition.h`). 전역으로 남을 것은 "아이콘 없는 아이템의 폴백" 하나 — 즉 `PlaceholderPickupMesh`와 **같은 부류의 임시 필드**(§6) |
| 슬롯 머티리얼 | 위젯 BP 안. C++이 참조할 이유가 없다 |

**전역 후보로 남는 것은 "폴백" 뿐이고, 폴백은 §6에서 보듯 `UDeveloperSettings`의 자리다.**

### 4-1-c. 그래서 판정

전제 자체는 유효하다. 다만 **"곧 바꿀 것"이 아니다.** 바꿀 근거가 §2 판정으로 약해졌고(§2 표), 바꾸는 비용은 미룬다고 늘지 않고, 늘어난다던 참조는 대체로 다른 곳으로 간다.

> 다만 4-1의 문제의식은 **한 항목에서는 정확히 맞다** — §8의 `TSubclassOf`다. 그건 지금 코드에 들어가는 중이고, 나중에 바꾸면 `.ini` 값·헤더·소비자 세 곳을 동시에 고쳐야 한다.

---

## 5. 4-2 — A~E 중 무엇인가

### **A 유지.** 근거 넷:

1. **Lyra가 A를 한다.** `ULyraAudioSettings : UDeveloperSettings`가 `FSoftObjectPath` 9개+를 든다(§3-3). 엔진도 같다
2. **B의 이점이 실질적으로 1 → 1이다**(§2)
3. **`DA_EPGameData`는 어느 문서에도 없다.** 요청 §4-2가 스스로 인정했다. CLAUDE.md §2의 판단 기준 — *"이 확장점이 `DOCS/` 어딘가에 이름으로 적혀 있는가"* — 에 **없다**. 그리고 §4에서 봤듯 그 문서를 먼저 고칠 근거도 안 나왔다
4. **포트폴리오 규모에서 계층 하나가 더 는다.** 지금 `DT_Items`를 읽으려면 `Settings → LoadSynchronous → Table` 3단계다. B는 `Settings(.ini) → GameData → Table` 로 **에셋 로드가 하나 더 낀다.** 서브시스템 `Initialize()`가 그만큼 늦어지고, "GameData가 아직 안 왔다" 분기가 새로 생긴다

### C(하이브리드)를 안 쓰는 이유

C의 논리 — *"성격이 다른 값을 섞지 않는다"* — 는 옳지만, **이미 지켜지고 있다.** `UEPCombatDeveloperSettings`(스칼라·디버그)와 `UEPLootDeveloperSettings`(데이터·디버그)가 **도메인으로** 갈려 있다. C는 여기에 "에셋이냐 스칼라냐"라는 **두 번째 분류축**을 더한다 — 그러면 `bEnableLootDebugLog`(Loot·스칼라)와 `PlaceholderPickupMesh`(Loot·에셋)가 갈리는데, **둘 다 "Loot 설정 페이지에서 같이 보고 싶은 것"이다.** 축이 하나 늘어난 값을 얻지 못한다.

### D를 지금 하지 않는 이유 — 그리고 D의 "0개"는 사실이 아니다

D의 매력은 *"`.ini` 경로가 0개가 된다"* 인데, **`SpecificAssets`도 `.ini` 문자열이다.** Lyra가 정확히 그 상태다(§3-4 — 경로가 두 곳에 있다). 진짜 0개가 되려면 `Directories=((Path="/Game/Data"))` 기반이어야 하고, 그러면 요청 §3-D가 스스로 우려한 *"DT가 여러 개면 구분 불가"* 가 그대로 실현된다.

**게다가 D가 지켜줄 것이 이미 지켜져 있다** — `DT_Items`는 DA 하드 참조로 쿡된다(§1-3 Q4). D는 쿡 문제가 아니라 리네임 문제만 남기는데, 그건 `Directories` 방식이어야 해결되고 그 방식은 취약하다.

> **다만 D는 "기각"이 아니라 "지금 아님"이다.** 만약 `DT_Items` 외에 두 번째 전역 DT(예: 등급표·가격표)가 생기면, 그때는 D가 A보다 낫다 — 등록 규칙 한 줄로 둘 다 해결된다. 이건 문서에 이름이 있는 확장점이 아니므로 **지금 만들지 않는다.**

### E — 우리가 못 떠올린 것

없다. §1~§3의 조사 범위에서 5번째 구조는 나오지 않았다. 실무는 A(설정에 소프트 참조) / D(AssetManager 등록) / 하드 참조(DA→DT) 세 가지의 조합이고, **우리는 이미 셋 다 쓰고 있다.**

---

## 6. 4-3 — `PlaceholderPickupMesh`는 어디로 가는가

**옮길 대상이 아니다. `UDeveloperSettings`가 이 필드의 정확한 자리다.**

이 필드의 성격은 "게임 데이터"가 아니라 **"데이터가 비었을 때의 개발용 폴백"** 이다. 실제 메시가 들어오면 사라지는 게 아니라 **폴백으로 강등**된다고 요청이 적었는데, 폴백은 영구적으로 필요하다 — 새 아이템을 추가하고 `WorldMesh`를 아직 안 채운 상태에서 크래시하지 않게 하는 장치다.

**성격이 `bEnableSpawnerDebugDraw`와 같은 부류다.** 프로덕션 데이터가 아니라 **개발 편의**다. 이걸 `DA_EPGameData`에 넣으면 "게임 데이터"와 "개발 폴백"이 한 에셋에 섞인다 — 오히려 C가 반대하던 그 문제가 생긴다.

> 부수 확인: `01_Spawner.md:730-731`의 `Settings->PlaceholderPickupMesh.LoadSynchronous()`는 그대로 유효하다. 폴백 경로는 동기 로드가 맞다 — 이미 `WorldMesh` 비동기 로드가 실패했거나 없는 상황이고, 여기서 또 기다릴 이유가 없다.

---

## 7. 4-4 — `PickupClass`는 정말 전역인가

**전역 하나 유지가 §7까지 버틴다.** 다만 기각 근거를 §7 기준으로 다시 확인하면 한 항목이 갈린다.

### 7-1. 컨테이너(§7-1) — 반례가 아니다

컨테이너는 **레벨에 직접 배치하는 액터**고 *"스포너와 달리 오브젝트 자체가 월드에 남는다"* 고 문서가 못박았다(`05_Loot_DOCS.md` §7-1 표). `AEPPickup`이 아니고 `PickupClass`로 스폰되지도 않는다. **다른 클래스**다.

### 7-2. 부착물(§7-3) — 판단이 정확히 유지된다

문서가 이미 **평면 배열 + `ParentEntryId`** 로 확정했다(`05_Loot_DOCS.md` §7-3). 부착물 4개 달린 AK를 버리면:

```
AEPPickup 하나,  Payload = [ {Id=7,Parent=NONE,AK74}, {12,7,Optic}, {13,7,Muzzle}, {14,7,Mag,Charges=30} ]
```

- 메시는 `WorldMesh`(AK) 하나 — 배그도 바닥 아이템에 부착물을 안 그린다
- 그리려면 `Payload`를 훑어 SM 컴포넌트를 붙이면 되고, 그건 **같은 클래스 안의 로직**이다
- **기각 근거 *"메시는 `WorldMesh`, 등급 이펙트는 `Rarity`, 배낭 내용물은 `Payload`로 전부 데이터가 해결한다"* 가 그대로 성립한다**

### 7-3. ★ 자판기 상자(§7-2) — 여기서 두 번째 축이 생긴다

`05_Loot_DOCS.md:641`:

> 상자는 §7-1 컨테이너의 특수 케이스로 구현한다 — 검색 시간 0, 1회용, 파괴됨.

즉 자판기가 런타임에 스폰하는 것은 **픽업이 아니라 컨테이너**다. 그러면 "스폰할 액터 클래스"라는 물음이 두 번째로 생긴다.

**하지만 이 축은 아이템별이 아니라 *스폰 지점별*이다.** 자판기는 자기 필드로 상자 클래스를 갖고(등급별 상자라면 등급→클래스 맵), 스포너·버리기는 `PickupClass`를 쓴다. **축이 다르므로 지금 결정이 무효화되지 않는다.** 지금 만들 것도 없다 — 자판기 클래스 자체가 없다.

**다만 문서에 한 줄이 필요하다.** 안 적으면 §7-2를 구현할 때 *"픽업 클래스는 전역 하나로 정했잖아"* 와 충돌하고, 그 시점엔 왜 그렇게 정했는지 아무도 기억 못 한다. → §10-3.

### 7-4. 판정

| | 유지 |
|---|---|
| 채택: `UEPLootDeveloperSettings::PickupClass` (프로젝트 전체 하나) | ✅ |
| 기각: `UEPItemDefinition::PickupClass` (아이템별) | ✅ 기각 유지. §7-3이 오히려 근거를 강화한다 |
| 기각: `AEPItemSpawner` UPROPERTY (스포너별) | ✅ 기각 유지 (`01_Spawner.md:479`의 근거 — 버리기 경로에 스포너가 없다 — 가 그대로 맞다) |

---

## 8. ★ 4-5 — `TSubclassOf`는 틀렸다. 이번 요청의 실제 발견

### 8-1. 질문에 대한 직답: **모듈 로드 시점이 아니라 CDO 생성 시점에 동기 로드된다**

요청의 우려 — *"하드 클래스 참조라 모듈 로드 시점에 전부 끌려오는 것이면"* — 는 방향이 맞고, 시점은 그보다 더 이르다.

```cpp
// UObjectGlobals.cpp:4379-4382  (FObjectInitializer::PostConstructInit)
if (bIsCDO || Class->HasAnyClassFlags(CLASS_PerObjectConfig))
{
    Obj->LoadConfig(NULL, NULL, bIsCDO ? UE::LCPF_ReadParentSections : UE::LCPF_None);
}
```

**CDO가 만들어지는 순간 `LoadConfig()`가 돈다.** `UDeveloperSettings`의 CDO는 모듈 로드 중 클래스 등록과 함께 생성된다.

그 안에서 `TSubclassOf`(= `FClassProperty`)는 이 경로를 탄다:

```cpp
// PropertyBaseObject.cpp:389/394  ParseObjectPropertyValue → FindImportedObject
// PropertyBaseObject.cpp:566-596
if (!Result && Dot && !GIsSavingPackage)
{
    ...
    const uint32 LoadFlags = LOAD_NoWarn | LOAD_FindIfFail;                    // :593
    UE_LOG(LogProperty, Verbose, TEXT("FindImportedObject is attempting to import [%s] ... with StaticLoadObject"), ...);
    Result = StaticLoadObject(ObjectClass, nullptr, Text, nullptr, LoadFlags, nullptr, true);   // :596  ★ 동기 로드
}
```

**`.ini`에 `PickupClass=/Game/Blueprints/BP_Pickup.BP_Pickup_C`를 적는 순간, 엔진 초기화 중에 그 BP가 `StaticLoadObject`로 로드된다.** BP를 로드하면 BP의 CDO가 만들어지고, CDO가 하드 참조하는 메시·머티리얼·나이아가라가 전부 딸려온다.

### 8-2. 더 나쁜 것 — 실패가 조용하다

`:593`의 `LOAD_NoWarn | LOAD_FindIfFail`을 보라.

- 실패해도 **경고가 안 뜬다**
- 실패하면 `ParseObjectPropertyValue`가 `:409-416`에서 `return false` → 값은 **null**
- `bWarnOnnullptr`는 `PPF_CheckReferences`가 있을 때만 켜지는데(`:371`), config 로드는 그 플래그를 안 준다

즉 **"엔진 초기화가 너무 일러서 BP를 못 읽었다"와 "경로 오타"와 "`.ini`에 아예 안 적었다"가 전부 같은 증상 — 조용한 null — 으로 수렴한다.** 그리고 그 증상은 게임 시작 후 첫 `SpawnLoot()`에서 *"PickupClass 미설정"* 으로 나타난다. 원인과 증상 사이가 멀다.

이건 5차 §2에서 확정한 판단 기준과 같은 부류다 — **양쪽 다 컴파일되고, 틀린 쪽이 조용하다.**

### 8-3. 채택 근거를 다시 본다

> `01_Spawner.md:797`의 근거: *"스폰 순간 무조건 필요하므로 지연 로드 이득이 0이고, `LoadSynchronous()`와 null 폴백 분기가 사라진다."*

- **"지연 로드 이득이 0"** → 맞다. 스폰할 때 필요하다
- **"`LoadSynchronous()`가 사라진다"** → **틀렸다.** 사라지는 게 아니라 **엔진 초기화 중으로 옮겨 갈 뿐**이다(`:596`). 우리가 부르지 않을 뿐 여전히 동기 로드다. 그것도 **더 이르고 더 위험한 시점에**
- **"null 폴백 분기가 사라진다"** → **틀렸다.** `LOAD_FindIfFail` 실패로 null이 될 수 있으므로 분기는 여전히 필요하다. 없애면 `SpawnActor(nullptr)`이다

### 8-4. Lyra는 같은 자리에서 무엇을 하는가

```cpp
// LyraUIMessaging.h:32-43   ★ 정확히 우리 상황이다
private:
    UPROPERTY()
    TSubclassOf<UCommonGameDialog> ConfirmationDialogClassPtr;   // ← 런타임 해석본. config 아님
    UPROPERTY()
    TSubclassOf<UCommonGameDialog> ErrorDialogClassPtr;

    UPROPERTY(config)
    TSoftClassPtr<UCommonGameDialog> ConfirmationDialogClass;    // ← config에 노출되는 쪽은 소프트
    UPROPERTY(config)
    TSoftClassPtr<UCommonGameDialog> ErrorDialogClass;
```

**config에 노출되는 것은 소프트, 런타임에 쓰는 것은 `TSubclassOf`, 둘을 분리한다.** 같은 패턴이:

- `LyraReplicationGraphSettings.h:26-27` — `UPROPERTY(config) FSoftClassPath DefaultReplicationGraphClass`
- `Lyra/DefaultGame.ini:77, 84` — `DefaultUIPolicyClass`, `LoadingScreenWidget` 전부 `_C` 소프트 경로

엔진 전체를 세어 보면 `UPROPERTY(config)` 바로 다음 줄이 `TSubclassOf`인 경우는 **11건**, `TSoftClassPtr`/`FSoftClassPath`인 경우는 **24건**이다. 그리고 11건 중 `Runtime/Engine/Classes`에서 확인되는 것은 `WorldSettings.h:933-934`의 `DefaultBookmarkClass` 하나인데, **그건 에디터 전용 네이티브 클래스**다 — BP가 아니라서 로드가 없다.

**결론: config + `TSubclassOf`는 "네이티브 클래스일 때만" 쓰는 형태다. BP를 가리킬 자리가 아니다.**

### 8-5. 권고 — `EPItemSpawner.cpp`를 쓰는 중이므로 지금 바꾼다

```cpp
// EPLootDeveloperSettings.h
UPROPERTY(Config, EditAnywhere, Category = "Loot",
          meta = (MetaClass = "/Script/EmploymentProj.EPPickup"))
TSoftClassPtr<AEPPickup> PickupClass;
```

소비 지점(`01_Spawner.md:389` 부근):

```cpp
const UEPLootDeveloperSettings* Settings = GetDefault<UEPLootDeveloperSettings>();

// 첫 스폰에서 한 번만 동기 로드된다. 이후에는 이미 메모리에 있어 즉시 반환.
UClass* PickupCls = Settings->PickupClass.IsNull()
    ? AEPPickup::StaticClass()                      // .ini 미설정 = 정상. C++ 기본으로 간다
    : Settings->PickupClass.LoadSynchronous();

if (!PickupCls)                                     // 설정은 됐는데 로드 실패 = 진짜 오류
{
    UE_LOG(LogTemp, Error, TEXT("[Loot] PickupClass 로드 실패: %s"), *Settings->PickupClass.ToString());
    return;
}
```

**이 형태가 §8-2의 세 증상을 갈라 준다** — 5차에서 `RollLootTable`에 세운 "반환 규약 두 겹"과 같은 구조다:

| 상태 | 의미 | 처리 |
|---|---|---|
| `IsNull()` | `.ini` 미설정 — **정상** | `AEPPickup::StaticClass()` |
| `!IsNull()` + 로드 실패 | 경로 오타/에셋 삭제 — **오류** | Error 로그 |
| 로드 성공 | 정상 | 사용 |

> ⚠️ 기존 `TSubclassOf` 안(`01_Spawner.md:791`)의 `= AEPPickup::StaticClass()` 기본값은 **`TSoftClassPtr`로 바꾸면 없어진다.** 소프트 포인터의 기본값은 "빈 경로"고, C++ 기본값을 헤더에 적어도 `.ini`가 이기지 못한다(빈 값도 값이다). 그래서 **폴백을 소비 지점으로 옮긴다.** 위 코드가 그렇게 돼 있다.

**비용:** `.ini` 값 형식은 그대로(`/Game/.../BP_X.BP_X_C`), 헤더 한 줄, 소비 지점 네 줄. **지금이 가장 싸다** — `EPItemSpawner.cpp`가 작성 중이고 소비 지점이 하나뿐이다. Step 03의 버리기가 두 번째 소비자가 되면 두 곳이 된다.

---

## 9. 지금 조치 — 우선순위 순

| # | 항목 | 근거 | 비용 |
|---|---|---|---|
| **1** | `PickupClass`를 `TSoftClassPtr<AEPPickup>`으로. 폴백을 소비 지점으로 | §8 | 헤더 1줄 + 소비 4줄. **작성 중이라 지금이 최저** |
| **2** | `05_Loot_DOCS.md:823` / `00_ItemCore.md:610`의 근거 문장 교체 | §1-2, §1-4 | 문서만 |
| **3** | `ItemDataTable`에 `meta = (AllowedClasses = "/Script/Engine.DataTable")` 추가 | §3-3 (Lyra·엔진 공통 관례). Project Settings 피커에 DT만 뜬다 | 1줄 |
| **4** | `05_Loot_DOCS.md` §7-2에 "상자는 `PickupClass`가 아니다" 한 줄 | §7-3 | 문서만 |
| **5** | `01_Spawner.md:797`의 `TSubclassOf` 근거 3줄 교체 | §8-3 (근거 두 개가 사실이 아니다) | 문서만 |
| — | `DA_EPGameData` 도입 | **하지 않는다** (§5) | — |
| — | `ItemDataTable`을 AssetManager로 (D) | **지금 아님.** 두 번째 전역 DT가 생기면 재검토 (§5) | — |
| — | `PlaceholderPickupMesh` 이동 | **하지 않는다** (§6) | — |

---

## 10. 문서 수정 문안

> CLAUDE.md의 워크플로에 따라, 아래는 **제안 문안**이다. 적용 여부는 사용자가 결정한다.

### 10-1. `05_Loot_DOCS.md:823` / `05_Loot_00_ItemCore.md:610` 대체

```markdown
> **`FName`/`FString` 경로가 아니라 `TSoftObjectPtr<UDataTable>`을 쓴다.** 다만 이유는
> "리다이렉터가 알아서 따라가서"가 아니다 — **`.ini`에 적힌 경로는 에셋 리네임 시
> 자동으로 고쳐지지 않는다.** 엔진이 리네임 다이얼로그에 그렇게 적어 놓았다:
> *"Source code, config INI, and text files may need Find/Replace ... Otherwise assets
> can be missing from cooked builds"* (`AssetRenameManager.cpp:463`).
>
> `TSoftObjectPtr`를 쓰는 진짜 이득은 **깨질 때 시끄럽다**는 것이다.
>
> | | `FName`/`FString` | `TSoftObjectPtr` |
> |---|---|---|
> | Project Settings UI | 텍스트 입력 (오타 가능) | 에셋 피커 + `AllowedClasses` |
> | 리네임 탐지 | ❌ **침묵** | ✅ `FindCDOReferences`(`:708`)가 CDO를 직렬화해 잡는다 |
> | 리다이렉터 강제 생성 | ❌ | ✅ (`:796`) |
> | 경고 다이얼로그 | ❌ | ✅ (`:463`) |
>
> **그러므로 `.ini` 경로를 가진 에셋을 리네임할 때 뜨는 다이얼로그를 무시하지 않는다.**
> 무시하면 리다이렉터가 유일한 생명줄이 되고, `Fix Up Redirectors`가 그것마저 지운다.
>
> 참고: `DT_Items` 자체는 각 `UEPItemDefinition`의 `ItemDataRow`
> (`FDataTableRowHandle::DataTable` = 하드 참조)로 이미 쿡이 보장된다. `.ini` 경로가
> 깨져도 쿡 누락은 안 나고 런타임 null만 난다 — `BuildDataCache`의 Error 로그가 잡는다.
```

### 10-2. `01_Spawner.md:797` (「`PickupClass`를 설정에 두는 이유」) 아래 근거 교체

```markdown
**타입은 `TSoftClassPtr<AEPPickup>`이다. `TSubclassOf`가 아니다.**

`UPROPERTY(Config) TSubclassOf<X>`에 `.ini`로 BP 경로를 주면, **CDO 생성 시점**
(`UObjectGlobals.cpp:4379-4382`의 `LoadConfig`)에 `StaticLoadObject`
(`PropertyBaseObject.cpp:596`)가 돈다. 즉 지연 로드를 없앤 게 아니라
**엔진 초기화 중으로 옮긴 것**이다. 게다가 로드 플래그가
`LOAD_NoWarn | LOAD_FindIfFail`(`:593`)이라 **실패해도 조용히 null**이 되어,
"초기화가 너무 이름" / "경로 오타" / "미설정"이 전부 같은 증상으로 수렴한다.

엔진·Lyra 관례도 소프트다 — `LyraUIMessaging.h:39-43`은 config에 `TSoftClassPtr`,
런타임에 `TSubclassOf`를 **쌍으로** 둔다. 엔진에서 `UPROPERTY(config) + TSubclassOf`는
`WorldSettings::DefaultBookmarkClass`처럼 **네이티브 클래스**일 때만 쓴다.

소비 지점에서 세 상태를 가른다 (`RollLootTable`의 반환 규약 두 겹과 같은 형태):

| 상태 | 의미 | 처리 |
|---|---|---|
| `IsNull()` | `.ini` 미설정 — 정상 | `AEPPickup::StaticClass()` |
| `!IsNull()` + 로드 실패 | 경로 오타·에셋 삭제 — 오류 | Error 로그 후 중단 |
| 로드 성공 | 정상 | 사용 |
```

### 10-3. `05_Loot_DOCS.md` §7-2 (`:641` 다음)

```markdown
> **상자는 `PickupClass`로 스폰되지 않는다.** 상자는 컨테이너이므로 자판기가 자기
> 필드(등급→상자 클래스)로 들고 있다. `UEPLootDeveloperSettings::PickupClass`가
> "프로젝트 전체 하나"인 것과 충돌하지 않는다 — 축이 다르다. `PickupClass`는
> **바닥에 떨어지는 아이템 픽업**의 클래스이고, 상자 클래스는 **스폰 지점(자판기)** 의
> 속성이다.
```

### 10-4. `EPLootDeveloperSettings.h` (§9-1, §9-3 반영형)

```cpp
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "EP Loot"))
class EMPLOYMENTPROJ_API UEPLootDeveloperSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category = "Data",
              meta = (AllowedClasses = "/Script/Engine.DataTable"))
    TSoftObjectPtr<UDataTable> ItemDataTable;

    /** WorldMesh가 없는 아이템의 폴백 메시. 개발용이며 프로덕션에서도 남는다 */
    UPROPERTY(Config, EditAnywhere, Category = "Loot",
              meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
    TSoftObjectPtr<UStaticMesh> PlaceholderPickupMesh;

    /** 비우면 AEPPickup을 그대로 쓴다. BP로 확장할 때만 지정 */
    UPROPERTY(Config, EditAnywhere, Category = "Loot",
              meta = (MetaClass = "/Script/EmploymentProj.EPPickup"))
    TSoftClassPtr<AEPPickup> PickupClass;

    UPROPERTY(Config, EditAnywhere, Category = "Debug")
    bool bEnableLootDebugLog = false;

    UPROPERTY(Config, EditAnywhere, Category = "Debug")
    bool bEnableSpawnerDebugDraw = false;
};
```

---

## 11. 참고 — 인용 출처

**엔진** (`C:\Program Files\Epic Games\UE_5.7\Engine\Source`)

| 파일:줄 | 내용 |
|---|---|
| `Developer/AssetTools/Private/AssetRenameManager.cpp:437-468` | CDO 참조 경고 흐름 |
| `:463` | **"config INI ... may need Find/Replace ... assets can be missing from cooked builds"** |
| `:629, :708-806` | `FSoftObjectPathRenameSerializer`, `FindCDOReferences` |
| `:796` | `bCreateRedirector |= bSetRedirectorFlags` |
| `Editor/UnrealEd/Private/CookOnTheFlyServer.cpp:8840-8867` | config 키 5개 하드코딩 (`AddDefaultObject`) |
| `:8646-8654` | 리다이렉터 감지 시 쿡 에러 |
| `Runtime/CoreUObject/Private/UObject/UObjectGlobals.cpp:4379-4382` | CDO 생성 시 `LoadConfig` |
| `Runtime/CoreUObject/Private/UObject/PropertyBaseObject.cpp:566-596` | `StaticLoadObject`, `LOAD_NoWarn \| LOAD_FindIfFail` |
| `:409-416` | 로드 실패 시 조용한 null |
| `Runtime/Engine/Classes/Engine/RendererSettings.h:1523-1539` | 엔진 `UDeveloperSettings`의 `FSoftObjectPath` |
| `Runtime/Engine/Classes/Engine/UserInterfaceSettings.h:133, 180` | 엔진 `FSoftClassPath` |
| `Runtime/Engine/Classes/GameFramework/WorldSettings.h:933-934` | config `TSubclassOf` — 네이티브 클래스 |

**Lyra** (`C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame`)

| 파일:줄 | 내용 |
|---|---|
| `Source/LyraGame/System/LyraAssetManager.h:87-97` | config 소프트 경로 **2개** |
| `Source/LyraGame/System/LyraGameData.h:19-43` | 필드 **3개**, 전부 GE |
| `Source/LyraGame/System/LyraAssetManager.cpp:144-189` | 쿡 빌드는 `PrimaryAssetType`으로 로드 |
| `Source/LyraGame/Audio/LyraAudioSettings.h:14-59` | **`UDeveloperSettings`에 에셋 참조 9개+** |
| `Source/LyraGame/UI/Subsystem/LyraUIMessaging.h:32-43` | **`TSoftClassPtr`(config) + `TSubclassOf`(런타임) 쌍** |
| `Source/LyraGame/System/LyraReplicationGraphSettings.h:14, 26-27` | `UDeveloperSettingsBackedByCVars` + `FSoftClassPath` |
| `Config/DefaultGame.ini:54-56, 62, 77, 80-97, 228-241` | config 에셋 경로 20개+, `LyraGameData` `AlwaysCook` 등록 |

**프로젝트**

| 파일:줄 | 내용 |
|---|---|
| `Public/Data/EPLootDeveloperSettings.h:18-19` | 현재 상태 |
| `Config/DefaultGame.ini:14-15, 34-35` | `ItemDef`/`LootTable` 등록, `ItemDataTable` 경로 |
| `Private/Data/EPItemDefinitionSubsystem.cpp:57` | 소비자 1 + Error 로그 |
| `05_Loot_DOCS.md:140, 823` / `00_ItemCore.md:610` | §2 모순 지점 |
| `05_Loot_DOCS.md:635-641` | §7-2 상자 = 컨테이너 특수 케이스 |
| `05_Loot_01_Spawner.md:389, 418, 479, 730-731, 786-797` | `PickupClass`/`PlaceholderPickupMesh` |

**웹** (§3-6에만 사용)

- [Fix: Unreal Data Asset Config Not Included in Cooked Build — Bugnet](https://bugnet.io/blog/fix-unreal-dataasset-config-not-included-cooked-build)
- [Adding 'Project Settings' to Unreal Engine (DeveloperSettings) — Tom Looman](https://tomlooman.com/unreal-engine-developer-settings/)
- [Asset Management in Unreal Engine — Epic](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine)
