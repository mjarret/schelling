#pragma once
/*
metrics.hpp — Minimal per-vertex metrics using on-demand neighbor scans
Updated: 2025-09-01

GOAL
- Match the prior codebase style: do NOT keep per-vertex arrays for static degree
  or occupied-neighbor counts. Instead, scan neighbors when we need to (degrees
  are small: 4 on torus; modest on lollipop).

WHAT THIS PROVIDES
- Tracks only:
    * unhappy[v]      — 0/1 flag
    * unhappy_total   — global count
    * unhappy_list    — pool of unhappy occupied vertices (for sampling)
    * pos_unhappy[v]  — index of v in unhappy_list, or NPOS if not present
- Defines happiness via CURRENT neighbors:
      unhappy(v) := [ same_occ(v) < ceil(tau * occ_neigh(v)) ],
  with occ_neigh(v) = number of OCCUPIED neighbors, same_occ(v) = occupied
  neighbors of the SAME TYPE as v. We treat occ_neigh(v)==0 as happy/neutral.

USAGE
- bind(G,W,tau) initializes the unhappy flags/pool from scratch.
- is_unhappy(v): query flag.
- has_unhappy(), random_unhappy(rng): sample an unhappy vertex in O(1).
- recompute_unhappy_scan(v): recompute via a neighbor scan (no aux arrays).
- for_each_occ_neighbor(v,f): iterate over currently-occupied neighbors.

NOTES
- No dynamic allocations in inner loops; scans are over small degrees.
- Graph API required: num_vertices(), for_each_neighbor(v,f).
- World API required: is_occ(v), type1(v).

*/

#include <vector>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <limits>
#include "../rng/splitmix64.hpp"

class WorldPacked; // fwd decl

template<class Graph>
struct Metrics {
    // ----- graph & world -----
    const Graph* G{nullptr};
    const WorldPacked* W{nullptr};

    // ----- unhappy flags & pool -----
    std::vector<unsigned char> unhappy;    // 0/1 unhappy flag
    std::size_t unhappy_total{0};

    static constexpr std::size_t NPOS = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> unhappy_list; // indices of vertices currently unhappy
    std::vector<std::size_t> pos_unhappy;  // position of v in unhappy_list or NPOS

    // ----- rule parameter -----
    double tau{0.0};                        // satisfaction threshold in [0,1]

    // Initialize from current world state
    void bind(const Graph* g, const WorldPacked* w, double tau_in) {
        G = g; W = w; tau = tau_in;
        const std::size_t N = G->num_vertices();

        unhappy.assign(N, 0);
        pos_unhappy.assign(N, NPOS);
        unhappy_list.clear();
        unhappy_total = 0;
        unhappy_list.reserve(N / 4 + 16);

        for (std::size_t v = 0; v < N; ++v) {
            if (!W->is_occ(v)) continue;
            const bool is_unh = recompute_unhappy_scan(v);
            if (is_unh) add_unhappy_(v);
        }
    }

    // ----- queries -----
    inline bool is_unhappy(std::size_t v) const { return unhappy[v] != 0; }
    inline bool has_unhappy() const { return unhappy_total > 0; }

    // Sample a random unhappy occupied vertex (caller must ensure has_unhappy()).
    inline std::size_t random_unhappy(SplitMix64& rng) const {
        return unhappy_list[rng.uniform_index(unhappy_list.size())];
    }

    // Iterate occupied neighbors only.
    template<class F>
    inline void for_each_occ_neighbor(std::size_t v, F&& f) const {
        G->for_each_neighbor(v, [&](std::size_t u){
            if (W->is_occ(u)) f(u);
        });
    }

    // Recompute unhappy flag for a *currently occupied* v by a small neighbor scan.
    // Returns the new unhappy value (true/false). Does not mutate state.
    inline bool recompute_unhappy_scan(std::size_t v) const {
        if (!W->is_occ(v)) return false; // empty vertices treated as not-unhappy
        const bool t1 = W->type1(v);

        std::uint16_t occ = 0, same = 0;
        G->for_each_neighbor(v, [&](std::size_t u){
            if (!W->is_occ(u)) return;
            ++occ;
            same += static_cast<std::uint16_t>(W->type1(u) == t1);
        });

        if (occ == 0) return false; // neutral by convention
        const double need_d = std::ceil(tau * static_cast<double>(occ) - 1e-12);
        const std::uint16_t need = static_cast<std::uint16_t>(
            std::max(0.0, std::min<double>(need_d, static_cast<double>(occ)))
        );
        return same < need;
    }

    // Set/unset unhappy flag for v and maintain pool & counters (O(1)).
    inline void set_unhappy(std::size_t v, bool flag) {
        const bool cur = unhappy[v] != 0;
        if (cur == flag) return;
        unhappy[v] = flag ? 1u : 0u;
        if (flag) {
            // add to pool
            pos_unhappy[v] = unhappy_list.size();
            unhappy_list.push_back(v);
            ++unhappy_total;
        } else {
            // remove from pool if present
            const std::size_t pos = pos_unhappy[v];
            if (pos != NPOS) {
                const std::size_t last = unhappy_list.size() - 1;
                const std::size_t moved = unhappy_list[last];
                std::swap(unhappy_list[pos], unhappy_list[last]);
                pos_unhappy[moved] = pos;
                unhappy_list.pop_back();
                pos_unhappy[v] = NPOS;
            }
            if (unhappy_total > 0) --unhappy_total;
        }
    }

private:
    inline void add_unhappy_(std::size_t v) {
        unhappy[v] = 1u;
        pos_unhappy[v] = unhappy_list.size();
        unhappy_list.push_back(v);
        ++unhappy_total;
    }
};
