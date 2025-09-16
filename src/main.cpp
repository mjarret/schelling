#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <chrono>
#include <vector>

#include "graphs/lollipop_graph.hpp"
#include "graphs/path_graph.hpp"
#include "graphs/clique_graph.hpp"
#include "core/rng.hpp"
#include "core/random_transition.hpp"

#ifndef K
#define K 2
#endif

#ifndef N
#define N 64
#endif

/**
 * @brief Print CLI usage information.
 * @param prog Program name.
 */
static void print_help(const char* prog) {
  std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --help                 Show this help\n"
              << "  --seed <u64>           RNG seed\n"
              << "  --steps <u64>          Max steps (default 100000)\n"
              << "  --stats-every <u64>    Snapshot interval (default 1000)\n"
              << "  --quiet                Suppress summary output\n"
              << "  --n <n>                Number of vertices (default compile-time)\n"
              << "  --clique <m>           Lollipop clique size (overrides --n split)\n"
              << "  --tail <t>             Lollipop path length (overrides --n split)\n"
              << "  --tau <f64>            Satisfaction threshold in [0,1] (default 0.5)\n"
              << "  --bench                Benchmark: many runs, report avg ms/run\n"
              << "  --bench-sec <f64>      Target total seconds (default 3.0)\n"
              << "  --bench-min <u64>      Minimum runs (default 100)\n";
}

namespace {

struct BenchMetrics {
    double init_ms = 0.0;
    double should_ms = 0.0;
    double pick_empty_ms = 0.0;
    double update_ms = 0.0;
    double apply_ms = 0.0;
    double proposal_ms = 0.0;
    std::uint64_t steps = 0;
};

template<class Graph>
/**
 * @brief Check if vertex v is unhappy at tolerance tau.
 * @param g Graph reference.
 * @param v Vertex index.
 * @param tau Tolerance in [0,1].
 * @return True if unhappy.
 */
inline bool is_unhappy(const Graph& g, index_t v, double tau) {
    const std::uint32_t c = g.get_color(v);
    if (c == 0u) return false;
    const std::uint32_t neigh = g.occupied_neighbor_count(v);
    const auto lf = g.local_frustration(v);
    return lf > tau * neigh;
}

template<class Graph, class Rng>
/**
 * @brief Initialize a lollipop graph with an occupancy ratio.
 * @param g Graph instance.
 * @param n_clique Size of clique portion.
 * @param n_path Length of path portion.
 * @param rng Random generator.
 * @param occ_ratio Target occupancy ratio in [0,1].
 */
inline void init_lollipop(Graph& g, std::size_t n_clique, std::size_t n_path, Rng& rng, double occ_ratio = 0.80) {
    const index_t Ntot = g.size();
    const index_t bridge = (n_clique ? (n_clique - 1) : 0);
    const index_t target_fill = Ntot * occ_ratio;
    std::uint32_t next_color = 1u;
    index_t filled = 0;
    while (filled < target_fill) {
        index_t v = rng.uniform_index(Ntot);
        if (v < bridge) {
            std::uint32_t occ_core = 0; for (index_t i = 0; i < bridge; ++i) occ_core += (g.get_color(i) != 0u);
            const double occ_r = bridge ? (occ_core * (1.0 / bridge)) : 1.0;
            if (rng.uniform01() < occ_r) continue;
        }
        if (g.get_color(v) != 0u) continue;
        g.change_color(v, next_color, 0u);
        next_color = (next_color == 1u ? 2u : 1u);
        ++filled;
    }
}

template<class Graph>
/**
 * @brief Collect vertices whose happiness can change due to a move.
 * @param from Source vertex.
 * @param to Destination vertex.
 * @param n_clique Clique size.
 * @param n_path Path length.
 * @param out Output vector of candidate indices.
 */
inline void collect_path_bridge_candidates(index_t from, index_t to, std::size_t n_clique, std::size_t n_path, std::vector<index_t>& out) {
    const index_t bridge = (n_clique ? (n_clique - 1) : 0);
    auto push = [&](index_t v){ for (auto x: out) if (x==v) return; out.push_back(v); };
    auto add_path = [&](index_t gv){ index_t idx = gv - n_clique; push(gv); if (idx > 0) push(gv - 1); if (idx + 1 < n_path) push(gv + 1); if (idx == 0) push(bridge); };
    if (from == bridge) { push(bridge); if (n_path>0) push(n_clique); }
    else if (from >= n_clique) { add_path(from); }
    if (to == bridge) { push(bridge); if (n_path>0) push(n_clique); }
    else if (to >= n_clique) { add_path(to); }
}

template<class Graph>
/**
 * @brief Apply a move and update unhappy count using localized checks.
 * @param g Graph instance.
 * @param from Source vertex.
 * @param to Destination vertex.
 * @param cf Color moved.
 * @param n_clique Clique size.
 * @param n_path Path length.
 * @param tau Tolerance parameter.
 * @param unhappy In/out unhappy count.
 * @param bm Optional benchmark metrics.
 */
inline void apply_move_and_update_unhappy(Graph& g,
                                          index_t from, index_t to, std::uint32_t cf,
                                          std::size_t n_clique, std::size_t n_path,
                                          double tau, std::int32_t& unhappy,
                                          BenchMetrics* bm = nullptr) {
    const index_t bridge = (n_clique ? (n_clique - 1) : 0);
    std::vector<index_t> cand; cand.reserve(8);
    collect_path_bridge_candidates<Graph>(from, to, n_clique, n_path, cand);
    std::vector<char> before(cand.size());
    for (std::size_t i=0;i<cand.size();++i) before[i] = is_unhappy(g, cand[i], tau);
    const bool touches_core = (from < bridge) || (to < bridge) || (from == bridge) || (to == bridge);
    std::uint32_t core_before = 0, core_after = 0;
    if (touches_core) {
        for (index_t v = 0; v < bridge; ++v) core_before += is_unhappy(g, v, tau);
    }
    const auto ap0 = std::chrono::high_resolution_clock::now();
    g.change_color(from, 0u, cf);
    g.change_color(to, cf, 0u);
    const auto ap1 = std::chrono::high_resolution_clock::now();
    if (bm) bm->apply_ms += std::chrono::duration<double, std::milli>(ap1 - ap0).count();
    const auto upd0 = std::chrono::high_resolution_clock::now();
    if (touches_core) {
        for (index_t v = 0; v < bridge; ++v) core_after += is_unhappy(g, v, tau);
        unhappy += static_cast<std::int32_t>(core_after) - static_cast<std::int32_t>(core_before);
    }
    for (std::size_t i=0;i<cand.size();++i) {
        const bool after = is_unhappy(g, cand[i], tau);
        unhappy += int(after) - int(before[i]);
    }
    const auto upd1 = std::chrono::high_resolution_clock::now();
    if (bm) bm->update_ms += std::chrono::duration<double, std::milli>(upd1 - upd0).count();
}

template<class Graph, class Rng>
/**
 * @brief Run a single experiment using tau-guided moves.
 * @param n_clique Clique size.
 * @param n_path Path length.
 * @param tau Tolerance parameter.
 * @param rng Random generator.
 * @param quiet Suppress periodic output.
 * @param stats_every Interval for status prints.
 * @param bm Optional benchmark metrics.
 * @return Number of unhappy agents at termination.
 */
inline std::uint32_t run_once(std::size_t n_clique, std::size_t n_path, double tau, Rng& rng, bool quiet, std::uint64_t stats_every, BenchMetrics* bm = nullptr) {
    Graph g(n_clique, n_path);
    const index_t Ntot = g.size();
    const auto i0 = std::chrono::high_resolution_clock::now();
    init_lollipop(g, n_clique, n_path, rng, 0.80);
    const auto i1 = std::chrono::high_resolution_clock::now();
    if (bm) bm->init_ms += std::chrono::duration<double, std::milli>(i1 - i0).count();
    auto count_unhappy = [&](){ std::uint32_t c=0; for(index_t v=0; v<Ntot; ++v) c += is_unhappy(g, v, tau); return c; };
    std::int32_t unhappy = count_unhappy();
    const auto t_start = std::chrono::high_resolution_clock::now();
    const auto time_limit = std::chrono::seconds(10);
    std::uint64_t frustrated = g.total_frustration();
    for (std::uint64_t step=1; ; ++step) {
        if (bm) bm->steps++;
        index_t from = rng.uniform_index(Ntot);
        std::uint32_t cf = g.get_color(from);
        if (cf != 0u) {
            const auto sh0 = std::chrono::high_resolution_clock::now();
            const bool move_now = is_unhappy(g, from, tau);
            const auto sh1 = std::chrono::high_resolution_clock::now();
            if (bm) bm->should_ms += std::chrono::duration<double, std::milli>(sh1 - sh0).count();
            if (move_now) {
                index_t to = from;
                const auto pe0 = std::chrono::high_resolution_clock::now();
                for (int t=0; t<16 && to==from; ++t) { index_t cand = rng.uniform_index(Ntot); if (g.get_color(cand) == 0u) to = cand; }
                const auto pe1 = std::chrono::high_resolution_clock::now();
                if (bm) bm->pick_empty_ms += std::chrono::duration<double, std::milli>(pe1 - pe0).count();
                if (to != from) {
                    apply_move_and_update_unhappy(g, from, to, cf, n_clique, n_path, tau, unhappy, bm);
                    frustrated = g.total_frustration();
                }
            }
        }
        if (unhappy == 0) break;
        if (!quiet && (step % stats_every == 0)) std::cout << "step=" << step << " frustration=" << frustrated << "\n";
        if (std::chrono::high_resolution_clock::now() - t_start >= time_limit) break;
    }
    if (!quiet) std::cout << "final_unhappy=" << (unhappy < 0 ? 0 : unhappy) << "\n";
    return (unhappy < 0 ? 0u : std::uint32_t(unhappy));
}

template<class Graph, class Rng>
/**
 * @brief Run using random proposer independent of tau.
 * @param n_clique Clique size.
 * @param n_path Path length.
 * @param tau Tolerance parameter (used for termination metrics only).
 * @param rng Random generator.
 * @param quiet Suppress periodic output.
 * @param stats_every Interval for status prints.
 * @param bm Optional benchmark metrics.
 * @return Number of unhappy agents at termination.
 */
inline std::uint32_t run_once_random(std::size_t n_clique, std::size_t n_path, double tau, Rng& rng, bool quiet, std::uint64_t stats_every, BenchMetrics* bm = nullptr) {
    Graph g(n_clique, n_path);
    const index_t Ntot = g.size();
    const auto i0 = std::chrono::high_resolution_clock::now();
    init_lollipop(g, n_clique, n_path, rng, 0.80);
    const auto i1 = std::chrono::high_resolution_clock::now();
    if (bm) bm->init_ms += std::chrono::duration<double, std::milli>(i1 - i0).count();

    auto count_unhappy = [&](){ std::uint32_t c=0; for(index_t v=0; v<Ntot; ++v) c += is_unhappy(g, v, tau); return c; };
    std::int32_t unhappy = count_unhappy();
    const auto t_start = std::chrono::high_resolution_clock::now();
    const auto time_limit = std::chrono::seconds(10);
    std::uint64_t frustrated = g.total_frustration();

    for (std::uint64_t step=1; ; ++step) {
        if (bm) bm->steps++;
        const auto pr0 = std::chrono::high_resolution_clock::now();
        auto [from, to] = core::random_transition(g, rng);
        const auto pr1 = std::chrono::high_resolution_clock::now();
        if (bm) bm->proposal_ms += std::chrono::duration<double, std::milli>(pr1 - pr0).count();

        if (from != to) {
            const std::uint32_t cf = g.get_color(from);
            if (cf != 0u && g.get_color(to) == 0u) {
                apply_move_and_update_unhappy(g, from, to, cf, n_clique, n_path, tau, unhappy, bm);
                frustrated = g.total_frustration();
            }
        }
        if (unhappy == 0) break;
        if (!quiet && (step % stats_every == 0)) std::cout << "step=" << step << " frustration=" << frustrated << "\n";
        if (std::chrono::high_resolution_clock::now() - t_start >= time_limit) break;
    }
    if (!quiet) std::cout << "final_unhappy=" << (unhappy < 0 ? 0 : unhappy) << "\n";
    return (unhappy < 0 ? 0u : std::uint32_t(unhappy));
}

}

/**
 * @brief Program entry point.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status code.
 */
int main(int argc, char** argv) {
    using Graph = graphs::LollipopGraph<K>;

    uint64_t seed = 0; bool seed_set = false;
    uint64_t max_steps = 100000; uint64_t stats_every = 1000; bool quiet = false;
    std::size_t n = N;
    std::size_t m_override = 0, t_override = 0; bool have_mt = false;
    double tau = 0.5;
    bool bench = false; double bench_sec = 3.0; std::uint64_t bench_min = 100;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") { print_help(argv[0]); return 0; }
        else if (arg == "--seed" && i + 1 < argc) { seed = std::strtoull(argv[++i], nullptr, 10); seed_set = true; }
        else if (arg == "--steps" && i + 1 < argc) { max_steps = std::strtoull(argv[++i], nullptr, 10); }
        else if (arg == "--n" && i + 1 < argc) { n = std::strtoull(argv[++i], nullptr, 10); }
        else if (arg == "--clique" && i + 1 < argc) { m_override = std::strtoull(argv[++i], nullptr, 10); have_mt = true; }
        else if (arg == "--tail" && i + 1 < argc) { t_override = std::strtoull(argv[++i], nullptr, 10); have_mt = true; }
        else if (arg == "--tau" && i + 1 < argc) { tau = std::stod(argv[++i]); }
        else if (arg == "--bench") { bench = true; quiet = true; }
        else if (arg == "--bench-sec" && i + 1 < argc) { bench_sec = std::stod(argv[++i]); }
        else if (arg == "--bench-min" && i + 1 < argc) { bench_min = std::strtoull(argv[++i], nullptr, 10); }
        else if (arg == "--epsilon" && i + 1 < argc) { }
        else { std::cerr << "Unknown or incomplete option: " << arg << "\n"; print_help(argv[0]); return 2; }
    }

    if (!seed_set) seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    std::size_t n_clique = n / 2;
    std::size_t n_path   = n - n_clique;
    if (!bench) {
        core::Xoshiro256ss rng(seed_set ? seed : std::chrono::high_resolution_clock::now().time_since_epoch().count());
        (void)run_once<Graph, core::Xoshiro256ss>(n_clique, n_path, tau, rng, quiet, stats_every);
        return 0;
    } else {
        const auto t0 = std::chrono::high_resolution_clock::now();
        std::uint64_t runs = 0; double total_ms = 0.0; BenchMetrics agg{};
        uint64_t base_seed = seed_set ? seed : t0.time_since_epoch().count();
        do {
            core::Xoshiro256ss rng2(base_seed + runs);
            const auto r0 = std::chrono::high_resolution_clock::now();
            (void)run_once_random<Graph, core::Xoshiro256ss>(n_clique, n_path, tau, rng2, true, stats_every, &agg);
            const auto r1 = std::chrono::high_resolution_clock::now();
            total_ms += std::chrono::duration<double, std::milli>(r1 - r0).count();
            ++runs;
        } while (runs < bench_min || (std::chrono::high_resolution_clock::now() - t0) < std::chrono::duration<double>(bench_sec));
        const double avg = total_ms / runs;
        std::cout << "bench_runs=" << runs
                  << " avg_ms_per_run=" << avg
                  << " init_ms/run=" << (agg.init_ms / runs)
                  << " should_ms/run=" << (agg.should_ms / runs)
                  << " pick_empty_ms/run=" << (agg.pick_empty_ms / runs)
                  << " update_ms/run=" << (agg.update_ms / runs)
                  << " apply_ms/run=" << (agg.apply_ms / runs)
                  << " proposal_ms/run=" << (agg.proposal_ms / runs)
                  << " steps/run=" << (double(agg.steps) / runs)
                  << "\n";
        return 0;
    }
}
