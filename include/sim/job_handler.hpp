// job_handler.hpp — streamed heatmap with OpenMP reduction (no histories, no locks)
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <atomic>
#include <algorithm>
#include <tbb/blocked_range.h>
#include <tbb/parallel_reduce.h>
#include <tbb/global_control.h>

#include "core/rng.hpp"
#include "sim/graph_concepts.hpp"
#include "sim/sim.hpp"
#include "sim/step_dense.hpp"
#include <memory>

namespace sim {

// ---------- Config ----------
struct JobConfig {
    std::size_t jobs{100};      // 0 -> 1
    double      density{0.8};
    int         threads{0};   // 0 -> tbb default
};

// Result types and helpers moved to sim/step_dense.hpp and sim/reductions.hpp

// ---------- Parallel hitting-time runner (no heatmap) ----------
// Runs J independent experiments in parallel and returns the total steps
// (hitting time measured as number of moves to reach zero-unhappy), leaving
// aggregation/averaging to the caller. This keeps the OpenMP parallel loop
// but eliminates any StepDense/heatmap work.
template <class Graph, class SeedRng>
    requires GraphLike<Graph, SeedRng>
inline size_t
run_jobs_hitting_time(const JobConfig& cfg_in, SeedRng& master_rng) {
    const std::size_t J = (cfg_in.jobs == 0) ? 1 : cfg_in.jobs;
    const int NT        = (cfg_in.threads > 0) ? cfg_in.threads : tbb::this_task_arena::max_concurrency();

    // Deterministic per-job seeds (no RNG races)
    std::vector<std::uint64_t> seeds(J);
    for (std::size_t i = 0; i < J; ++i) seeds[i] = core::splitmix_hash(master_rng());

    auto total = tbb::parallel_reduce(
        tbb::blocked_range<std::size_t>(0, J),
        std::uint64_t{0},
        [&](const tbb::blocked_range<std::size_t>& r, std::uint64_t init) {
            for (std::size_t j = r.begin(); j != r.end(); ++j) {
                core::Xoshiro256ss rng(seeds[j]);
                Graph g;
                init += static_cast<std::uint64_t>(
                    sim::run_schelling_process(g, cfg_in.density, rng));
            }
            return init;
        },
        std::plus<std::uint64_t>{});

    return static_cast<size_t>(total);
}

} // namespace sim
