// cli.hpp — Command-line parsing interface (cxxopts)
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>

namespace cli {

struct Options {
    // Schelling threshold as p/q
    std::uint64_t p = 1;
    std::uint64_t q = 2;
    bool pq_overridden = false;
    // Experiment config
    std::uint64_t experiments = 0; // 0 means "use default"
    std::size_t   max_steps  = 0;  // 0 means "use default"
    // Graph size (Lollipop)
    std::size_t clique_size = 50;
    std::size_t path_length = 450;
    // Threads
    bool threads_set = false;
    bool threads_max = false; // if true, use omp_get_max_threads()
    int  threads = 0;         // explicit thread count if set
    // Misc
    bool show_help = false;   // legacy; parse_args returns want_help separately
    // Agent density (optional runtime parameter), TODO: integrate into initialization
    std::optional<double> agent_density; // e.g., 0.0..1.0; currently unused
};

// Parse CLI arguments with Boost.Program_options.
// On success, returns filled Options and sets want_help/help_text for --help or errors.
Options parse_args(int argc, char** argv, bool& want_help, std::string& help_text);

} // namespace cli
