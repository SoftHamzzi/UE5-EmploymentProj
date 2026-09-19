# Polish — 무기 발사 속도 / 재장전 태그 / 탄약 동기화

**상태(2026-09-18):** §1~§2는 코드에 적용돼 있다. **§4(발사 간격 로컬 타이머)와 §5(재장전
태그)는 미구현** — 스킬 쪽(`04_Polish_SkillDisplay.md`) 구현이 끝난 뒤 같은 부품으로 한다.
`Entry.State.Charges` 이관은 Loot Step 05로 미룬 범위 밖 항목.

관련 소스: `EPGA_Item_PrimaryUse.h/.cpp`, `EPGA_Item_Reload.h/.cpp`, `EPCombatComponent.cpp`,
`EPWeapon.h/.cpp`, `GE_FireCooldown`·`GE_Reloading`(에셋).
원칙과 부품(`FEPLocalTimer`, `ActivationOwnedTags`, 서버 허용 오차, 로컬 시계)은
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
직접, 또는 `Server_ConfirmFire_Implementation`(Unreliable)이 원격 클라의 RPC를 받아서.
서버는 `FGameplayAbilitySpecHandle`로 정확히 그 인스턴스를 찾는다 — `FindAbilitySpecFromClass`는
BP 서브클래스에서 완전 일치 실패로 항상 `nullptr`이었다(실측).

**구현 노트:**
- `CommitAbility()` 대신 `CommitAbilityCost` + `CommitAbilityCooldown(ForceCooldown=true)` —
  `CheckCooldown()`이 직전 발의 쿨다운에 이번 발을 스스로 걸리게 만들었다. (§4 뒤엔 둘 다 안 씀)
- `bServerRespectsRemoteAbilityCancellation = true` — `false`면 `Input_StopFire`가 클라
  인스턴스만 멈추고 서버는 탄창이 빌 때까지 계속 쏜다. **유지.**

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
- **클라 탄약 예측** — `ServerConfirmOneShot`이 `CommitAbilityCost`를 서버에서만 부르므로
  HUD 잔탄이 RTT 늦게 준다. 탄약은 "내가 시작한 시간 상태"가 아니라 즉시 값이므로
  §4와 무관 — Instant GE 예측으로 따로 풀 수 있지만 지금은 안 한다.
- **`Auto`의 서버 쪽 U** — 서버는 클라 첫 발보다 U 늦게 시작하고 U 늦게 끝난다. 발사
  판정 자체는 SSR이 되감으니 무관. 이동처럼 정정이 생기는 것도 없다.

---

## 4. 미해결 → 설계 — 발사 간격을 GE에서 뺀다

> 2026-09-05 증상 확인·원인 규명. 2026-09-18 스킬 쪽 부품 확정 후 설계 갱신. 코드 미적용.

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
든다. 필요한 건 둘 — `Auto`의 **루프 페이싱**(이미 `SetTimer`가 함), `Single`의 **연타 제한**
(로컬 게이트) — 그리고 어느 쪽이든 **서버 검증**. 복제도 예측도 필요 없다.

### 4-2. 구조

```
                         클라 (오너)                              서버
                ┌──────────────────────────┐          ┌──────────────────────────────┐
 Single 클릭 →  │ CanActivateAbility        │          │ CanActivateAbility            │
                │   Weapon->FireTimer 검사  │ ──RPC──▶ │   (간격 검사 안 함, 태그만)   │
                │ FireOnce                  │          │                              │
 Auto 타이머 →  │   FireTimer.Start(Now,    │ ──RPC──▶ │ ServerConfirmOneShot          │
                │     Interval)             │ Unrel.   │   Weapon->FireLimiter.TryTake │ ← 유일한 서버 검증
                │   PlayLocalMuzzleEffect   │          │   ? 발사 : 버림               │
                └──────────────────────────┘          └──────────────────────────────┘
```

**두 부품, 위치는 둘 다 `AEPWeapon` (무기 인스턴스).** Lyra가 `TimeLastFired`를
`ULyraWeaponInstance`에 두는 것과 같다. 어빌리티는 InstancedPerActor라 무기를 바꿔도
인스턴스가 하나 — 어빌리티에 두면 교체 직후 간격이 이전 무기에 묶인다.

| 부품 | 쪽 | 타입 | 역할 |
|---|---|---|---|
| `FireTimer` | 클라(오너)·호스트 | `FEPLocalTimer` (스킬과 같은 것) | `Single` 연타 제한. `Auto`에선 `SetTimer`가 페이싱하므로 검사는 형식적 |
| `FireLimiter` | 서버 | `FEPRateLimiter` (신규, 아래) | 서버 검증. **타이머가 아니라 토큰 버킷** |

### 4-3. 왜 서버는 타이머가 아니라 토큰 버킷인가

스킬은 `Cooldown − Tolerance`로 서버가 검사했다. 무기에 그대로 쓰면 두 가지가 깨진다:

1. **비율 문제** — 간격 0.1s에 허용 오차 0.1s면 검사가 없는 것과 같다. 간격에 비례시키면
   (`Interval × 0.5`) 조작 클라가 그 비율만큼 **지속적으로** 빠르게 쏠 수 있다.
2. **패킷 로스 순서 보장** (`04_Polish_SkillDisplay.md` §3-9에서 인계) — 두 RPC가 재전송
   뒤 **같이 도착**하면 타이머는 둘째를 무조건 버린다. `Server_ConfirmFire`는 Unreliable이라
   재전송은 없지만, 같은 프레임에 두 패킷이 몰려 처리되는 건 흔하다.

토큰 버킷은 둘 다 푼다 — **순간 몰림은 허용하되 지속 속도는 정확히 `FireRate`로 상한**:

```cpp
// Public/Combat/EPRateLimiter.h — 헤더 전용, 복제 안 함
struct FEPRateLimiter
{
    void  Reset(float Now, float InRefillPerSecond, float InMaxTokens)
    { RefillPerSecond = InRefillPerSecond; MaxTokens = InMaxTokens; Tokens = InMaxTokens; LastUpdate = Now; }

    /** 토큰이 1 이상이면 하나 쓰고 true. 아니면 false — 이번 요청을 버린다 */
    bool  TryTake(float Now)
    {
        Tokens = FMath::Min(MaxTokens, Tokens + (Now - LastUpdate) * RefillPerSecond);
        LastUpdate = Now;
        if (Tokens < 1.f) return false;
        Tokens -= 1.f;
        return true;
    }
private:
    float Tokens = 0.f, MaxTokens = 1.f, RefillPerSecond = 0.f, LastUpdate = 0.f;
};
```

- `RefillPerSecond = FireRate`, `MaxTokens = FireRateBurstAllowance`(설정, 기본 **2**).
- 정직한 클라: 간격마다 1발 → 토큰이 1씩 차고 1씩 빠져 항상 통과. 두 발이 몰려 와도
  2까지는 통과. 세 발이 몰리면 셋째를 버린다 — 그 정도 몰림은 정상 네트워크에서 안 난다.
- 조작 클라: 초당 `FireRate`를 넘는 만큼은 **무조건** 버려진다. 처음 `MaxTokens`발만 앞당길
  수 있고 그 뒤론 정확히 `FireRate`. 상한이 명시적이다(스킬의 `Tolerance`와 같은 성질).
- 시계는 `World->GetTimeSeconds()` — 서버 자기 것끼리만 뺀다.

### 4-4. 코드 변경점

**`AEPWeapon`**
```cpp
// EPWeapon.h
#include "GAS/EPLocalTimer.h"
#include "Combat/EPRateLimiter.h"
public:
    FEPLocalTimer&   GetFireTimer()   { return FireTimer; }
    FEPRateLimiter&  GetFireLimiter() { return FireLimiter; }
    float GetFireInterval() const { return WeaponDef ? 1.f / WeaponDef->FireRate : 0.2f; }   // PrimaryUse의 static과 통합
private:
    FEPLocalTimer  FireTimer;      // 오너 클라·호스트의 연타 제한
    FEPRateLimiter FireLimiter;    // 서버 검증
```
`BeginPlay`(서버)에서 `FireLimiter.Reset(Now, WeaponDef->FireRate, Settings->FireRateBurstAllowance)`.
`WeaponDef`가 런타임에 바뀌지 않으므로 한 번이면 된다.

**`UEPGA_Item_PrimaryUse`**
- `ApplyCooldown` 오버라이드 **삭제**, `FireOnce`의 `CommitAbilityCooldown` **삭제**
- `CanActivateAbility` 오버라이드 추가 — **로컬 컨트롤 쪽만** 간격 검사:
```cpp
bool UEPGA_Item_PrimaryUse::CanActivateAbility(...) const
{
    if (!Super::CanActivateAbility(...)) return false;               // Dead/Reloading 태그
    if (!ActorInfo->IsLocallyControlled()) return true;               // 서버의 원격 클라 인스턴스: 간격은 ServerConfirmOneShot이 본다
    const AEPWeapon* Weapon = ...GetEquippedWeapon();
    if (!Weapon) return false;
    const float Now = Weapon->GetWorld()->GetTimeSeconds();
    return Weapon->GetFireTimer().IsElapsed(Now, 1.f, Weapon->GetFireInterval() * 0.01f);   // 부동소수점 여유 1%
}
```
- `FireOnce`에서 `CommitAbilityCooldown` 자리에:
```cpp
    if (CurrentActorInfo->IsLocallyControlled())
        Weapon->GetFireTimer().Start(GetWorld()->GetTimeSeconds(), Weapon->GetFireInterval());
```
  (`IsLocallyControlled` 가드 뒤로 옮겨도 된다 — 어차피 그 아래는 로컬만 간다.)
- `ServerConfirmOneShot` 첫 줄:
```cpp
    if (!Weapon->GetFireLimiter().TryTake(GetWorld()->GetTimeSeconds())) return true;   // 너무 빠름 → 이 발만 버림. 어빌리티는 안 끝냄
```
  반환 `true`인 이유: `false`는 호스트 경로에서 `EndAbility`를 부른다(탄약 소진용). 속도 초과는
  한 발 버리는 것이지 사격 종료가 아니다.

**`Auto` 타이머와의 관계.** `SetTimer(Interval)`가 `FireOnce`를 부르면 그 안에서 `FireTimer.Start`.
`CanActivateAbility`는 첫 발에만 도니 타이머 발은 검사 없이 나간다 — 맞다, 페이싱은 타이머가
한다. `FireTimer`가 `Auto`에서 하는 일은 "사격 중단 직후 `Single`로 바꿔 클릭" 같은 경계
케이스뿐이다.

**롤백.** `Single`의 서버 거절(`CanActivateAbility` 태그 실패)은 `ClientActivateAbilityFailed`로
오지만 클라가 잃는 건 총구 이펙트 한 번뿐 — `FireTimer`를 되돌릴 필요가 없다(다음 클릭이
`Interval` 뒤라는 제한은 거절과 무관하게 맞다). 스킬의 `NewRejectedDelegate` 훅은 **안 쓴다.**

**삭제:** `GE_FireCooldown` 에셋, `BP_GA_Item_PrimaryUse`의 `CooldownGameplayEffectClass`,
태그 `Cooldown.Weapon.PrimaryUse`·`State.FireCooldown`(둘 다 코드 소비자 없음 — grep 확인 후).

### 4-5. 설정

`UEPCombatDeveloperSettings`:
```cpp
    // 서버 발사 검증 토큰 버킷 상한. 정직한 클라의 패킷 몰림 허용치. 1이면 몰림 불허, 2면 두 발까지
    UPROPERTY(Config, EditAnywhere, Category="GAS|FireRate", meta=(ClampMin="1", ClampMax="4"))
    float FireRateBurstAllowance = 2.f;
```

### 4-6. 대가

- `CheckCooldown()`을 안 타므로 GAS 활성화 실패 로그에 "쿨다운"으로 안 잡힌다. HUD가
  발사 간격을 표시하지 않으므로 문제 아님.
- 서버가 버린 발은 클라에 알리지 않는다(Unreliable이라 원래 유실과 구분 안 됨). 정직한
  클라는 `MaxTokens` 덕에 버려질 일이 없다.

### 4-7. 범위 밖 — RPC 플러딩 방어

토큰 버킷은 **게임 상태**(발사 판정)만 지킨다. 로컬 게이트를 건너뛰는 클라(커스텀
네트워크 클라이언트 — 메모리 인젝션 없이 프로토콜만 흉내내는 부류, 안티치트가 못
잡는 영역)가 `ServerTryActivateAbility`를 간격 무시하고 스팸하면, 원격 클라 인스턴스의
`CanActivateAbility`는 간격을 안 보므로(§4-4) 매번 활성화가 통과되고 `EndAbility`까지
도는 비용이 그대로 남는다 — 발사 자체는 막히지만 RPC 처리 비용/트래픽은 안 막힌다.
다른 층의 문제라 §4 스코프 밖:

- **`CanActivateAbility`에 소비 없는 피크 추가** — `FireLimiter`에 `HasTokenAvailable()`
  (토큰을 안 깎고 `Tokens >= 1`만 보는 읽기 전용 함수)를 두고 원격 클라 인스턴스의
  `CanActivateAbility`에서 먼저 걸러 활성화 자체를 거절 — `EndAbility` 사이클을 아예
  안 돌게 한다. `TryTake()`를 그 자리에 직접 못 쓰는 이유는 소비 부작용이 있어서다
  (활성화 단계에서 한 번, `ServerConfirmOneShot`에서 또 한 번 깎는 이중 소비가 됨 —
  순수 조회와 소비를 같은 메서드로 섞지 않는다).
- **엔진 레벨 — `FRPCDoSDetection`**(`Engine/Public/Net/RPCDoSDetection.h`, UE5.7 확인).
  커넥션 단위로 RPC 호출 횟수/시간 예산을 추적해 단계적으로 심각도를 올리고(`ERPCDoSSeverityUpdate::Escalate`)
  최종적으로 플레이어를 킥하는(`bKickPlayer`), GAS와 무관한 순수 네트워크 레이어 방어 —
  특정 어빌리티가 아니라 모든 RPC가 대상. 설정만 잡으면 되는 엔진 기본 기능이라
  구현 비용이 거의 없다.

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
| 3 | `Auto` + `NetEmulation.PktLoss 10` | 거부 로그 여전히 0 (몰림은 `MaxTokens`가 흡수). 유실된 발만 빠짐 |
| 4 | 조작 시뮬 — `GetFireInterval() * 0.5f`로 임시 변경 | 서버 거부 로그가 절반. 서버 발사 수 = `FireRate × 시간` |
| 5 | 재장전 직후 `Single` 첫 발 ×10 | 헛발(거절) ≤ 1회 |
| 6 | 재장전 HUD | 재장전 끝과 동시에 표시 꺼짐 (전엔 ~D 늦음) |
| 7 | 호스트 회귀 | 위 전부 리슨 서버에서 정상 |

검증용 임시 로그는 `ServerConfirmOneShot`의 `TryTake` 실패 분기 하나면 된다.

---

## 7. 함께 볼 것

- `AEPWeapon::CalculateSpread()`에 디버그 `UE_LOG(LogTemp, Log, TEXT("%.3f"), Spread)`가
  남아 있다. 발사마다 서버에서 돈다 — §4 작업 때 같이 지운다.
- `AEPWeapon::ApplySpread()`는 호출자 없는 죽은 코드 — 같이 지운다.
- `Issue/FireRate_GECooldownPrediction.md`의 "Ability Batching" 절은 이름이 틀렸다(타이머 루프
  재설계를 그렇게 불렀음). §4 적용 후 그 문서에 "해결됨" 표시하면서 같이 정정.
