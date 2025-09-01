#pragma once
/*
GnuplotLive — minimal, robust pipe wrapper with points, lines, and combined plot.

CHANGES (2025-09-01):
- Added plot_lines(...) and plot_points_and_lines(...) so we can render scatter
  and a fitted polyline in a SINGLE frame. This prevents the “alternating” effect
  you were seeing when issuing two separate plot commands back-to-back.
- Each method snapshots lengths, checks for empties, and sends exactly as many
  data blocks ('-') as promised in the header. Any write failure disables plotting.
*/

#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <utility>
#include <limits>
#include <cmath>
#include <algorithm>

class GnuplotLive {
public:
    explicit GnuplotLive(const std::string& title) {
#if defined(_WIN32)
        gp_ = _popen("gnuplot -persist", "w");
#else
        gp_ = popen("gnuplot -persist", "w");
#endif
        if (!gp_) { valid_ = false; return; }
        valid_ = true;
        // Default to qt if available; switch to another terminal in your app if desired.
        cmd("set term qt enhanced");
        cmd(std::string("set title '") + escape_(title) + "'");
        cmd("set grid");
    }

    ~GnuplotLive() {
        if (gp_) {
#if defined(_WIN32)
            _pclose(gp_);
#else
            pclose(gp_);
#endif
            gp_ = nullptr;
        }
    }

    bool valid() const { return valid_; }

    void cmd(const std::string& s) {
        if (!valid_) return;
        std::string line = s;
        if (!line.empty() && line.back() != '\n') line.push_back('\n');
        if (std::fputs(line.c_str(), gp_) < 0) { disable_(); return; }
        if (std::fflush(gp_) != 0) { disable_(); return; }
    }

    // Optional manual range setters
    void set_xrange(double a, double b) { cmd("set xrange [" + to_str_(a) + ":" + to_str_(b) + "]"); }
    void set_yrange(double a, double b) { cmd("set yrange [" + to_str_(a) + ":" + to_str_(b) + "]"); }

    // Simple polyline (x,y)
    void plot_lines(const std::vector<double>& x,
                    const std::vector<double>& y,
                    const std::string& title) {
        if (!valid_) return;
        const std::size_t n = std::min(x.size(), y.size());
        if (n == 0) return; // nothing to plot

        if (std::fprintf(gp_, "plot '-' with lines lw 2 title '%s'\n",
                         escape_(title).c_str()) < 0) { disable_(); return; }
        for (std::size_t i = 0; i < n; ++i) {
            if (std::fprintf(gp_, "%.*g %.*g\n", 16, x[i], 16, y[i]) < 0) { disable_(); return; }
        }
        if (std::fputs("e\n", gp_) < 0) { disable_(); return; }
        if (std::fflush(gp_) != 0) { disable_(); return; }
    }

    // Simple points (x,y)
    void plot_points(const std::vector<double>& x,
                     const std::vector<double>& y,
                     const std::string& title,
                     double point_size = 0.25) {
        if (!valid_) return;
        const std::size_t n = std::min(x.size(), y.size());
        if (n == 0) return; // nothing to plot

        if (std::fprintf(gp_, "plot '-' with points pt 7 ps %.*g title '%s'\n",
                         16, point_size, escape_(title).c_str()) < 0) { disable_(); return; }
        for (std::size_t i = 0; i < n; ++i) {
            if (std::fprintf(gp_, "%.*g %.*g\n", 16, x[i], 16, y[i]) < 0) { disable_(); return; }
        }
        if (std::fputs("e\n", gp_) < 0) { disable_(); return; }
        if (std::fflush(gp_) != 0) { disable_(); return; }
    }

    // Combined: points + lines in ONE frame (prevents alternating plot effect).
    void plot_points_and_lines(const std::vector<double>& xpts, const std::vector<double>& ypts,
                               const std::vector<double>& xline, const std::vector<double>& yline,
                               const std::string& points_title,
                               const std::string& line_title,
                               double point_size = 0.25) {
        if (!valid_) return;

        const std::size_t np = std::min(xpts.size(), ypts.size());
        const std::size_t nl = std::min(xline.size(), yline.size());

        if (np == 0 && nl == 0) return; // nothing to draw

        if (np > 0 && nl > 0) {
            // Both datasets: header declares two '-' blocks
            if (std::fprintf(gp_,
                "plot '-' with points pt 7 ps %.*g title '%s', "
                "'-' with lines lw 2 title '%s'\n",
                16, point_size, escape_(points_title).c_str(),
                escape_(line_title).c_str()) < 0) { disable_(); return; }

            // dataset 1 (points)
            for (std::size_t i = 0; i < np; ++i) {
                if (std::fprintf(gp_, "%.*g %.*g\n", 16, xpts[i], 16, ypts[i]) < 0) { disable_(); return; }
            }
            if (std::fputs("e\n", gp_) < 0) { disable_(); return; }

            // dataset 2 (line)
            for (std::size_t i = 0; i < nl; ++i) {
                if (std::fprintf(gp_, "%.*g %.*g\n", 16, xline[i], 16, yline[i]) < 0) { disable_(); return; }
            }
            if (std::fputs("e\n", gp_) < 0) { disable_(); return; }

            if (std::fflush(gp_) != 0) { disable_(); return; }
            return;
        }

        // Fallback: only one dataset available
        if (np > 0) {
            plot_points(xpts, ypts, points_title, point_size);
        } else {
            plot_lines(xline, yline, line_title);
        }
    }

private:
    static std::string escape_(const std::string& s) {
        std::string t; t.reserve(s.size()*2);
        for (char c: s) { if (c=='\'' || c=='\\') t.push_back('\\'); t.push_back(c); }
        return t;
    }
    static std::string to_str_(double v) {
        std::ostringstream oss; oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
        oss.precision(16); oss << v; return oss.str();
    }
    void disable_() { valid_ = false; }

    FILE* gp_{nullptr};
    bool valid_{false};
};
