#!/usr/bin/env python3
"""
Process-parallel runner for the Python Schelling simulation to collect
more data faster than Google Benchmark within CPython (avoids GIL limits).

Features
- Fans out (cs, pl) sizes across a process pool.
- Runs multiple iterations per process to amortize import/startup cost.
- Emits Google-Benchmark-style CSV so existing plotters can be reused.

Usage examples
- Lollipop family (PL = 9*CS), 10 sizes, 8 processes, 20 iters per size:
  python3 scripts/py_schelling_mp_runner.py \
    --family lollipop --cs0 50 --step 50 --count 10 \
    --iterations 20 --procs 8 --batch 5 \
    --out bench_py_mp.csv

- Clique-only (PL fixed), 10 sizes, 8 processes:
  python3 scripts/py_schelling_mp_runner.py \
    --family clique-only --cs0 50 --step 50 --count 10 --pl-fixed 10000 \
    --iterations 10 --procs 8 --batch 2 \
    --out bench_py_mp.csv

Environment
- PY_GBENCH_SITE_PACKAGES: optional, colon-separated site-packages to prepend to sys.path
  (e.g., repo_local/.venv/lib/python3.x/site-packages)

CSV schema (single header row):
  name,iterations,real_time,cpu_time,time_unit,bytes_per_second,items_per_second,label,error_occurred,error_message
Where:
  - name = PySchelling/Lollipop/CS=<cs>/PL=<pl>
  - iterations = total iterations aggregated for that size
  - real_time/cpu_time = ms per call (averaged)
  - time_unit = ms
  - items_per_second = iterations / total_wall_seconds
Other columns are left empty to keep schema compatibility.
"""
from __future__ import annotations

import argparse
import os
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from datetime import datetime
from typing import Iterable, List, Tuple, Dict


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
    ap = argparse.ArgumentParser(description="Process-parallel Python Schelling runner")
    ap.add_argument("--family", choices=["lollipop", "clique-only"], default="lollipop")
    ap.add_argument("--cs0", type=int, default=50, help="Initial clique size (default 50)")
    ap.add_argument("--step", type=int, default=50, help="Clique step increment (default 50)")
    ap.add_argument("--count", type=int, default=10, help="Number of sizes (default 10)")
    ap.add_argument("--num", type=int, default=9, help="PL formula numerator (default 9)")
    ap.add_argument("--den", type=int, default=1, help="PL formula denominator (default 1)")
    ap.add_argument("--off", type=int, default=0, help="PL formula offset (default 0)")
    ap.add_argument("--pl-fixed", type=int, default=10000, help="PL fixed for clique-only family (default 10000)")
    ap.add_argument("--iterations", type=int, default=10, help="Iterations per size (default 10)")
    ap.add_argument("--density", type=float, default=0.8, help="Agent density (default 0.8)")
    ap.add_argument("--homophily", type=float, default=0.5, help="Homophily threshold (default 0.5)")
    ap.add_argument("--seed", type=int, default=42, help="Base RNG seed (default 42)")
    ap.add_argument("--procs", type=int, default=0, help="Max worker processes (default: cpu_count)")
    ap.add_argument("--batch", type=int, default=1, help="Iterations per worker call to amortize startup (default 1)")
    ap.add_argument("--out", default=None,
                    help="Output CSV path (default: out/bench_py_mp_YYYYMMDD_HHMMSS.csv)")
    return ap.parse_args()


def gen_sizes(args: argparse.Namespace) -> List[Tuple[int, int]]:
    sizes: List[Tuple[int, int]] = []
    for i in range(int(args.count)):
        cs = int(args.cs0) + i * int(args.step)
        if args.family == "lollipop":
            pl = (cs * int(args.num)) // int(args.den) + int(args.off)
        else:
            pl = int(args.pl_fixed)
        sizes.append((cs, pl))
    return sizes


@dataclass
class Chunk:
    cs: int
    pl: int
    runs: int
    density: float
    homophily: float
    seed0: int


def _worker(chunk: Chunk) -> Tuple[int, int, int, float, float]:
    # Returns (cs, pl, runs, wall_ms_total, cpu_ms_total)
    _wire_sys_path()
    from py_api import run_schelling_process_py  # type: ignore
    cs, pl = chunk.cs, chunk.pl
    t0w = time.perf_counter()
    t0c = time.process_time()
    s = int(chunk.seed0)
    for j in range(int(chunk.runs)):
        # Vary the seed across runs for better sampling
        _ = run_schelling_process_py(cs, pl, density=float(chunk.density), homophily=float(chunk.homophily), seed=(s + j))
    t1c = time.process_time()
    t1w = time.perf_counter()
    return cs, pl, int(chunk.runs), (t1w - t0w) * 1e3, (t1c - t0c) * 1e3


def main() -> int:
    args = parse_args()
    sizes = gen_sizes(args)

    # Build work chunks
    chunks: List[Chunk] = []
    for idx, (cs, pl) in enumerate(sizes):
        remaining = int(args.iterations)
        seed0 = int(args.seed) ^ (cs * 1315423911) ^ (pl * 2654435761)
        while remaining > 0:
            b = min(max(1, int(args.batch)), remaining)
            chunks.append(Chunk(cs, pl, b, float(args.density), float(args.homophily), seed0))
            remaining -= b

    # Execute in parallel
    max_workers = None if args.procs in (None, 0) else int(args.procs)
    results: Dict[Tuple[int, int], Tuple[int, float, float, float]] = {}
    # key -> (iters, wall_ms_total, cpu_ms_total, start_time) [start_time for ips denom]
    start_wall = time.perf_counter()
    with ProcessPoolExecutor(max_workers=max_workers) as ex:
        futs = [ex.submit(_worker, ch) for ch in chunks]
        for fut in as_completed(futs):
            cs, pl, runs, wall_ms, cpu_ms = fut.result()
            key = (cs, pl)
            iters, wtot, ctot, _ = results.get(key, (0, 0.0, 0.0, start_wall))
            results[key] = (iters + runs, wtot + wall_ms, ctot + cpu_ms, _)

    # Emit GB-style CSV
    if args.out:
        out = Path(args.out)
    else:
        repo_root = Path(__file__).resolve().parent.parent
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        out = repo_root / "out" / f"bench_py_mp_{ts}.csv"
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", encoding="utf-8") as f:
        f.write("name,iterations,real_time,cpu_time,time_unit,bytes_per_second,items_per_second,label,error_occurred,error_message\n")
        for (cs, pl), (iters, wall_ms_total, cpu_ms_total, t0) in sorted(results.items()):
            name = f"PySchelling/Lollipop/CS={cs}/PL={pl}" if args.family == "lollipop" else f"PySchelling/LollipopCliqueOnly/CS={cs}/PL={pl}"
            ms_per_call_wall = wall_ms_total / max(1, iters)
            ms_per_call_cpu  = cpu_ms_total  / max(1, iters)
            seconds_total_wall = wall_ms_total / 1e3
            ips = (iters / seconds_total_wall) if seconds_total_wall > 0 else 0.0
            f.write(
                f"{name},{iters},{ms_per_call_wall:.6f},{ms_per_call_cpu:.6f},ms,,{ips:.6f},,,\n"
            )
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
