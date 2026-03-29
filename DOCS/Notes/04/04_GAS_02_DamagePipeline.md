# 기획서: 데미지 / HP 파이프라인 GAS 이관

> 우선순위 2 — 기반 세팅(Foundation) 완료 후 진행.
> `TakeDamage` + 수동 HP 관리를 GE 기반으로 전환.

---

## 1. 목표

- `AEPCharacter::TakeDamage()` 오버라이드 제거
- `HP` / `MaxHP` UPROPERTY 제거 → `UEPAttributeSet::Health` 사용
- 피격 경로: `SSR::ConfirmHitscan` → `GE_Damage` 적용 → `PostGameplayEffectExecute` → 사망 이벤트
- `GA_Death` — `Event.Death` 트리거로 자동 활성화, `GE_State_Dead` (Infinite)로 State.Dead 복제
- `Die()` / `Multicast_Die()` → GA_Death 내부로 이전

완료 기준: 피격 → Health Attribute 감소 → 0이 되면 `Multicast_Die` 실행, `State.Dead` 복제 확인.

---

## 2. 현재 코드 상태

```cpp
// EPCharacter.h
UPROPERTY(ReplicatedUsing = OnRep_HP) int32 HP = 100;
int32 MaxHP = 100;
FORCEINLINE bool IsDead() const { return HP <= 0; }
virtual float TakeDamage(...) override;
void Die(AController* Killer);
UFUNCTION(NetMulticast, Reliable) void Multicast_Die();
```

```cpp
// EPCombatComponent.cpp — 현재
UGameplayStatics::ApplyPointDamage(HitChar, FinalDamage, ...);
```

---

## 3. 변경 대상 파일

| 파일 | 작업 |
|------|------|
| `EPCharacter.h/cpp` | HP/MaxHP/TakeDamage/OnRep_HP 제거, IsDead 수정, Die 제거 |
| `EPCombatComponent.h/cpp` | ApplyPointDamage → ApplyGEDamage 헬퍼, GE_DamageClass UPROPERTY 추가 |
| `EPAttributeSet.cpp` | PostGameplayEffectExecute에서 사망 이벤트 발송 (Foundation에서 구현) |
| `GAS/GE_Damage` | **Blueprint 에셋** — Instant GE, IncomingDamage SetByCaller |
| `GAS/GE_State_Dead` | **Blueprint 에셋** — Infinite GE, GrantedTags: State.Dead |
| `GAS/GA_Death.h/cpp` | **신규** — Event.Death 트리거, ServerOnly |

---

## 4. 구현 순서

### Step 1 — GE_Damage Blueprint 에셋 생성

```
Content/Data/GAS/GE_Damage (GameplayEffect Blueprint):
- DurationPolicy: Instant
- Modifiers:
    Attribute:    EPAttributeSet.IncomingDamage
    ModifierOp:   Add
    MagnitudeCalc: SetByCaller
    DataTag:      Data.Damage
```

> `Data.Damage` 태그를 NativeGameplayTags에 추가:
> ```cpp
> UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage)
> UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage, "Data.Damage")
> ```

### Step 2 — ApplyGEDamage 헬퍼 구현

GA 외부(SSR/CombatComponent)에서 GE를 적용할 때는 Target의 ASC에서 직접 호출.

```cpp
// EPCombatComponent.cpp
static void ApplyGEDamage(
    AActor* Target,
    AActor* Instigator,
    TSubclassOf<UGameplayEffect> GEClass,
    float FinalDamage)
{
    if (!Target || !GEClass) return;

    IAbilitySystemInterface* TargetIF = Cast<IAbilitySystemInterface>(Target);
    UAbilitySystemComponent* TargetASC = TargetIF ? TargetIF->GetAbilitySystemComponent() : nullptr;
    IAbilitySystemInterface* InstigatorIF = Cast<IAbilitySystemInterface>(Instigator);
    UAbilitySystemComponent* InstigatorASC = InstigatorIF ? InstigatorIF->GetAbilitySystemComponent() : nullptr;
    if (!TargetASC || !InstigatorASC) return;

    FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
    Context.AddInstigator(Instigator, Instigator);

    FGameplayEffectSpecHandle Spec =
        InstigatorASC->MakeOutgoingSpec(GEClass, 1.f, Context);
    if (!Spec.IsValid()) return;

    // GameplayTag 버전 SetByCaller 사용 (FName 버전보다 권장 — 오타 방지)
    Spec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Damage, FinalDamage);

    InstigatorASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
}
```

`CombatComponent`에 UPROPERTY 추가:
```cpp
UPROPERTY(EditDefaultsOnly, Category = "GAS")
TSubclassOf<UGameplayEffect> GE_DamageClass;
```

### Step 3 — ApplyPointDamage 교체

```cpp
// 기존
UGameplayStatics::ApplyPointDamage(HitChar, FinalDamage, ...);

// 변경 후
ApplyGEDamage(HitChar, OwnerChar, GE_DamageClass, FinalDamage);
```

### Step 4 — GE_State_Dead Blueprint 에셋 생성

```
Content/Data/GAS/GE_State_Dead (GameplayEffect Blueprint):
- DurationPolicy: Infinite
- GrantedTags: State.Dead
```

> **주의**: `GA_Death`의 `ActivationOwnedTags`에 `State.Dead`를 넣어도 되지만,
> `ActivationOwnedTags`는 복제되지 않아 시뮬레이티드 프록시에서 보이지 않음.
> 다른 클라이언트에서 `State.Dead` 여부를 쿼리해야 한다면 Infinite GE로 부여해야 함.

### Step 5 — GA_Death 구현

```cpp
// GAS/GA_Death.h
UCLASS()
class EMPLOYMENTPROJ_API UGA_Death : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Death();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_StateDeadClass;
};
```

```cpp
// GAS/GA_Death.cpp
UGA_Death::UGA_Death()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // Server Respects Remote Ability Cancellation 비활성화 권장
    // (클라이언트 완료가 서버를 강제 종료하는 오동작 방지)
    bServerRespectsRemoteAbilityCancellation = false;

    // Event.Death 발생 시 자동 활성화
    FAbilityTriggerData Trigger;
    Trigger.TriggerTag    = EmpGameplayTags::TAG_Event_Death;
    Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(Trigger);
}

void UGA_Death::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // State.Dead 태그를 Infinite GE로 부여 — 복제 보장
    if (GE_StateDeadClass)
    {
        ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
            GE_StateDeadClass.GetDefaultObject(), 1.f);
    }

    // 사망 처리 (Multicast)
    if (AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get()))
    {
        Char->Multicast_Die();
    }

    // EndAbility 반드시 호출 — 누락 시 Ability가 종료되지 않음
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
```

> **GA_Death Grant 위치**: `AEPPlayerState::PossessedBy` 또는 Character BeginPlay 서버측에서
> `ASC->GiveAbility(FGameplayAbilitySpec(UGA_Death::StaticClass(), 1))` 호출.
> 무기 장착/해제와 무관하게 항상 활성화되어야 하므로 PlayerState에서 한 번만 Grant.

### Step 6 — EPCharacter HP 코드 제거

```cpp
// 제거
UPROPERTY(ReplicatedUsing = OnRep_HP) int32 HP;
int32 MaxHP;
void OnRep_HP();
virtual float TakeDamage(...) override;
void Die(AController* Killer);
```

```cpp
// IsDead 수정
FORCEINLINE bool IsDead() const
{
    if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        return ASC->HasMatchingGameplayTag(EmpGameplayTags::TAG_State_Dead);
    return false;
}
```

### Step 7 — UI 연동

`OnRep_HP` 제거 후 Attribute 변경 델리게이트로 교체:

```cpp
// InitASC() 또는 BeginPlay — 클라이언트 측
if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
{
    ASC->GetGameplayAttributeValueChangeDelegate(
        UEPAttributeSet::GetHealthAttribute()
    ).AddUObject(this, &AEPCharacter::OnHealthChanged);
}
```

---

## 5. 데이터 흐름

```
[SSR::ConfirmHitscan] 히트 확정
    → ApplyGEDamage(Target, Instigator, GE_Damage, FinalDamage)
        → SetSetByCallerMagnitude(TAG_Data_Damage, FinalDamage)
        → InstigatorASC->ApplyGameplayEffectSpecToTarget(...)
    → AttributeSet::PostGameplayEffectExecute
        → IncomingDamage 소모 → Health 감소
        → Health <= 0 → HandleGameplayEvent(EmpGameplayTags::TAG_Event_Death, &Payload)
    → GA_Death::ActivateAbility (ServerOnly, Event.Death 트리거)
        → GE_State_Dead (Infinite) 적용 → State.Dead 복제
        → Multicast_Die()
        → EndAbility()
```

---

## 6. 완료 체크리스트

- [ ] `GE_Damage` Blueprint 에셋 생성 (SetByCaller Data.Damage)
- [ ] `GE_State_Dead` Blueprint 에셋 생성 (Infinite, GrantedTags: State.Dead)
- [ ] `AEPCharacter::HP` / `MaxHP` UPROPERTY 제거 후 컴파일
- [ ] `TakeDamage()` / `OnRep_HP()` 제거
- [ ] `ApplyPointDamage` → `ApplyGEDamage` 교체
- [ ] `GA_Death` Grant 확인 (PlayerState 서버측)
- [ ] PIE: 피격 → Health Attribute 감소 확인
- [ ] PIE: Health 0 → `State.Dead` 태그 복제 확인 (showdebug abilitysystem)
- [ ] PIE: `Multicast_Die()` 실행 → 래그돌 확인
- [ ] `IsDead()` = `ASC->HasMatchingGameplayTag(EmpGameplayTags::TAG_State_Dead)` 동작 확인

---

## 7. 함정 & 주의사항

| 상황 | 원인 | 해결 |
|------|------|------|
| GE 적용 후 Health 안 줄어듦 | Data.Damage 태그 누락 또는 SetByCaller 미주입 | Spec에 SetSetByCallerMagnitude 호출 확인 |
| 사망 이벤트 두 번 발생 | PostGEExecute 중복 진입 | Health 이미 0이면 얼리 리턴 추가 |
| State.Dead 다른 클라에서 안 보임 | ActivationOwnedTags는 복제 안 됨 | GE_State_Dead (Infinite) 사용 |
| GA_Death 서버에서 활성화 안 됨 | GA Grant 누락 | PlayerState 서버측에서 GiveAbility 확인 |
| `ApplyGEDamage` 클라에서 호출 | ConfirmHitscan 외 경로 추가 시 | `GetOwner()->HasAuthority()` 가드 필수 |
