#pragma once
#include <string>
#include <vector>
#include <sstream>
#include "live_plot.hpp"
#include "kcenter_hist.hpp"

class HistogramPlotter {
public:
    HistogramPlotter(bool enabled, const std::string& out_png = "", const std::string& title = "Moves")
      : enabled_(enabled), out_png_(out_png), title_(title) {}

    // raw stopping times, desired k (0 => auto), render
    void render(const std::vector<uint64_t>& raw_times, std::size_t k_bins) {
        if (!enabled_) return;
        if (raw_times.empty()) { std::cerr << "[note] no converged runs; histogram skipped.\n"; return; }

        std::vector<double> xmid, count, w;
        kcenter1d::histogram_kcenter(raw_times, k_bins, xmid, count, w);

        const double xmin = (xmid.empty()? 0.0 : *std::min_element(xmid.begin(), xmid.end()));
        const double xmax = (xmid.empty()? 1.0 : *std::max_element(xmid.begin(), xmid.end()));
        const double pad  = (xmid.empty()? 1.0 : 0.05 * std::max(1.0, xmax - xmin));

        GnuplotLive gp(title_);
        if (!gp.valid()) {
            std::cerr << "[warn] gnuplot not found; histogram skipped.\n";
            return;
        }
        gp.cmd("set ylabel 'Frequency'");
        gp.cmd("set xlabel 'Moves to settle'");
        gp.set_xrange(std::max(0.0, xmin - pad), xmax + pad);
        gp.plot_histogram_variable(xmid, count, w, title_);

        if (!out_png_.empty()) {
            gp.cmd("set term pngcairo size 1200,600");
            gp.cmd(std::string("set output '") + out_png_ + "'");
            gp.plot_histogram_variable(xmid, count, w, title_);
            gp.cmd("unset output");
            gp.cmd("set term qt enhanced");
        }
    }

private:
    bool        enabled_;
    std::string out_png_;
    std::string title_;
};
