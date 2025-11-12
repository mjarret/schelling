// sim.hpp — generic Schelling process helpers over GraphLike graphs
#pragma once
#include <concepts>
#include <vector>
#include <algorithm>
#include "sim/graph_concepts.hpp"
#include "core/config.hpp"
#include "sim/init.hpp"

namespace sim {


// Initialization helpers are provided by sim/init.hpp


template <class G, class URBG>
    requires GraphLike<G, URBG>
inline core::count_t schelling_step(G& graph, [[maybe_unused]] double density, URBG& rng) {
    const core::size_t from = graph.get_unhappy(rng);
    const core::size_t to   = graph.get_unoccupied(rng);
    graph.place_agent(to, graph.pop_agent(from));
    return graph.unhappy_count();
} 


// run_schelling_process removed in favor of run_schelling_process

template <class G, class URBG>
    requires GraphLike<G, URBG>
inline size_t run_schelling_process(G& graph, double density, URBG& rng) {
    initialize_graph(graph, density, rng);
    std::size_t hitting_time = 0;
    if (graph.unhappy_count() == 0) return 0;
    while (schelling_step(graph, density, rng) > 0) ++hitting_time;
    return hitting_time;
}


} // namespace sim
