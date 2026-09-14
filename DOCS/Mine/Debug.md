# Debug: Server Rewind Primitive Visualization

## 목표
- 디버그 토글 ON 시, 서버가 한 발을 검증할 때 아래를 동시에 본다.
- `Blue`: 서버 현재 시점의 `FBodyInstance` primitive
- `Red`: 클라이언트 발사 시점으로 리와인드된 `FBodyInstance` primitive
- `White`: 해당 발사 요청의 트레이스 궤적(Origin -> End)
- `Yellow`: 최종 확정 히트 포인트

---

## 원칙
- 임시 위치 캡슐 디버그는 사용하지 않는다.
- 반드시 HitBones의 실제 물리 primitive를 동일하게 그린다.
- 기준 데이터는 메시 본 transform이 아니라 `FBodyInstance` + `UBodySetup::AggGeom`이다.

---

## 현재 코드 기준 배치
- SSR 핵심 로직은 `UEPServerSideRewindComponent::ConfirmHitscan`에 있다.
- 따라서 디버그 draw도 여기서 수행하는 것이 맞다.
- `UEPCombatComponent`는 최종 데미지 적용/이펙트만 담당한다.

---

## 토글 설계
`UEPCombatDeveloperSettings`에 추가:
- `bEnableSSRDebugDraw` (bool)
- `SSRDebugDrawDuration` (float, default 2.0)
- `SSRDebugLineThickness` (float, default 1.5)
- `bEnableSSRDebugLog` (bool)

가드:
```cpp
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
// Debug draw code
#endif
```

---

## 구현 포인트

### 1) 서버 현재 primitive 표시 (Blue)
위치: `ConfirmHitscan` 내부, 후보 타깃(`Candidates`) 루프

- 리와인드 적용 전, 각 HitBone에 대해:
  - `FBodyInstance* Body = Char->GetMesh()->GetBodyInstance(BoneName)`
  - `Body->GetUnrealWorldTransform()`를 기준으로 `AggGeom`을 draw
  - 색상은 `FColor::Blue`

### 2) 리와인드 primitive 표시 (Red)
위치: 스냅샷(`Snap`) 적용 직후

- 동일한 본 목록을 순회하며:
  - 적용된 `Body`의 월드 transform 기준 `AggGeom`을 draw
  - 색상은 `FColor::Red`

### 3) 발사 궤적 표시 (White)
위치: `Directions`를 순회하며 `LineTraceSingleByChannel` 직전

- `End = Origin + Dir * TraceDistanceCm`
- `DrawDebugLine(..., Origin, End, FColor::White, ...)`

### 4) 히트 지점 표시 (Yellow)
위치: `LineTraceSingleByChannel`가 true이고 후보군 필터 통과 후

- `DrawDebugSphere(..., Hit.ImpactPoint, 8~12.f, ..., FColor::Yellow, ...)`
- 필요 시 노멀:
  - `DrawDebugDirectionalArrow(..., Hit.ImpactPoint, Hit.ImpactPoint + Hit.ImpactNormal * 30.f, ..., FColor::Yellow, ...)`

---

## AggGeom draw 규칙 (필수)
- `FSphereElem` -> `DrawDebugSphere`
- `FKSphylElem` -> `DrawDebugCapsule`
- `FKBoxElem` -> `DrawDebugBox`
- (필요 시) `FKConvexElem` -> `DrawDebugSolidMesh` 또는 생략 + 로그

변환:
- Shape 로컬 transform + Body 월드 transform 합성 후 월드 pose 계산
- 스케일은 `Body` 월드 스케일 반영
- 본 하나에 primitive 여러 개면 전부 draw

권장 헬퍼:
- `DrawBodyInstancePrimitives(UWorld*, const FBodyInstance*, const FColor, float Duration, float Thickness)`

---

## 최소 로그 세트
- `ServerNow`, `ClientFireTime`, `ServerNow - ClientFireTime`
- `Candidates.Num()`, `OutConfirmedHits.Num()`
- `HitActor`, `Hit.BoneName`, `Hit.PhysMaterial`

로그와 draw를 같이 봐야 타임윈도우/후보군/충돌채널 문제를 분리할 수 있다.

---

## 체크리스트 (실전 테스트)
1. PIE 2클라 + Dedicated Server + 지연 200/200 + PacketLoss 5%
2. 디버그 ON 후 30발 테스트
3. 판정 규칙:
   - Red primitive와 White 선이 교차하는데 히트가 없으면: 채널/응답/후보군 필터 점검
   - Blue primitive 쪽만 일치하면: 리와인드 미적용 또는 타임스탬프 문제 점검
   - Red/Blue 차이가 거의 없으면: 리와인드 윈도우/클램프/히스토리 점검

---

## 작업 순서
1. `UEPCombatDeveloperSettings`에 디버그 토글/수치 추가
2. `DrawBodyInstancePrimitives` 헬퍼 구현 (`AggGeom` 기반)
3. `UEPServerSideRewindComponent::ConfirmHitscan`에 Blue/Red/White/Yellow draw 추가
3. 로그 4종 추가
4. 200/200 환경에서 판독 후 문제 원인 분리
