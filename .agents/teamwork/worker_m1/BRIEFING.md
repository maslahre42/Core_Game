# BRIEFING — 2026-10-24T08:08:30Z

## Mission
Upgrade scripts/scrimmage.py to a robust, transparent, and authoritative tournament scrimmage harness supporting replay telemetry, action failure detection, ASan verification, latency parsing, and opponent league execution.

## 🔒 My Identity
- Archetype: worker
- Roles: implementer, qa, specialist
- Working directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/worker_m1
- Original parent: fc9da3cb-50f8-4a94-8339-0b42020ff84a
- Milestone: M1 (Scrimmage Harness Upgrades)

## 🔒 Key Constraints
- EXCLUSIVELY own and modify: /home/malshare/Evaluations/Core_game/scripts/scrimmage.py
- DO NOT modify any other files in `bots/` or `server/`.
- NO CHEATING / FAUX IMPLEMENTATIONS. Real logic only.
- Output handoff report to .agents/teamwork/worker_m1/handoff.md.

## Current Parent
- Conversation ID: fc9da3cb-50f8-4a94-8339-0b42020ff84a
- Updated: not yet

## Task Summary
- **What to build**: Robust scrimmage harness in `scripts/scrimmage.py` with stdout/stderr capture, clean process lifecycle, ASan detection, authoritative replay parsing, action failure tallying, latency parsing, and full 6-opponent league runner.
- **Success criteria**:
  1. Capture stdout/stderr via PIPE, stdin DEVNULL. (COMPLETED)
  2. communicate(timeout=2.0) on bot procs, check for ASan leak reports. (COMPLETED)
  3. Authoritative replay parsing (replays/replay_latest.json). (COMPLETED)
  4. Action failure detection from stdout and replay debug_info. (COMPLETED)
  5. Latency telemetry parsing from bot stdout. (COMPLETED)
  6. Opponent league CLI (`--league --games N --base-port PORT`) with summary table. (COMPLETED)
  7. Verified single match execution. (COMPLETED)
- **Interface contracts**: /home/malshare/Evaluations/Core_game/.agents/teamwork/PROJECT.md § Harness ↔ Bot Telemetry Contract
- **Code layout**: /home/malshare/Evaluations/Core_game/.agents/teamwork/PROJECT.md § Code Layout

## Key Decisions Made
- Structured `scripts/scrimmage.py` modularly with `parse_replay()`, `parse_bot_stdout()`, `check_asan()`, `run_single_match()`, `run_series()`, `run_league()`.
- Unified stdout and replay action failure extraction using regex and tick unit-mapping with deduplication to ensure zero missed failures without double-counting.
- Used dynamic isolated ports `base_port + index` preventing socket collisions.
- Defaulted timeout to 60s to accommodate long matches such as opponent-brawler (900+ ticks).

## Artifact Index
- /home/malshare/Evaluations/Core_game/scripts/scrimmage.py — The tournament scrimmage and league harness script.
- /home/malshare/Evaluations/Core_game/.agents/teamwork/worker_m1/handoff.md — Final handoff report.

## Change Tracker
- **Files modified**: `scripts/scrimmage.py` (complete rewrite into authoritative harness).
- **Build status**: `py_compile scripts/scrimmage.py` PASSED (code 0).
- **Pending issues**: None.

## Quality Status
- **Build/test result**: Single match vs opponent-rush and 6-match league passed.
- **Lint status**: Clean.
- **Tests added/modified**: Verified latency regex, ASan leak detection, single match execution, and multi-opponent league table.

## Loaded Skills
- **Source**: /home/malshare/Evaluations/Core_game/.agents/skills/core-bot-architect/SKILL.md
  - **Local copy**: /home/malshare/Evaluations/Core_game/.agents/teamwork/worker_m1/skills/core-bot-architect/SKILL.md
  - **Core methodology**: Designing, building, benchmarking, and optimizing C bot for 42 CORE with zero action errors and strict resource constraints.
- **Source**: /home/malshare/Evaluations/Core_game/.agents/skills/core-competitive-bot/SKILL.md
  - **Local copy**: /home/malshare/Evaluations/Core_game/.agents/teamwork/worker_m1/skills/core-competitive-bot/SKILL.md
  - **Core methodology**: Competitive bot engineering, match automation, opponent league, evidence-based verification, and iterative improvement.
