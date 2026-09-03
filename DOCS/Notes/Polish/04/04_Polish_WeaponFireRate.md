# Polish — 무기 발사 속도 / 탄약 동기화

**상태:** 전부 적용 완료. `Entry.State.Charges` 이관만 Step 05로 명시적으로
미룬 범위 밖 항목이다.

관련 소스: `EPGA_Item_PrimaryUse.h/.cpp`, `EPCombatComponent.cpp`,
`GE_FireCooldown`(에셋).

---

## 1. 현재 구조

**쿨다운 GE** — `GE_FireCooldown`의 Duration Set by Caller 태그는
`Data.Cooldown`(코드가 `SetSetByCallerMagnitude(TAG_Data_Cooldown, ...)`로
값을 넘기는 그 자리)이다. 상태 태그(`Cooldown.Weapon.PrimaryUse`, 재발동
차단용)와 값 전달용 태그를 혼동하지 않는다 — 서로 역할이 다르다.

**발사 확정 RPC** — `EPCombatComponent::Server_ConfirmFire`는 `Unreliable`.
같은 컴포넌트의 `Multicast_PlayMuzzleEffect`/`Multicast_PlayImpactEffect`와
같은 패턴. 드문 패킷 유실 시 그 한 발의 판정이 누락될 수 있지만, 초당
수발 쏘는 완전자동에서는 체감이 거의 없다.

**완전자동 어빌리티** — 총알 한 발마다 어빌리티를 새로 활성화하지 않는다.
`Input_Fire`(`Started`)가 어빌리티를 한 번 활성화하면, `EEPFireMode::Auto`일
때 `ActivateAbility()`가 `FireOnce()`를 첫 발 쏜 뒤 내부 `FTimerHandle`로
`FireInterval`마다 반복한다. `Input_StopFire`(`Completed`/`Canceled`)가
`ASC->CancelAbilities()`로 끝낼 때까지 어빌리티 인스턴스는 계속 살아있다.
`Single`/`Burst`는 첫 발 직후 바로 `EndAbility`.

```cpp
void UEPGA_Item_PrimaryUse::FireOnce()
{
    ...
    if (!Char || !Weapon || !Weapon->CanFire())
    {
        EndAbility(...);   // 탄약 소진 시 자동 종료
        return;
    }

    CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);

    if (!CurrentActorInfo->IsLocallyControlled()) return;   // 서버가 들고 있는 "원격 클라의"
                                                             // 권위 인스턴스 틱은 여기서 끝 —
                                                             // 발사에 관여하지 않는다

    ...
    if (CurrentActorInfo->IsNetAuthority())
        ServerConfirmOneShot(Origin, Direction);            // 호스트 자신 — 왕복 없이 즉시
    else
    {
        Combat->PlayLocalMuzzleEffect(Origin);               // 원격 클라 — 코스메틱만 로컬
        Combat->Server_ConfirmFire(Origin, Direction, CurrentSpecHandle);  // 실제 확정은 RPC로
    }
}
```

`ServerConfirmOneShot()`이 유일한 발사 확정 지점이다 — `CommitAbilityCost`(탄약
소모)와 `HandleServerFire`(히트스캔·데미지)를 여기서만 부른다. 호출 경로는
둘뿐: 호스트의 `FireOnce()`가 직접, 또는 `Server_ConfirmFire_Implementation`이
원격 클라의 RPC를 받아서.

**`Server_ConfirmFire`는 발사한 어빌리티 인스턴스의 `FGameplayAbilitySpecHandle`을
같이 실어 보낸다** — 서버가 그 핸들로 정확히 그 인스턴스를 찾는다:

```cpp
// EPCombatComponent.h
UFUNCTION(Server, Unreliable)
void Server_ConfirmFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction,
    FGameplayAbilitySpecHandle AbilityHandle);
```

```cpp
// EPCombatComponent.cpp
void UEPCombatComponent::Server_ConfirmFire_Implementation(FVector_NetQuantize Origin,
    FVector_NetQuantizeNormal Direction, FGameplayAbilitySpecHandle AbilityHandle)
{
    AEPCharacter* Char = GetOwnerCharacter();
    UAbilitySystemComponent* ASC = Char ? Char->GetAbilitySystemComponent() : nullptr;
    FGameplayAbilitySpec* Spec = ASC ? ASC->FindAbilitySpecFromHandle(AbilityHandle) : nullptr;
    if (UEPGA_Item_PrimaryUse* Ability = Spec ? Cast<UEPGA_Item_PrimaryUse>(Spec->GetPrimaryInstance()) : nullptr)
        Ability->ServerConfirmOneShot(Origin, Direction);
}
```

```cpp
// EPGA_Item_PrimaryUse.cpp, FireOnce()의 원격 클라 분기
Combat->Server_ConfirmFire(Origin, Char->GetControlRotation().Vector(), CurrentSpecHandle);
```

**왜 핸들인가 — `FindAbilitySpecFromClass`는 완전 일치만 본다.** 처음엔
`ASC->FindAbilitySpecFromClass(UEPGA_Item_PrimaryUse::StaticClass())`로
찾았는데, 실제로 부여된 어빌리티는 블루프린트 서브클래스(`BP_GA_Item_PrimaryUse`)라
`Spec.Ability->GetClass() == InAbilityClass`(엔진, 완전 일치만 허용)에
걸려 항상 `nullptr`이었다 — RPC는 도착하는데 대상을 못 찾아 아무 반응도
없는 버그로 실측 확인됨. `IsLocallyControlled()` 게이트를 추가하기 전엔
서버가 원격 클라 캐릭터에 대해서도 자기 타이머로 어빌리티 인스턴스(`this`)를
직접 들고 있었어서 이 조회 자체가 필요 없었고, 그래서 이 버그가 한 번도
드러나지 않았었다. `FindAbilitySpecFromHandle`은 핸들을 그대로 비교하므로
서브클래싱과 무관하게 항상 정확한 인스턴스를 찾는다 — GAS 자체의
`ServerTryActivateAbility(FGameplayAbilitySpecHandle, ...)`도 같은 패턴이다.

**구현 노트:**
- `CommitAbility()`가 아니라 `CommitAbilityCost` + `CommitAbilityCooldown(ForceCooldown=true)`로
  나눠서 쓴다 — `CommitAbility()`의 `CheckCooldown()`은 쿨다운 지속시간이
  타이머 간격과 같아서 직전 발이 건 쿨다운에 이번 발이 스스로 걸리게 만든다.
- 생성자의 `bServerRespectsRemoteAbilityCancellation = true` — `false`면
  `Input_StopFire`가 클라 쪽 로컬 인스턴스만 멈추고 서버 권위 인스턴스는
  계속 돈다(트리거를 짧게 눌러도 탄창이 다 빌 때까지 서버가 계속 쏨).

## 2. 결과

- GAS 자체 Reliable RPC(`ServerTryActivateAbility`/`ServerEndAbility`)는
  연사(버스트) 한 번당 딱 한 쌍만 나간다 — 핑 의존성이 사라짐.
- `!IsLocallyControlled()` 가드로 서버가 원격 클라의 발사를 자기 타이머로
  이중으로 세는 경로가 없다 — RPC 수신 횟수와 실제 발사 수가 1:1이다.
- 호스트/스탠드얼론에서도 `HandleServerFire`가 정상적으로 돈다(예전엔
  `IsNetAuthority()`만으로 분기해서 호스트 자신의 발사가 히트스캔을 전혀
  안 태우던 문제가 있었다 — 지금은 로컬+권위 분기가 `ServerConfirmOneShot`을
  직접 부른다).

## 3. 범위 밖

- **`Entry.State.Charges` 이관** — 탄약을 GAS `Ammo` 어트리뷰트 대신
  인벤토리 아이템 인스턴스에 귀속시키는 것. `05_Loot_05_Equipment.md`에
  이미 설계돼 있고(`LOOT_STATUS.md`: "탄약은 `Entry.State.Charges`가
  진실, GAS `Ammo`는 뷰"), Step 03(인벤토리)·04(인벤토리 UI) 완성을
  전제로 짜여 있어 지금 당겨오지 않는다. 이관 시 `CommitAbilityCost`
  자리만 `Entry.State.Charges` 기반으로 바뀌고 `ServerConfirmOneShot`
  구조 자체는 그대로 호환된다.
- **`FireMode::Burst`** — "N발 고정 연사 후 자동 종료" 분기(발사 카운터)가
  아직 없다. 지금은 `Auto`만 타이머 루프로 처리하고 `Single`/`Burst`는
  기존 즉시-종료 경로로 묶여 있다.
