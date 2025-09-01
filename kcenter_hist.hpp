#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace kcenter1d {

// ---- helpers ----
inline bool feasible_radius(const std::vector<double>& x, double r, std::size_t k) {
    // Greedy cover by intervals of length 2r
    const std::size_t n = x.size();
    std::size_t used = 0, i = 0;
    while (i < n) {
        ++used;
        const double cover_to = x[i] + 2.0 * r;
        // move i to first uncovered
        while (i < n && x[i] <= cover_to + 1e-12) ++i;
        if (used > k) return false;
    }
    return true;
}

// Find minimal radius r* so that <=k intervals of length 2r* cover all points
inline double solve_radius_minimax(const std::vector<double>& x_sorted, std::size_t k) {
    if (x_sorted.empty()) return 0.0;
    double lo = 0.0;
    double hi = x_sorted.back() - x_sorted.front(); // trivially feasible
    // 60 iters ~ double precision
    for (int it=0; it<60; ++it) {
        double mid = 0.5 * (lo + hi);
        if (feasible_radius(x_sorted, mid, k)) hi = mid; else lo = mid;
    }
    return hi;
}

// Produce centers at greedy positions for a given r
inline void greedy_centers(const std::vector<double>& x, double r, std::vector<double>& centers, std::vector<std::pair<std::size_t,std::size_t>>& index_ranges) {
    centers.clear();
    index_ranges.clear();
    const std::size_t n = x.size();
    std::size_t i = 0;
    while (i < n) {
        const double left = x[i];
        const double right = left + 2.0 * r;
        std::size_t j = i;
        while (j + 1 < n && x[j+1] <= right + 1e-12) ++j;
        centers.push_back(0.5 * (left + x[j]));
        index_ranges.emplace_back(i, j);
        i = j + 1;
    }
}

// Build Voronoi bins from centers; return [L,R) intervals
inline void voronoi_bins(const std::vector<double>& centers, double xmin, double xmax,
                         std::vector<double>& L, std::vector<double>& R) {
    const std::size_t m = centers.size();
    L.resize(m); R.resize(m);
    if (m == 0) return;
    // boundaries at midpoints
    std::vector<double> mid; mid.reserve(m-1);
    for (std::size_t i=0;i+1<m;++i) mid.push_back(0.5*(centers[i]+centers[i+1]));
    L[0] = xmin; R[m-1] = xmax;
    for (std::size_t i=0;i+1<m;++i) {
        R[i]   = mid[i];
        L[i+1] = mid[i];
    }
}

// Split the widest bin in two (by midpoint) to increase bin count by 1
inline void split_widest(std::vector<double>& L, std::vector<double>& R,
                         std::vector<unsigned>& counts) {
    std::size_t m = L.size();
    if (m == 0) return;
    // find widest by (R-L)
    double bestW = -1.0; std::size_t idx = 0;
    for (std::size_t i=0;i<m;++i) {
        double w = R[i] - L[i];
        if (w > bestW) { bestW = w; idx = i; }
    }
    double mid = 0.5*(L[idx]+R[idx]);
    unsigned cL = counts[idx]/2;
    unsigned cR = counts[idx]-cL;

    // insert new bin after idx
    std::vector<double> L2, R2; L2.reserve(m+1); R2.reserve(m+1);
    std::vector<unsigned> C2;   C2.reserve(m+1);
    for (std::size_t i=0;i<m;++i) {
        if (i == idx) {
            L2.push_back(L[i]);  R2.push_back(mid); C2.push_back(cL);
            L2.push_back(mid);   R2.push_back(R[i]); C2.push_back(cR);
        } else {
            L2.push_back(L[i]);  R2.push_back(R[i]); C2.push_back(counts[i]);
        }
    }
    L.swap(L2); R.swap(R2); counts.swap(C2);
}

// Assign counts to Voronoi bins using centers
inline void counts_from_centers(const std::vector<double>& x,
                                const std::vector<double>& centers,
                                std::vector<unsigned>& counts) {
    counts.assign(centers.size(), 0);
    if (centers.empty()) return;
    std::size_t m = centers.size();
    std::size_t j = 0;
    for (double v : x) {
        // move j so that |v - centers[j]| >= |v - centers[j+1]| if possible
        while (j + 1 < m && std::abs(v - centers[j+1]) < std::abs(v - centers[j])) ++j;
        ++counts[j];
    }
}

// Main: compute k-center histogram bins (variable widths).
// If requested_k == 0, choose k by a robust rule of thumb (sqrt(n)), then enforce k exactly.
inline void histogram_kcenter(const std::vector<uint64_t>& raw,
                              std::size_t requested_k,
                              std::vector<double>& x_mid,
                              std::vector<double>& y_count,
                              std::vector<double>& width)
{
    x_mid.clear(); y_count.clear(); width.clear();
    if (raw.empty()) return;

    // to double + sort
    std::vector<double> x; x.reserve(raw.size());
    for (auto v : raw) x.push_back(double(v));
    std::sort(x.begin(), x.end());
    const std::size_t n = x.size();
    const double xmin = x.front(), xmax = x.back();

    // choose k (if not specified) — use sqrt(n) as a robust minimal‑assumption heuristic,
    // cap sensibly to avoid too many tiny bins
    std::size_t k = (requested_k > 0 ? requested_k
                                     : std::min<std::size_t>(std::max<std::size_t>(1, (std::size_t)std::floor(std::sqrt(double(n)))), 200u));
    k = std::min(k, n); // cannot exceed #points

    // solve exact 1-D k-center by binary search on radius
    double r = solve_radius_minimax(x, k);

    // build centers & their index ranges (contiguous segments)
    std::vector<double> centers;
    std::vector<std::pair<std::size_t,std::size_t>> ranges;
    greedy_centers(x, r, centers, ranges);

    // convert to voronoi bins
    std::vector<double> L, R;
    voronoi_bins(centers, xmin, xmax, L, R);

    // counts per center
    std::vector<unsigned> counts;
    counts_from_centers(x, centers, counts);

    // if we got fewer than requested k (can happen at "flat" radius thresholds), split widest bins until we reach k
    while (L.size() < k) {
        split_widest(L, R, counts);
    }

    // output midpoints, counts, widths
    const std::size_t m = L.size();
    x_mid.resize(m); y_count.resize(m); width.resize(m);
    for (std::size_t i=0;i<m;++i) {
        x_mid[i]   = 0.5 * (L[i] + R[i]);
        y_count[i] = double(counts[i]);
        width[i]   = std::max(1e-12, R[i] - L[i]);
    }
}

} // namespace kcenter1d
