#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

#include "clique_graph.hpp"   // graphs::CliqueGraph (counts-first, boundary aware)
#include "path_graph.hpp"     // PathGraph with two-ghost PathGraph_64 blocks

namespace graphs {

// Lollipop = (clique with boundary ghost) + (path whose block-0 LEFT GHOST is that boundary).
// No explicit "bridge" vertex is part of the graph.
class LollipopGraph {
public:
    using index_t = std::uint64_t;

    LollipopGraph(std::uint64_t n_clique, std::uint64_t n_path)
      : n_clique_(n_clique),
        n_path_(n_path),
        clique_(n_clique),
        path_(n_path),
        b_occ_(false),
        b_color_(false) {
        sync_shared_boundary_(); // initialize path's left ghost from boundary
    }

    // ---- sizes ----
    [[nodiscard]] std::uint64_t size()        const noexcept { return n_clique_ + n_path_; }
    [[nodiscard]] std::uint64_t clique_size() const noexcept { return n_clique_; }
    [[nodiscard]] std::uint64_t path_size()   const noexcept { return n_path_;   }

    // ---- shared boundary controls (single source of truth) ----
    inline void set_shared_boundary(bool occ, bool color) {
        b_occ_ = occ; b_color_ = color;
        clique_.set_right_boundary(b_occ_, b_color_);
        sync_shared_boundary_(); // mirrors into path block-0 LEFT ghost
    }
    [[nodiscard]] bool boundary_occupied() const noexcept { return b_occ_; }
    [[nodiscard]] bool boundary_color()     const noexcept { return b_color_; }

    // ---- conceptual color by global index ----
    [[nodiscard]] std::optional<bool> get_color(index_t v) const {
        if (v < n_clique_) return clique_.get_color(v);
        const index_t pv = v - n_clique_;
        if (pv >= n_path_) return std::nullopt;
        return path_.get_color(pv);
    }

    // ---- clique operations (forwarded) ----
    inline void clique_set_color(index_t v, bool c) {
        if (v < n_clique_) clique_.set_color(v, c);
    }
    [[nodiscard]] std::uint64_t clique_unoccupied_count() const {
        return clique_.unoccupied_count();
    }

    // ---- path operations (forwarded) ----
    inline void path_set_occupied(index_t pv) {
        if (pv < n_path_) path_.set_occupied(pv);
    }
    inline void path_clear_occupied(index_t pv) {
        if (pv < n_path_) path_.clear_occupied(pv);
    }
    inline void path_set_color(index_t pv, bool color) {
        if (pv < n_path_) path_.set_color(pv, color);
    }

    // ---- unhappy queries ----
    [[nodiscard]] std::uint64_t unhappy_count() const {
        return clique_.unhappy_agent_count() + path_.unhappy_count();
    }

    // Uniform sampling over all unoccupied vertices.
    template<class Rng>
    [[nodiscard]] std::optional<index_t> get_random_unoccupied(Rng& rng) const {
        const std::uint64_t uc_clique = clique_.unoccupied_count();
        const std::uint64_t uc_path   = path_unoccupied_count_();

        const double w[2] = { static_cast<double>(uc_clique),
                              static_cast<double>(uc_path) };
        const double sum = w[0] + w[1];
        if (sum == 0.0) return std::nullopt;

        std::discrete_distribution<int> pick(std::begin(w), std::end(w)); // [0,2), prob ∝ w_i
        if (pick(rng) == 0) {
            auto cidx = clique_.get_random_unoccupied();
            return cidx; // already a global index inside [0..n_clique_-1]
        } else {
            auto pidx = path_.get_random_unoccupied(rng);
            return pidx ? std::optional<index_t>(n_clique_ + *pidx) : std::nullopt;
        }
    }

    // Uniform sampling over all unhappy vertices.
    template<class Rng>
    [[nodiscard]] std::optional<index_t> get_random_unhappy(Rng& rng) const {
        const std::uint64_t uh_clique = clique_.unhappy_agent_count();
        const std::uint64_t uh_path   = path_.unhappy_count();

        const double w[2] = { static_cast<double>(uh_clique),
                              static_cast<double>(uh_path) };
        const double sum = w[0] + w[1];
        if (sum == 0.0) return std::nullopt;

        std::discrete_distribution<int> pick(std::begin(w), std::end(w)); // [0,2)
        if (pick(rng) == 0) {
            auto cidx = clique_.get_random_unhappy(rng);
            return cidx; // conceptual index inside the clique
        } else {
            auto pidx = path_.get_random_unhappy(rng);
            return pidx ? std::optional<index_t>(n_clique_ + *pidx) : std::nullopt;
        }
    }

    // Optional: expose components
    graphs::CliqueGraph&       clique()       { return clique_; }
    const graphs::CliqueGraph& clique() const { return clique_; }
    PathGraph&                 path()         { return path_; }
    const PathGraph&           path()   const { return path_; }

private:
    std::uint64_t   n_clique_;
    std::uint64_t   n_path_;
    graphs::CliqueGraph clique_;  // counts-first with boundary
    PathGraph       path_;        // two-ghost path
    bool            b_occ_;
    bool            b_color_;

    // Mirror the shared boundary into the path's block-0 LEFT ghost.
    inline void sync_shared_boundary_() {
        if (path_.num_blocks() == 0) return;
        path_.block(0).set_left_boundary(b_occ_, b_color_);
        // Clique already updated via set_right_boundary(...) in set_shared_boundary(...)
    }

    // Sum unoccupied across path blocks via public per-block counters.
    [[nodiscard]] std::uint64_t path_unoccupied_count_() const {
        std::uint64_t sum = 0;
        const auto B = path_.num_blocks();
        for (std::size_t b = 0; b < B; ++b) sum += path_.block(b).unoccupied_count();
        return sum;
    }
};

} // namespace graphs
