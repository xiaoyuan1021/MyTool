#pragma once
#include <chrono>
#include <functional>
#include "spdlog/spdlog.h"

class BenchmarkTimer {
    const char* m_name;
    std::chrono::steady_clock::time_point m_start;
    double* m_outMs;
public:
    explicit BenchmarkTimer(const char* name, double* outMs = nullptr)
        : m_name(name)
        , m_start(std::chrono::steady_clock::now())
        , m_outMs(outMs) {}

    ~BenchmarkTimer() {
        auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - m_start).count();
        if (m_outMs) *m_outMs = elapsed;
        if (elapsed >= 1.0) {  // 只在耗时 ≥1ms 时打印
            spdlog::info("[BENCH] {}: {:.2f} ms", m_name, elapsed);
        }
    }

    double elapsedMs() const {
        return std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - m_start).count();
    }
};

inline void benchmarkAvg(const char* name, int iterations,
                          const std::function<void()>& fn) {
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < iterations; ++i) fn();
    auto total = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    spdlog::info("[BENCH] {}: avg={:.2f}ms (total={:.2f}ms, n={})",
                 name, total / iterations, total, iterations);
}
