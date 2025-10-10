// Google Benchmark: Schelling process runtime on LollipopGraph
#include <benchmark/benchmark.h>

#include "graphs/lollipop.hpp"
#include "sim/sim.hpp"
#include "core/rng.hpp"
#include "core/schelling_threshold.hpp"

template <std::size_t CS, std::size_t PL>
static void BM_Schelling_Lollipop(benchmark::State& state) {
    core::schelling::init_program_threshold(1, 2);
    for (auto _ : state) {
        core::Xoshiro256ss rng(0xDEADBEEFCAFELL);
        graphs::LollipopGraph<CS, PL> g;
        auto history = sim::run_schelling_process(g, 0.8, rng);
        benchmark::DoNotOptimize(history.size());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_Schelling_Lollipop, 50, 450)->UseRealTime()->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_Schelling_Lollipop, 13, 87)->UseRealTime()->Unit(benchmark::kMillisecond);

template <std::size_t CS, std::size_t PL>
static void BM_Schelling_Lollipop_Batch(benchmark::State& state) {
    const std::size_t batch = static_cast<std::size_t>(state.range(0));
    core::schelling::init_program_threshold(1, 2);
    for (auto _ : state) {
        core::Xoshiro256ss rng(0xBEEFBABEULL);
        for (std::size_t i = 0; i < batch; ++i) {
            graphs::LollipopGraph<CS, PL> g;
            auto history = sim::run_schelling_process(g, 0.8, rng);
            benchmark::DoNotOptimize(history.size());
        }
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * batch);
}

BENCHMARK_TEMPLATE(BM_Schelling_Lollipop_Batch, 50, 450)
    ->ArgName("processes")
    ->Arg(1000000)
    ->UseRealTime()
    ->Unit(benchmark::kSecond);

BENCHMARK_MAIN();
