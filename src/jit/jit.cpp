// jit.cpp — compile-on-demand runner for LollipopGraph
//
// This module implements a tiny JIT that:
//  1) Emits a single-file translation unit specializing
//     graphs::LollipopGraph<CS,PL> and running the Schelling process.
//  2) Compiles it to a shared object with the system C++ compiler.
//  3) Loads the shared object and invokes an extern "C" entrypoint.
//
// Design notes
// ------------
// - Codegen is intentionally minimal and self-contained: includes headers,
//   constructs the graph, initializes the global threshold, runs the sim,
//   and returns summary counters.
// - We store artifacts in `_jit/` and rebuild the .so if it's missing or
//   older than its source. The generator always rewrites the .cpp to avoid
//   stale code when templates or sim APIs change.
// - We rely on dlopen/dlsym (POSIX). Portability to other platforms is out
//   of scope here but can be added with alternative loader hooks.
// - Error codes are documented in include/jit/jit.hpp.

#include "jit/jit.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace fs = std::filesystem;

namespace jit {

static std::string sanitize_for_filename(std::string_view s) {
    std::string out; out.reserve(s.size());
    for (char ch : s) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) out.push_back(ch);
        else out.push_back('_');
    }
    if (out.size() > 120) out.resize(120);
    return out;
}

static std::string compiler_cmd() {
    const char* cxx = std::getenv("CXX");
    return cxx ? std::string(cxx) : std::string("c++");
}

// Choose the fastest viable index type for the given max size hint.
// Heuristic: prefer 32-bit on 64-bit hosts when it fits; otherwise 64-bit.
// On 32-bit hosts, use 32-bit.
static std::string select_index_type(std::uint64_t max_size_hint) {
    // Choose the fastest viable width based on required range.
    // Minimal width selection: 8/16/32/64 bits, letting the compiler optimize.
    // If no hint is provided (0), default to 32-bit on 64-bit hosts, else 32-bit.
    if (max_size_hint == 0) {
        return (sizeof(void*) == 8) ? std::string("std::uint32_t") : std::string("std::uint32_t");
    }
    if (max_size_hint <= 0xFFull)      return "std::uint8_t";
    if (max_size_hint <= 0xFFFFull)    return "std::uint16_t";
    if (max_size_hint <= 0xFFFFFFFFull) return "std::uint32_t";
    return "std::uint64_t";
}

static std::string jit_src_code(std::string_view include_header, std::string_view graph_type_expr) {
    std::ostringstream oss;
    oss << R"CPP(
#include <cstdint>
#include <vector>
#include ")CPP" << include_header << R"CPP("
#include "sim/sim.hpp"
#include "core/rng.hpp"
#include "core/schelling_threshold.hpp"
extern "C" int run_once(unsigned long long p, unsigned long long q, double density,
                        unsigned long long* moves_out,
                        unsigned long long* final_unhappy_out) {
    core::schelling::init_program_threshold((core::color_count_t)p, (core::color_count_t)q);
    core::Xoshiro256ss rng(core::splitmix_hash(0xD1E5EEDULL));
    )CPP" << graph_type_expr << R"CPP( g;
    auto history = sim::run_schelling_process(g, density, rng);
    if (moves_out) *moves_out = history.empty() ? 0ULL : (unsigned long long)(history.size() - 1);
    if (final_unhappy_out) *final_unhappy_out = history.empty() ? 0ULL : (unsigned long long)history.back();
    return 0;
}
)CPP";
    return oss.str();
}

int run_graph_once(std::string_view include_header,
                   std::string_view graph_type_expr,
                   std::uint64_t p,
                   std::uint64_t q,
                   double density,
                   std::uint64_t max_size_hint,
                   unsigned long long& moves_out,
                   unsigned long long& final_unhappy_out,
                   std::string* build_log) {
    try {
        fs::path jit_dir = fs::path("_jit");
        fs::create_directories(jit_dir);
        std::string base = std::string("g_") + sanitize_for_filename(graph_type_expr);
        fs::path src = jit_dir / (base + ".cpp");
        const char* ext =
#if defined(__APPLE__)
            ".dylib";
#elif defined(_WIN32)
            ".dll";
#else
            ".so";
#endif
        fs::path so  = jit_dir / (base + ext);

        // (Re)write source to ensure it reflects current generator
        {
            std::ofstream ofs(src);
            ofs << jit_src_code(include_header, graph_type_expr);
            ofs.close();
        }

        // Compile if .so missing or older than source
        bool need_build = !fs::exists(so) || fs::last_write_time(so) < fs::last_write_time(src);
        if (need_build) {
            std::ostringstream cmd;
            std::string cxx = compiler_cmd();
            std::string idx_t = select_index_type(max_size_hint);
#if defined(_WIN32)
            // Prefer MSVC/clang-cl style flags if using cl/clang-cl
            if (cxx.rfind("cl", 0) == 0 || cxx.find("clang-cl") != std::string::npos) {
                cmd << cxx
                    << " /nologo /O2 /EHsc /std:c++20 /LD " << src.string()
                    << " /Iinclude /Iinclude/core /Iinclude/graphs /Iinclude/sim /Iinclude/third_party"
                    << " /DCORE_INDEX_T=" << idx_t
                    << " /link /OUT:" << so.string();
            } else {
                // MinGW/other gcc-like
                cmd << cxx
                    << " -std=gnu++20 -O3 -DNDEBUG -shared -static -static-libgcc -static-libstdc++"
                    << " -Iinclude -Iinclude/core -Iinclude/graphs -Iinclude/sim -Iinclude/third_party"
                    << " -DCORE_INDEX_T=" << idx_t
                    << " -o " << so.string() << " " << src.string();
            }
#elif defined(__APPLE__)
            cmd << cxx
                << " -std=c++20 -O3 -DNDEBUG -fPIC -dynamiclib -march=native"
                << " -Iinclude -Iinclude/core -Iinclude/graphs -Iinclude/sim -Iinclude/third_party"
                << " -DCORE_INDEX_T=" << idx_t
                << " -o " << so.string() << " " << src.string();
#else
            cmd << cxx
                << " -std=gnu++20 -O3 -DNDEBUG -fPIC -shared -march=native -mbmi -mbmi2"
                << " -Iinclude -Iinclude/core -Iinclude/graphs -Iinclude/sim -Iinclude/third_party"
                << " -DCORE_INDEX_T=" << idx_t
                << " -o " << so.string() << " " << src.string();
#endif
            int rc = std::system(cmd.str().c_str());
            if (build_log) *build_log = cmd.str();
            if (rc != 0) return 3;
        }

        // Load and run
        using RunFn = int(*)(unsigned long long, unsigned long long, double, unsigned long long*, unsigned long long*);
#if defined(_WIN32)
        HMODULE handle = LoadLibraryA(so.string().c_str());
        if (!handle) return 4;
        RunFn run = reinterpret_cast<RunFn>(GetProcAddress(handle, "run_once"));
        if (!run) { FreeLibrary(handle); return 5; }
        int r = run(p, q, density, &moves_out, &final_unhappy_out);
        FreeLibrary(handle);
#else
        void* handle = dlopen(so.c_str(), RTLD_NOW);
        if (!handle) return 4;
        RunFn run = reinterpret_cast<RunFn>(dlsym(handle, "run_once"));
        if (!run) { dlclose(handle); return 5; }
        int r = run(p, q, density, &moves_out, &final_unhappy_out);
        dlclose(handle);
#endif
        return r;
    } catch (...) {
        return 2;
    }
}

int run_lollipop_once(std::size_t clique_size,
                      std::size_t path_length,
                      std::uint64_t p,
                      std::uint64_t q,
                      double density,
                      unsigned long long& moves_out,
                      unsigned long long& final_unhappy_out,
                      std::string* build_log) {
    std::ostringstream type;
    type << "graphs::LollipopGraph<" << clique_size << "," << path_length << ">";
    std::uint64_t max_size_hint = static_cast<std::uint64_t>(clique_size) + static_cast<std::uint64_t>(path_length);
    return run_graph_once("graphs/lollipop.hpp", type.str(), p, q, density, max_size_hint, moves_out, final_unhappy_out, build_log);
}

} // namespace jit
