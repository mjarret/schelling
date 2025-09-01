#pragma once
#include <string>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <cstdlib>

#include "move_rule.hpp"

// Graph selection
enum class GraphKind { TORUS, LOLLIPOP };

struct CLIOptions {
    // Required conceptual inputs (some may be filled from config)
    GraphKind graph = GraphKind::TORUS;
    uint32_t dimA = 0, dimB = 0;       // torus: W,H  | lollipop: m,n
    MoveRule  move = MoveRule::Any;
    double density = 0.0;
    double threshold = 0.0;            // used if move==FirstAccepting

    // Anytime CS stopping (the ONLY rule now)
    double alpha = 1e-4;               // familywise error
    double eps   = 5e-4;               // curve sup-norm tolerance

    // Optional knobs
    int threads = 0;                   // 0 => auto (leave 1–2 free)
    bool plot_enabled = true;
    uint32_t k = 32;                   // only for move==FirstAccepting
    uint64_t seed = 0;                 // 0 => auto
    bool debug = false;
    uint32_t debug_every = 10;

    // parser result
    int exit_code = 0;
};

// --------- helpers used here and in main.cpp ----------
static inline char _lower_ascii(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

static inline bool _to_bool(const std::string& s, bool& v) {
    std::string t; t.reserve(s.size());
    for (char c: s) t.push_back(_lower_ascii(c));
    if (t=="1"||t=="true"||t=="yes"||t=="y"||t=="on")  { v = true;  return true; }
    if (t=="0"||t=="false"||t=="no" ||t=="n"||t=="off"){ v = false; return true; }
    return false;
}

static inline bool _get_opt(int argc, char** argv, const std::string& key, std::string& out) {
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == key && i + 1 < argc) { out = argv[i+1]; return true; }
        auto p = s.find('=');
        if (p != std::string::npos && s.substr(0, p) == key) { out = s.substr(p+1); return true; }
    }
    return false;
}
static inline bool _has_flag(int argc, char** argv, const std::string& key) {
    for (int i = 1; i < argc; ++i) if (key == argv[i]) return true;
    return false;
}

// Exposed: peek just for --config
static inline bool cli_peek_option(int argc, char** argv, const std::string& key, std::string& out) {
    return _get_opt(argc, argv, key, out);
}

// Parse size: torus expects "WxH"; lollipop expects "m:n"
static inline bool _parse_size(const std::string& s, uint32_t& a, uint32_t& b, bool is_torus) {
    std::string t = s;
    for (char& c: t) if (c=='X' || c=='x' || c=='+') c = (is_torus ? 'x' : ':');
    auto p = t.find(is_torus ? 'x' : ':');
    if (p == std::string::npos) return false;
    a = static_cast<uint32_t>(std::stoull(t.substr(0,p)));
    b = static_cast<uint32_t>(std::stoull(t.substr(p+1)));
    return (a>0 && b>0);
}

static inline void _print_help() {
    std::cout <<
R"(Schelling simulator — modular, *only* anytime-valid CS stopping (confidence sequence).

You can provide a config file and override with CLI:
  --config path/to/schelling.cfg

Required (from config or CLI):
  --graph torus|lollipop
  --size   WxH          (if --graph torus, e.g. 256x256)
           m:n          (if --graph lollipop, clique m and path n, e.g. 64:512)
  --move any|first      (any = just move; first = first acceptable up to k tries, else stay)
  --density D           (0..1)
  --threshold T         (0..1) — only used if --move first

Anytime CS options (only rule):
  --eps E               (curve won't move by more than E thereafter, with prob ≥ 1-α; default 5e-4)
  --alpha A             (familywise error, default 1e-4)

Other:
  --threads K           (0=auto; default auto leaves 1–2 cores free)
  --plot on|off         (default on)
  --seed auto|<u64>     (default auto)
  --k N                 (candidate tries per step if --move first; default 32)
  --debug on|off        (extra terminal output; default off)
  --debug-every N       (print every N completed runs; default 10)
)" << std::endl;
}

static inline bool parse_cli(int argc, char** argv, CLIOptions& o) {
    if (_has_flag(argc, argv, "--help") || _has_flag(argc, argv, "-h")) {
        _print_help(); o.exit_code = 0; return false;
    }

    std::string s;

    // config was handled in main; here we only override

    // graph
    if (_get_opt(argc, argv, "--graph", s)) {
        std::string t; for (char c: s) t.push_back(_lower_ascii(c));
        if      (t=="lollipop") o.graph = GraphKind::LOLLIPOP;
        else if (t=="torus")    o.graph = GraphKind::TORUS;
        else { std::cerr << "ERROR: --graph must be 'torus' or 'lollipop'\n"; o.exit_code = 2; return false; }
    }

    // move
    if (_get_opt(argc, argv, "--move", s)) {
        std::string t; for (char c: s) t.push_back(_lower_ascii(c));
        if      (t=="any")   o.move = MoveRule::Any;
        else if (t=="first") o.move = MoveRule::FirstAccepting;
        else { std::cerr << "ERROR: --move must be 'any' or 'first'\n"; o.exit_code = 2; return false; }
    }

    // size
    if (_get_opt(argc, argv, "--size", s)) {
        if (!_parse_size(s, o.dimA, o.dimB, o.graph==GraphKind::TORUS)) {
            std::cerr << "ERROR: --size format invalid for selected graph\n"; o.exit_code = 2; return false;
        }
    }

    // core parameters
    if (_get_opt(argc, argv, "--density", s))   o.density   = std::clamp(std::atof(s.c_str()), 0.0, 1.0);
    if (_get_opt(argc, argv, "--threshold", s)) o.threshold = std::clamp(std::atof(s.c_str()), 0.0, 1.0);

    // CS options
    if (_get_opt(argc, argv, "--alpha", s)) o.alpha = std::max(1e-16, std::atof(s.c_str()));
    if (_get_opt(argc, argv, "--eps",   s)) o.eps   = std::max(0.0, std::atof(s.c_str()));
    // allow legacy --delta as alias for --eps
    if (_get_opt(argc, argv, "--delta", s)) o.eps   = std::max(0.0, std::atof(s.c_str()));

    // optional common
    if (_get_opt(argc, argv, "--threads", s))      o.threads = std::atoi(s.c_str()); // 0 => auto
    if (_get_opt(argc, argv, "--plot", s))         { bool b; if (_to_bool(s,b)) o.plot_enabled=b; }
    if (_get_opt(argc, argv, "--seed", s))         o.seed = (s=="auto"? 0ULL : (uint64_t)std::strtoull(s.c_str(), nullptr, 10));
    if (_get_opt(argc, argv, "--k", s))            o.k = std::max<uint32_t>(1, std::strtoul(s.c_str(), nullptr, 10));
    if (_get_opt(argc, argv, "--debug", s))        { bool b; if (_to_bool(s,b)) o.debug=b; }
    if (_get_opt(argc, argv, "--debug-every", s))  o.debug_every = std::max<uint32_t>(1, std::strtoul(s.c_str(), nullptr, 10));

    // Validate requireds (after config+CLI merge)
    if (o.dimA==0 || o.dimB==0) { std::cerr << "ERROR: graph --size is required (config or CLI)\n"; o.exit_code=2; return false; }
    if (o.density<=0.0) { std::cerr << "ERROR: --density must be > 0 (config or CLI)\n"; o.exit_code=2; return false; }
    if (o.move==MoveRule::FirstAccepting && (o.threshold<0.0 || o.threshold>1.0)) {
        std::cerr << "ERROR: --threshold must be in [0,1] when --move first\n"; o.exit_code=2; return false;
    }
    if (o.eps<=0.0) { std::cerr << "ERROR: --eps must be > 0 (config or CLI)\n"; o.exit_code=2; return false; }

    o.exit_code = 0;
    return true;
}
