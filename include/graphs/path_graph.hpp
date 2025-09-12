#pragma once

#include <cstdint>
#include <array>

#include "core/bitpack.hpp"
#include "graphs/core/graph_base.hpp"

namespace graphs {

template <std::size_t K>
class PathGraph : public GraphBase<PathGraph<K>, K + 1> {
public:
    static constexpr std::size_t Bits = core::BitsForK<K>::value;
    using Base = GraphBase<PathGraph<K>, K + 1>;
    using Base::num_vertices;
    using Base::counts;
    using Base::tf;

    explicit PathGraph(index_t n) {
        num_vertices = n;
        colors_.resize(n);
        counts.fill(0);
        counts[0] = n; // initially all unoccupied
        tf = 0; // no disagreeing edges initially
    }

    index_t size() const { return num_vertices; }
    bool is_occupied(index_t v) const { return get_color(v) != 0; }

    std::uint32_t get_color(index_t v) const {
        if (v >= num_vertices) return 0;
        return colors_.get(v);
    }

    void set_color(index_t v, std::uint32_t c) { change_color(v, c, get_color(v)); }

    // Number of disagreeing occupied neighbors for current color at v.
    // Branch-choice rationale:
    // - We deliberately use short-circuit (&&) instead of fully branchless
    //   arithmetic because it benchmarks faster on our expected occupancy mix.
    // - get_color(v-1)/get_color(v+1) are allowed to "wrap"; get_color returns 0
    //   out of range, so we avoid explicit bounds branches.
    std::uint64_t local_frustration(index_t v) const {
        const std::uint32_t c = get_color(v);
        return (c!=0) * ((get_color(v-1)!=0 && get_color(v-1)!=c) + (get_color(v+1)!=0 && get_color(v+1)!=c));
    }

    // Number of occupied neighbors (ignores color equality).
    // Uses the same "wrap to 0" bounds trick as local_frustration to avoid
    // extra branches or guards in the hot path.
    std::uint32_t occupied_neighbor_count(index_t v) const {
        return (get_color(v - 1) != 0) + (get_color(v + 1) != 0);
    }

    // Maintain tf by removing/adding the local contribution. No scanning.
    // Keep exactly this pattern; replacing it with invalidation+recompute
    // regresses perf significantly in tight loops.
    void change_color(index_t v, std::uint32_t c, std::uint32_t c_original = 0) {
        if (c_original == 0) c_original = get_color(v);
        if (c == c_original) return;

        // Original, fast local update: remove old local contribution, apply new one
        tf -= local_frustration(v);
        --counts[c_original];
        ++counts[c];
        colors_.set(v, c);
        tf += local_frustration(v);
    }

    // We keep tf hot via change_color. The base's total_frustration() should not
    // be needed in steady state; it remains as a correctness fallback.

private:
    core::BitPackedVector<Bits> colors_;
};

} // namespace graphs
