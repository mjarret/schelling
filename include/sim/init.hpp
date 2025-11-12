// init.hpp — graph initialization strategies (no logic change)
#pragma once

#include <algorithm>
#include <random>

#include "core/bitset.hpp"
#include "core/rng.hpp"
#include "sim/graph_concepts.hpp"

namespace sim {

// Rejection-sampling initializer:
// Picks uniform indices in [0, N) until exactly K = floor(density*N)
// unique positions are chosen. Uses a local bitset to track picks and
// alternates colors by placement order. Expected O(N) trials for
// constant density, no scans or auxiliary containers.
template <class G, class URBG>
    requires GraphLike<G, URBG>
inline void initialize_graph_rejection(G& graph, double density, URBG& rng) {
    using count_t = core::count_t;
    using size_t  = core::size_t;
    const count_t N = static_cast<count_t>(G::TotalSize);
    const count_t K = static_cast<count_t>(static_cast<double>(N) * density);

    core::bitset<G::TotalSize> chosen; // all zeros
    count_t placed = 0;
    if (N == 0 || K == 0) return;
    std::uniform_int_distribution<size_t> pick(0, static_cast<size_t>(N - 1));
    while (placed < K) {
        const std::size_t i = static_cast<std::size_t>(pick(rng));
        if (!chosen.test(i)) {
            chosen.set(i);
            graph.place_agent(static_cast<size_t>(i), static_cast<bool>(placed & 1));
            ++placed;
        }
    }
}

// Construct a random occupancy bitstring of Hamming weight ~= density * N
// using Floyd's algorithm to avoid scans. Returns a core::bitset<G::TotalSize>
// with 1 = occupied, 0 = unoccupied.
template <class G, class URBG>
inline core::bitset<G::TotalSize>
make_random_occupancy_bitset(double density, URBG& rng) {
    using count_t = core::count_t;
    const count_t N = static_cast<count_t>(G::TotalSize);
    const count_t K = static_cast<count_t>(static_cast<double>(N) * density);

    core::bitset<G::TotalSize> out; // all zeros
    if (K == 0) return out;
    if (K >= N) { out.set(); return out; }

    for (count_t j = N - K; j < N; ++j) {
        const std::uint64_t r = core::uniform_bounded(rng, static_cast<std::uint64_t>(j + 1));
        const std::size_t t = static_cast<std::size_t>(r);
        if (out.test(t)) out.set(static_cast<std::size_t>(j));
        else             out.set(t);
    }
    return out;
}

// Initialize a graph using the permuted-bitstring approach above. Colors
// alternate deterministically with placement order to match legacy behavior.
template <class G, class URBG>
    requires GraphLike<G, URBG>
inline void initialize_graph_permuted(G& graph, double density, URBG& rng) {
    using count_t = core::count_t;
    const auto occ = make_random_occupancy_bitset<G>(density, rng);
    const count_t total = static_cast<count_t>(G::TotalSize);
    count_t placed = 0;
    for (count_t i = 0; i < total; ++i) {
        if (occ.test(static_cast<std::size_t>(i))) {
            graph.place_agent(static_cast<core::size_t>(i), static_cast<bool>(placed & 1));
            ++placed;
        }
    }
}

// Default initializer delegating to rejection (current default)
template <class G, class URBG>
    requires GraphLike<G, URBG>
inline void initialize_graph(G& graph, double density, URBG& rng) {
    initialize_graph_rejection<G>(graph, density, rng);
}

} // namespace sim

