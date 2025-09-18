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
class PathGraph_64 {
public:
    using index_t = std::uint8_t;          // local interior index ∈ [1..len_]
    static constexpr index_t kMaxInterior = 62;
    static constexpr index_t first_index = 1;    // max interior length
    static constexpr index_t last_index = first_index + kMaxInterior - 1; // 0 indexing

    // Construct an empty block with given interior length in [0..62].
    /**
     * @brief Construct empty block with interior length in [0..62].
     */
    explicit PathGraph_64(index_t len = kMaxInterior)
        : occ_(0), col_(0), len_(len),
          mask_interior_(make_interior_mask_(len)) {
        SCHELLING_DEBUG_ASSERT(len <= kMaxInterior);
    }

    PathGraph_64(std::uint64_t occ, std::uint64_t col, index_t len)
        : occ_(occ), col_(col), len_(len),
          mask_interior_(make_interior_mask_(len)) {
        SCHELLING_DEBUG_ASSERT(len <= kMaxInterior);
    }

    inline uint8_t loc_2_glob_idx(index_t local) const {
        return local - first_index; // adjust for ghost at 0
    }

    inline uint8_t glob_2_loc_idx(index_t global) const {
        return global + first_index; // adjust for ghost at 0
    }
 
    // -------------------- Boundary (ghost) controls --------------------
    // bit 0 = left ghost, bit 63 = right ghost
    inline void set_left_boundary(std::pair<bool,bool> p)  { set_bit_(0,  p.first, p.second); }  // ghost write
    inline void set_right_boundary(std::pair<bool,bool> p) { set_bit_(63, p.first, p.second); } // ghost write
    
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
        SCHELLING_DEBUG_ASSERT(v >= 1 && v <= len_);
        return ((unhappy_mask_() >> v) & 1ull) != 0;
    }
    
    inline std::pair<bool,bool> get_first() const {
        const bool occ = ((occ_ >> first_index) & 1ull) != 0; 
        const bool col = ((col_ >> first_index) & 1ull) != 0; 
        return {occ, col};
    }

    inline std::pair<bool,bool> get_last() const {
        const bool occ = ((occ_ >> kMaxInterior) & 1ull) != 0; 
        const bool col = ((col_ >> kMaxInterior) & 1ull) != 0; 
        return {occ, col};
    }

private:
    std::uint64_t occ_;   // bit 0 = left ghost, 1..len_ interior, 63 = right ghost
    std::uint64_t col_;
    index_t       len_;   // interior length (0..62)
    std::uint64_t mask_interior_; // bits 1..len_ set; 0 and 63 clear

    // Precompute interior mask once: bits 1..len_ set, else 0.
    static inline std::uint64_t make_interior_mask_(index_t len) {
        // ((1 << (len+1)) - 1) yields bits [0..len] set; & ~1 clears bit 0 (left ghost).
        // With len ≤ 62, (len+1) ≤ 63 → no UB.
        uint64_t mask = ~0ull << first_index; // bits 1..63 set
        mask = mask >> 63-last_index;
        return ~(1ull) & ~(32ull);
        const unsigned s = len + 1;
        return (((1ull << s) - 1ull) & ~1ull); // bits 1..len set
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
