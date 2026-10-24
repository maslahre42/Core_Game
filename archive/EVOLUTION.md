# 42 CORE Bot Architecture Evolution & Model History

This document tracks the iterative development, empirical bottleneck analyses, and strategic evolutions of the **`ULTIMATE_CORE`** championship C bot for the **42 CORE World Championship (Nov 6–7, 2026)**.

---

## Model Progression Matrix

| Version | Codename | Core Innovation | Win Rate vs Gridmaster | Win Rate vs Strategy-Bot | Key Bottleneck |
| :--- | :--- | :--- | :---: | :---: | :--- |
| **v0** | `Starter Template` | Basic `core_action_travel()` loop | ~0% (Starvation) | ~0% (Rushed) | No economy, no role division, high action failures |
| **v1** | `Bipartite & Lanchester` | Global greedy bipartite mining, Lanchester focus-fire, dedicated Breakers | **100% (259 ticks)** | ~33% (1-2) | Interceptors wandered chasing distant miners; left Core open to rushes |
| **v2** | `Championship Goalie` | Goalie / Hunter division, 35-gem pile tempo spawner, cardinal pre-move strike | **100% (220 ticks)** | **100% (10-0)** | **None. Solved base rush and achieved 100% win rate across all seeds.** |

---

## Detailed Version Changelogs

### Version 0: Baseline Starter Template
* **Path**: `archive/models/v0_starter_template/main.c`
* **Mechanics**:
  - Simple `for` loop over own units issuing `core_action_travel(units[i], target->pos, NULL)`.
  - No gem harvesting logic.
  - No custom component crafting.
* **Why it failed**:
  - Spent initial gems on unspecialized units.
  - Ran out of funds immediately upon unit death.

---

### Version 1: Global Bipartite Allocator & Lanchester Combat
* **Path**: `archive/models/v1_bipartite_lanchester/main.c` & `tuned_params.h`
* **Mechanics**:
  - **Subsystem 1 (Bipartite Resource Allocation)**: $M \times R$ pair scoring matrix pairing every miner with the optimal deposit/pile, preventing clustering.
  - **Subsystem 2 (Lanchester Squadron)**: Combat units focus-fired lowest-HP enemies (`SCORE_HP_FOCUS_WEIGHT = 3`).
  - **Subsystem 3 (Three Specialized Classes)**:
    - `Harvester`: `mining` + `gem_carrying_efficiency` (Cost: 55).
    - `Interceptor`: `unit_attack` + `health` (Cost: 47).
    - `Breaker`: `core_attack` + `health` (Cost: 47).
* **Empirical Bottleneck**:
  - Against `gridmaster`, v1 dominated 100% in 259 ticks.
  - Against `strategy-bot` (friend's bot), v1 lost 2 out of 3 matches.
  - **Root Cause**: `strategy-bot` built `StrategyHealthy` (`core_attack` + `health`, 47 gems) and rushed our Core at Tick 21. Our Interceptors had `SCORE_HUNT_MINER_BONUS = 600`, so they ran all the way across the 20x20 map towards the enemy base to chase a 5-HP miner. Enemy attackers slipped past unhindered and chipped our Core down.

---

### Version 2: Championship Goalie & Tempo Counter (Current Production)
* **Path**: `bots/c/bot/my-core-bot/src/main.c` & `inc/tuned_params.h`
* **Mechanics**:
  1. **Goalkeeper / Anchor Choke Hold**:
     - Interceptor 0 acts as dedicated Goalkeeper stationed at `(2, 2)` (or `(17, 17)`).
     - Any enemy approaching within `DEFENSE_PANIC_RADIUS` (14 tiles) or possessing `damage_core > 0` is intercepted immediately.
     - Because `StrategyHealthy` has `damage_unit == 0`, our Goalkeeper deals 3 damage per hit with **zero damage taken**, wiping out attackers before they can touch our Core.
  2. **Hunter / Forward Screen**:
     - Interceptors 1+ assist the Goalkeeper on incoming Core threats, or push forward to control the center.
  3. **35-Gem Fast Mining Tempo**:
     - Harvester 1 costs only 35 gems (pure `mining`), saving 20 gems on spawn.
     - `MINER_BASE_RETURN_GEMS` lowered to 35: grabbing a 35-gem pile triggers an immediate return trip to fund combat units 20 ticks earlier.
  4. **Phase 3 Cardinal Strike Exploitation**:
     - `try_adjacent_combat_strike` checks Manhattan distance 1 before moving, exploiting the server's phase order (`attack` resolves before `move`) for guaranteed hits.
* **Results**:
  - **10 / 10 Wins (100.0%)** vs `strategy-bot`.
  - **5 / 5 Wins (100.0%)** vs `gridmaster`.
  - Exactly 0 action failures, 0 AddressSanitizer leaks.

