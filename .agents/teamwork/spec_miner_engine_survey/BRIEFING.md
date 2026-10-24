# BRIEFING — 2026-10-24T08:15:00Z

## Mission
Investigate the 42 CORE game engine source code and protocol definitions across the repository. Extract exact server engine mechanics, formulas, and rules with source file path and line number citations for docs/MECHANICS.md.

## 🔒 My Identity
- Archetype: Specification Miner
- Roles: Spec Miner Engine Survey
- Working directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/spec_miner_engine_survey/
- Original parent: fc9da3cb-50f8-4a94-8339-0b42020ff84a
- Milestone: Engine Survey & Mechanics Extraction (Complete)

## 🔒 Key Constraints
- Investigate server engine mechanics, formulas, and rules with exact file paths and line number citations.
- Probe 6 core areas: Action Readiness, Transfer Reset, Unit Design Validation, Accurate Economy Prices, Movement/Collision/Combat/Tick Resolution Order, Server Error & Action Failure Signals.
- Document Discovered Features and Edge Cases tables.
- Do NOT implement bot or engine code — read-only extraction and documentation.
- Deliver self-contained handoff.md with 5 components (Observation, Logic Chain, Caveats, Conclusion, Verification Method).

## Current Parent
- Conversation ID: fc9da3cb-50f8-4a94-8339-0b42020ff84a
- Updated: 2026-10-24T08:15:00Z

## Task Summary
- **What to build**: Comprehensive mechanics extraction report in handoff.md for docs/MECHANICS.md.
- **Success criteria**: Exact server formulas, constants, order of operations, validation rules, error strings, with source file line numbers verified by inspection and runnable probe where possible.
- **Interface contracts**: ORIGINAL_REQUEST.md, server source code (`server/src/`, `server/inc/`), JSON schemas (`server/data/json-schemas/`), client library headers (`bots/c/client_lib/`).
- **Code layout**: Documentation report in `/home/malshare/Evaluations/Core_game/.agents/teamwork/spec_miner_engine_survey/handoff.md`.

## Loaded Skills
- **Source**: /home/malshare/Evaluations/Core_game/.agents/skills/core-bot-architect/SKILL.md
  - **Local copy**: /home/malshare/Evaluations/Core_game/.agents/teamwork/spec_miner_engine_survey/skills/core-bot-architect/SKILL.md
  - **Core methodology**: Tactical architecture, action phase exploitation, unit design formulas, C memory safety rules.
- **Source**: /home/malshare/Evaluations/Core_game/.agents/skills/core-competitive-bot/SKILL.md
  - **Local copy**: /home/malshare/Evaluations/Core_game/.agents/teamwork/spec_miner_engine_survey/skills/core-competitive-bot/SKILL.md
  - **Core methodology**: Evidence hierarchy, repository audit, revision-specific pitfalls, action safety, match orchestration.

## Key Decisions Made
- Discovered that server executes actions strictly in 4 phases (0: transfer_gems, 1: attack, 2: move, 3: create); `build` does not exist in the engine.
- Discovered standing cooldown reset mechanism: when idle beyond `unitStandingCooldownLimit` (50 ticks), cooldown resets to positive 50 penalty.
- Discovered AST invalid condition evaluator for unit design: speed + attack and 2x speed are mathematically rejected.
- Discovered single-currency economy: only gems exist (no energy/minerals).
- Discovered delayed dead object removal in Step 3 of gameloop, preventing movement onto tiles of units/walls killed on same tick.
- Discovered team corner assignment uses `std::random_device` non-deterministically, overriding map seed determinism.

## Artifact Index
- DISPATCH.md — Initial dispatch instructions
- BRIEFING.md — Persistent context & state
- progress.md — Liveness heartbeat & task progress
- handoff.md — Complete 5-component report with Features Discovered and Edge Cases tables
