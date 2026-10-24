# Bot Architecture Survey & Candidate ex_v01 Blueprint Report

## 1. Observation

### 1.1 Inspection of `bots/c/bot/my-core-bot/`
- **Makefile** (`bots/c/bot/my-core-bot/Makefile:6-10`):
  ```makefile
  CC ?= cc
  CFLAGS ?= -Wall -Wextra -Werror -g -fsanitize=address
  LDFLAGS ?= -lm
  CLIENT_LIB_DIR ?= $(if $(wildcard /core/inc/core_lib.h),/core,$(if $(wildcard ../../client_lib/inc/core_lib.h),../../client_lib,/core))
  INC = -I inc -I $(CLIENT_LIB_DIR)/inc
  EXEC = bot
  ```
  Verified by tool command: `cd bots/c/bot/my-core-bot && make re` completed with exit code 0 and 0 warnings / 0 errors under `cc -Wall -Wextra -Werror -g -fsanitize=address`.
- **Modules & Source Architecture**:
  - `inc/bot.h:1-16`: Object filter query prototypes (`ft_get_core_own`, `ft_get_core_opponent`, `ft_get_gems_nearest`, `ft_get_deposit_nearest`, `ft_get_deposit_gems_nearest`, `ft_get_units_opponent_nearest`, `ft_get_units_own`, `ft_get_units_opponent`).
  - `inc/tuned_params.h:1-35`: Static hyperparameters for unit blueprints (`HARVESTER_COST` 35, `INTERCEPTOR_COST` 47, `BREAKER_COST` 47, `DEFENSE_PANIC_RADIUS` 14, Lanchester focus weights).
  - `src/getters.c:1-80`: Predicate helpers wrapping `core_get_obj_filter_nearest()` and `core_get_objs_filter()`.
  - `src/main.c:1-568`: Implements global greedy bipartite harvester allocator, goalkeeper/hunter tactical division, breaker siege wave, and sequential spawner.

### 1.2 Root Cause Tracing of Prototype Action Failures
Direct inspection of engine source code and empirical match execution logs revealed the following failure mechanisms:
1. **Action Readiness (`action_cooldown <= 0`)**:
   - `server/src/object/Unit.cpp:34-50`:
     ```cpp
     void Unit::tickActionCooldown() {
         ...
         action_cooldown_--;
         if (-action_cooldown_ > static_cast<long long>(unitStandingCooldownLimit))
             action_cooldown_ = unitStandingCooldownLimit;
     }
     ```
   - When idle, `action_cooldown_` decrements into negative values down to `-unitStandingCooldownLimit` (-50).
   - In `server/src/action/MoveAction.cpp:31`, `server/src/action/AttackAction.cpp:40`, and `server/src/action/TransferGemsAction.cpp:123`, the check is strictly `if (unit->getActionCooldown() > 0) return "...";`.
   - Any client check using `action_cooldown == 0` causes idle-ready units with negative values to stall. The guard must always be `action_cooldown <= 0`.
2. **Transfer Cooldown Reset Violation**:
   - `server/src/action/TransferGemsAction.cpp:128`:
     ```cpp
     srcUnit->setBalance(srcUnit->getBalance() - amount_);
     srcUnit->resetActionCooldown();
     ```
   - Server resolves actions in 5 distinct phases (`server/src/game/Game.cpp:211`): Phase 1 (`transfer_gems`) $\rightarrow$ Phase 2 (`build`) $\rightarrow$ Phase 3 (`attack`) $\rightarrow$ Phase 4 (`move`) $\rightarrow$ Phase 5 (`create`).
   - In `bots/c/bot/my-core-bot/src/main.c:339-344` (`run_interceptors`) and `448-452` (`run_breakers`):
     ```c
     if (own_core && f->s_unit.gems > 0 && manhattan_dist(f->pos, own_core->pos) <= 1)
     {
         core_action_transferGems(f, own_core->pos, f->s_unit.gems);
     }
     if (f->s_unit.action_cooldown > 0) continue;
     ```
     Because client memory does not reflect the server's immediate reset until the next tick, `f->s_unit.action_cooldown > 0` evaluates to false. The unit proceeds to issue `try_adjacent_combat_strike()` or `core_action_travel()`.
     On the server, Phase 1 executes transfer and sets unit cooldown to 3. When Phase 4 executes the queued `move`, the server rejects it with:
     `"transfer_gems: unit is on action cooldown"`.
3. **Collision on Occupied Tiles (`move: invalid target position. should be empty`)**:
   - `server/src/action/MoveAction.cpp:37`:
     ```cpp
     if (Board::instance().getObjectAtPos(target_)) return "invalid target position. should be empty";
     ```
   - Direct execution of `my-core-bot` vs `strategy-bot` generated 20+ verbatim failure logs in a single match:
     ```
     Tick 122: Action Failure: move: invalid target position. should be empty ({"type":"move","unit_id":170,"x":4,"y":1})
     Tick 124: Action Failure: move: invalid target position. should be empty ({"type":"move","unit_id":142,"x":14,"y":15})
     Tick 125: Action Failure: move: invalid target position. should be empty ({"type":"move","unit_id":160,"x":5,"y":1})
     Tick 126: Action Failure: move: invalid target position. should be empty ({"type":"move","unit_id":160,"x":4,"y":0})
     Tick 129: Action Failure: move: invalid target position. should be empty ({"type":"move","unit_id":176,"x":2,"y":2})
     ...
     ```
   - In `bots/c/client_lib/src/public/action_travel.c:286-290`, `core_action_travel()` checks `obstacle = core_get_obj_from_pos(next_pos)`. If `obstacle == NULL` during the client tick, it calls `core_action_move(unit, next_pos)`. Because multiple friendly units calculate their moves independently without a shared reservation table, two or more allies select the same vacant tile. On the server, the first unit occupies the tile, causing the second unit's move to fail.
4. **Invalid Create Commands (`createUnit: illegal unit configuration`)**:
   - `bots/configs/fake_demo/game.config.json:132-175`:
     ```json
     "invalidConditions": [
         {
             "message": "Your action cooldown may not be less than your total attacks added together",
             "condition": {
                 "type": "less_than",
                 "1": { "type": "property", "property": "baseActionCooldown" },
                 "2": { "type": "sum", ... "damageUnit" + "damageCore" + "damageObject" }
             }
         },
         {
             "message": "You may not have more than 4 armor components",
             "condition": { "type": "greater_than", ... "value": 4 }
         }
     ]
     ```
   - `server/src/action/CreateAction.cpp:111-112`: Calls `ComponentLogic::getInvalidConditionMessage()`.
   - Combining `speed` (-2 cooldown, leaving `baseActionCooldown` = 1) with any attack component (+3 damage) violates $1 \ge 3$, triggering immediate server rejection.
   - Core balance must also satisfy: `own_core->s_core.gems >= unitCost` and `own_core->s_core.spawn_cooldown == 0`.

### 1.3 Memory Management & Heap Allocations
- In `bots/c/bot/my-core-bot/src/main.c:227` (`run_harvesters`):
  ```c
  pairs = (t_pair_score *)malloc(sizeof(t_pair_score) * pair_cap);
  ...
  free(pairs);
  ```
  Every tick, `run_harvesters()` dynamically allocates the bipartite scoring matrix on the heap.
- Additionally, `core_get_objs_filter()` and `core_get_units_byName()` in `client_lib/src/public/get.c:30-45` call `malloc` to return `t_obj **` pointer arrays.

### 1.4 Execution Speed Profile
- Measured empirically across a complete 546-tick match:
  - Total wall time: 4.449 seconds (including server physics, 2 bot clients, IPC sockets, JSON serialization/parsing).
  - Round-trip IPC average per tick: 8.149 ms.
  - Internal bot tick execution time: `ft_on_tick()` consists of 400-node Dijkstra searches (10–25 µs each for 4–8 units) and direct static matrix evaluation. Average execution time within `ft_on_tick()` is **0.05 ms to 0.15 ms**, strictly meeting the `< 0.5 ms` requirement.

### 1.5 Inspection of `archive/models/v2_goalkeeper_championship/`
- Contains identical source files to the active champion: `main.c` (16,317 bytes) and `tuned_params.h` (1,089 bytes).
- Architecture strengths:
  - **Goalie Anchor Choke Hold**: Unit 0 anchors at `(2, 2)` or `(17, 17)`. Intercepts rushers with `damage_core > 0` or within `DEFENSE_PANIC_RADIUS` (14 tiles).
  - **Zero-Damage Immunity**: Rush bots (`strategy-bot`'s `StrategyHealthy`, `opponent-rush`) have `damage_unit == 0`. Under `AttackAction.cpp:93-94`, units take 0 damage from core rushers, allowing the Goalie to defeat rushers in 3 hits with 0 damage sustained.
  - **35-Gem Spawning Tempo**: Pure miner (`mining` only) costs 35 gems and returns at 35 gems (1 gem pile), spawning the first combat unit at tick 10 instead of tick 30.
- Limitations to evolve for `ex_v01`:
  - Lacks friendly tile reservation (causing tile collisions).
  - Leaks transfer cooldown on fighters/breakers.
  - Dynamic bipartite allocation uses `malloc`.
  - Static cost constants instead of dynamic config calculation.

### 1.6 Inspection of Benchmark Bots & Opponent League
Inspected binaries, source code, and build configurations in `bots/c/bot/`:
1. `gridmaster` (`bots/c/bot/gridmaster/src/main.c:19-27`):
   - Spawns "Gridfighter" with legacy component names ("combat", "siege", "health") which fail under `fake_demo` config. Issues naive single-file `travel` to opponent core.
2. `strategy-bot` (`bots/c/bot/strategy-bot/src/main.c` & `strategy.c`):
   - Parameter-optimized policy bot. Produces `StrategyMiner` (35 gems), `StrategyGuard` (47 gems), and rushes `StrategyHealthy` (`core_attack` + `health`, 47 gems) directly to enemy core.
3. `opponent-rush` (`bots/c/bot/opponent-rush/src/main.c`):
   - Relentless core rush swarm. Spawns `RushAttacker` (`core_attack` + `health`, 47 gems) and `RushLight` (`core_attack`, 35 gems).
4. `opponent-economy` (`bots/c/bot/opponent-economy/src/main.c`):
   - Rapid expansion. Spawns up to 4 `EcoMiner` units (35 gems each) before producing defenders (`EcoDefender`, 47 gems).
5. `opponent-brawler` (`bots/c/bot/opponent-brawler/src/main.c`):
   - Heavy skirmisher. Spawns `HeavyBrawler` (`unit_attack` + 2x `health`, 59 gems, 11 HP) and `Brawler` (47 gems). Seeks out and engages friendly units.
6. `opponent-turtle` (`bots/c/bot/opponent-turtle/src/main.c`):
   - Defensive bunker. Spawns `ArmoredTurtle` (`unit_attack` + `armor`, 65 gems) and `GuardTurtle` (47 gems). Holds tight perimeter at radius 2-3 around friendly core.

---

## 2. Logic Chain

1. **Premise 1**: Acceptance Criteria mandates **exactly 0 action failures** across all evaluation matches (0 transfer cooldown violations, 0 move-onto-occupied errors, 0 invalid create commands).
2. **From 1.2 (Action Readiness)**: Server allows actions when `unit->getActionCooldown() <= 0`. Because idle units decrement below 0 down to -50, guarding with `action_cooldown <= 0` is strictly necessary to prevent stalling.
3. **From 1.2 (Transfer Reset)**: Because `server/src/action/TransferGemsAction.cpp:128` resets unit cooldown to `baseActionCooldown` on the server in Phase 1, any unit issuing `core_action_transferGems()` must immediately terminate its turn (`continue;`) so no subsequent `move` or `attack` is emitted in Phase 3 or 4.
4. **From 1.2 (Collision on Occupied Tiles)**: Empirical verification proved that `core_action_travel()` emits `move` to empty tiles without cross-unit synchronization, causing 20+ collisions per match. Therefore, `ex_v01` must maintain a shared per-tick `reserved_tiles[20][20]` table. Any planned destination must be reserved; if a tile is already reserved by an ally, the unit must select an alternate path or hold position.
5. **From 1.2 (Component Validation)**: Server rule `baseActionCooldown >= (damageUnit + damageCore + damageObject)` forbids `speed` with any attack component. Dynamic price calculation must evaluate configuration properties dynamically rather than relying on hardcoded constants.
6. **From 1.3 (Memory Safety)**: Acceptance criteria requires 0 bytes leaked under AddressSanitizer and 0 heap allocations in per-tick tracking loops. Replacing dynamic `malloc(sizeof(t_pair_score) * pair_cap)` with a static buffer (`static t_pair_score g_pairs[256];`) and iterating `game.objects` directly eliminates all dynamic allocations from the tick loop.
7. **From 1.5 & 1.6 (Strategy Synthesis)**:
   - The Goalie / Hunter division from model v2 completely counters rush strategies because rushers deal 0 damage to units (`damage_unit == 0`).
   - Corner core geometry at `(19, 19)` offers only 2 cardinal attack tiles (`(18, 19)` and `(19, 18)`). Limiting active siege breakers to 2–3 units and holding staging reserves prevents deadlocks.
   - For economy, persistent state latching (SEEK $\rightarrow$ MINE $\rightarrow$ RETURN $\rightarrow$ DEPOSIT) latches return once 35 gems are gathered, eliminating target switching and distance oscillations.

---

## 3. Caveats

1. **Network & Dynamic Port Isolation**: Rapid consecutive server invocations can encounter port `TIME_WAIT` socket errors if ports are reused within 60 seconds. `scripts/scrimmage.py` must use dynamic ports (`base_port + match_index`) and clean child processes via PID or port unbinding rather than unreliable `pkill -f` regexes.
2. **`core_lib` Stdin Handling**: `client_lib/src/con_lib.c:19` checks `isatty(STDIN_FILENO)`. When running under automated test scripts with `DEVNULL` or pipes, the prompt returns immediately without hanging.
3. **Server Action Shuffling**: In `server/src/game/Game.cpp:211`, `shuffle_actions_vector(actions, seed + tick)` randomly shuffles action order within each phase. Therefore, friendly units must never rely on server execution ordering to resolve contested tiles; collision avoidance must be fully enforced on the client side before emitting actions.

---

## 4. Conclusion & Architectural Recommendation for Candidate `ex_v01`

To achieve 100% win rate, 0 action failures, and 0 heap allocations, candidate `ex_v01` should be structured as follows:

```
bots/c/bot/my-core-bot/
├── Makefile               # cc -Wall -Wextra -Werror -g -fsanitize=address
├── inc/
│   ├── bot.h              # Declarations and shared types
│   └── tuned_params.h     # Dynamic configs, roles, thresholds
└── src/
    ├── getters.c          # Fast direct iterators over game.objects
    └── main.c             # Unified ex_v01 state engine
```

### Detailed Component Blueprints for `ex_v01`

```c
/* ========================================================================== */
/* 1. ZERO-ALLOCATION STATIC MEMORY ARCHITECTURE                              */
/* ========================================================================== */
#define MAX_TRACKED_UNITS 1024
#define MAX_TRACKED_RES   256
#define MAX_PAIRS         256

typedef enum e_harvester_state {
    H_STATE_SEEK = 0,
    H_STATE_MINE,
    H_STATE_RETURN,
    H_STATE_DEPOSIT
} t_harvester_state;

typedef struct s_unit_brain {
    t_harvester_state h_state;
    unsigned long target_id;
    t_pos last_pos;
    unsigned int stuck_ticks;
} t_unit_brain;

static t_unit_brain g_unit_brains[MAX_TRACKED_UNITS];
static t_pair_score g_pairs[MAX_PAIRS];
static bool         g_reserved_tiles[20][20];

/* ========================================================================== */
/* 2. STRICT ACTION SAFETY & TILE RESERVATION                                 */
/* ========================================================================== */
static void reset_tick_reservations(void) {
    memset(g_reserved_tiles, 0, sizeof(g_reserved_tiles));
    /* Mark positions of all currently static objects (cores, walls, deposits) */
    for (size_t i = 0; game.objects && game.objects[i]; i++) {
        t_obj *o = game.objects[i];
        if (o->type == OBJ_CORE || o->type == OBJ_WALL || o->type == OBJ_DEPOSIT) {
            if (o->pos.x < 20 && o->pos.y < 20) g_reserved_tiles[o->pos.x][o->pos.y] = true;
        }
    }
}

static bool safe_issue_move(t_obj *unit, t_pos dest) {
    if (dest.x >= 20 || dest.y >= 20) return false;
    if (g_reserved_tiles[dest.x][dest.y]) return false;
    core_action_move(unit, dest);
    g_reserved_tiles[dest.x][dest.y] = true;
    return true;
}

/* ========================================================================== */
/* 3. DYNAMIC ECONOMY WITH PERSISTENT STATE LATCHING                          */
/* ========================================================================== */
/*
** Harvesters execute a 4-state latched cycle:
** 1. H_STATE_SEEK: Assigned optimal resource via static bipartite matrix.
** 2. H_STATE_MINE: Approaches and harvests until gems >= 35 or target depleted.
** 3. H_STATE_RETURN: Latched return to Core. Will NOT oscillate back to mining.
** 4. H_STATE_DEPOSIT: At Manhattan distance 1 from Core: calls transfer_gems,
**    sets unit_acted = true, and DOES NOT issue move/travel on that tick.
*/
static void run_harvesters_ex_v01(t_obj *own_core, t_obj **enemies);

/* ========================================================================== */
/* 4. GOALKEEPER & HUNTER ANTI-RUSH DEFENSE                                   */
/* ========================================================================== */
/*
** - Unit 0: Dedicated Goalie holding anchor choke point (2, 2) or (17, 17).
** - Priority 1: Intercept any enemy carrying damage_core > 0 or within 14 tiles.
** - Exploit: Core rushers carry damage_unit == 0, dealing 0 damage to our Goalie.
** - Pre-Move Strike: If enemy adjacent (Manhattan dist == 1), attack in Phase 3
**   before target moves in Phase 4.
*/
static void run_interceptors_ex_v01(t_obj *own_core, t_obj *enemy_core, t_obj **enemies);

/* ========================================================================== */
/* 5. CORNER-AWARE SIEGE BREAKERS                                             */
/* ========================================================================== */
/*
** - Spawns Breakers (core_attack + health, 47 gems).
** - Corner Core Geometry: Corner at (19, 19) only has 2 adjacent cardinal tiles.
** - At most 2 Breakers occupy cardinal attack tiles; 3rd Breaker holds staging
**   tile to avoid blocking friendly movement.
*/
static void run_breakers_ex_v01(t_obj *own_core, t_obj *enemy_core, t_obj **enemies);
```

---

## 5. Verification Method

To independently verify these findings and benchmark candidate `ex_v01`:

1. **Compilation & Memory Safety**:
   ```bash
   cd /home/malshare/Evaluations/Core_game/bots/c/bot/my-core-bot
   make re
   ```
   *Expected output*: 0 errors, 0 warnings under `-Wall -Wextra -Werror -g -fsanitize=address`.

2. **Action Failure Trace Verification**:
   Inspect the stderr and stdout of `bot` during a match:
   ```bash
   python3 -c "
   import scripts.scrimmage as sc
   winner, out = sc.run_single_match('./bots/c/bot/my-core-bot/bot', './bots/c/bot/strategy-bot/strategy-bot', port=4580)
   print('Result:', winner)
   "
   ```
   *Pass criterion*: Exactly 0 lines containing `"Action Failure:"`.

3. **Full League Benchmark Harness**:
   Execute the automated league series against all 6 opponents:
   ```bash
   python3 scripts/scrimmage.py --league --games 5 --base-port 4600
   ```
   *Pass criteria*:
   - Total matches $\ge 20$ (e.g. 5 matches $\times$ 6 opponents = 30 matches).
   - Win rate $\ge 85\%$ across all archetypes.
   - 0 AddressSanitizer memory leaks across all games.
