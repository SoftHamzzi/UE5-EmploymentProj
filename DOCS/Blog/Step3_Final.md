# Step 3 블로그 포스팅 계획

## 개요

Step 3는 **BoneHitbox**와 **NetPrediction**을 하나의 챕터로 묶는다.
두 시스템은 같은 인프라(HitboxHistory, SSR 컴포넌트, Lag Compensation)를 공유하기 때문이다.

> **포스팅 상태 기준**
> - BoneHitbox — 구현 완료 → 바로 작성 가능
> - NetPrediction — 미구현 → BoneHitbox 포스팅 이후 구현 완료 후 작성

---

## 포스팅 목록

### Post 3-1 — 본 단위 히트박스: 왜 캡슐로는 부족한가
**파일**: `Step3_Post1_BoneHitbox_Concept.md`

**게시 제목**: `[UE5] 추출 슈터 3-1. 본 단위 히트박스: 캡슐 판정의 한계와 Physics Asset 설계`

**한 줄 요약**: 캡슐 단일 콜리전의 정밀도 한계를 분석하고, Physics Asset 기반 본 단위 히트박스와 전용 트레이스 채널 구조를 설계하는 과정.

**다룰 내용:**
- 캡슐 콜리전 방식의 구조적 한계 (헤드샷 판정 불가, 팔다리 관통)
- Physics Asset 에디터에서 본별 콜리전 바디 설정
- Weapon 전용 트레이스 채널 (`EP_TraceChannel_Weapon`) 신설 이유
- `FEPBoneSnapshot` / `FEPHitboxSnapshot` 구조체 설계
- `VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones` 필요 이유

**핵심 기술 키워드**: Physics Asset, TraceChannel, FEPHitboxSnapshot, FBodyInstance

---

### Post 3-2 — 서버 사이드 리와인드 컴포넌트 구현
**파일**: `Step3_Post2_SSR_Component.md`

**게시 제목**: `[UE5] 추출 슈터 3-2. 서버 사이드 리와인드: 히스토리 기록과 본 단위 보간`

**한 줄 요약**: UEPServerSideRewindComponent가 매 Tick마다 본 Transform을 기록하고, 클라이언트 발사 시각으로 히트박스를 되돌리는 전체 흐름.

**다룰 내용:**
- SSR 컴포넌트 분리 이유 (Character 비대화 방지, 서버 전용 로직 격리)
- `SaveHitboxSnapshot` — `FBodyInstance::GetUnrealWorldTransform`으로 본 Transform 기록
- `GetSnapshotAtTime` — Before/After 탐색 + per-bone `FTransform::BlendWith` 보간
- Broad Phase (`GetHitscanCandidates`) — O(N) AllPlayers 순회, 사망 캐릭터 필터
- `FBodyInstance::SetBodyTransform(ETeleportType::TeleportPhysics)` — 올바른 리와인드 API
- `ConfirmHitscan` 전체 흐름 (리와인드 → Narrow Trace → 복구)
- 디버그 시각화 (`UEPCombatDeveloperSettings`, Blue/Red/White/Yellow 라인)

**핵심 기술 키워드**: ServerSideRewind, FBodyInstance, FTransform::BlendWith, Broad Phase, Narrow Phase

---

### Post 3-3 — 판정 연결 + 부위별 데미지 + 클라이언트 예측 이펙트
**파일**: `Step3_Post3_BoneHitbox_Complete.md`

**게시 제목**: `[UE5] 추출 슈터 3-3. 히트스캔 완성: 부위 배율, 클라이언트 예측 이펙트, 래그돌 처리`

**한 줄 요약**: HandleHitscanFire에서 SSR 위임 + 부위별 데미지 배율 적용, RequestFire에서 총구 이펙트 즉시 예측 재생, 사망 시 Groom 처리까지 히트스캔 루프 완성.

**다룰 내용:**
- `HandleHitscanFire` — SSR 위임 패턴 (`ConfirmHitscan` 호출 + Damage Block 분리)
- `GetBoneMultiplier` + `GetMaterialMultiplier` (head:2.0, limb:0.75)
- `UEPPhysicalMaterial` (bIsWeakSpot + WeakSpotMultiplier)
- `ApplyPointDamage` (BoneName 전달 → 부위 배율 계산)
- `RequestFire`에서 총구 이펙트 즉시 예측 (RTT 없음)
- `Multicast_PlayMuzzleEffect` — `IsLocallyControlled()` 중복 방지 패턴
- `ClientFireTime = GS->GetServerWorldTimeSeconds()` — 시간 기준 통일
- 래그돌 사망 시 Groom(머리카락) 처리 — `GetChildrenComponents + SetVisibility`
- GAS 전환 포인트 명시 (Damage Block만 교체하면 됨)

**핵심 기술 키워드**: HandleHitscanFire, BoneMultiplier, PhysicalMaterial, ClientPrediction, GetServerWorldTimeSeconds

---

### Post 3-4 — 서버 검증 강화 + 탄도 방식 분리 (NetPrediction 전반)
**파일**: `Step3_Post4_NetPrediction_Validation.md`

**게시 제목**: `[UE5] 추출 슈터 3-4. 서버 검증과 탄도 분리: FireRate 조작 방지와 EEPBallisticType`

**한 줄 요약**: 클라이언트 RPC 스팸과 위치 조작을 서버에서 독립 검증하고, Hitscan/ProjectileFast/ProjectileSlow로 탄도 방식을 분리해 확장 가능한 발사 구조를 완성.

**다룰 내용:**
- `LastServerFireTime` — 서버 독립 FireRate 검증 (클라이언트 조작 불가)
- Origin drift 검증 (200cm 허용치, 벽 너머 조작 방지)
- `EEPBallisticType` enum 설계 이유 (`EEPFireMode`와 별개인 이유)
- `WeaponDefinition`에 `BallisticType` + `ProjectileClass` + `PelletCount` 추가
- `Server_Fire_Implementation` switch 구조로 전환
- `AEPWeapon::Fire` 오버로드 — 결정론적 RNG 펠릿 생성 (`FRandomStream(ClientFireTime * 1000)`)
- 결정론적 RNG 설계 이유 (핵 클라이언트가 방향 조작 불가)

**핵심 기술 키워드**: LastServerFireTime, EEPBallisticType, PelletCount, FRandomStream, Origin Validation

---

### Post 3-5 — 투사체 시스템: ProjectileFast vs ProjectileSlow
**파일**: `Step3_Post5_Projectile.md`

**게시 제목**: `[UE5] 추출 슈터 3-5. 투사체 시스템: 고속/저속 분리와 클라이언트 코스메틱 스폰`

**한 줄 요약**: 870m/s 소총탄은 복제 불가능하므로 서버 시뮬 + 클라 코스메틱으로 분리하고, 수류탄/로켓은 Actor 복제로 처리하는 ProjectileFast/Slow 이중 구조 설계.

**다룰 내용:**
- 고속/저속 분리 이유 (네트워크 틱레이트 vs 투사체 속도)
- `AEPProjectile` 클래스 설계 (`USphereComponent` + `UProjectileMovementComponent`)
- `SetCosmeticOnly()` — 충돌/데미지 비활성화, 궤적 렌더링 전용
- `HandleProjectileFire` — ProjectileFast/Slow 분기 (bReplicates Blueprint 설정)
- `Multicast_SpawnCosmeticProjectile` — 발사자/서버 중복 방지 패턴
- `RequestFire`에 ProjectileFast 코스메틱 즉시 스폰 추가
- `OnProjectileHit` — 서버 권한 데미지, 래그 보상 불필요 이유

**핵심 기술 키워드**: AEPProjectile, SetCosmeticOnly, ProjectileFast, ProjectileSlow, CosmeticProjectile

---

## 포스팅 순서 및 분량 예상

| # | 제목 | 상태 | 예상 분량 | 난이도 |
|---|---|---|---|---|
| 3-1 | 본 단위 히트박스 개념 + Physics Asset | 작성 가능 | 중 | 중 |
| 3-2 | SSR 컴포넌트 구현 (히스토리 + 리와인드) | 작성 가능 | 상 | 상 |
| 3-3 | 판정 연결 + 부위 데미지 + 클라 예측 | 작성 가능 | 중 | 중 |
| 3-4 | 서버 검증 + 탄도 분리 | NetPrediction 구현 후 | 중 | 중 |
| 3-5 | 투사체 시스템 | NetPrediction 구현 후 | 중 | 상 |

---

## 공통 포스팅 형식

```
# 제목

## 개요
- 이 포스팅에서 다루는 것
- 왜 이렇게 구현했는가 (설계 의도)

## 구현 전 상태 (Before)
- 기존 구조의 한계

## 구현 내용
- 개념 설명
- 핵심 코드 + 주석
- 에디터 설정 (스크린샷 위치 표시)

## 결과
- 구현 후 동작 확인 항목
- 한계 및 향후 개선 방향

## 참고
- 관련 문서 링크
```
