#pragma once
#include <cstdint>
#include <concepts>

template <class W>
concept WorldLike =
    requires(W w, uint32_t v, uint32_t type) {
        { w.num_vertices() } -> std::convertible_to<uint32_t>;
        { w.num_agents()   } -> std::convertible_to<uint32_t>;
        { w.occupied(v)    } -> std::same_as<bool>;
        { w.type_of(v)     } -> std::convertible_to<uint32_t>;
        { w.clear_vertex(v) };
        { w.set_vertex(v, type) };
        { w.move(v, v) };
        // iteration: accepts a callable f(v, type)
        { w.for_each_agent([](uint32_t, uint32_t){}) };
    };
