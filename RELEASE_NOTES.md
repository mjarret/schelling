# Release 0.02 — 2025-11-13

Highlights
- Benchmarks: added `hitting_time_bench` (Google Benchmark) and updated sim/CLI plumbing and graph internals to support hitting-time measurements.
- Python: introduced a Mesa-based Python version of the model with glue for benchmarking and comparison.
- Repo hygiene: added scripts, benches, and data artifacts to aid reproducibility.

Breaking/behavioral changes
- None intended.

Upgrade notes
- Rebuild with `make` (default release flags) or `make bench` to build the C++ benchmarks.

# Release 0.01 — 2025-10-11

Highlights
- Faster job aggregation: replaced unordered_map histograms with dense per‑step rows.
- RNG picks: switched Lollipop picks to integer‑threshold selection (weighted_pick2/3).
- Plotting: added gnuplot PNG output with axis labels and auto‑trim to data; kept PPM fallback.
- CLI: `--experiments` controls job count; throttled progress output (every 10k).
- Padded bitset: fixed default initialization and sentinel mapping.
- Profiling: Makefile `profile` target (non‑PIE, -pg) for clean gprof attribution.
- Docs: AUTHORS, CITATION.cff (ORCID), README “How to Cite”; collaborators listed.

Breaking/behavioral changes
- None intended. Heatmap output format unchanged; image/CSV files are generated locally but are now git‑ignored.

Upgrade notes
- Rebuild with `make` (release) or `make profile` (gprof). Use `OMP_NUM_THREADS=1` for single‑threaded profiling clarity.
