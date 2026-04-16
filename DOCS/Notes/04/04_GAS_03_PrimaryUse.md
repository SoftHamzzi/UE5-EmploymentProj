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
| `EPCombatComponent.h/cpp` | Server_Fire/LastServerFireTime/RequestFire 제거, HandleServerFire 신규, GA Grant/Remove |
| `EPWeaponDefinition.h` | PrimaryUseAbilityClass UPROPERTY 추가 |
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
class EMPLOYMENTPROJ_API UEPGA_Item_PrimaryUse : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UEPGA_Item_PrimaryUse();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    // 커스텀 활성화 조건 — ActivateAbility 진입 전에 GAS가 먼저 호출
    // 여기서 막으면 클라이언트 예측 자체를 방지할 수 있어 불필요한 롤백 없음
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    // Cooldown GE 재사용 패턴: GetCooldownTags + ApplyCooldown 오버라이드
    virtual const FGameplayTagContainer* GetCooldownTags() const override;
    virtual void ApplyCooldown(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) const override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
    FGameplayTagContainer CooldownTags; // Cooldown.Weapon.PrimaryUse

    // GetCooldownTags() 반환용 임시 컨테이너 (CDO에 쓰이므로 Transient)
    UPROPERTY(Transient)
    mutable FGameplayTagContainer TempCooldownTags;

};
```

```cpp
// GAS/GA_Item_PrimaryUse.cpp
UEPGA_Item_PrimaryUse::UEPGA_Item_PrimaryUse()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 클라이언트 완료가 서버를 강제 종료하는 오동작 방지 — 비활성화 권장
    bServerRespectsRemoteAbilityCancellation = false;

    // TryActivateAbilitiesByTag가 이 태그로 GA를 식별함
    // DynamicAbilityTags(Spec)에만 추가하면 CDO 탐색 시 누락될 수 있으므로 여기서도 추가
    AbilityTags.AddTag(EmpGameplayTags::TAG_Ability_Item_PrimaryUse);

    // 이 태그가 있으면 발사 불가 (GAS Tag Container 자동 검사)
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);
}

// Cooldown 재사용 패턴 Step A: 현재 쿨타임 태그 반환
const FGameplayTagContainer* UEPGA_Item_PrimaryUse::GetCooldownTags() const
{
    FGameplayTagContainer* MutableTags = const_cast<FGameplayTagContainer*>(&TempCooldownTags);
    MutableTags->Reset(); // CDO에 쓰이므로 매 호출마다 초기화
    if (const FGameplayTagContainer* ParentTags = Super::GetCooldownTags())
        MutableTags->AppendTags(*ParentTags);
    MutableTags->AppendTags(CooldownTags);
    return MutableTags;
}

// Cooldown 재사용 패턴 Step B: SetByCaller로 쿨타임 Duration 주입
void UEPGA_Item_PrimaryUse::ApplyCooldown(
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

bool UEPGA_Item_PrimaryUse::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
        return false;

    const AEPCharacter* Char  = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
    const AEPWeapon* Weapon   = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;

    // 무기 없거나 발사 불가 상태 (WeaponState, 탄약) — 여기서 막으면 예측 롤백 없음
    return Char && Weapon && Weapon->CanFire();
}

void UEPGA_Item_PrimaryUse::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AEPCharacter* Char  = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
    AEPWeapon* Weapon   = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
    if (!Char || !Weapon)
    {
        // CanActivateAbility에서 통과했는데 여기서 실패 → 방어 코드
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // CommitAbility: CheckCost + CheckCooldown 재확인 후 Cooldown GE 적용
    // 실패 시 bWasCancelled=true로 종료 (GASDoc 패턴)
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const FVector Origin     = Char->GetCameraComponent()->GetComponentLocation();
    const AGameStateBase* GS = Char->GetWorld()->GetGameState<AGameStateBase>();
    // GS null 시 로컬 시간으로 폴백 — SSR 정확도가 떨어지지만 크래시 방지
    const float ClientTime   = GS ? GS->GetServerWorldTimeSeconds()
                                  : Char->GetWorld()->GetTimeSeconds();

    // 서버 전용: 실제 대미지 판정
    if (ActorInfo->IsNetAuthority())
    {
        UEPCombatComponent* Combat = Char->GetCombatComponent();
        Combat->HandleServerFire(Origin, Char->GetControlRotation().Vector(), ClientTime);
    }

    // 로컬 코스메틱 — 소유 클라이언트에서만 실행
    // 시뮬레이티드 프록시는 GA가 실행되지 않으므로 Multicast_PlayMuzzleEffect로 전달
    if (!ActorInfo->IsNetAuthority())
    {
        UEPCombatComponent* Combat = Char->GetCombatComponent();
        if (Combat)
        {
            Combat->PlayLocalMuzzleEffect(Origin);

            // 코스메틱 투사체는 ProjectileFast 무기에서만 스폰
            if (Weapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
                Combat->SpawnLocalCosmeticProjectile(Origin, Char->GetControlRotation().Vector());
        }
    }

    // bReplicateEndAbility=false: 서버도 독립적으로 EndAbility를 호출하므로 별도 RPC 불필요
    // bWasCancelled=false: 정상 완료
    EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
```

> **LocalPredicted 주의**:
> - `ActivateAbility`는 소유 클라이언트와 서버 양쪽에서 실행됨
> - 시뮬레이티드 프록시에서는 GA가 실행되지 않음 → 코스메틱은 Multicast RPC 또는 GameplayCue로 처리
> - `IsNetAuthority()` 로 서버 전용 로직 가드

### Step 3 — CombatComponent 정리

**제거:**
```cpp
UFUNCTION(Server, Reliable) void Server_Fire(...);   // GA가 대체
float LastServerFireTime;                             // Cooldown GE가 대체
void RequestFire(...);                                // Input_Fire → TryActivateAbilitiesByTag로 대체
```

> **`AEPWeapon::CanFire()` FireRate 체크 제거 필요**
> `CanFire()`는 현재 `CurrentTime - LastFireTime < FireInterval` 체크를 포함하고 있다.
> GA 이관 후 FireRate는 GAS Cooldown GE가 관리하므로 이 체크는 중복이다.
> `CanFire()`에서 `LastFireTime` 관련 로직을 제거하고, WeaponState와 Ammo 체크만 남긴다.
> 제거하지 않으면 GAS Cooldown이 풀렸어도 무기의 `LastFireTime`이 블록하는 상황이 발생할 수 있다.

**`Server_Fire_Implementation`의 내용을 `HandleServerFire`로 이전 (public):**
```cpp
// EPCombatComponent.h — public
void HandleServerFire(
    const FVector& Origin,
    const FVector& Direction,
    float ClientFireTime);
```

```cpp
// EPCombatComponent.cpp
void UEPCombatComponent::HandleServerFire(
    const FVector& Origin,
    const FVector& Direction,
    float ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;

    AEPCharacter* Owner = GetOwnerCharacter();
    if (!Owner) return;

    // OriginDrift 검증 (200cm) — 클라이언트 위치 조작 방지
    constexpr float MaxOriginDrift = 200.f;
    if (FVector::DistSquared(Origin, Owner->GetActorLocation()) > FMath::Square(MaxOriginDrift))
        return;

    // CanFire 재확인 (GA CommitAbility 이후 상태 변화 방지)
    if (!EquippedWeapon->CanFire()) return;

    // BallisticType 분기
    switch (EquippedWeapon->WeaponDef->BallisticType)
    {
    case EEPBallisticType::Hitscan:
    default:
        {
            TArray<FVector> PelletDirs;
            EquippedWeapon->Fire(Direction, ClientFireTime, PelletDirs);
            HandleHitscanFire(Owner, Origin, PelletDirs, ClientFireTime);
            break;
        }
    case EEPBallisticType::ProjectileFast:
    case EEPBallisticType::ProjectileSlow:
        {
            FVector SpreadDir = Direction;
            TArray<FVector> Discarded;
            EquippedWeapon->Fire(SpreadDir, ClientFireTime, Discarded);
            HandleProjectileFire(Owner, Origin, SpreadDir);
            break;
        }
    }

    // 시뮬레이티드 프록시용 코스메틱 Multicast
    const FVector MuzzleLoc =
        EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
        ? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
        : EquippedWeapon->GetActorLocation();
    Multicast_PlayMuzzleEffect(MuzzleLoc);
}
```

> `FireRate` 검증(LastServerFireTime)은 Cooldown GE가 대체하므로 제거.
> `CanFire()` 재확인은 GA `ActivateAbility`의 `CommitAbility` 이후 탄약이 0이 되는 엣지 케이스 방어.

**유지 (HandleServerFire 내부에서 호출, private 유지):**
```cpp
void HandleHitscanFire(...);
void HandleProjectileFire(...);
```

**유지 (GA / 로컬에서 호출, public으로 변경):**
```cpp
void PlayLocalMuzzleEffect(...);
void SpawnLocalCosmeticProjectile(...);
```

**탄약 감소 위치:**  
현재 `AEPWeapon::Fire()` 내부에서 `CurrentAmmo--`가 처리된다고 가정.  
GA_Reload 단계에서 탄약을 GAS Attribute로 이관할 예정이므로, 이관 전까지는 기존 방식 유지.

### Step 4 — CombatComponent: GA Grant / Remove

Grant 위치는 `UEPCombatComponent::EquipWeapon / UnequipWeapon`이다.
(`AEPWeapon`에는 `EquipWeapon` 메서드가 없다 — 현재 코드 기준.)

`UEPGA_Item_PrimaryUse::StaticClass()`를 직접 넘기면 `GE_FireCooldown`이 항상 null이 된다
(`EditDefaultsOnly`는 Blueprint 서브클래스에서만 할당 가능).
무기마다 다른 PrimaryUse GA를 쓸 수 있도록 `UEPWeaponDefinition`에 ability class를 두는 것이
데이터 드리븐 구조에 맞다:

```cpp
// EPWeaponDefinition.h — 추가
UPROPERTY(EditDefaultsOnly, Category = "GAS")
TSubclassOf<UEPGA_Item_PrimaryUse> PrimaryUseAbilityClass;
```

```cpp
// EPCombatComponent.h — private 추가
FGameplayAbilitySpecHandle GrantedPrimaryUseHandle;
```

```cpp
// EPCombatComponent.cpp
void UEPCombatComponent::EquipWeapon(AEPWeapon* NewWeapon)
{
    // ... 기존 장착 로직 ...

    // 서버에서만 GA Grant
    AEPCharacter* Owner = GetOwnerCharacter();
    if (GetOwner()->HasAuthority() && Owner && NewWeapon->WeaponDef
        && NewWeapon->WeaponDef->PrimaryUseAbilityClass)
    {
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
        {
            // 재장착 시 이전 핸들이 살아있으면 먼저 제거 — 중복 Grant 방지
            if (GrantedPrimaryUseHandle.IsValid())
                ASC->ClearAbility(GrantedPrimaryUseHandle);

            FGameplayAbilitySpec Spec(NewWeapon->WeaponDef->PrimaryUseAbilityClass, 1);
            Spec.DynamicAbilityTags.AddTag(EmpGameplayTags::TAG_Ability_Item_PrimaryUse);
            GrantedPrimaryUseHandle = ASC->GiveAbility(Spec);
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
            ASC->ClearAbility(GrantedPrimaryUseHandle);
    }
    GrantedPrimaryUseHandle = FGameplayAbilitySpecHandle();
}
```

에디터에서 `DA_AK74`(WeaponDefinition)의 `PrimaryUseAbilityClass`에 `BP_GA_Item_PrimaryUse`를 할당하면 된다.

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

### Step 6 — (선택) Ability Batching

> GASDocumentation은 히트스캔 총에 Ability Batching을 명시적으로 권장한다.
> 히트스캔 GA는 활성화 → 판정 → 종료가 한 프레임에 완료되므로
> 기본 3개 RPC(`CallServerTryActivateAbility` + `ServerEndAbility`)를 1개로 줄일 수 있다.

포트폴리오 단계에서는 필수가 아니지만, 현업 프로젝트에서는 플레이어 수가 많아질수록 효과가 크다.

**적용 방법:**

1. ASC를 상속한 `UEPAbilitySystemComponent`를 만들고 `ShouldDoServerAbilityRPCBatch()`를 override:
```cpp
virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }
```

2. `Input_Fire`에서 `TryActivateAbilitiesByTag` 대신 Spec Handle로 직접 호출:
```cpp
// Batching은 FGameplayAbilitySpecHandle로만 가능 — Tag 기반 호출 불가
if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(GrantedHandle))
{
    FScopedServerAbilityRPCBatcher Batcher(ASC, Spec->Handle);
    ASC->TryActivateAbility(Spec->Handle, true);
}
```

> 현재 구조에서 Spec Handle을 `Input_Fire`까지 전달하려면 `CombatComponent`에서 노출하거나
> `TryActivateAbilitiesByTag`로 Handle을 먼저 찾는 래핑 함수가 필요하다.
> 구조 변경이 수반되므로 기능 안정화 후 별도로 적용 권장.

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

- [ ] `EPWeaponDefinition`에 `PrimaryUseAbilityClass` UPROPERTY 추가
- [ ] `DA_AK74`에 `BP_GA_Item_PrimaryUse` 할당
- [ ] `GE_FireCooldown` Blueprint 에셋 생성 (Cooldown.Weapon.PrimaryUse 태그 포함)
- [ ] `EPCombatComponent::EquipWeapon / UnequipWeapon`에 GA Grant/Remove 추가
- [ ] `Server_Fire` RPC / `LastServerFireTime` / `RequestFire` 제거 후 컴파일
- [ ] `AEPWeapon::CanFire()`에서 `LastFireTime` FireRate 체크 제거
- [ ] `HandleServerFire` public 메서드 추가
- [ ] `Input_Fire` → `TryActivateAbilitiesByTag` 교체
- [ ] PIE: 발사 입력 → GA 활성화 로그 확인
- [ ] PIE: 연사 제한 동작 확인 (FireRate 쿨타임)
- [ ] PIE: `State.Dead` 중 발사 차단 확인
- [ ] PIE: `State.Reloading` 중 발사 차단 확인
- [ ] PIE: 히트스캔 / 투사체 정상 판정 확인
- [ ] PIE: `CanFire() = false` 상태에서 발사 차단 확인 (`CanActivateAbility` 경로)
- [ ] 무기 교체 → GA Grant/Remove 누수 없음 확인
- [ ] 무기 재장착 → 중복 Grant 없음 확인 (로그: `showdebug abilitysystem`)

---

## 7. 함정 & 주의사항

| 상황 | 원인 | 해결 |
|------|------|------|
| 서버 활성화 실패 | `CommitAbility` 실패 | Cooldown GE GrantedTags 확인, CooldownTags 일치 여부 확인 |
| 쿨타임 무시 | `GetCooldownTags()` 미오버라이드 | CooldownTags와 GE GrantedTags 동일한 태그인지 확인 |
| 무기 교체 후 GA 남음 | `ClearAbility` 미호출 | UnequipWeapon에서 GrantedPrimaryUseHandle로 제거 |
| 재장착 시 GA 중복 | EquipWeapon에서 이전 핸들 미제거 | Grant 전 `GrantedPrimaryUseHandle.IsValid()` 확인 후 ClearAbility |
| 코스메틱 시뮬레이티드 프록시에서 미재생 | GA는 SimProxy에서 실행 안 됨 | Multicast_PlayMuzzleEffect 또는 GameplayCue 사용 |
| 발사 후 GA 종료 안 됨 | `EndAbility` 누락 | ActivateAbility 모든 경로에서 `EndAbility` 호출 확인 |
| `TryActivateAbilitiesByTag` 실패 | `AbilityTags`에 TAG 누락 | 생성자에 `AbilityTags.AddTag(TAG_Ability_Item_PrimaryUse)` 확인 |
| 발사 후 예측 롤백 발생 | `CanFire` 체크를 `ActivateAbility`에서 함 | `CanActivateAbility` 오버라이드로 이전 |
| GA 이관 후 FireRate 무시됨 | `CanFire()`의 `LastFireTime` 체크가 GAS Cooldown과 이중으로 존재 | `CanFire()`에서 `LastFireTime` 체크 제거 — WeaponState + Ammo 체크만 유지 |
| 재장전 중 발사 차단 안 됨 | `WeaponState`가 복제 안 됨 → 클라이언트 `CanFire()`는 WeaponState 항상 Idle | GA_Reload 구현 시 `TAG_State_Reloading` GE로 부여해야 `ActivationBlockedTags` 동작. 그 전까지는 서버 `CanFire()` 체크에만 의존 |
