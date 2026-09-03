# 지연 보상 복구 — 서버 RTT 예측 (UT 참고, RTT/2 → RTT 전체로 정정)

**목적:** `DOCS/Mine/ServerSideRewind.md` §4 **C1**에서 확인한 회귀
("GAS 전환에서 `ClientFireTime`이 유실돼 지연 보상이 사실상 꺼져 있다")를 고친다.

**방향 결정 (2026-09-01, 두 번째 뒤집힘):**

1. 처음엔 "서버가 RTT만으로 계산한다"(순수 (b))
2. → "클라이언트가 발사 시각을 보내고 서버가 RTT 기대치로 검증한다"(하이브리드)로 바꿈
3. → **다시 순수 (b)로 되돌아왔다.** 이번엔 `C:\Github\unrealTournament`(Epic의 공식 UT 소스)를
   직접 읽고 확인한 뒤 내린 결정이다 — 실제 출시작이 이미 이 길을 갔고, 이유가 타당했다.

**왜 하이브리드를 버렸나 (실측 근거):** 하이브리드안을 실제로 구현하고 테스트하는 과정에서
다음 문제가 전부 `ClientFireTime`이 클라이언트의 `GetServerWorldTimeSeconds()`에서 나온다는
사실 하나에서 파생됐다 —

- `ExpectedDelay`(RTT/2)와 `ClaimedDelay`(≈RTT 전체)의 스케일이 애초에 안 맞았다
  (`*0.5` 스케일 버그, `GetServerWorldTimeSeconds.md` 참고)
- 로그 두 곳에 같은 계산이 중복돼 있어 한쪽만 고치는 실수가 났다
- `Tolerance` 하나로 좋은 네트워크와 나쁜 네트워크를 동시에 만족시킬 수 없었다
  (좁히면 나쁜 네트워크에서 정직한 클라이언트도 잘리고, 넓히면 검증이 무의미해짐)
- `ini`에 남은 실험값(`FireTimeToleranceSeconds=0.7`)이 검증을 사실상 꺼버린 채 방치됐다
- 그런데도 나쁜 네트워크에서 총알이 계속 빗나갔다 — 검증 클램프가 개입 안 하는 상황에서도
  틀렸다는 뜻이라, **재료(`ClientFireTime`) 자체가 나쁜 네트워크에서 불안정**했던 것

UT는 이 재료를 아예 안 쓴다. 클라이언트가 발사 시각을 보고하지 않고, 서버가 자기가 이미
알고 있는 핑(`PlayerState->ExactPing`)만으로 "얼마나 되돌릴지"를 계산한다. 이 문서는 그
구조로 다시 쓴 것이다.

**참고 소스:** `C:\Github\unrealTournament\UnrealTournament\Source\UnrealTournament`
(`UTPlayerController.cpp:281-285`, `UTCharacter.cpp:414-435`, `UTWeapon.cpp:1604-1762`).
엔진 인용은 UE 5.7 (`C:\Program Files\Epic Games\UE_5.7\Engine`) 직독.
**코드는 이 문서를 보고 사용자가 직접 작성한다** (`CLAUDE.md` 워크플로).

---

## 1. 무엇이 고장났는가 (배경, 변경 없음)

`EPGA_Item_PrimaryUse.cpp:49-58`, 서버 브랜치:

```cpp
const float ClientTime = GS ? GS->GetServerWorldTimeSeconds() : Char->GetWorld()->GetTimeSeconds();
...
if (ActorInfo->IsNetAuthority())
{
    Combat->HandleServerFire(Origin, Char->GetControlRotation().Vector(), ClientTime);
}
```

이름은 `ClientTime`인데 값은 서버가 그 자리에서 읽은 자기 시계다. `Server_Fire` RPC가
GAS 전환 때 사라지면서 클라이언트 발사 시각을 서버로 나르던 유일한 경로가 없어졌고,
서버 브랜치가 자기 시계로 그 빈자리를 채우고 있다. 결과: `ServerNow - ClientFireTime ≈ 0`,
되돌리는 양이 한 프레임 이하.

**이번 수정은 이 빈자리를 "클라이언트 시각을 복원"해서 채우지 않는다.** 애초에 그 자리에
클라이언트 시각이 필요 없는 구조로 바꾼다.

## 2. 새 설계: 서버가 자기 핑만으로 계산한다

```
PredictionDelay(Shooter) = Clamp(
    Shooter->GetPlayerState()->GetPingInMilliseconds() * 0.001 - PredictionFudgeSeconds,
    0, MaxRewindSeconds)
```

**`RTT/2`가 아니라 `RTT` 전체를 쓴다 (2026-09-01, 실측으로 정정).** 처음엔
`UTPlayerController::GetPredictionTime()`(`UTPlayerController.cpp:281-285`)을 그대로 따라
`RTT/2`로 시작했다:

```cpp
// UT 원본 — RTT/2
float AUTPlayerController::GetPredictionTime()
{
    return (PlayerState && (GetNetMode() != NM_Standalone))
        ? (0.0005f * FMath::Clamp(PlayerState->ExactPing - PredictionFudgeFactor, 0.f, MaxPredictionPing))
        : 0.f;
}
```

그런데 실제 200/200 PktLag 테스트에서 **RTT/2로는 보상이 절반밖에 안 됐다** — 표적이 화면에
보이는 위치보다 캐릭터 너비만큼 앞을 쏴야 맞았다. 원인을 유도해보면:

```
사수가 절대시각 A에 발사
사수 화면이 보여준 표적 상태 = A - D(다운링크) - InterpDelay 시점의 참값
발사 명령은 U(업링크)를 거쳐 ServerNow = A + U에 서버 도착

RewindTime = A - D - InterpDelay
           = (ServerNow - U) - D - InterpDelay
           = ServerNow - (U + D) - InterpDelay
           = ServerNow - RTT - InterpDelay     ← RTT 전체, RTT/2가 아니다
```

`U`(발사 명령의 사수→서버 편도)와 `D`(표적 위치가 사수 화면에 뜨기까지의 서버→사수 편도)는
**서로 다른 구간**이고, 둘 다 되돌려야 한다. `RTT/2`만 쓰면 이 중 한쪽만 보상하는 셈이라
정확히 절반 부족하게 되돌아간다 — 실측(리드해야 하는 양이 지금 보정량과 비슷한 규모)과
정확히 일치한다.

**UT는 왜 RTT/2만 쓰는가:** 확실치 않다. UT 자체의 밸런스 선택(사수에게 완전 보상을 주지
않아 "코너 뒤에서도 맞는" 체감을 줄이는 의도적 언더컴펜세이션)이었을 가능성이 있다 — 참고
사례로는 여전히 유효하지만, 이 프로젝트는 "사수가 화면에서 본 대로 맞는다"가 목표이므로
실측을 따라 RTT 전체로 간다.

클라이언트는 이 계산에 전혀 관여하지 않는다 — **서버가 이미 알고 있는 값(그 사수의 핑)만으로 결정**한다.

**`PredictionFudgeSeconds`가 왜 있는가:** RTT/2는 "왕복이 정확히 대칭"이라는 가정 위의
근사치다. 이 여유값은 그 근사가 살짝 어긋나는 쪽(서버 프레임 시간, RTT 비대칭 등)을
안전 방향으로 깎아내리는 고정 상수다. `Tolerance`처럼 "두 값을 비교해서 얼마나 봐줄지"가
아니라 **그냥 한 번 빼는 상수**라 튜닝 부담이 훨씬 작다.

## 3. 왜 클라이언트 시계 문제가 통째로 사라지는가

`ClientFireTime`이 없으니 `GetServerWorldTimeSeconds()`의 클라-서버 편향
(`GetServerWorldTimeSeconds.md`가 다룬 그 `D` 편향)이 이 계산에 아예 들어올 자리가 없다.
`PredictionDelay`의 재료는 `GetPingInMilliseconds()`(서버가 직접 측정) 하나뿐이고, 이건
클라이언트가 조작할 수도, 클라이언트 시계 오차가 섞여 들어올 수도 없는 값이다.

**보안 측면도 이득이다.** 하이브리드안은 "클라가 거짓 시각을 보낼 수 있다"는 위협을
`Tolerance`로 막는 구조였다. 이번 설계는 클라가 조작할 수 있는 값 자체가 없으니 그
위협이 성립하지 않는다.

---

## 4. 보간 지연 (2026-09-01, 실측 결과 최종 코드에서 제외)

> **결론만 먼저: 이 절의 `InterpDelay`는 최종 코드에 안 들어간다.** 이론적으로는 타당해
> 보였지만(§4.1-4.2), 200/200 PktLag 실측에서 `TotalDelay = RTT`(InterpDelay 없이)만으로
> 이미 오차가 30~50ms(잡음 수준)까지 좁혀졌고, 여기에 `InterpDelay(0.1초)`를 추가로 더하면
> 오히려 그만큼 과보정됐다(§9 실측 참고).
>
> **업계 사례도 갈린다 (2026-09-02 추가 확인):** Valve의 Source 엔진 공식 렉 보상 공식은
> `Command Execution Time = Current Server Time - Packet Latency - Client View Interpolation`으로,
> `Client View Interpolation`(기본 `cl_interp` 100ms)을 **이론적으로는 명시적으로 더한다** —
> 우리가 §4.1-4.2에서 유도한 것과 같은 항이다. 그런데 **UT의 실제 출시 코드
> (`AUTCharacter::GetRewindLocation`, `UTCharacter.cpp:414-435`)에는 이 항이 아예 없다** —
> `PredictionTime`(RTT/2 - Fudge) 하나만 쓰고, 사수 자신의 화면 렌더링 지연을 보정하는
> 자리가 코드 어디에도 없다. 즉 Source는 이론상 넣고, UT는 실전에서 뺐다 — 업계 표준이
> 하나로 통일돼 있지 않다는 뜻이고, 우리 실측 결과(빼는 쪽)가 근거 없는 우연이 아니라
> **UT라는 실제 선례와 일치하는 선택**이었다는 걸 확인해준다. 아래 이론 자체는 개념
> 설명으로 남겨두지만, **`ComputeRewindTime`에는 반영하지 않는다.** §5·§6.7이 최종 코드다.

RTT/핑 계산만으로는 부족해 보였다. **사수가 화면에서 본 것 자체가 상대의 진짜 위치보다
한 번 더 뒤처져 있다는 게 이론적 근거였다.** 검증 대상이 아니라 서버가 항상 별도로 더하는
상수로 설계했었다 — 아래는 그 이론의 기록이다.

### 4.1 CMC가 하는 일

`UEPCharacterMovement` 생성자(`EPCharacterMovement.cpp:10`)는 스무딩 모드를 지정한다:

```cpp
NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
NetworkMaxSmoothUpdateDistance = 256.f;
NetworkNoSmoothUpdateDistance  = 384.f;
```

`ENetworkSmoothingMode::Exponential`(`EngineTypes.h`, 주석: *"Exponential. Faster as you are
further from target."*)은 시뮬레이션 프록시(다른 클라이언트가 보는 남의 캐릭터)의 **메시만**
부드럽게 따라가게 하고, **캡슐(충돌)은 리플리케이션 값으로 즉시 스냅**시킨다.

`SmoothCorrection` (`CharacterMovementComponent.cpp:8071-8105`):

```cpp
// We shouldn't be running this on a server that is not a listen server.
checkSlow(GetNetMode() != NM_DedicatedServer);
```

**이 함수는 데디케이티드 서버에서 아예 안 돈다.** SSR이 서버에서 찍는 스냅샷
(`SaveHitboxSnapshot`)에는 이 지연이 섞이지 않는다 — 문제는 **사수 클라이언트가 상대를
렌더링할 때** 생긴다.

`SmoothClientPosition_Interpolate`의 Exponential 분기(`CharacterMovementComponent.cpp:8352-8360`):

```cpp
const float SmoothLocationTime = Velocity.IsZero() ? 0.5f*ClientData->SmoothNetUpdateTime : ClientData->SmoothNetUpdateTime;
if (DeltaSeconds < SmoothLocationTime)
{
    ClientData->MeshTranslationOffset = (ClientData->MeshTranslationOffset * (1.f - DeltaSeconds / SmoothLocationTime));
}
```

시상수 `T = SmoothNetUpdateTime = NetworkSimulatedSmoothLocationTime`, 기본값 **0.1초**
(`CharacterMovementComponent.cpp:702`, 헤더 주석: *"How long to take to smoothly interpolate
from the old pawn position on the client to the corrected one sent by the server."*).
`UEPCharacterMovement`는 이 값을 건드리지 않으므로 프로젝트 전체에서 기본값 0.1초가 그대로 쓰인다.

### 4.2 왜 이게 "지연"인가

지수 감쇠 필터가 등속으로 움직이는 목표를 따라갈 때, 정상 상태의 지연은 시상수 `T`에
수렴한다. 즉 **상대가 계속 움직이고 있다면, 사수의 화면에 보이는 메시는 상대의 진짜
위치보다 대략 `T ≈ 0.1초`만큼 뒤에서 따라온다.** 사수는 캡슐이 아니라 눈에 보이는
메시를 조준하므로, 위치를 한 번 더 되돌려야 한다 — 이건 시각(time)이 아니라
위치(location) 축의 문제라 `PredictionDelay`와는 독립적으로 더한다.

---

## 5. 최종 공식 (2026-09-01, InterpDelay 제외 확정)

```
PredictionDelay(Shooter) = Clamp(
                                Shooter->GetPlayerState()->GetPingInMilliseconds()
                                    * 0.001 - PredictionFudgeSeconds,
                                0, MaxRewindSeconds)

RewindTime                = ServerNow - PredictionDelay(Shooter)
```

`InterpDelay`가 빠지면서 `RewindTime`이 **표적(Target)과 무관하게 사수(Shooter) 하나로만
결정된다** — 같은 샷 안에서 후보가 여럿이어도 전부 같은 `RewindTime`을 쓴다는 뜻이다.
`ComputeRewindTime`의 `Target` 파라미터도 이제 안 쓰이므로 같이 지운다(§6.6-6.7).

**참고 (지금 당장 안 함):** `RewindTime`이 후보마다 안 달라지니, 샷당 한 번만 계산해서
`ConfirmHitscan`에서 `GetHitscanCandidates`로 넘기는 식으로 재구성하면 `ComputeRewindTime`
중복 호출(§9 마지막 항목, `ServerSideRewind.md` §A8)을 없앨 수 있다. 8인 규모에서는 여전히
급하지 않아 이번엔 안 건드린다 — 나중에 이 함수를 다시 만질 일이 생기면 같이 정리해도 된다.

`ServerNow`는 계속 `GS->GetServerWorldTimeSeconds()`로 잰다 — UT는 `GetWorld()->GetTimeSeconds()`를
쓰지만, 이건 **UT 자체의 내부 일관성**일 뿐이다. 이 프로젝트는 `SaveHitboxSnapshot`이 이미
`GetServerWorldTimeSeconds()`로 스냅샷 시각을 찍고 있으므로(`MEMORY.md`의 기존 설계 결정,
`EPCharacterMovement::OnMovementUpdated`), `ConfirmHitscan`의 `ServerNow`도 **같은 시계**를
계속 써야 `HitboxHistory`와 시간축이 맞는다. 서버 자신에게는 이 함수가 편향 없이 동작하니
(`GetServerWorldTimeSeconds.md` 참고) 바꿀 이유가 없다.

---

## 6. 구현 지침

### 6.1 `Public/Combat/EPCombatDeveloperSettings.h`

`FireTimeToleranceSeconds`를 지우고 `PredictionFudgeSeconds`로 교체한다:

```cpp
UPROPERTY(Config, EditAnywhere, Category="LagComp")
float MaxRewindSeconds = 0.5f;

// 제거: float FireTimeToleranceSeconds = 0.1f;

UPROPERTY(Config, EditAnywhere, Category="LagComp")
float PredictionFudgeSeconds = 0.02f;   // UT의 PredictionFudgeFactor(20ms)와 같은 역할
```

**`DefaultGame.ini`도 같이 정리한다.** 지금 `FireTimeToleranceSeconds=0.700000`이 남아
있는데(이번 세션에서 검증이 무력화된 원인이었던 그 값), 필드 자체를 지웠으니 이 줄을
지운다. 필요하면 새 키로 `PredictionFudgeSeconds=...`를 넣는다.

### 6.2 `Public/Combat/EPWeapon.h` / `Private/Combat/EPWeapon.cpp`

`Fire()`의 `ClientFireTime` 파라미터는 원래도 함수 본문에서 안 쓰였다
(`ServerSideRewind.md` C6). 이번에 호출부에서 넘길 값 자체가 없어지므로 지운다:

```cpp
// 이전
void Fire(const FVector& AimDir, float ClientFireTime, TArray<FVector>& OutPellets);
// 이후
void Fire(const FVector& AimDir, TArray<FVector>& OutPellets);
```

`.cpp` 쪽 정의도 시그니처만 맞추면 된다 — 본문은 원래 `ClientFireTime`을 안 썼으니 그대로.

### 6.3 `Public/Combat/EPCombatComponent.h`

세 곳에서 `ClientFireTime` 파라미터를 지운다:

```cpp
public:
    void HandleServerFire(const FVector& Origin, const FVector& Direction);   // ClientFireTime 제거

    UFUNCTION(Server, Reliable)
    void Server_ConfirmFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction);   // ClientFireTime 제거

private:
    void HandleHitscanFire(
        AEPCharacter*          Owner,
        const FVector&         Origin,
        const TArray<FVector>& Directions);   // ClientFireTime 제거
```

`Server_ConfirmFire`가 `public:`(`HandleServerFire` 옆)에 있어야 하는 이유는 이전과
동일하다 — 다른 클래스(`UEPGA_Item_PrimaryUse`)가 밖에서 불러야 하므로.

### 6.4 `Private/Combat/EPCombatComponent.cpp`

```cpp
void UEPCombatComponent::Server_ConfirmFire_Implementation(
    FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction)
{
    HandleServerFire(Origin, Direction);
}

void UEPCombatComponent::HandleServerFire(const FVector& Origin, const FVector& Direction)
{
    if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;

    AEPCharacter* Owner = GetOwnerCharacter();
    if (!Owner) return;

    constexpr float MaxOriginDrift = 200.f;
    if (FVector::DistSquared(Origin, Owner->GetActorLocation()) > FMath::Square(MaxOriginDrift))
        return;

    switch (EquippedWeapon->WeaponDef->BallisticType)
    {
    case EEPBallisticType::Hitscan:
    default:
        {
            TArray<FVector> PelletDirs;
            EquippedWeapon->Fire(Direction, PelletDirs);              // ClientFireTime 인자 제거
            HandleHitscanFire(Owner, Origin, PelletDirs);              // ClientFireTime 인자 제거
            break;
        }
    case EEPBallisticType::ProjectileFast:
    case EEPBallisticType::ProjectileSlow:
        {
            FVector SpreadDir = Direction;
            TArray<FVector> DiscardedPellets;
            EquippedWeapon->Fire(SpreadDir, DiscardedPellets);         // ClientFireTime 인자 제거
            HandleProjectileFire(Owner, Origin, SpreadDir);
            break;
        }
    }

    // 발사 이펙트 — 변경 없음
    ...
}
```

`HandleHitscanFire` 본문도 `ClientFireTime`을 넘기던 자리만 지운다:

```cpp
void UEPCombatComponent::HandleHitscanFire(
    AEPCharacter* Owner, const FVector& Origin, const TArray<FVector>& Directions)
{
    if (!Owner || !Owner->GetServerSideRewindComponent()) return;

    TArray<FHitResult> ConfirmedHits;
    Owner->GetServerSideRewindComponent()->ConfirmHitscan(Owner, EquippedWeapon, Origin, Directions, ConfirmedHits);
    // 이하 데미지 적용 로직은 변경 없음
    ...
}
```

### 6.5 `Private/GAS/EPGA_Item_PrimaryUse.cpp`

클라이언트 브랜치가 훨씬 단순해진다 — `GS`/`ClientTime` 계산 자체가 필요 없다:

```cpp
// 이전:
// const FVector Origin = ...;
// const AGameStateBase* GS = ...;
// const float ClientTime = GS ? GS->GetServerWorldTimeSeconds() : ...;
// Combat->Server_ConfirmFire(Origin, Char->GetControlRotation().Vector(), ClientTime);

// 이후:
if (!ActorInfo->IsNetAuthority())
{
    const FVector Origin = Char->GetCameraComponent()->GetComponentLocation();

    UEPCombatComponent* Combat = Char->GetCombatComponent();
    if (Combat)
    {
        Combat->Server_ConfirmFire(Origin, Char->GetControlRotation().Vector());

        Combat->PlayLocalMuzzleEffect(Origin);
        if (Weapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
            Combat->SpawnLocalCosmeticProjectile(Origin, Char->GetControlRotation().Vector());
    }
}
```

`if (ActorInfo->IsNetAuthority()) { ... }` 블록은 여전히 통째로 삭제 상태로 둔다 — 발사
확정은 `Server_ConfirmFire` RPC가 도착했을 때 처리된다. (데디케이티드 서버 + 원격
클라이언트 전제는 이전과 동일 — `LagCompensationFix.md` 이전 판 §5.4의 전제 문단 그대로 유효.)

### 6.6 `Public/Combat/EPServerSideRewindComponent.h`

세 함수 모두 `ClientFireTime`을 지운다. `ServerNow`는 그대로 유지한다
(`ConfirmHitscan`이 한 번 잰 값을 아래로 전달해서 `GetServerWorldTimeSeconds()` 중복 호출을 피함):

```cpp
public:
    bool ConfirmHitscan(
        AEPCharacter* Shooter,
        AEPWeapon* EquippedWeapon,
        const FVector& Origin,
        const TArray<FVector>& Directions,
        TArray<FHitResult>& OutConfirmedHits);          // ClientFireTime 제거

private:
    TArray<AEPCharacter*> GetHitscanCandidates(
        AEPCharacter* Shooter,
        AEPWeapon* EquippedWeapon,
        const FVector& Origin,
        const TArray<FVector>& Directions,
        float ServerNow) const;                          // ClientFireTime 제거, ServerNow 유지

    float ComputeRewindTime(
        const AEPCharacter* Shooter,
        float ServerNow) const;                          // ClientFireTime 제거, Target도 제거(InterpDelay 미사용)
```

### 6.7 `Private/Combat/EPServerSideRewindComponent.cpp`

`ComputeRewindTime` 전체 교체:

```cpp
float UEPServerSideRewindComponent::ComputeRewindTime(
    const AEPCharacter* Shooter, float ServerNow) const
{
    const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();

    float PredictionDelay = 0.f;
    if (const APlayerState* PS = Shooter ? Shooter->GetPlayerState() : nullptr)
    {
        PredictionDelay = FMath::Clamp(
            PS->GetPingInMilliseconds() * 0.001f - CombatSettings->PredictionFudgeSeconds,   // RTT 전체, *0.5f 없음
            0.f, CombatSettings->MaxRewindSeconds);
    }

    return ServerNow - PredictionDelay;
}
```

`InterpDelay`/`Target` 관련 코드는 전부 지운다 — §4의 결론대로 최종 코드엔 안 들어간다.

`GetHitscanCandidates` 시그니처와 내부 호출부(`ClientFireTime` 인자만 빠짐, `ComputeRewindTime` 호출에서 `Char` 인자도 빠짐):

```cpp
TArray<AEPCharacter*> UEPServerSideRewindComponent::GetHitscanCandidates(
    AEPCharacter* Shooter, AEPWeapon* EquippedWeapon,
    const FVector& Origin, const TArray<FVector>& Directions, float ServerNow) const
{
    ...
    const FEPHitboxSnapshot Snap = TargetSSR->GetSnapshotAtTime(
        ComputeRewindTime(Shooter, ServerNow));   // ClientFireTime, Char(Target) 인자 모두 빠짐
    ...
}
```

`ConfirmHitscan` 본문 — `ClientFireTime`을 받던 자리와 그걸 넘기던 호출부만 지운다:

```cpp
bool UEPServerSideRewindComponent::ConfirmHitscan(
    AEPCharacter* Shooter, AEPWeapon* EquippedWeapon,
    const FVector& Origin, const TArray<FVector>& Directions,
    TArray<FHitResult>& OutConfirmedHits)
{
    ...
    const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    ...
    const TArray<AEPCharacter*> Candidates =
        GetHitscanCandidates(Shooter, EquippedWeapon, Origin, Directions, ServerNow);   // ClientFireTime 인자만 빠짐

    if (bDebugLog)
    {
        float PredictionDelayLog = 0.f;
        if (const APlayerState* PS = Shooter ? Shooter->GetPlayerState() : nullptr)
        {
            PredictionDelayLog = FMath::Clamp(
                PS->GetPingInMilliseconds() * 0.001f - CombatSettings->PredictionFudgeSeconds,   // RTT 전체
                0.f, CombatSettings->MaxRewindSeconds);
        }
        UE_LOG(LogTemp, Log, TEXT("[SSR] ServerNow=%.3f PredictionDelay=%.3f Candidates=%d"),
            ServerNow, PredictionDelayLog, Candidates.Num());
    }
    ...
    for (AEPCharacter* Char : Candidates)
    {
        ...
        const float RewindTime = ComputeRewindTime(Shooter, ServerNow);   // ClientFireTime, Char(Target) 인자 모두 빠짐
        const FEPHitboxSnapshot Snap = TargetSSR->GetSnapshotAtTime(RewindTime);
        ...
    }
    ...
}
```

**로그 중복 계산 문제(이전 판에서 겪었던 그 버그)에 대한 근본 대책:** 이번에도 `ComputeRewindTime`
내부 계산과 로그용 계산이 따로 있다. 완전히 없애려면 `ComputeRewindTime`이 `PredictionDelay`를
`out` 파라미터로도 내보내게 하는 걸 고려할 수 있다 — 지금 규모에서 필수는 아니지만, 두 계산식이
또 벌어지는(하나만 고치는) 실수를 하고 싶지 않다면 이번엔 이 방식으로 가는 걸 권한다.

---

## 7. 정리되는 것 — 한눈에

| 파일 | 변경 |
|---|---|
| `EPCombatDeveloperSettings.h` | `FireTimeToleranceSeconds` 삭제, `PredictionFudgeSeconds` 추가 |
| `DefaultGame.ini` | `FireTimeToleranceSeconds=0.7` 삭제 (검증 무력화의 직접 원인이었던 잔존값) |
| `EPWeapon.h/.cpp` | `Fire()`의 미사용 `ClientFireTime` 파라미터 삭제 |
| `EPCombatComponent.h/.cpp` | `Server_ConfirmFire`/`HandleServerFire`/`HandleHitscanFire`에서 `ClientFireTime` 삭제 |
| `EPGA_Item_PrimaryUse.cpp` | 클라이언트 브랜치에서 `GS`/`ClientTime` 계산 삭제 |
| `EPServerSideRewindComponent.h/.cpp` | `ConfirmHitscan`/`GetHitscanCandidates`/`ComputeRewindTime`에서 `ClientFireTime` 삭제, `ComputeRewindTime`은 `Target`/`InterpDelay`도 제거하고 `PredictionDelay`(RTT 전체) 계산만 남김 |

`HitboxHistory`/`SaveHitboxSnapshot`/`OnServerMoveProcessed`/`GetSnapshotAtTime`은 **손대지
않는다** — "언제를 볼지" 계산만 바뀌는 거고, "그 시각의 참값을 어떻게 재구성하는지"는
완전히 별개다.

---

## 8. 검증 방법

`CLAUDE.md` §4 기준으로 성공 조건을 정한다.

1. **핑이 오르면 `PredictionDelay`도 비례해서 커지는지** — `net PktLag=200` 같은 콘솔
   명령으로 핑을 인위적으로 올리고, `[SSR] ServerNow=... PredictionDelay=...` 로그가
   `GetPingInMilliseconds()*0.001 - PredictionFudgeSeconds`(RTT 전체)와 일치하는지 확인한다.
   (이전처럼 두 값을 "비교"하는 게 아니라, 계산이 그 공식대로 나오는지만 확인하면 된다 —
   비교 대상이 되는 두 번째 값 자체가 없다.)
2. **정직한 클라이언트가 이동 중인 표적을 정상적으로 맞히는지** —
   `bEnableSSRDebugDraw = true` 상태에서 빨간 히트박스(리와인드 후)가 파란 히트박스(현재)보다
   뚜렷하게 뒤에 그려져야 한다.
3. **나쁜 네트워크(지터·패킷 유실 포함 프로파일)에서도 안정적인지** — 이게 이번 설계 변경의
   핵심 검증 대상이다. 하이브리드안은 정확히 이 조건에서 계속 빗나갔다(`ClientFireTime`이
   나쁜 네트워크에서 불안정했기 때문). `PredictionDelay`는 클라이언트 시계를 아예 안 쓰므로,
   같은 조건에서 이 문제가 재현되지 않아야 한다. 재현되면 원인은 `ClientFireTime`이 아니었던
   것이니 §9의 다른 후보(`HitboxHistory` 간격)를 봐야 한다.
4. **AI(폰이 없는 표적) 회귀 확인** — `Shooter->GetPlayerState()`가 null인 경우
   `PredictionDelay = 0`으로 안전하게 빠지는지 확인한다. `HitboxHistory`가 비어 있는 AI 표적
   문제(`ServerSideRewind.md` C2)는 이 수정과 별개로 남아 있다.
5. **`MaxRewindSeconds`로 잘리는 극단값 확인** — 핑을 아주 크게 올려서(`PktLag=2000` 등)
   `PredictionDelay`가 `MaxRewindSeconds`에서 잘리는지, 그 상태에서 리와인드가 폭주하지
   않는지 확인한다.

---

## 9. 알아둘 것 / 한계

- **실측 경과(2026-09-01, PktLag 인바운드/아웃바운드 모두 200/200 고정) — 세 단계:**
  1. `RTT/2`(`PredictionDelay≈0.196`, `TotalDelay≈0.295`, `InterpDelay` 포함): 캐릭터
     너비만큼 **앞**을 쏴야 맞음 (부족)
  2. `RTT` 전체 + `InterpDelay`(`PredictionDelay≈0.408`, `TotalDelay≈0.508`): 캐릭터
     너비만큼 **뒤**(`-`)를 쏴야 맞음 (과함) — 1과 2의 오차가 크기는 같고 부호만 반대라
     정답은 정확히 중간(`TotalDelay≈0.402`)이라는 뜻
  3. `RTT` 전체, `InterpDelay` 없이(`TotalDelay≈0.408`): 여전히 아주 살짝 `+` 방향이었지만
     **0.03~0.05초어치 전진 수준**으로 잡음 범위 — 2번의 중간값(0.402)과도 0.006초 차이로
     거의 일치. **최종 결정: `TotalDelay = PredictionDelay`(RTT 전체, `InterpDelay` 없음)로
     확정.** 남은 잔차는 핑 롤링평균 지연 등 잡음으로 보고 더 쫓지 않는다 — 8인 규모
     포트폴리오 프로젝트에서 여기까지가 실익 대비 적정선이다.
- **`GetPingInMilliseconds()`가 진짜 `U+D`(왕복) 전체를 반영한다는 전제 위에 있다.** 이건
  일반적으로 맞다 — 엔진의 핑 측정 자체가 왕복 시간이다. `PredictionFudgeSeconds`는 그
  측정의 잡음(핑 롤링평균 지연 등)을 흡수하는 작은 여유값일 뿐, RTT/2 시절처럼 "구조적으로
  절반 모자란 걸 메꾸는" 역할은 이제 없다.
- **비대칭 네트워크(업링크≠다운링크)에서는 여전히 근사다.** `U`와 `D`를 따로 측정할 방법이
  없어 `RTT = U+D`만 알고 `U`, `D` 개별 값은 모른다. 대부분의 경우 이 둘을 따로 쓸 이유가
  없어(둘 다 더하기만 하면 되므로) 문제되지 않지만, 극단적으로 비대칭인 회선에서는 `RewindTime`
  유도의 `U + D` 항이 정확해도 `InterpDelay`처럼 한쪽 편도에만 의존하는 별도 보정이 필요해질
  수 있다 — 8인 규모 프로젝트에서는 현재로선 과한 걱정이다.
- **`RPC를 직접 호출하는 부정행위는 이 수정의 범위 밖이다.`** `Server_ConfirmFire`는 소유
  커넥션이면 누구나 호출 가능한 평범한 RPC다. 다만 이번 설계는 클라이언트가 보낼 수 있는
  "시각" 자체가 없어졌으니, 예전에 걱정했던 "유리한 순간을 골라 보낸다"는 공격 표면은 없다 —
  남은 건 어빌리티의 쿨다운·탄약 체크를 우회하는 반복 호출뿐이고, 이건 이전 판과 동일한
  별개의 노출이다.
- **`GetSnapshotAtTime`이 후보마다 두 번 불리는 기존 비용**(`ServerSideRewind.md` §A8)은
  그대로 남는다. 8인 규모에서는 여전히 급하지 않다.

---

## 10. 관련 문서

- `DOCS/Mine/ServerSideRewind.md` §4 C1 — 이 버그의 최초 발견, 회귀 경위
- `DOCS/Mine/GetServerWorldTimeSeconds.md` — **이 기능에는 더 이상 적용되지 않는다** (`ClientFireTime`을
  아예 안 쓰므로). 다만 그 문서가 다룬 `GetServerWorldTimeSeconds()`의 클라-서버 편향 자체는
  여전히 유효한 일반 지식이고, `CooldownPrediction.md`(GAS 쿨다운 위젯)처럼 **클라이언트가
  실제로 서버 시각을 추정해야 하는 다른 기능**에는 그대로 적용된다. 이 기능만 그 문제를
  회피한 것이지, 문제 자체가 프로젝트에서 사라진 게 아니다.
- `DOCS/Blog/Submit/2026-03-14-EP_NetPrediction-2.md` §정직하게 ① — "정석은 서버가 잰
  왕복 시간을 쓰는 것"이라고 이미 적어둔 문장. 하이브리드를 거쳐 결국 이 문장 그대로
  돌아왔다 — 블로그를 다시 손볼 필요는 없다(정직하게 ①이 이미 정답을 말하고 있었다).
