# Post 4-3 작성 가이드 — 발사 어빌리티: RPC를 GA로 대체

> **예상 제목**: `[UE5] 추출 슈터 4-3. 발사 RPC를 어빌리티로: LocalPredicted GA와 쿨타임 GE`
> **참고 문서**: `DOCS/Notes/04/04_GAS_03_PrimaryUse.md`, `04_GAS_00_Reference.md` §2·§6

---

## 개요

**이 포스팅에서 다루는 것:**
- `Server_Fire` RPC → `GA_Item_PrimaryUse` (LocalPredicted) 이관
- 수동 FireRate 검증(`LastServerFireTime`) → `GE_FireCooldown` Duration GE
- 무기 장착에 어빌리티 수명을 묶는 Grant/Clear 패턴
- 3단계 SSR을 그대로 재사용하는 방법

**왜 이렇게 구현했는가 (설계 의도):**
- 발사는 **입력 → 검증 → 판정 → 코스메틱**이 한 흐름인데, RPC 방식에서는 이게 Character·CombatComponent·Weapon 세 클래스에 흩어져 있었다
- GA는 이 흐름을 **한 클래스 안에 담고, 클라 예측까지 공짜로 준다**
- 무기가 바뀌면 발사 방식도 바뀌어야 한다 → 어빌리티를 **무기 데이터가 지정**하게 만든다

---

## 구현 전 상태 (Before)

```cpp
// 3단계 구조
void AEPCharacter::Input_Fire(...)  { CombatComponent->RequestFire(...); }

// UEPCombatComponent
void RequestFire(...)                       // 로컬 코스메틱 즉시 재생 + 서버 RPC
UFUNCTION(Server, Reliable) void Server_Fire(...);
float LastServerFireTime;                   // 수동 FireRate 검증
```

**문제점:**
- **입력 핸들러가 CombatComponent를 직접 안다.** 무기 종류가 늘거나 스킬이 추가되면 Character가 계속 커진다
- FireRate 검증이 `LastServerFireTime` 수동 비교 — 무기마다 다른 연사 속도를 코드가 들고 있다
- 클라 예측이 "코스메틱만 미리 재생"하는 수준. 롤백 개념이 없다
- 재장전 중 발사 차단이 `WeaponState` enum인데 **이게 복제되지 않아 다른 클라에서는 항상 Idle로 보인다** (4-4에서 해결)

---

## 구현 내용

### 1. ★ 입력 추상화 — 단일 경로

```cpp
// Before
void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
    CombatComponent->RequestFire(...);
}

// After
void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->TryActivateAbilitiesByTag(
            FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Item_PrimaryUse));
}
```

**이 세 줄이 이 편에서 가장 중요하다.** Character는 이제 "발사"가 뭔지 모른다. 태그 하나만 던진다.
- 무기가 히트스캔이든 투사체든 같은 코드
- 나중에 스킬(4-7)도 정확히 같은 형태로 붙는다
- 어빌리티를 교체해도 Character는 무변경

### 2. LocalPredicted — 클라와 서버가 각자 실행한다

```cpp
UEPGA_Item_PrimaryUse::UEPGA_Item_PrimaryUse()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bServerRespectsRemoteAbilityCancellation = false;

    // TryActivateAbilitiesByTag가 이 태그로 GA를 찾는다
    FGameplayTagContainer Tags = GetAssetTags();
    Tags.AddTag(EmpGameplayTags::TAG_Ability_Item_PrimaryUse);
    SetAssetTags(Tags);

    // 이 태그가 있으면 활성화 자체가 막힌다 — GAS가 자동 검사
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);
}
```

> **★ `AbilityTags`는 UE 5.5에서 막혔다 — 튜토리얼 코드가 그대로는 안 붙는다**
>
> GAS 자료 대부분이 `AbilityTags.AddTag(...)`로 되어 있는데, 5.5부터 이 필드는 직접 접근이 막혔다.
> ```cpp
> // GameplayAbility.h:497-499
> UE_DEPRECATED_FORGAME(5.5, "Use GetAssetTags(). This is being made non-mutable,
>                             private and renamed to AssetTags in the future.
>                             Use SetAssetTags to set defaults (in constructor only).")
> FGameplayTagContainer AbilityTags;
> ```
> 읽기는 `GetAssetTags()`(`:192`), 쓰기는 `SetAssetTags()`(`:546`)다. 그리고 **쓰기는 생성자에서만** 하라고 못 박혀 있다 — 런타임에 바꾸면 같은 Spec에서 나온 인스턴스들이 서로 다른 태그를 갖게 된다.
>
> 이 프로젝트의 GA 5종(`PrimaryUse`/`Reload`/`Dash`/`Heal`/`ShieldOn`)이 전부 이 형태다. **이름이 `Ability`에서 `Asset`으로 바뀐 이유**도 한 줄 짚고 갈 만하다 — 이 태그는 "이 어빌리티가 활성화되면 붙는 태그"가 아니라 **"이 어빌리티 에셋을 식별하는 태그"**다. `ActivationOwnedTags`와 헷갈리기 쉬운 자리다.

**NetExecutionPolicy 4종** (포스팅에서 표로):

| 정책 | 실행 위치 | 이 프로젝트 사용처 |
|------|-----------|--------------------|
| LocalOnly | 클라만 | — |
| **LocalPredicted** | 클라 즉시 + 서버 검증 | **발사, 재장전, 스킬** |
| ServerOnly | 서버만 | `GA_Death` (4-2) |
| ServerInitiated | 서버가 시작 후 클라 통보 | — |

**시뮬레이티드 프록시에서는 GA가 아예 실행되지 않는다.** 이게 코스메틱 처리를 갈라놓는 원인이므로 먼저 짚고 간다.

```
[소유 클라]  GA 실행 O  → 자기 총구 화염을 즉시 본다
[서버]       GA 실행 O  → 실제 판정
[타 클라]    GA 실행 X  → Multicast RPC로 따로 전달해야 한다
```

### 3. ★ `CanActivateAbility` — 되돌릴 수 없는 것을 막는 자리

```cpp
bool UEPGA_Item_PrimaryUse::CanActivateAbility(...) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
        return false;

    const AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
    const AEPWeapon* Weapon  = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;

    // 무기가 없거나 발사 불가 — 여기서 막으면 클라가 예측 실행조차 하지 않는다
    return Char && Weapon && Weapon->CanFire();
}
```

**`ActivateAbility`에서 체크하면 안 되는 이유**를 그림으로:

```
[CanActivateAbility에서 차단]        [ActivateAbility에서 차단]
클라: 활성화 시도 → 거부              클라: 활성화 → 총구 화염 재생 → 탄약 감소
     아무 일도 안 일어남                    ↓ 서버가 거부
                                     클라: 탄약은 되돌아옴
                                           총구 화염은 그대로 남는다  ← 되돌릴 수단이 없다
```

> **★ 여기서 "롤백"이라는 말을 조심해서 써야 한다 — GAS에는 범용 롤백이 없다**
>
> 서버가 거부하면 `ClientActivateAbilityFailed`가 온다. 이 함수가 실제로 하는 일은 셋뿐이다.
> ```cpp
> // AbilitySystemComponent_Abilities.cpp:2245-2298
> FPredictionKeyDelegates::BroadcastRejectedDelegate(PredictionKey);  // ① 예측 GE 제거
> Ability->CurrentActivationInfo.SetActivationRejected();             // ② 상태 표시
> Ability->K2_EndAbility();                                           // ③ 어빌리티 종료
> ```
> **①이 되돌리는 건 예측으로 적용한 GameplayEffect뿐이다.** 탄약 Cost GE가 여기 해당한다.
> `PlayLocalMuzzleEffect`가 스폰한 Niagara와 Sound는 **GAS가 알지도 못하고, 되돌리는 코드도 없다.**
> 몽타주는 예외적으로 되감기는데(`CurrentMontageStop`), 그건 AbilityTask가 따로 챙겨주기 때문이다.
>
> 즉 정확한 서술은 *"롤백돼서 이펙트가 났다 사라진다"*가 아니라
> **"수치는 되돌아오고 화면에 뿌린 것은 남는다"**다.
> **그리고 이게 `CanActivateAbility`로 옮겨야 하는 이유를 더 강하게 만든다** —
> 되돌릴 수 있는 것(탄약)이 아니라 **되돌릴 수 없는 것(연출)** 때문에 앞에서 막는 것이다.

예측을 쓰는 시스템에서는 **"막을 거면 예측 전에 막는다"**가 원칙이다.
GAS의 예측은 *"틀리면 되돌린다"*가 아니라 ***"되돌릴 수 있는 것만 예측한다"***에 가깝다.

### 4. ★ FireRate를 Duration GE로

수동 시간 비교를 GAS의 쿨타임 시스템으로 옮긴다.

```
Content/Data/GAS/GE_FireCooldown:
- DurationPolicy    : HasDuration
- DurationMagnitude : SetByCaller (Data.Cooldown)
- GrantedTags       : Cooldown.Weapon.PrimaryUse   ← 쿨타임 식별 태그
```

> **스크린샷 위치**: GE_FireCooldown의 Duration 설정 + GrantedTags 패널

**Cooldown GE는 GrantedTags로 자기 존재를 알린다.** GA는 이 태그가 있는지로 쿨타임을 판정한다. 그래서 두 함수를 오버라이드해야 한다:

```cpp
// A. 이 GA의 쿨타임 태그가 뭔지 알려준다
const FGameplayTagContainer* UEPGA_Item_PrimaryUse::GetCooldownTags() const
{
    FGameplayTagContainer* MutableTags = const_cast<FGameplayTagContainer*>(&TempCooldownTags);
    MutableTags->Reset();  // CDO에서 쓰이므로 매번 초기화
    if (const FGameplayTagContainer* ParentTags = Super::GetCooldownTags())
        MutableTags->AppendTags(*ParentTags);
    MutableTags->AppendTags(CooldownTags);
    return MutableTags;
}

// B. 쿨타임을 적용할 때 무기의 FireRate를 Duration으로 주입한다
void UEPGA_Item_PrimaryUse::ApplyCooldown(...) const
{
    UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
    if (!CooldownGE) return;

    FGameplayEffectSpecHandle SpecHandle =
        MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());
    SpecHandle.Data->DynamicGrantedTags.AppendTags(CooldownTags);

    // ★ 무기 데이터에서 읽는다 — 코드에 숫자가 없다
    const AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
    const AEPWeapon* Weapon  = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
    const float Duration     = Weapon ? (1.f / Weapon->WeaponDef->FireRate) : 0.2f;

    SpecHandle.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, Duration);
    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
```

**얻는 것:**
- 연사 속도가 **WeaponDefinition 데이터**가 됐다. 새 무기 = 새 DataAsset, 코드 무변경
- 서버가 GE로 강제하므로 클라가 RPC를 스팸해도 `CommitAbility`가 막는다
- `showdebug abilitysystem`에서 쿨타임이 눈에 보인다 (디버깅이 쉬워짐)

> **주의**: `AEPWeapon::CanFire()`에 남아 있던 `LastFireTime` 체크는 **반드시 제거해야 한다.**
> GAS 쿨타임과 이중으로 존재하면, 쿨타임이 풀렸는데 무기 쪽 타이머가 막는 상황이 생긴다.
> `CanFire()`는 이제 **탄약 체크만** 담당한다.

### 5. `ActivateAbility` — 서버와 클라가 각자 할 일

```cpp
void UEPGA_Item_PrimaryUse::ActivateAbility(...)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
    AEPWeapon* Weapon  = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
    if (!Char || !Weapon)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // CommitAbility = CheckCost + CheckCooldown + 둘 다 적용
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const FVector Origin     = Char->GetCameraComponent()->GetComponentLocation();
    const AGameStateBase* GS = Char->GetWorld()->GetGameState<AGameStateBase>();
    // 3단계에서 정한 규칙 그대로 — 서버/클라가 같은 시계를 쓴다
    const float ClientTime   = GS ? GS->GetServerWorldTimeSeconds()
                                  : Char->GetWorld()->GetTimeSeconds();

    // ── 서버: 실제 판정
    if (ActorInfo->IsNetAuthority())
    {
        Char->GetCombatComponent()->HandleServerFire(
            Origin, Char->GetControlRotation().Vector(), ClientTime);
    }
    // ── 클라: 코스메틱 즉시 재생 (RTT 대기 없음)
    else
    {
        UEPCombatComponent* Combat = Char->GetCombatComponent();
        if (Combat)
        {
            Combat->PlayLocalMuzzleEffect(Origin);
            if (Weapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
                Combat->SpawnLocalCosmeticProjectile(Origin, Char->GetControlRotation().Vector());
        }
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
```

**`ClientTime`이 3단계 SSR과 연결되는 지점**이다. `GS->GetServerWorldTimeSeconds()`를 양쪽이 쓴다는 규칙이 GAS로 옮긴 뒤에도 그대로다.

**`CommitAbility` 위치가 중요하다** — `CanActivateAbility`(예측 차단) → `CommitAbility`(비용·쿨타임 확정) → 실제 로직 순서. `CommitAbility`가 실패하면 `bWasCancelled = true`로 종료한다. 그래서 **코스메틱을 `CommitAbility` 뒤에 두는 것**이 위 §3과 짝을 이룬다 — 커밋 전에 뿌리면 되돌릴 수 없는 것이 먼저 나간다.

### 6. ★ SSR 재사용 — 3단계 자산은 하나도 안 버렸다

```cpp
// 이 함수는 Server_Fire_Implementation의 내용을 그대로 옮긴 것이다
void UEPCombatComponent::HandleServerFire(
    const FVector& Origin, const FVector& Direction, float ClientFireTime)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;
    AEPCharacter* Owner = GetOwnerCharacter();
    if (!Owner) return;

    // 위치 조작 방지 — 3단계에서 만든 검증 그대로 유지
    constexpr float MaxOriginDrift = 200.f;
    if (FVector::DistSquared(Origin, Owner->GetActorLocation()) > FMath::Square(MaxOriginDrift))
        return;

    if (!EquippedWeapon->CanFire()) return;  // CommitAbility 이후 탄약 0이 되는 엣지 방어

    switch (EquippedWeapon->WeaponDef->BallisticType)
    {
    case EEPBallisticType::Hitscan:
    default:
        {
            TArray<FVector> PelletDirs;
            EquippedWeapon->Fire(Direction, ClientFireTime, PelletDirs);
            HandleHitscanFire(Owner, Origin, PelletDirs, ClientFireTime);  // → SSR ConfirmHitscan
            break;
        }
    case EEPBallisticType::ProjectileFast:
    case EEPBallisticType::ProjectileSlow:
        { /* ... */ }
    }

    // 시뮬레이티드 프록시용 코스메틱 — GA로는 도달할 수 없다
    Multicast_PlayMuzzleEffect(MuzzleLoc);
}
```

**바뀐 것과 안 바뀐 것을 표로 정리한다:**

| 요소 | 상태 |
|------|------|
| `UEPServerSideRewindComponent` 전체 | **무변경** |
| `ConfirmHitscan` / 리와인드 / 보간 | **무변경** |
| `HandleHitscanFire` / `HandleProjectileFire` | 무변경, private 유지 |
| `EEPBallisticType` switch | 무변경, 호출자만 GA로 (단, ★ 아래) |
| Origin drift 200cm 검증 | 무변경 |
| `LastServerFireTime` FireRate 검증 | **제거** → `GE_FireCooldown` |
| `Server_Fire` RPC | **제거** → GA |
| `RequestFire` | **제거** → `TryActivateAbilitiesByTag` |

> **이 표가 "GAS 이관은 전면 재작성이 아니다"라는 이 시리즈의 메시지다.** 랙 보상 같은 어려운 부분은 그대로 두고, 호출자만 교체했다.

> **★ 그런데 이 switch에는 옮기면서 같이 딸려온 문제가 하나 있다 — 솔직하게 적는다**
>
> ```cpp
> case EEPBallisticType::Hitscan:
> default:                          // ← 히트스캔과 default가 붙어 있다
> ```
> `EEPBallisticType`에 값을 하나 추가하면 — 예를 들어 근접무기나 수류탄 — **그게 조용히 히트스캔으로 발사된다.** `default:`가 있어서 `-Wswitch` 경고도 뜨지 않는다. 컴파일러가 아무 말도 안 한다.
>
> "무변경"이라고 적어놨지만, 정확히는 **문제가 있는 채로 무변경**이다. 3단계에서는 탄도 종류가 실제로 히트스캔뿐이라 드러나지 않았고, GAS 이관은 호출자만 바꿨으니 그대로 넘어왔다.
>
> 이건 3단계 시리즈에서 반복해 나온 *"선언은 했는데 아무 일도 일어나지 않는 코드"*의 사촌이다 — **"새 값을 추가했는데 아무도 알려주지 않는 코드"**. 조치는 `DOCS/BACKLOG.md` **B-3**에 이월했고, 근접·투척 무기를 넣기 전에 처리한다.

### 7. 탄약 소모 — Cost GE

```
GE_ConsumeAmmo:
- DurationPolicy : Instant
- Modifier       : Ammo / Add / -1
```

GA Blueprint에서 `CostGameplayEffectClass = GE_ConsumeAmmo`. `CommitAbility`가 쿨타임과 함께 처리하므로 코드가 필요 없다.

> **`CheckCost`는 차단하지 않는다.** Ammo가 0일 때 -1을 적용해도 `PreAttributeChange`가 0으로 클램핑할 뿐 실패로 처리되지 않는다.
> **명시적으로 `CanFire()`에서 `Ammo <= 0`을 체크해야** 발사가 막힌다. 처음에 이걸 몰라서 탄약 0에 계속 발사됐다.

```cpp
bool AEPWeapon::CanFire() const
{
    if (!WeaponDef) return false;

    if (AEPCharacter* EPOwner = Cast<AEPCharacter>(GetOwner()))
        if (UAbilitySystemComponent* ASC = EPOwner->GetAbilitySystemComponent())
            if (UEPAttributeSet* AS = Cast<UEPAttributeSet>(
                    ASC->GetAttributeSet(UEPAttributeSet::StaticClass())))
                if (AS->GetAmmo() <= 0.f) return false;

    return true;
}
```

### 8. ★ Grant/Clear — 어빌리티에 수명을 준다

발사 어빌리티는 무기가 있을 때만 존재해야 한다. **어빌리티 부여를 장착에 묶는다.**

```cpp
// EPWeaponDefinition.h — 무기 데이터가 자기 어빌리티를 지정한다
UPROPERTY(EditDefaultsOnly, Category = "GAS")
TSubclassOf<UEPGA_Item_PrimaryUse> PrimaryUseAbilityClass;
```
```cpp
void UEPCombatComponent::EquipWeapon(AEPWeapon* NewWeapon)
{
    // ... 기존 장착 로직 ...
    AEPCharacter* Owner = GetOwnerCharacter();
    if (GetOwner()->HasAuthority() && Owner && NewWeapon->WeaponDef
        && NewWeapon->WeaponDef->PrimaryUseAbilityClass)
    {
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
        {
            // 재장착 시 이전 핸들을 먼저 지운다 — 중복 Grant 방지
            if (GrantedPrimaryUseHandle.IsValid())
                ASC->ClearAbility(GrantedPrimaryUseHandle);

            FGameplayAbilitySpec Spec(NewWeapon->WeaponDef->PrimaryUseAbilityClass, 1);
            GrantedPrimaryUseHandle = ASC->GiveAbility(Spec);
        }
    }
}

void UEPCombatComponent::UnequipWeapon()
{
    // ... 기존 해제 로직 ...
    if (GetOwner()->HasAuthority() && GetOwnerCharacter())
        if (UAbilitySystemComponent* ASC = GetOwnerCharacter()->GetAbilitySystemComponent())
            ASC->ClearAbility(GrantedPrimaryUseHandle);
    GrantedPrimaryUseHandle = FGameplayAbilitySpecHandle();
}
```

**핸들 관리를 빼먹으면 생기는 일:**
- `ClearAbility` 누락 → 무기를 버려도 계속 발사됨
- 재장착 전 제거 누락 → 어빌리티가 중복 부여돼 한 발에 두 번 판정

> 이 단일 핸들 방식은 4-4에서 Reload가 추가되며 **배열로 일반화**된다. 어빌리티가 늘 때마다 핸들 멤버가 늘어나기 때문. 포스팅에서는 "여기서는 이렇게 시작했고, 다음 편에서 이 판단을 뒤집는다"고 예고한다.

> **에디터 작업**: `DA_AK74_HitScan`의 `PrimaryUseAbilityClass`에 `BP_GA_Item_PrimaryUse` 할당
> **스크린샷 위치**: WeaponDefinition DataAsset의 GAS 카테고리

---

## 함정 정리

| 상황 | 원인 | 해결 |
|------|------|------|
| `TryActivateAbilitiesByTag`가 GA를 못 찾음 | 생성자에 Asset 태그 누락 | `GetAssetTags()` → `AddTag` → `SetAssetTags()` 확인 |
| 튜토리얼의 `AbilityTags.AddTag(...)`가 컴파일 안 됨 | UE 5.5에서 deprecated (`GameplayAbility.h:497`) | `SetAssetTags()`로 교체. **생성자에서만** |
| 쿨타임이 무시됨 | `GetCooldownTags()` 미오버라이드 | CooldownTags와 GE GrantedTags 일치 확인 |
| GA 이관 후에도 FireRate가 이상함 | `CanFire()`의 `LastFireTime` 체크가 GAS와 이중 | `CanFire()`에서 시간 체크 제거 |
| 서버가 거부해도 총구 화염이 남음 | 차단 조건을 `ActivateAbility`에 둠. **연출은 롤백되지 않는다** | `CanActivateAbility`로 이전 |
| 탄약 0인데 계속 발사됨 | `CheckCost`는 차단하지 않음 | `CanFire()`에 `Ammo <= 0` 명시 |
| 무기 교체 후에도 발사됨 | `ClearAbility` 누락 | `UnequipWeapon`에서 핸들 제거 |
| 한 발에 두 번 판정 | 재장착 시 중복 Grant | Grant 전 기존 핸들 확인 |
| 타 플레이어 총구 화염이 안 보임 | SimProxy에서 GA 미실행 | `Multicast_PlayMuzzleEffect` 유지 |
| `GE_FireCooldown`이 항상 null | C++ GA를 직접 할당 | Blueprint 서브클래스 사용 (4-2와 동일) |

---

## 결과

**확인 항목 (PIE 2인):**
- 발사 → `showdebug abilitysystem`에 `Cooldown.Weapon.PrimaryUse` 태그가 FireRate만큼 유지
- 마우스 연타 → 쿨타임 중에는 활성화 자체가 안 됨 (이펙트도 안 남)
- 높은 핑(100ms 에뮬)에서 발사 → 총구 화염은 즉시, 판정은 3단계 SSR 기준으로 정확
- 무기 해제 → 발사 입력 무반응
- 타 클라 화면에서 상대 총구 화염 보임

**한계 및 향후 개선:**
- **Ability Batching 미적용.** 히트스캔 GA는 활성화→판정→종료가 한 프레임인데, 현재는 활성화·타겟데이터·종료가 각각 RPC로 나간다. GASDocumentation은 배칭으로 이를 1회로 합치길 권장한다. 발사 빈도가 높은 게임이라 **적용 가치가 있으나 이번 단계에서는 보류**했다
- 스프레드가 아직 균등 난수다 → **4-5에서 CDF 방식으로 교체**
- 부위 배율이 아직 본 이름 기반이다 → **4-6에서 태그 기반으로 교체** (그리고 4-6에서 밝히지만, 이 시점의 본 이름 배율은 **실제로는 한 번도 작동하지 않고 있었다**)

---

## 참고

- `DOCS/Notes/04/04_GAS_03_PrimaryUse.md` — 구현 전체
- `DOCS/Notes/04/04_GAS_00_Reference.md` §2 (GameplayAbility), §6 (네트워크)
- Step 3-2 포스팅 — `ConfirmHitscan` 내부 구조 (이 편에서 재사용)
