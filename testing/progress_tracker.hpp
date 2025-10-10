// Simple, single-bar progress tracker for the path tests.
// Minimal wrapper around p-ranav/indicators to show progress for one job.

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

// Use the single-include header from sibling indicators checkout.
// Path is relative to this file: Schelling/testing -> Workspace/indicators
#include "../../indicators/single_include/indicators/indicators.hpp"

namespace indicators {

inline std::atomic<bool> kill_signal_received{false};

class ProgressTracker {
private:
    size_t total_work = 100;
    std::chrono::high_resolution_clock::time_point start_time{};
    indicators::ProgressBar bar_{};

    static std::string format_count(unsigned long long v) {
        static const char* suf[] = {"", "K", "M", "B", "T", "P", "E"};
        int si = 0;
        double x = static_cast<double>(v);
        while (x >= 1000.0 && si < 6) { x /= 1000.0; ++si; }
        std::ostringstream oss;
        if (x >= 100.0) oss << std::fixed << std::setprecision(0) << x;
        else if (x >= 10.0) oss << std::fixed << std::setprecision(1) << x;
        else oss << std::fixed << std::setprecision(2) << x;
        oss << suf[si];
        return oss.str();
    }

    void init_bar(int job_id, size_t total_work) {
        std::ostringstream ss;
        ss << job_id;
        const std::string prefix = std::string("Path B=") + ss.str();

        auto term_w = indicators::terminal_size().second;
        // Keep plenty of headroom for times + counts to avoid wrapping
        bar_.set_option(indicators::option::BarWidth{term_w > 100 ? term_w - 100 : 20});
        bar_.set_option(indicators::option::ForegroundColor{indicators::Color::green});
        bar_.set_option(indicators::option::ShowPercentage{true});
        bar_.set_option(indicators::option::ShowElapsedTime{true});
        bar_.set_option(indicators::option::ShowRemainingTime{true});
        bar_.set_option(indicators::option::MaxPostfixTextLen{32});
        bar_.set_option(indicators::option::MaxProgress{total_work});
        bar_.set_option(indicators::option::PrefixText{prefix});
        bar_.set_option(indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}});
    }

public:
    ProgressTracker(int job_id, size_t total_work_)
        : total_work(total_work_) {
        init_bar(job_id, total_work);
        start_time = std::chrono::high_resolution_clock::now();
    }

    void complete(size_t result_size) {
        if (kill_signal_received.load()) return;
        (void)result_size;
        bar_.set_progress(total_work);
    }

    inline void set_progress(size_t progress, size_t /*set_size*/) {
        if (kill_signal_received.load()) return;
        // Append compact completed/total to the postfix text (e.g., 235M/1.85P)
        std::ostringstream oss;
        oss << format_count(progress) << "/" << format_count(total_work);
        bar_.set_option(indicators::option::PostfixText{oss.str()});
        bar_.set_progress(progress);
    }
};

} // namespace indicators
