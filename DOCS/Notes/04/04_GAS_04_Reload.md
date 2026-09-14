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
// UEPCombatComponent::EquipWeapon 서버측 — Attribute 초기화
// (AEPWeapon에는 EquipWeapon이 없음 — Step 5 참고)
// PS는 컴포넌트 스코프에 없으므로 Owner(AEPCharacter*)를 통해 얻어야 함
AEPCharacter* Owner = GetOwnerCharacter();
if (AEPPlayerState* PS = Owner ? Owner->GetPlayerState<AEPPlayerState>() : nullptr)
{
    if (UEPAttributeSet* AS = PS->GetAttributeSet())
    {
        AS->InitAmmo(static_cast<float>(NewWeapon->WeaponDef->MaxAmmo));
        AS->InitMaxAmmo(static_cast<float>(NewWeapon->WeaponDef->MaxAmmo));
    }
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

    UFUNCTION()
    void OnReloadComplete_Task();
};
```

```cpp
// GAS/GA_Item_Reload.cpp
// #include "Abilities/Tasks/AbilityTask_WaitDelay.h" 추가 필요

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

### Step 5 — AEPWeapon 기존 코드 제거 + CombatComponent: GA Grant 일반화

```cpp
// AEPWeapon.h에서 제거
void StartReload();
void FinishReload();
FTimerHandle ReloadTimerHandle;
EEPWeaponState WeaponState;
UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo) uint8 CurrentAmmo;
UPROPERTY(Replicated) uint8 MaxAmmo;
void OnRep_CurrentAmmo();
```

> **주의**: GA Grant는 `AEPWeapon`이 아니라 `UEPCombatComponent::EquipWeapon/UnequipWeapon`에서 처리한다
> (PrimaryUse 기획서에서 확정된 위치 — `AEPWeapon`에는 `EquipWeapon` 메서드 자체가 없음).

> **개선 — 핸들 1개씩 늘리는 방식 대신 배열로 일반화**
> PrimaryUse 구현 당시 `GrantedPrimaryUseHandle` 단일 핸들 + `PrimaryUseAbilityClass` 단일 필드로 처리했다.
> Reload GA가 추가되며 같은 패턴을 반복하면 어빌리티가 늘어날 때마다 핸들/필드가 계속 늘어난다.
> Lyra의 `UAbilitySet` + `FAbilitySet_GrantedHandles` 패턴처럼 **배열로 일반화**하는 것이 더 낫다:

```cpp
// EPWeaponDefinition.h
// 기존 PrimaryUseAbilityClass(TSubclassOf<UEPGA_Item_PrimaryUse>) 제거하고 아래로 통합
UPROPERTY(EditDefaultsOnly, Category = "GAS")
TArray<TSubclassOf<UGameplayAbility>> WeaponAbilities; // PrimaryUse, Reload, ...
```

```cpp
// EPCombatComponent.h
// 기존 GrantedPrimaryUseHandle(단일 핸들) 제거하고 아래로 통합
TArray<FGameplayAbilitySpecHandle> GrantedWeaponAbilityHandles; // private
```

```cpp
// EPCombatComponent.cpp
void UEPCombatComponent::EquipWeapon(AEPWeapon* NewWeapon)
{
    // ... 기존 장착 로직 ...

    AEPCharacter* Owner = GetOwnerCharacter();
    if (GetOwner()->HasAuthority() && Owner && NewWeapon->WeaponDef)
    {
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
        {
            // 재장착 시 이전 핸들 일괄 제거 — 중복 Grant 방지
            for (const FGameplayAbilitySpecHandle& Handle : GrantedWeaponAbilityHandles)
                if (Handle.IsValid())
                    ASC->ClearAbility(Handle);
            GrantedWeaponAbilityHandles.Reset();

            for (const TSubclassOf<UGameplayAbility>& AbilityClass : NewWeapon->WeaponDef->WeaponAbilities)
            {
                if (!AbilityClass) continue;

                FGameplayAbilitySpec Spec(AbilityClass, 1);
                GrantedWeaponAbilityHandles.Add(ASC->GiveAbility(Spec));
            }
        }
    }
}

void UEPCombatComponent::UnequipWeapon()
{
    // ... 기존 해제 로직 ...

    AEPCharacter* Owner = GetOwnerCharacter();
    if (GetOwner()->HasAuthority() && Owner)
    {
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
            for (const FGameplayAbilitySpecHandle& Handle : GrantedWeaponAbilityHandles)
                if (Handle.IsValid())
                    ASC->ClearAbility(Handle);
    }
    GrantedWeaponAbilityHandles.Reset();
}
```

> **AbilityTags 식별은 GA 생성자에서 처리**: 배열 방식에서는 `Spec`에 `DynamicAbilityTags`를 추가하지 않는다.
> 대신 각 GA의 생성자에서 `SetAssetTags()`로 `TAG_Ability_Item_PrimaryUse` / `TAG_Ability_Item_Reload`를 직접 설정한다
> (PrimaryUse 구현 패턴과 동일). `TryActivateAbilitiesByTag`는 이 태그로 GA를 찾는다.

> **마이그레이션 비용**: 이 변경은 이미 구현된 PrimaryUse Grant 로직(`GrantedPrimaryUseHandle`, `PrimaryUseAbilityClass`)도
> 함께 배열 방식으로 옮겨야 함을 의미한다. 어빌리티가 PrimaryUse + Reload 2개뿐이라면 굳이 리팩토링하지 않고
> `GrantedReloadHandle` + `ReloadAbilityClass`를 PrimaryUse와 동일한 패턴으로 하나 더 추가하는 것도 충분하다.
> 향후 무기별 어빌리티가 3개 이상으로 늘어날 가능성이 있을 때 배열 방식으로 리팩토링 권장.

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
