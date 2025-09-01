#pragma once
#include <chrono>

struct Stopwatch {
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    void reset() { t0 = std::chrono::steady_clock::now(); }
    double seconds() const {
        using namespace std::chrono;
        return duration_cast<duration<double>>(steady_clock::now() - t0).count();
    }
};
