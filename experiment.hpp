// experiment.hpp — run_once_segmented<Storage, Geometry> with optional per-run curve capture.
#pragma once
#include <cstdint>
#include <vector>
#include <cmath>
#include <chrono>
#include <limits>

#include "metrics.hpp"
#include "random_fill.hpp"
#include "sim_random.hpp"
#include "rng.hpp"
#include "cost_aggregator.hpp"
#include "move_rule.hpp"

struct RunResult {
    uint32_t agents=0, types=0;
    double density=0.0, threshold=0.0;
    uint64_t seed=0, max_iters=0;
    uint32_t k_candidates=0;
    bool ignore_satisfaction=true;
    uint64_t moves=0;
    bool converged=false;
    double elapsed=0.0;
    uint32_t init_unhappy=0, final_unhappy=0;
    double init_idx=0.0, init_avg=0.0, final_idx=0.0, final_avg=0.0;
};

// NOTE: move_rule controls exactly two behaviors:
//   MoveRule::Any           → just move to a random empty site (k forced to 1; no checks)
//   MoveRule::FirstAccepting→ sample up to k empty sites; move to the first that meets the threshold; else stay
template<class Storage, class Geometry>
RunResult run_once_segmented(Geometry& geom,
                             double density, double threshold,
                             uint32_t n_types, uint64_t seed,
                             uint64_t max_iters, uint32_t k_candidates,
                             MoveRule move_rule,
                             const std::vector<uint64_t>* cps,
                             CostAggregator* curve_aggr,
                             std::vector<uint32_t>* out_U_by_cp = nullptr)
{
    RunResult R{};
    R.density=density; R.threshold=threshold; R.types=n_types; R.seed=seed; R.max_iters=max_iters; R.k_candidates=k_candidates;
    R.ignore_satisfaction = (move_rule == MoveRule::Any);

    const uint32_t N = geom.N();
    uint32_t n_agents = static_cast<uint32_t>(std::llround(density * double(N)));
    if (n_agents > N) n_agents = N;
    R.agents = n_agents;

    Storage world(N);
    SplitMix64 rng(seed);
    random_fill(world, n_agents, n_types, rng);

    Metrics<Storage, Geometry> meas(world, geom);
    Threshold th(threshold);
    auto [u0, avg0] = meas.unhappy_and_avg_same_fraction(th);
    R.init_unhappy = u0;
    R.init_idx     = (n_agents ? double(u0)/double(n_agents) : 0.0);
    R.init_avg     = avg0;

    RandomStepper<Storage, Geometry> stepper(world, geom, n_agents);
    stepper.initialize(th);

    ProgressOptionsRandom prog;
    prog.enabled = false;
    prog.report_every_moves = std::numeric_limits<uint64_t>::max();
    prog.report_every_ms    = 0;

    RandomMoveOptions ropt;
    // Enforce the two-option policy:
    if (move_rule == MoveRule::Any) {
        ropt.k_candidates        = 1;     // exactly one draw; just move
        ropt.ignore_satisfaction = true;  // no threshold checks
        ropt.fallback_best       = false; // irrelevant
    } else { // FirstAccepting
        ropt.k_candidates        = (k_candidates == 0 ? 1u : k_candidates);
        ropt.ignore_satisfaction = false; // check threshold
        ropt.fallback_best       = false; // DO NOT move to “best” if none pass — stay put
    }

    const bool capture_curve = (out_U_by_cp != nullptr);
    auto t_start = std::chrono::high_resolution_clock::now();

    if (!cps) {
        SimResult res = stepper.run(th, rng, max_iters, &prog, &ropt);
        R.moves         = res.moves;
        R.converged     = res.converged;
        R.final_unhappy = res.final_unhappy;
        R.final_idx     = (n_agents ? double(R.final_unhappy)/double(n_agents) : 0.0);
        R.final_avg     = res.final_avg_same;
    } else {
        const std::vector<uint64_t>& cp = *cps;
        size_t k = 0;
        uint64_t total_moves = 0;
        bool converged = false;
        uint32_t last_u = u0;
        double   last_avg_same = avg0;

        if (capture_curve) {
            out_U_by_cp->clear();
            out_U_by_cp->reserve(cp.size());
            out_U_by_cp->push_back(u0);
        } else if (curve_aggr) {
            curve_aggr->record(0, u0, n_agents);
        }
        ++k;

        while (k < cp.size() && !converged && (max_iters==0 || total_moves < max_iters)) {
            uint64_t target = cp[k];
            if (target <= total_moves) {
                if (capture_curve) out_U_by_cp->push_back(last_u);
                else if (curve_aggr) curve_aggr->record(k, last_u, n_agents);
                ++k;
                continue;
            }
            uint64_t seg_moves = (max_iters==0 ? (target - total_moves) : std::min<uint64_t>(target - total_moves, max_iters - total_moves));
            SimResult res = stepper.run(th, rng, seg_moves, &prog, &ropt);
            total_moves += res.moves;
            converged = res.converged;
            last_u = res.final_unhappy;
            last_avg_same = res.final_avg_same;

            if (capture_curve) out_U_by_cp->push_back(last_u);
            else if (curve_aggr) curve_aggr->record(k, last_u, n_agents);
            ++k;
            if (converged) break;
        }
        if (converged) {
            if (capture_curve) {
                while (k++ < cp.size()) out_U_by_cp->push_back(0u);
            } else if (curve_aggr) {
                curve_aggr->pad_zeros_from(k, n_agents);
            }
        } else {
            if (capture_curve) {
                while (k < cp.size()) { out_U_by_cp->push_back(last_u); ++k; }
            }
        }

        R.moves         = total_moves;
        R.converged     = converged;
        R.final_unhappy = last_u;
        R.final_idx     = (n_agents ? double(R.final_unhappy)/double(n_agents) : 0.0);
        R.final_avg     = last_avg_same;
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    R.elapsed = std::chrono::duration<double>(t_end - t_start).count();
    return R;
}
