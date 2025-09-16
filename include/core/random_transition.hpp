#pragma once

#include <cstddef>
#include <utility>
#include "core/config.hpp"
#include "core/schelling_threshold.hpp"

namespace core {

/**
 * @brief Proposed move between two vertices.
 */
struct Transition {
    std::size_t from;
    std::size_t to;
};

/**
 * @brief Propose a random transition (from, to) without mutating the graph.
 * @tparam Graph Graph type exposing size(), relative_frustration(), get_color().
 * @tparam Rng RNG type exposing uniform_index(n).
 * @param g Graph instance (read-only operations).
 * @param rng Random generator.
 * @return Pair of indices (from, to) where from is the index of the unhappy agent and to is the index of the target.
 */
template <class ColoredGraph, class Rng>
inline std::pair<std::size_t, std::size_t> random_transition(const ColoredGraph& g, Rng& rng) {
    const std::size_t n = g.size();
    const std::size_t unhappy_count = g.unhappy_agent_count();
    const std::size_t unocc = n - g.total_occupied();

    if (unhappy_count == 0 || occ == n) return {}; // No valid move possible
    auto unhappy_index = rng.uniform_index(unhappy_count);
    auto unocc_index = rng.uniform_index(unocc);
    return {g.index_of(1, unhappy_index), g.index_of(0, unocc_index)};
}

}
