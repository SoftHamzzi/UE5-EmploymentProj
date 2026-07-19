# GAS 스킬 시스템 — Dash / Heal / ShieldOn

> 캐릭터 고유 스킬 3종. 무기 어빌리티(PrimaryUse, Reload)와 달리 장착 무기와 무관하게 항상 보유.

**GAME.md 스펙 기준:**

| 스킬 | 효과 | 쿨타임 |
|------|------|--------|
| Dash | 이동 방향으로 즉시 대시 (트레이서형 — 쿨타임만 있음) | 10초 |
| Heal | 3초 채널링 → HP 30 회복. 채널링 중 피격 시 취소 (쿨타임 없음) | 20초 (성공 시만) |
| Shield | 5초간 피해 50% 감소 | 30초 |

> **스태미나 시스템 폐기 (2026-07 설계 변경):** 달리기·점프는 자원 제한 없이 자유롭게 사용한다.
> 스프린트는 기존 CMC saved-move 경로(`bWantsToSprint`), 점프는 엔진 기본 경로를 그대로 쓰며 **GAS가 관여하지 않는다.**
> 이 변경으로 이미 코드에 들어간 Stamina 잔재를 제거해야 한다 (Step 1).
> 스펙 문서 동기화 완료: GAME.md는 원래 스태미나 언급 없음(수정 불필요), `DOCS.md`·`04_GAS_DOCS.md`는 갱신됨.

완료 기준: 3종 GA 활성화, 채널링 피격 취소, 쿨타임 GE 복제가 PIE 2인 멀티에서 정상 동작. 스태미나 잔재 제거 후 빌드·기존 기능(스프린트/점프/데미지 파이프라인) 이상 없음.

---

## 1. 아키텍처 요약

```
입력 (Q/E/F)
  → ASC->TryActivateAbilitiesByTag(TAG_Ability_Skill_*)
  → GA_* (LocalPredicted, InstancedPerActor)
      Dash    → LaunchCharacter + GE_Dash_Cooldown (10s) → 즉시 종료
      Heal    → GE_Healing (State.Healing, 3s) + WaitDelay(3s) ∥ WaitGameplayEvent(Event.Damaged)
                  └→ 완료: GE_Heal (+30 HP) + GE_Heal_Cooldown (20s)
                  └→ 피격 취소: 쿨타임 없이 종료
      ShieldOn → GE_ShieldOn (State.Shielded, 5s) + GE_Shield_Cooldown (30s) → 즉시 종료
                  └→ PostGEExecute에서 State.Shielded 확인 시 IncomingDamage * 0.5
```

- 스킬 GA는 `PossessedBy`의 DefaultAbilities 루프로 부여 (기존 구현).
- 스프린트/점프는 이 문서 범위 밖 — 기존 `Input_StartSprint`/`Input_StopSprint`(CMC 플래그 직접 조작, `bWantsToAim` 가드 포함)와 엔진 점프를 **수정하지 않는다.**

---

## 2. 현재 코드 상태 (2026-07 기준)

이미 구현 완료 (유지):
- `PostGameplayEffectExecute`: Shield 50% 감산 + Event.Damaged 발송 ✔
- `PreAttributeChange`: Health 상한 클램프 ✔ (GE_Heal의 MaxHealth 초과 방지 — Heal에 필수, 유지)
- NativeGameplayTags: Skill/Cooldown/Event.Damaged/Data.HealAmount 태그 ✔
- `EPGA_Skill_Dash` 생성 ✔ (단, `SetAssetTags` 누락 — Step 2에서 수정)

제거 대상 (스태미나 폐기로 잔재가 된 코드):
- `UEPAttributeSet`: Stamina/MaxStamina 전체 (UPROPERTY, OnRep, DOREPLIFETIME, 클램프)
- `EPCharacter.cpp` PossessedBy: `InitStamina`/`InitMaxStamina`
- `EPCharacter.h`: `GE_SprintDrainClass`, `GE_StaminaRegenClass`, `SprintDrainHandle`, `#include "ActiveGameplayEffectHandle.h"`
- `TAG_State_Dashing`: 아무도 부여하지 않는 죽은 태그

미구현: `EPGA_Skill_Heal`, `EPGA_Skill_ShieldOn`, GE 에셋 6종, 스킬 입력 3종

---

## 3. 변경 대상 파일

| 파일 | 작업 |
|------|------|
| `EPAttributeSet.h/.cpp` | Stamina/MaxStamina 제거 |
| `EPCharacter.h/.cpp` | 스태미나 잔재 제거, 스킬 입력 3종 추가 |
| `EPNativeGameplayTags.h/.cpp` | `Data.Duration` 추가, `State.Dashing` 제거 |
| `EPGA_Skill_Dash.cpp` | SetAssetTags 추가, State.Dashing 참조 제거 |
| `EPGA_Skill_Heal.h/.cpp` | 신규 |
| `EPGA_Skill_ShieldOn.h/.cpp` | 신규 |
| `EPPlayerController.h` | Dash/Heal/Shield InputAction 슬롯 추가 |

---

## 4. 구현 순서

### Step 0 — NativeGameplayTags 정리

추가 (`EPNativeGameplayTags.h/.cpp`):
```cpp
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Duration)
```
```cpp
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Duration,  "Data.Duration")
```

제거: `TAG_State_Dashing` (h/cpp 양쪽 + Dash 생성자의 참조).

> `Data.Duration`은 채널링/실드 지속시간용. `Data.Cooldown`을 지속시간에 겸용하면 GE 에셋을 읽을 때 의미가 섞이므로 분리.

---

### Step 1 — 스태미나 잔재 제거

**`EPAttributeSet.h`:**
- `Stamina` / `MaxStamina` UPROPERTY + `ATTRIBUTE_ACCESSORS` 2쌍 제거
- `OnRep_Stamina` / `OnRep_MaxStamina` 선언 제거

**`EPAttributeSet.cpp`:**
- `PreAttributeChange`의 Stamina 클램프 블록 제거 (**Health 상한 클램프는 유지** — GE_Heal이 사용)
- `GetLifetimeReplicatedProps`의 `DOREPLIFETIME_CONDITION_NOTIFY(... Stamina/MaxStamina ...)` 2줄 제거
- `OnRep_Stamina` / `OnRep_MaxStamina` 구현 제거

**`EPCharacter.cpp` — `PossessedBy`:**
- `AS->InitStamina(100.f);` / `AS->InitMaxStamina(100.f);` 제거

**`EPCharacter.h`:**
- `GE_SprintDrainClass`, `GE_StaminaRegenClass` UPROPERTY 제거
- `SprintDrainHandle` 멤버 제거
- `#include "ActiveGameplayEffectHandle.h"` 제거 (핸들 제거로 불필요)

> 제거 후 빌드하여 참조 잔재가 없는지 확인. `Input_StartSprint`/`Input_StopSprint`/`bWantsToSprint`/점프 관련 코드는 건드리지 않는다.

---

### Step 2 — GA_Skill_Dash 마무리 (구현 완료분 수정)

`EPGA_Skill_Dash.cpp` 생성자:
```cpp
// 추가 — 이것이 없으면 TryActivateAbilitiesByTag가 이 어빌리티를 찾지 못한다
FGameplayTagContainer Tags = GetAssetTags();
Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Dash);
SetAssetTags(Tags);

// 제거 — State.Dashing은 아무도 부여하지 않는 죽은 태그 (Dash는 즉발 종료라 쿨다운 태그로 충분)
// ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dashing);
```

**대시 방향/발사 수정** (`ActivateAbility`):
```cpp
// 방향: GetLastMovementInputVector는 로컬 전용이라 서버에서 Zero → 서버만 전방 대시되는 버그.
// CMC Acceleration은 saved move로 서버에 복제되므로 클라/서버가 같은 방향을 읽는다.
UCharacterMovementComponent* CMC = Char->GetCharacterMovement();
FVector DashDir = CMC ? CMC->GetCurrentAcceleration().GetSafeNormal2D() : FVector::ZeroVector;
if (DashDir.IsNearlyZero())
    DashDir = Char->GetActorForwardVector().GetSafeNormal2D();

// 지상 마찰 대책: 수평 발사만 하면 즉시 착지 → GroundFriction/Braking이 속도를 잡아먹음.
// Z 부스트로 잠깐 체공시켜 수평 속도 유지 (BrakingDecelerationFalling = 0).
FVector LaunchVel = DashDir * DashImpulse;
LaunchVel.Z = DashZBoost;
Char->LaunchCharacter(LaunchVel, true, true);   // bZOverride=true
```

헤더에 추가:
```cpp
UPROPERTY(EditDefaultsOnly, Category = "Dash")
float DashZBoost = 250.f;   // 150~300 사이 튜닝
```

> **알려진 한계 (의도적 수용):** `LaunchCharacter`는 클라 예측과 서버 실행이 각각 발사되므로 실제 네트워크 지연에서는 서버 보정 러버밴딩이 발생할 수 있다. 실무 정석은 Root Motion Source(`AbilityTask_ApplyRootMotionConstantForce`) — 포트폴리오 범위에서는 현 구현을 유지하되 이 한계를 인지하고 있음을 문서화한다.

---

### Step 3 — GE 에셋 생성

폴더: `Content/Blueprints/GE/Skill/`

| 에셋 | Duration | 설정 |
|------|----------|------|
| `GE_Dash_Cooldown` | HasDuration | SetByCaller(`Data.Cooldown`), GrantedTags: `Cooldown.Skill.Dash` |
| `GE_Healing` | HasDuration | SetByCaller(`Data.Duration`), GrantedTags: `State.Healing` |
| `GE_Heal` | Instant | Modifier: `Health` Add SetByCaller(`Data.HealAmount`) |
| `GE_Heal_Cooldown` | HasDuration | SetByCaller(`Data.Cooldown`), GrantedTags: `Cooldown.Skill.Heal` |
| `GE_ShieldOn` | HasDuration | SetByCaller(`Data.Duration`), GrantedTags: `State.Shielded`. **Modifier 없음** |
| `GE_Shield_Cooldown` | HasDuration | SetByCaller(`Data.Cooldown`), GrantedTags: `Cooldown.Skill.Shield` |

> UE5.3+에서 GrantedTags는 GE 에디터의 **컴포넌트** 섹션에서 추가한다: `Target Tags Gameplay Effect Component` (Grant Tags to Target Actor).
> `GE_ShieldOn`의 50% 감소는 Modifier가 아니라 PostGEExecute의 태그 확인으로 처리 (구현 완료).

---

### Step 4 — GA_Skill_Heal (채널링)

`EPGA_Skill_Heal.h`:
```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Skill_Heal : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UEPGA_Skill_Heal();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_HealingClass;        // 채널링 상태 (State.Healing)

    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_HealClass;           // 완료 시 HP 회복

    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_HealCooldownClass;

    UPROPERTY(EditDefaultsOnly, Category = "Heal")
    float HealAmount = 30.f;

    UPROPERTY(EditDefaultsOnly, Category = "Heal")
    float HealDuration = 3.f;

    UPROPERTY(EditDefaultsOnly, Category = "Heal")
    float HealCooldown = 20.f;

private:
    FActiveGameplayEffectHandle HealingEffectHandle;

    UFUNCTION()
    void OnHealComplete();

    UFUNCTION()
    void OnDamageTaken(FGameplayEventData Payload);
};
```

`EPGA_Skill_Heal.cpp`:
```cpp
UEPGA_Skill_Heal::UEPGA_Skill_Heal()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    FGameplayTagContainer Tags = GetAssetTags();
    Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Heal);
    SetAssetTags(Tags);

    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Healing);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_Cooldown_Skill_Heal);
}

void UEPGA_Skill_Heal::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 채널링 상태 GE
    if (GE_HealingClass)
    {
        FGameplayEffectSpecHandle HealingSpec = MakeOutgoingGameplayEffectSpec(GE_HealingClass);
        HealingSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Duration, HealDuration);
        HealingEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, HealingSpec);
    }

    // 3초 대기 → 완료
    UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, HealDuration);
    WaitTask->OnFinish.AddDynamic(this, &UEPGA_Skill_Heal::OnHealComplete);
    WaitTask->ReadyForActivation();

    // 피격 이벤트 → 취소 (Event.Damaged는 서버 PostGEExecute에서만 발송됨
    //  → 서버 인스턴스가 취소하고 클라로 복제)
    UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this, EmpGameplayTags::TAG_Event_Damaged, nullptr, false, true);
    EventTask->EventReceived.AddDynamic(this, &UEPGA_Skill_Heal::OnDamageTaken);
    EventTask->ReadyForActivation();
}

void UEPGA_Skill_Heal::OnHealComplete()
{
    // 태스크 콜백에는 Handle/ActorInfo 파라미터가 없다 — InstancedPerActor의 Current* 멤버 사용
    if (GE_HealClass)
    {
        FGameplayEffectSpecHandle HealSpec = MakeOutgoingGameplayEffectSpec(GE_HealClass);
        HealSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_HealAmount, HealAmount);
        ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HealSpec);
    }
    if (GE_HealCooldownClass)
    {
        FGameplayEffectSpecHandle CDSpec = MakeOutgoingGameplayEffectSpec(GE_HealCooldownClass);
        CDSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, HealCooldown);
        ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, CDSpec);
    }
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UEPGA_Skill_Heal::OnDamageTaken(FGameplayEventData Payload)
{
    // 피격 취소 — 쿨타임 없음 (쿨타임은 OnHealComplete 경로에서만)
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UEPGA_Skill_Heal::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 취소 시 State.Healing 즉시 해제 (완료 시엔 이미 만료됐으므로 no-op)
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        if (HealingEffectHandle.IsValid())
        {
            ASC->RemoveActiveGameplayEffect(HealingEffectHandle);
            HealingEffectHandle.Invalidate();
        }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
```

> WaitDelay ∥ WaitGameplayEvent 병렬 실행 → 먼저 발동한 쪽이 EndAbility → Super::EndAbility가 나머지 태스크를 자동 정리.

---

### Step 5 — GA_Skill_ShieldOn

`EPGA_Skill_ShieldOn.h`:
```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Skill_ShieldOn : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UEPGA_Skill_ShieldOn();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_ShieldOnClass;

    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_ShieldCooldownClass;

    UPROPERTY(EditDefaultsOnly, Category = "Shield")
    float ShieldDuration = 5.f;

    UPROPERTY(EditDefaultsOnly, Category = "Shield")
    float ShieldCooldown = 30.f;
};
```

`EPGA_Skill_ShieldOn.cpp`:
```cpp
UEPGA_Skill_ShieldOn::UEPGA_Skill_ShieldOn()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    FGameplayTagContainer Tags = GetAssetTags();
    Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Shield);
    SetAssetTags(Tags);

    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Shielded);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_Cooldown_Skill_Shield);
}

void UEPGA_Skill_ShieldOn::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // State.Shielded 부여 — PostGEExecute에서 이 태그 확인 후 피해 50% 감소 (구현 완료)
    if (GE_ShieldOnClass)
    {
        FGameplayEffectSpecHandle ShieldSpec = MakeOutgoingGameplayEffectSpec(GE_ShieldOnClass);
        ShieldSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Duration, ShieldDuration);
        ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, ShieldSpec);
    }

    if (GE_ShieldCooldownClass)
    {
        FGameplayEffectSpecHandle CDSpec = MakeOutgoingGameplayEffectSpec(GE_ShieldCooldownClass);
        CDSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, ShieldCooldown);
        ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CDSpec);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
```

---

### Step 6 — 입력 연동 (스킬 3종만)

기존 스프린트 입력(`Input_StartSprint`/`Input_StopSprint`)은 **수정하지 않는다.**

`EPCharacter.h` 선언 + `EPCharacter.cpp` 구현:
```cpp
void AEPCharacter::Input_Dash(const FInputActionValue& Value)
{
    if (ASC)
        ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Skill_Dash));
}
// Input_Heal / Input_Shield 동일 패턴 (TAG_Ability_Skill_Heal / TAG_Ability_Skill_Shield)
```

`SetupPlayerInputComponent`에 Triggered 바인딩 추가:
```cpp
if (PC->GetDashAction())
    EnhancedInput->BindAction(PC->GetDashAction(),   ETriggerEvent::Triggered, this, &AEPCharacter::Input_Dash);
if (PC->GetHealAction())
    EnhancedInput->BindAction(PC->GetHealAction(),   ETriggerEvent::Triggered, this, &AEPCharacter::Input_Heal);
if (PC->GetShieldAction())
    EnhancedInput->BindAction(PC->GetShieldAction(), ETriggerEvent::Triggered, this, &AEPCharacter::Input_Shield);
```

`EPPlayerController.h`에 DashAction/HealAction/ShieldAction UPROPERTY + 게터 추가 (기존 SprintAction 패턴 동일).

---

### Step 7 — 에디터 설정

1. `IA_Dash`, `IA_Heal`, `IA_Shield` InputAction 생성 (Boolean), IMC에 키 매핑 (예: Q=Dash, E=Heal, F=Shield)
2. `BP_PlayerController`에 액션 슬롯 연결
3. `BP_GA_Skill_Dash/Heal/ShieldOn` 생성 (부모: 각 C++ 클래스), GE 슬롯 연결
4. `BP_EPCharacter` → DefaultAbilities에 스킬 3종 추가

---

## 5. 완료 체크리스트

### Step 0 — 태그
- [x] Skill/Cooldown/Event.Damaged/Data.HealAmount 태그 (완료)
- [ ] `Data.Duration` 추가
- [ ] `TAG_State_Dashing` 제거 (h/cpp + Dash 생성자 참조)

### Step 1 — 스태미나 잔재 제거
- [ ] `EPAttributeSet.h/.cpp`: Stamina/MaxStamina 전체 제거 (UPROPERTY, 접근자, OnRep, DOREPLIFETIME, 클램프)
- [ ] `EPCharacter.cpp`: InitStamina/InitMaxStamina 제거
- [ ] `EPCharacter.h`: GE_SprintDrainClass/GE_StaminaRegenClass/SprintDrainHandle/불필요 include 제거
- [ ] 제거 후 빌드 확인 (Health 클램프, Shield 감산, Event.Damaged는 유지)

### Step 2~5 — GA C++
- [x] `EPGA_Skill_Dash` 골격 (완료)
- [ ] `EPGA_Skill_Dash`: SetAssetTags 추가, State.Dashing 참조 제거
- [ ] `EPGA_Skill_Heal` (콜백은 Current* 멤버 사용)
- [ ] `EPGA_Skill_ShieldOn`

### Step 3 — GE 에셋 (`GE/Skill/`)
- [ ] GE_Dash_Cooldown / GE_Healing / GE_Heal / GE_Heal_Cooldown / GE_ShieldOn / GE_Shield_Cooldown

### Step 6~7 — 입력/에디터
- [ ] Input_Dash/Heal/Shield 함수 + 바인딩 (스프린트 입력은 무변경)
- [ ] PlayerController 액션 슬롯 3종
- [ ] IA 3종 + IMC 매핑, BP_GA 3종, DefaultAbilities 등록

### 검증 (PIE 2인 멀티)
- [ ] 스태미나 제거 후: 스프린트/점프 기존 동작 그대로, 데미지 파이프라인 이상 없음
- [ ] Dash: 이동 방향 대시, 10초 쿨타임 GE 복제
- [ ] Heal: 3초 후 HP +30 (**MaxHealth 초과 안 함**), 20초 쿨타임
- [ ] Heal 취소: 채널링 중 피격 → State.Healing 즉시 해제, 쿨타임 미적용
- [ ] Shield: State.Shielded 복제, 피격 데미지 절반, 5초 후 자동 해제
- [ ] `showdebug abilitysystem`으로 태그/GE/쿨타임 확인

---

## 6. 함정 & 주의사항

| 상황 | 원인 | 해결 |
|------|------|------|
| TryActivateAbilitiesByTag가 GA를 못 찾음 | 생성자에서 `SetAssetTags` 누락 (현재 Dash가 이 상태) | 각 GA 생성자에서 AssetTags에 Ability 태그 추가 |
| 스태미나 제거 후 컴파일 에러 | Stamina 접근자/OnRep/Init 참조 잔재 | Step 1 목록 순서대로 제거 후 빌드 |
| Heal로 HP가 100을 넘음 | PreAttributeChange의 Health 상한 클램프를 실수로 함께 제거 | Health 클램프는 유지 (Stamina 클램프만 제거) |
| 채널링 중 피격해도 취소 안 됨 | Event.Damaged를 PostGEExecute에서 발송 안 함 | 기존 구현 유지 확인 |
| 피격 취소 시 쿨타임이 걸림 | OnDamageTaken에서 쿨타임 GE 적용 | 쿨타임은 OnHealComplete 경로에서만 |
| 힐 완료 후 State.Healing 잔류 | EndAbility에서 HealingEffectHandle 제거 누락 | EndAbility override 확인 |
| 태스크 콜백에서 컴파일 에러 | 콜백엔 Handle/ActorInfo 파라미터가 없음 | `CurrentSpecHandle`/`CurrentActorInfo`/`CurrentActivationInfo` 사용 |
| Dash가 서버에서만 전방으로 감 (러버밴딩) | GetLastMovementInputVector는 로컬 전용 — 서버에선 Zero | `CMC->GetCurrentAcceleration()` 사용 (saved move로 복제됨) |
| 지상 Dash가 거의 안 나감 | 수평 발사 → 즉시 착지 → GroundFriction/Braking이 감속 | Z 부스트(DashZBoost)로 잠깐 체공, bZOverride=true |
| DefaultAbilities 중복 부여 | PossessedBy 재호출 | 현 프로젝트는 재빙의 없음 — 도입 시 가드 추가 |

---

## 7. 향후 확장 — 스킬 슬롯 + 키 리바인딩

> 07 단계 범위 밖. 캐릭터 세팅에서 슬롯별 스킬 배정, 게임 설정에서 키 변경을 지원하기 위한 방향.
> **현 구조가 마이그레이션을 막지 않는다** — GA 클래스/GE 에셋/채널링·쿨타임 로직은 전부 그대로 재사용.

두 요구사항은 별개 계층으로 분리:

### 7-1. 키 리바인딩 (Q → T) — Enhanced Input 계층

스킬과 무관한 순수 입력 문제. InputAction은 고정하고 키 매핑만 유저 세팅으로 교체:
- IMC 매핑에 **Player Mappable Key Settings** 부여 (매핑 이름 예: `"Skill1"`)
- 설정 UI에서 `UEnhancedInputUserSettings::MapPlayerKey()` → `SaveSettings()`
- IA/게임 코드 무변경. UE5.3+ 표준 경로.

### 7-2. 슬롯에 스킬 배정 (Lyra 방식 동적 슬롯 태그)

현재는 키↔스킬 하드코딩 (`Input_Dash` → `Ability.Skill.Dash`). 슬롯화하려면 **부여 시점에 슬롯 태그를 스펙에 동적으로 추가**:

```cpp
// 태그: InputTag.Skill.Slot1 / Slot2 / Slot3 (신규)

// 로드아웃 데이터 (DataAsset 또는 PlayerState): Slot1 → GA_Skill_Dash ...
FGameplayAbilitySpec Spec(AbilityClass, 1);
Spec.GetDynamicSpecSourceTags().AddTag(TAG_InputTag_Skill_Slot1);
ASC->GiveAbility(Spec);

// 입력은 슬롯 태그만 안다
void Input_Skill1()
{
    ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(TAG_InputTag_Skill_Slot1));
}
```

`TryActivateAbilitiesByTag`는 AssetTags 외에 스펙의 `DynamicSpecSourceTags`도 검색하므로, GA 클래스는 자기 슬롯을 몰라도 된다. 스킬 교체 = 스펙 제거 후 새 클래스로 재부여.

```
[유저 키 설정]     Q ↔ T                    ← EnhancedInputUserSettings
[InputAction]      IA_Skill1                 ← 고정
[슬롯 태그]        InputTag.Skill.Slot1      ← 고정
[로드아웃 데이터]  Slot1 = Dash              ← 캐릭터 세팅 화면에서 변경 (서버 권위)
[GA]               GA_Skill_Dash
```

### 전환 시 변경 지점 (딱 2곳)

1. `PossessedBy` 부여 루프: `DefaultAbilities` 배열 → 로드아웃 데이터 + 슬롯 태그 부여
2. 입력 핸들러: `Input_Dash/Heal/Shield` 3개 → `Input_Skill1/2/3` 통합

로드아웃은 장비/인벤토리와 같은 서버 권위 데이터 계층 — 인벤토리 단계(로드맵 11번)와 묶어 "매치 시작 전 로비 세팅"으로 설계하는 것이 자연스럽다.
