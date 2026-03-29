# 기획서: GAS 기반 세팅 (ASC + AttributeSet)

> 우선순위 1 — 이후 모든 GAS 작업의 전제 조건.
> 이 단계 없이는 GE 적용, Ability Grant, 태그 쿼리 불가.

---

## 1. 목표

- `AEPPlayerState`에 `UAbilitySystemComponent` + `UEPAttributeSet` 탑재
- `AEPCharacter`가 `IAbilitySystemInterface`를 구현하여 ASC 노출
- 서버(`PossessedBy`) + 클라(`OnRep_PlayerState`) 양쪽 `InitAbilityActorInfo` 호출
- `Build.cs` 모듈 추가, `GameInstance::Init()` 글로벌 데이터 초기화
- Attribute 복제 및 초기화 (C++ 직접, Blueprint 에셋 불필요)

완료 기준: PIE 2인 멀티에서 양쪽 `GetAbilitySystemComponent()` null 없음, `Health = 100` 복제 확인.

---

## 2. 변경 대상 파일

| 파일 | 작업 |
|------|------|
| `EmploymentProj.Build.cs` | GameplayAbilities, GameplayTags, GameplayTasks 모듈 추가 |
| `EPGameInstance.h/cpp` | `UAbilitySystemGlobals::InitGlobalData()` 호출 |
| `EPPlayerState.h/cpp` | ASC + AttributeSet 추가, `GetAbilitySystemComponent()` 구현 |
| `EPCharacter.h/cpp` | `IAbilitySystemInterface` 구현, `PossessedBy` + `OnRep_PlayerState` 추가 |
| `EPAttributeSet.h/cpp` | **신규** — Health/MaxHealth + IncomingDamage 어트리뷰트 정의 |
| `GAS/EPNativeGameplayTags.h/cpp` | **신규** — 네이티브 태그 매크로 모음 |

---

## 3. 구현 순서

### Step 1 — Build.cs 모듈 추가

```cpp
// EmploymentProj.Build.cs
// Public 헤더(EPPlayerState.h, EPCharacter.h 등)에서 GAS 헤더를 include하므로
// Public으로 선언해야 이 모듈에 의존하는 다른 모듈도 GAS 타입을 볼 수 있음
PublicDependencyModuleNames.AddRange(new string[] {
    "GameplayAbilities",
    "GameplayTags",
    "GameplayTasks"
});
```

### Step 2 — GameInstance 글로벌 초기화

> `InitGlobalData()`는 프로젝트에서 **반드시 한 번** 호출해야 GE Execution / Cue 등이 등록됨.
> `EPGameInstance`가 없으면 신규 C++ 클래스 생성 후 Project Settings → Maps & Modes → **Game Instance Class** 지정. ← 유일한 에디터 작업.

```cpp
// EPGameInstance.cpp
#include "AbilitySystemGlobals.h"

void UEPGameInstance::Init()
{
    Super::Init();
    UAbilitySystemGlobals::Get().InitGlobalData();
}
```

### Step 3 — EPAttributeSet 신규 구현

헤더에는 `AttributeSet.h`만 include. `AbilitySystemComponent.h`는 무거우므로 .cpp에서만 포함.

```cpp
// Public/GAS/EPAttributeSet.h
#pragma once
#include "AttributeSet.h"
#include "AbilitySystemComponent.h" // ATTRIBUTE_ACCESSORS 매크로에 필요
#include "EPAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName)           \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)               \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)               \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class EMPLOYMENTPROJ_API UEPAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    // --- Health ---
    UPROPERTY(BlueprintReadOnly, Category = "Attribute|Health",
        ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UEPAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, Category = "Attribute|Health",
        ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UEPAttributeSet, MaxHealth)

    // --- IncomingDamage (메타 어트리뷰트 — 복제 없음, 서버 전용) ---
    // GE_Damage가 이 값을 Add하면 PostGameplayEffectExecute에서 Health로 변환 후 초기화
    UPROPERTY(BlueprintReadOnly, Category = "Attribute|Meta")
    FGameplayAttributeData IncomingDamage;
    ATTRIBUTE_ACCESSORS(UEPAttributeSet, IncomingDamage)

    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
};
```

```cpp
// Private/GAS/EPAttributeSet.cpp
#include "GAS/EPAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GAS/EPNativeGameplayTags.h"

// Attribute 복제 등록 — 누락 시 클라이언트에서 값 변경 안 됨
void UEPAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // REPNOTIFY_Always: 예측으로 인해 로컬 값이 서버 값과 같아도 OnRep 트리거
    DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, Health,    COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    // IncomingDamage는 복제 없음 — 생략
}

// OnRep — GAMEPLAYATTRIBUTE_REPNOTIFY 없으면 GAS가 Attribute 변경을 인식 못 함
void UEPAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, Health, OldValue);
}

void UEPAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, MaxHealth, OldValue);
}

// PreAttributeChange: 클램핑 전용. Gameplay 로직 금지 (GE/setter 양쪽에서 호출됨)
void UEPAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    }
    else if (Attribute == GetMaxHealthAttribute())
    {
        // 0 이하 방지만 — 이 프로젝트는 MaxHealth 버프 시스템 없음
        // (GASDoc의 AdjustAttributeForMaxChange 비율 유지 패턴은 RPG용이므로 미적용)
        NewValue = FMath::Max(NewValue, 1.f);
    }

// PostGameplayEffectExecute: instant GE의 BaseValue 변경 후만 호출됨
// IncomingDamage 처리 + 사망 이벤트 발송
void UEPAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    // PostGameplayEffectExecute는 항상 이 AttributeSet 소유자에게 GE가 적용됐을 때만 호출됨
    // → TargetActor는 항상 GetOwningActor()와 동일하므로 별도 추출 불필요
    AEPCharacter* TargetCharacter = Cast<AEPCharacter>(GetOwningActor());

    // --- Source Actor 추출 (킬러 정보, 킬 크레딧 등에 사용) ---
    UAbilitySystemComponent* SourceASC = Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent();
    AActor* SourceActor = nullptr;
    if (SourceASC && SourceASC->AbilityActorInfo.IsValid() && SourceASC->AbilityActorInfo->AvatarActor.IsValid())
    {
        SourceActor = SourceASC->AbilityActorInfo->AvatarActor.Get();
    }

    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        const float Damage = GetIncomingDamage();
        SetIncomingDamage(0.f); // 즉시 초기화 — 누적 방지

        if (Damage > 0.f)
        {
            // WasAlive 플래그: 이미 죽은 대상에게 데미지가 중복 적용되는 것을 방지 (GASDoc 패턴)
            // AttributeSet 안에서는 GetHealth()로 직접 판단 — IsDead()는 GAS 이관 전후 구현이 달라 혼용 금지
            const bool bWasAlive = GetHealth() > 0.f;

            const float NewHealth = FMath::Max(GetHealth() - Damage, 0.f);
            SetHealth(NewHealth);

            if (bWasAlive && NewHealth <= 0.f)
            {
                // State.Dead 체크: 연속 피격 시 GA_Death 중복 활성화 방지
                UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
                if (TargetASC && !TargetASC->HasMatchingGameplayTag(TAG_State_Dead))
                {
                    // 사망 GameplayEvent → GA_Death가 AbilityTrigger로 수신하여 Multicast_Die 호출
                    FGameplayEventData Payload;
                    Payload.Instigator = SourceActor;
                    TargetASC->HandleGameplayEvent(TAG_Event_Death, &Payload);
                }
            }
        }
    }
}
```

### Step 4 — EPPlayerState: ASC + AttributeSet 탑재

```cpp
// Public/Core/EPPlayerState.h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "EPPlayerState.generated.h"

class UAbilitySystemComponent;
class UEPAttributeSet;

UCLASS()
class EMPLOYMENTPROJ_API AEPPlayerState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AEPPlayerState();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    UEPAttributeSet* GetAttributeSet() const { return AttributeSet; }

    // 기존 멤버 유지 ...

private:
    UPROPERTY(VisibleAnywhere, Category = "GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY()
    TObjectPtr<UEPAttributeSet> AttributeSet;
};
```

```cpp
// Private/Core/EPPlayerState.cpp
#include "AbilitySystemComponent.h"
#include "GAS/EPAttributeSet.h"

AEPPlayerState::AEPPlayerState()
{
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);

    // Mixed: 소유자에게는 GE Full 복제, 타인에게는 Tag/Cue만 복제. 멀티 슈터 표준.
    // Mixed 모드는 OwnerActor의 Owner가 Controller여야 함 → PossessedBy에서 SetOwner 호출
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // PlayerState NetUpdateFrequency 기본값이 낮으면 Attribute 복제 지연 발생
    NetUpdateFrequency = 100.f;

    // AttributeSet은 ASC와 같은 Actor에 SubObject로 생성하면 자동 등록됨
    AttributeSet = CreateDefaultSubobject<UEPAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AEPPlayerState::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}
```

### Step 5 — EPCharacter: IAbilitySystemInterface 구현 + InitAbilityActorInfo

```cpp
// Public/Core/EPCharacter.h 추가
#include "AbilitySystemInterface.h"

class UAbilitySystemComponent;

UCLASS()
class EMPLOYMENTPROJ_API AEPCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
    virtual void PossessedBy(AController* NewController) override;   // 서버
    virtual void OnRep_PlayerState() override;                       // 클라

private:
    // PS의 ASC를 캐싱 — 매번 PS를 거치지 않기 위함
    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    void InitASC();
};
```

```cpp
// Private/Core/EPCharacter.cpp
#include "Core/EPPlayerState.h"
#include "AbilitySystemComponent.h"

UAbilitySystemComponent* AEPCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

// 서버: Controller Possess 직후 호출
void AEPCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // Mixed 모드 요구사항: OwnerActor(PS)의 Owner가 Controller여야 함
    // UE 4.24+에서는 PossessedBy가 Pawn의 Owner를 Controller로 설정하지만 명시적으로 처리
    if (AEPPlayerState* PS = GetPlayerState<AEPPlayerState>())
    {
        AbilitySystemComponent = PS->GetAbilitySystemComponent();
        InitASC();

        // Attribute 초기화 — 서버에서만 수행, 클라는 복제로 동기화
        if (UEPAttributeSet* AS = PS->GetAttributeSet())
        {
            AS->InitHealth(100.f);
            AS->InitMaxHealth(100.f);
        }

        // 리스폰 시: State.Dead 태그 강제 제거 (GASDoc 패턴)
        // 첫 스폰에는 영향 없음, 재스폰 시 이전 사망 상태가 남아있는 경우 방지
        AbilitySystemComponent->SetTagMapCount(TAG_State_Dead, 0);

        // 기본 Ability 부여 — GA_Death 등 항상 필요한 어빌리티 (추후 구현 시 여기서 GrantAbility)
        // AddCharacterAbilities();
    }
}

// 클라: OnRep_PlayerState에서 PS 복제 완료 후 호출
void AEPCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (AEPPlayerState* PS = GetPlayerState<AEPPlayerState>())
    {
        AbilitySystemComponent = PS->GetAbilitySystemComponent();
        InitASC();
    }
}

void AEPCharacter::InitASC()
{
    AEPPlayerState* PS = GetPlayerState<AEPPlayerState>();
    if (!PS || !AbilitySystemComponent) return;

    // Owner = PlayerState (Ability 상태 보존 주체)
    // Avatar = Character (실제 월드 Actor)
    // 캐릭터 리스폰 시 PS는 유지 → Ability/GE 상태 보존됨
    AbilitySystemComponent->InitAbilityActorInfo(PS, this);
}
```

> **`InitAbilityActorInfo` 두 번 호출** (서버 PossessedBy + 클라 OnRep_PlayerState) 은 의도된 패턴.
> 클라에서 `Can't activate LocalOnly or LocalPredicted ability` 에러가 나면 클라 초기화 누락.

### Attribute 변경 델리게이트 (UI 갱신용)

UI에서 HP바를 갱신하려면 Attribute 변경 시 콜백을 등록해야 한다 (GASDoc 패턴).
`OnRep_Health`와는 별개 — OnRep는 GAS 내부 동기화용이고, 이 델리게이트는 게임 로직/UI 반응용.

```cpp
// EPPlayerState.h — 델리게이트 핸들 보관
protected:
    virtual void BeginPlay() override;
    FDelegateHandle HealthChangedDelegateHandle;

    void HealthChanged(const FOnAttributeChangeData& Data);
```

```cpp
// EPPlayerState.cpp
void AEPPlayerState::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystemComponent && AttributeSet)
    {
        // Attribute 변경 → UI 갱신 콜백 등록
        HealthChangedDelegateHandle =
            AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
                AttributeSet->GetHealthAttribute())
            .AddUObject(this, &AEPPlayerState::HealthChanged);

        // Tag 변경 콜백 등록 (State.Dead 등)
        // AbilitySystemComponent->RegisterGameplayTagEvent(TAG_State_Dead, ...)
    }
}

void AEPPlayerState::HealthChanged(const FOnAttributeChangeData& Data)
{
    // UI 갱신 or 사망 처리 연결
    // GASDoc은 여기서 IsAlive() 체크 후 Die() 호출하지만,
    // 이 프로젝트는 PostGameplayEffectExecute → GA_Death 이벤트 방식을 사용
}
```

> 델리게이트 해제: Actor 소멸 시 자동 해제되지 않으므로 `EndPlay`에서
> `AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(...).Remove(Handle)` 권장.

### Step 6 — EPNativeGameplayTags 신규 작성

```cpp
// Public/GAS/EPNativeGameplayTags.h
#pragma once
#include "NativeGameplayTags.h"

namespace EmpGameplayTags
{
    // State
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Dead)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Reloading)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_UsingItem)

    // Event
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Death)

    // Ability
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Item_PrimaryUse)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Item_Reload)

    // Cooldown (GE_FireCooldown GrantedTags 식별용)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Cooldown_Weapon_PrimaryUse)

    // Data — SetByCaller Magnitude 주입용 태그
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Cooldown)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_ReloadDuration)

    // HitZone
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitZone_Head)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitZone_Chest)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitZone_Limbs)
}
```

```cpp
// Private/GAS/EPNativeGameplayTags.cpp
#include "GAS/EPNativeGameplayTags.h"

namespace EmpGameplayTags
{
    UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dead,                 "State.Dead")
    UE_DEFINE_GAMEPLAY_TAG(TAG_State_Reloading,            "State.Reloading")
    UE_DEFINE_GAMEPLAY_TAG(TAG_State_UsingItem,            "State.UsingItem")
    UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Death,                "Event.Death")
    UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Item_PrimaryUse,    "Ability.Item.PrimaryUse")
    UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Item_Reload,        "Ability.Item.Reload")
    UE_DEFINE_GAMEPLAY_TAG(TAG_Cooldown_Weapon_PrimaryUse, "Cooldown.Weapon.PrimaryUse")
    UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage,                "Data.Damage")
    UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Cooldown,              "Data.Cooldown")
    UE_DEFINE_GAMEPLAY_TAG(TAG_Data_ReloadDuration,        "Data.ReloadDuration")
    UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Head,               "HitZone.Head")
    UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Chest,              "HitZone.Chest")
    UE_DEFINE_GAMEPLAY_TAG(TAG_HitZone_Limbs,              "HitZone.Limbs")
}
```

> `UE_DEFINE_GAMEPLAY_TAG`는 엔진 시작 시 자동 등록 — 에디터에서 별도 태그 생성 불필요.
> Project Settings → GameplayTags에서 등록 결과를 확인만 하면 됨.

---

## 4. Attribute 초기화 방식 비교

| 방식 | 설명 | 사용 시기 |
|------|------|-----------|
| `InitHealth(float)` (이번 선택) | ATTRIBUTE_ACCESSORS 자동 생성 함수. C++에서 직접 호출 | 고정값, 단순 구조 |
| Instant GE (Blueprint 에셋) | GE_InitStats를 PossessedBy에서 Apply | 레벨/직업별로 다른 초기값 필요 시 |
| DataTable + InitAttributeSetDefaults | 레벨 기반 초기값 테이블 | 레벨 시스템 있을 때 |

이 프로젝트는 HP 고정값이므로 `InitHealth(100.f)` 직접 호출로 충분.

---

## 5. 완료 체크리스트

- [ ] Build.cs 3개 모듈 추가 후 컴파일 성공
- [ ] Project Settings → Game Instance Class → `EPGameInstance` 지정
- [ ] `GameInstance::Init()`에서 `InitGlobalData()` 호출 확인
- [ ] PIE 서버: `PossessedBy` → `InitASC()` + `InitHealth(100)` 실행 확인
- [ ] PIE 클라: `OnRep_PlayerState` → `InitASC()` 실행 확인
- [ ] `GetAbilitySystemComponent()` null 없음 (서버/클라 모두)
- [ ] `AttributeSet::Health` = 100 복제 확인 (`showdebug abilitysystem` 커맨드)
- [ ] 네이티브 태그 Project Settings에서 자동 등록 확인

---

## 6. 함정 & 주의사항

| 상황 | 원인 | 해결 |
|------|------|------|
| 클라 ASC null / Ability 활성화 안 됨 | `OnRep_PlayerState`에서 InitAbilityActorInfo 미호출 | 반드시 양쪽(PossessedBy + OnRep_PlayerState)에서 호출 |
| `Can't activate LocalOnly or LocalPredicted ability` | 클라 ASC 초기화 누락 | OnRep_PlayerState 경로 확인 |
| Attribute 복제 안 됨 | `GetLifetimeReplicatedProps` 또는 `OnRep` 미구현 | DOREPLIFETIME_CONDITION_NOTIFY + GAMEPLAYATTRIBUTE_REPNOTIFY 확인 |
| Mixed 모드에서 GE 복제 안 됨 | OwnerActor의 Owner가 Controller가 아님 | PossessedBy에서 `SetOwner(NewController)` 또는 UE 4.24+ 자동 처리 확인 |
| GE 적용 안 됨 | `InitGlobalData()` 미호출 | GameInstance::Init에서 반드시 호출 |
| 복제 지연 | PlayerState NetUpdateFrequency 기본값 낮음 | `NetUpdateFrequency = 100.f` |
