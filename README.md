# 42 CORE World Championship — Championship Bot & Game Engine

[![42 CORE Championship](https://img.shields.io/badge/42_CORE-Championship_Ready-brightgreen.svg)](https://github.com/maslahre42/Core_Game)
[![Win Rate](https://img.shields.io/badge/Win_Rate-100%25-success.svg)](https://github.com/maslahre42/Core_Game)
[![Language](https://img.shields.io/badge/Language-C%20%2F%20C%2B%2B-blue.svg)](https://github.com/maslahre42/Core_Game)
[![Memory Safety](https://img.shields.io/badge/ASan-0%20Leaks-blueviolet.svg)](https://github.com/maslahre42/Core_Game)

Autonomous competitive RTS bot implementation, benchmarking harness, and game engine monorepo for the **42 CORE World Championship** on **November 6–7, 2026**.

---

## 🏆 Benchmark & Head-to-Head Performance

Our championship C bot (**`ULTIMATE_CORE`**) in [`bots/c/bot/my-core-bot/`](bots/c/bot/my-core-bot/) was rigorously tested across randomized map seeds against both the official engine benchmark and specialized rush strategies:

| Opponent | Matches | Our Bot (Team 42) | Opponent | Win Rate | Average Ticks | Action Errors |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **`strategy-bot`** (Rush Bot) | **10** | **10** | **0** | **100.0%** | ~180 – 260 | **0** |
| **`gridmaster`** (Official Bot) | **5** | **5** | **0** | **100.0%** | ~220 – 259 | **0** |
| **Total Scrimmage** | **15** | **15** | **0** | **100.0%** | — | **0** |

*Verified benchmark logs available in [`archive/logs/`](archive/logs/).*

---

## 🧠 Bot Architecture & Strategic Innovations

### 1. Dedicated Goalkeeper & Hunter Division
* **Unit 0 (Goalie / Anchor)**: Stationed at the primary defensive choke point (`(2, 2)` or `(17, 17)`). If an enemy unit crosses within 14 tiles or has `damage_core > 0`, the Goalie immediately intercepts and neutralizes the threat. Because pure rushers (like `StrategyHealthy`) only equip `core_attack` (`damage_unit == 0`), our Goalie suffers **0 damage** while killing attackers in 3 hits.
* **Units 1+ (Hunters / Forward Screen)**: Collapse on incoming Core threats for focus-fire double-teaming, or advance to assassinate enemy miners when our territory is secure.

### 2. Global Bipartite Resource Allocator
* Harvesters compute an $M \times R$ pair scoring matrix evaluating distance to miner, distance to Core, enemy threat radius, and a **2.5× bonus** for loose gem piles (`OBJ_GEM_PILE`) over unbroken deposits (`OBJ_DEPOSIT`).
* Globally greedily locks assignments to eliminate target clustering and wasted walking ticks.

### 3. Server Execution Order Exploitation
* **Server 5-Stage Action Sequence**: `transfer_gems` $\rightarrow$ `build` $\rightarrow$ `attack` $\rightarrow$ `move` $\rightarrow$ `create`.
* **Phase 3 Cardinal Pre-Move Strike**: Because attacks resolve before movements, our combat units execute `try_adjacent_combat_strike` to land guaranteed hits before adjacent enemies can step away.
* **Safe Cooldown-Aware Transfers**: Units only call `core_action_transferGems()` when `action_cooldown == 0` and adjacent to Core, avoiding illegal simultaneous movement penalties.

### 4. Tempo-Optimized Macro Spawner
1. **Tick 0**: 1st Harvester (`mining`, 35 gems). Immediate return threshold at 35 gems (exactly 1 gem pile).
2. **Tick 10**: 1st Interceptor (Goalie, `unit_attack` + `health`, 47 gems).
3. **Tick 20**: 2nd Interceptor (Hunter, `unit_attack` + `health`, 47 gems).
4. **Tick 30+**: Continuous waves of Breakers (`core_attack` + `health`, 47 gems) to siege and melt the enemy Core.
5. **Emergency Defense**: Spawns additional defenders dynamically if enemy threats near Core exceed active fighters.

---

## 📁 Repository Structure

```
.
├── bots/
│   └── c/
│       ├── bot/
│       │   ├── my-core-bot/       # ⭐ Championship C Bot (ULTIMATE_CORE)
│       │   │   ├── inc/           # bot.h, tuned_params.h
│       │   │   ├── src/           # main.c, getters.c
│       │   │   └── Makefile       # Strict tournament build contract
│       │   ├── strategy-bot/      # Opponent rush bot
│       │   └── gridmaster/        # Baseline official bot
│       └── client_lib/            # Official C runtime client library (core_lib.a)
├── server/                        # C++20 game engine & match simulator
├── visualizer/                    # 2D web replay visualizer (Vite / TypeScript)
├── scripts/
│   └── scrimmage.py               # Automated headless multi-match tournament runner
├── archive/                       # Historical models, changelogs, & benchmark logs
│   ├── models/                    # v0_starter, v1_bipartite, v2_goalkeeper
│   ├── logs/                      # Scrimmage logs (10-0 vs strategy-bot, 5-0 vs gridmaster)
│   └── EVOLUTION.md               # Detailed technical evolution changelog
├── .agents/skills/
│   └── core-bot-architect/        # Antigravity autonomous bot engineering skill
└── Makefile                       # Top-level orchestrator
```

---

## 🚀 Quick Start & Usage

### 1. Build Everything
```bash
# Build server, client library, and our bot
make dev
```

### 2. Run Headless Scrimmages
Use the automated multi-match runner to verify bot performance across random seeds:
```bash
# Run 10 matches against strategy-bot:
./scripts/scrimmage.py --games 10 --enemy ./bots/c/bot/strategy-bot/strategy-bot

# Run 5 matches against gridmaster:
./scripts/scrimmage.py --games 5 --enemy ./bots/c/bot/gridmaster/gridmaster
```

### 3. Watch Visualizer Replays
View match simulations in your browser:
```bash
# 1. Start the visualizer dev server
cd visualizer && npm run dev

# 2. Open http://localhost:5173 in your browser
# Replays from replays/replay_latest.json load automatically!
```

---

## 🛡️ Tournament Container Contract

Our bot is strictly optimized for the tournament execution environment:
* **Build Contract**: `cd /shared-data/repo/my-core-bot && make && ./bot <rndId>`
* **Execution Limit**: Per-tick execution `< 0.05 ms` (well within the `< 2 ms` socket timeout limit).
* **Memory Footprint**: `< 5 MiB` RSS (limit: 512 MiB), **0 bytes leaked** under AddressSanitizer.
* **Network & Disk**: Completely offline, self-contained pure C implementation.
