# Step 4 블로그 포스팅 계획 — GAS 마이그레이션

## 개요

Step 4는 **기존 자체 구현 전투 시스템을 GAS(Gameplay Ability System)로 전면 이관**하는 챕터다.
`Server_Fire` RPC, `HP` UPROPERTY, `WeaponState` enum, 타이머 기반 재장전이
각각 GA / Attribute / GameplayTag / Duration GE로 교체된다.

> **포스팅 상태 기준**
> 진실의 원천은 `DOCS/Notes/04/GAS_STATUS.md`와 각 단계 STATUS 파일이다.
> **2026-07-26 기준 01~08 전 단계 구현 + PIE 검수 완료** → 8편 모두 즉시 작성 가능.
>
> **작성 가이드 8편은 모두 작성되어 있다** (`Step4_Post1_*.md` ~ `Step4_Post8_*.md`).
> 이 문서는 전체 구성과 편 간 연결을 잡는 마스터 문서이고, 실제 글감·코드·함정은 각 포스팅 파일에 있다.

### 시리즈를 관통하는 주제

포스팅 전체에서 반복해서 돌아올 축. 각 편에서 이 중 하나 이상을 구체 사례로 보여준다.

| 축 | 내용 |
|---|---|
| **복제되는 상태 vs 안 되는 상태** | `ActivationOwnedTags`는 복제 안 됨 → 다른 클라가 쿼리할 상태는 반드시 GE `GrantedTags` |
| **예측(Prediction)과 권한(Authority)** | `LocalPredicted` GA에서 클라/서버 인스턴스가 각자 돈다 — **되돌릴 수 있는 것(GE)과 없는 것(연출)의 구분**, 레이스, authority 가드 |
| **데이터를 코드에서 에셋으로** | 하드코딩 배율 → `TagDamageMultiplierMap`, 타이머 → Duration GE |
| **기존 자산 재사용** | SSR(3단계), CombatComponent 코스메틱 헬퍼는 버리지 않고 GA가 호출하는 형태로 유지 |

---

## 포스팅 목록

### Post 4-1 — 왜 GAS인가 + ASC/AttributeSet 기반 세팅
**파일**: `Step4_Post1_Foundation.md`
**참고**: `04_GAS_DOCS.md` §1~4, `04_GAS_01_Foundation.md`, `04_GAS_00_Reference.md`

**게시 제목**: `[UE5] 추출 슈터 4-1. GAS 도입: ASC를 PlayerState에 두는 이유와 AttributeSet 설계`

**한 줄 요약**: 자체 구현 전투 시스템이 부딪힌 한계를 정리하고, ASC 배치 위치·Replication Mode·NativeGameplayTags까지 GAS의 뼈대를 세우는 과정.

**다룰 내용:**
- 이관 전 구조의 문제 (상태 분산 / 복제 한계 / 확장 비용 / 수치 관리) — `04_GAS_DOCS.md` §1 표를 Before-After로 재구성
- **ASC를 Character가 아닌 PlayerState에 두는 이유** — 사망/리스폰 시 Ability Grant·GE·Attribute 보존
- `ReplicationMode::Mixed` — 소유 클라엔 GE 전체, 타 클라엔 Tag/Cue만. 요구조건은 PlayerState의 Owner가 Controller일 것
- **`InitAbilityActorInfo`를 서버(`PossessedBy`)와 클라(`OnRep_PlayerState`) 양쪽에서 호출해야 하는 이유** — 클라 누락 시 `Can't activate LocalOnly or LocalPredicted ability`
- `UEPAttributeSet` — `Health` / `MaxHealth` / 메타 `IncomingDamage`, `ATTRIBUTE_ACCESSORS` 매크로
- `DOREPLIFETIME_CONDITION_NOTIFY` + `GAMEPLAYATTRIBUTE_REPNOTIFY` 쌍이 필요한 이유 (`REPNOTIFY_Always`)
- `UAbilitySystemGlobals::InitGlobalData()`를 GameInstance에서 호출 — 빠뜨리면 GE가 조용히 무시됨
- 태그를 `NativeGameplayTags`로 선언하는 이유 (문자열 오타 → 컴파일 타임 검출)

**핵심 기술 키워드**: AbilitySystemComponent, PlayerState, Replication Mode Mixed, InitAbilityActorInfo, AttributeSet, NativeGameplayTags

---

### Post 4-2 — 데미지/HP 파이프라인: 메타 어트리뷰트와 사망 처리
**파일**: `Step4_Post2_DamagePipeline.md`
**참고**: `04_GAS_02_DamagePipeline.md`, `04_GAS_00_Reference.md` §4

**게시 제목**: `[UE5] 추출 슈터 4-2. TakeDamage를 버리다: 메타 어트리뷰트 데미지 파이프라인과 GA_Death`

**한 줄 요약**: `TakeDamage()` + `HP` UPROPERTY를 `GE_Damage` → `IncomingDamage` → `PostGameplayEffectExecute` 파이프라인으로 교체하고, 사망을 이벤트 기반 어빌리티로 분리한 과정.

**다룰 내용:**
- **메타 어트리뷰트 패턴** — 왜 `Health`를 직접 깎지 않고 `IncomingDamage`를 경유하는가 (방어력·실드·배율이 끼어들 지점을 한 곳으로 모음)
- `PreAttributeChange`(클램핑) vs `PostGameplayEffectExecute`(실제 변환) 역할 구분
- `GE_Damage` — Instant, `SetByCaller(Data.Damage)`. **SetByCaller는 반드시 GameplayTag 버전** (FName 버전 금지 — 오타가 런타임까지 감)
- `ApplyGEDamage` 헬퍼와 `HasAuthority()` 가드
- **`State.Dead`를 `ActivationOwnedTags`가 아니라 `GE_State_Dead`(Infinite)로 부여하는 이유** — 다른 클라이언트에서 `IsDead` 쿼리가 필요하기 때문
- `Event.Death` → `GA_Death` AbilityTrigger → `Multicast_Die()` 흐름
- 사망 이벤트 중복 발생 함정 (Health가 이미 0이면 얼리 리턴)
- 3단계에서 만든 부위 판정이 그대로 살아있음을 언급 → 4-6으로 연결

**핵심 기술 키워드**: IncomingDamage, PostGameplayEffectExecute, SetByCaller, GE_State_Dead, GA_Death, AbilityTrigger

---

### Post 4-3 — 발사 어빌리티: Server_Fire RPC를 GA로 대체
**파일**: `Step4_Post3_PrimaryUse.md`
**참고**: `04_GAS_03_PrimaryUse.md`

**게시 제목**: `[UE5] 추출 슈터 4-3. 발사 RPC를 어빌리티로: LocalPredicted GA와 쿨타임 GE`

**한 줄 요약**: 커스텀 `Server_Fire` RPC + `LastServerFireTime` 수동 검증을 `GA_Item_PrimaryUse`(LocalPredicted) + `GE_FireCooldown`으로 교체하고, 무기 장착에 GA 수명을 묶은 과정.

**다룰 내용:**
- **입력 추상화** — `CombatComponent->RequestFire()` 직접 호출 → `ASC->TryActivateAbilitiesByTag()` 단일 경로. 키/무기가 바뀌어도 Character 코드 무변경
- `LocalPredicted` 실행 흐름 (클라 즉시 실행 → 서버 검증 → 거부 시 **예측 GE만** 되돌아옴)
- **`LastServerFireTime` 수동 검증을 `GE_FireCooldown`(HasDuration + `SetByCaller(Data.Cooldown)`)으로 교체** — FireRate를 데이터로
- `CommitAbility` / `GetCooldownTags()` 오버라이드 — CooldownTags와 GE GrantedTags가 같은 태그여야 함
- **차단 조건은 `ActivateAbility`가 아니라 `CanActivateAbility`에서** — 서버가 거부해도 이미 뿌린 총구 화염은 **되돌릴 수단이 없다** (탄약만 복구된다)
- 무기 장착 = GA Grant, 해제 = `ClearAbility` — 핸들 관리 누락 시 GA 중복/잔류
- **3단계 SSR 재사용** — `ConfirmHitscan` 호출 위치만 CombatComponent → GA로 이동, 구조 변경 없음
- 코스메틱은 GA로 못 감 — SimProxy에서 GA가 안 돌기 때문에 Multicast RPC 유지

**핵심 기술 키워드**: GA_Item_PrimaryUse, LocalPredicted, CommitAbility, GE_FireCooldown, CanActivateAbility, GiveAbility/ClearAbility

---

### Post 4-4 — 재장전: 복제되는 상태 태그와 AbilityTask
**파일**: `Step4_Post4_Reload.md`
**참고**: `04_GAS_04_Reload.md`, `04_GAS_04_Reload_STATUS.md`

**게시 제목**: `[UE5] 추출 슈터 4-4. 재장전 어빌리티: 타이머를 AbilityTask로, enum을 태그로`

**한 줄 요약**: `StartReload/FinishReload` + 타이머 + `WeaponState` enum을 `GA_Item_Reload` + `WaitDelay` + `GE_Reloading`으로 바꾸고, 탄약을 Attribute로 옮긴 과정.

**다룰 내용:**
- `Ammo` / `MaxAmmo` Attribute 신설 — `AEPWeapon::CurrentAmmo` UPROPERTY 제거
- **`WeaponState` enum이 복제 안 돼서 생긴 실제 문제** — 다른 클라이언트에서 "재장전 중" 판정 불가 → `GE_Reloading`(HasDuration, GrantedTags `State.Reloading`)으로 해결. 시리즈 관통 주제의 가장 명확한 사례
- `ActivationBlockedTags`에 `State.Reloading` → 발사·중복 재장전 자동 차단
- `UAbilityTask_WaitDelay` — **C++에서는 `ReadyForActivation()` 수동 호출 필수**
- `GE_Reload_Ammo` — Override + AttributeBased(MaxAmmo, Source, `bSnapshot=false`). 무기별 MaxAmmo가 달라도 에셋 하나로 처리
- `EndAbility`에서 `RemoveActiveGameplayEffect(ReloadingEffectHandle)` — 취소 시 태그 잔류 방지
- **STATUS 기록 버그 2건 (실제 겪은 것)**
  - 사망 시 `GA_Item_Reload`가 취소되지 않던 문제
  - PIE 시작 직후 LocalPredicted 어빌리티 활성화 실패 (초기화 타이밍)

**핵심 기술 키워드**: GA_Item_Reload, GE_Reloading, ActivationBlockedTags, UAbilityTask_WaitDelay, AttributeBased Modifier

---

### Post 4-5 — 탄 분포와 탄흔: CDF 역변환 샘플링 + 데칼
**파일**: `Step4_Post5_SpreadDecal.md`
**참고**: `04_GAS_05_Spread.md`, `04_GAS_05_WeaponDecals.md`, `04_GAS_05_WeaponDecals_STATUS.md`

**게시 제목**: `[UE5] 추출 슈터 4-5. 산탄 분포 설계: CDF 역변환 샘플링과 층화 샘플링, 그리고 탄흔 데칼`

**한 줄 요약**: 균일 난수 산탄이 도넛 모양으로 뭉치는 문제를 커브 기반 CDF 역변환 샘플링으로 풀고, 층화 샘플링으로 각도 편중까지 잡은 뒤 탄흔 데칼을 붙인 과정.

**다룰 내용:**
- 균일 난수의 문제 — 원 안에서 `(r, θ)`를 균일 추출하면 중심이 비고 바깥이 몰림
- **커브 에셋(CDF)으로 반경 분포를 디자이너가 직접 그리게 하는 구조** — 코드 수정 없이 무기별 탄착군 튜닝
- 역변환 샘플링(Inverse Transform Sampling) 개념을 그림으로
- **층화 샘플링(Stratified Sampling)** — Phi(각도) 편중 문제와 수정 방식. 펠릿 N개를 N개 구간에 나눠 배정
- 데칼 아키텍처 — `PlayLocalImpactEffect` / `Multicast_PlayImpactEffect` 배열 시그니처 / 무기 BP의 `BP_PlayImpactEffect` 오버라이드
- **선행 버그: `ConfirmHitscan`이 환경 히트를 버리고 있었음** — SSR은 캐릭터 판정용이라 벽 히트를 반환하지 않아 데칼이 안 찍힘
- **STATUS 기록 버그: 샷건 10발 중 2발만 데칼** — 배열 전달로 수정
- 종료 시 `Set Fade Screen Size` null 오류

**핵심 기술 키워드**: CDF, Inverse Transform Sampling, Stratified Sampling, UCurveFloat, SpawnDecalAtLocation, Multicast RPC

---

### Post 4-6 — 부위별 데미지: bool 플래그에서 GameplayTag로
**파일**: `Step4_Post6_HitZoneDamage.md`
**참고**: `04_GAS_06_HitZoneDamage.md`, `04_GAS_06_HitZoneDamage_STATUS.md`

**게시 제목**: `[UE5] 추출 슈터 4-6. 부위별 데미지 태그화: bIsWeakSpot을 버리고 HitZone 태그로`

**한 줄 요약**: `bIsWeakSpot` bool + `BoneDamageMultiplierMap`(본 이름 문자열)을 `HitZone.*` 태그 + `TagDamageMultiplierMap`으로 바꿔, 부위 정의와 무기별 배율을 분리한 과정.

**다룰 내용:**
- 기존 구조의 한계 — bool은 "약점이냐 아니냐" 2단계뿐, 본 이름 매칭은 스켈레톤에 종속
- **★ 이 편의 진짜 소재** — `BoneDamageMultiplierMap`은 `UPROPERTY`가 없어 **한 번도 채워진 적이 없었다.** `GetBoneMultiplier`는 늘 1.0을 반환했고, 헤드샷이 아팠던 건 PhysicalMaterial 쪽 덕이었다. **3단계 포스팅 수정본과 서술을 반드시 맞출 것**
- **`UEPPhysicalMaterial::MaterialTags`(FGameplayTagContainer)** — 부위 정의를 PhysicalMaterial 에셋에 위임
- **`UEPWeaponDefinition::TagDamageMultiplierMap`(TMap\<FGameplayTag, float\>)** — 배율은 무기가 소유. 같은 헤드샷도 무기마다 배율이 다를 수 있음
- `GetBoneMultiplier` + `GetMaterialMultiplier` 두 함수 → `GetTagDamageMultiplier` 하나로
- 태그 없는 부위 → 1.0x 폴백 설계
- 3단계에서 만든 Physics Asset이 그대로 재사용됨 — 각 본에 PM 에셋만 할당
- 함정: Step 1(제거)과 Step 3(교체)을 동시에 해야 컴파일이 깨지지 않음
- 향후 확장 여지 — GE Context에 HitZone 태그를 실어 부위별 방어구 어트리뷰트 감산 (`04_GAS_DOCS.md` §3)

**핵심 기술 키워드**: HitZone.Head/Chest/Limbs, MaterialTags, TagDamageMultiplierMap, UPhysicalMaterial, GameplayTagContainer

---

### Post 4-7 — 스킬 3종과 베이스 클래스: 채널링, 상호 잠금, 예측 레이스
**파일**: `Step4_Post7_Skills.md`
**참고**: `04_GAS_07_Skills.md`, `04_GAS_07_Skills_STATUS.md`

**게시 제목**: `[UE5] 추출 슈터 4-7. 스킬 시스템: 채널링 취소, 스킬 상호 잠금, 그리고 예측 레이스 버그`

**한 줄 요약**: Dash / Heal / ShieldOn 세 스킬을 만들면서 하드코딩 분기를 `UEPGA_Skill_Base` 템플릿 메서드로 수렴시키고, LocalPredicted 예측이 만든 실전 레이스 버그를 추적한 과정.

**다룰 내용:**
- 스킬 3종 설계 — Dash(순간) / Heal(3초 채널링) / ShieldOn(5초 지속 버프). 지속 형태가 셋 다 달라서 좋은 예제
- **`UEPGA_Skill_Base` 템플릿 메서드** — `CastTime` + `bInterruptibleOnDamage` + `State.Casting` 단일 잠금 태그. 스킬마다 흩어져 있던 분기를 베이스로 수렴
- 채널링 취소 — `Event.Damaged`를 `PostGameplayEffectExecute`에서 발송 → 힐 취소. **취소 시엔 쿨타임을 걸지 않는다**(쿨타임은 완료 경로에서만)
- **스킬 상호 잠금** — `State.Casting`을 `GE_CastingClass`의 GrantedTags로 부여하고 베이스의 `ActivationBlockedTags`에 넣는 방식. 새 스킬은 베이스만 상속하면 자동 편입
- **★ 이 편의 하이라이트 — 예측 레이스 버그**
  - 증상: 시전을 다 채웠는데 힐이 자주 무시됨 (태그는 정상, 쿨다운도 같이 안 돎)
  - 원인: 클라 타이머가 먼저 끝나 `EndAbility` → `ServerEndAbility` RPC가 **서버 자신의 `WaitDelay` 발화보다 먼저 도착** → 서버 인스턴스가 강제 종료되어 서버측 완료 처리 전체 스킵
  - 해결: `bServerRespectsRemoteAbilityCancellation = false`
  - 교훈: LocalPredicted에서 클라와 서버는 각자 타이머를 돌린다. RTT가 어중간하면 프레임 내 처리 순서로 결과가 갈린다
- `RemoveActiveGameplayEffect called without Authority` 경고 — `EndAbility`가 클라 예측 인스턴스에서도 도는 탓. `ActorInfo->IsNetAuthority()` 가드
- **Dash가 서버에서만 앞으로 안 가던 문제** — `GetLastMovementInputVector`는 로컬 전용. `CMC->GetCurrentAcceleration()`(saved move로 복제됨)로 교체
- 지상 Dash가 즉시 감속되는 문제 → Z 부스트로 잠깐 체공
- 이동속도 감산은 GE Modifier `Multiply`로 (Add로 하면 중첩 계산이 무너짐)
- 향후 확장 — 슬롯 태그(`InputTag.Skill.Slot1`) + `DynamicSpecSourceTags` 기반 Lyra식 스킬 배정, 변경 지점은 2곳뿐

**핵심 기술 키워드**: UEPGA_Skill_Base, State.Casting, bServerRespectsRemoteAbilityCancellation, AbilityTask, GetCurrentAcceleration, Event.Damaged

---

### Post 4-8 — HUD: GAS 상태에 반응하는 UI
**파일**: `Step4_Post8_HUD.md`
**참고**: `04_GAS_08_HUD.md`, `04_GAS_08_HUD_STATUS.md`

**게시 제목**: `[UE5] 추출 슈터 4-8. 오버워치형 HUD: 4-상태 스킬 슬롯과 인터페이스로 분리한 게이지`

**한 줄 요약**: Attribute·Tag·GE 세 계층을 전부 이벤트 구동으로 받아 스킬 슬롯 4상태와 중앙 시전 게이지를 그리고, 게이지 모양을 인터페이스로 교체 가능하게 만든 과정.

**다룰 내용:**
- **C++ 베이스 + WBP 서브클래스(`BindWidget`)** 패턴 — 로직은 C++, 배치/스타일은 디자이너
- **데이터 소스 3계층과 각각의 구독 방법**
  - Attribute(Health/Ammo) → `GetGameplayAttributeValueChangeDelegate`
  - Tag(State.Shielded 등) → `RegisterGameplayTagEvent`
  - GE(쿨타임 남은 시간) → `GetActiveEffectsTimeRemainingAndDuration` (**쿨타임 중일 때만** Tick 쿼리, 상시 폴링 금지)
- **초기화 타이밍** — PC `BeginPlay`에서 하면 클라 ASC가 null일 수 있다. `InitASC` 수렴 지점에서 `InitHUD`
- **델리게이트 수명** — ASC는 PlayerState 소속이라 위젯보다 오래 산다. `NativeDestruct`에서 해제 필수, `InitWithASC`는 재진입 안전(리스폰 재바인딩)
- 스킬 슬롯 4-상태(Ready / Cooldown / Active / Locked)와 우선순위 (`Active > Locked`)
- **`IEPGaugeVisual` 인터페이스로 게이지 로직과 모양 분리** — 링/호(머티리얼 마스크)와 막대(ProgressBar)를 같은 로직에 갈아끼움
- 채널링 게이지가 `State.Casting` 공용 태그를 보게 한 이유 — 새 스킬을 추가해도 위젯 수정 불필요
- **Mixed 모드 제약** — 쿨타임 GE 쿼리는 소유 클라 전용. 타 플레이어 UI는 복제되는 태그만 사용 가능
- `BindWidgetOptional`의 함정 — 이름이 틀려도 컴파일은 통과하고 조용히 null

**핵심 기술 키워드**: BindWidget, RegisterGameplayTagEvent, GetActiveEffectsTimeRemainingAndDuration, IEPGaugeVisual, UUserWidget::NativeDestruct

---

## 포스팅 순서 및 분량 예상

| # | 제목 | 상태 | 예상 분량 | 난이도 | 하이라이트 |
|---|---|---|---|---|---|
| 4-1 | 왜 GAS인가 + Foundation | 작성 가능 | 중 | 중 | ASC 배치 / Mixed 모드 |
| 4-2 | 데미지 파이프라인 + 사망 | 작성 가능 | 중 | 중 | 메타 어트리뷰트 |
| 4-3 | 발사 어빌리티 | 작성 가능 | 상 | 상 | RPC → LocalPredicted GA |
| 4-4 | 재장전 + 탄약 | 작성 가능 | 중 | 중 | enum → 복제되는 태그 |
| 4-5 | 스프레드 CDF + 탄흔 | 작성 가능 | 중 | 상 | 역변환·층화 샘플링 |
| 4-6 | 부위별 데미지 태그화 | 작성 가능 | 하 | 중 | bool → 태그 |
| 4-7 | 스킬 3종 + 베이스 클래스 | 작성 가능 | 상 | 상 | **예측 레이스 버그** |
| 4-8 | HUD | 작성 가능 | 상 | 중 | 이벤트 구동 UI |

> **분량 조절 여지**: 4-6은 단독으로 짧다. 분량이 부족하면 4-5와 묶어
> "무기 판정 마무리 — 탄 분포·탄흔·부위 배율" 한 편으로 합쳐 7편 구성도 가능하다.
> 다만 4-6은 "bool → 태그" 전환이라는 독립된 설계 이야기라 분리를 기본으로 둔다.

---

## 공통 포스팅 형식

Step 3과 동일한 골격에 **함정 섹션을 추가**한다.
`DOCS/Notes/04`의 각 문서 말미 "함정 & 주의사항" 표가 이 시리즈에서 가장 값어치 있는 자산이며,
GAS는 조용히 실패하는 지점이 많아 독자에게 실질적인 도움이 된다.

```
# 제목

## 개요
- 이 포스팅에서 다루는 것
- 왜 이렇게 구현했는가 (설계 의도)

## 구현 전 상태 (Before)
- 기존 구조의 한계 (자체 구현 코드 인용)

## 구현 내용
- 개념 설명 (GAS 용어는 처음 등장할 때 한 줄 정의)
- 핵심 코드 + 주석
- Blueprint GE 에셋 설정 (스크린샷 위치 표시)

## 겪은 문제
- 실제로 부딪힌 버그와 원인 추적 과정
- STATUS 파일에 기록된 항목 우선

## 함정 정리
- 표 형식: 상황 / 원인 / 해결

## 결과
- PIE 2인 멀티 검증 항목
- 한계 및 향후 개선 방향

## 참고
- 관련 문서 링크
```

---

## 작성 시 유의사항

**GAS 용어 진입장벽**
- ASC / GA / GE / AttributeSet / GameplayTag는 **4-1에서 한 번만 정의하고**, 이후 편에서는 링크로 넘긴다
- `04_GAS_00_Reference.md`가 개념 레퍼런스이므로, 4-1 작성 시 이 문서를 요약해 "용어 정리" 박스로 삽입

**Before/After가 이 시리즈의 최대 강점**
- 다른 GAS 튜토리얼과 차별화되는 지점은 **"자체 구현으로 만들어보고 한계를 겪은 뒤 GAS로 옮겼다"**는 것
- 매 편 Before 코드를 반드시 인용한다 (`04_GAS_DOCS.md` §1·§7 표가 원본)
- 제거 대상 목록(`04_GAS_DOCS.md` §7)과 유지 대상 목록(§8)을 4-1 또는 마지막 편에 로드맵으로 제시

**정직하게 남길 것**
- `GAS_STATUS.md`의 "남은 이슈" 4건 — 알면서 남겨둔 문제로 4-8 말미나 시리즈 마무리에 언급
  - `EPGA_Skill_Base.cpp:85` 피격 중단인데 `bWasCancelled = false`
  - `EPGA_Skill_ShieldOn.h:31` 쿨타임 50초 vs GAME.md 스펙 30초
  - `EPSkillSlotWidget.cpp:143-149` 상태 변화 없이 `ApplyState` 호출
  - `UEPCastGaugeWidget` 명명 부정확 (개명하려면 CoreRedirects 필요)
- 완료 판정은 항상 STATUS 파일 기준. 단계 문서는 예정 코드를 보여줄 뿐이다

**시리즈 연결**
- 3단계(SSR / Physics Asset / 부위 판정)가 GAS 이관 후에도 그대로 살아있다는 점을 4-3, 4-6에서 명시 → 시리즈 연속성
- 5단계(Loot/인벤토리)로 넘어가는 고리는 4-7 말미의 "스킬 슬롯 로드아웃" 확장 계획

---

## 참고

- `DOCS/Notes/04/GAS_STATUS.md` — **진행 상황의 진실의 원천**
- `DOCS/Notes/04/04_GAS_DOCS.md` — 총괄 기획서 (배경/아키텍처/태그/에셋/로드맵)
- `DOCS/Notes/04/04_GAS_00_Reference.md` — GAS 개념 레퍼런스
- `DOCS/Notes/04/04_GAS_0X_*.md` — 단계별 구현서
- `DOCS/Notes/04/04_GAS_0X_*_STATUS.md` — 단계별 실제 구현/버그 기록
- `DOCS/Blog/03/Step3_Final.md` — 이전 챕터 포스팅 계획 (형식 원본)
