// job_handler.hpp — streamed heatmap with OpenMP reduction (no histories, no locks)
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
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

// ---------- Result type (dense rows) ----------
// For performance, accumulate per-step counts into dense rows of length `bins`.
// Bins are known at run time (Graph::TotalSize + 1) and small in our targets.
using StepDense  = std::vector<std::vector<std::uint64_t>>; // step -> row (bins length)

// Merge helper (used by OpenMP to reduce thread‑local partials) — assumes rows
// are either empty or sized to `bins` consistently across threads.
inline void merge_step_dense(StepDense& dst, const StepDense& src) {
    if (src.size() > dst.size()) dst.resize(src.size());
    for (std::size_t s = 0; s < src.size(); ++s) {
        const auto& sr = src[s];
        if (sr.empty()) continue;
        auto& dr = dst[s];
        if (dr.empty()) { dr = sr; continue; }
        const std::size_t B = dr.size();
        for (std::size_t j = 0; j < B; ++j) dr[j] += sr[j];
    }
}

// Declare a custom OpenMP reduction for StepDense
#pragma omp declare reduction (merge_step_dense : sim::StepDense : \
    sim::merge_step_dense(omp_out, omp_in)) initializer(omp_priv = sim::StepDense{})

// Optional: convert to dense if you need a matrix later
struct Heatmap {
    std::size_t bins{0};
    std::size_t rows{0};
    std::vector<std::uint64_t> data; // row-major rows × bins
};
inline Heatmap to_dense(const StepDense& dense, std::size_t bins) {
    Heatmap out; out.bins = bins; out.rows = dense.size();
    out.data.assign(out.rows * out.bins, 0ull);
    for (std::size_t r = 0; r < out.rows; ++r) {
        const auto& row = dense[r];
        if (row.empty()) continue;
        std::copy(row.begin(), row.begin() + std::min<std::size_t>(row.size(), out.bins),
                  out.data.begin() + static_cast<std::ptrdiff_t>(r * out.bins));
    }
    return out;
}

// ---------- Streamed heatmap builder ----------
// ProgressCb: void(std::size_t done, std::size_t total)
template <class Graph, class SeedRng, class ProgressCb>
    requires GraphLike<Graph, SeedRng>
inline StepDense
run_jobs_heatmap_streamed(const JobConfig& cfg_in, SeedRng& master_rng, ProgressCb&& on_progress) {
    const std::size_t J = (cfg_in.jobs == 0) ? 1 : cfg_in.jobs;
    const int NT        = (cfg_in.threads > 0) ? cfg_in.threads : omp_get_max_threads();
    const std::size_t bins = static_cast<std::size_t>(Graph::TotalSize) + 1;

    // Deterministic per-job seeds (no RNG races)
    std::vector<std::uint64_t> seeds(J);
    for (std::size_t i = 0; i < J; ++i) seeds[i] = core::splitmix_hash(master_rng());

    std::atomic<std::size_t> done{0};

    StepDense result; // OpenMP gives each thread a private copy (via reduction)

    #pragma omp parallel for schedule(static) num_threads(NT) reduction(merge_step_dense: result)
    for (std::ptrdiff_t j = 0; j < static_cast<std::ptrdiff_t>(J); ++j) {
        core::Xoshiro256ss rng(seeds[static_cast<std::size_t>(j)]);
        Graph g;

        // Stream increments directly as the process evolves
        sim::run_schelling_process_visit(g, cfg_in.density, rng,
            [&](std::size_t s, core::count_t unhappy) {
                std::size_t u = static_cast<std::size_t>(unhappy);
                if (u >= bins) u = bins - 1;          // defensive clamp
                if (s >= result.size()) result.resize(s + 1);
                auto& row = result[s];
                if (row.empty()) row.assign(bins, 0ull);
                row[u] += 1ull;                        // thread‑local (no locks)
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
