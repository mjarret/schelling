#!/usr/bin/env python3
"""
Overlay plots from Google Benchmark CSV outputs for C++ and Python drivers.

Usage examples:
  # Generate CSVs in milliseconds
  ./hitting_time_bench --benchmark_format=csv --benchmark_time_unit=ms \
      --benchmark_out=bench_cpp.csv
  ./py_hitting_time_bench --benchmark_format=csv --benchmark_time_unit=ms \
      --benchmark_out=bench_py.csv

  # Plot CPU-time per call vs N = CS + PL for Lollipop family
  python3 scripts/plot_gbench_csv_compare.py \
      --cpp bench_cpp.csv --py bench_py.csv --out scaling.png

Notes:
  - Defaults select names matching Schelling Lollipop families in each driver.
  - Assumes times are already in milliseconds (use --benchmark_time_unit=ms).
  - Skips aggregate rows (mean/median/stddev) in the CSV.
"""
import argparse
import csv
import re
from typing import List, Tuple
from datetime import datetime
from pathlib import Path


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="Plot Google Benchmark CSV (C++ vs Python)")
    ap.add_argument("--cpp", required=True, help="C++ Google Benchmark CSV path")
    ap.add_argument("--py", default="", help="Python Google Benchmark CSV path (optional)")
    ap.add_argument("--out", default=None,
                    help="Output PNG path (default: out/scaling_YYYYMMDD_HHMMSS.png)")
    ap.add_argument("--cpp-filter", default=r"^Schelling/Lollipop/",
                    help="Regex to select C++ benchmark names (default Lollipop)")
    ap.add_argument("--py-filter", default=r"^PySchelling/Lollipop/",
                    help="Regex to select Python benchmark names (default Lollipop)")
    ap.add_argument("--time", choices=["cpu", "real"], default="cpu",
                    help="Time source column (default cpu)")
    ap.add_argument("--loglog", action="store_true", help="Use log-log axes")
    return ap.parse_args()


AGG_MARKERS = ("/mean", "/median", "/stddev", "/cv")


def load_points_csv(path: str, name_filter: str, time_kind: str) -> List[Tuple[int, float]]:
    rx = re.compile(name_filter)
    pts: List[Tuple[int, float]] = []  # (N, ms)
    with open(path, "r", encoding="utf-8") as f:
        r = csv.DictReader(f)
        has_run_type = "run_type" in r.fieldnames if r.fieldnames else False
        for row in r:
            name = row.get("name", "")
            if not name or any(m in name for m in AGG_MARKERS):
                continue
            if has_run_type and row.get("run_type", "") == "aggregate":
                continue
            if not rx.search(name):
                continue
            m = re.search(r"CS=(\d+)/PL=(\d+)", name)
            if not m:
                continue
            cs, pl = map(int, m.groups())
            N = cs + pl
            # Expect ms if user passed --benchmark_time_unit=ms
            if time_kind == "cpu":
                t = row.get("cpu_time", "")
            else:
                t = row.get("real_time", "")
            if not t:
                continue
            try:
                ms = float(t)
            except ValueError:
                continue
            pts.append((N, ms))
    pts.sort(key=lambda x: x[0])
    return pts


def main() -> int:
    args = parse_args()
    cpp_pts = load_points_csv(args.cpp, args.cpp_filter, args.time)
    if not cpp_pts:
        print("No C++ points matched. Check --cpp, --cpp-filter, and time unit.")
        return 2
    py_pts: List[Tuple[int, float]] = []
    if args.py:
        py_pts = load_points_csv(args.py, args.py_filter, args.time)

    try:
        import matplotlib.pyplot as plt
    except Exception:
        print("matplotlib is required. Try: pip install matplotlib")
        return 3

    plt.figure()
    xs, ys = zip(*cpp_pts)
    plt.plot(xs, ys, "o-", label="C++", markersize=3)
    if py_pts:
        xs2, ys2 = zip(*py_pts)
        plt.plot(xs2, ys2, "s-", label="Python", markersize=3)

    if args.loglog:
        plt.xscale("log"); plt.yscale("log")
        title_suffix = " (log-log)"
    else:
        title_suffix = " (linear)"
    plt.xlabel("Total size N = CS + PL")
    plt.ylabel(("CPU" if args.time == "cpu" else "Wall") + " time per call (ms)")
    plt.title("Scaling: run_schelling_process" + title_suffix)
    plt.grid(True, alpha=0.3)
    plt.legend()
    plt.tight_layout()
    if args.out:
        out_path = Path(args.out)
    else:
        repo_root = Path(__file__).resolve().parent.parent
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_path = repo_root / "out" / f"scaling_{ts}.png"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, dpi=150)
    print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
