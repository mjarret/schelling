// Main entry: run many Schelling processes on a Lollipop graph and build a heatmap
#include <cstdint>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <algorithm>

#include <omp.h>

#include "graphs/lollipop.hpp"
#include "core/rng.hpp"
#include "core/schelling_threshold.hpp"
#include "sim/sim.hpp"              // includes run_schelling_process_visit overload
#include "sim/job_handler.hpp"      // contains run_jobs_heatmap_streamed + to_dense
#include "cli/cli.hpp"

// ---- Build-time graph sizes (override with -DLOLLIPOP_CLIQUE=... -DLOLLIPOP_PATH=...) ----
#ifndef LOLLIPOP_CLIQUE
#define LOLLIPOP_CLIQUE 100000
#endif
#ifndef LOLLIPOP_PATH
#define LOLLIPOP_PATH 900000
#endif

using G = graphs::LollipopGraph<LOLLIPOP_CLIQUE, LOLLIPOP_PATH>;

int main(int argc, char** argv) {
    // Parse CLI
    bool want_help = false; std::string help_text;
    cli::Options opt = cli::parse_args(argc, argv, want_help, help_text);
    if (want_help) { std::cout << help_text; return 0; }

    // Initialize Schelling threshold (tau defaults to 1/2)
    core::schelling::init_program_threshold(opt.p, opt.q);

    // Job handler configuration:
    sim::JobConfig cfg{ .jobs = opt.experiments, .density = opt.agent_density, .threads = opt.threads };

    // Deterministic master RNG (constant seed by default; set SEED env to override)
    std::uint64_t seed = 123456789ULL;
    if (const char* es = std::getenv("SEED")) {
        unsigned long long v = std::strtoull(es, nullptr, 10);
        if (v != 0ULL) seed = static_cast<std::uint64_t>(v);
    }
    core::Xoshiro256ss master_rng(seed);

    // ---- Run ----
    double avg_steps = sim::run_jobs_hitting_time<G>(cfg, master_rng) / static_cast<double>(opt.experiments);
    std::cout << "Average steps: " << avg_steps << "\n";
    return 0;
}
