# Post 4-4 작성 가이드 — 재장전: 복제되는 상태 태그와 AbilityTask

> **예상 제목**: `[UE5] 추출 슈터 4-4. 재장전 어빌리티: 타이머를 AbilityTask로, enum을 태그로`
> **참고 문서**: `DOCS/Notes/04/04_GAS_04_Reload.md`, `04_GAS_04_Reload_STATUS.md`

---

## 개요

**이 포스팅에서 다루는 것:**
- 탄약을 `uint8` UPROPERTY에서 Attribute로 옮기기
- **`WeaponState` enum이 복제되지 않아 생긴 실제 문제**와 Duration GE 해법
- `SetTimer` 대신 `UAbilityTask_WaitDelay`를 쓰는 이유
- AttributeBased Modifier로 무기별 MaxAmmo를 에셋 하나가 처리하기

**왜 이렇게 구현했는가 (설계 의도):**
- 이 편은 **시리즈 관통 주제 "복제되는 상태 vs 안 되는 상태"가 가장 선명하게 드러나는 단계**다
- 4-2에서 `State.Dead`로 한 번 겪은 문제를 `State.Reloading`에서 다시 만난다. **두 번째 만남에서 이게 패턴이라는 게 확인된다**

---

## 구현 전 상태 (Before)

```cpp
// AEPWeapon — 무기가 자기 탄약과 상태를 직접 관리
UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo) uint8 CurrentAmmo;
UPROPERTY(Replicated) uint8 MaxAmmo;
EEPWeaponState WeaponState;          // Idle / Firing / Reloading
FTimerHandle ReloadTimerHandle;

void StartReload();                  // 타이머 시작 + WeaponState 변경
void FinishReload();                 // 타이머 콜백 — 탄약 채우고 상태 복구
void OnRep_CurrentAmmo();
```

**문제점 — 특히 마지막이 결정적이다:**
- 탄약이 무기 Actor에 있어 **HP와 복제 전략이 다르다** (하나는 UPROPERTY, 하나는 Attribute)
- 타이머가 GA 생명주기와 분리돼 있다 — 중간에 죽으면 타이머가 남는다
- **★ `WeaponState`가 복제되지 않는다.** 그래서 다른 클라이언트에서 `CanFire()`를 호출하면 `WeaponState`는 언제나 `Idle`이다

세 번째 문제를 그림으로:

```
[내 화면]                        [상대 화면]
나: 재장전 중 (WeaponState=Reloading)   나: WeaponState=Idle (복제 안 됨)
   → 발사 차단 O                          → "재장전 중인지" 판정 불가
                                          → 클라 측 예측 판단이 전부 틀린다
```

3단계까지는 **서버 `CanFire()` 체크에만 의존**해서 넘어갔다. 클라는 틀린 판단을 하고 서버가 거부하는 구조였다.

---

## 구현 내용

### 1. 탄약을 Attribute로

```cpp
// EPAttributeSet.h — Health와 완전히 같은 패턴
UPROPERTY(BlueprintReadOnly, Category = "Attribute|Ammo", ReplicatedUsing = OnRep_Ammo)
FGameplayAttributeData Ammo;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, Ammo)

UPROPERTY(BlueprintReadOnly, Category = "Attribute|Ammo", ReplicatedUsing = OnRep_MaxAmmo)
FGameplayAttributeData MaxAmmo;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, MaxAmmo)
```
```cpp
// GetLifetimeReplicatedProps
DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, Ammo,    COND_None, REPNOTIFY_Always);
DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, MaxAmmo, COND_None, REPNOTIFY_Always);

// PreAttributeChange
if (Attribute == GetAmmoAttribute())
    NewValue = FMath::Clamp(NewValue, 0.f, GetMaxAmmo());
```

**얻는 것:** HP·탄약이 같은 복제 전략, 같은 델리게이트, 같은 클램핑 규약을 쓴다. 4-8의 HUD가 두 값을 **동일한 코드로** 구독할 수 있게 된다.

장착 시 초기화:
```cpp
// UEPCombatComponent::EquipWeapon 서버측
// 컴포넌트에는 PS가 없으므로 Owner Character를 거친다
AEPCharacter* Owner = GetOwnerCharacter();
if (AEPPlayerState* PS = Owner ? Owner->GetPlayerState<AEPPlayerState>() : nullptr)
    if (UEPAttributeSet* AS = PS->GetAttributeSet())
    {
        AS->InitAmmo(static_cast<float>(NewWeapon->WeaponDef->MaxAmmo));
        AS->InitMaxAmmo(static_cast<float>(NewWeapon->WeaponDef->MaxAmmo));
    }
```

> **탄약이 float가 됐다.** Attribute는 전부 float다. 처음엔 어색하지만, 덕분에 "탄약 소모 50% 감소" 같은 배율 버프가 자연스럽게 붙는다. 표시할 때만 정수로 자르면 된다.

### 2. ★ `State.Reloading` — Duration GE로 복제한다

이 편의 핵심.

```
Content/Data/GAS/GE_Reloading:
- DurationPolicy    : HasDuration
- DurationMagnitude : SetByCaller (Data.ReloadDuration)
- GrantedTags       : State.Reloading
```

> **스크린샷 위치**: GE_Reloading의 Duration + GrantedTags 패널

**왜 Infinite가 아니라 Duration인가** — 재장전 시간이 지나면 태그가 **자동으로** 풀린다. 4-2의 `State.Dead`는 죽은 상태가 계속 유지돼야 해서 Infinite였다. **지속 시간이 정해진 상태는 Duration GE가 타이머 역할까지 겸한다.**

이제 발사 차단이 자동으로 동작한다:
```cpp
// GA_Item_PrimaryUse 생성자 (4-3에서 이미 작성해둔 줄)
ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);
```

**`ActivationBlockedTags`는 GAS가 알아서 검사한다.** 코드 한 줄도 추가하지 않았는데, 태그가 복제되기 시작하자 **다른 클라이언트에서도 발사 차단이 맞게 동작하기 시작했다.**

이 대비를 포스팅에서 강조한다:

| | Before (`WeaponState` enum) | After (`State.Reloading` 태그) |
|---|---|---|
| 복제 | 안 됨 | GE를 타고 복제됨 |
| 타 클라 쿼리 | 불가 | 가능 |
| 차단 코드 | `if (WeaponState == Reloading) return false;` | **없음** (`ActivationBlockedTags`) |
| 만료 처리 | 타이머 콜백에서 수동 복구 | Duration 만료 시 자동 |
| 중복 재장전 방지 | 별도 체크 | 같은 태그로 자동 |

### 3. `GE_Reload_Ammo` — AttributeBased Modifier

```
Content/Data/GAS/GE_Reload_Ammo:
- DurationPolicy : Instant
- Modifiers:
    Attribute       : EPAttributeSet.Ammo
    ModifierOp      : Override
    MagnitudeCalc   : AttributeBased
        SourceAttribute : EPAttributeSet.MaxAmmo
        AttributeSource : Source (자기 자신)
        bSnapshot       : false
```

**"Ammo를 MaxAmmo 값으로 덮어쓴다"를 에셋 하나로 표현한다.** 무기마다 MaxAmmo가 달라도 GE는 하나면 된다.

- `Override` — Add가 아니라 덮어쓰기. 남은 탄이 몇 발이든 가득 찬다
- `bSnapshot = false` — GE Spec을 만드는 시점이 아니라 **적용하는 시점**의 MaxAmmo를 읽는다. 재장전 도중 무기를 바꿔도 맞는 값이 들어간다

> 탄창 시스템(예비 탄약에서 차감)을 넣으려면 이 GE를 `Add`로 바꾸고 예비 탄약 Attribute를 추가하면 된다. **현재는 무한 예비탄 가정이라 Override로 충분하다.**

### 4. ★ `WaitDelay` AbilityTask — 타이머와 뭐가 다른가

```cpp
void UGA_Item_Reload::ActivateAbility(...)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
    AEPWeapon* Weapon  = Char ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;

    if (!Char || !Weapon || !CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }

    const float ReloadTime = Weapon->WeaponDef->ReloadTime;

    // State.Reloading 태그 부여 — Duration은 무기 데이터에서
    if (GE_ReloadingClass)
    {
        FGameplayEffectSpecHandle SpecHandle =
            MakeOutgoingGameplayEffectSpec(GE_ReloadingClass, GetAbilityLevel());
        SpecHandle.Data->SetSetByCallerMagnitude(
            EmpGameplayTags::TAG_Data_ReloadDuration, ReloadTime);
        ReloadingEffectHandle =
            ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
    }

    UAbilityTask_WaitDelay* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReloadTime);
    WaitTask->OnFinish.AddDynamic(this, &UGA_Item_Reload::OnReloadComplete_Task);
    WaitTask->ReadyForActivation();   // ★ C++에서는 필수. Blueprint는 자동 호출
}
```

| | `SetTimer` + 람다 | `UAbilityTask_WaitDelay` |
|---|---|---|
| GA 취소 시 | **타이머가 살아남는다** | 태스크가 자동 정리 |
| 예측 지원 | 없음 | GAS 예측 키와 통합 |
| GA 인스턴스 접근 | 람다 캡처 필요 | `GetCurrentAbilitySpecHandle()` 등 |
| C++ 주의점 | — | **`ReadyForActivation()` 수동 호출** |

**`ReadyForActivation()`을 빼먹으면 콜백이 영원히 안 온다.** 에러도 로그도 없다. Blueprint에서는 노드가 자동으로 호출해주기 때문에 C++로 옮길 때만 생기는 함정이다.

```cpp
void UGA_Item_Reload::OnReloadComplete_Task()
{
    // 콜백에는 Handle/ActorInfo 파라미터가 없다 — InstancedPerActor라 인스턴스에서 꺼낸다
    const FGameplayAbilitySpecHandle Handle        = GetCurrentAbilitySpecHandle();
    const FGameplayAbilityActorInfo* ActorInfo     = GetCurrentActorInfo();
    const FGameplayAbilityActivationInfo ActivInfo = GetCurrentActivationInfo();

    // 탄약 보충은 서버 권한에서만
    if (ActorInfo->IsNetAuthority() && GE_ReloadAmmoClass)
        ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivInfo,
            GE_ReloadAmmoClass.GetDefaultObject(), 1.f);

    EndAbility(Handle, ActorInfo, ActivInfo, true, false);
}
```

### 5. 취소 시 태그 정리 — `EndAbility` 오버라이드

```cpp
void UGA_Item_Reload::EndAbility(..., bool bReplicateEndAbility, bool bWasCancelled)
{
    // Duration이 남아있는 채로 취소된 경우 태그가 남는다 → 수동 제거
    if (ReloadingEffectHandle.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(ReloadingEffectHandle);
        ReloadingEffectHandle.Invalidate();
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
```

**빠뜨리면**: 재장전 중 사망 → GA는 취소되는데 `GE_Reloading`은 Duration이 다 될 때까지 남는다 → 리스폰 후에도 발사가 막힌다.

> 이 authority 처리는 4-7에서 다시 문제가 된다. `EndAbility`는 **클라 예측 인스턴스에서도 실행**되는데, 거기서 `RemoveActiveGameplayEffect`를 부르면 경고가 뜬다. 4-7의 `UEPGA_Skill_Base`에서 `IsNetAuthority()` 가드로 정리된다.

### 6. Grant 방식을 배열로 일반화 — 4-3의 판단을 뒤집는다

4-3에서는 `GrantedPrimaryUseHandle` 단일 핸들이었다. Reload가 추가되면서 같은 패턴을 반복하면 어빌리티가 늘 때마다 멤버가 늘어난다.

```cpp
// EPWeaponDefinition.h — 단일 필드를 배열로 통합
UPROPERTY(EditDefaultsOnly, Category = "GAS")
TArray<TSubclassOf<UGameplayAbility>> WeaponAbilities;   // PrimaryUse, Reload, ...

// EPCombatComponent.h
TArray<FGameplayAbilitySpecHandle> GrantedWeaponAbilityHandles;
```
```cpp
void UEPCombatComponent::EquipWeapon(AEPWeapon* NewWeapon)
{
    // ... 기존 장착 로직 ...
    if (GetOwner()->HasAuthority() && Owner && NewWeapon->WeaponDef)
        if (UAbilitySystemComponent* ASC = Owner->GetAbilitySystemComponent())
        {
            for (const FGameplayAbilitySpecHandle& Handle : GrantedWeaponAbilityHandles)
                if (Handle.IsValid()) ASC->ClearAbility(Handle);
            GrantedWeaponAbilityHandles.Reset();

            for (const TSubclassOf<UGameplayAbility>& AbilityClass : NewWeapon->WeaponDef->WeaponAbilities)
                if (AbilityClass)
                    GrantedWeaponAbilityHandles.Add(ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1)));
        }
}
```

Lyra의 `UAbilitySet` + `FAbilitySet_GrantedHandles` 패턴과 같은 발상이다.

**배열 방식에서는 Spec에 `DynamicAbilityTags`를 넣지 않는다.** 대신 각 GA 생성자에서 `SetAssetTags()`로 자기 식별 태그를 설정한다. `TryActivateAbilitiesByTag`는 이 태그로 찾는다.

> **정직하게 남길 것**: 어빌리티가 2개뿐일 때는 `GrantedReloadHandle`을 하나 더 추가하는 것도 충분하다. **3개 이상이 될 게 확실해질 때 리팩토링하는 게 맞다.** 이 프로젝트는 스킬(4-7)까지 예정돼 있어 미리 옮겼다.

### 7. 입력 — 4-3과 완전히 같은 형태

```cpp
void AEPCharacter::Input_Reload(const FInputActionValue& Value)
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
        ASC->TryActivateAbilitiesByTag(
            FGameplayTagContainer(EmpGameplayTags::TAG_Ability_Item_Reload));
}
```

태그만 다르고 나머지가 동일하다. **입력 추상화가 실제로 값을 하는 지점**이다.

---

## 겪은 문제

`04_GAS_04_Reload_STATUS.md`에 기록된 두 건. 둘 다 **타이밍 문제**라는 공통점이 있다.

### ① 사망 시 재장전 어빌리티가 취소되지 않음

재장전 도중 죽으면 `GA_Item_Reload`가 계속 살아 있었다. 3초 뒤 `OnReloadComplete_Task`가 시체의 탄약을 채웠다.

- **원인**: `GA_Death`가 활성화돼도 다른 GA를 자동으로 끄지는 않는다. `State.Dead`는 **새 활성화만** 막는다 (`ActivationBlockedTags`) — 이미 돌고 있는 어빌리티에는 영향이 없다
- **교훈**: `ActivationBlockedTags`는 **진입 차단**이지 **중단**이 아니다. 진행 중인 어빌리티를 끊으려면 `CancelAbilities` 또는 `AbilityTagPropagation` 설정이 따로 필요하다

### ② PIE 시작 직후 LocalPredicted 어빌리티 활성화 실패

게임 시작하자마자 재장전을 누르면 활성화가 실패했다. 몇 초 뒤에는 정상.

- **원인**: 클라 ASC 초기화(`OnRep_PlayerState` → `InitAbilityActorInfo`)가 아직 안 끝난 상태에서 입력이 들어옴
- **4-1의 함정 표에 있던 바로 그 에러**(`Can't activate LocalOnly or LocalPredicted ability`)가 실전에서 나온 경우
- **교훈**: ASC 초기화는 **비결정적 시점**에 완료된다. 이 사실은 4-8의 HUD 초기화 타이밍에서 다시 문제가 된다

---

## 함정 정리

| 상황 | 원인 | 해결 |
|------|------|------|
| 재장전 중 발사가 안 막힘 | `State.Reloading`이 복제 안 되는 방식 | `ActivationOwnedTags` 대신 `GE_Reloading` |
| `WaitDelay` 콜백이 안 옴 | `ReadyForActivation()` 미호출 | C++에서는 반드시 수동 호출 |
| 취소했는데 `State.Reloading`이 남음 | `EndAbility`에서 GE 미제거 | `RemoveActiveGameplayEffect` |
| 탄약이 안 채워짐 | `GE_Reload_Ammo`의 MaxAmmo 캡처 실패 | `AttributeSource = Source` 확인 |
| 재장전이 두 번 발동 | `ActivationBlockedTags`에 `State.Reloading` 누락 | 자기 자신도 막아야 한다 |
| 죽었는데 재장전이 완료됨 | 진행 중 GA는 태그로 안 막힌다 | 사망 시 명시적 취소 |

---

## 결과

**확인 항목 (PIE 2인):**
- 재장전 → **상대 화면의 `showdebug abilitysystem`에서도** `State.Reloading` 태그 확인 ← 이 편의 핵심 검증
- 재장전 중 발사 시도 → 활성화 차단 (이펙트 없음)
- 재장전 중 재장전 → 차단
- 완료 → `Ammo`가 `MaxAmmo`로 복원
- 재장전 중 사망 → 리스폰 후 정상 발사 (태그 잔류 없음)

**한계 및 향후 개선:**
- **재장전 애니메이션이 없다.** 현재는 `WaitDelay` 시간만 흐른다. `PlayMontageAndWait` 태스크로 교체하면 몽타주 종료와 동시에 완료 처리를 묶을 수 있다
- 예비 탄약 개념이 없다 (무한 탄창). 넣으려면 `ReserveAmmo` Attribute + `GE_Reload_Ammo`를 Override에서 Add 계산으로
- 취소 가능한 재장전(발사로 캔슬)은 미구현

---

## 참고

- `DOCS/Notes/04/04_GAS_04_Reload.md` — 구현 전체
- `DOCS/Notes/04/04_GAS_04_Reload_STATUS.md` — 실제 발생 버그 기록
- `DOCS/Notes/04/04_GAS_00_Reference.md` §2 — AbilityTask
