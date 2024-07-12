#!/usr/bin/env python3
"""Aggregate coincenter `replay` results into a per-algorithm performance report.

The coincenter replay command, when run with JSON output (`-o json`), produces a deeply
nested structure under the "out" key:

    out -> [ [algorithmName, [ [ [ [exchangeName, marketResult], ... ] ] ] ], ... ]

where each `marketResult` is a dict with keys:
    algorithm, market, startAmounts{base, quote}, profitAndLoss, stats{...}, matchedOrders[...]

This script walks that structure (robustly, by recursion), computes a percentage return for
each (algorithm, exchange, market) result as:

    return_pct = 100 * profitAndLoss / startAmounts.quote

(profitAndLoss and the quote start amount are both expressed in the market's quote currency, so
this ratio is dimensionless and comparable across markets. Note the engine starts with roughly
equal value in base and quote, so the total deployed capital is ~2x the quote leg; the ranking is
unaffected by that constant factor.)

It then ranks algorithms by their MEAN return across all markets (the selection objective:
maximize the average return), and also reports median, best/worst market, and the fraction of
markets that were profitable so risk can be eyeballed.

Usage:
    # 1) parse an existing JSON dump
    coincenter --data ./data -o json replay 900d > results.json
    scripts/replay_report.py results.json

    # 2) or let the script run coincenter for you
    scripts/replay_report.py --run \\
        --coincenter ./build-release/coincenter --data ./data --window 900d \\
        [--market BTC-USDT] [--algorithms mean-reversion,grid]
"""
import argparse
import json
import re
import subprocess
import sys
from statistics import mean, median

_NUM_RE = re.compile(r"[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?")


def parse_amount(value):
    """Extract a float from a MonetaryAmount, serialized either as a number or as a
    "<amount> <CUR>" string. Returns None if no number can be found."""
    if value is None:
        return None
    if isinstance(value, (int, float)):
        return float(value)
    if isinstance(value, str):
        m = _NUM_RE.search(value)
        return float(m.group(0)) if m else None
    return None


def is_market_result(node):
    return (
        isinstance(node, dict)
        and "profitAndLoss" in node
        and "market" in node
        and "algorithm" in node
    )


def collect_results(node, exchange=None, out=None):
    """Recursively collect (exchange, market_result_dict) leaves.

    Exchange names appear as the first element of a [exchangeName, marketResult] pair, so we
    capture them when we descend through such a pair."""
    if out is None:
        out = []
    if is_market_result(node):
        out.append((exchange, node))
        return out
    if isinstance(node, list):
        # Detect the [exchangeName, marketResult] pair form.
        if len(node) == 2 and isinstance(node[0], str) and is_market_result(node[1]):
            out.append((node[0], node[1]))
            return out
        for item in node:
            collect_results(item, exchange, out)
    elif isinstance(node, dict):
        for value in node.values():
            collect_results(value, exchange, out)
    return out


def build_rows(results):
    rows = []
    for exchange, res in results:
        start_quote = parse_amount(res.get("startAmounts", {}).get("quote"))
        pnl = parse_amount(res.get("profitAndLoss"))
        if start_quote is None or pnl is None or start_quote == 0:
            continue
        rows.append(
            {
                "algorithm": res.get("algorithm", "?"),
                "exchange": exchange or "?",
                "market": str(res.get("market", "?")),
                "pnl": pnl,
                "return_pct": 100.0 * pnl / start_quote,
                "n_orders": len(res.get("matchedOrders", []) or []),
            }
        )
    return rows


def report(rows, verbose=False):
    if not rows:
        print("No trading results found in the JSON input.", file=sys.stderr)
        return 1

    algos = {}
    for r in rows:
        algos.setdefault(r["algorithm"], []).append(r)

    summary = []
    for algo, rs in algos.items():
        returns = [r["return_pct"] for r in rs]
        summary.append(
            {
                "algorithm": algo,
                "n_markets": len(rs),
                "mean": mean(returns),
                "median": median(returns),
                "best": max(returns),
                "worst": min(returns),
                "pct_winning": 100.0 * sum(1 for x in returns if x > 0) / len(returns),
                "total_orders": sum(r["n_orders"] for r in rs),
            }
        )

    summary.sort(key=lambda s: s["mean"], reverse=True)

    header = (
        f"{'algorithm':<28}{'mkts':>5}{'mean%':>9}{'median%':>9}"
        f"{'best%':>9}{'worst%':>9}{'win%':>7}{'orders':>8}"
    )
    print(header)
    print("-" * len(header))
    for s in summary:
        print(
            f"{s['algorithm']:<28}{s['n_markets']:>5}{s['mean']:>9.2f}{s['median']:>9.2f}"
            f"{s['best']:>9.2f}{s['worst']:>9.2f}{s['pct_winning']:>7.0f}{s['total_orders']:>8}"
        )

    if verbose:
        print("\nPer-market detail (sorted by return):")
        for r in sorted(rows, key=lambda x: x["return_pct"], reverse=True):
            print(
                f"  {r['algorithm']:<28}{r['exchange']:<10}{r['market']:<12}"
                f"{r['return_pct']:>9.2f}%  ({r['n_orders']} orders)"
            )
    return 0


def run_coincenter(args):
    cmd = [args.coincenter, "--data", args.data, "-o", "json", "--log", "warning"]
    replay_arg = args.window
    cmd += ["replay", replay_arg]
    if args.market:
        cmd += ["--market", args.market]
    if args.algorithms:
        cmd += ["--algorithms", args.algorithms]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    # Logs may go to stderr; JSON is on stdout. Extract the outermost JSON object defensively.
    text = proc.stdout
    start = text.find("{")
    end = text.rfind("}")
    if start == -1 or end == -1:
        sys.stderr.write(proc.stderr)
        raise SystemExit("coincenter did not produce JSON output")
    return json.loads(text[start : end + 1])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("json_file", nargs="*", help="Path(s) to coincenter replay JSON dump(s); '-' or none reads stdin")
    parser.add_argument("--run", action="store_true", help="Run coincenter to produce the JSON")
    parser.add_argument("--coincenter", default="./build-release/coincenter")
    parser.add_argument("--data", default="./data")
    parser.add_argument("--window", default="900d", help="Replay duration, e.g. 900d")
    parser.add_argument("--market", default=None)
    parser.add_argument("--algorithms", default=None)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    rows = []
    if args.run:
        rows = build_rows(collect_results(run_coincenter(args)))
    elif not args.json_file or args.json_file == ["-"]:
        rows = build_rows(collect_results(json.load(sys.stdin)))
    else:
        for path in args.json_file:
            with open(path) as f:
                rows += build_rows(collect_results(json.load(f)))

    sys.exit(report(rows, verbose=args.verbose))


if __name__ == "__main__":
    main()
