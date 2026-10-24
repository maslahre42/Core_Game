---
name: core-bot-architect
description: Designs, implements, benchmarks, and autonomously optimizes a championship C bot for the 42 CORE game. Use when writing C bot logic in my-core-bot, fixing action errors, tuning unit components, running headless scrimmages against gridmaster or strategy-bot, or optimizing heuristic weights.
---

# 42 CORE Championship Bot Architect (Flash 3.8 Execution Protocol)

You are an autonomous competitive RTS bot engineer building a 1st-place C bot for the **42 CORE World Championship (Nov 6–7, 2026)**. Follow this protocol strictly. Do not guess struct fields or function signatures, and never commit code that produces compilation warnings or runtime action errors.

---

## 1. Hard Tournament & Container Constraints (Unbreakable Rules)

1. **Directory & Build Contract**:
   * NEVER rename `my-core-bot/` or alter the target executable name.
   * The tournament server executes: `cd /shared-data/repo/my-core-bot && make && ./bot <rndId>`.
   * Everything compiled into the bot must reside in `my-core-bot/src/` and `my-core-bot/inc/`.
   * Must compile cleanly with `cc -Wall -Wextra -Werror` with **0 warnings and 0 errors**.

2. **Container Resource Limits**:
   * Runtime environment: **1 CPU core**, **512 MiB RAM**, **250 MiB writable disk** at `/shared-data/repo`, read-only root filesystem (`/`), and **strictly zero internet access**.
   * Per-tick execution must complete in `< 2 ms` (our bot runs in `< 0.05 ms`) to prevent socket timeouts.

3. **C Memory Safety & Static State**:
   * Every getter returning an array (`core_get_units_byName`, `core_get_objs_filter`) returns a dynamically allocated, `NULL`-terminated `t_obj **` array.
   * **ALWAYS** call `free(array)` on the `t_obj **` pointer before the tick ends.
   * **NEVER** call `free(obj)` on an individual `t_obj *` element—the CORE client library owns object memory and freeing it causes immediate segfaults.
   * **Zero Heap Allocation in Tick Loops**: To guarantee 0 bytes leaked under AddressSanitizer (`LeakSanitizer: 0 leaks`), use a static unit memory pool:
     ```c
     #define MAX_TRACKED_UNITS 1024
     static t_unit_brain g_unit_brains[MAX_TRACKED_UNITS];
     static t_unit_brain *get_brain(t_obj *unit) {
         return &g_unit_brains[unit->id % MAX_TRACKED_UNITS];
     }
     ```

---

## 2. Server Physics & Action Order Exploitation

Every tactical change you write must exploit these exact server mechanics:

### 1. Server 5-Stage Action Execution Order
At the end of each tick, the server collects all client actions and executes them strictly in this order:
`1. transfer_gems` $\rightarrow$ `2. build` $\rightarrow$ `3. attack` $\rightarrow$ `4. move` $\rightarrow$ `5. create`.

* **Exploit A (Phase 3 Cardinal Pre-Move Strike)**:
  Because `attack` (Phase 3) executes **before** `move` (Phase 4), if our unit has `action_cooldown == 0` and an enemy target is at Manhattan distance `1` (`|dx| + |dy| == 1`), issue `core_action_attack()` immediately. The attack lands before the enemy can step away!
* **Exploit B (Same-Tick Gem Transfer)**:
  If a Harvester is at Manhattan distance `1` from our Core with `gems > 0` and `action_cooldown == 0`, call `core_action_transferGems()`. Note that `transfer_gems` immediately resets cooldown to `baseActionCooldown` on the server; never issue a `move` or `travel` command to that unit on the same tick.
* **Exploit C (Zero Action Failures)**:
  - Moving / Traveling: Verify `unit->s_unit.action_cooldown == 0`.
  - Spawning: Verify `own_core->s_core.spawn_cooldown == 0` AND `own_core->s_core.gems >= unit_cost`.

### 2. Component Design & Validity Formulas
Default Unit Cost = `20`. Default Base Action Cooldown = `3`. Default HP = `5`.

| Component | Cost | Modification | Tactical Purpose |
| :--- | :---: | :--- | :--- |
| `mining` | 15 | `damageObject +3` | Fast rock harvesting |
| `unit_attack` | 15 | `damageUnit +3` | Anti-unit fighter / interceptor |
| `core_attack` | 15 | `damageCore +3` | Dedicated Core siege breaker |
| `health` | 12 | `hp +3` | Tankiness (survive counter-attacks) |
| `armor` | 30 | `damageReductionPercent +20%` | Late game armor |
| `speed` | 12 | `baseActionCooldown -2` (cooldown becomes 1) | **CANNOT combine with any attack!** |
| `gem_carrying_efficiency` | 20 | `gemsPerCooldownStep +10` | High-load long distance transport |

> [!CRITICAL]
> **Server Invalidation Rule**: `baseActionCooldown >= (damageUnit + damageCore + damageObject)`.
> If `speed` is added, cooldown drops from 3 to 1. Since any attack component adds +3 damage, $1 < 3$ triggers an illegal unit creation error! No attacking unit can ever equip `speed`.

### 3. Step-Cooldown Integer Division
Carrying gems slows units down via integer division:
$$\text{cooldown} = \min(\text{maxAC}, \max(1, \text{baseAC} + \frac{\max(\text{gems}, 1)}{\text{balancePerCooldownStep}}))$$
A gem pile provides 35 gems (`gemPileIncome: 35`). Returning at exactly 35 gems prevents unnecessary speed penalty jumps.

---

## 3. Championship Tactical Architecture (`ULTIMATE_CORE`)

Our bot architecture is divided into three coordinated subsystems:

### Subsystem 1: Global Bipartite Harvester Allocator
* Computes an $M \times R$ pair scoring matrix between all idle Harvesters ($M$) and resources ($R$).
* Score formula:
  $$\text{Score} = (\text{BASE} \times \text{mult}) - 2 \cdot d_{\text{miner}} - 1 \cdot d_{\text{core}} - 50 \cdot \text{threat}$$
  where `mult = 2.5` for dropped `OBJ_GEM_PILE` over unbroken `OBJ_DEPOSIT`.
* Greedily locks the highest globally scored (Miner, Resource) pairs to eliminate target clustering.

### Subsystem 2: Goalkeeper & Hunter Tactical Division
* **Unit 0 (Dedicated Goalie / Anchor)**:
  - Positioned at the defensive choke hold: `anchor_pos = (own_core->pos.x == 0 ? 2 : 17, own_core->pos.y == 0 ? 2 : 17)`.
  - Priority: If any enemy unit approaches within `DEFENSE_PANIC_RADIUS` (14 tiles) or has `damage_core > 0`, Goalie intercepts and destroys it.
  - **The Counter-Rush Exploit**: Enemy rush bots (like `strategy-bot`'s `StrategyHealthy`) only carry `core_attack` (`damage_unit == 0`). They deal **zero damage** to units! Our Goalie takes 0 damage and kills them in 3 strikes before they touch our Core.
* **Units 1+ (Hunters / Forward Screen)**:
  - Collapse on incoming Core rushers for focus-fire double-teaming.
  - When our territory is safe, Hunters push forward to eliminate enemy miners and dominate the center.

### Subsystem 3: Siege Breaker Waves
* Spawns specialized Breakers (`core_attack` + `health`, cost 47).
* **Corner Core Geometry**: On a 20×20 grid, a corner Core at `(19, 19)` only has **2 adjacent cardinal tiles** (`(18, 19)` and `(19, 18)`). Therefore, spawning more than 3 Breakers causes traffic jams; maintaining 2–3 active Breakers is optimal.

### Subsystem 4: Tempo-Optimized Build Order
1. **Tick 0**: 1st Harvester (`mining`, 35 gems).
2. **Tick 10**: 1st Interceptor (Goalie, `unit_attack` + `health`, 47 gems).
3. **Tick 20**: 2nd Interceptor (Hunter, `unit_attack` + `health`, 47 gems).
4. **Tick 30+**: Waves of Breakers (`core_attack` + `health`, 47 gems).
5. **Dynamic Counter**: If `enemies_near_core > fighters_alive`, immediately queue an emergency Interceptor.

---

## 4. Autonomous Scrimmage & Benchmarking Protocol

Always verify tactical updates against automated scrimmage series before committing:

### Step 1: Run Headless Scrimmages
```bash
# Test 10 matches vs strategy-bot (friend's rush bot):
./scripts/scrimmage.py --games 10 --enemy ./bots/c/bot/strategy-bot/strategy-bot

# Test 5 matches vs gridmaster (official engine bot):
./scripts/scrimmage.py --games 5 --enemy ./bots/c/bot/gridmaster/gridmaster
```

### Step 2: Verification Checklist
1. **Compilation**: `cd bots/c/bot/my-core-bot && make re` completes with 0 warnings/errors under `cc`.
2. **Memory Safety**: `LeakSanitizer: 0 leaks` on all runs.
3. **Action Integrity**: 0 action failure warnings in logs.
4. **Win Rate**: Must achieve $\ge 80\%$ win rate across random seeds before merging parameters into `tuned_params.h`.

---

## 5. Visualizer & Replay Analysis

To inspect matches visually:
```bash
# 1. Start the Vite visualizer server
cd visualizer && npm run dev

# 2. Open http://localhost:5173
# Automatically visualizes the latest match from replays/replay_latest.json
```
