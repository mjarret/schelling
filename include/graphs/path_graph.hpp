#pragma once

#include <cstdint>
#include <array>

#include "core/bitpack.hpp"
#include "graphs/core/graph_base.hpp"

namespace graphs {

template <std::size_t K>
class PathGraph : public GraphBase<PathGraph<K>, K> {
public:
    static constexpr std::size_t Bits = core::BitsForK<K>::value;
    using Base = GraphBase<PathGraph<K>, K>;
    using Base::num_vertices;
    using Base::counts;
    using Base::tf;

    explicit PathGraph(index_t n) {
        num_vertices = n;
        colors_.resize(n);
        counts.fill(0);
        counts[0] = n; // initially all unoccupied
        tf = static_cast<std::uint64_t>(-1);
    }

    index_t size() const { return num_vertices; }
    bool is_occupied(index_t v) const { return get_color(v) != 0; }

    std::uint32_t get_color(index_t v) const {
        if (v >= num_vertices) return 0;
        return colors_.get(v);
    }

    void set_color(index_t v, std::uint32_t c) {
        const std::uint32_t old = get_color(v);
        if (c == old) return;
        colors_.set(v, c);
        if (old <= K) { if (counts[old] > 0) --counts[old]; }
        if (c <= K) { ++counts[c]; }
        tf = static_cast<std::uint64_t>(-1);
    }

    // Number of disagreeing occupied neighbors for current color at v
    std::uint64_t local_frustration(index_t v) const {
        const std::uint32_t c = get_color(v);
        if (c == 0) return 0;
        std::uint64_t d = 0;
        if (v > 0) {
            const std::uint32_t nl = get_color(v - 1);
            d += (nl != 0 && nl != c);
        }
        if (v + 1 < num_vertices) {
            const std::uint32_t nr = get_color(v + 1);
            d += (nr != 0 && nr != c);
        }
        return d;
    }

    void change_color(index_t v, std::uint32_t c, std::uint32_t c_original = 0) {
        if (c_original == 0) c_original = get_color(v);
        if (c == c_original) return;
        colors_.set(v, c);
        if (c_original <= K) { if (counts[c_original] > 0) --counts[c_original]; }
        if (c <= K) { ++counts[c]; }
        tf = static_cast<std::uint64_t>(-1);
    }

private:
    core::BitPackedVector<Bits> colors_;
};

} // namespace graphs
