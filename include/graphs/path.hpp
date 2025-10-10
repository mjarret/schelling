// Path — 1D line graph with padded bitset backend
//
// Implementation notes:
// - Padded bitset backend supplies guard cells so neighbor reads (v-1,v+1)
//   need no branches.
// - Unhappy mask is derived from edge mismatches via bitwise lifts; per-op
//   updates touch only a 3-cell window around the mutation.
// - Random picks use kth-zero/one selection for uniformity without scans.
// - Counts/bitset-first; no adjacency or per-vertex storage.
// - set_sentinel() toggles a boundary via logical index -1 addressing the left padding.

#pragma once
#include <cstdint>
#include <optional>
#include <random>
#include <limits>

#include "core/bitset.hpp"
#include "core/config.hpp"
#include "core/schelling_threshold.hpp"
#include "graphs/detail/padded_bitset.hpp"

// Optional test-accessor forward decl; compiled-in only if enabled.
#if SCHELLING_TEST_ACCESSORS
namespace graphs { namespace test { template<std::size_t B> struct PathAccess; } }
#endif

template<core::size_t B = 60>
class Path {
    using padded_bitset = graphs::detail::PaddedBitset<B>;

public:
    using size_t = core::size_t;
    using count_t = core::count_t;
    // Grant test-only accessor friend rights when enabled.
    #if SCHELLING_TEST_ACCESSORS
    friend struct graphs::test::PathAccess<B>;
    #endif
    Path() {
        unhappy_mask_cache_ = unhappy_mask_();
    }
    Path(const core::bitset<B>& unocc, const core::bitset<B>& col)
        : occ_(padded_bitset(~unocc)), col_(col) {
        unhappy_mask_cache_ = unhappy_mask_();
    }

    // -------------------- Counts --------------------------------------
    inline count_t count_by_color(std::optional<bool> c = std::nullopt) const {
        const count_t occ_count = occ_.count();
        if (c == std::nullopt) return B - occ_count;
        return c.value() ? col_.count() : occ_count - col_.count();
    }
    // O(1): return count of unhappy vertices from cached mask.
    inline count_t unhappy_count() const { return unhappy_mask_cache_.count(); }

    // Uniform pick over unoccupied vertices (kth-zero based).
    template<class URBG>
    inline size_t get_unoccupied(URBG& rng) const noexcept {
        CORE_ASSERT_H(occ_.count() < B, "Path::get_unoccupied: no unoccupied vertices");
        return occ_.random_unsetbit_index(rng);
    }

    // Uniform pick among currently-unhappy vertices from cached mask.
    template<class URBG>
    inline size_t get_unhappy(URBG& rng) const noexcept {
        CORE_ASSERT_H(unhappy_mask_cache_.count() > 0, "Path::get_unhappy: no unhappy vertices");
        return unhappy_mask_cache_.random_setbit_index(rng);
    }

    // Pop agent; maintain unhappy cache via local 3-cell reset/update.
    bool pop_agent(size_t from) noexcept {
        CORE_ASSERT_H(from < B, "Path::pop_agent: index out of range");
        CORE_ASSERT_H(occ_[from], "Path::pop_agent: vertex not occupied");
        bool c = col_[from];
        local_unhappy_reset(from);
        occ_.reset(from);
        col_.reset(from);
        local_unhappy_update(from);
        return c;
    }

    // Place agent; same localized unhappy cache maintenance.
    void place_agent(size_t to, bool c) noexcept {
        CORE_ASSERT_H(to < B, "Path::place_agent: index out of range");
        CORE_ASSERT_H(!occ_[to], "Path::place_agent: vertex already occupied");
        local_unhappy_reset(to);
        occ_.set(to);
        if(c) col_.set(to);
        local_unhappy_update(to);
    }

    // -------------------- Basic accessors -----------------------------------
    inline bool is_occupied(size_t v) const { return occ_[v]; }
    inline bool is_unoccupied(size_t v) const { return !occ_[v]; }
    inline bool get_color(size_t v) const { return col_[v]; }
    inline bool is_unhappy(size_t v) const {  return core::schelling::is_unhappy(local_frustration(v), neighbors(v)); }
    // Bridge integration: toggle a boundary sentinel used by lollipop graphs.
    // NOTE: relies on addressing left padding via logical index -1.
    // TODO: Revisit negative index mapping; consider a dedicated API surface.
    inline void set_sentinel(size_t occ, size_t col) {
        local_unhappy_reset(0);
        if(occ) {
            occ_.set(-1); 
            if(col) col_.set(-1); 
            else col_.reset(-1);
        } else {
            occ_.reset(-1); col_.reset(-1);
        }
        local_unhappy_update(0);
    }

    // Testing hooks moved behind SCHELLING_TEST_ACCESSORS in
    // include/graphs/testing/path_access.hpp to avoid polluting the
    // runtime API surface.

 private:
    padded_bitset occ_, col_, unhappy_mask_cache_;

    uint8_t local_frustration(size_t v) const { return (disagree_right(v) + disagree_left(v)); }
    bool disagree_left(size_t v) const { return occ_[v] && occ_[v-1] && (col_[v] != col_[v-1]); }
    bool disagree_right(size_t v) const { return occ_[v] && occ_[v+1] && (col_[v] != col_[v+1]); }
    uint8_t neighbors(size_t v) const { return static_cast<uint8_t>(occ_[v-1]) + static_cast<uint8_t>(occ_[v+1]); }

    // Only called for idx <= B-1
    void local_unhappy_reset(size_t idx) {
        unhappy_mask_cache_.reset(idx-1);
        unhappy_mask_cache_.reset(idx);
        unhappy_mask_cache_.reset(idx+1);
    }

    // Update cached mask over window [idx-1, idx, idx+1].
    void local_unhappy_update(size_t idx) {
        if(is_unhappy(idx-1))  unhappy_mask_cache_.set(idx-1);
        if(is_unhappy(idx))    unhappy_mask_cache_.set(idx);
        if(is_unhappy(idx+1))  unhappy_mask_cache_.set(idx+1);
    }

    // Full recompute: derive vertex-unhappy mask from edge incidence and
    // color differences, avoiding per-vertex branching.
    inline padded_bitset unhappy_mask_() const {
        // unhappy(d,n): true iff d/n > τ (τ = p/q)
        const bool one_mismatch_unhappy = core::schelling::is_unhappy(1, 2); // true iff τ < 1/2

        const auto e    = occ_ & (occ_ << 1);      // edges (i-1,i), anchored at i
        const auto diff = col_ ^ (col_ << 1);      // color-difference on edges
        const auto mis  = e & diff;                // mismatching edges only
        const auto match = e & ~diff;              // matching edges only
        auto lift = [](const padded_bitset& x) { return x | (x >> 1); }; // edge → incident vertices

        // τ < 1/2 : any mismatch makes you unhappy
        // τ ≥ 1/2 : unhappy if you have a neighbor and none match
        return one_mismatch_unhappy ? lift(mis) : (lift(e) & ~lift(match));
    }
};
