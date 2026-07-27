# Post 4-1 작성 가이드 — GAS 도입 배경과 기반 세팅

> **예상 제목**: `[UE5] 추출 슈터 4-1. GAS 도입: ASC를 PlayerState에 두는 이유와 AttributeSet 설계`
> **참고 문서**: `DOCS/Notes/04/04_GAS_DOCS.md` §1~4, `04_GAS_01_Foundation.md`, `04_GAS_00_Reference.md`

---

## 개요

**이 포스팅에서 다루는 것:**
- 직접 만든 전투 시스템이 어디서 막혔는가 — GAS로 옮긴 실제 이유
- ASC를 Character가 아닌 PlayerState에 두는 판단
- `UEPAttributeSet` 설계와 Attribute 복제 규약
- NativeGameplayTags로 태그를 코드에 못 박기

**왜 이렇게 구현했는가 (설계 의도):**
- 3단계까지 히트스캔·부위 판정·랙 보상을 전부 직접 구현했다. 동작은 했지만 **기능을 하나 추가할 때마다 Character와 CombatComponent가 같이 부풀었다.**
- GAS는 "전투 기능을 클래스 바깥의 데이터로 빼는" 구조다. 이 시리즈는 GAS 튜토리얼이 아니라 **자체 구현 → GAS 이관 기록**이다.

> **용어 정리 박스 (이 편에서 한 번만, 이후 편은 여기로 링크)**
> - **ASC** (AbilitySystemComponent) — GAS의 중심. Attribute·Tag·Ability·Effect를 전부 소유한다
> - **GA** (GameplayAbility) — "발사한다", "재장전한다" 같은 행동 하나
> - **GE** (GameplayEffect) — Attribute를 바꾸거나 Tag를 붙이는 효과. 즉발/지속/무한
> - **AttributeSet** — HP·탄약 같은 수치 묶음
> - **GameplayTag** — 계층형 문자열 상태 표식 (`State.Reloading`)

---

## 구현 전 상태 (Before)

3단계까지의 구조를 표로 정리하고, 각각이 어디서 막혔는지 보여준다.

| 문제 | 당시 코드 | 실제로 겪은 증상 |
|------|-----------|------------------|
| 상태 분산 | `WeaponState` enum, `HP` UPROPERTY, `LastServerFireTime`이 각자 관리 | 상태 충돌, 같은 검증을 여러 곳에서 중복 |
| 복제 한계 | `State.Reloading`에 해당하는 개념이 없음 | **다른 클라이언트에서 "쟤 재장전 중"을 알 수 없다** |
| 확장 비용 | 무기/스킬 추가 시 Character·CombatComponent 직접 수정 | 사이드이펙트 위험 |
| 수치 관리 | 탄약·HP·쿨타임을 각각 UPROPERTY로 복제 | 복제 전략이 제각각 |

```cpp
// 3단계 EPCharacter.h — 직접 관리하던 HP
UPROPERTY(ReplicatedUsing = OnRep_HP) int32 HP = 100;
int32 MaxHP = 100;
FORCEINLINE bool IsDead() const { return HP <= 0; }
virtual float TakeDamage(...) override;
```

> **이 표가 시리즈 전체의 목차 역할을 한다.** 각 행이 어느 편에서 해결되는지 미리 링크를 걸어둔다.
> 상태 분산 → 4-2·4-4 / 복제 한계 → 4-4 / 확장 비용 → 4-3·4-7 / 수치 관리 → 4-2·4-4

---

## 구현 내용

### 1. 모듈 추가 — Public이어야 하는 이유

```csharp
// EmploymentProj.Build.cs
// Public 헤더(EPPlayerState.h, EPCharacter.h)에서 GAS 헤더를 include하므로
// Public으로 선언해야 이 모듈에 의존하는 다른 모듈도 GAS 타입을 볼 수 있다
PublicDependencyModuleNames.AddRange(new string[] {
    "GameplayAbilities", "GameplayTags", "GameplayTasks"
});
```

`IAbilitySystemInterface`를 public 헤더에서 상속하므로 Private으로 넣으면 링크가 깨진다.

### 2. `InitGlobalData()` — 빠뜨리면 조용히 죽는다

```cpp
// EPGameInstance.cpp
void UEPGameInstance::Init()
{
    Super::Init();
    UAbilitySystemGlobals::Get().InitGlobalData();
}
```

프로젝트 전체에서 **반드시 한 번** 호출해야 GE Execution / GameplayCue 등이 등록된다.
안 하면 GE를 적용해도 **에러 없이 아무 일도 일어나지 않는다.** GAS 첫 삽에서 가장 많이 막히는 지점.

> **에디터 작업**: Project Settings → Maps & Modes → Game Instance Class 지정.
> 이 단계에서 유일한 에디터 작업이다.
> **스크린샷 위치**: Project Settings의 Game Instance Class 드롭다운

### 3. ★ ASC를 PlayerState에 둔다

이 편의 핵심 판단. GAS 도입 시 첫 번째로 결정해야 하는 것이다.

| 배치 | 장점 | 단점 |
|------|------|------|
| Character | 접근이 짧다, 캐릭터별 완결 | **죽으면 ASC가 같이 사라진다** |
| **PlayerState (선택)** | 사망/리스폰 시 Ability Grant·GE·Attribute 보존 | ASC 접근이 한 단계 길어짐 |

추출 슈터는 **사망과 리스폰이 반복되는 게임**이다. Character에 ASC를 두면 리스폰마다 어빌리티를 다시 부여하고 쿨타임이 초기화된다. PlayerState에는 이미 `Kills`, `Money` 같은 영속 데이터가 있으므로 자연스러운 확장 위치다.

```cpp
// EPPlayerState.cpp — 생성자
AEPPlayerState::AEPPlayerState()
{
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);

    // Mixed: 소유자에게는 GE 전체 복제, 타인에게는 Tag/Cue만. 멀티 슈터 표준
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    // PlayerState 기본 NetUpdateFrequency가 낮으면 Attribute 복제가 눈에 띄게 늦다
    NetUpdateFrequency = 100.f;

    // ASC와 같은 Actor에 SubObject로 만들면 자동 등록된다
    AttributeSet = CreateDefaultSubobject<UEPAttributeSet>(TEXT("AttributeSet"));
}
```

**Replication Mode 3종 비교** (포스팅에서 표로):

| 모드 | 대상 | 용도 |
|------|------|------|
| Full | 모두에게 GE 전체 복제 | 싱글플레이 |
| **Mixed (선택)** | 소유 클라 = GE 전체, 타 클라 = Tag/Cue만 | **멀티플레이어 플레이어 캐릭터** |
| Minimal | 아무에게도 GE 미복제, Tag/Cue만 | AI / NPC |

> Mixed는 **OwnerActor(PlayerState)의 Owner가 Controller**여야 동작한다. UE 4.24+에서 `PossessedBy`가 처리하지만, 안 되면 GE가 소유 클라에도 복제되지 않는다.
> 이 제약은 4-8(HUD)에서 다시 등장한다 — 타 플레이어의 쿨타임을 UI에 못 그리는 이유가 이것.

### 4. ★ `InitAbilityActorInfo`는 서버·클라 양쪽에서

가장 흔한 실수 지점이다.

```cpp
// 서버 경로
void AEPCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (AEPPlayerState* PS = GetPlayerState<AEPPlayerState>())
    {
        AbilitySystemComponent = PS->GetAbilitySystemComponent();
        InitASC();

        // Attribute 초기화는 서버만 — 클라는 복제로 동기화된다
        if (UEPAttributeSet* AS = PS->GetAttributeSet())
        {
            AS->InitHealth(100.f);
            AS->InitMaxHealth(100.f);
        }

        // 리스폰 시 이전 사망 상태 잔류 방지
        AbilitySystemComponent->SetTagMapCount(TAG_State_Dead, 0);
    }
}

// 클라 경로 — PlayerState 복제 완료 시점
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

    // Owner  = PlayerState (Ability 상태를 보존하는 주체)
    // Avatar = Character   (실제 월드에 있는 Actor)
    AbilitySystemComponent->InitAbilityActorInfo(PS, this);
}
```

**Owner / Avatar 분리가 핵심이다.** Owner는 죽어도 남고, Avatar만 교체된다. 이게 PlayerState 배치를 선택한 이유가 코드로 드러나는 지점이므로 포스팅에서 그림으로 설명한다:

```
[리스폰 전]                    [리스폰 후]
Owner  = PlayerState  ───────  Owner  = PlayerState  (그대로)
Avatar = Character_A           Avatar = Character_B  (교체)
         └ 쿨타임·GE·Attribute는 Owner 쪽에 있으므로 살아남는다
```

> 클라에서 `Can't activate LocalOnly or LocalPredicted ability` 에러가 나면 **십중팔구 `OnRep_PlayerState` 경로 누락**이다.

### 5. AttributeSet — 매크로 4종 세트

```cpp
// Public/GAS/EPAttributeSet.h
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName)           \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)               \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)               \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UPROPERTY(BlueprintReadOnly, Category = "Attribute|Health", ReplicatedUsing = OnRep_Health)
FGameplayAttributeData Health;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, Health)

UPROPERTY(BlueprintReadOnly, Category = "Attribute|Health", ReplicatedUsing = OnRep_MaxHealth)
FGameplayAttributeData MaxHealth;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, MaxHealth)

// 메타 Attribute — 복제하지 않는다. 서버에서만 존재하고 즉시 소모된다 (4-2에서 상세)
UPROPERTY(BlueprintReadOnly, Category = "Attribute|Meta")
FGameplayAttributeData IncomingDamage;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, IncomingDamage)
```

이 매크로 한 줄이 `GetHealth()` / `SetHealth()` / `InitHealth()` / `GetHealthAttribute()` 네 개를 생성한다.

**`FGameplayAttributeData`가 float가 아닌 이유** — BaseValue와 CurrentValue를 나눠 갖는다. 버프가 CurrentValue만 바꾸고 사라지면 BaseValue로 복귀한다. 이 프로젝트는 아직 버프가 없지만 구조는 그대로 얻는다.

### 6. Attribute 복제 — 매크로 두 개가 짝

```cpp
void UEPAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // REPNOTIFY_Always: 예측 때문에 로컬 값이 서버 값과 같아도 OnRep를 강제로 트리거
    DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, Health,    COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UEPAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    // IncomingDamage는 복제하지 않는다 — 서버 전용 메타
}

void UEPAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UEPAttributeSet, Health, OldValue);
}
```

**`REPNOTIFY_Always`가 왜 필요한가** — 클라가 예측으로 HP를 이미 90으로 낮췄는데 서버도 90을 보내면, 기본 설정에서는 "값이 같다"고 OnRep을 건너뛴다. 그러면 GAS 내부 델리게이트가 브로드캐스트되지 않아 **UI가 갱신되지 않는다.** 4-8(HUD)에서 이 설정에 그대로 의존한다.

**`GAMEPLAYATTRIBUTE_REPNOTIFY` 없이 빈 OnRep을 두면** 값은 복제되는데 GAS가 변경을 인식하지 못한다. 증상이 "복제는 되는데 반응이 없음"이라 원인 찾기가 오래 걸린다.

### 7. NativeGameplayTags — 문자열을 컴파일 타임으로

```cpp
// Public/GAS/EPNativeGameplayTags.h
namespace EmpGameplayTags
{
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Dead)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Reloading)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Death)
    EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Data_Damage)
    // ...
}
```
```cpp
// Private/GAS/EPNativeGameplayTags.cpp
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Dead,      "State.Dead")
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Reloading, "State.Reloading")
UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Death,     "Event.Death")
UE_DEFINE_GAMEPLAY_TAG(TAG_Data_Damage,     "Data.Damage")
```

`FGameplayTag::RequestGameplayTag(FName("State.Dead"))` 방식은 오타가 런타임까지 간다. 네이티브 선언은 **오타가 컴파일 에러**가 되고 IDE 자동완성도 된다. 엔진 시작 시 자동 등록되므로 에디터에서 태그를 만들 필요도 없다.

**태그 네임스페이스 규칙** (시리즈 내내 유지):

| 접두사 | 의미 | 예 |
|--------|------|-----|
| `State.*` | 지속 상태 | `State.Reloading` |
| `Event.*` | 순간 신호 | `Event.Death` |
| `Ability.*` | GA 식별 | `Ability.Item.PrimaryUse` |
| `Cooldown.*` | 쿨타임 GE 표식 | `Cooldown.Weapon.PrimaryUse` |
| `Data.*` | SetByCaller 수치 주입 키 | `Data.Damage` |
| `HitZone.*` | 피격 부위 (4-6) | `HitZone.Head` |

---

## 함정 정리

| 상황 | 원인 | 해결 |
|------|------|------|
| 클라 ASC null / 어빌리티 활성화 실패 | `OnRep_PlayerState`에서 `InitAbilityActorInfo` 미호출 | 서버·클라 양쪽에서 호출 |
| `Can't activate LocalOnly or LocalPredicted ability` | 위와 동일 | 클라 초기화 경로 확인 |
| Attribute 복제 안 됨 | `GetLifetimeReplicatedProps` 또는 `OnRep` 누락 | 두 매크로가 짝인지 확인 |
| 값은 복제되는데 UI 반응 없음 | `GAMEPLAYATTRIBUTE_REPNOTIFY` 누락 | OnRep 본문 확인 |
| Mixed 모드인데 GE가 소유 클라에도 안 옴 | PlayerState의 Owner가 Controller가 아님 | `PossessedBy` 경로 확인 |
| GE를 적용해도 아무 일 없음 | `InitGlobalData()` 미호출 | GameInstance::Init |
| Attribute 복제가 눈에 띄게 느림 | PlayerState 기본 NetUpdateFrequency | `NetUpdateFrequency = 100.f` |

---

## 결과

**확인 항목 (PIE Dedicated Server + Client 2인):**
- 서버·클라 양쪽에서 `GetAbilitySystemComponent()`가 null이 아님
- `showdebug abilitysystem` 콘솔에서 `Health = 100` 복제 확인
- Project Settings → GameplayTags에 네이티브 태그가 자동 등록됨

> **스크린샷 위치**: `showdebug abilitysystem` 화면 (Attribute 목록 + Tag 목록이 보이는 상태)
> 이 화면은 이후 모든 편의 검증 도구이므로 여기서 한 번 제대로 소개한다.

**한계 및 향후 개선:**
- Attribute 초기값을 `InitHealth(100.f)` C++ 직접 호출로 처리했다. 레벨·직업별 초기값이 필요해지면 Instant GE(`GE_InitStats`)나 DataTable 방식으로 바꿔야 한다. **지금은 HP가 고정값이라 가장 단순한 방법을 골랐다.**
- `MaxHealth` 버프 시스템이 없으므로 `PreAttributeChange`에서 비율 유지 로직(`AdjustAttributeForMaxChange`)을 넣지 않았다. RPG였다면 필요하다.

---

## 참고

- `DOCS/Notes/04/04_GAS_DOCS.md` §1~4 — 도입 배경, 아키텍처 결정, 어트리뷰트·태그 설계
- `DOCS/Notes/04/04_GAS_01_Foundation.md` — 구현 전체
- `DOCS/Notes/04/04_GAS_00_Reference.md` — GAS 개념 레퍼런스
- GASDocumentation (tranek) — ASC 배치 및 Replication Mode 논의
