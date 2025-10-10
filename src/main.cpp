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
#define LOLLIPOP_CLIQUE 50
#endif
#ifndef LOLLIPOP_PATH
#define LOLLIPOP_PATH 450
#endif

using G = graphs::LollipopGraph<LOLLIPOP_CLIQUE, LOLLIPOP_PATH>;

// Simple CSV writer for a dense heatmap (rows × bins, row-major)
static void write_heatmap_csv(const sim::Heatmap& hm, const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) { std::cerr << "Failed to open '" << path.string() << "' for writing\n"; return; }
    for (std::size_t r = 0; r < hm.rows; ++r) {
        const std::uint64_t* row = hm.data.data() + r * hm.bins;
        for (std::size_t b = 0; b < hm.bins; ++b) {
            if (b) out << ',';
            out << row[b];
        }
        out << '\n';
    }
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

    const double density = opt.agent_density;

    // Job handler configuration:
    // - jobs and threads optionally taken from env (JOBS / OMP_NUM_THREADS) to keep this file self-contained.
    //   If your cli::Options already provides these, feel free to replace the env reads with opt.*.
    std::size_t jobs = 1000;
    if (const char* ej = std::getenv("JOBS")) {
        unsigned long long v = std::strtoull(ej, nullptr, 10);
        if (v > 0) jobs = static_cast<std::size_t>(v);
    }
    int threads = 0; // 0 -> omp_get_max_threads()
    if (const char* et = std::getenv("OMP_NUM_THREADS")) {
        int v = std::atoi(et);
        if (v > 0) threads = v;
    }

    sim::JobConfig cfg{ .jobs = jobs, .density = density, .threads = threads };

    // Deterministic master RNG (constant seed by default; set SEED env to override)
    std::uint64_t seed = 123456789ULL;
    if (const char* es = std::getenv("SEED")) {
        unsigned long long v = std::strtoull(es, nullptr, 10);
        if (v != 0ULL) seed = static_cast<std::uint64_t>(v);
    }
    core::Xoshiro256ss master_rng(seed);

    // Progress callback (prints ~100 updates max)
    const std::size_t tick_every = std::max<std::size_t>(1, cfg.jobs / 100);
    auto on_progress = [tick_every](std::size_t done, std::size_t total) {
        if (done == total || (done % tick_every) == 0) {
            double pct = (100.0 * static_cast<double>(done)) / static_cast<double>(total);
            std::cerr << "\rRunning jobs: " << done << "/" << total
                      << " (" << static_cast<int>(pct) << "%)" << std::flush;
            if (done == total) std::cerr << "\n";
        }
    };

    // ---- Run: stream increments directly into per-step histograms (no per-job histories) ----
    // This uses the OpenMP reduction in job_handler.hpp to merge thread-local maps efficiently.
    auto sparse = sim::run_jobs_heatmap_streamed<G>(cfg, master_rng, on_progress);

    // Convert to a dense matrix if you want to export/plot it
    const std::size_t bins = static_cast<std::size_t>(G::TotalSize) + 1;
    sim::Heatmap hm = sim::to_dense(sparse, bins);

    // Emit a CSV (set HEATMAP_CSV=path to override)
    std::filesystem::path out_csv = "heatmap.csv";
    if (const char* hp = std::getenv("HEATMAP_CSV")) out_csv = hp;
    write_heatmap_csv(hm, out_csv);
    std::cout << "Wrote heatmap to " << out_csv.string()
              << " (" << hm.rows << " rows × " << hm.bins << " bins)\n";

    // Best-effort cleanup of any legacy JIT cache directory (harmless if absent)
    try {
        std::filesystem::remove_all("_jit");
    } catch (...) {
        // ignore
    }
    return 0;
}
