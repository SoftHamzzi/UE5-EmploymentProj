# 기획서: 재장전 어빌리티 (GA_Item_Reload)

> 우선순위 4 — GA_Item_PrimaryUse 완료 후 진행.
> `Server_Reload` RPC + `StartReload/FinishReload` + `ReloadTimerHandle`을 GA로 대체.

---

## 1. 목표

- `AEPWeapon::StartReload` / `FinishReload` / `ReloadTimerHandle` 제거
- `UEPCombatComponent::Server_Reload` RPC 제거
- `GA_Item_Reload` — LocalPredicted, `State.Reloading` GE로 발사 차단 (복제 보장)
- 탄약 보충 → `GE_Reload_Ammo` (Instant GE, `Ammo` Attribute Override)
- `AEPWeapon::CurrentAmmo` / `MaxAmmo` UPROPERTY → `AttributeSet::Ammo` / `MaxAmmo`로 이전

완료 기준: 재장전 입력 → `State.Reloading` 태그 복제 → 발사 차단 → `ReloadTime` 경과 후 탄약 보충 → 태그 해제.

---

## 2. 현재 코드 상태

```cpp
// AEPWeapon
void StartReload();         // WeaponState = Reloading, SetTimer → FinishReload
void FinishReload();        // CurrentAmmo = MaxAmmo, WeaponState = Idle
FTimerHandle ReloadTimerHandle;
EEPWeaponState WeaponState;

UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo) uint8 CurrentAmmo = 0;
UPROPERTY(Replicated) uint8 MaxAmmo = 30;
```

---

## 3. 변경 대상 파일

| 파일 | 작업 |
|------|------|
| `EPAttributeSet.h/cpp` | `Ammo`, `MaxAmmo` Attribute 추가 |
| `EPWeapon.h/cpp` | StartReload/FinishReload/ReloadTimerHandle/WeaponState/CurrentAmmo/MaxAmmo 제거, GA Grant 추가 |
| `EPCombatComponent.h/cpp` | Server_Reload 제거 |
| `EPCharacter.cpp` | 재장전 입력 → TryActivateAbilitiesByTag |
| `GAS/GA_Item_Reload.h/cpp` | **신규** |
| `GAS/GE_Reloading` | **Blueprint 에셋** — Duration GE, GrantedTags: State.Reloading |
| `GAS/GE_Reload_Ammo` | **Blueprint 에셋** — Instant GE, Ammo Override |

---

## 4. 구현 순서

### Step 1 — AttributeSet에 Ammo 추가

```cpp
// EPAttributeSet.h 추가
UPROPERTY(BlueprintReadOnly, Category = "Attribute|Ammo",
    ReplicatedUsing = OnRep_Ammo)
FGameplayAttributeData Ammo;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, Ammo)

UPROPERTY(BlueprintReadOnly, Category = "Attribute|Ammo",
    ReplicatedUsing = OnRep_MaxAmmo)
FGameplayAttributeData MaxAmmo;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, MaxAmmo)
```

```cpp
// GetLifetimeReplicatedProps 추가
DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, Ammo,    COND_None, REPNOTIFY_Always);
DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, MaxAmmo, COND_None, REPNOTIFY_Always);

// OnRep 구현
void UEPAttributeSet::OnRep_Ammo(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, Ammo, OldValue);
}

// PreAttributeChange 클램핑 추가
if (Attribute == GetAmmoAttribute())
    NewValue = FMath::Clamp(NewValue, 0.f, GetMaxAmmo());
```

무기 장착 시 `InitAmmo` / `InitMaxAmmo` 호출:
```cpp
// AEPWeapon::EquipWeapon 서버측 — Attribute 초기화
if (UEPAttributeSet* AS = PS->GetAttributeSet())
{
    AS->InitAmmo(static_cast<float>(Weapon->WeaponDef->MaxAmmo));
    AS->InitMaxAmmo(static_cast<float>(Weapon->WeaponDef->MaxAmmo));
}
```

### Step 2 — GE_Reloading Blueprint 에셋 생성

```
Content/Data/GAS/GE_Reloading (GameplayEffect Blueprint):
- DurationPolicy: HasDuration
- DurationMagnitude: SetByCaller (Data.ReloadDuration)
- GrantedTags: State.Reloading
```

> **핵심**: `State.Reloading`을 Infinite GE가 아닌 Duration GE로 부여.
> 재장전 완료 시 자동으로 태그 해제됨.
> `ActivationOwnedTags`는 복제 안 되므로 이 GE가 필수.

### Step 3 — GE_Reload_Ammo Blueprint 에셋 생성

```
Content/Data/GAS/GE_Reload_Ammo (GameplayEffect Blueprint):
- DurationPolicy: Instant
- Modifiers:
    Attribute:    EPAttributeSet.Ammo
    ModifierOp:   Override
    MagnitudeCalc: AttributeBased
        SourceAttribute: EPAttributeSet.MaxAmmo
        AttributeSource: Source (자기 자신)
        bSnapshot: false
```

### Step 4 — GA_Item_Reload 구현

```cpp
// GAS/GA_Item_Reload.h
UCLASS()
class EMPLOYMENTPROJ_API UGA_Item_Reload : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Item_Reload();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_ReloadingClass;   // Duration GE

    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    TSubclassOf<UGameplayEffect> GE_ReloadAmmoClass;  // Instant GE

private:
    FActiveGameplayEffectHandle ReloadingEffectHandle;
    FTimerHandle ReloadTimerHandle;

    UFUNCTION()
    void OnReloadComplete(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo);
};
```

```cpp
// GAS/GA_Item_Reload.cpp
UGA_Item_Reload::UGA_Item_Reload()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    bServerRespectsRemoteAbilityCancellation = false;

    // 이 태그가 있으면 재장전 불가
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_UsingItem);
    // State.Reloading이 있으면 재장전 중복 방지
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);
}

void UGA_Item_Reload::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AEPCharacter* Char  = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
    AEPWeapon* Weapon   = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;

    if (!Char || !Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    const float ReloadTime = Weapon->WeaponDef->ReloadTime;

    // GE_Reloading 적용 — State.Reloading 태그 복제 (Duration = ReloadTime)
    if (GE_ReloadingClass)
    {
        FGameplayEffectSpecHandle SpecHandle =
            MakeOutgoingGameplayEffectSpec(GE_ReloadingClass, GetAbilityLevel());
        SpecHandle.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_ReloadDuration, ReloadTime);
        ReloadingEffectHandle =
            ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
    }

    // WaitDelay AbilityTask 사용 — SetTimer보다 GAS 생명주기와 통합됨
    // C++에서는 ReadyForActivation() 반드시 수동 호출
    UAbilityTask_WaitDelay* WaitTask =
        UAbilityTask_WaitDelay::WaitDelay(this, ReloadTime);
    WaitTask->OnFinish.AddDynamic(this, &UGA_Item_Reload::OnReloadComplete_Task);
    WaitTask->ReadyForActivation(); // C++에서는 필수 — Blueprint와 달리 자동 호출 안 됨
}

void UGA_Item_Reload::OnReloadComplete_Task()
{
    // 이 함수는 GA의 SpecHandle/ActorInfo/ActivationInfo에 직접 접근 가능
    // (InstancedPerActor이므로 안전)
    const FGameplayAbilitySpecHandle Handle     = GetCurrentAbilitySpecHandle();
    const FGameplayAbilityActorInfo* ActorInfo  = GetCurrentActorInfo();
    const FGameplayAbilityActivationInfo ActivInfo = GetCurrentActivationInfo();

    // 탄약 보충 GE 적용 (서버만)
    if (ActorInfo->IsNetAuthority() && GE_ReloadAmmoClass)
    {
        ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivInfo,
            GE_ReloadAmmoClass.GetDefaultObject(), 1.f);
    }

    EndAbility(Handle, ActorInfo, ActivInfo, true, false);
}

void UGA_Item_Reload::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // GE_Reloading 수동 제거 (Duration이 남아있을 때 취소된 경우)
    if (ReloadingEffectHandle.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(ReloadingEffectHandle);
        ReloadingEffectHandle.Invalidate();
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
```

> **WaitDelay vs SetTimer**:
> AbilityTask(`WaitDelay`)를 사용하면 GA가 취소될 때 자동으로 Task도 정리됨.
> `SetTimer` + 람다는 GA 생명주기와 분리되어 GA 취소 시 타이머가 남을 수 있음.
> C++에서는 `ReadyForActivation()` 수동 호출 필수 (Blueprint는 자동 호출).

### Step 5 — AEPWeapon: GA Grant + 기존 코드 제거

```cpp
// 제거
void StartReload();
void FinishReload();
FTimerHandle ReloadTimerHandle;
EEPWeaponState WeaponState;
UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo) uint8 CurrentAmmo;
UPROPERTY(Replicated) uint8 MaxAmmo;
void OnRep_CurrentAmmo();

// 장착 시 GA_Reload도 Grant
FGameplayAbilitySpecHandle GrantedReloadHandle; // private

void AEPWeapon::EquipWeapon(AEPCharacter* NewOwner)
{
    // ... GA_PrimaryUse Grant (PrimaryUse 기획서 참조)

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (UAbilitySystemComponent* ASC = NewOwner->GetAbilitySystemComponent())
        {
            FGameplayAbilitySpec ReloadSpec(UGA_Item_Reload::StaticClass(), 1);
            ReloadSpec.DynamicAbilityTags.AddTag(EmpGameplayTags::TAG_Ability_Item_Reload);
            GrantedReloadHandle = ASC->GiveAbility(ReloadSpec);
        }
    }
}

void AEPWeapon::UnequipWeapon()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (UAbilitySystemComponent* ASC = ...)
        {
            ASC->ClearAbility(GrantedPrimaryUseHandle);
            ASC->ClearAbility(GrantedReloadHandle);
        }
    }
}
```

### Step 6 — 입력 연동

```cpp
// EPCharacter.cpp
void AEPCharacter::Input_Reload(const FInputActionValue& Value)
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        ASC->TryActivateAbilitiesByTag(
            FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Item_Reload));
    }
}
```

---

## 5. GA Tags 참조

| Tag Container | 설명 | 이 GA에서 사용 |
|---|---|---|
| `Activation Blocked Tags` | 이 태그가 있으면 활성화 불가 | `State.Dead`, `State.UsingItem`, `State.Reloading` |
| `Activation Owned Tags` | GA 활성 중 부여 (복제 안 됨) | (미사용) |

> `State.Reloading`은 `GE_Reloading` (Duration GE)으로 부여해야 복제됨.
> GA가 끝나면 Duration GE도 `EndAbility`에서 수동 제거.

---

## 6. 완료 체크리스트

- [ ] `Server_Reload` RPC 제거 후 컴파일
- [ ] `StartReload` / `FinishReload` / `ReloadTimerHandle` 제거
- [ ] `WeaponState` 제거
- [ ] `CurrentAmmo` / `MaxAmmo` UPROPERTY → AttributeSet 이전
- [ ] `GE_Reloading` Blueprint 에셋 (Duration, State.Reloading 태그)
- [ ] `GE_Reload_Ammo` Blueprint 에셋 (Instant, Ammo Override)
- [ ] 재장전 입력 → GA 활성화 → `State.Reloading` 태그 복제 확인
- [ ] 재장전 중 발사 차단 확인 (GA_PrimaryUse ActivationBlockedTags)
- [ ] `ReloadTime` 경과 후 `Ammo` Attribute = `MaxAmmo` 확인
- [ ] GA 취소 시 `State.Reloading` 태그 즉시 해제 확인
- [ ] 무기 교체 시 GA 누수 없음 확인

---

## 7. 함정 & 주의사항

| 상황 | 원인 | 해결 |
|------|------|------|
| 재장전 중 발사 안 막힘 | State.Reloading이 복제 안 됨 | ActivationOwnedTags 대신 GE_Reloading 사용 |
| WaitDelay 작동 안 함 | `ReadyForActivation()` 미호출 | C++에서는 반드시 수동 호출 |
| GA 취소 시 State.Reloading 태그 남음 | EndAbility에서 GE 미제거 | `RemoveActiveGameplayEffect(ReloadingEffectHandle)` 호출 |
| 탄약 보충 안 됨 | GE_Reload_Ammo의 MaxAmmo Capture 실패 | AttributeSource = Source 확인 |
| 재장전 두 번 발동 | ActivationBlockedTags에 State.Reloading 누락 | ActivationBlockedTags 확인 |
