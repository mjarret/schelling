#pragma once
#include <vector>
#include <cstddef>
#include <stdexcept>
#include <algorithm>

/*
Lollipop graph: clique K_m (vertices [0..m-1]) + path of length L (vertices [m..m+L-1]).
Attach path node m to clique node 0.

Adjacency is built once into CSR-like arrays: offsets[N+1], nbrs[2E].
Degree(v) = offsets[v+1]-offsets[v].
*/

struct LollipopGraph {
    using index_type = std::size_t;

    std::size_t m{0}, L{0}, N{0};
    std::vector<index_type> offsets;
    std::vector<index_type> nbrs;

    LollipopGraph() = default;
    LollipopGraph(std::size_t clique_m, std::size_t path_L) { build(clique_m, path_L); }

    void build(std::size_t clique_m, std::size_t path_L) {
        if (clique_m < 2) throw std::invalid_argument("Lollipop: m >= 2");
        m = clique_m; L = path_L; N = m + L;
        offsets.assign(N+1, 0);

        // Count degrees
        std::vector<std::size_t> deg(N, 0);
        // Clique
        for (std::size_t v = 0; v < m; ++v) deg[v] = m - 1;
        // Attach edge between clique node 0 and path node m
        deg[0] += (L > 0 ? 1 : 0);
        if (L > 0) deg[m] += 1;
        // Path internal edges
        for (std::size_t i = 0; i + 1 < L; ++i) {
            deg[m + i] += 1;
            deg[m + i + 1] += 1;
        }
        // Prefix sum
        for (std::size_t v = 0; v < N; ++v) offsets[v+1] = offsets[v] + deg[v];
        nbrs.assign(offsets.back(), 0);

        // Fill adjacency
        std::vector<std::size_t> cur = offsets;
        // Clique edges
        for (std::size_t u = 0; u < m; ++u) {
            for (std::size_t v = 0; v < m; ++v) if (u != v) {
                nbrs[cur[u]++] = v;
            }
        }
        // Attach edge
        if (L > 0) {
            nbrs[cur[0]++] = m;
            nbrs[cur[m]++] = 0;
        }
        // Path edges
        for (std::size_t i = 0; i + 1 < L; ++i) {
            const std::size_t a = m + i;
            const std::size_t b = m + i + 1;
            nbrs[cur[a]++] = b;
            nbrs[cur[b]++] = a;
        }
    }

    inline std::size_t num_vertices() const { return N; }
    inline std::size_t degree(std::size_t v) const { return offsets[v+1] - offsets[v]; }

    template<class F>
    inline void for_each_neighbor(std::size_t v, F&& f) const {
        for (std::size_t i = offsets[v]; i < offsets[v+1]; ++i) f(nbrs[i]);
    }
};
