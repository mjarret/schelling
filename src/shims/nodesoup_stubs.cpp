// Minimal stubs to satisfy Matplot++ static library references to nodesoup
// Used only for linking; implementations are simplistic and sufficient for
// non-network plots (e.g., heatmap) where these are not actually invoked.

#include <vector>
#include <functional>
#include <cmath>

namespace nodesoup {

struct Point2D { double x; double y; };
using adj_list_t = std::vector<std::vector<unsigned long>>;
using iter_callback_t = std::function<void(const std::vector<Point2D>&, int)>;

std::vector<double> size_radiuses(const adj_list_t& g, double /*min_r*/ = 0.0, double /*max_r*/ = 0.0) {
    std::vector<double> r; r.resize(g.size(), 1.0);
    return r;
}

std::vector<Point2D> fruchterman_reingold(const adj_list_t& g,
                                          unsigned int width,
                                          unsigned int height,
                                          unsigned int /*iters*/,
                                          double /*k*/,
                                          iter_callback_t /*cb*/) {
    std::vector<Point2D> pos;
    const std::size_t n = g.size();
    pos.reserve(n);
    // Place nodes on a circle as a harmless fallback
    const double cx = static_cast<double>(width) / 2.0;
    const double cy = static_cast<double>(height) / 2.0;
    const double rad = std::min(cx, cy) * 0.9;
    for (std::size_t i = 0; i < n; ++i) {
        double theta = (n ? (2.0 * M_PI * static_cast<double>(i) / static_cast<double>(n)) : 0.0);
        pos.push_back(Point2D{cx + rad * std::cos(theta), cy + rad * std::sin(theta)});
    }
    return pos;
}

std::vector<Point2D> kamada_kawai(const adj_list_t& g,
                                  unsigned int width,
                                  unsigned int height,
                                  double /*k*/,
                                  double /*energy_threshold*/) {
    // Reuse the same simple circle placement
    return fruchterman_reingold(g, width, height, 0, 0.0, nullptr);
}

} // namespace nodesoup

