<!-- Please read AGENTS.md before submitting. -->

Summary
- What changed and why:

Contract Checklist (see AGENTS.md)
- [ ] No runtime consistency checks or defensive guards added
- [ ] Branchless where practical; branches only when profiled faster
- [ ] Types via include/core/config.hpp (no ad-hoc typedefs)
- [ ] Complexity + memory impact noted
- [ ] Perf intent and micro-optimizations explained; measurements included when relevant

Testing/Validation
- [ ] Built with `-Wall -Wextra -Wpedantic` clean
- [ ] Benchmarks/tests run for touched hot paths

