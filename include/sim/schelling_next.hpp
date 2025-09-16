#pragma once

#include <cstdint>
#include <utility>
#include "core/random_transition.hpp"

namespace sim {

template <class Graph, class Rng>
/**
 * @brief Perform one Schelling move and return delta frustration.
 * @tparam Graph Graph type exposing size(), total_frustration(), get_color(), change_color().
 * @tparam Rng RNG type used by the proposer.
 * @param g Graph to mutate.
 * @param rng Random generator.
 * @return Delta in total frustration (after - before).
 */
inline std::int64_t schelling_next(Graph& g, Rng& rng) {
    const std::size_t n = g.size();
    if (n <= 1) return 0;

    auto [from, to] = core::random_transition(g, rng);
    const auto c_from = g.get_color(from);

    const std::uint64_t before = g.total_frustration();
    g.change_color(from, 0u, c_from);
    g.change_color(to, c_from, 0u);
    const std::uint64_t after = g.total_frustration();
    return static_cast<std::int64_t>(after) - static_cast<std::int64_t>(before);
}

}
