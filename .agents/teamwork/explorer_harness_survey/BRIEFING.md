# BRIEFING — 2026-10-24T09:53:00Z

## Mission
Investigate the scrimmage runner, evaluation harness, opponent archetypes, and documentation manifests for the 42 CORE championship bot project.

## 🔒 My Identity
- Archetype: explorer
- Roles: Scrimmage & League Harness Survey agent
- Working directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_harness_survey
- Original parent: fc9da3cb-50f8-4a94-8339-0b42020ff84a
- Milestone: Scrimmage & League Harness Survey

## 🔒 Key Constraints
- Read-only investigation — do NOT implement
- Write only to working directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_harness_survey/
- Produce 5-component handoff report: handoff.md

## Current Parent
- Conversation ID: fc9da3cb-50f8-4a94-8339-0b42020ff84a
- Updated: 2026-10-24T09:53:00Z

## Investigation State
- **Explored paths**:
  - `scripts/scrimmage.py`
  - `server/src/` (`main.cpp`, `Server.cpp`, `Game.cpp`, `ReplayEncoder.cpp`, `Logger.cpp`, `Bridge.cpp`, `Stats.cpp`)
  - `bots/c/client_lib/` (`con_lib.c`, `parse_json_state.c`, `core_lib.h`)
  - `bots/c/bot/` (`gridmaster/`, `strategy-bot/`, `opponent-rush/`, `opponent-economy/`, `opponent-brawler/`, `opponent-turtle/`, `my-core-bot/`)
  - `replays/replay_latest.json`
  - `docs/` (`STATUS.md`, `MECHANICS.md`, `EXPERIMENTS.md`, `KNOWN_FAILURES.md`)
  - `archive/` (`EVOLUTION.md`, `models/`)
- **Key findings**:
  - Critical harness defect: `scripts/scrimmage.py` redirects bot stdout/stderr to `DEVNULL`. This masked 7-11 action failures (`move: invalid target position`) per match and suppresses ASan memory leak reports!
  - `replays/replay_latest.json` contains full authoritative game results (`misc.team_results`, `misc.stats`, `full_tick_amount`), and records action failures in object `debug_info` under `[begin_errs]`.
  - Opponent league has 6 opponents ready: `gridmaster`, `strategy-bot`, `opponent-rush`, `opponent-economy`, `opponent-brawler`, `opponent-turtle`.
  - Bot execution latency is currently unmeasured; needs `clock_gettime(CLOCK_MONOTONIC)` in `ft_on_tick()`.
  - Stdin redirection to `DEVNULL` is mandatory to avoid `con_lib.c:core_static_awaitEnterPress()` blocking.
- **Unexplored areas**: None within the survey scope.

## Key Decisions Made
- Fully documented harness lifecycle, opponent league structure, 20+ match test architecture, and docs manifest audit in `handoff.md`.

## Artifact Index
- `DISPATCH.md` — Initial dispatch message
- `BRIEFING.md` — Working state and memory
- `progress.md` — Liveness heartbeat
- `handoff.md` — Complete 5-component survey report
