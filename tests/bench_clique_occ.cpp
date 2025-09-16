#include <array>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <vector>

#include "graphs/clique_graph.hpp"
#include "core/rng.hpp"

using Clock = std::chrono::high_resolution_clock;

template <std::size_t K>
struct CliqueOcc {
    using u64 = std::uint64_t;
    std::size_t n{0};
    std::array<u64, K+1> counts{};
    u64 occ{0};
    /**
     * @brief Construct with vertex count and color counts.
     * @param n_ Vertex count.
     * @param c Counts including unoccupied at 0.
     */
    explicit CliqueOcc(std::size_t n_, const std::array<u64, K+1>& c) : n(n_), counts(c) {
        occ = n - counts[0];
    }
    /**
     * @brief Total occupied vertices.
     */
    inline u64 total_occupied() const { return occ; }
    /**
     * @brief Number of vertices.
     */
    inline std::size_t size() const { return n; }
    /**
     * @brief Map index to color via prefix sums.
     * @param v Vertex index.
     * @return Color in [0..K].
     */
    inline std::uint32_t get_color(std::size_t v) const {
        std::array<u64, K+1> prefix{};
        std::partial_sum(counts.begin(), counts.end(), prefix.begin());
        auto it = std::upper_bound(prefix.begin(), prefix.end(), static_cast<u64>(v));
        return static_cast<std::uint32_t>(std::distance(prefix.begin(), it));
    }
    /**
     * @brief Local disagreement count at vertex v.
     */
    inline std::uint64_t local_frustration(std::size_t v) const {
        return total_occupied() - counts[get_color(v)];
    }
    /**
     * @brief Change counts-based color.
     */
    inline void change_color(std::uint32_t from, std::uint32_t to) {
        if (from == to) return;
        if (to == 0) {
            if (counts[from]) --counts[from];
            ++counts[0];
            --occ;
            return;
        }
        if (from == 0) {
            if (counts[0]) --counts[0];
            ++counts[to];
            ++occ;
            return;
        }
        if (counts[from]) --counts[from];
        ++counts[to];
    }
};

template <class Fn>
/**
 * @brief Benchmark function wrapper averaging over iterations.
 * @param fn Callable to bench.
 * @param iters Number of iterations.
 * @return Average nanoseconds per call.
 */
static inline std::uint64_t bench(Fn&& fn, int iters=5) {
    std::uint64_t total_ns = 0;
    for (int i=0;i<iters;++i) {
        auto t0 = Clock::now(); fn(); auto t1 = Clock::now();
        total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    }
    return total_ns / iters;
}

/**
 * @brief Entry point for clique occupancy benchmark.
 */
int main() {
    constexpr std::size_t K = 2;
    const std::size_t N = 1u << 18;
    core::Xoshiro256ss rng(42);
    
    std::array<std::uint64_t, K+1> counts{};
    counts[0] = static_cast<std::uint64_t>(N * 0.30);
    const std::uint64_t occ = N - counts[0];
    for (std::size_t c = 1; c <= K; ++c) counts[c] = occ / K + ((c <= (occ % K)) ? 1 : 0);
    
    graphs::CliqueGraph<K> g(N, counts);
    CliqueOcc<K> gc(N, counts);
    volatile std::uint64_t sink = 0;
    auto read_heavy_baseline = [&](){
        std::uint64_t acc = 0;
        for (std::size_t v=0; v<N; ++v) acc += g.local_frustration(v);
        sink ^= acc;
    };
    auto read_heavy_cached = [&](){
        std::uint64_t acc = 0;
        for (std::size_t v=0; v<N; ++v) acc += gc.local_frustration(v);
        sink ^= acc;
    };
    auto mixed_updates_baseline = [&](){
        std::uint64_t acc = 0;
        for (int it=0; it<5000; ++it) {
            std::uint32_t a = static_cast<std::uint32_t>(rng.uniform_u64(K+1));
            std::uint32_t b = static_cast<std::uint32_t>(rng.uniform_u64(K+1));
            if (a == b) b = (b+1)%(K+1);
            for (int s=0; s<8; ++s) {
                std::size_t v = rng.uniform_index(N);
                acc += g.local_frustration(v);
            }
        }
        sink ^= acc;
    };
    auto mixed_updates_cached = [&](){
        std::uint64_t acc = 0;
        for (int it=0; it<5000; ++it) {
            std::uint32_t a = static_cast<std::uint32_t>(rng.uniform_u64(K+1));
            std::uint32_t b = static_cast<std::uint32_t>(rng.uniform_u64(K+1));
            if (a == b) b = (b+1)%(K+1);
            gc.change_color(a,b);
            for (int s=0; s<8; ++s) {
                std::size_t v = rng.uniform_index(N);
                acc += gc.local_frustration(v);
            }
        }
        sink ^= acc;
    };

    auto t_read_base   = bench(read_heavy_baseline);
    auto t_read_cached = bench(read_heavy_cached);
    auto t_mix_base    = bench(mixed_updates_baseline);
    auto t_mix_cached  = bench(mixed_updates_cached);
    std::cout << "read_local_frustration baseline: " << t_read_base   << " ns\n";
    std::cout << "read_local_frustration cached  : " << t_read_cached << " ns\n";
    std::cout << "mixed updates baseline         : " << t_mix_base    << " ns\n";
    std::cout << "mixed updates cached           : " << t_mix_cached  << " ns\n";
    (void)sink;
    return 0;
}
