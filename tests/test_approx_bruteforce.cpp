// Brute-force clarity test for core/approx.hpp
// Protocol: maximum readability, brute force within small bounds (K<=8),
// parallelized with a simple thread fan-out and a console progress bar.

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <thread>
#include <vector>

#include "core/config.hpp"
#include "core/approx.hpp"

// Exact rational comparison via cross-multiply (no FP rounding).
static inline int cmp_frac(color_count_t p1, color_count_t q1,
                           color_count_t p2, color_count_t q2) {
    unsigned __int128 lhs = static_cast<unsigned __int128>(p1) * q2;
    unsigned __int128 rhs = static_cast<unsigned __int128>(p2) * q1;
    if (lhs < rhs) return -1;
    if (lhs > rhs) return 1;
    return 0;
}

struct BruteBounds { color_count_t lp, lq, hp, hq; };

// Brute-force lower/upper approximants for tau with denominators <= K.
// Lower = max{ p/q <= tau }, Upper = min{ p/q >= tau } over 1<=q<=K, 0<=p<=q.
static BruteBounds brute_bounds(double tau, color_count_t K) {
    bool has_lo = false, has_hi = false;
    color_count_t lp=0, lq=1, hp=1, hq=1;
    for (color_count_t q = 1; q <= K; ++q) {
        for (color_count_t p = 0; p <= q; ++p) {
            const double v = static_cast<double>(p) / static_cast<double>(q);
            if (v <= tau) {
                if (!has_lo || cmp_frac(p,q, lp,lq) > 0) { lp=p; lq=q; has_lo=true; }
            }
            if (v >= tau) {
                if (!has_hi || cmp_frac(p,q, hp,hq) < 0) { hp=p; hq=q; has_hi=true; }
            }
        }
    }
    return {lp,lq,hp,hq};
}

int main() {
    // Test configuration (no CLI for tests; keep legible and bounded):
    // - agents_max anchors tau_den_max = agents_max - 1 (occupancy-based denominator bound)
    // - Kmax caps K in [1..Kmax]
    // - Rational enumeration bound is derived from agents_max (q_enum = tau_den_max)
    //   Keep agents_max modest so the O(q^2) brute force remains tractable.
    constexpr color_count_t agents_max = static_cast<color_count_t>(2048);
    constexpr color_count_t Kmax = static_cast<color_count_t>(8);
    const std::size_t threads = std::max(1u, std::thread::hardware_concurrency());
    const bool show_progress = true;
    // Build worklist of (K, tau) pairs. For each K in [1..8],
    // add:
    // - All rationals p/q with q in [1..8]
    // - Immediate neighbors around each p/q via nextafter (one ulp up/down)
    // - Dyadic grid with denominator 2^(ceil(log2(K))+1) to go one bit beyond
    //   the nominal resolution suggested by K.
    struct Job { color_count_t K; double tau; color_count_t p_ref, q_ref; };
    std::vector<Job> jobs;
    jobs.reserve(8 * 128);
    auto add_tau = [&](color_count_t K, double tau, color_count_t p=0, color_count_t q=1){ jobs.push_back(Job{K, tau, p, q}); };
    const color_count_t tau_den_max = (agents_max > 0 ? agents_max - 1 : 1);
    const color_count_t q_enum = tau_den_max; // derive directly from agents_max
    for (color_count_t K = 1; K <= Kmax; ++K) {
        // Rational grid q<=q_enum
        for (color_count_t q = 1; q <= q_enum; ++q) {
            for (color_count_t p = 0; p <= q; ++p) {
                const double t = static_cast<double>(p) / static_cast<double>(q);
                add_tau(K, t, p, q);
                // One ulp on each side (clamped to [0,1])
                const double down = std::nextafter(t, 0.0);
                const double up   = std::nextafter(t, 1.0);
                if (down >= 0.0) add_tau(K, down, p, q);
                if (up   <= 1.0) add_tau(K, up,   p, q);
            }
        }
        // Dyadic grid with 1 extra bit beyond ceil(log2(tau_den_max))
        unsigned bits = 0; while ((color_count_t(1) << bits) < tau_den_max && bits < 8*sizeof(color_count_t)-1) ++bits;
        const color_count_t D = color_count_t(1) << (bits + 1);
        for (color_count_t n = 0; n <= D; ++n) add_tau(K, static_cast<double>(n) / static_cast<double>(D));
    }

    std::atomic<std::size_t> done{0};
    const std::size_t total = jobs.size();
    // threads set from CLI (default 1); set to hardware_concurrency for full speed

    auto print_progress = [&](std::size_t d){
        const double pct = total ? (100.0 * double(d) / double(total)) : 100.0;
        std::fprintf(stderr, "\r[approx] %6.2f%% (%zu/%zu)", pct, d, total);
        std::fflush(stderr);
    };

    std::atomic<bool> ok{true};
    struct Mismatch { color_count_t K; double tau; color_count_t blp, blq, bhp, bhq, alp, alq, ahp, ahq; };
    std::atomic<bool> have_mismatch{false};
    Mismatch mm{};
    auto worker = [&](std::size_t tid){
        for (std::size_t i = tid; i < total; i += threads) {
            const auto& jb = jobs[i];
            const auto b = brute_bounds(jb.tau, jb.K);
            const auto a = core::approx::farey_neighbors(jb.tau, jb.K);
            if (!(b.lp == a.lo.p && b.lq == a.lo.q && b.hp == a.hi.p && b.hq == a.hi.q)) {
                ok.store(false, std::memory_order_relaxed);
                if (!have_mismatch.exchange(true)) {
                    mm = Mismatch{jb.K, jb.tau, b.lp,b.lq,b.hp,b.hq, a.lo.p,a.lo.q,a.hi.p,a.hi.q};
                }
                return;
            }
            const auto d = done.fetch_add(1, std::memory_order_relaxed) + 1;
            if (show_progress && (d % 256) == 0 && tid == 0) print_progress(d);
        }
    };

    std::vector<std::thread> pool; pool.reserve(threads);
    for (std::size_t t=0; t<threads; ++t) pool.emplace_back(worker, t);
    for (auto& th : pool) th.join();
    if (show_progress) { print_progress(done.load()); std::fprintf(stderr, "\n"); }

    if (!ok.load()) {
        std::cerr << "FAIL: K="<<mm.K
                  << " tau="<<mm.tau
                  << " brute_lo="<<mm.blp<<"/"<<mm.blq
                  << " approx_lo="<<mm.alp<<"/"<<mm.alq
                  << " brute_hi="<<mm.bhp<<"/"<<mm.bhq
                  << " approx_hi="<<mm.ahp<<"/"<<mm.ahq
                  << "\n";
        return 1;
    }
    std::cout << "APPROX BRUTE-FORCE TESTS PASSED\n";
    return 0;
}
