#pragma once
#include <cstdint>
#include <vector>
#include <limits>
#include <tuple>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include "geometry.hpp"
#include "metrics.hpp"   // Threshold
#include "rng.hpp"

// -------- Fast, event-driven Schelling stepper --------------------------------
// Maintains:
//   - counts[type*N + v]  : #neighbors of 'type' around vertex v (uint8_t)
//   - total[v]            : total occupied neighbors around v (uint8_t)
//   - vacancy list + O(1) membership map
//   - unhappy list + O(1) membership map
// Each move updates only O(deg) vertices (deg=8 on Moore grid).

struct ProgressOptionsFast {
    bool     enabled             = true;
    uint64_t report_every_moves  = 10000; // print every N moves (0 = disabled)
    uint64_t report_every_ms     = 1000;  // or every T milliseconds (0 = disabled)
    bool     print_header_once   = true;  // print a single header line
};

struct FastOptions {
    uint32_t k_candidates     = 32;   // sample K random vacancies per move
    bool     fallback_best    = true; // if none satisfies, move to the candidate with max "same" (else skip)
};

struct SimResult {
    uint64_t moves = 0;
    bool     converged = false;
    uint32_t final_unhappy = 0;
    double   final_avg_same = 0.0;
    double   elapsed_sec = 0.0;
};

// Small utility: swap-pop removal for indexable lists with position map
template <class T>
inline void list_remove_at(std::vector<T>& a, uint32_t idx) {
    const uint32_t last = static_cast<uint32_t>(a.size() - 1);
    if (idx != last) std::swap(a[idx], a[last]);
    a.pop_back();
}

// -------------------- FastStepper --------------------------------------------
template <class World, class Geometry>
class FastStepper {
public:
    FastStepper(World& w, const Geometry& g, uint32_t n_types)
        : W_(w), G_(g), n_types_(n_types), N_(g.N())
        , counts_(size_t(n_types) * size_t(N_), 0), total_(N_, 0)
        , vac_pos_(N_, kNPOS), unhap_pos_(N_, kNPOS)
    {}

    // Build counts, vacancy list, unhappy list from current world
    void initialize(const Threshold& th) {
        // Fill vacancy list
        vacancy_.clear();
        for (uint32_t v = 0; v < N_; ++v) {
            if (!W_.occupied(v)) {
                vac_pos_[v] = static_cast<uint32_t>(vacancy_.size());
                vacancy_.push_back(v);
            }
        }

        // Build neighbor counts by scanning occupied vertices once
        // For each occupied v of type t, increment counts[t][u] for all neighbors u of v.
        W_.for_each_agent([&](uint32_t v, uint32_t t){
            G_.for_each_neighbor(v, [&](uint32_t u){
                ++counts_[idx(t, u)];
                ++total_[u];
            });
        });

        // Build unhappy list once using the counts
        unhappy_.clear();
        for (uint32_t v = 0; v < N_; ++v) {
            if (!W_.occupied(v)) continue;
            const uint32_t t = W_.type_of(v);
            const uint32_t same  = counts_[idx(t, v)];
            const uint32_t other = total_[v] - same;
            if (!th.satisfied(same, other)) add_unhappy(v);
        }
    }

    // Run asynchronous dynamics with O(1) agent sampling and O(deg) updates per move
    template <class RNG>
    SimResult run_until_equilibrium(const Threshold& th, RNG& rng,
                                    uint64_t max_moves,
                                    const ProgressOptionsFast* prog = nullptr,
                                    const FastOptions* fopt_in = nullptr)
    {
        const FastOptions fopt = fopt_in ? *fopt_in : FastOptions{};
        using clock = std::chrono::steady_clock;
        const auto t0 = clock::now();
        auto last_report = t0;
        bool printed_header = false;

        SimResult res{};

        for (uint64_t step = 0; step < max_moves; ++step) {
            // Progress
            if (prog && prog->enabled) {
                const bool due_moves = (prog->report_every_moves > 0) && (step % prog->report_every_moves == 0);
                bool due_time = false;
                if (prog->report_every_ms > 0) {
                    auto now = clock::now();
                    due_time = (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_report).count()
                               >= static_cast<long long>(prog->report_every_ms));
                    if (due_time) last_report = now;
                }
                if ((due_moves || due_time)) {
                    const auto now = clock::now();
                    const double elapsed = std::chrono::duration<double>(now - t0).count();
                    const double mps = (elapsed > 0.0) ? double(step) / elapsed : 0.0;
                    const double unhappy_frac = (num_agents_cached_ ? double(unhappy_.size())/double(num_agents_cached_) : 0.0);
                    const double eta_budget = (mps > 0.0) ? double(max_moves - step) / mps : std::numeric_limits<double>::infinity();

                    if (prog->print_header_once && !printed_header) {
                        std::cerr << "moves\t%budget\tunhappy\tunhappy%\tmoves/s\tETA(budget)s\n";
                        printed_header = true;
                    }
                    std::cerr << step << '\t'
                              << std::setprecision(3) << std::fixed << (100.0 * (double(step) / double(max_moves))) << '\t'
                              << unhappy_.size() << '\t'
                              << std::setprecision(3) << (100.0 * unhappy_frac) << '\t'
                              << std::setprecision(1) << mps << '\t'
                              << std::setprecision(1) << eta_budget << '\n';
                }
            }

            if (unhappy_.empty()) {
                const auto t1 = clock::now();
                res.moves = step;
                res.converged = true;
                res.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
                finalize_metrics(th, res);
                return res;
            }

            if (vacancy_.empty()) {
                // No place to move; we cannot progress further
                const auto t1 = clock::now();
                res.moves = step;
                res.converged = false;
                res.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
                finalize_metrics(th, res);
                return res;
            }

            // Pick an unhappy agent in O(1)
            const uint32_t pick = static_cast<uint32_t>(rng.uniform_u64(unhappy_.size()));
            const uint32_t src  = unhappy_[pick];
            const uint32_t t    = W_.type_of(src);

            // Choose a destination by sampling K random vacancies
            uint32_t dst = UINT32_MAX;
            uint32_t best_dst = UINT32_MAX;
            uint32_t best_same = 0;
            const uint32_t K = std::min<uint32_t>(fopt.k_candidates, static_cast<uint32_t>(vacancy_.size()));
            for (uint32_t k = 0; k < K; ++k) {
                const uint32_t vi = static_cast<uint32_t>(rng.uniform_u64(vacancy_.size()));
                const uint32_t cand = vacancy_[vi];
                const uint32_t same_c  = counts_[idx(t, cand)];
                const uint32_t other_c = total_[cand] - same_c;
                if (fopt.fallback_best && same_c > best_same) { best_same = same_c; best_dst = cand; }
                if (th.satisfied(same_c, other_c)) { dst = cand; break; }
            }
            if (dst == UINT32_MAX) {
                if (fopt.fallback_best && best_dst != UINT32_MAX) {
                    // Optional: move to "best" even if not satisfied (greedy hill-climbing)
                    dst = best_dst;
                } else {
                    // Skip this agent this step; try another next step
                    continue;
                }
            }

            // Remove 'src' from unhappy (it will be re-evaluated at dst)
            remove_unhappy(src);

            // Update neighbor counts around src (agent of type t leaves)
            G_.for_each_neighbor(src, [&](uint32_t u){
                --counts_[idx(t, u)];
                --total_[u];
                // If u is occupied, its satisfaction may have changed
                if (W_.occupied(u)) update_unhappy(u, th);
            });

            // Move in the world state
            W_.move(src, dst);

            // Maintain vacancy list: add src, remove dst
            vacancy_add(src);
            vacancy_remove(dst);

            // Update neighbor counts around dst (agent of type t arrives)
            G_.for_each_neighbor(dst, [&](uint32_t u){
                ++counts_[idx(t, u)];
                ++total_[u];
                if (W_.occupied(u)) update_unhappy(u, th);
            });

            // Re-evaluate moved agent at its new location
            update_unhappy(dst, th);

            // Step finished; continue
            if (step + 1 == max_moves) {
                const auto t1 = std::chrono::steady_clock::now();
                res.moves = max_moves;
                res.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
                finalize_metrics(th, res);
                return res;
            }
        }
        return res; // not reached
    }

    // Set optional cached #agents (for progress)
    void set_num_agents_cached(uint32_t num_agents) { num_agents_cached_ = num_agents; }

private:
    World&          W_;
    const Geometry& G_;
    uint32_t        n_types_;
    uint32_t        N_;

    // neighbor counts: counts_[type*N + v]
    std::vector<uint8_t> counts_;
    std::vector<uint8_t> total_;

    // vacancy management
    static constexpr uint32_t kNPOS = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> vacancy_;
    std::vector<uint32_t> vac_pos_;   // vac_pos[v] = index into vacancy_ or kNPOS

    // unhappy management (occupied vertices only)
    std::vector<uint32_t> unhappy_;
    std::vector<uint32_t> unhap_pos_; // unhap_pos[v] = index into unhappy_ or kNPOS

    uint32_t num_agents_cached_ = 0;

    inline size_t idx(uint32_t type, uint32_t v) const {
        return size_t(type) * size_t(N_) + size_t(v);
    }

    inline void vacancy_add(uint32_t v) {
        if (vac_pos_[v] != kNPOS) return;
        vac_pos_[v] = static_cast<uint32_t>(vacancy_.size());
        vacancy_.push_back(v);
    }
    inline void vacancy_remove(uint32_t v) {
        const uint32_t p = vac_pos_[v];
        if (p == kNPOS) return; // not in list
        const uint32_t last = static_cast<uint32_t>(vacancy_.size() - 1);
        if (p != last) {
            const uint32_t moved_v = vacancy_[last];
            vacancy_[p] = moved_v;
            vac_pos_[moved_v] = p;
        }
        vacancy_.pop_back();
        vac_pos_[v] = kNPOS;
    }

    inline void add_unhappy(uint32_t v) {
        if (unhap_pos_[v] != kNPOS) return;
        unhap_pos_[v] = static_cast<uint32_t>(unhappy_.size());
        unhappy_.push_back(v);
    }
    inline void remove_unhappy(uint32_t v) {
        const uint32_t p = unhap_pos_[v];
        if (p == kNPOS) return;
        const uint32_t last = static_cast<uint32_t>(unhappy_.size() - 1);
        if (p != last) {
            const uint32_t moved_v = unhappy_[last];
            unhappy_[p] = moved_v;
            unhap_pos_[moved_v] = p;
        }
        unhappy_.pop_back();
        unhap_pos_[v] = kNPOS;
    }

    // Recheck v and add/remove from unhappy set
    inline void update_unhappy(uint32_t v, const Threshold& th) {
        if (!W_.occupied(v)) { remove_unhappy(v); return; }
        const uint32_t t = W_.type_of(v);
        const uint32_t same  = counts_[idx(t, v)];
        const uint32_t other = total_[v] - same;
        const bool sat = th.satisfied(same, other);
        if (sat) remove_unhappy(v); else add_unhappy(v);
    }

    // Final metrics (one pass using Metrics)
    inline void finalize_metrics(const Threshold& th, SimResult& res) const {
        Metrics<World, Geometry> meas(W_, G_);
        auto [u, avg] = meas.unhappy_and_avg_same_fraction(th);
        res.final_unhappy = u;
        res.final_avg_same = avg;
    }
};
