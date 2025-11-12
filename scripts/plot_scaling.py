#!/usr/bin/env python3
"""
Plot scaling from Google Benchmark JSON for Lollipop sweeps.

Usage examples:
  python3 scripts/plot_scaling.py bench.json --out scaling.png
  python3 scripts/plot_scaling.py bench.json --filter '^Schelling/LollipopCliqueOnly/' --out scaling_clique_only.png

Defaults:
  - X axis: N = CS + PL extracted from benchmark name
  - Y axis: CPU time per call in milliseconds
  - Linear axes (no log scaling)
"""
import argparse
import json
import re
import sys
from datetime import datetime
from pathlib import Path


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="Plot scaling from Google Benchmark JSON")
    ap.add_argument("json", help="Path to bench.json produced by hitting_time_bench")
    ap.add_argument("--out", default=None,
                    help="Output PNG path (default: out/scaling_YYYYMMDD_HHMMSS.png)")
    ap.add_argument("--filter", default=r"^Schelling/Lollipop/", help="Regex to select benchmark names")
    ap.add_argument("--time", choices=["cpu", "real"], default="cpu", help="Time source (default cpu)")
    ap.add_argument("--csv", default="", help="Optional CSV output of (N, ms) pairs")
    ap.add_argument("--loglog", action="store_true", help="Use log-log axes instead of linear")
    return ap.parse_args()


def unit_to_ms(unit: str) -> float:
    return {"ns": 1e-6, "us": 1e-3, "ms": 1.0, "s": 1e3}.get(unit, 1.0)


def load_points(path: str, name_filter: str, time_kind: str):
    js = json.load(open(path, "r", encoding="utf-8"))
    rx = re.compile(name_filter)
    pts = []  # (N, ms)
    for b in js.get("benchmarks", []):
        # Skip aggregates; use per-call iterations only
        if b.get("run_type") == "aggregate":
            continue
        name = b.get("name", "")
        if not rx.search(name):
            continue
        m = re.search(r"CS=(\d+)/PL=(\d+)", name)
        if not m:
            continue
        cs, pl = map(int, m.groups())
        N = cs + pl
        if time_kind == "cpu":
            t = b.get("cpu_time", None)
        else:
            t = b.get("real_time", None)
        if t is None:
            continue
        mul = unit_to_ms(b.get("time_unit", "ns"))
        pts.append((N, float(t) * mul))
    pts.sort(key=lambda x: x[0])
    return pts


def maybe_write_csv(csv_path: str, pts):
    if not csv_path:
        return
    import csv
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["N", "ms_per_call"])
        for n, ms in pts:
            w.writerow([n, f"{ms:.6f}"])


def main():
    args = parse_args()
    pts = load_points(args.json, args.filter, args.time)
    if not pts:
        print("No points matched. Check --filter and input file.", file=sys.stderr)
        return 2
    try:
        import matplotlib.pyplot as plt
    except Exception as e:
        print("matplotlib is required to plot. Try: pip install matplotlib", file=sys.stderr)
        return 3

    xs, ys = zip(*pts)
    plt.figure()
    plt.plot(xs, ys, "o-", markersize=4)
    if args.loglog:
        plt.xscale("log"); plt.yscale("log")
        title_suffix = " (log-log)"
    else:
        title_suffix = " (linear)"
    plt.xlabel("Total size N = CS + PL")
    plt.ylabel(("CPU" if args.time == "cpu" else "Wall") + " time per call (ms)")
    plt.title("Scaling: run_schelling_process" + title_suffix)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    if args.out:
        out_path = Path(args.out)
    else:
        repo_root = Path(__file__).resolve().parent.parent
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_path = repo_root / "out" / f"scaling_{ts}.png"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, dpi=150)
    # Optional CSV output path handling; if relative to repo, ensure dir
    if args.csv:
        csv_path = Path(args.csv)
        csv_path.parent.mkdir(parents=True, exist_ok=True)
        maybe_write_csv(str(csv_path), pts)
        print(f"wrote {out_path} and {csv_path}")
    else:
        maybe_write_csv("", pts)
        print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
