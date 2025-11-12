// cli.cpp — Command-line parsing implementation using cxxopts

#include "cli/cli.hpp"

#include <cxxopts.hpp>
#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>
#include <iostream>
#include <cmath>
#include <optional>

namespace cli {
static inline std::optional<std::pair<std::uint64_t,std::uint64_t>> parse_pq(std::string_view s) {
    auto pos = s.find('/');
    if (pos == std::string_view::npos) return std::nullopt;
    std::string_view sp = s.substr(0, pos);
    std::string_view sq = s.substr(pos + 1);
    std::uint64_t p=0, q=0;
    auto to_u64 = [](std::string_view x, std::uint64_t& out)->bool{
        const char* b = x.data();
        const char* e = b + x.size();
        auto res = std::from_chars(b, e, out); return res.ec == std::errc{} && res.ptr == e;
    };
    if (!to_u64(sp, p) || !to_u64(sq, q) || q == 0) return std::nullopt;
    return std::make_pair(p,q);
}


Options parse_args(int argc, char** argv, bool& want_help, std::string& help_text) {
    Options opt;
    want_help = false;

    std::string tau_s;        // p/q or decimal
    std::string density_s;    // p/q or decimal for agent density
    double agent_density_val = 0.8; // final parsed value
    std::size_t max_steps_val = 0;  // if present -> set; absent -> ∞

    cxxopts::Options desc("lollipop", "Schelling Lollipop Options");
    // register with defaults where applicable (cxxopts API: spec, desc, value)
    desc.add_options()
        ("h,help", "Show this help")
        ("t,tau", "Threshold tau as p/q or decimal", cxxopts::value<std::string>(tau_s))
        ("clique-size", "Clique size (compiled combos)", cxxopts::value<std::size_t>(opt.clique_size)->default_value("50"))
        ("path-length", "Path length (compiled combos)", cxxopts::value<std::size_t>(opt.path_length)->default_value("450"))
        ("d,agent-density", "Agent density in [0,1] as p/q or decimal", cxxopts::value<std::string>(density_s)->default_value("0.8"))
        ("e,experiments", "Number of experiments (default 1000)", cxxopts::value<std::size_t>(opt.experiments)->default_value("1000"))
        ("threads", "Number of threads (default: OMP_NUM_THREADS or max)", cxxopts::value<int>(opt.threads)->default_value("0"))
        ("m,max-steps", "Maximum steps per experiment (default ∞)", cxxopts::value<std::size_t>(max_steps_val))
    ;
    help_text = desc.help();
    auto result = desc.parse(argc, argv);
    if (result.count("help")) { want_help = true; return opt; }

    // Parse tau: accept p/q or decimal; default to 1/2 if not provided
    if (!tau_s.empty()) {
        if (auto pq = parse_pq(std::string_view(tau_s))) {
            opt.p = pq->first; opt.q = pq->second;
        } else {
            if (!tau_s.empty() && (tau_s.back()=='f' || tau_s.back()=='F')) tau_s.pop_back();
            char* endp = nullptr;
            double tau = std::strtod(tau_s.c_str(), &endp);
            if (endp && *endp == '\0') {
                if (tau <= 0.0) { opt.p = 0; opt.q = 1; }
                else if (tau >= 1.0) { opt.p = 1; opt.q = 1; }
                else { opt.q = 1000000ULL; opt.p = static_cast<std::uint64_t>(std::llround(tau * static_cast<double>(opt.q))); }
                if (opt.q == 0) opt.q = 1;
            } else {
                std::cerr << "Invalid --tau value; expected p/q or decimal.\n";
                want_help = true;
                return opt;
            }
        }
    }

    // Parse agent density: accept p/q or decimal; clamp to [0,1]
    if (!density_s.empty()) {
        if (auto pq = parse_pq(std::string_view(density_s))) {
            // convert to double safely
            agent_density_val = (pq->second == 0) ? 0.0 : static_cast<double>(pq->first) / static_cast<double>(pq->second);
        } else {
            char* endp = nullptr;
            double d = std::strtod(density_s.c_str(), &endp);
            if (endp && *endp == '\0') {
                agent_density_val = d;
            } else {
                std::cerr << "Invalid --agent-density; expected p/q or decimal.\n";
                want_help = true;
                return opt;
            }
        }
    }
    if (agent_density_val < 0.0) agent_density_val = 0.0;
    if (agent_density_val > 1.0) agent_density_val = 1.0;
    opt.agent_density = agent_density_val;

    // Optional max steps: set only if provided
    if (result.count("max-steps")) {
        opt.max_steps = max_steps_val;
    }

    return opt;
}

} // namespace cli
