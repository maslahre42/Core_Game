#!/usr/bin/env python3
import argparse
import os
import subprocess
import time
import sys

def run_single_match(our_bot, enemy_bot, port=4500, timeout=30):
    env = os.environ.copy()
    env["SERVER_PORT"] = str(port)

    # Clean any stale instances
    subprocess.run(["pkill", "-9", "-f", f"SERVER_PORT={port}"], capture_output=True)

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

    p_our = subprocess.Popen(
        [our_bot, "42"],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )
    p_enemy = subprocess.Popen(
        [enemy_bot, "418"],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )

    try:
        s_out, s_err = server_proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        server_proc.kill()
        p_our.kill()
        p_enemy.kill()
        server_proc.communicate()
        return "TIMEOUT", 0

    p_our.kill()
    p_enemy.kill()

    winner = "UNKNOWN"
    if "Team 42" in s_out and "won the game" in s_out:
        winner = "WIN_42"
    elif "Team 418" in s_out and "won the game" in s_out:
        winner = "WIN_418"

    # Extract ticks if available from replay or output
    ticks = 0
    for line in s_out.splitlines():
        if "Game created with" in line:
            pass
    return winner, s_out

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--games", type=int, default=5)
    parser.add_argument("--enemy", default="./bots/c/bot/strategy-bot/strategy-bot")
    parser.add_argument("--base-port", type=int, default=4500)
    args = parser.parse_args()

    our_bot = "./bots/c/bot/my-core-bot/bot"
    enemy_bot = args.enemy

    print(f"Starting Scrimmage: Our Bot vs {os.path.basename(enemy_bot)} ({args.games} matches)")
    results = {"WIN_42": 0, "WIN_418": 0, "TIMEOUT": 0, "UNKNOWN": 0}

    for i in range(args.games):
        port = args.base_port + (i % 50)
        winner, out = run_single_match(our_bot, enemy_bot, port=port)
        results[winner] = results.get(winner, 0) + 1
        status_sym = "✅" if winner == "WIN_42" else ("❌" if winner == "WIN_418" else "⚠️")
        print(f"Match {i+1:2d}/{args.games:2d} (Port {port}): {status_sym} {winner}")
        time.sleep(0.1)

    win_rate = (results["WIN_42"] / args.games) * 100.0
    print("\n" + "="*40)
    print(f"FINAL SCORE: Team 42 (Our Bot): {results['WIN_42']} | Enemy: {results['WIN_418']} | Other: {results['TIMEOUT'] + results['UNKNOWN']}")
    print(f"Win Rate: {win_rate:.1f}%")
    print("="*40)

if __name__ == "__main__":
    main()

