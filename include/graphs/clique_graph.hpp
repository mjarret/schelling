#pragma once

#include <cstdint>
#include <array>
#include <cassert>

#include "core/graph_base.hpp"

namespace graphs {

template <std::size_t K>
class CliqueGraph : public GraphBase<CliqueGraph<K>> {
public:
    using GraphBase::num_vertices;
    using GraphBase::tf;
    using GraphBase::counts;

    static_assert(K >= 1, "K must be at least 1");

    explicit CliqueGraph(index_t n, std::array<uint64_t, K> c) : num_vertices(n), counts(c) {}

    uint64_t local_frustration(index_t v)   const { return total_occupied() - counts[get_color(v)]; }
    uint64_t total_frustration() const {
        if (tf == static_cast<uint64_t>(-1)) return tf;
        tf = total_occupied() * total_occupied(); // (n - counts[0])^2
        for (size_t k = 1; k < K; ++k) {
            tf -= counts[k] * counts[k]; // - sum(counts[k]^2)
        }
        tf /= 2; // each edge counted twice
        return tf; // Is now = (edges^2 - local_edges^2)) / 2
    }

    void set_colors(const std::array<uint64_t, K>& color_counts) { counts = color_counts; }

    index_t get_color(index_t v) const {
        // Use std::partial_sum and std::upper_bound for efficiency and clarity
        std::array<uint64_t, K> prefix{};
        std::partial_sum(counts.begin(), counts.end(), prefix.begin());
        auto it = std::upper_bound(prefix.begin(), prefix.end(), v);
        return static_cast<index_t>(std::distance(prefix.begin(), it));
    }
    
    void change_color(index_t v, uint32_t c, uint32_t c_original = get_color(v)) {
        // Adjust total frustration by removing old contribution and adding new one
        // total_frustration -= total_occupied() - counts[c_original];
        // total_frustration += total_occupied() - (counts[c] + 1); // -1 because v itself is changing color
        total_frustration += counts[c_original] - counts[c] - 1;
        
        // Update color counts
        --counts[c_original];
        ++counts[c];        
    }
   
};

} // namespace graphs
