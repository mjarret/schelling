// main.cpp — minimalist: load config, parse CLI, run app.

#include "cli.hpp"
#include "config.hpp"
#include "app.hpp"

int main(int argc, char** argv) {
    // First: detect --config early (so config provides defaults)
    std::string cfg_path;
    if (cli_peek_option(argc, argv, "--config", cfg_path)) {
        // ok
    }

    CLIOptions opt; // defaults
    if (!cfg_path.empty()) {
        if (!load_config(cfg_path, opt)) {
            std::fprintf(stderr, "ERROR: failed to load config file: %s\n", cfg_path.c_str());
            return 2;
        }
    }

    // Then parse CLI (overrides config/defaults)
    if (!parse_cli(argc, argv, opt)) return opt.exit_code;  // prints help/errors

    return run_app(opt);
}
