// job_handler.hpp — streamed heatmap with OpenMP reduction (no histories, no locks)
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <algorithm>
#include <omp.h>

#include "core/rng.hpp"
#include "sim/graph_concepts.hpp"
#include "sim/sim.hpp"

namespace sim {

// ---------- Config ----------
struct JobConfig {
    std::size_t jobs{1};      // 0 -> 1
    double      density{0.8};
    int         threads{0};   // 0 -> omp_get_max_threads()
};

// ---------- Result type ----------
using StepHist  = std::unordered_map<std::size_t, std::uint64_t>; // bin -> count
using StepHists = std::vector<StepHist>;                           // step -> histogram

// Merge helper (used by OpenMP to reduce thread‑local partials)
inline void merge_step_hists(StepHists& dst, const StepHists& src) {
    if (src.size() > dst.size()) dst.resize(src.size());
    for (std::size_t s = 0; s < src.size(); ++s) {
        auto& d = dst[s];
        for (const auto& [u, c] : src[s]) d[u] += c;
    }
}

// Declare a custom OpenMP reduction for StepHists
#pragma omp declare reduction (merge_step_hists : sim::StepHists : \
    sim::merge_step_hists(omp_out, omp_in)) initializer(omp_priv = sim::StepHists{})

// Optional: convert to dense if you need a matrix later
struct Heatmap {
    std::size_t bins{0};
    std::size_t rows{0};
    std::vector<std::uint64_t> data; // row-major rows × bins
};
inline Heatmap to_dense(const StepHists& sparse, std::size_t bins) {
    Heatmap out; out.bins = bins; out.rows = sparse.size();
    out.data.assign(out.rows * out.bins, 0ull);
    for (std::size_t r = 0; r < out.rows; ++r) {
        const auto& row = sparse[r];
        auto* dst = out.data.data() + r * out.bins;
        for (const auto& [u, c] : row) dst[u] += c;
    }
    return out;
}

// ---------- Streamed heatmap builder ----------
// ProgressCb: void(std::size_t done, std::size_t total)
template <class Graph, class SeedRng, class ProgressCb>
    requires GraphLike<Graph, SeedRng>
inline StepHists
run_jobs_heatmap_streamed(const JobConfig& cfg_in, SeedRng& master_rng, ProgressCb&& on_progress) {
    const std::size_t J = (cfg_in.jobs == 0) ? 1 : cfg_in.jobs;
    const int NT        = (cfg_in.threads > 0) ? cfg_in.threads : omp_get_max_threads();
    const std::size_t bins = static_cast<std::size_t>(Graph::TotalSize) + 1;

    // Deterministic per-job seeds (no RNG races)
    std::vector<std::uint64_t> seeds(J);
    for (std::size_t i = 0; i < J; ++i) seeds[i] = core::splitmix_hash(master_rng());

    std::atomic<std::size_t> done{0};

    StepHists result; // OpenMP gives each thread a private copy (via reduction)

    #pragma omp parallel for schedule(static) num_threads(NT) reduction(merge_step_hists: result)
    for (std::ptrdiff_t j = 0; j < static_cast<std::ptrdiff_t>(J); ++j) {
        core::Xoshiro256ss rng(seeds[static_cast<std::size_t>(j)]);
        Graph g;

        // Stream increments directly as the process evolves
        sim::run_schelling_process_visit(g, cfg_in.density, rng,
            [&](std::size_t s, core::count_t unhappy) {
                std::size_t u = static_cast<std::size_t>(unhappy);
                if (u >= bins) u = bins - 1;          // defensive clamp
                if (s >= result.size()) result.resize(s + 1);
                result[s][u] += 1ull;                 // thread‑local (no locks)
            });

        const std::size_t now = done.fetch_add(1, std::memory_order_relaxed) + 1;
        #pragma omp critical(sim_progress)
        { on_progress(now, J); }
    }

    return result; // already merged by OpenMP
}

// Convenience: same name/shape as before, but returns dense
template <class Graph, class SeedRng, class ProgressCb>
    requires GraphLike<Graph, SeedRng>
inline Heatmap
run_jobs_heatmap(const JobConfig& cfg, SeedRng& master_rng, ProgressCb&& on_progress) {
    auto sparse = run_jobs_heatmap_streamed<Graph>(cfg, master_rng, std::forward<ProgressCb>(on_progress));
    const std::size_t bins = static_cast<std::size_t>(Graph::TotalSize) + 1;
    return to_dense(sparse, bins);
}

template <class Graph, class SeedRng>
    requires GraphLike<Graph, SeedRng>
inline Heatmap
run_jobs_heatmap(const JobConfig& cfg, SeedRng& master_rng) {
    auto noop = [](std::size_t, std::size_t){};
    return run_jobs_heatmap<Graph>(cfg, master_rng, noop);
}

} // namespace sim
