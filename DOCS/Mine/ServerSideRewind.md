# ServerSideRewind 검증 — 블로그 글 대 엔진 소스 대 현재 코드

**목적:** 블로그에 이미 올라간 글
`_posts/game_dev/devlog/2026-03-14-EP_NetPrediction-2.md`
("서버 사이드 리와인드: 두 함수가 다른 프레임의 시계를 보고 있었다")를
**UE 5.7 엔진 소스 직독**과 **현재 프로젝트 코드**에 대조해서,
포트폴리오에 그대로 내놔도 되는 부분과 고쳐야 할 부분을 가른다.

**검증 일자:** 2026-09-01
**엔진:** `C:\Program Files\Epic Games\UE_5.7\Engine` (직독. 기억이나 문서 요약 인용 아님)
**브랜치:** `feature-loot` (GAS 전환이 이미 들어와 있는 상태)

**읽는 법:**
- §1 요약표만 봐도 판정은 다 나온다.
- §2는 "맞았다"의 근거, §3은 "틀렸다"의 근거 — 둘 다 파일:줄로 추적 가능하다.
- §4는 **글과 무관하게 지금 코드에 있는 결함**이다. C1이 가장 중요하다.
- §6은 글을 어떻게 고칠지의 구체 목록이다.

---

## 1. 요약표

| # | 글의 주장 | 판정 | 근거 |
|---|---|---|---|
| A1 | `LevelTick.cpp:1545/1581/1749` 줄번호 | ✅ | 5.7에서 그대로 |
| A2 | `GetServerWorldTimeSeconds()`는 `TimeSeconds`를 그대로 쓴다 | ✅ | `GameStateBase.cpp:144` |
| A3 | 같은 틱이라도 호출 위치에 따라 값이 한 프레임 다르다 | ✅ | 1545 < 1581 < 1749 |
| A4 | 서버는 Move 묶음을 도착한 틱에 한 번에 처리한다 | ✅ | `CharacterMovementComponent.cpp:9801` |
| A5 | `NewMove`만 걸러야 한다 | ✅ (이유는 글보다 하나 더 있다) | §2.3 |
| A6 | `SetBodyTransform(..., TeleportPhysics)`가 맞다 | ✅ (이유가 글과 다르다) | `BodyInstance.cpp:2786` |
| A7 | `SetBoneTransformByName`이 아니라 물리 바디여야 한다 | ✅ | `PhysAnim.cpp:706` |
| A8 | 한 발에 `GetSnapshotAtTime`을 두 번 부른다 | ✅ | `EPServerSideRewindComponent.cpp:277, 358` |
| A9 | 미래 시각은 결과적으로 무해 | ✅ | 같은 파일 `:209` |
| A10 | GAS로 가도 `ConfirmHitscan`은 재사용된다 | ✅ 실제로 그렇게 됐다 | `EPGA_Item_PrimaryUse.cpp:57` |
| A11 | ini를 바꾸면 버퍼 크기가 따라온다 | ✅ 그리고 안전 방향이다 | §2.5 |
| **B1** | `OnMovementUpdated`가 `BroadcastPostTickDispatch`에서 돈다 | ❌ | `TickDispatch` 안이다. `NetDriver.cpp:2762` |
| **B2** | `FMath::Lerp(FRotator)`는 랩어라운드를 못 한다 | ❌ | `Rotator.h:926`이 처리한다 |
| **B3** | `BlendWith`는 내부적으로 Slerp | ❌ | `FQuat::FastLerp` = nlerp. `Quat.h:1373` |
| **B4** | `MaxRewindSeconds = 0.5` | ❌ | ini가 **0.7**로 덮는다 |
| **B5** | `+4`는 한 틱에 여러 Move가 처리되기 때문 | ❌ | 여러 Move는 스냅샷 **하나로 합쳐진다** |

---

## 2. 확정된 것 — 엔진 직독으로 맞다

### 2.1 글의 핵심 주장은 5.7에서 그대로 성립한다

`UWorld::Tick` (`Engine/Source/Runtime/Engine/Private/LevelTick.cpp`):

```
1545:  BroadcastTickDispatch(DeltaSeconds);   ← ServerMove RPC가 여기서 실행된다
1546:  BroadcastPostTickDispatch();
1555:  RealTimeSeconds += DeltaSeconds;
1577:  UnpausedTimeSeconds += DeltaSeconds;
1581:      TimeSeconds += DeltaSeconds;       ← 월드 시간은 그 뒤에 증가한다
1721:  RunTickGroup(TG_PrePhysics);
1749:  RunTickGroup(TG_PostPhysics);          ← SSR::TickComponent가 여기서 돈다
```

`GameStateBase.cpp:144`:

```cpp
double AGameStateBase::GetServerWorldTimeSeconds() const
{
    UWorld* World = GetWorld();
    if (World) return World->GetTimeSeconds() + ServerWorldTimeSecondsDelta;
    return 0.;
}
```

따라서 **"같은 틱이니 같은 시각"은 틀렸다**는 글의 결론은 정확하다.
1545에서 읽은 값과 1749에서 읽은 값의 차이는 정확히 `DeltaSeconds`다.

> 덧붙일 사실: `ServerWorldTimeSecondsDelta`는 클라이언트에서만 채워진다.
> **서버에서 `GetServerWorldTimeSeconds()`는 `World->GetTimeSeconds()`와 같은 값**이다.
> 서버 쪽 이야기를 할 때 굳이 GameState를 경유하는 이유는 "클라와 같은 시계를 쓴다"를
> 코드로 드러내기 위해서지, 서버에서 값이 달라지기 때문이 아니다. 글에 한 줄 넣을 가치가 있다.

### 2.2 Move 묶음은 한 틱에 통째로 처리된다

`CharacterMovementComponent.cpp:9801` `ServerMove_HandleMoveData`:

```cpp
if (MoveDataContainer.bHasOldMove)      { SetCurrentNetworkMoveData(OldMove);     ServerMove_PerformMovement(*OldMove); }
if (MoveDataContainer.bHasPendingMove)  { SetCurrentNetworkMoveData(PendingMove); ServerMove_PerformMovement(*PendingMove); }
if (FCharacterNetworkMoveData* NewMove = ...) { SetCurrentNetworkMoveData(NewMove); ServerMove_PerformMovement(*NewMove); }
SetCurrentNetworkMoveData(nullptr);
```

`ENetworkMoveType`은 `CharacterMovementReplication.h:99`에 `{ NewMove, PendingMove, OldMove }`.
글의 "패킷이 도착한 틱에 묶음 전체를 한 번에 처리한다"는 그대로 맞다.

### 2.3 `NewMove` 필터에는 글이 안 쓴 더 강한 이유가 있다

글은 "묶음의 마지막 이동이 완료된 시점이라서"라고만 적었다. 실제 이유는 둘이다.

1. **PendingMove까지 기록하면 중복이다** — 같은 틱에 두 점이 들어가고, 둘 다 "지금" 시각을 갖는다.
2. **OldMove를 기록하면 시간 오름차순 불변식이 깨진다** — OldMove는 패킷 손실 대비 **재전송**이다.
   위치는 과거인데 `GetServerWorldTimeSeconds()`는 현재를 준다.
   이걸 배열에 넣으면 `GetSnapshotAtTime`의 선형 탐색이 전제하는 "시간 오름차순"이 무너진다.

②가 더 중요하다. `HitboxHistory`가 배열인 이유(글의 §보간)와 `NewMove` 필터가 **같은 불변식을 지키는 한 쌍**이라는 게 설계의 핵심인데, 글에는 따로따로 적혀 있다.

### 2.4 `TeleportPhysics`가 필요한 진짜 이유

글은 "속도를 만들어내지 않으려고"라고 적었다. 맞지만 **부차적**이다.

`BodyInstance.cpp:2786`:

```cpp
if (bIsSimKinematic && Teleport == ETeleportType::None)
{
    Scene->SetKinematicTarget_AssumesLocked(this, NewTransform, true);   // ← 다음 물리 스텝에 반영
}
else
{
    if (bIsSimKinematic) FPhysicsInterface::SetKinematicTarget_AssumesLocked(Actor, NewTransform);
    FPhysicsInterface::SetGlobalPose_AssumesLocked(Actor, NewTransform); // ← 즉시
}
```

그리고 `ChaosEngineInterface.cpp:2544`:

```cpp
Body_External.SetX(InNewPose.GetLocation());
Body_External.SetR(InNewPose.GetRotation());
Body_External.UpdateShapeBounds();
if (FChaosScene* Scene = GetCurrentScene(InActorReference))
    Scene->UpdateActorInAccelerationStructure(InActorReference);   // ★
```

**이 마지막 줄이 기법 전체가 성립하는 근거다.** 가속 구조가 그 자리에서 갱신되므로
같은 프레임의 `LineTraceSingleByChannel`이 되돌린 위치를 본다.

`ETeleportType::None`이었다면 `SetKinematicTarget`만 걸리고 실제 포즈는 **다음 물리 스텝**에 반영된다.
즉 같은 프레임에 쏘는 트레이스는 **옛 위치를 그대로 본다** — 리와인드가 통째로 무시된다.
"속도를 안 만든다"보다 이쪽이 훨씬 강한 논거다. 글에서 바꿔 쓸 것.

### 2.5 `FBodyInstance`가 맞다 — 엔진 자신이 그렇게 한다

`PhysAnim.cpp:706` (`USkeletalMeshComponent::UpdateKinematicBonesToAnim`):

```cpp
if (!bTeleport) PhysScene->SetKinematicTarget_AssumesLocked(BodyInst, BoneTransform, true);
else            FPhysicsInterface::SetGlobalPose_AssumesLocked(ActorHandle, BoneTransform);
```

여기서 `BoneTransform`은 **월드 공간 본 트랜스폼**이다.
매 프레임 엔진이 본 → 바디로 한 방향으로 밀어넣는다.

이게 두 가지를 동시에 증명한다.

- `SetBoneTransformByName`으로 본 배열을 건드려봐야 물리 바디에 안 닿는다 (글의 주장 ✅)
- `GetBoneTransform(BoneIndex)`(월드)를 저장했다가 `SetBodyTransform`에 넣는 SSR의 방식은
  **엔진의 정규 경로를 그대로 되짚은 것**이다. 좌표계 변환이 따로 필요 없는 이유가 여기 있다.

부산물로 하나 더: 리와인드된 바디는 어차피 다음 프레임 `UpdateKinematicBonesToAnim`이 덮는다.
그래도 같은 프레임 안에서 복구해야 하는 이유는 **그 프레임의 다른 트레이스·오버랩** 때문이다.
글의 "복구는 즉시" 절이 맞는 이유를 이렇게 적으면 더 단단하다.

### 2.6 ini 기반 버퍼 크기 산정은 안전 방향이다

`GetClientNetSendDeltaTime` (`CharacterMovementComponent.cpp:12781`)은
`ClientNetSendMoveDeltaTime`에서 시작해 조건에 따라 **키우기만** 한다
(`ClientNetSendMoveDeltaTimeThrottled`, `...Stationary`를 `FMath::Max`로).
호출부(`:8921`)는 `FMath::Clamp(..., 1/120, 1/5)`인데 현재 값 0.0166 > 1/120이라 하한이 걸리지 않는다.

⇒ 실제 전송 간격은 **항상 `ClientNetSendMoveDeltaTime` 이상**이다.
   그 값으로 나눈 개수는 **필요한 최댓값**이므로 히스토리는 절대 모자라지 않는다.

현재 값으로 계산: `ceil(0.7 / 0.0166) + 4 = 43 + 4 = 47`.
(글은 0.5 기준으로 서술 — §3.4 참고)

---

## 3. 틀린 것 — 블로그를 고쳐야 하는 부분

### 3.1 [B1] `OnMovementUpdated`는 `BroadcastPostTickDispatch`에서 돌지 않는다

글의 코드 주석:

```cpp
BroadcastTickDispatch(DeltaSeconds);      // ← ServerMove RPC 수신·처리
BroadcastPostTickDispatch();              //   CMC::OnMovementUpdated가 여기서 돈다   ← 틀림
```

`ServerMovePacked_ServerReceive`는 RPC 실행 그 자체이고, 들어온 번치를 처리하는 시점은
`BroadcastTickDispatch`(1545) 안이다. `ServerMove_HandleMoveData` → `ServerMove_PerformMovement`
→ `PerformMovement` → `OnMovementUpdated`까지 **동기 호출**이다 (`CharacterMovementComponent.cpp:9756-9837`).

`UNetDriver::PostTickDispatch`(`NetDriver.cpp:2762`)가 하는 일은
transactional RPC 실행, out-of-order 패킷 캐시 플러시, `PostDispatchSendUpdate` 뿐이다.
CMC와는 무관하다.

**영향:** 결론은 안 바뀐다. 1545든 1546이든 `TimeSeconds += DeltaSeconds`(1581)보다 앞이다.
하지만 "엔진 소스를 직접 읽었다"가 이 글의 핵심 자산이므로, 이 한 줄이 틀리면 그 자산이 깎인다.

**고칠 문장:** 주석을 `// ← ServerMove RPC 수신·실행. CMC::OnMovementUpdated까지 여기서 동기로 돈다`로.

### 3.2 [B2] `FMath::Lerp(FRotator, FRotator)`는 랩어라운드를 처리한다

글:
> `FMath::Lerp(FRotator, FRotator)`는 각도 랩어라운드를 처리하지 못한다.
> -179°에서 181°로 가는 걸 **360도 반대로 돌아가는 것**으로 계산한다.

`Rotator.h:926`:

```cpp
template<typename T>
struct TCustomLerp<UE::Math::TRotator<T>>
{
    constexpr static bool Value = true;   // FMath::Lerp<TRotator>()가 아래를 부르게 한다
    template<class U>
    static RotatorType Lerp(const RotatorType& A, const RotatorType& B, const U& Alpha)
    {
        return A + (B - A).GetNormalized() * Alpha;   // ← GetNormalized()가 (-180,180]로 접는다
    }
};
```

`GetNormalized()`가 축별로 최단 방향을 고른다. 글이 든 예시가 정확히 그 처리 대상이다.
(반대로 **긴 쪽으로 돌고 싶을 때** 쓰라고 `FMath::LerpRange`가 `Rotator.h:940`에 따로 있다.)

**`BlendWith`를 쓴 것 자체는 여전히 옳다. 이유가 다를 뿐이다:**

1. 오일러 축별 보간은 3D 최단 회전호가 아니다. 축이 정렬되는 구간에서 눈에 띄게 튄다.
2. 본은 애초에 `FTransform`(쿼터니언)으로 저장돼 있다. `FRotator`로 왕복하면 변환 비용 + 정밀도 손실만 는다.
3. `BlendWith`는 Translation/Rotation/Scale을 한 번에 처리한다 — 손으로 세 줄 쓸 일이 없다.

### 3.3 [B3] `BlendWith`는 Slerp가 아니라 nlerp다

`TransformNonVectorized.h:405`:

```cpp
Translation = FMath::Lerp(Translation, OtherAtom.Translation, Alpha);
Scale3D     = FMath::Lerp(Scale3D,     OtherAtom.Scale3D,     Alpha);
Rotation    = TQuat<T>::FastLerp(Rotation, OtherAtom.Rotation, Alpha);   // ← Slerp 아님
```

`Quat.h:1373`:

```cpp
// Fast Linear Quaternion Interpolation.  Result is NOT normalized.
inline TQuat<T> TQuat<T>::FastLerp(const TQuat<T>& A, const TQuat<T>& B, const T Alpha)
{
    // To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
    const T DotResult = (A | B);
    const T Bias = FMath::FloatSelect(DotResult, T(1.0f), T(-1.0f));
    return (B * Alpha) + (A * (Bias * (1.f - Alpha)));
}
```

정확히는 **nlerp**(선형 보간 후 정규화)이고, `Bias`가 dot 부호를 보정하므로
**"최단 경로로 돈다"는 결론은 맞다.** Slerp와의 차이는 각속도가 일정하지 않다는 것뿐이고,
스냅샷 간격 16ms에서 본 하나가 도는 각도는 작아서 실질 차이가 없다.

(벡터화 경로 `TransformVectorized.h:466`도 같다: `VectorLerpQuat` + `VectorNormalizeQuaternion`.)

**고칠 문장:** "내부적으로 쿼터니언 Slerp이라" → "내부적으로 쿼터니언 nlerp(`FQuat::FastLerp`)이고,
dot 부호를 보정해 최단 경로를 보장한다. 등속은 아니지만 16ms 간격에서는 차이가 없다."

### 3.4 [B4] `MaxRewindSeconds`는 지금 0.5가 아니라 0.7이다

- C++ 기본값: `EPCombatDeveloperSettings.h` `float MaxRewindSeconds = 0.5f;`
- **실제 적용값:** `EmploymentProj/Config/DefaultGame.ini:24` `MaxRewindSeconds=0.700000`

`UDeveloperSettings(Config=Game)`이므로 ini가 이긴다.
글의 마지막 장("지연 보상은 공정성을 만들지 않는다")이 통째로 0.5를 전제로 논증한다.

**둘 중 하나를 해야 한다.** ini를 0.5로 되돌리든가, 글을 0.7로 고치고
"왜 0.5에서 0.7로 올렸는가"를 한 줄 적든가. 후자가 글로서는 더 재미있다 —
"디자인 결정이다"라고 써놓고 실제로 그 다이얼을 돌린 기록이 되니까.

### 3.5 [B5] `+4`의 이유가 구현과 어긋난다

글:
> `+ 4`는 여유분이다. 패킷이 몰려 오면 한 틱에 여러 Move가 처리될 수 있고 ...

구현은 그렇지 않다 (`EPServerSideRewindComponent.cpp:149-163`):

```cpp
// TickComponent (PostPhysics)
if (bHasPendingSnapshot) { SaveHitboxSnapshot(PendingSnapshotTime, PendingSnapshotLocation); bHasPendingSnapshot = false; }

// OnServerMoveProcessed (TickDispatch)
bHasPendingSnapshot = true;  PendingSnapshotTime = Time;  PendingSnapshotLocation = Location;
```

pending은 **bool 하나**다. 한 틱에 ServerMove가 두 번 오면 뒤엣것이 앞엣것을 덮는다.

⇒ 스냅샷은 **"ServerMove 수신마다 하나"가 아니라 "프레임당 최대 하나"**다.

이게 나쁜 건 아니다. 오히려 **버퍼 산정을 더 안전하게 만든다** —
합쳐질수록 스냅샷 간격이 벌어지고, 같은 개수로 더 긴 시간을 덮으니까.
하지만 글이 적은 이유("여러 Move가 쌓이니까 여유가 필요")는 반대 방향이다.

**고칠 문장:** "`+4`는 경계에서 보간할 두 점을 항상 남기기 위한 여유다.
한 틱에 여러 Move가 처리되면 pending이 덮어써져 스냅샷 하나로 합쳐지는데,
그건 히스토리가 덮는 시간을 **늘리는** 쪽이라 이 계산을 깨지 않는다."

그리고 **손실도 같이 적으면 정직하다:** 합쳐질 때 중간 위치는 버려지고,
그 구간은 선형 보간으로 메워진다. 서버 틱레이트가 클라 전송 주기보다 낮으면 이 손실이 커진다.

---

## 4. 글과 무관하게, 지금 코드에 있는 문제

### C1. [치명] GAS 전환에서 `ClientFireTime`이 유실됐다 — 지연 보상이 사실상 꺼져 있다

`EPGA_Item_PrimaryUse.cpp:49-58`:

```cpp
const FVector Origin = Char->GetCameraComponent()->GetComponentLocation();
const AGameStateBase* GS = Char->GetWorld()->GetGameState<AGameStateBase>();
const float ClientTime = GS ? GS->GetServerWorldTimeSeconds()
                            : Char->GetWorld()->GetTimeSeconds();

if (ActorInfo->IsNetAuthority())
{
    UEPCombatComponent* Combat = Char->GetCombatComponent();
    Combat->HandleServerFire(Origin, Char->GetControlRotation().Vector(), ClientTime);
}
```

어빌리티는 `LocalPredicted`라 클라와 서버 양쪽에서 `ActivateAbility`가 돈다.
그런데 `HandleServerFire`를 부르는 건 **서버 브랜치**이고, 그때 `ClientTime`은
**서버가 자기 시계를 그 자리에서 읽은 값**이다. 클라가 보낸 시각이 아니다.

결과를 따라가면:

```
ClientTime == ServerNow
  → ConfirmHitscan: ServerNow - ClientFireTime ≈ 0     (:326 클램프에 안 걸린다)
  → GetSnapshotAtTime(≈ServerNow)
  → TargetTime >= HitboxHistory.Last().ServerTime      (:209)
  → return HitboxHistory.Last()
```

**되돌리는 양이 한 프레임 이하다.** 0.7초 창은 한 번도 쓰이지 않는다.

`Server_Fire` RPC는 코드에 더 이상 없다(`grep -rn "Server_Fire"` → 0건).
즉 클라 시각을 서버로 나르던 유일한 경로가 GAS 전환 때 사라졌고,
`ClientFireTime`이라는 **이름만 파이프라인에 남았다**.

블로그의 242cm → 2.3cm는 `Server_Fire` 시절 측정치다. 지금 코드로 다시 재면 그 수치가 안 나온다.

**되살리는 길 두 가지:**

**(a) 클라가 시각을 보낸다 (원래 설계 복원)**
`UAbilityTask_ServerWaitForClientTargetData` + 커스텀 `FGameplayAbilityTargetData`에
`Origin` / `Direction` / `FireTime`을 실어 보낸다. 예측 키 흐름은 그대로다.
장점: 글이 서술한 동작 그대로. 단점: 글이 스스로 지적한 신뢰 문제도 그대로.

**(b) 서버가 잰다 (글의 "남은 것 ①"을 지금 구현한다)**
```cpp
const float RTTHalf    = Shooter->GetPlayerState()->GetPingInMilliseconds() * 0.001f * 0.5f;
const float RewindTime = ServerNow - FMath::Clamp(RTTHalf, 0.f, MaxRewindSeconds);
```
`GetPingInMilliseconds()`는 `PlayerState.h:348`에 있고 **서버에서는 `ExactPing`을 반환**한다.
클라 값이 애초에 안 오는 지금 상태에서는 (b)가 **회귀 수정과 치트 대응을 동시에** 한다.

> **권고: (b).** (a)는 잃어버린 신뢰 경로를 되살리는 일인데, 글이 이미 "정석은 (b)"라고 적었다.
> 되살릴 걸 되살리느라 두 번 일하는 대신, 한 번에 정석으로 가는 편이 낫다.
> 정밀도가 부족하면 나중에 (a)를 얹어 "클라 값을 받되 RTT 기대치에서 벗어나면 기각"으로 확장하면 된다 —
> 이건 글이 이미 예고해둔 확장점이다.

> ### ✅ 2026-09-01 해결 — 그런데 (b)가 아니라 **(a)+(b) 혼합**으로 갔다
>
> 위 권고는 **(b) 단독**이었는데, 실제 구현은 **(a)를 살리고 (b)의 검증을 얹는** 쪽으로 뒤집혔다.
> 구현서: **`DOCS/Mine/LagCompensationFix.md`**
>
> ```
> ExpectedDelay  = 서버가 잰 사수의 RTT/2
> ClaimedDelay   = ServerNow - ClientFireTime          ← 클라가 보낸 값 (Server_ConfirmFire RPC)
> ValidatedDelay = Clamp(ClaimedDelay, ExpectedDelay ± Tolerance)
> InterpDelay    = 표적 CMC의 NetworkSimulatedSmoothLocationTime   ← 위 권고에 없던 항
> TotalDelay     = Clamp(ValidatedDelay + InterpDelay, 0, MaxRewindSeconds)
> ```
>
> **위 권고가 놓쳤던 것 둘:**
>
> 1. **보간 지연.** 클라가 상대를 스무딩해 뒤처지게 그리므로, 네트워크 지연만 빼면 **과소 보정**된다.
>    (b) 단독 공식에는 이 항이 아예 없었다.
> 2. **`Tolerance`의 실제 성격.** "보안 여유"라고 생각했는데, 실측해보니
>    **클라이언트 시계 추정 오차를 서버 RTT 쪽으로 당겨 깎는 보정 폭**으로 작동하고 있었다.
>    `0.1` → `0.7`로 넓히자 `TotalDelay`가 `0.376` → `0.507`로 늘어 **실제로 표적을 못 맞혔다**
>    (이 조건에서 클라의 `GetServerWorldTimeSeconds()` 추정치가 서버 RTT 실측치보다 230ms 컸다).
>
> **C6**(`AEPWeapon::Fire`의 죽은 `ClientFireTime` 파라미터)은 아직 안 지웠다.
> **C8**(`AddTickPrerequisiteComponent(Mesh)`)도 아직 안 넣었다 — 둘 다 서류 마감 뒤로 미뤘다.

> **최종 결정 (뒤집힘): (a) + (b) 혼합.** 순수 (b)로 갔다가, 클라이언트가 발사 시각을
> 다시 보내고 서버가 그 값을 RTT 기대치로 검증하는 방식으로 바꿨다 — §마지막 문단
> ("혹은 클라이언트 값을 받되 RTT 기대치에서 벗어나면 기각한다")을 실제로 구현하는 것이다.
> 여기에 표적 CMC의 `NetworkSimulatedSmoothLocationTime`을 더하는 **보간 지연** 보정까지
> 얹었다. 구체적인 구현 지침은 `DOCS/Mine/LagCompensationFix.md` 참고.

**포트폴리오 관점:** 이 글은 "고친 이야기"인데 현재 코드에서 그 고침이 무력화돼 있다.
면접에서 코드를 열면 바로 보인다. 셋 중 하나를 골라야 한다.

1. 코드를 고치고 글은 그대로 둔다 (권장)
2. 글에 "GAS 전환 과정에서 이 경로가 끊겼다 — 아래처럼 되살린다" 한 절을 추가한다
   (오히려 **"내 코드의 회귀를 내가 찾아냈다"**는 서사가 된다. 이 글의 성격과도 맞는다)
3. 둘 다 한다

### C2. AI 적은 영원히 못 맞힌다 — 다음 단계에 바로 걸린다

스냅샷은 `GetCurrentNetworkMoveData()`가 `NewMove`일 때만 쌓인다
(`EPCharacterMovement.cpp:27-32`). AI가 조종하는 `AEPCharacter`는 ServerMove가 없다.

```
HitboxHistory 비어 있음
  → GetSnapshotAtTime → FEPHitboxSnapshot{}          (:199-202, Location = ZeroVector)
  → GetHitscanCandidates: PointDistToSegment(원점, ...) > BroadRadius   (:288)
  → 후보 탈락
  → CandidateSet에 없으므로 Narrow Phase 히트도 버려진다               (:414)
```

`RewindComponent`는 `AEPCharacter` 생성자에서 무조건 만들어지므로(`EPCharacter.cpp:70`)
컴포넌트는 있는데 히스토리만 빈, 가장 조용한 형태로 실패한다.

**남은 구현 순서가 "인벤토리 → AI 적"이므로 지금 정해야 한다.** 두 갈래:

- **서버 소유 캐릭터는 매 프레임 PostPhysics에서 무조건 찍는다** — `TickComponent`에서
  pending이 없고 `GetController()`가 로컬(=ServerMove가 올 리 없는) 폰이면 현재 시각·위치로 저장.
  일관성은 좋지만 AI 수만큼 매 프레임 본 19개를 읽는 비용이 든다.
- **히스토리가 비면 현재 위치로 폴백** — `GetSnapshotAtTime`이 빈 배열일 때
  `FEPHitboxSnapshot{}` 대신 소유자의 현재 위치·본을 채워 돌려준다.
  싸고, "AI는 지연 보상 대상이 아니다"라는 결정을 코드로 드러낸다.

> 후자를 권한다. AI는 서버가 직접 움직이므로 되돌릴 "클라이언트의 과거 주장"이 없다.
> 되돌릴 게 없다는 사실을 폴백으로 표현하는 편이, 안 쓰는 히스토리를 쌓는 것보다 정직하다.
> 다만 **`DOCS/`에 이 결정이 이름으로 적혀야** 한다 — 지금은 어디에도 없다.

### C3. 후보가 아닌 캐릭터가 총알을 삼킨다

`EPServerSideRewindComponent.cpp:411-434`:

```cpp
if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, EP_TraceChannel_Weapon, Params))
{
    AEPCharacter* HitChar = Cast<AEPCharacter>(Hit.GetActor());
    if (HitChar && CandidateSet.Contains(HitChar)) { OutConfirmedHits.Add(Hit); ... }
    else if (!HitChar)                             { OutConfirmedHits.Add(Hit); }
    // HitChar && !Contains → 아무것도 안 한다
}
```

세 번째 경우에 트레이스는 **이미 그 캐릭터에서 멈췄다**. 뒤에 있는 벽도, 진짜 후보도 못 맞힌다.
후보가 아닌 캐릭터가 유령 방패가 된다.

글은 "후보가 아닌 캐릭터는 걸러야 한다"까지만 적었고 이 부작용은 안 적었다.
`MultiLineTrace` 후 필터링하거나, 후보 아닌 캐릭터를 미리 `Params.AddIgnoredActor`로 넣어야 한다.

### C4. 확정 히트 디버그 구가 가드 밖에 있다

`:418-421` — `if (bDebugDraw)`가 통째로 주석 처리돼 있어 노란 구가 **항상** 그려진다.

```cpp
// if (bDebugDraw)
// {
    DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 12, FColor::Yellow, ...);
// }
```

`DrawDebugSphere` 자체는 Shipping에서 컴파일 아웃되지만, `#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)`로
`bDebugDraw = false`를 만들어둔 §디버그 시각화의 의도와 어긋난다.

### C5. `GetSnapshotAtTime`의 경계 로그가 무조건 찍힌다

`:206`, `:211`, `:231`의 `UE_LOG`가 `bDebugLog`와 무관하다.
한 발마다 `GetSnapshotAtTime`이 **후보 수 × 2회** 불리므로(§A8) 로그가 금방 잠긴다.

### C6. `AEPWeapon::Fire`의 `ClientFireTime`은 죽은 파라미터다

`EPWeapon.cpp:69-104` 안에서 한 번도 안 쓰인다. `LastFireTime`은 별도로
`GetWorld()->GetTimeSeconds()`로 채운다.

C1을 (a)로 고치든 (b)로 고치든 이 파라미터는 지워야 한다.
(C1을 (b)로 가면 `HandleServerFire`의 `ClientFireTime` 인자도 통째로 사라진다.)

### C7. `Location`과 `Bones`의 정합성은 전제 위에 서 있다

`Location`은 TickDispatch에서 CMC가 준 값, `Bones`는 PostPhysics에서 읽은 값이다.
둘이 같은 순간을 가리키는 건 **"그 사이에 액터를 움직이는 다른 주체가 없다"**는 전제 때문이다.

지금은 참이다. 하지만 넉백/텔레포트 GameplayEffect, 물리 임펄스, 시퀀서 같은 걸 붙이면 깨진다.
글이 "셋이 같은 순간을 가리켜야 한다"를 규칙으로 세웠으니,
그 규칙이 **무엇 위에 서 있는지**도 같이 적어야 규칙이 완성된다.

### C8. `TG_PostPhysics` 안에서 본이 확정됐다는 보장이 없다 — 프리리퀴짓이 빠져 있다

SSR은 `TG_PostPhysics`에 틱을 걸어두고 본을 읽는다(`:106`). 그 근거는
"스켈레탈 메시의 `EndTickGroup`이 `TG_PostPhysics`라 그 그룹에서 애님 태스크 완료가 보장된다"인데,
**보장되는 시점이 그 그룹의 *끝*이다.**

`FTickTaskSequencer::ReleaseTickGroup` (`TickTaskManager.cpp:933`):

```cpp
DispatchTickGroup(ENamedThreads::GameThread, WorldTickGroup);   // ← SSR 틱이 여기서 큐에 들어간다
...
if (bBlockTillComplete || bSingleThreadMode)
{
    for (ETickingGroup Block = WaitForTickGroup; Block <= WorldTickGroup; ...)
        WaitUntilTasksComplete(TickCompletionEvents[Block], ...);   // ← 애님 완료를 여기서 기다린다
}
```

**디스패치가 먼저, 대기가 나중이다.** 메시의 완료 이벤트는
`DontCompleteUntil(FParallelAnimationCompletionTask)`로 확장돼 있어서
`FinalizeBoneTransform`(버퍼 플립) 전까지 안 끝나는데, 그 대기는 SSR이 이미 틱한 **뒤**에 온다.

엔진이 이 사실을 스스로 인정하는 줄이 있다 (`TickTaskManager.cpp:696-703`):

```cpp
FORCEINLINE bool ShouldConsiderPrerequisite(FTickFunction* TickFunction, FTickFunction* Prereq)
{
    // Ignore prereqs that are guaranteed to finish in a previous group
    TEnumAsByte<enum ETickingGroup> PrereqEnd = Prereq->GetActualEndTickGroup();
    return (!bAllowOptimizedPrerequisites || PrereqEnd >= TickFunction->TickGroup || PrereqEnd == TG_DuringPhysics);
}
```

`PrereqEnd >= 내 TickGroup`이면 **"이전 그룹에서 끝났다고 보장할 수 없다"**는 뜻이다.
메시(`EndTickGroup = TG_PostPhysics`)와 SSR(`TickGroup = TG_PostPhysics`)이 정확히 그 경우다.

**지금 안 터지는 이유:** 애님 평가는 `TG_PrePhysics`에서 던져지고, 그 뒤 물리 블록
(`TG_StartPhysics` ~ `TG_EndPhysics`)에서 게임 스레드가 태스크 큐를 펌프한다.
캐릭터 하나의 애님 평가는 물리 한 프레임보다 짧으니 그 사이에 완료 태스크가 소화된다.
**부하가 걸리면(캐릭터 다수, 무거운 AnimGraph) 순서가 뒤집힐 수 있고, 그때 증상은
"가끔 한 프레임 옛날 본으로 스냅샷이 찍힌다"**  — 이 글이 잡은 버그와 똑같은 모양이다.

**고치는 법은 한 줄이다.** `BeginPlay`에서:

```cpp
if (USkeletalMeshComponent* Mesh = OwnerChar->GetMesh())
{
    AddTickPrerequisiteComponent(Mesh);
}
```

`QueueTickFunction`(`TickTaskManager.cpp:2668-2682`)을 따라가면 부작용이 없다는 게 확인된다.

| 값 | 계산 | 결과 |
|---|---|---|
| `MyActualTickGroup` | `Max(메시 Start=PrePhysics, 내 TickGroup=PostPhysics, ...)` | `TG_PostPhysics` — **그룹은 그대로** |
| `MyActualEndTickGroup` | `Max(메시 End=PostPhysics, 내 End=PostPhysics, ...)` | `TG_PostPhysics` — **번지지 않는다** |
| `RawPrerequisites` | 메시의 완료 핸들이 진짜 그래프 프리리퀴짓으로 들어간다 | 애님 완료 전에는 태스크가 시작하지 않는다 |

나중에 래그돌이 붙어 메시의 `EndTickGroup`이 `TG_PrePhysics`로 뒤집히면
`ShouldConsiderPrerequisite`이 이 프리리퀴짓을 **알아서 최적화로 걷어낸다** — 그때는 진짜로 보장되니까.
즉 이 한 줄은 필요할 때만 비용을 내고, 안 필요하면 스스로 사라진다.

> 지금 코드가 의존하는 건 **"실측으로 맞더라"**다. 이 글이 스스로 세운 기준
> ("값이 일정하게 틀리면 구조를 의심한다")을 적용하면, 여기는 구조로 못 박아야 하는 자리다.
> 틱 그룹 자체의 근거는 `DOCS/Mine/Concepts/TickGroup.md` §SkeletalMeshComponent 참고 —
> `EndTickGroup`은 생성자 상수가 아니라 매 프레임 재계산되는 값이다.

#### ★★ 2026-09-01 추가 — CMC 자신이 같은 처방을 쓴다

이 처방(`AddTickPrerequisiteComponent`)이 임의로 지어낸 게 아니라는 근거가 엔진 안에 있다.
`UCharacterMovementComponent`가 **정확히 같은 모양의 문제**를 이미 겪고 있고, 같은 방식으로 풀어뒀다.

```cpp
// CharacterMovementComponent.h:52-58 — 전용 틱 함수를 따로 둔다
struct FCharacterMovementComponentPostPhysicsTickFunction : public FTickFunction
{
    class UCharacterMovementComponent* Target;
    ...
};

// CharacterMovementComponent.cpp:651 — TG_PostPhysics
PostPhysicsTickFunction.TickGroup = TG_PostPhysics;

// CharacterMovementComponent.cpp:11770 — ★ 자기 자신의 PrePhysics 틱을 프리리퀴짓으로 명시
PostPhysicsTickFunction.AddPrerequisite(this, this->PrimaryComponentTick);
```

`PostPhysicsTickComponent`가 실제로 하는 일(`:1795-1809`)은 `bDeferUpdateBasedMovement`다 —
캐릭터가 딛고 선 발판이 물리 시뮬레이션 대상이면, 발판의 최종 위치는 물리가 끝나야 알 수 있으므로
**그 갱신을 `PrePhysics`에서 즉시 하지 않고 `PostPhysics`까지 미룬다.** "의존하는 값이 아직 안 끝났으니
프리리퀴짓으로 순서를 강제하고 늦게 처리한다"는 논리가 SSR이 본을 나중에 읽는 것과 완전히 같다.

**결론: SSR의 구조(전용 `PostPhysics` 틱 + 명시적 프리리퀴짓)는 엔진이 같은 문제에 실제로 쓰는 관례다.**
더 깔끔한 대안을 찾자면 이것이 이미 그 답이었다는 뜻이고, 빠진 건 프리리퀴짓 한 줄뿐이었다.

---

---

## 5. 설정·문서 불일치

| 위치 | 내용 | 문제 |
|---|---|---|
| `Config/DefaultGame.ini:25` | `SnapshotIntervalSeconds=0.010000` | `UEPCombatDeveloperSettings`에 그런 UPROPERTY가 **없다**. 고정 타이머 시절 잔재 |
| `Config/DefaultGame.ini:30` | `bReproduceLegacyBug=True` | 마찬가지로 존재하지 않는 필드 |
| `Config/DefaultGame.ini:24` | `MaxRewindSeconds=0.700000` | 글·코드 기본값은 0.5 (§3.4) |
| `Config/DefaultGame.ini:26,29` | `bEnableSSRDebugDraw/Log=True` | 현재 디버그가 켜져 있다. 의도한 것인지 확인 |

죽은 ini 키는 지우거나, 남길 거면 왜 남기는지 주석을 단다.
지금 상태는 "설정이 있는데 코드가 안 읽는다"라서, 나중에 읽는 사람이 반드시 헛다리를 짚는다.

---

## 6. 블로그 수정 목록 (그대로 적용 가능)

| 위치 | 지금 | 고칠 것 |
|---|---|---|
| §원인, `UWorld::Tick` 코드블록 | `BroadcastPostTickDispatch(); // CMC::OnMovementUpdated가 여기서 돈다` | `BroadcastTickDispatch(...); // ← ServerMove RPC 수신·실행. OnMovementUpdated까지 여기서 동기로 돈다` |
| §보간 | "`FMath::Lerp(FRotator, FRotator)`는 각도 랩어라운드를 처리하지 못한다 … 360도 반대로 돌아가는 것으로 계산한다" | 삭제. 대신 "오일러 축별 보간은 3D 최단 회전호가 아니고, 본은 원래 쿼터니언으로 저장돼 있어 왕복 변환이 낭비다" |
| §보간 | "`BlendWith`는 내부적으로 쿼터니언 Slerp" | "`FQuat::FastLerp`(nlerp) + 정규화. dot 부호 보정(`Bias`)으로 최단 경로는 보장되고, 등속이 아닌 차이는 16ms 간격에서 무시된다" |
| §ConfirmHitscan | "`TeleportPhysics`: 순간이동으로 처리해서 속도를 만들어내지 않는다" | 즉시 반영이 주된 이유임을 앞세운다. `SetGlobalPose` → `UpdateActorInAccelerationStructure`까지 인용 (§2.4) |
| §히스토리 크기 | "`+4`는 … 한 틱에 여러 Move가 처리될 수 있고" | pending이 bool 하나라 합쳐진다는 사실 + 그게 안전 방향이라는 설명 (§3.5) |
| §마지막 장 | `MaxRewindSeconds = 0.5` | 0.7로 바꾸고 "왜 올렸는가"를 한 줄 |
| §남은 두 가지 | ①이 "언젠가 할 일" | **"GAS 전환에서 이 경로가 끊겼다"**를 사실로 추가 (§4 C1) |
| §참고 | `LevelTick.cpp:1545, 1581, 1749 / GameStateBase.cpp:144` | 그대로 유효. 5.7 기준임을 명시하면 더 좋다 |

**추가하면 좋을 것 (글이 더 강해지는 재료):**

- `NewMove` 필터가 **시간 오름차순 불변식을 지키는 장치**라는 설명 (§2.3).
  지금은 "묶음의 마지막이라서"로만 적혀 있는데, 배열 선택 이유와 짝을 이루면 설계가 하나로 읽힌다.
- 엔진 자신이 `PhysAnim.cpp:706`에서 같은 일을 한다는 것 (§2.5).
  "내가 발명한 우회로"가 아니라 "엔진의 정규 경로를 되짚었다"가 된다.
- `ClientNetSendMoveDeltaTime`이 **하한**이라 버퍼 계산이 과소평가될 수 없다는 증명 (§2.6).
  "ini를 바꾸면 따라온다"에 안전성 논거가 붙는다.

---

## 7. 인용 색인

**엔진 (UE 5.7, `Engine/Source/Runtime/`)**

| 파일 | 줄 | 내용 |
|---|---|---|
| `Engine/Private/LevelTick.cpp` | 1545 / 1546 | `BroadcastTickDispatch` / `BroadcastPostTickDispatch` |
| | 1577 / 1581 | `UnpausedTimeSeconds +=` / `TimeSeconds +=` |
| | 1721 / 1749 | `RunTickGroup(TG_PrePhysics)` / `(TG_PostPhysics)` |
| `Engine/Private/GameStateBase.cpp` | 144 | `GetServerWorldTimeSeconds` |
| `Engine/Private/NetDriver.cpp` | 2762 | `UNetDriver::PostTickDispatch` — CMC와 무관 |
| `Engine/Private/Components/CharacterMovementComponent.cpp` | 9756 | `ServerMovePacked_ServerReceive` |
| | 9801 | `ServerMove_HandleMoveData` (Old → Pending → New) |
| | 8921 | `Clamp(GetClientNetSendDeltaTime(...), 1/120, 1/5)` |
| | 12781 | `GetClientNetSendDeltaTime` — Max로만 키운다 |
| `Engine/Classes/GameFramework/CharacterMovementReplication.h` | 99 | `ENetworkMoveType { NewMove, PendingMove, OldMove }` |
| `Engine/Classes/GameFramework/GameNetworkManager.h` | 153-173 | `ClientNetSendMoveDeltaTime` 계열 |
| `Engine/Classes/GameFramework/PlayerState.h` | 348 | `GetPingInMilliseconds` (서버는 `ExactPing`) |
| `Engine/Private/PhysicsEngine/BodyInstance.cpp` | 2741 / 2786 | `SetBodyTransform` / Teleport 분기 |
| `Engine/Private/PhysicsEngine/PhysAnim.cpp` | 538 / 706 | `UpdateKinematicBonesToAnim` / `SetGlobalPose_AssumesLocked` |
| `PhysicsCore/Private/ChaosEngineInterface.cpp` | 2544 | `SetGlobalPose` → `UpdateActorInAccelerationStructure` |
| `Core/Public/Math/Quat.h` | 1373 | `FQuat::FastLerp` (nlerp + dot 부호 보정) |
| `Core/Public/Math/Rotator.h` | 926 / 940 | `TCustomLerp<TRotator>` / `LerpRange` |
| `Core/Public/Math/TransformNonVectorized.h` | 405 | `BlendWith` |
| `Core/Public/Math/TransformVectorized.h` | 466 | `BlendWith` (벡터화) |
| `Engine/Private/TickTaskManager.cpp` | 933 | `ReleaseTickGroup` — 디스패치 먼저, 완료 대기 나중 (C8) |
| | 696-703 | `ShouldConsiderPrerequisite` — "이전 그룹 완료 보장 없음"의 판정 |
| | 2668-2682 | 프리리퀴짓으로 인한 틱 그룹 승격 계산 |
| `Engine/Private/Character.cpp` | 124 | `Mesh->VisibilityBasedAnimTickOption = AlwaysTickPose` (프로젝트가 되돌리는 대상) |

**프로젝트 (`EmploymentProj/Source/EmploymentProj/`)**

| 파일 | 줄 | 내용 |
|---|---|---|
| `Private/Combat/EPServerSideRewindComponent.cpp` | 103-108 | 생성자 — `TG_PostPhysics`, 복제 off |
| | 110-134 | `BeginPlay` — `MaxHistoryCount` 산정, CMC 구독 |
| | 136-154 | `TickComponent` — pending 커밋 |
| | 156-163 | `OnServerMoveProcessed` — pending은 **bool 하나** |
| | 165-195 | `SaveHitboxSnapshot` |
| | 197-256 | `GetSnapshotAtTime` (206/211/231 = 가드 없는 로그) |
| | 277 / 358 | `GetSnapshotAtTime` 이중 호출 |
| | 326-330 | `MaxRewindSeconds` 클램프 |
| | 411-434 | Narrow Phase + `CandidateSet` 필터 (C3) |
| | 418-421 | 가드가 주석 처리된 `DrawDebugSphere` (C4) |
| `Private/Movement/EPCharacterMovement.cpp` | 19-33 | `OnMovementUpdated` — `NewMove` 필터 + 브로드캐스트 |
| `Private/GAS/EPGA_Item_PrimaryUse.cpp` | 49-58 | **`ClientTime`을 서버가 자기 시계로 만든다 (C1)** |
| `Private/Combat/EPCombatComponent.cpp` | 59-98 | `HandleServerFire` |
| | 294-303 | `HandleHitscanFire` → `ConfirmHitscan` |
| `Private/Combat/EPWeapon.cpp` | 69-104 | `Fire` — `ClientFireTime` 미사용 (C6) |
| `Private/Core/EPCharacter.cpp` | 49 / 70 | `AlwaysTickPoseAndRefreshBones`(← `ACharacter`가 낮춘 것을 되돌린다) / `RewindComponent` 생성 |
| `Public/Types/EPTypes.h` | 51-71 / 96 | `FEPBoneSnapshot`·`FEPHitboxSnapshot` / `EP_TraceChannel_Weapon` |
| `Config/DefaultGame.ini` | 23-33 | Combat 설정 + `ClientNetSendMoveDeltaTime` |

---

## 8. 한 줄 결론

**글의 핵심 발견 — "`GetServerWorldTimeSeconds()`는 프레임 안 호출 위치에 따라 값이 다르다" — 은 UE 5.7에서 그대로 맞다.**
줄번호도, 인용도, 원인 추적 서사도 유효하다.

고칠 것은 **주변부 세 가지**(어느 브로드캐스트인가, `FRotator` Lerp, Slerp vs nlerp)와
**숫자 하나**(0.5 vs 0.7), 그리고 **`+4`의 이유**다. 결론을 흔드는 건 하나도 없다.

진짜 문제는 글이 아니라 코드에 있다: **GAS 전환에서 `ClientFireTime`이 유실돼
지연 보상이 사실상 꺼져 있다(§4 C1).** 글이 자랑하는 2.3cm를 지금 다시 재면 나오지 않는다.
포트폴리오를 내기 전에 이것부터 결정해야 한다.
