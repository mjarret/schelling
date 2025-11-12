// Google Benchmark driver that calls the Python implementation per iteration
#include <benchmark/benchmark.h>
#include <Python.h>
#include <thread>
#include <string>
#include <filesystem>
#include <chrono>
#include <cstdlib>
#include <tbb/global_control.h>

// Initialize sys.path: always include Python_Version, and honor optional
// env-provided paths to reduce friction when running outside a venv.
static void init_sys_path_once() {
    static bool inited = false;
    if (inited) return;
    inited = true;
    try {
        std::string p = (std::filesystem::current_path() / "Python_Version").string();
        std::string cmd = std::string("import sys; sys.path.insert(0, r'" ) + p + "')";
        PyRun_SimpleString(cmd.c_str());
    } catch (...) {
        PyRun_SimpleString("import sys; sys.path.insert(0, 'Python_Version')");
    }
    // Also prepend any paths from PY_GBENCH_SITE_PACKAGES (colon-separated)
    if (const char* sp = std::getenv("PY_GBENCH_SITE_PACKAGES")) {
        std::string cmd = std::string("import sys; sys.path[:0] = [p for p in r'") + sp + "'.split(':') if p]";
        PyRun_SimpleString(cmd.c_str());
    }
    // Respect PYTHONPATH similarly so a venv can be used without wrapper scripts
    if (const char* pp = std::getenv("PYTHONPATH")) {
        std::string cmd = std::string("import sys; sys.path[:0] = [p for p in r'") + pp + "'.split(':') if p]";
        PyRun_SimpleString(cmd.c_str());
    }
}

static std::string g_init_error;

// ---------------- Amortized time budget (env-driven) ----------------
// PY_GBENCH_TOTAL_BUDGET_S: total wall-clock seconds budget for the whole run.
// PY_GBENCH_AMORTIZED_S: per-benchmark amortized cap (seconds). If set, it
//   overrides the computed (total/registered) share.
static double read_env_double(const char* name, double defv) {
    if (const char* s = std::getenv(name)) {
        char* end = nullptr;
        double v = std::strtod(s, &end);
        if (end && end != s) return v;
    }
    return defv;
}
static const double g_budget_total_s = read_env_double("PY_GBENCH_TOTAL_BUDGET_S", 0.0);
static const double g_budget_per_s   = read_env_double("PY_GBENCH_AMORTIZED_S", 0.0);
static int read_env_int(const char* name, int defv) {
    if (const char* s = std::getenv(name)) {
        char* end = nullptr;
        long v = std::strtol(s, &end, 10);
        if (end && end != s) return static_cast<int>(v);
    }
    return defv;
}
static const int g_cfg_iters   = read_env_int("PY_GBENCH_ITERATIONS", 10);
static const int g_cfg_threads = read_env_int("PY_GBENCH_THREADS",1);
static std::chrono::steady_clock::time_point g_run_start = std::chrono::steady_clock::now();
static std::atomic<int> g_registered{0};

struct PyRunner {
    PyObject* mod = nullptr;
    PyObject* fun = nullptr; // py_api.run_schelling_process_py
    PyRunner() {
        Py_Initialize();
        init_sys_path_once();
        PyObject* name = PyUnicode_DecodeFSDefault("py_api");
        mod = PyImport_Import(name);
        Py_DECREF(name);
        if (!mod) {
            PyErr_Print();
            g_init_error = "ImportError: cannot import py_api (check sys.path and Python deps)";
            return;
        }
        fun = PyObject_GetAttrString(mod, "run_schelling_process_py");
        if (!fun) {
            PyErr_Print();
            g_init_error = "AttributeError: py_api.run_schelling_process_py not found";
            return;
        }
    }
    ~PyRunner() {
        Py_XDECREF(fun);
        Py_XDECREF(mod);
        // Intentionally omit Py_Finalize() to avoid shutdown ordering issues in benchmarks
    }
};

template <std::size_t CS, std::size_t PL>
static void BM_Py_Schelling(benchmark::State& state) {
    static PyRunner R;
    if (R.fun == nullptr) {
        state.SkipWithMessage(g_init_error.empty() ? "Python init failed" : g_init_error.c_str());
        return;
    }
    // Amortized budget guard: skip if remaining total budget is below per-benchmark cap
    if (g_budget_total_s > 0.0 || g_budget_per_s > 0.0) {
        using clock = std::chrono::steady_clock;
        const double elapsed = std::chrono::duration<double>(clock::now() - g_run_start).count();
        const int reg = std::max(1, g_registered.load(std::memory_order_relaxed));
        const double cap = (g_budget_per_s > 0.0) ? g_budget_per_s
                          : (g_budget_total_s > 0.0 ? (g_budget_total_s / reg) : 0.0);
        const double remaining = (g_budget_total_s > 0.0) ? (g_budget_total_s - elapsed) : 1e300;
        if (cap > 0.0 && remaining < cap) {
            std::string msg = "Skipping: remaining budget " + std::to_string(remaining)
                            + "s < per-benchmark cap " + std::to_string(cap) + "s";
            state.SkipWithMessage(msg.c_str());
            return;
        }
    }
    for (auto _ : state) {
        PyGILState_STATE g = PyGILState_Ensure();
        PyObject* args = Py_BuildValue("(ii)", (int)CS, (int)PL); // defaults density=0.8, homophily=0.5
        PyObject* ret  = PyObject_CallObject(R.fun, args);
        if (ret == nullptr) {
            PyErr_Print();
            Py_DECREF(args);
            PyGILState_Release(g);
            state.SkipWithMessage("Python call failed; see stderr");
            return;
        }
        Py_DECREF(args);
        Py_DECREF(ret);
        PyGILState_Release(g);
    }
    state.SetItemsProcessed(state.iterations());
}

namespace {
template <std::size_t CS0, std::size_t Step, std::size_t Count,
          std::size_t Num, std::size_t Den, std::size_t Off>
struct PyLollipopSweep {
    template <std::size_t I>
    static void one() {
        constexpr std::size_t cs = CS0 + I * Step;
        constexpr std::size_t pl = (cs * Num) / Den + Off;
        auto* b = ::benchmark::RegisterBenchmark(
            (std::string("PySchelling/Lollipop/CS=") + std::to_string(cs) +
             "/PL=" + std::to_string(pl)).c_str(),
            &BM_Py_Schelling<cs, pl>);
        b->Unit(benchmark::kMillisecond)
         ->Iterations(g_cfg_iters)
         ->Threads(g_cfg_threads);
        g_registered.fetch_add(1, std::memory_order_relaxed);
    }
    template <std::size_t... Is>
    static void all(std::index_sequence<Is...>) { (one<Is>(), ...); }
    PyLollipopSweep() { all(std::make_index_sequence<Count>{}); }
};

// Mirror the C++ registrar sweep: CS in {50,100,...}, PL = 9*CS
static PyLollipopSweep</*CS0*/50, /*Step*/50, /*Count*/7,
                      /*Num*/9,  /*Den*/1,  /*Off*/0> g_py_sweep;
} // namespace

// -----------------------------------------------------------------------------
// Clique-only scaling: vary CS, hold PL fixed (mirror C++ family)
// -----------------------------------------------------------------------------
namespace {
template <std::size_t CS0, std::size_t Step, std::size_t Count, std::size_t PLFixed>
struct PyLollipopCliqueOnly {
    template <std::size_t I>
    static void one() {
        constexpr std::size_t cs = CS0 + I * Step;
        constexpr std::size_t pl = PLFixed;
        auto* b = ::benchmark::RegisterBenchmark(
            (std::string("PySchelling/LollipopCliqueOnly/CS=") + std::to_string(cs) +
             "/PL=" + std::to_string(pl)).c_str(),
            &BM_Py_Schelling<cs, pl>);
        b->Unit(benchmark::kMillisecond)
         ->Iterations(g_cfg_iters)
         ->Threads(g_cfg_threads);
        g_registered.fetch_add(1, std::memory_order_relaxed);
    }
    template <std::size_t... Is>
    static void all(std::index_sequence<Is...>) { (one<Is>(), ...); }
    PyLollipopCliqueOnly() { all(std::make_index_sequence<Count>{}); }
};

// Match C++ config: CS in {50,100,...}, PL fixed at 10000 (per current C++ code)
static PyLollipopCliqueOnly</*CS0*/50, /*Step*/50, /*Count*/7, /*PLFixed*/10000> g_py_clique_only;
} // namespace

BENCHMARK_MAIN();
