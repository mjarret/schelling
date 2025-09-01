// survival_utils.hpp
#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <cmath>

struct SurvivalPoint { double T; double S; };

enum class SurvivalMethod { KM, CONVERGED };

inline std::vector<SurvivalPoint>
kaplan_meier(const std::vector<uint64_t>& times, const std::vector<uint8_t>& event) {
    const size_t n = times.size();
    if (n == 0) return {};
    std::vector<size_t> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b){
        if (times[a] != times[b]) return times[a] < times[b];
        return event[a] > event[b];
    });
    struct Agg { uint64_t t; uint32_t d; uint32_t c; };
    std::vector<Agg> aggs;
    for (size_t k=0; k<n; ) {
        const uint64_t t = times[idx[k]];
        uint32_t d=0, c=0;
        while (k<n && times[idx[k]]==t) {
            if (event[idx[k]]) ++d; else ++c;
            ++k;
        }
        aggs.push_back({t,d,c});
    }
    std::vector<SurvivalPoint> curve;
    double S = 1.0;
    uint64_t at_risk = n;
    for (const auto& a : aggs) {
        if (at_risk == 0) break;
        if (a.d > 0) { S *= (1.0 - double(a.d)/double(at_risk)); curve.push_back({ double(a.t), S }); }
        if (at_risk >= a.d + a.c) at_risk -= (a.d + a.c); else at_risk = 0;
    }
    if (curve.empty()) {
        uint64_t tmax = 0; for (auto t : times) tmax = std::max<uint64_t>(tmax, t);
        curve.push_back({ double(tmax), 1.0 });
    }
    if (curve.front().T > 0.0) curve.insert(curve.begin(), SurvivalPoint{0.0, 1.0});
    return curve;
}

inline std::vector<SurvivalPoint>
empirical_converged(const std::vector<uint64_t>& times, const std::vector<uint8_t>& event) {
    std::vector<double> ev; ev.reserve(times.size());
    for (size_t i=0;i<times.size();++i) if (event[i]) ev.push_back(double(times[i]));
    if (ev.empty()) return { SurvivalPoint{0.0, 1.0} };
    std::sort(ev.begin(), ev.end());
    std::vector<SurvivalPoint> curve; curve.push_back({0.0,1.0});
    double S = 1.0;
    size_t n = ev.size();
    for (size_t i=0; i<n; ) {
        size_t j = i; while (j < n && ev[j] == ev[i]) ++j;
        S -= double(j - i)/double(n);
        curve.push_back({ ev[i], std::max(0.0, S) });
        i = j;
    }
    return curve;
}

inline double rmst(const std::vector<SurvivalPoint>& curve) {
    if (curve.empty()) return 0.0;
    double area = 0.0, prev_t = 0.0, prev_S = 1.0;
    for (const auto& p : curve) { if (p.T > prev_t) area += (p.T - prev_t) * prev_S; prev_t = p.T; prev_S = p.S; }
    return area;
}
