#pragma once

#include <cstdint>
#include <array>
#include <cassert>

#include "core/bitpack.hpp"
#include "core/graph_base.hpp"

namespace graphs {

template <std::size_t K>
class PathGraph : public GraphBase<PathGraph<K>> {
public:
    static constexpr std::size_t Bits = core::BitsForK<K>::value;
    using index_t = std::size_t;
    using GraphBase::tf;
    using GraphBase::counts;
    using GraphBase::occupied;

    explicit PathGraph(index_t n) : num_vertices(n) {
        colors_.resize(num_vertices, 0);
        tf = 0; 
    }

    index_t size() const { return num_vertices; }
    bool is_occupied(index_t v) const { return get_color(v) != 0; }
    index_t total_occupied() const {
        if (occupied != -1) return occupied;
        for(index_t i = 0; i < num_vertices; ++i) {
            if(colors_.get(i) != 0) ++occupied;
        }
        return occupied;
    }    
    uint32_t get_color(index_t v) const {
        if(v >= num_vertices) return 0;
        return colors_.get(v);
    }
    void set_color(index_t v, uint32_t c) { assert(c < K); colors_.set(v, c); }
    index_t neighbors(index_t v) const { return is_occupied(v-1) + is_occupied(v+1); }

    // Local frustration (occupancy-only model, unoccupied=0): occ(v) * (occ(v-1) + occ(v+1))
    uint64_t local_frustration(index_t v, uint32_t /*unoccupied*/ = 0) const {
        const uint32_t color = colors.get(v);
        return color * (v > 0 ? (colors.get(v - 1) != color) : 0) + (v + 1 < num_vertices ? (colors.get(v + 1) != color) : 0);
    }

    // Total frustration: memoized; recomputed on initialization and adjusted on moves
    uint64_t total_frustration(uint32_t unoccupied = 0) const {
        index_t total_frustration = 0;
        for(size_t i = 0; i < num_vertices; ++i) {
            total_frustration += local_frustration(i);
        }
        tf = total_frustration / 2;
        return tf; // each edge counted twice
    }

    void change_color(index_t v, uint32_t c, uint32_t c_original = get_color(v)) {
        if (c == c_original) return; // No change

        // Adjust total frustration by removing old contribution and adding new one
        tf -= local_frustration(v);
        colors_.set(v, c);
        tf += local_frustration(v);
        occupied += (c != 0) - (c_original != 0);
    }

private:
    core::BitPackedVector<Bits> colors_;
};

} // namespace graphs
