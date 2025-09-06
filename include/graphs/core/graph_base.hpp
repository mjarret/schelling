// graph_base.hpp
#pragma once
#include <cstddef>
#include <cstdint>

using index_t = std::size_t;

template <class Derived>
class GraphBase {
    public:
    GraphBase(size_t n, std::array<uint64_t, K> c) : num_vertices(n), counts(c) {}
    
    std::size_t size() const { return num_vertices; }
    index_t total_occupied() const { return num_vertices - counts[0]; }

    // bool         is_occupied(std::size_t v) const;
    // std::uint32_t get_color(std::size_t v) const;
    // std::uint32_t local_frustration(std::size_t v, std::uint32_t color) const;
    // std::uint32_t occupied_neighbor_count(std::size_t v) const;
    
    std::uint64_t total_frustration() const {
        const auto& self = static_cast<const Derived&>(*this);
        if (tf == static_cast<uint64_t>(-1)) {
            tf = 0;
            for (size_t i = 0; i < num_vertices; ++i) {
                tf += self.local_frustration(i);
            }
            tf /= 2; // each edge counted twice
        }
        return tf;
    }

    protected:
        index_t num_vertices;
        std::array<uint64_t, K> counts{}; // one number per color
        uint64_t tf = -1; // -1 means "needs recompute"
};
