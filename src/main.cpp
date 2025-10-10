// Main entry: run Schelling processes on a Lollipop graph
#include <cstdint>
#include <iostream>
#include <cstdlib>
#include <filesystem>

#include "graphs/lollipop.hpp"
#include "core/rng.hpp"
#include "core/schelling_threshold.hpp"
#include "sim/sim.hpp"
#include "cli/cli.hpp"
#include "jit/jit.hpp"

static int run_once_jit(std::size_t cs, std::size_t pl, std::uint64_t p, std::uint64_t q, double density) {
    unsigned long long moves = 0, final_u = 0;
    std::string log;
    int rc = jit::run_lollipop_once(cs, pl, p, q, density, moves, final_u, &log);
    if (rc != 0) {
        std::cerr << "JIT compile/run failed (code " << rc << ")\n";
        return rc;
    }
    (void)moves; (void)final_u;
    return 0;
}

int main(int argc, char** argv) {
    // Parse CLI
    bool want_help = false; std::string help_text;
    cli::Options opt = cli::parse_args(argc, argv, want_help, help_text);
    if (want_help) { std::cout << help_text; return 0; }

    // Initialize Schelling threshold (tau defaults to 1/2)
    core::schelling::init_program_threshold(
        static_cast<core::color_count_t>(opt.p),
        static_cast<core::color_count_t>(opt.q));

    // Determine agent density (default 0.8 if unspecified)
    const double density = opt.agent_density.has_value() ? *opt.agent_density : 0.8;

    // Compile-and-run specialized LollipopGraph at requested sizes
    int rc = run_once_jit(opt.clique_size, opt.path_length, opt.p, opt.q, density);
    if (rc == 0) {
        // On successful completion, remove JIT cache directory
        try {
            std::filesystem::remove_all("_jit");
        } catch (...) {
            // Best-effort cleanup; ignore failures
        }
    }
    return rc;
}
