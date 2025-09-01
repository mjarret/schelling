// cost_aggregator.hpp — aggregates mean unhappy fraction across runs at checkpoints.
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>
#include <cmath>
#include <algorithm>

enum class CurveWeight { RUNS, AGENTS };

struct CostAggregator {
    static constexpr uint64_t SCALE = 1000000000ULL; // 1e9 fixed-point

    const std::vector<uint64_t>& checkpoints;
    CurveWeight weight;
    bool pad_zeros;
    const size_t m;

    std::unique_ptr<std::atomic<uint64_t>[]> sum_frac_scaled;    // sum of U/N, scaled
    std::unique_ptr<std::atomic<uint64_t>[]> sum_frac_sq_scaled; // sum of (U/N)^2, scaled^2
    std::unique_ptr<std::atomic<uint64_t>[]> count_runs;         // #runs added at cp
    std::unique_ptr<std::atomic<uint64_t>[]> sum_U;              // AGENTS mode
    std::unique_ptr<std::atomic<uint64_t>[]> sum_N;              // AGENTS mode

    CostAggregator(const std::vector<uint64_t>& cps, CurveWeight w, bool pad0)
    : checkpoints(cps), weight(w), pad_zeros(pad0), m(cps.size()),
      sum_frac_scaled(new std::atomic<uint64_t>[m]),
      sum_frac_sq_scaled(new std::atomic<uint64_t>[m]),
      count_runs(new std::atomic<uint64_t>[m]),
      sum_U(new std::atomic<uint64_t>[m]),
      sum_N(new std::atomic<uint64_t>[m]) {
        for (size_t i=0;i<m;++i) {
            sum_frac_scaled[i].store(0, std::memory_order_relaxed);
            sum_frac_sq_scaled[i].store(0, std::memory_order_relaxed);
            count_runs[i].store(0, std::memory_order_relaxed);
            sum_U[i].store(0, std::memory_order_relaxed);
            sum_N[i].store(0, std::memory_order_relaxed);
        }
    }

    inline void record(size_t idx, uint32_t U, uint32_t N) {
        if (idx >= m) return;
        if (weight == CurveWeight::RUNS) {
            double y = (N ? double(U) / double(N) : 0.0);
            y = std::clamp(y, 0.0, 1.0);
            uint64_t val  = (uint64_t)llround(y * double(SCALE));
            uint64_t val2 = (uint64_t)llround(y * y * double(SCALE) * double(SCALE));
            sum_frac_scaled[idx].fetch_add(val,  std::memory_order_relaxed);
            sum_frac_sq_scaled[idx].fetch_add(val2, std::memory_order_relaxed);
            count_runs[idx].fetch_add(1, std::memory_order_relaxed);
        } else {
            sum_U[idx].fetch_add(U, std::memory_order_relaxed);
            sum_N[idx].fetch_add(N, std::memory_order_relaxed);
        }
    }

    inline void pad_zeros_from(size_t idx, uint32_t N) {
        if (!pad_zeros) return;
        for (size_t k = idx; k < m; ++k) {
            if (weight == CurveWeight::RUNS) {
                count_runs[k].fetch_add(1, std::memory_order_relaxed);
            } else {
                sum_N[k].fetch_add(N, std::memory_order_relaxed);
            }
        }
    }
};
