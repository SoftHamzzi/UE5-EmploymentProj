# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UE5 C++ multiplayer extraction shooter ("EmploymentProj") - a portfolio project demonstrating UE5 gameplay programming, networking/replication, and GAS. All documentation is in Korean.

- **DOCS/DOCS.md**: Technical roadmap - maps job posting requirements to implementation stages and UE5 architecture decisions
- **DOCS/GAME.md**: Game design document - gameplay systems, core loop, and scope definitions
- **DOCS/Mine/**: System design documents (Item.md, Animation.md, MetaHuman.md, CMC.md, Rep.md) - contain architecture decisions and implementation guides
- **DOCS/Notes/**: Per-stage technical study notes and implementation checklists

## Architecture

UE5 dedicated server model. All game logic is server-authoritative.

**Gameplay Framework layout:**
- GameMode (server only): match state machine (Waiting→Playing→Ended), spawn rules, extraction/vending machine judgments
- GameState (replicated): round timer, player count, match state
- PlayerController (owning client): input, UI, server RPC requests
- PlayerState (replicated): kills, extraction status, quest progress
- Character (replicated): movement, animation, weapons, GAS abilities

**Item 3-tier architecture (DOCS/Mine/Item.md):**
- `FEPItemData` (FTableRowBase): DataTable row for balance/operational data (price, stack, slot, rarity)
- `UEPItemDefinition` (UPrimaryDataAsset): static asset references (mesh, icon, FX). Subclassed per type (`UEPWeaponDefinition`)
- `UEPItemInstance` (UObject): runtime state (ammo, durability). Subclassed per type (`UEPWeaponInstance`)
- All three layers linked by `ItemId` (FName)

**Combat flow:**
- `AEPCharacter` → input → `UEPCombatComponent` → `Server_Fire` RPC → server raycast → `ApplyDamage`
- `AEPWeapon` holds `UEPWeaponDefinition` reference (as `WeaponDef`) for weapon stats
- Fire effects via `Multicast` RPCs (Unreliable)

**Movement:**
- Custom CMC (`UEPCharacterMovement`) extends `UCharacterMovementComponent`
- Sprint/ADS via `CompressedFlags` in `FSavedMove` (not Server RPCs) for prediction-accurate sync

**Death:**
- Character self-ragdoll via `Multicast_Die()` — no separate Corpse actor spawning
- Capsule collision disabled, Body mesh set to Ragdoll profile with physics

**Animation (Lyra-style Linked Anim Layer, DOCS/Mine/Animation.md):**
- `UEPAnimInstance`: main AnimBP C++ backend
- `UEPWeaponAnimInstance`: per-weapon AnimBP base
- `ALI_EPWeapon`: Animation Layer Interface with layer functions
- `LinkAnimClassLayers()` swaps weapon animations at runtime via `WeaponDef->WeaponAnimLayer`

**MetaHuman integration (DOCS/Mine/MetaHuman.md):**
- Body = `GetMesh()`, Face/Outfit = additional `USkeletalMeshComponent` with `SetLeaderPoseComponent`

## Project Structure

```
UE5-EmploymentProj/          <- Git root
├── CLAUDE.md
├── DOCS/                    <- Design docs & study notes
│   ├── DOCS.md              <- Technical roadmap
│   ├── GAME.md              <- Game design document
│   ├── Mine/                <- System design docs (Item, Animation, MetaHuman, CMC, Rep)
│   └── Notes/               <- Study notes & implementation checklists per stage
├── .claude/                 <- Claude Code config
└── EmploymentProj/          <- UE5 project root
    ├── Source/EmploymentProj/
    │   ├── Public/           <- Headers by feature (Core/, Combat/, Data/, Movement/, Animation/, Types/)
    │   └── Private/          <- Implementations mirroring Public/ structure
    ├── Content/Data/         <- DataAssets, DataTables (DA_AK74, DT_Items)
    └── Config/
```

## Build Commands

```bash
# Generate VS project files (Windows)
UnrealBuildTool.exe -projectfiles -project="EmploymentProj/EmploymentProj.uproject" -game -engine

# Build (Development Editor)
UnrealBuildTool.exe EmploymentProj Win64 Development -project="EmploymentProj/EmploymentProj.uproject"

# Build (Development Server - for dedicated server)
UnrealBuildTool.exe EmploymentProjServer Win64 Development -project="EmploymentProj/EmploymentProj.uproject"
```

## Design Principles

This is a **job portfolio project**. Code must be structurally complete, not just functional.

- **Public/Private folder separation**: Follow UE5 standard module layout even for a single module
- **Minimal access**: Use protected/private for members not needed externally. Apply least-privilege UPROPERTY specifiers
- **Minimal includes**: Use forward declarations in headers. Only #include in .cpp files
- **Comments explain "why"**: Let code speak for "what". Comments justify design decisions
- **Server authority**: All game logic runs on server. Clients only request. Always check HasAuthority()
- **Lyra patterns**: Follow Lyra project conventions where applicable (Linked Anim Layers, data-driven design)

## Conventions

- All gameplay C++ classes use UE5 reflection macros (UCLASS, UPROPERTY, UFUNCTION)
- Replicated variables use `ReplicatedUsing` with `OnRep_` callbacks where client-side reaction is needed
- Server RPCs prefixed `Server_`, Client RPCs prefixed `Client_`, Multicast RPCs prefixed `Multicast_`
- Weapon data accessed via `WeaponDef->` (type: `UEPWeaponDefinition`)
- Source layout: `Public/` for headers, `Private/` for .cpp, organized by feature (Core/, Combat/, Data/, Movement/, Animation/, Types/)
- Git branching: `main` (always builds), `feature/*` per system
- Platform: Windows (win32)

## Agent Rules

- **No sub-agents**: Do NOT use the Task tool to spawn sub-agents unless the user explicitly permits it
