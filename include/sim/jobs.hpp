// jobs.hpp — small job-runner utilities (seeding, bins, progress)
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <atomic>

#include "core/rng.hpp"

namespace sim {

// Seed J jobs deterministically using the master RNG (no races)
template <class SeedRng>
inline std::vector<std::uint64_t> seed_jobs(std::size_t J, SeedRng& master_rng) {
    std::vector<std::uint64_t> seeds(J);
    for (std::size_t i = 0; i < J; ++i) seeds[i] = core::splitmix_hash(master_rng());
    return seeds;
}

// Compute number of bins for a Graph (TotalSize + 1)
template <class Graph>
inline std::size_t calc_bins() {
    return static_cast<std::size_t>(Graph::TotalSize) + 1;
}

// Report progress inside a critical region
template <class ProgressCb>
inline void progress_tick(ProgressCb&& cb, std::atomic<std::size_t>& done, std::size_t total) {
    const std::size_t now = done.fetch_add(1, std::memory_order_relaxed) + 1;
    #pragma omp critical(sim_progress)
    { cb(now, total); }
}

} // namespace sim

