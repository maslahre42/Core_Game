## 2026-10-24T07:44:25Z
From: fc9da3cb-50f8-4a94-8339-0b42020ff84a
Priority: MESSAGE_PRIORITY_HIGH

You are the Bot Architecture Survey agent.
Your Working Directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_bot_survey/
Authoritative Request: /home/malshare/Evaluations/Core_game/.agents/teamwork/ORIGINAL_REQUEST.md

Available Skills:
- core-bot-architect: /home/malshare/Evaluations/Core_game/.agents/skills/core-bot-architect/SKILL.md
- core-competitive-bot: /home/malshare/Evaluations/Core_game/.agents/skills/core-competitive-bot/SKILL.md
Consult these skills and the codebase.

Mission:
Investigate the existing C bot codebases, archive, and benchmark bots:
1. Inspect `bots/c/bot/my-core-bot/`:
   - Architecture, modules, header definitions, and `Makefile` (check -Wall -Wextra -Werror, ASan support).
   - Trace the exact causes of prototype action failures (action readiness, transfer cooldown reset, collision on occupied tiles, invalid create commands).
   - Memory management: locate any heap allocations (`malloc`, `calloc`, `strdup`) and determine how to enforce 0 heap allocations in per-tick tracking loops using static arrays / ring buffers.
   - Measure/analyze execution speed per tick (must be strictly < 0.5 ms).
2. Inspect `archive/models/v2_goalkeeper_championship/`:
   - Analyze previous champion design, unit blueprints, strategy, and reason for success.
   - Note key lessons to incorporate into candidate ex_v01.
3. Inspect benchmark bots (`gridmaster`, `strategy-bot` and any others):
   - Locations, compilation mechanisms, unit compositions, and play styles.
4. Recommend concrete architecture for candidate ex_v01:
   - Dynamic resource harvesting with persistent state latching (no oscillation).
   - Early defense against rushes with coordinated focus fire.
   - Adaptive siege breakers to finish damaged opponent cores.
   - Static memory management with 0 heap allocations in per-tick loops.

Output:
Write your complete report with verified code paths and recommendations to:
/home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_bot_survey/handoff.md
Send a completion message back when finished.
