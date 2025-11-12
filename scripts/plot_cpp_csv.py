#!/usr/bin/env python3
"""
Plot a single Google Benchmark CSV (C++ only), extracting CS/PL→X and
CPU/real time→Y. Useful when the C++ curve is hard to see on overlays.

Examples
  # Ensure you write real CSV (not JSON):
  #   ./hitting_time_bench \
  #     --benchmark_out=bench_cpp.csv --benchmark_out_format=csv \
  #     --benchmark_time_unit=ms

  # Plot total N = CS + PL vs CPU time (ms)
  python3 scripts/plot_cpp_csv.py --csv bench_cpp.csv --out cpp_only.png

  # Plot clique-only family with a filter
  python3 scripts/plot_cpp_csv.py --csv bench_cpp.csv \
    --filter '^Schelling/LollipopCliqueOnly/' --out cpp_clique_only.png

Notes
  - This script tolerates GBench preamble in the file and finds the CSV header.
  - It converts per-row time units to milliseconds using the time_unit column.
"""
import argparse
import csv
import re
import sys
from datetime import datetime
from pathlib import Path


AGG_MARKERS = ("/mean", "/median", "/stddev", "/cv")


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="Plot C++ Google Benchmark CSV (single curve)")
    ap.add_argument("--csv", required=True, help="Path to C++ benchmark CSV file")
    ap.add_argument("--out", default=None,
                    help="Output PNG path (default: out/cpp_only_YYYYMMDD_HHMMSS.png)")
    ap.add_argument("--filter", default=r"^Schelling/Lollipop/", help="Regex to select C++ benchmark names")
    ap.add_argument("--time", choices=["cpu", "real"], default="cpu", help="Time source (default cpu)")
    ap.add_argument("--x", choices=["N", "CS", "PL"], default="N", help="X axis: N=CS+PL, CS, or PL (default N)")
    ap.add_argument("--loglog", action="store_true", help="Use log-log axes")
    return ap.parse_args()


def unit_to_ms(unit: str) -> float:
    u = (unit or "ms").strip().lower()
    if u == "ns":
        return 1e-6
    if u == "us":
        return 1e-3
    if u == "ms":
        return 1.0
    if u == "s":
        return 1e3
    return 1.0


def read_csv_rows(path: str):
    # Some GB outputs include a text preamble. Seek the header line first.
    with open(path, "r", encoding="utf-8") as f:
        lines = f.readlines()
    hdr_idx = None
    for i, line in enumerate(lines):
        if line.startswith("name,iterations,real_time,cpu_time,time_unit"):
            hdr_idx = i
            break
    if hdr_idx is None:
        print("Could not find CSV header in file (check --benchmark_out_format=csv)", file=sys.stderr)
        return []
    reader = csv.DictReader(lines[hdr_idx:])
    return list(reader)


def extract_points(rows, name_rx: re.Pattern, time_kind: str, x_kind: str):
    pts = []  # (x, y_ms)
    for row in rows:
        name = row.get("name", "")
        if not name or any(m in name for m in AGG_MARKERS):
            continue
        if row.get("run_type", "") == "aggregate":
            continue
        if not name_rx.search(name):
            continue
        m = re.search(r"CS=(\d+)/PL=(\d+)", name)
        if not m:
            continue
        cs, pl = map(int, m.groups())
        if x_kind == "N":
            x = cs + pl
        elif x_kind == "CS":
            x = cs
        else:
            x = pl
        t = row.get("cpu_time" if time_kind == "cpu" else "real_time", "")
        if not t:
            continue
        try:
            t_val = float(t)
        except ValueError:
            continue
        mul = unit_to_ms(row.get("time_unit", "ms"))
        pts.append((x, t_val * mul))
    pts.sort(key=lambda p: p[0])
    return pts


def main() -> int:
    args = parse_args()
    rows = read_csv_rows(args.csv)
    if not rows:
        return 2
    rx = re.compile(args.filter)
    pts = extract_points(rows, rx, args.time, args.x)
    if not pts:
        print("No points matched. Check --filter and input file.", file=sys.stderr)
        return 3

    try:
        import matplotlib.pyplot as plt
    except Exception:
        print("matplotlib is required. Try: pip install matplotlib", file=sys.stderr)
        return 4

    xs, ys = zip(*pts)
    plt.figure()
    plt.plot(xs, ys, "o-", label="C++", markersize=4)
    if args.loglog:
        plt.xscale("log"); plt.yscale("log")
        title_suffix = " (log-log)"
    else:
        title_suffix = " (linear)"
    xlabel = {"N": "Total size N = CS + PL", "CS": "Clique size CS", "PL": "Path length PL"}[args.x]
    plt.xlabel(xlabel)
    plt.ylabel(("CPU" if args.time == "cpu" else "Wall") + " time per call (ms)")
    plt.title("C++ Only: run_schelling_process" + title_suffix)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    if args.out:
        out_path = Path(args.out)
    else:
        repo_root = Path(__file__).resolve().parent.parent
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_path = repo_root / "out" / f"cpp_only_{ts}.png"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_path, dpi=150)
    print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
