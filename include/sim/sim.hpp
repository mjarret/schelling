// schelling_sim.hpp
#pragma once
#include <concepts>
#include <cstdint>
#include <vector>
#include "graphs/lollipop.hpp"
// Note: concepts header removed; remaining helpers don't require it.

namespace sim {

// ---------------- Lollipop-specific process helpers ----------------

// Initialize a lollipop by placing agents with alternating colors at random
// unoccupied vertices until 4/5 of the vertices are occupied.
// Precondition: the graph starts empty or with enough vacancies to reach the target.
template <std::size_t CS, std::size_t PL, class URBG>
inline void initialize_lollipop_four_fifths(graphs::LollipopGraph<CS, PL>& graph, URBG& rng) {
    const std::size_t total = graphs::LollipopGraph<CS, PL>::TotalSize;
    const std::size_t target = (total * 4u) / 5u;
    bool color = false;
    for (std::size_t i = 0; i < target; ++i) {
        const std::size_t to = graph.get_unoccupied(rng);
        graph.place_agent(to, color);
        color = !color;
    }
}

// Run the Schelling process on a lollipop graph: initialize to 4/5 occupancy
// with alternating colors at random positions, then repeatedly move a random
// unhappy agent to a random vacancy until no unhappy agents remain.
template <std::size_t CS, std::size_t PL, class URBG>
inline std::vector<std::size_t> run_lollipop_process(graphs::LollipopGraph<CS, PL>& graph, URBG& rng) {
    initialize_lollipop_four_fifths(graph, rng);
    std::vector<std::size_t> history;
    history.push_back(graph.unhappy_count());
    while (graph.unhappy_count() > 0) {
        const std::size_t from = graph.get_unhappy(rng);
        const bool c = graph.pop_agent(from);
        const std::size_t to = graph.get_unoccupied(rng);
        graph.place_agent(to, c);
        history.push_back(graph.unhappy_count());
    }
    return history;
}

} // namespace sim
