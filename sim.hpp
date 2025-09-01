#pragma once
#include <cstdint>
#include <limits>
#include <utility>
#include <tuple>
#include <chrono>
#include <iostream>
#include <iomanip>

#include "metrics.hpp"   // Threshold, Metrics<World,Geometry>
#include "geometry.hpp"  // Torus2D or any Geometry policy

// ---------- Helpers ----------
static inline uint32_t wrap_add(uint32_t x, int delta, uint32_t mod) {
    int64_t v = static_cast<int64_t>(x) + static_cast<int64_t>(delta);
    v %= static_cast<int64_t>(mod);
    if (v < 0) v += mod;
    return static_cast<uint32_t>(v);
}

// Evaluate satisfaction for agent of type 'type' if located at vertex 'v'
template <class World, class Geometry>
inline bool satisfied_at(const World& W, const Geometry& G,
                         uint32_t v, uint32_t type, const Threshold& th)
{
    uint32_t same = 0, other = 0;
    G.for_each_neighbor(v, [&](uint32_t u){
        if (!W.occupied(u)) return;
        uint32_t t = W.type_of(u);
        if (t == type) ++same; else ++other;
    });
    return th.satisfied(same, other);
}

// Find nearest satisfactory vacancy via L∞-rings on the torus
template <class World, class Geometry>
inline uint32_t find_nearest_satisfactory_vacancy(const World& W, const Geometry& G,
                                                  uint32_t src_v, uint32_t type,
                                                  const Threshold& th)
{
    uint32_t x0, y0; G.xy(src_v, x0, y0);
    const uint32_t Wdim = G.W, Hdim = G.H;
    const uint32_t Rmax = (Wdim > Hdim ? Wdim : Hdim);

    for (uint32_t r = 1; r <= Rmax; ++r) {
        // top & bottom
        uint32_t yt = wrap_add(y0, -static_cast<int>(r), Hdim);
        uint32_t yb = wrap_add(y0,  static_cast<int>(r), Hdim);
        for (int dx = -static_cast<int>(r); dx <= static_cast<int>(r); ++dx) {
            uint32_t x = wrap_add(x0, dx, Wdim);
            uint32_t vt = G.idx(x, yt);
            if (!W.occupied(vt) && satisfied_at(W, G, vt, type, th)) return vt;
            uint32_t vb = G.idx(x, yb);
            if (!W.occupied(vb) && satisfied_at(W, G, vb, type, th)) return vb;
        }
        // left & right (skip corners)
        uint32_t xl = wrap_add(x0, -static_cast<int>(r), Wdim);
        uint32_t xr = wrap_add(x0,  static_cast<int>(r), Wdim);
        for (int dy = -static_cast<int>(r) + 1; dy <= static_cast<int>(r) - 1; ++dy) {
            uint32_t y = wrap_add(y0, dy, Hdim);
            uint32_t vl = G.idx(xl, y);
            if (!W.occupied(vl) && satisfied_at(W, G, vl, type, th)) return vl;
            uint32_t vr = G.idx(xr, y);
            if (!W.occupied(vr) && satisfied_at(W, G, vr, type, th)) return vr;
        }
    }
    return std::numeric_limits<uint32_t>::max();
}

// Pick a random unhappy agent using reservoir sampling.
// Returns (found?, vertex, type, unhappy_count).
template <class World, class Geometry, class RNG>
inline std::tuple<bool, uint32_t, uint32_t, uint32_t>
pick_random_unhappy(const World& W, const Geometry& G, const Threshold& th, RNG& rng)
{
    uint32_t chosen_v = 0, chosen_t = 0, unhappy_count = 0;
    bool found = false;

    W.for_each_agent([&](uint32_t v, uint32_t t){
        uint32_t same = 0, other = 0;
        G.for_each_neighbor(v, [&](uint32_t u){
            if (!W.occupied(u)) return;
            uint32_t tu = W.type_of(u);
            if (tu == t) ++same; else ++other;
        });
        if (!th.satisfied(same, other)) {
            ++unhappy_count;
            if (rng.uniform_u64(unhappy_count) == 0) {
                chosen_v = v; chosen_t = t; found = true;
            }
        }
    });
    return {found, chosen_v, chosen_t, unhappy_count};
}

// ---------- Progress options ----------
struct ProgressOptions {
    bool     enabled             = true;
    uint64_t report_every_moves  = 10000; // print every N moves (0 = disabled)
    uint64_t report_every_ms     = 1000;  // or every T milliseconds (0 = disabled)
    bool     show_avg_same       = false; // if true, compute avg same-type (expensive)
    bool     print_header_once   = true;  // header line before first progress
};

// ---------- Result ----------
struct SimResult {
    uint64_t moves = 0;
    bool     converged = false;
    uint32_t final_unhappy = 0;
    double   final_avg_same = 0.0;
    double   elapsed_sec = 0.0;
};

// Asynchronous moves until equilibrium or move budget. Reports progress if requested.
template <class World, class Geometry, class RNG>
inline SimResult run_until_equilibrium(World& W, const Geometry& G, const Threshold& th,
                                       RNG& rng, uint64_t max_moves,
                                       uint32_t n_agents,
                                       const ProgressOptions* prog = nullptr)
{
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    auto last_report = t0;
    bool printed_header = false;

    SimResult res{};

    for (uint64_t step = 0; step < max_moves; ++step) {
        // pick one unhappy agent (if any)
        auto [found, src_v, type, unhappy_cnt] = pick_random_unhappy(W, G, th, rng);

        // ---- Progress (cheap; no extra scans) ----
        if (prog && prog->enabled) {
            bool due_moves = (prog->report_every_moves > 0) && (step % prog->report_every_moves == 0);
            bool due_time  = false;
            if (prog->report_every_ms > 0) {
                auto now = clock::now();
                due_time = (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_report).count()
                            >= static_cast<long long>(prog->report_every_ms));
                if (due_time) last_report = now;
            }
            if ((due_moves || due_time)) {
                const auto now = clock::now();
                const double elapsed = std::chrono::duration<double>(now - t0).count();
                const double mps = (elapsed > 0.0) ? (double(step) / elapsed) : 0.0;
                const double unhappy_frac = (n_agents ? double(unhappy_cnt) / double(n_agents) : 0.0);
                const double eta_budget = (mps > 0.0) ? double(max_moves - step) / mps : std::numeric_limits<double>::infinity();

                if (prog->print_header_once && !printed_header) {
                    std::cerr << "moves\t%budget\tunhappy\tunhappy%\tmoves/s\tETA(budget)s";
                    if (prog->show_avg_same) std::cerr << "\tavg_same";
                    std::cerr << "\n";
                    printed_header = true;
                }

                std::cerr << step << '\t'
                          << std::setprecision(3) << std::fixed << (100.0 * (double(step) / double(max_moves))) << '\t'
                          << unhappy_cnt << '\t'
                          << std::setprecision(3) << (100.0 * unhappy_frac) << '\t'
                          << std::setprecision(1) << mps << '\t'
                          << std::setprecision(1) << eta_budget;

                if (prog->show_avg_same) {
                    Metrics<World, Geometry> meas(W, G); // full scan (expensive), gated by interval
                    auto [u_tmp, avg_tmp] = meas.unhappy_and_avg_same_fraction(th);
                    (void)u_tmp;
                    std::cerr << '\t' << std::setprecision(6) << avg_tmp;
                }
                std::cerr << std::endl; // line per report; use '\r' if you prefer one-line updates
            }
        }
        // ------------------------------------------

        if (!found) {
            // already at equilibrium
            const auto t1 = clock::now();
            res.moves = step;
            res.converged = true;
            res.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
            // compute final avg_same once
            Metrics<World, Geometry> meas(W, G);
            auto [u, avg] = meas.unhappy_and_avg_same_fraction(th);
            res.final_unhappy = u;
            res.final_avg_same = avg;
            return res;
        }

        // find nearest satisfactory vacancy, move if possible
        uint32_t dst = find_nearest_satisfactory_vacancy(W, G, src_v, type, th);
        if (dst != std::numeric_limits<uint32_t>::max()) {
            W.move(src_v, dst);
        }
        // If none found for this agent, skip; others may create opportunities later.

        if (step + 1 == max_moves) {
            const auto t1 = clock::now();
            Metrics<World, Geometry> meas(W, G);
            auto [u, avg] = meas.unhappy_and_avg_same_fraction(th);
            res.moves = max_moves;
            res.converged = (u == 0);
            res.final_unhappy = u;
            res.final_avg_same = avg;
            res.elapsed_sec = std::chrono::duration<double>(t1 - t0).count();
            return res;
        }
    }
    // Should not get here
    return res;
}
