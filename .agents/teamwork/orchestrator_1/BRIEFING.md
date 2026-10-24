# BRIEFING — 2026-10-24T07:45:00Z

## Mission
Build and benchmark adaptive C bot ex_v01 in bots/c/bot/my-core-bot/ with zero action failures, engine cooldown compliance, 4 opponent archetypes, full docs, and >=85% win rate.

## 🔒 My Identity
- Archetype: orchestrator
- Roles: orchestrator, user_liaison, human_reporter, successor
- Working directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/orchestrator_1/
- Original parent: parent
- Original parent conversation ID: 7539a154-f218-496a-9f02-9bb04d154d0f

## 🔒 My Workflow
- **Pattern**: Project Pattern (Survey -> Decompose & Interface Contracts -> Iteration Loop with Explorer/Worker/Reviewer/Challenger/Auditor)
- **Scope document**: /home/malshare/Evaluations/Core_game/PROJECT.md
1. **Decompose**: Survey existing codebase, engine specs, docs, and skills via 3 parallel Explorers. Synthesize into PROJECT.md with architecture, feature inventory, milestones, interface contracts, and code layout.
2. **Dispatch & Execute**:
   - For each milestone: 3 Explorers -> 1 Worker (armed with domain skills) -> 2 Reviewers + 2 Challengers + 1 Forensic Auditor. Strict gate pass criteria.
   - Dual-track: E2E testing / scrimmage league + implementation milestones.
3. **On failure** (in this order):
   - Retry: nudge stuck agent or re-send task
   - Replace: spawn fresh agent with partial progress
   - Skip: proceed without (only if non-critical; auditor is NEVER skipped)
   - Redistribute: split stuck agent's remaining work
   - Redesign: re-partition decomposition
   - Escalate: report to parent (last resort)
4. **Succession**: Threshold: 16 spawns. When reached and subagents complete, soft handoff to successor.
- **Work items**:
  1. Survey phase (3 parallel explorers) [in-progress]
  2. PROJECT.md synthesis & decomposition [pending]
  3. Milestone execution [pending]
  4. Final evaluation & adversarial hardening [pending]
- **Current phase**: 0 (Survey)
- **Current focus**: Launching Survey Explorers

## 🔒 Key Constraints
- NEVER write, modify, or create source code files directly.
- NEVER run build/test commands yourself — require workers to do so.
- NEVER investigate or explore the problem at the code level — dispatch Explorers for technical investigation.
- You MAY use file-editing tools ONLY for metadata/state files (.md) in your .agents/teamwork/ folder.
- AUDIT ENFORCEMENT: Forensic auditor reports INTEGRITY VIOLATION -> immediate failure and veto.
- Include path to ORIGINAL_REQUEST.md in EVERY subagent dispatch.
- Mandatory integrity warning in Worker dispatch prompts.

## Current Parent
- Conversation ID: 7539a154-f218-496a-9f02-9bb04d154d0f
- Updated: 2026-10-24T07:42:22Z

## Key Decisions Made
- Initializing Project Pattern with survey phase using 3 parallel explorers to inspect engine mechanics, bot architecture, and scrimmage framework.

## Team Roster
| Agent | Type | Work Item | Status | Conv ID |
|-------|------|-----------|--------|---------|
| spec_miner_engine_survey | teamwork_preview_spec_miner | Engine Mechanics & Rules Mining | completed | 7b341bfb-bcff-4fbc-88cd-454034955c7a |
| explorer_bot_survey | teamwork_preview_explorer | Bot Architecture & Prototype Survey | completed | 7d720a64-3f3f-4250-9300-8ef15f684774 |
| explorer_harness_survey | teamwork_preview_explorer | Scrimmage Harness & League Survey | completed | eb3bf5e2-ab09-4823-a4c0-9155dbc781e0 |
| worker_m1 | teamwork_preview_worker | Scrimmage Harness Upgrades (scripts/scrimmage.py) | completed | 78dc20ab-67c3-4049-b0e7-819e8c97e631 |
| reviewer_m1_1 | teamwork_preview_reviewer | M1 Review 1: Process & Replay | in-progress | b95568d0-ba61-402b-ab32-ac9279002893 |
| reviewer_m1_2 | teamwork_preview_reviewer | M1 Review 2: Edge Cases & Sockets | in-progress | 4b12f425-3e38-4549-8653-e69f9329133d |
| challenger_m1_1 | teamwork_preview_challenger | M1 Challenger 1: Stress & Port Isol | in-progress | 798bcca4-7abd-4628-a1f7-6c796323d190 |
| challenger_m1_2 | teamwork_preview_challenger | M1 Challenger 2: Replay Parsing | in-progress | 0a075e07-4322-4309-8dda-1c025d97a374 |
| auditor_m1 | teamwork_preview_auditor | M1 Forensic Integrity Audit | in-progress | b9e15778-c78e-493b-9709-a8cfd010f0fa |

## Succession Status
- Succession required: no
- Spawn count: 9 / 16
- Pending subagents: b95568d0-ba61-402b-ab32-ac9279002893, 4b12f425-3e38-4549-8653-e69f9329133d, 798bcca4-7abd-4628-a1f7-6c796323d190, 0a075e07-4322-4309-8dda-1c025d97a374, b9e15778-c78e-493b-9709-a8cfd010f0fa
- Predecessor: none
- Successor: not yet spawned

## Active Timers
- Heartbeat cron: task-18 (every 10m)
- Safety timer: none

## Artifact Index
- /home/malshare/Evaluations/Core_game/.agents/teamwork/ORIGINAL_REQUEST.md — Authoritative User Request
- /home/malshare/Evaluations/Core_game/.agents/teamwork/orchestrator_1/DISPATCH.md — Parent dispatch log
- /home/malshare/Evaluations/Core_game/.agents/teamwork/orchestrator_1/BRIEFING.md — Persistent working memory
- /home/malshare/Evaluations/Core_game/.agents/teamwork/orchestrator_1/progress.md — Liveness & status tracking
- /home/malshare/Evaluations/Core_game/PROJECT.md — Master project architecture, feature inventory, milestones (TBD after survey)
