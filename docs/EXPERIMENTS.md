# Experiment Log & Iterative Benchmarks

This log tracks every empirical hypothesis, tested code change, sample sizes, and results across bot iterations.

---

## Experiment 1: Baseline Architecture vs Gridmaster
* **Hypothesis**: Replacing naive `travel` loop with a 3-role state machine (`Harvester`, `Interceptor`, `Breaker`) and bipartite resource allocator will defeat the engine baseline.
* **Tested Changes**:
  - Global greedy bipartite resource allocator matrix.
  - Lanchester focus-fire scoring (`SCORE_HP_FOCUS_WEIGHT = 3`).
  - Unit designs: Harvester (55 gems), Interceptor (47 gems), Breaker (47 gems).
* **Sample Size**: 3 matches vs `gridmaster`.
* **Result**: **3 / 3 Wins** (100% win rate). Core destroyed in 259 ticks.
* **Status**: **PROMOTED to Model v1**.

---

## Experiment 2: Benchmark Against `strategy-bot` (Friend's Rush Bot)
* **Hypothesis**: Model v1 will maintain high win rate against external rush strategies.
* **Sample Size**: 3 matches vs `strategy-bot`.
* **Result**: **1 Win, 2 Losses** (33.3% win rate).
* **Failure Analysis**:
  - `strategy-bot` spawned `StrategyHealthy` (`core_attack` + `health`, 47 gems) at Tick 21 and rushed our Core.
  - Model v1 Interceptors had `SCORE_HUNT_MINER_BONUS = 600`, causing them to run across the map towards the enemy base to chase an enemy miner, leaving our Core unguarded.
  - Enemy Breakers arrived 14 ticks earlier and chipped our Core to 0 while enemy Core was at 15 HP.

---

## Experiment 3: Goalkeeper / Anchor Division & 35-Gem Spawner (Model v2)
* **Hypothesis**: Designating Unit 0 as a tethered Goalie at choke point `(2, 2)` and lowering Harvester cost to 35 gems will eliminate base rush vulnerability and accelerate Breaker production.
* **Tested Changes**:
  - Unit 0 holds anchor position `(2, 2)` or intercepts any enemy within 14 tiles or carrying `damage_core > 0`.
  - Harvester 1 spawns with `mining` only (35 gems), returning immediately at 35 gems (1 gem pile).
  - Breakers spawn starting at Tick 30 (11 ticks earlier).
  - Pre-movement Phase 3 cardinal attacks land guaranteed strikes before enemies move.
* **Sample Size**: 10 matches vs `strategy-bot`, 5 matches vs `gridmaster`.
* **Result**:
  - **10 / 10 Wins (100.0%)** vs `strategy-bot`.
  - **5 / 5 Wins (100.0%)** vs `gridmaster`.
  - 0 action failures, 0 AddressSanitizer leaks.
* **Status**: **PROMOTED to Model v2**.

---

## Experiment 4: Model `ex_v01` — Engine Pitfall Elimination & Full League Evaluation
* **Hypothesis**: Addressing the 5 verified engine mechanics pitfalls identified in the strategy PDF review (handling `action_cooldown <= 0`, same-tick transfer cooldown resets, valid component combinations, dynamic pricing, and collision-free tile reservations) will achieve $\ge 85\%$ win rate across a 6-opponent league with exactly 0 action failures.
* **Tested Changes**:
  - Guarded action readiness with `action_cooldown <= 0` (handling idle standing decrements to negative values).
  - Enforced immediate turn termination (`continue;`) following `core_action_transferGems()` to prevent Phase 4 movement cooldown violations.
  - Implemented dynamic tile reservation grid (`g_reserved_tiles[64][64]`) and custom `safe_travel_policy` callback to prevent allied move collisions and contested-tile races with enemies.
  - Deployed Dual-Harvester economic opening (Tick 1 and Tick 11) to prevent resource bankruptcy against idle-income timeouts.
  - Structured hunter perimeter escort preventing Interceptors from bottlenecking corner core attack tiles (`(18, 19)`, `(19, 18)`).
* **Sample Size**: 24-Match Multi-Archetype League Tournament (4 matches each against `gridmaster`, `strategy-bot`, `opponent-rush`, `opponent-economy`, `opponent-brawler`, `opponent-turtle`).
* **Results**:
  - vs `gridmaster`: **4 / 4 Wins (100.0%)** | 0 Action Failures
  - vs `strategy-bot`: **4 / 4 Wins (100.0%)** | 0 Action Failures
  - vs `opponent-rush`: **4 / 4 Wins (100.0%)** | 0 Action Failures
  - vs `opponent-economy`: **4 / 4 Wins (100.0%)** | 0 Action Failures
  - vs `opponent-brawler`: **4 / 4 Wins (100.0%)** | 0 Action Failures
  - vs `opponent-turtle`: **4 / 4 Wins (100.0%)** | 0 Action Failures
  - **Overall League Record**: **24 / 24 Wins (100.0%)** | **0 Action Failures** | **0 AddressSanitizer Leaks**
* **Status**: **PROMOTED to Active Champion (`ex_v01`)**.
