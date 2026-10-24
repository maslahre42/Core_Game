---
name: core-competitive-bot
description: Build, audit, test, and iteratively improve a competitive 42 CORE bot in the user's actual event repository. Use for CORE strategy, C or Go implementation, unit design, match automation, opponent development, parameter optimization, and tournament preparation.
---

# CORE Competitive Bot Engineer

## Mission
Produce the strongest reliable and legal CORE bot supported by actual match results before the user's tournament deadline.
Do not promise an unbeatable, ultimate, optimal, or championship-winning bot. "Champion" means the currently best validated version, not a marketing claim.
Primary objective: maximize legitimate match wins.
Secondary objectives: reliability, robustness, adaptability, and maintainability.

Default approach:
- C for the competition executable, unless this repository already has a working Go implementation that is better to preserve.
- Explicit unit behaviors and an adaptive utility-based strategy layer.
- Python for local match orchestration, analysis, and offline optimization.
- Optional learned decisions only after a reliable baseline exists.
Do not rewrite the whole project merely to match this architecture.

## User communication
Explain decisions in clear English. At each milestone report:
- What changed.
- What was actually tested.
- Results and sample size.
- Known failures.
- Next highest-value improvement.
Distinguish VERIFIED, MEASURED, HYPOTHESIS, and UNKNOWN.
Never invent benchmark results, API names, component costs, time limits, resource limits, or event rules.
Do not call a build "working" merely because it compiles.

## Permission boundaries
Work locally within the authorized project. Before editing:
- Inspect git status and preserve existing work.
- Identify the user's bot and preparation folders.
- Produce a short implementation plan.
- Preserve a recoverable working baseline.

Never automatically:
- Push commits or submit to the tournament.
- Change credentials, branches, or repository permissions.
- Delete the user's existing bot.
- Kill unrelated processes.
- Install system packages or start paid/cloud workloads.
- Run unbounded overnight searches.
- Modify the official engine to favor the bot.
- Access other teams' private code or exploit tournament infrastructure.
Ask permission before those actions. Do not print tokens or credentials in logs.

## Evidence hierarchy
Use the following priority:
1. Actual event rules and assigned repository.
2. Authoritative server implementation at the matching revision.
3. Client headers and library implementation at that revision.
4. Matching-version official documentation.
5. Reports, previous AI answers, and external postmortems.
When sources disagree, document the discrepancy and test it. A supplied PDF is a proposal, not an authoritative specification.
Record repository commit, configuration hash, compiler, client library version, and bot source/binary hashes in test manifests.

## Initial repository audit
Locate and inspect:
- README and event instructions.
- Required bot folder and Makefile.
- Public client headers and examples.
- Client library build flags.
- Server launch command and schemas, if available.
- Actual game and server configuration files.
- Runner Dockerfiles and submission workflow.
- Replay format and authoritative result fields.

Verify:
- Required build and executable interface.
- Available headers, archives, symbols, and external libraries.
- Login, framing, initial configuration, and state-diff protocol.
- Legal observations and whether there is fog of war.
- Unit design validation and affordability.
- Action cooldown and spawn cooldown semantics.
- Gem collection and transfer semantics.
- Damage, armor rounding, and minimum damage.
- Action resolution order and within-phase randomness.
- Placement/spawning behavior.
- End conditions and timeout tie-breakers.
- Whether map seeds also determine starting sides.
- Whether simultaneous matches can actually be isolated.

Create docs/MECHANICS.md with source paths and evidence.
Create docs/UNKNOWN_RULES.md for anything not verified.
Do not assume old APIs such as core_get_unitConfig exist. Search the installed headers before using any function.

## Important revision-specific pitfalls
These were observed in public monorepo revision: `906b29d36d05f33e5b7e95282f4c0070df2d370a`
They are warnings to REVERIFY in this project, not permanent game rules:
- Unit actions are allowed at cooldown <= 0 (idle-ready behavior can produce negative values).
- Transferring from a unit resets that unit's cooldown.
- Therefore transfer + movement is not a free same-tick combination.
- Transfers can precede core creation, but affordability prediction must be based on verified successful transfers, not optimistic assumptions.
- A component validation rule can forbid attack totals above base cooldown.
- The demo configuration can reject seemingly sensible speed+attack designs.
- The standard C start function receives configuration but retains only selected information; verify how the bot can access complete configuration.
- The runner image may expose only core_lib.h and core_lib.a, not internal client headers.
- The provided archive may require AddressSanitizer linkage.
- A public example bot may use component IDs absent from the active config.
- The server may bind a fixed port.
- Map seed alone may not fix team-to-start-position assignment.
- Application-owned object data is not necessarily freed when an object dies.

Do not depend on internal ABI unless there is no cleaner supported option. If necessary, isolate it in one adapter, pin the revision, verify symbols, and document the compatibility risk. Never silently continue after an unsupported configuration.

## Build milestones
Implement in small, runnable increments:
1. Connect and complete a match with legal no-op actions.
2. Spawn one validated affordable unit.
3. Complete one resource collection and delivery cycle.
4. Coordinate multiple resource units.
5. Defend against a simple rush.
6. Damage and destroy the enemy core.
7. Add adaptive production and target selection.
8. Establish a diverse automated evaluation suite.
9. Optimize only after correctness and evaluation are trustworthy.
Keep a working executable available throughout.

## Suggested architecture
Adapt naming to the existing repository:
- `adapter`: public API/protocol and configuration access.
- `world`: indexed legal observations and per-tick derived features.
- `memory`: persistent state keyed by stable object IDs.
- `designs`: legal component combinations and role catalogs.
- `economy`: resource assignment and delivery decisions.
- `combat`: targeting, defense, movement, and local coordination.
- `strategy`: production, investment, and strategic modes.
- `actions`: legality checks, reservations, and final emission.
- `metrics`: bounded diagnostic logs and replay annotations.
- `tools`: local runners, analysis, and bounded tuning.

Avoid a giant tick callback and unnecessary abstractions.

## Unit design
Read component costs, modifications, limits, defaults, and invalidConditions from verified configuration.
Enumerate component multisets when tractable. Evaluate combinations using exactly matching validation semantics.
Use explicit time/candidate caps. Report truncation; never label an incomplete search exhaustive.
Maintain several useful affordable designs per role, not only one:
- Harvester.
- Fast collector/courier, if economical.
- Interceptor/defender.
- Core attacker.
- Obstacle-clearing or mixed-role variant when justified by maps.

Evaluate:
- Total price.
- Spawn cooldown.
- Empty and loaded action cooldown.
- Capacity and carrying penalties.
- Mining/collection cycle time.
- Actual damage after armor rounding.
- Effective survival against representative enemies.
- Travel time and ability to reach targets.
- Payback time and remaining match horizon.

A static design score proposes candidates; matches decide their value.
Do not assume more speed, armor, capacity, or damage is always better. Do not always spend all available component slots.

## Economy behaviors
Use explicit persistent states: SEEK, APPROACH, MINE_OR_COLLECT, RETURN, DEPOSIT, and ESCAPE as needed.
RETURN must remain latched until delivery or a justified state transition. Do not recompute it from a threshold in a way that causes oscillation.
Score resources using estimates of:
- Reachable path time.
- Actual collectible gems.
- Remaining cargo capacity.
- Required mining attacks.
- Gem-pile collection action.
- Loaded return travel.
- Delivery action.
- Enemy danger and interception risk.
- Existing assignments and congestion.

Prioritize delivered value, not gems carried far from home.
Use centralized greedy assignment first. Reserve useful resource capacity rather than permanently forbidding cooperation on every large deposit.
Add target persistence and invalidate targets when they disappear.
Consider partial loads when faster delivery has higher value. Do not copy a fixed return threshold from an unrelated configuration.
Stop excessive economic investment when resources are depleted, unsafe, or unlikely to repay before the match ends.

## Navigation and congestion
Use an indexed occupancy grid where appropriate.
Start with the supported weighted pathfinder if it is correct and fast enough. Add custom pathfinding only for a measured limitation.
Consider:
- Blocked and occupied cells.
- Destructible obstacle cost.
- Unit-specific damage capabilities.
- Loaded movement cost.
- Enemy attack danger.
- Friendly congestion.
- Recent failed movement.

Reserve intended destinations before emitting actions.
Prevent two friendly units from selecting the same destination.
Do not assume another friendly unit will vacate in a favorable random order.
Do not assume an attacked object disappears early enough for movement.
Detect stuck units and target oscillation.
Use bounded recovery: alternative route, alternative target, or yield behavior.
Do not allow recovery logic to disable core defense.

## Combat
Implement a complete path to victory, including core damage.
At each ready unit evaluate legal options:
- Immediate useful attack.
- Intercept a threat.
- Defend core or valuable workers.
- Join an attack.
- Reposition.
- Retreat or wait when justified.

Use exact verified damage and cooldown calculations.
Coordinate damage to avoid unnecessary overkill, but re-evaluate assumptions about simultaneous attacks and targets that may disappear.
Do not treat all adjacent attacks as automatically optimal.
Account for the value of the target, retaliation, and strategic urgency.
Prefer tactical calculations over decorative complexity.
Avoid:
- Endless chasing.
- Core attackers diverted into unwinnable skirmishes.
- Fighters attacking cores with zero core damage.
- Unprotected economic expansion during a lethal rush.
- Rally conditions that wait forever. A simple rally must have an escape condition.
Use formation or local search only after proving a benefit.

## Adaptive strategy
Do not implement only "N miners, then attackers."
Maintain interpretable scores for:
- Economic expansion.
- Immediate defense.
- Interception/raiding.
- Core pressure.
- Finishing a damaged enemy core.
- Preserving an advantage near a verified timeout.

Use reachable opportunities, not only Manhattan distance.
Introduce hysteresis so modes do not flip unnecessarily.
Treat the number of miners, army ratio, defense radius, and attack timing as context-dependent choices or tunable parameters, not proven universal values.
Preserve a conservative fallback strategy if an experimental layer fails.

## Action safety
Centralize final action validation. Check:
- Object exists and belongs to this player.
- Required cooldown is ready (`action_cooldown <= 0`).
- Target and position are legal.
- Damage/capacity is sufficient for intended action.
- Unit design is valid.
- Core can afford the chosen design.
- Friendly reservations are consistent.
- No conflicting second action was assigned to the same unit.

Action phase order does not imply free extra actions.
Classify failures:
- Preventable bot bug.
- Friendly coordination problem.
- Legitimate simultaneous opponent conflict.
- Stale target caused by resolution order.
- Protocol or infrastructure problem.
Reduce preventable failures first. Do not promise zero failures in a game with simultaneous hidden intentions.

## Memory and runtime reliability
Never free client-owned objects. Verify ownership of returned arrays.
Track application memory by object ID and clean it when the object disappears.
Handle allocation failure, empty collections, missing targets, and disconnects.
Avoid unbounded per-tick allocations and logging.
Test with compiler warnings and available sanitizers.
Record whether sanitizers instrument both bot and library or only the bot.
Measure runtime and memory under representative late-game unit counts.
Do not claim compliance from early-game observations alone.

## Match runner
Build a runner that:
- Starts an unmodified official server with explicit configuration.
- Starts exactly the intended bots.
- Uses safe readiness detection.
- Records actual starting positions.
- Applies startup, match, and cleanup timeouts.
- Cleans only processes it started.
- Reads authoritative results, not guessed log phrases.
- Distinguishes game loss from crash, disconnect, or invalid experiment.
- Stores seed, config hash, source/binary hash, parameters, and opponent.
- Preserves logs and useful replays.
- Produces CSV and JSON results.

Do not use a TCP readiness probe if the server treats it as a player connection. Default to one match at a time.
Enable parallelism only after proving independent ports or network namespaces.
Never have workers rewrite and compile the same source tree concurrently. Use immutable builds and separate work directories.

## Opponent league
Create deliberately different legal local opponents:
- Immediate core rush.
- Economy expansion.
- Anti-worker raider.
- Defensive turtle with eventual attack.
- Unit-combat-heavy brawler.
- Earlier frozen champion versions.
- Independent opponents provided legally by the user.

Check that each opponent actually spawns valid units and follows its strategy. A broken example bot is not a meaningful benchmark.
Keep opponent implementations independent enough to expose shared assumptions.
Never weaken opponents just to improve reported win rates.
Maintain:
- champion: strongest validated submission candidate.
- challenger: current experiment.
- league: frozen opponents.
- stress opponents: specialized counters.
A "test ultimate bot" should be a strong benchmark portfolio or adaptive opponent, not an untested invincibility claim.

## Improvement loop
For each iteration:
1. Inspect failures, losses, and replays.
2. Write one falsifiable hypothesis.
3. Identify the smallest useful change.
4. Preserve the current champion.
5. Build and run focused regressions.
6. Evaluate against the frozen league.
7. Compare by opponent and starting side.
8. Promote only if evidence supports it.
9. Otherwise revert or retain as an experiment.
10. Update the experiment log and next action.

Prefer correcting strategic weaknesses over tuning many arbitrary weights.
Never repeatedly tune on a supposedly untouched final test set.
After inspecting a holdout failure, that case becomes a regression case.
Reserve fresh seeds and opponents for subsequent independent evaluation.

## Required deliverables
Produce:
- Working bot source and compatible Makefile.
- Reproducible build instructions.
- Match runner.
- Several verified local opponents.
- Regression tests.
- Bounded tuning workflow.
- CSV/JSON result records.
- Useful replays and failure analysis.
- Submission checklist.
- A preserved fallback champion.

## First task when invoked
Inspect the actual repository and produce:
1. A concise compatibility audit.
2. The highest-priority verified mechanics.
3. A small implementation plan.
4. One runnable baseline milestone.
Then implement, test, and iterate within the agreed local budget. Do not stop after providing another architecture essay.

