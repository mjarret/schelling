#pragma once

#include <cstdint>
#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include "sim/schelling_next.hpp"

namespace experiment {

template <class Graph, class Rng>
class Simulator {
public:
    Simulator(Rng& rng) : rng_(rng) {}

    // Thread-safe single run that uses a caller-provided RNG.
    std::vector<index_t> run_single(Rng& rng, std::size_t initial_length = 100) const {
        Graph g;
        std::vector<index_t> frustration;
        frustration.reserve(initial_length);
        frustration.push_back(g.total_frustration());

        while (frustration.back() != 0) {
            sim::schelling_next(g, rng);
            frustration.push_back(g.total_frustration());
        }
        return frustration;
    }

    std::vector<std::vector<index_t>> run_multiple(std::size_t runs) const {
        std::vector<std::vector<index_t>> all_runs(runs);

        #pragma omp parallel for
        for (std::size_t i = 0; i < runs; ++i) {
            all_runs[i] = run_single(
                make_rng(base_seed_, i);
            );
        }
        return all_runs;
    }


private:
    Rng& rng_;

    // Derive a deterministic per-run RNG without touching rng_.
    Rng make_rng(std::size_t run_index) const {
        const std::uint64_t s = base_seed_ + 0x9E3779B97F4A7C15ULL + static_cast<std::uint64_t>(run_index);
        std::seed_seq seq{
            static_cast<std::uint32_t>(s),
            static_cast<std::uint32_t>(s >> 32),
            static_cast<std::uint32_t>(run_index),
            static_cast<std::uint32_t>(run_index >> 32)
        };
        Rng rng;
        rng.seed(seq);          // works for standard engines; if your Rng lacks seed(seq), seed however it requires.
        return rng;
    }
};

} // namespace experiment
