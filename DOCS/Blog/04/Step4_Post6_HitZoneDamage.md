# Post 4-6 작성 가이드 — 부위별 데미지: bool 플래그에서 GameplayTag로

> **예상 제목**: `[UE5] 추출 슈터 4-6. 부위별 데미지 태그화: bIsWeakSpot을 버리고 HitZone 태그로`
> **참고 문서**: `DOCS/Notes/04/04_GAS_06_HitZoneDamage.md`, `04_GAS_06_HitZoneDamage_STATUS.md`

---

## 개요

**이 포스팅에서 다루는 것:**
- `bIsWeakSpot` bool + 본 이름 문자열 매칭의 한계 — **그리고 그 본 이름 배율이 사실 작동한 적이 없었다는 발견**
- 부위 정의를 PhysicalMaterial에, 배율을 무기에 나눠 두는 이유
- 두 함수를 하나로 합치며 배율 계산이 어떻게 단순해졌는가

**왜 이렇게 구현했는가 (설계 의도):**
- 3단계에서 만든 본 단위 히트박스는 **어느 본을 맞았는지**까지만 알려준다. 그걸 배율로 바꾸는 부분은 임시 구현이었다
- **부위는 캐릭터의 성질이고, 배율은 무기의 성질이다.** 이 둘이 한 자료구조에 섞여 있던 게 문제였다
- 이 편은 짧지만 **"데이터를 코드에서 에셋으로" 주제가 가장 압축적으로 드러나는 단계**다

---

## 구현 전 상태 (Before)

배율 계산이 **두 갈래**로 나뉘어 있었다.

```cpp
// ① 본 이름 문자열로 배율 조회
float UEPCombatComponent::GetBoneMultiplier(const FName& BoneName) const;
// UEPWeaponDefinition::BoneDamageMultiplierMap — TMap<FName, float>
//   "head" → 2.0, "thigh_l" → 0.75 ...

// ② PhysicalMaterial의 bool 플래그로 약점 판정
static float UEPCombatComponent::GetMaterialMultiplier(const UPhysicalMaterial* PM);
// UEPPhysicalMaterial::bIsWeakSpot / WeakSpotMultiplier

// 최종
const float FinalDamage = BaseDamage * GetBoneMultiplier(Hit.BoneName)
                                     * GetMaterialMultiplier(Hit.PhysMaterial.Get());
```

**문제점:**

| 문제 | 설명 |
|------|------|
| bool은 2단계뿐 | "약점이냐 아니냐". 머리 2.5배 / 팔다리 0.75배 같은 **3단계 이상을 표현할 수 없다** |
| 배율의 소유자가 애매하다 | `BoneDamageMultiplierMap`은 무기에 있는데, 키가 캐릭터의 본 이름이다 |
| 본 이름은 스켈레톤 종속 | `"thigh_l"`은 UE5 마네킹 규약이다. 다른 스켈레톤을 쓰는 적을 넣으면 그대로는 못 쓴다 |

두 번째가 핵심이다. **무기 데이터가 캐릭터의 뼈 이름을 알아야 하는 구조**였다.

### ★ 그런데 ①은 애초에 작동한 적이 없었다

이 편을 쓰려고 Before 코드를 다시 열었을 때 발견한 것이다. 선언이 이랬다.

```cpp
// EPWeaponDefinition.h — 커밋 158e8b1~1 기준
// 부위별 대미지(GAS 이후 태그 기반으로 수정)
TMap<FName, float> BoneDamageMultiplierMap;   // ← UPROPERTY가 없다
```

`UPROPERTY`가 없으니 **에디터에 뜨지 않고, DataAsset에 직렬화되지도 않는다.** 그리고 저장소 전체를 뒤져도 이 맵에 값을 **넣는 코드가 한 줄도 없다** — 읽는 곳(`GetBoneMultiplier`)만 있다. 문서에만 *"C++ 또는 DataAsset 로직으로 채운다"*고 적혀 있고 채우는 코드는 끝내 없었다.

즉 `GetBoneMultiplier`는 **항상 1.0을 반환하고 있었다.** 그래서:

| 원래 적었던 문제 | 실제 |
|---|---|
| "본 이름 배율과 약점 플래그가 같은 히트에 둘 다 곱해진다 — 중복" | **일어난 적 없다.** 본 이름 쪽이 늘 1.0이라 곱해봐야 그대로다 |
| "다른 스켈레톤을 넣으면 매칭이 전부 깨진다" | 깨질 매칭 자체가 없었다. 설계상의 문제로는 유효하지만 **겪은 문제는 아니다** |
| 3단계 결과 로그의 "팔 26.25 (35 × 0.75)" | **나올 수 없는 값이다.** 부위 배율이 실제로 붙은 적이 없다 |

헤드샷이 제대로 아팠던 건 순전히 `UEPPhysicalMaterial`의 약점 배율 덕이었다. **다른 경로가 그럴듯한 결과를 만들어주는 바람에 끝까지 몰랐다.**

> **이게 3단계 시리즈에서 반복된 그 패턴이다** — `UEPItemInstance`의 복제 미등록, `AEPWeapon::MaxAmmo`의 `GetLifetimeReplicatedProps` 누락과 같은 모양이다.
> **UE에서 `UPROPERTY`는 문법이 아니라 계약이다.** 빠뜨리면 컴파일 에러가 나는 게 아니라 **조용히 없던 일이 된다.** 경고도 없고 크래시도 없다.
>
> 그래서 이 편의 진짜 교훈은 "본 이름 → 태그"가 아니라 **"안 되는 걸 몰랐다"**에 가깝다.
> 태그로 옮기면서 `UPROPERTY(EditDefaultsOnly)`를 붙였고(`EPWeaponDefinition.h:55-56`), 그제서야 **에디터에서 값이 보이기 시작했다.** 값이 보인다는 건 비어 있는 것도 보인다는 뜻이다.

---

## 구현 내용

### 1. ★ 책임을 두 에셋으로 나눈다

```
[PhysicalMaterial]  "이 부위는 머리다"          ← 캐릭터의 성질
        ↓ 태그로 전달
[WeaponDefinition]  "머리를 맞추면 2.5배다"      ← 무기의 성질
```

이 분리가 이 편의 전부다. 그림으로 그리면:

```
PM_Head    ─ MaterialTags: {HitZone.Head}
PM_Chest   ─ MaterialTags: {HitZone.Chest}
PM_Limbs   ─ MaterialTags: {HitZone.Limbs}
PM_Default ─ MaterialTags: {}                  → 1.0x 폴백
                    │
                    │  태그
                    ▼
DA_AK74    ─ TagDamageMultiplierMap: { Head:2.5, Chest:1.0, Limbs:0.75 }
DA_Sniper  ─ TagDamageMultiplierMap: { Head:4.0, Chest:1.0, Limbs:0.60 }
DA_Shotgun ─ TagDamageMultiplierMap: { Head:1.5, Chest:1.0, Limbs:0.90 }
```

**같은 헤드샷이 무기마다 다른 배율을 갖는다.** 저격총 헤드샷은 즉사, 산탄총 헤드샷은 그렇지 않다 — 이건 게임 밸런스의 기본인데 이전 구조로는 표현할 수 없었다.

### 2. `UEPPhysicalMaterial` — bool을 태그 컨테이너로

```cpp
// Before
UPROPERTY(EditDefaultsOnly, Category = "Damage") bool  bIsWeakSpot;
UPROPERTY(EditDefaultsOnly, Category = "Damage") float WeakSpotMultiplier;

// After
UPROPERTY(EditDefaultsOnly, Category = "Damage")
FGameplayTagContainer MaterialTags;
```

**필드가 2개에서 1개로 줄었는데 표현력은 늘었다.** 태그는 계층형이고 개수 제한이 없다.

향후 확장 여지도 열린다 — `MaterialTags`에 `HitZone.Head`뿐 아니라 `Surface.Metal` 같은 걸 같이 넣으면 4-5의 표면별 데칼 분기에도 쓸 수 있다. **하나의 태그 컨테이너가 여러 시스템의 입력이 된다.**

### 3. `UEPWeaponDefinition` — 키를 태그로

```cpp
// Before — 캐릭터의 본 이름이 키
TMap<FName, float> BoneDamageMultiplierMap;

// After — 추상화된 부위 태그가 키
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
TMap<FGameplayTag, float> TagDamageMultiplierMap;
```

이제 무기 데이터는 **스켈레톤을 모른다.** 캐릭터 모델이 바뀌어도, 적이 인간형이 아니어도 무기 에셋은 그대로다.

> 키 타입만 바뀐 게 아니다. **`UPROPERTY`가 붙었다.** 위 §Before에서 봤듯 이전 맵은 `UPROPERTY`가 없어 에디터에도 안 뜨고 직렬화도 안 됐다 — "하드코딩"이 아니라 **아무도 채우지 않는 빈 맵**이었다.
> 태그로 오면서 비로소 DataAsset에서 값이 보이고, 편집되고, 저장된다.

### 4. 두 함수를 하나로

```cpp
// Before — 2개 함수
float GetBoneMultiplier(const FName& BoneName) const;
static float GetMaterialMultiplier(const UPhysicalMaterial* PM);

// After — 1개
static float GetTagDamageMultiplier(const UEPPhysicalMaterial* PM, const UEPWeaponDefinition* WeaponDef);
```

```cpp
float UEPCombatComponent::GetTagDamageMultiplier(
    const UEPPhysicalMaterial* PM, const UEPWeaponDefinition* WeaponDef)
{
    if (!PM || !WeaponDef) return 1.f;

    for (const FGameplayTag& Tag : PM->MaterialTags)
        if (const float* Multiplier = WeaponDef->TagDamageMultiplierMap.Find(Tag))
            return *Multiplier;

    return 1.f;   // 태그 없음 / 매칭 없음 → 폴백
}
```

**함수 전체가 8줄이다.** 이전에는 본 이름 문자열 비교와 bool 분기가 두 함수에 흩어져 있었다.

**폴백 설계가 중요하다** — 태그가 없거나 매칭되지 않으면 조용히 1.0을 반환한다. 새 부위를 추가했는데 무기 맵에 등록하지 않으면 **에러 대신 기본 데미지**가 나간다. 크래시보다 낫고, 밸런스 이슈로는 금방 눈에 띈다.

> **알려진 제약**: 복수 태그가 매칭될 때(예: `HitZone.Head` + `Armor.Heavy`) **첫 번째 매칭만 반환**한다. 현재는 PM 하나에 태그를 하나만 부여하므로 문제가 없지만, 방어구 태그를 도입하면 곱연산으로 바꿔야 한다.

### 5. 호출부 — 4줄이 2줄로

```cpp
// Before
const float BaseDamage         = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
const float BoneMultiplier     = GetBoneMultiplier(Hit.BoneName);
const float MaterialMultiplier = GetMaterialMultiplier(Hit.PhysMaterial.Get());
const float FinalDamage        = BaseDamage * BoneMultiplier * MaterialMultiplier;
UE_LOG(LogTemp, Log, TEXT("[BoneHitbox] ..."));
ApplyGEDamage(Hit.GetActor(), Owner, GE_DamageClass, FinalDamage);

// After
const float BaseDamage    = EquippedWeapon ? EquippedWeapon->GetDamage() : 0.f;
const UEPPhysicalMaterial* PM = Cast<UEPPhysicalMaterial>(Hit.PhysMaterial.Get());
const float FinalDamage   = BaseDamage * GetTagDamageMultiplier(PM, EquippedWeapon->WeaponDef);
ApplyGEDamage(Hit.GetActor(), Owner, GE_DamageClass, FinalDamage);
```

`Hit.PhysMaterial`이 채워지려면 3단계에서 이미 설정해둔 것이 있다:
```cpp
Params.bReturnPhysicalMaterial = true;   // ConfirmHitscan의 FCollisionQueryParams
```
**3단계에서 미리 켜둔 옵션이 여기서 값을 한다.** 당시엔 약점 판정용이었는데 태그 시스템에서 그대로 재사용된다.

### 6. 3단계 자산이 또 그대로 쓰인다

이 편에서 **Physics Asset은 한 곳도 수정하지 않았다.** 각 본에 PhysicalMaterial 에셋만 할당하면 끝난다.

> **에디터 작업** — 캐릭터 PhysAsset에서 각 본 → Simple Collision → Physical Material:
> - `head` → `PM_Head`
> - `spine_04` 또는 `spine_02` → `PM_Chest` (`spine_03`은 3단계 `HitBones` 배열에 없다)
> - `lowerarm_l/r`, `thigh_l/r`, `calf_l/r` → `PM_Limbs`
>
> **스크린샷 위치**: Physics Asset 에디터에서 본 선택 → Physical Material 슬롯

| 3단계에서 만든 것 | 이 편에서 |
|---|---|
| Physics Asset 본별 바디 | **무변경** |
| `EP_TraceChannel_Weapon` | **무변경** |
| `bReturnPhysicalMaterial = true` | **무변경, 여기서 활용** |
| SSR 리와인드 전체 | **무변경** |
| `bIsWeakSpot` 판정 | 태그로 교체 |

---

## 겪은 문제

### 컴파일이 중간에 깨지는 순서 의존

`bIsWeakSpot`을 제거하면 `GetMaterialMultiplier`가 그 필드를 참조하므로 **즉시 컴파일 에러**가 난다. 반대로 함수를 먼저 지우면 호출부가 깨진다.

- **해결**: Step 1(PhysicalMaterial 수정)과 Step 3(함수 교체)을 **한 번에 빌드**한다
- **교훈**: 필드와 그 필드를 읽는 함수를 동시에 교체하는 마이그레이션은 **중간 상태가 존재하지 않는다.** 단계를 나눌 수 없다는 걸 문서에 미리 적어두는 게 낫다

### 태그 이름 통일 — `HitZone.Limb` vs `HitZone.Limbs`

초기에 `TAG_HitZone_Limb`("HitZone.Limb")로 선언했다가 `Limbs`로 통일했다. 단수/복수가 섞이면 나중에 반드시 헷갈린다.

```cpp
// Before
UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Limb,  "HitZone.Limb")
// After
UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Limbs, "HitZone.Limbs")
```

**네이티브 태그의 장점이 여기서 드러난다** — 문자열이었다면 에셋에 박힌 옛 이름이 조용히 매칭 실패했을 것이다. 네이티브 선언이라 **참조하는 코드가 전부 컴파일 에러**로 잡혔다.

---

## 함정 정리

| 상황 | 원인 | 해결 |
|------|------|------|
| Step 1 후 컴파일 오류 | `bIsWeakSpot` 제거 → `GetMaterialMultiplier` 참조 실패 | Step 1 + Step 3 동시 빌드 |
| PhysicalMaterial이 항상 null | PhysAsset 본에 PM 에셋 미할당 | 각 본 → Simple Collision → Physical Material 확인 |
| `Hit.PhysMaterial`이 비어 있음 | 트레이스 파라미터 누락 | `bReturnPhysicalMaterial = true` |
| 태그 키가 에디터에서 안 보임 | GameplayTags 모듈 / 태그 등록 문제 | Build.cs + 네이티브 태그 정의 확인 |
| 헤드샷인데 배율이 1.0 | 무기 맵에 태그 미등록 | `TagDamageMultiplierMap` 확인 (조용히 폴백된다) |
| 복수 태그 매칭 시 하나만 적용 | 구현상 첫 매칭 반환 | 현재는 단일 태그 정책이라 무해. 방어구 도입 시 곱연산으로 |

---

## 결과

**확인 항목 (PIE 2인):**
- `HitZone.Head` 부위 피격 → 기본 대비 2.5배 데미지
- `HitZone.Limbs` 피격 → 0.75배
- PM이 할당되지 않은 부위 피격 → 1.0배 폴백 (에러 없음)
- 무기를 바꿔 같은 부위 사격 → 다른 배율 적용

> **스크린샷 위치**: 부위별 데미지 로그 또는 HP 감소량 비교 (머리/몸통/팔다리 3회 사격)

**한계 및 향후 개선:**
- **부위별 방어구가 아직 없다.** 설계 방향은 정해뒀다 — GE Context에 HitZone 태그를 실어 보내고, `PostGameplayEffectExecute`에서 `ArmorHead`/`ArmorChest`/`ArmorLimbs` Attribute를 조회해 `IncomingDamage`를 감산한다. **무기 배율(`TagDamageMultiplierMap`)과 방어력은 독립 계층으로 적용**해야 곱연산이 꼬이지 않는다
- 현재 배율이 `HandleHitscanFire`에서 곱해져 `GE_Damage`에 최종값으로 들어간다. 방어구를 넣으려면 **배율 적용 지점을 `PostGEExecute`로 옮겨야** 한다 (4-2에서 만든 메타 어트리뷰트 구조가 이걸 위한 것)
- 관통(한 발이 여러 부위 통과) 미지원

---

## 참고

- `DOCS/Notes/04/04_GAS_06_HitZoneDamage.md` — 구현 전체
- `DOCS/Notes/04/04_GAS_06_HitZoneDamage_STATUS.md` — 실제 구현 기록
- `DOCS/Notes/04/04_GAS_DOCS.md` §3 — 부위별 방어력 설계 방향
- Step 3-1 / 3-3 포스팅 — Physics Asset 구성, 그리고 **작동하지 않았던** 원래의 부위 배율 (3-3편 수정본에 같은 사실이 적혀 있다 — 두 글의 서술을 반드시 일치시킬 것)
