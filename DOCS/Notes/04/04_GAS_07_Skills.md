# GAS 스킬 시스템 — Dash / Heal / ShieldOn

> 캐릭터 고유 스킬 3종. 무기 어빌리티(PrimaryUse, Reload)와 달리 장착 무기와 무관하게 항상 보유.

**GAME.md 스펙 기준:**

| 스킬 | 효과 | 쿨타임 |
|------|------|--------|
| Dash | 이동 방향으로 즉시 대시, Stamina 30 소모 | 10초 |
| Heal | 3초 채널링 → HP 30 회복. 채널링 중 피격 시 취소 (쿨타임 없음) | 20초 (성공 시만) |
| Shield | 5초간 피해 50% 감소 | 30초 |

완료 기준: 3종 GA 활성화, 채널링 피격 취소, 쿨타임 GE 복제, Stamina 차감이 PIE 2인 멀티에서 정상 동작.

---

## 1. 아키텍처 요약

```
입력 (Shift/Q/E)
  → ASC->TryActivateAbilitiesByTag(TAG_Ability_Skill_*)
  → GA_Skill_* (LocalPredicted, InstancedPerActor)
      Dash    → LaunchCharacter + GE_Dash_Cost (Stamina -30) + GE_Dash_Cooldown (10s)
      Heal    → GE_Healing (State.Healing 3s) + WaitDelay(3s) / WaitGameplayEvent(Event.Damaged)
                  └→ 완료: GE_Heal (+30 HP) + GE_Heal_Cooldown (20s)
                  └→ 피격 취소: 쿨타임 없이 종료
      ShieldOn → GE_ShieldOn (State.Shielded, 5s) + GE_Shield_Cooldown (30s)
                  └→ PostGEExecute에서 IncomingDamage * 0.5 (State.Shielded 태그 확인)
```

스킬 GA는 `PossessedBy`에서 캐릭터에 부여 (무기 GA와 분리).

---

## 2. 현재 코드 상태

- `UEPAttributeSet`: Health/MaxHealth/IncomingDamage/Ammo/MaxAmmo 존재. Stamina 없음.
- `PostGameplayEffectExecute`: IncomingDamage → Health 직접 차감. 감산 로직 없음.
- NativeGameplayTags: `TAG_HitZone_*` 까지 정의됨. Skill 태그 없음.
- `AEPCharacter::PossessedBy`: ASC InitAbilityActorInfo + Attribute 초기화. DefaultAbilities 부여 없음.

---

## 3. 변경 대상 파일

| 파일 | 작업 |
|------|------|
| `EPNativeGameplayTags.h/.cpp` | Skill/State/Cooldown/Event 태그 추가 |
| `EPAttributeSet.h/.cpp` | Stamina/MaxStamina 추가 + 50% 감산 로직 + Event.Damaged 발송 |
| `EPCharacter.h/.cpp` | DefaultAbilities 배열 + PossessedBy 부여 + Stamina 초기화 |
| `EPGA_Skill_Dash.h/.cpp` | 신규 |
| `EPGA_Skill_Heal.h/.cpp` | 신규 |
| `EPGA_Skill_ShieldOn.h/.cpp` | 신규 |

---

## 4. 구현 순서

### Step 0 — NativeGameplayTags 추가

`EPNativeGameplayTags.h`:
```cpp
// Skill Ability
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Skill_Dash)
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Skill_Heal)
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Skill_Shield)

// Skill State
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Dashing)
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Shielded)
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Healing)

// Skill Cooldown
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Skill_Dash)
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Skill_Heal)
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Skill_Shield)

// Data / Event
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_HealAmount)
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Damaged)
```

`EPNativeGameplayTags.cpp`:
```cpp
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Skill_Dash,    "Ability.Skill.Dash")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Skill_Heal,    "Ability.Skill.Heal")
UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Skill_Shield,  "Ability.Skill.Shield")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dashing,         "State.Dashing")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Shielded,        "State.Shielded")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Healing,         "State.Healing")
UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Skill_Dash,   "Cooldown.Skill.Dash")
UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Skill_Heal,   "Cooldown.Skill.Heal")
UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Skill_Shield, "Cooldown.Skill.Shield")
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_HealAmount,       "Data.HealAmount")
UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Damaged,         "Event.Damaged")
```

---

### Step 1 — AttributeSet: Stamina 추가

`EPAttributeSet.h` — 기존 Ammo 섹션 아래에 추가:
```cpp
UPROPERTY(BlueprintReadOnly, Category = "Attribute|Stamina", ReplicatedUsing = OnRep_Stamina)
FGameplayAttributeData Stamina;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, Stamina);

UPROPERTY(BlueprintReadOnly, Category = "Attribute|Stamina", ReplicatedUsing = OnRep_MaxStamina)
FGameplayAttributeData MaxStamina;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, MaxStamina);
```

OnRep 선언 2개 추가.

`EPAttributeSet.cpp` — `PreAttributeChange`:
```cpp
if (Attribute == GetStaminaAttribute())
    NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
```

`GetLifetimeReplicatedProps`:
```cpp
DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, Stamina,    COND_None, REPNOTIFY_Always);
DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
```

OnRep 구현 2개 추가 (기존 패턴 동일).

---

### Step 2 — PostGameplayEffectExecute 수정

기존 `IncomingDamage` 처리 블록을 교체:

```cpp
if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
{
    const float Damage = GetIncomingDamage();
    SetIncomingDamage(0.f);

    if (Damage > 0.f)
    {
        const bool bWasAlive = GetHealth() > 0.f;
        UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();

        // Shield: 피해 50% 감소
        float RemainingDamage = Damage;
        if (ASC && ASC->HasMatchingGameplayTag(EmpGameplayTags::TAG_State_Shielded))
            RemainingDamage *= 0.5f;

        const float NewHealth = FMath::Max(GetHealth() - RemainingDamage, 0.f);
        SetHealth(NewHealth);

        // 힐 채널링 취소용 이벤트 발송
        if (ASC)
        {
            FGameplayEventData DmgPayload;
            ASC->HandleGameplayEvent(EmpGameplayTags::TAG_Event_Damaged, &DmgPayload);
        }

        if (bWasAlive && NewHealth <= 0.f)
        {
            if (ASC && !ASC->HasMatchingGameplayTag(EmpGameplayTags::TAG_State_Dead))
            {
                FGameplayEventData Payload;
                Payload.Instigator = SourceActor;
                ASC->HandleGameplayEvent(EmpGameplayTags::TAG_Event_Death, &Payload);
            }
        }
    }
}
```

> `TAG_State_Shielded`는 GE_ShieldOn의 GrantedTags로 부여되고 Duration 만료 시 자동 해제됨.

---

### Step 3 — Blueprint GE 에셋 생성

`Content/Blueprints/GAS/` 에 생성:

| 에셋 | Duration | 설정 |
|------|----------|------|
| `GE_Dash_Cost` | Instant | Modifier: `Stamina`, Add, `-30.0` |
| `GE_Dash_Cooldown` | HasDuration | DurationMagnitude: SetByCaller(`Data.Cooldown`), GrantedTags: `Cooldown.Skill.Dash` |
| `GE_StaminaRegen` | Infinite | Period: 0.5, Modifier: `Stamina`, Add, `+5.0` |
| `GE_Healing` | HasDuration | DurationMagnitude: SetByCaller(`Data.Cooldown`) (3초), GrantedTags: `State.Healing` |
| `GE_Heal` | Instant | Modifier: `Health`, Add, SetByCaller(`Data.HealAmount`) |
| `GE_Heal_Cooldown` | HasDuration | DurationMagnitude: SetByCaller(`Data.Cooldown`), GrantedTags: `Cooldown.Skill.Heal` |
| `GE_ShieldOn` | HasDuration | DurationMagnitude: SetByCaller(`Data.Cooldown`) (5초), GrantedTags: `State.Shielded`. **Modifier 없음** |
| `GE_Shield_Cooldown` | HasDuration | DurationMagnitude: SetByCaller(`Data.Cooldown`), GrantedTags: `Cooldown.Skill.Shield` |

> `GE_ShieldOn`은 Attribute를 수정하지 않는다. 피해 50% 감소는 PostGEExecute에서 태그 확인으로 처리.
> `GE_StaminaRegen`: PossessedBy에서 Infinite GE로 항상 부여.

---

### Step 4 — GA_Skill_Dash

`EPGA_Skill_Dash.h`:
```cpp
UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Skill_Dash : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UEPGA_Skill_Dash();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_DashCostClass;

    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_DashCooldownClass;

    UPROPERTY(EditDefaultsOnly, Category = "Dash")
    float DashImpulse = 1200.f;

    UPROPERTY(EditDefaultsOnly, Category = "Dash")
    float DashCooldown = 10.f;
};
```

`EPGA_Skill_Dash.cpp`:
```cpp
UEPGA_Skill_Dash::UEPGA_Skill_Dash()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dashing);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_Cooldown_Skill_Dash);
}

void UEPGA_Skill_Dash::ActivateAbility(...)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (GE_DashCostClass)
        ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
            GE_DashCostClass.GetDefaultObject(), GetAbilityLevel());

    ACharacter* Char = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (Char)
    {
        FVector DashDir = Char->GetLastMovementInputVector();
        if (DashDir.IsNearlyZero())
            DashDir = Char->GetActorForwardVector();

        // bXYOverride=true(수평 속도 재설정), bZOverride=false(Z축 유지)
        Char->LaunchCharacter(DashDir.GetSafeNormal() * DashImpulse, true, false);
    }

    if (GE_DashCooldownClass)
    {
        FGameplayEffectSpecHandle CDSpec = MakeOutgoingGameplayEffectSpec(GE_DashCooldownClass);
        CDSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, DashCooldown);
        ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CDSpec);
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
```

---

### Step 5 — GA_Skill_Heal (채널링)

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
    TSubclassOf<UGameplayEffect> GE_HealingClass;       // 채널링 상태 (State.Healing)

    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_HealClass;           // 완료 시 HP 회복

    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_HealCooldownClass;

    UPROPERTY(EditDefaultsOnly, Category = "Heal")
    float HealAmount   = 30.f;
    float HealDuration = 3.f;
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

    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Healing);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_Cooldown_Skill_Heal);
}

void UEPGA_Skill_Heal::ActivateAbility(...)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 채널링 상태 GE 적용
    if (GE_HealingClass)
    {
        FGameplayEffectSpecHandle HealingSpec = MakeOutgoingGameplayEffectSpec(GE_HealingClass);
        HealingSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, HealDuration);
        HealingEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, HealingSpec);
    }

    // 3초 대기 → 완료 시 힐 적용
    UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, HealDuration);
    WaitTask->OnFinish.AddDynamic(this, &UEPGA_Skill_Heal::OnHealComplete);
    WaitTask->ReadyForActivation();

    // 피격 이벤트 → 채널링 취소
    UAbilityTask_WaitGameplayEvent* EventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this, EmpGameplayTags::TAG_Event_Damaged, nullptr, false, true);
    EventTask->EventReceived.AddDynamic(this, &UEPGA_Skill_Heal::OnDamageTaken);
    EventTask->ReadyForActivation();
}

void UEPGA_Skill_Heal::OnHealComplete()
{
    if (GE_HealClass)
    {
        FGameplayEffectSpecHandle HealSpec = MakeOutgoingGameplayEffectSpec(GE_HealClass);
        HealSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_HealAmount, HealAmount);
        ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, HealSpec);
    }
    if (GE_HealCooldownClass)
    {
        FGameplayEffectSpecHandle CDSpec = MakeOutgoingGameplayEffectSpec(GE_HealCooldownClass);
        CDSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, HealCooldown);
        ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, CDSpec);
    }
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UEPGA_Skill_Heal::OnDamageTaken(FGameplayEventData Payload)
{
    // 피격 취소 — 쿨타임 없음
    EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
}

void UEPGA_Skill_Heal::EndAbility(...)
{
    // State.Healing GE 명시적 제거 (취소 시 즉시 해제)
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        if (HealingEffectHandle.IsValid())
            ASC->RemoveActiveGameplayEffect(HealingEffectHandle);

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
```

> WaitDelay와 WaitGameplayEvent 두 태스크가 병렬 실행되다가 먼저 발동된 쪽의 콜백에서 EndAbility가 호출되면 나머지 태스크는 자동 정리됨.
> 피격 취소 시 `bWasCancelled=true` — 쿨타임 GE는 OnHealComplete 경로에서만 적용.

---

### Step 6 — GA_Skill_ShieldOn

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
    float ShieldCooldown = 30.f;
};
```

`EPGA_Skill_ShieldOn.cpp`:
```cpp
UEPGA_Skill_ShieldOn::UEPGA_Skill_ShieldOn()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Shielded);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_Cooldown_Skill_Shield);
}

void UEPGA_Skill_ShieldOn::ActivateAbility(...)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // State.Shielded 부여 — PostGEExecute에서 이 태그 확인 후 피해 50% 감소
    if (GE_ShieldOnClass)
    {
        FGameplayEffectSpecHandle ShieldSpec = MakeOutgoingGameplayEffectSpec(GE_ShieldOnClass);
        ShieldSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, ShieldDuration);
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

### Step 7 — 캐릭터에 스킬 부여

`EPCharacter.h`:
```cpp
protected:
    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_StaminaRegenClass;
```

`EPCharacter.cpp` — `PossessedBy` 마지막 (HasAuthority() 블록 안):
```cpp
for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
{
    if (AbilityClass)
        ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
}

if (UEPAttributeSet* AS = ASC->GetSet<UEPAttributeSet>())
{
    AS->InitMaxStamina(100.f);
    AS->InitStamina(100.f);
}
if (GE_StaminaRegenClass)
    ASC->ApplyGameplayEffectToSelf(
        GE_StaminaRegenClass.GetDefaultObject(), 1.f,
        ASC->MakeEffectContext());
```

---

### Step 8 — 입력 연동

`EPCharacter.h` — 입력 함수 선언:
```cpp
void Input_Dash(const FInputActionValue& Value);
void Input_Heal(const FInputActionValue& Value);
void Input_Shield(const FInputActionValue& Value);
```

`EPCharacter.cpp` — SetupPlayerInputComponent:
```cpp
if (PC->GetDashAction())
    EnhancedInput->BindAction(PC->GetDashAction(), ETriggerEvent::Triggered, this, &AEPCharacter::Input_Dash);
if (PC->GetHealAction())
    EnhancedInput->BindAction(PC->GetHealAction(), ETriggerEvent::Triggered, this, &AEPCharacter::Input_Heal);
if (PC->GetShieldAction())
    EnhancedInput->BindAction(PC->GetShieldAction(), ETriggerEvent::Triggered, this, &AEPCharacter::Input_Shield);
```

구현:
```cpp
void AEPCharacter::Input_Dash(const FInputActionValue& Value)
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Skill_Dash));
}
void AEPCharacter::Input_Heal(const FInputActionValue& Value)
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Skill_Heal));
}
void AEPCharacter::Input_Shield(const FInputActionValue& Value)
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Skill_Shield));
}
```

`EPPlayerController.h`에 DashAction/HealAction/ShieldAction InputAction UPROPERTY 추가 (기존 패턴 동일).

---

### Step 9 — 에디터 설정

1. `IA_Dash`, `IA_Heal`, `IA_Shield` InputAction 에셋 생성 (Value Type: Boolean)
2. `IMC_DefaultMappingContext`에 키 매핑 추가 (예: Shift=Dash, Q=Heal, E=Shield)
3. `BP_PlayerController`에 DashAction/HealAction/ShieldAction 슬롯 연결
4. `BP_GA_Skill_Dash/Heal/ShieldOn` Blueprint 생성 (부모: 각 C++ 클래스), GE 슬롯 연결
5. `BP_EPCharacter` → DefaultAbilities에 3종 GA 추가, GE_StaminaRegenClass 연결

---

## 5. 완료 체크리스트

### Step 0 — 태그
- [ ] `EPNativeGameplayTags.h/.cpp`: Skill/State/Cooldown/Event 태그 11개 추가

### Step 1 — AttributeSet Stamina
- [ ] Stamina/MaxStamina UPROPERTY 추가
- [ ] PreAttributeChange 클램핑 추가
- [ ] GetLifetimeReplicatedProps 등록
- [ ] OnRep 2개 구현

### Step 2 — PostGEExecute 수정
- [ ] Shield 50% 감산 로직 추가
- [ ] Event.Damaged 발송 추가

### Step 3 — GE 에셋
- [ ] GE_Dash_Cost, GE_Dash_Cooldown 생성
- [ ] GE_StaminaRegen 생성 (Period=0.5, Stamina +5)
- [ ] GE_Healing 생성 (HasDuration, GrantedTags: State.Healing)
- [ ] GE_Heal 생성 (Instant, SetByCaller Health +HealAmount)
- [ ] GE_Heal_Cooldown 생성
- [ ] GE_ShieldOn 생성 (HasDuration, GrantedTags: State.Shielded, Modifier 없음)
- [ ] GE_Shield_Cooldown 생성

### Step 4~6 — GA C++
- [ ] `EPGA_Skill_Dash` 생성 및 구현
- [ ] `EPGA_Skill_Heal` 생성 및 구현 (채널링, EndAbility override)
- [ ] `EPGA_Skill_ShieldOn` 생성 및 구현

### Step 7 — 캐릭터 부여
- [ ] `EPCharacter.h`: DefaultAbilities, GE_StaminaRegenClass 추가
- [ ] `EPCharacter.cpp`: PossessedBy에 부여 + Stamina 초기화

### Step 8 — 입력
- [ ] PlayerController에 DashAction/HealAction/ShieldAction 추가
- [ ] Character에 Input_Dash/Heal/Shield 함수 + 바인딩

### Step 9 — 에디터
- [ ] IA 에셋 3종 + IMC 키 매핑
- [ ] BP_GA 3종 생성, GE 슬롯 연결
- [ ] BP_EPCharacter DefaultAbilities/GE_StaminaRegenClass 설정

### 검증
- [ ] Dash: 이동 방향 대시, Stamina -30, 10초 쿨타임 GE 복제
- [ ] Heal 채널링: State.Healing 태그 복제, 3초 후 HP +30, 20초 쿨타임 적용
- [ ] Heal 취소: 채널링 중 피격 시 State.Healing 즉시 해제, 쿨타임 미적용
- [ ] Shield: State.Shielded 태그 복제, 피격 시 데미지 절반, 5초 후 자동 해제
- [ ] `showdebug abilitysystem`으로 3종 쿨타임 GE 확인

---

## 6. 함정 & 주의사항

| 상황 | 원인 | 해결 |
|------|------|------|
| 채널링 중 WaitGameplayEvent 미동작 | Event.Damaged를 PostGEExecute에서 발송 안 함 | Step 2 확인 |
| 힐 완료 후 State.Healing 태그 잔류 | EndAbility에서 HealingEffectHandle 제거 누락 | EndAbility override 확인 |
| 피격 취소 시 쿨타임이 걸림 | OnDamageTaken에서 쿨타임 GE를 적용하면 안 됨 | OnDamageTaken → EndAbility(cancelled=true)만 호출 |
| Dash 방향이 항상 앞쪽 | 이동 입력 없을 때 GetLastMovementInputVector()가 Zero | ForwardVector 폴백 처리 (구현에 포함) |
| DefaultAbilities 중복 부여 | PossessedBy 재호출 | HasAuthority() 가드 확인 |
| Stamina 회복이 MaxStamina 초과 | PreAttributeChange 클램핑 누락 | Step 1 클램핑 확인 |
