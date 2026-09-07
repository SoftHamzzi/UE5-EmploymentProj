# 서버의 이동 처리가 끊겨 들어오는 것을 눈으로 보기

**목적:** `OnMovementUpdated`로 이동 묶음이 뚝뚝 끊겨 도착하는 것을,
`DrawBodyAggGeomPrimitives`처럼 히트박스를 찍어서 시각화한다.
**작성:** 2026-09-06 · 코드 직독 기준

> 이 문서가 재현하려는 것은 `DOCS/Notes/03/03_BoneHitbox.md`와 포트폴리오 §2-1의
> *"②③은 시각만 다르고 위치는 ①과 동일. ④에서 300만큼 점프"* 표다. 말로 적힌 그 표를
> 화면에서 직접 보이게 만드는 것.

---

## 1. 먼저 알아야 할 함정 셋

### 함정 ① `OnMovementUpdated` 시점에는 본이 아직 이번 프레임 값이 아니다

이게 이 프로젝트가 스냅샷을 `TG_PostPhysics`까지 미루는 이유 그 자체다:

```cpp
// EPServerSideRewindComponent.cpp:158-165
void UEPServerSideRewindComponent::OnServerMoveProcessed(float Time, FVector Location)
{
    // TickDispatch 시점 — 본 Transform이 아직 갱신되지 않았으므로 값만 보관.
    // 실제 스냅샷 저장은 PostPhysics Tick(본 갱신 완료 후)에서 수행.
    bHasPendingSnapshot = true;
    ...
}
```

**여기서 `DrawHitBonesPrimitivesForCharacter`를 그냥 부르면 직전 프레임의 본 포즈가 그려진다.**
`FBodyInstance::GetUnrealWorldTransform()`을 읽는 구조이기 때문이다
(`EPServerSideRewindComponent.cpp:40`).

**단, 캡슐 루트 위치는 이 시점에 이미 유효하다** — 이동이 방금 적용됐으니까. 그래서
델리게이트가 `Location`을 같이 넘긴다. 이 차이가 아래 안 A와 안 B를 가른다.

### 함정 ② 두 그리기 함수는 파일 로컬(`static`)이다

`DrawBodyAggGeomPrimitives`(28행)와 `DrawHitBonesPrimitivesForCharacter`(71행) 모두
`EPServerSideRewindComponent.cpp` 안의 `static` 자유 함수다. **다른 `.cpp`에서 못 부른다.**

→ 그리는 코드는 **SSR 컴포넌트 안**에 두는 게 맞다. 이미 델리게이트를 구독하고 있으니
(`BeginPlay`, 132-135행) 자리도 자연스럽다. `EPCharacterMovement.cpp`에 넣으려 하지 말 것.

### 함정 ③ 서버 전용이라 데디케이티드에서는 안 보인다

브로드캐스트가 authority 가드 뒤에 있다:

```cpp
// EPCharacterMovement.cpp:27
if (!GetOwner()->HasAuthority()) return;
```

데디케이티드 서버는 렌더링을 안 하므로 `DrawDebug*`가 화면에 안 나온다.
**PIE에서 "Play As Listen Server"로 띄우고, 서버 창에서 다른 클라의 캐릭터를 봐야 한다.**

---

## 2. 안 A — 캡슐만, 즉시 (가장 단순. 함정 ①에 안 걸림)

`OnServerMoveProcessed()` 안에서 바로 그린다. 이 시점의 위치는 유효하므로 정확하다.

```cpp
// EPServerSideRewindComponent.cpp — OnServerMoveProcessed 끝에 추가
void UEPServerSideRewindComponent::OnServerMoveProcessed(float Time, FVector Location)
{
    bHasPendingSnapshot = true;
    PendingSnapshotTime = Time;
    PendingSnapshotLocation = Location;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();
    if (CombatSettings->bEnableSSRDebugDraw)
    {
        if (const AEPCharacter* Char = Cast<AEPCharacter>(GetOwner()))
        {
            if (const UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
            {
                DrawDebugCapsule(GetWorld(), Location,
                    Capsule->GetScaledCapsuleHalfHeight(), Capsule->GetScaledCapsuleRadius(),
                    Char->GetActorQuat(), FColor::Green, false, 5.f, 0, 1.f);
            }
        }
    }
#endif
}
```

- **`GetActorLocation()`을 다시 읽지 말고 인자로 받은 `Location`을 쓴다** — §2-1 해결 2가
  세운 원칙("관련된 값은 같은 시점에 묶어 커밋한다")을 여기서도 지키는 것
- **지속시간을 길게(5초) 준다.** 연속된 캡슐이 공중에 남아 **간격 자체가 그림이 된다.**
  이게 보고 싶은 것이다
- 새 `#include`가 필요 없다 — `CapsuleComponent.h`(7행), `DrawDebugHelpers.h`(9행) 둘 다 이미 있다

## 3. 안 B — 본 바디까지 (원하는 그림에 가장 가까움)

이미 있는 pending 구조를 그대로 쓴다. **PostPhysics까지 미뤄졌으므로 본이 이번 프레임 확정값이다.**

```cpp
// EPServerSideRewindComponent.cpp:151-155 — 커밋 블록에 추가
if (bHasPendingSnapshot)
{
    SaveHitboxSnapshot(PendingSnapshotTime, PendingSnapshotLocation);
    bHasPendingSnapshot = false;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    const UEPCombatDeveloperSettings* CombatSettings = GetDefault<UEPCombatDeveloperSettings>();
    if (CombatSettings->bEnableSSRDebugDraw)
    {
        DrawHitBonesPrimitivesForCharacter(GetWorld(), OwnerChar, HitBones,
            FColor::Green, 5.f, CombatSettings->SSRDebugLineThickness);
    }
#endif
}
```

- **기존 함수를 그대로 재사용한다.** 추가 코드가 사실상 한 줄
- **이동 패킷이 도착한 프레임에만 그려진다.** 안 도착한 프레임엔 아무것도 안 그려지고,
  그 공백이 곧 "끊김"이다
- **색은 `Green`이 비어 있다** — 기존 배색: Blue(현재 물리) / Red(리와인드 후) /
  White(트레이스 선) / Yellow(확정 히트) (`ConfirmHitscan`, 402·420·438·451행)

## 4. 간격을 숫자로 같이 찍기 (이게 §2-1 표를 그대로 재현한다)

그림만으로는 "얼마나" 끊겼는지 안 보인다. 히스토리 마지막 둘을 비교해 거리와 시간차를 띄운다.

```cpp
// SaveHitboxSnapshot() 끝, HitboxHistory.Add(Snapshot) 다음
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    if (GetDefault<UEPCombatDeveloperSettings>()->bEnableSSRDebugLog && HitboxHistory.Num() >= 2)
    {
        const FEPHitboxSnapshot& Prev = HitboxHistory[HitboxHistory.Num() - 2];
        const FEPHitboxSnapshot& Curr = HitboxHistory.Last();
        const float Dist = FVector::Dist(Prev.Location, Curr.Location);
        const float Dt   = Curr.ServerTime - Prev.ServerTime;

        UE_LOG(LogTemp, Log, TEXT("[MoveBurst] dt=%.4f dist=%.1fcm speed=%.0f"),
            Dt, Dist, Dt > KINDA_SMALL_NUMBER ? Dist / Dt : 0.f);

        DrawDebugString(GetWorld(), Curr.Location + FVector(0, 0, 100.f),
            FString::Printf(TEXT("%.0fcm / %.3fs"), Dist, Dt), nullptr, FColor::Green, 5.f);
    }
#endif
```

**읽는 법:** `dt`가 거의 일정한데 `dist`가 0에 가깝다가 갑자기 커지면 그게 묶음 도착이다.
등속으로 달리는데 `speed`가 0 ↔ 큰 값으로 튀면 §2-1이 말한 *"멈췄다가 순간이동"*이 재현된 것.

> `bEnableSSRDebugLog`는 이미 ini에 `True`다(`DefaultGame.ini`).

---

## 5. 테스트 방법

1. PIE를 **Play As Listen Server**, 플레이어 2인으로 띄운다 (데디케이티드는 화면이 없어 안 보임 — 함정 ③)
2. `bEnableSSRDebugDraw = True` 확인 (`DefaultGame.ini` `[/Script/EmploymentProj.EPCombatDeveloperSettings]`, 이미 True)
3. **클라이언트 창에서 캐릭터를 직선으로 달리게 하고, 서버 창에서 그 캐릭터를 본다**
4. 네트워크를 나쁘게 만들수록 간격이 커진다 — 콘솔에서 `Net PktLag=200`, `Net PktLoss=5` 등
5. 지금 구조(`NewMove` 가드 + PostPhysics 커밋)에서는 간격이 **일정**해야 정상이다.
   불규칙하게 벌어지면 그게 §2-1이 고친 그 버그의 재발이다

---

## 6. 안 C — 히스토리 전체를 그리기 (다른 목적. 지금은 불필요)

"판정에 실제로 쓰이는 값"을 보고 싶다면 라이브 바디가 아니라 `HitboxHistory`의
`FEPBoneSnapshot::WorldTransform`(`EPTypes.h:52-61`)을 직접 그려야 한다.

문제는 `DrawBodyAggGeomPrimitives`가 transform을 **`FBodyInstance`에서 꺼내 쓴다**는 것이다
(`Body->GetUnrealWorldTransform()`, 40행). 임의 transform으로 그리려면 인자로 받는 형태가 필요하다:

```cpp
// transform을 인자로 받는 형태로 분리하고, 기존 함수는 여기에 위임
static void DrawAggGeomAt(UWorld* World, const FBodyInstance* Body, const FTransform& BodyWorld,
                          const FColor& Color, float Duration, float Thickness);
```

**지금은 필요 없다.** `ConfirmHitscan`은 바디를 실제로 리와인드 위치로 옮긴 뒤 그리므로
현재 함수로 충분하다(405-421행). 히스토리를 옮기지 않고 통째로 훑어보고 싶어질 때만 하면 된다.

---

## 7. 권장 순서

1. **안 A**로 "간격이 존재한다"를 먼저 확인한다 (한 블록, 되돌리기 쉬움)
2. **§4의 숫자**를 붙여 얼마나 끊기는지 잰다 — 여기서 대부분의 답이 나온다
3. 본 단위 그림이 필요하면 **안 B**를 켠다
4. 안 C는 히스토리 검증이 별도로 필요해질 때

세 안 모두 실링/테스트 빌드에서는 빠져야 한다. 기존 코드는 `ConfirmHitscan`에서
**플래그를 죽이는 형태**로 같은 일을 한다(`EPServerSideRewindComponent.cpp:350-353`):

```cpp
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
    bDebugDraw = false;
    bDebugLog = false;
#endif
```

위 예시들은 코드 블록 자체를 빼는 `#if !(...)` 형태로 썼다. 둘 다 결과는 같으니,
`ConfirmHitscan`처럼 플래그를 한 번 계산해 두고 쓰는 쪽이 취향에 맞으면 그렇게 맞춰도 된다.

---

## 8. 참고

- `DOCS/Notes/03/03_BoneHitbox_Implementation.md` — SSR 구현 단계별 문서
- `DOCS/Mine/ServerSideRewind.md` — 엔진 직독 검증 기록
- `EPServerSideRewindComponent.cpp:28-87` — 두 그리기 함수
- `EPServerSideRewindComponent.cpp:138-165` — pending 커밋과 델리게이트 수신부
- `EPCharacterMovement.cpp:22-36` — `OnMovementUpdated`와 `NewMove` 가드
