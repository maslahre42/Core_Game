#!/usr/bin/env python3
"""
42 CORE Autonomous Tournament Scrimmage & League Harness.

Provides authoritative match execution, replay telemetry extraction,
clean process lifecycle management, AddressSanitizer verification,
action failure detection, latency logging, and multi-opponent league evaluation.
"""

import argparse
import json
import os
import re
import socket
import subprocess
import sys
import time

DEATH_REASON_NAMES = {
    0: "NONE_SURVIVED",
    1: "CORE_DESTROYED",
    2: "DISCONNECTED",
    3: "SPAMMED",
    4: "DID_NOT_CONNECT",
    5: "TIMEOUT_SENDING_DATA",
    6: "TIMEOUT_CORE_HP",
    7: "TIMEOUT_UNIT_HP",
    8: "TIMEOUT_RANDOM",
}

DEFAULT_LEAGUE_OPPONENTS = [
    "./bots/c/bot/opponent-rush/opponent-rush",
    "./bots/c/bot/opponent-economy/opponent-economy",
    "./bots/c/bot/opponent-brawler/opponent-brawler",
    "./bots/c/bot/opponent-turtle/opponent-turtle",
    "./bots/c/bot/gridmaster/gridmaster",
    "./bots/c/bot/strategy-bot/strategy-bot",
]

ANSI_ESCAPE_RE = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
LATENCY_RE = re.compile(r'\[LATENCY\]\s+avg=([0-9.]+)\s*ms(?:,\s*max=([0-9.]+)\s*ms)?(?:,\s*ticks=([0-9]+))?')

FAILURE_PATTERNS = [
    "action failure:",
    "invalid target position",
    "on action cooldown",
    "action cooldown",
    "cannot afford unit",
    "cannot afford",
    "action cooldown may not be less than",
    "more than 4 armor",
    "unit with this name does not exist",
    "invalid action",
]


def is_port_in_use(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.bind(('127.0.0.1', port))
            return False
        except OSError:
            return True


def get_next_free_port(start_port):
    port = start_port
    while port < 65000:
        if not is_port_in_use(port):
            return port
        port += 1
    return start_port


def parse_replay(replay_path="replays/replay_latest.json"):
    """
    Parses replays/replay_latest.json for authoritative outcome, match stats,
    tick duration, and Team 42 action failures.
    """
    if not os.path.isfile(replay_path):
        return None

    try:
        with open(replay_path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except Exception as e:
        print(f"  [Warning] Failed to parse replay JSON ({replay_path}): {e}")
        return None

    misc = data.get("misc", {})
    team_results = misc.get("team_results", [])
    stats = misc.get("stats", {})
    full_ticks = data.get("full_tick_amount", 0)

    winner = "UNKNOWN"
    death_42 = "UNKNOWN"
    death_enemy = "UNKNOWN"

    for tr in team_results:
        tid = tr.get("id")
        place = tr.get("place")
        d_code = tr.get("death_reason", -1)
        d_name = DEATH_REASON_NAMES.get(d_code, f"UNKNOWN_{d_code}")

        if tid == 42:
            death_42 = d_name
            if place == 0:
                winner = "WIN_42"
        elif tid == 418:
            death_enemy = d_name
            if place == 0:
                winner = "WIN_418"

    # Fallback if place wasn't explicitly set to 0
    if winner == "UNKNOWN" and team_results:
        if death_42 == "NONE_SURVIVED" and death_enemy != "NONE_SURVIVED":
            winner = "WIN_42"
        elif death_enemy == "NONE_SURVIVED" and death_42 != "NONE_SURVIVED":
            winner = "WIN_418"

    # Parse ticks for unit errors
    unit_teams = {}
    replay_failures = []
    ticks = data.get("ticks", {})
    for t_str in sorted(ticks.keys(), key=lambda x: int(x) if x.isdigit() else 0):
        t_data = ticks[t_str]
        for obj in t_data.get("objects", []):
            uid = obj.get("id")
            if "teamId" in obj:
                unit_teams[uid] = obj["teamId"]
            debug_info = obj.get("debug_info", "")
            if "[begin_errs]" in debug_info:
                team_id = unit_teams.get(uid)
                if team_id == 42:
                    in_errs = False
                    for line in debug_info.splitlines():
                        line = line.strip()
                        if not line:
                            continue
                        if "[begin_errs]" in line:
                            in_errs = True
                            continue
                        if in_errs:
                            replay_failures.append(line)

    return {
        "winner": winner,
        "ticks": full_ticks,
        "death_reason_42": death_42,
        "death_reason_enemy": death_enemy,
        "replay_failures": replay_failures,
        "stats": stats,
    }


def parse_bot_stdout(bot_stdout):
    """
    Parses bot stdout for:
    1. Latency telemetry lines: [LATENCY] avg=... max=...
    2. Action failure strings
    """
    avg_latency = None
    max_latency = None
    latency_ticks = None
    stdout_failures = []

    for raw_line in bot_stdout.splitlines():
        clean_line = ANSI_ESCAPE_RE.sub('', raw_line).strip()
        if not clean_line:
            continue

        # Check for latency telemetry
        lat_match = LATENCY_RE.search(clean_line)
        if lat_match:
            try:
                avg_latency = float(lat_match.group(1))
                if lat_match.group(2):
                    max_latency = float(lat_match.group(2))
                if lat_match.group(3):
                    latency_ticks = int(lat_match.group(3))
            except ValueError:
                pass

        # Check for action failures
        lower_line = clean_line.lower()
        if any(pat in lower_line for pat in FAILURE_PATTERNS):
            stdout_failures.append(clean_line)

    return {
        "avg_latency": avg_latency,
        "max_latency": max_latency,
        "latency_ticks": latency_ticks,
        "stdout_failures": stdout_failures,
    }


def check_asan(bot_stderr, returncode, timed_out=False):
    """
    Inspects bot stderr and exit status for AddressSanitizer or LeakSanitizer errors.
    """
    is_clean = True
    status = "CLEAN"
    details = []

    if "LeakSanitizer" in bot_stderr:
        is_clean = False
        status = "FAIL (LeakSanitizer)"
        details.append("LeakSanitizer detected memory leaks in stderr")

    if "AddressSanitizer" in bot_stderr:
        is_clean = False
        status = "FAIL (AddressSanitizer)"
        details.append("AddressSanitizer detected memory error in stderr")

    if not timed_out and returncode != 0:
        is_clean = False
        if status == "CLEAN":
            status = f"FAIL (Exit {returncode})"
        details.append(f"Bot exited with non-zero returncode {returncode}")

    return {
        "asan_clean": is_clean,
        "asan_status": status,
        "asan_details": "\n".join(details) if details else "",
    }


def run_single_match(our_bot, enemy_bot, port=4500, timeout=75, replay_file="replays/replay_latest.json"):
    """
    Executes a single match between our_bot and enemy_bot on isolated port.
    Captures stdout/stderr, applies clean lifecycle termination,
    and extracts authoritative replay + ASan + latency telemetry.
    """
    env = os.environ.copy()
    env["SERVER_PORT"] = str(port)

    # Clean any stale replay file before running
    if os.path.isfile(replay_file):
        try:
            os.remove(replay_file)
        except OSError:
            pass

    os.makedirs(os.path.dirname(os.path.abspath(replay_file)), exist_ok=True)

    cmd_server = [
        "./server/server",
        "bots/configs/fake_demo/server.config.json",
        "bots/configs/fake_demo/game.config.json",
        "server/data",
        "42",
        "418"
    ]

    server_proc = subprocess.Popen(
        cmd_server,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    time.sleep(0.15)

    # Launch bots with stdin=DEVNULL so core_static_awaitEnterPress() never blocks
    p_our = subprocess.Popen(
        [our_bot, "42"],
        env=env,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    p_enemy = subprocess.Popen(
        [enemy_bot, "418"],
        env=env,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )

    server_timed_out = False
    s_out, s_err = "", ""
    try:
        s_out, s_err = server_proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        server_timed_out = True
        server_proc.kill()
        s_out, s_err = server_proc.communicate()

    # Clean process lifecycle: allow p_our and p_enemy up to 2.0s to cleanly exit
    our_out, our_err = "", ""
    try:
        our_out, our_err = p_our.communicate(timeout=2.0)
    except subprocess.TimeoutExpired:
        p_our.kill()
        our_out, our_err = p_our.communicate()

    try:
        p_enemy.communicate(timeout=2.0)
    except subprocess.TimeoutExpired:
        p_enemy.kill()
        p_enemy.communicate()

    # Ensure processes are cleaned up
    for proc in (p_our, p_enemy, server_proc):
        if proc.poll() is None:
            try:
                proc.kill()
            except Exception:
                pass

    # Authoritative replay telemetry
    rep = parse_replay(replay_file)

    # Stdout telemetry (latency and action failures)
    stdout_telemetry = parse_bot_stdout(our_out)

    # ASan verification
    asan_info = check_asan(our_err, p_our.returncode, timed_out=server_timed_out)

    # Winner determination
    if rep and rep["winner"] != "UNKNOWN":
        winner = rep["winner"]
    elif server_timed_out:
        winner = "TIMEOUT"
    elif "Team 42" in s_out and "won the game" in s_out:
        winner = "WIN_42"
    elif "Team 418" in s_out and "won the game" in s_out:
        winner = "WIN_418"
    else:
        winner = "UNKNOWN"

    # Merge and deduplicate action failures
    replay_errs = rep["replay_failures"] if rep else []
    stdout_errs = stdout_telemetry["stdout_failures"]

    all_failures = []
    seen = set()
    for err in stdout_errs + replay_errs:
        c_err = err.strip()
        if c_err and c_err not in seen:
            seen.add(c_err)
            all_failures.append(c_err)

    return {
        "winner": winner,
        "ticks": rep["ticks"] if rep else 0,
        "death_reason_42": rep["death_reason_42"] if rep else "UNKNOWN",
        "death_reason_enemy": rep["death_reason_enemy"] if rep else "UNKNOWN",
        "action_failures": all_failures,
        "action_failure_count": len(all_failures),
        "stdout_failures": stdout_errs,
        "replay_failures": replay_errs,
        "asan_clean": asan_info["asan_clean"],
        "asan_status": asan_info["asan_status"],
        "asan_details": asan_info["asan_details"],
        "avg_latency": stdout_telemetry["avg_latency"],
        "max_latency": stdout_telemetry["max_latency"],
        "stats": rep["stats"] if rep else {},
        "our_stdout": our_out,
        "our_stderr": our_err,
        "server_stdout": s_out,
    }


def run_series(our_bot, enemy_bot, games=5, base_port=4500, timeout=75, verbose=False):
    """
    Runs a series of matches against a single opponent using dynamic isolated ports.
    """
    enemy_name = os.path.basename(enemy_bot)
    results = {"WIN_42": 0, "WIN_418": 0, "TIMEOUT": 0, "UNKNOWN": 0}
    total_failures = 0
    all_failures_list = []
    all_asan_clean = True
    latencies = []

    print(f"\n--- Testing vs {enemy_name} ({games} matches) ---")

    current_port = base_port
    for i in range(games):
        port = get_next_free_port(current_port)
        current_port = port + 1
        match = run_single_match(our_bot, enemy_bot, port=port, timeout=timeout)

        winner = match["winner"]
        results[winner] = results.get(winner, 0) + 1
        sym = "✅" if winner == "WIN_42" else ("❌" if winner == "WIN_418" else "⚠️")

        fail_cnt = match["action_failure_count"]
        total_failures += fail_cnt
        if fail_cnt > 0:
            all_failures_list.extend(match["action_failures"])

        if not match["asan_clean"]:
            all_asan_clean = False

        if match["avg_latency"] is not None:
            latencies.append(match["avg_latency"])

        lat_str = f"{match['avg_latency']:.4f} ms" if match["avg_latency"] is not None else "N/A"
        ticks_str = f"{match['ticks']} ticks"
        reason_str = f"Enemy: {match['death_reason_enemy']}" if winner == "WIN_42" else f"Own: {match['death_reason_42']}"

        print(f"  Match {i+1:2d}/{games:2d} (Port {port}): {sym} {winner} ({ticks_str}, {reason_str}) | ASan: {match['asan_status']} | Latency: {lat_str} | Failures: {fail_cnt}")

        if fail_cnt > 0:
            print(f"    Verbatim Action Failures ({fail_cnt}):")
            for f_msg in match["action_failures"][:5]:
                print(f"      - {f_msg}")
            if len(match["action_failures"]) > 5:
                print(f"      ... and {len(match['action_failures']) - 5} more")

        if verbose and match["stats"]:
            print(f"    Replay Stats: {match['stats']}")

        time.sleep(0.05)

    win_rate = (results["WIN_42"] / games) * 100.0 if games > 0 else 0.0
    series_avg_lat = (sum(latencies) / len(latencies)) if latencies else None

    return {
        "results": results,
        "win_rate": win_rate,
        "total_failures": total_failures,
        "all_failures": all_failures_list,
        "all_asan_clean": all_asan_clean,
        "asan_status": "CLEAN" if all_asan_clean else "FAIL",
        "avg_latency": series_avg_lat,
    }


def run_league(our_bot, opponents, games=4, base_port=5000, timeout=60, verbose=False):
    """
    Executes league tournament across all opponents and prints an authoritative summary table.
    """
    print("=" * 95)
    print("                      42 CORE CHAMPIONSHIP LEAGUE EVALUATION HARNESS")
    print(f"Candidate Bot: {our_bot}")
    print(f"Opponents ({len(opponents)}): {', '.join(os.path.basename(o) for o in opponents)}")
    print(f"Matches per opponent: {games} (Total: {len(opponents) * games} matches)")
    print(f"Base Port: {base_port}")
    print("=" * 95)

    total_wins = 0
    total_losses = 0
    total_games = 0
    total_failures = 0
    overall_asan = True
    all_latencies = []
    league_rows = []

    port_offset = base_port
    for opp in opponents:
        if not os.path.exists(opp):
            print(f"\n[Warning] Opponent binary not found at {opp}, skipping.")
            continue

        res = run_series(
            our_bot,
            opp,
            games=games,
            base_port=port_offset,
            timeout=timeout,
            verbose=verbose
        )
        port_offset += games

        opp_name = os.path.basename(opp)
        wins = res["results"]["WIN_42"]
        losses = res["results"]["WIN_418"]
        wr = res["win_rate"]
        af = res["total_failures"]
        asan_s = res["asan_status"]
        lat = res["avg_latency"]

        total_wins += wins
        total_losses += losses
        total_games += games
        total_failures += af
        if not res["all_asan_clean"]:
            overall_asan = False
        if lat is not None:
            all_latencies.append(lat)

        league_rows.append((opp_name, games, wins, losses, wr, af, asan_s, lat))

    # Evaluation summary table
    print("\n" + "=" * 95)
    print("                                 LEAGUE EVALUATION SUMMARY")
    print("=" * 95)
    print(f"{'Opponent':<18} | {'Games':<7} | {'Wins':<6} | {'Losses':<6} | {'Win Rate':<10} | {'Action Failures':<15} | {'ASan Status':<11} | {'Avg Latency':<11}")
    print("-" * 95)
    for row in league_rows:
        opp_name, g, w, l, wr, af, asan_s, lat = row
        lat_str = f"{lat:.4f} ms" if lat is not None else "N/A"
        print(f"{opp_name:<18} | {g:>7d} | {w:>6d} | {l:>6d} | {wr:>9.1f}% | {af:>15d} | {asan_s:>11} | {lat_str:>11}")
    print("-" * 95)

    overall_wr = (total_wins / total_games * 100.0) if total_games > 0 else 0.0
    total_lat_str = f"{(sum(all_latencies) / len(all_latencies)):.4f} ms" if all_latencies else "N/A"
    total_asan = "CLEAN" if overall_asan else "FAIL"
    print(f"{'TOTAL':<18} | {total_games:>7d} | {total_wins:>6d} | {total_losses:>6d} | {overall_wr:>9.1f}% | {total_failures:>15d} | {total_asan:>11} | {total_lat_str:>11}")
    print("=" * 95)

    return {
        "total_games": total_games,
        "total_wins": total_wins,
        "total_losses": total_losses,
        "overall_wr": overall_wr,
        "total_failures": total_failures,
        "overall_asan": total_asan,
        "overall_latency": total_lat_str,
    }


def main():
    parser = argparse.ArgumentParser(
        description="42 CORE Tournament Scrimmage & League Harness"
    )
    parser.add_argument(
        "--games",
        type=int,
        default=5,
        help="Number of matches per opponent (default: 5)"
    )
    parser.add_argument(
        "--enemy",
        default="./bots/c/bot/strategy-bot/strategy-bot",
        help="Path to enemy bot executable (default: ./bots/c/bot/strategy-bot/strategy-bot)"
    )
    parser.add_argument(
        "--our-bot",
        default="./bots/c/bot/my-core-bot/bot",
        help="Path to our candidate bot executable (default: ./bots/c/bot/my-core-bot/bot)"
    )
    parser.add_argument(
        "--base-port",
        type=int,
        default=5000,
        help="Base TCP port for isolated match runs (default: 5000)"
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=60,
        help="Match timeout in seconds (default: 60)"
    )
    parser.add_argument(
        "--league",
        action="store_true",
        help="Execute full tournament league across all 6 opponents"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Enable verbose output including replay statistics"
    )
    args = parser.parse_args()

    # Ensure running from repository root
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if os.path.abspath(os.getcwd()) != repo_root:
        os.chdir(repo_root)

    if not os.path.isfile(args.our_bot):
        print(f"Error: Candidate bot binary not found at '{args.our_bot}'. Run 'make' first.")
        sys.exit(1)

    if args.league:
        run_league(
            our_bot=args.our_bot,
            opponents=DEFAULT_LEAGUE_OPPONENTS,
            games=args.games,
            base_port=args.base_port,
            timeout=args.timeout,
            verbose=args.verbose,
        )
    else:
        if not os.path.isfile(args.enemy):
            print(f"Error: Enemy bot binary not found at '{args.enemy}'.")
            sys.exit(1)

        res = run_series(
            our_bot=args.our_bot,
            enemy_bot=args.enemy,
            games=args.games,
            base_port=args.base_port,
            timeout=args.timeout,
            verbose=args.verbose,
        )

        enemy_name = os.path.basename(args.enemy)
        wins = res["results"]["WIN_42"]
        losses = res["results"]["WIN_418"]
        lat_str = f"{res['avg_latency']:.4f} ms" if res["avg_latency"] is not None else "N/A"

        print("\n" + "=" * 60)
        print(f"SERIES SUMMARY vs {enemy_name}")
        print("=" * 60)
        print(f"Matches: {args.games} | Won: {wins} | Lost: {losses} | Win Rate: {res['win_rate']:.1f}%")
        print(f"Action Failures: {res['total_failures']}")
        print(f"ASan Status:     {res['asan_status']}")
        print(f"Avg Latency:     {lat_str}")
        print("=" * 60)


if __name__ == "__main__":
    main()
