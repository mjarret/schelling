#pragma once
#include <thread>
#include <atomic>
#include <string>
#include <sstream>
#include <chrono>
#include <algorithm>

#include <indicators/progress_bar.hpp>
#include <indicators/cursor_control.hpp>

#include "term_utils.hpp"
#include "cost_aggregator.hpp"
#include "cs_stop.hpp"

class ProgressManager {
public:
    ProgressManager(CostAggregator& curve, std::size_t K, double alpha, double eps)
    : curve_(curve), K_(K), alpha_(alpha), eps_(eps) {}

    void start() {
        indicators::show_console_cursor(false);
        bar_ = std::make_unique<indicators::ProgressBar>(
            indicators::option::BarWidth{50},
            indicators::option::Start{"["},
            indicators::option::Fill{"="},
            indicators::option::Lead{">"},
            indicators::option::Remainder{" "},
            indicators::option::End{"]"},
            indicators::option::ForegroundColor{indicators::Color::green},
            indicators::option::ShowElapsedTime{true},
            indicators::option::ShowRemainingTime{false},
            indicators::option::MaxProgress{1}
        );

        stop_flag_.store(false, std::memory_order_relaxed);
        th_ = std::thread([&](){
            using namespace std::chrono_literals;
            int lastBarWidth = -1;
            std::string lastPostfix;
            while (!stop_flag_.load(std::memory_order_relaxed)) {
                uint64_t n = curve_.count_runs[0].load(std::memory_order_relaxed);
                double w = cs::halfwidth_anytime_hoeffding(n, K_, alpha_, 1.0);
                bool done = cs::should_stop(n, K_, alpha_, eps_, 1.0);

                std::ostringstream oss;
                oss << "n=" << n
                    << "  2w=" << fmt_sci_short(2.0*w,2)
                    << "  ε="  << fmt_sci_short(eps_,2)
                    << "  α="  << fmt_sci_short(alpha_,2);
                std::string postfix = oss.str();

                if (done) {
                    bar_->set_progress(1);
                    break;
                }

                bool resized = false;
#if !defined(_WIN32)
                if (terminal_resized_flag().load(std::memory_order_relaxed)) {
                    terminal_resized_flag().store(false, std::memory_order_relaxed); resized = true;
                }
#endif
                const int cols = get_terminal_width();
                const int overhead = 18;
                const int minBarW = 10, maxBarW = 60;
                int barW = 50;
                if (cols > 0) {
                    int reservedPostfix = 25;
                    barW = std::min(barW, std::max(minBarW, cols - overhead - reservedPostfix));
                    barW = std::clamp(barW, minBarW, maxBarW);
                }
                int available = (cols > 0 ? cols - overhead - barW : 80 - overhead - barW);
                if (available < 0) available = 0;
                postfix = trim_fit(postfix, static_cast<std::size_t>(available));

                if (resized || barW != lastBarWidth) { bar_->set_option(indicators::option::BarWidth{barW}); lastBarWidth = barW; }
                if (resized || postfix != lastPostfix) { bar_->set_option(indicators::option::PostfixText{postfix}); lastPostfix = postfix; }

                std::this_thread::sleep_for(100ms);
            }
            bar_->mark_as_completed();
            indicators::show_console_cursor(true);
        });
    }

    void stop() {
        stop_flag_.store(true, std::memory_order_relaxed);
        if (th_.joinable()) th_.join();
        bar_.reset();
    }

private:
    CostAggregator& curve_;
    std::size_t K_;
    double alpha_, eps_;

    std::unique_ptr<indicators::ProgressBar> bar_;
    std::atomic<bool> stop_flag_{false};
    std::thread th_;
};
