// sim.hpp — generic Schelling process helpers over GraphLike graphs
#pragma once
#include <concepts>
#include <vector>
#include "sim/graph_concepts.hpp"
#include "core/config.hpp"

namespace sim {


template <class G, class URBG>
    requires GraphLike<G, URBG>
inline void initialize_graph(G& graph, double density, URBG& rng) {
    using size_t = core::size_t; using count_t = core::count_t;
    const count_t total = G::TotalSize;
    const count_t target = static_cast<count_t>(total * density);
    for (count_t i = 0; i < target; ++i) {
        const size_t to = graph.get_unoccupied(rng);
        graph.place_agent(to, i % 2);   // Alternate colors for initial placement
    }
}

template <class G, class URBG>
    requires GraphLike<G, URBG>
inline std::vector<core::count_t> run_schelling_process(G& graph, double density, URBG& rng) {
    initialize_graph(graph, density, rng);
    std::vector<core::count_t> history;
    history.push_back(graph.unhappy_count());
    while (graph.unhappy_count() > 0) {
        const size_t from = graph.get_unhappy(rng);
        const size_t to = graph.get_unoccupied(rng);
        graph.place_agent(to, graph.pop_agent(from));
        history.push_back(graph.unhappy_count());
    }
    return history;
}

} // namespace sim
