#include <iostream>
#include <exception>
#include "../include/cli/cli.hpp"
#include "../include/app/app.hpp"

int main(int argc, char** argv) {
    try {
        CLIOptions opt = parse_cli(argc, argv);
        return run_app(opt);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
