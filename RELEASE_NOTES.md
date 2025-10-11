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

