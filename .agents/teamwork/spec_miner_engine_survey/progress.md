# Progress — Spec Miner Engine Survey

Last visited: 2026-10-24T08:15:00Z

## Status: COMPLETE

### Completed Steps
- [x] Initialized DISPATCH.md, BRIEFING.md, and local skill mirrors.
- [x] Read ORIGINAL_REQUEST.md.
- [x] Located and examined all server source code (`server/src/`, `server/inc/`, `server/data/json-schemas/`).
- [x] Examined client library implementation (`bots/c/client_lib/`).
- [x] Examined wiki documentation and official mechanics specs (`wiki/`).
- [x] Verified Action Readiness & Cooldown mechanics (`Unit.cpp:19-36`, `action_cooldown_ <= 0` handling, negative idle decrement, reset to positive `unitStandingCooldownLimit` past threshold).
- [x] Verified Transfer Reset & same-tick move/transfer restrictions (`TransferGemsAction.cpp:128`, `Utils.cpp:14-27` 4-phase ordering, `MoveAction.cpp:31`).
- [x] Verified Unit Design Validation rules, AST expression evaluator, Rule 1 (`baseActionCooldown < attack sum`), speed+attack constraints, 2x speed illegality, Rule 2 (`armor <= 4`), allowed component ranges (`CreateAction.cpp:80-124`, `ComponentLogic.cpp:1-122`, `game.config.json`).
- [x] Verified Economy Prices: currency strictly gems (no minerals/energy), dynamic unit creation cost formula ($20 + \sum \text{cost}$), passive core income (+3 gems/tick for first 100 ticks), gem pile (35 gems), deposit (50 HP, 100 gems, requires mining), gem carrying penalty integer division formula.
- [x] Verified Movement, Collision, Combat & Tick Resolution Order (8-step tick loop, 4 action phases, intra-phase mt19937_64 shuffle, dead unit delayed cleanup in Step 3 preventing same-tick movement onto killed units, armor formula, core destruction).
- [x] Verified Server Error & Action Failure Signals (exact strings for every failure branch in Move, Attack, TransferGems, Create, parsing errors, debug errors).
- [x] Compiled comprehensive 5-component handoff report in `handoff.md` with Features Discovered and Edge Cases tables.
- [x] Updated BRIEFING.md with final state.
- [x] Prepared completion message for orchestrator (`fc9da3cb-50f8-4a94-8339-0b42020ff84a`).
