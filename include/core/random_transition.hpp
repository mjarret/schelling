// random_transition.hpp
#pragma once

#include <cstddef>
#include <utility>

namespace core {

// Simple proposal for a move: pick two vertices at random.
struct Transition {
    std::size_t from;
    std::size_t to;
};

// Returns a proposed (from, to) pair without mutating the graph.
// Chooses uniformly at random; ensures from != to when n > 1.
template <class Graph, class Rng>
inline std::pair<std::size_t, std::size_t> random_transition(const Graph& g, Rng& rng) {
    const std::size_t n = g.size();
    std::size_t from = rng.uniform_index(n);
    while (g.local_frustration(from) == 0) {
        from = rng.uniform_index(n);
    }
    
    std::size_t to = rng.uniform_index(n);
    while (g.get_color(to) != 0) {
        to = rng.uniform_index(n);
    }
    return {from, to};
}

} // namespace core
