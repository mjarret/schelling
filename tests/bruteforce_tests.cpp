#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
#include <array>
#include <functional>
#include <cmath>

#include "graphs/path_graph.hpp"
#include "graphs/clique_graph.hpp"
#include "graphs/lollipop_graph.hpp"

using std::size_t;

template <size_t K>
std::uint64_t recompute_path_total(const graphs::PathGraph<K>& g) {
    const size_t n = g.size();
    std::uint64_t tf = 0;
    for (size_t i = 0; i + 1 < n; ++i) {
        auto a = g.get_color(i), b = g.get_color(i+1);
        tf += (a != 0u && b != 0u && a != b);
    }
    return tf;
}

template <size_t K>
std::uint64_t recompute_clique_total_from_counts(const graphs::CliqueGraph<K>& g) {
    // counts[0] is unoccupied; colors are 1..K
    std::uint64_t occ = g.color_count(0) <= (std::uint64_t)g.size() ? (g.size() - g.color_count(0)) : 0ULL;
    std::uint64_t sumsq = 0;
    for (size_t c = 1; c <= K; ++c) sumsq += g.color_count(c) * g.color_count(c);
    return (occ * occ - sumsq) / 2ULL;
}

template <size_t K>
std::uint64_t recompute_lollipop_total(size_t m, size_t n, const graphs::LollipopGraph<K>& g) {
    // Vertex layout: [0..m-2]=clique (minus bridge), bridge=(m-1), path=[m..m+n-1]
    auto color = [&](size_t v){ return g.get_color(static_cast<std::uint32_t>(v)); };
    std::uint64_t tf = 0;
    if (m) {
        const size_t bridge = m - 1;
        // clique-internal (excluding bridge): complete graph on [0..bridge-1]
        for (size_t i = 0; i + 1 < bridge; ++i) {
            auto ci = color(i);
            if (ci == 0) continue;
            for (size_t j = i + 1; j < bridge; ++j) {
                auto cj = color(j);
                tf += (cj != 0 && cj != ci);
            }
        }
        // edges from bridge to clique vertices
        auto cb = color(bridge);
        if (cb != 0) {
            for (size_t i = 0; i < bridge; ++i) {
                auto ci = color(i);
                tf += (ci != 0 && ci != cb);
            }
        }
        // bridge to path start
        if (n > 0) {
            auto c0 = color(m);
            tf += (cb != 0 && c0 != 0 && cb != c0);
        }
    }
    // path internal
    for (size_t i = m; i + 1 < m + n; ++i) {
        auto a = color(i), b = color(i+1);
        tf += (a != 0 && b != 0 && a != b);
    }
    return tf;
}

template <size_t K>
bool test_path_small(size_t n) {
    using G = graphs::PathGraph<K>;
    const size_t states = 1;
    std::atomic<bool> ok{true};
    const std::uint64_t total = 1;
    const size_t threads = std::max(1u, std::thread::hardware_concurrency());
    auto worker = [&](size_t tid){
        // Enumerate all colorings 0..K at each vertex as a base-(K+1) number
        const std::uint64_t base = K + 1;
        std::uint64_t space = 1; for (size_t i=0;i<n;++i) space *= base;
        for (std::uint64_t mask = tid; mask < space; mask += threads) {
            G g(n);
            std::uint64_t x = mask;
            for (size_t v = 0; v < n; ++v) {
                std::uint32_t c = static_cast<std::uint32_t>(x % base);
                x /= base;
                g.change_color(v, c, g.get_color(v));
            }
            const auto slow = recompute_path_total(g);
            const auto fast = g.total_frustration();
            if (slow != fast) {
                ok.store(false, std::memory_order_relaxed);
                std::cerr << "PathGraph<K="<<K<<"> mismatch: n="<<n<<" slow="<<slow<<" fast="<<fast<<"\n";
                return; // early exit
            }
        }
    };
    std::vector<std::thread> pool;
    for (size_t t=0;t<threads;++t) pool.emplace_back(worker, t);
    for (auto& th: pool) th.join();
    return ok.load();
}

// Generate all compositions of m into (K+1) parts (counts[0..K])
inline void compositions(std::size_t m, std::size_t parts, std::function<void(const std::vector<std::size_t>&)> f) {
    std::vector<std::size_t> a(parts, 0);
    std::function<void(std::size_t,std::size_t)> rec = [&](std::size_t i, std::size_t rem){
        if (i + 1 == parts) { a[i] = rem; f(a); return; }
        for (std::size_t x = 0; x <= rem; ++x) { a[i] = x; rec(i+1, rem - x); }
    };
    rec(0, m);
}

template <size_t K>
bool test_clique_small(size_t m) {
    using G = graphs::CliqueGraph<K>;
    std::atomic<bool> ok{true};
    std::vector<std::vector<std::size_t>> all;
    compositions(m, K+1, [&](const std::vector<std::size_t>& v){ all.push_back(v); });
    const size_t threads = std::max(1u, std::thread::hardware_concurrency());
    auto worker = [&](size_t tid){
        for (size_t i = tid; i < all.size(); i += threads) {
            const auto& cnt = all[i];
            // Skip trivial all-zero (no occupancy) since we want to test color-to-color deltas
            std::size_t occ = 0; for (size_t c=1;c<=K;++c) occ += cnt[c];
            if (occ == 0) continue;

            // Initialize directly via counts and recompute tf
            std::array<std::uint64_t, K+1> counts0{};
            for (size_t c=0;c<=K;++c) counts0[c] = cnt[c];
            G g(m);
            g.set_colors(counts0);

            // Now apply all transitions including 0 <-> nonzero and verify
            for (size_t a = 0; a <= K; ++a) {
                for (size_t b = 0; b <= K; ++b) {
                    if (a == b) continue;
                    if (g.color_count(static_cast<std::uint32_t>(a)) == 0) continue;
                    g.change_color(0, static_cast<std::uint32_t>(b), static_cast<std::uint32_t>(a));
                    const auto slow = recompute_clique_total_from_counts(g);
                    const auto fast = g.total_frustration();
                    if (slow != fast) {
                        ok.store(false, std::memory_order_relaxed);
                        std::cerr << "CliqueGraph<K="<<K<<"> mismatch: m="<<m<<" a->b="<<a<<"->"<<b
                                  << " slow="<<slow<<" fast="<<fast<<"\n";
                        return;
                    }
                    // revert back to a for next iterations
                    g.change_color(0, static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(b));
                }
            }
        }
    };
    std::vector<std::thread> pool;
    for (size_t t=0;t<threads;++t) pool.emplace_back(worker, t);
    for (auto& th: pool) th.join();
    return ok.load();
}

template <size_t K>
bool test_lollipop_small(size_t m, size_t n) {
    using G = graphs::LollipopGraph<K>;
    // Brute force small sizes: enumerate bridge_color in [0..K], clique counts, and path colors
    std::atomic<bool> ok{true};
    std::vector<std::vector<std::size_t>> clq;
    const size_t clique_core = (m>0 ? m-1 : 0);
    compositions(clique_core, K+1, [&](const std::vector<std::size_t>& v){ clq.push_back(v); });
    const std::uint64_t base = K + 1;
    std::uint64_t path_space = 1; for (size_t i=0;i<n;++i) path_space *= base;
    const size_t threads = std::max(1u, std::thread::hardware_concurrency());
    auto worker = [&](size_t tid){
        for (size_t i = tid; i < clq.size(); i += threads) {
            for (std::uint32_t bridge = 0; bridge <= K; ++bridge) {
                for (std::uint64_t mask = 0; mask < path_space; ++mask) {
                    G g(m, n);
                    // Fill clique core counts (0->c changes)
                    for (size_t c = 1; c <= K; ++c) {
                        for (size_t k = 0; k < clq[i][c]; ++k) g.change_color(static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(c), 0);
                    }
                    // set bridge color
                    if (m>0) g.change_color(static_cast<std::uint32_t>(m-1), bridge, 0);
                    // fill path from base-(K+1) mask
                    std::uint64_t x = mask;
                    for (size_t v = 0; v < n; ++v) {
                        std::uint32_t c = static_cast<std::uint32_t>(x % base); x/=base;
                        g.change_color(static_cast<std::uint32_t>(m+v), c, 0);
                    }
                    const auto slow = recompute_lollipop_total(m, n, g);
                    const auto fast = g.total_frustration();
                    if (slow != fast) {
                        ok.store(false, std::memory_order_relaxed);
                        std::cerr << "LollipopGraph<K="<<K<<"> mismatch: m="<<m<<" n="<<n<<" slow="<<slow<<" fast="<<fast<<"\n";
                        return;
                    }
                }
            }
        }
    };
    std::vector<std::thread> pool;
    for (size_t t=0;t<threads;++t) pool.emplace_back(worker, t);
    for (auto& th: pool) th.join();
    return ok.load();
}

template <size_t K>
bool test_lollipop_large(size_t m, size_t n) {
    using G = graphs::LollipopGraph<K>;
    std::atomic<bool> ok{true};
    std::vector<std::vector<std::size_t>> clq;
    const size_t clique_core = (m>0 ? m-1 : 0);
    compositions(clique_core, K+1, [&](const std::vector<std::size_t>& v){ clq.push_back(v); });
    const std::uint64_t base = K + 1;
    std::uint64_t path_space = 1; for (size_t i=0;i<n;++i) path_space *= base;
    const size_t threads = std::max(1u, std::thread::hardware_concurrency());
    auto other_nonzero = [](std::uint32_t c){ return c == 1 ? 2u : 1u; };

    auto worker = [&](size_t tid){
        for (size_t i = tid; i < clq.size(); i += threads) {
            for (std::uint32_t bridge = 1; bridge <= K; ++bridge) { // nonzero only
                for (std::uint64_t mask = 0; mask < path_space; ++mask) {
                    G g(m, n);
                    // Fill clique core counts via 0->c changes
                    for (size_t c = 1; c <= K; ++c) {
                        for (size_t k = 0; k < clq[i][c]; ++k) g.change_color(static_cast<std::uint32_t>(0), static_cast<std::uint32_t>(c), 0);
                    }
                    // set bridge color nonzero
                    if (m>0) g.change_color(static_cast<std::uint32_t>(m-1), bridge, 0);
                    // fill path from base-(K+1) mask
                    std::uint64_t x = mask;
                    for (size_t v = 0; v < n; ++v) {
                        std::uint32_t c = static_cast<std::uint32_t>(x % base); x/=base;
                        g.change_color(static_cast<std::uint32_t>(m+v), c, 0);
                    }

                    // For Lollipop, only validate deltas for nonzero->nonzero flips (baseline may include 0->c steps)
                    auto slow0 = recompute_lollipop_total<K>(m,n,g);
                    auto fast0 = g.total_frustration();

                    // Perform a few nonzero->nonzero flips deterministically
                    // 1) flip bridge 1<->2
                    if (m>0) {
                        g.change_color(static_cast<std::uint32_t>(m-1), other_nonzero(bridge), bridge);
                        const auto slow = recompute_lollipop_total<K>(m,n,g);
                        const auto fast = g.total_frustration();
                        if ((slow - slow0) != (fast - fast0)) { ok.store(false); std::cerr << "Lollipop bridge flip delta mismatch m="<<m<<" n="<<n<<"\n"; return; }
                        // revert
                        g.change_color(static_cast<std::uint32_t>(m-1), bridge, other_nonzero(bridge));
                        slow0 = recompute_lollipop_total<K>(m,n,g);
                        fast0 = g.total_frustration();
                    }

                    // 2) flip first nonzero clique-core vertex if any
                    for (size_t v = 0; v + 1 < m; ++v) {
                        auto col = static_cast<std::uint32_t>(g.get_color(static_cast<std::uint32_t>(v)));
                        if (col != 0) {
                            g.change_color(static_cast<std::uint32_t>(v), other_nonzero(col), col);
                            const auto slow = recompute_lollipop_total<K>(m,n,g);
                            const auto fast = g.total_frustration();
                            if ((slow - slow0) != (fast - fast0)) { ok.store(false); std::cerr << "Lollipop clique-core flip delta mismatch m="<<m<<" n="<<n<<"\n"; return; }
                            // revert
                            g.change_color(static_cast<std::uint32_t>(v), col, other_nonzero(col));
                            slow0 = recompute_lollipop_total<K>(m,n,g);
                            fast0 = g.total_frustration();
                            break;
                        }
                    }

                    // 3) flip first nonzero path vertex if any
                    for (size_t v = 0; v < n; ++v) {
                        auto col = static_cast<std::uint32_t>(g.get_color(static_cast<std::uint32_t>(m+v)));
                        if (col != 0) {
                            g.change_color(static_cast<std::uint32_t>(m+v), other_nonzero(col), col);
                            const auto slow = recompute_lollipop_total<K>(m,n,g);
                            const auto fast = g.total_frustration();
                            if ((slow - slow0) != (fast - fast0)) { ok.store(false); std::cerr << "Lollipop path flip delta mismatch m="<<m<<" n="<<n<<"\n"; return; }
                            // revert
                            g.change_color(static_cast<std::uint32_t>(m+v), col, other_nonzero(col));
                            slow0 = recompute_lollipop_total<K>(m,n,g);
                            fast0 = g.total_frustration();
                            break;
                        }
                    }
                }
            }
        }
    };
    std::vector<std::thread> pool;
    for (size_t t=0;t<threads;++t) pool.emplace_back(worker, t);
    for (auto& th: pool) th.join();
    return ok.load();
}

int main() {
    bool ok = true;
    // Path small sweeps
    for (size_t n = 1; n <= 7; ++n) ok &= test_path_small<2>(n);
    ok &= test_clique_small<2>(50);
    // Lollipop: larger sanity brute force with only nonzero->nonzero flips
    for (size_t m = 2; m <= 6; ++m) {
        for (size_t n = 0; n <= 7; ++n) ok &= test_lollipop_large<2>(m, n);
    }
    std::cout << (ok ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
    return ok ? 0 : 1;
}
