CXX ?= c++

MODE ?= release

CXXFLAGS_COMMON := -std=gnu++20 -Wall -Wextra -Wpedantic \
                   -Iinclude -Iinclude/core -Iinclude/graphs -Iinclude/sim -Iinclude/third_party \
                   -DSCHELLING_NO_HEATMAP

CXXFLAGS_RELEASE := -O3 -DNDEBUG -fno-omit-frame-pointer -march=native -mbmi -mbmi2
CXXFLAGS_DEBUG   := -Og -g3 -fno-omit-frame-pointer -march=native -mbmi -mbmi2

TRACE        ?= 0
DEBUG_PRINTS ?= 0

ifeq ($(TRACE),1)
  CXXFLAGS_COMMON += -DSCHELLING_DEBUG_TRACE_STEPS
endif
ifeq ($(DEBUG_PRINTS),1)
  CXXFLAGS_COMMON += -DSCHELLING_ENABLE_DEBUG_PRINTS
endif

# oneTBB linkage
TBB_LIBS ?= -ltbb

ifeq ($(MODE),debug)
  CXXFLAGS_SELECTED := $(CXXFLAGS_DEBUG)
else
  CXXFLAGS_SELECTED := $(CXXFLAGS_RELEASE)
endif

LP_SRCS := src/main.cpp src/cli/cli.cpp src/jit/jit.cpp
ifeq ($(MATPLOT),1)
  LP_SRCS += src/shims/nodesoup_stubs.cpp
endif
LP_BIN := lollipop

# Google Benchmark targets (requires libbenchmark installed)
GBENCH_LIBS ?= -lbenchmark -lpthread
BENCH_SRC := testing/bench/lollipop_bench.cpp
BENCH_BIN := lollipop_bench
HT_BENCH_SRC := testing/bench/hitting_time_bench.cpp
HT_BENCH_BIN := hitting_time_bench

# Python gbench target (embeds Python, calls Python_Version/py_api)
PY_HT_BENCH_SRC := Python_Version/python_gbench.cpp
PY_HT_BENCH_BIN := py_hitting_time_bench
PYTHON_CFLAGS   := $(shell python3-config --includes)
PYTHON_LDFLAGS  := $(shell python3-config --embed --ldflags 2>/dev/null || python3-config --ldflags)

.PHONY: all release debug run bench clean purge print-flags help

all: $(LP_BIN)

release: MODE := release
release: $(LP_BIN)

debug: MODE := debug
debug: $(LP_BIN)

DL_LIBS ?= -ldl
LDFLAGS ?=

# --- Opt-in sanitizers via `make SAN=...` ---
# Examples:
#   make SAN=address,undefined   # ASan + UBSan
#   make SAN=thread              # TSan (run separately)
#   make SAN=leak                # LeakSanitizer (Clang on Linux)
SAN ?=
ifneq ($(strip $(SAN)),)
SANFLAGS := -fsanitize=$(SAN) -fno-omit-frame-pointer -g -O1
CXXFLAGS_RELEASE += $(SANFLAGS)
CXXFLAGS_DEBUG   += $(SANFLAGS)
LDFLAGS += $(SANFLAGS)
export ASAN_OPTIONS=abort_on_error=1,detect_stack_use_after_return=1,strict_string_checks=1
export UBSAN_OPTIONS=print_stacktrace=1,halt_on_error=1
endif

# Profiling (gprof) toggle. When PROFILE=1, build with -pg.
PROFILE ?= 0
ifeq ($(PROFILE),1)
  # Enable gprof instrumentation and disable PIE to improve attribution
  CXXFLAGS_COMMON += -pg -fno-pie
  LDFLAGS += -pg -no-pie
endif

# Optional Matplot++ integration (disabled by default)
# Enable with: make MATPLOT=1 [MATPLOT_LIBS='-lmatplot']
MATPLOT ?= 0
MATPLOT_LIBS ?= -lmatplot -lnodesoup
ifeq ($(MATPLOT),1)
  CXXFLAGS_COMMON += -DIO_USE_MATPLOT
  ifneq (,$(strip $(MАТPLOT_LIBS)))
    DL_LIBS += $(MATPLOT_LIBS)
  endif
endif

# Some Matplot++ builds are instrumented with ASan/UBSan. If linking fails
# with __asan/__ubsan symbols, rebuild with MATPLOT_SAN=1 to link sanitizers.
MATPLOT_SAN ?= 0
ifeq ($(MATPLOT_SAN),1)
  CXXFLAGS_RELEASE += -fsanitize=address,undefined
  CXXFLAGS_DEBUG   += -fsanitize=address,undefined
  DL_LIBS += -fsanitize=address,undefined
endif

$(LP_BIN): $(LP_SRCS)
	$(CXX) $(CXXFLAGS_COMMON) $(CXXFLAGS_SELECTED) $(EXTRA_CXXFLAGS) $(LDFLAGS) $(LP_SRCS) -o $@ $(DL_LIBS) $(TBB_LIBS)


run: $(LP_BIN)
	ulimit -s unlimited && ./$(LP_BIN)

bench: $(BENCH_BIN) $(HT_BENCH_BIN)

$(BENCH_BIN): $(BENCH_SRC)
	$(CXX) $(CXXFLAGS_COMMON) $(CXXFLAGS_SELECTED) $(EXTRA_CXXFLAGS) $(LDFLAGS) $< -o $@ $(GBENCH_LIBS) $(TBB_LIBS)

$(HT_BENCH_BIN): $(HT_BENCH_SRC)
	$(CXX) $(CXXFLAGS_COMMON) $(CXXFLAGS_SELECTED) $(EXTRA_CXXFLAGS) $(LDFLAGS) $< -o $@ $(GBENCH_LIBS) $(TBB_LIBS)

$(PY_HT_BENCH_BIN): $(PY_HT_BENCH_SRC)
	$(CXX) $(CXXFLAGS_COMMON) $(CXXFLAGS_SELECTED) $(PYTHON_CFLAGS) $(EXTRA_CXXFLAGS) $(LDFLAGS) $< -o $@ $(GBENCH_LIBS) $(TBB_LIBS) $(PYTHON_LDFLAGS)

clean:
	rm -f $(LP_BIN) *.o

purge: clean
	rm -rf build-fast build-bench CMakeCache.txt CMakeFiles cmake_install.cmake _deps _jit


print-flags:
	@echo "CXX=$(CXX)";
	@echo "MODE=$(MODE)";
	@echo "CXXFLAGS_COMMON=$(CXXFLAGS_COMMON)";
	@echo "CXXFLAGS_SELECTED=$(CXXFLAGS_SELECTED)";
	@echo "OPENMP_FLAGS=$(OPENMP_FLAGS)";

help:
	@echo "Usage: make <target> [MODE=release|debug] [OPENMP=auto|0|1] [TRACE=1] [DEBUG_PRINTS=1]";
	@echo "Targets:";
	@echo "  all (default)   -> build lollipop";
	@echo "  release         -> build lollipop with Release flags";
	@echo "  debug           -> build lollipop with Debug flags";
	@echo "  profile         -> build with -pg enabled for gprof";
	@echo "  run             -> run lollipop after build";
	@echo "  bench           -> build lollipop_bench (Google Benchmark)";
	@echo "  py_hitting_time_bench -> build Python-embedded gbench (requires python3-dev)";
	@echo "  clean           -> remove built binaries and *.o";
	@echo "  purge           -> clean + remove common CMake artifacts";
	@echo "  print-flags     -> display current compiler flags";

.PHONY: profile
profile: PROFILE := 1
profile: MODE := release
profile: $(LP_BIN)
