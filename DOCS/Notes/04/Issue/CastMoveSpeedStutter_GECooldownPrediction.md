# 이슈 — 캐스팅형 스킬(E, Heal) 사용 시 CMC가 잠깐 버벅거림

**상태:** 증상 확인됨(실측). 원인은 **가설 단계, 미검증** — 로그로 아직
확인 안 했다.

---

## 증상

`Heal`(E) 사용 시 캐릭터 무브먼트(CMC)가 순간적으로 버벅거린다.

## 처음 의심했던 것 — 틱 순서 (반증됨)

`GE_Healing`(캐스팅 GE)이 타깃 액터에 `State.Casting`/`State.Healing` 태그를
붙이면서 동시에 `EPAttributeSet::MoveSpeedMultiplier`에 `SetByCaller` 모디파이어를
건다. 처음엔 "태그가 떨어지는 시점과 모디파이어가 떨어지는 시점이 서로 다른
틱에 걸려서" 버벅거리는 게 아닐까 의심했다.

**코드로 확인한 결과, 이건 성립하지 않는다.** `MoveSpeedMultiplier`는
델리게이트로 어딘가에 푸시되는 값이 아니라 `EPCharacterMovement.cpp:43-53`
`GetMaxSpeed()`가 **매번 그 자리에서 직접 읽어오는(pull)** 값이다:

```cpp
float UEPCharacterMovement::GetMaxSpeed() const {
    float Base = Super::GetMaxSpeed();
    if (bWantsToSprint && IsMovingOnGround() && !IsCrouching()) Base = SprintSpeed;
    else if (bWantsToAim) Base = AimSpeed;

    float Multiplier = 1.f;
    if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
        Multiplier = ASC->GetNumericAttribute(UEPAttributeSet::GetMoveSpeedMultiplierAttribute());

    return Base * Multiplier;
}
```

태그와 모디파이어는 같은 GE 하나에 묶여 있어서 제거될 때 항상 같이
제거되고, CMC는 이 함수가 불릴 때마다 그 순간의 값을 즉석에서 읽을 뿐이다
— "태그는 먼저, 모디파이어는 나중" 같은 프레임 내 틱 순서 어긋남이 애초에
성립할 자리가 없다.

## 유력 가설(미검증) — 예측본/확정본이 서로 다른 실제 시각에 제거됨

`GE_Healing`도 `HasDuration` Duration GE라 `SkillCooldown_GECooldownPrediction.md`와
**같은 근본 원인**(GE 적용은 예측되지만 제거는 예측 안 됨)을 물려받는다.
캐스팅 종료 시점에 두 가지 제거가 서로 다른 실제 시각에 일어날 수 있다:

1. 클라의 로컬 예측 GE — 클라 자신의 캐스팅 완료 시점(`WaitDelay`/
   `NetworkSyncPoint`)에 맞춰 제거됨
2. 서버가 확정한 진짜 GE — 서버 자신의 시각 기준으로 별도로 제거되고,
   그 결과가 리플리케이트되어 클라에 나중에 반영됨

이 둘이 서로 다른 순간에 `MoveSpeedMultiplier`를 되돌리면, `GetMaxSpeed()`가
캐스팅 종료 전후로 **한 프레임이 아니라 실제 시간차를 두고 두 번** 다른
값을 읽게 되고, `MaxWalkSpeed`가 그때마다 즉시 바뀌면서 CMC 입장에서
"버벅거림"으로 체감될 수 있다.

**검증 방법(아직 안 함):** `GetMaxSpeed()`의 `Multiplier` 읽는 줄 옆에
`UE_LOG`로 `Multiplier`, `GetServerWorldTimeSeconds()`를 찍어서 캐스팅
종료 전후로 값이 몇 번, 언제 바뀌는지 확인한다.

## 참고

- `DOCS/Notes/04/Issue/SkillCooldown_GECooldownPrediction.md` — 같은 근본 원인(GE 제거 예측 불가)의 원조 이슈
- `DOCS/Notes/04/Polish/04_Polish_SkillDisplay.md` §3 — 관찰 대상으로 남겨둔 메모
