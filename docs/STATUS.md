# Project Status: 42 CORE Championship Bot

> Last Updated: 2026-10-26
> Active Champion: `ex_v02` (Multi-Agent Evolutionary Tournament Champion)
> Predecessor: `ex_v01` (Adaptive Utility & Zero-Collision Architecture)
> Repository: `git@github.com:maslahre42/Core_Game.git`
> Working Directory: `/home/malshare/Evaluations/Core_game`
> Target Tournament Deadline: November 6–7, 2026

---

## 1. Champion Performance Summary

| Metric | Target | Achieved (`ex_v02`) | Status |
| :--- | :--- | :--- | :--- |
| **Clean Compilation** | `-Wall -Wextra -Werror` | 0 errors, 0 warnings (`cc -g -fsanitize=address`) | ✅ PASS |
| **Memory Safety** | 0 bytes leaked (ASan) | 0 bytes leaked across continuous tournament runs | ✅ PASS |
| **Action Failure Rate** | Exactly 0 failures | **0 failures** across all evaluation matches | ✅ PASS |
| **League Win Rate** | $\ge 85.0\%$ | **100.0%** across all completed official matches | ✅ PASS |
| **Direct Head-to-Head** | $\ge 60.0\%$ | **10 - 3 (76.9%)** vs Citadel Bot Beta | ✅ PASS |
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

## 3. Core Architectural Upgrades in `ex_v02`

Derived from the 3-generation autonomous evolutionary tournament between Bot Alpha (Aggressor) and Bot Beta (Citadel):

1. **Outer Border Flank Hugging (`breaker_travel_policy`)**:
   Breakers traverse the outermost boundary tiles (`x == 0 || x == 19` / `y == 0 || y == 19`). In 20×20 grids, corner defensive chokes anchored at `(2,1)` and `(1,2)` leave a 2-Manhattan-distance blind spot at `(0,1)` and `(1,0)`. Because enemy units have attack range 1, our Breakers slip through untouched to demolish the enemy Core.
2. **Disciplined Corridor Corking (`run_hunters`)**:
   Hunter sentries cork `(2, 0)` and `(0, 2)` (or opposite base equivalent). Sentries unconditionally hold position against distant sirens, only engaging threats within 1 tile or adjacent to the Core, guaranteeing defense against retaliatory perimeter flanking.
3. **Armored Siege Breakers (`core_attack` + `armor` + `health`)**:
   Cost 77 gems, 8 HP, 20% damage reduction, 3 core damage. High durability prevents Breakers from being one-shot by base defenders while demolishing the Core in ~33 hits.
4. **Lanchester Wolfpack Concentration**:
   `SCORE_WOLFPACK_BONUS (+2500)` forces allied combat units to concentrate within 3 tiles of each other, focus-firing targets and exploiting Lanchester's Square Law.
5. **Target Overkill Prevention (`g_pending_target_dmg`)**:
   Static tracking of cumulative per-tick damage prevents units from wasting strikes on targets that are already dead.
6. **Inherited Engine Conformance**:
   - Idle-Ready Negative Cooldown Conformance (`<= 0`).
   - Transfer Cooldown Reset Action Safety (`continue;`).
   - Collision-Free Tile Reservation System (`g_reserved_tiles[64][64]` + contested tile avoidance).
   - Dynamic Component Validation.
   - Zero-Allocation Static Memory Architecture (100% ASan clean).

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
