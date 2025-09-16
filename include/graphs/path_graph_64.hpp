// path_graph_64.hpp
#pragma once
#include <bit>        // std::popcount, std::countr_zero (C++20)
#include <cstdint>
#include <optional>
#include <random>
#include "core/bitops.hpp"  // core::random_setbit_index_u64(mask, rng): returns bit position (0..63)

class PathGraph_64 {
public:
    using index_t = std::uint8_t;          // local interior index ∈ [1..len_]
    static constexpr index_t kMaxInterior = 62;

    // Construct an empty block with given interior length in [0..62].
    explicit PathGraph_64(index_t len = kMaxInterior)
        : occ_(0), col_(0), len_(len),
          mask_interior_(make_interior_mask_(len)) {
    }

    PathGraph_64(std::uint64_t occ, std::uint64_t col, index_t len)
        : occ_(occ), col_(col), len_(len),
          mask_interior_(make_interior_mask_(len)) {
    }

    // -------------------- Boundary (ghost) controls --------------------
    // bit 0 = left ghost, bit 63 = right ghost
    inline void set_left_boundary(bool occ, bool color)  { set_bit_(0,  occ, color); }
    inline void set_right_boundary(bool occ, bool color) { set_bit_(63, occ, color); }

    // -------------------- Accessors for composite graph ----------------
    [[nodiscard]] inline index_t      length()          const { return len_; }
    [[nodiscard]] inline std::uint64_t occupancy_mask() const { return occ_ & mask_interior_; }
    [[nodiscard]] inline std::uint64_t color_mask()     const { return col_ & mask_interior_; }

    // Query color if occupied (interior only)
    [[nodiscard]] inline std::optional<bool> get_color(index_t v) const {
        const bool here = ((occ_ >> v) & 1ull) != 0;
        if (!here) return std::nullopt;
        return ((col_ >> v) & 1ull) != 0;
    }

    // -------------------- Counts (interior only) ----------------------
    [[nodiscard]] inline index_t unoccupied_count() const {
        const std::uint64_t mask = (~occ_) & mask_interior_;
        return static_cast<index_t>(std::popcount(mask));  // C++20 <bit>
    }

    [[nodiscard]] inline index_t unhappy_agent_count() const {
        // Edge mismatch (includes ghosts): bit i == 1 iff (i,i+1) both occupied & colors differ.
        const std::uint64_t em = edge_mismatch_();
        // Unhappy vertices are endpoints of mismatched edges; restrict to interior.
        const std::uint64_t unhappy = (em | (em << 1)) & mask_interior_;
        return static_cast<index_t>(std::popcount(unhappy));
    }

    // -------------------- Random picks over interior -------------------
    template<class Rng>
    [[nodiscard]] inline std::optional<index_t> get_random_unoccupied(Rng& rng) const {
        const std::uint64_t mask = (~occ_) & mask_interior_;
        if (mask == 0) return std::nullopt;

        auto bitpos = core::random_setbit_index_u64(mask, rng); // returns 0..63 (bit position)
        if (!bitpos) return std::nullopt;
        return static_cast<index_t>(*bitpos);                  // guaranteed ∈ [1..len_]
    }

    template<class Rng>
    [[nodiscard]] inline std::optional<index_t> get_random_unhappy(Rng& rng) const {
        const std::uint64_t em   = edge_mismatch_();
        const std::uint64_t mask = (em | (em << 1)) & mask_interior_;
        if (mask == 0) return std::nullopt;

        auto bitpos = core::random_setbit_index_u64(mask, rng);
        if (!bitpos) return std::nullopt;

#ifndef NDEBUG
        assert((mask_interior_ >> *bitpos) & 1ull);
#endif
        return static_cast<index_t>(*bitpos);                  // guaranteed ∈ [1..len_]
    }

    // -------------------- Mutators (interior only) --------------------
    inline void set_occupied(index_t v) {
        occ_ |= (1ull << v);
    }
    inline void clear_occupied(index_t v) {
        occ_ &= ~(1ull << v);
    }
    inline void set_color(index_t v, bool c) {
        const std::uint64_t m = 1ull << v;
        if (c) col_ |= m; else col_ &= ~m;
    }

    // Local unhappy? (interior only; ghosts already encode neighbors)
    [[nodiscard]] inline bool is_unhappy(index_t v) const {
        const std::uint64_t em   = edge_mismatch_();
        const std::uint64_t here = (occ_ >> v) & 1ull;
        const std::uint64_t left  = ((em >> ((v - 1) & 63)) & 1ull);
        const std::uint64_t right = ((em >> v) & 1ull);
        return (here & (left | right)) != 0;
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
        const unsigned s = static_cast<unsigned>(len) + 1u;
        return (((1ull << s) - 1ull) & ~1ull);
    }

    // Right-shift on unsigned zero-fills; em covers edges (i,i+1), including ghosts.
    [[nodiscard]] inline std::uint64_t edge_mismatch_() const {
        return (col_ ^ (col_ >> 1)) & (occ_ & (occ_ >> 1));
    }

    inline void set_bit_(std::uint8_t bit, bool occ, bool color) {
        const std::uint64_t m = 1ull << bit;
        if (occ)   occ_ |= m; else occ_ &= ~m;
        if (color) col_ |= m; else col_ &= ~m;
    }
};
