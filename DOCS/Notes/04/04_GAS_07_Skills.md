# GAS 스킬 시스템 — Dash / Heal / ShieldOn

> 캐릭터 고유 스킬 3종. 무기 어빌리티(PrimaryUse, Reload)와 달리 장착 무기와 무관하게 항상 보유.

**GAME.md 스펙 기준:**

| 스킬 | 효과 | 쿨타임 |
|------|------|--------|
| Dash | 이동 방향으로 즉시 대시 (트레이서형 — 쿨타임만 있음) | 10초 |
| Heal | 3초 채널링 → HP 30 회복, 채널링 중 이동속도 20%로 감소, Dash/Shield 잠금. 채널링 중 피격 시 취소 (쿨타임 없음) | 20초 (성공 시만) |
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
      Heal    → GE_Healing (State.Healing, 3s) + GE_MoveSpeed_Modifier (×0.2, 3s) + WaitDelay(3s) ∥ WaitGameplayEvent(Event.Damaged)
                  └→ 완료: GE_Heal (+30 HP) + GE_Heal_Cooldown (20s)
                  └→ 피격 취소: 쿨타임 없이 종료
                  └→ State.Healing이 Dash/ShieldOn의 ActivationBlockedTags에 걸려 채널링 중 다른 스킬 잠김 (Step 8)
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
| `EPAttributeSet.h/.cpp` | `MoveSpeedMultiplier` 어트리뷰트 추가 (Step 8) |
| `EPCharacterMovement.h/.cpp` | `MoveSpeedMultiplier` 필드 + `GetMaxSpeed()` 곱연산 반영 (Step 8) |
| `EPCharacter.h/.cpp` | `InitASC`에 어트리뷰트 변경 구독 추가 (Step 8) |
| `EPGA_Skill_Heal.h/.cpp` | 슬로우 GE 적용 + `EndAbility` authority 가드 (Step 8) |
| `EPGA_Skill_Dash.cpp` / `EPGA_Skill_ShieldOn.cpp` | `ActivationBlockedTags`에 `State.Healing` 추가 (Step 8) |

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

### Step 8 — 힐 이동속도 감소 + 스킬 상호 잠금 (신규)

**설계 요지**: 이동속도 감소를 Heal에 하드코딩하지 않고 GAS 표준 패턴인 **어트리뷰트 + Multiply 모디파이어**로 구현한다 — 나중에 다른 감속/가속 효과가 추가돼도 새 어트리뷰트나 CMC 코드 없이 GE만 추가하면 되도록 하기 위함(실무에서 흔한 확장 포인트). 스킬 잠금은 신규 개념이 아니라 이미 있는 `ActivationBlockedTags`를 재사용한다.

#### 8-1. `MoveSpeedMultiplier` 어트리뷰트 추가

`EPAttributeSet.h` — Health/Ammo와 같은 자리에 추가:
```cpp
UPROPERTY(BlueprintReadOnly, Category = "Attribute|Movement", ReplicatedUsing = OnRep_MoveSpeedMultiplier)
FGameplayAttributeData MoveSpeedMultiplier;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, MoveSpeedMultiplier);
```
```cpp
UFUNCTION()
void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);
```

`EPAttributeSet.cpp`:
```cpp
// PreAttributeChange에 추가 — 0 이하로 떨어져 정지/역주행하는 사고 방지, 상한은 넉넉하게
if (Attribute == GetMoveSpeedMultiplierAttribute())
    NewValue = FMath::Clamp(NewValue, 0.05f, 3.f);
```
```cpp
// GetLifetimeReplicatedProps에 추가
DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, MoveSpeedMultiplier, COND_None, REPNOTIFY_Always);
```
```cpp
void UEPAttributeSet::OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, MoveSpeedMultiplier, OldValue);
}
```

`EPCharacter.cpp` `PossessedBy` — 기존 `InitHealth`/`InitMaxHealth` 옆:
```cpp
AS->InitMoveSpeedMultiplier(1.f);
```

> `04_GAS_01_Foundation.md`가 정의한 AttributeSet을 Health/Ammo와 동일 패턴으로 확장하는 것이라 Foundation 문서 자체는 갱신하지 않는다.

#### 8-2. `EPCharacterMovement`에 배속 훅 추가

기존 `GetMaxSpeed()`의 Sprint/Aim 분기 결과에 **곱연산**으로 얹는다 (힐 중 스프린트를 시도하면 `SprintSpeed × 0.2`가 나와야 자연스럽다):

```cpp
// EPCharacterMovement.h — public에 추가. UPROPERTY 아님(디자이너 노출 불필요, 순수 게임플레이 코드로만 갱신)
float MoveSpeedMultiplier = 1.f;
```
```cpp
// EPCharacterMovement.cpp
float UEPCharacterMovement::GetMaxSpeed() const
{
    float Base = Super::GetMaxSpeed();
    if (bWantsToSprint && IsMovingOnGround()) Base = SprintSpeed;
    else if (bWantsToAim) Base = AimSpeed;
    return Base * MoveSpeedMultiplier;
}
```

> `MoveSpeedMultiplier`는 CMC 레벨에서 별도로 복제하지 않는다 — 서버와 소유 클라 양쪽이 **각자 자신의 ASC 복제 어트리뷰트**로부터 독립적으로 채우므로(8-3), Sprint/Aim처럼 CompressedFlags로 복제할 필요가 없다.

#### 8-3. `EPCharacter::InitASC`에서 어트리뷰트 변경 구독

기존 HUD 바인딩과 같은 지점(`InitASC`)에 추가하되, `IsLocallyControlled()` 가드 **밖에** 둔다 — 서버 권위 시뮬레이션과 소유 클라 예측 둘 다 `GetMaxSpeed()`를 실행하므로 둘 다 필요하다:

```cpp
// EPCharacter.h — private
UFUNCTION()
void OnMoveSpeedMultiplierChanged(const FOnAttributeChangeData& Data);

FDelegateHandle MoveSpeedMultiplierHandle;
```
```cpp
// EPCharacter.cpp — InitASC()
void AEPCharacter::InitASC()
{
    AEPPlayerState* PS = GetPlayerState<AEPPlayerState>();
    if (!PS || !ASC) return;

    ASC->InitAbilityActorInfo(PS, this);

    // 리스폰 재호출 대비 — 기존 바인딩 해제 후 재바인딩 (HUD 위젯과 동일 관례)
    if (MoveSpeedMultiplierHandle.IsValid())
        ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMoveSpeedMultiplierAttribute())
            .Remove(MoveSpeedMultiplierHandle);

    MoveSpeedMultiplierHandle = ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMoveSpeedMultiplierAttribute())
        .AddUObject(this, &AEPCharacter::OnMoveSpeedMultiplierChanged);

    // 초기값 즉시 반영 (리스폰 등으로 이미 1.0이 아닐 수 있음)
    if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
        CMC->MoveSpeedMultiplier = ASC->GetNumericAttribute(UEPAttributeSet::GetMoveSpeedMultiplierAttribute());

    if (IsLocallyControlled())
        if (AEPPlayerController* PC = GetController<AEPPlayerController>())
            PC->InitHUD(ASC);
}

void AEPCharacter::OnMoveSpeedMultiplierChanged(const FOnAttributeChangeData& Data)
{
    if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
        CMC->MoveSpeedMultiplier = Data.NewValue;
}
```

#### 8-4. 재사용 가능한 감속 GE — `GE_MoveSpeed_Modifier`

Heal 전용이 아니라 **범용 GE**. `Content/Blueprints/GE/` 최상위(스킬 전용 폴더가 아님 — 향후 다른 디버프/버프도 재사용).

| 항목 | 값 |
|------|-----|
| Duration Policy | Has Duration, SetByCaller(`Data.Duration`) |
| Modifier | `MoveSpeedMultiplier`, **Multiply**, SetByCaller(`Data.MoveSpeedMultiplier`) |
| GrantedTags | 없음 (State.Healing은 이미 `GE_Healing`이 부여) |

> Modifier Op을 **Multiply**로 하는 것이 핵심 — 나중에 감속 효과 두 개가 겹쳐도 GAS가 자동으로 곱연산 누적한다. Add를 쓰면 여러 효과가 겹쳤을 때 서로 값을 빼앗는 식으로 계산이 꼬인다.

신규 태그 (`EPNativeGameplayTags.h/.cpp`):
```cpp
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_MoveSpeedMultiplier)
```
```cpp
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_MoveSpeedMultiplier, "Data.MoveSpeedMultiplier")
```

#### 8-5. `EPGA_Skill_Heal`에서 적용 + EndAbility 정리 (기존 버그 동시 수정)

```cpp
// EPGA_Skill_Heal.h
UPROPERTY(EditDefaultsOnly, Category = "GAS")
TSubclassOf<UGameplayEffect> GE_MoveSpeedModifierClass;

UPROPERTY(EditDefaultsOnly, Category = "Heal")
float HealMoveSpeedMultiplier = 0.2f;   // GAME.md 스펙: 채널링 중 20%로 감소

private:
    FActiveGameplayEffectHandle MoveSpeedEffectHandle;   // HealingEffectHandle 옆에 추가
```

```cpp
// EPGA_Skill_Heal.cpp — ActivateAbility, GE_HealingClass 적용 블록 옆에 추가
if (GE_MoveSpeedModifierClass)
{
    FGameplayEffectSpecHandle SlowSpec = MakeOutgoingGameplayEffectSpec(GE_MoveSpeedModifierClass);
    SlowSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Duration, HealDuration);
    SlowSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_MoveSpeedMultiplier, HealMoveSpeedMultiplier);
    MoveSpeedEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SlowSpec);
}
```

`EndAbility` — 새 핸들 정리에 곁들여 기존 버그도 고친다. 지금 코드는 `LocalPredicted`라 클라 예측 인스턴스에서도 `EndAbility`가 돌기 때문에, authority 체크 없이 `RemoveActiveGameplayEffect`를 부르면 `LogAbilitySystem: Warning: RemoveActiveGameplayEffect called without Authority...`가 매번 찍힌다 (엔진 소스 `AbilitySystemComponent.cpp:1177`에서 `IsOwnerActorAuthoritative()`가 false면 그냥 경고만 찍고 반환하는 것을 확인). 서버에서만 실행되도록 감싼다:

```cpp
void UEPGA_Skill_Heal::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // LocalPredicted라 이 함수는 서버 인스턴스 + 클라 예측 인스턴스 양쪽에서 실행된다.
    // RemoveActiveGameplayEffect는 서버 권위에서만 허용되므로 가드 필수 (없으면 클라에서 매번 경고 로그).
    if (ActorInfo->IsNetAuthority())
    {
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        {
            if (HealingEffectHandle.IsValid())
            {
                ASC->RemoveActiveGameplayEffect(HealingEffectHandle);
                HealingEffectHandle.Invalidate();
            }
            if (MoveSpeedEffectHandle.IsValid())
            {
                ASC->RemoveActiveGameplayEffect(MoveSpeedEffectHandle);
                MoveSpeedEffectHandle.Invalidate();
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
```

> `EPGA_Item_Reload.cpp`의 `EndAbility`(67행)에도 authority 가드 없이 동일한 `RemoveActiveGameplayEffect` 호출이 있다 — 같은 버그가 잠재되어 있으니 리로드 쪽도 같은 패턴으로 고치는 것을 권장한다 (이 문서 범위 밖, `04_GAS_04_Reload.md` 참고).

#### 8-6. 스킬 상호 잠금 — Dash/ShieldOn에 `State.Healing` 차단 태그 추가

신규 메커니즘이 아니라 기존 `ActivationBlockedTags`에 태그 하나씩 추가하는 것뿐이다. `TryActivateAbilitiesByTag`는 클라이언트에서 먼저 `CanActivateAbility`(ActivationBlockedTags 검사 포함)를 통과해야 서버로 요청을 보내므로, 이 한 줄로 클라/서버 양쪽 다 즉시 차단된다 — 입력 쪽에 별도 가드 코드 불필요.

```cpp
// EPGA_Skill_Dash.cpp 생성자에 추가
ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Healing);

// EPGA_Skill_ShieldOn.cpp 생성자에 추가
ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Healing);
```

> 잠긴 슬롯이 빨갛게 변하는 시각 피드백은 GAS 레이어가 아니라 HUD 위젯의 몫이다 — `04_GAS_08_HUD.md`의 `LockTags` 참고.

**verify:** 빌드 통과. PIE에서 힐 시전 중 이동속도 체감 20%로 감소(스프린트해도 SprintSpeed×0.2), Dash/Shield 키 입력이 무반응(서버 CommitAbility 실패 로그도 없어야 함 — 클라에서 아예 활성화 시도가 막힘), 힐 종료/취소 시 즉시 원래 속도로 복귀, `RemoveActiveGameplayEffect` 경고 로그 더 이상 안 뜸.

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

### Step 8 — 힐 이동속도 감소 + 스킬 잠금 (신규)
- [ ] `EPAttributeSet`: `MoveSpeedMultiplier` 어트리뷰트(초기값 1.0) + PreAttributeChange 클램프 + 복제
- [ ] `EPCharacterMovement::GetMaxSpeed()`가 `MoveSpeedMultiplier`를 곱연산으로 반영
- [ ] `EPCharacter::InitASC`에서 어트리뷰트 변경 구독 (리스폰 재바인딩 안전)
- [ ] `GE_MoveSpeed_Modifier` 에셋 (Multiply, SetByCaller 2종)
- [ ] `EPGA_Skill_Heal`: 슬로우 GE 적용 + `EndAbility` authority 가드 추가(경고 로그 수정 겸함)
- [ ] `EPGA_Skill_Dash`/`EPGA_Skill_ShieldOn`: `ActivationBlockedTags`에 `State.Healing` 추가

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
| `RemoveActiveGameplayEffect called without Authority` 경고 | `LocalPredicted` 어빌리티의 `EndAbility`가 클라 예측 인스턴스에서도 실행되는데, authority 체크 없이 GE를 지우려 함 | `ActorInfo->IsNetAuthority()`로 감싸 서버에서만 실행 (Step 8-5) |
| 힐 중 스프린트해도 속도가 안 줄어듦 | `MoveSpeedMultiplier`를 Sprint/Aim 분기 **이전**에 곱하거나, 분기 자체를 건드림 | `GetMaxSpeed()`에서 Sprint/Aim으로 Base를 정한 **뒤에** 마지막으로 `* MoveSpeedMultiplier` |
| 감속 GE가 겹칠 때 계산이 이상함 | Modifier Op을 Add로 설정 | Multiply로 설정 — 여러 감속 효과가 자동으로 곱연산 누적됨 (Step 8-4) |

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
