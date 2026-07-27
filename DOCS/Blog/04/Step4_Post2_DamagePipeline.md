# Post 4-2 작성 가이드 — 데미지/HP 파이프라인과 사망 처리

> **예상 제목**: `[UE5] 추출 슈터 4-2. TakeDamage를 버리다: 메타 어트리뷰트 데미지 파이프라인과 GA_Death`
> **참고 문서**: `DOCS/Notes/04/04_GAS_02_DamagePipeline.md`, `04_GAS_00_Reference.md` §4

---

## 개요

**이 포스팅에서 다루는 것:**
- `TakeDamage()` + `HP` UPROPERTY를 GE 파이프라인으로 교체
- 메타 어트리뷰트(`IncomingDamage`) 패턴 — 왜 Health를 직접 깎지 않는가
- 사망을 이벤트 기반 어빌리티(`GA_Death`)로 분리
- `State.Dead`를 태그로 복제하는 방법과, 그 방법이 하나뿐인 이유

**왜 이렇게 구현했는가 (설계 의도):**
- 언리얼 기본 `TakeDamage`는 **데미지를 받는 쪽이 계산까지 다 한다.** 방어력·부위 배율·실드가 붙기 시작하면 `TakeDamage` 하나가 계속 커진다
- GE 파이프라인은 계산이 끼어들 지점을 **한 곳(`PostGameplayEffectExecute`)으로 모은다.** 4-6(부위 배율)과 4-7(실드 50% 감산)이 실제로 이 지점에 얹힌다

---

## 구현 전 상태 (Before)

```cpp
// EPCharacter.h — 직접 관리하던 HP
UPROPERTY(ReplicatedUsing = OnRep_HP) int32 HP = 100;
int32 MaxHP = 100;
FORCEINLINE bool IsDead() const { return HP <= 0; }
virtual float TakeDamage(...) override;
void Die(AController* Killer);
UFUNCTION(NetMulticast, Reliable) void Multicast_Die();
```
```cpp
// EPCombatComponent.cpp — 히트 확정 후
UGameplayStatics::ApplyPointDamage(HitChar, FinalDamage, ...);
```

**문제점:**
- `TakeDamage` 안에서 HP 감소 → 사망 판정 → 래그돌 → 킬 크레딧이 전부 한 함수에 섞여 있다
- HP가 `int32` UPROPERTY라 **버프/디버프가 끼어들 자리가 없다**
- 사망 상태를 다른 클라이언트가 알려면 별도 복제 변수가 또 필요하다

---

## 구현 내용

### 1. ★ 메타 어트리뷰트 — Health를 직접 깎지 않는 이유

이 편의 핵심 개념. **GE는 `Health`가 아니라 `IncomingDamage`를 건드린다.**

```
[직접 방식]                      [메타 어트리뷰트 방식]
GE → Health -= 30                GE → IncomingDamage += 30
                                       ↓
                                 PostGameplayEffectExecute
                                   ├ 부위 배율 적용   (4-6)
                                   ├ 실드 50% 감산    (4-7)
                                   ├ (향후) 방어구 감산
                                   ↓
                                 Health -= 최종값
                                 IncomingDamage = 0  ← 즉시 초기화
```

직접 방식은 감산 로직을 넣을 자리가 **GE를 만드는 쪽(공격자)** 밖에 없다. 그러면 방어구를 추가할 때 모든 공격 경로를 고쳐야 한다.
메타 방식은 감산 로직이 **맞는 쪽의 AttributeSet 한 곳**에 모인다.

```cpp
// EPAttributeSet.h — 복제하지 않는다
UPROPERTY(BlueprintReadOnly, Category = "Attribute|Meta")
FGameplayAttributeData IncomingDamage;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, IncomingDamage)
```

**복제하지 않는 이유**: 서버에서 생겼다가 같은 프레임에 0으로 초기화되고 사라진다. 클라가 볼 필요도, 볼 수도 없는 값이다.

### 2. `PreAttributeChange` vs `PostGameplayEffectExecute`

둘을 헷갈리면 버그가 난다. 역할이 완전히 다르다.

| | `PreAttributeChange` | `PostGameplayEffectExecute` |
|---|---|---|
| 호출 시점 | 값이 바뀌기 **직전** | Instant GE가 BaseValue를 바꾼 **직후** |
| 호출 경로 | GE, `SetXxx()`, 초기화 등 **전부** | Instant GE **만** |
| 해야 할 일 | **클램핑만** | 게임플레이 로직 |
| 하면 안 되는 일 | 이벤트 발송, 상태 변경 | — |

```cpp
// 클램핑 전용 — 여기에 게임 로직을 넣으면 예상치 못한 경로에서 터진다
void UEPAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    else if (Attribute == GetMaxHealthAttribute())
        NewValue = FMath::Max(NewValue, 1.f);
}
```

> 이 Health 상한 클램프가 4-7에서 **힐이 100을 넘지 않게 하는 역할**을 그대로 한다. 별도 코드가 필요 없다.

### 3. 데미지 → 사망까지 한 함수에서

```cpp
void UEPAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    // 이 콜백은 항상 "이 AttributeSet 소유자에게 GE가 적용됐을 때"만 불린다
    // → Target은 언제나 GetOwningActor(). 별도 추출 불필요
    AEPCharacter* TargetCharacter = Cast<AEPCharacter>(GetOwningActor());

    // 공격자 추출 — 킬 크레딧용
    UAbilitySystemComponent* SourceASC =
        Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent();
    AActor* SourceActor = nullptr;
    if (SourceASC && SourceASC->AbilityActorInfo.IsValid()
        && SourceASC->AbilityActorInfo->AvatarActor.IsValid())
    {
        SourceActor = SourceASC->AbilityActorInfo->AvatarActor.Get();
    }

    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        const float Damage = GetIncomingDamage();
        SetIncomingDamage(0.f);            // ★ 즉시 초기화 — 안 하면 다음 피격에 누적된다

        if (Damage > 0.f)
        {
            // 이미 죽은 대상에게 데미지가 중복 적용되는 것을 막는 플래그
            const bool bWasAlive = GetHealth() > 0.f;

            const float NewHealth = FMath::Max(GetHealth() - Damage, 0.f);
            SetHealth(NewHealth);

            if (bWasAlive && NewHealth <= 0.f)
            {
                UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
                // State.Dead 재확인 — 연사로 동시에 여러 발이 들어올 때 GA_Death 중복 방지
                if (TargetASC && !TargetASC->HasMatchingGameplayTag(TAG_State_Dead))
                {
                    FGameplayEventData Payload;
                    Payload.Instigator = SourceActor;
                    TargetASC->HandleGameplayEvent(TAG_Event_Death, &Payload);
                }
            }
        }
    }
}
```

**중복 방어가 두 겹인 이유**를 포스팅에서 설명한다. 연사 무기는 한 프레임에 여러 발이 들어올 수 있다:
- `bWasAlive` — 이미 HP 0인 대상에 대한 추가 데미지 차단
- `HasMatchingGameplayTag(TAG_State_Dead)` — 사망 이벤트 중복 발송 차단

### 4. `GE_Damage` — SetByCaller로 수치를 런타임에 주입

```
Content/Data/GAS/GE_Damage (GameplayEffect Blueprint):
- DurationPolicy : Instant
- Modifiers:
    Attribute      : EPAttributeSet.IncomingDamage
    ModifierOp     : Add
    MagnitudeCalc  : SetByCaller
    DataTag        : Data.Damage
```

> **스크린샷 위치**: GE_Damage의 Modifiers 패널 (SetByCaller + DataTag 설정이 보이는 상태)

무기마다 데미지가 다르고 부위 배율까지 곱해지므로, GE 에셋에 숫자를 박을 수 없다. **에셋은 "IncomingDamage에 Data.Damage만큼 더한다"는 형태만 정의하고 값은 코드가 넣는다.**

```cpp
void UEPCombatComponent::ApplyGEDamage(
    AActor* Target, AActor* Instigator,
    TSubclassOf<UGameplayEffect> GEClass, float FinalDamage)
{
    if (!Target || !GEClass) return;

    IAbilitySystemInterface* TargetIF = Cast<IAbilitySystemInterface>(Target);
    UAbilitySystemComponent* TargetASC = TargetIF ? TargetIF->GetAbilitySystemComponent() : nullptr;
    IAbilitySystemInterface* InstigatorIF = Cast<IAbilitySystemInterface>(Instigator);
    UAbilitySystemComponent* InstigatorASC = InstigatorIF ? InstigatorIF->GetAbilitySystemComponent() : nullptr;
    if (!TargetASC || !InstigatorASC) return;

    FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
    Context.AddInstigator(Instigator, Instigator);

    FGameplayEffectSpecHandle Spec = InstigatorASC->MakeOutgoingSpec(GEClass, 1.f, Context);
    if (!Spec.IsValid()) return;

    // ★ GameplayTag 버전 SetByCaller — FName 버전은 쓰지 않는다
    Spec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Damage, FinalDamage);

    InstigatorASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
}
```

**SetByCaller는 반드시 GameplayTag 버전을 쓴다.** `FName` 오버로드도 있지만 오타가 런타임까지 가고 에디터 자동완성이 없다. 태그 버전은 4-1에서 만든 네이티브 태그를 그대로 쓰므로 컴파일 타임에 걸린다.

**ASC를 얻는 방법**도 관례를 정한다:
```cpp
// 채택
Cast<IAbilitySystemInterface>(Actor)->GetAbilitySystemComponent()
// 사용하지 않음 — Blueprint wrapper, 내부적으로 같은 일을 하면서 한 단계 더 거친다
UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)
```

교체는 한 줄이다:
```cpp
// 기존
UGameplayStatics::ApplyPointDamage(HitChar, FinalDamage, ...);
// 변경 후
ApplyGEDamage(HitChar, OwnerChar, GE_DamageClass, FinalDamage);
```

### 5. ★ `State.Dead`는 Infinite GE로만 복제된다

시리즈 관통 주제가 처음 등장하는 지점.

```
ActivationOwnedTags (GA가 켜져 있는 동안 자동 부여)
  → 소유 클라이언트 + 서버에서만 보인다
  → 다른 클라이언트에서는 존재하지 않는 것과 같다

GE GrantedTags (GE가 붙어 있는 동안 부여)
  → GE가 복제되므로 태그도 따라간다
```

`GA_Death`의 `ActivationOwnedTags`에 `State.Dead`를 넣는 게 자연스러워 보이지만, **다른 플레이어가 "쟤 죽었나"를 판정할 수 없다.** 그래서 Infinite GE를 따로 만든다.

```
Content/Data/GAS/GE_State_Dead:
- DurationPolicy : Infinite
- GrantedTags    : State.Dead
```

이 판단은 4-4에서 `State.Reloading`에 그대로 반복되고, 4-7의 `State.Casting`·`State.Shielded`까지 이어진다. **"다른 클라가 쿼리해야 하는 상태인가?"가 GE로 만들지 말지의 기준.**

### 6. `GA_Death` — 이벤트 트리거 어빌리티

```cpp
UEPGA_Death::UEPGA_Death()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bServerRespectsRemoteAbilityCancellation = false;

    // Event.Death가 발생하면 자동으로 활성화된다 — 아무도 직접 호출하지 않는다
    FAbilityTriggerData Trigger;
    Trigger.TriggerTag    = EmpGameplayTags::TAG_Event_Death;
    Trigger.TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;
    AbilityTriggers.Add(Trigger);
}
```

**ServerOnly인 이유**: 사망은 예측할 대상이 아니다. 클라가 먼저 죽여봐야 서버가 부정하면 되살아나는 그림이 나온다.

```cpp
void UEPGA_Death::ActivateAbility(...)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // State.Dead를 Infinite GE로 부여 → 복제 보장
    if (GE_StateDeadClass)
        ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo,
            GE_StateDeadClass.GetDefaultObject(), 1.f);

    if (AEPCharacter* Char = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get()))
    {
        // 무기 숨김 — 래그돌 위에 무기가 떠 있는 현상 방지
        if (UEPCombatComponent* CC = Char->GetCombatComponent())
            if (AEPWeapon* Weapon = CC->GetEquippedWeapon())
            {
                Weapon->SetActorHiddenInGame(true);
                Weapon->SetActorEnableCollision(false);
            }

        // 킬 크레딧 — Payload.Instigator로 넘어온 공격자
        const AActor* Killer = TriggerEventData ? TriggerEventData->Instigator.Get() : nullptr;
        AController* KillerController = Killer ? Killer->GetInstigatorController() : nullptr;
        if (AEPGameMode* GM = Char->GetWorld()->GetAuthGameMode<AEPGameMode>())
            GM->OnPlayerKilled(KillerController, Char->GetController());

        Char->Multicast_Die();                              // 래그돌

        if (AController* Controller = Char->GetController()) // 죽어서도 입력이 들어오는 것 방지
            Controller->UnPossess();
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
```

**`TakeDamage` 시절과 뭐가 달라졌나** — 포스팅에서 이 비교를 명확히 한다:

| | Before | After |
|---|---|---|
| 사망 판정 | `TakeDamage` 안에서 HP 체크 | `PostGEExecute`에서 이벤트 발송 |
| 사망 처리 | `Die()` 직접 호출 | `Event.Death` → GA가 알아서 활성화 |
| 결합도 | 데미지 코드가 GameMode·무기·컨트롤러를 다 안다 | 데미지 코드는 **이벤트만 던진다** |

### 7. Grant 시점 — `EditDefaultsOnly` 함정

```cpp
// EPCharacter.h — protected
UPROPERTY(EditDefaultsOnly, Category = "GAS")
TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;
```
```cpp
// PossessedBy 서버측
for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
    if (AbilityClass)
        ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
```

> **`UEPGA_Death::StaticClass()`를 직접 넘기면 안 된다.** `GE_StateDeadClass`가 `EditDefaultsOnly`라서 C++ 클래스의 CDO에서는 항상 null이다. **Blueprint 서브클래스(`BP_GA_Death`)를 만들어 에디터에서 GE를 할당한 뒤 그 클래스를 참조**해야 한다.
>
> 이 패턴은 4-3(`PrimaryUseAbilityClass`), 4-7(스킬 GA) 전부에 반복된다. **"C++ GA + Blueprint 서브클래스에서 에셋 주입"이 이 프로젝트의 표준.**

리스폰해도 ASC는 PlayerState에 살아있으므로 재Grant가 필요 없다 — 4-1의 배치 결정이 여기서 이득으로 돌아온다.

### 8. UI 연동 — `OnRep_HP`가 사라진 자리

`HP` UPROPERTY를 지우면 `OnRep_HP` 경로도 없어진다. Attribute 변경 델리게이트로 교체한다.

```cpp
// EPCharacter.cpp — InitASC() 내부
// IsLocallyControlled() 가드 필수 — 서버에서는 모든 Character의 InitASC가 호출된다
if (IsLocallyControlled())
{
    ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetHealthAttribute())
       .AddUObject(this, &AEPCharacter::OnHealthChanged);
}
```

**역할을 두 곳으로 나눈다:**

| 등록 위치 | 콜백 | 용도 |
|---|---|---|
| `AEPPlayerState::BeginPlay` | `HealthChanged` | 서버 로직 (킬 크레딧 등) |
| `AEPCharacter::InitASC` | `OnHealthChanged` | **로컬 UI 갱신만** |

한 곳에 몰아넣고 조건 분기하면 서버/클라 실행 경로가 섞여 나중에 디버깅이 어려워진다. 이 델리게이트 구조가 4-8(HUD)의 기반이 된다.

---

## 데이터 흐름 (포스팅용 다이어그램)

```
[SSR::ConfirmHitscan] 히트 확정
  → ApplyGEDamage(Target, Instigator, GE_Damage, FinalDamage)
      → SetSetByCallerMagnitude(Data.Damage, FinalDamage)
      → ApplyGameplayEffectSpecToTarget
  → AttributeSet::PostGameplayEffectExecute
      → IncomingDamage 소모 → Health 감소
      → Health <= 0 → HandleGameplayEvent(Event.Death)
  → GA_Death::ActivateAbility (ServerOnly, 트리거 자동 활성화)
      → GE_State_Dead (Infinite) → State.Dead 복제
      → 무기 숨김 → 킬 크레딧 → Multicast_Die() → UnPossess()
      → EndAbility
```

이 그림이 **3단계(SSR)와 4단계(GAS)가 붙는 지점**이다. `ConfirmHitscan`은 한 줄도 바뀌지 않았고, 그 뒤 한 줄만 교체됐다는 점을 강조한다.

---

## 함정 정리

| 상황 | 원인 | 해결 |
|------|------|------|
| GE를 적용해도 Health가 안 줄어듦 | `Data.Damage` SetByCaller 미주입 | `SetSetByCallerMagnitude` 호출 확인 |
| 사망 이벤트가 두 번 발생 | 한 프레임에 여러 발 명중 | `bWasAlive` + `State.Dead` 이중 체크 |
| `State.Dead`가 다른 클라에서 안 보임 | `ActivationOwnedTags`는 복제 안 됨 | Infinite GE `GrantedTags`로 부여 |
| `GA_Death`가 서버에서 활성화 안 됨 | GA Grant 누락 | `PossessedBy` 서버측 `GiveAbility` 확인 |
| `GE_StateDeadClass`가 항상 null | C++ 클래스를 직접 `DefaultAbilities`에 넣음 | Blueprint 서브클래스 사용 |
| `ApplyGEDamage`가 클라에서 실행됨 | 호출 경로 추가 시 가드 누락 | `HasAuthority()` 가드 |

---

## 결과

**확인 항목 (PIE 2인):**
- 피격 → `showdebug abilitysystem`에서 Health 감소 확인
- Health 0 → `State.Dead` 태그가 **양쪽 클라 모두에서** 보임
- `Multicast_Die()` → 래그돌, 무기 숨김, `UnPossess` 동작
- 연사로 오버킬 → `GA_Death`가 한 번만 활성화

**한계 및 향후 개선:**
- `IncomingDamage`에 부위 배율이 아직 적용되지 않는다 — 배율은 아직 `HandleHitscanFire`에서 곱해서 넘긴다. **4-6에서 태그 기반으로 교체된다.**
- 방어구(`ArmorHead`/`ArmorChest`/`ArmorLimbs`)는 미구현. GE Context에 HitZone 태그를 실어 보내 `PostGEExecute`에서 감산하는 방향으로 설계만 해둔 상태
- 데미지 숫자·피격 방향 인디케이터 같은 피드백은 GameplayCue로 붙일 자리가 이미 열려 있다

---

## 참고

- `DOCS/Notes/04/04_GAS_02_DamagePipeline.md` — 구현 전체
- `DOCS/Notes/04/04_GAS_00_Reference.md` §4 — 메타 어트리뷰트 패턴
- `DOCS/Notes/04/04_GAS_DOCS.md` §3 — 어트리뷰트 설계 및 향후 확장
