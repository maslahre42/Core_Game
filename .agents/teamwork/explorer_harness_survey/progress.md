# Progress — Scrimmage & League Harness Survey

- Last visited: 2026-10-24T09:53:00Z
- Status: Investigation complete. All 4 mission objectives thoroughly analyzed with empirical verification.
- Key findings:
  1. `scripts/scrimmage.py` process lifecycle, port isolation, and critical defect identified (bot stdout/stderr redirected to DEVNULL, hiding all action failures and ASan leaks).
  2. Replay structure in `replays/replay_latest.json` analyzed; server logs action failures into unit `debug_info` under `[begin_errs]`.
  3. Opponent league architecture verified: all 4 archetypes (`opponent-rush`, `opponent-economy`, `opponent-brawler`, `opponent-turtle`) are implemented in `bots/c/bot/` with standalone Makefiles.
  4. Evaluation suite criteria designed: 24-match league matrix, ASan capture on stderr, action failure detection from stdout & replay, high-res latency instrumentation via `clock_gettime()`.
  5. Status of `docs/` manifests inspected (`STATUS.md`, `MECHANICS.md`, `EXPERIMENTS.md`, `KNOWN_FAILURES.md`).
