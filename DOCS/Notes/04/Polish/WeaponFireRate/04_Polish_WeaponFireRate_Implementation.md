# 04 Polish 구현서: 무기 발사 (발사 간격 · TargetData 발사 전송 · 원점 동기 · 재장전 태그 · 어빌리티 배칭)

> 기준 문서: `04_Polish_WeaponFireRate_STATUS.md` §1 (결정), `../04_Polish_WeaponFireRate.md` §4 (설계 이유)
> 전제 조건: `feature-gas-polish` 브랜치, 스킬 구현(`ea08cfc`) 빌드 통과, `FEPLocalTimer`·`FEPLocalModifiers` 존재
> 목표: **현재 코드에서 바로 따라 구현 가능한 순서** 제공. 코드는 사용자가 직접 작성한다
> 문서 우선순위: 구현 시 충돌하면 이 문서 → STATUS → 설계 문서 순
> 2026-09-21: UT식 원점 동기(타임스탬프) 반영 — Step 2·4·6·8·15 변경
> 2026-09-22: 탄약 예측 반영 — Step 9 `FireOnce`·`SendFireTargetData`, §15 15번, 함정표

---

## 0. 구현 철학

1. **발사 시각은 클라가 정하고, 서버는 개수만 자른다** — 페이싱은 클라 `FEPLocalTimer`, 검증은 서버 토큰 버킷. 서버는 발사를 미루지 않는다(SSR 되감기 시각과 어긋남)
2. **방향은 클라가, 원점은 서버가 — 단 "어느 무브의 위치인지"는 클라가 알려준다** — 페이로드는 `Direction` + `ClientMoveTimeStamp`. 서버 SSR 컴포넌트가 무브마다 카메라 위치를 기억해 두고 타임스탬프로 꺼낸다(UT `SavedPositions` — "과거 위치"의 주인은 하나). 없으면 현재 위치로 폴백, 기다리지 않는다
3. **발사 전송은 GAS TargetData** — 커스텀 RPC 없음. `CallServerSetReplicatedTargetData` → `AbilityTargetDataSetDelegate`
4. **`Auto`/`Single` 한 수명 경로** — 발사 → 다음 발 재예약 → 틱에서 (`Auto` 또는 예약) 계속 / 아니면 종료. `Single`+연타 = 2발짜리 `Auto`
5. **시계는 `World->GetTimeSeconds()`, 절대 시각은 `double`** — 값이 기계를 건너가지 않는다. 남은 시간·차이는 `float`
6. **배칭은 마지막에, 따로** — Step 1~13으로 동작 확인 후 Step 14. 서브클래스 하나 되돌리면 배칭 전 상태

### 하드코딩 금지

| 값 | 위치 |
|---|---|
| 기본 발사 속도 | `WeaponDef->FireRate` (기존) → `AEPWeapon::GetBaseFireRate()` |
| 발사 속도 배율 | `AEPCharacter::LocalModifiers` `Modifier.FireRate.*` (스킬과 같은 저장소) |
| 서버 버킷 상한 | `UEPCombatDeveloperSettings::FireRateBurstAllowance` (ini) |
| 원점 히스토리 크기 | `UEPCombatDeveloperSettings::ShotOriginHistoryCount` (ini) |
| 발사 모드 | `WeaponDef->FireMode` (기존) |

---

## 1. 수정/추가 대상 파일

**신규:**
- `Public/Combat/EPRateLimiter.h` (헤더 전용)
- `Public/GAS/EPTargetData_Fire.h` (헤더 전용, USTRUCT)
- Step 14: `Public/GAS/EPAbilitySystemComponent.h`, `Private/GAS/EPAbilitySystemComponent.cpp`

**수정:**
- `Public/GAS/EPNativeGameplayTags.h`, `Private/GAS/EPNativeGameplayTags.cpp`
- `Public/Combat/EPCombatDeveloperSettings.h`
- `Public/GAS/EPLocalTimer.h` (시각 `double`)
- `Public/Combat/EPWeapon.h`, `Private/Combat/EPWeapon.cpp`
- `Public/Movement/EPCharacterMovement.h`, `Private/Movement/EPCharacterMovement.cpp` (델리게이트 인자 추가, 모든 무브에서 발행)
- `Public/Combat/EPServerSideRewindComponent.h`, `Private/Combat/EPServerSideRewindComponent.cpp` ← 원점 히스토리
- `Public/Combat/EPCombatComponent.h`, `Private/Combat/EPCombatComponent.cpp`
- `Public/GAS/EPGA_Item_PrimaryUse.h`, `Private/GAS/EPGA_Item_PrimaryUse.cpp` ← 핵심
- `Private/Core/EPCharacter.cpp` (`Input_Fire`, `Input_StopFire`)
- `Public/GAS/EPGA_Item_Reload.h`, `Private/GAS/EPGA_Item_Reload.cpp`
- Step 14: `Private/Core/EPPlayerState.cpp`

**에디터:**
- `BP_GA_Item_PrimaryUse` — `CooldownGameplayEffectClass` 비우기
- `BP_GA_Item_Reload` — 사라진 `GE_ReloadingClass` 정리
- `GE_FireCooldown`, `GE_Reloading` — 참조 끊고 삭제
- `Config/DefaultGame.ini` — `FireRateBurstAllowance`, `ShotOriginHistoryCount` (선택)

**변경 없음 (확인용):** `EPHUDWidget`(재장전 표시는 태그 카운터만 봄), SSR의 `SaveHitboxSnapshot`/`GetSnapshotAtTime`/`ConfirmHitscan`, `HandleHitscanFire`/`HandleProjectileFire` 본문, `EPAttributeSet`, `EPWeaponDefinition`

---

## Step 1) `FEPRateLimiter` — `Public/Combat/EPRateLimiter.h`

```cpp
#pragma once

#include "CoreMinimal.h"

/**
 * 서버 발사 검증용 토큰 버킷. 복제하지 않는다 — 서버 시계 값끼리만 뺀다.
 * 발사 시각은 건드리지 않고 개수만 제한한다: 순간 몰림은 MaxTokens까지 허용, 지속 속도는 RefillPerSecond로 상한.
 * 리필 속도를 저장하지 않는 이유: 발사 속도 배율이 버프로 바뀌므로 호출자가 매번 유효 발사 속도를 넘긴다.
 * 절대 시각은 double — float는 월드 시간이 커질수록 정밀도가 떨어진다 (UWorld::GetTimeSeconds()도 double).
 */
struct FEPRateLimiter
{
	void Reset(double Now, float InMaxTokens)
	{
		MaxTokens  = FMath::Max(1.f, InMaxTokens);
		Tokens     = MaxTokens;
		LastUpdate = Now;
	}

	/** 지난 시간만큼 채운 뒤, 1 이상이면 하나 쓰고 true. 아니면 false — 호출자는 이번 요청을 버린다. */
	bool TryTake(double Now, float RefillPerSecond)
	{
		const float Elapsed = static_cast<float>(Now - LastUpdate);
		Tokens     = FMath::Min(MaxTokens, Tokens + Elapsed * FMath::Max(0.f, RefillPerSecond));
		LastUpdate = Now;
		if (Tokens < 1.f) return false;
		Tokens -= 1.f;
		return true;
	}

private:
	float  Tokens     = 0.f;
	float  MaxTokens  = 1.f;
	double LastUpdate = 0.0;
};
```

`HasTokenAvailable`(소비 없는 피크)은 설계 문서 §4-8 범위 밖 — 지금 넣지 않는다.

**`FEPLocalTimer`도 같은 이유로** `LastUpdate`·`PrevLastUpdate`를 `double`, `Start/Bank/GetRemaining/IsElapsed`의
`Now`를 `double`로 바꾼다. `Remaining`(남은 시간)은 `float` 그대로. 스킬 쪽 호출자는 `GetTimeSeconds()`가
이미 `double`이라 그대로 컴파일된다.

---

## Step 2) `FEPTargetData_Fire` — `Public/GAS/EPTargetData_Fire.h`

에디터 New C++ Class 마법사에는 안 나온다(`USTRUCT`) — 파일을 직접 만든다.

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "EPTargetData_Fire.generated.h"

/**
 * 발사 한 발의 페이로드. 원점 없음(서버가 계산), 순번 없음(Reliable).
 * ClientMoveTimeStamp: 클라가 쏜 순간 마지막으로 완료된 세이브드 무브의 타임스탬프 — 서버가 그 무브 결과의 카메라 위치를 원점으로 쓴다.
 * 호스트는 -1 (히스토리 없음, 현재 위치).
 */
USTRUCT()
struct EMPLOYMENTPROJ_API FEPTargetData_Fire : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	UPROPERTY()
	float ClientMoveTimeStamp = -1.f;

	virtual UScriptStruct* GetScriptStruct() const override { return FEPTargetData_Fire::StaticStruct(); }
	virtual FString ToString() const override { return TEXT("FEPTargetData_Fire"); }

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		Direction.NetSerialize(Ar, Map, bOutSuccess);
		Ar << ClientMoveTimeStamp;
		bOutSuccess = true;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FEPTargetData_Fire> : public TStructOpsTypeTraitsBase2<FEPTargetData_Fire>
{
	enum
	{
		WithNetSerializer = true   // FGameplayAbilityTargetDataHandle 직렬화에 필수 (엔진 주석 "REQUIRED", GameplayAbilityTargetTypes.h:384)
	};
};
```

`GetScriptStruct()`가 없으면 서버가 베이스 타입으로 역직렬화해 필드가 사라진다 — 잊지 말 것.
타임스탬프는 `float` — `FSavedMove_Character::TimeStamp`와 같은 타입, 서버가 받은 값과 비트 단위로 같다.

---

## Step 3) 태그 — `EPNativeGameplayTags.h/.cpp`

**추가** (`// Modifier` 블록):
```cpp
// .h
EMPLOYMENTPROJ_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Modifier_FireRate)
// .cpp
UE_DEFINE_GAMEPLAY_TAG(TAG_Modifier_FireRate, "Modifier.FireRate")
```

**삭제** (코드 소비자 없음 — 2026-09-20 `grep -rn "State_FireCooldown\|Cooldown_Weapon_PrimaryUse\|State.FireCooldown\|Cooldown.Weapon" Source Config` 결과 정의뿐):
- `TAG_State_FireCooldown` (`.h:13`, `.cpp:12`)
- `TAG_Cooldown_Weapon_PrimaryUse` (`.h:33`, `.cpp:33`)

`GE_FireCooldown` 에셋이 `Cooldown.Weapon.PrimaryUse`를 GrantedTags로 들고 있다 — Step 12에서 에셋을 지우기 **전에** 태그를 지우면 에셋 로드 경고가 난다. 순서: 에셋 참조 정리 → 태그 삭제 → 빌드.

---

## Step 4) 설정 — `EPCombatDeveloperSettings.h`

```cpp
	// 서버 발사 검증 토큰 버킷 상한. 정직한 클라의 패킷 몰림 허용치 — 1이면 몰림 불허, 2면 두 발까지 동시 도착 허용
	UPROPERTY(Config, EditAnywhere, Category="GAS|FireRate", meta=(ClampMin="1", ClampMax="4"))
	float FireRateBurstAllowance = 2.f;

	// 서버가 무브별 카메라 위치를 기억하는 개수 (발사 원점 동기). 64 ≈ 60fps 1초. RTT가 길거나 fps가 높으면 늘린다
	UPROPERTY(Config, EditAnywhere, Category="LagComp", meta=(ClampMin="16", ClampMax="256"))
	int32 ShotOriginHistoryCount = 64;
```

---

## Step 5) `AEPWeapon`

### 5-1. `EPWeapon.h`

```cpp
#include "GAS/EPLocalTimer.h"          // ←
#include "Combat/EPRateLimiter.h"      // ←

public:
	// --- 발사 간격 (복제 X) ---
	FEPLocalTimer&       GetFireTimer()       { return FireTimer; }
	const FEPLocalTimer& GetFireTimer() const { return FireTimer; }   // CanActivateAbility(const)의 IsElapsed 읽기용
	FEPRateLimiter&      GetFireLimiter()     { return FireLimiter; } // 서버 TryTake(쓰기)뿐 → const 버전 불필요
	/** 배율 적용 전 발사 속도(발/초). PrimaryUse의 static GetFireInterval을 대체 */
	float GetBaseFireRate() const { return (WeaponDef && WeaponDef->FireRate > 0.f) ? WeaponDef->FireRate : 5.f; }

private:
	FEPLocalTimer  FireTimer;     // 오너 클라·호스트의 페이싱. 남은 시간 모델
	FEPRateLimiter FireLimiter;   // 서버 검증 버킷
```

`ApplySpread` 선언 **삭제** (호출자 없음, 설계 문서 §7).

### 5-2. `EPWeapon.cpp`

```cpp
#include "Combat/EPCombatDeveloperSettings.h"   // ←

void AEPWeapon::BeginPlay()
{
	Super::BeginPlay();
	BuildSpreadCDFTable();

	if (HasAuthority())
	{
		const UEPCombatDeveloperSettings* Settings = GetDefault<UEPCombatDeveloperSettings>();
		FireLimiter.Reset(GetWorld()->GetTimeSeconds(), Settings->FireRateBurstAllowance);
	}
}
```

- `ApplySpread()` 정의 **삭제**
- `CalculateSpread()`의 `UE_LOG(LogTemp, Log, TEXT("%.3f"), Spread);` **삭제** (발사마다 서버에서 돌던 디버그 로그)

---

## Step 6) 원점 히스토리 — CMC 델리게이트 확장 + `UEPServerSideRewindComponent`

"이 캐릭터의 과거 위치"의 주인은 SSR 컴포넌트 하나다(UT `SavedPositions`). CMC는 무브가 적용됐음을
알리기만 하고, SSR 컴포넌트가 두 배열을 든다 — 되감기용 본 스냅샷(기존, NewMove만, PostPhysics)과
발사 원점용 카메라 위치(신규, **모든 무브**, 동기). 배열이 둘인 이유: 본 트랜스폼은 애니 평가 뒤에야
맞고 번들의 마지막 무브에서만 찍을 수 있지만, 원점은 무브마다 필요하고 카메라 위치 하나면 된다.

### 6-1. `EPCharacterMovement.h` — 델리게이트 인자 추가

```cpp
// 서버가 무브 하나를 처리할 때마다 발행 (Old/Pending/New 전부). bNewMove = 묶음의 마지막 무브
DECLARE_MULTICAST_DELEGATE_FourParams(FEPOnServerMoveProcessed,
	float /*ServerTime*/, FVector /*Location*/, float /*ClientTimeStamp*/, bool /*bNewMove*/);
```
멤버·오버라이드 선언은 그대로.

### 6-2. `EPCharacterMovement.cpp` — 모든 무브에서 브로드캐스트

```cpp
void UEPCharacterMovement::OnMovementUpdated(
	float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	if (!GetOwner()->HasAuthority()) return;

	// 호스트는 MoveData가 없다(ServerMove를 안 탄다) → 아무것도 안 함
	const FCharacterNetworkMoveData* MoveData = GetCurrentNetworkMoveData();
	if (!MoveData) return;

	const bool bNewMove = MoveData->NetworkMoveType == FCharacterNetworkMoveData::ENetworkMoveType::NewMove;

	const AGameStateBase* GS = GetWorld()->GetGameState<AGameStateBase>();
	const float T = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();

	OnServerMoveProcessed.Broadcast(T, GetActorLocation(), MoveData->TimeStamp, bNewMove);
}
```
바뀐 건 "NewMove가 아니면 return"이 사라지고 인자 둘이 는 것뿐. 주석(`// 서버가 RPC 묶음 … NewMove에서만 호출됨`)도 고친다.

### 6-3. `EPServerSideRewindComponent.h`

```cpp
public:
	/** 서버: 클라 무브 타임스탬프 → 그 무브 결과의 카메라 위치. 없으면 false (무브 미도착 / 타임스탬프 리셋 직후 / 호스트) */
	bool GetShotOriginAt(float ClientTimeStamp, FVector& OutOrigin) const;

protected:
	void OnServerMoveProcessed(float ServerTime, FVector Location, float ClientTimeStamp, bool bNewMove);   // 시그니처 변경

private:
	// --- 발사 원점 히스토리 (서버 전용, 복제 X) — UT SavedPositions. 되감기 스냅샷(HitboxHistory)과 별도 ---
	struct FEPShotOriginEntry
	{
		float   TimeStamp = -1.f;
		FVector Origin    = FVector::ZeroVector;
	};
	TArray<FEPShotOriginEntry> ShotOriginHistory;   // 링버퍼, 크기 = Settings->ShotOriginHistoryCount
	int32 ShotOriginNext = 0;

	void RecordShotOrigin(float ClientTimeStamp, const FVector& Origin);
```

### 6-4. `EPServerSideRewindComponent.cpp`

```cpp
#include "Camera/CameraComponent.h"   // ←

void UEPServerSideRewindComponent::BeginPlay()
{
	// ... 기존 그대로 (MaxHistoryCount 계산, CMC 구독) ...
	ShotOriginHistory.SetNum(FMath::Max(16, CombatSettings->ShotOriginHistoryCount));   // ← 한 번 할당
	ShotOriginNext = 0;
}

void UEPServerSideRewindComponent::OnServerMoveProcessed(float ServerTime, FVector Location, float ClientTimeStamp, bool bNewMove)
{
	// 발사 원점 — 매 무브, 지금 즉시. 캡슐이 방금 움직였으므로 붙어 있는 카메라의 월드 위치도 갱신돼 있다
	if (const AEPCharacter* OwnerChar = Cast<AEPCharacter>(GetOwner()))
		if (const UCameraComponent* Cam = OwnerChar->GetCameraComponent())
			RecordShotOrigin(ClientTimeStamp, Cam->GetComponentLocation());

	// 되감기 스냅샷 — 묶음의 마지막 무브만, 본 Transform이 갱신되는 PostPhysics에서 커밋 (기존 동작)
	if (bNewMove)
	{
		bHasPendingSnapshot     = true;
		PendingSnapshotTime     = ServerTime;
		PendingSnapshotLocation = Location;
	}
}

void UEPServerSideRewindComponent::RecordShotOrigin(float ClientTimeStamp, const FVector& Origin)
{
	if (ShotOriginHistory.IsEmpty()) return;
	ShotOriginHistory[ShotOriginNext] = { ClientTimeStamp, Origin };
	ShotOriginNext = (ShotOriginNext + 1) % ShotOriginHistory.Num();
}

bool UEPServerSideRewindComponent::GetShotOriginAt(float ClientTimeStamp, FVector& OutOrigin) const
{
	if (ClientTimeStamp < 0.f || ShotOriginHistory.IsEmpty()) return false;

	// 최신부터 거슬러 — 보통 첫 몇 개 안에서 맞는다
	const int32 Num = ShotOriginHistory.Num();
	for (int32 i = 1; i <= Num; ++i)
	{
		const FEPShotOriginEntry& Entry = ShotOriginHistory[(ShotOriginNext - i + Num) % Num];
		if (Entry.TimeStamp < 0.f) break;                                              // 아직 안 채워진 칸
		if (FMath::IsNearlyEqual(Entry.TimeStamp, ClientTimeStamp, KINDA_SMALL_NUMBER))
		{
			OutOrigin = Entry.Origin;
			return true;
		}
	}
	return false;
}
```

`SaveHitboxSnapshot`·`GetSnapshotAtTime`·`ConfirmHitscan`은 손대지 않는다.

---

## Step 7) `UEPCombatComponent` — 원점을 히스토리에서, 발사 RPC 삭제

### 7-1. `EPCombatComponent.h`

```cpp
	/** 서버. ClientMoveTimeStamp: 페이로드 값. 히스토리에 없으면(호스트 -1 포함) 현재 카메라 위치 */
	void HandleServerFire(const FVector& Direction, float ClientMoveTimeStamp);   // Origin 매개변수 삭제

	// 삭제:
	// UFUNCTION(Server, Reliable)
	// void Server_ConfirmFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction, FGameplayAbilitySpecHandle AbilityHandle);
```

`#include "GameplayAbilitySpecHandle.h"`는 `GrantedWeaponAbilityHandles`가 쓰므로 유지.

### 7-2. `EPCombatComponent.cpp`

`HandleServerFire` 앞부분 — 드리프트 검사 블록(`:67-71`) 삭제:
```cpp
#include "Camera/CameraComponent.h"            // 없으면 추가
#include "Combat/EPServerSideRewindComponent.h" // 이미 있음

void UEPCombatComponent::HandleServerFire(const FVector& Direction, float ClientMoveTimeStamp)
{
	if (!EquippedWeapon || !EquippedWeapon->WeaponDef) return;

	AEPCharacter* Owner = GetOwnerCharacter();
	if (!Owner || !Owner->GetCameraComponent()) return;

	// 원점 — 클라가 쏜 순간의 무브 결과 위치. 없으면 현재 (호스트 / 무브 미도착 / 리셋 직후)
	FVector Origin;
	const UEPServerSideRewindComponent* SSR = Owner->GetServerSideRewindComponent();   // HandleHitscanFire(:313)와 같은 게터
	const bool bHistoryHit = SSR && SSR->GetShotOriginAt(ClientMoveTimeStamp, Origin);
	if (!bHistoryHit)
		Origin = Owner->GetCameraComponent()->GetComponentLocation();

	// --- 탄도 분기 --- (이하 기존 코드 그대로. Origin은 지역 변수, Direction은 매개변수)
```

`Server_ConfirmFire_Implementation` 정의 **삭제**. `#include "GAS/EPGA_Item_PrimaryUse.h"`는 이 함수만 썼으면 같이 삭제.

---

## Step 8) `EPGA_Item_PrimaryUse.h`

전체 교체:
```cpp
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EPGA_Item_PrimaryUse.generated.h"

class AEPCharacter;
class AEPWeapon;
struct FGameplayAbilityTargetDataHandle;

/**
 * 무기 주 사용(발사). 연사 한 번당 활성화 한 번.
 * 오너 클라·호스트: FireOnce → 다음 발 재예약(FireTimer) → 틱에서 Auto/예약이면 계속, 아니면 종료.
 * 서버(원격 클라 인스턴스): 쏘지 않고 TargetData만 받아 ServerConfirmOneShot. 종료는 클라의 ServerEndAbility.
 */
UCLASS()
class EMPLOYMENTPROJ_API UEPGA_Item_PrimaryUse : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEPGA_Item_PrimaryUse();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	/** 간격 안의 재클릭 → 한 칸 예약 (Single/Burst). Auto는 무시 */
	virtual void InputPressed(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** Auto: 떼면 종료. Single/Burst: 무시 (스스로 끝난다) */
	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) override;

private:
	// === 변수 ===
	FTimerHandle    FireTimerHandle;            // 다음 발 재예약 (반복 X)
	bool            bPendingShot = false;       // 연타 예약 한 칸
	FDelegateHandle TargetDataDelegateHandle;   // 서버: AbilityTargetDataSetDelegate 바인딩

	// === 함수 ===
	// --- 오너 클라·호스트 ---
	void FireOnce();
	void ArmNextShot();
	void OnFireTimerTick();
	void SendFireTargetData(const FVector& Direction, float ClientMoveTimeStamp);

	// --- 서버 (호스트는 FireOnce에서 직접) ---
	void OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	/** 유일한 발사 확정 지점. false = 탄약 소진(어빌리티 종료). 버킷 거절은 true(한 발만 버림) */
	bool ServerConfirmOneShot(const FVector& Direction, float ClientMoveTimeStamp);

	// --- 헬퍼 ---
	AEPCharacter* GetCharacter() const;
	AEPWeapon*    GetWeapon() const;
	float         GetFireRateMultiplier() const;
	float         GetClientMoveTimeStamp() const;   // 원격 클라: CMC CurrentTimeStamp. 호스트: -1
	static bool   IsAutoFire(const AEPWeapon* Weapon);
};
```

`ApplyCooldown` 오버라이드, `static GetFireInterval`, 공개 `ServerConfirmOneShot(Origin, Direction)` — 전부 사라진다.

---

## Step 9) `EPGA_Item_PrimaryUse.cpp`

전체 교체:
```cpp
#include "GAS/EPGA_Item_PrimaryUse.h"

#include "AbilitySystemComponent.h"
#include "TimerManager.h"
#include "Camera/CameraComponent.h"
#include "Combat/EPCombatComponent.h"
#include "Combat/EPWeapon.h"
#include "Core/EPCharacter.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/EPNativeGameplayTags.h"
#include "GAS/EPTargetData_Fire.h"

UEPGA_Item_PrimaryUse::UEPGA_Item_PrimaryUse()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	bServerRespectsRemoteAbilityCancellation = true;   // 유지 — false면 서버가 탄창이 빌 때까지 쏜다

	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(EmpGameplayTags::TAG_Ability_Item_PrimaryUse);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Dead);
	ActivationBlockedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);
}

// ---------------------------------------------------------------- 활성화 / 종료

bool UEPGA_Item_PrimaryUse::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
		return false;                                        // Dead / Reloading 태그

	if (!ActorInfo->IsLocallyControlled())
		return true;                                         // 서버가 든 원격 클라 인스턴스: 속도는 ServerConfirmOneShot의 버킷이 본다

	// 연타 가드. 예약 슬롯 덕에 보통은 안 걸린다 — 어빌리티가 Interval 동안 살아 있어 클릭이 InputPressed로 간다.
	// 탄약 소진 등으로 먼저 끝난 직후의 클릭만 여기 온다.
	const AEPCharacter* Char   = Cast<AEPCharacter>(ActorInfo->AvatarActor.Get());
	const AEPWeapon*    Weapon = (Char && Char->GetCombatComponent()) ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
	const UWorld*       World  = Weapon ? Weapon->GetWorld() : nullptr;
	if (!Weapon || !World) return false;

	const float Rate      = Char->GetLocalModifiers().Product(EmpGameplayTags::TAG_Modifier_FireRate);
	const float Tolerance = 0.01f / Weapon->GetBaseFireRate();   // 간격의 1% — 부동소수점 여유
	return Weapon->GetFireTimer().IsElapsed(World->GetTimeSeconds(), Rate, Tolerance);
}

void UEPGA_Item_PrimaryUse::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bPendingShot = false;

	if (!GetCharacter() || !GetWeapon())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ActorInfo->IsLocallyControlled())
	{
		// 서버가 든 원격 클라 인스턴스 — 쏘지 않고 TargetData만 기다린다. 종료는 클라의 ServerEndAbility가 한다.
		// 같은 채널(PlayerState)이라 TargetData는 활성화 RPC 뒤에 도착한다. 배치 RPC도 활성화 → TargetData 순으로 푼다.
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		TargetDataDelegateHandle = ASC->AbilityTargetDataSetDelegate(Handle, ActivationInfo.GetActivationPredictionKey())
			.AddUObject(this, &UEPGA_Item_PrimaryUse::OnTargetDataReady);
		return;
	}

	FireOnce();      // 첫 발 — 배칭 시 이 안의 TargetData 호출이 활성화와 같은 배치에 들어간다
	ArmNextShot();
}

void UEPGA_Item_PrimaryUse::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo)) return;   // 이미 끝난 인스턴스에 두 번 오는 경우 방어

	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(FireTimerHandle);
	bPendingShot = false;

	if (ActorInfo && !ActorInfo->IsLocallyControlled())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			const FPredictionKey Key = ActivationInfo.GetActivationPredictionKey();
			ASC->AbilityTargetDataSetDelegate(Handle, Key).Remove(TargetDataDelegateHandle);
			ASC->ConsumeClientReplicatedTargetData(Handle, Key);   // 캐시에 남은 것 비움 (Lyra EndAbility와 동일)
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ---------------------------------------------------------------- 입력

void UEPGA_Item_PrimaryUse::InputPressed(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!IsAutoFire(GetWeapon()))
		bPendingShot = true;   // 간격 안의 재클릭 → 다음 틱에 한 발 (UT PendingFireSequence). 한 칸뿐 — 3연타는 2발
}

void UEPGA_Item_PrimaryUse::InputReleased(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (IsAutoFire(GetWeapon()))
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);   // 떼면 연사 중단 → ServerEndAbility (예전 CancelAbilities와 같은 경로)
}

// ---------------------------------------------------------------- 오너 클라·호스트

void UEPGA_Item_PrimaryUse::FireOnce()
{
	AEPCharacter*       Char   = GetCharacter();
	AEPWeapon*          Weapon = GetWeapon();
	UEPCombatComponent* Combat = Char ? Char->GetCombatComponent() : nullptr;
	if (!Char || !Weapon || !Combat || !Weapon->CanFire())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);   // 탄약 소진 — 서버 인스턴스도 끝내야 하므로 복제
		return;
	}

	Weapon->GetFireTimer().Start(GetWorld()->GetTimeSeconds(), 1.f / Weapon->GetBaseFireRate());
	const FVector Direction         = Char->GetControlRotation().Vector();
	const float   ClientMoveTimeStamp = GetClientMoveTimeStamp();     // 지금 위치를 만든 무브 (CMC 틱 전이므로 직전 프레임 것)

	if (CurrentActorInfo->IsNetAuthority())
	{
		// 호스트 — 왕복 없이 직접. 타임스탬프 -1 → 현재 위치
		if (!ServerConfirmOneShot(Direction, ClientMoveTimeStamp))
			EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 원격 클라 — 탄약 예측 + 코스메틱 + TargetData, 셋이 같은 예측 키 아래.
	// 첫 발은 활성화 윈도우 안(활성화 키 유효) → 새 키를 만들지 않는다: 배치 RPC가 활성화 키만 싣고, CatchUpTo는 정확히 그 키만 잡는다.
	// 타이머 발은 윈도우 밖 → 새 키.
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	FScopedPredictionWindow ScopedPrediction(ASC, /*bCanGenerateNewKey*/ !ASC->ScopedPredictionKey.IsValidForMorePrediction());

	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))   // GE_ConsumeAmmo 예측 적용 → HUD 즉시 −1
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	const FVector CamLoc = Char->GetCameraComponent()->GetComponentLocation();
	Combat->PlayLocalMuzzleEffect(CamLoc);                                               // 코스메틱은 클라 값으로
	if (Weapon->WeaponDef->BallisticType == EEPBallisticType::ProjectileFast)
		Combat->SpawnLocalCosmeticProjectile(CamLoc, Direction);

	SendFireTargetData(Direction, ClientMoveTimeStamp);                                  // 윈도우 안에서 — ScopedPredictionKey를 싣는다
}

void UEPGA_Item_PrimaryUse::ArmNextShot()
{
	if (!IsActive()) return;                   // FireOnce가 탄약 소진으로 방금 끝냈으면 예약하지 않는다

	AEPCharacter* Char   = GetCharacter();
	AEPWeapon*    Weapon = GetWeapon();
	if (!Char || !Weapon) return;

	const float Remaining = Weapon->GetFireTimer().GetRemaining(GetWorld()->GetTimeSeconds(), GetFireRateMultiplier());
	GetWorld()->GetTimerManager().SetTimer(FireTimerHandle, this, &UEPGA_Item_PrimaryUse::OnFireTimerTick,
		FMath::Max(Remaining, KINDA_SMALL_NUMBER), /*bLoop*/ false);
}

void UEPGA_Item_PrimaryUse::OnFireTimerTick()
{
	if (IsAutoFire(GetWeapon()) || bPendingShot)
	{
		bPendingShot = false;
		FireOnce();
		ArmNextShot();                          // 배율이 바뀌었으면 여기서부터 새 간격 (진행 중이던 간격은 옛 배율)
		return;
	}
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ true, false);
}

void UEPGA_Item_PrimaryUse::SendFireTargetData(const FVector& Direction, float ClientMoveTimeStamp)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!ASC) return;

	FEPTargetData_Fire* Data = new FEPTargetData_Fire();   // 핸들이 TSharedPtr로 소유권을 가져간다
	Data->Direction           = Direction;
	Data->ClientMoveTimeStamp = ClientMoveTimeStamp;
	const FGameplayAbilityTargetDataHandle Handle(Data);

	// CurrentPredictionKey = 이 발의 예측 키 (FireOnce의 윈도우). 서버가 같은 키로 윈도우를 열어 CommitAbilityCost → ack → 클라 예측 GE 제거.
	// 거절(버킷·탄약)이면 차감 없이 ack만 → 예측 GE 제거 = 탄약 복구
	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
		Handle, FGameplayTag(), ASC->ScopedPredictionKey);
}

// ---------------------------------------------------------------- 서버

void UEPGA_Item_PrimaryUse::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag /*ApplicationTag*/)
{
	UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!ASC) return;

	// 캐시를 먼저 비운다 — 다음 발이 "overriding pending replicated target data" 로그를 내지 않게
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());

	const FEPTargetData_Fire* Fire = Data.IsValid(0) ? static_cast<const FEPTargetData_Fire*>(Data.Get(0)) : nullptr;
	if (!Fire) return;   // 배치 RPC는 TargetData가 비어 있어도 델리게이트를 부른다 (첫 발이 CanFire 실패한 경우)

	if (!ServerConfirmOneShot(FVector(Fire->Direction), Fire->ClientMoveTimeStamp))
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);   // 탄약 소진 → ClientEndAbility로 클라도 따라 끝난다
}

bool UEPGA_Item_PrimaryUse::ServerConfirmOneShot(const FVector& Direction, float ClientMoveTimeStamp)
{
	AEPCharacter*       Char   = GetCharacter();
	AEPWeapon*          Weapon = GetWeapon();
	UEPCombatComponent* Combat = Char ? Char->GetCombatComponent() : nullptr;
	if (!Char || !Weapon || !Combat) return false;

	const float RefillPerSecond = Weapon->GetBaseFireRate() * GetFireRateMultiplier();   // TryTake 시점의 유효 발사 속도
	if (!Weapon->GetFireLimiter().TryTake(GetWorld()->GetTimeSeconds(), RefillPerSecond))
		return true;                                          // 너무 빠름 → 이 발만 버림. 어빌리티는 유지

	if (!CommitAbilityCost(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
		return false;                                         // 탄약 소진 → 종료

	Combat->HandleServerFire(Direction, ClientMoveTimeStamp); // 원점은 안에서 서버가 히스토리에서
	return true;
}

// ---------------------------------------------------------------- 헬퍼

AEPCharacter* UEPGA_Item_PrimaryUse::GetCharacter() const
{
	return CurrentActorInfo ? Cast<AEPCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
}

AEPWeapon* UEPGA_Item_PrimaryUse::GetWeapon() const
{
	const AEPCharacter* Char = GetCharacter();
	return (Char && Char->GetCombatComponent()) ? Char->GetCombatComponent()->GetEquippedWeapon() : nullptr;
}

float UEPGA_Item_PrimaryUse::GetFireRateMultiplier() const
{
	const AEPCharacter* Char = GetCharacter();
	return Char ? Char->GetLocalModifiers().Product(EmpGameplayTags::TAG_Modifier_FireRate) : 1.f;
}

float UEPGA_Item_PrimaryUse::GetClientMoveTimeStamp() const
{
	if (!CurrentActorInfo || CurrentActorInfo->IsNetAuthority()) return -1.f;   // 호스트: 히스토리 없음
	const AEPCharacter* Char = GetCharacter();
	const UCharacterMovementComponent* CMC = Char ? Char->GetCharacterMovement() : nullptr;
	const FNetworkPredictionData_Client_Character* ClientData = CMC ? CMC->GetPredictionData_Client_Character() : nullptr;
	return ClientData ? ClientData->CurrentTimeStamp : -1.f;   // CMC 틱에서 갱신되므로 지금은 직전 프레임 무브의 값
}

bool UEPGA_Item_PrimaryUse::IsAutoFire(const AEPWeapon* Weapon)
{
	return Weapon && Weapon->WeaponDef && Weapon->WeaponDef->FireMode == EEPFireMode::Auto;
}
```

**읽는 순서 (오너 클라, `Single` 더블클릭):**
```
클릭1  Input_Fire → 스펙 비활성 → TryActivateAbility
        ActivateAbility → FireOnce(발사, FireTimer.Start, TargetData{Dir, TS}) → ArmNextShot(Interval 뒤 틱)
클릭2  Input_Fire → 스펙 활성 → AbilitySpecInputPressed → InputPressed → bPendingShot = true
틱     OnFireTimerTick → bPendingShot → FireOnce(발사) → ArmNextShot
틱     OnFireTimerTick → Auto 아님, 예약 없음 → EndAbility(복제) → ServerEndAbility
```

**서버 (원격 클라 인스턴스):**
```
ServerMove ×N → CMC OnMovementUpdated → SSR OnServerMoveProcessed → RecordShotOrigin(TS, 카메라)   (캐릭터 채널, 계속)
ServerTryActivateAbility → ActivateAbility → 델리게이트 바인딩, 대기         (PlayerState 채널)
ServerSetReplicatedTargetData ×2 → OnTargetDataReady → ServerConfirmOneShot(버킷 → 탄약 → HandleServerFire[TS → 원점])
ServerEndAbility → EndAbility → 델리게이트 해제
```

---

## Step 10) `AEPCharacter::Input_Fire` / `Input_StopFire` — `EPCharacter.cpp`

태그 활성화 → **핸들 기반 + 예약 분기**. 헤더 변경 없음 (헬퍼는 파일 로컬).

```cpp
#include "Abilities/GameplayAbilityTypes.h"   // FScopedServerAbilityRPCBatcher (Step 14 전엔 no-op)

namespace
{
	/** 에셋 태그로 스펙을 찾는다 — "지금 장착된 것이 무엇이든 그 PrimaryUse 어빌리티". FindAbilitySpecFromClass는 BP 서브클래스에서 nullptr (설계 문서 §1) */
	FGameplayAbilitySpec* FindSpecByAssetTag(UAbilitySystemComponent* ASC, const FGameplayTag& Tag)
	{
		TArray<FGameplayAbilitySpec*> Specs;
		ASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(FGameplayTagContainer(Tag), Specs, /*bOnlyAbilitiesThatSatisfyTagRequirements*/ false);
		return Specs.Num() > 0 ? Specs[0] : nullptr;
	}
}

void AEPCharacter::Input_Fire(const FInputActionValue& Value)
{
	if (!CombatComponent || !ASC) return;

	FGameplayAbilitySpec* Spec = FindSpecByAssetTag(ASC, EmpGameplayTags::TAG_Ability_Item_PrimaryUse);
	if (!Spec) return;

	if (Spec->IsActive())
	{
		ASC->AbilitySpecInputPressed(*Spec);    // 간격 안의 클릭 → 어빌리티 InputPressed → 예약
		return;
	}

	FScopedServerAbilityRPCBatcher Batcher(ASC, Spec->Handle);   // ShouldDoServerAbilityRPCBatch()가 false면 아무것도 안 한다
	ASC->TryActivateAbility(Spec->Handle);
}

void AEPCharacter::Input_StopFire(const FInputActionValue& Value)
{
	if (!ASC) return;

	if (FGameplayAbilitySpec* Spec = FindSpecByAssetTag(ASC, EmpGameplayTags::TAG_Ability_Item_PrimaryUse))
		ASC->AbilitySpecInputReleased(*Spec);   // 활성일 때만 InputReleased가 불린다 — Auto면 어빌리티가 스스로 끝낸다
}
```

`bOnlyAbilitiesThatSatisfyTagRequirements=false`인 이유: 재장전 중(`Reloading` 태그)에도 스펙을 찾아야
`InputReleased`가 살아 있는 `Auto` 인스턴스에 닿는다. 활성화 자체는 `TryActivateAbility` 안의
`CanActivateAbility`가 태그로 막는다.

`Input_StopFire`가 `CancelAbilities`에서 `AbilitySpecInputReleased`로 바뀌는 이유: `Single`의 클릭은
"누름+뗌"이라 뗌이 취소면 예약 슬롯이 절대 안 찬다. 뗌의 의미를 어빌리티가 모드별로 정한다.

---

## Step 11) `UEPGA_Item_Reload` — `GE_Reloading` → `ActivationOwnedTags`

### 11-1. `EPGA_Item_Reload.h`
- `EndAbility` 오버라이드 선언 **삭제** (본문이 GE 제거뿐이었음)
- `GE_ReloadingClass` 필드 **삭제**, `ReloadingEffectHandle` **삭제**

### 11-2. `EPGA_Item_Reload.cpp`
```cpp
UEPGA_Item_Reload::UEPGA_Item_Reload()
{
	// ... 기존 그대로 ...
	ActivationOwnedTags.AddTag(EmpGameplayTags::TAG_State_Reloading);   // ← 재장전 어빌리티 수명 = 태그 수명. 양쪽에서 즉시
}
```
- `ActivateAbility`의 `if (GE_ReloadingClass) { ... }` 블록(`:48-54`) **삭제**
- `EndAbility` 정의(`:62-71`) **삭제**
- `TAG_Data_ReloadDuration`은 소비자가 없어지지만 태그는 둔다

`ActivationBlockedTags`(`Reloading` 자기 자신)는 그대로 — `ActivationOwnedTags`는 `PreActivate`에서
붙으므로 `CanActivateAbility` 검사 뒤다. 재장전 중 재장전은 여전히 막힌다.

---

## Step 12) 에셋 정리 (에디터)

순서 중요 — 참조를 먼저 끊고 삭제한다. **Step 3의 태그 삭제는 이 단계 뒤에** 빌드한다.

1. `Content/Characters/Player/Abilities/BP_GA_Item_PrimaryUse` 열기 → Class Defaults → **Cooldowns → Cooldown Gameplay Effect Class 비우기** → 컴파일·저장.
2. `BP_GA_Item_Reload` 열기 → 컴파일. 사라진 `GE_ReloadingClass`는 자동으로 떨어진다. `GE_ReloadAmmoClass`는 **그대로**. 저장.
3. Reference Viewer로 참조 0 확인 후 삭제: `Content/GameplayEffects/Core/GE_FireCooldown`, `Content/GameplayEffects/Core/GE_Reloading`.
4. 그 다음 Step 3의 태그 두 개 삭제 → 빌드.

**확인:** 에디터 재시작 후 로그에 "Failed to load" / "Invalid tag" 없음.

---

## Step 13) 빌드 확인 + 1차 PIE (배칭 전)

```
UnrealBuildTool.exe -projectfiles -project="EmploymentProj/EmploymentProj.uproject" -game -engine
UnrealBuildTool.exe EmploymentProj Win64 Development -project="EmploymentProj/EmploymentProj.uproject"
```

새 `USTRUCT` 헤더(Step 2)는 첫 빌드가 돌아야 `.generated.h`가 생긴다 — 그 전의 IDE 에러는 무시.

여기서 **§15 표 1~12를 먼저 통과**시킨다. 배칭(Step 14)은 그 뒤 — 문제가 생기면 두 층을 따로 볼 수 있다.

---

## Step 14) 어빌리티 배칭 — ASC 서브클래스 (독립 단계)

### 14-1. `Public/GAS/EPAbilitySystemComponent.h`
```cpp
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "EPAbilitySystemComponent.generated.h"

/** 서버 어빌리티 RPC 배칭 켜기 — Input_Fire의 FScopedServerAbilityRPCBatcher가 이 값을 본다 (ASC.h:1305 기본 false) */
UCLASS()
class EMPLOYMENTPROJ_API UEPAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }
};
```

### 14-2. `Private/GAS/EPAbilitySystemComponent.cpp`
```cpp
#include "GAS/EPAbilitySystemComponent.h"
```

### 14-3. `EPPlayerState.cpp:14`
```cpp
#include "GAS/EPAbilitySystemComponent.h"   // ←

	ASC = CreateDefaultSubobject<UEPAbilitySystemComponent>(TEXT("ASC"));   // UAbilitySystemComponent → UEPAbilitySystemComponent
```
멤버 타입(`TObjectPtr<UAbilitySystemComponent> ASC`)과 `GetAbilitySystemComponent()`는 그대로.

### 14-4. 효과 (Step 10의 `Batcher` 줄이 살아난다)

| 모드 | 배칭 전 | 배칭 후 |
|---|---|---|
| `Single` 클릭 | `ServerTryActivateAbility` + `ServerSetReplicatedTargetData` + `ServerEndAbility` = 3 | `ServerAbilityRPCBatch`[활성화+TD] + `ServerEndAbility` = **2** |
| `Auto` N발 | N+2 | **N+1** |

되돌리기: 14-3 한 줄을 `UAbilitySystemComponent`로. 다른 코드는 그대로 동작한다.

**횡단 변경 기록:** `Notes/04/Status/GAS_STATUS.md`에 "ASC 서브클래스 `UEPAbilitySystemComponent`(배칭 전용)" 한 줄. `PROJECT_CONTEXT.md`는 pre-commit 훅이 갱신.

---

## 15. PIE 검증

설정: PIE Net Mode = Play As Client, 클라 2, 원격 클라 창에서 `NetEmulation.PktLag 100`.

검증용 임시 로그 — 끝나면 지운다:
```cpp
// FireOnce, FireTimer.Start 직후
UE_LOG(LogTemp, Log, TEXT("[Fire] CL shot now=%.3f ts=%.4f cam=%s dir=%s"), GetWorld()->GetTimeSeconds(), ClientMoveTimeStamp,
	*Char->GetCameraComponent()->GetComponentLocation().ToString(), *Direction.ToString());
// ServerConfirmOneShot, TryTake 실패 분기
UE_LOG(LogTemp, Warning, TEXT("[Fire] SV bucket rejected now=%.3f"), GetWorld()->GetTimeSeconds());
// HandleServerFire, Origin 확정 직후
UE_LOG(LogTemp, Log, TEXT("[Fire] SV shot ts=%.4f hit=%d origin=%s dir=%s"), ClientMoveTimeStamp, bHistoryHit ? 1 : 0, *Origin.ToString(), *Direction.ToString());
```

| # | 확인 | 통과 기준 |
|---|---|---|
| 1 | `Single` 연타 (FireRate 4 무기) | 클릭마다 정확히 0.25s 간격. `ClientActivateAbilityFailed` 로그 0 |
| 2 | `Auto` 3초 사격 | 서버 `[Fire] SV shot` 수 = 클라 `[Fire] CL shot` 수. `bucket rejected` 0 |
| 3 | `Auto` + `NetEmulation.PktLoss 10` | `bucket rejected` 0 (재전송 몰림은 `MaxTokens`가 흡수). 서버 발사 수 = 클라 발사 수 |
| 4 | 조작 시뮬 — `ArmNextShot`에서 임시로 `Rate *= CurrentActorInfo->IsNetAuthority() ? 1.f : 2.f` | `bucket rejected`가 클라 발사의 절반. 서버 발사 수 ≈ `FireRate × 시간` |
| 5 | 재장전 직후 `Single` 첫 발 ×10 | 헛발(서버 거절) ≤ 1회 |
| 6 | 재장전 HUD | 재장전 끝과 동시에 표시 꺼짐 (전엔 ~D 늦음) |
| 7 | 호스트 회귀 | 1·2·5·6이 리슨 서버 호스트에서도 정상. 호스트 로그 `hit=0` (정상 — 히스토리 없음) |
| 8 | **원점 동기** — 1번 `cam` vs 3번 `origin`, 걷는 중·스트레이프 중 30발 | `hit=1`인 발: ≤ 포즈 차(수 cm), 이동 속도와 무관. 적중률 ≥ 95%. `hit=0`인 발만 ≤ 이동 1프레임분 |
| 9 | 방향 일치 — 같은 로그의 `dir` | 항상 동일 (페이로드 그대로) |
| 10 | 단발 예약 — `Single` 간격 안 더블클릭 / 3연타 | 정확히 `Interval` 간격으로 2발 / 2발 (한 칸만 예약) |
| 11 | 발사 속도 배율 — `ActivateAbility` 첫 줄에 임시로 `GetCharacter()->GetLocalModifiers().Set(EmpGameplayTags::TAG_Modifier_FireRate, 2.f)` | `Auto` 간격 절반, `bucket rejected` 0. 지우면 원래대로 |
| 12 | 탄약 소진 — `Auto`로 탄창 비우기 | 클라·서버 인스턴스 둘 다 종료 (`showdebug abilitysystem`에서 활성 0). 재장전 후 다시 발사됨 |
| 13 | 타임스탬프 리셋 — 임시로 `MinTimeBetweenTimeStampResets`를 10초로 (`EPCharacterMovement` 생성자) 하고 30초 연사 | 리셋 직후 1~2발 `hit=0`, 이후 다시 `hit=1`. 거부·오발 없음 |
| 14 | (Step 14 뒤) 배칭 — 서버 콘솔 `AbilitySystem.ServerRPCBatching.Log 1` | `Single` 클릭당 `::ServerAbilityRPCBatch_Implementation` 1회 + `ServerEndAbility` 1회. `Auto` N발에 배치 1 + TD N−1 + End 1 |
| 15 | 탄약 예측 — `PktLag 200`, `Auto` 사격하며 HUD 잔탄 | 클릭 즉시 −1 (전엔 RTT 뒤). −2로 튀지 않음. 4번 조작 시뮬에서 거절된 발은 잔탄이 다시 +1. `showdebug abilitysystem`에 `GE_ConsumeAmmo`가 잠깐 보였다 사라짐. **Step 14 뒤에 다시** — 배칭 첫 발도 즉시 −1이고 남는 GE 없음 |

---

## 함정표 (구현 수준)

| 함정 | 대응 |
|---|---|
| `FEPTargetData_Fire`에 `GetScriptStruct()` 누락 | 서버가 베이스 타입으로 받아 필드가 기본값. Step 2 그대로 — `StaticStruct()` 반환 필수 |
| `TStructOpsTypeTraits` `WithNetSerializer` 누락 | 핸들 직렬화가 통째로 실패. 엔진 주석 "REQUIRED" |
| 타임스탬프와 위치가 다른 무브 것이면 | 안 생긴다 — `CurrentTimeStamp`와 캐릭터 위치는 둘 다 "마지막으로 실행된 무브"의 것이라 언제 읽어도 짝이 맞는다. 첫 발(`Input_Fire`, `TickPlayerInput` 안)은 CMC 틱 **전**이라 무브 N−1 짝, 타이머 발(`OnFireTimerTick`)은 `FTimerManager::Tick`이 `TG_PrePhysics` **뒤**에 돌아(`LevelTick.cpp:1721 → :1787`) 무브 N 짝. 후자는 무브 N이 같은 프레임에 나가므로 TargetData가 먼저 도착하면 미스 → 폴백. 로그 8번 `hit` 비율로 확인 |
| 히스토리를 `NewMove`만 기록 | 번들 앞쪽 무브(Old/Pending)에서 쏜 발이 미스. Step 6-2 — CMC가 **모든 타입**에서 브로드캐스트하고, SSR은 원점은 매번 / 본 스냅샷은 `bNewMove`만 |
| SSR 컴포넌트가 없는 캐릭터 | `GetServerSideRewindComponent()`가 null → 폴백(현재 위치). 지금 `HandleHitscanFire:313`도 같은 전제 |
| 히스토리 크기가 RTT보다 짧다 | 144fps·RTT 300ms면 43개 — 64로 충분. 로그 `hit=0`이 잦으면 `ShotOriginHistoryCount` 상향 |
| 호스트 `GetPredictionData_Client_Character()` | 호스트는 클라 예측 데이터가 없다 — `GetClientMoveTimeStamp()`가 `IsNetAuthority()`로 먼저 -1 반환 |
| `CanActivateAbility`(const)에서 `GetFireTimer()` 호출 시 "const 한정자가 없습니다" | `const AEPWeapon*`로는 non-const 접근자를 못 부른다. Step 5-1 `const FEPLocalTimer& GetFireTimer() const` 오버로드 |
| `CanActivateAbility`는 `const`라 `CurrentActorInfo`가 없다 | 매개변수 `ActorInfo`로 캐릭터·무기를 찾는다 (Step 9 코드). `GetWeapon()` 헬퍼는 활성화 뒤에만 |
| `FireOnce`가 탄약 소진으로 `EndAbility`한 뒤 `ArmNextShot`이 타이머를 다시 건다 | `ArmNextShot` 첫 줄 `IsActive()` 가드 |
| 서버가 `EndAbility`를 두 번 받는다 (`ServerEndAbility` + 자기 탄약 소진) | `IsEndAbilityValid` 가드 — 두 번째는 no-op |
| `Input_StopFire`를 `CancelAbilities`로 두면 `Single` 예약이 안 찬다 | Step 10 — `AbilitySpecInputReleased`. 뗌의 의미는 어빌리티 `InputReleased`가 모드별로 |
| `InputPressed`가 안 불린다 | `AbilitySpecInputPressed`는 **스펙이 활성일 때만** 인스턴스에 전달(`ASC_Abilities.cpp:2879`). `Input_Fire`의 `IsActive()` 분기 확인 |
| 배치 RPC가 빈 TargetData로 델리게이트를 부른다 | `OnTargetDataReady`의 `IsValid(0)` 검사. 첫 발이 `CanFire` 실패면 빈 배치가 온다 |
| 서버 로그 "overriding pending replicated target data" | 직전 발을 `Consume`하지 않았다는 뜻. `OnTargetDataReady` 첫 줄에서 소비 |
| 첫 발에서 새 예측 키를 만든다 (`FScopedPredictionWindow(ASC)` 무조건) | 활성화 윈도우 안에서 만든 종속 키는 배치 RPC가 안 싣고(`ASC_Abilities.cpp:4131` 활성화 키만), `CatchUpTo`는 정확히 그 키만 잡아(`GameplayPrediction.cpp:321`) 예측 GE가 남는다 → HUD −1 고정. `bCanGenerateNewKey = !ScopedPredictionKey.IsValidForMorePrediction()` |
| `CommitAbilityCost`를 윈도우 **밖**에서 부른다 | `GetPredictionKeyForNewAction()`이 무효 → 예측 안 됨. `FScopedPredictionWindow` 선언 **뒤에** 부른다 |
| 서버 `ServerConfirmOneShot`에서 `CommitAbilityCost`가 클라 키를 안 쓴다 | 서버는 `ServerSetReplicatedTargetData_Implementation`이 연 윈도우 안에 있어 `GetPredictionKeyForNewAction()`이 클라 키 — 자동. 호스트 경로(윈도우 없음)는 권위라 예측 자체가 없다 |
| 잔탄 HUD가 한 프레임 −2로 튄다 | 예측 GE 제거(키 ack)와 서버 base 복제의 적용 순서. 같은 업데이트에 오면 프레임 안에서 해소. 지속되면 `GE_ConsumeAmmo`가 Instant인지, 스택 설정이 없는지 확인 |
| 클라에서 `FScopedPredictionWindow(ASC, Key)` 2-인자 생성자 | 클라(`IsNetSimulating`)에선 **아무것도 안 한다**(`GameplayPrediction.cpp:378`). 불필요 |
| 남이 건 발사 속도 버프 **종료** 직후 한두 발 거절 | 클라가 D 늦게 알아 초과분 `(빠른 rps − 느린 rps) × D`. 2배·20rps·50ms = 1발, `MaxTokens=2` 안. 넘으면 `FireRateBurstAllowance` 상향이 아니라 배율 상한을 검토 |
| 배율 변경이 진행 중 간격에 안 먹는다 | 의도 — 다음 발부터 (STATUS §2 09-20). 최대 한 간격 한 번 |
| 무기 교체 직후 첫 발이 이전 무기 간격에 걸린다 | `FireTimer`가 **무기에** 있으므로 새 무기는 새 타이머 — 안 걸린다. 걸리면 어빌리티에 잘못 뒀는지 확인 |
| `Burst` 모드 | 지금은 `Single`과 같은 경로(`IsAutoFire` false). N발 카운터는 범위 밖 (설계 문서 §3) |
| 재장전 중 `Auto`가 계속 쏜다 | **기존 동작** — `Reload`가 `PrimaryUse`를 취소하지 않는다. 이 문서 범위 밖. 필요하면 `Reload` 생성자 `CancelAbilitiesWithTag(Ability.Item.PrimaryUse)` 한 줄 |
| `Modifier.FireRate` 배율을 누가 세팅하나 | 지금은 아무도 안 한다 — 저장소·태그·읽는 코드만 준비. 첫 소비자는 발사 속도 버프 스킬(`04_Polish_SkillDisplay.md` §5 timed-buff 패턴) |
| 리슨 서버 호스트 | `IsLocallyControlled() && IsNetAuthority()` → `FireOnce`가 `ServerConfirmOneShot` 직접, 타임스탬프 -1 → 현재 카메라. 버킷도 통과한다(자기 타이머로 페이싱하니 정확히 1발/간격) |
| `AEPWeapon::BeginPlay`에서 `GetDefault<UEPCombatDeveloperSettings>()` | `Config=Game`이라 ini 값이 들어온다. 미설정이면 C++ 기본 |
| `FEPLocalTimer`를 `double`로 바꾸면 스킬 코드가 깨지나 | `GetTimeSeconds()`가 이미 `double`이라 호출자는 그대로. `Revert`의 `PrevLastUpdate`도 같이 `double` |

---

## 완료 후

- `04_Polish_WeaponFireRate_STATUS.md` 상단 "구현 상태"를 코드 기준으로 — **§15 표 전부 통과한 뒤에만** "완료". 통과 전엔 "구현 완료, PIE 미검증". 14번은 Step 14 뒤 따로.
- `../../Status/04_Polish_STATUS.md`에 항목 추가.
- 설계 문서 `../04_Polish_WeaponFireRate.md` 상태 줄 갱신, §1 "현재 구조"를 새 코드 기준으로 다시 씀 (`Server_ConfirmFire`·`CommitAbilityCooldown` 언급 제거).
- `Issue/FireRate_GECooldownPrediction.md`에 "해결됨 — `Polish/WeaponFireRate/`" + "Ability Batching" 절 이름 정정(진짜 배칭은 Step 14).
- `DOCS/Mine/LagCompensationFix.md`·포트폴리오의 "클라가 원점·방향을 보낸다" → "방향과 무브 타임스탬프를 보내고, 원점은 서버가 그 무브의 위치에서".
- 임시 `[Fire]` 로그 제거, 13번의 `MinTimeBetweenTimeStampResets` 임시값 제거.
- `Public/Core/EPLocalModifiers.h:4` `#include "AnimationEditorTypes.h"` 제거 (Persona 에디터 헤더 — 이 작업과 무관하지만 같은 빌드에서).
