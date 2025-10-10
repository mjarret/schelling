Schelling — High‑Performance C++20 Experiments
=============================================

Overview
- Fast, counts‑first implementations of Schelling segregation dynamics on small structured graphs (e.g., lollipop: clique + path), written for performance experiments.
- Header‑heavy design with branchless hot paths and JIT specialization to keep runtime code minimal while enabling runtime size selection.

Key Features
- Graphs: `graphs::Clique`, `graphs::Path`, and `graphs::LollipopGraph<CS,PL>`.
- Simulation: `sim::run_schelling_process` over any `GraphLike` graph (`include/sim`).
- JIT: `jit::run_lollipop_once` compiles and runs a specialized graph at runtime (`_jit/` cache; removed automatically on successful run).
- CLI: minimal flags for τ = p/q, sizes, and density.

Build
- Requirements: modern C++20 compiler with `-march=native` support.
- Make targets:
  - `make` or `make release` — build the main binary `lollipop`.
  - `make debug` — debug build.
  - `make run` — build and run.
  - `make bench` — Google Benchmark for the lollipop demo (source under `testing/bench`).
  - `make purge` — remove generated artifacts including `_jit/`.

Run
- Default (τ=1/2, CS=50, PL=450, density=0.8):
  - `./lollipop`
- Examples:
  - `./lollipop --tau 2/3 --clique-size 100 --path-length 400`
  - `./lollipop -p 1 -q 3 --clique-size 51 --path-length 249`

Directory Layout
- `include/` — headers
  - `core/` — types, RNG, config, threshold
  - `graphs/` — graph implementations and internals under `graphs/detail/`
  - `sim/` — concepts and simulation helpers
  - `jit/` — JIT interface
  - `third_party/` — single‑header third‑party deps (cxxopts, RNG backends)
- `src/` — CLI and JIT implementations
- `testing/` — small doctest‑based suites and microbenchmarks
- `_jit/` — JIT cache (cleaned automatically on successful runs)

Performance Notes
- Counts/symmetry based formulations; avoid per‑vertex storage and adjacency scans in hot paths.
- Prefer branchless formulations and arithmetic identities when it improves throughput.
- Build defaults: `-O3 -march=native -DNDEBUG` (debug builds keep `-g3`).

License
- This repository is licensed under a restrictive George Mason University license. See `LICENSE`.
- Author of this version: Michael Jarret (mjarretb@gmu.edu).

