## 2026-10-24T07:57:41Z
You are the Worker for Milestone 1 (M1: Scrimmage Harness Upgrades).
Your Working Directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/worker_m1/
Authoritative Request: /home/malshare/Evaluations/Core_game/.agents/teamwork/ORIGINAL_REQUEST.md
Master Project Plan: /home/malshare/Evaluations/Core_game/.agents/teamwork/PROJECT.md
Survey Analysis: /home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_harness_survey/handoff.md

Available Skills:
- core-bot-architect: /home/malshare/Evaluations/Core_game/.agents/skills/core-bot-architect/SKILL.md
- core-competitive-bot: /home/malshare/Evaluations/Core_game/.agents/skills/core-competitive-bot/SKILL.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

File Write Ownership:
You EXCLUSIVELY own and modify:
- /home/malshare/Evaluations/Core_game/scripts/scrimmage.py
DO NOT modify any other files in `bots/` or `server/`.

Task Requirements:
Upgrade `scripts/scrimmage.py` to be a robust, transparent, and authoritative tournament scrimmage harness:
1. Fix Bot Stdout/Stderr Redirection:
   - Eliminate `stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL` for our bot (`p_our`).
   - Capture `stdout` and `stderr` using `subprocess.PIPE`.
   - Set `stdin=subprocess.DEVNULL` so `core_static_awaitEnterPress()` never blocks.
2. Clean Process Lifecycle & ASan Detection:
   - When server finishes, allow `p_our` and `p_enemy` up to 2 seconds to cleanly exit via `communicate(timeout=2.0)`.
   - Do NOT immediately `SIGKILL` the bot process, as this terminates the process before AddressSanitizer can write leak summaries.
   - Inspect `stderr` for `LeakSanitizer:` or `AddressSanitizer:` errors.
3. Authoritative Replay Telemetry Parsing:
   - Parse `replays/replay_latest.json` after each match.
   - Extract `misc.team_results` (winner `place == 0`, `death_reason`), `misc.stats`, and `full_tick_amount`.
4. Action Failure Detection:
   - Detect and tally any action failure strings in bot stdout (e.g. `Action Failure:`, `invalid target position`, `on action cooldown`).
   - Parse `ticks` in `replays/replay_latest.json` to detect any unit `debug_info` containing `[begin_errs]` for Team 42.
   - Return failure count and verbatim failure messages.
5. Latency Telemetry:
   - Parse bot stdout for `[LATENCY] avg=... max=...` lines and record average tick latency.
6. Opponent League Suite:
   - Implement CLI `--league --games N --base-port PORT` testing our bot against the 6 opponents in `bots/c/bot/`:
     1. `bots/c/bot/opponent-rush/opponent-rush`
     2. `bots/c/bot/opponent-economy/opponent-economy`
     3. `bots/c/bot/opponent-brawler/opponent-brawler`
     4. `bots/c/bot/opponent-turtle/opponent-turtle`
     5. `bots/c/bot/gridmaster/gridmaster`
     6. `bots/c/bot/strategy-bot/strategy-bot`
   - Use dynamic isolated ports (`base_port + index`) and print an evaluation summary table: Opponent, Games, Wins, Losses, Win Rate, Action Failures, ASan Status, Avg Latency.
7. Verification:
   - Run a single test match (e.g. `python3 scripts/scrimmage.py --games 1 --enemy ./bots/c/bot/opponent-rush/opponent-rush --base-port 4980`) and verify that output, action failures, and replay telemetry are accurately captured and reported.

Output:
Write your complete handoff report to:
/home/malshare/Evaluations/Core_game/.agents/teamwork/worker_m1/handoff.md
Include verification commands and outputs. Send a completion message when finished.
