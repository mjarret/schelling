#!/usr/bin/env python3
"""
Bench the Python implementation to mirror the C++ registrar at
testing/bench/hitting_time_bench.cpp:58

That registrar defines a sweep:
  CS(i) = 50 + i*50 for i in [0, 50)
  PL(i) = 9 * CS(i)

We measure average ms per call using the Cython wrapper (cython_bench).
By default we repeat calls until a minimum wall time is reached (like gbench).

Output: CSV with columns N,ms_per_call,CS,PL
"""
from __future__ import annotations
import argparse
import os
import sys
import time
from datetime import datetime
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="Python/Cython scaling sweep matching C++ registrar")
    ap.add_argument("--out", default=None,
                    help="Output CSV path (default: out/py_scaling_YYYYMMDD_HHMMSS.csv)")
    ap.add_argument("--cs0", type=int, default=50, help="Clique size start (default 50)")
    ap.add_argument("--step", type=int, default=50, help="Clique step (default 50)")
    ap.add_argument("--count", type=int, default=50, help="Number of sizes (default 50)")
    ap.add_argument("--num", type=int, default=9, help="PL numerator in PL=(CS*num)/den + off (default 9)")
    ap.add_argument("--den", type=int, default=1, help="PL denominator (default 1)")
    ap.add_argument("--off", type=int, default=0, help="PL offset (default 0)")
    ap.add_argument("--min-time", type=float, default=10.0, dest="min_time",
                    help="Target minimum seconds per size (default 10.0; set 0 to disable)")
    ap.add_argument("--reps", type=int, default=5, help="Fixed repetitions if --min-time=0 (default 5)")
    return ap.parse_args()


def time_ms_per_call(cs: int, pl: int, min_time: float, fixed_reps: int) -> float:
    import cython_bench  # built via setup.py
    if min_time and min_time > 0:
        calls = 0
        t0 = time.perf_counter()
        # Call in chunks to amortize Python<->C crossing overhead
        while True:
            # 5 calls per chunk by default inside cython
            ms = cython_bench.ms_per_call(cs, pl, reps=5)
            calls += 5
            if (time.perf_counter() - t0) >= min_time:
                # Convert last chunk’s ms to seconds to accumulate, then average
                # We only need average ms/call; compute total seconds / calls.
                break
        # Re-run once to measure total elapsed precisely
        t1 = time.perf_counter()
        # Use measured elapsed; ms_per_call approximated by (elapsed / calls)
        elapsed = t1 - t0
        return 1e3 * elapsed / max(1, calls)
    else:
        return float(cython_bench.ms_per_call(cs, pl, reps=max(1, fixed_reps)))


def main() -> int:
    args = parse_args()
    rows = [("N", "ms_per_call", "CS", "PL")]
    for i in range(args.count):
        cs = args.cs0 + i * args.step
        pl = (cs * args.num) // args.den + args.off
        n = cs + pl
        ms = time_ms_per_call(cs, pl, args.min_time, args.reps)
        rows.append((n, f"{ms:.6f}", cs, pl))
        print(f"CS={cs:5d} PL={pl:6d} N={n:7d}  ms/call={ms:.6f}")

    if args.out:
        out_path = Path(args.out)
    else:
        repo_root = Path(__file__).resolve().parent.parent
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_path = repo_root / "out" / f"py_scaling_{ts}.csv"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(",".join(map(str, rows[0])) + "\n")
        for n, ms, cs, pl in rows[1:]:
            f.write(f"{n},{ms},{cs},{pl}\n")
    print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
