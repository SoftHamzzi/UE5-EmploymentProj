# Rewind 분리 가이드: `UEPServerSideRewindComponent`

## 목표
- `UEPCombatComponent`에서 리와인드 관련 책임을 분리한다.
- 리와인드 로직의 단일 진입점을 만든다.
- 이후 GAS 전환 시 판정 모듈은 유지하고 데미지 적용만 교체할 수 있게 한다.

---

## 왜 분리하나
1. 현재 `EPCombatComponent.cpp`가 과도하게 비대해짐
2. Hitscan/Projectile 공통으로 쓸 판정 코드를 재사용하기 어려움
3. 디버그(`DrawDebug`)와 데미지 로직이 섞여 버그 추적이 힘듦

---

## 최종 책임 분리

### `UEPCombatComponent`
1. 입력 검증(탄약, 연사속도)
2. 서버 RPC 진입(`Server_Fire`)
3. SSR 컴포넌트 호출
4. 반환 히트에 대한 데미지 계산/적용
5. 이펙트/사운드 재생 동기화

### `UEPServerSideRewindComponent`
1. 히스토리 기록
2. 시점 보간
3. 후보 선정(Broad Phase)
4. 리와인드/복구
5. 최종 판정(Narrow Phase)
6. 디버그 시각화

---

## 파일 배치

### 신규
- `EmploymentProj/Source/EmploymentProj/Public/Combat/EPServerSideRewindComponent.h`
- `EmploymentProj/Source/EmploymentProj/Private/Combat/EPServerSideRewindComponent.cpp`

### 수정
- `EmploymentProj/Source/EmploymentProj/Public/Core/EPCharacter.h`
- `EmploymentProj/Source/EmploymentProj/Private/Core/EPCharacter.cpp`
- `EmploymentProj/Source/EmploymentProj/Public/Combat/EPCombatComponent.h`
- `EmploymentProj/Source/EmploymentProj/Private/Combat/EPCombatComponent.cpp`

---

## 데이터 구조

### 유지할 기존 타입
- `FEPBoneSnapshot` (`EPTypes.h`)
- `FEPHitboxSnapshot` (`EPTypes.h`)
- `EP_TraceChannel_Weapon` (`EPTypes.h`)

### 신규 내부 타입 (`EPServerSideRewindComponent.cpp` 파일 스코프)
```cpp
struct FEPRewindEntry
{
    AEPCharacter* Character = nullptr;
    TArray<FEPBoneSnapshot> SavedBones; // 리와인드 전 월드 트랜스폼 백업
};
```

---

## 컴포넌트 API 설계

`EPServerSideRewindComponent.h` 권장 시그니처:

```cpp
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class EMPLOYMENTPROJ_API UEPServerSideRewindComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    bool ConfirmHitscan(
        AEPCharacter* Shooter,
        AEPWeapon* EquippedWeapon,
        const FVector& Origin,
        const TArray<FVector>& Directions,
        float ClientFireTime,
        TArray<FHitResult>& OutConfirmedHits);

    FEPHitboxSnapshot GetSnapshotAtTime(float TargetTime) const;

private:
    void SaveHitboxSnapshot();
    TArray<AEPCharacter*> GetHitscanCandidates(
        AEPCharacter* Shooter,
        AEPWeapon* EquippedWeapon,
        const FVector& Origin,
        const TArray<FVector>& Directions,
        float ClientFireTime) const;
};
```

포인트:
- `EquippedWeapon`을 넘겨 trace distance/ignore actor를 내부에서 처리
- `OutConfirmedHits`만 반환하고 데미지 계산은 `CombatComponent`에 남김

---

## 이전 순서 (실제 작업 순서)

## Step 1) SSR 컴포넌트 생성
1. 헤더/소스 파일 생성
2. 기존 `AEPCharacter`의 히스토리 멤버/함수 내용을 SSR로 옮김
3. `BeginPlay`에서 `MaxHistoryCount` 계산
4. `TickComponent`에서 `SnapshotInterval`마다 저장

완료 기준:
- SSR 단독 컴파일 통과
- 서버에서 `HitboxHistory`가 증가 로그 확인

## Step 2) Character에서 SSR 멤버 참조 연결
`AEPCharacter.h`:
- `UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")`
  `TObjectPtr<UEPServerSideRewindComponent> ServerSideRewindComponent;`

`AEPCharacter` 생성자:
- `ServerSideRewindComponent = CreateDefaultSubobject<UEPServerSideRewindComponent>(TEXT("ServerSideRewindComponent"));`

`AEPCharacter`에서 제거:
- `HitBones`
- `HitboxHistory`
- `SnapshotAccumulator`
- `MaxHistoryCount`
- `SaveHitboxSnapshot`
- `GetSnapshotAtTime`

완료 기준:
- `AEPCharacter`에 리와인드 관련 상태/함수 직접 구현이 사라짐

## Step 3) CombatComponent 호출 경로 교체
`UEPCombatComponent::HandleHitscanFire`에서 제거:
- 후보군 선정
- 리와인드/복구
- Narrow trace

대체:
```cpp
TArray<FHitResult> ConfirmedHits;
const bool bHasHits = OwnerSSR->ConfirmHitscan(
    Owner, EquippedWeapon, Origin, Directions, ClientFireTime, ConfirmedHits);
```

`UEPCombatComponent`에 남기는 것:
- `GetBoneMultiplier`
- `GetMaterialMultiplier`
- `ApplyPointDamage`

완료 기준:
- `EPCombatComponent.cpp`에서 `FEPRewindEntry` 삭제
- `GetHitscanCandidates`/리와인드 루프가 SSR로 이동

## Step 4) 후보군 외 히트 차단 규칙 고정
SSR 내부 `ConfirmHitscan`에서:
1. 후보군 집합 생성
2. Narrow hit 결과가 후보군에 속한 경우만 `OutConfirmedHits`에 push

이 규칙이 없으면 “현재 상태 fallback 히트”가 섞일 수 있음.

완료 기준:
- 후보군 외 캐릭터는 맞아도 데미지 적용 안 됨

## Step 5) 디버그도 SSR로 이동
`DrawDebug` 로직을 SSR 내부에만 배치:
- Green: 현재
- Red: 리와인드
- Blue: 복구 후
- White: 트레이스
- Yellow: 히트

토글:
- `bDebugRewindPrimitives`
- `DebugDrawDuration`

완료 기준:
- CombatComponent에서 디버그 코드 제거

---

## `ConfirmHitscan` 내부 흐름 (권장)

```cpp
bool UEPServerSideRewindComponent::ConfirmHitscan(..., TArray<FHitResult>& OutConfirmedHits)
{
    // 0) 시간 클램프
    // 1) 후보군 선정
    // 2) 후보군 리와인드 + SavedBones 백업
    // 3) Narrow phase trace
    // 4) 후보군 포함 hit만 OutConfirmedHits에 채택
    // 5) 복구 (무조건 실행)
    // 6) 디버그/로그
    // 7) return OutConfirmedHits.Num() > 0
}
```

필수 안전장치:
- 복구는 `return` 전에 무조건 실행되게 구성
- 가능하면 scope guard 패턴 사용

---

## Build.cs / include 주의

필요 include 예시:
- `EngineUtils.h`
- `Components/CapsuleComponent.h`
- `PhysicsEngine/BodyInstance.h`
- `GameFramework/GameStateBase.h`
- `Combat/EPCombatDeveloperSettings.h`
- `DrawDebugHelpers.h` (디버그 시)

---

## 테스트 계획

## 기능 테스트
1. PIE Dedicated + 2클라
2. 200/200 지연 고정
3. 이동 타깃 30발 테스트
4. `PreHit` 대비 `RewindHit` 명중률 비교

## 로그 최소 세트
- `Delta = ServerNow - ClientFireTime`
- `Clamped`
- `Candidates.Num`
- `ConfirmedHits.Num`
- `Hit.BoneName`, `Hit.PhysMaterial`

---

## 흔한 실패 원인
1. Character에서 선언만 남기고 구현 제거 -> `LNK2019`
2. 후보군 외 히트 필터 누락 -> 예측샷으로도 히트 발생
3. Mesh/Capsule 채널 분리 미흡 -> 의도치 않은 충돌 우선순위
4. `SnapshotInterval` 과대 -> 리와인드 정밀도 저하

---

## GAS 전환 시 유지/교체

유지:
- `UEPServerSideRewindComponent` 전체
- `ConfirmHitscan` 결과 데이터

교체:
- `ApplyPointDamage` -> `GameplayEffectSpec` 적용
- 재질 배율(`bIsWeakSpot`) -> `MaterialTags + WeaponDefinition(MaterialDamageMultiplier)`

---

## 최종 완료 기준
1. `UEPCombatComponent`에서 리와인드 코드 제거 완료
2. SSR 컴포넌트 단일 경로로만 판정 수행
3. 후보군 외 fallback 히트 제거
4. 200/200 지연에서 명중률 개선이 로그/체감으로 확인
