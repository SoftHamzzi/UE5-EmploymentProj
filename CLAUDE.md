# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

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

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.

---

## Project Overview

UE5 C++ multiplayer extraction shooter ("EmploymentProj") - portfolio project. All documentation is in Korean.

- **DOCS/DOCS.md**: Technical roadmap
- **DOCS/GAME.md**: Game design document
- **DOCS/Mine/**: System design docs (Item, Animation, MetaHuman, CMC, Rep, Proj)
- **DOCS/Notes/**: Per-stage study notes and implementation checklists (03_BoneHitbox, 04_GAS etc.)

## Architecture

UE5 dedicated server model. All game logic is server-authoritative.

**Item 3-tier:** `FEPItemData` (DataTable) → `UEPItemDefinition` (DataAsset, subclassed as `UEPWeaponDefinition`) → `UEPItemInstance` (runtime state). Linked by `ItemId` (FName).

**Combat flow:** Input → `UEPCombatComponent` → `HandleServerFire` → SSR `ConfirmHitscan` → GE damage apply. Fire effects via Multicast RPCs (Unreliable).

**Lag Compensation:** `UEPServerSideRewindComponent` (server-only). Snapshot on `TG_PostPhysics` after `CMC::OnMovementUpdated`. Timestamps use `GS->GetServerWorldTimeSeconds()` on both sides.

**Animation:** Lyra-style Linked Anim Layer. `LinkAnimClassLayers()` swaps weapon anim at runtime via `WeaponDef->WeaponAnimLayer`.

## GAS Migration State (feature-gas branch)

Master spec: `DOCS/Notes/04/04_GAS_DOCS.md`. Implementation order:

1. Foundation (ASC + AttributeSet) — `04_GAS_01_Foundation.md`
2. Damage/HP pipeline — `04_GAS_02_DamagePipeline.md`
3. `GA_Item_PrimaryUse` (replaces `Server_Fire`) — `04_GAS_03_PrimaryUse.md`
4. `GA_Item_Reload` (replaces `Server_Reload`) — `04_GAS_04_Reload.md`
5. Spread CDF table — `04_GAS_05_Spread.md`
6. Hit zone damage tag system — `04_GAS_06_HitZoneDamage.md`

**진행 상태의 진실의 원천은 STATUS 파일이다, 단계 문서가 아니다.**
- `DOCS/Notes/04/GAS_STATUS.md` — 전체 단계 진행 상황
- `DOCS/Notes/04/04_GAS_0X_XXX_STATUS.md` — 단계별 상세 (Step 완료 여부, 버그, 미완료 항목)
- 단계 문서(`04_GAS_0X_XXX.md`)는 예정 코드를 보여줄 뿐 실제 구현 여부를 보장하지 않음 — 항상 STATUS 파일로 확인할 것

NativeGameplayTags: `Public/GAS/EPNativeGameplayTags.h` (`namespace EmpGameplayTags`).

Key pending removals: `Server_Fire`/`Server_Reload` RPCs, `AEPCharacter::HP`/`TakeDamage()`, `AEPWeapon::CurrentAmmo`/`StartReload`/`FinishReload`, `BoneDamageMultiplierMap`.

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
- GAS 관련 작업 시작 시 `GAS_STATUS.md` + 해당 단계 STATUS 파일을 먼저 확인한다.
- 코드 수정 후 사용자가 요청하면 STATUS 파일을 코드 기준으로 갱신한다. 세부 사항은 `SESSION.md` 참고.

## Agent Rules

- **No sub-agents**: Do NOT use the Task tool to spawn sub-agents unless the user explicitly permits it
