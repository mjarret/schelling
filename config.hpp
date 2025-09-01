#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <cctype>
#include <iostream>
#include <algorithm>

#include "cli.hpp"

// trim helpers
static inline std::string _trim(const std::string& s) {
    size_t a=0,b=s.size();
    while (a<b && std::isspace((unsigned char)s[a])) ++a;
    while (b>a && std::isspace((unsigned char)s[b-1])) --b;
    return s.substr(a,b-a);
}
static inline std::string _lower(const std::string& s) {
    std::string t; t.reserve(s.size());
    for (char c: s) t.push_back((char)std::tolower((unsigned char)c));
    return t;
}

// Parse size string using the same semantics as CLI
static inline bool cfg_parse_size(const std::string& s, uint32_t& a, uint32_t& b, bool is_torus) {
    std::string t = s;
    for (char& c: t) if (c=='X' || c=='x' || c=='+') c = (is_torus ? 'x' : ':');
    auto p = t.find(is_torus ? 'x' : ':');
    if (p == std::string::npos) return false;
    a = static_cast<uint32_t>(std::stoull(t.substr(0,p)));
    b = static_cast<uint32_t>(std::stoull(t.substr(p+1)));
    return (a>0 && b>0);
}

inline bool load_config(const std::string& path, CLIOptions& o) {
    std::ifstream fin(path);
    if (!fin) return false;

    std::unordered_map<std::string,std::string> kv;
    std::string line;
    while (std::getline(fin, line)) {
        // strip comments
        auto phash = line.find('#'); if (phash!=std::string::npos) line = line.substr(0, phash);
        auto psemi = line.find(';'); if (psemi!=std::string::npos) line = line.substr(0, psemi);
        line = _trim(line);
        if (line.empty()) continue;
        auto peq = line.find('=');
        if (peq == std::string::npos) continue;
        std::string k = _trim(line.substr(0, peq));
        std::string v = _trim(line.substr(peq+1));
        kv[_lower(k)] = v;
    }

    auto has = [&](const char* key){ return kv.find(key)!=kv.end(); };
    auto get = [&](const char* key){ return kv.at(key); };

    // graph first (so size parsing knows separator)
    if (has("graph")) {
        auto t = _lower(get("graph"));
        if      (t=="torus")    o.graph = GraphKind::TORUS;
        else if (t=="lollipop") o.graph = GraphKind::LOLLIPOP;
        else std::cerr << "WARN: config graph must be torus|lollipop; got '" << t << "'\n";
    }

    if (has("size")) {
        uint32_t A=0,B=0;
        if (!cfg_parse_size(get("size"), A, B, o.graph==GraphKind::TORUS)) {
            std::cerr << "WARN: config size invalid: " << get("size") << "\n";
        } else { o.dimA=A; o.dimB=B; }
    }

    if (has("move")) {
        auto t = _lower(get("move"));
        if      (t=="any")   o.move = MoveRule::Any;
        else if (t=="first") o.move = MoveRule::FirstAccepting;
        else std::cerr << "WARN: move must be any|first; got '" << t << "'\n";
    }

    if (has("density"))   o.density   = std::clamp(std::atof(get("density").c_str()), 0.0, 1.0);
    if (has("threshold")) o.threshold = std::clamp(std::atof(get("threshold").c_str()), 0.0, 1.0);

    // CS options
    if (has("alpha")) o.alpha = std::max(1e-16, std::atof(get("alpha").c_str()));
    if (has("eps"))   o.eps   = std::max(0.0,   std::atof(get("eps").c_str()));

    // Optional
    if (has("threads")) o.threads = std::atoi(get("threads").c_str());
    if (has("plot"))    { bool b=false; if (_to_bool(get("plot"),b)) o.plot_enabled=b; }
    if (has("seed"))    { auto v=get("seed"); o.seed = (v=="auto"? 0ULL : (uint64_t)std::strtoull(v.c_str(), nullptr, 10)); }
    if (has("k"))       o.k = std::max<uint32_t>(1, std::strtoul(get("k").c_str(), nullptr, 10));
    if (has("debug"))   { bool b=false; if (_to_bool(get("debug"),b)) o.debug=b; }
    if (has("debug_every")) o.debug_every = std::max<uint32_t>(1, std::strtoul(get("debug_every").c_str(), nullptr, 10));

    return true;
}
