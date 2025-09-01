#pragma once
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>

struct CLIOptions {
    std::string graph;     // "torus" or "lollipop"
    std::string size;      // "WxH" or "m:L"
    double density{0.7};   // 0..1
    double threshold{0.5}; // 0..1
    double eps{0.02};      // >0
    double alpha{1e-4};    // (0,1)
    std::uint64_t seed{0}; // 0 => random device
};

inline CLIOptions parse_cli(int argc, char** argv) {
    CLIOptions o{};
    for (int i=1;i<argc;++i) {
        std::string a = argv[i];
        auto need = [&](const char* name){ if (i+1>=argc) throw std::runtime_error(std::string("Missing value for ")+name); return std::string(argv[++i]); };
        if (a=="--graph")      o.graph = need("--graph");
        else if (a=="--size")  o.size = need("--size");
        else if (a=="--density")   o.density = std::stod(need("--density"));
        else if (a=="--threshold") o.threshold = std::stod(need("--threshold"));
        else if (a=="--eps")       o.eps = std::stod(need("--eps"));
        else if (a=="--alpha")     o.alpha = std::stod(need("--alpha"));
        else if (a=="--seed")      o.seed = std::stoull(need("--seed"));
        else if (a=="--help" || a=="-h") {
            throw std::runtime_error(
                "Usage: schelling --graph {torus|lollipop} --size WxH|m:L "
                "--density ρ --threshold τ --eps ε --alpha α [--seed S]");
        } else {
            throw std::runtime_error("Unknown arg: "+a);
        }
    }
    if (o.graph.empty() || o.size.empty())
        throw std::runtime_error("Required: --graph and --size");
    if (!(o.density > 0.0 && o.density < 1.0))
        throw std::runtime_error("density must be in (0,1)");
    if (!(o.threshold >= 0.0 && o.threshold <= 1.0))
        throw std::runtime_error("threshold must be in [0,1]");
    if (!(o.eps > 0.0)) throw std::runtime_error("eps must be > 0");
    if (!(o.alpha > 0.0 && o.alpha < 1.0)) throw std::runtime_error("alpha in (0,1)");
    return o;
}
