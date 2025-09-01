#pragma once
/*
Experiment orchestrator (scatter + global averaged-fit + anytime stop)

- Scatter: reservoir-sampled (moves, unhappy_fraction).
- Fit line: GLOBAL bin averages (all points so far), updated at the end of each run.
- Overlay (right margin): runs, eps (2*w_n achieved), α_rem, δ_n.
- Stop: when eps_current <= --eps, join workers and exit.

Fixes for your issues:
- The fit line no longer uses the reservoir (so it stabilizes).
- Scatter and line are drawn in ONE gnuplot command to avoid alternating frames.
- Guarded against empty datasets to avoid "warning: empty x range".
*/

#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <cstdio>
#include <mutex>
#include <numeric>
#include <cmath>
#include <chrono>
#include <limits>
#include <algorithm>

#include "../cli/cli.hpp"
#include "../graphs/torus_grid.hpp"
#include "../graphs/lollipop_graph.hpp"
#include "../sim/world_packed.hpp"
#include "../sim/metrics.hpp"
#include "../sim/random_stepper.hpp"
#include "../rng/splitmix64.hpp"
#include "../stats/dyn_cap.hpp"
#include "../stats/eb_cs.hpp"
#include "../io/live_plot.hpp"
#include "../io/scatter_reservoir.hpp"

// -------------------- Single-run scatter producer --------------------

struct RunScatter {
    bool settled{false};
    std::size_t moves{0};
    std::vector<double> xs, ys; // full trajectory for global bins
};

template<class Graph>
static RunScatter run_single_scatter(const Graph& G,
                                     double density, double tau,
                                     std::uint64_t seed,
                                     ScatterReservoir& reservoir,
                                     std::size_t plot_every_moves = 10)
{
    RunScatter rc{};
    WorldPacked W; W.resize(G.num_vertices());
    SplitMix64 rng(seed);

    // Random placement
    for (std::size_t v=0; v<W.N; ++v) {
        if (rng.next_unit_double() < density) {
            const bool t1 = (rng.next_u64() & 1ull);
            W.set_occupied(v, t1);
        } else {
            W.set_empty(v);
        }
    }

    Metrics<Graph> M; M.bind(&G, &W, tau);
    RandomStepperAny<Graph> stepper; stepper.bind(&G, &W, &M);

    auto unhappy_frac = [&]() {
        const double occ = static_cast<double>(W.occupied.size());
        return (occ <= 0.0) ? 0.0 : static_cast<double>(M.unhappy_total) / occ;
    };

    std::size_t move = 0, next_emit = 0;
    DynamicCap cap; cap.reset();
    cap.configure(1000, 1000, 1e-6);
    const double delta_cap = 1e-6;

    // Initial point
    {
        const double f0 = unhappy_frac();
        rc.xs.push_back(0.0);
        rc.ys.push_back(f0);
        reservoir.add(0.0, f0, rng);
        if (M.unhappy_total == 0) { rc.settled = true; rc.moves = 0; return rc; }
    }

    std::size_t last_unhappy = M.unhappy_total;
    while (true) {
        if (!stepper.step(rng)) break; // no move possible
        ++move;
        const double frac = unhappy_frac();

        rc.xs.push_back(static_cast<double>(move));
        rc.ys.push_back(frac);

        if (move >= next_emit) {
            reservoir.add(static_cast<double>(move), frac, rng);
            next_emit = move + plot_every_moves;
        }

        const bool decreased = (M.unhappy_total < last_unhappy);
        last_unhappy = M.unhappy_total;
        cap.on_move(decreased, M.unhappy_total, delta_cap);

        if (M.unhappy_total == 0) { rc.settled = true; break; }
        if (cap.exceeded())       { rc.settled = false; break; }
    }

    rc.moves = move;
    const double fend = unhappy_frac();
    rc.xs.push_back(static_cast<double>(move));
    rc.ys.push_back(fend);
    reservoir.add(static_cast<double>(move), fend, rng);
    return rc;
}

// -------------------- Global bin accumulator (ALL data) --------------------

struct GlobalBins {
    std::size_t num_bins;
    double xmin, xmax;
    std::vector<double> sum, count;
    mutable std::mutex mu;

    GlobalBins(std::size_t nb=100,double x0=0,double x1=1e6)
      : num_bins(nb), xmin(x0), xmax(x1),
        sum(nb,0.0), count(nb,0.0) {}

    inline std::size_t bin_of(double x) const {
        if (x <= xmin) return 0;
        if (x >= xmax) return num_bins - 1;
        const double r = (x - xmin) / (xmax - xmin);
        return static_cast<std::size_t>(r * (num_bins - 1));
    }

    void add_run(const std::vector<double>& xs,
                 const std::vector<double>& ys) {
        std::lock_guard<std::mutex> lk(mu);
        const std::size_t n = std::min(xs.size(), ys.size());
        for (std::size_t i = 0; i < n; ++i) {
            const double x = xs[i], y = ys[i];
            const std::size_t b = bin_of(x);
            sum[b]   += y;
            count[b] += 1.0;
        }
    }

    void averages(std::vector<double>& bx,
                  std::vector<double>& by) const {
        std::lock_guard<std::mutex> lk(mu);
        bx.clear(); by.clear();
        bx.reserve(num_bins); by.reserve(num_bins);
        for (std::size_t b = 0; b < num_bins; ++b) {
            if (count[b] > 0.0) {
                const double xb = xmin + (xmax - xmin) * (double(b) / (num_bins - 1));
                bx.push_back(xb);
                by.push_back(sum[b] / count[b]);
            }
        }
    }
};

// -------------------- App entry --------------------

inline int run_app(const CLIOptions& opt) {
    // Graph kind
    std::string gname = opt.graph;
    for (auto& c : gname) c = std::tolower(c);

    auto make_seed = [&](std::uint64_t salt){
        std::uint64_t s = opt.seed;
        if (s == 0) {
            std::random_device rd;
            s = (static_cast<std::uint64_t>(rd()) << 32) ^ rd();
        }
        SplitMix64 mix(s ^ (salt * 0x9E3779B97F4A7C15ULL));
        return mix.next_u64();
    };

    const bool is_torus = (gname == "torus");
    const bool is_lolli = (gname == "lollipop");
    if (!is_torus && !is_lolli) {
        std::fprintf(stderr, "Unknown --graph: %s (expected torus|lollipop)\n", opt.graph.c_str());
        return 2;
    }

    // Parse size
    std::size_t W=0,H=0,m=0,L=0;
    if (is_torus) {
        const auto x = opt.size.find('x');
        if (x == std::string::npos) {
            std::fprintf(stderr, "For torus, --size must be WxH\n");
            return 2;
        }
        W = std::stoul(opt.size.substr(0, x));
        H = std::stoul(opt.size.substr(x+1));
    } else {
        const auto c = opt.size.find(':');
        if (c == std::string::npos) {
            std::fprintf(stderr, "For lollipop, --size must be m:L\n");
            return 2;
        }
        m = std::stoul(opt.size.substr(0, c));
        L = std::stoul(opt.size.substr(c+1));
    }

    // Plot + data structures
    ScatterReservoir reservoir(/*capacity=*/100000);
    GlobalBins gbins(/*num_bins=*/100, /*xmin=*/0.0, /*xmax=*/1e6);

    GnuplotLive plot("Unhappiness vs. moves (all-data fit)");
    if (plot.valid()) {
        plot.cmd("set xlabel 'moves'");
        plot.cmd("set ylabel 'unhappy fraction'");
        plot.cmd("set key left top");
        plot.cmd("set rmargin at screen 0.80"); // space for overlays
        // Note: do NOT set xrange/yrange; let autoscale adapt to data
    }

    std::atomic<bool> stop{false};
    std::atomic<std::size_t> runs_completed{0};
    std::atomic<std::size_t> runs_settled{0};

    // Worker
    auto worker = [&](std::uint64_t salt){
        const std::uint64_t base_seed = make_seed(salt);
        SplitMix64 thread_rng(base_seed);

        if (is_torus) {
            TorusGrid G(W,H);
            while (!stop.load(std::memory_order_relaxed)) {
                const auto seed = thread_rng.next_u64();
                auto r = run_single_scatter(G, opt.density, opt.threshold, seed, reservoir, /*plot_every_moves=*/10);
                gbins.add_run(r.xs, r.ys); // add ALL the points to global bins
                runs_completed.fetch_add(1, std::memory_order_relaxed);
                if (r.settled) runs_settled.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            LollipopGraph G(m, L);
            while (!stop.load(std::memory_order_relaxed)) {
                const auto seed = thread_rng.next_u64();
                auto r = run_single_scatter(G, opt.density, opt.threshold, seed, reservoir, /*plot_every_moves=*/10);
                gbins.add_run(r.xs, r.ys);
                runs_completed.fetch_add(1, std::memory_order_relaxed);
                if (r.settled) runs_settled.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    // Launch threads
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const unsigned T = std::max(1u, hw > 2 ? hw - 1 : hw);
    std::vector<std::thread> threads;
    threads.reserve(T);
    for (unsigned i=0;i<T;++i) threads.emplace_back(worker, static_cast<std::uint64_t>(i+1));

    // UI loop
    auto now_sec = []{
        using clock = std::chrono::steady_clock;
        return std::chrono::duration_cast<std::chrono::duration<double>>(clock::now().time_since_epoch()).count();
    };
    double last_plot = now_sec();

    // Risk bookkeeping
    std::size_t last_n_accum = 0;
    double H2_accum = 0.0;
    const double PI = 3.14159265358979323846;

    bool should_break = false;

    for (;;) {
        const double t = now_sec();
        const std::size_t n = runs_completed.load(std::memory_order_relaxed);
        const std::size_t s = runs_settled.load(std::memory_order_relaxed);

        if (n > last_n_accum) {
            for (std::size_t m_it = last_n_accum + 1; m_it <= n; ++m_it) {
                H2_accum += 1.0 / (static_cast<double>(m_it) * static_cast<double>(m_it));
            }
            last_n_accum = n;
        }

        double eps_current = std::numeric_limits<double>::infinity();
        if (n >= 2) {
            auto st = ebcs::update_and_bound(n, s, opt.alpha);
            eps_current = st.two_w;
            if (eps_current <= opt.eps) {
                stop.store(true, std::memory_order_relaxed);
                should_break = true;
            }
        }

        // Refresh
        if (plot.valid() && (t - last_plot > 0.5)) {
            // Overlays in right margin
            const double alpha_used = opt.alpha * (6.0 / (PI * PI)) * H2_accum;
            const double alpha_rem  = std::max(0.0, opt.alpha - alpha_used);
            const double delta_n    = (n > 0) ? opt.alpha * (6.0 / (PI * PI)) / (double(n) * double(n)) : 0.0;

            plot.cmd("set label 900 'runs: " + std::to_string(n) + "' at screen 0.98,0.95 right front");
            {
                char buf[128]; std::snprintf(buf, sizeof(buf), "eps: %.6g", eps_current);
                plot.cmd(std::string("set label 901 '") + buf + "' at screen 0.98,0.92 right front");
            }
            {
                char buf1[128], buf2[128];
                std::snprintf(buf1, sizeof(buf1), "α_rem: %.6g", alpha_rem);
                std::snprintf(buf2, sizeof(buf2), "δ_n: %.6g", delta_n);
                plot.cmd(std::string("set label 902 '") + buf1 + "' at screen 0.98,0.89 right front");
                plot.cmd(std::string("set label 903 '") + buf2 + "' at screen 0.98,0.86 right front");
            }

            // Snapshot scatter + compute global averaged fit (all data)
            std::vector<double> px, py; reservoir.snapshot(px, py);
            std::vector<double> bx, by; gbins.averages(bx, by);

            // Draw both in a single frame; guard empties to avoid "empty x range"
            if (!px.empty() || (by.size() >= 2)) {
                plot.plot_points_and_lines(px, py, bx, by, "scatter", "fit", /*ps*/0.2);
            } else if (!px.empty()) {
                // Only scatter available yet
                plot.plot_points(px, py, "scatter", 0.2);
            } else if (by.size() >= 2) {
                // Only fit has enough points (unlikely early on)
                plot.plot_lines(bx, by, "fit");
            }
            last_plot = t;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (should_break) break;
    }

    // Final frame
    if (plot.valid()) {
        plot.cmd("set label 904 'stopped: eps<=target' at screen 0.98,0.83 right front");
        std::vector<double> px, py; reservoir.snapshot(px, py);
        std::vector<double> bx, by; gbins.averages(bx, by);
        if (!px.empty() || (by.size() >= 2)) {
            plot.plot_points_and_lines(px, py, bx, by, "scatter", "fit", 0.2);
        } else if (!px.empty()) {
            plot.plot_points(px, py, "scatter", 0.2);
        } else if (by.size() >= 2) {
            plot.plot_lines(bx, by, "fit");
        }
    }

    for (auto& th : threads) th.join();
    std::fprintf(stderr, "\nDONE: met epsilon target. runs=%zu\n", runs_completed.load());
    return 0;
}
