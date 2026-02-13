# 2단계 보강: CombatComponent 리팩터링 (GAS 전환 전 기반 작업)

> 기준 문서: `DOCS/Mine/Item.md`
> 핵심 모델: `FTableRowBase(Row)` + `UItemDefinition(UPrimaryDataAsset)` + `UItemInstance`
> 위치: **완전 GAS화 이전의 준비 단계(Pre-GAS Foundation)**

---

## 0. 목표

`AEPCharacter`에 몰린 전투 책임을 `UEPCombatComponent`로 분리하면서,
완전 GAS 이관 전에 필요한 구조적 기반을 깔아둔다.

이번 단계 목표는 "완전 GAS화"가 아니라 아래 2가지다.

1. Character에서 전투 코드 분리
2. 무기 상태 소스 오브 트루스를 `ItemInstance`로 옮기기 쉬운 형태로 정리

---

## 1. 아키텍처 전제

### 1-1. 데이터 소스

| 계층 | 역할 |
|------|------|
| `FEPItemRow` | 운영 수치 (가격/스택/슬롯/드랍 그룹/확률 등) |
| `UEPItemDefinition` | 정적 참조 (메시/아이콘/FX/SFX/Ability 클래스) |
| `UEPItemInstance` | 런타임 상태 (수량/탄약/내구도/부착물) |

### 1-2. 런타임 소유

| 대상 | 소유 |
|------|------|
| Character | 현재 장착 슬롯 선택 상태 |
| InventoryComponent | `UEPItemInstance` 목록 |
| CombatComponent | 현재 장착 상태를 사용해 발사/재장전 실행 (GAS 이관 전 임시 실행 레이어) |
| Weapon Actor | 월드 표현체(시각/애니/총구 소켓) |

중요:
- `AEPWeapon`은 최종적으로 상태 원본이 아니다.
- Pre-GAS 단계에서는 기존 무기 상태와 병행 가능하되, 호출 경로는 CombatComponent로 통일한다.

---

## 2. 현재 문제와 교정 방향

현재 문제:

1. Character가 입력/이동/전투/RPC/이펙트를 모두 담당
2. `EquippedWeapon` 중심이라 무기 외 아이템 확장이 어려움
3. 발사/탄착 이펙트 좌표/호출 버그가 Character 내부에서 반복됨

교정 방향:

1. 전투 로직을 `UEPCombatComponent`로 이동
2. CombatComponent는 `EquippedItemInstance`를 기준으로 동작
3. Weapon Actor는 소켓 위치/시각 표현 담당

---

## 3. 1차 리팩터링 범위

### 3-1. Character에 남길 것

- 이동/시점/점프/스프린트/ADS
- 입력 수집
- `Input_Fire -> CombatComponent` 위임
- `SetEquippedWeaponActor` 같은 표현체 세팅 위임 퍼사드

### 3-2. CombatComponent로 이동할 것

1. 발사 입력 사전검증 (`연사속도`, `장착 상태`)
2. `Server_Fire`, `Server_Reload`
3. `Multicast_PlayMuzzleEffect`, `Multicast_PlayImpactEffect`
4. 장착 표현체 attach/동기화
5. VFX/SFX 참조(최종적으로는 Definition 참조로 교체)

### 3-3. 상태 소스 전환 (Pre-GAS 과도기)

기존:
- `AEPWeapon::CurrentAmmo` 중심

목표(준비 단계):
- 최종 목표는 `UEPItemInstance::CurrentAmmo` 중심
- 현재 단계는 `AEPWeapon::CurrentAmmo`와 병행 허용
- 단, 외부 진입점은 CombatComponent로 단일화

---

## 4. 구현 단계

### Step A. 컴포넌트 분리

1. `Combat/EPCombatComponent.h/.cpp` 생성
2. Character에 `CombatComponent` 생성 및 참조
3. 발사/재장전/RPC/FX 코드를 Component로 이동

완료 기준:
- Character 전투 함수 대부분 제거
- 기존 발사 동작 parity 유지

### Step B. 장착 데이터 축 분리

1. Character/Inventory에서 `EquippedPrimaryItemInstanceId` 유지
2. CombatComponent가 인벤토리에서 Instance 조회
3. 표현체(`AEPWeapon`)는 Instance 기반으로 갱신

완료 기준:
- 무기 교체 시 Instance 기준으로 상태 지속

### Step C. 과도기 동기화

1. 발사 시 기존 탄약 로직 유지
2. 필요 시 Instance 미러링 필드만 준비 (즉시 강제 전환하지 않음)
3. UI/로직 진입점은 Component 기준으로 먼저 정렬

완료 기준:
- GAS 이관 시 탄약 소스 전환이 파일 대규모 수정 없이 가능

### Step D. GAS 연결 준비

1. `PrimaryUse/Reload` 입력을 CombatComponent 진입점으로 고정
2. 이후 동일 진입점을 GAS Ability에서 호출 가능하게 정리

완료 기준:
- `GA_Item_PrimaryUse` 이관 시 Character 수정 최소화
- Pre-GAS 구현을 유지한 채 단계적 치환 가능

---

## 5. 코드 레벨 규칙

1. RPC `_Implementation` 직접 호출 금지
- 항상 선언된 RPC 함수명 호출

2. 좌표 분리
- 트레이스 시작: 카메라
- 머즐 이펙트: WeaponMesh `MuzzleSocket`
- 탄착 이펙트: Hit `ImpactPoint/ImpactNormal`

3. 널 가드 필수
- `Definition`, `Row`, `Instance`, `FX/SFX` 모두 null 체크

4. Attach 검증
- 서버 장착 시 attach
- OnRep 장착 시 attach

---

## 6. 테스트 체크리스트 (PIE 2~3인)

- [ ] 발사 시 머즐 이펙트가 총구 소켓에 붙음
- [ ] 히트 시 임팩트 이펙트가 올바른 위치에 생성
- [ ] 사람/월드 히트 판정이 동일하게 동작
- [ ] 연사속도/재장전 제약 유지
- [ ] 탄약 상태가 Instance 기준으로 유지
- [ ] Character 코드량 감소 확인

---

## 7. 다음 단계 연결 (GAS 전면 이관 시)

1. Inventory를 `UEPItemInstance` 중심으로 완전 전환
2. CombatComponent의 `HandlePrimaryUse/HandleReload`를 GAS Ability로 이관
3. `AEPWeapon`의 상태 로직 축소(표현체화)
