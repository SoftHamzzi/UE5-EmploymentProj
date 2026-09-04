# Content 폴더 구조 정리 — Lyra 기준

> 작성일: 2026-08-02
> 근거: 프로젝트 `Content/` 실측 + Lyra 직독(`C:\Users\wnsgn\문서\Unreal Projects\LyraStarterGame\Content`) + UE 5.7 플러그인 소스
> 관련: `DOCS/Mine/Concepts/SourceNavigation.md` (Source 쪽 탐색)

---

## 0. 결론 먼저

**직접 만든 에셋은 79개다. 전체 2,159개의 3.7%.** 나머지는 전부 임포트했거나 Lyra에서 가져온 것이다.

**그 79개가 구조 문제의 100%다.** 그리고 79개를 옮기는 비용은 지금이 가장 싸다 — Step 03(인벤토리)·04(UI)가 들어오면 UI 위젯만 20개가 넘는다.

**그리고 이미 Lyra 구조의 절반이 프로젝트 안에 들어와 있다.** `Audio/`, `Effects/`, `Characters/Heroes/`는 Lyra에서 통째로 가져온 폴더다(§2에서 확인). 문제는 **내가 만든 것이 그 옆에 `Blueprints/`라는 타입 기준 트리로 따로 자란 것**이다. 새 구조를 발명할 필요가 없고, 이미 있는 절반에 나머지를 맞추면 된다.

---

## 1. 현재 상태 — 실측

```
Content/                     에셋 수    출처
├─ Characters/                1,293
│   ├─ Heroes/                  752    ← Lyra 유래 (마네퀸)
│   ├─ MetaHuman/               528    ← 리타깃 애니메이션 (프로젝트)
│   └─ InputActions/             12    ← 프로젝트
├─ MetaHumans/                  287    ← MetaHuman 플러그인 익스포트
├─ FPS_Weapon_Bundle/           220    ← 마켓플레이스
├─ MSPresets/                    95    ← Quixel Bridge
├─ AnimStarterPack/              92    ← 마켓플레이스
├─ Audio/                        61    ← Lyra 유래
├─ Blueprints/                   45    ← 프로젝트 ★문제의 폴더
├─ Effects/                      39    ← Lyra 유래 + 프로젝트 혼재
├─ Data/                         19    ← 프로젝트
├─ Megascans/                     5    ← Quixel Bridge
├─ Maps/                          1    ← 프로젝트
├─ Materials/                     1    ← 프로젝트 (M_Green 하나)
├─ PhysicsMaterials/              1    ← 프로젝트 (PM_Character 하나)
├─ Collections/                   0    ← 엔진 관례
└─ Developers/                    0    ← 엔진 관례
```

**프로젝트가 직접 만든 것:** `Blueprints/` 45 + `Data/` 19 + `InputActions/` 12 + `Maps/` 1 + `Materials/` 1 + `PhysicsMaterials/` 1 = **79개**

---

## 2. 진단

### 2-1. Lyra 구조가 이미 절반 들어와 있다

다음 경로가 Lyra에 **그대로 존재한다**(직접 확인).

| 우리 경로 | Lyra에 있는가 |
|---|---|
| `Audio/Modulation/ControlBuses` | ✅ |
| `Audio/Submixes` | ✅ |
| `Effects/MaterialFunctions/MF_MannequinEdge.uasset` | ✅ |
| `Effects/Materials/Decals` | ✅ |
| `Characters/Heroes/Mannequin/Animations/Locomotion/Rifle` | ✅ |

`Effects/MaterialFunctions/`의 `MF_Mannequin*` 9개, `Audio/`의 61개, `Characters/Heroes/`의 752개가 전부 Lyra에서 온 것이다. **즉 이 프로젝트의 폴더 규칙은 "Lyra 것 + 내가 만든 것"이 서로 다른 규칙으로 병존하는 상태다.**

### 2-2. ★ `Blueprints/`는 **기능이 아니라 타입** 기준이다

```
Blueprints/
├─ GA/            어빌리티 6개
├─ GE/            이펙트 15개
├─ HUD/           위젯·머티리얼 11개
├─ VFX/           나이아가라 3개
├─ Loot/          스포너 1개
├─ BP_EPCharacter, BP_MH_Char1, BP_EPCorpse
├─ BP_EPGameMode, BP_EPPlayerController
├─ BP_WeaponAK74, BP_Projectile_Fast, BP_Projectile_Slow
└─ WBP_Crosshair        ← HUD/ 옆이 아니라 루트에 혼자
```

**Lyra에는 `Blueprints`라는 폴더 자체가 없다.** 최상위가 전부 기능이다.

```
Audio  Characters  ContextEffects  Editor  Effects  Environments  Feedback
GameplayCueNotifies  GameplayEffects  Input  Legal  Localization
PhysicsMaterials  System  Tools  UI  Weapons  _Export
```

타입 기준의 실제 비용은 **"BP인가 아닌가"로 한 기능이 갈린다**는 것이다.

| 기능 | 흩어진 곳 |
|---|---|
| 무기 AK74 | `Blueprints/BP_WeaponAK74` + `Data/Weapons/DA_AK74_*` + `FPS_Weapon_Bundle/Weapons/Meshes/Ka47` |
| 캐릭터 | `Blueprints/BP_EPCharacter` + `Blueprints/BP_MH_Char1` + `Characters/Heroes` + `Characters/MetaHuman` |
| HUD | `Blueprints/HUD/WBP_*` 11개 + `Blueprints/WBP_Crosshair` 1개 |
| 물리 재질 | `PhysicsMaterials/PM_Character` + `Data/PM_{Head,Chest,Limbs,WeakSpot}` + `Effects/Materials/Decals/PhM_{Body,Concrete,Glass}` — **세 곳** |

물리 재질이 셋으로 갈린 게 가장 나쁘다. `PM_` / `PhM_` 접두사도 서로 다르고, Lyra는 `PhysMat_`을 쓴다(`Characters/Heroes/PhysMat_Player.uasset`). **셋 다 같은 것을 가리키는데 이름도 위치도 다르다.**

### 2-3. 임포트 에셋이 최상위에 5개 흩어져 있다

`AnimStarterPack`, `FPS_Weapon_Bundle`, `MSPresets`, `Megascans`, `MetaHumans` — 최상위 16개 폴더 중 5개, 에셋 수로는 699개다. **콘텐츠 브라우저 최상위에서 "내 것"과 "남의 것"이 알파벳순으로 섞인다.**

> Lyra에는 임포트 폴더 관례가 없다 — 마켓플레이스 에셋을 안 쓰기 때문이다. `_Export/`는 반대 방향(내보내기)이다. **이 항목은 Lyra를 베낄 수 없고, 우리가 정해야 한다.**

### 2-4. 입력이 `Characters/` 아래에 묻혀 있다

Lyra는 `Input/`이 **최상위**이고 `Actions/`, `Mappings/`, `Settings/`로 나뉜다. 입력은 캐릭터만의 것이 아니다 — Step 02가 `IA_Interact`를, Step 04가 `IA_ToggleInventory`를 추가한다.

---

## 3. 제안 구조

```
Content/
├─ Characters/
│   ├─ Heroes/                    (Lyra 유래 마네퀸 — 그대로)
│   ├─ MetaHuman/                 (리타깃 애님 — 그대로)
│   └─ Player/            ★신설
│       ├─ Abilities/               BP_GA_Death, BP_GA_Item_*, BP_GA_Skill_*
│       ├─ BP_EPCharacter
│       ├─ BP_MH_Char1
│       └─ BP_EPCorpse
│
├─ Weapons/                ★신설 — 무기 BP·아트 (DA는 Data/에 남는다. §6-3)
│   ├─ AK74/                      BP_WeaponAK74
│   └─ Projectiles/               BP_Projectile_Fast, BP_Projectile_Slow
│
├─ Items/                  ★신설 — 무기 아닌 아이템의 아트 (§6-3)
│   ├─ Backpack_Small/            SM_/MI_/T_ + 아이콘
│   ├─ Bandage/
│   └─ AmmoBox_545/               폴더 이름 = ItemId
│
├─ Input/                  ★승격 (Characters/InputActions →)
│   ├─ Actions/                   IA_* 11개 (Skill/ 하위 유지)
│   └─ Mappings/                  IMC_DefaultMappingContext
│
├─ GameplayEffects/        ★신설 (Blueprints/GE →)
│   ├─ Core/                      GE_Damage, GE_ConsumeAmmo, ...
│   └─ Skill/                     GE_Dash_Cooldown, GE_Heal, ...
│
├─ UI/                     ★신설 (Blueprints/HUD + WBP_Crosshair →)
│   └─ Hud/                       WBP_*, M_RingGauge, SlotBorder, ...
│
├─ System/                 ★신설
│   ├─ BP_EPGameMode
│   └─ BP_EPPlayerController
│
├─ Loot/                   ★신설
│   └─ BP_EPItemSpawner
│
├─ Data/                   ★그대로 둔다 — §5. AssetManager 계약이 걸려 있다
│   ├─ Ammo/ Consumable/ Misc/ QuestItem/ Weapons/   DA_*
│   ├─ Loot/                      LT_*
│   └─ DT_Items, CF_Spread
│
├─ Effects/                       (Lyra 유래 + Blueprints/VFX 흡수)
├─ Audio/                         (Lyra 유래 — 그대로)
├─ PhysicsMaterials/       ★통합   PM_Character + Data/PM_* + Effects/.../PhM_*
├─ Maps/                          Main
├─ Developers/ Collections/       (엔진 관례 — 그대로)
│
├─ _Import/                ★신설
│   ├─ AnimStarterPack/
│   └─ FPS_Weapon_Bundle/
│
├─ MetaHumans/             ★최상위 유지 — §6. 도구가 이 경로를 안다
├─ Megascans/              ★최상위 유지 — §6
└─ MSPresets/              ★최상위 유지 — §6
```

### 왜 `Abilities/`가 최상위가 아닌가

Lyra가 GA를 소유자 밑에 둔다 — `Content/Characters/Heroes/Abilities/`. GE는 반대로 최상위 `GameplayEffects/`다(`Damage/`, `Heal/`). 이유가 있다: **어빌리티는 누가 가지는지가 정해져 있고, 이펙트는 아무나 적용한다.**

우리 GA 6개는 전부 `DefaultAbilities`(`EPCharacter.h:61`)로 캐릭터에 부여된다. Step 02의 `GA_Interact`도 마찬가지다. **`Characters/Player/Abilities/`가 맞는 자리다.**

> **`Characters/Heroes/Abilities/`에 넣지 않는다.** `Heroes/`는 Lyra에서 통째로 가져온 폴더다. 내 것을 그 안에 섞으면 나중에 Lyra 콘텐츠를 갱신하거나 걷어낼 때 뭘 지워도 되는지 알 수 없게 된다. **"임포트한 것과 만든 것은 절대 같은 폴더에 넣지 않는다"** — 이게 §2-3과 같은 원칙이다.

### 접두사 정리 (선택, 낮은 우선순위)

| 현재 | Lyra | 비고 |
|---|---|---|
| `BP_GA_Skill_Dash` | `GA_*` | `BP_` + `GA_`는 중복이다 |
| `PM_Head` / `PhM_Body` | `PhysMat_*` | 같은 타입인데 접두사가 둘 |

**지금 하지 않는다.** 리네임도 리다이렉터를 만들고, 이동과 섞으면 문제가 생겼을 때 원인이 둘로 갈린다. **이동을 끝내고 PIE가 통과한 뒤에 별건으로 한다.**

---

## 4. 이동 매핑

| 옮길 것 | 어디로 | 개수 |
|---|---|---|
| `AnimStarterPack/` | `_Import/AnimStarterPack/` | 92 |
| `FPS_Weapon_Bundle/` | `_Import/FPS_Weapon_Bundle/` | 220 |
| `Characters/InputActions/IA_*` | `Input/Actions/` | 11 |
| `Characters/InputActions/IMC_*` | `Input/Mappings/` | 1 |
| `Blueprints/GA/**` | `Characters/Player/Abilities/` | 6 |
| `Blueprints/GE/**` | `GameplayEffects/` (Core·Skill 유지) | 15 |
| `Blueprints/HUD/**` + `Blueprints/WBP_Crosshair` | `UI/Hud/` | 12 |
| `Blueprints/VFX/**` | `Effects/Particles/` | 3 |
| `Blueprints/Loot/BP_EPItemSpawner` | `Loot/` | 1 |
| `Blueprints/BP_EPCharacter`, `BP_MH_Char1`, `BP_EPCorpse` | `Characters/Player/` | 3 |
| `Blueprints/BP_WeaponAK74` | `Weapons/AK74/` | 1 |
| `Blueprints/BP_Projectile_*` | `Weapons/Projectiles/` | 2 |
| **`Blueprints/BP_EPGameMode`, `BP_EPPlayerController`** | **`System/`** | 2 ★ini 수정 동반 |
| `Data/PM_*` + `Effects/Materials/Decals/PhM_*` | `PhysicsMaterials/` | 7 |
| `Materials/M_Green` | 참조 없으면 `Developers/wnsgn/`, 있으면 `Effects/Materials/` — §6-2 | 1 |

**`Data/`의 `DA_*`, `LT_*`, `DT_Items`, `CF_Spread`는 옮기지 않는다.** 이유는 다음 절.

---

## 5. ★ 옮기면 깨지는 것 — 리다이렉터가 못 고치는 7곳

에디터에서 에셋을 옮기면 **리다이렉터(redirector)** 가 남아서 기존 참조가 계속 산다. 블루프린트 참조, DataAsset 참조, 레벨 배치는 전부 자동으로 따라온다.

**`.ini`는 아니다.** config의 경로는 그냥 문자열이라 리다이렉터를 안 탄다. 그리고 **조용히 실패한다.**

| 파일 : 줄 | 값 | 영향 |
|---|---|---|
| `DefaultEngine.ini:4` | `GameDefaultMap=/Game/Maps/Main.Main` | `Maps/`를 안 건드리면 무해 |
| `DefaultEngine.ini:5` | `EditorStartupMap=/Game/Maps/Main.Main` | 위와 같음 |
| **`DefaultEngine.ini:6`** | **`GlobalDefaultGameMode=/Game/Blueprints/BP_EPGameMode.BP_EPGameMode_C`** | **★ `System/`으로 옮기면 반드시 같이 고친다** |
| `DefaultGame.ini:10,12` | Map 스캔 `/Game/Maps` | 무해 |
| **`DefaultGame.ini:14`** | **ItemDef 스캔 `Directories=(Path="/Game/Data")`** | **★ `Data/`를 안 건드리는 이유** |
| **`DefaultGame.ini:15`** | **LootTable 스캔 `Directories=(Path="/Game/Data/Loot")`** | **★ 같은 이유** |
| **`DefaultGame.ini:35`** | **`ItemDataTable=/Game/Data/DT_Items.DT_Items`** | **★ `DT_Items`를 옮기면 같이 고친다** |

> `DefaultGame.ini:38-39`의 `PlaceholderPickupMesh=/Engine/BasicShapes/Cube.Cube`와 `PickupClass=/Script/EmploymentProj.EPPickup`은 각각 엔진 콘텐츠와 C++ 클래스 경로라 **영향 없다.**

> **C++에는 하드코딩된 `/Game/` 경로가 없다.** 전체 `Source/`에서 `/Game/`·`FSoftObjectPath`·`ConstructorHelpers`를 찾으면 `EPItemDefinition.cpp:30`의 자기 자신 비교 하나뿐이다. **코드는 이 이동에 안전하다.**

### `GlobalDefaultGameMode`가 깨지면 어떻게 보이는가

경로가 안 풀리면 엔진이 `AGameModeBase` 기본값으로 떨어진다. **에러 대화상자가 안 뜬다.** 증상은 *"PIE를 켰는데 캐릭터가 아니라 기본 폰이 나오고 총이 없다"*이고, 로그의 경고 한 줄이 유일한 단서다. **`System/` 단계는 이동과 ini 수정을 같은 커밋에 넣는다.**

### `Data/`를 그대로 두는 이유

`ItemDef` 스캔이 `/Game/Data`를, `LootTable` 스캔이 `/Game/Data/Loot`를 가리킨다. 옮기려면 ini 두 줄을 고쳐야 하는데, **지금 구조가 이미 맞다.** 기능별로 갈라져 있고(`Ammo/`, `Consumable/`, `Loot/`, `Misc/`, `QuestItem/`, `Weapons/`) Lyra에도 대응하는 관례가 없다. **고칠 이유가 없는 유일한 폴더다.**

> **`Data/Weapons/DA_AK74_*`도 옮기지 않는다.** `Weapons/AK74/`로 모으면 보기 좋지만 ItemDef 스캔이 `/Game/Data`를 벗어나 `.ini`에 `/Game/Weapons`를 추가해야 하고, **다른 아이템은 전부 `Data/`에 데이터를 두는데 무기만 예외가 된다.** §6-3의 규칙 — *"AssetManager가 스캔하는 것만 `Data/`, 아트는 기능 폴더"* — 이 여기서도 답이다.

---

## 6. ★ 손대면 안 되는 폴더 3개

### `MetaHumans/` — 도구가 이 경로를 기본값으로 안다

```cpp
// Engine/Plugins/MetaHuman/MetaHumanSDK/.../Public/Import/MetaHumanImport.h:74
inline static const FString DefaultDestinationPath = TEXT("/Game/MetaHumans");
```

**바꿀 수는 있다.** 프로젝트 설정에 오버라이드가 있다.

```cpp
// MetaHumanSDKSettings.h:24, :28
FDirectoryPath CinematicImportPath{TEXT("/Game/MetaHumans")};
FDirectoryPath OptimizedImportPath{TEXT("/Game/MetaHumans")};
```

**하지만 `Common` 하위 폴더는 못 바꾼다.**

```cpp
// MetaHumanImport.cpp:597
FString DestinationCommonAssetPath = ImportDescription.DestinationPath / FImportPaths::CommonFolderName;
                                                          // ↑ 주석: "At the moment this can not be changed"
```

그리고 **설정으로도 못 바꾸는 하드코딩이 따로 있다.**

```cpp
// MetaHumanCharacterEditor/Private/Subsystem/MetaHumanCharacterBuild.cpp:540
FString PathToMHCommonSkeleton =
    TEXT("/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel.metahuman_base_skel");
```

```python
# MetaHumanAnimator/Content/Python/export_performance.py:30
target_skeleton = unreal.load_asset('/Game/MetaHumans/Common/Face/Face_Archetype_Skeleton.Face_Archetype_Skeleton')
```

**이 두 경로가 우리 프로젝트에 실제로 존재한다** — `MetaHumans/Common/Female/Medium/NormalWeight/Body`, `MetaHumans/Common/Face`. 옮기면 문자열 리터럴이라 리다이렉터를 안 타고 그냥 못 찾는다.

**결론:** 옮기면 두 가지가 동시에 터진다 — ① 다음 임포트가 `/Game/MetaHumans`를 다시 만들어 287개가 중복되고, ② 위 리터럴 경로를 쓰는 도구가 실패한다. **그냥 둔다.**

### `Megascans/` · `MSPresets/` — ★ 옮기면 Bridge가 깨진다

`MSPresets`는 "얻는 게 없다" 수준이 아니다. **Bridge 플러그인이 이 경로를 문자열 리터럴로 10곳에서 들고 있다.**

```cpp
// Engine/Plugins/Bridge/Source/MegascansPlugin/Private/UI/SMSWindow.cpp:212, :229, :245
FString MasterMaterialPath = TEXT("/Game/MSPresets/M_MS_Default_Material/M_MS_Default_Material.M_MS_Default_Material");
FString MasterMaterialPath = TEXT("/Game/MSPresets/M_MS_Foliage_Material/...");
FString MasterMaterialPath = TEXT("/Game/MSPresets/M_MS_Surface_Material/...");

// Bridge/Source/MegascansPlugin/Private/Utilities/MiscUtils.cpp:92-96, :152
MaterialBasePath.Add("/Game/MSPresets");
MaterialBasePath.Add(FPaths::Combine(TEXT("/Game/MSPresets"), MaterialName));
SyncPaths.Add(TEXT("/Game/MSPresets"));

// Bridge/Source/Bridge/Private/UI/BrowserBinding.cpp:485
FString SurfaceInstancePath = TEXT("/Game/MSPresets/M_MS_Surface_Material/M_MS_Surface_Material.M_MS_Surface_Material");
```

**설정이 없다.** `FDirectoryPath`도 아니고 그냥 `TEXT()` 리터럴이다. 우리 `MSPresets/`에 `M_MS_Surface_Material`, `M_MS_Default_Material`, `M_MS_Foliage_Material`이 실제로 있으니(§1 실측) 정확히 이 경로들을 가리키고 있다.

옮기면 **다음 Megascans 임포트가 마스터 머티리얼을 못 찾는다.** 증상은 임포트한 서피스가 회색으로 나오는 것이고, 원인이 폴더 이동이라는 걸 알아채기 어렵다.

> 최신 Fab 임포터는 목적지를 파라미터로 받으므로(`Fab/.../QuixelGLTFImporter.cpp:138` 등) **새로** 임포트하는 것은 원하는 곳에 넣을 수 있다. 하지만 구형 Bridge가 내보낸 기존 95개는 그 자리에 있어야 한다.

> **`_Import/`에 넣을 것은 `AnimStarterPack`과 `FPS_Weapon_Bundle` 둘뿐이다.** 이 둘은 마켓플레이스 에셋이라 재임포트 경로를 아는 도구가 없다.

---

## 6-2. ★ 월드 배치 액터 / 테스트 에셋은 어디로

세 종류가 섞여서 헷갈리는 자리다. **Lyra는 셋을 서로 다른 곳에 둔다.**

| 종류 | 예 | 어디로 | 쿠킹에 들어가나 |
|---|---|---|---|
| ① 기능에 속한 월드 배치 액터 | `BP_EPItemSpawner` | **기능 폴더** (`Loot/`) | ✅ |
| ② 기능에 딸린 **정식** 테스트 대상 | 사격 연습 표적 | **그 기능 폴더의 `Tests/`** | ✅ |
| ③ 내 개인 실험물 | `M_Green` | **`Developers/wnsgn/`** | ❌ (설정 필요) |

### ① `BP_EPItemSpawner` — 기능 폴더. `Environments/`가 아니다

Lyra는 월드에 배치하는 게임플레이 액터를 두 군데에 나눠 둔다.

```
Environments/Gameplay/BP_GameplayEffectPad.uasset      ← 어느 기능에도 안 속하는 범용 패드
Weapons/Spawnpad/                                      ← 무기 기능에 속하는 것은 무기 폴더
```

**판정 기준: `DOCS/`에 이름이 있는 기능에 속하는가.**

`BP_EPItemSpawner`는 `05_Loot_01_Spawner.md`가 통째로 다루는 대상이고, `Data/Loot/LT_*`와 짝을 이룬다. **`Loot/`가 맞다.**

그리고 `Loot/`의 경계는 명확하다 — **`UEPLootTable`을 쓰는 액터**다.

```
Loot/
├─ BP_EPItemSpawner            (Step 01)
├─ (BP_EPPickup 파생이 생기면)   (Step 01~03)
├─ (BP_EPContainer)            (§7-1 컨테이너 — LT를 그대로 재사용한다)
└─ (BP_EPVendingMachine)       (§7-2 자판기 — 같은 이유)
```

> **탈출 지점(로드맵 12)은 여기가 아니다.** `IEPInteractable`은 구현하지만 `UEPLootTable`을 안 쓴다. 그때 `Environments/Gameplay/`를 만들거나 `Extraction/`을 만든다 — **지금 만들지 않는다.** 소비자가 없다.

### ② 기능 테스트 대상 — 그 기능 폴더의 `Tests/`

Lyra의 실제 예다.

```
Weapons/Tests/
├─ B_ShootingTarget.uasset            사격 연습 표적 액터
├─ GE_HugeHealthTarget.uasset         표적용 체력 이펙트
├─ ShootingTarget_AbilitySet.uasset
└─ ShootingTarget_PawnData.uasset
```

**이건 지워도 되는 물건이 아니다.** 사격 연습장 레벨에 실제로 배치돼 있고 쿠킹에 들어간다. *"테스트"*라는 이름이지만 **정식 콘텐츠**다.

우리 기준으로는 이런 것들이 여기 간다.

- 루트 확률 검증용 표적/더미 → `Loot/Tests/`
- 히트박스 검증용 더미 폰 → `Characters/Tests/`

> 콘솔 명령(`EP.Loot.RollTable` 등)은 에셋이 아니라 코드라 이 규칙과 무관하다. 이미 `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)`로 격리돼 있다(`EPLootDebugCommands.cpp:13`).

### ③ ★ 개인 실험물 — `Developers/<이름>/`. 엔진이 예약한 이름이다

`M_Green` 하나 때문에 최상위에 `Materials/` 폴더가 있는 게 지금 상태다. **이런 건 `Developers/`로 간다.**

`Developers`는 엔진이 이름을 아는 폴더다.

```cpp
// Runtime/Core/Private/Misc/Paths.cpp:641-644
FStringView FPaths::DevelopersFolderName()
{
    return TEXTVIEW("Developers");
}
```

세 가지가 따라온다.

| 동작 | 근거 |
|---|---|
| 콘텐츠 브라우저가 **기본으로 숨긴다** | `ContentBrowserUtils.cpp:1238` — `GetDisplayDevelopersFolder()` |
| 커맨드릿이 기본으로 제외한다 | `PackageUtilities.cpp:110-117` — `NormalizePackageNames`가 이 경로를 거른다 |
| 사용자별 하위 폴더를 자동 생성 | `Paths.cpp:672` — `GameUserDeveloperFolderName()` |

**이 프로젝트에 이미 `Developers/wnsgn/`이 있다**(§1 실측, 에셋 0개). 만들 필요 없이 쓰면 된다.

> **★ 숨기는 것과 쿠킹에서 빼는 것은 별개다.** 콘텐츠 브라우저가 안 보여주는 것뿐이고, 다른 에셋이 참조하면 그대로 패키징에 들어간다. 확실히 빼려면 `Project Settings → Packaging → Directories to never cook`에 `/Game/Developers`를 추가한다.
>
> 그래서 규칙이 하나 붙는다 — **`Developers/` 안의 것을 정식 에셋이 참조하면 안 된다.** 참조가 생기는 순간 그건 실험물이 아니라 콘텐츠이고, 위 표의 ① 또는 ②로 옮겨야 한다.

### 판별 한 줄

> **"내일 지워도 아무도 모르는가?"** → `Developers/wnsgn/`
> **"레벨에 배치돼 있거나 다른 에셋이 참조하는가?"** → 기능 폴더 (테스트 성격이면 그 밑 `Tests/`)

---

## 6-3. ★ 새 아트 에셋(메시·머티리얼·텍스처)은 어디로

### 규칙 하나로 갈린다 — **AssetManager가 스캔하는가**

`.ini`가 딱 두 경로를 스캔한다(§5).

```ini
; DefaultGame.ini:14   ItemDef    → /Game/Data
; DefaultGame.ini:15   LootTable  → /Game/Data/Loot
```

| 에셋 | 어디 | 이유 |
|---|---|---|
| `UEPItemDefinition` 파생 DA | **반드시 `Data/` 아래** | 스캔 경로를 벗어나면 `Definitions = 0`이 된다 |
| `UEPLootTable` (`LT_*`) | **반드시 `Data/Loot/`** | 같은 이유 |
| `DT_Items`, `CF_Spread` | `Data/` | `ItemDataTable=/Game/Data/DT_Items.DT_Items`(`:35`) |
| **메시·머티리얼·텍스처·아이콘·BP** | **기능 폴더 어디든** | 스캔 대상이 아니다 |

**데이터와 아트를 붙여 놓을 기술적 이유가 없다.**

```cpp
// EPItemDefinition.h:31, :35
TSoftObjectPtr<UStaticMesh> WorldMesh;
TSoftObjectPtr<UTexture2D>  Icon;
```

둘 다 **소프트 참조**다. 경로가 달라도 되고, 나중에 옮겨도 리다이렉터가 따라온다.

### 그래서 새 아이템 아트는 `Items/<아이템Id>/`

```
Content/
├─ Data/                          ← 데이터 (건드리지 않는다)
│   ├─ Misc/DA_Backpack_Small     ── WorldMesh ─┐  소프트 참조
│   └─ Consumable/DA_Bandage                    │
│                                               ↓
└─ Items/                         ★신설 — 아트
    ├─ Backpack_Small/
    │   ├─ SM_Backpack_Small
    │   ├─ MI_Backpack_Small
    │   └─ T_Backpack_Small_D / _N / _ORM
    ├─ Bandage/
    └─ AmmoBox_545/
```

**폴더 이름은 `ItemId`와 같게 쓴다.** `DT_Items`의 행 이름, `DA_` 접미사, 폴더 이름이 전부 같으면 셋 중 하나만 알아도 나머지를 찾는다.

> **에셋이 8개를 넘으면 그때 타입 하위 폴더를 만든다.** Lyra의 `Weapons/Rifle/{Animations, Materials, Mesh, Sounds, Textures}`가 그 형태인데, 소총은 수십 개다. 붕대는 메시 1 + 머티리얼 1 + 텍스처 2다 — **처음부터 5단계로 파면 클릭만 늘어난다.**

### 무기는 `Weapons/`, 나머지 아이템은 `Items/`

무기는 BP 액터·애님·사운드가 붙어서 아이템과 규모가 다르다. Lyra도 `Weapons/`를 따로 둔다.

```
Weapons/
├─ AK74/            BP_WeaponAK74 + (나중에 애님·사운드)
└─ Projectiles/     BP_Projectile_Fast, BP_Projectile_Slow
```

**`DA_AK74_*` 3개는 `Data/Weapons/`에 그대로 둔다.** `Weapons/AK74/`로 옮기면 ItemDef 스캔(`/Game/Data`)을 벗어나서 `.ini`에 `/Game/Weapons`를 추가해야 한다. **다른 아이템은 전부 `Data/`에 데이터를 두는데 무기만 예외가 되는 것도 나쁘다.**

### 임포트한 것 vs 내가 만든 것

| 출처 | 어디 |
|---|---|
| 마켓플레이스/외주 팩을 **통째로** 받았다 | `_Import/<팩이름>/` — **팩 구조를 그대로 유지한다** |
| 내가 모델링했거나, 팩에서 **골라 가공**했다 | `Items/<ItemId>/` 또는 `Weapons/<이름>/` |

**팩을 뜯어서 재배치하지 않는다.** 팩이 갱신되면 재임포트가 원래 구조로 덮어쓴다 — 뜯어 놓았으면 뭐가 새 것이고 뭐가 내가 옮긴 것인지 구분이 안 된다. `FPS_Weapon_Bundle`이 정확히 그 경우다.

> 팩의 메시를 그대로 쓰되 머티리얼만 바꿨다면, **메시는 `_Import/`에 두고 머티리얼 인스턴스만** `Items/<ItemId>/`에 만든다. `MI_`는 소프트가 아니라 하드 참조지만 리다이렉터가 따라오므로 문제없다.

### 아이콘

`Icon`(`EPItemDefinition.h:35`)은 UI 리소스다. **아이템 폴더에 같이 둔다** — `Items/Backpack_Small/T_Icon_Backpack_Small`. `UI/`로 빼지 않는다. `UI/`는 위젯과 그 전용 리소스의 자리이고, 아이콘은 아이템이 사라지면 같이 사라진다.

### 지금 정리할 것

| 대상 | 판정 |
|---|---|
| `Materials/M_Green` | 참조가 없으면 `Developers/wnsgn/`로. **그러면 최상위 `Materials/` 폴더가 통째로 사라진다** |
| `Characters/MetaHuman/Animations/test/` | 이름이 소문자 `test`다. 열어서 ②인지 ③인지 판정 — **모르면 `Developers/`가 아니라 그대로 둔다** (참조가 있을 수 있다) |
| `Audio/Test/` (3개) | Lyra 유래다. 우리가 만든 게 아니므로 건드리지 않는다 |

> `M_Green`을 옮기기 전에 **우클릭 → Reference Viewer**로 참조를 확인한다. 레벨이나 머티리얼 인스턴스가 물고 있으면 ③이 아니다.

---

## 6-4. ★ 미리 만들 폴더 — 3개뿐이다

### 먼저: 빈 폴더를 미리 파는 것은 **효과가 없다**

**git은 빈 디렉터리를 추적하지 않는다.** `Content/`는 git에 들어 있지만(1,010개 추적 중), 에셋이 없는 폴더는 커밋해도 아무것도 안 남는다. 다른 머신에서 클론하면 사라진다.

`.gitkeep`을 넣으면 남긴 하는데, **그건 폴더가 아니라 문서가 할 일이다.** 자리를 예약하는 진짜 수단은 *"이 에셋이 생기면 여기로 간다"*가 어딘가에 **글자로 적혀 있는 것**이고, 그게 지금 이 문서다.

> CLAUDE.md §2가 코드에 대해 말하는 것과 같다 — *"이 확장점이 `DOCS/` 어딘가에 이름으로 적혀 있는가? 적혀 있으면 만든다. 없으면 그 문서를 먼저 고친다."* **폴더도 똑같다.**

### 지금 만들 것 — Step 02가 **이번에** 에셋을 만든다

| 폴더 | 무엇이 들어오나 | 왜 지금인가 |
|---|---|---|
| **`Input/Actions/`** | `IA_Interact` | `05_Loot_02_Interaction.md` 02-6이 만들라고 적어 놨다 |
| **`Characters/Player/Abilities/`** | `BP_GA_Interact` | 02-6 GAS 배선. `DefaultAbilities`에 넣을 BP |
| **`Loot/`** | `BP_EPItemSpawner` (이동) | Step 01이 진행 중이다 |

**이 셋을 안 만들면 Step 02의 새 에셋 2개가 `Characters/InputActions/`와 `Blueprints/GA/`에 생긴다.** 그러면 나중에 옮길 것이 2개 늘어난다. 지금 만드는 비용은 0이고, 만들자마자 채워지므로 빈 폴더 문제도 없다.

> `Input/Mappings/`도 같이 만든다 — `IMC_DefaultMappingContext`가 `IA_Interact` 추가 때문에 어차피 수정된다.

### 다음 Step이 요구하는 것 — **그때 만든다**

| 폴더 | 문서 근거 | 언제 |
|---|---|---|
| `Items/<ItemId>/` | §6-3. `Data/`에 `DA_*` 9개가 이미 있는데 **아이콘이 0개다** | Step 04 (`05_Loot_04_InventoryUI.md` 완료 조건이 *"아이콘·이름·칸 수가 표시"*) |
| `UI/Inventory/` | Step 04 인벤토리 위젯 | Step 04 |
| `Loot/` 안의 컨테이너·자판기 | `05_Loot_DOCS.md` §7-1, §7-2 / 로드맵 7 | 미정 |
| `Weapons/<이름>/` 추가 | Step 05 `DefaultLoadout` | Step 05 |

**전부 폴더가 아니라 에셋이 병목이다.** `Items/Backpack_Small/`을 지금 만들어 봐야 안에 넣을 메시가 없다.

### 만들지 않는 것

| 후보 | 왜 안 만드나 |
|---|---|
| `AI/` | 로드맵 8. **미착수**이고 클래스가 하나도 없다 |
| `Extraction/` | 로드맵 12. 같은 이유. §6-2에서 이미 *"그때 정한다"*로 남겼다 |
| `Environments/` | 로드맵 13(맵 제작)이 올 때. 지금 배치할 액터가 `BP_EPItemSpawner` 하나뿐이고 그건 `Loot/`다 |
| `Localization/` `Legal/` `Tools/` `ContextEffects/` `Feedback/` | **Lyra엔 있지만 우리 계획엔 없다.** 상상한 확장점이다 |
| `System/Experiences/` `System/Playlists/` | Lyra의 Game Feature 구조 전용. §9에서 도입하지 않기로 했다 |

### 폴더보다 먼저 챙길 것

**`DA_*` 9개에 `Icon`이 하나도 없다.** `EPItemDefinition.h:35`의 `TSoftObjectPtr<UTexture2D> Icon`이 전부 비어 있다는 뜻이고, Step 04 완료 조건이 아이콘 표시다. **폴더를 미리 파는 것보다 이게 실제 병목이다.**

---

## 7. 실행 순서

**위험이 낮은 것부터. 각 단계마다 커밋하고 PIE 한 번.**

| # | 단계 | ini 수정 | 검증 |
|---|---|---|---|
| 1 | `_Import/` 만들고 `AnimStarterPack`, `FPS_Weapon_Bundle` 이동 | 없음 | 무기 메시가 보인다 |
| 2 | `Input/Actions`, `Input/Mappings` 승격 | 없음 | 이동·사격·스킬 입력이 먹는다 |
| 3 | `GameplayEffects/` (Blueprints/GE →) | 없음 | 사격·재장전·대시·힐·실드 |
| 4 | `UI/Hud/` (Blueprints/HUD + WBP_Crosshair →) | 없음 | HUD 게이지·슬롯·크로스헤어 |
| 5 | `Characters/Player/` + `Abilities/` | 없음 | 캐릭터가 나오고 어빌리티가 돈다 |
| 6 | `Weapons/`, `Loot/`, `Effects/Particles`(VFX) | 없음 | 발사·임팩트·스포너 |
| 7 | **`System/` + `DefaultEngine.ini:6` 수정** | **★ 1곳** | **PIE에서 BP_EPGameMode가 뜬다** |
| 8 | `PhysicsMaterials/` 통합 | 없음 | 부위별 데미지 배율, 데칼 |
| 9 | 빈 `Blueprints/`·`Materials/` 삭제, Fix Up Redirectors 전체 | 없음 | 에디터 재시작 후 경고 0 |

### 이동 규칙

1. **반드시 콘텐츠 브라우저에서 옮긴다.** 탐색기에서 폴더를 옮기면 리다이렉터가 안 생기고 **참조가 통째로 끊긴다.** 되돌릴 방법은 git뿐이다.
2. **한 단계 = 한 커밋.** `.uasset`은 바이너리라 병합이 불가능하다. 중간에 꼬이면 `git checkout`으로 그 단계만 되돌린다.
3. **각 단계 뒤 `Fix Up Redirectors in Folder`** (폴더 우클릭). 안 하면 리다이렉터가 쌓이고 그대로 쿠킹에 들어간다.
4. **7번 단계는 이동과 ini 수정을 같은 커밋에.** 나눠 커밋하면 그 사이 커밋에서 프로젝트가 안 뜬다.
5. 9번의 전체 Fix Up **뒤에 에디터를 재시작**한다. 리다이렉터 정리는 재시작 전까지 캐시에 남는다.

---

## 8. Source는 손대지 않는다

```
Public/                       Private/
├─ Animation  2               ├─ Animation  2
├─ Combat     6               ├─ Combat     6
├─ Core       7               ├─ Core       7
├─ Data       5               ├─ Data       4
├─ GAS        9               ├─ GAS        9
├─ HUD        7               ├─ HUD        6
├─ Loot       3               ├─ Loot       4
├─ Movement   1               └─ Movement   1
└─ Types      1
```

**이미 기능별이고 `Public`/`Private`이 정확히 미러링된다.** `Types/`가 `Public`에만 있는 것은 헤더 전용이라 정상이다. **고칠 것이 없다.**

---

## 9. 하지 않는 것

- **Game Feature Plugin (Lyra의 `ShooterCore` 방식)** — Lyra는 게임플레이 콘텐츠 대부분을 `Plugins/GameFeatures/` 5개로 뺀다. 그건 **여러 모드/맵을 런타임에 켜고 끄기 위한 구조**다. 우리는 맵이 하나(`Maps/Main`)고 모드가 하나다. 지금 도입하면 빌드 설정·에셋 경로·로딩 순서가 전부 복잡해지고 얻는 게 없다.
- **접두사 일괄 리네임** — §3 끝에 적은 이유로 이동 완료 후 별건.
- **`Characters/MetaHuman/Animations/test/`** — 실측에 이름이 `test`인 폴더가 있다. 안에 뭐가 있는지 확인하고 지울지 결정하는 건 이동과 섞지 않는다.
- **`Data/` 재편** — §5. 유일하게 config 계약이 걸렸고 지금 구조가 맞다.

---

## 10. 이 정리가 작동하고 있다면

- 새 위젯을 만들 때 `UI/Hud/`인지 `Blueprints/HUD/`인지 고민하지 않는다
- 무기를 하나 추가할 때 세 폴더를 안 열어도 된다
- 콘텐츠 브라우저 최상위에서 내가 만든 것과 임포트한 것이 한눈에 갈린다
- Step 02의 `IA_Interact`가 갈 곳이 이미 정해져 있다 (`Input/Actions/`)
