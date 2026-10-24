# Orchestrator Plan

## Goal
Build and benchmark candidate ex_v01 in `bots/c/bot/my-core-bot/` achieving 0 action failures, strict engine cooldown compliance, zero leaks under ASan, <0.5ms tick latency, >=85% win rate over 20+ matches against 4 archetypes (rush, economy, brawler, turtle) + gridmaster and strategy-bot, with full documentation.

## Phases
1. **Phase 0: Survey & Ground Truth Mapping**
   - Explorer 1: Inspect server engine mechanics, rules, action validation, cooldowns, component prices, and network protocol.
   - Explorer 2: Inspect existing C bots (`my-core-bot`, `archive/models/v2_goalkeeper_championship`, benchmark bots, protocol parsing, memory management).
   - Explorer 3: Inspect evaluation harness, scrimmage scripts (`scripts/scrimmage.py`), port isolation, logging, and test fixtures.
2. **Phase 1: Master Specification (PROJECT.md)**
   - Synthesize survey findings into Feature Inventory, Component Costs, Engine Constraints, Architecture, and Milestone Plan.
3. **Phase 2: Milestone Decomposition & Execution**
   - M1: Opponent League Archetypes & Automated Scrimmage Infrastructure.
   - M2: Action Safety & Engine Conformance Core (cooldown tracker, unit design validator, dynamic cost model, collision avoidance).
   - M3: Adaptive Strategy Engine ex_v01 (harvesting state latching, anti-rush defense, siege breaker, static memory).
   - M4: League Benchmarking & Performance Optimization (ASan check, <0.5ms latency, >=85% win rate).
   - M5: Documentation & Verification Manifests (`docs/STATUS.md`, `docs/MECHANICS.md`, `docs/EXPERIMENTS.md`, `docs/KNOWN_FAILURES.md`).
4. **Phase 3: Final E2E Audit & Adversarial Hardening**
   - Forensic audit, 20-match validation suite, ASan run, adversarial stress tests.
