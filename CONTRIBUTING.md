# Contributing

Please read and follow AGENTS.md (Performance-First Coding Contract).

- Re-reference AGENTS.md in PRs and reviews; quote the exact clauses you applied (e.g., no runtime consistency checks, branchless by default).
- Prefer focused, measurable improvements; include complexity, memory notes, and perf rationale.
- Use central types from `include/core/config.hpp`.
- Keep hot paths free of runtime asserts/guards; rely on documented preconditions and compile-time constraints.

## Test Protocol (Clarity-First Brute Force)

- Maximum clarity and readability over performance in tests.
- Prefer obvious, brute-force reference implementations over cleverness.
- For numeric/approximation utilities, exhaustively enumerate small parameter spaces and compare directly to the implementation under test.
  - Example: for rational approximations, scan all `p/q` with `1 <= q <= 8`, `0 <= p <= q` and select lower/upper/best by simple loops; compare to library results.
- Keep types consistent with `include/core/config.hpp` (e.g., `color_count_t`).
- Avoid parallelism unless a test is prohibitively slow; default to single-threaded, linear code.
- Document any tie-breaking rules explicitly in the test.
