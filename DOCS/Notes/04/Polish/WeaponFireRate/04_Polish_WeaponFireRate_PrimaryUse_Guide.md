# 04 Polish 작성 가이드: `EPGA_Item_PrimaryUse.cpp` — 코드 없이, 계약만

> 대상: `Private/GAS/EPGA_Item_PrimaryUse.cpp`. 헤더는 이미 완성돼 있고(2026-09-21 확인), cpp는 함수 껍데기만 있다.
> 이 문서는 **코드를 주지 않는다.** 함수마다 "무엇을, 어떤 순서로, 어떤 엔진 API로"만 적는다. 코드는 본인이 쓴다.
> 막히면 그 함수만 묻는다. 다 쓴 뒤 리뷰를 요청한다 — 그때 `04_Polish_WeaponFireRate_Implementation.md` Step 9와 비교한다.
> 결정의 근거는 `04_Polish_WeaponFireRate_STATUS.md` §1, 설계는 `../04_Polish_WeaponFireRate.md` §4-2.

---

## 0. 시작 전 정리 (cpp 위쪽)

- [x] 헤더에 남은 `ApplyCooldown` 선언과 cpp의 정의 **삭제** — 쿨다운 GE는 안 쓴다 (STATUS §1 "쿨다운 GE")
- [x] 옛 `ServerConfirmOneShot(const FVector& Origin, const FVector& Direction)` 정의 **삭제** — 헤더에 없다
- [x] `GetFireInterval` 호출 두 곳 **삭제** — 함수가 없다. 간격은 `1 / Weapon->GetBaseFireRate()`
- [x] include: `GameFramework/GameStateBase.h`는 이제 안 쓴다. 대신 필요한 것 — `AbilitySystemComponent.h`(델리게이트·TargetData 호출), `GameFramework/CharacterMovementComponent.h`(클라 예측 데이터), `GAS/EPTargetData_Fire.h`
- [x] `EPTargetData_Fire.h`에 **`TStructOpsTypeTraits` 특수화가 빠져 있다** — `WithNetSerializer = true` 없으면 핸들 직렬화가 안 된다 (엔진 주석 "REQUIRED", `GameplayAbilityTargetTypes.h:384`). 구현서 Step 2 참고. `.cpp`는 비어 있어도 되지만 없어도 된다

생성자는 지금 그대로 맞다.

---

## 1. 작성 순서

헬퍼 → 서버 쪽 → 클라 쪽 → 수명 순으로 쓰면 각 단계에서 컴파일이 된다.

```
1. 헬퍼 5개          컴파일
2. ServerConfirmOneShot, OnTargetDataReady     컴파일
3. SendFireTargetData, FireOnce, ArmNextShot, OnFireTimerTick     컴파일
4. ActivateAbility, EndAbility, InputPressed, InputReleased, CanActivateAbility     빌드 → PIE
```

---

## 2. 함수별 계약

### 2-1. 헬퍼

**`GetCharacter()`** — `CurrentActorInfo`의 `AvatarActor`를 `AEPCharacter`로. `CurrentActorInfo`가 null일 수 있다(활성화 전) → null 반환.

**`GetWeapon()`** — 캐릭터 → `GetCombatComponent()` → `GetEquippedWeapon()`. 어느 단계든 null이면 null.

**`GetFireRateMultiplier()`** — 캐릭터의 `GetLocalModifiers().Product(TAG_Modifier_FireRate)`. 캐릭터 없으면 `1.f`.
왜 여기서 읽나: 클라 페이싱과 서버 버킷이 **같은 식**을 써야 한다 (STATUS §1 "배율 출처").

**`GetClientMoveTimeStamp()`** — 원격 클라만 의미 있다.
- `IsNetAuthority()`면 `-1.f` (호스트는 클라 예측 데이터가 없다)
- 아니면 `GetCharacterMovement()->GetPredictionData_Client_Character()->CurrentTimeStamp`
- 왜 이 값인가: 이 값과 캐릭터 위치는 둘 다 "마지막으로 실행된 무브"의 것이라 언제 읽어도 짝이 맞는다 (설계 §4-2 타임스탬프 동기 문단)

**`IsAutoFire(Weapon)`** — `Weapon->WeaponDef->FireMode == EEPFireMode::Auto`. null 안전.

### 2-2. 서버 쪽

**`ServerConfirmOneShot(Direction, ClientMoveTimeStamp)`** — 유일한 발사 확정 지점. 서버에선 `ServerSetReplicatedTargetData_Implementation`이 클라 키로 연 윈도우 **안에서** 불리므로, 여기의 `CommitAbilityCost`가 자동으로 그 키를 쓴다 — 코드에 키가 안 보여도 맞다. 거절 경로(버킷·탄약)는 차감 없이 끝나지만 윈도우 소멸자가 키를 ack하므로 클라의 예측 −1이 저절로 돌아온다. 반환값의 뜻을 먼저 정하라: **`false` = 탄약 소진(어빌리티 종료), `true` = 그 외 전부** (버킷 거절 포함 — 한 발 버리는 것이지 사격 종료가 아니다).
순서:
1. 캐릭터·무기·컴뱃 컴포넌트 확보. 없으면 `false`
2. 버킷: `Weapon->GetFireLimiter().TryTake(지금, 유효 발사 속도)`. 유효 발사 속도 = `GetBaseFireRate() × GetFireRateMultiplier()`. 거절이면 **`true`** 반환
3. `CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo)` 실패면 `false`
4. `Combat->HandleServerFire(Direction, ClientMoveTimeStamp)` → `true`
왜 버킷이 탄약보다 먼저인가: 거절된 발은 탄약을 깎으면 안 된다.

**`OnTargetDataReady(Data, ApplicationTag)`** — 서버에서 클라 TargetData가 도착할 때마다.
1. `ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, 활성화 예측 키)` **먼저** — 안 하면 다음 발이 "overriding pending replicated target data" 로그
2. `Data.IsValid(0)`이면 `Data.Get(0)`을 `const FEPTargetData_Fire*`로 캐스팅. 비어 있을 수 있다 — 배치 RPC는 TargetData 없이도 델리게이트를 부른다
3. `ServerConfirmOneShot(방향, 타임스탬프)`가 `false`면 `EndAbility(..., bReplicateEndAbility=true, bWasCancelled=false)` — 서버가 끝내면 `ClientEndAbility`로 클라도 따라 끝난다
활성화 예측 키: `CurrentActivationInfo.GetActivationPredictionKey()`.

### 2-3. 클라 쪽 (오너·호스트)

**`SendFireTargetData(Direction, ClientMoveTimeStamp)`** — 원격 클라 전용.
1. `new FEPTargetData_Fire()` 로 만들고 필드 둘 채운다. `FGameplayAbilityTargetDataHandle`에 넘기면 핸들이 소유권을 가진다(`TSharedPtr`) — 직접 delete 금지
2. `ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, 활성화 예측 키, 핸들, FGameplayTag(), ASC->ScopedPredictionKey)`
마지막 인자가 **이 발의 예측 키**다 — `FireOnce`가 연 윈도우의 키. 서버가 같은 키로 윈도우를 열어 탄약을 차감하고 ack하면 클라의 예측 GE가 제거된다. 반드시 `FireOnce`의 윈도우 **안에서** 불려야 한다(2026-09-22 탄약 예측).

**`FireOnce()`** — 로컬 컨트롤에서만 불린다는 전제.
1. 캐릭터·무기·컴뱃 확보 + `Weapon->CanFire()`. 하나라도 실패면 `EndAbility(..., true, true)` — 탄약 소진은 서버 인스턴스도 끝내야 하니 **복제한다**
2. `Weapon->GetFireTimer().Start(지금, 1 / GetBaseFireRate())`
3. 방향 = `Char->GetControlRotation().Vector()`, 타임스탬프 = `GetClientMoveTimeStamp()`
4. `IsNetAuthority()`(호스트)면 `ServerConfirmOneShot` 직접. `false`면 `EndAbility(..., true, true)`. 여기서 return
5. 원격 클라 — **탄약 예측 (2026-09-22):** `FScopedPredictionWindow`를 연다. 단, 새 키를 만들지는 **조건부**로 — `bCanGenerateNewKey = !ASC->ScopedPredictionKey.IsValidForMorePrediction()`. 첫 발은 `ActivateAbility` 안이라 활성화 키가 이미 유효하고, 그 키를 그대로 써야 배치 RPC(활성화 키만 싣는다)와 맞는다. 타이머 발은 윈도우 밖이라 새 키가 생긴다
6. 윈도우 안에서 `CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo)` — 실패면 `EndAbility(..., true, true)` return. 성공하면 `GE_ConsumeAmmo`가 예측 적용돼 HUD가 즉시 −1
7. 코스메틱 — `PlayLocalMuzzleEffect(카메라 위치)`, `ProjectileFast`면 `SpawnLocalCosmeticProjectile(카메라 위치, 방향)`
8. `SendFireTargetData` — **아직 윈도우 안**이어야 `ASC->ScopedPredictionKey`가 이 발의 키다
코스메틱이 클라 카메라 위치인 이유: 플레이어가 보는 건 클라 값. 서버 원점은 판정용 (설계 §4-2).
왜 윈도우가 함수 끝까지 열려 있어야 하나: 지역 변수라 스코프를 나가면 키가 복원된다. `CommitAbilityCost`와 `SendFireTargetData` 둘 다 그 안에서.

**`ArmNextShot()`** — 다음 발 예약. 반복 타이머가 아니라 **매번 한 번짜리**.
1. `IsActive()` 아니면 return — `FireOnce`가 방금 탄약 소진으로 끝냈을 수 있다
2. 남은 시간 = `Weapon->GetFireTimer().GetRemaining(지금, GetFireRateMultiplier())`
3. `SetTimer(FireTimerHandle, this, &OnFireTimerTick, max(남은 시간, KINDA_SMALL_NUMBER), bLoop=false)`
왜 반복 타이머가 아닌가: 배율이 바뀌면 다음 간격이 달라져야 한다 (STATUS §1 "페이싱").

**`OnFireTimerTick()`** — 한 수명 경로의 분기점.
- `IsAutoFire` **또는** `bPendingShot`이면: 예약 비우고 → `FireOnce()` → `ArmNextShot()`
- 아니면: `EndAbility(..., bReplicateEndAbility=true, bWasCancelled=false)` — 서버 인스턴스는 이 RPC로 끝난다. 서버가 자기 타이머로 먼저 끝내면 늦게 온 예약 발이 죽은 어빌리티 앞으로 온다 (STATUS §1 "단발 연타 예약")

### 2-4. 수명

**`CanActivateAbility(...)`** — `const`라 `CurrentActorInfo`가 없다. **매개변수 `ActorInfo`** 로 캐릭터·무기를 찾는다.
1. `Super` 실패면 `false` (Dead/Reloading 태그)
2. `!ActorInfo->IsLocallyControlled()`면 `true` — 서버 인스턴스는 속도를 버킷이 본다
3. 무기·월드 없으면 `false`
4. `Weapon->GetFireTimer().IsElapsed(지금, 배율, 허용오차)`. 허용오차는 간격의 1% 정도 (부동소수점 여유)
이 검사가 실제로 걸리는 경우는 드물다 — 예약 슬롯 덕에 간격 안의 클릭은 `InputPressed`로 간다. 탄약 소진으로 먼저 끝난 직후 클릭만 여기 온다.

**`ActivateAbility(...)`** — 양쪽에서 불린다. 갈림길은 `ActorInfo->IsLocallyControlled()`.
1. `Super` → `bPendingShot = false`
2. 캐릭터·무기 없으면 `EndAbility(..., true, true)` return
3. **로컬 컨트롤이 아니면** (서버가 든 원격 클라 인스턴스): `ASC->AbilityTargetDataSetDelegate(Handle, 활성화 예측 키).AddUObject(this, &OnTargetDataReady)`의 반환값을 `TargetDataDelegateHandle`에 보관하고 return. **쏘지 않는다.** 종료도 클라가 `ServerEndAbility`로 해준다
4. 로컬이면: `FireOnce()` → `ArmNextShot()`
왜 `CallReplicatedTargetDataDelegatesIfSet`를 안 부르나: TargetData는 활성화와 같은 채널이라 항상 뒤에 오고, 배치 RPC도 활성화 → TargetData 순으로 푼다(`ASC_Abilities.cpp:4130-4131`).

**`EndAbility(...)`** — 두 번 불릴 수 있다(클라 `ServerEndAbility` + 서버 자기 탄약 소진).
1. `IsEndAbilityValid(Handle, ActorInfo)` 아니면 return
2. 타이머 정리(지금 코드), `bPendingShot = false`
3. `ActorInfo`가 있고 로컬 컨트롤이 아니면: 델리게이트 `.Remove(TargetDataDelegateHandle)` + `ConsumeClientReplicatedTargetData` (Lyra `EndAbility`와 동일)
4. `Super`

**`InputPressed(...)`** — `Input_Fire`가 스펙이 활성일 때 `AbilitySpecInputPressed`로 넘겨준다. `IsAutoFire`가 아니면 `bPendingShot = true`. Auto는 이미 쏘는 중이라 무시. `Super`는 비어 있다 — 안 불러도 된다.

**`InputReleased(...)`** — `IsAutoFire`면 `EndAbility(Handle, ActorInfo, ActivationInfo, true, true)` — 떼면 연사 중단, 예전 `CancelAbilities`와 같은 경로. Single은 무시(스스로 끝난다). 이게 `Input_StopFire`를 `CancelAbilities`에서 `AbilitySpecInputReleased`로 바꿔야 하는 이유다 — 뗌이 취소면 Single의 예약이 절대 안 찬다.

---

## 3. 다 쓴 뒤 자기 점검 — 코드 안 보고 답할 수 있어야 한다

1. `Single` 더블클릭이 정확히 2발이 되는 경로를 함수 이름으로 나열하라.
2. 서버 인스턴스는 언제 끝나는가? 두 경로.
3. 버킷이 거절한 발에서 탄약이 안 깎이는 이유는?
4. `ServerConfirmOneShot`이 `true`를 반환하는 경우가 두 종류인 이유는?
5. `EndAbility`가 두 번 불려도 안전한 이유는?
6. 호스트는 `SendFireTargetData`를 안 타는데 어디서 갈라지나?
7. `ArmNextShot`이 `IsActive()`를 먼저 보는 이유는?
8. 첫 발에서 새 예측 키를 만들면 안 되는 이유는? (배칭·`CatchUpTo`)
9. 버킷이 거절한 발의 클라 잔탄이 어떻게 돌아오나? 롤백 코드가 없는데.

하나라도 막히면 그 함수의 계약(§2)을 다시 읽고, 그래도 안 되면 그 번호로 묻는다.

---

## 4. 리뷰 요청

빌드가 통과하면 "PrimaryUse.cpp 리뷰"라고 요청한다. 리뷰는 구현서 Step 9와의 차이를 짚는다 — 차이가 곧 배운 것이고, 차이 없이 통과하면 다음 기능부터 구현서에서 코드 블록을 뺀다.

PIE는 구현서 §15 표대로. 임시 로그 세 줄도 거기 있다.
