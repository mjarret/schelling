#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include <numeric>

template <class World, class RNG>
inline void random_fill(World& world, uint32_t n_to_place, uint32_t n_types, RNG& rng) {
    const uint32_t N = world.num_vertices();
    std::vector<uint32_t> idx(N);
    std::iota(idx.begin(), idx.end(), 0u);
    // Partial Fisher–Yates shuffle
    for (uint32_t i = 0; i < n_to_place; ++i) {
        uint32_t j = i + static_cast<uint32_t>(rng.uniform_u64(N - i));
        std::swap(idx[i], idx[j]);
        uint32_t v = idx[i];
        if (!world.occupied(v)) {
            uint32_t t = static_cast<uint32_t>(rng.uniform_u64(n_types));
            world.set_vertex(v, t);
        }
    }
}
