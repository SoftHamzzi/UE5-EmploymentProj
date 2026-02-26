# Post 2-7 작성 가이드 — 애니메이션 에셋 준비 (Lyra 마이그레이션 + MetaHuman 리타게팅)

> **예상 제목**: `[UE5] 추출 슈터 2-7. Lyra 애니메이션을 MetaHuman에 리타게팅하기 (IK Retargeter)`
> **참고 문서**: DOCS/Mine/Animation.md 섹션 5

---

## 개요

**이 포스팅에서 다루는 것:**
- Lyra 프로젝트에서 필요한 에셋을 선별해 이 프로젝트로 가져오는 방법
- MetaHuman 스켈레톤으로 리타게팅하는 IK Retargeter 설정 과정

**왜 이렇게 구현했는가 (설계 의도):**
- 처음부터 모든 애니메이션을 직접 제작하는 것은 포트폴리오 범위를 벗어남
- Lyra는 Epic Games 공식 샘플이라 라이선스 문제 없이 활용 가능
- 리타게팅으로 MetaHuman 스켈레톤에 Lyra 애니메이션을 그대로 적용 가능

---

## 구현 전 상태 (Before)

- 캐릭터가 T-포즈로 서 있거나, 이전 마네킹용 애니메이션이 MetaHuman에 어색하게 적용됨
- Lyra 스켈레톤(Mannequin)과 MetaHuman 스켈레톤이 달라 직접 할당 불가

---

## 구현 내용

### 1. Lyra에서 가져올 에셋 선별

**전부 가져오지 않는다. 필요한 것만 골라서:**

```
Lyra 에셋 중 필요한 것:
├── Rigs/
│   └── IK_Mannequin           ← 리타게팅 소스 IK Rig
├── Characters/Heroes/Mannequin/Animations/Locomotion/
│   ├── MM_Rifle_Idle_Hipfire
│   ├── MM_Rifle_Walk_Fwd/Bwd/Left/Right
│   ├── MM_Rifle_Jog_Fwd/Bwd/Left/Right
│   └── MM_Rifle_Crouch_Idle
├── Characters/Heroes/Mannequin/Animations/Actions/
│   ├── MM_Rifle_Fire
│   ├── MM_Rifle_Reload
│   ├── MM_Death (래그돌 대체로 사용 안 할 수 있음)
│   └── MM_HitReact
└── Characters/Heroes/Mannequin/Animations/AimOffsets/
    └── AimOffset 포즈 시퀀스들
```

**마이그레이션 시 주의:**
- 에셋 우클릭 → Asset Actions → Migrate → 이 프로젝트 Content 폴더 지정
- 의존 에셋이 자동으로 같이 딸려옴 (Audio, Effects, PhysicsMaterials 등)
- 딸려온 Lyra 전용 에셋은 확인 후 정리 필요

> **스크린샷 위치**: Lyra 에셋 Migrate 다이얼로그 화면

### 2. IK Rig — 스켈레톤 구조 정의

리타게팅은 IK Rig → IK Retargeter 2단계:

**IK Rig**: 스켈레톤의 본 체인을 의미적으로 정의 (어디가 척추, 어디가 왼팔 등)

- Lyra용: `IK_Mannequin` (Lyra 내 기존 에셋 사용)
- MetaHuman용: MetaHuman 플러그인에 내장된 IK Rig 활용 (`RTG_MetaHuman_Body` 또는 `IK_MetaHuman`)

> **스크린샷 위치**: IK Rig 에디터에서 체인(Chain) 목록 화면

### 3. IK Retargeter 생성

```
Content Browser → Animation → IK Retargeter
이름: RTG_MannequinToMetaHuman

Source IK Rig: IK_Mannequin
Target IK Rig: MetaHuman Body용 IK Rig
```

**체인 매핑 확인:**

| Lyra 체인 | MetaHuman 체인 |
|-----------|----------------|
| Spine | Spine |
| LeftArm | LeftArm |
| RightArm | RightArm |
| LeftLeg | LeftLeg |
| RightLeg | RightLeg |
| Head | Head |

- 체인 이름이 달라도 역할이 같으면 수동으로 연결 가능
- Preview에서 MetaHuman에 Mannequin 포즈가 맵핑되는지 확인

> **스크린샷 위치**: IK Retargeter에서 Source(Mannequin) / Target(MetaHuman) 비교 프리뷰

### 4. 리타게팅 실행

```
IK Retargeter 에서:
1. Source 열에서 리타겟할 시퀀스 선택 (다중 선택 가능)
2. Export Selected Animations
3. 저장 경로: Content/Characters/MetaHuman/Animations/
4. 리타겟된 시퀀스 이름: 원본_Retargeted 자동 생성
```

> **스크린샷 위치**: 리타게팅 결과 비교 (Mannequin 원본 vs MetaHuman 리타겟)

### 5. BlendSpace / AimOffset은 별도 처리

**IK Retargeter로 직접 리타게팅 불가한 에셋:**
- BlendSpace (.bs)
- AimOffset

**해결 방법:**
- AimOffset: 리타겟된 개별 포즈 시퀀스들로 새로 구성
- BlendSpace: 리타겟된 시퀀스들을 새 BlendSpace에 직접 할당

```
(기존) BS_Rifle_Idle_Walk_Jog (Mannequin용)
    → 리타겟된 시퀀스들로 Content/Characters/MetaHuman/Animations/에 새 BS 생성
    → BS_Rifle_IdleWalkRun_MH (MetaHuman용)
```

### 6. 리타게팅 한계와 수동 보정

대부분 자동으로 잘 맞지만, 손 그립 위치 등은 스켈레톤 비율 차이로 어긋날 수 있음:

- MetaHuman과 Mannequin의 팔 비율/손 크기가 달라 총 그립 위치가 맞지 않을 수 있음
- 해결: Post 2-8에서 FABRIK Left Hand IK로 왼손 위치 보정

---

## 결과

**확인 항목:**
- 리타겟된 Idle/Walk/Jog 시퀀스가 MetaHuman 캐릭터에서 자연스럽게 재생됨
- T-포즈나 손 회전 같은 심각한 오류 없음
- 리타겟된 시퀀스 파일들이 MetaHuman 전용 폴더에 정리됨

**한계 및 향후 개선:**
- 왼손 그립 위치 불일치 → Post 2-8에서 FABRIK으로 해결
- Start/Stop 애니메이션 부드러움 → GAS 이후 Distance Matching으로 개선 예정
- 총구 위치(Muzzle Socket) 오프셋은 BP_WeaponAK74에서 수동 조정

---

## 참고

- `DOCS/Mine/Animation.md` 섹션 5 (리타게팅)
- UE5 공식: IK Rig and IK Retargeter 문서
- Lyra Sample Project GitHub
