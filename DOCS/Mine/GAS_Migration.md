# GAS 이관 우선순위 가이드

기존 기능을 GAS로 이관하는 순서와 각 단계에서 변경할 대상을 정리한 문서.
스킬(Dash/Heal/Shield) 추가는 이관 완료 후 진행.

---

## 우선순위 순서

```
1. 기반 세팅 (ASC + AttributeSet)
2. 데미지/HP 파이프라인
3. 발사 어빌리티 (GA_Item_PrimaryUse)
4. 재장전 어빌리티 (GA_Item_Reload)
5. 스프레드 개선 (SpreadDistributionCurve PDF 룩업 테이블)
```

---

## 1. 기반 세팅

**대상 파일:** `EPPlayerState.h/cpp`, `EPCharacter.h/cpp`

**현재 상태:**
- `AEPPlayerState` — ASC 없음
- `AEPCharacter` — `IAbilitySystemInterface` 미구현, `PossessedBy`/`OnRep_PlayerState` 없음

**변경 내용:**
- `AEPPlayerState`에 `UAbilitySystemComponent` + `UEPAttributeSet` 추가
- `AEPCharacter`에 `IAbilitySystemInterface` 구현 → `GetAbilitySystemComponent()` 반환
- `AEPCharacter::PossessedBy` (서버) + `OnRep_PlayerState` (클라) 에서 `InitAbilityActorInfo(PS, this)` 호출
- `Build.cs`에 `GameplayAbilities`, `GameplayTags`, `GameplayTasks` 추가
- `GameInstance::Init()`에 `UAbilitySystemGlobals::Get().InitGlobalData()` 호출

**완료 기준:** PIE에서 ASC가 정상 초기화되고 AttributeSet의 HP가 복제됨

---

## 2. 데미지 / HP 파이프라인

**대상 파일:** `EPCharacter.h/cpp`, `EPAttributeSet.h/cpp` (신규)

**현재 상태:**
```cpp
// EPCharacter.cpp
float AEPCharacter::TakeDamage(float DamageAmount, ...)
{
    HP = FMath::Clamp(HP - DamageAmount, 0, MaxHP);
    if (HP <= 0) Die(EventInstigator);
}

UPROPERTY(ReplicatedUsing = OnRep_HP)
int32 HP = 100;
int32 MaxHP = 100;
```

**변경 내용:**

`UEPAttributeSet` 신규 구현:
- `Health`, `MaxHealth`, `Stamina`, `Shield` 어트리뷰트 정의
- `IncomingDamage` 메타 어트리뷰트 (복제 안 함, 서버 전용)
- `PreAttributeChange` — 클램핑
- `PostGameplayEffectExecute` — IncomingDamage 처리: Shield 흡수 → HP 감소 → HP≤0이면 `Event.Death` 발송

`AEPCharacter`:
- `HP` / `MaxHP` UPROPERTY 제거 → `UEPAttributeSet::GetHealth()` 사용
- `TakeDamage()` 오버라이드 제거 → SSR `ConfirmHitscan`에서 `GE_Damage` 적용으로 대체
- `IsDead()` → `AttributeSet->GetHealth() <= 0` 기반으로 변경
- `Die()` / `Multicast_Die()` → `WaitGameplayEvent(Event.Death)` 수신으로 전환

**완료 기준:** 피격 → HP AttributeSet 감소 → 0이 되면 사망 처리. `OnRep_HP` 제거 후 UI는 Attribute 변경 델리게이트로 갱신

---

## 3. 발사 어빌리티 (GA_Item_PrimaryUse)

**대상 파일:** `EPCombatComponent.h/cpp`, `EPWeapon.h/cpp`

**현재 상태:**
```
RequestFire() → Server_Fire RPC → 검증 3단계 → BallisticType switch
             → HandleHitscanFire → SSR::ConfirmHitscan → ApplyPointDamage
             → HandleProjectileFire → SpawnActor<AEPProjectile>
```

**변경 내용:**

`GA_Item_PrimaryUse` (C++ 기반 GameplayAbility):
- `NetExecutionPolicy`: `LocalPredicted`
- `ActivationBlockedTags`: `State.Dead`, `State.Reloading`
- `CooldownGameplayEffectClass`: `GE_FireCooldown` → `LastServerFireTime` 수동 관리 제거
- `ActivateAbility` 내부에서 기존 `Server_Fire_Implementation` 로직 수행:
  - Origin drift 검증 (200cm)
  - `BallisticType` switch
  - Hitscan → SSR `ConfirmHitscan` 위임 (그대로 유지)
  - Projectile → `HandleProjectileFire` 로직
  - `Multicast_PlayMuzzleEffect` 호출
- `CommitAbility` 호출로 탄약 GE 적용 + 쿨타임 시작

`AEPWeapon`:
- 장착 시 (`EquipWeapon`) → ASC에 `GA_Item_PrimaryUse` Grant
- 해제 시 (`UnequipWeapon`) → Grant된 Ability Remove

`UEPCombatComponent`:
- `Server_Fire` RPC → 제거 (GA가 대체)
- `LastServerFireTime` → 제거
- `HandleHitscanFire` / `HandleProjectileFire` → GA 내부 함수 또는 CombatComponent 헬퍼로 유지

`AEPCharacter`:
- `Input_Fire` → `ASC->TryActivateAbilitiesByTag(TAG_Ability_Item_PrimaryUse)` 호출

**완료 기준:** 발사 입력 → GA 활성화 → FireRate 쿨타임 GE 동작 → 히트스캔/투사체 판정 정상

---

## 4. 재장전 어빌리티 (GA_Item_Reload)

**대상 파일:** `EPWeapon.h/cpp`, `EPCombatComponent.h/cpp`

**현재 상태:**
```cpp
// EPWeapon.cpp
void AEPWeapon::StartReload() { ... SetTimer → FinishReload }
void AEPWeapon::FinishReload() { CurrentAmmo = MaxAmmo; }

// EPCombatComponent
UFUNCTION(Server, Reliable)
void Server_Reload();
```

**변경 내용:**

`GA_Item_Reload`:
- `NetExecutionPolicy`: `LocalPredicted`
- `ActivationBlockedTags`: `State.Dead`, `State.UsingItem`
- `ActivationOwnedTags`: `State.Reloading` (GA_Item_PrimaryUse의 ActivationBlockedTags에 포함)
- `WaitDelay(ReloadTime)` 후 탄약 보충 Instant GE 적용
- `PlayMontageAndWait` 또는 `WaitDelay`로 재장전 애니 연동

`AEPWeapon`:
- `StartReload` / `FinishReload` / `ReloadTimerHandle` 제거
- 탄약 보충 → `GE_Reload_Ammo` Instant GE (CurrentAmmo → MaxAmmo Override)
- 장착 시 `GA_Item_Reload`도 함께 Grant

`UEPCombatComponent`:
- `Server_Reload` RPC 제거

`AEPCharacter`:
- 재장전 입력 → `ASC->TryActivateAbilitiesByTag(TAG_Ability_Item_Reload)`

**완료 기준:** 재장전 입력 → State.Reloading 태그 부여 → 발사 차단 → 완료 후 탄약 보충

---

## 5. 스프레드 개선 (BuildSpreadCDFTable)

**대상 파일:** `EPWeapon.h/cpp`

**현재 상태:**
```cpp
// Fire() 내부
const float R = WeaponDef->SpreadDistributionCurve
    ? WeaponDef->SpreadDistributionCurve->GetFloatValue(FMath::FRand())
    : FMath::FRand();
// 문제: 커브를 X→Y 방향으로 읽어 단순 리매핑. PDF 확률 분포로 동작하지 않음
```

**변경 내용:**

`EPWeapon.h` 추가:
```cpp
private:
    TArray<float> SpreadCDFTable;
    static constexpr int32 CDFTableSize = 256;
    void BuildSpreadCDFTable();
    float SampleSpread() const; // 이진탐색으로 역CDF 샘플링
```

`EPWeapon.cpp`:
- `BeginPlay`에서 `BuildSpreadCDFTable()` 호출
- `SpreadDistributionCurve`를 PDF(X=반경비율, Y=확률)로 해석 → 적분 → CDF 룩업 테이블 생성
- `Fire()` 내부에서 `R = SampleSpread()`로 교체

**커브 그리기 방식:** X=0(중심) Y=1, X=1(가장자리) Y=0 형태의 PDF로 직관적으로 그리면 됨

**완료 기준:** 산탄총 PelletCount=5 테스트 시 펠릿이 중심부에 밀집되는 것 확인

---

## 3-1. 입력 연동 추가

**대상 파일:** `EPCharacter.cpp`

**현재 상태:**
```cpp
void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
    CombatComponent->RequestFire(...);
}
```

**변경 내용:**
- `Input_Fire` → `ASC->TryActivateAbilitiesByTag(TAG_Ability_Item_PrimaryUse)`
- 재장전 입력 추가 시 → `ASC->TryActivateAbilitiesByTag(TAG_Ability_Item_Reload)`

---

## 6. 부위별 대미지 — 태그 기반으로 전환

**대상 파일:** `EPPhysicalMaterial.h`, `EPWeaponDefinition.h`, `EPCombatComponent.cpp`

**현재 상태:**
```cpp
// EPPhysicalMaterial.h
bool bIsWeakSpot = false;
float WeakSpotMultiplier = 2.0f;
// FGameplayTagContainer MaterialTags; ← 주석 처리됨

// EPWeaponDefinition.h
TMap<FName, float> BoneDamageMultiplierMap; // UPROPERTY 없음, 에디터 노출 X
// 주석: "GAS 이후 태그 기반으로 수정"

// EPCombatComponent.cpp
float GetBoneMultiplier(const FName& BoneName) const;
static float GetMaterialMultiplier(const UPhysicalMaterial* PM);
```

**변경 내용:**

`UEPPhysicalMaterial`:
- `bIsWeakSpot` / `WeakSpotMultiplier` 제거
- `MaterialTags` (FGameplayTagContainer) 주석 해제 + UPROPERTY 추가
- 에디터에서 피격 부위별 태그 설정 (예: `HitZone.Head`, `HitZone.Limb`)

`UEPWeaponDefinition`:
- `BoneDamageMultiplierMap` (`TMap<FName, float>`) 제거
- `TagDamageMultiplierMap` (`TMap<FGameplayTag, float>`) 로 교체 + UPROPERTY 추가
- 에디터에서 태그별 배율 설정 (예: `HitZone.Head` → 2.5, `HitZone.Limb` → 0.75)

`UEPCombatComponent` — 대미지 계산 흐름:
```
FHitResult 획득
    → PhysicalMaterial 캐스트 → UEPPhysicalMaterial::MaterialTags 읽기
    → MaterialTags 순회 → WeaponDef->TagDamageMultiplierMap에서 배율 조회
    → FinalDamage = BaseDamage * Multiplier
    → GE_Damage에 SetByCaller(Data.Damage, FinalDamage)로 전달
```

- `GetBoneMultiplier(FName)` 제거 → 태그 조회로 대체
- `GetMaterialMultiplier(UPhysicalMaterial*)` 제거 → 태그 조회로 대체

**태그 설계 예시:**
```
HitZone.Head     → 2.5x (헤드샷)
HitZone.Chest    → 1.0x (기본)
HitZone.Limb     → 0.75x (팔/다리)
```

**완료 기준:** 머리 피격 시 기본 대비 2.5배 데미지 적용, 태그 없는 부위는 1.0x 폴백

---

## 이관 후 제거 대상 정리

| 현재 코드 | 대체 |
|-----------|------|
| `AEPCharacter::HP` / `MaxHP` UPROPERTY | `UEPAttributeSet::Health` |
| `AEPCharacter::TakeDamage()` | `GE_Damage` → `PostGameplayEffectExecute` |
| `AEPCharacter::Die()` / `Multicast_Die()` | `WaitGameplayEvent(Event.Death)` |
| `AEPCharacter::OnRep_HP()` | Attribute 변경 델리게이트 |
| `UEPCombatComponent::Server_Fire` RPC | `GA_Item_PrimaryUse` |
| `UEPCombatComponent::Server_Reload` RPC | `GA_Item_Reload` |
| `UEPCombatComponent::LastServerFireTime` | `GE_FireCooldown` |
| `AEPWeapon::StartReload` / `FinishReload` | `GA_Item_Reload` + `GE_Reload_Ammo` |
| `AEPWeapon::ReloadTimerHandle` | `WaitDelay` AbilityTask |
| `AEPWeapon::CurrentAmmo` / `MaxAmmo` UPROPERTY + `OnRep_CurrentAmmo` | `AttributeSet::Ammo` / `MaxAmmo` + Attribute 변경 델리게이트 |
| `AEPWeapon::WeaponState` (EEPWeaponState) | GameplayTag (`State.Reloading` 등) |
| `AEPWeapon::LastFireTime` + `CombatComponent::LastServerFireTime` (중복) | `GE_FireCooldown` 하나로 통합 |
| `AEPWeapon::CanFire()` | GAS 조건(ActivationBlockedTags + Cooldown GE)으로 대체 후 제거 |
| `AEPCharacter::Input_Fire` → `RequestFire()` 직접 호출 | `ASC->TryActivateAbilitiesByTag()` |

## 유지할 것

- `UEPServerSideRewindComponent` — 호출 위치만 CombatComponent → GA로 이동
- `UEPCombatComponent` — EquipWeapon/UnequipWeapon, 코스메틱 헬퍼 함수들
- `UEPCombatComponent::HandleHitscanFire` / `HandleProjectileFire` — GA 내부에서 호출하거나 헬퍼로 유지
- `AEPWeapon::Fire()` — 스프레드 계산 + 탄약 차감 로직 (탄약 차감은 GE로 이전 후 제거 고려)
- `EEPBallisticType` switch 구조 — GA 내부로 이동
