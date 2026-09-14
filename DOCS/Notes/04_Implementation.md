# 4단계 구현 설계서: GAS 실무 도입 (EmploymentProj)

> 목표: 기존 RPC 전투 구조를 한 번에 버리지 않고, GAS 기반으로 안전하게 이전한다.
> 원칙: 입력은 Action으로 통일하고, 아이템 조작은 Ability로 처리한다.

적용 트랙:
- 이 문서는 기본 구현 트랙(main 01→04) 완료 후,
- `feature/item-gas-architecture` 같은 리팩터링 트랙에서 적용하는 것을 기본 원칙으로 한다.
- 단, Step 1~2(ASC 초기화/태그 체계)는 main에서도 선행 실험 가능하다.

---

## 0. 도입 범위

이번 단계에서 최소 달성:

- ASC/AttributeSet/Tag 파이프라인 안정화
- `PrimaryUse`, `SecondaryUse`, `Reload`, `Interact` 입력을 GAS Ability로 연결
- 현재 `AEPCharacter` 사격 RPC와 병행 운용 후 점진 제거

---

## 1. 아키텍처 목표

### 1-1. 책임 분리

| 대상 | 책임 |
|------|------|
| Character | 이동, 카메라, 입력 전달 |
| ASC (PlayerState) | Ability 활성화, GE 적용, 태그 상태 |
| ItemInstance | 탄약/내구도/부착물 등 런타임 상태 |
| Ability | 실제 사용 로직 (발사/재장전/사용/상호작용) |

### 1-2. 입력 추상화

```
LMB -> Input.Action.PrimaryUse
RMB -> Input.Action.SecondaryUse
R -> Input.Action.Reload
F -> Input.Action.Interact
```

키는 바뀔 수 있지만 Action 의미는 유지된다.

---

## 2. 구현 순서

### Step 1. GAS 기반 클래스 준비

1. `Build.cs`에 GAS 모듈 확인
2. `AEPPlayerState`에 ASC + AttributeSet 추가
3. `AEPCharacter`에서 `IAbilitySystemInterface` 구현
4. `PossessedBy` / `OnRep_PlayerState`에서 `InitAbilityActorInfo`

완료 기준:
- 서버/클라 모두 `GetAbilitySystemComponent()` 정상
- PIE 멀티에서 null 없이 초기화 로그 확인

### Step 2. 최소 태그 체계 확정

필수 태그:

```
Input.Action.PrimaryUse
Input.Action.SecondaryUse
Input.Action.Reload
Input.Action.Interact

State.Dead
State.Reloading
State.UsingItem
State.ADS
```

완료 기준:
- 태그 테이블과 코드 참조 경로 일치
- 문자열 하드코딩 금지, 네이티브 태그 매크로 사용

### Step 3. 입력 바인딩을 Ability 활성화로 전환

1. `EnhancedInput` 액션을 기존 함수 대신 "ASC 입력 전달"로 연결
2. Pressed/Released 모두 처리
3. 아직 기존 RPC는 유지 (fallback)

완료 기준:
- 입력 이벤트가 Ability 활성화 요청까지 도달
- 기존 전투 로직과 공존 가능

### Step 4. 아이템 공용 Ability 4종 구현

1. `GA_Item_PrimaryUse`
2. `GA_Item_SecondaryUse`
3. `GA_Item_Reload`
4. `GA_World_Interact`

공통 규칙:
- Ability는 "현재 장착 ItemInstance"를 조회
- 타입 캐스팅 난발 대신 인터페이스 호출
- 서버 권한 로직은 Ability 내부에서 최종 검증

완료 기준:
- 장착 아이템 종류가 달라도 같은 입력 파이프라인으로 동작

### Step 5. ItemInstance 도입

1. 인벤토리 엔트리에 ItemData 참조 + 상태 데이터 저장
   - 확정 구조: `ItemId(Row/Definition 키) + ItemInstance 상태`
2. 무기 탄약/장전 상태를 Actor에서 Instance로 이동
3. 장착 시 Ability Grant, 해제 시 Remove

완료 기준:
- 무기 Actor 재스폰/드랍과 무관하게 상태 보존
- Ability 누수 없이 장착/해제 반복 가능

### Step 6. 기존 RPC 단계적 제거

1. `Server_Fire`, `Server_Reload` 사용 경로를 Ability 중심으로 치환
2. 동작 안정 후 Character 직접 전투 함수 삭제
3. 문서/블로그 코드 스니펫도 최신화

완료 기준:
- Character에 총기 전용 핵심 로직이 남지 않음

---

## 3. 테스트 시나리오 (멀티 기준)

1. 전투 입력
- 클라 2인, 동일 무기 장착
- LMB 연사, R 재장전, RMB 보조동작 반복
- 태그 충돌(`State.Reloading` 중 발사 금지) 확인

2. 장착 교체
- 무기 A/B 교체 시 Ability Grant/Remove 누수 확인
- 교체 직후 입력 오동작 여부 확인

3. 서버 권한
- 클라에서 비정상 연사 시도
- 서버 Ability 검증에서 거부되는지 확인

4. 예측/보정
- LocalPredicted Ability에서 서버 거부 시 롤백 확인
- 탄약/상태/UI 불일치 여부 확인

---

## 4. Do / Don't

Do:
- 입력을 Action 단위로 고정
- Ability에서 태그로 충돌 제어
- Item 상태는 Instance 단위로 관리

Don't:
- 키 입력을 아이템 클래스마다 직접 처리
- Character에 무기 세부 로직 재집중
- 장착 해제 시 GrantedAbility 정리 생략

---

## 5. 최종 완료 기준

- [ ] GAS 초기화/ASC/AttributeSet 안정화
- [ ] Action 입력 4종이 Ability로 동작
- [ ] ItemInstance 기반 상태 보존
- [ ] Character 전투 로직이 Component/Ability로 이관
- [ ] 멀티 테스트(2~3 클라)에서 권한/복제/예측 문제 없음
