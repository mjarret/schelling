// Google Benchmark: run_jobs_hitting_time over LollipopGraph
#include <benchmark/benchmark.h>
#include <atomic>
#include <thread>

#include "graphs/lollipop.hpp"
#include "sim/sim.hpp"
#include "core/rng.hpp"
#include "core/schelling_threshold.hpp"
#include <tbb/global_control.h>
#include <string>


template <std::size_t CS, std::size_t PL>
static void BM_SchellingProcess(benchmark::State& state) {
    const double density = 0.9;
    core::schelling::init_program_threshold(1, 2); // tau = 1/2
    for (auto _ : state) {
        core::Xoshiro256ss rng(0xDEADBEEFCAFELL);
        graphs::LollipopGraph<CS, PL> g;
        auto steps = sim::run_schelling_process(g, density, rng);
        benchmark::DoNotOptimize(steps);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations());
}

// -----------------------------------------------------------------------------
// Automated size sweep registration
// -----------------------------------------------------------------------------
// Registers a family of benchmarks over a compile-time sweep of (CliqueSize, PathLength)
// using: CS(i) = CS0 + i*Step  for i in [0, Count)
//        PL(i) = (CS(i) * Num) / Den + Offset
// Defaults below reproduce a proportional sweep (PL ~= 9*CS) for i=0..8.
namespace {
template <std::size_t CS0, std::size_t Step, std::size_t Count,
          std::size_t Num, std::size_t Den, std::size_t Offset>
struct LollipopSweepRegistrar {
    template <std::size_t I>
    static void register_one_() {
        constexpr std::size_t cs = CS0 + I * Step;
        constexpr std::size_t pl = (cs * Num) / Den + Offset;
        const std::string name = std::string("Schelling/Lollipop/CS=") + std::to_string(cs)
                               + "/PL=" + std::to_string(pl);
        auto* b = ::benchmark::RegisterBenchmark(name.c_str(), &BM_SchellingProcess<cs, pl>);
        b->Iterations(1)
         ->Threads(tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism))
         ->Unit(benchmark::kMillisecond);
    }

    template <std::size_t... Is>
    static void register_all_(std::index_sequence<Is...>) {
        (register_one_<Is>(), ...);
    }

    LollipopSweepRegistrar() {
        register_all_(std::make_index_sequence<Count>{});
    }
};

// Sweep configuration: CS in {50,100,...,Step*Count}, PL ≈ 9*CS
static LollipopSweepRegistrar</*CS0*/50000, /*Step*/50, /*Count*/50,
                               /*Num*/20, /*Den*/1, /*Offset*/0> g_lolli_sweep;
} // namespace

// // -----------------------------------------------------------------------------
// // Clique-only scaling: vary CS, hold PL fixed
// // -----------------------------------------------------------------------------
// namespace {
// template <std::size_t CS0, std::size_t Step, std::size_t Count, std::size_t PLFixed>
// struct LollipopCliqueOnlyRegistrar {
//     template <std::size_t I>
//     static void register_one_() {
//         constexpr std::size_t cs = CS0 + I * Step;
//         constexpr std::size_t pl = PLFixed;
//         const std::string name = std::string("Schelling/LollipopCliqueOnly/CS=") + std::to_string(cs)
//                                + "/PL=" + std::to_string(pl);
//         auto* b = ::benchmark::RegisterBenchmark(name.c_str(), &BM_SchellingProcess<cs, pl>);
//         b->Iterations(100)
//          ->Threads(tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism))
//          ->Unit(benchmark::kMillisecond);
//     }

//     template <std::size_t... Is>
//     static void register_all_(std::index_sequence<Is...>) {
//         (register_one_<Is>(), ...);
//     }

//     LollipopCliqueOnlyRegistrar() {
//         register_all_(std::make_index_sequence<Count>{});
//     }
// };

// // Sweep configuration: CS in {50,100,...}, PL fixed at 450
// static LollipopCliqueOnlyRegistrar</*CS0*/50, /*Step*/50, /*Count*/50, /*PLFixed*/10000> g_lolli_clique_only_sweep;
// } // namespace


BENCHMARK_MAIN();
