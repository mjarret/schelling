// graph_base.hpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <array>

using index_t = std::size_t;

template <class Derived, std::size_t K>
class GraphBase {
public:
    GraphBase() = default;

    std::size_t size() const noexcept { return num_vertices; }
    index_t total_occupied() const noexcept { return static_cast<index_t>(num_vertices - counts[0]); }

    // Default total frustration: sum local_frustration(v) and halve.
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

protected:
    index_t num_vertices = 0;
    std::array<std::uint64_t, K> counts{}; // counts[0]=unoccupied if applicable
    mutable std::uint64_t tf = static_cast<std::uint64_t>(-1); // -1 means "needs recompute"
};
