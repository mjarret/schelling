// path_graph.hpp
#pragma once
#include <bit>            // std::popcount
#include <cstdint>
#include <optional>
#include <random>
#include <vector>
#include "path_graph_64.hpp"
#include "core/bitops.hpp" // core::random_setbit_index_u64(mask, rng)

class PathGraph {
public:
    using gindex_t = std::uint64_t;  // global vertex index
    using lindex_t = std::uint8_t;   // local interior index ∈ [1..len]

    explicit PathGraph(std::uint64_t n)
      : n_(n), blocks_(num_blocks_for_(n)) {
        for (std::size_t b = 0; b < blocks_.size(); ++b)
            blocks_[b] = PathGraph_64(block_len_(b));
        sync_all_boundaries_();
    }

    std::uint64_t size()       const noexcept { return n_; }
    std::size_t   num_blocks() const noexcept { return blocks_.size(); }

    // Global index → (block, local interior index)
    static std::size_t block_of(gindex_t v)  noexcept { return static_cast<std::size_t>(v / PathGraph_64::kMaxInterior); }
    static lindex_t    offset_of(gindex_t v) noexcept { return static_cast<lindex_t>(v % PathGraph_64::kMaxInterior + 1); }
    static gindex_t    base_of(std::size_t b) noexcept { return static_cast<gindex_t>(b) * PathGraph_64::kMaxInterior; }

    // -------------------- Setters / Getters ----------------------------
    inline void set_occupied(gindex_t v) {
        if (v >= n_) return;
        const std::size_t b = block_of(v);
        const lindex_t    o = offset_of(v);
        blocks_[b].set_occupied(o);
        sync_boundaries_after_local_change_(b, o);
    }
    inline void clear_occupied(gindex_t v) {
        if (v >= n_) return;
        const std::size_t b = block_of(v);
        const lindex_t    o = offset_of(v);
        blocks_[b].clear_occupied(o);
        sync_boundaries_after_local_change_(b, o);
    }
    inline void set_color(gindex_t v, bool c) {
        if (v >= n_) return;
        const std::size_t b = block_of(v);
        const lindex_t    o = offset_of(v);
        blocks_[b].set_color(o, c);
        sync_boundaries_after_local_change_(b, o);
    }
    inline std::optional<bool> get_color(gindex_t v) const {
        if (v >= n_) return std::nullopt;
        return blocks_[block_of(v)].get_color(offset_of(v));
    }

    // Unhappy? — ask the block; ghosts encode neighbors already
    inline bool is_unhappy(gindex_t v) const {
        if (v >= n_) return false;
        return blocks_[block_of(v)].is_unhappy(offset_of(v));
    }

    // Fast unhappy count: sum per-block counts; no boundary fixups needed
    inline std::uint64_t unhappy_count() const {
        std::uint64_t total = 0;
        for (const auto& blk : blocks_) total += blk.unhappy_agent_count();
        return total;
    }

    // -------------------- Sampling via per-block weights ----------------
    template<class Rng>
    inline std::optional<gindex_t> get_random_unoccupied(Rng& rng) const {
        const auto b = choose_block_by_weights_(rng, [](const PathGraph_64& blk){
            return static_cast<double>(blk.unoccupied_count());
        });
        if (!b) return std::nullopt;
        auto li = blocks_[*b].get_random_unoccupied(rng);
        return li ? std::optional<gindex_t>(base_of(*b) + static_cast<gindex_t>(*li - 1)) : std::nullopt;
    }

    template<class Rng>
    inline std::optional<gindex_t> get_random_unhappy(Rng& rng) const {
        const auto b = choose_block_by_weights_(rng, [](const PathGraph_64& blk){
            return static_cast<double>(blk.unhappy_agent_count());
        });
        if (!b) return std::nullopt;
        auto li = blocks_[*b].get_random_unhappy(rng);
        return li ? std::optional<gindex_t>(base_of(*b) + static_cast<gindex_t>(*li - 1)) : std::nullopt;
    }

    // Optional block access
    PathGraph_64&       block(std::size_t i)       { return blocks_[i]; }
    const PathGraph_64& block(std::size_t i) const { return blocks_[i]; }

private:
    std::uint64_t             n_;
    std::vector<PathGraph_64> blocks_;

    // Block partitioning (62 interior per block)
    static std::size_t num_blocks_for_(std::uint64_t n) {
        return static_cast<std::size_t>((n + PathGraph_64::kMaxInterior - 1) / PathGraph_64::kMaxInterior);
    }
    inline PathGraph_64::index_t block_len_(std::size_t b) const {
        const std::uint64_t start = base_of(b);
        if (start >= n_) return 0;
        const std::uint64_t rem = n_ - start;
        return static_cast<PathGraph_64::index_t>(rem >= PathGraph_64::kMaxInterior ? PathGraph_64::kMaxInterior : rem);
    }

    // Keep ghost boundaries consistent (construction + after updates)
    inline void sync_all_boundaries_() {
        const std::size_t B = blocks_.size();
        for (std::size_t b = 0; b < B; ++b) {
            // left ghost of b mirrors last interior of b-1
            if (b == 0 || blocks_[b-1].length() == 0) {
                blocks_[b].set_left_boundary(false, false);
            } else {
                const auto lp  = blocks_[b-1].length();
                const bool occ = ((blocks_[b-1].occupancy_mask() >> lp) & 1ull) != 0;
                const bool col = ((blocks_[b-1].color_mask()     >> lp) & 1ull) != 0;
                blocks_[b].set_left_boundary(occ, col);
            }
            // right ghost of b mirrors first interior of b+1
            if (b + 1 >= B || blocks_[b+1].length() == 0) {
                blocks_[b].set_right_boundary(false, false);
            } else {
                const bool occ = ((blocks_[b+1].occupancy_mask() >> 1) & 1ull) != 0;
                const bool col = ((blocks_[b+1].color_mask()     >> 1) & 1ull) != 0;
                blocks_[b].set_right_boundary(occ, col);
            }
        }
    }

    inline void sync_boundaries_after_local_change_(std::size_t b, lindex_t o) {
        // If first interior changed (o==1), update RIGHT ghost of previous block
        if (o == 1 && b > 0) {
            const bool occ = ((blocks_[b].occupancy_mask() >> 1) & 1ull) != 0;
            const bool col = ((blocks_[b].color_mask()     >> 1) & 1ull) != 0;
            blocks_[b-1].set_right_boundary(occ, col);
        }
        // If last interior changed (o==len), update LEFT ghost of next block
        const auto len = blocks_[b].length();
        if (o == len && (b + 1) < blocks_.size()) {
            const bool occ = ((blocks_[b].occupancy_mask() >> len) & 1ull) != 0;
            const bool col = ((blocks_[b].color_mask()     >> len) & 1ull) != 0;
            blocks_[b+1].set_left_boundary(occ, col);
        }
    }

    template<class Rng, class WeightFn>
    inline std::optional<std::size_t> choose_block_by_weights_(Rng& rng, WeightFn wfn) const {
        const std::size_t B = blocks_.size();
        std::vector<double> w; w.reserve(B);
        double sum = 0.0;
        for (std::size_t b = 0; b < B; ++b) {
            const double wb = wfn(blocks_[b]);
            w.push_back(wb);
            sum += wb;
        }
        if (sum == 0.0) return std::nullopt; // no eligible vertices

        std::discrete_distribution<std::size_t> dist(w.begin(), w.end()); // [0,B), prob ∝ w_i
        return std::optional<std::size_t>(dist(rng));
    }
};
