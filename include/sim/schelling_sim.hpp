// schelling_sim.hpp
#pragma once
#include <cstdint>
#include <optional>
#include <random>
#include <limits>
#include "../graphs/lollipop_graph.hpp"   

namespace sim {

// --- Helpers ---------------------------------------------------------------

// Keep the clique/path shared boundary equal to path vertex #0.
inline void resync_boundary_from_path0(::graphs::LollipopGraph& G) {
    if (G.path_size() == 0) {
        G.set_shared_boundary(false, false);
        return;
    }
    const auto p0 = G.get_color(G.clique_size()); // global index of path[0]
    G.set_shared_boundary(p0.has_value(), p0.value_or(false));
}

// Place `color` at global vertex `v` (v in [0..size)).
inline void emplace_at(::graphs::LollipopGraph& G, std::uint64_t v, bool color) {
    if (v < G.clique_size()) {
        // Clique: add one of this color (counts-first).
        (void)G.clique().try_add(color);
    } else {
        // Path: set occupied + color at local index
        const std::uint64_t pv = v - G.clique_size();
        G.path_set_occupied(pv);
        G.path_set_color(pv, color);
        if (pv == 0) resync_boundary_from_path0(G);
    }
}

// Remove the agent currently at global vertex `v` (must be occupied).
inline void erase_at(::graphs::LollipopGraph& G, std::uint64_t v, bool color) {
    if (v < G.clique_size()) {
        (void)G.clique().try_remove(color);
    } else {
        const std::uint64_t pv = v - G.clique_size();
        G.path_clear_occupied(pv);
        if (pv == 0) resync_boundary_from_path0(G);
    }
}

// --- Initialization --------------------------------------------------------
// Randomly populate with exactly n0 zeros and n1 ones by repeatedly
// drawing a random unoccupied vertex and placing the color.
//
// Precondition: n0 + n1 <= G.size() and (ideally) n0 + n1 < G.size()
//               so that the dynamics (which require a vacancy) can run.
//
template <class URBG>
inline void populate_random(::graphs::LollipopGraph& G,
                            std::uint64_t n0, std::uint64_t n1,
                            URBG& rng)
{
    const std::uint64_t N = G.size();
    if (n0 + n1 > N) throw std::invalid_argument("n0+n1 exceeds capacity");

    // Clear boundary to path[0] state before we begin
    resync_boundary_from_path0(G);

    auto place_color = [&](bool color, std::uint64_t count) {
        for (std::uint64_t i = 0; i < count; ++i) {
            auto where = G.get_random_unoccupied(rng);   // uniform over all empties
            if (!where) throw std::runtime_error("No unoccupied slot available during initialization");
            emplace_at(G, *where, color);
        }
    };
    place_color(false, n0);
    place_color(true,  n1);
}

// --- Dynamics --------------------------------------------------------------
// Run the vacancy-moving Schelling process until no vertex is unhappy
// or until `max_steps` relocations have occurred. Returns #relocations.
//
// Each step:
//   1) draw `to` uniformly among all unhappy agents;
//   2) draw `from` uniformly among all unoccupied sites;
//   3) move color(to) to `from` (vacate `to`).
//
// Note: This is the “move to vacancy” variant (not exchange). See references. :contentReference[oaicite:2]{index=2}
template <class URBG>
inline std::uint64_t run_until_stable(::graphs::LollipopGraph& G,
                                      URBG& rng,
                                      std::uint64_t max_steps = std::numeric_limits<std::uint64_t>::max())
{
    std::uint64_t steps = 0;

    while (steps < max_steps) {
        if (G.unhappy_count() == 0) break;

        auto to = G.get_random_unhappy(rng);      // uniform over unhappy set
        if (!to) break;                            // nothing unhappy

        auto from = G.get_random_unoccupied(rng); // uniform over empties
        if (!from) {
            // No vacancy: the vacancy-moving Schelling dynamics can’t proceed.
            // (You asked for move-to-empty specifically.)
            break;
        }

        // Read color before erasing
        const auto c = G.get_color(*to);
        if (!c) {
            // Defensive: should not happen since `to` is drawn from occupied-unhappy set.
            continue;
        }

        // Move
        erase_at(G, *to,  *c);
        emplace_at(G, *from, *c);

        ++steps;
    }

    return steps;
}

} // namespace sim
