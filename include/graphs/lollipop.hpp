// LollipopGraph — composite graph (clique + path) with a single bridge
#pragma once

#include <cstddef>
#include <cstdint>
#include "core/rng.hpp"
#include "core/schelling_threshold.hpp"
#include "graphs/clique.hpp"
#include "graphs/path.hpp"

#if SCHELLING_TEST_ACCESSORS
namespace graphs { namespace test { template<std::size_t CS, std::size_t PL> struct LollipopAccess; } }
#endif

namespace graphs {
using size_t = core::size_t;
using count_t = core::count_t;

template<size_t CliqueSize = 50, size_t PathLength = 450>
class LollipopGraph {
public:
    static constexpr size_t CliqueBase = 0;
    static constexpr size_t PathBase   = CliqueSize;
    static constexpr size_t TotalSize  = CliqueSize + PathLength;

#if SCHELLING_TEST_ACCESSORS
    friend struct graphs::test::LollipopAccess<CliqueSize, PathLength>;
#endif

    LollipopGraph() = default;

    // ----------------------------- Public API -----------------------------

    // Count unhappy vertices (bridge evaluated against its full lollipop neighborhood,
    // then reconciled with the clique-only view to avoid double counting)
    inline count_t unhappy_count() const {
        return bridge_unhappy() 
            - bridge_unhappy_in_clique_sense_() 
            + clique_.unhappy_count() 
            + path_.unhappy_count();
    }

    // Uniformly sample an unhappy vertex index over the lollipop graph
    template<class Rng>
    inline size_t get_unhappy(Rng& rng) const {
        const std::uint64_t w0 = static_cast<std::uint64_t>(clique_.unhappy_count());
        const std::uint64_t w1 = static_cast<std::uint64_t>(path_.unhappy_count());
        const std::uint64_t w2 = static_cast<std::uint64_t>(bridge_unhappy() != bridge_unhappy_in_clique_sense_());
        const int pick = core::weighted_pick3(rng, w0, w1, w2);
        if (pick == 0) return clique_.get_unhappy(rng).value();
        if (pick == 1) return path_.get_unhappy(rng) + PathBase;
        return bridge_index_();
    }

    // Uniformly sample an unoccupied vertex (correctly deduplicating the bridge)
    template<class Rng>
    inline size_t get_unoccupied(Rng& rng) const {
        const std::uint64_t w0 = static_cast<std::uint64_t>(clique_.count_by_color(std::nullopt));
        const std::uint64_t w1 = static_cast<std::uint64_t>(path_.count_by_color(std::nullopt));
        const int side = core::weighted_pick2(rng, w0, w1);
        return side ? (path_.get_unoccupied(rng) + PathBase) : clique_.get_unoccupied(rng);
    }

    // Pop/place with proper bridge synchronization
    inline bool pop_agent(size_t from) noexcept {
        CORE_ASSERT_H(from < TotalSize, "LollipopGraph::pop_agent: index out of range");
        CORE_ASSERT_H(is_occupied(from), "LollipopGraph::pop_agent: vertex not occupied");
        if (from == bridge_index_()) [[unlikely]] {
            bool c = bridge_color_;
            clique_.pop_agent(from);
            path_.set_sentinel(0,0);
            bridge_occupied_ = false;
            bridge_color_    = false; 
            return c;
        } else if (from < CliqueSize) {
            return clique_.pop_agent(from).value();
        } else {
            return path_.pop_agent(from - PathBase);
        }
    }

    inline void place_agent(size_t to, bool c) noexcept {
        CORE_ASSERT_H(to < TotalSize, "LollipopGraph::place_agent: index out of range");
        CORE_ASSERT_H(!is_occupied(to), "LollipopGraph::place_agent: vertex already occupied");

        // Original bridge-branch based on bridge_index_()
        if (to == bridge_index_()) [[unlikely]] {
            path_.set_sentinel(true,c);
            clique_.place_agent(to, c);
            bridge_occupied_ = true;
            bridge_color_    = c;
        } else if (to < CliqueSize) {
            clique_.place_agent(to, c);
        } else {
            path_.place_agent(to - PathBase, c);
        }
    }

private:
    // ----------------------------- Helpers -----------------------------

    // Convention:
    //   - bridge is always the first element of its current block in the clique's ordering
    //     color 0: [0, c0_) → bridge idx = 0
    //     color 1: [c0_, c0_+c1_) → bridge idx = c0_
    //     none    : [c0_+c1_, CliqueSize) → bridge idx = c0_ + c1_ = occupied_count()
    inline size_t bridge_index_() const noexcept {
        return (bridge_occupied_) ? (bridge_color_*clique_.count_by_color(0)) : clique_.occupied_count();
    }

    // --- Bridge unhappy (three senses) ---
    // These use only counts + neighbor color on path[1], so they are O(1).
    inline bool bridge_unhappy_in_clique_sense_() const noexcept {
        if(!bridge_occupied_) [[likely]] return false;
        const count_t neigh = clique_.occupied_count() - 1;
        const count_t disagree = clique_.count_by_color(!bridge_color_);
        return core::schelling::is_unhappy(disagree, neigh);
    }

    inline bool bridge_unhappy() const noexcept {
        if (!bridge_occupied_) return false;
        return core::schelling::is_unhappy(bridge_frustration(), bridge_neighbors());
    }

    inline count_t bridge_neighbors() const noexcept {
        return clique_.occupied_count() - 1 + path_.is_occupied(0);
    }

    inline count_t bridge_frustration() const noexcept {
        return clique_.count_by_color(!bridge_color_) + (path_.is_occupied(0) && (path_.get_color(0) != bridge_color_));
    }

    // ----------------------------- Data -----------------------------
    Clique<CliqueSize>  clique_{};
    Path<PathLength>    path_{};

    // Bridge state we must track to compute its clique-index and true status.
    // (These are updated when the bridge is popped/placed.)
    bool bridge_occupied_ = false;
    bool bridge_color_    = false;
};

} // namespace graphs
