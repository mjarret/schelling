// checkpoints.hpp
#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>

inline std::vector<uint64_t> make_checkpoints_linear(uint64_t horizon, uint64_t step) {
    std::vector<uint64_t> cp;
    cp.push_back(0);
    if (step == 0) step = 1;
    for (uint64_t t = step; t < horizon; t += step) cp.push_back(t);
    cp.push_back(horizon);
    return cp;
}
inline std::vector<uint64_t> make_checkpoints_log(uint64_t horizon, size_t points) {
    if (points < 2) points = 2;
    std::vector<uint64_t> cp; cp.reserve(points);
    cp.push_back(0);
    double L = std::log(double(horizon));
    for (size_t k = 1; k < points-1; ++k) {
        double f = double(k) / double(points - 1);
        uint64_t t = std::max<uint64_t>(1, (uint64_t)std::floor(std::exp(f * L)));
        if (t > cp.back()) cp.push_back(t);
    }
    cp.push_back(horizon);
    cp.erase(std::unique(cp.begin(), cp.end()), cp.end());
    return cp;
}
