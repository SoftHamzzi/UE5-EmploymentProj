# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

UE5 C++ multiplayer extraction shooter ("EmploymentProj") - a portfolio project demonstrating UE5 gameplay programming, networking/replication, and GAS. All documentation is in Korean.

- **DOCS/DOCS.md**: Technical roadmap - maps job posting requirements to implementation stages and UE5 architecture decisions
- **DOCS/GAME.md**: Game design document - gameplay systems, core loop, and scope definitions
- **DOCS/Notes/**: Per-stage technical study notes (01_GameplayFramework, 02_Replication, 03_NetPrediction, 04_GAS) - contain UE5 API details, code patterns, and implementation checklists

## Architecture (Target)

UE5 dedicated server model. All game logic is server-authoritative.

**Gameplay Framework layout:**
- GameMode (server only): match state machine (Waiting→Playing→Ended), spawn rules, extraction/vending machine judgments
- GameState (replicated): round timer, player count, match state
- PlayerController (owning client): input, UI, server RPC requests
- PlayerState (replicated): money, kills, extraction status, quest progress
- Character (replicated): movement, animation, weapons, GAS abilities

**Key technical systems:**
- Replication: UPROPERTY replication, OnRep callbacks, Server/Client/NetMulticast RPCs
- Combat: server-authoritative hitscan with lag compensation (hitbox history ring buffer + server rewind)
- GAS: AttributeSet (HP/Stamina/Shield), GameplayEffects, 3 GameplayAbilities (Dash/Heal/Shield)
- Vending machine: server-authoritative gacha with Multicast RPC for sound, DataAsset-based loot tables
- AI: Behavior Tree based simple enemies (patrol→detect→chase→shoot)
- Data-driven: UPrimaryDataAsset for weapons, items, vending machine tables

## Build Commands

UE5 project (once .uproject exists):

```bash
# Generate VS project files (Windows)
UnrealBuildTool.exe -projectfiles -project="EmploymentProj.uproject" -game -engine

# Build (Development Editor)
UnrealBuildTool.exe EmploymentProj Win64 Development -project="EmploymentProj.uproject"

# Build (Development Server - for dedicated server)
UnrealBuildTool.exe EmploymentProjServer Win64 Development -project="EmploymentProj.uproject"
```

## Conventions

- All gameplay C++ classes use UE5 reflection macros (UCLASS, UPROPERTY, UFUNCTION)
- Replicated variables use `ReplicatedUsing` with `OnRep_` callbacks where client-side reaction is needed
- Server RPCs are prefixed `Server_` (e.g., `Server_Fire`, `Server_UseVendingMachine`)
- Git branching: `main` (always builds), `feature/*` per system (e.g., `feature/net-fire`, `feature/gas`)
- Platform: Windows (win32)
