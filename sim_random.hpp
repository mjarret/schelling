#pragma once
#include <cstdint>
#include <vector>
#include <limits>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <algorithm>

#include "geometry.hpp"  // Torus2D (or your geometry policy)
#include "metrics.hpp"   // Threshold (for satisfied test, final stats)
#include "rng.hpp"       // SplitMix64 (uniform_u64)

struct ProgressOptionsRandom {
    bool     enabled             = true;
    uint64_t report_every_moves  = 10000; // print every N iterations (0 = disabled)
    uint64_t report_every_ms     = 1000;  // or time-gated (0 = disabled)
    bool     print_header_once   = true;
};

// Move policy:
//   - If ignore_satisfaction = true (default): move to a uniformly random vacancy.
//   - If ignore_satisfaction = false: you can optionally try K random vacancies and (if desired)
//     fall back to "best". That path kept for experimentation; default is to ignore satisfaction.
struct RandomMoveOptions {
    uint32_t k_candidates        = 32;
    bool     fallback_best       = true;
    bool     ignore_satisfaction = true; // <— default: destination satisfaction is ignored
};

#ifndef SCHELLING_SIM_RESULT_DEF
#define SCHELLING_SIM_RESULT_DEF
struct SimResult {
    uint64_t moves = 0;          // counts only successful moves
    bool     converged = false;  // true iff we ended with 0 unhappy agents
    uint32_t final_unhappy = 0;
    double   final_avg_same = 0.0;
    double   elapsed_sec = 0.0;
};
#endif

// -------------------- RandomStepper --------------------
// Picks a random occupied vertex each iteration; if unhappy, moves it.
// By default, the destination is a uniformly random vacancy, independent of satisfaction.
// Maintains an exact "unhappy" set incrementally, so we can stop immediately at 0.
// Counts "moves" only when a move actually occurs.
template <class World, class Geometry>
class RandomStepper {
public:
    RandomStepper(World& w, const Geometry& g, uint32_t n_agents_estimate = 0)
        : W_(w), G_(g), N_(g.N()),
          occ_pos_(N_, kNPOS), vac_pos_(N_, kNPOS), unhap_pos_(N_, kNPOS)
    {
        occupied_.reserve(n_agents_estimate ? n_agents_estimate : N_);
        vacant_.reserve(N_ - (n_agents_estimate ? n_agents_estimate : 0));
        unhappy_.reserve(n_agents_estimate ? n_agents_estimate : N_);
    }

    // Build occupied/vacant lists and initialize the unhappy set exactly.
    void initialize(const Threshold& th) {
        occupied_.clear(); vacant_.clear(); unhappy_.clear();
        std::fill(occ_pos_.begin(),  occ_pos_.end(),  kNPOS);
        std::fill(vac_pos_.begin(),  vac_pos_.end(),  kNPOS);
        std::fill(unhap_pos_.begin(),unhap_pos_.end(),kNPOS);

        for (uint32_t v = 0; v < N_; ++v) {
            if (W_.occupied(v)) {
                occ_pos_[v] = static_cast<uint32_t>(occupied_.size());
                occupied_.push_back(v);
            } else {
                vac_pos_[v] = static_cast<uint32_t>(vacant_.size());
                vacant_.push_back(v);
            }
        }
        // Exact unhappy set (one-time O(|A|))
        for (uint32_t v : occupied_) {
            const uint32_t t = W_.type_of(v);
            if (!is_satisfied_at(v, t, th)) add_unhappy(v);
        }
    }

    template <class RNG>
    SimResult run(const Threshold& th, RNG& rng, uint64_t max_iters,
                  const ProgressOptionsRandom* prog = nullptr,
                  const RandomMoveOptions* opt_in   = nullptr)
    {
        const RandomMoveOptions opt = opt_in ? *opt_in : RandomMoveOptions{};
        using clock = std::chrono::steady_clock;
        const auto t0 = clock::now();
        auto last_report = t0;
        bool printed_header = false;

        SimResult res{};
        uint64_t successful_moves = 0;

        if (occupied_.empty()) {
            finalize_metrics(th, res);
            res.converged = (res.final_unhappy == 0);
            res.moves = successful_moves;
            return res;
        }

        for (uint64_t iter = 0; iter < max_iters; ++iter) {
            // If nobody is unhappy, stop RIGHT NOW.
            if (unhappy_.empty()) {
                const auto t1 = clock::now();
                res.converged   = true;
                res.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
                res.final_unhappy = 0;
                Metrics<World, Geometry> meas(W_, G_);
                auto [u, avg] = meas.unhappy_and_avg_same_fraction(th);
                (void)u;
                res.final_avg_same = avg;
                res.moves = successful_moves;
                return res;
            }

            // ---- Progress (interval-gated; uses exact unhappy_ set) ----
            if (prog && prog->enabled) {
                bool due_iters = (prog->report_every_moves > 0) && (iter % prog->report_every_moves == 0);
                bool due_time  = false;
                if (prog->report_every_ms > 0) {
                    auto now = clock::now();
                    due_time = (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_report).count()
                                >= static_cast<long long>(prog->report_every_ms));
                    if (due_time) last_report = now;
                }
                if (due_iters || due_time) {
                    const auto now = clock::now();
                    const double elapsed = std::chrono::duration<double>(now - t0).count();
                    const double iters_per_s = (elapsed > 0.0) ? double(iter) / elapsed : 0.0;
                    const double unhappy_pct = occupied_.empty() ? 0.0
                        : (100.0 * double(unhappy_.size()) / double(occupied_.size()));
                    const double eta_budget = (iters_per_s > 0.0) ? double(max_iters - iter) / iters_per_s
                        : std::numeric_limits<double>::infinity();

                    if (prog->print_header_once && !printed_header) {
                        std::cerr << "iters\t%budget\tunhappy\tunhappy%\tmoves_ok\titers/s\tETA(budget)s\n";
                        printed_header = true;
                    }

                    std::cerr << iter << '\t'
                              << std::setprecision(3) << std::fixed << (100.0 * (double(iter) / double(max_iters))) << '\t'
                              << unhappy_.size() << '\t'
                              << std::setprecision(3) << unhappy_pct << '\t'
                              << successful_moves << '\t'
                              << std::setprecision(1) << iters_per_s << '\t'
                              << std::setprecision(1) << eta_budget << '\n';
                }
            }
            // ------------------------------------------------------------

            // 1) pick a random agent uniformly in O(1)
            if (occupied_.empty()) break; // degenerate
            const uint32_t pick = static_cast<uint32_t>(rng.uniform_u64(occupied_.size()));
            const uint32_t src  = occupied_[pick];
            const uint32_t t    = W_.type_of(src);

            // 2) check if happy via neighbors; if happy, skip; if unhappy, move it
            if (is_satisfied_at(src, t, th)) {
                // happy; do nothing
            } else {
                // temporarily remove from unhappy (we will re-evaluate / reinsert as needed)
                remove_unhappy(src);

                // If there are no vacancies, we can't move anyone
                if (vacant_.empty()) {
                    const auto t1 = clock::now();
                    res.converged   = false;
                    res.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
                    finalize_metrics(th, res);
                    res.moves = successful_moves;
                    return res;
                }

                uint32_t dst = UINT32_MAX;

                if (opt.ignore_satisfaction) {
                    // **Uniform random vacancy**, independent of satisfaction
                    const uint32_t vi = static_cast<uint32_t>(rng.uniform_u64(vacant_.size()));
                    dst = vacant_[vi];
                } else {
                    // Optional alternative: try to find a satisfied spot among K random vacancies
                    uint32_t best_dst = UINT32_MAX, best_same = 0;
                    const uint32_t K = std::min<uint32_t>(opt.k_candidates, static_cast<uint32_t>(vacant_.size()));
                    for (uint32_t k = 0; k < K; ++k) {
                        const uint32_t vi   = static_cast<uint32_t>(rng.uniform_u64(vacant_.size()));
                        const uint32_t cand = vacant_[vi];
                        uint32_t same = same_count_at(cand, t);
                        uint32_t other = total_count_at(cand) - same;
                        if (opt.fallback_best && same > best_same) { best_same = same; best_dst = cand; }
                        if (th.satisfied(same, other)) { dst = cand; break; }
                    }
                    if (dst == UINT32_MAX && opt.fallback_best && best_dst != UINT32_MAX) {
                        dst = best_dst; // may remain unhappy
                    }
                    if (dst == UINT32_MAX) {
                        // No destination chosen → put src back into unhappy and continue
                        update_unhappy(src, th);
                        continue;
                    }
                }

                // apply move in O(1): update world + lists
                W_.move(src, dst);

                // occupied list: replace src->dst
                occupied_[pick] = dst;
                occ_pos_[dst] = pick;
                occ_pos_[src] = kNPOS;

                // vacancy list: swap-pop remove dst; add src
                vacancy_remove(dst);
                vacancy_add(src);

                ++successful_moves;  // only count successful moves

                // Update unhappy set locally: neighbors of src (lost neighbor) and dst (gained neighbor),
                // plus the moved agent at its new location (which can still be unhappy).
                G_.for_each_neighbor(src, [&](uint32_t u){ update_unhappy(u, th); });
                G_.for_each_neighbor(dst, [&](uint32_t u){ update_unhappy(u, th); });
                update_unhappy(dst, th);

                // If no one is unhappy anymore, stop immediately.
                if (unhappy_.empty()) {
                    const auto t1 = clock::now();
                    res.converged   = true;
                    res.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
                    res.final_unhappy = 0;
                    Metrics<World, Geometry> meas(W_, G_);
                    auto [u, avg] = meas.unhappy_and_avg_same_fraction(th);
                    (void)u;
                    res.final_avg_same = avg;
                    res.moves = successful_moves;
                    return res;
                }
            }

            if (iter + 1 == max_iters) {
                const auto t1 = clock::now();
                res.converged   = unhappy_.empty();
                res.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
                Metrics<World, Geometry> meas(W_, G_);
                auto [u, avg] = meas.unhappy_and_avg_same_fraction(th);
                res.final_unhappy = u;
                res.final_avg_same = avg;
                res.moves = successful_moves;
                return res;
            }
        }

        const auto t1 = std::chrono::steady_clock::now();
        res.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
        Metrics<World, Geometry> meas(W_, G_);
        auto [u, avg] = meas.unhappy_and_avg_same_fraction(th);
        res.final_unhappy = u;
        res.final_avg_same = avg;
        res.converged = (u == 0);
        res.moves = successful_moves;
        return res;
    }

private:
    World&          W_;
    const Geometry& G_;
    const uint32_t  N_;

    static constexpr uint32_t kNPOS = std::numeric_limits<uint32_t>::max();

    // lists + position maps (swap-pop O(1) updates)
    std::vector<uint32_t> occupied_;
    std::vector<uint32_t> occ_pos_; // index in occupied_ or kNPOS
    std::vector<uint32_t> vacant_;
    std::vector<uint32_t> vac_pos_; // index in vacant_ or kNPOS

    // exact unhappy set + position map
    std::vector<uint32_t> unhappy_;
    std::vector<uint32_t> unhap_pos_; // index in unhappy_ or kNPOS

    inline void vacancy_add(uint32_t v) {
        if (vac_pos_[v] != kNPOS) return;
        vac_pos_[v] = static_cast<uint32_t>(vacant_.size());
        vacant_.push_back(v);
    }
    inline void vacancy_remove(uint32_t v) {
        uint32_t p = vac_pos_[v];
        if (p == kNPOS) return;
        uint32_t last = static_cast<uint32_t>(vacant_.size() - 1);
        if (p != last) {
            uint32_t moved = vacant_[last];
            vacant_[p] = moved;
            vac_pos_[moved] = p;
        }
        vacant_.pop_back();
        vac_pos_[v] = kNPOS;
    }

    inline void add_unhappy(uint32_t v) {
        if (unhap_pos_[v] != kNPOS) return;
        unhap_pos_[v] = static_cast<uint32_t>(unhappy_.size());
        unhappy_.push_back(v);
    }
    inline void remove_unhappy(uint32_t v) {
        uint32_t p = unhap_pos_[v];
        if (p == kNPOS) return;
        uint32_t last = static_cast<uint32_t>(unhappy_.size() - 1);
        if (p != last) {
            uint32_t moved = unhappy_[last];
            unhappy_[p] = moved;
            unhap_pos_[moved] = p;
        }
        unhappy_.pop_back();
        unhap_pos_[v] = kNPOS;
    }
    inline void update_unhappy(uint32_t v, const Threshold& th) {
        if (!W_.occupied(v)) { remove_unhappy(v); return; }
        const uint32_t t = W_.type_of(v);
        const bool sat = is_satisfied_at(v, t, th);
        if (sat) remove_unhappy(v); else add_unhappy(v);
    }

    // Fast local checks (just read world for neighbors)
    inline bool is_satisfied_at(uint32_t v, uint32_t t, const Threshold& th) const {
        uint32_t same = 0, other = 0;
        G_.for_each_neighbor(v, [&](uint32_t u){
            if (!W_.occupied(u)) return;
            if (W_.type_of(u) == t) ++same; else ++other;
        });
        return th.satisfied(same, other);
    }
    inline uint32_t same_count_at(uint32_t v, uint32_t t) const {
        uint32_t same = 0;
        G_.for_each_neighbor(v, [&](uint32_t u){
            if (!W_.occupied(u)) return;
            if (W_.type_of(u) == t) ++same;
        });
        return same;
    }
    inline uint32_t total_count_at(uint32_t v) const {
        uint32_t tot = 0;
        G_.for_each_neighbor(v, [&](uint32_t u){
            if (W_.occupied(u)) ++tot;
        });
        return tot;
    }

    inline void finalize_metrics(const Threshold& th, SimResult& res) const {
        Metrics<World, Geometry> meas(W_, G_);
        auto [u, avg] = meas.unhappy_and_avg_same_fraction(th);
        res.final_unhappy = u;
        res.final_avg_same = avg;
    }
};
