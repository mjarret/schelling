#!/usr/bin/env python3
import argparse
import os
import shlex
import subprocess
import sys
import time
from math import inf
from datetime import datetime
from pathlib import Path

try:
    import resource  # Unix: allow increasing stack soft-limit
except Exception:  # pragma: no cover
    resource = None


def run(cmd, cwd=None, env=None, timeout=None):
    return subprocess.run(
        cmd,
        cwd=cwd,
        env=env,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
    )


def build(clique: int, path: int, jobs: int = 0, cwd: str = "."):
    # Clean to force rebuild with new template instantiation
    try:
        run(["make", "clean"], cwd=cwd, timeout=120)
    except subprocess.CalledProcessError as e:
        print(f"make clean failed: {e.stderr}", file=sys.stderr)
    make_args = ["make"]
    if jobs and jobs > 0:
        make_args.append(f"-j{jobs}")
    # Use EXTRA_CXXFLAGS to avoid overriding Makefile's include paths/flags.
    make_args += [f"EXTRA_CXXFLAGS=-DLOLLIPOP_CLIQUE={clique} -DLOLLIPOP_PATH={path}"]
    run(make_args, cwd=cwd, timeout=600)


def compute_sizes(min_total: int, max_total: int):
    # Always include requested milestones
    milestones = [
        50, 100, 200, 500, 1000, 2000, 5000, 10000,
        25000, 50000, 100000, 250000, 500000,
    ]
    sizes = sorted(set(x for x in milestones if min_total <= x <= max_total))
    return sizes


def derive_lollipop(total: int):
    # Enforce exact relation P = 9*C by rounding total to the nearest multiple of 10.
    # This guarantees C = P/9 exactly. Favor rounding to lower multiple to not exceed total.
    if total < 10:
        return 1, 9
    c = max(1, total // 10)
    p = 9 * c
    return int(c), int(p)


def main():
    ap = argparse.ArgumentParser(description="Benchmark hitting-time (steps) and wall time across lollipop sizes.")
    ap.add_argument("--min", type=int, default=50, help="Minimum total size (default 50)")
    ap.add_argument("--max", type=int, default=500000, help="Maximum total size (default 500000)")
    ap.add_argument("--experiments", "-e", type=int, default=100, help="Experiments per size (default 100)")
    ap.add_argument("--density", "-d", type=float, default=0.8, help="Agent density (default 0.8)")
    ap.add_argument("--tau", "-t", default="1/2", help="Threshold tau as p/q or decimal (default 1/2)")
    ap.add_argument("--threads", type=int, default=0, help="OMP threads (0 = default runtime setting)")
    ap.add_argument("--make-jobs", type=int, default=0, help="Parallel jobs for make (default 0 = let make decide)")
    ap.add_argument("--out", default=None,
                    help="Output CSV path (default: out/bench_results_YYYYMMDD_HHMMSS.csv)")
    ap.add_argument("--skip-totals", nargs="*", type=int, default=[], help="Totals to skip if unstable (default: none)")
    ap.add_argument("--smoke", type=int, default=1, help="Smoke-test experiments before full run (default 1)")
    ap.add_argument("--omp-stack", default="", help="Set OMP_STACKSIZE/GOMP_STACKSIZE (e.g. 16M). Empty to leave unchanged")
    ap.add_argument("--timeout", type=float, default=300.0, help="Per-subprocess timeout seconds (default 300)")
    args = ap.parse_args()
    # Default batch size to reduce long-run instability; can be overridden via env.
    try:
        default_batch = int(os.environ.get("BENCH_BATCH", "1000"))
    except ValueError:
        default_batch = 1000
    batch_size = max(1, default_batch)

    sizes = [s for s in compute_sizes(args.min, args.max) if s not in set(args.skip_totals)]
    if not sizes:
        print("No sizes in range", file=sys.stderr)
        return 1

    # Raise stack soft-limit to hard-limit to mimic `ulimit -s unlimited` behavior where possible
    if resource is not None:
        try:
            soft, hard = resource.getrlimit(resource.RLIMIT_STACK)
            if soft < hard:
                resource.setrlimit(resource.RLIMIT_STACK, (hard, hard))
        except Exception:
            pass

    # Resolve output path and ensure directory exists
    if args.out:
        out_path = Path(args.out)
    else:
        repo_root = Path(__file__).resolve().parent.parent
        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        out_path = repo_root / "out" / f"bench_results_{ts}.csv"
    out_path.parent.mkdir(parents=True, exist_ok=True)

    # CSV header
    with open(out_path, "w", encoding="utf-8") as fout:
        fout.write("total_size,clique_size,path_length,experiments,avg_steps,seconds_total,seconds_per_experiment\n")
        for total in sizes:
            c, p = derive_lollipop(total)
            # Build for this size
            try:
                build(c, p, jobs=args.make_jobs)
            except subprocess.CalledProcessError as e:
                print(f"Build failed for total={total} C={c} P={p}:\n{e.stderr}", file=sys.stderr)
                # Record a failed row and continue
                fout.write(f"{total},{c},{p},{args.experiments},nan,nan,nan\n")
                fout.flush()
                continue

            env = os.environ.copy()
            if args.threads and args.threads > 0:
                env["OMP_NUM_THREADS"] = str(args.threads)
            # Respect user-provided OMP stack only; do NOT inflate by default to avoid overcommit
            if args.omp_stack:
                env["OMP_STACKSIZE"] = args.omp_stack
                env["GOMP_STACKSIZE"] = args.omp_stack
            # If the build was sanitized, disable leak checks that can abort under ptrace/CI
            env.setdefault("ASAN_OPTIONS", "detect_leaks=0:abort_on_error=1")
            env.setdefault("LSAN_OPTIONS", "detect_leaks=0")

            # Base command
            cmd = [
                os.path.join(".", "lollipop"),
                "--agent-density", str(args.density),
                "--tau", str(args.tau),
            ]
            # Smoke test (fast) to weed out crashing sizes
            if args.smoke and args.smoke > 0:
                smoke_cmd = cmd + ["--experiments", str(args.smoke)]
                try:
                    _ = run(smoke_cmd, env=env, timeout=args.timeout)
                except subprocess.CalledProcessError as e:
                    msg = (e.stdout or "") + ("\n" + e.stderr if e.stderr else "")
                    print(f"Smoke run failed for total={total} C={c} P={p}:\n{msg}", file=sys.stderr)
                    fout.write(f"{total},{c},{p},{args.experiments},nan,nan,nan\n")
                    fout.flush()
                    continue

            # Full measured run
            # Full measured run with adaptive batching: progressively reduce batch size if failures occur
            def run_with_batch(batch):
                remaining_local = int(args.experiments)
                ws, secs = 0.0, 0.0
                local_env = env
                while remaining_local > 0:
                    this_batch = min(batch, remaining_local)
                    full_cmd = cmd + ["--experiments", str(this_batch)]
                    t0 = time.perf_counter()
                    try:
                        proc = run(full_cmd, env=local_env, timeout=args.timeout)
                        out = proc.stdout.strip()
                    except subprocess.CalledProcessError:
                        # Try single-thread fallback once per batch
                        local_env = local_env.copy()
                        local_env["OMP_NUM_THREADS"] = "1"
                        try:
                            proc = run(full_cmd, env=local_env, timeout=args.timeout)
                            out = proc.stdout.strip()
                        except subprocess.CalledProcessError as e3:
                            msg = (e3.stdout or "") + ("\n" + e3.stderr if e3.stderr else "")
                            print(f"Batch run failed for total={total} C={c} P={p} batch={this_batch}:\n{msg}", file=sys.stderr)
                            return None, None
                    t1 = time.perf_counter()
                    try:
                        avg_steps_batch = float(out.splitlines()[-1].strip())
                    except Exception:
                        print(f"Failed to parse steps for total={total} C={c} P={p} batch={this_batch}: {out!r}", file=sys.stderr)
                        return None, None
                    ws += avg_steps_batch * float(this_batch)
                    secs += (t1 - t0)
                    remaining_local -= this_batch
                return ws, secs

            ws_final, secs_final = None, None
            # Try unique batch sizes, capped at experiments to avoid repeating same size
            tried = set()
            for div in (1, 2, 5, 10):
                b = min(max(1, batch_size // div), int(args.experiments))
                if b in tried:
                    continue
                tried.add(b)
                ws, secs = run_with_batch(b)
                if ws is not None:
                    ws_final, secs_final = ws, secs
                    break

            if ws_final is None:
                fout.write(f"{total},{c},{p},{args.experiments},nan,nan,nan\n")
                fout.flush()
                continue

            avg_steps = ws_final / float(args.experiments)
            seconds_total = secs_final
            seconds_per_experiment = seconds_total / float(args.experiments)

            fout.write(f"{total},{c},{p},{args.experiments},{avg_steps:.6f},{seconds_total:.6f},{seconds_per_experiment:.9f}\n")
            fout.flush()
            print(f"OK total={total} C={c} P={p} avg_steps={avg_steps:.3f} secs/exp={seconds_per_experiment:.6f}")
    print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
