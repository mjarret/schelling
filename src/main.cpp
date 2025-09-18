#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include "core/approx.hpp"
#include "core/rng.hpp"
#include "core/schelling_threshold.hpp"
#include "graphs/lollipop_graph.hpp"
#include "sim/schelling_sim.hpp"

namespace {

/**
 * @brief Print command-line usage help.
 */
void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
                 "Options:\n"
                 "  --help                 Show this help\n"
                 "  --seed <u64>           RNG seed\n"
                 "  --clique <u64>         Clique size (default 32)\n"
                 "  --tail <u64>           Path length (default 32)\n"
                 "  --tau <f64>            Satisfaction threshold in [0,1] (default 0.5)\n"
                 "  --occupancy <f64>      Fraction of occupied sites (default 0.80)\n"
                 "  --max-steps <u64>      Relocation cap (default unlimited)\n"
                 "  --quiet                Suppress simulation report\n";
}

/**
 * @brief Monotonic-ish tick count for default seeding.
 */
inline std::uint64_t now_ticks() {
    return static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

} // namespace

/**
 * @brief CLI entry: sets up graph, threshold, RNG; runs simulation.
 */
int main(int argc, char** argv) {
    std::uint64_t n_clique = 32;
    std::uint64_t n_path = 32;
    std::uint64_t seed = 0;
    bool seed_set = false;
    double tau = 0.5;
    double occupancy = 0.80;
    std::uint64_t max_steps = std::numeric_limits<std::uint64_t>::max();
    bool quiet = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--help") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::strtoull(argv[++i], nullptr, 10);
            seed_set = true;
        } else if (arg == "--clique" && i + 1 < argc) {
            n_clique = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--tail" && i + 1 < argc) {
            n_path = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--tau" && i + 1 < argc) {
            tau = std::stod(argv[++i]);
        } else if (arg == "--occupancy" && i + 1 < argc) {
            occupancy = std::stod(argv[++i]);
        } else if (arg == "--max-steps" && i + 1 < argc) {
            max_steps = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--quiet") {
            quiet = true;
        } else {
            std::cerr << "Unknown or incomplete option: " << arg << "\n";
            print_help(argv[0]);
            return 2;
        }
    }

    if (!seed_set) seed = now_ticks();

    const std::uint64_t total_vertices = n_clique + n_path;
    const std::uint64_t max_agents = total_vertices == 0 ? 0 : (total_vertices - 1);
    double occ = occupancy;
    if (occ < 0.0) occ = 0.0;
    if (occ > 1.0) occ = 1.0;
    std::uint64_t target_agents = static_cast<std::uint64_t>(occ * static_cast<double>(total_vertices));
    if (target_agents > max_agents) target_agents = max_agents;
    const std::uint64_t color1 = target_agents / 2;
    const std::uint64_t color0 = target_agents - color1;

    const auto rat = core::approx::best_with_max_den(
        tau, static_cast<color_count_t>(std::max<std::uint64_t>(1, total_vertices)));
    core::schelling::program_threshold = core::schelling::PqThreshold(rat.p, rat.q);
    core::schelling::program_threshold_initialized = true;

    graphs::LollipopGraph graph(n_clique, n_path);
    core::Xoshiro256ss rng(seed);

    sim::populate_random(graph, color0, color1, rng);
    const std::uint64_t steps = sim::run_until_stable(graph, rng, max_steps);
    const std::uint64_t unhappy = graph.unhappy_count();

    if (!quiet) {
        std::cout << "steps=" << steps
                  << " unhappy=" << unhappy
                  << " occupied=" << (color0 + color1)
                  << " total_vertices=" << total_vertices
                  << "\n";
    }

    return unhappy == 0 ? 0 : 1;
}
