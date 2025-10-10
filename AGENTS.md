SINGLE MOST IMPORTANT RULE — REFRESH CONTEXT FIRST
- Before making any change, writing code, or running commands, the agent MUST refresh its knowledge of the codebase. This includes re-scanning the repo (e.g., `rg`, tree), rereading AGENTS.md files in scope, and verifying current naming/concepts in relevant headers and sources. No actions proceed based on stale memory.
- Confirm key contracts and identifiers (e.g., GraphLike vs Graph) before edits. If uncertainty remains, add a TODO and stop rather than guessing.

Zero-Overreach Rule (Do Exactly What Was Asked)
- Perform only the explicitly requested change(s). Do not refactor, rename, “fix,” or modify adjacent code, files, or logic unless the user asks for it.
- If a requested change cannot be completed without additional edits, stop and ask for confirmation with the minimal options needed.
- Keep edits strictly scoped to the files and lines necessary for the request.
- No opportunistic cleanups, build fixes, or style changes outside scope.

Performance-First Coding Contract

Scope
- This repo targets academic, performance-first C++20 experiments. Correctness is required; we prefer special-purpose, maintainable implementations over general frameworks.

Reference Discipline
- Re-reference this contract in PR descriptions, code reviews, and performance notes. Quote the specific clauses you’re applying (e.g., "No runtime consistency checks", "Branchless by default"). Treat omissions as process regressions.

Ground Rules
- No runtime asserts by default: Do not add assert()/defensive guards unless explicitly requested. Compile-time static_assert is acceptable.
- No runtime consistency checks: Do not add range/validity guards (e.g., color-in-range, bounds checks, "skip invalid" branches) in library code or hot paths. The introduction of any such checks is treated as a failure unless explicitly requested. Use compile-time constraints (static_assert, types) instead.
- Branchless by default: Prefer branchless formulations and strength reduction. Keep a branch only when it is demonstrably faster on the expected data distribution.
- Throughput over footprint: Moderate, purposeful memory overhead is acceptable to reduce latency, allocations, or cache misses. Avoid unbounded growth or needless copies.
- Modern C++20: RAII, const-correctness, move semantics, contiguous storage, avoid virtual indirection and heap allocations in hot paths.
- Measurements: Include time complexity, memory notes, and a micro-optimization rationale when proposing changes.

Extreme Efficiency Clause
- Abandon defensive coding in hot paths: Prefer unchecked, straight-line operations guided by documented preconditions. Use compile-time constraints (types, concepts, static_assert) instead of runtime guards.
- Remove non-utility casts/checks: Avoid redundant static_cast or conversions added solely to appease defensive style; keep only those that affect codegen, silence real warnings, or express intent.
- Prioritize pipeline throughput: Optimize for instruction-level parallelism and branch elimination even if it reduces generality. Keep behavior correct for documented preconditions.

Build/Profile Defaults
- Release: -O3 -march=native -DNDEBUG -fno-omit-frame-pointer, LTO when supported.
- Debug: -O1 -g3 -fno-omit-frame-pointer. Sanitizers are opt-in, not default.
- Warnings: -Wall -Wextra -Wpedantic; treat new warnings as issues to fix or consciously waive.

Utilities
- include/core/perf.hpp provides: PERF_HOT/COLD, PERF_ALWAYS_INLINE, PERF_LIKELY/UNLIKELY, PERF_ASSUME, prefetch helpers, and branchless select/min/max.
- Prefer std::span/std::string_view for non-owning views; preallocate when sizes are known.
- Succinct constructors: Prefer compact, single-line initializers when intent is clear and no logic is duplicated. Mirror CliqueGraph style; keep RAII and avoid defensive guards in hot paths.
- Schelling threshold: Use program-wide comparator (include/core/schelling_threshold.hpp). Initialize once from tau→p/q; graphs must not store tau/pq per-instance.

When Branches Beat Branchless
- If profiling shows predictable, highly skewed branches with minimal mispredict cost, keep the branch. Otherwise, favor arithmetic/bitwise forms.

What Not To Do
- Do not introduce runtime asserts or heavy defensive checks unless requested.
- Do not introduce runtime consistency guards (e.g., filter invalid inputs, early-outs for bad states) in core code; rely on documented preconditions and compile-time constraints.
- Do not add virtual layers or heap allocations in tight loops.
- Do not default to exceptions or complex error plumbing in hot paths.

Change Checklist
- Performance intent is clear (branchless choices, memory tradeoffs explained).
- Complexity and memory impact stated.
- Hot functions annotated (PERF_HOT) where it helps; avoid premature over-annotation.
- No new runtime asserts; compile still clean with -Wall -Wextra -Wpedantic.
- No new runtime consistency checks or guards introduced in hot/library code.

Test Protocol (Clarity-First)
- Maximum clarity and readability for tests; performance is secondary.
- Prefer brute-force reference implementations (explicit scans, exact comparisons) over cleverness; keep bounds small (e.g., K≤8) but exhaustive.
- For rational approximation tests, enumerate all relevant τ values that can change behavior:
  - All rationals p/q with 1≤q≤Q (Q≥8), plus one-ULP neighbors via nextafter.
  - A dyadic grid with denominator 2^(⌈log2(K)⌉+1) to go one bit beyond nominal resolution.
- Provide simple parallel fan-out over worklists using std::thread and display a concise progress indicator.

CliqueGraph Policy (Counts-First)
- Use counts and symmetries, not vertices: Implement CliqueGraph algorithms using color counts and combinatorial identities. Do not introduce per-vertex storage or adjacency scans.
- Local frustration from counts: Compute local_frustration(v) as total_occupied() - counts[color(v)] without touching neighbors.
- Total frustration by formula: Maintain tf via O(1) deltas or recompute from counts using (occ^2 - sum_k counts[k]^2) / 2; never iterate vertices/edges.
- Updates via counts only: change_color modifies counts and adjusts tf using derived constants; avoid touching unrelated state.
- Index→color mapping: When mapping a vertex index to a color, use prefix sums over counts; avoid materializing vertex arrays.
- Complexity target: All per-operation costs O(1) or O(K) (small), independent of N except through counts.

Bit-Twiddling Hacks (HPC)
- Source reference: “Bit Twiddling Hacks” by Sean Eron Anderson (Stanford). Use these patterns where they improve throughput. Prefer standard C++20 `<bit>` first; fall back to compiler intrinsics; use raw arithmetic/bitwise identities when they simplify codegen. Do not copy from the site verbatim; apply the techniques idiomatically in modern C++.
- Counting bits:
  - Prefer `std::popcount(x)` over loops. Fallback: Kernighan (`for (t = x; t; ++c) t &= (t - 1);`).
- Trailing/leading zeros:
  - `std::countr_zero(x)`, `std::countl_zero(x)` (non-zero precondition in hot paths). Intrinsics: `__builtin_ctzll`, `__builtin_clzll`.
- Lowest/highest set bit:
  - Isolate lowest: `x & -x` (two’s complement). Clear lowest: `x & (x - 1)`.
  - Index of lowest: `std::countr_zero(x)`. Index of highest: `std::bit_width(x) - 1` (or `63 - __builtin_clzll(x)`).
- Power of two / rounding:
  - Is power of two: `x && !(x & (x - 1))`.
  - Round up to next power of two: `std::bit_ceil(x)` (or spread/shift sequence when unavailable).
- Word/byte reversal:
  - Prefer intrinsics: `__builtin_bswap64/32`; use `std::byteswap` when available.
- Parity:
  - `std::popcount(x) & 1` or xor-folding if not available.
- Branchless min/max/select:
  - Use arithmetic/bitwise forms (`a ^ ((a ^ b) & -(a < b))`) when they measure faster on target data; otherwise keep predictable branches.
- Intra-word k-th set bit (select):
  - If BMI2: `idx = std::countr_zero(_pdep_u64(1ull << k, word));` else clear k lows then `idx = std::countr_zero(word);`.
- Scans over sets:
  - Iterate set bits: `for (t = x; t; t &= (t - 1)) { i = countr_zero(t); … }`.
- Subset iteration within a mask:
  - `for (s = sub; s; s = (s - 1) & sub) { … }`.
- Bitfield compress/expand (BMI2):
  - Prefer `PEXT/PDEP` when available for masked gather/scatter; otherwise use shifts/mults.
- Multiplication/division by constants:
  - Replace with shifts/adds where codegen is better; verify with compiler output.
- Endianness-sensitive ops:
  - Keep ops endian-agnostic or isolate byte-order conversions behind intrinsics.
- General guidance:
  - Prefer hardware intrinsics (`POPCNT`, `LZCNT/TZCNT`, `PEXT/PDEP`) with `-march=native`. Guard with feature macros; provide efficient fallbacks. Keep hot paths branchless and rely on documented preconditions.
