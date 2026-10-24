# Project Status: 42 CORE Championship Bot

> Last Updated: 2026-10-24
> Active Champion: `ex_v01` (Adaptive Utility & Zero-Collision Architecture)
> Repository: `git@github.com:maslahre42/Core_Game.git`
> Working Directory: `/home/malshare/Evaluations/Core_game`
> Target Tournament Deadline: November 6–7, 2026

---

## 1. Champion Performance Summary

| Metric | Target | Achieved (`ex_v01`) | Status |
| :--- | :--- | :--- | :--- |
| **Clean Compilation** | `-Wall -Wextra -Werror` | 0 errors, 0 warnings (`cc -g -fsanitize=address`) | ✅ PASS |
| **Memory Safety** | 0 bytes leaked (ASan) | 0 bytes leaked across 30,000 continuous ticks | ✅ PASS |
| **Action Failure Rate** | Exactly 0 failures | **0 failures** across all evaluation matches | ✅ PASS |
| **League Win Rate** | $\ge 85.0\%$ | **100.0%** (24 / 24 wins across 6 archetypes) | ✅ PASS |
| **Average Per-Tick Latency** | $< 0.50\text{ ms}$ | **$0.05 - 0.15\text{ ms}$** internal tick execution | ✅ PASS |

---

## 2. Opponent League Portfolio

| Opponent Archetype | Strategy Profile | Matches | Results | Action Failures | ASan Leaks |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`gridmaster`** | Naive direct-travel baseline | 4 | **4 / 4 (100%)** | 0 | 0 |
| **`strategy-bot`** | External rush & balanced policy | 4 | **4 / 4 (100%)** | 0 | 0 |
| **`opponent-rush`** | Relentless core-attacker swarm | 4 | **4 / 4 (100%)** | 0 | 0 |
| **`opponent-economy`** | Rapid 4-miner bank expansion | 4 | **4 / 4 (100%)** | 0 | 0 |
| **`opponent-brawler`** | Heavy unit-combat skirmisher | 4 | **4 / 4 (100%)** | 0 | 0 |
| **`opponent-turtle`** | Armored perimeter base bunker | 4 | **4 / 4 (100%)** | 0 | 0 |
| **TOTAL** | **Full Tournament League** | **24** | **24 / 24 (100%)** | **0** | **0** |

---

## 3. Core Architectural Upgrades in `ex_v01`

1. **Idle-Ready Negative Cooldown Conformance**:
   Guards all action emitters with `action_cooldown <= 0` rather than `== 0`, correctly accounting for `server/src/object/Unit.cpp:32` where idle standing decrements cooldown into negative values (down to `-50`).
2. **Transfer Cooldown Reset Action Safety**:
   In Phase 1 (`transfer_gems`), server immediately invokes `srcUnit->resetActionCooldown()`. `ex_v01` executes a strict `continue;` immediately following any gem transfer, preventing same-tick movement or attack commands.
3. **Collision-Free Tile Reservation System**:
   Maintains a tick-level `g_reserved_tiles[64][64]` reservation grid. Unvacated allied objects, planned friendly destinations, and contested tiles adjacent to ready enemy units are marked as impassable or heavily weighted, eliminating `move: invalid target position. should be empty`.
4. **Dynamic Component Validation**:
   Enforces `baseActionCooldown >= (damageUnit + damageCore + damageObject)` dynamically. Prevents invalid illegal combinations (such as `speed` with attack components).
5. **Zero-Allocation Static Memory Architecture**:
   Zero `malloc`/`calloc`/`free` calls in the per-tick execution loop. Uses static buffers (`g_brains[1024]`, `g_pairs[512]`, `g_reserved_tiles[64][64]`).
6. **Adaptive Lanchester Macro Economy**:
   Dual-harvester economic opening (Ticks 1 and 11) securing uninterrupted gem flow, Goalkeeper anchor at `(2, 2)`, Hunter escort perimeter holding, and early Breakers (Tick 31) marching to enemy cores.

---

## 4. Build & Test Commands

* **Compile Candidate Bot**:
  ```bash
  cd bots/c/bot/my-core-bot && make re
  ```
* **Run 24-Match Multi-Archetype League Tournament**:
  ```bash
  python3 scripts/scrimmage.py --league --games 4 --base-port 4700
  ```
* **Run Single Match vs Specific Opponent**:
  ```bash
  python3 scripts/scrimmage.py --games 1 --enemy ./bots/c/bot/strategy-bot/strategy-bot
  ```
* **Run Visualizer**:
  ```bash
  cd visualizer && npm run dev
  ```
