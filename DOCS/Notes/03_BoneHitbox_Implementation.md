# 3단계 외전: BoneHitBox 구현 (Pre-GAS, GAS 전환 대비)

## 목적
- 지금은 `ApplyPointDamage` 기반으로 동작시킨다.
- 나중에 GAS로 전환할 때, `히트 정보 수집 로직`을 그대로 재사용한다.
- Lyra 방식처럼 `Physical Material + Gameplay Tag + 무기별 배율` 구조를 미리 깔아둔다.

---

## 핵심 설계 (실무 기준)
1. 판정 책임 분리
- `EPCombatComponent`: 트레이스/히트 수집/최종 데미지 계산 입력값 조합
- `EPWeapon(혹은 WeaponDefinition)`: 무기 고유 배율 데이터 보관
- `EPCharacter`: 받은 최종 데미지 반영(HP, 사망 처리)

2. 배율 분리
- 본 배율: `Hit.BoneName` 기반
- 재질 배율: `Hit.PhysMaterial`에 달린 태그 기반
- 최종 공식: `FinalDamage = BaseDamage * BoneMultiplier * MaterialMultiplier`

3. GAS 전환성
- 지금은 `ApplyPointDamage(FinalDamage)` 호출
- 나중엔 동일 계산 입력을 `GameplayEffectSpec`의 `SetByCaller`로 넘기면 된다.

---

## 권장 데이터 구조

### A. Physical Material 태그
- 일반 피직스 머티리얼: 예) `PM_Default`
- 약점 피직스 머티리얼: 예) `PM_WeakSpot`
- 약점 쪽에 태그 부여: `Gameplay.Zone.WeakSpot`

Lyra처럼 쓰려면 커스텀 PhysicalMaterial 클래스에 태그 컨테이너를 두는 방식이 가장 깔끔하다.
- 예시 클래스: `UEPPhysicalMaterialWithTags : UPhysicalMaterial`
- 예시 멤버: `FGameplayTagContainer MaterialTags`

### B. 무기 배율 맵
- 무기 데이터(권장: `UEPWeaponDefinition`)에 아래 멤버 추가:
```cpp
UPROPERTY(EditAnywhere, Category = "Weapon Config")
TMap<FGameplayTag, float> MaterialDamageMultiplier;
```
- 예시 세팅:
  - `Gameplay.Zone.WeakSpot -> 2.0`

참고:
- 지금 프로젝트에서 실제 발사 주체가 `AEPWeapon + WeaponDef`라면, 우선 `WeaponDefinition`에 두는 게 자연스럽다.
- 이후 인챈트/모드/탄종 시스템이 커지면 `WeaponInstance`에서 최종 배율을 오버라이드하는 계층을 추가한다.

---

## 구현 순서

## Step 1) 트레이스 히트 정보 품질 올리기
- `LineTraceSingleByChannel`의 `FCollisionQueryParams`에 `bReturnPhysicalMaterial = true`를 켠다.
- 반드시 `FHitResult`에서 다음을 확보한다.
  - `Hit.BoneName`
  - `Hit.PhysMaterial`
  - `Hit.ImpactPoint`, `Hit.ImpactNormal`

목적:
- 지금 당장 데미지 처리뿐 아니라, 나중에 GAS Execution에서 같은 데이터를 재사용하기 위함.

## Step 2) 본 배율 함수 만들기
- `BoneName -> Multiplier` 매핑 함수를 한 곳에 둔다.
- 최소 추천:
  - Head: `2.0`
  - Spine/Torso: `1.0`
  - Limb: `0.75`

주의:
- 이 함수는 `Character::TakeDamage`보다 `CombatComponent` 또는 별도 `DamageHelper`에 두는 쪽이 전환이 쉽다.

## Step 3) 재질 태그 배율 함수 만들기
- 히트한 `PhysMaterial`에서 태그를 읽는다.
- 무기의 `MaterialDamageMultiplier` 맵에서 매칭되는 태그 배율을 찾는다.
- 태그가 여러 개인 경우:
  - 기본은 `최대값` 선택을 권장(보수적이고 디버깅 쉬움).

기본값:
- 태그 없음/매칭 실패 시 `1.0`.

## Step 4) 최종 데미지 계산 위치 고정
- `Server_Fire` 내부에서 `FinalDamage`를 계산한 뒤 `ApplyPointDamage` 호출.
- `ApplyDamage` 대신 `ApplyPointDamage`를 쓰는 이유:
  - `FPointDamageEvent`로 Bone/Impact 정보를 유지할 수 있음.

즉시 전환 포인트:
- 이후 GAS 도입 시 이 구간을 `ApplyGameplayEffectSpecToTarget`로 교체하면 된다.

## Step 5) 캐릭터는 결과만 반영
- `AEPCharacter::TakeDamage`는 가능하면 단순화:
  - HP 감소
  - 사망/피격 리액션
- 본/재질 배율 재계산은 여기서 하지 않는다.

이유:
- 서버 데미지 계산 책임을 한 곳으로 모아야 중복 계산/불일치 버그를 줄일 수 있다.

---

## Lag Compensation과 결합 시 기준
- 리와인드로 얻은 `RewindHit`를 최종 판정 히트로 사용한다.
- 데미지 계산도 반드시 `RewindHit` 기준으로 한다.
- 이펙트(`Impact FX`)는 시각 동기화 목적이므로 `PreHit` 또는 `RewindHit` 중 팀 룰을 정해 일관되게 유지한다.

실무 권장:
- 판정/데미지: `RewindHit`
- VFX/SFX: 실제 판정 히트와 동일한 Hit 사용(디버깅 쉬움)

---

## GAS 전환 시 바뀌는 부분 (미리 알고 가기)
- 유지되는 것:
  - 트레이스
  - Bone/PhysMat 추출
  - 배율 계산 입력값
- 바뀌는 것:
  - `ApplyPointDamage` -> `GameplayEffectSpec` 적용
  - `FinalDamage`는 `SetByCaller` 또는 Execution Calculation 입력으로 전달

권장 태그 흐름:
- `Gameplay.Zone.WeakSpot`
- `Gameplay.Damage.Type.Bullet`
- `Gameplay.Damage.Source.Weapon`

---

## 에디터 세팅 체크리스트
- Physics Asset에서 각 히트 바디가 `ECC_GameTraceChannel1`을 `Block`하는지 확인
- 약점 부위(예: head) 바디에 약점용 Physical Material이 들어가 있는지 확인
- 무기 데이터의 `MaterialDamageMultiplier`에 약점 태그 배율이 들어가 있는지 확인
- 실제 PIE에서 로그로 다음을 검증
  - BoneName
  - PhysMat 이름
  - 태그 매칭 결과
  - FinalDamage

---

## 구현 완료 기준
- 같은 BaseDamage로 발사했을 때:
  - 몸통 > 기본값
  - 팔다리 < 몸통
  - 약점 PhysMat 부위 > 몸통
- 약점 태그 제거 시 배율이 즉시 1.0으로 떨어진다.
- (나중에) GAS 전환 시 트레이스/배율 코드 대부분 재사용 가능하다.
