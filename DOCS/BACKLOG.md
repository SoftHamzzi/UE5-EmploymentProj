# 이월 목록 — 지금 안 하기로 한 것들

> **여기 있는 것은 전부 "안 해도 돌아간다".** 그래서 미뤘고, 그래서 잊힌다.
>
> 이 파일의 목적은 목록을 늘리는 게 아니라 **같은 토론을 두 번 하지 않는 것**이다.
> 항목마다 **지금 안 하는 이유**가 반드시 적혀 있어야 한다. 없으면 그건 결정이 아니라 미룸이다.

## 여기 적는 것 / 안 적는 것

| | 어디로 |
|---|---|
| 지금 안 해도 되지만 나중에 바꾸려는 설계 | **여기** |
| 버그, 미완료 완료조건, 단계 진행 상황 | `Notes/0X/*_STATUS.md` |
| 그 단계가 **반드시** 건드려야 하는 것 | `LOOT_STATUS.md` §기존 코드에서 반드시 손대야 할 것 |
| 기획 변경 | `GAME.md` |
| 아직 안 만든 기능 | `DOCS.md` §5 |

**항목 형식:** 현재 / 바꿀 것 / **지금 안 하는 이유** / 트리거 / 근거

---

## B-1. 무기 FX 에셋이 컴포넌트에 있다 ★

**현재** — `EPCombatComponent.h:66-73`
```cpp
UPROPERTY(EditDefaultsOnly, Category = "VFX|Fire")
TObjectPtr<UNiagaraSystem> MuzzleFX, ImpactFX;
TObjectPtr<USoundBase>     FireSFX, ImpactSFX;
```
`BP_EPCharacter`의 컴포넌트 기본값이라 **모든 무기가 같은 총구 화염·발사음·임팩트**를 쓴다. AK든 샷건이든 동일.

**바꿀 것** — `UEPWeaponDefinition`으로 이동.

**지금 안 하는 이유** — 무기가 셋뿐이고 전부 소총 계열이라 체감 차이가 없다. 단독으로 하면 DA 3종 재저장이 필요하다.

**트리거** — **Step 05 (무기 장착 이관).** 어차피 `WeaponDef`를 만지므로 그때 거의 공짜다. B-2의 선행 조건이기도 하다.

**근거**
- 프로젝트 자신의 결정과 충돌: `LOOT_STATUS.md` — *"획득 사운드·VFX·메시는 `UEPItemDefinition`"*
- DT vs DA 배치 원칙 — *"에셋 참조·virtual·타입 전용은 DA"*
- Lyra: `ULyraPickupDefinition`이 `DisplayMesh`/`PickedUpSound`/`PickedUpEffect`를 들고 액터 클래스 필드는 없다

---

## B-2. 발사·착탄 연출을 GameplayCue로

**현재** — `PlayLocalMuzzleEffect`(`:114`) / `PlayLocalImpactEffect`(`:131`)가 Niagara·Sound를 직접 스폰. `Multicast_PlayImpactEffect`(`:241`)가 착탄점 배열을 멀티캐스트.

**바꿀 것 (Lyra 방식)**
```
발사    → 무기별 Cue 에셋      GCN_Weapon_{Rifle,Pistol,Shotgun}_Fire
착탄    → 공용 Cue 하나        GCN_Weapon_Impact
재질별  → FGameplayCueParameters::PhysicalMaterial (GameplayEffectTypes.h:913)
```

**지금 안 하는 이유**
1. **B-1이 선행**이다. FX가 컴포넌트에 있는 한 Cue로 바꿔도 무기별이 안 된다 — Lyra가 무기별 Cue를 가질 수 있는 건 무기 데이터가 자기 Cue 태그를 알기 때문이다
2. 8주에 구현 4건(인벤토리·AI·탈출·로비)이 있다. 연출 리팩터는 그 뒤

**★ 전부 바꾸지는 않는다 — 산탄 때문**

`FGameplayCueParameters`의 `Location`(`:893`) / `Normal`(`:897`)이 **단수**다. 배열이 없다.
지금은 `Multicast_PlayImpactEffect(TArray<...> Points, TArray<...> Normals)`로 **샷건 펠릿 8발을 한 패킷**에 보낸다. Cue로 바꾸면 **펠릿당 멀티캐스트 1회**가 된다.

> **절충:** 캐릭터 히트만 Cue(대미지 GE에 태그를 붙이면 호출 코드가 사라진다), **벽/지형 착탄은 기존 Multicast 유지.**

**Lyra에는 이 문제가 없는 이유 (구조 차이 — 복사되지 않음)**
Lyra는 `FGameplayAbilityTargetDataHandle`을 GAS 표준 경로로 복제해서(`ServerSetReplicatedTargetData`) 히트 목록이 이미 네트워크로 오간다. 그래서 별도 멀티캐스트 RPC가 **아예 없다.**
우리는 TargetData 대신 SSR + 직접 Multicast다. **틀린 게 아니라 랙 보상을 직접 구현한 결과**고, 갈아엎는 범위가 커서 여기 없다.

**근거 (전부 직독)**
- Lyra C++에 `ExecuteGameplayCue`/`AddGameplayCue` 직접 호출 **0건** — 연출은 전부 BP
- `LyraGameplayAbility_RangedWeapon.h:118-119` — `OnRangedWeaponTargetDataReady`가 `BlueprintImplementableEvent`. C++은 타겟 데이터까지만
- Cue 에셋 위치: `Plugins/GameFeatures/ShooterCore/Content/Weapons/{Pistol,Rifle,Shotgun}/GCN_*_Fire.uasset`, `Content/GameplayCueNotifies/GCN_Weapon_Impact.uasset`
- 등록: `Config/DefaultGame.ini:20-21` `+GameplayCueNotifyPaths=`
- 재질 분기는 Lyra에선 Cue 밖 — `LyraContextEffectsLibrary.h:31-41` (`EffectTag` × `Context` 태그 두 축으로 에셋 조회). 우리는 `UEPPhysicalMaterial`이 이미 있어 Cue 파라미터로 충분

---

## B-3. `case Hitscan: default:` — 새 enum 값이 조용히 히트스캔이 된다 ★

**현재** — `EPCombatComponent.cpp:71-73`
```cpp
switch (EquippedWeapon->WeaponDef->BallisticType)
{
case EEPBallisticType::Hitscan:
default:                          // ★
```
`EEPBallisticType`에 `Melee`를 추가하면 **도끼가 히트스캔으로 발사된다.** `default:`가 있어 `-Wswitch` 경고도 안 뜬다. **컴파일러가 아무 말도 안 한다.**

**바꿀 것** — `default:`를 `Hitscan`에서 떼고 별도 분기(또는 `checkNoEntry()`)로. 새 값 추가 시 컴파일러가 잡게 한다.

**지금 안 하는 이유** — 없다. **두 줄이고 지금이 제일 싸다.** 미룬 이유는 순전히 우선순위.

**트리거** — **근접·투척 무기를 추가하기 전에 반드시.** 추가하는 사람은 그게 히트스캔으로 흐른다는 걸 모른 채 추가하게 된다.

> 재작업 검토서 C-8(*"선언은 했는데 아무 일도 일어나지 않는 코드"*)의 사촌 — **"새 값을 추가했는데 아무도 안 알려주는 코드"**.

---

## B-4. 같은 enum을 서버·클라가 따로 본다

**현재**
```
서버   EPCombatComponent.cpp:70        switch (BallisticType)      → 판정
클라   EPGA_Item_PrimaryUse.cpp:66     if (== ProjectileFast)      → 연출
```
새 값이 생기면 양쪽 다 고쳐야 하는데 **한쪽만 고쳐도 컴파일된다.** 증상도 다르다 — 클라는 조용히 연출 없음(무증상), 서버는 오작동.

**바꿀 것** — 판정을 한 곳에서 하고 양쪽이 그 결과를 본다.

**지금 안 하는 이유** — 값이 셋이고 실제로 갈리는 건 둘뿐이라 아직 안 터진다.

**트리거** — B-3과 같이. 또는 `BallisticType`에 값이 하나라도 추가될 때.

**근거** — CLAUDE.md §2: *"한 값을 두 경로가 봐야 하면 둘 다 볼 수 있는 곳에 둔다 (호출자 단위 필드로 두면 갈린다)"*

---

## B-5. `GetEquippedWeapon()`이 "장착된 것"의 진실 노릇을 하고 있다

**현재** — `UEPCombatComponent::EquippedWeapon`(`AEPWeapon*`)이 유일한 소스라 진실처럼 보인다. **붕대는 `AEPWeapon`이 아니라 여기 못 들어온다.**

**바꿀 것** — 진실은 `EquippedEntryId`(int32). 무기 액터는 **파생값**.

**지금 안 하는 이유** — 아직 장착 가능한 게 무기뿐이다.

**트리거** — **Step 05.** 이미 그 방향으로 설계돼 있다:
- `LOOT_STATUS.md` 장비 슬롯 결정 — *"`EquippedEntryId` / `EquippedBackpackEntryId` 필드 둘"*
- `05_Loot_05_Equipment.md:119` — `Inv->GetEquippedEntryId()`

> **★ Step 05에서 지킬 것:** 새 코드의 진입점으로 `GetEquippedWeapon()`을 쓰지 않는다.
> `GetEquippedEntryId()`를 먼저 만들고 무기 쪽이 그걸 통해 액터를 찾게 하면 B-7이 거의 공짜가 된다.

---

## B-6. 소모품 사용 경로가 없다

**현재** — `GA_Item_PrimaryUse`가 `Weapon && Weapon->CanFire()`를 요구한다(`:85`). `HandleServerFire:61`도 `WeaponDef`가 없으면 즉시 return. **`UEPWeaponDefinition`이 없는 아이템은 이 어빌리티를 못 탄다.**

**바꿀 것** — `GA_Consume(EntryId)`. `UEPInventoryComponent`만 보고 **`CombatComponent`를 거치지 않는다.** 손에 드는 액터를 만들지 않고 몽타주로 처리.

**지금 안 하는 이유** — 인벤토리(Step 03)가 없으면 `EntryId`가 없다.

**트리거** — 붕대·구급상자 사용 구현 시. **이미 예정돼 있다:**
- 자리는 잡혀 있음: `UEPItemDefinition::GrantedAbility` (`EPItemDefinition.h:38`)
- `05_Loot_DOCS.md:561` — *"루트 테이블에 붕대·회복키트가 들어가는데 사용할 방법이 없다. 구현하지는 않되 자리는 잡는다"*
- `05_Loot_00_ItemCore.md:889` — *"`GrantedAbility` 실제 사용 → 소모품 구현 시점"*
- `GAME.md:77` 붕대가 루트 일반 50%, `:156` 1칸, `:171` 사용 횟수

**판정선** — *손에 든 것이 **지속적인 상태**를 갖는가?*
총(잔탄·부착물·조준) → 액터 필요 / 붕대(상태는 `Entry.State.Charges`가 이미 가짐) → **액터 불필요**

---

## B-7. `AEPEquippable` 베이스 추출 — 조건부

**바꿀 것** — `AEPWeapon`의 부모로 "손에 드는 액터"의 공통 조상을 뽑는다.

**지금 안 하는 이유** — 파생이 `AEPWeapon` 하나뿐이다. CLAUDE.md §2가 정확히 금지: *"두 번째 구현자가 없는 인터페이스·베이스 클래스"*.

**트리거 (둘 다 충족될 때)** — 손에 드는 **비무기** 액터가 **둘 이상** 필요해질 때. 예: 수류탄(궤적 표시) + 근접무기(히트박스).

**★ 그때도 `BallisticType` 분기는 없어지지 않는다.** 총기 어빌리티 안으로 한 단계 들어갈 뿐이다.
```
GrantedAbility로 아이템이 자기 어빌리티를 들고 온다
  ├ GA_Item_PrimaryUse (총) → 안에서 BallisticType 분기   ← 여전히 유효
  ├ GA_Melee / GA_Throw
  └ GA_Consume  (B-6)
```

---

## 우선순위

| | 항목 | 언제 |
|---|---|---|
| 1 | **B-3** `default:` 두 줄 | **지금이 제일 싸다.** 근접·투척 추가 전 필수 |
| 2 | **B-1** FX → `WeaponDefinition` | Step 05에 얹으면 거의 공짜 |
| 3 | **B-5** `EquippedEntryId` 우선 | Step 05. 안 지키면 B-7이 비싸진다 |
| 4 | B-6 `GA_Consume` | 붕대 구현 시 |
| 5 | B-2 Cue 전환 | B-1 뒤, 여유 있으면 |
| 6 | B-4 / B-7 | 트리거가 오면 |

**B-1·B-3·B-5는 Step 05 안에서 대부분 흡수된다.** 별도 일정이 필요한 건 사실상 B-2뿐이다.
