#pragma once
#include <cstdint>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>

static inline uint64_t mix64(uint64_t z) {
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z ^= (z >> 31);
    return z;
}
static inline uint64_t make_random_seed() {
    uint64_t s = 0;
    try {
        std::random_device rd;
        uint64_t a = (uint64_t(rd()) << 32) ^ uint64_t(rd());
        s ^= mix64(a);
        if (rd.entropy() > 0) {
            uint64_t b = (uint64_t(rd()) << 32) ^ uint64_t(rd());
            s ^= mix64(b);
        }
    } catch (...) {}
    uint64_t t = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    s ^= mix64(t);
    uint64_t tid = static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    s ^= mix64(tid);
    uint64_t addr = reinterpret_cast<uintptr_t>(&t);
    s ^= mix64(addr);
    if (s == 0) s = 0x9e3779b97f4a7c15ULL;
    return s;
}
static inline uint64_t next_seed(uint64_t base, std::atomic<uint64_t>& ctr) {
    uint64_t k = ctr.fetch_add(1, std::memory_order_relaxed);
    return mix64(base + k);
}
