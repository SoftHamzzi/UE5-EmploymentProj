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
