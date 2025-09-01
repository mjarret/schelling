// geometry_lollipop.hpp — Lollipop graph L(m, n) with adjacency.
// Clique K_m on vertices [0..m-1], path P_n on [m..m+n-1], bridge (m-1) <-> m.
#pragma once
#include <cstdint>
#include <vector>

struct LollipopGraph {
    LollipopGraph(uint32_t m_clique, uint32_t n_path)
    : m_(m_clique), n_(n_path) {
        if (m_ < 1) m_ = 1;
        if (n_ < 1) n_ = 1;
        build_adj();
    }

    inline uint32_t N() const { return static_cast<uint32_t>(adj_.size()); }

    // Neighborhood enumerator: writes neighbor ids to out[], sets deg.
    inline void neighbors(uint32_t v, uint32_t* out, uint32_t& deg) const {
        const auto& nb = adj_[v];
        deg = static_cast<uint32_t>(nb.size());
        for (uint32_t i=0;i<deg;++i) out[i] = nb[i];
    }

    // API used by metrics.hpp / sim_random.hpp
    template <class F>
    inline void for_each_neighbor(uint32_t v, F&& f) const {
        const auto& nb = adj_[v];
        for (uint32_t u : nb) f(u);
    }

    inline uint32_t degree(uint32_t v) const { return static_cast<uint32_t>(adj_[v].size()); }

private:
    uint32_t m_, n_;
    std::vector<std::vector<uint32_t>> adj_;

    void build_adj() {
        const uint32_t N = m_ + n_;
        adj_.assign(N, {});

        // Clique K_m
        for (uint32_t i=0;i<m_;++i) {
            adj_[i].reserve(m_ - 1 + (i==m_-1 ? 1u : 0u));
            for (uint32_t j=0;j<m_;++j) if (i!=j) adj_[i].push_back(j);
        }

        // Path P_n (indices m..m+n-1)
        for (uint32_t k=0;k<n_;++k) {
            uint32_t v = m_ + k;
            if (k > 0)    adj_[v].push_back(v-1);
            if (k+1 < n_) adj_[v].push_back(v+1);
        }

        // Bridge
        adj_[m_-1].push_back(m_);
        adj_[m_].push_back(m_-1);
    }
};
