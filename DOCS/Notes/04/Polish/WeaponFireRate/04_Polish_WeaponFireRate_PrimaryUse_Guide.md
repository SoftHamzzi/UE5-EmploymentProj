# 04 Polish 작성 가이드: `EPGA_Item_PrimaryUse.cpp` — 코드 없이, 계약만

> 대상: `Private/GAS/EPGA_Item_PrimaryUse.cpp`. 헤더는 이미 완성돼 있고(2026-09-21 확인), cpp는 함수 껍데기만 있다.
> 이 문서는 **코드를 주지 않는다.** 함수마다 "무엇을, 어떤 순서로, 어떤 엔진 API로"만 적는다. 코드는 본인이 쓴다.
> 막히면 그 함수만 묻는다. 다 쓴 뒤 리뷰를 요청한다 — 그때 `04_Polish_WeaponFireRate_Implementation.md` Step 9와 비교한다.
> 결정의 근거는 `04_Polish_WeaponFireRate_STATUS.md` §1, 설계는 `../04_Polish_WeaponFireRate.md` §4-2.
>
> **2026-09-23 구조 변경.** 역할별 우회 경로를 없앴다 — `SendFireTargetData`와 `ServerConfirmOneShot`이
> 사라지고 `OnTargetDataReady` 하나로 합쳐진다. 근거는 Lyra `LyraGameplayAbility_RangedWeapon.cpp:489`, `:596`.
> 이미 쓴 세 함수는 §2-2의 새 계약으로 다시 쓴다. §3의 답은 옛 구조 기준이라 그대로 두고, §3-B를 새로 답한다.

---

## 0. 시작 전 정리 (cpp 위쪽)

- [x] 헤더에 남은 `ApplyCooldown` 선언과 cpp의 정의 **삭제** — 쿨다운 GE는 안 쓴다 (STATUS §1 "쿨다운 GE")
- [x] 옛 `ServerConfirmOneShot(const FVector& Origin, const FVector& Direction)` 정의 **삭제** — 헤더에 없다
- [x] `GetFireInterval` 호출 두 곳 **삭제** — 함수가 없다. 간격은 `1 / Weapon->GetBaseFireRate()`
- [x] include: `GameFramework/GameStateBase.h`는 이제 안 쓴다. 대신 필요한 것 — `AbilitySystemComponent.h`(델리게이트·TargetData 호출), `GameFramework/CharacterMovementComponent.h`(클라 예측 데이터), `GAS/EPTargetData_Fire.h`
- [x] `EPTargetData_Fire.h`에 **`TStructOpsTypeTraits` 특수화가 빠져 있다** — `WithNetSerializer = true` 없으면 핸들 직렬화가 안 된다 (엔진 주석 "REQUIRED", `GameplayAbilityTargetTypes.h:384`). 구현서 Step 2 참고. `.cpp`는 비어 있어도 되지만 없어도 된다

- [ ] (09-23) 헤더에서 `SendFireTargetData`·`ServerConfirmOneShot` 선언 **삭제**, cpp의 두 정의도 삭제
- [ ] (09-23) `FireOnce`의 `IsNetAuthority()` 분기 **삭제** — 호스트도 `OnTargetDataReady`로 간다

생성자는 지금 그대로 맞다.

---

## 1. 작성 순서

헬퍼 → 처리 지점 → 페이싱 → 수명 순으로 쓰면 각 단계에서 컴파일이 된다.

```
1. 헬퍼 5개          컴파일
2. OnTargetDataReady          컴파일        ← 발 하나가 처리되는 유일한 함수
3. FireOnce, ArmNextShot, OnFireTimerTick     컴파일
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

### 2-2. 발 하나의 처리 — `OnTargetDataReady(Data, ApplicationTag)`

**이 함수 하나에 세 역할이 전부 들어온다.** 진입 경로만 다르다:
- 오너 클라 / 호스트 — `FireOnce`가 **직접 호출**
- 서버 인스턴스 — `ActivateAbility`에서 붙인 델리게이트가 호출 (TargetData RPC 도착)

그래서 함수 안에서 역할을 묻는 지역 변수 둘을 먼저 만든다:
```
bLocallyControlled = CurrentActorInfo->IsLocallyControlled()
bAuthority         = CurrentActorInfo->IsNetAuthority()
```

| | 오너 클라 | 호스트 | 서버 인스턴스 |
|---|:---:|:---:|:---:|
| `bLocallyControlled` | ○ | ○ | ✗ |
| `bAuthority` | ✗ | ○ | ○ |

순서와 조건:

0. **`bLocallyControlled`가 아니면** `ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, 활성화 키)`.
   안 하면 다음 발이 "overriding pending replicated target data" 로그를 낸다.
1. `Data.IsValid(0)`면 `Data.Get(0)`을 `const FEPTargetData_Fire*`로 `static_cast`. 없으면 return —
   배치 RPC는 TargetData 없이도 델리게이트를 부른다.
2. 캐릭터·무기·컴뱃 확보. 하나라도 없으면 return.
3. **예측 창을 연다.** `FScopedPredictionWindow(ASC, !ASC->ScopedPredictionKey.IsValidForMorePrediction())`.
   서버·호스트에서는 생성자가 즉시 return하므로 아무 일도 안 한다(`GameplayPrediction.cpp:406`) — 역할을 묻지 말고 그냥 연다.
   첫 발은 활성화 창 안이라 활성화 키를 재사용하고, 타이머 발은 새 독립 키를 만든다.
4. **`bAuthority`면 버킷.** `Weapon->GetFireLimiter().TryTake(지금, GetBaseFireRate() × GetFireRateMultiplier())`.
   거절이면 **그냥 return** — 이 발만 버리고 어빌리티는 유지. 클라의 예측 −1은 키 ack으로 저절로 돌아온다.
   **탄약 차감보다 반드시 위**에 와야 한다.
5. **`CommitAbilityCost`.** 역할 조건 없이 **한 번만** 부른다 — 클라에선 예측(창 안), 권위에선 확정.
   실패면 `EndAbility(..., bReplicateEndAbility=true, bWasCancelled=false)` 후 return.
6. **`bLocallyControlled && !bAuthority`면** `ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, 활성화 키, Data, FGameplayTag(), ASC->ScopedPredictionKey)`.
   호스트는 이 조건에서 자동으로 걸러진다 — 자기 자신에게 RPC를 보낼 이유가 없다.
7. **`bLocallyControlled`면 코스메틱.** 카메라 위치로 `PlayLocalMuzzleEffect`, `ProjectileFast`면 `SpawnLocalCosmeticProjectile`.
8. **`bAuthority`면** `Combat->HandleServerFire(Fire->Direction, Fire->ClientMoveTimeStamp)`. 원점은 그 안에서 히스토리 조회.

주의할 점:
- **5번을 역할별 분기 안에 넣지 마라.** 호스트는 로컬이면서 권위라 두 분기에 걸치므로 탄약이 두 배로 닳는다.
  이전 판(`IsNetAuthority()` 분기 + `ServerConfirmOneShot`)에서 실제로 났던 버그다.
- 4번이 5번보다 위인 이유: 거절된 발은 서버 탄약을 깎으면 안 된다.
- 6번을 5번 뒤에 두는 이유: 클라가 이미 실패를 아는 발을 서버에 보내지 않는다.
  (Lyra는 전송이 먼저다 — 활성화당 한 발이라 차이가 안 난다.)

### 2-3. 페이싱 — 로컬 컨트롤만

**`FireOnce()`** — 로컬(오너 클라·호스트)에서만 불린다. **역할 분기가 없다.**
1. 캐릭터·무기 + `Weapon->CanFire()`. 실패면 `EndAbility(..., true, false)` return
2. `Weapon->GetFireTimer().Start(지금, 1 / Weapon->GetBaseFireRate())`
   — **`Duration` 자리에 배율을 넣지 마라.** 배율은 `GetRemaining`/`IsElapsed`의 `Rate` 인자로만
3. `new FEPTargetData_Fire()`에 `Direction = Char->GetControlRotation().Vector()`,
   `ClientMoveTimeStamp = GetClientMoveTimeStamp()` (호스트는 -1)
4. `OnTargetDataReady(FGameplayAbilityTargetDataHandle(Data), FGameplayTag())` — 핸들이 소유권을 가진다. delete 금지

**`ArmNextShot()`** — 다음 발 예약. 반복 타이머가 아니라 **매번 한 번짜리**.
1. `IsActive()` 아니면 return — `FireOnce`가 방금 탄약 소진으로 끝냈을 수 있다
2. 남은 시간 = `Weapon->GetFireTimer().GetRemaining(지금, GetFireRateMultiplier())`
3. `SetTimer(FireTimerHandle, this, &OnFireTimerTick, max(남은 시간, KINDA_SMALL_NUMBER), bLoop=false)`
왜 반복 타이머가 아닌가: 배율이 바뀌면 다음 간격이 달라져야 한다 (STATUS §1 "페이싱").

**`OnFireTimerTick()`** — 한 수명 경로의 분기점.
- `IsAutoFire` **또는** `bPendingShot`이면: 예약 비우고 → `FireOnce()` → `ArmNextShot()`
- 아니면: `EndAbility(..., bReplicateEndAbility=true, bWasCancelled=false)` — 서버 인스턴스는 이 RPC로 끝난다

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
3. **로컬 컨트롤이 아니면** (서버가 든 원격 클라 인스턴스): `ASC->AbilityTargetDataSetDelegate(Handle, 활성화 예측 키).AddUObject(this, &OnTargetDataReady)`의 반환값을 `TargetDataDelegateHandle`에 보관하고 return. **`FireOnce`를 타지 않는다** — 발 하나하나는 TargetData가 도착할 때 `OnTargetDataReady`로 들어온다. 종료도 클라가 `ServerEndAbility`로 해준다
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

## 3. 자기 점검 (2026-09-22 판 — 옛 구조 기준, 답 그대로 보존)

> 아래 9문항은 `SendFireTargetData`/`ServerConfirmOneShot`이 있던 구조에 대한 것이다.
> 답은 학습 기록이라 고치지 않는다. 새 구조에 대한 점검은 §3-B.

1. `Single` 더블클릭이 정확히 2발이 되는 경로를 함수 이름으로 나열하라.
input_fire -> canactivateability -> activateability -> fireonce -> armnextshot -> fireonce -> armnextshot -> endability
2. 서버 인스턴스는 언제 끝나는가? 두 경로.
activateability 이후 fireonce -> armnextshot->endability에서 inputreleased로 인한 발사 신호가 없어서 종료
잔탄이 없어 commitabilitycost에 실패하며 종료?
3. 버킷이 거절한 발에서 탄약이 안 깎이는 이유는?
발사 요청이 몰려왔기때문에 그 발사를 거절하고 연사를 이어나간다. 이때 그 발은 발사 신호가 보내지지 않기에 발사 + 소모가 되지 않는다
4. `ServerConfirmOneShot`이 `true`를 반환하는 경우가 두 종류인 이유는?
하나는 패킷이 몰려와서 FEPFireLimit에 걸린 경우 그 발사는 취소하고 연사를 이어나가기 위함
다른 하나는 정상 발사시 true를 반환한다
5. `EndAbility`가 두 번 불려도 안전한 이유는?
잘 몰라서 코드를 보았다. IsValidEndAbility 함수를 호출하여 체크하기 때문이라 생각한다.
또한 클라가 총 발사를 그만두는 순간 inputreleased가 발동되어 서버의 endability를 호출하고, 서버는 탄약이 소진되었을때 endability를 호출한다
6. 호스트는 `SendFireTargetData`를 안 타는데 어디서 갈라지나?
클라이언트 측의 fireonce에서 targetdata를 보냈던 것으로 기억한다.
그러고 서버는 dataready같은 네트워크로 따지면 IOCP 같은 함수에서 데이터를 처리한다
7. ArmNextShot`이 `IsActive()`를 먼저 보는 이유는?
클라의 inputreleased로 인해 어빌리티가 종료되었을수 있어서
8. 첫 발에서 새 예측 키를 만들면 안 되는 이유는? (배칭·`CatchUpTo`)
어빌리티는 정상적으로 취소되지만 이미 소모된 탄약은 취소되지 않는 문제가 발생한다.
그렇기에 어빌리티 생성시 가지고 있었던 예측키를 그대로 사용한다
9. 버킷이 거절한 발의 클라 잔탄이 어떻게 돌아오나? 롤백 코드가 없는데.
버킷 거절시 발사 명령이 내려지지 않으며, 서버에서는 commitabilitycost를 통해 ge 잔탄 값을 바꾸지 않게 된다. 그러므로 클라값도 이후 예측했던 값이 무효화되며 이전 값으로 돌아온다

하나라도 막히면 그 함수의 계약(§2)을 다시 읽고, 그래도 안 되면 그 번호로 묻는다.

---

## 3-B. 자기 점검 (2026-09-23 구조)

1. `OnTargetDataReady`에 들어오는 경로가 셋이다. 각각 누가 부르는가?
2. `bLocallyControlled`와 `bAuthority`의 조합 네 가지 중 실제로 존재하는 셋은? 각각 어느 단계를 타는가?
3. `CommitAbilityCost`를 역할 분기 안에 넣으면 무엇이 깨지는가? 어느 역할에서?
4. 버킷 `TryTake`가 `CommitAbilityCost`보다 **위**여야 하는 이유는?
5. 호스트가 `CallServerSetReplicatedTargetData`를 건너뛰는 것은 `if`인가, 아니면 다른 장치인가?
6. 예측 창을 역할 확인 없이 그냥 여는 것이 왜 안전한가? (근거 파일:줄)
7. `FireOnce`에 이제 역할 분기가 하나도 없다. 그래도 호스트와 원격 클라가 다르게 동작하는 이유는?

---

## 4. 리뷰 요청

빌드가 통과하면 "PrimaryUse.cpp 리뷰"라고 요청한다. 리뷰는 구현서 Step 9와의 차이를 짚는다 — 차이가 곧 배운 것이고, 차이 없이 통과하면 다음 기능부터 구현서에서 코드 블록을 뺀다.

PIE는 구현서 §15 표대로. 임시 로그 세 줄도 거기 있다.
