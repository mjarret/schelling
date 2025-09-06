// schelling_next.hpp
#pragma once

namespace sim {

template <class Graph, class Rng>
inline std::int64_t schelling_next(Graph& g, Rng& rng) {
    return g.schelling_next(rng);
}

} // namespace sim
