# GAS 개념 레퍼런스

> 이 프로젝트에서 사용하는 GAS 패턴 빠른 참조용.
> 프로젝트 설계/이관 계획은 `04_GAS_DOCS.md` 참조.

---

## 1. 핵심 구성 요소

| 구성 요소 | 역할 |
|-----------|------|
| **AbilitySystemComponent (ASC)** | 중앙 허브. Ability 관리, GE 적용, 태그 추적, Attribute 관리 |
| **GameplayAbility (GA)** | 하나의 능력 단위. 활성화 조건 + 실행 로직 + 종료 처리 |
| **GameplayEffect (GE)** | Attribute 변경 규칙. 데이터 전용 객체 (Blueprint로 설정, C++ 서브클래싱 불필요) |
| **AttributeSet** | Attribute(HP, Stamina 등) 컨테이너. ASC와 같은 Actor의 SubObject로 등록하면 자동 인식 |
| **GameplayTag** | 계층적 태그 시스템. 상태/조건을 문자열 없이 컴파일 타임 안전하게 표현 |
| **GameplayCue** | VFX/SFX 재생용 cosmetic. 게임 로직 없음 |

---

## 2. GameplayAbility

### 활성화 흐름

```
TryActivateAbility()
    ↓
CanActivateAbility()
    ├─ 이미 활성 중인가? (InstancingPolicy)
    ├─ ActivationRequiredTags 충족?
    ├─ ActivationBlockedTags 없음?
    ├─ CheckCost()
    └─ CheckCooldown()
    ↓ (전부 통과)
ActivateAbility()
    ├─ CommitAbility() → 비용 지불 + 쿨타임 GE 적용
    └─ 로직 실행 (동기 또는 AbilityTask 비동기)
    ↓
EndAbility() / CancelAbility()
    ← 반드시 호출. 누락 시 다음 활성화 불가
```

### InstancingPolicy

| 정책 | 동작 | 선택 기준 |
|------|------|-----------|
| NonInstanced | CDO 사용, 상태 저장 불가 | 단순 패시브, 즉발 효과 |
| **InstancedPerActor** ✓ | Actor당 1개 인스턴스, 재사용 | 대부분의 능력 (이 프로젝트 표준) |
| InstancedPerExecution | 실행마다 새 인스턴스 | 독립 상태가 필요한 투사체 다수 동시 처리 |

### CommitAbility

```cpp
void UGA_Example::ActivateAbility(...)
{
    Super::ActivateAbility(...);

    // CommitAbility = CheckCost + CheckCooldown 재확인 → 비용 지불 + 쿨타임 GE 적용
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        CancelAbility(Handle, ActorInfo, ActivationInfo, true);
        return;
    }
    // 이후 로직...
}
```

> `CanActivateAbility`와 `CommitAbility` 사이에 상태가 바뀔 수 있으므로 반드시 두 번 확인.

### AbilityTask

| 태스크 | 용도 | C++ 주의 |
|--------|------|----------|
| `WaitDelay` | 지정 시간 대기 | `ReadyForActivation()` 수동 호출 필수 |
| `PlayMontageAndWait` | 몽타주 재생 후 완료/취소 콜백 | |
| `WaitGameplayEvent` | 특정 GameplayEvent 수신 | |
| `WaitGameplayTagAdded/Removed` | 태그 변화 감지 | |
| `WaitGameplayEffectRemoved` | GE 제거 감지 | |

> **C++에서 AbilityTask 생성 시 `ReadyForActivation()` 수동 호출 필수.** Blueprint는 노드 연결 시 자동 호출.

---

## 3. GameplayEffect

### Duration 타입

| 타입 | Attribute 영향 | 주요 용도 |
|------|----------------|-----------|
| **Instant** | **BaseValue 영구 변경** | 데미지, 힐, 탄약 보충 |
| **HasDuration** | CurrentValue 임시 변경. 만료 시 자동 복구 | 쿨타임, 재장전 상태 GE |
| **Infinite** | CurrentValue 임시 변경. 수동 제거 필요 | 영구 상태 (State.Dead) |

> Instant만 BaseValue를 변경한다. Duration/Infinite는 CurrentValue 위에 레이어로 쌓이고, 제거 시 원복.

### Modifier 연산

| 연산 | 공식 | 예시 |
|------|------|------|
| Additive | BaseValue + 합계 | 데미지 Add -30, 힐 Add +50 |
| Multiplicative | Additive 결과 × (1 + 곱) | 데미지 1.5배 버프 |
| Override | = 고정값 | 탄약 MaxAmmo로 리셋 |

> 적용 순서: Additive → Multiplicative → Division → Override

### SetByCaller (런타임 값 주입)

```cpp
// GameplayTag 버전 권장 — FName 버전은 런타임 오타 위험, 에디터 자동완성 없음
FGameplayEffectContextHandle Context = InstigatorASC->MakeEffectContext();
FGameplayEffectSpecHandle Spec = InstigatorASC->MakeOutgoingSpec(GEClass, 1.f, Context);
Spec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Damage, FinalDamage);
InstigatorASC->ApplyGameplayEffectSpecToTarget(*Spec.Data, TargetASC);
```

> GE Blueprint의 Modifier에 해당 DataTag가 등록되어 있어야 한다. 미등록 시 런타임 오류.

---

## 4. AttributeSet

### PreAttributeChange vs PostGameplayEffectExecute

| 훅 | 호출 시점 | 용도 | 주의 |
|----|-----------|------|------|
| `PreAttributeChange` | Attribute 값 변경 직전 | **클램핑만** | GE/직접 setter 양쪽에서 호출됨. Gameplay 로직(GE 적용, 이벤트 발송) 금지 |
| `PostGameplayEffectExecute` | Instant/Periodic GE BaseValue 변경 직후 | 사망 처리, 메타 Attribute 변환 | GE 경유 변경에만 호출됨. 항상 소유자에게만 호출 |

### FGameplayAttributeData

```cpp
struct FGameplayAttributeData
{
    float BaseValue;     // Instant GE가 영구 변경하는 값
    float CurrentValue;  // BaseValue + Duration/Infinite modifier 합산 결과
};
// GetHealth() → CurrentValue 반환
// SetHealth(v) → BaseValue 설정
```

### 메타 Attribute 패턴 (IncomingDamage)

GE가 직접 Health를 줄이지 않고 IncomingDamage에 Add → PostGEExecute에서 처리:

```cpp
void UEPAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        const float Damage = GetIncomingDamage();
        SetIncomingDamage(0.f);  // 즉시 초기화 — 누적 방지

        if (Damage > 0.f)
        {
            // WasAlive: 연속 피격 중 중복 사망 이벤트 방지 (GASDoc 패턴)
            const bool bWasAlive = GetHealth() > 0.f;
            SetHealth(FMath::Max(GetHealth() - Damage, 0.f));

            if (bWasAlive && GetHealth() <= 0.f)
            {
                UAbilitySystemComponent* TargetASC = GetOwningAbilitySystemComponent();
                // State.Dead 체크: GA_Death 중복 활성화 방지
                if (TargetASC && !TargetASC->HasMatchingGameplayTag(EmpGameplayTags::TAG_State_Dead))
                {
                    FGameplayEventData Payload;
                    Payload.Instigator = /* SourceActor */;
                    // SendGameplayEventToActor Blueprint wrapper 아닌 직접 호출
                    TargetASC->HandleGameplayEvent(EmpGameplayTags::TAG_Event_Death, &Payload);
                }
            }
        }
    }
}
```

> `PostGameplayEffectExecute`는 항상 AttributeSet 소유자에게만 호출된다. `GetOwningActor()` = Target.

---

## 5. GameplayTag

### 어빌리티 태그 프로퍼티

| 프로퍼티 | 용도 | 복제 |
|----------|------|------|
| `AbilityTags` | GA 자신 식별 | - |
| `ActivationOwnedTags` | 활성 중 ASC에 추가 (종료 시 자동 제거) | **✗ 시뮬레이티드 프록시에서 보이지 않음** |
| `ActivationRequiredTags` | 이 태그 있어야 활성화 가능 | - |
| `ActivationBlockedTags` | 이 태그 있으면 활성화 불가 | - |
| `CancelAbilitiesWithTag` | 활성화 시 이 태그 가진 다른 GA 취소 | - |

> **`ActivationOwnedTags`는 복제되지 않는다.** 다른 클라이언트에서 상태를 쿼리해야 한다면 Duration/Infinite GE의 `GrantedTags`로 부여해야 한다.

### 쿨타임 SetByCaller 패턴

`CooldownGameplayEffectClass`만 설정하면 GE의 Duration이 고정값으로만 설정된다. 런타임 값(무기 FireRate 등)을 주입하려면 `GetCooldownTags()` + `ApplyCooldown()` 오버라이드가 필요하다.

```cpp
// Step A: 현재 쿨타임 태그 반환 (CDO 안전 처리)
const FGameplayTagContainer* UGA_Item_PrimaryUse::GetCooldownTags() const
{
    FGameplayTagContainer* MutableTags = const_cast<FGameplayTagContainer*>(&TempCooldownTags);
    MutableTags->Reset();  // CDO 재사용 시 오염 방지
    if (const FGameplayTagContainer* ParentTags = Super::GetCooldownTags())
        MutableTags->AppendTags(*ParentTags);
    MutableTags->AppendTags(CooldownTags);
    return MutableTags;
}

// Step B: SetByCaller로 Duration 주입
void UGA_Item_PrimaryUse::ApplyCooldown(...) const
{
    UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
    if (!CooldownGE) return;

    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());
    Spec.Data->DynamicGrantedTags.AppendTags(CooldownTags);  // GA의 CooldownTags와 GE GrantedTags 동일해야 함
    Spec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, Duration);
    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}
```

---

## 6. GAS 네트워크

### Net Execution Policy

| 정책 | 동작 | 사용처 |
|------|------|--------|
| **LocalPredicted** | 클라이언트 즉시 실행 + 서버 확인/거부 | 발사, 재장전 등 플레이어 GA |
| ServerOnly | 서버에서만 실행, 클라에 복제 안 됨 | GA_Death |
| ServerInitiated | 서버 먼저 실행, 클라에 복제 | AI 능력, 서버 이벤트 트리거 |
| LocalOnly | 소유 클라이언트에서만 | 순수 cosmetic |

> 시뮬레이티드 프록시에서는 GA가 실행되지 않는다. 다른 플레이어에게 보여야 하는 코스메틱(총구 화염 등)은 Multicast RPC 또는 GameplayCue로 처리.

### LocalPredicted 예측 흐름

```
클라이언트                        서버
  │                               │
  ├─ TryActivateAbility ─────────>│
  │  (PredictionKey 생성)          │
  │  (로컬 즉시 실행)               │
  │                               ├─ CanActivateAbility?
  │                               │  YES → 서버 실행
  │<─── Confirm PredictionKey ────│  → 클라이언트 예측 유지
  │                               │
  │                               │  NO → Reject
  │<─── Reject PredictionKey ─────│  → 클라이언트 롤백
  │  (GA 종료, GE 제거,             │
  │   Attribute 복원)              │
```
