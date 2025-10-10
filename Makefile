CXX ?= c++

MODE ?= release

CXXFLAGS_COMMON := -std=gnu++20 -Wall -Wextra -Wpedantic \
                   -Iinclude -Iinclude/core -Iinclude/graphs -Iinclude/sim

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

OPENMP ?= auto
OPENMP_FLAGS :=
ifeq ($(OPENMP),1)
  OPENMP_FLAGS := -fopenmp
else ifeq ($(OPENMP),yes)
  OPENMP_FLAGS := -fopenmp
else ifeq ($(OPENMP),auto)
  OPENMP_PRESENT := $(shell echo 'int main(){return 0;}' | $(CXX) $(CXXFLAGS_COMMON) -fopenmp -x c++ - -o /dev/null >/dev/null 2>&1 && echo yes || echo no)
  ifeq ($(OPENMP_PRESENT),yes)
    OPENMP_FLAGS := -fopenmp
  endif
endif

CXXFLAGS_RELEASE += $(OPENMP_FLAGS)
CXXFLAGS_DEBUG   += $(OPENMP_FLAGS)

ifeq ($(MODE),debug)
  CXXFLAGS_SELECTED := $(CXXFLAGS_DEBUG)
else
  CXXFLAGS_SELECTED := $(CXXFLAGS_RELEASE)
endif

LP_SRC := src/main.cpp
LP_BIN := lollipop

# Google Benchmark target (requires libbenchmark installed)
GBENCH_LIBS ?= -lbenchmark -lpthread
BENCH_SRC := bench/lollipop_bench.cpp
BENCH_BIN := lollipop_bench

.PHONY: all release debug run bench clean purge print-flags help

all: $(LP_BIN)

release: MODE := release
release: $(LP_BIN)

debug: MODE := debug
debug: $(LP_BIN)

$(LP_BIN): $(LP_SRC)
	$(CXX) $(CXXFLAGS_COMMON) $(CXXFLAGS_SELECTED) $< -o $@

run: $(LP_BIN)
	./$(LP_BIN)

bench: $(BENCH_BIN)

$(BENCH_BIN): $(BENCH_SRC)
	$(CXX) $(CXXFLAGS_COMMON) $(CXXFLAGS_SELECTED) $< -o $@ $(GBENCH_LIBS)

clean:
	rm -f $(LP_BIN) *.o

purge: clean
	rm -rf build-fast build-bench CMakeCache.txt CMakeFiles cmake_install.cmake _deps

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
	@echo "  run             -> run lollipop after build";
	@echo "  bench           -> build lollipop_bench (Google Benchmark)";
	@echo "  clean           -> remove built binaries and *.o";
	@echo "  purge           -> clean + remove common CMake artifacts";
	@echo "  print-flags     -> display current compiler flags";
