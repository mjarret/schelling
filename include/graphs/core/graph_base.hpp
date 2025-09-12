// graph_base.hpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <array>

using index_t = std::size_t;

// Performance contract (do not "safety up" without consent):
// - counts has K slots; when graphs include an "unoccupied" bucket it lives at counts[0].
// - total_occupied() must remain O(1): derive as (num_vertices - counts[0]).
// - total_frustration() is a memoized cache (tf). Hot paths in concrete graphs
//   keep tf up-to-date via O(1) deltas or explicitly invalidate it.
// - No runtime asserts/guards in hot paths; rely on documented preconditions
//   in each graph (see change_color/local_frustration notes).

template <class Derived, std::size_t K>
class GraphBase {
public:
    GraphBase() = default;

    std::size_t size() const noexcept { return num_vertices; }
    index_t total_occupied() const noexcept { return static_cast<index_t>(num_vertices - counts[0]); }

    // Default total frustration: sum local_frustration(v) and halve.
    // Concrete graphs may keep tf hot with O(1) deltas and never hit this path.
    std::uint64_t total_frustration() const noexcept {
        if (tf != static_cast<std::uint64_t>(-1)) return tf;
        const auto& self = static_cast<const Derived&>(*this);
        std::uint64_t acc = 0;
        for (std::size_t i = 0; i < num_vertices; ++i) {
            acc += self.local_frustration(static_cast<index_t>(i));
        }
        tf = acc / 2; // each edge counted twice
        return tf;
    }

    std::index_t is_happy(index_t v, double tau) const noexcept { 
        return local_frustration(v) <= tau * neighbors(v); 
    }

    std::index_t happiness(double tau) const noexcept {
        if (happiness_ != static_cast<index_t>(-1)) return happiness_;
        const auto& self = static_cast<const Derived&>(*this);
        index_t happiness_ = 0;
        for (std::size_t i = 0; i < num_vertices; ++i) {
            happiness_ += self.is_happy(static_cast<index_t>(i), tau);
        }
        return happiness_;
    }

    template <typename Func>
    auto sum_over_neighbors(index_t v, Func&& func) const {
        const auto& self = static_cast<const Derived&>(*this);
        auto sum = decldtype(func(v, index_t{})){};
        for (index_t n = 0; n < self.neighbors(v); ++n) {
            sum += func(v, self.nth_neighbor(v, n));
        }
        return sum;
    }

protected:
    index_t num_vertices = 0;
    index_t happiness_ = static_cast<index_t>(-1); // -1 means "needs recompute"
    std::array<std::uint64_t, K> counts{}; // counts[0]=unoccupied if applicable
    mutable std::uint64_t tf = static_cast<std::uint64_t>(-1); // -1 means "needs recompute"
};
