#include <cstdint>
#include <cstdio>
#include <chrono>
#include <vector>
#include <iostream>

#include "graphs/path_graph.hpp"
#include "core/rng.hpp"

using Clock = std::chrono::high_resolution_clock;

template <std::size_t K>
/**
 * @brief Original local frustration computation for path graph.
 */
static inline std::uint64_t local_orig(const graphs::PathGraph<K>& g, std::size_t v) {
    const std::uint32_t c = g.get_color(v);
    return (c != 0u) * static_cast<std::uint64_t>(
        (g.get_color(v - 1) != 0u && g.get_color(v - 1) != c) +
        (g.get_color(v + 1) != 0u && g.get_color(v + 1) != c)
    );
}

template <std::size_t K>
/**
 * @brief Folded/branch-lean local frustration computation.
 */
static inline std::uint64_t local_fold(const graphs::PathGraph<K>& g, std::size_t v) {
    const std::uint32_t c  = g.get_color(v);
    const std::uint32_t nl = g.get_color(v - 1);
    const std::uint32_t nr = g.get_color(v + 1);
    const std::uint32_t left  = (nl != 0u) & (nl != c);
    const std::uint32_t right = (nr != 0u) & (nr != c);
    return (c != 0u) * static_cast<std::uint64_t>(left + right);
}

/**
 * @brief Entry point for path local frustration benchmark.
 */
int main() {
    constexpr std::size_t K = 2;
    const std::size_t N = 1u << 18;
    graphs::PathGraph<K> g(N);
    core::Xoshiro256ss rng(12345);
    
    for (std::size_t v = 0; v < N; ++v) {
        const bool occ = (rng.uniform01() < 0.70);
        const std::uint32_t c = occ ? (1u + static_cast<std::uint32_t>(rng.uniform_u64(K) % K)) : 0u;
        g.change_color(v, c, g.get_color(v));
    }
    volatile std::uint64_t sink = 0;
    auto bench = [&](auto fn, const char* name) {
        const int iters = 5;
        std::uint64_t total_ns = 0;
        for (int it = 0; it < iters; ++it) {
            auto t0 = Clock::now();
            std::uint64_t acc = 0;
            for (std::size_t v = 0; v < N; ++v) acc += fn(g, v);
            auto t1 = Clock::now();
            total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
            sink ^= acc;
        }
        std::cout << name << ": " << (total_ns / iters) << " ns avg\n";
    };

    bench(local_orig<K>,  "orig_shortcircuit");
    bench(local_fold<K>,  "folded_branchless");
    (void)sink;
    return 0;
}
