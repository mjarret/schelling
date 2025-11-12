// step_dense.hpp — heatmap row types and helpers (no logic change)
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace sim {

// Dense per-step rows: step -> row (bins length)
using StepDense = std::vector<std::vector<std::uint64_t>>;

// Merge helper for OpenMP reduction: assumes rows are either empty or sized to bins
inline void merge_step_dense(StepDense& dst, const StepDense& src) {
    if (src.size() > dst.size()) dst.resize(src.size());
    for (std::size_t s = 0; s < src.size(); ++s) {
        const auto& sr = src[s];
        if (sr.empty()) continue;
        auto& dr = dst[s];
        if (dr.empty()) { dr = sr; continue; }
        const std::size_t B = dr.size();
        for (std::size_t j = 0; j < B; ++j) dr[j] += sr[j];
    }
}

// Ensure a step row exists and is sized to bins (no-op if already present)
inline void ensure_step_row(StepDense& result, std::size_t s, std::size_t bins) {
    if (s >= result.size()) result.resize(s + 1);
    auto& row = result[s];
    if (row.empty()) row.assign(bins, 0ull);
}

// Heatmap dense matrix representation
struct Heatmap {
    std::size_t bins{0};
    std::size_t rows{0};
    std::vector<std::uint64_t> data; // row-major rows × bins
};

inline Heatmap to_dense(const StepDense& dense, std::size_t bins) {
    Heatmap out; out.bins = bins; out.rows = dense.size();
    out.data.assign(out.rows * out.bins, 0ull);
    for (std::size_t r = 0; r < out.rows; ++r) {
        const auto& row = dense[r];
        if (row.empty()) continue;
        std::copy(row.begin(), row.begin() + std::min<std::size_t>(row.size(), out.bins),
                  out.data.begin() + static_cast<std::ptrdiff_t>(r * out.bins));
    }
    return out;
}

} // namespace sim

