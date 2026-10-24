# Original User Request

## Initial Request — 2026-10-24T07:41:15Z

Build and benchmark an adaptive C bot (ex_v01) in bots/c/bot/my-core-bot/ that eliminates all prototype action failures, complies strictly with inspected engine cooldown and unit design rules, and validates performance across a league of diverse opponent styles.

Working directory: /home/malshare/Evaluations/Core_game
Integrity mode: development

## Requirements

### R1. Bot Implementation (ex_v01)
Implement the C bot candidate ex_v01 in bots/c/bot/my-core-bot/ (keeping previous champion archived in archive/models/v2_goalkeeper_championship/).
The bot must execute an adaptive utility-based strategy layer combining:
- Dynamic resource harvesting with persistent state latching (no oscillation).
- Early defense against rushes with coordinated focus fire.
- Adaptive siege breakers to finish damaged opponent cores.
- Static memory management with 0 heap allocations in per-tick tracking loops.

### R2. Strict Action Safety & Cooldown Conformance
Eliminate all preventable action failures by strictly adhering to verified engine physics:
- Action Readiness: Allow actions when action_cooldown <= 0 (handling idle-ready negative values).
- Transfer Reset: Never issue move or travel on the same tick as transfer_gems, as transfer immediately resets unit cooldown on the server.
- Unit Design Validation: Enforce baseActionCooldown >= (damageUnit + damageCore + damageObject) so illegal combinations (such as speed + attack) are never emitted.
- Accurate Economy Prices: Compute true component costs dynamically rather than relying on static placeholders.

### R3. Opponent League & Automated Evaluation Harness
Establish a local test league with 4 distinct opponent archetypes in bots/c/bot/:
1. Rush: Immediate aggressive core attacker swarm.
2. Economy: Rapid multi-harvester expansion.
3. Brawler: Heavy unit-combat escort and skirmisher.
4. Turtle: Defensive bunker with late-game push.
Incorporate existing benchmarks (gridmaster and strategy-bot) into the test suite. Ensure matches run with isolated ports/processes via scripts/scrimmage.py.

### R4. Documentation & Verification Manifests
Maintain progress checkpoints in docs/:
- docs/STATUS.md: Active champion, exact build/test commands, win/loss tallies.
- docs/MECHANICS.md: Verified engine rules with source line citations.
- docs/EXPERIMENTS.md: Log of each hypothesis, test results, and sample sizes.
- docs/KNOWN_FAILURES.md: Trace of any remaining edge-case action failures or losses.

## Acceptance Criteria

### Compilation & Memory Safety
- [ ] cd bots/c/bot/my-core-bot && make re compiles cleanly with -Wall -Wextra -Werror (0 errors, 0 warnings).
- [ ] Memory safety: 0 bytes leaked under AddressSanitizer across 20+ continuous ticks and multiple matches.

### Action Integrity
- [ ] Exactly 0 action failures logged across all evaluation matches (0 transfer cooldown violations, 0 move-onto-occupied errors, 0 invalid create commands).

### Competitive Performance
- [ ] >= 85% win rate across a minimum 20-match evaluation suite against the 4 opponent archetypes, gridmaster, and strategy-bot.
- [ ] Execution speed strictly < 0.5 ms average per tick (well within the 2 ms tournament limit).
