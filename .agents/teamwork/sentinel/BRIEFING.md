# BRIEFING — 2026-10-24T07:41:15Z

## Mission
Oversee the implementation, benchmarking, and verification of adaptive C bot ex_v01 in bots/c/bot/my-core-bot/ ensuring engine cooldown/unit rule compliance, zero action failures, and >=85% league win rate.

## 🔒 My Identity
- Archetype: sentinel
- Working directory: /home/malshare/Evaluations/Core_game/.agents/teamwork/sentinel/
- Orchestrator: fc9da3cb-50f8-4a94-8339-0b42020ff84a
- Victory Auditor: to be spawned on victory claim

## 🔒 Key Constraints
- No technical decisions — relay only
- Victory Audit is MANDATORY before reporting completion
- Keep context ultra-light; do not write code or analyze problems
- Must run progress reporting cron and liveness check cron
- Clean up all crons and subagents upon completion

## User Context
- **Last user request**: Build and benchmark adaptive C bot ex_v01 with strict action safety, zero action failures, opponent league evaluation, and documentation checkpoints.
- **Pending clarifications**: none
- **Delivered results**: none
- **Cron 1 (Progress)**: task-18 (*/8 * * * *)
- **Cron 2 (Liveness)**: task-20 (*/10 * * * *)

## Project Status
- **Phase**: in progress
- **Route**: General (`teamwork_preview_orchestrator`)
- **Route Rationale**: Complex multi-component SWE task requiring C bot implementation, strict engine action safety, local test league across 4 archetypes, documentation checkpoints, ASan verification, and >=85% win rate. Not document review, math/proof, or small SWE light.
- **Orchestrator Directory**: /home/malshare/Evaluations/Core_game/.agents/teamwork/orchestrator_1/

## Victory Audit Status
- **Triggered**: no
- **Verdict**: pending
- **Retry count**: 0

## Artifact Index
- /home/malshare/Evaluations/Core_game/.agents/teamwork/ORIGINAL_REQUEST.md — Authoritative record of user request
