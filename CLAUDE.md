# CLAUDE.md

UE5 C++ 멀티플레이 익스트랙션 슈터 "EmploymentProj" — 포트폴리오 프로젝트. 문서는 전부 한국어로 쓰고, 응답도 한국어로 한다.

## 1. 작업 전에 생각한다

추측하지 않는다. 헷갈리는 것을 숨기지 않는다. 트레이드오프를 드러낸다.

- 가정을 명시한다. 확실하지 않으면 묻는다.
- 해석이 여럿이면 골라 버리지 말고 제시한다.
- 더 단순한 방법이나 더 확장 가능한 방법이 있으면 말한다. 근거가 있으면 반박한다.
- 불분명하면 멈추고, 무엇이 불분명한지 말하고 묻는다.
- 요청에 대상 · 성공 조건 · 제약 중 하나라도 빠져 있으면 바로 작업하지 않고 빠진 것을 되묻는다.

## 2. 확장성 우선 — 단, 확장점은 문서에 이름이 있거나 사용자가 명시해야 한다

확장성과 구성가능성을 추구한다. 이 프로젝트는 단계별로 계속 자라며(`DOCS/Notes/05/05_Loot_Design.md` §7 등), 나중에 붙일 것이 문서에 이미 적혀 있다.

만든다 — 확장점이 계획서에 이름으로 있거나, 사용자가 그 확장을 명시적으로 요청했을 때
- 스폰할 액터 클래스, 데이터 테이블, 에셋 참조는 설정·DataAsset·`TSubclassOf`로 뺀다. 하드코딩하지 않는다
- 한 값을 두 경로가 봐야 하면 둘 다 볼 수 있는 곳에 둔다 (호출자 단위 필드로 두면 갈린다)
- 지금 소비자가 하나여도, 문서에 두 번째 소비자가 예고돼 있으면 그 자리를 만든다
- 나중에 넣기 비싼 것은 지금 넣는다 — 식별자 안정성, 복제 조건, 계약(반환 규약·순서)

만들지 않는다 — 상상한 확장점
- 문서에도 기획에도 없는 "혹시 나중에"
- 두 번째 구현자가 없는 인터페이스·베이스 클래스
- 도달 불가한 분기의 에러 처리

판단 기준: *"이 확장점이 `DOCS/` 어딘가에 이름으로 적혀 있는가, 아니면 사용자가 지금 명시했는가?"* 둘 중 하나면 만든다. 둘 다 아니면 그 문서를 먼저 고친다.

## 3. 필요한 곳만 고친다

- 요청받은 문서·절만 고친다. 다른 문서의 옛 내용은 고치지 말고 지적만 한다.
- 내가 만든 고아(깨진 참조, 안 쓰게 된 절)만 정리한다.
- 기존 문체와 형식을 따른다.
- 구현서에 적는 코드도 같다 — 요청과 무관한 코드는 리팩터링하지 않는다. 요청이 구조 변경을 필요로 하면 그건 리팩터링이 아니라 작업 범위다.

## 4. 검증할 수 있게 쓴다

- 구현서·가이드마다 검증 방법을 적는다: PIE 확인 표(조작 → 기대 결과), 필요한 임시 로그, 기대 결과.
- 성공 조건이 약한 요청("되게 해줘")은 되묻는다.
- 여러 단계 작업은 단계마다 확인 방법을 붙인 짧은 계획을 먼저 말한다.

## 작업 방식

- **코드는 사용자가 직접 작성한다. Claude는 코드 파일을 직접 수정하지 않는다.** 코드 검토, 오류 지적, 설계 설명은 한다. Edit/Write는 문서에만 쓴다.
- **STATUS 파일이 진행 상태의 진실의 원천이다.** 단계 문서는 예정 코드를 보여줄 뿐 구현 여부를 보장하지 않는다. 진행 상황은 영역 STATUS를 본다. 운영 절차는 `SESSION.md`.
- 세션은 역할별로 나눈다: `/session-start <설계|검토|개발|리뷰|질문> <영역> [대상]`으로 시작하고, 끝낼 때는 compact 대신 `/handoff`.
- 문서 위치·이름·가이드 형식은 `DOCS/ROADMAP.md` §6.
- 외부 리뷰 답변(`Review/*_Answer.md`, Codex 답 포함)은 그대로 반영하지 않고 `review-verifier`로 검증한 뒤 CONFIRMED된 주장만 반영한다.
- Codex 사용처: 검토 세션은 `/codex:adversarial-review`(설계 문서 반박), 리뷰 세션은 `/codex:review`(사용자 코드 diff).

## 세션 시작

`.claude/PROJECT_CONTEXT.md`를 소스 파일 탐색 전에 먼저 읽는다 — 클래스/함수 구조를 파일당 읽지 않고 한 번에 파악할 수 있다. `.cpp`/`.h`가 바뀐 커밋에서는 pre-commit 훅이 자동 갱신한다. 수동 갱신:
```bash
python .claude/scripts/code_mapper.py . -o .claude/PROJECT_CONTEXT.md
```

## Architecture

UE5 dedicated server model. All game logic is server-authoritative.

**Item 3-tier:** `FEPItemData` (DataTable) → `UEPItemDefinition` (DataAsset, subclassed as `UEPWeaponDefinition`) → `FEPItemState` (runtime state, value type, `Types/EPTypes.h`). Linked by `ItemId` (FName).

**Combat flow:** Input → `UEPCombatComponent` → `HandleServerFire` → SSR `ConfirmHitscan` → GE damage apply. Fire effects via Multicast RPCs (Unreliable).

**Lag Compensation:** `UEPServerSideRewindComponent` (server-only). Snapshot on `TG_PostPhysics` after `CMC::OnMovementUpdated`. Timestamps use `GS->GetServerWorldTimeSeconds()` on both sides.

**Animation:** Lyra-style Linked Anim Layer. `LinkAnimClassLayers()` swaps weapon anim at runtime via `WeaponDef->WeaponAnimLayer`.

## Build Commands

```bash
# Generate VS project files
UnrealBuildTool.exe -projectfiles -project="EmploymentProj/EmploymentProj.uproject" -game -engine

# Build (Development Editor)
UnrealBuildTool.exe EmploymentProj Win64 Development -project="EmploymentProj/EmploymentProj.uproject"
```

## 하위 에이전트

- 기본적으로 하위 에이전트를 쓰지 않는다. 사용자가 허락할 때만 쓴다.
- 예외 둘은 묻지 않고 쓴다: `review-verifier`(리뷰 답변 검증), `unreal-engine-researcher`(대화 맥락이 필요 없는 자기완결적 UE5 기능 조사).

C++ 관례는 `.claude/rules/ue-cpp.md`, Notes 문서 규칙은 `.claude/rules/notes-docs.md` — 해당 파일을 읽을 때 로드된다.
