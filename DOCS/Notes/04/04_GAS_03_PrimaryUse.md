# 기획서: 발사 어빌리티 (GA_Item_PrimaryUse + 입력 연동)

> 우선순위 3 — 기반 세팅 + 데미지 파이프라인 완료 후 진행.
> `Server_Fire` RPC를 `GA_Item_PrimaryUse`로 대체.
> 입력 연동(3-1)을 함께 처리.

---

## 1. 목표

- `UEPCombatComponent::Server_Fire` RPC + `LastServerFireTime` 제거
- `GA_Item_PrimaryUse` (C++) — LocalPredicted, Cooldown GE 기반 FireRate 관리
- 검증 3단계(FireRate/CanFire/OriginDrift) + BallisticType 분기 → GA 내부로 이전
- `Input_Fire` → `ASC->TryActivateAbilitiesByTag(EmpGameplayTags::TAG_Ability_Item_PrimaryUse)`
- 무기 장착 시 GA Grant (서버), 해제 시 Remove

완료 기준: 발사 입력 → GA 활성화 → Cooldown GE로 FireRate 제어 → 히트스캔/투사체 정상 판정.

---

## 2. 현재 코드 상태

```
[Input_Fire]
    → CombatComponent->RequestFire(Origin, Direction, ClientFireTime)
    → Server_Fire RPC
        → 검증 1: FireRate (LastServerFireTime 기반)
        → 검증 2: CanFire() (WeaponState == Idle, CurrentAmmo > 0)
        → 검증 3: OriginDrift (200cm)
        → BallisticType switch → HandleHitscanFire / HandleProjectileFire
        → Multicast_PlayMuzzleEffect
```

---

## 3. 변경 대상 파일

| 파일 | 작업 |
|------|------|
| `EPCharacter.cpp` | Input_Fire → TryActivateAbilitiesByTag |
| `EPCombatComponent.h/cpp` | Server_Fire/LastServerFireTime/RequestFire 제거 |
| `EPWeapon.h/cpp` | EquipWeapon: GA Grant, UnequipWeapon: GA Remove |
| `GAS/GA_Item_PrimaryUse.h/cpp` | **신규** |
| `GAS/GE_FireCooldown` | **Blueprint 에셋** — Duration GE, SetByCaller 쿨타임 |

---

## 4. 구현 순서

### Step 1 — GE_FireCooldown Blueprint 에셋 생성

```
Content/Data/GAS/GE_FireCooldown (GameplayEffect Blueprint):
- DurationPolicy: HasDuration
- DurationMagnitude: SetByCaller (Data.Cooldown)
- GrantedTags: Cooldown.Weapon.PrimaryUse  ← 쿨타임 식별 태그
```

> Cooldown GE는 GE 자체에 `GrantedTags`로 고유 태그를 가져야 함.
> GA는 이 태그의 존재 여부로 쿨타임 상태를 확인.
> `Data.Cooldown` 태그도 NativeGameplayTags에 추가 필요.

### Step 2 — GA_Item_PrimaryUse 구현

```cpp
// GAS/GA_Item_PrimaryUse.h
UCLASS()
class EMPLOYMENTPROJ_API UGA_Item_PrimaryUse : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Item_PrimaryUse();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    // Cooldown GE 재사용 패턴: GetCooldownTags + ApplyCooldown 오버라이드
    virtual const FGameplayTagContainer* GetCooldownTags() const override;
    virtual void ApplyCooldown(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
    FScalableFloat CooldownDuration; // WeaponDef->FireRate로 런타임 설정

    UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
    FGameplayTagContainer CooldownTags; // Cooldown.Weapon.PrimaryUse

    // GetCooldownTags() 반환용 임시 컨테이너 (CDO에 쓰이므로 Transient)
    UPROPERTY(Transient)
    mutable FGameplayTagContainer TempCooldownTags;

private:
    FGameplayAbilitySpecHandle GrantedHandle;
    friend class AEPWeapon;
};
```

```cpp
// GAS/GA_Item_PrimaryUse.cpp
UGA_Item_PrimaryUse::UGA_Item_PrimaryUse()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 클라이언트 완료가 서버를 강제 종료하는 오동작 방지 — 비활성화 권장
    bServerRespectsRemoteAbilityCancellation = false;

    // 이 태그가 있으면 발사 불가 (GAS Tag Container 자동 검사)
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);
}

// Cooldown 재사용 패턴 Step A: 현재 쿨타임 태그 반환
const FGameplayTagContainer* UGA_Item_PrimaryUse::GetCooldownTags() const
{
    FGameplayTagContainer* MutableTags = const_cast<FGameplayTagContainer*>(&TempCooldownTags);
    MutableTags->Reset(); // CDO에 쓰이므로 매 호출마다 초기화
    if (const FGameplayTagContainer* ParentTags = Super::GetCooldownTags())
        MutableTags->AppendTags(*ParentTags);
    MutableTags->AppendTags(CooldownTags);
    return MutableTags;
}

// Cooldown 재사용 패턴 Step B: SetByCaller로 쿨타임 Duration 주입
void UGA_Item_PrimaryUse::ApplyCooldown(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo) const
{
    UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
    if (!CooldownGE) return;

    FGameplayEffectSpecHandle SpecHandle =
        MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());

    // 쿨타임 식별 태그 동적 추가
    SpecHandle.Data->DynamicGrantedTags.AppendTags(CooldownTags);

    // FireRate 기반 Duration 주입 (무기에서 읽음)
    const AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
    const AEPWeapon* Weapon  = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
    const float Duration     = Weapon ? (1.f / Weapon->WeaponDef->FireRate) : 0.2f;

    SpecHandle.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, Duration);
    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}

void UGA_Item_PrimaryUse::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AEPCharacter* Char   = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
    AEPWeapon* Weapon    = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;

    if (!Char || !Weapon || !Weapon->CanFire())
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    // CommitAbility: CheckCost + CheckCooldown 재확인 후 Cooldown GE 적용
    // 실패 시 Ability를 취소해야 함
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    const FVector Origin   = Char->GetFirstPersonCamera()->GetComponentLocation();
    const float ClientTime = Char->GetGameState<AGameStateBase>()
                                 ->GetServerWorldTimeSeconds();

    // 서버 전용: 실제 대미지 판정
    if (ActorInfo->IsNetAuthority())
    {
        // Origin drift 검증 (200cm)
        // BallisticType switch → HandleHitscanFire / HandleProjectileFire
        UEPCombatComponent* Combat = Char->GetCombatComponent();
        Combat->HandleServerFire(Origin, Char->GetControlRotation().Vector(), ClientTime);
    }

    // 로컬 코스메틱 (LocalPredicted: 클라/서버 양쪽 실행)
    // 시뮬레이티드 프록시는 GA가 실행되지 않으므로 Multicast_PlayMuzzleEffect로 전달
    UEPCombatComponent* Combat = Char->GetCombatComponent();
    if (Combat)
    {
        Combat->PlayLocalMuzzleEffect(Origin);
        Combat->SpawnLocalCosmeticProjectile(Origin, Char->GetControlRotation().Vector());
    }

    // EndAbility 반드시 호출 — 누락 시 Ability가 종료되지 않음
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
```

> **LocalPredicted 주의**:
> - `ActivateAbility`는 소유 클라이언트와 서버 양쪽에서 실행됨
> - 시뮬레이티드 프록시에서는 GA가 실행되지 않음 → 코스메틱은 Multicast RPC 또는 GameplayCue로 처리
> - `IsNetAuthority()` 로 서버 전용 로직 가드

### Step 3 — CombatComponent 정리

```cpp
// 제거
UFUNCTION(Server, Reliable) void Server_Fire(...);
float LastServerFireTime;
void RequestFire(...);

// HandleServerFire로 이름 변경 (GA에서 호출)
// public으로 접근 가능하도록 변경
void HandleServerFire(const FVector& Origin, const FVector& Direction, float ClientFireTime);

// 유지 (GA 내부에서 호출)
void HandleHitscanFire(...);
void HandleProjectileFire(...);
void PlayLocalMuzzleEffect(...);
void SpawnLocalCosmeticProjectile(...);
```

### Step 4 — AEPWeapon: GA Grant / Remove

```cpp
// EPWeapon.h — private
FGameplayAbilitySpecHandle GrantedPrimaryUseHandle;

// EPWeapon.cpp
void AEPWeapon::EquipWeapon(AEPCharacter* NewOwner)
{
    // ... 기존 장착 로직

    // 서버에서만 Grant
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (UAbilitySystemComponent* ASC = NewOwner->GetAbilitySystemComponent())
        {
            FGameplayAbilitySpec Spec(UGA_Item_PrimaryUse::StaticClass(), 1);
            Spec.DynamicAbilityTags.AddTag(EmpGameplayTags::TAG_Ability_Item_PrimaryUse);
            GrantedPrimaryUseHandle = ASC->GiveAbility(Spec);
        }
    }
}

void AEPWeapon::UnequipWeapon()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (AEPCharacter* Char = Cast<AEPCharacter>(GetOwner()))
        {
            if (UAbilitySystemComponent* ASC = Char->GetAbilitySystemComponent())
                ASC->ClearAbility(GrantedPrimaryUseHandle);
        }
    }
}
```

### Step 5 — 입력 연동

```cpp
// EPCharacter.cpp
void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        ASC->TryActivateAbilitiesByTag(
            FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Item_PrimaryUse));
    }
}
```

---

## 5. GA Tags 참조

| Tag Container | 설명 | 이 GA에서 사용 |
|---|---|---|
| `Ability Tags` | GA 자신을 설명하는 태그 | `Ability.Item.PrimaryUse` |
| `Activation Blocked Tags` | 이 태그가 있으면 활성화 불가 | `State.Dead`, `State.Reloading` |
| `Activation Owned Tags` | GA 활성 중 부여되는 태그 | (미사용 — 복제 안 됨) |

> `ActivationOwnedTags`는 복제되지 않아 시뮬레이티드 프록시에서 보이지 않음.
> 복제가 필요한 상태(예: 발사 중 State.Firing)는 Infinite GE로 별도 처리.

---

## 6. 완료 체크리스트

- [ ] `Server_Fire` RPC / `LastServerFireTime` 제거 후 컴파일
- [ ] `GE_FireCooldown` Blueprint 에셋 생성 (Cooldown.Weapon.PrimaryUse 태그 포함)
- [ ] `Input_Fire` → `TryActivateAbilitiesByTag` 교체
- [ ] PIE: 발사 입력 → GA 활성화 로그 확인
- [ ] PIE: 연사 제한 동작 확인 (FireRate 쿨타임)
- [ ] PIE: `State.Dead` 중 발사 차단 확인
- [ ] PIE: `State.Reloading` 중 발사 차단 확인
- [ ] 무기 교체 → GA Grant/Remove 누수 없음 확인

---

## 7. 함정 & 주의사항

| 상황 | 원인 | 해결 |
|------|------|------|
| 서버 활성화 실패 | `CommitAbility` 실패 | Cooldown GE GrantedTags 확인, CooldownTags 일치 여부 확인 |
| 쿨타임 무시 | GetCooldownTags() 미오버라이드 | CooldownTags와 GE GrantedTags 동일한 태그인지 확인 |
| 무기 교체 후 GA 남음 | `ClearAbility` 미호출 | UnequipWeapon에서 GrantedPrimaryUseHandle로 제거 |
| 코스메틱 시뮬레이티드 프록시에서 미재생 | GA는 SimProxy에서 실행 안 됨 | Multicast_PlayMuzzleEffect 또는 GameplayCue 사용 |
| EndAbility 누락 | 발사 후 GA가 종료되지 않아 다음 발사 불가 | ActivateAbility 모든 경로에서 EndAbility 또는 CancelAbility 호출 확인 |
