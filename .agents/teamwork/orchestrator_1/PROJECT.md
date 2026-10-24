# Project: 42 CORE Adaptive Championship Bot (ex_v01)

## Architecture
- **Server Engine**: Headless 42 CORE server (`server/server`) on configurable port (`SERVER_PORT`), communicating over TCP sockets with JSON protocol. 4-phase sequential action execution (Transfer -> Attack -> Move -> Create) with deterministic intra-phase shuffling.
- **Client Library**: Public C client library (`bots/c/client_lib/inc/core_lib.h`) linking with bot binaries.
- **Bot Candidate (`ex_v01`)**:
  - `bots/c/bot/my-core-bot/Makefile`: Clean compilation under `cc -Wall -Wextra -Werror -g -fsanitize=address`.
  - `bots/c/bot/my-core-bot/inc/bot.h` & `inc/tuned_params.h`: Static parameters, state structs, function prototypes.
  - `bots/c/bot/my-core-bot/src/getters.c`: Fast inline/static iterators over `game.objects`.
  - `bots/c/bot/my-core-bot/src/main.c`: Complete adaptive utility bot with:
    1. Static 0-heap memory pools (`g_unit_brains[1024]`, `g_pairs[256]`, `g_reserved_tiles[20][20]`).
    2. Shared per-tick destination reservation table eliminating `move: invalid target position` collisions.
    3. Strict action readiness guard (`action_cooldown <= 0`) and standing cooldown timer awareness.
    4. Immediate turn exit (`continue;`) after `transfer_gems` preventing cooldown violation in move phase.
    5. Dynamic component cost evaluation ($20 + \sum \text{comp.cost}$) and AST unit design validation ($AC \ge \text{damageUnit} + \text{damageCore} + \text{damageObject}$, $\text{armor} \le 4$).
    6. Goalkeeper anchor choke hold at `(2, 2)` or `(17, 17)` exploiting rushers' 0 unit-damage.
    7. Dynamic resource harvesting with persistent state latching at 35 gems (SEEK -> MINE -> RETURN -> DEPOSIT).
    8. Corner-aware siege breakers (max 2 active cardinal attackers at enemy core, 1 staging reserve).
    9. High-resolution tick latency telemetry via `clock_gettime(CLOCK_MONOTONIC)`.
- **Opponent League**: 6 opponent archetypes in `bots/c/bot/`:
  1. `opponent-rush`: Core attacker swarm (`core_attack` + `health`).
  2. `opponent-economy`: 4-harvester rapid expansion (`mining` + `unit_attack`).
  3. `opponent-brawler`: Heavy skirmisher (`unit_attack` + 2x `health`).
  4. `opponent-turtle`: Defensive bunker with late-game push (`unit_attack` + `armor`).
  5. `gridmaster`: Baseline bot.
  6. `strategy-bot`: Optimized rush policy bot.
- **Scrimmage & Evaluation Harness**:
  - `scripts/scrimmage.py`: Upgraded to capture bot stdout and stderr, parse `replays/replay_latest.json` for authoritative outcomes, detect any action failures in `[begin_errs]`, verify clean ASan exit (0 leaks), parse `[LATENCY]` metrics (<0.5ms average), and run automated 24-match league series (`--league --games 4`).

---

## Feature Inventory

Every feature from the Survey phase is mapped to a specific milestone.

| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | Scrimmage Stdout/Stderr Capture | Capture bot stdout and stderr in `scrimmage.py` instead of `DEVNULL` | M1 | Harness Survey §1.1 |
| 2 | Scrimmage ASan Leak Assertion | Verify clean exit and 0 leak errors on bot stderr | M1 | Harness Survey §1.1 |
| 3 | Scrimmage Action Failure Detection | Parse stdout and `replays/replay_latest.json` `[begin_errs]` for 0 failures | M1 | Harness Survey §1.2 |
| 4 | Scrimmage Authoritative Replay Parsing | Extract match winner, place, death reasons, and ticks from `replays/replay_latest.json` | M1 | Harness Survey §1.2 |
| 5 | Scrimmage Tick Latency Tracking | Parse bot `[LATENCY]` output and assert avg < 0.5 ms | M1 | Harness Survey §1.4 |
| 6 | Automated 24-Match League Runner | Implement `--league --games 4` across all 6 opponents with isolated ports | M1 | Harness Survey §2 |
| 7 | Tile Destination Reservation | Cross-unit reservation table `g_reserved_tiles[20][20]` to prevent move collisions | M2 | Bot Survey §1.2 |
| 8 | Negative Cooldown Action Readiness | Guard action readiness with `action_cooldown <= 0` | M2 | Spec Miner §1.1 |
| 9 | Transfer Cooldown Reset Safety | Immediate turn exit after `transfer_gems` so move is never issued on same tick | M2 | Spec Miner §1.2 |
| 10 | Dynamic Economy Price Calculation | Calculate creation cost dynamically: $20 + \sum \text{comp.cost}$ | M2 | Spec Miner §1.4 |
| 11 | Unit Design Invalidation Guard | Enforce $AC \ge \text{damageUnit} + \text{damageCore} + \text{damageObject}$ and $\text{armor} \le 4$ | M2 | Spec Miner §1.3 |
| 12 | Delayed Dead Object Move Prevention | Prevent moving into tile of enemy/wall killed on same tick | M2 | Spec Miner §1.5 |
| 13 | High-Resolution Latency Measurement | Embed `clock_gettime(CLOCK_MONOTONIC)` inside `ft_on_tick()` | M2 | Harness Survey §1.4 |
| 14 | 0-Heap Static Allocation in Tick Loop | Eliminate `malloc` in `run_harvesters` (`main.c:227`), replace with static pools | M2 | Bot Survey §1.3 |
| 15 | Static Array Sizing & Bounds Checking | Bounds-checked static buffers (`g_pairs[256]`, `g_unit_brains[1024]`) | M2 | Bot Survey §4 |
| 16 | ASan Compliance & Leak-Free Exit | Ensure 0 bytes leaked under `-fsanitize=address` across 20+ ticks | M2 | Bot Survey §1.1 |
| 17 | Persistent State Latched Economy | Harvester 4-state machine (SEEK, MINE, RETURN, DEPOSIT) latched at 35 gems | M2 | Bot Survey §4 |
| 18 | Goalkeeper Anti-Rush Choke Defense | Dedicated unit at choke point (2,2)/(17,17) with zero unit-damage exploit | M2 | Bot Survey §1.5 |
| 19 | Corner-Aware Siege Breakers | Max 2 cardinal attackers on enemy corner core, 1 staging reserve | M2 | Bot Survey §4 |
| 20 | Pre-Move Combat Strike | Attack adjacent enemy in Phase 1 before enemy moves in Phase 2 | M2 | Spec Miner §1.2 |
| 21 | League Benchmarking (24 Matches) | Execute 4 matches each vs 6 opponents, achieving >=85% win rate | M3 | Harness Survey §2 |
| 22 | Manifest: docs/STATUS.md | Document active champion, build/test commands, win/loss records | M3 | Harness Survey §1.5 |
| 23 | Manifest: docs/MECHANICS.md | Verified engine rules with exact source line citations | M3 | Spec Miner §1 |
| 24 | Manifest: docs/EXPERIMENTS.md | Document hypothesis, test setup, sample sizes, and results | M3 | Harness Survey §1.5 |
| 25 | Manifest: docs/KNOWN_FAILURES.md | Document failure analysis, edge cases, and resolution verification | M3 | Harness Survey §1.5 |
| 26 | Forensic Integrity Audit | Independent verification of zero hardcoding, zero cheating, authentic logic | M4 | Orchestrator Protocol |

---

## Milestones

| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Scrimmage Harness Upgrades | `scripts/scrimmage.py`: stdout/stderr capture, replay parsing, 0 action failure detection, ASan verification, latency logging, 24-match league suite | none | IN_PROGRESS |
| M2 | Candidate ex_v01 Implementation | `bots/c/bot/my-core-bot/`: action safety, 0-heap static memory, latched economy, goalkeeper defense, siege breakers, dynamic pricing, AST validation, latency timing | M1 | PLANNED |
| M3 | League Benchmarking & Manifests | Run full 24-match evaluation suite, achieve >=85% win rate, update docs/STATUS.md, MECHANICS.md, EXPERIMENTS.md, KNOWN_FAILURES.md | M1, M2 | PLANNED |
| M4 | Forensic Integrity Audit & Attestation | Independent forensic audit (`teamwork_preview_auditor`) verifying zero hardcoding, clean ASan, 0 action failures, valid strategy | M3 | PLANNED |

---

## Interface Contracts

### Harness ↔ Bot Telemetry Contract
- **Stdout Telemetry**:
  - `[LATENCY] avg=X.XXXX ms, max=X.XXXX ms, ticks=N` emitted upon bot termination or every 100 ticks.
  - No action failures printed.
- **Stderr Telemetry**:
  - AddressSanitizer output stream. Must be completely empty or contain no `LeakSanitizer` / `AddressSanitizer` error blocks.
- **Replay Telemetry**:
  - `replays/replay_latest.json`: `misc.team_results[place]` must indicate `0` for Team 42.
  - Zero `[begin_errs]` in `objects.debug_info` for Team 42 units.

### Bot Internal Module Contracts
- **`g_reserved_tiles[20][20]`**: Reset at start of `ft_on_tick()` to static obstacles (walls, cores, deposits). Each unit calls `safe_issue_move()`; if target tile is reserved, unit holds or recalculates.
- **`g_unit_brains[MAX_TRACKED_UNITS]`**: Static array indexed by unit ID mod 1024. Persists harvester state (`H_STATE_SEEK`, `MINE`, `RETURN`, `DEPOSIT`), target resource ID, and position history.
- **`safe_issue_transfer()`**: Issues `core_action_transferGems()` and sets `unit_acted = true`. Calling loop immediately `continue;`s so no move or attack is emitted on that tick.

---

## Code Layout
- `bots/c/bot/my-core-bot/`:
  - `Makefile`: Compiler flags `-Wall -Wextra -Werror -g -fsanitize=address`, LDFLAGS `-lm`.
  - `inc/bot.h`: Struct definitions, prototypes, static buffer bounds.
  - `inc/tuned_params.h`: Dynamic pricing constants, roles, choke point coords.
  - `src/getters.c`: Inline/direct object getters.
  - `src/main.c`: Full candidate `ex_v01` state machine and tick loop.
- `archive/models/v2_goalkeeper_championship/`: Preserved untouched previous champion.
- `scripts/scrimmage.py`: Upgraded evaluation and league runner.
- `docs/`: `STATUS.md`, `MECHANICS.md`, `EXPERIMENTS.md`, `KNOWN_FAILURES.md`.
