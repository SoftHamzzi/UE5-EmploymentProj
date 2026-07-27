# Post 4-7 작성 가이드 — 스킬 3종과 베이스 클래스: 채널링, 상호 잠금, 예측 레이스

> **예상 제목**: `[UE5] 추출 슈터 4-7. 스킬 시스템: 채널링 취소, 스킬 상호 잠금, 그리고 예측 레이스 버그`
> **참고 문서**: `DOCS/Notes/04/04_GAS_07_Skills.md`, `04_GAS_07_Skills_STATUS.md`

> **이 편이 시리즈의 정점이다.** GAS 예측 모델을 이해해야만 나오는 버그 하나를 추적하는 과정이 절반을 차지한다.
> 분량이 크면 "① 스킬 3종 + 베이스 클래스" / "② 예측 레이스 버그" 두 편으로 쪼개도 된다.

---

## 개요

**이 포스팅에서 다루는 것:**
- 지속 형태가 전부 다른 스킬 3종(즉발 / 채널링 / 지속 버프)을 하나의 베이스로 수렴시키기
- 채널링 중 피격 취소를 이벤트로 처리하기
- **공용 태그 하나로 스킬 상호 잠금을 N² 없이 구현하기**
- **★ `bServerRespectsRemoteAbilityCancellation` 레이스 버그 추적**
- 클라 전용 API를 서버에서 호출해 생긴 대시 버그

**왜 이렇게 구현했는가 (설계 의도):**
- 4-3, 4-4에서 어빌리티를 두 개 만들었다. 세 개를 더 만들면서 **공통 골격이 보이기 시작했다**
- 스킬은 나중에 계속 늘어난다. **스킬을 추가할 때 기존 스킬 파일을 열지 않아도 되는 구조**가 목표

---

## 스킬 스펙 (GAME.md 기준)

| 스킬 | 효과 | 쿨타임 | 지속 형태 |
|------|------|--------|-----------|
| Dash | 이동 방향으로 즉시 대시 | 10초 | **즉발** — 지속 상태 없음 |
| Heal | 3초 채널링 → HP 30 회복, 채널링 중 이동속도 20%, 피격 시 취소 | 20초 (성공 시만) | **채널링** — 효과 발동 *전* 유지 |
| Shield | 5초간 피해 50% 감소 | 30초 | **지속 버프** — 효과 발동 *후* 유지 |

**세 개가 전부 다른 형태라는 게 좋은 예제다.** 하나만 만들었으면 안 보였을 공통점이 셋을 만들면서 드러난다.

---

## 구현 전 상태 (Before)

Dash만 구현돼 있었고, 나머지는 각자 `ActivateAbility`를 직접 구현할 예정이었다.

**최초 설계의 문제 — 스킬 상호 잠금을 하드코딩하려 했다:**

```cpp
// 최초 설계 (폐기)
// GA_Skill_Dash 생성자
ActivationBlockedTags.AddTag(TAG_State_Healing);   // 힐 중에는 대시 금지

// GA_Skill_ShieldOn 생성자
ActivationBlockedTags.AddTag(TAG_State_Healing);   // 힐 중에는 실드 금지
```

**N² 문제:** 스킬을 하나 추가할 때마다 **기존 스킬 파일을 전부 열어서** 새 태그를 추가해야 한다. 스킬 5개면 각 파일이 4개 태그를 알아야 한다.

또한 "시전 시간"과 "피격 시 취소 여부"가 스킬마다 제각각 흩어진다.

> **스태미나 폐기**도 이 단계에서 함께 정리했다. 달리기·점프에 자원 제한을 두지 않기로 하면서 `Stamina`/`MaxStamina` Attribute, `GE_SprintDrain`, `GE_StaminaRegen`, `State.Dashing` 태그가 전부 죽은 코드가 됐다. **아무도 부여하지 않는 태그가 코드에 남아 있는 상태**였다는 걸 정직하게 적는다.

---

## 구현 내용

### 1. ★ 공용 잠금 태그 하나 — `State.Casting`

N² 문제의 해법은 간단하다. **스킬끼리 서로를 알게 하지 말고, 전부 같은 태그 하나를 보게 한다.**

```
[Before — N²]                    [After — 공용 태그]
Dash  ← blocks ← Healing         Dash    ─┐
Dash  ← blocks ← Shielding       Heal    ─┼→ blocked by State.Casting
Heal  ← blocks ← Dashing         Shield  ─┘
Heal  ← blocks ← Shielding            ↑
Shield← blocks ← ...            시전 중인 스킬이 자기 GE로 이 태그를 건다
(스킬 N개면 N(N-1)개 관계)      (스킬이 몇 개든 관계 1개)
```

```cpp
// UEPGA_Skill_Base 생성자 — 이 두 줄이 모든 스킬에 상속된다
ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Casting);
ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
```

**새 스킬을 추가할 때 기존 파일을 한 글자도 안 고쳐도 된다.** 베이스를 상속하기만 하면 자동으로 잠금 체계에 편입된다.

부수 효과 하나 — **자기 자신 재시전도 자동으로 막힌다.** 힐 시전 중에 힐을 또 누르면, 자기가 건 `State.Casting`이 자기를 막는다. 별도 코드가 필요 없다.

### 2. ★ `UEPGA_Skill_Base` — 템플릿 메서드 패턴

모든 스킬이 공유하는 것은 셋뿐이다: **시전 시간**, **피격 취소 여부**, **시전 중 부여할 GE**.

```cpp
UCLASS(Abstract)
class EMPLOYMENTPROJ_API UEPGA_Skill_Base : public UGameplayAbility
{
public:
    // ★ final — 서브클래스가 공용 시전 흐름을 깨뜨리지 못하게 막는다
    virtual void ActivateAbility(...) override final;
    virtual void EndAbility(...) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Cast")
    float CastTime = 0.f;                     // 0이면 즉시시전

    UPROPERTY(EditDefaultsOnly, Category = "Cast")
    bool bInterruptibleOnDamage = false;      // Heal만 true

    UPROPERTY(EditDefaultsOnly, Category = "Cast")
    TSubclassOf<UGameplayEffect> GE_CastingClass;

    // ── 서브클래스가 채우는 훅 ──
    virtual void OnCastComplete() PURE_VIRTUAL(UEPGA_Skill_Base::OnCastComplete, );
    virtual void OnCastInterrupted() {}
    virtual void ConfigureCastingSpec(FGameplayEffectSpecHandle& SpecHandle) {}

private:
    UFUNCTION() void OnCastTimerComplete();
    UFUNCTION() void OnDamageDuringCast(FGameplayEventData Payload);
    FActiveGameplayEffectHandle CastingEffectHandle;
};
```

**`override final`이 설계 의도를 코드로 못 박는다.** 서브클래스가 `ActivateAbility`를 다시 구현하면 상호잠금과 취소 처리가 통째로 빠진다. 컴파일러가 막아준다.

```cpp
void UEPGA_Skill_Base::ActivateAbility(...)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 즉시시전 — 캐스팅 GE/태그를 아예 적용하지 않는다 (Dash, Shield)
    if (CastTime <= 0.f)
    {
        OnCastComplete();
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // 채널링 — 시전 GE 부여 (여기에 State.Casting이 들어 있다)
    if (GE_CastingClass)
    {
        FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(GE_CastingClass);
        Spec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Duration, CastTime);
        ConfigureCastingSpec(Spec);          // 서브클래스가 추가 SetByCaller를 넣는 자리
        CastingEffectHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
    }

    UAbilityTask_WaitDelay* WaitDelay = UAbilityTask_WaitDelay::WaitDelay(this, CastTime);
    WaitDelay->OnFinish.AddDynamic(this, &UEPGA_Skill_Base::OnCastTimerComplete);
    WaitDelay->ReadyForActivation();

    // 피격 취소가 필요한 스킬만 이벤트 태스크를 추가로 건다
    if (bInterruptibleOnDamage)
    {
        UAbilityTask_WaitGameplayEvent* WaitDamage =
            UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
                this, EmpGameplayTags::TAG_Event_Damaged, nullptr, false, true);
        WaitDamage->EventReceived.AddDynamic(this, &UEPGA_Skill_Base::OnDamageDuringCast);
        WaitDamage->ReadyForActivation();
    }
}
```

**두 태스크가 경쟁하는 구조다.** 먼저 발화하는 쪽이 이긴다:
```
WaitDelay(3초) ────────────────→ OnCastComplete()  → EndAbility(bWasCancelled=false)
WaitGameplayEvent(Event.Damaged) → OnCastInterrupted() → EndAbility(bWasCancelled=true)
```

```cpp
void UEPGA_Skill_Base::OnDamageDuringCast(FGameplayEventData Payload)
{
    // OnCastComplete()를 일부러 호출하지 않는다 — "완료되지 않은 채 끝남"이 핵심
    OnCastInterrupted();
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
```

**쿨타임이 `OnCastComplete`에만 들어 있으므로, 피격 취소 시 쿨타임이 안 걸린다.** 별도 분기 없이 구조로 해결된다.

### 3. 서브클래스는 얼마나 짧아지는가

```cpp
// EPGA_Skill_Heal.cpp — 생성자가 스킬 정의의 전부다
UEPGA_Skill_Heal::UEPGA_Skill_Heal()
{
    CastTime = 3.f;
    bInterruptibleOnDamage = true;

    FGameplayTagContainer Tags = GetAssetTags();
    Tags.AddTag(EmpGameplayTags::TAG_Ability_Skill_Heal);
    SetAssetTags(Tags);

    ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_Cooldown_Skill_Heal);
    // State.Dead / State.Casting은 베이스가 이미 추가 — 중복 금지
}

void UEPGA_Skill_Heal::ConfigureCastingSpec(FGameplayEffectSpecHandle& SpecHandle)
{
    SpecHandle.Data->SetSetByCallerMagnitude(
        EmpGameplayTags::TAG_Data_MoveSpeedMultiplier, HealMoveSpeedMultiplier);
}

void UEPGA_Skill_Heal::OnCastComplete()
{
    if (GE_HealClass)
    {
        FGameplayEffectSpecHandle HealSpec = MakeOutgoingGameplayEffectSpec(GE_HealClass);
        HealSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_HealAmount, HealAmount);
        ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, HealSpec);
    }
    if (GE_HealCooldownClass)
    {
        FGameplayEffectSpecHandle CDSpec = MakeOutgoingGameplayEffectSpec(GE_HealCooldownClass);
        CDSpec.Data->SetSetByCallerMagnitude(EmpGameplayTags::TAG_Data_Cooldown, HealCooldown);
        ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, CDSpec);
    }
}
```

**"피격 시 취소"를 구현하는 코드가 Heal에 한 줄도 없다.** `bInterruptibleOnDamage = true` 뿐이다.
Dash와 Shield는 `CastTime = 0`이라 `OnCastComplete()`만 채우면 끝난다.

### 4. 채널링 취소 이벤트는 어디서 오는가

4-2에서 만든 `PostGameplayEffectExecute`가 데미지를 처리할 때 이벤트를 하나 더 발송한다.

```
[피격]
 → GE_Damage → IncomingDamage
 → PostGameplayEffectExecute
     ├ Health 감소
     ├ HandleGameplayEvent(Event.Death)   ← 4-2
     └ HandleGameplayEvent(Event.Damaged) ← 4-7에서 추가
                    ↓
         WaitGameplayEvent 태스크가 수신
                    ↓
         OnCastInterrupted() → 힐 취소
```

**데미지 코드는 "누가 이 이벤트를 듣는지" 모른다.** 4-2에서 만든 이벤트 구조가 확장 지점으로 값을 하는 순간이다.

### 5. 이동속도 감소 — 하드코딩 대신 어트리뷰트

힐 중 이동속도 20%를 `Heal`에 직접 넣지 않고 GAS 표준 방식으로 만든다.

```cpp
// EPAttributeSet — 새 어트리뷰트
UPROPERTY(BlueprintReadOnly, Category = "Attribute|Movement", ReplicatedUsing = OnRep_MoveSpeedMultiplier)
FGameplayAttributeData MoveSpeedMultiplier;
ATTRIBUTE_ACCESSORS(UEPAttributeSet, MoveSpeedMultiplier);

// PreAttributeChange — 0 이하로 떨어져 정지/역주행하는 사고 방지
if (Attribute == GetMoveSpeedMultiplierAttribute())
    NewValue = FMath::Clamp(NewValue, 0.05f, 3.f);
```

```cpp
// EPCharacterMovement.cpp — ★ 곱하는 위치가 중요하다
float UEPCharacterMovement::GetMaxSpeed() const
{
    float Base = Super::GetMaxSpeed();
    if (bWantsToSprint && IsMovingOnGround()) Base = SprintSpeed;
    else if (bWantsToAim)                     Base = AimSpeed;
    return Base * MoveSpeedMultiplier;   // ← Sprint/Aim 분기로 Base를 정한 "뒤에" 마지막에 곱한다
}
```

**분기 이전에 곱하면 힐 중 스프린트가 감속을 무시한다.** `SprintSpeed`가 Base를 덮어써버리기 때문. 이건 실제로 겪은 실수라 함정 표에도 넣는다.

```cpp
// EPCharacter::InitASC — IsLocallyControlled() 가드 "밖"에 둔다
// 서버 권위 시뮬레이션과 소유 클라 예측 둘 다 GetMaxSpeed()를 실행하므로 양쪽 다 필요
if (MoveSpeedMultiplierHandle.IsValid())
    ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMoveSpeedMultiplierAttribute())
       .Remove(MoveSpeedMultiplierHandle);

MoveSpeedMultiplierHandle =
    ASC->GetGameplayAttributeValueChangeDelegate(UEPAttributeSet::GetMoveSpeedMultiplierAttribute())
       .AddUObject(this, &AEPCharacter::OnMoveSpeedMultiplierChanged);

// 리스폰 등으로 이미 1.0이 아닐 수 있으므로 초기값 즉시 반영
if (UEPCharacterMovement* CMC = Cast<UEPCharacterMovement>(GetCharacterMovement()))
    CMC->MoveSpeedMultiplier = ASC->GetNumericAttribute(UEPAttributeSet::GetMoveSpeedMultiplierAttribute());
```

**CMC의 `MoveSpeedMultiplier`는 별도로 복제하지 않는다.** 서버와 소유 클라가 **각자 자기 ASC의 복제된 어트리뷰트로부터** 채우므로, Sprint/Aim처럼 CompressedFlags에 실을 필요가 없다.

`GE_Healing` 에셋에 Modifier 한 줄만 추가하면 된다:

| 항목 | 값 |
|------|-----|
| Duration Policy | Has Duration, SetByCaller(`Data.Duration`) |
| GrantedTags | `State.Healing` **+ `State.Casting`** ← 이 한 줄이 곧 "다른 스킬 잠금" 스위치 |
| Modifier | `MoveSpeedMultiplier`, **Multiply**, SetByCaller(`Data.MoveSpeedMultiplier`) |

**Modifier Op이 Multiply여야 한다.** Add로 하면 감속 효과가 두 개 겹칠 때 계산이 무너진다. Multiply면 자동으로 곱연산 누적된다 (0.2 × 0.5 = 0.1).

> **확장성은 "GE를 공유하는 것"이 아니라 "어트리뷰트 + Multiply 연산"에 있다.** 나중에 다른 스킬이 자기 GE에 같은 Modifier 한 줄만 추가하면 그대로 재사용된다. 새 어트리뷰트도, CMC 코드 수정도 필요 없다.

---

## 겪은 문제

이 편의 절반. 셋 다 **"클라와 서버가 다른 것을 본다"**는 한 뿌리에서 나왔다.

### ★ ① 시전을 다 채웠는데 힐이 자주 무시된다

**증상이 이상했다:**
- 시전 게이지는 끝까지 찬다
- `State.Casting` / `State.Healing` 태그는 정상 동작한다 (다른 스킬이 제대로 잠긴다)
- 그런데 **HP가 안 오른다**
- 그리고 **쿨다운도 안 돈다** — 바로 다시 쓸 수 있다
- 확률적이다. 될 때도 있고 안 될 때도 있다

**"쿨다운도 같이 안 도는 것"이 결정적 단서였다.** HP만 안 올랐다면 `GE_Heal` 문제겠지만, 쿨다운까지 같이 빠졌다는 건 **`OnCastComplete()` 전체가 서버에서 실행되지 않았다**는 뜻이다.

**원인 — LocalPredicted에서 클라와 서버가 각자 타이머를 돌린다:**

```
편도 지연 L, 시전 시간 3초라고 하면

t=0        클라: 활성화 → WaitDelay(3) 시작
t=L        서버: 활성화 RPC 도착 → WaitDelay(3) 시작    ← L만큼 늦게 시작
t=3        클라: WaitDelay 완료 → EndAbility → ServerEndAbility RPC 발신
t=3+L      서버: RPC 도착
t=L+3      서버: 자기 WaitDelay 완료

          ★ 3+L 과 L+3 은 같은 시각이다
```

**클라 타이머 완료 시점과 서버 타이머 완료 시점 사이의 지연이 정확히 상쇄된다.** 그래서 두 이벤트가 같은 프레임에 도착하고, **프레임 내 처리 순서**로 승패가 갈린다.

```
[한 프레임 내부]
TickDispatch (프레임 초반) — 네트워크 RPC 처리   ← ServerEndAbility가 여기
     ↓
World Tick                 — 타이머/태스크 발화  ← 서버 WaitDelay가 여기
```

RPC가 먼저 처리되므로 **높은 확률로** 서버 GA가 강제 종료되고, 그 뒤에 발화하려던 서버의 `WaitDelay`는 이미 죽은 태스크가 된다. → `OnCastComplete()` 스킵 → 힐·쿨다운 GE 전부 미적용.

"높은 확률로"이지 항상은 아니라서 **확률적으로 보였던 것**이다.

**해결 — 한 줄:**

```cpp
// UEPGA_Skill_Base 생성자
bServerRespectsRemoteAbilityCancellation = false;
```

기본값 `true`는 "클라가 끝났다고 하면 서버도 끝낸다"는 뜻이다. **예측 어빌리티에서는 이게 거의 항상 틀린 선택이다** — 서버는 자기 타이머로 자기 일을 끝내야 한다.

> 서버→클라 종료 복제는 이 플래그와 무관하게 항상 동작하므로, **피격 인터럽트 경로는 영향받지 않는다.** 서버가 취소하면 클라도 취소된다.

**교훈 (포스팅의 결론):**
- LocalPredicted GA에서 **클라 인스턴스와 서버 인스턴스는 별개의 객체**다. 각자 타이머를 돌리고 각자 종료한다
- 클라가 서버보다 먼저 끝나는 건 **정상이다.** 문제는 그 종료가 서버를 죽이도록 허용한 것
- **증상이 확률적이면 레이스를 의심한다.** 그리고 "무엇이 같이 실패했는가"가 범위를 좁혀준다

> 참고로 4-2의 `GA_Death`와 4-3의 `GA_Item_PrimaryUse`에도 같은 플래그를 이미 넣어뒀다. **이 버그를 겪고 나서 소급 적용한 것**이 아니라, GASDocumentation 권장을 따랐던 게 여기서 이유를 알게 된 경우다.

### ② `RemoveActiveGameplayEffect called without Authority` 경고

힐을 쓸 때마다 클라 로그에 경고가 쌓였다.

- **원인**: `EndAbility`는 **클라 예측 인스턴스에서도 실행된다.** 거기서 GE를 지우려 하는데, GE 제거는 서버 권한에서만 허용된다
- **수정**:

```cpp
void UEPGA_Skill_Base::EndAbility(..., bool bReplicateEndAbility, bool bWasCancelled)
{
    if (ActorInfo->IsNetAuthority())    // ★ 가드
    {
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
            if (CastingEffectHandle.IsValid())
            {
                ASC->RemoveActiveGameplayEffect(CastingEffectHandle);
                CastingEffectHandle.Invalidate();
            }
    }
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
```

> **4-4의 `GA_Item_Reload`에도 같은 패턴이 authority 가드 없이 남아 있다.** 같은 잠재 버그이므로 정직하게 언급한다.

### ③ 대시가 서버에서만 전방으로 안 간다 (러버밴딩)

클라에서는 잘 가는데 서버 시뮬레이션은 제자리라 위치가 튀었다.

- **원인**: `GetLastMovementInputVector()`는 **로컬 전용**이다. 서버에서는 Zero를 반환한다
- **수정**: `CMC->GetCurrentAcceleration()` — CMC의 saved move로 **복제되는** 값

```cpp
// Before: const FVector Dir = Char->GetLastMovementInputVector();   // 서버에서 Zero
// After : const FVector Dir = CMC->GetCurrentAcceleration();        // saved move로 복제됨
```

**교훈**: 예측 어빌리티에서 입력 정보를 쓸 때는 **"이 값이 서버에도 있는가"**를 먼저 확인해야 한다. CMC의 saved move 경로를 타는 값만 안전하다.

### ④ 지상 대시가 거의 안 나간다

수평으로 발사하면 즉시 착지 → `GroundFriction`/`BrakingDeceleration`이 바로 감속시킨다.

- **수정**: Z 부스트(`DashZBoost`)로 잠깐 체공시키고 `bZOverride = true`

작지만 **"물리적으로 맞는 구현"과 "게임에서 원하는 느낌"이 다른** 전형적인 사례라 짧게 언급할 가치가 있다.

---

## 함정 정리

| 상황 | 원인 | 해결 |
|------|------|------|
| **시전 완료했는데 효과가 자주 무시됨** | 클라 `EndAbility` RPC가 서버 타이머를 이기는 레이스 | `bServerRespectsRemoteAbilityCancellation = false` |
| `RemoveActiveGameplayEffect without Authority` | `EndAbility`가 클라 인스턴스에서도 실행 | `ActorInfo->IsNetAuthority()` 가드 |
| 대시가 서버에서 안 감 | `GetLastMovementInputVector`는 로컬 전용 | `CMC->GetCurrentAcceleration()` |
| 지상 대시가 즉시 감속 | 착지 후 마찰 | Z 부스트 + `bZOverride` |
| `TryActivateAbilitiesByTag`가 GA를 못 찾음 | 생성자에서 `SetAssetTags` 누락 | 각 GA에서 자기 태그 설정 |
| 힐 중 스프린트해도 안 느려짐 | Sprint/Aim 분기 **이전**에 배율을 곱함 | Base를 정한 **뒤에** 마지막으로 곱한다 |
| 감속 GE가 겹칠 때 계산이 이상함 | Modifier Op이 Add | **Multiply**로 설정 |
| 힐 중 힐을 또 눌러도 안 막힘 | `GE_Healing`의 GrantedTags에 `State.Casting` 누락 | 이 태그가 실제로 부여돼야 자기 잠금이 동작 |
| 새 스킬을 추가했는데 잠금이 하나도 안 걸림 | `UGameplayAbility`를 직접 상속 | 반드시 `UEPGA_Skill_Base` 상속 |
| 피격 취소인데 완료 처리가 됨 | `OnDamageDuringCast`에서 `OnCastComplete()` 호출 | 베이스는 `OnCastInterrupted()`만 부른다 |
| 힐로 HP가 100을 넘음 | `PreAttributeChange`의 Health 클램프를 실수로 제거 | 4-2의 클램프 유지 |

---

## 결과

**확인 항목 (PIE 2인):**
- Dash/Heal/Shield 각각 활성화 → 쿨타임 GE가 **양쪽 클라에서** 확인됨
- 힐 시전 중 Dash/Shield 입력 → 전부 차단
- 힐 시전 중 힐 재입력 → 차단 (자기 잠금)
- 힐 시전 중 피격 → 취소되고 **쿨타임이 걸리지 않음**
- 힐 시전 중 이동 → 20% 속도, 스프린트를 눌러도 감속 유지
- 힐 시전 완료 → **10번 중 10번 HP 30 회복 + 쿨다운 시작** ← 레이스 버그 수정 검증
- Shield 발동 → 5초간 피해 50% 감소, `State.Shielded` 복제
- 높은 핑(100ms 에뮬)에서 위 전부 재확인 ← **레이스 버그는 핑이 있어야 재현된다**

**한계 및 향후 개선 (알면서 남긴 것):**
- **`EPGA_Skill_Base.cpp:85`** — 피격 중단 경로인데 `bWasCancelled = false`로 넘긴다. 현재 동작에는 영향이 없지만 의미상 틀렸다
- **`EPGA_Skill_ShieldOn.h:31`** — `ShieldCooldown = 50.f`인데 GAME.md 스펙은 30초. 밸런스 테스트 중 바꿔놓고 되돌리지 않았다
- 시전 애니메이션 없음 (4-4의 재장전과 동일한 한계)
- `GA_Death`가 진행 중인 스킬을 취소하지 않는다 — 4-4에서 겪은 것과 같은 문제

**향후 확장 — 스킬 슬롯 (5단계로 이어지는 고리):**

현재는 키↔스킬이 하드코딩이다(`Input_Dash` → `Ability.Skill.Dash`). Lyra 방식 동적 슬롯 태그로 바꾸면 로드아웃에서 스킬을 배정할 수 있다:

```cpp
// 부여 시점에 슬롯 태그를 스펙에 동적으로 추가
FGameplayAbilitySpec Spec(AbilityClass, 1);
Spec.GetDynamicSpecSourceTags().AddTag(TAG_InputTag_Skill_Slot1);
ASC->GiveAbility(Spec);

// 입력은 슬롯만 안다 — 어떤 스킬인지 모른다
void Input_Skill1()
{
    ASC->TryActivateAbilitiesByTag(FGameplayTagContainer(TAG_InputTag_Skill_Slot1));
}
```

`TryActivateAbilitiesByTag`는 AssetTags뿐 아니라 스펙의 `DynamicSpecSourceTags`도 검색하므로 **GA 클래스는 자기 슬롯을 몰라도 된다.**

```
[유저 키 설정]     Q ↔ T                    ← EnhancedInputUserSettings (리바인딩은 별개 계층)
[InputAction]      IA_Skill1                 ← 고정
[슬롯 태그]        InputTag.Skill.Slot1      ← 고정
[로드아웃 데이터]  Slot1 = Dash              ← 캐릭터 세팅에서 변경 (서버 권위)
[GA]               GA_Skill_Dash
```

**변경 지점은 딱 2곳이다** — `PossessedBy`의 부여 루프와 입력 핸들러 3개를 슬롯 3개로 통합. GA 클래스, GE 에셋, 채널링·쿨타임 로직은 전부 그대로 재사용된다.

로드아웃은 장비/인벤토리와 같은 **서버 권위 데이터 계층**이므로, 5단계(Loot/인벤토리)와 묶어 "매치 시작 전 로비 세팅"으로 설계하는 것이 자연스럽다.

---

## 참고

- `DOCS/Notes/04/04_GAS_07_Skills.md` — 구현 전체 (Step 8이 베이스 클래스 재설계)
- `DOCS/Notes/04/04_GAS_07_Skills_STATUS.md` — 실제 버그 및 남은 이슈
- 엔진 `AbilitySystemComponent.cpp:1177` — `IsOwnerActorAuthoritative()` (경고 발생 지점)
