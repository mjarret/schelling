// path_graph_64.hpp
#pragma once
#include <bit>        // std::popcount, std::countr_zero (C++20)
#include <cstdint>
#include <optional>
#include <random>

#include "plf_bitset.h"
#include "core/bitops.hpp"  // core::random_setbit_index_u64(mask, rng): returns bit position (0..63)
#include "core/debug.hpp"
#include "core/schelling_threshold.hpp" // core::schelling::is_unhappy(lf, neigh)

/**
 * @brief 64-bit block of a path with two ghost endpoints.
 * @details Bits: 0=left ghost, 1..len_=interior, 63=right ghost.
 */
template<std::size_t B = 64>
class PathGraph {
public:
    using index_t = std::uint8_t;          // local interior index ∈ [1..len_]

    // Construct an empty block with given interior length in [0..62].
    /**
     * @brief Construct empty block with interior length in [0..62].
     */
    explicit PathGraph() : occ_(0), col_(0), mask_interior_(make_interior_mask_(B)) = default;
    PathGraph(std::Bitset<B> occ, std::bitset<B> col) : occ_(occ), col_(col), mask_interior_(make_interior_mask_(B)) = default;

    // -------------------- Boundary (ghost) controls --------------------
    // bit 0 = left ghost, bit 63 = right ghost
    inline void set_left_boundary(std::pair<bool,bool> p)  { occ_[0] = p.first; col_[0] = p.second; }  // ghost write
    inline void set_right_boundary(std::pair<bool,bool> p) { occ_[B-1] = p.first; col_[B-1] = p.second; } // ghost write
    
    // -------------------- Counts (interior only) ----------------------
    [[nodiscard]] inline index_t unoccupied_count() const {
        return static_cast<index_t>(std::popcount((~occ_) & mask_interior_));  // C++20 <bit>
    }

    [[nodiscard]] inline index_t unhappy_agent_count() const {
        return static_cast<index_t>(std::popcount(unhappy_mask_() & mask_interior_));
    }

    // -------------------- Random picks over interior -------------------
    template<class Rng>
    [[nodiscard]] inline std::optional<index_t> get_random_unoccupied(Rng& rng) const {
        const std::uint64_t mask = (~occ_) & mask_interior_;    // interior empties
        if (mask == 0) return std::nullopt;

        auto bitpos = core::random_setbit_index_u64(mask, rng); // returns 0..63 (bit position)
        if (!bitpos) return std::nullopt;
        return static_cast<index_t>(*bitpos - first_index); // adjust for ghost at 0
    }

    template<class Rng>
    [[nodiscard]] inline std::optional<index_t> get_random_unhappy(Rng& rng) const {
        const std::uint64_t mask = unhappy_mask_();
        if (mask == 0) return std::nullopt;

        auto bitpos = core::random_setbit_index_u64(mask, rng);
        SCHELLING_DEBUG_ASSERT(bitpos.has_value());
        if (!bitpos) return std::nullopt;
        SCHELLING_DEBUG_ASSERT(((mask_interior_ >> *bitpos) & 1ull) != 0);
        return static_cast<index_t>(*bitpos);                  // guaranteed ∈ [1..len_]
    }

    // -------------------- Mutators (interior only) --------------------
    inline void set(index_t v, bool occ = true, bool color = false) {
        v+= first_index; // adjust for ghost at 0
        occ_ &= ~(1ull << v); // clear first
        occ_ |= (occ << v); 
        col_ &= ~(1ull << v);
        col_ |= (color << v);
    }


    // Local unhappy? (interior only; ghosts already encode neighbors)
    [[nodiscard]] inline bool is_unhappy(index_t v) const {
        return ((unhappy_mask_() >> v) & 1ull) != 0;
    }

private:
    plf::bitset<B> occ_;
    plf::bitset<B> col_;
    std::uint64_t mask_interior_; // bits 1..len_ set; 0 and 63 clear

    // Precompute interior mask once: bits 1..len_ set, else 0.
    static inline plf::bitset<B> make_interior_mask_(index_t len) {
        plf::bitset<B> mask = 0;
        mask[0] = 1;
        mask[B-1] = 1;
        return ~mask;
    }

    // Right-shift on unsigned zero-fills; em covers edges (i,i+1), including ghosts.
    [[nodiscard]] inline std::uint64_t edge_mismatch_() const {
        return (col_ ^ (col_ >> 1)) & (occ_ & (occ_ >> 1)); // XOR colors where both endpoints occupied
    }

    [[nodiscard]] inline std::uint64_t edges() const {
        return (occ_ & (occ_ >> 1)); // edges where both endpoints occupied
    }

    // Bitwise mask of interior vertices unhappy under the majority/minority rule.
    [[nodiscard]] inline std::uint64_t minority_unhappy_mask_() const {
        const uint64_t nm = ~edge_mismatch_() & edges();        // edge mismatches (occupancy-gated)
        const uint64_t is_happy = nm | (nm << 1);          // at least one matched edge is satisfying
        return (~is_happy) & occ_;
    }

    [[nodiscard]] inline std::uint64_t majority_unhappy_mask_() const {
        const uint64_t em = edge_mismatch_();                 // edge mismatches (occupancy-gated)
        return em | (em << 1);                                // at least one mismatched edge
    }

    [[nodiscard]] inline std::uint64_t unhappy_mask_() const {
        const bool minority_ok = ::core::schelling::is_minority_ok();
        return minority_ok ? minority_unhappy_mask_() : majority_unhappy_mask_();
    }

    inline void set_bit_(std::uint8_t bit, bool occ, bool color) {
        const std::uint64_t m = 1ull << bit;        // single-bit mask
        if (occ)   occ_ |= m; else occ_ &= ~m;      // set/clear occupancy
        if (color) col_ |= m; else col_ &= ~m;      // set/clear color
    }
};
