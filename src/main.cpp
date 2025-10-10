// Minimal main: run the Schelling process with a Lollipop graph
#include <cstdint>
#include <random>
#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <omp.h>
#include <cstdlib>
#include <cstring>
#include "graphs/lollipop.hpp"
#include "core/rng.hpp"
#include "core/schelling_threshold.hpp"
#include "sim/sim.hpp"

int main(int argc, char** argv) {
    // Seed RNG from system entropy
    std::random_device rd;
    const std::uint64_t seed = (static_cast<std::uint64_t>(rd()) << 32) ^ static_cast<std::uint64_t>(rd());
    core::Xoshiro256ss rng(seed);

    // Initialize Schelling threshold (tau = 1/2 by default)
    core::schelling::init_program_threshold(1, 2);

    // Instantiate a Lollipop graph with default sizes (CS=50, PL=450)
    graphs::LollipopGraph<> g;

    // Run the lollipop-specific process: fill 4/5 randomly with alternating colors,
    // then iterate moving unhappy agents until stable.
    auto history = sim::run_lollipop_process(g, rng);

    // Print a short summary
    std::cout << "Lollipop process finished in " << (history.size() ? history.size()-1 : 0)
              << " moves. Final unhappy_count= " << (history.empty() ? 0 : history.back())
              << "\n";

    // ---------------------------------------------------------------
    // Massive parallel run: collect heatmap of step index vs unhappy_count
    // ---------------------------------------------------------------
    // Tunables
    #ifndef LOLLIPOP_HEATMAP_PROCESSES
    #define LOLLIPOP_HEATMAP_PROCESSES 50000000ULL
    #endif
    #ifndef LOLLIPOP_HEATMAP_MAX_STEPS
    #define LOLLIPOP_HEATMAP_MAX_STEPS 1024
    #endif

    // Allow overriding via CLI or environment to make large runs practical.
    std::uint64_t processes = LOLLIPOP_HEATMAP_PROCESSES;
    std::size_t max_steps = LOLLIPOP_HEATMAP_MAX_STEPS;
    if (const char* env_p = std::getenv("LOLLIPOP_HEATMAP_PROCESSES")) {
        processes = std::strtoull(env_p, nullptr, 10);
    }
    if (const char* env_s = std::getenv("LOLLIPOP_HEATMAP_MAX_STEPS")) {
        max_steps = static_cast<std::size_t>(std::strtoull(env_s, nullptr, 10));
    }
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--processes") == 0) {
            processes = std::strtoull(argv[i+1], nullptr, 10);
        } else if (std::strcmp(argv[i], "--max-steps") == 0) {
            max_steps = static_cast<std::size_t>(std::strtoull(argv[i+1], nullptr, 10));
        }
    }

    using G = graphs::LollipopGraph<>; // default sizes
    static constexpr std::size_t UNHAPPY_BINS = G::TotalSize + 1; // 0..N
    // Clamp runtime parameters to sane minima
    if (processes == 0) processes = 1;
    if (max_steps == 0) max_steps = 1;
    const std::size_t MAX_STEPS = max_steps;

    const std::size_t cells = MAX_STEPS * UNHAPPY_BINS;

    // Per-thread heatmaps to avoid synchronization on the hot path
    const int T = omp_get_max_threads();
    if (cells == 0) {
        std::cerr << "Heatmap dimensions collapsed (cells=0).\n";
        return 1;
    }

    // We batch work in chunks to limit per-thread buffers and runtime.
    const std::uint64_t chunk_size = std::min<std::uint64_t>(processes, 1000000ULL);

    std::vector<unsigned long long> global_heat(cells, 0ull);

    std::uint64_t processed = 0;
    while (processed < processes) {
        const std::uint64_t remaining = processes - processed;
        const std::uint64_t this_chunk = std::min(remaining, chunk_size);
        const std::size_t total_cells = static_cast<std::size_t>(T) * cells;
        std::vector<unsigned long long> heat_by_thread(total_cells, 0ull);

        omp_set_dynamic(0);
        omp_set_num_threads(T);

    // Parallel fan-out over many independent processes
    #pragma omp parallel num_threads(T)
    {
        const int tid = omp_get_thread_num();
        const int NT  = omp_get_num_threads();
        // Static partition of [0, processes)
        const std::uint64_t base = this_chunk / NT;
        const std::uint64_t rem  = this_chunk % NT;
        const std::uint64_t my_count = base + (static_cast<std::uint64_t>(tid) < rem ? 1ULL : 0ULL);

        core::Xoshiro256ss thr_rng(core::splitmix_hash(0xC0FFEEULL ^ (unsigned long long)tid));
        unsigned long long* local_heat = heat_by_thread.data() + static_cast<std::size_t>(tid) * cells;

        for (std::uint64_t k = 0; k < my_count; ++k) {
            // Per-process RNG derived from thread RNG
            core::Xoshiro256ss rng(core::splitmix_hash(thr_rng()));
            G g;
            // Initialize to 4/5 occupancy with alternating colors
            sim::initialize_lollipop_four_fifths(g, rng);
            // Record step 0 (initial unhappy)
            std::size_t u = g.unhappy_count();
            {
                std::size_t col = (u < UNHAPPY_BINS) ? u : (UNHAPPY_BINS - 1);
                local_heat[0 * UNHAPPY_BINS + col] += 1u;
            }
            // Evolve until stable; stream into heatmap without storing history
            std::size_t step = 1;
            while (u > 0) {
                const std::size_t from = g.get_unhappy(rng);
                const bool c = g.pop_agent(from);
                const std::size_t to = g.get_unoccupied(rng);
                g.place_agent(to, c);
                u = g.unhappy_count();
                const std::size_t row = (step < MAX_STEPS) ? step : (MAX_STEPS - 1);
                std::size_t col = (u < UNHAPPY_BINS) ? u : (UNHAPPY_BINS - 1);
                local_heat[row * UNHAPPY_BINS + col] += 1u;
                ++step;
            }
        }

        }

        // Merge this chunk into the global heatmap
        for (int t = 0; t < T; ++t) {
            const unsigned long long* lh = heat_by_thread.data() + static_cast<std::size_t>(t) * cells;
            for (std::size_t i = 0; i < cells; ++i) global_heat[i] += lh[i];
        }

        processed += this_chunk;
        std::cout << "Processed chunk ending at " << processed << " / " << processes << '\n';
    }

    // Create a heatmap image (PPM) with log-scaled intensities
    unsigned long long max_count = 0;
    for (auto v : global_heat) if (v > max_count) max_count = v;
    const double log_max = std::log(1.0 + static_cast<double>(max_count));

    const std::size_t width  = UNHAPPY_BINS;
    const std::size_t height = MAX_STEPS;
    std::ofstream ppm("heatmap.ppm", std::ios::binary);
    ppm << "P6\n" << width << " " << height << "\n255\n";
    for (std::size_t y = 0; y < height; ++y) {
        const std::size_t row = (height - 1 - y); // plot earliest steps at top
        for (std::size_t x = 0; x < width; ++x) {
            const unsigned long long v = global_heat[row * UNHAPPY_BINS + x];
            const double norm = (log_max > 0.0) ? (std::log(1.0 + static_cast<double>(v)) / log_max) : 0.0;
            const unsigned char c = static_cast<unsigned char>(std::round(255.0 * norm));
            // Simple grayscale
            ppm.put(static_cast<char>(c));
            ppm.put(static_cast<char>(c));
            ppm.put(static_cast<char>(c));
        }
    }
    ppm.close();
    std::cout << "Heatmap written to heatmap.ppm (" << width << "x" << height << ")\n";
    std::cout << "Aggregated processes: " << processes << ", max_steps binned: " << MAX_STEPS
              << ", threads: " << omp_get_max_threads() << "\n";
    return 0;
}
