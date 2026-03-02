# 3단계 구현서: NetPrediction (현재 코드 기준)

> 기준 문서: `03_NetPrediction.md`
> 목표: **지금 바로 따라 구현 가능한 순서** 제공
> 범위: Hit Validation + Lag Compensation + Reconciliation
> 비범위: 헤드샷/약점 배율 **구현하지 않음** (언급만 유지)

---

## 0. 구현 철학 (이번 단계)

1. 서버 권한 판정은 유지한다.
2. 클라 체감은 예측으로 보완한다.
3. 판정 시점 불일치는 리와인드로 보정한다.
4. 데미지 모델(헤드샷/재질 태그/GAS)은 다음 단계로 미룬다.

즉, 이번 단계의 핵심은 "얼마나 아픈가"가 아니라 "맞았는가"다.

---

## 1. 수정 대상 파일

- `EmploymentProj/Source/EmploymentProj/Public/Types/EPTypes.h`
- `EmploymentProj/Source/EmploymentProj/Public/Core/EPCharacter.h`
- `EmploymentProj/Source/EmploymentProj/Private/Core/EPCharacter.cpp`
- `EmploymentProj/Source/EmploymentProj/Public/Combat/EPCombatComponent.h`
- `EmploymentProj/Source/EmploymentProj/Private/Combat/EPCombatComponent.cpp`

---

## 2. Step-by-Step

## Step 1) 히스토리 스냅샷 타입 추가

파일: `Public/Types/EPTypes.h`

아래 구조체를 enum 아래쪽에 추가한다.

```cpp
USTRUCT()
struct FEPHitboxSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	float ServerTime = 0.f;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;
};
```

설명:
- 지금 단계는 캡슐/액터 기준 리와인드만 사용한다.
- 본 단위 스냅샷은 `03_BoneHitbox_Implementation.md`에서 확장한다.

---

## Step 2) Character에 히스토리 버퍼 추가

파일: `Public/Core/EPCharacter.h`

### 2-1. include/forward 선언
- `FEPHitboxSnapshot`을 쓰기 위해 `#include "Types/EPTypes.h"`를 추가하거나 forward 선언을 사용한다.

### 2-2. 멤버/함수 추가

```cpp
protected:
	virtual void Tick(float DeltaTime) override;

private:
	static constexpr int32 MaxHitboxHistory = 20; // 100ms * 20 = 약 2초

	UPROPERTY()
	TArray<FEPHitboxSnapshot> HitboxHistory;

	int32 HistoryIndex = 0;
	float HistoryTimer = 0.f;

	void SaveHitboxSnapshot();

public:
	FEPHitboxSnapshot GetSnapshotAtTime(float TargetTime) const;
```

주의:
- `UPROPERTY()`는 GC 안정성/디버깅 가독성 때문에 권장.
- 히스토리는 서버 전용으로만 기록한다.

---

## Step 3) Character.cpp에 기록/보간 구현

파일: `Private/Core/EPCharacter.cpp`

### 3-1. include 추가

```cpp
#include "Types/EPTypes.h"
```

### 3-2. 생성자에서 Tick 보장

이미 Tick이 켜져 있지 않으면 생성자에 아래를 넣는다.

```cpp
PrimaryActorTick.bCanEverTick = true;
```

### 3-3. Tick 구현

```cpp
void AEPCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority()) return;

	HistoryTimer += DeltaTime;
	if (HistoryTimer >= 0.1f)
	{
		HistoryTimer = 0.f;
		SaveHitboxSnapshot();
	}
}
```

### 3-4. SaveHitboxSnapshot 구현

```cpp
void AEPCharacter::SaveHitboxSnapshot()
{
	FEPHitboxSnapshot Snapshot;
	Snapshot.ServerTime = GetWorld()->GetTimeSeconds();
	Snapshot.Location = GetActorLocation();
	Snapshot.Rotation = GetActorRotation();

	if (HitboxHistory.Num() < MaxHitboxHistory)
	{
		HitboxHistory.Add(Snapshot);
	}
	else
	{
		HitboxHistory[HistoryIndex] = Snapshot;
		HistoryIndex = (HistoryIndex + 1) % MaxHitboxHistory;
	}
}
```

### 3-5. GetSnapshotAtTime 구현 (보간)

```cpp
FEPHitboxSnapshot AEPCharacter::GetSnapshotAtTime(float TargetTime) const
{
	const FEPHitboxSnapshot* Before = nullptr;
	const FEPHitboxSnapshot* After = nullptr;

	for (const FEPHitboxSnapshot& Snap : HitboxHistory)
	{
		if (Snap.ServerTime <= TargetTime)
		{
			if (!Before || Snap.ServerTime > Before->ServerTime)
			{
				Before = &Snap;
			}
		}

		if (Snap.ServerTime >= TargetTime)
		{
			if (!After || Snap.ServerTime < After->ServerTime)
			{
				After = &Snap;
			}
		}
	}

	if (!Before && !After)
	{
		FEPHitboxSnapshot Current;
		Current.ServerTime = TargetTime;
		Current.Location = GetActorLocation();
		Current.Rotation = GetActorRotation();
		return Current;
	}

	if (!Before) return *After;
	if (!After) return *Before;
	if (Before == After) return *Before;

	const float Range = After->ServerTime - Before->ServerTime;
	const float Alpha = Range > KINDA_SMALL_NUMBER
		? FMath::Clamp((TargetTime - Before->ServerTime) / Range, 0.f, 1.f)
		: 0.f;

	FEPHitboxSnapshot Result;
	Result.ServerTime = TargetTime;
	Result.Location = FMath::Lerp(Before->Location, After->Location, Alpha);
	Result.Rotation = FQuat::Slerp(
		Before->Rotation.Quaternion(),
		After->Rotation.Quaternion(),
		Alpha).Rotator();

	return Result;
}
```

---

## Step 4) CombatComponent RPC 시그니처 확장

파일: `Public/Combat/EPCombatComponent.h`

기존:
```cpp
UFUNCTION(Server, Reliable)
void Server_Fire(const FVector& Origin, const FVector& Direction);
```

변경:
```cpp
UFUNCTION(Server, Reliable)
void Server_Fire(const FVector_NetQuantize& Origin, const FVector_NetQuantizeNormal& Direction, float ClientFireTime);
```

포인트:
- 방향은 `FVector_NetQuantizeNormal`이 맞다.
- UE5에서는 `_Validate` 패턴 대신 서버 내부 검증 로직으로 처리한다.

---

## Step 5) RequestFire에서 서버 기준 시각 전달

파일: `Private/Combat/EPCombatComponent.cpp`

`RequestFire`에서 서버 시간 기준 발사 시각을 구해서 RPC에 넘긴다.

```cpp
const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
const float ClientFireTime = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();

Server_Fire(Origin, Direction.GetSafeNormal(), ClientFireTime);
```

include 필요:
```cpp
#include "GameFramework/GameStateBase.h"
```

---

## Step 6) Server_Fire에 리와인드 적용

파일: `Private/Combat/EPCombatComponent.cpp`

핵심 흐름은 아래 고정:
1. 입력 검증/탄약 검증
2. Spread 적용 + 탄약 차감
3. LagComp 윈도우 클램프
4. PreTrace로 후보 타깃 1명 찾기
5. 해당 타깃만 리와인드
6. ReTrace 판정
7. 복구
8. 데미지/이펙트 처리

### 구현 골격

```cpp
void UEPCombatComponent::Server_Fire_Implementation(
	const FVector_NetQuantize& Origin,
	const FVector_NetQuantizeNormal& Direction,
	float ClientFireTime)
{
	if (!EquippedWeapon || !EquippedWeapon->CanFire()) return;

	AEPCharacter* Owner = GetOwnerCharacter();
	if (!Owner) return;

	FVector SpreadDir = Direction;
	EquippedWeapon->Fire(SpreadDir);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ServerFire), false);
	Params.AddIgnoredActor(Owner);
	Params.AddIgnoredActor(EquippedWeapon);

	const FVector Start = Origin;
	const FVector End = Start + SpreadDir * 10000.f;

	const AGameStateBase* GS = GetWorld()->GetGameState();
	const float ServerNow = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float MaxLagCompWindow = 0.2f;
	if (ServerNow - ClientFireTime > MaxLagCompWindow)
	{
		ClientFireTime = ServerNow;
	}

	FHitResult PreHit;
	GetWorld()->LineTraceSingleByChannel(PreHit, Start, End, ECC_GameTraceChannel1, Params);

	AEPCharacter* HitChar = Cast<AEPCharacter>(PreHit.GetActor());
	bool bConfirmedHit = false;
	FHitResult FinalHit = PreHit;

	if (HitChar)
	{
		const FVector OriginalLocation = HitChar->GetActorLocation();
		const FRotator OriginalRotation = HitChar->GetActorRotation();

		const FEPHitboxSnapshot Snapshot = HitChar->GetSnapshotAtTime(ClientFireTime);
		HitChar->SetActorLocationAndRotation(
			Snapshot.Location,
			Snapshot.Rotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		FHitResult RewindHit;
		bConfirmedHit = GetWorld()->LineTraceSingleByChannel(
			RewindHit, Start, End, ECC_GameTraceChannel1, Params)
			&& RewindHit.GetActor() == HitChar;

		if (bConfirmedHit)
		{
			FinalHit = RewindHit;
		}

		HitChar->SetActorLocationAndRotation(
			OriginalLocation,
			OriginalRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}
	else
	{
		bConfirmedHit = PreHit.bBlockingHit;
	}

	if (bConfirmedHit && FinalHit.GetActor())
	{
		UGameplayStatics::ApplyDamage(
			FinalHit.GetActor(),
			EquippedWeapon->GetDamage(),
			Owner->GetController(),
			Owner,
			nullptr);
	}

	// 이펙트는 기존 정책 유지
	const FVector MuzzleLocation =
		EquippedWeapon->WeaponMesh->DoesSocketExist(TEXT("MuzzleSocket"))
		? EquippedWeapon->WeaponMesh->GetSocketLocation(TEXT("MuzzleSocket"))
		: EquippedWeapon->GetActorLocation();

	Multicast_PlayMuzzleEffect(MuzzleLocation);
	if (bConfirmedHit)
	{
		Multicast_PlayImpactEffect(FinalHit.ImpactPoint, FinalHit.ImpactNormal);
	}
}
```

중요:
- 이 단계에서는 `ApplyPointDamage`로 바꾸지 않아도 된다.
- 헤드샷 구현은 Chapter 4 직전에 한 번에 정리한다.

---

## Step 7) 최소 Reconciliation 훅 넣기

현재 구조에서는 발사 VFX/SFX가 Multicast로만 보인다.
체감 개선을 위해 다음 원칙을 유지한다.

- 로컬 플레이어:
  - 클릭 즉시 카메라 반동/로컬 발사감 유지 (이미 일부 구현됨)
- 서버 확정:
  - 히트 여부 UI(히트마커 등)는 서버 확정 신호로만 표시

이 단계에서 최소 구현만 한다면:
- 기존 `Client_PlayHitConfirmSound`를 "서버 확정 히트"에만 실행되도록 유지하면 충분하다.

---

## 3. 테스트 절차 (반드시 멀티로)

1. PIE 설정:
- Dedicated Server ON
- Client 2~3명

2. 테스트 케이스:
- 낮은 핑 환경: 기존과 동일하게 맞는지
- 높은 핑 환경(네트워크 에뮬): 조준 시점 기준 히트 체감 개선되는지
- `MaxLagCompWindow` 초과 상황: 과도한 과거 판정이 허용되지 않는지

3. 확인 로그:
- `ServerNow`, `ClientFireTime`, `Delta`
- `PreHitActor`, `RewindHitActor`, `bConfirmedHit`

---

## 4. 트러블슈팅

1. 캐릭터가 안 맞는 경우
- Physics Asset에서 `ECC_GameTraceChannel1 = Block` 확인
- `Params.AddIgnoredActor`에 대상이 잘못 들어가 있지 않은지 확인

2. 리와인드 후 위치가 틀어지는 경우
- 복구 코드 누락 확인
- `ETeleportType::TeleportPhysics` 사용 여부 확인

3. 히트는 되는데 체감이 이상한 경우
- PreHit/FinalHit 혼용 여부 확인
- Impact 이펙트가 어떤 Hit를 기준으로 나가는지 일관성 확인

---

## 5. 다음 단계 연결

이 단계 완료 후 Chapter 4(GAS)에서 할 일:
1. `ApplyDamage` -> GAS Effect 적용
2. 헤드샷/약점(태그/재질) 배율 계산 추가
3. 무기 정의 데이터(`MaterialDamageMultiplier`) 연동

즉, 지금 만든 NetPrediction/LagComp 파이프라인은 그대로 유지하고, 데미지 계산기만 교체한다.
