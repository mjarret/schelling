// schelling_next.hpp
#pragma once

#include <cstdint>
#include <utility>
#include "core/random_transition.hpp"

namespace sim {

// One Schelling move: pick an unhappy occupied vertex and move it to a random empty slot.
// Returns delta in total frustration (new - old). If no valid move found, returns 0.
template <class Graph, class Rng>
inline std::int64_t schelling_next(Graph& g, Rng& rng) {
    const std::size_t n = g.size();
    if (n <= 1) return 0;

    auto [from, to] = core::random_transition(g, rng);
    if (from == to) return 0;

    const auto c_from = g.get_color(from);
    if (c_from == 0) return 0; // nothing to move
    if (g.local_frustration(from) == 0) return 0; // already happy
    if (g.get_color(to) != 0) return 0; // not empty

    const std::uint64_t before = g.total_frustration();
    // Move via two color changes (avoid graph structure changes)
    g.change_color(from, 0u, static_cast<std::uint32_t>(c_from));
    g.change_color(to, static_cast<std::uint32_t>(c_from), 0u);
    const std::uint64_t after = g.total_frustration();
    return static_cast<std::int64_t>(after) - static_cast<std::int64_t>(before);
}

} // namespace sim
