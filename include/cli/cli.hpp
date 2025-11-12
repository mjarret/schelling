// cli.hpp — Command-line parsing interface (cxxopts)
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <thread>
#include <optional>
#include "core/schelling_threshold.hpp"


namespace cli {

struct Options {
    // Schelling threshold as p/q (defaults to 1/2)
    core::color_count_t p = 1;
    core::color_count_t q = 2;

    // Graph size (Lollipop) with defaults
    std::size_t clique_size = 500;
    std::size_t path_length = 4500;

    // Agent density in [0,1] (default set)
    double agent_density = 0.8;
    // Number of experiments to run (default set)
    std::size_t experiments = 1000;

    // Number of threads (keep one open by default)
    int threads = std::thread::hardware_concurrency() > 1 ? static_cast<int>(std::thread::hardware_concurrency()) - 1 : 1;
    
    // Optional maximum simulation steps; nullopt => ∞ (no cap)
    std::optional<std::size_t> max_steps;
};

// Parse CLI arguments with Boost.Program_options.
// On success, returns filled Options and sets want_help/help_text for --help or errors.
Options parse_args(int argc, char** argv, bool& want_help, std::string& help_text);

} // namespace cli
