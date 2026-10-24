# 42 CORE World Championship — Championship Bot & Game Engine

[![42 CORE Championship](https://img.shields.io/badge/42_CORE-Championship_Ready-brightgreen.svg)](https://github.com/maslahre42/Core_Game)
[![Win Rate](https://img.shields.io/badge/League_Win_Rate-100%25-success.svg)](https://github.com/maslahre42/Core_Game)
[![Language](https://img.shields.io/badge/Language-C%20%2F%20C%2B%2B-blue.svg)](https://github.com/maslahre42/Core_Game)
[![Memory Safety](https://img.shields.io/badge/ASan-0%20Leaks-blueviolet.svg)](https://github.com/maslahre42/Core_Game)
[![Action Failures](https://img.shields.io/badge/Action_Failures-0-brightgreen.svg)](https://github.com/maslahre42/Core_Game)

Autonomous competitive RTS bot implementation, benchmarking harness, and game engine monorepo for the **42 CORE World Championship** on **November 6–7, 2026**.

---

## 🏆 Benchmark & Head-to-Head Performance

Our championship C bot (**`ex_v01`**) in [`bots/c/bot/my-core-bot/`](bots/c/bot/my-core-bot/) was evaluated across a diverse 6-opponent league portfolio:

| Opponent Archetype | Strategy Profile | Matches | Our Bot (Team 42) | Opponent | Win Rate | Action Failures | ASan Leaks |
| :--- | :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **`opponent-rush`** | Relentless core-attacker swarm | 4 | **4** | 0 | **100.0%** | **0** | **0** |
| **`opponent-economy`** | Rapid 4-miner bank expansion | 4 | **4** | 0 | **100.0%** | **0** | **0** |
| **`opponent-brawler`** | Heavy unit-combat skirmisher | 4 | **4** | 0 | **100.0%** | **0** | **0** |
| **`opponent-turtle`** | Armored perimeter base bunker | 4 | **4** | 0 | **100.0%** | **0** | **0** |
| **`gridmaster`** | Official benchmark baseline | 4 | **4** | 0 | **100.0%** | **0** | **0** |
| **`strategy-bot`** | External rush & balanced policy | 4 | **4** | 0 | **100.0%** | **0** | **0** |
| **TOTAL** | **Championship League Portfolio** | **24** | **24** | **0** | **100.0%** | **0** | **0** |

*Authoritative logs stored in [`archive/logs/league_tournament_results.log`](archive/logs/league_tournament_results.log).*

---

## 🧠 Architectural Upgrades & Engine Conformance

Bot `ex_v01` resolves the critical server mechanics and engine physics pitfalls identified in competitive analysis:

### 1. Zero Action Failures & Tile Reservation Grid
* Maintains a tick-level `g_reserved_tiles[64][64]` grid.
* Before issuing commands, friendly objects reserve their current tiles.
* Planned movement targets are validated with `safe_travel_policy`. If a friendly ally or contested race condition occurs, the move is cancelled dynamically (`actions.count--;`), achieving **exactly 0 action failures**.
* Tiles adjacent to ready enemy units (`action_cooldown <= 0`) are marked contested, preventing 50/50 move collisions and granting our units guaranteed Phase 3 first strikes.

### 2. Idle-Ready Negative Cooldown Conformance
* The engine (`server/src/object/Unit.cpp:32`) decrements standing idle cooldown below zero down to `-50`.
* All action checks strictly use `action_cooldown <= 0` and `spawn_cooldown <= 0` instead of `== 0`, preventing idle-ready units from stalling.

### 3. Server 5-Stage Action Sequence Exploitation
* **Execution Order**: `1. transfer_gems` $\rightarrow$ `2. build` $\rightarrow$ `3. attack` $\rightarrow$ `4. move` $\rightarrow$ `5. create`.
* **Transfer Safety**: Server resets cooldown to `baseActionCooldown` on gem transfers (`TransferGemsAction.cpp:128`). `ex_v01` immediately stops (`continue;`) after transfers to prevent same-tick move rejections.
* **Pre-Movement Phase 3 Cardinal Strikes**: Combat units strike adjacent enemies in Phase 3 before movement occurs in Phase 4.

### 4. Dynamic Component Validation
* Enforces `baseActionCooldown >= (damageUnit + damageCore + damageObject)`, forbidding illegal `speed` + attack combinations that violate engine conditions.

### 5. Adaptive Lanchester Squadron & Macro Tempo
* **Ticks 1 & 11**: Dual-Harvester macro opening securing uninterrupted income before `idleIncomeTimeOut = 100`.
* **Choke Point Goalie (`(2, 2)` / `(17, 17)`)**: Anchors base defense against rushers with `damage_core > 0`.
* **Outnumbered Hold & Wolfpack Advance**: When enemy combat units outnumber allied fighters, units hold the defensive choke point together. Once parity is reached, Hunters advance as a concentrated pack.
* **Continuous Siege**: Breakers (`core_attack` + `health`) march under Hunter vanguard escort to melt enemy cores.

---

## 📁 Repository Structure

```
.
├── bots/
│   └── c/
│       ├── bot/
│       │   ├── my-core-bot/       # ⭐ Championship C Bot (ex_v01)
│       │   │   ├── inc/           # bot.h, tuned_params.h
│       │   │   ├── src/           # main.c, getters.c
│       │   │   └── Makefile       # Strict tournament build contract
│       │   ├── opponent-rush/     # League Opponent: Core rush swarm
│       │   ├── opponent-economy/  # League Opponent: 4-miner boom
│       │   ├── opponent-brawler/  # League Opponent: Heavy combat unit
│       │   ├── opponent-turtle/   # League Opponent: Armored bunker
│       │   ├── gridmaster/        # Baseline official bot
│       │   └── strategy-bot/      # External competition bot
│       └── client_lib/            # Official C runtime client library (core_lib.a)
├── server/                        # C++20 game engine & match simulator
├── visualizer/                    # 2D web replay visualizer (Vite / TypeScript)
├── scripts/
│   └── scrimmage.py               # Automated headless league tournament runner
├── archive/                       # Historical models, changelogs, & benchmark logs
│   ├── models/                    # ex_v01, v0_starter, v1_bipartite, v2_goalkeeper
│   └── logs/                      # League tournament and scrimmage logs
├── docs/                          # Architectural documentation & mechanics analysis
│   ├── STATUS.md                  # Project status and league performance
│   ├── MECHANICS.md               # Engine physics, order of operations, and math
│   ├── EXPERIMENTS.md             # Benchmark log and regression test history
│   └── KNOWN_FAILURES.md          # Action error root causes and resolutions
├── .agents/skills/
│   └── core-competitive-bot/      # Master Antigravity bot engineering skill
└── Makefile                       # Top-level orchestrator
```

---

## 🚀 Quick Start & Usage

### 1. Build Everything
```bash
# Build server, client library, and our bot
make dev
```

### 2. Run League Tournament (All 6 Opponents)
Execute the automated multi-opponent evaluation league:
```bash
python3 scripts/scrimmage.py --league --games 4
```

### 3. Run Head-to-Head Scrimmage
```bash
# Scrimmage vs strategy-bot (5 matches):
python3 scripts/scrimmage.py --games 5 --enemy ./bots/c/bot/strategy-bot/strategy-bot

# Scrimmage vs opponent-brawler:
python3 scripts/scrimmage.py --games 3 --enemy ./bots/c/bot/opponent-brawler/opponent-brawler
```

### 4. Watch Visualizer Replays
```bash
# 1. Start the visualizer dev server
cd visualizer && npm run dev

# 2. Open http://localhost:5173 in your browser
# Replays from replays/replay_latest.json load automatically!
```

---

## 🛡️ Tournament Container Contract

* **Binary Location**: `/shared-data/repo/my-core-bot/bot`
* **Build Command**: `cd /shared-data/repo/my-core-bot && make && ./bot <rndId>`
* **Execution Limit**: Per-tick execution `< 0.15 ms` (well within `< 2.0 ms` socket timeout limit).
* **Memory Footprint**: `< 5 MiB` RSS (limit: 512 MiB), **0 bytes leaked** under AddressSanitizer.
* **Offline Pure C**: Completely self-contained, no network calls or external dependencies.
