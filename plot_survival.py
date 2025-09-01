#!/usr/bin/env python3
# plot_survival.py — Plot S(T) = P{ not hit by T } as a right-continuous step curve.
# Reads either:
#   • Text lines like:   "  T=123   S=0.987"
#   • CSV with header:   "T,S"
# Saves a PNG/PDF/SVG.

import argparse, sys, re, math
import matplotlib
matplotlib.use("Agg")  # headless-safe
import matplotlib.pyplot as plt

def parse_input(lines):
    T, S = [], []
    # Detect CSV header?
    # Accept both "T,S" and "t,s" etc.
    header = None
    for ln in lines:
        ln = ln.strip()
        if not ln:
            continue
        if header is None and ("," in ln):
            # Maybe CSV header or row
            parts = [p.strip() for p in ln.split(",")]
            if len(parts) >= 2 and parts[0].lower().startswith("t"):
                header = "csv"
                continue
            elif len(parts) >= 2 and parts[0].replace(".","",1).isdigit():
                header = "csv-data"
        if header == "csv" or header == "csv-data":
            try:
                parts = [p.strip() for p in ln.split(",")]
                t = float(parts[0]); s = float(parts[1])
                T.append(t); S.append(s); continue
            except Exception:
                # ignore non-data lines
                continue
        # Text mode: look for "T=...   S=..."
        m = re.search(r"T\s*=\s*([0-9.eE+-]+).*S\s*=\s*([0-9.eE+-]+)", ln)
        if m:
            T.append(float(m.group(1))); S.append(float(m.group(2)))
    # Ensure sorted by T and unique (keep last for same T)
    if not T:
        raise SystemExit("No survival points found. Did you run with --stats survival_curve ?")
    pairs = {}
    for t, s in zip(T, S):
        pairs[float(t)] = float(s)
    Tsorted = sorted(pairs.keys())
    Ssorted = [pairs[t] for t in Tsorted]
    # Prepend (0,1) if missing for right-continuous survival
    if Tsorted[0] > 0.0 or Ssorted[0] < 1.0:
        Tsorted = [0.0] + Tsorted
        Ssorted = [1.0] + Ssorted
    # Clamp S to [0,1]
    Ssorted = [min(1.0, max(0.0, s)) for s in Ssorted]
    return Tsorted, Ssorted

def main():
    ap = argparse.ArgumentParser(description="Plot survival curve from schelling output (S(T) = P{not hit by T}).")
    ap.add_argument("--in", dest="infile", default="-", help="Input file (curve.txt or CSV). Use '-' for stdin.")
    ap.add_argument("--out", default="survival.png", help="Output image (png|pdf|svg).")
    ap.add_argument("--title", default="Survival curve S(T)")
    ap.add_argument("--xscale", default="linear", choices=["linear","log"], help="X axis scale")
    ap.add_argument("--yscale", default="linear", choices=["linear","log"], help="Y axis scale")
    ap.add_argument("--dpi", type=int, default=140)
    args = ap.parse_args()

    lines = sys.stdin if args.infile == "-" else open(args.infile, "r")
    try:
        T, S = parse_input(lines)
    finally:
        if args.infile != "-":
            lines.close()

    plt.figure(figsize=(8, 5))
    # Right-continuous step: S is constant on (T_j, T_{j+1}], so use where='post'
    plt.step(T, S, where="post", linewidth=2)
    plt.xlabel("Moves (T)")
    plt.ylabel("Survival S(T) = P{ not hit by T }")
    plt.yscale(args.yscale)
    plt.xscale(args.xscale)
    plt.ylim(0, 1.02)
    plt.xlim(left=0)
    plt.grid(True, linestyle=":", linewidth=0.8)
    plt.title(args.title)
    plt.tight_layout()
    plt.savefig(args.out, dpi=args.dpi)
    # Print a quick textual summary to stdout
    print(f"Wrote {args.out}  (points={len(T)})")

if __name__ == "__main__":
    main()
