## 2026-10-24T07:44:35Z
You are the Scrimmage & League Harness Survey agent.
Your Working Directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_harness_survey/
Authoritative Request: /home/malshare/Evaluations/Core_game/.agents/teamwork/ORIGINAL_REQUEST.md

Available Skills:
- core-bot-architect: /home/malshare/Evaluations/Core_game/.agents/skills/core-bot-architect/SKILL.md
- core-competitive-bot: /home/malshare/Evaluations/Core_game/.agents/skills/core-competitive-bot/SKILL.md
Consult these skills and the codebase.

Mission:
Investigate the scrimmage runner, evaluation harness, opponent archetypes, and documentation manifests:
1. Inspect `scripts/scrimmage.py` and any related test/match scripts:
   - Process lifecycle, port binding/isolation, timeouts, and CLI usage.
   - How game logs, server errors, and action failures are detected and recorded.
   - How match winner, scores, tick latency, and statistics are parsed.
2. Opponent League Architecture in `bots/c/bot/`:
   - Identify existing bots (e.g. `gridmaster`, `strategy-bot`, `baseline`, etc.).
   - Define structure and placement for the 4 distinct opponent archetypes:
     1. Rush: Immediate aggressive core attacker swarm.
     2. Economy: Rapid multi-harvester expansion.
     3. Brawler: Heavy unit-combat escort and skirmisher.
     4. Turtle: Defensive bunker with late-game push.
   - Check how they are compiled and whether a shared runner/Makefile or separate directories should be used.
3. Automated Evaluation Suite:
   - How to orchestrate a 20+ match test suite against the 6 opponents (Rush, Economy, Brawler, Turtle, gridmaster, strategy-bot).
   - How to verify:
     * Clean make re (-Wall -Wextra -Werror)
     * ASan memory safety (0 leaks across continuous ticks)
     * Exactly 0 action failures
     * Win rate >= 85%
     * Tick latency < 0.5 ms
4. Manifests & Documentation Status:
   - Check if `docs/` exists and the current status of:
     * docs/STATUS.md
     * docs/MECHANICS.md
     * docs/EXPERIMENTS.md
     * docs/KNOWN_FAILURES.md

Output:
Write your complete report with verified paths and harness specifications to:
/home/malshare/Evaluations/Core_game/.agents/teamwork/explorer_harness_survey/handoff.md
Send a completion message back when finished.
