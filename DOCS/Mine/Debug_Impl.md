# Debug Implementation Guide (Primitive Only)

## 목표
- `UEPServerSideRewindComponent::ConfirmHitscan`에서만 디버그를 그린다.
- 임시 위치 캡슐 디버그는 사용하지 않는다.
- `FBodyInstance + AggGeom` 기반으로 HitBones primitive를 그대로 시각화한다.

색상:
- `Blue`: 서버 현재 시점 primitive
- `Red`: 리와인드 시점 primitive
- `White`: 발사 트레이스
- `Yellow`: 히트 포인트

---

## 완료 기준
1. 디버그 토글 ON 시, 후보 타깃의 Blue/Red primitive가 동시에 보인다.
2. White 트레이스가 발사마다 표시된다.
3. 확정 히트 시 Yellow 포인트가 표시된다.
4. 토글 OFF 시 DrawDebug 호출이 없다.

---

## Step 0) 디버그 설정값 추가

파일: `EmploymentProj/Source/EmploymentProj/Public/Combat/EPCombatDeveloperSettings.h`

`UEPCombatDeveloperSettings`에 아래 프로퍼티 추가:

```cpp
UPROPERTY(Config, EditAnywhere, Category="Debug|SSR")
bool bEnableSSRDebugDraw = false;

UPROPERTY(Config, EditAnywhere, Category="Debug|SSR", meta=(ClampMin="0.01"))
float SSRDebugDrawDuration = 2.0f;

UPROPERTY(Config, EditAnywhere, Category="Debug|SSR", meta=(ClampMin="0.1"))
float SSRDebugLineThickness = 1.5f;

UPROPERTY(Config, EditAnywhere, Category="Debug|SSR")
bool bEnableSSRDebugLog = false;
```

옵션: `EmploymentProj/Config/DefaultGame.ini`에도 기본값 반영

```ini
[/Script/EmploymentProj.EPCombatDeveloperSettings]
bEnableSSRDebugDraw=False
SSRDebugDrawDuration=2.0
SSRDebugLineThickness=1.5
bEnableSSRDebugLog=False
```

---

## Step 1) SSR cpp include 추가

파일: `EmploymentProj/Source/EmploymentProj/Private/Combat/EPServerSideRewindComponent.cpp`

상단 include 추가:

```cpp
#include "DrawDebugHelpers.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
```

---

## Step 2) 파일 스코프 헬퍼 추가 (핵심)

파일: `EmploymentProj/Source/EmploymentProj/Private/Combat/EPServerSideRewindComponent.cpp`

`struct FEPRewindEntry` 아래에 헬퍼 2개를 추가한다.

### 2-1) `DrawBodyAggGeomPrimitives`
- 입력: `UWorld*`, `FBodyInstance*`, `FColor`, `Duration`, `Thickness`
- 역할: Body의 `AggGeom`의 Sphere/Sphyl/Box를 월드로 변환해서 Draw

```cpp
static void DrawBodyAggGeomPrimitives(
	UWorld* World,
	const FBodyInstance* Body,
	const FColor& Color,
	float Duration,
	float Thickness)
{
	if (!World || !Body) return;

	const UBodySetup* BodySetup = Body->GetBodySetup();
	if (!BodySetup) return;

	const FTransform BodyWorld = Body->GetUnrealWorldTransform();

	// Sphere
	for (const FSphereElem& Sphere : BodySetup->AggGeom.SphereElems)
	{
		const FVector CenterWS = BodyWorld.TransformPosition(Sphere.Center);
		const float RadiusWS = Sphere.Radius * BodyWorld.GetScale3D().GetAbsMax();
		DrawDebugSphere(World, CenterWS, RadiusWS, 16, Color, false, Duration, 0, Thickness);
	}

	// Sphyl (UE 캡슐)
	for (const FKSphylElem& Sphyl : BodySetup->AggGeom.SphylElems)
	{
		const FQuat LocalRot = FQuat(Sphyl.Rotation);
		const FVector CenterWS = BodyWorld.TransformPosition(Sphyl.Center);
		const FQuat RotWS = BodyWorld.GetRotation() * LocalRot;

		const FVector Scale = BodyWorld.GetScale3D().GetAbs();
		const float RadiusWS = Sphyl.Radius * FMath::Max(Scale.X, Scale.Y);
		const float HalfHeightWS = (Sphyl.Length * 0.5f + Sphyl.Radius) * Scale.Z;

		DrawDebugCapsule(World, CenterWS, HalfHeightWS, RadiusWS, RotWS, Color, false, Duration, 0, Thickness);
	}

	// Box
	for (const FKBoxElem& Box : BodySetup->AggGeom.BoxElems)
	{
		const FQuat LocalRot = FQuat(Box.Rotation);
		const FVector CenterWS = BodyWorld.TransformPosition(Box.Center);
		const FQuat RotWS = BodyWorld.GetRotation() * LocalRot;

		const FVector Scale = BodyWorld.GetScale3D().GetAbs();
		const FVector ExtentWS = FVector(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f) * Scale;

		DrawDebugBox(World, CenterWS, ExtentWS, RotWS, Color, false, Duration, 0, Thickness);
	}
}
```

### 2-2) `DrawHitBonesPrimitivesForCharacter`
- 입력: `AEPCharacter*`, `HitBones`, 색상, Duration, Thickness
- 역할: 캐릭터의 HitBones를 순회하면서 body instance primitive를 그림

```cpp
static void DrawHitBonesPrimitivesForCharacter(
	UWorld* World,
	const AEPCharacter* Char,
	const TArray<FName>& Bones,
	const FColor& Color,
	float Duration,
	float Thickness)
{
	if (!World || !Char || !Char->GetMesh()) return;

	for (const FName& BoneName : Bones)
	{
		const FBodyInstance* Body = Char->GetMesh()->GetBodyInstance(BoneName);
		if (!Body) continue;
		DrawBodyAggGeomPrimitives(World, Body, Color, Duration, Thickness);
	}
}
```

---

## Step 3) ConfirmHitscan에 디버그 토글/설정 로드

파일: `EmploymentProj/Source/EmploymentProj/Private/Combat/EPServerSideRewindComponent.cpp`  
함수: `UEPServerSideRewindComponent::ConfirmHitscan`

`CombatSettings` 로드 직후:

```cpp
const bool bDebugDraw = CombatSettings->bEnableSSRDebugDraw;
const bool bDebugLog = CombatSettings->bEnableSSRDebugLog;
const float DebugDuration = CombatSettings->SSRDebugDrawDuration;
const float DebugThickness = CombatSettings->SSRDebugLineThickness;
```

가드:

```cpp
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
const bool bDebugDraw = false;
const bool bDebugLog = false;
#endif
```

---

## Step 4) Blue/Red primitive draw 삽입 위치

파일: `EmploymentProj/Source/EmploymentProj/Private/Combat/EPServerSideRewindComponent.cpp`  
함수: `UEPServerSideRewindComponent::ConfirmHitscan`

삽입 위치: `for (AEPCharacter* Char : Candidates)` 루프 내부

### 4-1) 리와인드 적용 전 (Blue)
- `TargetSSR` 체크 후, `Snap` 적용 전에 호출

```cpp
if (bDebugDraw)
{
	DrawHitBonesPrimitivesForCharacter(GetWorld(), Char, HitBones, FColor::Blue, DebugDuration, DebugThickness);
}
```

### 4-2) 리와인드 적용 후 (Red)
- `Body->SetBodyTransform(...)`를 모두 적용한 직후 호출

```cpp
if (bDebugDraw)
{
	DrawHitBonesPrimitivesForCharacter(GetWorld(), Char, HitBones, FColor::Red, DebugDuration, DebugThickness);
}
```

주의:
- Blue는 "현재 서버 바디 상태"
- Red는 "리와인드된 바디 상태"
- 둘 다 `GetBodyInstance` 기반으로 동일한 프리미티브 draw 경로를 써야 비교가 정확하다.

---

## Step 5) White 트레이스 / Yellow 히트포인트 삽입

파일: `EmploymentProj/Source/EmploymentProj/Private/Combat/EPServerSideRewindComponent.cpp`  
함수: `UEPServerSideRewindComponent::ConfirmHitscan`

### 5-1) White
삽입 위치: `for (const FVector& Dir : Directions)`에서 `LineTraceSingleByChannel` 호출 직전

```cpp
if (bDebugDraw)
{
	DrawDebugLine(GetWorld(), Origin, End, FColor::White, false, DebugDuration, 0, DebugThickness);
}
```

### 5-2) Yellow
삽입 위치: 후보군 필터를 통과해 `OutConfirmedHits.Add(Hit);` 하기 직전/직후

```cpp
if (bDebugDraw)
{
	DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 12, FColor::Yellow, false, DebugDuration, 0, DebugThickness);
}
```

---

## Step 6) 로그 추가 (선택)

파일: `EmploymentProj/Source/EmploymentProj/Private/Combat/EPServerSideRewindComponent.cpp`  
함수: `UEPServerSideRewindComponent::ConfirmHitscan`

권장 로그:

```cpp
if (bDebugLog)
{
	UE_LOG(LogTemp, Log, TEXT("[SSR] ServerNow=%.3f ClientFire=%.3f Delta=%.3f Candidates=%d"),
		ServerNow, ClientFireTime, ServerNow - ClientFireTime, Candidates.Num());
}
```

히트마다:

```cpp
if (bDebugLog)
{
	UE_LOG(LogTemp, Log, TEXT("[SSR] Hit Actor=%s Bone=%s PM=%s"),
		*GetNameSafe(Hit.GetActor()),
		*Hit.BoneName.ToString(),
		Hit.PhysMaterial.IsValid() ? *Hit.PhysMaterial->GetName() : TEXT("None"));
}
```

---

## Step 7) 복구 보장 확인

현재 코드의 `RewindEntries` 복구 루프는 유지한다.  
디버그 코드는 복구 로직보다 앞/뒤 어디에 있어도 되지만, 복구 누락이 없어야 한다.

필수 확인:
- `return` 전에 항상 복구 루프가 실행되는 구조인지 확인
- 중간 `continue`/`return` 추가 시 복구 스킵이 생기지 않게 주의

---

## Step 8) 검증 시나리오

1. PIE Dedicated + 2 Client
2. 지연 `200/200`, packet loss `5%`
3. 30발 테스트
4. 판독:
- Red primitive와 White 선이 교차하는데 미스: 충돌 채널/후보군 필터 확인
- Blue 기준으로만 맞는 경향: 타임스탬프/리와인드 윈도우 확인
- Red/Blue 거의 겹침: 클램프 또는 히스토리 설정 확인

---

## 자주 나는 컴파일 오류

1. `DrawDebug*` 미인식  
- `#include "DrawDebugHelpers.h"` 누락

2. `AggGeom`/shape 타입 미인식  
- `#include "PhysicsEngine/BodySetup.h"`  
- `#include "PhysicsEngine/AggregateGeom.h"`

3. `Body->GetBodySetup()` 접근 오류  
- 엔진 버전에 따라 접근 API가 다르면 `BodySetup` 접근 경로를 현재 버전에 맞게 조정

---

## 이 문서의 범위
- 본 문서는 디버그 시각화 구현만 다룬다.
- 데미지 계산/적용(`ApplyPointDamage`) 변경은 포함하지 않는다.
