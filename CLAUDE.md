# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** 속도보다 정확성에 무게를 둔다. 사소한 작업에는 판단력을 쓴다.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- 더 단순한 방법이나 더 확장 가능한 방법이 있으면 말한다. 근거가 있으면 반박한다.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Extensibility First — 단, 확장점은 문서에 이름이 있어야 한다

**확장성과 구성가능성을 추구한다.** 이 프로젝트는 단계별로 계속 자라며(`DOCS/Notes/05/05_Loot_DOCS.md` §7 등), 나중에 붙일 것이 문서에 이미 적혀 있다.

**만든다 — 확장점이 계획서에 이름으로 있을 때**
- 스폰할 액터 클래스, 데이터 테이블, 에셋 참조는 **설정·DataAsset·`TSubclassOf`로 뺀다.** 하드코딩하지 않는다
- 한 값을 두 경로가 봐야 하면 **둘 다 볼 수 있는 곳**에 둔다 (호출자 단위 필드로 두면 갈린다)
- 지금 소비자가 하나여도, 문서에 두 번째 소비자가 예고돼 있으면 그 자리를 만든다
- 나중에 넣기 **비싼 것**은 지금 넣는다 — 식별자 안정성, 복제 조건, 계약(반환 규약·순서)

**만들지 않는다 — 상상한 확장점**
- 문서에도 기획에도 없는 "혹시 나중에"
- 두 번째 구현자가 없는 인터페이스·베이스 클래스
- 도달 불가한 분기의 에러 처리

**판단 기준:** *"이 확장점이 `DOCS/` 어딘가에 이름으로 적혀 있는가?"* 적혀 있으면 만든다. 없으면 그 문서를 먼저 고친다.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- **요청과 무관한** 코드는 리팩터링하지 않는다. 요청이 구조 변경을 필요로 하면 그건 리팩터링이 아니라 **작업 범위다**
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: 바뀐 줄은 **요청** 또는 **§2가 승인한 확장점**으로 추적된다.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**이 지침이 작동하고 있다면:** 하드코딩한 값을 나중에 빼내는 재작업이 줄고, 계획에 없는 계층이 늘지 않고, 확인 질문이 실수 뒤가 아니라 구현 전에 나온다.

---

## Project Overview

UE5 C++ multiplayer extraction shooter ("EmploymentProj") - portfolio project. All documentation is in Korean.

- **DOCS/DOCS.md**: Technical roadmap
- **DOCS/GAME.md**: Game design document
- **DOCS/Mine/**: System design docs (Item, Animation, MetaHuman, CMC, Rep, Proj)
- **DOCS/Notes/**: Per-stage study notes and implementation checklists (03_BoneHitbox, 04_GAS etc.)

## Session Start

`.claude/PROJECT_CONTEXT.md`를 소스 파일 탐색 전에 먼저 읽는다 — 클래스/함수 구조를 파일당 읽지 않고 한 번에 파악할 수 있다. `.cpp`/`.h`가 바뀐 커밋에서는 pre-commit 훅이 자동 갱신하므로 보통은 재생성할 필요 없음. 훅 없이 수동 갱신하려면:
```bash
python .claude/scripts/code_mapper.py . -o .claude/PROJECT_CONTEXT.md
```

## Architecture

UE5 dedicated server model. All game logic is server-authoritative.

**Item 3-tier:** `FEPItemData` (DataTable) → `UEPItemDefinition` (DataAsset, subclassed as `UEPWeaponDefinition`) → `UEPItemInstance` (runtime state). Linked by `ItemId` (FName).

**Combat flow:** Input → `UEPCombatComponent` → `HandleServerFire` → SSR `ConfirmHitscan` → GE damage apply. Fire effects via Multicast RPCs (Unreliable).

**Lag Compensation:** `UEPServerSideRewindComponent` (server-only). Snapshot on `TG_PostPhysics` after `CMC::OnMovementUpdated`. Timestamps use `GS->GetServerWorldTimeSeconds()` on both sides.

**Animation:** Lyra-style Linked Anim Layer. `LinkAnimClassLayers()` swaps weapon anim at runtime via `WeaponDef->WeaponAnimLayer`.

## GAS Migration State

**완료 (2026-07-26).** 전체 이력과 레거시 제거 확인(grep 검증)은
`DOCS/Notes/04/Status/GAS_STATUS.md`에 있다. 마스터 스펙은 `DOCS/Notes/04/04_GAS_DOCS.md`.
이후 작업은 GAS 밖 — `DOCS/DOCS.md` §5 실행 순서 참조, 현재 `feature-loot`
브랜치에서 Loot/Inventory 진행 중.

NativeGameplayTags: `Public/GAS/EPNativeGameplayTags.h` (`namespace EmpGameplayTags`).

## Project Structure

```
UE5-EmploymentProj/
├── CLAUDE.md
├── DOCS/
└── EmploymentProj/
    ├── Source/EmploymentProj/
    │   ├── Public/    <- Headers by feature (Core/, Combat/, Data/, Movement/, Animation/, GAS/)
    │   └── Private/   <- Implementations mirroring Public/
    └── Content/Data/  <- DataAssets, DataTables
```

## Build Commands

```bash
# Generate VS project files
UnrealBuildTool.exe -projectfiles -project="EmploymentProj/EmploymentProj.uproject" -game -engine

# Build (Development Editor)
UnrealBuildTool.exe EmploymentProj Win64 Development -project="EmploymentProj/EmploymentProj.uproject"
```

## Conventions

- UE5 reflection macros on all gameplay classes (UCLASS, UPROPERTY, UFUNCTION)
- Replicated vars use `ReplicatedUsing` + `OnRep_` when client reaction needed
- RPC prefixes: `Server_`, `Client_`, `Multicast_`
- `UActorComponent` subclasses: use `GetOwner()->HasAuthority()`, not `HasAuthority()`
- Forward declarations in headers; `#include` only in .cpp
- Weapon data via `WeaponDef->` (type: `UEPWeaponDefinition`)
- Platform: Windows (win32)

## Workflow

- **코드는 사용자가 직접 작성한다.** Claude는 코드 파일을 직접 수정하지 않는다.
- **Claude는 구현 방법을 문서에 기술한다.** 구현 지침은 `DOCS/Notes/` 하위 해당 단계 문서에 작성한다.
- 코드 검토, 오류 지적, 설계 설명은 허용. 파일 Edit/Write는 문서에만 사용한다.
- **STATUS 파일이 진행 상태의 진실의 원천이다.** GAS/Loot/Polish 등 이 프로젝트의
  모든 단계별 작업 영역이 이 방식을 쓴다 — `GAS_STATUS.md`, `LOOT_STATUS.md`,
  `04_Polish_STATUS.md` 등. 해당 영역 작업 시작 시 그 영역 STATUS 파일 + 세부
  단계 STATUS 파일을 먼저 확인한다. 단계 문서(`_XXX.md`)는 예정 코드를 보여줄
  뿐 실제 구현 여부를 보장하지 않는다 — 항상 STATUS 파일로 확인할 것.
- 코드 수정 후 사용자가 요청하면 STATUS 파일을 코드 기준으로 갱신한다. 세부 사항은 `SESSION.md` 참고.
- **Notes 문서를 쓰거나 크게 고칠 때마다 `notes-review` 스킬을 쓴다** — 실무성·확장성·간결성 자체 점검 + 외부 리뷰 필요 여부 판단. 요청받지 않아도 기본으로 한다.
- **외부 리뷰(`Review/*_Answer.md`) 답변이 있으면 그대로 반영하지 않고 `review-verifier` 서브에이전트에 위임해 검증한다.** CONFIRMED된 주장만 설계·STATUS 문서에 반영한다.

## Agent Rules

- **No sub-agents by default**: Do NOT use the Task tool to spawn sub-agents unless the user explicitly permits it — **except** `review-verifier`(리뷰 답변 검증, 위 Workflow 규칙대로 항상 허용)와 `unreal-engine-researcher`(자기완결적인 UE5 기능 조사, 대화 맥락이 필요 없을 때 허용). 이 둘은 매번 물어보지 않고 쓴다.
