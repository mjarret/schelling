#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <chrono>

#include "graphs/lollipop_graph.hpp"
#include "graphs/path_graph.hpp"
#include "graphs/clique_graph.hpp"
#include "sim/schelling_next.hpp"
#include "core/rng.hpp"

#ifndef K
#define K 2
#endif

#ifndef N
#define N 64
#endif

static void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --help                 Show this help\n"
              << "  --seed <u64>           RNG seed\n"
              << "  --steps <u64>          Max steps (default 100000)\n"
              << "  --stats-every <u64>    Snapshot interval (default 1000)\n"
              << "  --quiet                Suppress summary output\n"
              << "  --n <n>                Number of vertices (default compile-time)\n";
}

int main(int argc, char** argv) {
    using Graph = graphs::LollipopGraph<K>;

    uint64_t seed = 0; bool seed_set = false;
    uint64_t max_steps = 100000; uint64_t stats_every = 1000; bool quiet = false;
    std::size_t n = N;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") { print_help(argv[0]); return 0; }
        else if (arg == "--seed" && i + 1 < argc) { seed = std::strtoull(argv[++i], nullptr, 10); seed_set = true; }
        else if (arg == "--steps" && i + 1 < argc) { max_steps = std::strtoull(argv[++i], nullptr, 10); }
        else if (arg == "--n" && i + 1 < argc) { n = std::strtoull(argv[++i], nullptr, 10); }
        else if (arg == "--epsilon" && i + 1 < argc) { /* epsilon = std::stod(argv[++i]); */ }
        else { std::cerr << "Unknown or incomplete option: " << arg << "\n"; print_help(argv[0]); return 2; }
    }

    if (!seed_set) seed = static_cast<uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());

    // Lollipop default split: half clique, half path
    const std::size_t n_clique = n / 2;
    const std::size_t n_path   = n - n_clique;
    Graph g(n_clique, n_path);

    // Initialize colors: 70% occupancy, colors 1..K uniform
    core::Xoshiro256ss rng(seed);
    for (std::size_t v = 0; v < g.size(); ++v) {
        const bool occ = (rng.uniform01() < 0.70);
        const std::uint32_t c = occ ? (1u + static_cast<std::uint32_t>(rng.uniform_u64(K) % K)) : 0u;
        g.change_color(static_cast<index_t>(v), c);
    }

    std::uint64_t frustrated = g.total_frustration();
    if (!quiet) {
        std::cout << "seed=" << seed
                  << " n=" << g.size()
                  << " initial_frustration=" << frustrated
                  << "\n";
    }

    for (std::uint64_t step = 1; step <= max_steps && frustrated != 0; ++step) {
        const auto delta = sim::schelling_next(g, rng);
        if (delta != 0) frustrated = g.total_frustration();
        if (!quiet && (step % stats_every == 0)) {
            std::cout << "step=" << step << " frustration=" << frustrated << "\n";
        }
    }

    if (!quiet) {
        std::cout << "final_frustration=" << g.total_frustration() << "\n";
    }
    return 0;
}
