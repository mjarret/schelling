#pragma once
#include <atomic>
#include <cstdint>
#include <vector>
#include <thread>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <limits>
#include <algorithm>

#include "cli.hpp"
#include "seeds.hpp"
#include "checkpoints.hpp"
#include "cost_aggregator.hpp"
#include "experiment.hpp"
#include "progress_manager.hpp"
#include "plot_manager.hpp"
#include "cs_stop.hpp"

#include "geometry.hpp"          // Torus2D
#include "geometry_lollipop.hpp" // LollipopGraph
#include "world_concept.hpp"
#include "world_grid_packed.hpp"
#include "world_agent_packed.hpp"
#include "histogram.hpp"
#include "histogram_plotter.hpp"


#if defined(USE_AGENT_PACKED)
  using Storage = WorldAgentPacked;
#else
  #ifndef TYPE_BITS
    #define TYPE_BITS 1
  #endif
  using Storage = WorldGridPacked<TYPE_BITS, DefaultWord>;
#endif

template<class Geometry>
static void run_workers_for_geometry_cs(const CLIOptions& opt,
                                        Geometry& geom,
                                        const std::vector<uint64_t>& checkpoints,
                                        CostAggregator& meanCurve)
{
    // thread plan
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 2;
    const unsigned reserve = (hw >= 6 ? 2u : 1u);    // leave 1–2 cores free by default
    unsigned threads = (opt.threads <= 0 ? (hw > reserve ? hw - reserve : 1u)
                                         : std::min<unsigned>((unsigned)opt.threads, hw ? hw : 1u));

    std::cerr << "HW threads: " << hw << "  using " << threads
              << " worker thread(s) (reserved " << reserve << ")\n" << std::flush;

    uint64_t baseSeed = (opt.seed == 0 ? make_random_seed() : opt.seed);

    // shared state
    std::atomic<bool> stopFlag{false};
    std::atomic<uint64_t> runCounter{0};
    std::atomic<bool> firstPrinted{false};
    std::mutex updateMx;

    const uint64_t K = checkpoints.size();

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (unsigned t=0; t<threads; ++t) {
        pool.emplace_back([&, t](){
            uint64_t localRuns = 0;
            while (!stopFlag.load(std::memory_order_relaxed)) {
                uint64_t seed = next_seed(baseSeed, runCounter);
                std::vector<uint32_t> U_at_cp;
                RunResult rr = run_once_segmented<Storage>(geom,
                                                           opt.density, opt.threshold, /*types*/2,
                                                           seed,
                                                           /*max_iters*/0, /*k*/opt.k,
                                                           opt.move,
                                                           &checkpoints, /*aggr*/nullptr, &U_at_cp);

                {
                    std::lock_guard<std::mutex> lk(updateMx);

                    for (size_t k=0; k<K; ++k) {
                        meanCurve.record(k, U_at_cp[k], rr.agents);
                    }

                    // First sample banner (stderr)
                    if (!firstPrinted.load(std::memory_order_relaxed)) {
                        bool expected = false;
                        if (firstPrinted.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
                            auto read_mean_at = [&](size_t idx)->double {
                                uint64_t cnt = meanCurve.count_runs[idx].load(std::memory_order_relaxed);
                                if (cnt == 0) return std::numeric_limits<double>::quiet_NaN();
                                uint64_t sum = meanCurve.sum_frac_scaled[idx].load(std::memory_order_relaxed);
                                return double(sum) / double(cnt) / double(CostAggregator::SCALE);
                            };
                            size_t idx0 = 0;
                            size_t idxMid = std::min<size_t>(K-1, K/4);
                            size_t idxEnd = K-1;
                            double mu0   = read_mean_at(idx0);
                            double muMid = read_mean_at(idxMid);
                            double muEnd = read_mean_at(idxEnd);

                            uint64_t n_now = meanCurve.count_runs[0].load(std::memory_order_relaxed);
                            double w = cs::halfwidth_anytime_hoeffding(n_now, K, opt.alpha, 1.0);

                            std::cerr << "[first sample]"
                                      << " move=" << (opt.move==MoveRule::Any ? "any" : "first")
                                      << "  n=" << n_now
                                      << "  2w=" << std::setprecision(6) << (2.0*w)
                                      << "  eps=" << std::setprecision(6) << opt.eps
                                      << "  alpha=" << std::setprecision(6) << opt.alpha
                                      << "  mean(U/N): t0=" << std::setprecision(4) << mu0
                                      << "  t25%=" << std::setprecision(4) << muMid
                                      << "  tEnd=" << std::setprecision(4) << muEnd
                                      << "  moves(last)=" << rr.moves
                                      << std::endl << std::flush;
                        }
                    }

                    ++localRuns;
                    if (opt.debug && (localRuns % opt.debug_every == 0)) {
                        auto read_mean_at = [&](size_t idx)->double {
                            uint64_t cnt = meanCurve.count_runs[idx].load(std::memory_order_relaxed);
                            if (cnt == 0) return std::numeric_limits<double>::quiet_NaN();
                            uint64_t sum = meanCurve.sum_frac_scaled[idx].load(std::memory_order_relaxed);
                            return double(sum) / double(cnt) / double(CostAggregator::SCALE);
                        };
                        size_t idx0 = 0;
                        size_t idxMid = std::min<size_t>(K-1, K/4);
                        size_t idxEnd = K-1;
                        double mu0   = read_mean_at(idx0);
                        double muMid = read_mean_at(idxMid);
                        double muEnd = read_mean_at(idxEnd);

                        uint64_t n_now = meanCurve.count_runs[0].load(std::memory_order_relaxed);
                        double w = cs::halfwidth_anytime_hoeffding(n_now, K, opt.alpha, 1.0);

                        std::cerr << "[run " << runCounter.load(std::memory_order_relaxed)
                                  << "] move=" << (opt.move==MoveRule::Any ? "any" : "first")
                                  << "  n=" << n_now
                                  << "  2w=" << std::setprecision(6) << (2.0*w)
                                  << "  eps=" << std::setprecision(6) << opt.eps
                                  << "  alpha=" << std::setprecision(6) << opt.alpha
                                  << "  mean(U/N): t0=" << std::setprecision(4) << mu0
                                  << "  t25%=" << std::setprecision(4) << muMid
                                  << "  tEnd=" << std::setprecision(4) << muEnd
                                  << "  moves(last)=" << rr.moves
                                  << std::endl << std::flush;
                    }

                    // Check anytime-CS stop
                    uint64_t n_now = meanCurve.count_runs[0].load(std::memory_order_relaxed);
                    if (cs::should_stop(n_now, K, opt.alpha, opt.eps, 1.0)) {
                        stopFlag.store(true, std::memory_order_relaxed);
                    }
                }

                if (stopFlag.load(std::memory_order_relaxed)) break;
            }
        });
    }
    for (auto& th : pool) th.join();
}

inline int run_app(const CLIOptions& opt) {
    // Base seed decided here so we can show it
    uint64_t baseSeed = (opt.seed == 0 ? make_random_seed() : opt.seed);

    // checkpoints (log-spaced), aggregator
    const uint64_t curveEnd   = 2000000ULL;
    const size_t   curvePts   = 96;
    std::vector<uint64_t> checkpoints = make_checkpoints_log(curveEnd, curvePts);
    CostAggregator meanCurve(checkpoints, CurveWeight::RUNS, /*pad_zeros*/true);

    // Banner (stderr)
    std::cerr << "CONFIG:"
              << " graph="   << (opt.graph==GraphKind::TORUS? "torus":"lollipop")
              << " size="    << opt.dimA << (opt.graph==GraphKind::TORUS? "x":":") << opt.dimB
              << " move="    << (opt.move==MoveRule::Any ? "any" : "first")
              << " density=" << std::fixed << std::setprecision(3) << opt.density
              << " threshold=" << std::setprecision(3) << opt.threshold
              << " alpha="   << std::setprecision(6) << opt.alpha
              << " eps="     << std::setprecision(6) << opt.eps
              << " k="       << (opt.move==MoveRule::Any ? 1u : opt.k)
              << " seed="    << baseSeed
              << " plot="    << (opt.plot_enabled? "on":"off")
              << " debug="   << (opt.debug? "on":"off")
              << std::endl << std::flush;

    // UI
    ProgressManager bar(meanCurve, checkpoints.size(), opt.alpha, opt.eps);
    bar.start();

    PlotManager plots(opt.plot_enabled,
                      checkpoints, meanCurve,
                      opt.eps, opt.alpha);
    plots.start();

    // Geometry dispatch
    if (opt.graph == GraphKind::TORUS) {
        Torus2D geom(opt.dimA, opt.dimB);
        run_workers_for_geometry_cs<Torus2D>(opt, geom, checkpoints, meanCurve);
    } else {
        LollipopGraph geom(opt.dimA, opt.dimB);
        run_workers_for_geometry_cs<LollipopGraph>(opt, geom, checkpoints, meanCurve);
    }

    plots.stop();
    bar.stop();

    std::cout << "Done. Stopped by anytime-CS rule."
              << "  ε=" << std::setprecision(6) << opt.eps
              << "  α=" << std::setprecision(6) << opt.alpha
              << ".\n";
    return 0;
}
