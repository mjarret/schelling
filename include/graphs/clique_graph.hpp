#pragma once

#include <algorithm>   // std::min
#include <cstdint>
#include <optional>
#include <random>      // std::uniform_int_distribution
#include <span>
#include <utility>     // std::pair

#include "core/schelling_threshold.hpp"

namespace graphs {

// Two-color clique (plus unoccupied), counts-first, with one boundary ghost.
// Boundary (b_occ_, b_color_) behaves like a single external neighbor connected to every occupied clique vertex.
class CliqueGraph {
public:
    using count_t  = std::uint64_t;
    using weights2 = std::pair<count_t, count_t>; // {w0, w1} = {color-0, color-1}

    explicit CliqueGraph(count_t n) noexcept
        : n_(n), c0_(0), c1_(0), b_occ_(false), b_color_(false) {}

    // From occupancy+color spans (true => occupied; color==true => color 1).
    explicit CliqueGraph(std::span<const bool> occ,
                         std::span<const bool> color) noexcept
        : n_(occ.size()), c0_(0), c1_(0), b_occ_(false), b_color_(false)
    {
        const std::size_t m = std::min<std::size_t>(occ.size(), color.size());
        for (std::size_t i = 0; i < m; ++i) {
            if (occ[i]) { if (color[i]) ++c1_; else ++c0_; }
        }
    }

    // --- Boundary (ghost) ---
    void set_right_boundary(bool occ, bool color) noexcept {
        b_occ_ = occ; b_color_ = color;
    }
    bool boundary_occupied() const noexcept { return b_occ_; }
    bool boundary_color()     const noexcept { return b_color_; }

    // --- Sizes / counts ---
    count_t size()             const noexcept { return n_; }
    count_t occupied_count()   const noexcept { return c0_ + c1_; }
    count_t unoccupied_count() const noexcept { return n_ - (c0_ + c1_); }

    // --- Conceptual color by index (keeps index API; still fully symmetric) ---
    // [0 .. c0_-1] => color 0, [c0_ .. c0_+c1_-1] => color 1, else unoccupied.
    std::optional<bool> get_color(count_t v) const noexcept {
        if (v < c0_)               return false;
        else if (v < c0_ + c1_)    return true;
        else                       return std::nullopt;
    }

    // --- Unhappy counts (O(1), from counts + boundary) ---
    count_t unhappy_agent_count() const {
        const auto w = unhappy_weights();                 // per-color unhappy counts
        return w.first + w.second;
    }

    // Per-color unhappy counts as a pair {u0, u1}.
    weights2 unhappy_weights() const {
        const count_t occ = c0_ + c1_;
        if (occ == 0) return {0, 0};

        const count_t deg   = (occ - 1) + (b_occ_ ? 1 : 0);
        const count_t u0_in = c1_ + ((b_occ_ &&  b_color_) ? 1 : 0); // unlike neighbors for color-0 agent
        const count_t u1_in = c0_ + ((b_occ_ && !b_color_) ? 1 : 0); // unlike neighbors for color-1 agent

        const bool u0 = core::schelling::is_unhappy(u0_in, deg);
        const bool u1 = core::schelling::is_unhappy(u1_in, deg);
        return { u0 ? c0_ : 0, u1 ? c1_ : 0 };
    }

    // --- Sampling ------------------------------------------------------
    // Unoccupied vertices are symmetric—return the first empty conceptual slot if any.
    std::optional<count_t> get_random_unoccupied() const noexcept {
        const count_t u = unoccupied_count();
        return u ? std::optional<count_t>(c0_ + c1_) : std::nullopt;
    }

    // Uniform over all unhappy clique vertices.
    // Returns a conceptual index inside the clique:
    //   - if color-0 chosen → return 0
    //   - if color-1 chosen → return c0_  (start of color-1 block)
    template<class URBG>
    std::optional<count_t> get_random_unhappy(URBG& rng) const {
        const auto w = unhappy_weights();             // {w0, w1}
        const count_t sum = w.first + w.second;
        if (sum == 0) return std::nullopt;

        // Weighted choice without containers: draw r ∈ [0, sum-1], pick 0 if r < w0 else 1.
        std::uniform_int_distribution<count_t> pick(0, sum - 1);  // inclusive range [a,b] per standard
        const count_t r = pick(rng);                               // [0, sum-1]
        return (r >= w.first) ? w.first : 0;
    }

    // --- Recolor one conceptual occupied vertex; no-ops for unoccupied / same color ---
    void set_color(count_t v, bool c) {
        const auto prev = get_color(v);
        if (!prev.has_value() || *prev == c) return;
        if (*prev == false) { --c0_; ++c1_; } else { --c1_; ++c0_; }
    }

private:
    count_t n_{0};   // capacity (occupied + unoccupied)
    count_t c0_{0};  // count of color 0
    count_t c1_{0};  // count of color 1
    bool    b_occ_{false};    // boundary (ghost) occupancy
    bool    b_color_{false};  // boundary (ghost) color
};
} // namespace graphs
