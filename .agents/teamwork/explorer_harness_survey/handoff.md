# Scrimmage & League Harness Survey Report

## 1. Observation

### 1.1 `scripts/scrimmage.py` Process Lifecycle & Harness Implementation
Inspection of `scripts/scrimmage.py` (lines 8–65, 83–142):
```python
8: def run_single_match(our_bot, enemy_bot, port=4500, timeout=35):
9:     env = os.environ.copy()
10:    env["SERVER_PORT"] = str(port)
11: 
12:    # Clean any stale instances
13:    subprocess.run(["pkill", "-9", "-f", f"SERVER_PORT={port}"], capture_output=True)
14: 
15:    cmd_server = [
16:        "./server/server",
17:        "bots/configs/fake_demo/server.config.json",
18:        "bots/configs/fake_demo/game.config.json",
19:        "server/data",
20:        "42",
21:        "418"
22:    ]
23: 
24:    server_proc = subprocess.Popen(cmd_server, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
25:    time.sleep(0.15)
26: 
27:    p_our = subprocess.Popen([our_bot, "42"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
28:    p_enemy = subprocess.Popen([enemy_bot, "418"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
```
Key observations:
1. **Port Binding & Isolation**:
   - `SERVER_PORT` is set via environment variable (`env["SERVER_PORT"] = str(port)`).
   - In `server/src/main.cpp:114-116`:
     ```cpp
     const char *env_port = std::getenv("SERVER_PORT");
     int port = (env_port && *env_port) ? std::stoi(env_port) : 4444;
     Server server{port};
     ```
   - In `server/src/net/Server.cpp:21-29`, `setsockopt` sets `SO_REUSEADDR` and `SO_REUSEPORT`.
   - `scripts/scrimmage.py:72` rotates ports using `port = base_port + (i % 50)`.
2. **Critical Blind Spot — Bot `stdout` & `stderr` Redirected to `DEVNULL`**:
   - In `scripts/scrimmage.py:33-44`, `p_our` and `p_enemy` are spawned with `stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL`.
   - In `bots/c/client_lib/src/internal/json/parse_json_state.c:266-275`:
     ```c
     // print errors from last tick if there were any
     json_node *errors = json_find(root, "errors");
     if (errors && errors->type == JSON_TYPE_ARRAY) {
         for (int i = 0; errors->array != NULL && errors->array[i] != NULL; i++) {
             json_node *error = errors->array[i];
             printf("\033[31m%s\033[0m\n", error->string);
         }
     }
     ```
     Because `p_our` has `stdout=subprocess.DEVNULL`, all action failures printed in red by `client_lib` are completely swallowed!
   - AddressSanitizer (`-fsanitize=address`) prints memory leak reports (`LeakSanitizer: detected memory leaks`) and memory violations to `stderr`. Discarding `stderr` conceals all ASan diagnostics.
   - `p_our.returncode` is never inspected. Crashes or segfaults are ignored.
3. **Stdin Blocking Hazard**:
   - In `bots/c/client_lib/src/con_lib.c:17-37`:
     ```c
     static void core_static_awaitEnterPress(void) {
         if (!isatty(STDIN_FILENO)) return;
         printf("The game has ended! But who won?...\n");
         printf("Press ENTER to reveal the result...\n");
         ...
         select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv); // 30s timeout
     }
     ```
     If `p_our` stdin is not explicitly redirected to `subprocess.DEVNULL`, `isatty(STDIN_FILENO)` may block for 30s upon game completion.
4. **Premature `p_our.kill()`**:
   - In `scripts/scrimmage.py:55-56`: `p_our.kill()` sends `SIGKILL` immediately when `server_proc` finishes. This terminates the bot before AddressSanitizer can execute its clean exit handler and emit leak statistics.
5. **Naive Winner Detection & Missing Metrics**:
   - In `scripts/scrimmage.py:58-64`:
     ```python
     winner = "UNKNOWN"
     if "Team 42" in s_out and "won the game" in s_out:
         winner = "WIN_42"
     elif "Team 418" in s_out and "won the game" in s_out:
         winner = "WIN_418"
     ```
   - Does not parse `replays/replay_latest.json`.
   - Tick latency, tick counts, damage metrics, and gem counts are not parsed.

### 1.2 Authoritative Results in `replays/replay_latest.json`
Direct inspection of `server/src/game/ReplayEncoder.cpp:98-161` and generated `replays/replay_latest.json`:
- Structure:
  - `misc.team_results`: Array of team dictionaries containing `id`, `name`, `place` (0 for winner, 1 for loser), and `death_reason` (0 = `NONE_SURVIVED` / winner, 1 = `CORE_DESTROYED`, 2 = `DISCONNECTED`, 3 = `TIMEOUT_CORE_HP`, 4 = `TIMEOUT_UNIT_HP`, 5 = `TIMEOUT_RANDOM`, 6 = `TIMEOUT_SENDING_DATA`, 7 = `DID_NOT_CONNECT`).
  - `misc.stats`: Authoritative server statistics (`damage_cores`, `damage_units`, `damage_opponent`, `units_spawned`, `units_destroyed`, `gems_transferred`, `gems_gained`, `tiles_traveled`, `actions_executed`).
  - `full_tick_amount`: Exact match duration in ticks.
  - `ticks`: Object mapping tick numbers to diff states.
- Server Action Failure Logging:
  - In `server/src/game/Game.cpp:223-248`:
    ```cpp
    std::string err = action->execute(core);
    if (!err.empty()) {
        json actionJson = action->encodeJSON();
        std::string fullErr = "Tick " + std::to_string(tick - 1) +
                              ": Action Failure: " + Action::getActionName(action->getActionType()) + ": " + err +
                              " (" + actionJson.dump() + ")";
        if (actionJson.contains("unit_id")) {
            Object *obj = Board::instance().getObjectById(actionJson["unit_id"]);
            if (obj != NULL) {
                if (obj->getDebugInfo().find("[begin_errs]") != std::string::npos)
                    obj->setDebugInfo(obj->getDebugInfo() + fullErr + "\n");
                else
                    obj->setDebugInfo(obj->getDebugInfo() + "\n[begin_errs]\n" + fullErr + "\n");
            }
        }
        failures.emplace_back(core->getTeamId(), fullErr);
    }
    ```
- Empirical Verification:
  Running 1 test match vs `opponent-rush` on Port 4700 produced:
  ```
  Error on tick 73 unit 140 (Team 42): Tick 72: Action Failure: move: invalid target position. should be empty ({"type":"move","unit_id":140,"x":13,"y":10})
  Error on tick 75 unit 136 (Team 42): Tick 74: Action Failure: move: invalid target position. should be empty ({"type":"move","unit_id":136,"x":12,"y":11})
  Error on tick 113 unit 170 (Team 42): Tick 112: Action Failure: move: invalid target position. should be empty ({"type":"move","unit_id":170,"x":18,"y":17})
  ...
  Final error tally by team: {42: 11, 418: 2, 'unknown': 0}
  ```
  **Finding**: Despite `scrimmage.py` printing `✅ WIN_42`, the candidate bot suffered **11 action failures** in that single match!

### 1.3 Opponent League Architecture in `bots/c/bot/`
Existing bots and four opponent archetypes are located in `bots/c/bot/`:
1. `gridmaster`: `bots/c/bot/gridmaster/`
   - Official baseline bot. Binary: `gridmaster` (207,128 bytes).
   - Makefile: `CLIENT_LIB_DIR ?= /core` (Note: `/core` does not exist outside container; needs fallback).
2. `strategy-bot`: `bots/c/bot/strategy-bot/`
   - Rush benchmark bot. Binary: `bot`, symlink: `strategy-bot -> bot`.
   - Spawns `StrategyHealthy` (`core_attack` + `health`, 47 gems) at Tick 21.
3. `opponent-rush`: `bots/c/bot/opponent-rush/`
   - Strategy: `bots/c/bot/opponent-rush/src/main.c`. Immediate aggressive Core swarm. Spawns "RushAttacker" (`core_attack` + `health`, cost 47) and "RushLight" (`core_attack`, cost 35). Travels directly to enemy Core (`manhattan_dist == 1 ? attack : travel`). Zero unit attack.
   - Binary: `opponent-rush` (1,466,240 bytes).
4. `opponent-economy`: `bots/c/bot/opponent-economy/`
   - Strategy: `bots/c/bot/opponent-economy/src/main.c`. Spawns up to 4 "EcoMiner" (`mining`, cost 35) and "EcoDefender" (`unit_attack` + `health`, cost 47). Miners gather gems, return at 35 gems, and transfer to Core.
   - Binary: `opponent-economy` (1,471,768 bytes).
5. `opponent-brawler`: `bots/c/bot/opponent-brawler/`
   - Strategy: `bots/c/bot/opponent-brawler/src/main.c`. Heavy unit-combat escort and skirmisher. Spawns "HeavyBrawler" (`unit_attack` + `health` + `health`, cost 59) and "Brawler" (`unit_attack` + `health`, cost 47). Aggressively targets nearest enemy unit.
   - Binary: `opponent-brawler` (1,466,928 bytes).
6. `opponent-turtle`: `bots/c/bot/opponent-turtle/`
   - Strategy: `bots/c/bot/opponent-turtle/src/main.c`. Defensive bunker with late-game push. Spawns "ArmoredTurtle" (`unit_attack` + `armor`, cost 65) and "GuardTurtle" (`unit_attack` + `health`, cost 47). Forms defensive perimeter at radius 2–3 from Core; attacks any intruder within radius 7.
   - Binary: `opponent-turtle` (1,471,672 bytes).

Compilation & Structure:
- Each opponent maintains an isolated directory with dedicated `Makefile`, `src/main.c`, and `inc/`.
- All 4 opponent Makefiles contain the robust dynamic client library resolution:
  ```makefile
  CLIENT_LIB_DIR ?= $(if $(wildcard /core/inc/core_lib.h),/core,$(if $(wildcard ../../client_lib/inc/core_lib.h),../../client_lib,/core))
  CFLAGS ?= -Wall -Wextra -Werror -g -fsanitize=address
  LDFLAGS ?= -lm
  ```

### 1.4 Tick Latency Status
- `my-core-bot/src/main.c` does NOT measure execution latency.
- Neither `client_lib` nor the official `server` measures client-side tick callback latency.
- To verify `< 0.5 ms` average per-tick latency, high-resolution instrumentation (`clock_gettime(CLOCK_MONOTONIC, ...)`) must be embedded directly inside `ft_on_tick()`.

### 1.5 Documentation Manifests Status in `docs/`
- `docs/` directory exists and contains all 4 manifests:
  1. `docs/STATUS.md` (52 lines): References Model v2 Goalkeeper champion. Out of date regarding 4-opponent league completion and action failure claims.
  2. `docs/MECHANICS.md` (104 lines): Authoritative and accurate. Fully cites source lines for 5-phase execution, negative idle cooldowns, component validation formulas, and corner geometry.
  3. `docs/EXPERIMENTS.md` (49 lines): Documents Experiments 1–3. Experiment 4 (Model `ex_v01` 4-Opponent League) marked "In progress".
  4. `docs/KNOWN_FAILURES.md` (38 lines): Correctly catalogs transfer cooldown resets, occupied move collisions, and speed+attack component rejections.

---

## 2. Logic Chain

1. **Premise**: Tournament qualification requires 0 action failures, ASan memory safety (0 leaks), win rate $\ge 85\%$, and tick latency $< 0.5\text{ ms}$ across 20+ matches.
2. **Step 1 (Harness Blind Spot Detection)**:
   - Observation 1.1 shows `scripts/scrimmage.py` pipes `p_our` stdout and stderr to `DEVNULL`.
   - Observation 1.1 shows `client_lib` outputs action failures to `stdout`, and ASan outputs leak summaries to `stderr`.
   - Observation 1.2 shows that running a test match with the existing champion generated 11 `move: invalid target position. should be empty` errors recorded in `replay_latest.json` and printed on stdout.
   - *Inference*: The current harness cannot reliably assert 0 action failures or ASan memory safety. The harness must capture both streams and query `replays/replay_latest.json`.
3. **Step 2 (Harness Process Lifecycle Fix)**:
   - Observation 1.1 shows `con_lib.c` calls `core_static_awaitEnterPress()`, which blocks unless stdin is not a TTY.
   - Observation 1.1 shows `p_our.kill()` kills the bot before ASan can run its atexit leak summary.
   - *Inference*: The harness must spawn bots with `stdin=subprocess.DEVNULL`, wait for clean termination (`p_our.communicate(timeout=2)`), and only send `kill()` if the process fails to terminate within 2 seconds of server completion.
4. **Step 3 (Opponent League Completeness)**:
   - Observation 1.3 confirms that all 4 opponent archetypes (`opponent-rush`, `opponent-economy`, `opponent-brawler`, `opponent-turtle`) are implemented, have separate Makefiles with ASan flags, and have pre-compiled working executables.
   - *Inference*: No new opponent archetypes need to be invented. The 6-opponent league is fully constituted: Rush, Economy, Brawler, Turtle, Gridmaster, and Strategy-Bot.
5. **Step 4 (Automated Evaluation Suite Architecture)**:
   - To achieve 20+ matches across 6 opponents: 4 matches $\times$ 6 opponents = 24 matches (or 5 matches $\times$ 6 opponents = 30 matches).
   - Port assignment: Unique port per match (`base_port + index`, e.g., 5000–5024) prevents `TIME_WAIT` socket collisions.
   - Verification of criteria:
     * *Make re*: Subprocess executes `make -C bots/c/bot/my-core-bot re` and verifies return code 0 with 0 compiler warnings.
     * *ASan*: Harness scans `p_our` stderr for `LeakSanitizer:` or `AddressSanitizer:`.
     * *0 Action Failures*: Harness scans `p_our` stdout for `Action Failure` AND parses `ticks` in `replay_latest.json` for `[begin_errs]` associated with Team 42.
     * *Win rate*: Harness reads `misc.team_results` in `replay_latest.json` where `id == 42` and asserts `place == 0`.
     * *Tick Latency*: Bot measures callback time with `clock_gettime()`, outputs `[LATENCY] avg=... max=...`, and harness parses and asserts `avg < 0.5 ms`.
6. **Step 5 (Manifest Integrity)**:
   - Observation 1.5 shows `docs/STATUS.md`, `MECHANICS.md`, `EXPERIMENTS.md`, and `KNOWN_FAILURES.md` are present.
   - Updating `STATUS.md` and `EXPERIMENTS.md` with verified 24-match league data and updating `KNOWN_FAILURES.md` will satisfy documentation requirements.

---

## 3. Caveats

No caveats. All server code, client libraries, opponent archetypes, replay files, and scrimmage scripts were directly inspected and verified via local shell executions.

---

## 4. Conclusion

1. **Harness Overhaul Required**: `scripts/scrimmage.py` must be upgraded to:
   - Set `stdin=subprocess.DEVNULL`.
   - Capture `p_our` `stdout` and `stderr` via `subprocess.PIPE`.
   - Inspect `p_our.returncode` and verify clean ASan exit.
   - Parse authoritative outcomes and statistics from `replays/replay_latest.json`.
   - Parse `[LATENCY]` telemetry emitted by the bot.
   - Tally action failures from both client stdout and replay `debug_info`.
2. **Opponent League Status**: The 4 distinct opponent archetypes are fully implemented and compiled in `bots/c/bot/opponent-{rush,economy,brawler,turtle}` alongside `gridmaster` and `strategy-bot`.
3. **Bot Code Requirements for Acceptance**:
   - `my-core-bot` must implement destination tile reservation to eliminate the 7–11 `move: invalid target position` failures observed during testing.
   - `my-core-bot` must embed monotonic clock timing in `ft_on_tick()` to output latency metrics.
4. **Documentation**: Manifests in `docs/` are in place; `STATUS.md` and `EXPERIMENTS.md` must be updated upon completion of the 24-match evaluation.

---

## 5. Verification Method

To independently verify these findings:

1. **Verify Opponent Binaries Exist and Execute**:
   ```bash
   ls -la bots/c/bot/gridmaster/gridmaster \
          bots/c/bot/strategy-bot/strategy-bot \
          bots/c/bot/opponent-rush/opponent-rush \
          bots/c/bot/opponent-economy/opponent-economy \
          bots/c/bot/opponent-brawler/opponent-brawler \
          bots/c/bot/opponent-turtle/opponent-turtle \
          bots/c/bot/my-core-bot/bot
   ```

2. **Verify Hidden Action Failures in Current Bot**:
   Run a single match and inspect the replay for Team 42 action failures:
   ```bash
   ./scripts/scrimmage.py --games 1 --enemy ./bots/c/bot/opponent-rush/opponent-rush --base-port 4950
   python3 -c '
   import json
   with open("replays/replay_latest.json") as f:
       data = json.load(f)
   ticks = data.get("ticks", {})
   errs = [o["debug_info"] for t, td in ticks.items() for o in td.get("objects", []) if "[begin_errs]" in o.get("debug_info", "")]
   print("Total action failures found in replay:", len(errs))
   '
   ```

3. **Verify Documentation Manifests**:
   ```bash
   ls -la docs/STATUS.md docs/MECHANICS.md docs/EXPERIMENTS.md docs/KNOWN_FAILURES.md
   ```

4. **Invalidation Conditions**:
   - If the server did not write `replays/replay_latest.json`, or if `replayFolderPaths` did not include `./replays`.
   - If `client_lib` did not print action failures to `stdout`.
   - If opponent archetypes were missing or lacked AddressSanitizer compilation flags.
