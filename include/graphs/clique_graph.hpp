#pragma once

#include <cstdint>
#include <array>
#include <cassert>

#include "graphs/core/graph_base.hpp"
#include <algorithm>
#include <numeric>

namespace graphs {

template <std::size_t K>
class CliqueGraph : public GraphBase<CliqueGraph<K>, K + 1> {
public:
    using Base = GraphBase<CliqueGraph<K>, K + 1>;
    using Base::num_vertices;
    using Base::tf;
    using Base::counts;
    using Base::happiness_;

    static_assert(K >= 1, "K must be at least 1");

    explicit CliqueGraph(index_t n) {
        num_vertices = n; counts.fill(0);
        counts[0] = n; // all unoccupied at start
        tf = 0;        // no disagreeing edges
    }
    explicit CliqueGraph(index_t n, const std::array<uint64_t, K + 1>& c) {
        num_vertices = n; counts = c;
        recompute_tf_from_counts_();
    }

    // Clique local disagreement count for vertex v:
    //   deg_disagree(v) = total_occupied() - counts[color(v)]
    // Fast path relies on counts and never scans neighbors.
    uint64_t local_frustration(index_t v)   const { (void)v; return this->total_occupied() - counts[get_color(v)]; }
    std::uint32_t occupied_neighbor_count(index_t v) const {
        (voccoid)v;
        const std::uint32_t self_occ = static_cast<std::uint32_t>(get_color(v) != 0);
        const std::uint32_t occ = static_cast<std::uint32_t>(this->total_occupied());
        return occ - self_occ;
    }

    // Use GraphBase::total_frustration(), which returns cached tf. We keep tf hot via change_color.

    void set_colors(const std::array<uint64_t, K + 1>& color_counts) { counts = color_counts; recompute_tf_from_counts_(); }

    /**
     * @brief Returns the color index associated with a given vertex index.
     *
     * This function maps a vertex index `v` to its corresponding color by using a prefix sum
     * of the `counts` array, where `counts[0]` represents unoccupied vertices and subsequent
     * elements represent the number of vertices for each color. The prefix sum array is used
     * to determine the color boundaries, and `std::upper_bound` is used to find the color
     * whose range contains the given vertex index.
     *
     * @param v The vertex index to map to a color.
     * @return index_t The color index corresponding to the vertex.
     */
    index_t get_color(index_t v) const {
        // Map index to color by counts prefix; counts[0] is unoccupied
        std::array<uint64_t, K + 1> prefix{};
        std::partial_sum(counts.begin(), counts.end(), prefix.begin());
        auto it = std::upper_bound(prefix.begin(), prefix.end(), static_cast<uint64_t>(v));
        return static_cast<index_t>(std::distance(prefix.begin(), it));
    }

    template <typename Func>
    auto sum_over_neighbors(index_t v, Func&& func) const {
        const auto& self = static_cast<const Derived&>(*this);
        auto sum = decldtype(func(v, index_t{})){};
        for (auto c = 1; c <= K; ++c) {
            sum += counts[get_color(v)]*func(v);
        }
        return sum;
    }        

    // O(1) tf maintenance — do not replace with recompute.
    // Handles all three cases using pre-change counts:
    //   a->b (a>0,b>0): tf += counts[a] - counts[b] - 1
    //   a->0:          tf += counts[a] - total_occupied()
    //   0->b:          tf += total_occupied() - counts[b]
    // Counts are updated after delta; no runtime guards.
    void change_color(index_t v, uint32_t c, uint32_t c_original) {
        (void)v; // storage-less model via counts
        if (c == c_original) return;
        if (c == 0) {
            tf -= this->total_occupied() - counts[c_original];
            --counts[c_original];
            ++counts[0];
            return;
        }
        if (c_original == 0) {
            tf += this->total_occupied() - counts[c];
            ++counts[c];
            --counts[0];
            return;
        }
        // Original fast rule (nonzero->nonzero expected usage)
        tf += counts[c_original] - counts[c] - 1;
        --counts[c_original];
        ++counts[c];
    }

    void change_color(index_t v, uint32_t c) { change_color(v, c, static_cast<uint32_t>(get_color(v))); }

    std::uint64_t color_count(std::uint32_t c) const { return counts[c]; }
private:
    void recompute_tf_from_counts_() {
        const std::uint64_t occ = this->total_occupied();
        std::uint64_t sumsq = 0;
        for (std::size_t k = 1; k <= K; ++k) sumsq += counts[k] * counts[k];
        tf = (occ * occ - sumsq) / 2ULL;
    }
   
};

} // namespace graphs
