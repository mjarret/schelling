// jit.hpp — compile-on-demand runner for GraphLike graphs
//
// Overview
// --------
// This header declares a minimal JIT (just‑in‑time) entry point that
// specializes and runs a Schelling process for arbitrary GraphLike graphs
// (see include/sim/graph_concepts.hpp) at runtime. The implementation
// (src/jit/jit.cpp) generates a tiny C++ translation unit that includes
// the requested graph header, constructs the requested graph type, compiles
// it into a shared object, then loads and executes it via dlopen/dlsym.
//
// Why JIT?
// --------
// Graphs typically use template parameters to keep hot paths fully optimized
// (no runtime branches). JIT lets users pick graph types/sizes at runtime
// while preserving the performance of static specialization. The first run
// for a given (Graph type expression) incurs a one‑time compile; subsequent
// runs reuse the cached .so.
//
// File layout & cache
// -------------------
// Generated code and shared objects live under `_jit/` using a stable
// naming scheme derived from the graph type expression, e.g.,
// `g_<sanitized-typename>.cpp/.so`. The generator always rewrites the
// source file to reflect current templates, and we rebuild the .so if it
// is missing or older than the source.
//
// Toolchain & flags
// -----------------
// The compiler is taken from $CXX if set, else `c++`. The .so is built
// with the appropriate platform flags (see src/jit/jit.cpp) and includes
// this repository's headers via `-Iinclude ...`. The JIT also defines
// `CORE_INDEX_T` (see include/core/config.hpp), chosen per specialization.
// Heuristic favors throughput: 32-bit where it fits, else 64-bit.
//
// Portability
// -----------
// Supported on Linux (dlopen, .so), macOS (dlopen, .dylib), and Windows
// (LoadLibrary/GetProcAddress, .dll). The implementation selects compiler
// flags and output extensions per platform; see src/jit/jit.cpp.
//
// Threading
// ---------
// The JIT path is not made thread‑safe. Concurrent calls might race on
// `_jit/<name>.cpp/.so`. Prefer serializing JIT invocations per size.
//
// Security note
// -------------
// The generated code is deterministic and derived solely from numeric
// parameters (no user‑supplied source). It still executes a compiler and
// loads a shared object at runtime; treat this path as trusted tooling.
//
// Clean‑up
// --------
// `make purge` removes `_jit/` along with other build artifacts.
//
// Error codes
// -----------
//  0: success
//  2: unexpected exception
//  3: compile failure (non‑zero compiler exit)
//  4: dlopen failed
//  5: dlsym failed (entry symbol not found)
//
// Usage (examples)
// ---------------
// Lollipop convenience wrapper:
//   unsigned long long moves=0, final_u=0;
//   int rc = jit::run_lollipop_once(50, 450, 1, 2, 0.8, moves, final_u);
//   // rc==0 -> use results; otherwise inspect rc for failure class.
//
// Generic GraphLike graph:
//   // Assume MyGraph<64> satisfies GraphLike and lives in "graphs/my.hpp"
//   unsigned long long m=0, fu=0;
//   int rc2 = jit::run_graph_once("graphs/my.hpp", "my::MyGraph<64>", 1, 2, 0.8, m, fu);
//   (void)rc2;
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace jit {

// Compile a specialized LollipopGraph<CS,PL> as a shared object and run one process.
// Returns 0 on success; non-zero on failure. Writes summary results via outputs.
/**
 * Compile and run a single Schelling process specialized for LollipopGraph
 * with the given sizes. Thin convenience wrapper over run_graph_once().
 *
 * @param clique_size    Lollipop clique size (template parameter).
 * @param path_length    Lollipop path length (template parameter).
 * @param p,q            Global Schelling threshold as p/q.
 * @param density        Initialization density in [0,1].
 * @param moves_out      Output: number of moves performed (history.size-1).
 * @param final_unhappy_out Output: terminal unhappy_count.
 * @param build_log      Optional: receives the compiler command used.
 * @return               0 on success; non‑zero on failure (see Error codes).
 */
int run_lollipop_once(std::size_t clique_size,
                      std::size_t path_length,
                      std::uint64_t p,
                      std::uint64_t q,
                      double density,
                      unsigned long long& moves_out,
                      unsigned long long& final_unhappy_out,
                      std::string* build_log = nullptr);

} // namespace jit
/**
 * Compile and run a single Schelling process for an arbitrary GraphLike type.
 *
 * @param include_header   Header path to include (e.g., "graphs/lollipop.hpp").
 * @param graph_type_expr  Fully qualified C++ type name to instantiate
 *                         (e.g., "graphs::LollipopGraph<50,450>").
 * @param p,q              Global Schelling threshold as p/q.
 * @param density          Initialization density in [0,1].
 * @param moves_out        Output: number of moves performed.
 * @param final_unhappy_out Output: terminal unhappy_count.
 * @param build_log        Optional: receives the compiler command used.
 * @return                 0 on success; non‑zero on failure.
 */
int run_graph_once(std::string_view include_header,
                   std::string_view graph_type_expr,
                   std::uint64_t p,
                   std::uint64_t q,
                   double density,
                   std::uint64_t max_size_hint,
                   unsigned long long& moves_out,
                   unsigned long long& final_unhappy_out,
                   std::string* build_log = nullptr);
