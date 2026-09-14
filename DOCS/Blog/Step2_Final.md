# Step 2 블로그 포스팅 계획

## 개요

Step 2에서 구현한 내용을 8개의 포스팅으로 나눈다.
각 포스팅은 독립적으로 읽을 수 있되, 순서대로 읽으면 전체 흐름이 이어지도록 구성.

> **포스팅 순서 기준**: 실제 개발 진행 순서 반영
> - CMC(2-1)는 이미 게시 완료
> - 이후 MetaHuman 통합 → 데이터 설계 → 코드 분리 → 복제/전투 → 애니메이션 → HUD 순으로 진행

---

## 포스팅 목록

### Post 2-1 — CMC 확장으로 Sprint/ADS/Crouch 네트워크 동기화 ✅ PUBLISHED
**파일**: (이미 게시됨)

**게시 제목**: `[UE5] 추출 슈터 2-1. CMC 확장으로 Sprint/ADS/Crouch 네트워크 동기화`

**한 줄 요약**: Server RPC 방식의 스냅/튐 문제를 해결하기 위해 CharacterMovementComponent를 확장하고 CompressedFlags로 Sprint/ADS를 이동 패킷에 통합.

**다뤘던 내용:**
- Server RPC 방식의 한계 (클라 예측 불일치 → 스냅 발생)
- `UEPCharacterMovement`: `GetMaxSpeed`, `UpdateFromCompressedFlags`
- `FSavedMove_EPCharacter`: `GetCompressedFlags`, `SetMoveFor`, `PrepMoveFor`, `CanCombineWith`
- `FObjectInitializer`로 기본 CMC 교체
- FLAG_Custom_0 (Sprint), FLAG_Custom_1 (ADS)
- Crouch는 CMC 내장 처리 (`bCanCrouch = true`)

---

### Post 2-2 — MetaHuman 통합
**파일**: `Step2_Post2_MetaHuman.md`

**한 줄 요약**: MetaHuman Creator로 제작한 캐릭터를 UE5 ACharacter 기반 EPCharacter에 통합하는 과정.

**다룰 내용:**
- MetaHuman Creator에서 캐릭터 제작 후 프로젝트로 임포트
- MetaHuman BP 구조 분석 (Body/Face/Outfit/Groom 컴포넌트)
- ACharacter의 GetMesh()를 Body로 활용하는 이유
- `SetLeaderPoseComponent` 패턴 (Face/Outfit이 Body 본 포즈를 따라가도록)
- FPS 시점 설정: `bOwnerNoSee`로 로컬 플레이어에게 Face/Outfit/Hair 숨기기
- `VisibilityBasedAnimTickOption` 설정 필요성 (bOwnerNoSee + LeaderPose 동시 사용 시)
- 멀티플레이어에서의 고려사항 (다른 클라이언트에게는 전신 보임)

**핵심 기술 키워드**: MetaHuman, SetLeaderPoseComponent, bOwnerNoSee, FPS Character

---

### Post 2-3 — 아이템 데이터 아키텍처 (3계층 구조)
**파일**: `Step2_Post3_ItemSystem.md`

**한 줄 요약**: 타르코프 스타일 아이템 시스템을 위한 DataTable Row + ItemDefinition + ItemInstance 3계층 구조 설계.

**다룰 내용:**
- 단순 WeaponData UObject 방식의 한계 (왜 리팩터링했나)
- 3계층 각각의 역할
  - `FEPItemData` (FTableRowBase): 밸런스/운영 수치 (가격, 무게, 희귀도)
  - `UEPItemDefinition` (UPrimaryDataAsset): 에셋 참조 (메시, 아이콘, FX, 애니레이어)
  - `UEPItemInstance` (UObject): 런타임 상태 (탄약, 내구도)
- `ItemId` (FName)으로 세 계층을 연결하는 방식
- 무기 전용 하위 클래스: `UEPWeaponDefinition`, `UEPWeaponInstance`
- DataTable 에디터 설정 및 DA_AK74 데이터 에셋 구성
- Epic 공식 Coder-05 학습 경로 참조 내용

**핵심 기술 키워드**: FTableRowBase, UPrimaryDataAsset, DataTable, ItemDefinition, ItemInstance

---

### Post 2-4 — CombatComponent & 무기 시스템
**파일**: `Step2_Post4_CombatComponent.md`

**한 줄 요약**: 비대해지는 ACharacter를 막기 위해 전투 로직을 CombatComponent로 분리하고, AEPWeapon 액터로 무기 시스템을 구성.

**다룰 내용:**
- Character 비대화 문제와 Component 분리 원칙
- `UEPCombatComponent` 설계
  - `EquipWeapon` / `UnequipWeapon`
  - `Server_Fire` RPC 위임
  - `OnRep_EquippedWeapon`
- `AEPWeapon` 액터 설계
  - `bReplicates = true`
  - WeaponMesh + WeaponDef 참조
  - `CurrentAmmo` (COND_OwnerOnly)
- 무기를 hand_r WeaponSocket에 부착하는 방법
- `LinkAnimClassLayers`로 무기 교체 시 애니메이션 레이어 교체
- 서버 권한 사격 (`Server_Fire` → 레이캐스트 → `ApplyDamage`)
- `Multicast_PlayFireEffect` (Unreliable) 총구/탄착 이펙트

**핵심 기술 키워드**: CombatComponent, AEPWeapon, WeaponSocket, Server RPC, Multicast RPC

---

### Post 2-5 — 복제(Replication) 개념 정리
**파일**: `Step2_Post5_Replication.md`

**한 줄 요약**: UE5 복제 시스템 핵심 개념 — 프로퍼티 복제, RPC 종류, HasAuthority() 패턴, 이 프로젝트에서 적용한 방식.

**다룰 내용:**
- **프로퍼티 복제**
  - `DOREPLIFETIME` / 조건별 COND (COND_None, COND_OwnerOnly 등)
  - `ReplicatedUsing` OnRep 콜백 패턴
  - `ForceNetUpdate` 사용 시점
- **RPC 종류**
  - Server / Client / Multicast
  - Reliable (중요 상태 변경) vs Unreliable (이펙트/피드백)
  - 호출 가능 조건과 실행 위치
- **HasAuthority() / ENetRole 분기 패턴**
  - 서버 권한 체크가 필요한 곳 vs 불필요한 곳
  - ROLE_Authority / ROLE_AutonomousProxy / ROLE_SimulatedProxy 구분
- **이 프로젝트 복제 설계표**
  - HP, EquippedWeapon, CurrentAmmo 등 각 변수의 동기화 방식 선택 이유
  - Sprint/ADS는 왜 UPROPERTY 복제가 아닌 CMC CompressedFlags인가

**핵심 기술 키워드**: DOREPLIFETIME, ReplicatedUsing, HasAuthority, ENetRole, Reliable/Unreliable

---

### Post 2-6 — 전투 네트워킹 (HP / 사격 / 사망 / 피격 피드백)
**파일**: `Step2_Post6_Combat_Network.md`

**한 줄 요약**: 서버 권한 HP 복제, 사격 판정, 사망 처리(셀프 래그돌), 피격 애니메이션/사운드의 멀티플레이어 동기화.

**다룰 내용:**
- `HP` 복제 (`ReplicatedUsing = OnRep_HP`, ForceNetUpdate)
- `TakeDamage` 오버라이드 패턴 (HasAuthority() 체크)
- 사망 처리: 별도 Corpse 액터 대신 **셀프 래그돌** (`Multicast_Die`)
  - 캡슐 콜리전 비활성화
  - 메시 Ragdoll 물리 활성화
  - 최종 결정 이유 (Corpse 액터 복잡도 vs 래그돌 단순성)
- 피격 애니메이션: `Multicast_PlayHitReact` (Unreliable)
  - AdditiveHitReact 슬롯 (현재 포즈 위에 additive 재생)
- 피격 사운드 2종
  - 쏜 사람: `Client_PlayHitConfirmSound` (챡) — 본인만 들음
  - 맞은 사람: `Multicast_PlayPainSound` (윽) — 전체에 들림
- FVector_NetQuantize / FVector_NetQuantizeNormal 사용 이유 (대역폭 절감)

**핵심 기술 키워드**: TakeDamage, Ragdoll, Multicast, Client RPC, AdditiveSlot, NetQuantize

---

### Post 2-7 — 애니메이션 에셋 준비 (Lyra 마이그레이션 + MetaHuman 리타게팅)
**파일**: `Step2_Post7_Animation_Assets.md`

**한 줄 요약**: Lyra 프로젝트에서 필요한 애니메이션 에셋을 선별 이주하고, MetaHuman 스켈레톤으로 리타게팅하는 과정.

**다룰 내용:**
- Lyra에서 가져올 에셋 선별 전략
  - Rig 폴더 (IK_Mannequin)
  - Locomotion/Rifle 시퀀스
  - AimOffset 포즈 시퀀스
  - Actions (Fire, Reload, Death, HitReact)
- 마이그레이션 시 딸려오는 의존 에셋 처리 (Audio, Effects, PhysicsMaterials)
- **IK Retargeter 설정**
  - MetaHuman 플러그인 내장 IK Rig 활용
  - `RTG_MannequinToMetaHuman` 생성 및 체인 매핑
  - 프리뷰로 포즈 검증
- 리타게팅 결과물 정리
  - AimOffset은 리타게터에서 직접 불가 → 리타겟된 시퀀스로 재구성
- 리타게팅 한계와 수동 보정 필요 부분

**핵심 기술 키워드**: IK Rig, IK Retargeter, RTG_MannequinToMetaHuman, 애니메이션 리타게팅

---

### Post 2-8 — 애니메이션 시스템 구현 (Linked Anim Layer + AimOffset + FABRIK + 크로스헤어)
**파일**: `Step2_Post8_Animation_System.md`

**한 줄 요약**: Lyra 스타일 Linked Anim Layer 구조로 무기 교체 시 애니메이션이 통째로 바뀌는 시스템 구현 + AimOffset, Orientation Warping, FABRIK 왼손 IK, 크로스헤어 HUD.

**다룰 내용:**
- **Linked Anim Layer 개념**
  - ALI (Animation Layer Interface) — 레이어 함수 슬롯 정의
  - ABP_EPCharacter (메인) / ABP_EPWeaponAnimLayersBase (베이스) / ABP_RifleAnimLayers (무기별)
  - Linked Anim Layer는 ABP당 레이어 함수 1회 인스턴싱 제약 → 단일 Grounded State로 처리
  - `LinkAnimClassLayers`로 런타임 무기 교체 시 애니메이션 전환
- **Property Access (Thread-Safe)**
  - GetMainAnimBPThreadSafe() C++ 캐싱 패턴
  - 워커 스레드에서 메인 AnimBP 변수 안전하게 읽기
- **AnimGraph 구조**
  - Locomotion SM → Rotate Root Bone → Orientation Warping → Cache → Layered Blend Per Bone(AimOffset) → FABRIK → DefaultSlot
  - Rotate Root Bone 위치: Layered Blend Per Bone 앞에 배치해야 상체(팔/총)에 영향 없음
- **AimOffset**: Layered Blend Per Bone으로 spine_01 이상 상체에만 적용
- **Orientation Warping**: 스트레이핑 시 하체 방향 워핑 (AnimationWarping 플러그인, Details 패널 본 설정)
- **FABRIK (왼손 그립 IK)**
  - 오른손은 Lyra 방식(IK 불필요 — 애니메이션이 올바른 그립 위치로 제작)
  - 왼손만 FABRIK: 무기 메시 LeftHandIK 소켓 기준
  - BCS_WorldSpace + GetSocketTransform
  - GetRelativeTransform으로 hand_r 로컬 스페이스 변환
  - Tip Bone 선택 (hand_l vs middle_01_l 트레이드오프)
- **크로스헤어 HUD**
  - UMG Widget, PlayerController에서 생성 (IsLocalController())
  - CrosshairSpread 동적 확산 (이동/공중/조준 상태 반영)

**핵심 기술 키워드**: Linked Anim Layer, ALI, Property Access, AimOffset, Orientation Warping, FABRIK, UMG

---

## 포스팅 순서 및 분량 예상

| # | 제목 | 상태 | 예상 분량 | 난이도 |
|---|---|---|---|---|
| 2-1 | CMC 확장 + Sprint/ADS/Crouch 동기화 | ✅ PUBLISHED | 상 | 상 |
| 2-2 | MetaHuman 통합 | 미작성 | 중 | 중 |
| 2-3 | 아이템 데이터 아키텍처 (3계층) | 미작성 | 중 | 중 |
| 2-4 | CombatComponent & 무기 시스템 | 미작성 | 중 | 중 |
| 2-5 | 복제(Replication) 개념 정리 | 미작성 | 중 | 중 |
| 2-6 | 전투 네트워킹 (HP/사격/사망/피격) | 미작성 | 중 | 중 |
| 2-7 | 애니메이션 에셋 준비 (Lyra + 리타게팅) | 미작성 | 중 | 중 |
| 2-8 | 애니메이션 시스템 구현 + 크로스헤어 | 미작성 | 상 | 상 |

---

## 공통 포스팅 형식 (각 파일 작성 시 기준)

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
