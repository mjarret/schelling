#pragma once
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
        cmd("set term qt enhanced");
        cmd(std::string("set title '") + escape(title) + "'");
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
        if (line.size() && line.back() != '\n') line.push_back('\n');
        std::fputs(line.c_str(), gp_);
        std::fflush(gp_);
    }

    void set_xrange(double a, double b) { cmd("set xrange [" + to_str(a) + ":" + to_str(b) + "]"); }
    void set_yrange(double a, double b) { cmd("set yrange [" + to_str(a) + ":" + to_str(b) + "]"); }

    void plot_lines(const std::vector<double>& x, const std::vector<double>& y, const std::string& title) {
        if (!valid_) return;
        std::fprintf(gp_, "plot '-' with lines lw 2 title '%s'\n", escape(title).c_str());
        for (std::size_t i=0;i<x.size() && i<y.size();++i)
            std::fprintf(gp_, "%.*g %.*g\n", 16, x[i], 16, y[i]);
        std::fprintf(gp_, "e\n");
        std::fflush(gp_);
    }

    void plot_points(const std::vector<double>& x, const std::vector<double>& y,
                     const std::string& title, double point_size=0.25) {
        if (!valid_) return;
        std::fprintf(gp_, "plot '-' with points pt 7 ps %.*g title '%s'\n",
                     16, point_size, escape(title).c_str());
        for (std::size_t i=0;i<x.size() && i<y.size();++i)
            std::fprintf(gp_, "%.*g %.*g\n", 16, x[i], 16, y[i]);
        std::fprintf(gp_, "e\n");
        std::fflush(gp_);
    }

    void plot_points_and_lines(const std::vector<double>& xpts, const std::vector<double>& ypts,
                               const std::vector<double>& xline, const std::vector<double>& yline,
                               const std::string& points_title,
                               const std::string& line_title,
                               double point_size=0.25) {
        if (!valid_) return;
        std::fprintf(gp_, "plot '-' with points pt 7 ps %.*g title '%s', "
                          "'-' with lines lw 2 title '%s'\n",
                     16, point_size, escape(points_title).c_str(),
                     escape(line_title).c_str());
        for (std::size_t i=0;i<xpts.size() && i<ypts.size();++i)
            std::fprintf(gp_, "%.*g %.*g\n", 16, xpts[i], 16, ypts[i]);
        std::fprintf(gp_, "e\n");
        for (std::size_t i=0;i<xline.size() && i<yline.size();++i)
            std::fprintf(gp_, "%.*g %.*g\n", 16, xline[i], 16, yline[i]);
        std::fprintf(gp_, "e\n");
        std::fflush(gp_);
    }

    // Variable-width histogram: using columns (xmid, count, width).
    void plot_histogram_variable(const std::vector<double>& xmid,
                                 const std::vector<double>& count,
                                 const std::vector<double>& w,
                                 const std::string& title)
    {
        if (!valid_) return;
        cmd("set style fill solid 0.8");
        std::fprintf(gp_, "plot '-' using 1:2:3 with boxes lc rgb '#4E79A7' title '%s'\n",
                     escape(title).c_str());
        const std::size_t m = std::min({xmid.size(), count.size(), w.size()});
        for (std::size_t i=0;i<m;++i)
            std::fprintf(gp_, "%.*g %.*g %.*g\n", 16, xmid[i], 16, count[i], 16, w[i]);
        std::fprintf(gp_, "e\n");
        std::fflush(gp_);
    }

private:
    static std::string escape(const std::string& s) {
        std::string t; t.reserve(s.size()*2);
        for (char c: s) { if (c=='\'' || c=='\\') t.push_back('\\'); t.push_back(c); }
        return t;
    }
    static std::string to_str(double v) {
        std::ostringstream oss; oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
        oss.precision(16); oss << v; return oss.str();
    }

    FILE* gp_{nullptr};
    bool valid_{false};
};
