# EmploymentProj

UE5 C++ 멀티플레이어 익스트랙션 슈터 - 취업 포트폴리오 프로젝트

## 프로젝트 소개

**채용공고의 필수/우대 요건을 직접 구현하여 증명하는 프로토타입 프로젝트입니다.**

UE 클라이언트 프로그래머 채용공고에서 공통적으로 요구하는 핵심 역량을 실제 게임 시스템으로 구현합니다. 템플릿이나 블루프린트에 의존하지 않고, C++로 바닥부터 설계/구축합니다.

## 증명 대상 역량

### 필수요건 대응

| 채용공고 요구사항 | 프로젝트 내 구현 |
|------------------|-----------------|
| UE Gameplay Framework 이해 | GameMode/GameState/PlayerController/PlayerState/Character 전체 설계 |
| 게임모드 및 기믹 개발 | 매치 상태머신(Waiting→Playing→Ended). 자판기/탈출 판정은 설계 단계 |
| Replication 기반 네트워크 개발 | UPROPERTY Replication, OnRep 콜백, Server/Client/NetMulticast RPC |
| 서버-클라이언트 동기화 이해 | 서버 권한 히트 판정, Lag Compensation(히트박스 리와인드), Client Prediction |
| C++ 심층 이해 | 전체 게임플레이 C++ 구현, UE Reflection 시스템 활용 |

### 우대요건 대응

| 채용공고 요구사항 | 프로젝트 내 구현 |
|------------------|-----------------|
| 데디케이티드 서버 게임모드 설계 | 서버 권한 아키텍처, 전 게임 로직 서버 판정 |
| GAS 활용 | AttributeSet(HP/Stamina/Shield), GameplayEffect, GameplayAbility(발사/재장전/스킬 3종) — 완료 |
| Git 협업 | feature 브랜치 전략, PR 기반 개발 |
| Data-Driven 설계 | UPrimaryDataAsset 기반 무기/아이템/자판기 테이블 |

## 게임 컨셉

"취업"을 테마로 한 1인칭 익스트랙션 슈터 (2~8인 멀티플레이어)

```
매치 시작 → 랜덤 스폰 → 파밍(루팅 + 자판기) → 교전(PvP/AI) → 탈출 → 보상
```

- 자판기: 돈 투입 → 5초 대기(소리로 위치 노출) → 확률 아이템 배출
- 퀘스트 아이템(이력서, 자격증 등)을 모두 수집하면 "취업 성공"
- 사망 시 소지품 전부 손실, 탈출 성공 시 보존

## 핵심 기술 시스템

| 시스템 | 기술 포인트 |
|--------|-----------|
| 사격/히트 판정 | 서버 권한 레이캐스트, Lag Compensation (히트박스 히스토리 링버퍼 + 서버 리와인드) — 완료 |
| GAS | AttributeSet, GameplayEffect(데미지/쿨다운), GameplayAbility(발사/재장전/Dash/Heal/Shield) — 완료 |
| 인벤토리/장비 | 슬롯 기반 아이템 관리, 배낭/장비 서브트리, 드래그 UI — 진행 중 (`feature-loot`) |
| 자판기 / AI | 서버 권한 확률 판정 + Behavior Tree — 예정 (미착수) |

## 구현 로드맵

1. ✅ **Gameplay Framework** - 매치 흐름, 스폰, 게임 규칙 (GameMode/GameState)
2. ✅ **Replication** - 멀티플레이어 이동/상태 동기화, 사격 RPC, HP 복제
3. ✅ **Net Prediction** - 서버 검증 강화, Lag Compensation (본 기반 히트박스 리와인드), 탄도 분리
4. ✅ **GAS** - 데미지 파이프라인, 발사/재장전 어빌리티, 스킬 3종(Dash/Heal/Shield)
5. 🔧 **Loot/Inventory** - 아이템 계층, 스포너, 상호작용, 인벤토리(진행 중)

세부 진행 상황: `DOCS/DOCS.md` §5, 단계별 완료 조건은 `DOCS/Notes/`.

## 기술 스택

- **엔진**: Unreal Engine 5.7
- **언어**: C++
- **네트워크**: UE5 Dedicated Server 모델
- **빌드**: MSVC (Visual Studio Build Tools 2022)
- **IDE**: JetBrains Rider
- **버전 관리**: Git

## 프로젝트 구조

```
CLAUDE.md            # Claude Code 작업 가이드
DOCS/
├── DOCS.md          # 기술 로드맵 (채용공고 요건 → 구현 매핑, §6: 문서 구조 규칙)
├── GAME.md          # 게임 디자인 문서
├── BACKLOG.md       # 보류한 설계 결정 (사유 포함)
├── POLISH.md        # 구현 후 발견된 버그/완성도 이슈
├── StudyPath.md     # 개인 학습 기록
├── Mine/            # 프로젝트별 설계/버그 조사 기록 (엔진 공통 개념은 Mine/Concepts/)
├── Blog/            # 단계별 개발기 초안(03/, 04/) + 발행용(Submit/)
└── Notes/           # 단계별 구현 스펙 + 진행 상태
    ├── 01/ 02/ 03/  # Gameplay Framework / Replication / Net Prediction·BoneHitbox
    ├── 04/          # GAS — Status/ Issue/ Polish/ 서브폴더 포함
    └── 05/          # Loot/Inventory (진행 중) — 04와 동일한 서브폴더 구조
```

## 빌드

```bash
# 프로젝트 파일 생성
UnrealBuildTool.exe -projectfiles -project="EmploymentProj.uproject" -game -engine

# Development Editor 빌드
UnrealBuildTool.exe EmploymentProj Win64 Development -project="EmploymentProj.uproject"

# Dedicated Server 빌드
UnrealBuildTool.exe EmploymentProjServer Win64 Development -project="EmploymentProj.uproject"
```
