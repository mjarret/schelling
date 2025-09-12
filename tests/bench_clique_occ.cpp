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
    std::array<u64, K+1> counts{}; // counts[0] = unoccupied
    u64 occ{0};

    explicit CliqueOcc(std::size_t n_, const std::array<u64, K+1>& c) : n(n_), counts(c) {
        occ = n - counts[0];
    }
    inline u64 total_occupied() const { return occ; }
    inline std::size_t size() const { return n; }
    inline std::uint32_t get_color(std::size_t v) const {
        std::array<u64, K+1> prefix{};
        std::partial_sum(counts.begin(), counts.end(), prefix.begin());
        auto it = std::upper_bound(prefix.begin(), prefix.end(), static_cast<u64>(v));
        return static_cast<std::uint32_t>(std::distance(prefix.begin(), it));
    }
    inline std::uint64_t local_frustration(std::size_t v) const {
        return total_occupied() - counts[get_color(v)];
    }
    inline void change_color(std::uint32_t from, std::uint32_t to) {
        if (from == to) return;
        // mirror production logic
        if (to == 0) { // a -> 0
            // delta tf not tracked here; only occ
            if (counts[from]) --counts[from];
            ++counts[0];
            --occ;
            return;
        }
        if (from == 0) { // 0 -> b
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
static inline std::uint64_t bench(Fn&& fn, int iters=5) {
    std::uint64_t total_ns = 0;
    for (int i=0;i<iters;++i) {
        auto t0 = Clock::now(); fn(); auto t1 = Clock::now();
        total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    }
    return total_ns / iters;
}

int main() {
    constexpr std::size_t K = 2;
    const std::size_t N = 1u << 18; // 262,144 vertices
    core::Xoshiro256ss rng(42);

    // Build counts ~70% occupied, split evenly between colors 1..K
    std::array<std::uint64_t, K+1> counts{};
    counts[0] = static_cast<std::uint64_t>(N * 0.30);
    const std::uint64_t occ = N - counts[0];
    for (std::size_t c = 1; c <= K; ++c) counts[c] = occ / K + ((c <= (occ % K)) ? 1 : 0);

    // Baseline graph
    graphs::CliqueGraph<K> g(N, counts);
    // Cached occ variant
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
            // random color transition including 0
            std::uint32_t a = static_cast<std::uint32_t>(rng.uniform_u64(K+1));
            std::uint32_t b = static_cast<std::uint32_t>(rng.uniform_u64(K+1));
            if (a == b) b = (b+1)%(K+1);
            // emulate change by direct counts in production graph
            // We only benchmark reads, so just sample a handful of vertices after logical change
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

