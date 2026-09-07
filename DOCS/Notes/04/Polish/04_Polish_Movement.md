# Polish — 이동(CMC) 최종 설계

**상태:** 전부 적용 완료.

관련 소스: `EPCharacterMovement.h/.cpp`, `EPGA_Skill_Base.cpp`(캐스팅 GE
제거 부분).

---

## 크라우치 중 Sprint 차단

`GetMaxSpeed()`가 `!IsCrouching()`을 확인한 뒤에만 `SprintSpeed`로 덮어쓴다:

```cpp
if (bWantsToSprint && IsMovingOnGround() && !IsCrouching()) Base = SprintSpeed;
```

## 공중 크라우치 차단

```cpp
bool UEPCharacterMovement::CanCrouchInCurrentState() const
{
    return Super::CanCrouchInCurrentState() && IsMovingOnGround();
}
```

엔진 기본값(`IsFalling()`도 크라우치 허용)을 `IsMovingOnGround()`로 좁혔다.

## `MoveSpeedMultiplier` — CMC에 캐싱하지 않고 매번 ASC를 직접 읽는다

```cpp
float UEPCharacterMovement::GetMaxSpeed() const
{
    float Base = Super::GetMaxSpeed();
    if (bWantsToSprint && IsMovingOnGround() && !IsCrouching()) Base = SprintSpeed;
    else if (bWantsToAim) Base = AimSpeed;

    float Multiplier = 1.f;
    if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
        Multiplier = ASC->GetNumericAttribute(UEPAttributeSet::GetMoveSpeedMultiplierAttribute());

    return Base * Multiplier;
}
```

`FSavedMove_EPCharacter`/`SetMoveFor`/`PrepMoveFor`는 `bWantsToSprint`/`bWantsToAim`
플래그만 스냅샷한다 — 배속 값 자체는 캐싱하지 않는다.

**왜 이렇게 됐는가:** 이전엔 GAS 어트리뷰트 변경 시점에 CMC의 평범한
`float MoveSpeedMultiplier` 필드에 값을 밀어 넣는 구조였는데, SavedMove
리플레이 도중 그 캐시가 오염되면(서버 보정이 걸리기 쉬운 착지 순간 등)
다음 GAS 이벤트가 올 때까지 계속 틀린 값에 갇히는 문제가 있었다 —
`bWantsToSprint`류와 달리 이 값은 매 틱 입력 코드가 재적용해주는 소스가
없어서 한 번 오염되면 자가 치유가 안 됐다. Epic 공식 샘플 Lyra의
`LyraCharacterMovementComponent::GetMaxSpeed()`(`:120-131`, 직접 확인)도
GAS 상태를 CMC 필드로 옮겨 담지 않고 호출마다 ASC를 라이브로 읽는 패턴을
쓴다 — 같은 패턴을 그대로 채택했다. 캐싱을 아예 안 하므로 "오염된 채
고착"되는 이 클래스의 버그는 구조적으로 성립하지 않는다.

**캐스팅 GE 종료는 핸들이 아니라 태그로 지운다** (`EPGA_Skill_Base::EndAbility`):

```cpp
const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
    FGameplayTagContainer(EmpGameplayTags::TAG_State_Casting));
ASC->RemoveActiveEffects(Query);
```

캐스팅 GE도 예측→확정 GE 스왑을 겪는 Duration GE라, 핸들을 캐싱해뒀다가
그 핸들로 제거하면 스왑 이후 stale해진 핸들이 실제 활성 GE를 못 지우는
문제가 있었다(캐스트 종료 시 `MoveSpeedMultiplier`가 안 풀리고 오래 남는
증상으로 실측 확인됨). `ActivationBlockedTags`에 같은 태그가 걸려있어
"지금 이 태그를 가진 활성 GE가 있다 = 방금 끝내려는 그 캐스트다"가 구조적으로
보장되므로, 핸들 대신 태그로 "지금 활성 상태인 것 전부"를 지워도 다른
캐스트를 잘못 지울 위험이 없다.

## 참고 — 아직 손 안 댄 것

`EPGA_Skill_Base` 생성자의 `bServerRespectsRemoteAbilityCancellation = false`는
그대로 남아있다. 무기 쪽(`EPGA_Item_PrimaryUse`)에서 이 필드가 `false`일
때 "서버가 클라의 취소 신호를 무시해서 한 번 탭했는데 계속 발사되는" 버그의
원인이었던 전례가 있다(`04_Polish_WeaponFireRate.md`). 스킬 쪽도
`bInterruptibleOnDamage`로 캐스트 중 취소가 가능한 구조라 같은 문제가
잠재해 있을 수 있다 — 지금까지 증상이 보고된 적은 없어 급하지 않지만,
언젠가 `true`로 맞추는 걸 검토할 것.
