#pragma once

#include <cstdint>
#include <array>
#include <cassert>

#include "graphs/core/graph_base.hpp"
#include <algorithm>
#include <numeric>

namespace graphs {

template <std::size_t K>
class CliqueGraph : public GraphBase<CliqueGraph<K>, K> {
public:
    using Base = GraphBase<CliqueGraph<K>, K>;
    using Base::num_vertices;
    using Base::tf;
    using Base::counts;

    static_assert(K >= 1, "K must be at least 1");

    explicit CliqueGraph(index_t n) { num_vertices = n; counts.fill(0); tf = static_cast<uint64_t>(-1); }
    explicit CliqueGraph(index_t n, const std::array<uint64_t, K>& c) { num_vertices = n; counts = c; tf = static_cast<uint64_t>(-1); }

    uint64_t local_frustration(index_t v)   const { (void)v; return this->total_occupied() - counts[get_color(v)]; }

    void set_colors(const std::array<uint64_t, K>& color_counts) { counts = color_counts; tf = static_cast<uint64_t>(-1); }

    index_t get_color(index_t v) const {
        // Use std::partial_sum and std::upper_bound for efficiency and clarity
        std::array<uint64_t, K> prefix{};
        std::partial_sum(counts.begin(), counts.end(), prefix.begin());
        auto it = std::upper_bound(prefix.begin(), prefix.end(), v);
        return static_cast<index_t>(std::distance(prefix.begin(), it));
    }
    
    void change_color(index_t v, uint32_t c, uint32_t c_original) {
        (void)v; // We model clique by counts only; v is not used for storage
        if (c == c_original) return;
        if (c_original < K+1) { if (counts[c_original] > 0) --counts[c_original]; }
        if (c < K+1) { ++counts[c]; }
        tf = static_cast<uint64_t>(-1);
    }

    void change_color(index_t v, uint32_t c) { change_color(v, c, static_cast<uint32_t>(get_color(v))); }

    std::uint64_t color_count(std::uint32_t c) const { return counts[c]; }
   
};

} // namespace graphs
