#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <limits>
#include <algorithm>
#include "live_plot.hpp"
#include "cost_aggregator.hpp"
#include "cs_stop.hpp"

class PlotManager {
public:
    PlotManager(bool enabled,
                const std::vector<uint64_t>& cps,
                CostAggregator& curve,
                double eps, double alpha)
    : enabled_(enabled), cps_(cps), curve_(curve), eps_(eps), alpha_(alpha) {}

    void start() {
        if (!enabled_) return;
        stop_flag_.store(false, std::memory_order_relaxed);
        th_ = std::thread([&](){
            GnuplotLive plotFull("Mean Unhappy Fraction (full)");
            GnuplotLive plotZoom("Mean Unhappy Fraction (zoom)");
            if (!plotFull.valid()) {
                std::cerr << "\n[warn] gnuplot not found. Live plot disabled.\n";
                return;
            }
            plotFull.cmd("set ylabel 'Mean Unhappy Fraction'");
            plotFull.set_yrange(0.0, 1.0);
            if (plotZoom.valid()) {
                plotZoom.cmd("set ylabel 'Mean Unhappy Fraction (zoom)'");
                plotZoom.set_yrange(0.0, 1.0);
            }

            const auto tick = std::chrono::milliseconds(250);
            int paintTick = 0;

            while (!stop_flag_.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(tick);

                const size_t M = cps_.size();
                std::vector<double> xs; xs.reserve(M);
                std::vector<double> ys; ys.reserve(M);

                size_t lastIdxWithData = 0;
                for (size_t k=0; k<M; ++k) {
                    uint64_t cnt = curve_.count_runs[k].load(std::memory_order_relaxed);
                    if (cnt == 0) continue;
                    uint64_t sum = curve_.sum_frac_scaled[k].load(std::memory_order_relaxed);
                    double y = double(sum) / double(cnt) / double(CostAggregator::SCALE);
                    xs.push_back(double(cps_[k]));
                    ys.push_back(std::clamp(y, 0.0, 1.0));
                    lastIdxWithData = k;
                }
                if (xs.empty()) continue;

                plotFull.cmd("unset label");
                plotZoom.cmd("unset label");

                uint64_t n = curve_.count_runs[0].load(std::memory_order_relaxed);
                double w = cs::halfwidth_anytime_hoeffding(n, cps_.size(), alpha_, 1.0);
                std::ostringstream lbl;
                lbl << "set label 1 at graph 0.02,0.95 'CS: n=" << n
                    << "  2w=" << std::setprecision(6) << (2.0*w)
                    << "  eps=" << std::setprecision(6) << eps_
                    << "  alpha=" << std::setprecision(6) << alpha_ << "' front";
                plotFull.cmd(lbl.str());
                plotZoom.cmd(lbl.str());

                double xmaxObserved = double(cps_[lastIdxWithData]);
                if (xmaxObserved < 1.0) xmaxObserved = 1.0;
                plotFull.set_xrange(0.0, xmaxObserved);
                plotFull.plot_lines(xs, ys, "mean(U/N)");

                if (plotZoom.valid()) {
                    if ((paintTick % 8) == 0) {
                        double y0 = ys.front();
                        double thresh = std::max(2.0 * eps_, 0.02 * y0);
                        size_t lastAbove = 0;
                        for (size_t i=0; i<ys.size(); ++i) if (ys[i] > thresh) lastAbove = i;
                        size_t idx = std::min(lastAbove + 5, ys.size()-1);
                        double xmaxZoom = std::max(1.0, xs[idx]);
                        plotZoom.set_xrange(0.0, xmaxZoom);
                    }
                    plotZoom.plot_lines(xs, ys, "mean(U/N) (zoom)");
                }
                ++paintTick;
            }
        });
    }

    void stop() {
        if (!enabled_) return;
        stop_flag_.store(true, std::memory_order_relaxed);
        if (th_.joinable()) th_.join();
    }

private:
    bool enabled_;
    const std::vector<uint64_t>& cps_;
    CostAggregator& curve_;
    double eps_, alpha_;

    std::atomic<bool> stop_flag_{false};
    std::thread th_;
};
