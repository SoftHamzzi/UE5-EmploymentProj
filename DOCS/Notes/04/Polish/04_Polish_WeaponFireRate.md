# Polish — 무기 발사 속도 / 재장전 태그 / 탄약 동기화

**상태(2026-09-20):** §1~§2는 코드에 적용돼 있다. **§4(발사 간격 + TargetData 발사 전송, 안 B)와
§5(재장전 태그)는 미구현** — 스킬 쪽(`04_Polish_SkillDisplay.md`) PIE 검증이 끝난 뒤 같은 부품으로
한다. 결정의 진실의 원천은 `WeaponFireRate/04_Polish_WeaponFireRate_STATUS.md`다 — 이 문서와
어긋나면 STATUS가 맞다. `Entry.State.Charges` 이관은 Loot Step 05로 미룬 범위 밖 항목.

관련 소스: `EPGA_Item_PrimaryUse.h/.cpp`, `EPGA_Item_Reload.h/.cpp`, `EPCombatComponent.cpp`,
`EPWeapon.h/.cpp`, `EPCharacter.cpp`(`Input_Fire`), `GE_FireCooldown`·`GE_Reloading`(에셋).
원칙과 부품(`FEPLocalTimer`, `FEPLocalModifiers`, `ActivationOwnedTags`, 로컬 시계)은
`04_Polish_SkillDisplay.md` §3에 있다 — 여기서 다시 유도하지 않는다.

---

## 1. 현재 구조 (적용 완료)

**완전자동 어빌리티** — 총알 한 발마다 어빌리티를 새로 활성화하지 않는다.
`Input_Fire`(`Started`)가 어빌리티를 한 번 활성화하면, `EEPFireMode::Auto`일 때
`ActivateAbility()`가 `FireOnce()`를 첫 발 쏜 뒤 내부 `FTimerHandle`로 `FireInterval`마다
반복한다. `Input_StopFire`가 `CancelAbilities()`로 끝낼 때까지 인스턴스가 살아 있다.
`Single`/`Burst`는 첫 발 직후 `EndAbility`.

```cpp
void UEPGA_Item_PrimaryUse::FireOnce()
{
    if (!Char || !Weapon || !Weapon->CanFire()) { EndAbility(...); return; }   // 탄약 소진 시 자동 종료

    CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);   // ← §4에서 사라진다

    if (!CurrentActorInfo->IsLocallyControlled()) return;   // 서버가 든 "원격 클라의" 인스턴스는 발사에 관여 안 함

    if (CurrentActorInfo->IsNetAuthority())
        ServerConfirmOneShot(Origin, Direction);             // 호스트 자신 — 왕복 없이 즉시
    else
    {
        Combat->PlayLocalMuzzleEffect(Origin);               // 원격 클라 — 코스메틱만 로컬
        Combat->Server_ConfirmFire(Origin, Direction, CurrentSpecHandle);   // 실제 확정은 RPC
    }
}
```

**`ServerConfirmOneShot()`이 유일한 발사 확정 지점** — `CommitAbilityCost`(탄약 소모)와
`HandleServerFire`(히트스캔·데미지)를 여기서만 부른다. 경로는 둘: 호스트의 `FireOnce()`가
직접, 또는 `Server_ConfirmFire_Implementation`(`Server, Reliable`, 캐릭터 채널)이 원격 클라의 RPC를 받아서.
서버는 `FGameplayAbilitySpecHandle`로 정확히 그 인스턴스를 찾는다 — `FindAbilitySpecFromClass`는
BP 서브클래스에서 완전 일치 실패로 항상 `nullptr`이었다(실측).

**원점·방향은 지금 클라가 RPC에 실어 보낸다** (`Origin`=카메라 위치, `Direction`=`GetControlRotation().Vector()`).
서버는 원점을 현재 위치 기준 200cm 드리프트로만 검사한다(`EPCombatComponent.cpp:67-71`) —
§4-2에서 원점 전송과 함께 없어진다.

**구현 노트:**
- `CommitAbility()` 대신 `CommitAbilityCost` + `CommitAbilityCooldown(ForceCooldown=true)` —
  `CheckCooldown()`이 직전 발의 쿨다운에 이번 발을 스스로 걸리게 만들었다. (§4 뒤엔 둘 다 안 씀)
- `bServerRespectsRemoteAbilityCancellation = true` — `false`면 `Input_StopFire`가 클라
  인스턴스만 멈추고 서버는 탄창이 빌 때까지 계속 쏜다. **유지.**
- `Single`은 `EndAbility(..., bReplicateEndAbility=false, ...)` — 양쪽이 각자 `ActivateAbility`
  안에서 끝나므로 RPC가 필요 없었다. §4-2의 예약 슬롯이 들어가면 `true`로 바뀐다.

## 2. 결과 (적용 완료)

- GAS Reliable RPC(`ServerTryActivateAbility`/`ServerEndAbility`)는 연사 한 번당 한 쌍 — 핑 의존성 없음
- `!IsLocallyControlled()` 가드로 서버가 원격 클라의 발사를 이중으로 세지 않는다 — RPC 수 : 발사 수 = 1:1
- 호스트/스탠드얼론에서도 `HandleServerFire`가 정상 (로컬+권위 분기가 `ServerConfirmOneShot` 직접 호출)

## 3. 범위 밖

- **`Entry.State.Charges` 이관** — 탄약을 `Ammo` 속성 대신 인벤토리 아이템 인스턴스에.
  `05_Loot_05_Equipment.md`에 설계돼 있고 Step 03·04 완성 전제. 이관 시 `CommitAbilityCost`
  자리만 바뀌고 `ServerConfirmOneShot` 구조는 그대로.
- **`FireMode::Burst`** — "N발 고정 연사 후 종료" 분기(발사 카운터)가 아직 없다. §4의 타이머는
  `Burst`에도 그대로 쓰이므로 카운터만 추가하면 된다.
- **`Auto`의 서버 쪽 U** — 서버는 클라 첫 발보다 U 늦게 시작하고 U 늦게 끝난다. 발사
  판정 자체는 SSR이 되감으니 무관. 이동처럼 정정이 생기는 것도 없다.

---

## 4. 미해결 → 설계 — 발사 간격을 GE에서 뺀다

> 2026-09-05 증상 확인·원인 규명. 2026-09-18 스킬 쪽 부품 확정 후 설계 갱신. 2026-09-20
> 안 B(TargetData 전송 + 클라 방향) 확정, 2026-09-21 UT식 원점 동기 추가 — 이력은 STATUS §2. 코드 미적용.

### 4-1. 증상과 원인

`Single`로 클릭하면 발사가 안 되거나 크게 늦다. `Auto`는 정상.

```
FireOnce → CommitAbilityCooldown(ForceCooldown=true) → GE_FireCooldown 적용 (1/FireRate 초, 태그 Cooldown.Weapon.PrimaryUse)
Single → 즉시 EndAbility
다음 클릭 → CanActivateAbility → CheckCooldown() → 태그 아직 있음 → 거부
```

`ForceCooldown`은 **적용**만 강제한다. 다음 활성화의 **검사**는 못 건너뛴다. `Auto`가 멀쩡한
건 어빌리티가 살아 있는 동안 타이머가 `FireOnce`를 직접 불러 `CheckCooldown()`을 안 거치기
때문이다. 실질 차단 시간은 `1/FireRate + RTT` — 쿨다운 GE의 제거가 예측되지 않아서다
(스킬과 같은 뿌리, `Issue/FireRate_GECooldownPrediction.md`).

**본질:** 발사 간격은 "내가 시작한 시간 상태"다. §3-0 규칙대로 GE가 아니라 어빌리티/무기가
든다. 필요한 건 셋 — **페이싱**(다음 발 언제: `Auto`의 루프, `Single`의 연타 예약),
**서버 검증**(이 발을 허락하나), 그리고 **발사 전송**(방향을 어떻게 서버에 보내나).
복제도 예측도 필요 없다.

### 4-2. 구조 — 안 B: 발마다 "쏴라 + 그때의 방향 + 어느 무브였나"를 TargetData로

```
                    클라 (오너)                                          서버
        ┌──────────────────────────────────────┐          ┌────────────────────────────────────────┐
 클릭 → │ Input_Fire                            │          │                                        │
        │   활성 중? → AbilitySpecInputPressed  │          │                                        │
        │   아니면 배치{ TryActivateAbility }   │──배치 1─▶│ ActivateAbility — 발사 안 함, 대기      │
        │ ActivateAbility → FireOnce            │  (활성화  │   TargetData 델리게이트 바인딩          │
        │   FireTimer.Start(Now, Interval)      │  +TD)    │                                        │
        │   PlayLocalMuzzleEffect               │          │ OnTargetDataReady(Direction)            │
        │   TargetData{Direction,TimeStamp} 전송│          │   → ServerConfirmOneShot(Direction, TS) │
        │   SetTimer(GetRemaining(Now,Rate), 1회)│          │       FireLimiter.TryTake ? 발사 : 버림 │
 틱 →   │   Auto 또는 bPendingShot → FireOnce ──│──TD 1──▶ │       원점 = 히스토리[TimeStamp] 카메라  │
        │   아니면 EndAbility(복제) ────────────│──End 1─▶ │       방향 = 페이로드 그대로            │
        └──────────────────────────────────────┘          └────────────────────────────────────────┘
```

**방향은 클라가 보내고, 원점은 서버가 계산한다 — 단, "어느 무브의 위치인지"는 클라가 알려준다.**

- **방향 클라:** `FireOnce` 순간 읽은 `GetControlRotation().Vector()`를 그대로 실으므로
  오차 0. 서버 컨트롤러 회전에서 꺼내는 방식(안 C, 폐기)은 `ServerMove`가 캐릭터 채널로,
  TargetData가 PlayerState(ASC) 채널로 따로 오기 때문에 순서 보장이 없고, 연사에선 서버
  타이머와 이동 도착이 경쟁해 항상 ±1프레임이 남았다(60fps 16ms — 100°/s 트래킹이면 1.6°,
  30m에서 84cm). 조작해도 얻는 게 없는 값이라 검증하지 않는다. Source/Valorant가 명령마다
  시점 각도를 동봉하는 usercmd 모델과 같은 모양.
- **원점 서버:** 클라가 보낼 이유가 없다(200cm 드리프트 검사 같은 조작 구멍만 생긴다).
  서버는 이동 패킷 사이에 자기 캐릭터를 움직이지 않으므로(`CharacterMovementComponent.cpp:1739-1754`)
  서버가 아는 위치는 항상 "어떤 무브의 결과"다. 문제는 TargetData(PlayerState 채널)와 그 프레임의
  `ServerMove`(캐릭터 채널) 중 어느 쪽이 먼저 처리되느냐 — 뒤집히면 이동 1프레임분(6m/s·60fps 10cm)
  앞선 위치가 원점이 된다.
- **타임스탬프 동기 (UT `SavedPositions` 방식):** 그래서 페이로드에 **클라가 쏜 순간 마지막으로 완료된
  무브의 타임스탬프**를 싣는다. 서버는 무브를 적용할 때마다 `{TimeStamp, 카메라 위치}`를 링버퍼에
  기록하고(주인은 SSR 컴포넌트 — UT `SavedPositions`처럼 "이 캐릭터의 과거 위치"는 한 곳에. CMC는 델리게이트로
  알리기만), `HandleServerFire`가 페이로드 타임스탬프로 그 위치를 꺼낸다 → 채널 순서와 무관하게
  **클라가 쏜 순간의 위치, 오차 0.** 히스토리에 없으면(그 무브가 아직 안 왔거나 4분 타임스탬프 리셋
  직후) 현재 카메라 위치로 폴백 — 그때만 위의 1프레임. UT처럼 무브가 올 때까지 기다리지 않는다:
  기다리면 발사 시각이 밀려 SSR 되감기와 어긋난다(버킷 대신 큐를 안 쓴 것과 같은 이유).
  타임스탬프는 검증하지 않는다 — 거짓 값으로 얻는 건 히스토리 안(≤1초)의 자기 과거 위치뿐이고,
  그 위치들은 서버가 이미 본 것이다.
  클라 쪽 값은 `CMC->GetPredictionData_Client_Character()->CurrentTimeStamp` — CMC 틱에서 `+= DeltaTime`
  뒤 그 프레임 무브의 `TimeStamp`가 된다(`:8743`, `:12548`). 이 값과 캐릭터 위치는 둘 다 "마지막으로 실행된
  무브"의 것이라 **언제 읽어도 짝이 맞는다** — 첫 발(`Input_Fire`, CMC 틱 전)은 무브 N−1 짝, 타이머 발
  (`FTimerManager::Tick`은 `TG_PrePhysics` 뒤, `LevelTick.cpp:1721→:1787`)은 무브 N 짝. 서버 쪽은 `ServerMove_PerformMovement`가
  `CurrentClientTimeStamp`를 세팅하고(`:9900`) `MoveAutonomous`(`:9924`) → `OnMovementUpdated`에서
  `GetCurrentNetworkMoveData()->TimeStamp`로 읽는다. 번들(`ServerMoveDual`)은 Old/New 각각 이 경로를 타므로
  **모든 `NetworkMoveType`을 기록**한다 — SSR 훅이 `NewMove`만 보는 것과 다르다.

**전송은 GAS TargetData** — `CallServerSetReplicatedTargetData` → 서버 `AbilityTargetDataSetDelegate`
→ `ConsumeClientReplicatedTargetData`. `ServerSetReplicatedTargetData`는 `Server, reliable`
(`AbilitySystemComponent.h:1570`)이라 정확히 한 번, 보낸 순서로 도착한다 — 순번(`FireIndex`)이
필요 없다. 예측 키로 서버가 활성화와 자동 매칭하고, 활성화보다 먼저 온 TargetData도
`AbilityTargetDataMap`에 캐시된다. 무기와 향후 스킬 타게팅이 한 파이프라인. Lyra와 같은 모양
(단, Lyra는 `FHitResult`를 보내고 우리는 방향만 보낸다 — STATUS §5).

**단발 연타 예약.** 간격 안의 클릭은 거절하지 않고 한 칸 예약한다(UT `PendingFireSequence`).
`Single` 어빌리티는 발사 후 바로 끝나지 않고 `Interval`까지 살아 있으며, 그 사이 클릭은
`Input_Fire`가 `AbilitySpecInputPressed`(`AbilitySystemComponent_Abilities.cpp:2879`)로 살아 있는
인스턴스에 넘겨 `InputPressed` → `bPendingShot = true`. 타이머 틱에서 예약이 있으면 한 발 더
쏘고 다시 예약, 없으면 종료. **`Auto`와 `Single`이 한 수명 경로** — `Single`+예약은 2발짜리
`Auto`다. 서버 인스턴스는 클라의 `ServerEndAbility`로 끝난다(`bReplicateEndAbility=true`) —
서버가 자기 타이머로 먼저 끝내면 늦게 온 예약 발이 죽은 어빌리티 앞으로 도착하기 때문.

**발사 속도 배율.** `유효 발사 속도 = WeaponDef->FireRate × LocalModifiers.Product(Modifier.FireRate)`.
무기 스탯을 바꾸지 않는다 — `WeaponDef`는 공유 DataAsset이고, 여우길형 버프는 **플레이어에게**
걸리는 것이라 무기를 바꿔도 따라가야 한다(`FEPLocalModifiers`는 캐릭터에 있다,
`EPCharacter.h:164`). 클라 페이싱(`FireTimer`의 `Rate`)과 서버 검증(버킷 `Refill`)이 같은 식을 쓴다.
배율 변경은 **다음 발부터** 반영 — 진행 중인 간격은 옛 배율로 끝난다(최대 한 간격, 한 번.
`Bank`+재예약은 안 한다).

**부품과 위치.**

| 부품 | 쪽 | 위치 | 역할 |
|---|---|---|---|
| `FireTimer` | 클라(오너)·호스트 | `AEPWeapon` `FEPLocalTimer` | 페이싱. 매 발 `Start(Now, Interval)` → `SetTimer(GetRemaining(Now, Rate), 반복 X)`로 다음 발 재예약. `CanActivateAbility`의 연타 가드 |
| `FireLimiter` | 서버 | `AEPWeapon` `FEPRateLimiter` (신규, §4-3) | 서버 검증. 타이머가 아니라 토큰 버킷 |
| `bPendingShot`·`FireTimerHandle` | 양쪽 | `UEPGA_Item_PrimaryUse` | 연타 예약 한 칸, 재예약 핸들 |
| `FEPTargetData_Fire` | 전송 | 신규 구조체 | `{ FVector_NetQuantizeNormal Direction, float ClientMoveTimeStamp }` — 원점 없음, 순번 없음 |
| `ShotOriginHistory` | 서버 | `UEPServerSideRewindComponent` 링버퍼 (`ShotOriginHistoryCount`, 기본 64 ≈ 60fps 1초). 되감기 스냅샷(`HitboxHistory`, 본 단위, PostPhysics, NewMove만)과 **별도 배열** — 원점은 무브마다·카메라 하나면 되고, 저장 시점도 동기 | 무브 타임스탬프 → 그 무브 결과의 카메라 위치. `GetShotOriginAt(TimeStamp, Out)` |

타이머·버킷을 무기 인스턴스에 두는 건 Lyra가 `TimeLastFired`를 `ULyraWeaponInstance`에 두는 것과
같다(`LyraWeaponInstance.cpp:58`) — 어빌리티는 InstancedPerActor라 무기를 바꿔도 인스턴스가
하나여서, 어빌리티에 두면 교체 직후 간격이 이전 무기에 묶인다.

### 4-3. 왜 서버는 타이머가 아니라 토큰 버킷인가

스킬은 `Cooldown − Tolerance`로 서버가 검사했다. 무기에 그대로 쓰면 두 가지가 깨진다:

1. **비율 문제** — 간격 0.1s에 허용 오차 0.1s면 검사가 없는 것과 같다. 간격에 비례시키면
   (`Interval × 0.5`) 조작 클라가 그 비율만큼 **지속적으로** 빠르게 쏠 수 있다.
2. **패킷 몰림** (`04_Polish_SkillDisplay.md` §3-9에서 인계) — TargetData는 Reliable이라 유실
   뒤 재전송된 발과 다음 발이 **같은 프레임에** 도착한다. 타이머는 둘째를 무조건 버린다.

토큰 버킷은 둘 다 푼다 — **순간 몰림은 허용하되 지속 속도는 정확히 유효 발사 속도로 상한.**
Source의 `sv_maxusrcmdprocessticks`(클라 명령 개수 제한)에 해당하는, "클라가 발사 시각을
정하는 설계"의 표준 짝이다.

```cpp
// Public/Combat/EPRateLimiter.h — 헤더 전용, 복제 안 함
struct FEPRateLimiter
{
    void Reset(float Now, float InMaxTokens) { MaxTokens = InMaxTokens; Tokens = InMaxTokens; LastUpdate = Now; }

    /** 토큰이 1 이상이면 하나 쓰고 true. 아니면 false — 이번 요청을 버린다. RefillPerSecond = 호출 시점의 유효 발사 속도 */
    bool TryTake(float Now, float RefillPerSecond)
    {
        Tokens = FMath::Min(MaxTokens, Tokens + (Now - LastUpdate) * RefillPerSecond);
        LastUpdate = Now;
        if (Tokens < 1.f) return false;
        Tokens -= 1.f;
        return true;
    }
private:
    float Tokens = 0.f, MaxTokens = 1.f, LastUpdate = 0.f;
};
```

- `RefillPerSecond = 유효 발사 속도`(§4-2, `TryTake` 시점 배율), `MaxTokens = FireRateBurstAllowance`(설정, 기본 **2**).
  리필 속도를 저장하지 않고 매번 받는 이유: 배율이 버프로 바뀌므로 버킷이 값을 들고 있으면 갈린다.
- 정직한 클라: 간격마다 1발 → 토큰이 1씩 차고 1씩 빠져 항상 통과. 두 발이 몰려 와도
  2까지는 통과. 세 발이 몰리면 셋째를 버린다 — 그 정도 몰림은 정상 네트워크에서 안 난다.
- 조작 클라: 초당 유효 발사 속도를 넘는 만큼은 **무조건** 버려진다. 처음 `MaxTokens`발만
  앞당길 수 있고 그 뒤론 정확히 상한. 상한이 명시적이다(스킬의 `Tolerance`와 같은 성질).
- 시계는 `World->GetTimeSeconds()` — 서버 자기 것끼리만 뺀다.

### 4-4. 코드 변경점

**`AEPWeapon`**
```cpp
// EPWeapon.h
#include "GAS/EPLocalTimer.h"
#include "Combat/EPRateLimiter.h"
public:
    FEPLocalTimer&  GetFireTimer()   { return FireTimer; }
    FEPRateLimiter& GetFireLimiter() { return FireLimiter; }
    float GetBaseFireRate() const { return WeaponDef ? WeaponDef->FireRate : 5.f; }   // PrimaryUse의 static GetFireInterval과 통합
private:
    FEPLocalTimer  FireTimer;     // 오너 클라·호스트의 페이싱
    FEPRateLimiter FireLimiter;   // 서버 검증
```
`BeginPlay`(서버)에서 `FireLimiter.Reset(Now, Settings->FireRateBurstAllowance)`.

**`FEPTargetData_Fire`** (`Public/GAS/EPTargetData_Fire.h`, 신규)
```cpp
USTRUCT()
struct FEPTargetData_Fire : public FGameplayAbilityTargetData
{
    GENERATED_BODY()
    UPROPERTY() FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

    virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);   // Direction만
};
template<> struct TStructOpsTypeTraits<FEPTargetData_Fire> : TStructOpsTypeTraitsBase2<FEPTargetData_Fire> { enum { WithNetSerializer = true }; };
```

**`UEPGA_Item_PrimaryUse`** — 수명이 이렇게 바뀐다:
- `ApplyCooldown` 오버라이드 **삭제**, `FireOnce`의 `CommitAbilityCooldown` **삭제**, `static GetFireInterval` **삭제**
- 멤버 추가: `bool bPendingShot`, `FDelegateHandle TargetDataDelegateHandle` (`FireTimerHandle`은 유지)
- `ActivateAbility`:
```cpp
    if (!ActorInfo->IsLocallyControlled())            // 서버가 든 원격 클라 인스턴스 — 쏘지 않고 TargetData만 기다린다
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
            .AddUObject(this, &UEPGA_Item_PrimaryUse::OnTargetDataReady);
        return;   // 같은 채널이라 TargetData는 활성화 뒤에 온다. 배치 RPC도 활성화 → TargetData 순
    }
    FireOnce();
    ArmNextShot();
```
- `CanActivateAbility` 오버라이드 — 로컬 컨트롤 쪽만 연타 가드(예약 슬롯 덕에 보통은 안 걸린다 —
  어빌리티가 `Interval` 동안 살아 있어 클릭이 `InputPressed`로 간다. 탄약 소진 등으로 먼저 끝난
  직후 클릭이 유일한 경로):
```cpp
    if (!Super::CanActivateAbility(...)) return false;               // Dead/Reloading 태그
    if (!ActorInfo->IsLocallyControlled()) return true;               // 서버 인스턴스: 속도는 ServerConfirmOneShot의 버킷이 본다
    const float Rate = Char->GetLocalModifiers().Product(EmpGameplayTags::TAG_Modifier_FireRate);
    return Weapon->GetFireTimer().IsElapsed(Now, Rate, 0.01f / Weapon->GetBaseFireRate());   // 여유 1%
```
- `FireOnce` (로컬 컨트롤에서만 불린다):
```cpp
    if (!Weapon->CanFire()) { EndAbility(..., true, true); return; }
    Weapon->GetFireTimer().Start(Now, 1.f / Weapon->GetBaseFireRate());
    const FVector Direction = Char->GetControlRotation().Vector();

    if (CurrentActorInfo->IsNetAuthority())
    {
        if (!ServerConfirmOneShot(Direction))                           // 호스트 — 왕복 없이
            EndAbility(..., true, true);
    }
    else
    {
        const FVector CamLoc = Char->GetCameraComponent()->GetComponentLocation();
        Combat->PlayLocalMuzzleEffect(CamLoc);                          // 코스메틱은 클라 값으로
        if (ProjectileFast) Combat->SpawnLocalCosmeticProjectile(CamLoc, Direction);

        // 탄약 예측 — 첫 발은 활성화 윈도우 안이라 활성화 키 재사용(배치 RPC가 활성화 키만 싣는다), 타이머 발은 새 키
        FScopedPredictionWindow ScopedPrediction(ASC, /*bCanGenerateNewKey*/ !ASC->ScopedPredictionKey.IsValidForMorePrediction());
        if (!CommitAbilityCost(...)) { EndAbility(..., true, true); return; }   // GE_ConsumeAmmo를 예측 적용 → HUD 즉시 −1

        FGameplayAbilityTargetDataHandle Data(new FEPTargetData_Fire{ Direction, ClientMoveTimeStamp });
        ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
                                               Data, FGameplayTag(), ASC->ScopedPredictionKey);   // 이 발의 예측 키 — 서버가 같은 키로 차감·ack
    }
```

**탄약 예측 (2026-09-22 추가).** 원격 클라가 발마다 예측 키 아래에서 `CommitAbilityCost`를 부르면
`GE_ConsumeAmmo`(Instant)가 클라에선 **무한 지속 GE로 예측 적용**된다(`AbilitySystemComponent.cpp:988`) —
HUD는 속성 변화 델리게이트를 보므로 즉시 −1. 그 키를 TargetData의 `CurrentPredictionKey`로 실으면 서버
`ServerSetReplicatedTargetData_Implementation`이 그 키로 윈도우를 열고, 델리게이트 → `ServerConfirmOneShot` →
`CommitAbilityCost`가 같은 키로 진짜 차감, 윈도우 소멸자가 키를 ack → 클라의 예측 GE 제거
(`GameplayEffect.cpp:4449`), 서버 복제값이 남는다. **버킷 거절·서버 탄약 부족이면 차감 없이 ack만** →
예측 GE 제거 = 탄약이 돌아온다. 롤백 코드가 따로 없다.
키 규칙 하나 — `CatchUpTo`는 **정확히 그 키만** 잡는다(`GameplayPrediction.cpp:321-335`). 배치 RPC는
`CurrentPredictionKey`에 활성화 키를 싣기 때문에(`ASC_Abilities.cpp:4131`), 첫 발에서 종속 키를 새로 만들면
그 키는 영영 ack되지 않아 예측 GE가 남는다. 그래서 활성화 윈도우 안(첫 발)이면 그 키를 그대로 쓰고,
타이머 발(윈도우 밖)만 새 키를 만든다 — 위 `bCanGenerateNewKey` 조건이 그것이다.
Lyra는 탄약을 예측하지 않는다(`ULyraAbilityCost_ItemTagStack::ApplyCost`가 권위에서만). 우리는 HUD 지연이
체감된 문제라 한다.
- `ArmNextShot` / `OnFireTimerTick` / `InputPressed`:
```cpp
void ArmNextShot()
{
    const float Rate = Char->GetLocalModifiers().Product(EmpGameplayTags::TAG_Modifier_FireRate);
    GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &ThisClass::OnFireTimerTick,
                                           Weapon->GetFireTimer().GetRemaining(Now, Rate), /*bLoop*/ false);
}
void OnFireTimerTick()
{
    if (FireMode == EEPFireMode::Auto || bPendingShot) { bPendingShot = false; FireOnce(); ArmNextShot(); }
    else EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ true, false);
}
void InputPressed(...) override  { if (FireMode != EEPFireMode::Auto) bPendingShot = true; }                 // Auto는 이미 쏘는 중
void InputReleased(...) override { if (FireMode == EEPFireMode::Auto) EndAbility(..., true, true); }   // 떼면 연사 중단. Single은 스스로 끝난다
```
`Input_StopFire`는 `CancelAbilities` 대신 `AbilitySpecInputReleased` — `Single`의 클릭은 "누름+뗌"이라
뗌이 취소면 예약 슬롯이 절대 안 찬다. 뗌의 의미를 어빌리티가 모드별로 정한다.
  `FireOnce`가 탄약 소진으로 `EndAbility`를 부른 뒤 `ArmNextShot`이 다시 돌지 않도록, `EndAbility`가
  `FireTimerHandle`을 지우는 지금 코드를 유지하고 `ArmNextShot`은 `IsActive()`를 확인한다.
- `OnTargetDataReady` (서버):
```cpp
void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag /*ActivationTag*/)
{
    const FEPTargetData_Fire* Fire = static_cast<const FEPTargetData_Fire*>(Data.Get(0));
    ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
    if (!Fire) return;
    if (!ServerConfirmOneShot(Fire->Direction))
        EndAbility(..., true, false);                                   // 탄약 소진 → 서버가 끝내면 클라도 따라 끝난다
}
```
- `ServerConfirmOneShot(const FVector& Direction)`:
```cpp
    const float Rate = Char->GetLocalModifiers().Product(EmpGameplayTags::TAG_Modifier_FireRate);
    if (!Weapon->GetFireLimiter().TryTake(Now, Weapon->GetBaseFireRate() * Rate)) return true;   // 너무 빠름 → 이 발만 버림, 어빌리티 유지
    if (!CommitAbilityCost(...)) return false;                                                    // 탄약 소진 → 종료
    Combat->HandleServerFire(Direction, ClientMoveTimeStamp);                                     // 원점은 안에서 서버가 히스토리에서
    return true;
```
  `true`/`false`의 의미: `false`만 `EndAbility`(탄약 소진). 속도 초과는 한 발 버리는 것이지 사격
  종료가 아니다.
- `EndAbility`: 타이머 정리(지금 코드) + 서버면 `AbilityTargetDataSetDelegate(...).Remove(TargetDataDelegateHandle)`.

**`UEPCombatComponent`**
```cpp
// .h — Server_ConfirmFire / Server_ConfirmFire_Implementation 삭제 (수신은 어빌리티의 델리게이트)
void HandleServerFire(const FVector& Direction, float ClientMoveTimeStamp);   // Origin → 타임스탬프. 호스트는 -1

// .cpp — HandleServerFire 앞부분. 드리프트 검사 블록(:67-71) 삭제
    AEPCharacter* Owner = GetOwnerCharacter();
    FVector Origin;
    if (!SSR->GetShotOriginAt(ClientMoveTimeStamp, Origin))                          // 그 무브 결과의 카메라 위치
        Origin = Owner->GetCameraComponent()->GetComponentLocation();                // 폴백: 현재 (호스트 / 무브 미도착 / 리셋 직후)
    // 이하 기존 탄도 분기 그대로 (Origin 지역 변수, Direction 매개변수)
```

**`UEPCharacterMovement`** — 델리게이트를 모든 무브에서, 인자 둘 추가:
```cpp
// .h
DECLARE_MULTICAST_DELEGATE_FourParams(FEPOnServerMoveProcessed, float /*ServerTime*/, FVector /*Location*/, float /*ClientTimeStamp*/, bool /*bNewMove*/);

// .cpp — OnMovementUpdated: NewMove-only return을 없애고 모든 무브 타입에서
    OnServerMoveProcessed.Broadcast(T, GetActorLocation(), MoveData->TimeStamp, bNewMove);
```

**`UEPServerSideRewindComponent`** — 원점 히스토리의 주인 (되감기 스냅샷 옆, 배열은 따로):
```cpp
// .h
bool GetShotOriginAt(float ClientTimeStamp, FVector& OutOrigin) const;   // 서버. 없으면 false
private:
    struct FEPShotOriginEntry { float TimeStamp; FVector Origin; };
    TArray<FEPShotOriginEntry> ShotOriginHistory;   // 링버퍼, 크기 Settings->ShotOriginHistoryCount
    int32 ShotOriginNext = 0;

// .cpp — OnServerMoveProcessed (기존 구독자)
    RecordShotOrigin(ClientTimeStamp, OwnerChar->GetCameraComponent()->GetComponentLocation());   // 매 무브, 동기
    if (bNewMove) { bHasPendingSnapshot = true; ... }                                             // 본 스냅샷은 지금처럼 NewMove만, PostPhysics에서
```
조회는 최신부터 거슬러 `FMath::IsNearlyEqual(TimeStamp, Entry.TimeStamp, KINDA_SMALL_NUMBER)` — 클라가 보낸
float과 `ServerMove`로 온 float이 같은 값이라 정확히 일치한다.

**`AEPCharacter::Input_Fire`** (`EPCharacter.cpp:433-442`) — 태그 활성화에서 **핸들 기반 + 예약 분기**로:
```cpp
    FGameplayAbilitySpec* Spec = FindPrimaryUseSpec();               // GetActivatableAbilities()에서 에셋 태그 TAG_Ability_Item_PrimaryUse로
    if (!Spec) return;
    if (Spec->IsActive()) { ASC->AbilitySpecInputPressed(*Spec); return; }   // 간격 안의 클릭 → 예약
    FScopedServerAbilityRPCBatcher Batcher(ASC, Spec->Handle);       // §4-5 배칭. 없어도 동작은 같다
    ASC->TryActivateAbility(Spec->Handle);
```
`FindAbilitySpecFromClass`는 BP 서브클래스에서 `nullptr`(§1)이므로 태그로 찾는다.

**`Modifier.FireRate` 태그** — `EPNativeGameplayTags`에 `TAG_Modifier_FireRate`(`"Modifier.FireRate"`) 추가.
버프 어빌리티가 `Modifier.FireRate.<Source>`로 `Set/Clear`한다(스킬 문서 §3-3).

**삭제:** `GE_FireCooldown` 에셋, `BP_GA_Item_PrimaryUse`의 `CooldownGameplayEffectClass`,
태그 `Cooldown.Weapon.PrimaryUse`·`State.FireCooldown`(`EPNativeGameplayTags.cpp:12, :33` — 코드 소비자
없음, grep 확인 후), `EPCombatComponent`의 `Server_ConfirmFire`.

### 4-5. 어빌리티 배칭 (독립 단계 — §4-4 동작 확인 뒤)

TargetData 전송으로 바꾸면 `Single` 클릭 하나가 Reliable RPC 3개(`ServerTryActivateAbility`,
`ServerSetReplicatedTargetData`, `ServerEndAbility`)가 된다. `FScopedServerAbilityRPCBatcher`
(`AbilitySystemComponent_Abilities.cpp:4095-4174`)가 스코프 안에서 일어난 활성화+TargetData(+End)를
`ServerAbilityRPCBatch` 하나로 묶는다 — `CallServerSetReplicatedTargetData`는 배치 중이면 RPC 대신
`ExistingBatchData->TargetData`에 담는다.

| 모드 | 배칭 전 | 배칭 후 |
|---|---|---|
| `Single` (예약 슬롯으로 `Interval`까지 생존) | 활성화 + TD + End = **3** | 배치[활성화+TD] + End = **2** — End는 타이머 틱에서 일어나 스코프 밖 |
| `Auto` N발 | 활성화 + TD×N + End = N+2 | 배치[활성화+TD₁] + TD×(N−1) + End = **N+1** |

전제 두 가지:
1. **ASC 서브클래스** `UEPAbilitySystemComponent : UAbilitySystemComponent` —
   `virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }`(기본 `false`,
   `AbilitySystemComponent.h:1305`). `EPPlayerState`(`EPPlayerState.cpp:14`)가 이 클래스로 생성. 횡단
   변경이라 `GAS_STATUS`·`PROJECT_CONTEXT` 갱신 대상.
2. `Input_Fire`가 핸들 기반(§4-4) — `FScopedServerAbilityRPCBatcher`가 핸들을 받는다. GASShooter의
   `BatchRPCTryActivateAbility` 패턴.

독립 단계로 두는 이유: 켜고 끄며 RPC 수를 비교할 수 있어야 한다(§6-10). 문제가 생기면 1번만
되돌리면 동작은 §4-4 그대로다.

### 4-6. 설정

`UEPCombatDeveloperSettings`:
```cpp
    // 서버 발사 검증 토큰 버킷 상한. 정직한 클라의 패킷 몰림 허용치. 1이면 몰림 불허, 2면 두 발까지
    UPROPERTY(Config, EditAnywhere, Category="GAS|FireRate", meta=(ClampMin="1", ClampMax="4"))
    float FireRateBurstAllowance = 2.f;

    // 서버가 무브별 카메라 위치를 기억하는 개수 (발사 원점 동기). 64 ≈ 60fps 1초. RTT가 길면 늘린다
    UPROPERTY(Config, EditAnywhere, Category="LagComp", meta=(ClampMin="16", ClampMax="256"))
    int32 ShotOriginHistoryCount = 64;
```

### 4-7. 대가

- `CheckCooldown()`을 안 타므로 GAS 활성화 실패 로그에 "쿨다운"으로 안 잡힌다. HUD가
  발사 간격을 표시하지 않으므로 문제 아님.
- 서버가 버킷으로 버린 발은 클라에 알리지 않는다. 정직한 클라는 `MaxTokens` 덕에 버려질 일이
  없다.
- 원점은 히스토리 적중 시 오차 0. **미스**(무브 미도착·타임스탬프 리셋 직후)일 때만 현재 위치 폴백 = 이동
  1프레임분. 카메라가 본에 붙어 있으면 양쪽 애니 포즈 차이만큼(수 cm) 더. §6-8에서 적중률과 함께 확인.
- 발당 4B(타임스탬프), 캐릭터당 링버퍼 64 × 16B.
- **남이 건 발사 속도 버프가 끝날 때** — 클라는 D 늦게 알아 그동안 빠른 속도로 더 쏜다. 초과분
  `(빠른 rps − 느린 rps) × D`. 2배·20rps·D 50ms = 1발, `MaxTokens=2` 안. 배율이 크거나 핑이 높으면
  버프 종료 직후 한두 발이 거절된다 — 자기 버프(`Modifier.FireRate`)는 양쪽이 같은 어빌리티로
  동시에 끄므로 해당 없음.
- `Single` 클릭당 Reliable RPC 2개(§4-5). 대역폭상 무시 가능 — STATUS §2 09-20.
- 탄약 예측의 한 프레임 흔들림 가능성 — 서버 복제값(속성)과 키 ack(`ReplicatedPredictionKeyMap`)는 같은 PlayerState 채널로 같은 업데이트에 오지만, 적용 순서에 따라 한 프레임 안에 값이 두 번 바뀔 수 있다(예측 GE 제거 → 복제 base 적용). HUD 델리게이트가 두 번 불려도 마지막 값이 맞다. §6-12에서 확인.

### 4-8. 범위 밖 — RPC 플러딩 방어 (구현서 대상 아님)

토큰 버킷은 **게임 상태**(발사 판정)만 지킨다. 로컬 게이트를 건너뛰는 클라(커스텀
네트워크 클라이언트 — 메모리 인젝션 없이 프로토콜만 흉내내는 부류, 안티치트가 못
잡는 영역)가 `ServerTryActivateAbility`를 간격 무시하고 스팸하면, 원격 클라 인스턴스의
`CanActivateAbility`는 간격을 안 보므로(§4-4) 매번 활성화가 통과되고 `EndAbility`까지
도는 비용이 그대로 남는다 — 발사 자체는 막히지만 RPC 처리 비용/트래픽은 안 막힌다.
다른 층의 문제라 §4 스코프 밖:

- **`CanActivateAbility`에 소비 없는 피크 추가** — `FireLimiter`에 `HasTokenAvailable(Now, Refill)`을
  두고 원격 클라 인스턴스의 `CanActivateAbility`에서 먼저 걸러 활성화 자체를 거절 —
  `EndAbility` 사이클을 아예 안 돌게 한다. `TryTake()`를 그 자리에 직접 못 쓰는 이유는 소비
  부작용이 있어서다(활성화 단계에서 한 번, `ServerConfirmOneShot`에서 또 한 번 깎는 이중
  소비 — 순수 조회와 소비를 같은 메서드로 섞지 않는다).
  **주의:** 저장된 `Tokens`는 마지막 `TryTake` 시점 값이라 그대로 비교하면 오래 쉰 정직한
  클라를 거절한다. 피크도 **리필을 계산은 하되 저장하지 않는다**:
  ```cpp
  bool HasTokenAvailable(float Now, float RefillPerSecond) const
  {
      return FMath::Min(MaxTokens, Tokens + (Now - LastUpdate) * RefillPerSecond) >= 1.f;
  }
  ```
- **엔진 레벨 — `FRPCDoSDetection`**(`Engine/Public/Net/RPCDoSDetection.h`, UE5.7 확인).
  커넥션 단위로 RPC 호출 횟수/시간 예산을 추적해 단계적으로 심각도를 올리고(`ERPCDoSSeverityUpdate::Escalate`, `:382`)
  최종적으로 플레이어를 킥하는(`bKickPlayer`, `:184`), GAS와 무관한 순수 네트워크 레이어 방어 —
  특정 어빌리티가 아니라 모든 RPC가 대상. **기본은 꺼져 있다** — `BaseEngine.ini:1865`
  `[GameNetDriver RPCDoSDetection]` `bRPCDoSDetection=false`. 단계(`Normal → Hitch → Burst →
  PersistentBurst → DoS → ExpensiveDoS → Kick`)와 allowlist(`ServerMove` 등 이동 RPC 제외)는
  이미 잡혀 있어 `DefaultEngine.ini`에 같은 섹션으로 `bRPCDoSDetection=true` 한 줄이면 켜진다.

이 프로젝트 스코프에선 미적용 — 필요해지면 여기 추가.

---

## 5. 재장전 태그 — `GE_Reloading` → `ActivationOwnedTags`

> `04_Polish_SkillDisplay.md` §3-0 태그 표에서 인계. `Casting`과 같은 구조.

**지금:** `EPGA_Item_Reload::ActivateAbility`가 `GE_Reloading`(`State.Reloading` 부여)을 적용하고,
`EndAbility`가 **서버에서만** 핸들로 제거한다(`:65-68`). 클라는 리플리케이션이 올 때까지
`Reloading` 상태 → 재장전 끝나고 ~D 동안 `PrimaryUse`가 `ActivationBlockedTags`에 막히고,
`EPHUDWidget`의 재장전 표시도 그만큼 늦게 꺼진다.

**변경:**
```cpp
// EPGA_Item_Reload 생성자
ActivationOwnedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);   // 재장전 어빌리티가 재장전 내내 활성 → 수명이 정확히 일치
```
- `ActivateAbility`의 `GE_ReloadingClass` 적용 블록 **삭제**, `EndAbility`의 핸들 제거 블록 **삭제**
  (`Super::EndAbility`가 뗀다), `GE_ReloadingClass`·`ReloadingEffectHandle` 필드 삭제,
  `GE_Reloading` 에셋 삭제. `Data.ReloadDuration` 태그는 소비자가 없어지지만 그대로 둔다.
- `EPHUDWidget`·`PrimaryUse`의 `ActivationBlockedTags`는 **변경 없음** — 태그 카운터 맵을 볼 뿐.
- `GE_ReloadAmmoClass`(탄약 채우기, Instant, 서버만)는 그대로. 재장전 *효과*는 남이 건 게
  아니라 내가 시작한 것이지만 Instant라 §3-0에 안 걸린다.

**함정 — 재장전 직후 첫 발.** 클라는 자기 재장전이 끝나는 순간 `Reloading`이 빠져 바로
쏠 수 있는데, 서버의 재장전은 U 늦게 끝난다. 발사 RPC도 U 늦게 도착하니 보통은 맞물리지만
지터로 서버 재장전 종료 **전에** 도착하면 서버 `CanActivateAbility`가 태그에 막아 거절 →
클라 총구 이펙트 한 번 헛발. `Auto`는 이미 활성이라 무관, `Single` 첫 발만 드물게. 수용 —
`ActivationBlockedTags`엔 허용 오차 개념이 없고, 이 한 발을 위해 만들 가치가 없다.

---

## 6. PIE 검증

`NetEmulation.PktLag 100`, 원격 클라:

| # | 확인 | 통과 기준 |
|---|---|---|
| 1 | `Single` 연타 (FireRate 4 무기) | 클릭마다 정확히 0.25s 간격으로 나감. `ClientActivateAbilityFailed` 로그 0 |
| 2 | `Auto` 3초 사격 | 서버 `HandleServerFire` 호출 수 = 클라 `FireOnce` 수. `FireLimiter` 거부 로그 0 |
| 3 | `Auto` + `NetEmulation.PktLoss 10` | 거부 로그 0 (Reliable 재전송으로 몰린 발은 `MaxTokens`가 흡수). 서버 발사 수 = 클라 발사 수 |
| 4 | 조작 시뮬 — `GetBaseFireRate()`에 임시 `× 2` | 서버 거부 로그가 절반. 서버 발사 수 = `FireRate × 시간` |
| 5 | 재장전 직후 `Single` 첫 발 ×10 | 헛발(거절) ≤ 1회 |
| 6 | 재장전 HUD | 재장전 끝과 동시에 표시 꺼짐 (전엔 ~D 늦음) |
| 7 | 호스트 회귀 | 위 전부 리슨 서버에서 정상 |
| 8 | 원점 일치 — 클라 `FireOnce`에서 카메라 위치 로그, 서버 `HandleServerFire`에서 원점 + 히스토리 적중 여부 로그 | 정지·걷는 중 모두 ≤ 포즈 차(수 cm). 적중률 ≥ 95% (`PktLag 100`). 미스 발만 ≤ 이동 1프레임분 |
| 9 | 단발 예약 — `Single` 간격 안 더블클릭 | 정확히 `Interval` 간격으로 2발. 3연타는 2발(한 칸만 예약) |
| 10 | 발사 속도 배율 — 임시로 `Modifier.FireRate.Test = 2` `Set` | `Auto` 간격 절반, 서버 거부 0. `Clear` 후 원래대로. 배율 변경이 진행 중 간격엔 반영 안 됨(다음 발부터) |
| 11 | 배칭 전후 RPC 수 — `ShouldDoServerAbilityRPCBatch` false/true, `log LogAbilitySystem verbose` | `Single` 클릭당 3 → 2, `Auto` N발 N+2 → N+1 |
| 12 | 탄약 예측 — `PktLag 200`에서 `Auto` 사격, HUD 잔탄 | 클릭 즉시 −1 (전엔 RTT 뒤). 흔들림(−2 → −1) 없음. 조작 시뮬(4번 방식)로 거절된 발은 잔탄이 다시 +1 |

검증용 임시 로그는 `ServerConfirmOneShot`의 `TryTake` 실패 분기 하나면 된다.

---

## 7. 함께 볼 것

- `AEPWeapon::CalculateSpread()`에 디버그 `UE_LOG(LogTemp, Log, TEXT("%.3f"), Spread)`가
  남아 있다. 발사마다 서버에서 돈다 — §4 작업 때 같이 지운다.
- `AEPWeapon::ApplySpread()`는 호출자 없는 죽은 코드 — 같이 지운다.
- `Issue/FireRate_GECooldownPrediction.md`의 "Ability Batching" 절은 이름이 틀렸다(타이머 루프
  재설계를 그렇게 불렀음). §4 적용 후 그 문서에 "해결됨" 표시하면서 같이 정정 — 진짜 배칭은 §4-5.
- `Public/Core/EPLocalModifiers.h:4` `#include "AnimationEditorTypes.h"` — Persona(에디터 모듈) 헤더.
  `Build.cs`에 없고 런타임에 필요 없다. 패키징 빌드에서 깨진다 — 지운다.
- `DOCS/Mine/LagCompensationFix.md`·포트폴리오의 "클라가 원점·방향을 보낸다" 서술은 §4 적용 후
  "방향만 보낸다"로 갱신 대상.
