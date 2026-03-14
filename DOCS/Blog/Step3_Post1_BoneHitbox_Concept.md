# Post 3-1 작성 가이드 — 본 단위 히트박스: 개념과 기반 설계

> **예상 제목**: `[UE5] 추출 슈터 3-1. 본 단위 히트박스: 캡슐 판정의 한계와 Physics Asset 설계`
> **참고 문서**: `DOCS/Notes/03_BoneHitbox.md`, `DOCS/Notes/03_BoneHitbox_Implementation.md` Step 0~1

---

## 개요

**이 포스팅에서 다루는 것:**
- 캡슐 콜리전 기반 히트 판정의 구조적 한계
- Physics Asset으로 본별 히트박스를 구성하는 방법
- 전용 트레이스 채널과 스냅샷 구조체 설계

**왜 이렇게 구현했는가 (설계 의도):**
- 추출 슈터는 헤드샷 배율이 핵심 전투 요소 — 캡슐로는 구현 불가능
- 본 단위 히트박스는 나중에 GAS 어빌리티로 판정 방식이 바뀌어도 인프라(Physics Asset + TraceChannel)는 그대로 재사용
- TraceChannel 분리로 환경(지형, 벽)과 히트박스 판정을 완전히 격리

---

## 구현 전 상태 (Before)

```cpp
// 2단계: 캡슐 단일 콜리전으로 히트 판정
FHitResult Hit;
GetWorld()->LineTraceSingleByChannel(
    Hit, Origin, End,
    ECC_Visibility,  // 환경과 동일 채널 — 벽에도 막힘
    Params);

if (Hit.GetActor())
    UGameplayStatics::ApplyDamage(Hit.GetActor(), Damage, ...);
// 어느 부위를 맞았는지 알 수 없음
```

**문제점:**
- 헤드샷/팔다리 구분 불가 → 모든 부위 동일 데미지
- 캡슐이 실제 캐릭터 실루엣보다 크다 → "분명 피했는데 맞음" 현상
- `ECC_Visibility`로 환경과 같은 채널 사용 → 문, 창문에도 막힘

---

## 구현 내용

### 1. 캡슐 방식의 구조적 한계

포스팅에서 그림과 함께 설명:

```
캡슐 방식:
         ┌──────┐
         │      │  ← 머리도 몸통 캡슐에 포함
         │      │
         │      │  ← 팔도 몸통 캡슐에 포함
         └──────┘

본 단위 방식:
         ○      ← head: 별도 Sphere 바디
        / \
       ○   ○    ← 상체: Capsule
      / \ / \
     ○   X   ○  ← 팔: Capsule
```

- 캡슐은 단일 판정 — 맞은 본 정보(BoneName)를 알 수 없음
- Physics Asset은 각 본에 독립 바디 → `FHitResult.BoneName`에 어느 본을 맞았는지 기록

### 2. 에디터 설정 — Physics Asset

> **스크린샷 위치**: Physics Asset 에디터 → 본 선택 → Collision 탭

설정할 본 목록과 바디 형태:

| 본 이름 | 바디 형태 | 비고 |
|---------|----------|------|
| `head` | Sphere | 헤드샷 배율 적용 (약점 PM) |
| `neck_01` | Capsule | |
| `spine_03` / `spine_02` | Capsule | 상체 |
| `upperarm_l/r` | Capsule | |
| `lowerarm_l/r` | Capsule | |
| `thigh_l/r` | Capsule | 사지 감소 배율 |
| `calf_l/r` | Capsule | |

**Collision 설정:**
```
Collision Response 탭:
- EP_TraceChannel_Weapon (WeaponTrace): Block  ← 히트 판정용
- 나머지 채널: Ignore                           ← 불필요한 충돌 차단
```

> **중요**: `Simulation Generates Hit Events = true` 설정 필요 없음.
> 트레이스 응답만 Block이면 LineTrace에 반응한다.

### 3. 전용 트레이스 채널 — EP_TraceChannel_Weapon

`DefaultEngine.ini`에 추가:

```ini
[/Script/Engine.CollisionProfile]
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,bTraceType=True,bStaticObject=False,Name="WeaponTrace")
```

**왜 전용 채널이 필요한가:**
```
ECC_Visibility (기본):
  - 지형, 벽, 유리, 문 → 모두 Block
  - 적 캡슐 → Block
  → 히트박스보다 벽이 먼저 막혀 판정 누락 발생

EP_TraceChannel_Weapon (전용):
  - 지형, 벽, 환경 → Ignore (DefaultResponse = ECR_Ignore)
  - Physics Asset 바디만 → Block
  → 히트박스만 감지, 환경 완전 격리
```

> 에디터 Project Settings → Collision → Trace Channels에서 `WeaponTrace` 추가 후
> Physics Asset 각 바디의 Response를 Block으로 변경.

### 4. 스냅샷 구조체 설계

`Public/Types/EPTypes.h`에 추가:

```cpp
// 본 하나의 월드 Transform 스냅샷
USTRUCT()
struct FEPBoneSnapshot
{
    GENERATED_BODY()
    UPROPERTY() FName      BoneName;
    UPROPERTY() FTransform WorldTransform; // GetUnrealWorldTransform() 기록값
};

// 캐릭터 전체 히트박스 스냅샷 (서버 특정 시각 기준)
USTRUCT()
struct FEPHitboxSnapshot
{
    GENERATED_BODY()
    UPROPERTY() float   ServerTime = 0.f;
    UPROPERTY() FVector Location   = FVector::ZeroVector; // Broad Phase용 루트 위치
    UPROPERTY() TArray<FEPBoneSnapshot> Bones;            // Narrow Phase 리와인드용
};

// Physics Asset 바디 채널
static constexpr ECollisionChannel EP_TraceChannel_Weapon = ECC_GameTraceChannel1;
```

**Location을 함께 저장하는 이유:**
- Broad Phase(후보 선정)는 루트 위치만으로 충분 — 본 Transform 계산 생략
- Narrow Phase 직전에만 본 Transform으로 리와인드

### 5. VisibilityBasedAnimTickOption 설정

```cpp
// AEPCharacter 생성자
GetMesh()->VisibilityBasedAnimTickOption =
    EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
```

**왜 필요한가:**
- 기본값은 카메라에 보이지 않으면 애니메이션 틱 생략
- 서버는 렌더러가 없어 모든 메시가 "보이지 않음" 처리
- 이 옵션 없으면 서버에서 본 Transform이 갱신되지 않음
- → `GetBodyInstance(BoneName)->GetUnrealWorldTransform()`이 사망 직전 포즈로 고정됨

---

## 결과

**확인 항목:**
- Physics Asset에서 각 본 바디에 WeaponTrace = Block 설정 확인
- PIE에서 `Show → Collision` 켜고 캐릭터 히트박스 바디 시각 확인
- LineTrace 테스트: 환경(벽)에 막히지 않고 히트박스 바디에만 반응하는지 확인

**한계 및 향후 개선:**
- 손가락, 발가락 등 세밀한 본은 배제 (퍼포먼스 대비 판정 기여도 낮음)
- Physics Asset 바디 수가 많을수록 ConfirmHitscan 비용 상승 — 최소한의 본만 설정

---

## 참고

- `DOCS/Notes/03_BoneHitbox.md` — 개념 전체
- `DOCS/Notes/03_BoneHitbox_Implementation.md` Step 0~1
- UE5 공식: Physics Asset Editor 문서
