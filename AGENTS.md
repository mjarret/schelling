Performance-First Coding Contract

Scope
- This repo targets academic, performance-first C++20 experiments. Correctness is required; we prefer special-purpose, maintainable implementations over general frameworks.

Ground Rules
- No runtime asserts by default: Do not add assert()/defensive guards unless explicitly requested. Compile-time static_assert is acceptable.
- Branchless by default: Prefer branchless formulations and strength reduction. Keep a branch only when it is demonstrably faster on the expected data distribution.
- Throughput over footprint: Moderate, purposeful memory overhead is acceptable to reduce latency, allocations, or cache misses. Avoid unbounded growth or needless copies.
- Modern C++20: RAII, const-correctness, move semantics, [[nodiscard]] where appropriate, contiguous storage, avoid virtual indirection and heap allocations in hot paths.
- Measurements: Include time complexity, memory notes, and a micro-optimization rationale when proposing changes.

Build/Profile Defaults
- Release: -O3 -march=native -DNDEBUG -fno-omit-frame-pointer, LTO when supported.
- Debug: -O1 -g3 -fno-omit-frame-pointer. Sanitizers are opt-in, not default.
- Warnings: -Wall -Wextra -Wpedantic; treat new warnings as issues to fix or consciously waive.

Utilities
- include/core/perf.hpp provides: PERF_HOT/COLD, PERF_ALWAYS_INLINE, PERF_LIKELY/UNLIKELY, PERF_ASSUME, prefetch helpers, and branchless select/min/max.
- Prefer std::span/std::string_view for non-owning views; preallocate when sizes are known.

When Branches Beat Branchless
- If profiling shows predictable, highly skewed branches with minimal mispredict cost, keep the branch. Otherwise, favor arithmetic/bitwise forms.

What Not To Do
- Do not introduce runtime asserts or heavy defensive checks unless requested.
- Do not add virtual layers or heap allocations in tight loops.
- Do not default to exceptions or complex error plumbing in hot paths.

Change Checklist
- Performance intent is clear (branchless choices, memory tradeoffs explained).
- Complexity and memory impact stated.
- Hot functions annotated (PERF_HOT) where it helps; avoid premature over-annotation.
- No new runtime asserts; compile still clean with -Wall -Wextra -Wpedantic.

