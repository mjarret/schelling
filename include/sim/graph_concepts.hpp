// graph_concepts.hpp — minimal graph requirements for Schelling sims
#pragma once

#include <concepts>
#include "core/config.hpp"

namespace sim {

// URBG is the random engine type used by callers (e.g., Xoshiro256ss).
// A Graph type G models GraphLike if it provides the minimal API consumed
// by the Schelling simulation helpers in sim/sim.hpp.
using size_t = core::size_t;
using count_t = core::count_t;

template <class G, class URBG>
concept GraphLike = requires(G& g, const G& cg, size_t idx, bool c, URBG& rng) {
    // Capacity known at compile time
    { G::TotalSize } -> std::convertible_to<size_t>;

    // Counts and random picks
    { cg.unhappy_count() } -> std::convertible_to<count_t>;
    { cg.get_unhappy(rng) } -> std::convertible_to<size_t>;
    { cg.get_unoccupied(rng) } -> std::convertible_to<size_t>;

    // Mutations
    { g.place_agent(idx, c) } -> std::same_as<void>;
    { g.pop_agent(idx) } -> std::convertible_to<bool>;
};

} // namespace sim
