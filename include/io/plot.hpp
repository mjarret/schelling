// io/plot.hpp — minimal plotting for sim::Heatmap (no external deps)
#pragma once
#include <cstdint>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>
#include <optional>
#include <cstdio>

// Matplot++ (optional): opt-in via -DIO_USE_MATPLOT and availability check.
#if defined(IO_USE_MATPLOT) && __has_include(<matplot/matplot.h>)
#  include <matplot/matplot.h>
#  define IO_HAVE_MATPLOT 1
#else
#  define IO_HAVE_MATPLOT 0
#endif

#include "sim/job_handler.hpp" // for sim::Heatmap (bins, rows, data)

namespace io {

// -------------------- Color utilities --------------------
struct RGB { std::uint8_t r, g, b; };

inline RGB lerp(RGB a, RGB b, double t) {
    if (t < 0.0) t = 0.0; else if (t > 1.0) t = 1.0;
    auto mix = [t](double x, double y) -> std::uint8_t {
        return static_cast<std::uint8_t>(std::lround(x + (y - x) * t));
    };
    return { mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b) };
}

// Simple perceptual-ish multi-stop colormap (black → purple → red → orange → yellow → white)
inline RGB colormap(double t) {
    static const RGB stops[] = {
        { 0, 0, 0 },       // black
        { 59,  0, 76 },    // purple
        {180,  0,  0 },    // red
        {255,128,  0 },    // orange
        {255,230,  0 },    // yellow
        {255,255,255 }     // white
    };
    constexpr int N = sizeof(stops)/sizeof(stops[0]);
    if (t <= 0.0) return stops[0];
    if (t >= 1.0) return stops[N-1];
    const double pos = t * (N - 1);
    const int i = static_cast<int>(std::floor(pos));
    const double f = pos - i;
    return lerp(stops[i], stops[i + 1], f);
}

// -------------------- PPM writer --------------------
// Writes a heatmap image to a PPM (P6) file. Scale enlarges each cell by 'scale' pixels.
// log_scale emphasizes low counts: t = log1p(v) / log1p(max).
inline bool write_heatmap_ppm(const sim::Heatmap& hm,
                              const std::filesystem::path& out_path,
                              int scale = 2,
                              bool log_scale = true)
{
    if (hm.rows == 0 || hm.bins == 0) {
        std::cerr << "write_heatmap_ppm: empty heatmap.\n";
        return false;
    }
    if (scale < 1) scale = 1;

    // Find max count for normalization
    const std::uint64_t maxv = *std::max_element(hm.data.begin(), hm.data.end());
    const double denom = log_scale ? std::log1p(static_cast<double>(maxv))
                                   : static_cast<double>(maxv ? maxv : 1);

    const int width  = static_cast<int>(hm.bins * static_cast<std::size_t>(scale));
    const int height = static_cast<int>(hm.rows * static_cast<std::size_t>(scale));

    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        std::cerr << "write_heatmap_ppm: cannot open " << out_path.string() << " for writing.\n";
        return false;
    }

    // PPM header (binary, maxval 255)
    out << "P6\n" << width << " " << height << "\n255\n";

    // Buffer for one scaled row of pixels
    std::vector<std::uint8_t> rowbuf(static_cast<std::size_t>(width) * 3);

    for (std::size_t r = 0; r < hm.rows; ++r) {
        // Build the pixel row for step r
        const std::uint64_t* row = hm.data.data() + r * hm.bins;
        // Fill rowbuf once, then write it 'scale' times
        std::size_t idx = 0;
        for (std::size_t b = 0; b < hm.bins; ++b) {
            const std::uint64_t v = row[b];
            double t;
            if (maxv == 0) t = 0.0;
            else if (log_scale) t = std::log1p(static_cast<double>(v)) / (denom > 0.0 ? denom : 1.0);
            else t = static_cast<double>(v) / (denom > 0.0 ? denom : 1.0);

            const RGB C = colormap(t);

            // replicate horizontally 'scale' times
            for (int sx = 0; sx < scale; ++sx) {
                rowbuf[idx++] = C.r;
                rowbuf[idx++] = C.g;
                rowbuf[idx++] = C.b;
            }
        }

        // replicate vertically 'scale' times
        for (int sy = 0; sy < scale; ++sy) {
            out.write(reinterpret_cast<const char*>(rowbuf.data()),
                      static_cast<std::streamsize>(rowbuf.size()));
        }
    }
    return true;
}

// -------------------- Matplot++ plot (optional) --------------------
// If Matplot++ is available, render the heatmap using heatmap();
// otherwise provide a stub that returns false.
#if IO_HAVE_MATPLOT
inline bool plot_heatmap_matplot(const sim::Heatmap& hm,
                                 bool log_scale = true,
                                 std::optional<std::filesystem::path> save_path = std::nullopt) {
    if (hm.rows == 0 || hm.bins == 0) {
        std::cerr << "plot_heatmap_matplot: empty heatmap.\n";
        return false;
    }
    try {
        const std::uint64_t maxv = *std::max_element(hm.data.begin(), hm.data.end());
        const double base = log_scale ? 1.0 : 0.0; // marker to choose transform

        std::vector<std::vector<double>> Z(hm.rows, std::vector<double>(hm.bins, 0.0));
        for (std::size_t r = 0; r < hm.rows; ++r) {
            const std::uint64_t* row = hm.data.data() + r * hm.bins;
            for (std::size_t c = 0; c < hm.bins; ++c) {
                const double v = static_cast<double>(row[c]);
                Z[r][c] = (log_scale ? std::log1p(v) : v);
            }
        }

        auto f = ::matplot::figure(true);
        auto h = ::matplot::heatmap(Z);
        ::matplot::colormap(::matplot::palette::hot());
        ::matplot::colorbar();
        ::matplot::title(std::string("Heatmap (") + (log_scale ? "log1p" : "linear") + ")");

        if (save_path.has_value()) {
            ::matplot::save(save_path->string());
        } else {
            ::matplot::show();
        }
        return true;
    } catch (...) {
        return false;
    }
}
#else
inline bool plot_heatmap_matplot(const sim::Heatmap&, bool = true,
                                 std::optional<std::filesystem::path> = std::nullopt) {
    // Matplot++ not enabled; silently no-op and report false.
    return false;
}
#endif

} // namespace io

// -------------------- Gnuplot heatmap (no external C++ libs) --------------------
// Renders the given CSV matrix using gnuplot, labeling axes. Requires `gnuplot`
// available on PATH. Returns true on success (script sent); false on failure.
namespace io {

inline bool plot_heatmap_gnuplot(const sim::Heatmap& hm,
                                 const std::filesystem::path& /*csv_path_unused*/,
                                 const std::filesystem::path& png_out = std::filesystem::path{"heatmap.png"},
                                 int scale = 2,
                                 std::string_view title = "Schelling Heatmap",
                                 std::string_view xlabel = "Step",
                                 std::string_view ylabel = "Unhappy Count",
                                 bool invert_y = false) {
    if (hm.rows == 0 || hm.bins == 0) return false;
    if (scale < 1) scale = 1;
    // We want x = steps (columns), y = unhappy count (rows).
    // Trim to maximal non-zero extents to avoid empty margins.
    std::size_t xmax = 0, ymax = 0; bool any = false;
    for (std::size_t x = 0; x < hm.rows; ++x) {
        const std::uint64_t* row = hm.data.data() + x * hm.bins;
        for (std::size_t y = 0; y < hm.bins; ++y) {
            if (row[y] != 0ull) { any = true; if (x > xmax) xmax = x; if (y > ymax) ymax = y; }
        }
    }
    if (!any) { xmax = (hm.rows ? hm.rows - 1 : 0); ymax = (hm.bins ? hm.bins - 1 : 0); }
    const int width  = static_cast<int>((xmax + 1) * static_cast<std::size_t>(scale));
    const int height = static_cast<int>((ymax + 1) * static_cast<std::size_t>(scale));

    auto sanitize = [](std::string_view s) {
        std::string out; out.reserve(s.size());
        for (char ch : s) out.push_back(ch == '\'' ? '"' : ch);
        return out;
    };

    FILE* gp = ::popen("GNUTERM=pngcairo gnuplot", "w");
    if (!gp) return false;

    std::string t = sanitize(title);
    std::string xl = sanitize(xlabel);
    std::string yl = sanitize(ylabel);

    // Use matrix mode to interpret CSV rows as a grid. Flip Y for steps increasing downward.
    std::fprintf(gp, "set term pngcairo size %d,%d\n", width, height);
    std::fprintf(gp, "set output '%s'\n", png_out.string().c_str());
    std::fprintf(gp, "set title '%s'\n", t.c_str());
    std::fprintf(gp, "set xlabel '%s'\n", xl.c_str());
    std::fprintf(gp, "set ylabel '%s'\n", yl.c_str());
    std::fprintf(gp, "unset key\n");
    // Avoid empty range warnings (when size == 1) by setting explicit ranges to used extents
    const std::size_t xr = (xmax > 0 ? xmax : 1);
    const std::size_t yr = (ymax > 0 ? ymax : 1);
    std::fprintf(gp, "set xrange [0:%zu]\n", xr);
    if (invert_y) std::fprintf(gp, "set yrange [0:%zu] reverse\n", yr);
    else          std::fprintf(gp, "set yrange [0:%zu]\n", yr);
    std::fprintf(gp, "unset multiplot\n");
    // Stream a transposed matrix directly to gnuplot so that columns=steps, rows=unhappy
    std::fprintf(gp, "plot '-' matrix with image\n");
    for (std::size_t y = 0; y <= ymax; ++y) {
        for (std::size_t x = 0; x <= xmax; ++x) {
            const std::uint64_t v = hm.data[x * hm.bins + y];
            std::fprintf(gp, "%llu%s", static_cast<unsigned long long>(v), (x == xmax) ? "\n" : " ");
        }
    }
    std::fprintf(gp, "e\n");
    std::fprintf(gp, "unset output\n");

    ::pclose(gp);
    return true;
}

} // namespace io
