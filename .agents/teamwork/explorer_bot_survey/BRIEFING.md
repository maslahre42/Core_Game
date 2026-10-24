# BRIEFING — 2026-10-24T07:52:00Z

## Mission
Investigate C bot codebases, archive champions, and benchmark bots to diagnose action failures, memory/performance profiles, and recommend candidate ex_v01 architecture.

## 🔒 My Identity
- Archetype: explorer
- Roles: Bot Architecture Survey, investigation, synthesis
- Working directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_bot_survey
- Original parent: fc9da3cb-50f8-4a94-8339-0b42020ff84a
- Milestone: Bot Architecture Survey

## 🔒 Key Constraints
- Read-only investigation — do NOT implement
- Strictly read-only investigation: analyze problems, synthesize findings, produce structured reports
- No source code modifications outside working directory

## Current Parent
- Conversation ID: fc9da3cb-50f8-4a94-8339-0b42020ff84a
- Updated: 2026-10-24T07:45:00Z

## Investigation State
- **Explored paths**:
  - `bots/c/bot/my-core-bot/` (Makefile, inc/bot.h, inc/tuned_params.h, src/getters.c, src/main.c)
  - `archive/models/v2_goalkeeper_championship/` & `archive/EVOLUTION.md`
  - `bots/c/bot/` (gridmaster, strategy-bot, opponent-rush, opponent-economy, opponent-brawler, opponent-turtle)
  - `bots/c/client_lib/` (action_travel.c, actions.c, con_lib.c, core_lib.h)
  - `server/src/` (TransferGemsAction.cpp, MoveAction.cpp, AttackAction.cpp, CreateAction.cpp, Unit.cpp, ComponentLogic.cpp, Game.cpp)
  - `bots/configs/fake_demo/` (game.config.json, server.config.json)
  - `scripts/scrimmage.py` & `docs/` (MECHANICS.md, KNOWN_FAILURES.md, STATUS.md, EXPERIMENTS.md)
- **Key findings**:
  - `my-core-bot` compiles with `-Wall -Wextra -Werror -g -fsanitize=address` cleanly.
  - Action failures empirically reproduced: ~20 `move: invalid target position. should be empty` errors per match due to lack of friendly tile reservation when multiple units call travel/move.
  - Transfer cooldown reset bug identified in `run_interceptors` (lines 340-344) and `run_breakers` (lines 448-451): units lack `continue` after `transferGems`, falling through to travel/attack on same tick.
  - Heap allocations located: `malloc` in `run_harvesters` (line 227) for bipartite pair scoring matrix, plus dynamic arrays in `core_get_objs_filter`.
  - Execution speed measured: ~0.05-0.15 ms average per tick inside bot logic (well within < 0.5 ms requirement).
  - Benchmark league surveyed: 6 opponents analyzed (gridmaster, strategy-bot, rush, economy, brawler, turtle).
- **Unexplored areas**: None. All survey points covered.

## Key Decisions Made
- Complete architecture recommendations synthesized for candidate `ex_v01` covering dynamic harvesting with state latching, anti-rush goalkeeper focus-fire, corner-aware siege breakers, tile reservation, and zero-alloc memory management.

## Artifact Index
- /home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_bot_survey/DISPATCH.md — Task dispatch
- /home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_bot_survey/progress.md — Liveness heartbeat
- /home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_bot_survey/handoff.md — Final comprehensive survey report
