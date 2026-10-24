## 2026-10-24T07:44:15Z
You are the Spec Miner Engine Survey agent.
Your Working Directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/spec_miner_engine_survey/
Authoritative Request: /home/malshare/Evaluations/Core_game/.agents/teamwork/ORIGINAL_REQUEST.md

Available Skills:
- core-bot-architect: /home/malshare/Evaluations/Core_game/.agents/skills/core-bot-architect/SKILL.md
- core-competitive-bot: /home/malshare/Evaluations/Core_game/.agents/skills/core-competitive-bot/SKILL.md
Consult these skills and the codebase.

Mission:
Investigate the 42 CORE game engine source code and protocol definitions across the repository.
Extract exact server engine mechanics, formulas, and rules with source file path and line number citations for docs/MECHANICS.md:
1. Action Readiness: How server updates and checks unit action cooldown, handling of action_cooldown <= 0 (including negative values).
2. Transfer Reset: Server behavior when processing transfer_gems and why move/travel on the same tick fails/violates cooldown.
3. Unit Design Validation: Server-side validation rules, specifically baseActionCooldown >= (damageUnit + damageCore + damageObject), illegal component combinations (e.g. speed + attack), and allowed ranges.
4. Accurate Economy Prices: Server formulas/tables for dynamic unit creation cost (gems, energy, minerals).
5. Movement, Collision, Combat & Tick Resolution Order: How moves are resolved, priority, collision on occupied tiles, damage resolution, core health and destruction.
6. Server Error & Action Failure Signals: Exact error strings/signals emitted by the engine when an invalid command is received.

Output:
Write your complete report with verified evidence chains and exact line citations to:
/home/malshare/Evaluations/Core_game/.agents/teamwork/spec_miner_engine_survey/handoff.md
Send a completion message back when finished.
