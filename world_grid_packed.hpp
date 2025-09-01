// world_grid_packed.hpp
#pragma once
#include "bitwords.hpp"
#include <cstdint>
#include <vector>
#include <cassert>

template <unsigned TypeBitsParam = 1, class WordT = DefaultWord>
class WorldGridPacked {
    static_assert(TypeBitsParam >= 1 && TypeBitsParam <= 8, "TypeBitsParam in [1..8]");
    static constexpr unsigned OCC_BITS = 1;
    static constexpr unsigned TYPE_BITS_LOCAL = TypeBitsParam;
    static constexpr unsigned BITS_PER = OCC_BITS + TYPE_BITS_LOCAL;
    static constexpr unsigned WB       = WORD_BITS<WordT>;

    uint32_t          n_vertices_;
    std::vector<WordT> limbs_; // contiguous, single allocation

    inline uint64_t bitpos_vertex(uint32_t v) const { return uint64_t(v) * BITS_PER; }
    inline       WordT* limb_data()       { return limbs_.data(); }
    inline const WordT* limb_data() const { return limbs_.data(); }

public:
    explicit WorldGridPacked(uint32_t n_vertices)
        : n_vertices_(n_vertices)
        , limbs_((uint64_t(n_vertices) * BITS_PER + WB - 1) / WB, WordT(0)) {}

    inline uint32_t num_vertices() const { return n_vertices_; }

    inline bool occupied(uint32_t v) const {
        const uint64_t base = bitpos_vertex(v);
        return bitpack::get_bits<WordT>(limb_data(), base, OCC_BITS) != 0;
    }

    inline uint32_t type_of(uint32_t v) const {
        const uint64_t base = bitpos_vertex(v) + OCC_BITS;
        return static_cast<uint32_t>(bitpack::get_bits<WordT>(limb_data(), base, TYPE_BITS_LOCAL));
    }

    inline void clear_vertex(uint32_t v) {
        const uint64_t base = bitpos_vertex(v);
        bitpack::set_bits<WordT>(limb_data(), base, BITS_PER, WordT(0));
    }

    inline void set_vertex(uint32_t v, uint32_t type) {
        const uint64_t base = bitpos_vertex(v);
        bitpack::set_bits<WordT>(limb_data(), base, OCC_BITS, WordT(1));
        bitpack::set_bits<WordT>(limb_data(), base + OCC_BITS, TYPE_BITS_LOCAL, WordT(type));
    }

    inline void move(uint32_t src, uint32_t dst) {
        const uint32_t t = type_of(src);
        clear_vertex(src);
        set_vertex(dst, t);
    }

    template <typename F>
    inline void for_each_agent(F&& f) const {
        for (uint32_t v = 0; v < n_vertices_; ++v) {
            if (occupied(v)) f(v, type_of(v));
        }
    }

    inline uint32_t num_agents() const {
        uint32_t cnt = 0;
        for (uint32_t v = 0; v < n_vertices_; ++v) if (occupied(v)) ++cnt;
        return cnt;
    }
};
