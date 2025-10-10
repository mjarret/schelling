// job_handler.hpp — OpenMP-only, deterministic fan-out + per-step histograms
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <atomic>
#include <limits>
#include <algorithm>

#include <omp.h>

#include "core/rng.hpp"
#include "sim/graph_concepts.hpp"
#include "sim/sim.hpp"

namespace sim {

// --------------------------------- Config / Stats -----------------------------

struct JobConfig {
    std::size_t jobs{1};      // 0 treated as 1
    double      density{0.8};
    int         threads{0};   // 0 => use omp_get_max_threads()
};

struct JobStats {
    std::uint64_t moves{0};
    std::uint64_t final_unhappy{0};
};

// --------------------------------- Utilities ----------------------------------

namespace detail {

inline int pick_num_threads(int requested) {
    return (requested > 0) ? requested : omp_get_max_threads();
}

// Derive per-job 64-bit seeds on the main thread (race-free, deterministic).
template <class SeedRng>
inline std::vector<std::uint64_t> derive_seeds(std::size_t n, SeedRng& master_rng) {
    std::vector<std::uint64_t> seeds(n);
    for (std::size_t i = 0; i < n; ++i) seeds[i] = core::splitmix_hash(master_rng());
    return seeds;
}

// Progress: relaxed atomic increment + serialized callback (UIs are often not thread-safe).
template <class ProgressCb>
inline void tick_progress(ProgressCb&& on_progress,
                          std::atomic<std::size_t>& done,
                          std::size_t total) {
    const std::size_t now = done.fetch_add(1, std::memory_order_relaxed) + 1;
    #pragma omp critical(sim_progress)
    { on_progress(now, total); }
}

} // namespace detail

// -------------------------- Per-step banded histogram -------------------------

// Compact histogram for one step s. Counts bins in [lo, hi] in a tight vector.
struct BandedRow {
    std::size_t lo{std::numeric_limits<std::size_t>::max()};
    std::size_t hi{0};
    std::vector<std::uint64_t> band; // length = hi - lo + 1, when non-empty

    bool empty() const noexcept { return band.empty(); }

    // Increment bin 'u' by 1; expands left/right on demand.
    inline void bump(std::size_t u) {
        if (band.empty()) {
            lo = hi = u;
            band.assign(1, 1ull);
            return;
        }
        if (u < lo) {
            const std::size_t add = lo - u;
            std::vector<std::uint64_t> next(add + band.size(), 0ull);
            std::copy(band.begin(), band.end(), next.begin() + add);
            next[0] += 1ull;          // bump at new leftmost
            band.swap(next);
            lo = u;
            return;
        }
        if (u > hi) {
            const std::size_t grow = u - hi;
            band.resize(band.size() + grow, 0ull);
            hi = u;
            band[hi - lo] += 1ull;    // bump at new rightmost
            return;
        }
        band[u - lo] += 1ull;
    }

    // Merge 'src' into '*this' (+=). After the call, '*this' covers the union range.
    inline void merge_from(const BandedRow& src) {
        if (src.empty()) return;
        if (empty()) { *this = src; return; }

        if (src.lo < lo) {
            const std::size_t add = lo - src.lo;
            std::vector<std::uint64_t> next(add + band.size(), 0ull);
            std::copy(band.begin(), band.end(), next.begin() + add);
            band.swap(next);
            lo = src.lo;
        }
        if (src.hi > hi) {
            band.resize(band.size() + (src.hi - hi), 0ull);
            hi = src.hi;
        }
        const std::size_t off = src.lo - lo;
        for (std::size_t i = 0; i < src.band.size(); ++i)
            band[off + i] += src.band[i];
    }
};

// Heatmap as a vector of per-step banded histograms.
struct BandedHeatmap {
    std::size_t bins{0};            // total possible bins = Graph::TotalSize + 1
    std::vector<BandedRow> steps;   // steps[s] = histogram for step s
};

// Dense/compat form (matches your original Heatmap API).
struct Heatmap {
    std::size_t bins{0};
    std::size_t rows{0};
    std::vector<std::uint64_t> data;   // row-major (rows × bins)
};

inline Heatmap to_dense(const BandedHeatmap& src) {
    Heatmap out;
    out.bins = src.bins;
    out.rows = src.steps.size();
    out.data.assign(out.rows * out.bins, 0ull);
    for (std::size_t r = 0; r < out.rows; ++r) {
        const auto& row = src.steps[r];
        if (row.empty()) continue;
        const std::size_t start = r * out.bins + row.lo;
        const std::size_t n = std::min(row.band.size(), out.bins - row.lo);
        std::copy_n(row.band.begin(), n, out.data.begin() + static_cast<std::ptrdiff_t>(start));
    }
    return out;
}

// ----------------------------------- Runners ----------------------------------

// Fan-out with stats (unchanged semantics).
// ProgressCb signature: void(std::size_t done, std::size_t total)
template <class Graph, class SeedRng, class ProgressCb>
    requires GraphLike<Graph, SeedRng>
inline std::vector<JobStats>
run_jobs(const JobConfig& cfg_in, SeedRng& master_rng, ProgressCb&& on_progress) {
    const std::size_t N  = (cfg_in.jobs == 0) ? 1 : cfg_in.jobs;
    const int NT         = detail::pick_num_threads(cfg_in.threads);
    auto seeds           = detail::derive_seeds(N, master_rng);

    std::vector<JobStats> out(N);
    std::atomic<std::size_t> done{0};

    #pragma omp parallel for schedule(static) num_threads(NT)
    for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(N); ++i) {
        core::Xoshiro256ss rng(seeds[static_cast<std::size_t>(i)]);
        auto history = sim::run_schelling_process(Graph{}, cfg_in.density, rng);

        JobStats s{};
        if (!history.empty()) {
            s.moves         = static_cast<std::uint64_t>(history.size() - 1);
            s.final_unhappy = static_cast<std::uint64_t>(history.back());
        }
        out[static_cast<std::size_t>(i)] = s;

        detail::tick_progress(on_progress, done, N);
    }
    return out;
}

// Overload with a no-op progress callback
template <class Graph, class SeedRng>
    requires GraphLike<Graph, SeedRng>
inline std::vector<JobStats>
run_jobs(const JobConfig& cfg, SeedRng& master_rng) {
    auto noop = [](std::size_t, std::size_t) {};
    return run_jobs<Graph>(cfg, master_rng, noop);
}

// Build a banded (sparse) heatmap: vector< histogram-per-step >
// ProgressCb signature: void(std::size_t done, std::size_t total)
template <class Graph, class SeedRng, class ProgressCb>
    requires GraphLike<Graph, SeedRng>
inline BandedHeatmap
run_jobs_heatmap_banded(const JobConfig& cfg_in, SeedRng& master_rng, ProgressCb&& on_progress) {
    const std::size_t total_jobs = (cfg_in.jobs == 0) ? 1 : cfg_in.jobs;
    const std::size_t bins       = static_cast<std::size_t>(Graph::TotalSize) + 1;
    const int NT                 = detail::pick_num_threads(cfg_in.threads);
    auto seeds                   = detail::derive_seeds(total_jobs, master_rng);

    struct TLS { std::vector<BandedRow> rows; };
    std::vector<TLS> tls; // sized inside the parallel region to match the actual team size
    std::atomic<std::size_t> done{0};

    #pragma omp parallel num_threads(NT)
    {
        const int tid = omp_get_thread_num();
        const int T   = omp_get_num_threads();

        #pragma omp single
        { tls.resize(static_cast<std::size_t>(T)); }

        TLS& t = tls[static_cast<std::size_t>(tid)];

        #pragma omp for schedule(static)
        for (std::ptrdiff_t i = 0; i < static_cast<std::ptrdiff_t>(total_jobs); ++i) {
            core::Xoshiro256ss rng(seeds[static_cast<std::size_t>(i)]);
            auto history = sim::run_schelling_process(Graph{}, cfg_in.density, rng);

            const std::size_t steps = history.size();
            if (steps > t.rows.size()) t.rows.resize(steps);
            for (std::size_t s = 0; s < steps; ++s) {
                std::size_t u = history[s];
                if (u >= bins) u = bins - 1; // defensive clamp
                t.rows[s].bump(u);
            }

            detail::tick_progress(on_progress, done, total_jobs);
        }
    }

    // Reduce TLS -> global banded heatmap
    BandedHeatmap hm;
    hm.bins = bins;

    std::size_t max_rows = 0;
    for (const auto& t : tls) if (t.rows.size() > max_rows) max_rows = t.rows.size();
    hm.steps.resize(max_rows);

    for (std::size_t r = 0; r < max_rows; ++r) {
        BandedRow acc;
        for (const auto& t : tls)
            if (r < t.rows.size()) acc.merge_from(t.rows[r]);
        hm.steps[r] = std::move(acc);
    }
    return hm;
}

// Compatibility wrapper: same signature/name as your original dense heatmap.
template <class Graph, class SeedRng, class ProgressCb>
    requires GraphLike<Graph, SeedRng>
inline Heatmap
run_jobs_heatmap(const JobConfig& cfg, SeedRng& master_rng, ProgressCb&& on_progress) {
    return to_dense(run_jobs_heatmap_banded<Graph>(cfg, master_rng, std::forward<ProgressCb>(on_progress)));
}

template <class Graph, class SeedRng>
    requires GraphLike<Graph, SeedRng>
inline Heatmap
run_jobs_heatmap(const JobConfig& cfg, SeedRng& master_rng) {
    auto noop = [](std::size_t, std::size_t) {};
    return run_jobs_heatmap<Graph>(cfg, master_rng, noop);
}

} // namespace sim
