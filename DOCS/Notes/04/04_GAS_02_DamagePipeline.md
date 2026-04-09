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

> `TAG_Data_Damage`는 Foundation(Step 6, EPNativeGameplayTags.h)에서 이미 정의됨 — 별도 추가 불필요.

### Step 2 — ApplyGEDamage 헬퍼 구현

GA 외부(SSR/CombatComponent)에서 GE를 적용할 때는 `UEPCombatComponent`의 `private static` 멤버 함수로 구현.

```cpp
// EPCombatComponent.h — private
private:
    static void ApplyGEDamage(
        AActor* Target,
        AActor* Instigator,
        TSubclassOf<UGameplayEffect> GEClass,
        float FinalDamage);
```

```cpp
// EPCombatComponent.cpp
void UEPCombatComponent::ApplyGEDamage(
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
// EPCombatComponent.h — public 또는 protected
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
class EMPLOYMENTPROJ_API UEPGA_Death : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UEPGA_Death();

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
UEPGA_Death::UEPGA_Death()
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

void UEPGA_Death::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // State.Dead 태그를 Infinite GE로 부여 — 복제 보장
    if (GE_StateDeadClass)
    {
        FActiveGameplayEffectHandle DeadHandle =
            ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
                GE_StateDeadClass.GetDefaultObject(), 1.f);
    }

    if (AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get()))
    {
        // 무기 숨김 — 래그돌 위에 무기가 떠있는 현상 방지
        if (UEPCombatComponent* CC = Char->GetCombatComponent())
        {
            if (AEPWeapon* Weapon = CC->GetEquippedWeapon())
            {
                Weapon->SetActorHiddenInGame(true);
                Weapon->SetActorEnableCollision(false);
            }
        }

        // GameMode 통보 — 킬 크레딧, 매치 상태 처리
        // TriggerEventData->Instigator: PostGEExecute에서 Payload.Instigator로 넘긴 SourceActor
        const AActor* Killer = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
        AController* KillerController = Killer ? Killer->GetInstigatorController() : nullptr;
        if (AEPGameMode* GM = Char->GetWorld()->GetAuthGameMode<AEPGameMode>())
            GM->OnPlayerKilled(KillerController, Char->GetController());

        // 래그돌 (Multicast)
        Char->Multicast_Die();

        // UnPossess — 컨트롤러가 계속 붙어있으면 죽어서도 입력이 들어옴
        if (AController* Controller = Char->GetController())
            Controller->UnPossess();
    }

    // EndAbility 반드시 호출 — 누락 시 Ability가 종료되지 않음
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
```

> **GA_Death Grant 위치**: `AEPCharacter::PossessedBy` 서버측.
>
> `GE_StateDeadClass`가 `EditDefaultsOnly`이므로 `UEPGA_Death::StaticClass()`를 직접 넘기면
> 항상 null이 된다. Blueprint 서브클래스(`BP_GA_Death`)를 만들어 에디터에서 GE를 할당한 뒤
> 그 클래스를 참조해야 한다.
>
> 나중에 기본 Ability가 늘어날 것을 감안해 `TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities`
> UPROPERTY를 Character에 두고 에디터에서 관리한다:
> ```cpp
> // EPCharacter.h — protected
> UPROPERTY(EditDefaultsOnly, Category = "GAS")
> TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;
> ```
> ```cpp
> // PossessedBy — GiveAbility 부분
> for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
> {
>     if (AbilityClass)
>         ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
> }
> ```
> `BP_EPCharacter`의 `DefaultAbilities` 배열에 `BP_GA_Death`를 추가하면 된다.
> 무기 장착/해제와 무관하게 항상 필요하므로 GA_PrimaryUse/GA_Reload(무기 Grant)와 달리
> Character Possess 시점에 한 번만 Grant. 리스폰 시 ASC는 PlayerState에 유지되므로 재Grant 불필요.

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

#### 배경

기존 `OnRep_HP`는 `HP` 변수가 복제될 때 클라이언트에서 자동 호출되어 UI를 갱신했다.  
`HP` UPROPERTY를 제거하면 이 경로가 사라지므로, ASC의 Attribute 변경 델리게이트로 교체해야 한다.

GAS는 Attribute가 변경될 때마다 `FOnGameplayAttributeValueChange` 델리게이트를 자동으로 브로드캐스트한다.  
클라이언트에서 이 델리게이트에 콜백을 등록해두면 `OnRep_HP`와 동일한 타이밍에 HP바를 갱신할 수 있다.

#### 역할 분리

| 위치 | 델리게이트 | 용도 |
|------|-----------|------|
| `AEPPlayerState::BeginPlay` | `HealthChanged` | **서버 로직** — 킬 크레딧, 게임모드 통보 등. Foundation에서 이미 등록됨 |
| `AEPCharacter::InitASC()` | `OnHealthChanged` | **로컬 UI 갱신** — HP바, 스크린 이펙트 등. 로컬 컨트롤러만 등록 |

> 같은 델리게이트에 둘 다 묶지 않는 이유: PlayerState의 콜백은 서버에서도 실행되어야 하고, UI 콜백은 로컬 클라이언트에서만 실행되어야 한다. 조건 분기로 한 곳에 몰아넣으면 나중에 서버/클 실행 경로가 섞여 디버깅이 어려워진다.

#### 등록 위치: `InitASC()`를 사용하는 이유

`BeginPlay`에 등록하면 ASC가 아직 초기화되지 않았을 수 있다 (PlayerState ASC는 `OnRep_PlayerState` / `PossessedBy` 이후에야 유효).  
Foundation에서 `InitASC()`가 이 타이밍을 보장하도록 설계되어 있으므로, UI 델리게이트 등록도 이 안에서 한다.

```cpp
// EPCharacter.cpp — InitASC() 내부
// IsLocallyControlled() 가드: 다른 플레이어 Character에는 등록하지 않음
// (서버에서 모든 Character의 InitASC가 호출되므로 가드 필수)
if (IsLocallyControlled())
{
    ASC->GetGameplayAttributeValueChangeDelegate(
        UEPAttributeSet::GetHealthAttribute()
    ).AddUObject(this, &AEPCharacter::OnHealthChanged);
}
```

#### 콜백 시그니처

```cpp
// EPCharacter.h — 선언
void OnHealthChanged(const FOnAttributeChangeData& Data);

// EPCharacter.cpp — 구현 예시
void AEPCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    // Data.NewValue: 변경 후 값
    // Data.OldValue: 변경 전 값
    // UI 위젯이나 PlayerController에 전달해 HP바 갱신
}
```

`FOnAttributeChangeData`는 `NewValue`, `OldValue`, `GEModData`(어떤 GE가 변경했는지) 세 필드를 가진다.  
HP바 갱신에는 `NewValue`와 MaxHealth Attribute 값을 함께 사용한다.

MaxHealth는 현재 AttributeSet에 없으므로 임시로 `AttributeSet->GetMaxHealth()` 또는 상수로 처리해도 무방.  
MaxHealth Attribute 추가는 이후 확장 시 진행.

#### 주의사항

- 델리게이트는 `UObject::AddUObject`로 등록하므로 `this`가 GC되면 자동 해제된다. 수동 해제(`RemoveAll`) 불필요.
- 단, `InitASC()`가 여러 번 호출되는 경조 (리스폰 등) 중복 등록될 수 있으므로, 필요 시 `RemoveAll(this)` 후 재등록하거나 한 번만 호출되도록 가드를 둔다.

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
        → 무기 숨김 (SetActorHiddenInGame / SetActorEnableCollision)
        → GM->OnPlayerKilled() (킬 크레딧)
        → Multicast_Die() (래그돌)
        → Controller->UnPossess()
        → EndAbility() 
```

---

## 6. 완료 체크리스트

- [ ] `GE_Damage` Blueprint 에셋 생성 (SetByCaller Data.Damage)
- [ ] `GE_State_Dead` Blueprint 에셋 생성 (Infinite, GrantedTags: State.Dead)
- [ ] `GA_Death` C++ 구현 + `EPCharacter::PossessedBy`에서 Grant
- [ ] `ApplyGEDamage` 헬퍼 구현 + `ApplyPointDamage` → `ApplyGEDamage` 교체
- [ ] PIE: 피격 → Health Attribute 감소 확인
- [ ] PIE: Health 0 → `State.Dead` 태그 복제 확인 (showdebug abilitysystem)
- [ ] PIE: `Multicast_Die()` 실행 → 래그돌 확인
- [ ] PIE: 무기 숨김, `GM->OnPlayerKilled()`, `UnPossess()` 동작 확인
- [ ] GE 파이프라인 동작 확인 후 `AEPCharacter::HP` / `MaxHP` / `TakeDamage()` / `OnRep_HP()` 제거
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
