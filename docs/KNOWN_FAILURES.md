# Known Failures & Edge-Case Audit

This document catalogues observed edge-case failures, root causes in engine source code, and verified resolutions.

---

## 1. Action Failures

### A. `transfer_gems: unit is on action cooldown`
* **Trigger**: Calling `core_action_travel()` or `core_action_move()` on the same tick as `core_action_transferGems()`.
* **Root Cause**: In `server/src/action/TransferGemsAction.cpp:128`, `srcUnit->resetActionCooldown()` resets the unit's cooldown immediately to `baseActionCooldown` (3). Because Phase 1 (`transfer_gems`) executes before Phase 4 (`move`), the subsequent move action evaluates `unit->getActionCooldown() > 0` and fails.
* **Resolution**: When a unit transfers gems, execute an immediate `continue;` so no subsequent move or attack is emitted on that tick.

### B. `move: invalid target position. should be empty` (Friendly Ally Collision)
* **Trigger**: Two friendly units selecting the same vacant tile in the same tick.
* **Root Cause**: `server/src/action/MoveAction.cpp:37` rejects moves if `getObjectAtPos(target_)` is non-null. The first friendly unit occupies the tile; the second unit fails.
* **Resolution**: Maintain `g_reserved_tiles[64][64]` reservation grid. When any friendly unit plans a move, the destination tile is immediately marked as reserved. Any subsequent allied unit checks reservations and paths around or holds position.

### C. `move: invalid target position. should be empty` (Stepping on Gem Piles)
* **Trigger**: Calling `move` onto a tile containing a `GemPile`.
* **Root Cause**: In engine physics (`server/src/object/GemPile.cpp:15-32`), `GemPile` inherits from `Object`. `MoveAction.cpp:37` requires `getObjectAtPos(target_) == nullptr`. Gem piles CANNOT be moved onto; they are collected by **attacking** them.
* **Resolution**: In `safe_travel_policy`, return `CORE_TRAVEL_ATTACK` for gem piles so the unit stops at Manhattan distance 1 and attacks to collect gems.

### D. `move: invalid target position. should be empty` (Contested Enemy-Adjacent Tiles)
* **Trigger**: Moving into an empty tile adjacent to an enemy unit that has `action_cooldown <= 0`.
* **Root Cause**: The enemy unit also attempts to step into the same vacant tile on that tick. The server shuffles Phase 4 moves randomly (`shuffle_actions_vector`). If the enemy's move executes first, the friendly move fails.
* **Resolution**: In `safe_travel_policy`, detect if a tile is adjacent to a ready enemy unit (`o->s_unit.action_cooldown <= 0 && manhattan_dist(pos, o->pos) == 1`). Return `CORE_TRAVEL_BLOCK` so our unit holds position, forcing the enemy to step forward and allowing our unit to strike first in Phase 3.

### E. `createUnit: illegal unit configuration`
* **Trigger**: Spawning a unit with `speed` and any attack component (`unit_attack`, `core_attack`, `mining`).
* **Root Cause**: `game.config.json` enforces:
  $$\text{baseActionCooldown} \ge (\text{damageUnit} + \text{damageCore} + \text{damageObject})$$
  `speed` reduces cooldown to 1. Attack components add +3 damage ($1 < 3$).
* **Resolution**: In bot unit spawner, validate all component multisets against the invalid condition rule before emitting `core_action_createUnit()`.

---

## 2. Strategic Deadlocks & Timing Issues

### A. Core Starvation Deadlock (`idleIncomeTimeOut = 100`)
* **Trigger**: All friendly miners die and Core gems drop below unit spawn cost (35 gems).
* **Root Cause**: In `game.config.json:5`, `idleIncomeTimeOut` is set to 100. After tick 100, the core receives 0 idle income. If an opponent only has anti-unit fighters (e.g. `opponent-brawler`), neither side can damage each other's core, causing a match to run until tick 30,000.
* **Resolution**:
  1. Dual-Harvester Opening: Spawns 2 Harvesters at Ticks 1 and 11 from the initial 300 gems, establishing a perpetual revenue stream.
  2. Harvester Retreat Guard: Harvesters immediately flee back to Core if an enemy enters Manhattan distance 3, protecting the economic backbone.

### B. Corner Core Bottlenecking
* **Trigger**: Units with 0 core damage (e.g. Interceptors) crowding adjacent to the opponent's core at `(19, 19)`.
* **Root Cause**: Corner cores have only 2 cardinal attack tiles (`(18, 19)` and `(19, 18)`). If Interceptors occupy these tiles, Breakers (`damage_core > 0`) cannot access the core.
* **Resolution**: In `safe_travel_policy`, units with `damage_core == 0` are forbidden from entering cardinal attack tiles adjacent to the enemy core, leaving them permanently open for Breakers.

---

## 3. Infrastructure & Runtime Errors

### A. `Server: Could not bind on socket: Address already in use`
* **Trigger**: Consecutive rapid server runs on port 4444.
* **Root Cause**: Accepted child TCP sockets entering `TIME_WAIT` after game shutdown.
* **Resolution**: Configured dynamic port assignment via `SERVER_PORT` environment variable in `scripts/scrimmage.py` and `server/src/main.cpp`.

### B. `clang: error: -lm: 'linker' input unused`
* **Trigger**: Compiling with `clang` when `-lm` is passed in `CFLAGS` during `-c` compilation steps.
* **Resolution**: Separated `CFLAGS` from `LDFLAGS` in `Makefile`.
