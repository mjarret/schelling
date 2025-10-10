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

    std::string tau_s;      // p/q or decimal
    double agent_density_val = 0.0; // set when flag present

    cxxopts::Options desc("lollipop", "Schelling Lollipop Options");
    // register with defaults where applicable (cxxopts API: spec, desc, value)
    desc.add_options()
        ("h,help", "Show this help")
        ("t,tau", "Threshold tau as p/q or decimal (double/float)", cxxopts::value<std::string>(tau_s))
        ("p", "Numerator p for tau=p/q", cxxopts::value<std::uint64_t>(opt.p)->default_value("1"))
        ("q", "Denominator q for tau=p/q (nonzero)", cxxopts::value<std::uint64_t>(opt.q)->default_value("2"))
        ("clique-size", "Clique size (compiled combos)", cxxopts::value<std::size_t>(opt.clique_size)->default_value("50"))
        ("path-length", "Path length (compiled combos)", cxxopts::value<std::size_t>(opt.path_length)->default_value("450"))
        ("d,agent-density", "Agent density in [0,1] (TODO: unused)", cxxopts::value<double>(agent_density_val))
    ;
    help_text = desc.help();
    auto result = desc.parse(argc, argv);
    if (result.count("help")) { want_help = true; return opt; }

    // Apply p/q overrides if explicitly provided (not defaults)
    if (result.count("p")) { opt.pq_overridden = true; }
    if (result.count("q")) { opt.pq_overridden = true; }

    // Parse tau string if provided (unless p/q explicitly override)
    if (!tau_s.empty() && !opt.pq_overridden) {
        if (auto pq = parse_pq(std::string_view(tau_s))) {
            opt.p = pq->first; opt.q = pq->second; opt.pq_overridden = true;
        } else {
            if (!tau_s.empty() && (tau_s.back()=='f' || tau_s.back()=='F')) tau_s.pop_back();
            char* endp = nullptr;
            double tau = std::strtod(tau_s.c_str(), &endp);
            if (endp && *endp == '\0') {
                if (tau <= 0.0) { opt.p = 0; opt.q = 1; }
                else if (tau >= 1.0) { opt.p = 1; opt.q = 1; }
                else { opt.q = 1000000ULL; opt.p = static_cast<std::uint64_t>(std::llround(tau * static_cast<double>(opt.q))); }
                if (opt.q == 0) opt.q = 1;
                opt.pq_overridden = true;
            } else {
                std::cerr << "Invalid --tau value; expected p/q or decimal.\n";
                want_help = true;
                return opt;
            }
        }
    }

    // Agent density flag (no validation, stored for later use)
    if (result.count("agent-density")) { opt.agent_density = agent_density_val; }

    return opt;
}

} // namespace cli
