# 4단계: Gameplay Ability System (GAS)

## 1. GAS 아키텍처 개요

GAS는 스킬/버프/디버프/상태이상 같은 게임플레이 능력을 구조화하여 구현하는 UE5 플러그인.
네트워크 복제와 예측을 내장하고 있어 멀티플레이어에 적합.

### 핵심 구성 요소

| 구성 요소 | 역할 |
|-----------|------|
| AbilitySystemComponent (ASC) | 중앙 허브. 어빌리티 관리, 이펙트 적용, 태그 추적, 어트리뷰트 관리 |
| GameplayAbility (GA) | 하나의 능력 (대시, 힐, 실드 등). 활성화/종료 로직 |
| GameplayEffect (GE) | 어트리뷰트 변경 규칙. 데미지, 힐, 버프 등의 데이터 정의 |
| AttributeSet | 어트리뷰트(HP, Stamina, Shield 등)를 정의하는 컨테이너 |
| GameplayTag | 계층적 태그 시스템. 상태/조건을 태그로 표현 |
| GameplayCue | 이펙트 발동 시 VFX/SFX 재생 (cosmetic) |

### 프로젝트에서 GAS 활성화

```csharp
// Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "GameplayAbilities",
    "GameplayTags",
    "GameplayTasks"
});
```

```cpp
// GameInstance::Init() 에서 반드시 호출
UAbilitySystemGlobals::Get().InitGlobalData();
```

---

## 2. AbilitySystemComponent (ASC) 배치

### Character vs PlayerState

| 배치 위치 | 장점 | 단점 |
|-----------|------|------|
| Character | 단순. ASC 수명 = Pawn 수명 | Pawn 사망/리스폰 시 ASC 파괴, 어트리뷰트 손실 |
| PlayerState | ASC가 리스폰에도 유지. 돈/경험치 등 영속 데이터 유지 | 설정이 좀 더 복잡 |

**EmploymentProj는 PlayerState에 배치** (PlayerState에 이미 Money, Kills 등이 있으므로).

### PlayerState에 ASC 배치 시 설정

```cpp
// PlayerState
UCLASS()
class AEPPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    AEPPlayerState();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    UAbilitySystemComponent* AbilitySystemComponent;

    UPROPERTY()
    UEPAttributeSet* AttributeSet;

    UAbilitySystemComponent* GetAbilitySystemComponent() const { return AbilitySystemComponent; }
};

// 생성자
AEPPlayerState::AEPPlayerState()
{
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UEPAttributeSet>(TEXT("AttributeSet"));
}
```

```cpp
// Character에서 ASC 접근 (IAbilitySystemInterface 구현)
class AEPCharacter : public ACharacter, public IAbilitySystemInterface
{
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override
    {
        AEPPlayerState* PS = GetPlayerState<AEPPlayerState>();
        return PS ? PS->GetAbilitySystemComponent() : nullptr;
    }
};
```

### InitAbilityActorInfo 호출 시점

ASC의 OwnerActor(소유자)와 AvatarActor(물리적 표현)를 설정해야 한다:

```cpp
// 서버: PossessedBy에서
void AEPCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    if (AEPPlayerState* PS = GetPlayerState<AEPPlayerState>())
    {
        PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);
    }
}

// 클라이언트: OnRep_PlayerState에서
void AEPCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (AEPPlayerState* PS = GetPlayerState<AEPPlayerState>())
    {
        PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);
    }
}
```

---

## 3. GameplayAbility (GA)

### 활성화 흐름

```
TryActivateAbility()
    │
    ▼
CanActivateAbility()
    ├─ 이미 활성 중인가? (InstancingPolicy에 따라)
    ├─ 태그 요구사항 충족? (ActivationRequired/BlockedTags)
    ├─ CheckCost() - 비용 충분?
    └─ CheckCooldown() - 쿨타임 만료?
    │
    ▼ (전부 통과)
ActivateAbility()
    ├─ CommitAbility() 호출 → 비용 지불 + 쿨타임 시작
    ├─ 로직 실행 (AbilityTask 사용)
    │
    ▼ (완료 또는 취소)
EndAbility()
    └─ 정리: 활성 태그 제거, 태스크 종료
```

### InstancingPolicy

| 정책 | 동작 | 사용처 |
|------|------|--------|
| NonInstanced | CDO 사용, 상태 저장 불가 | 단순 패시브, 즉발 효과 |
| InstancedPerActor | Actor당 1개 인스턴스, 재사용 | **대부분의 능력** (Dash, Heal, Shield) |
| InstancedPerExecution | 실행마다 새 인스턴스 | 독립 상태 필요 시 (투사체 다수 동시) |

### CommitAbility

```cpp
void UGA_Dash::ActivateAbility(...)
{
    // CommitAbility = ApplyCost + ApplyCooldown
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        // 비용 부족 또는 쿨타임 → 실패
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 능력 로직 실행...
}
```

- `CostGameplayEffectClass`: 비용 GE (예: Stamina -20, Instant)
- `CooldownGameplayEffectClass`: 쿨타임 GE (예: Duration 10초, Cooldown 태그 부여)

### AbilityTask (비동기 작업)

어빌리티 내에서 시간이 걸리는 작업을 처리:

| 태스크 | 용도 |
|--------|------|
| `WaitDelay` | 지정 시간 대기 |
| `PlayMontageAndWait` | 몽타주 재생 후 대기 |
| `WaitGameplayEvent` | 특정 GameplayEvent 수신 대기 |
| `WaitGameplayTagAdded` | 특정 태그 추가 대기 |
| `WaitGameplayTagRemoved` | 특정 태그 제거 대기 |
| `WaitGameplayEffectRemoved` | 특정 GE 제거 대기 |
| `WaitInputPress/Release` | 입력 대기 |
| `ApplyRootMotionConstantForce` | 루트 모션 적용 (대시) |

```cpp
// 사용 패턴
UAbilityTask_PlayMontageAndWait* MontageTask =
    UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        this, NAME_None, HealMontage, 1.0f);

MontageTask->OnCompleted.AddDynamic(this, &UGA_Heal::OnMontageCompleted);
MontageTask->OnInterrupted.AddDynamic(this, &UGA_Heal::OnMontageCancelled);
MontageTask->ReadyForActivation(); // 반드시 호출
```

---

## 4. GameplayEffect (GE)

데이터 전용 객체. C++에서 서브클래싱하지 않고, 프로퍼티 설정이나 블루프린트로 구성.

### Duration 타입

| 타입 | 동작 | 어트리뷰트 영향 |
|------|------|----------------|
| Instant | 즉시 1회 적용 | **BaseValue** 영구 변경 |
| HasDuration | 지정 시간 후 자동 제거 | **CurrentValue** 임시 변경. 제거 시 복구 |
| Infinite | 수동으로 제거해야 함 | CurrentValue 임시 변경. 제거 시 복구 |

핵심: Instant만 BaseValue를 변경. Duration/Infinite는 CurrentValue만 변경하고 제거 시 원복.

### Modifier (수정자)

| 연산 | 공식 | 예시 |
|------|------|------|
| Additive | BaseValue + 합계 | +50 힐, -30 데미지 |
| Multiplicative | Additive 결과 * (1 + 곱) | 1.5x 데미지 배율 |
| Division | Multiplicative 결과 / 값 | 잘 안 씀 |
| Override | = 고정값 | HP를 100으로 리셋 |

적용 순서: Additive → Multiplicative → Division → Override

### Magnitude 계산 방식

| 방식 | 설명 |
|------|------|
| ScalableFloat | 고정 값 (커브 테이블로 레벨별 스케일 가능) |
| AttributeBased | 다른 어트리뷰트 기반 (예: 데미지 = 공격력 * 계수) |
| CustomCalculationClass | `UGameplayModMagnitudeCalculation` 서브클래스 |
| SetByCaller | 런타임에 태그로 값 지정 |

```cpp
// SetByCaller 예시
FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(DamageEffect, 1, Context);
Spec.Data->SetSetByCallerMagnitude(
    FGameplayTag::RequestGameplayTag("Data.Damage"), -30.0f);
ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
```

### Stacking (중첩)

| 타입 | 동작 |
|------|------|
| None | 각 적용이 독립 인스턴스. 모두 개별 집계 |
| AggregateBySource | 같은 소스의 적용이 중첩. 다른 소스는 별도 |
| AggregateByTarget | 소스 무관, 대상에서 하나로 중첩 |

관련 설정: `StackLimitCount`, `StackDurationRefreshPolicy`, `StackExpirationPolicy`

### Periodic Effect (주기적 효과)

Duration/Infinite GE에 `Period` 설정:
```
예: 독 DoT
- DurationPolicy: HasDuration (10초)
- Period: 2.0초
- Modifier: Health, Additive, -5
- 결과: 2초마다 -5 HP, 총 -25 HP
```

---

## 5. AttributeSet

### 정의

```cpp
UCLASS()
class UEPAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    // ATTRIBUTE_ACCESSORS 매크로가 Get/Set/Init 함수 자동 생성
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes")
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UEPAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes")
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UEPAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes")
    FGameplayAttributeData Stamina;
    ATTRIBUTE_ACCESSORS(UEPAttributeSet, Stamina)

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "Attributes")
    FGameplayAttributeData Shield;
    ATTRIBUTE_ACCESSORS(UEPAttributeSet, Shield)

    // 메타 어트리뷰트 - 복제 안 됨, 서버에서만 사용 (데미지 파이프라인)
    UPROPERTY(BlueprintReadOnly, Category = "Meta")
    FGameplayAttributeData IncomingDamage;
    ATTRIBUTE_ACCESSORS(UEPAttributeSet, IncomingDamage)

    // OnRep 함수들
    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldHealth);
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
    UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldStamina);
    UFUNCTION() void OnRep_Shield(const FGameplayAttributeData& OldShield);
};
```

### FGameplayAttributeData

```cpp
struct FGameplayAttributeData
{
    float BaseValue;     // Instant GE가 변경하는 영구 값
    float CurrentValue;  // BaseValue + Duration/Infinite modifier 집계 결과
};
```

### 복제 등록

```cpp
void UEPAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, Shield, COND_None, REPNOTIFY_Always);
}
```

`REPNOTIFY_Always`: 값이 같더라도 OnRep 호출 (UI 갱신 보장).

### PreAttributeChange - CurrentValue 클램핑

```cpp
void UEPAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    else if (Attribute == GetShieldAttribute())
        NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
    else if (Attribute == GetStaminaAttribute())
        NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
}
```

### PostGameplayEffectExecute - 데미지 파이프라인

Instant/Periodic GE 적용 후 호출. 데미지 처리의 핵심:

```cpp
void UEPAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        float Damage = GetIncomingDamage();
        SetIncomingDamage(0.f); // 메타 어트리뷰트 리셋

        if (Damage > 0.f)
        {
            // 실드가 먼저 흡수
            float ShieldAbsorb = FMath::Min(Damage, GetShield());
            SetShield(GetShield() - ShieldAbsorb);
            Damage -= ShieldAbsorb;

            // 남은 데미지를 HP에 적용
            float NewHealth = FMath::Max(GetHealth() - Damage, 0.f);
            SetHealth(NewHealth);

            if (NewHealth <= 0.f)
            {
                // 사망 이벤트 발송
                FGameplayEventData EventData;
                EventData.Instigator = Data.EffectSpec.GetEffectContext().GetInstigator();
                UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
                    GetOwningActor(),
                    FGameplayTag::RequestGameplayTag("Event.Death"),
                    EventData);
            }
        }
    }

    // HP 클램핑
    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
    }
}
```

---

## 6. GameplayTag

### 기본 사용

```cpp
// 네이티브 태그 선언
// Header
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Dead);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Dash);

// Source
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dead, "State.Dead");
UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Dash, "Cooldown.Dash");
```

태그는 계층적: `"Ability.Skill.Dash"`는 `"Ability.Skill"`, `"Ability"`를 부모로 포함.

### 어빌리티 태그 설정

| 프로퍼티 | 효과 |
|----------|------|
| AbilityTags | 이 어빌리티를 식별하는 태그 |
| ActivationOwnedTags | 활성화 중 ASC에 추가되는 태그 (종료 시 제거) |
| ActivationRequiredTags | ASC에 이 태그가 있어야 활성화 가능 |
| ActivationBlockedTags | ASC에 이 태그가 있으면 활성화 불가 |
| CancelAbilitiesWithTag | 활성화 시, 이 태그를 가진 다른 어빌리티 취소 |
| BlockAbilitiesWithTag | 활성화 중, 이 태그를 가진 어빌리티 차단 |

### 쿨타임 패턴

```
GE_Cooldown_Dash:
  DurationPolicy: HasDuration
  Duration: 10초
  GrantedTags: { Cooldown.Dash }

GA_Dash:
  CooldownGameplayEffectClass: GE_Cooldown_Dash
  ActivationBlockedTags: { Cooldown.Dash }
```

쿨타임 남은 시간 확인:
```cpp
float Remaining, Duration;
bool bOnCooldown = ASC->GetCooldownRemainingForTag(
    FGameplayTagContainer(TAG_Cooldown_Dash), Remaining, Duration);
```

---

## 7. GAS 네트워크

### Replication Mode

ASC의 복제 모드:

| 모드 | 동작 | 사용처 |
|------|------|--------|
| Full | 모든 GE 정보를 모든 클라에 복제 | 싱글플레이어, 소규모 |
| **Mixed** | 소유 클라에 전체 GE, 비소유에 최소 | **멀티 플레이어 캐릭터 (권장)** |
| Minimal | 최소 복제만 | AI 캐릭터 |

### Net Execution Policy

각 GA의 네트워크 실행 방식:

| 정책 | 동작 | 사용처 |
|------|------|--------|
| **LocalPredicted** | 클라가 먼저 실행, 서버 확인/거부 | **플레이어 능력 (Dash, Heal, Shield)** |
| LocalOnly | 소유 클라에서만 실행 | cosmetic 전용 |
| ServerInitiated | 서버가 먼저 실행, 클라에 복제 | AI 능력, 서버 이벤트 |
| ServerOnly | 서버에서만 실행 | 서버 전용 로직 |

### 예측 흐름

```
클라이언트                        서버
  │                               │
  ├─ TryActivateAbility ─────────>│
  │  (PredictionKey 생성)          │
  │  (로컬에서 즉시 실행)           │
  │                               ├─ CanActivateAbility?
  │                               │  YES → ActivateAbility (권한적)
  │<── Confirm PredictionKey ─────│  → 클라 예측 유지
  │                               │
  │                               │  NO → Reject
  │<── Reject PredictionKey ──────│  → 클라 롤백
  │  (어빌리티 종료, GE 제거,       │
  │   어트리뷰트 복원)              │
```

---

## 8. EmploymentProj 능력 설계

### Dash (10초 쿨타임)

```
GA_Dash:
  InstancingPolicy: InstancedPerActor
  NetExecutionPolicy: LocalPredicted
  AbilityTags: { Ability.Skill.Dash }
  ActivationOwnedTags: { State.Dashing }
  ActivationBlockedTags: { State.Dead, Cooldown.Dash }
  CancelAbilitiesWithTag: { State.Channeling }
  CostGE: GE_Dash_Cost (Stamina -20, Instant)
  CooldownGE: GE_Dash_Cooldown (Duration 10초, Cooldown.Dash 태그)

  구현:
    CommitAbility → LaunchCharacter 또는 ApplyRootMotionConstantForce → EndAbility
```

### Heal (3초 채널링, HP +30, 20초 쿨타임, 피격 시 취소)

```
GA_Heal:
  InstancingPolicy: InstancedPerActor
  NetExecutionPolicy: LocalPredicted
  AbilityTags: { Ability.Skill.Heal }
  ActivationOwnedTags: { State.Channeling }
  ActivationBlockedTags: { State.Dead, Cooldown.Heal }
  CooldownGE: GE_Heal_Cooldown (Duration 20초, Cooldown.Heal 태그)

  구현:
    1. CommitAbility
    2. PlayMontageAndWait (채널링 3초)
    3. WaitGameplayTagAdded(State.Hit) → 태그 감지 시 취소
    4. 몽타주 완료 → GE_Heal_Effect 적용 (Health +30, Instant)
    5. 인터럽트 → EndAbility(bWasCancelled = true)
```

### Shield (5초 지속, 피해 50% 감소, 30초 쿨타임)

```
GA_Shield:
  InstancingPolicy: InstancedPerActor
  NetExecutionPolicy: LocalPredicted
  AbilityTags: { Ability.Skill.Shield }
  ActivationOwnedTags: { State.Shielded }
  ActivationBlockedTags: { State.Dead, Cooldown.Shield }
  CooldownGE: GE_Shield_Cooldown (Duration 30초, Cooldown.Shield 태그)

  구현:
    1. CommitAbility
    2. GE_Shield_Active 적용 (Duration 5초, State.Shielded 태그 부여)
    3. PostGameplayEffectExecute에서 State.Shielded 태그 확인 → IncomingDamage * 0.5
    4. WaitDelay(5.0) 또는 WaitGameplayEffectRemoved
    5. EndAbility
```

---

## 9. EmploymentProj 4단계 구현 체크리스트

- [ ] GAS 플러그인 활성화 (Build.cs에 모듈 추가)
- [ ] `UAbilitySystemGlobals::Get().InitGlobalData()` 호출
- [ ] ASC를 PlayerState에 배치
  - [ ] `IAbilitySystemInterface` 구현 (Character)
  - [ ] `InitAbilityActorInfo` 호출 (PossessedBy + OnRep_PlayerState)
- [ ] AttributeSet 구현
  - [ ] HP, MaxHP, Stamina, Shield 정의
  - [ ] IncomingDamage 메타 어트리뷰트
  - [ ] PreAttributeChange (클램핑)
  - [ ] PostGameplayEffectExecute (데미지 파이프라인)
  - [ ] 복제 등록 (GetLifetimeReplicatedProps)
- [ ] GA_Dash 구현
  - [ ] Cost GE, Cooldown GE 생성
  - [ ] 이동 로직 (LaunchCharacter 또는 RootMotion)
- [ ] GA_Heal 구현
  - [ ] 채널링 (PlayMontageAndWait 또는 WaitDelay)
  - [ ] 피격 시 취소 (WaitGameplayTagAdded)
  - [ ] 힐 GE 적용
- [ ] GA_Shield 구현
  - [ ] Duration GE (State.Shielded 태그)
  - [ ] 데미지 감소 로직 (PostGameplayEffectExecute)
- [ ] 태그 체계 정의
  - [ ] Ability.Skill.Dash / Heal / Shield
  - [ ] State.Dashing / Channeling / Shielded / Dead
  - [ ] Cooldown.Dash / Heal / Shield

---

## 10. 참고 자료

### 공식 문서
- GAS 개요: `dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-ability-system-for-unreal-engine`
- Gameplay Attributes and Effects: `dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-attributes-and-gameplay-effects-for-unreal-engine`
- Gameplay Tags: `dev.epicgames.com/documentation/en-us/unreal-engine/gameplay-tags-in-unreal-engine`

### 커뮤니티 (필수 읽기)
- **tranek/GASDocumentation**: `github.com/tranek/GASDocumentation` - GAS 커뮤니티 바이블. UE4 기반이지만 UE5에도 95% 적용됨

### UE5 소스 코드
- `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/AbilitySystemComponent.h`
- `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/GameplayAbility.h`
- `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/GameplayEffect.h`
- `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/AttributeSet.h`
- `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/Abilities/Tasks/` - 모든 AbilityTask

### 참고 프로젝트
- **Lyra Starter Game** (UE5 기본 제공) - 프로덕션 수준의 GAS 구현
- **Action RPG Sample** - 간단한 GAS 예제

### 검색 키워드
- "UE5 GAS tutorial C++"
- "UE5 AbilitySystemComponent PlayerState setup"
- "UE5 GameplayEffect instant vs duration"
- "UE5 AttributeSet damage pipeline meta attribute"
- "UE5 GAS prediction LocalPredicted"
- "tranek GASDocumentation"
