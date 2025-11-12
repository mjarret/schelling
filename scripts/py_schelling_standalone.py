#!/usr/bin/env python3
"""
Standalone runner for the Python Schelling simulation (no Google Benchmark).

- Adds repo's Python_Version to sys.path and also honors PY_GBENCH_SITE_PACKAGES.
- Runs run_schelling_process_py(cs, pl) for a fixed number of iterations.
- Reports timing in a simple CSV row compatible with the plotter expectations.

Usage:
  python3 scripts/py_schelling_standalone.py --cs 50 --pl 450 --iterations 10 \
      --density 0.8 --homophily 0.5 --seed 42

Output (to stdout):
  name,iterations,real_time,cpu_time,time_unit,items_per_second
  PySchelling/Standalone/CS=50/PL=450,10,<ms_wall>,<ms_cpu>,ms,<items_per_sec>

Notes:
  - This runner is single-threaded. If you need parallelism, run multiple
    processes (e.g. GNU parallel or xargs -P). CPython's GIL prevents
    true parallel execution of Python bytecode within one interpreter.
"""
from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path


def _wire_sys_path() -> None:
    repo_root = Path(__file__).resolve().parent.parent
    py_dir = repo_root / "Python_Version"
    if str(py_dir) not in sys.path:
        sys.path.insert(0, str(py_dir))
    sp = os.environ.get("PY_GBENCH_SITE_PACKAGES", "")
    if sp:
        for p in sp.split(":"):
            if p and p not in sys.path:
                sys.path.insert(0, p)


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="Run Python Schelling and time it")
    ap.add_argument("--cs", type=int, required=True, help="Clique size")
    ap.add_argument("--pl", type=int, required=True, help="Path length")
    ap.add_argument("--iterations", type=int, default=10, help="Number of runs (default 10)")
    ap.add_argument("--density", type=float, default=0.8, help="Agent density (default 0.8)")
    ap.add_argument("--homophily", type=float, default=0.5, help="Homophily threshold (default 0.5)")
    ap.add_argument("--seed", type=int, default=42, help="Seed (default 42)")
    return ap.parse_args()


def main() -> int:
    _wire_sys_path()
    try:
        from py_api import run_schelling_process_py
    except Exception as e:
        print(f"ImportError: cannot import py_api: {e}", file=sys.stderr)
        return 2

    args = parse_args()
    cs, pl = int(args.cs), int(args.pl)
    iters = max(1, int(args.iterations))

    # Measure both wall and CPU time for better comparability
    t0_wall = time.perf_counter()
    t0_cpu = time.process_time()
    for _ in range(iters):
        _steps = run_schelling_process_py(cs, pl, density=float(args.density), homophily=float(args.homophily), seed=int(args.seed))
    t1_cpu = time.process_time()
    t1_wall = time.perf_counter()

    ms_wall = (t1_wall - t0_wall) * 1e3
    ms_cpu  = (t1_cpu - t0_cpu) * 1e3
    ips = (iters * 1e3) / ms_wall if ms_wall > 0 else 0.0

    name = f"PySchelling/Standalone/CS={cs}/PL={pl}"
    print("name,iterations,real_time,cpu_time,time_unit,items_per_second")
    print(f"{name},{iters},{ms_wall:.6f},{ms_cpu:.6f},ms,{ips:.6f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

