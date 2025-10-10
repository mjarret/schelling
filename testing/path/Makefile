CXX ?= c++
# Optimized build with debug symbols and frame pointers for profiling
# Add -pg for gprof instrumentation (compile + link)
CXXFLAGS := -std=gnu++20 -Wall -Wextra -Wpedantic -Wno-stringop-overflow \
            -I../../include -I../../include/core -I../../include/graphs -I../../include/sim -I./src \
            -Ofast -march=native -funroll-loops -flto=auto -fno-omit-frame-pointer -fopenmp 
HARDENED ?= 0
CXXFLAGS += -DCORE_HARDENED=$(HARDENED)
# Enable test-only accessors for Path in tests
CXXFLAGS += -DSCHELLING_TEST_ACCESSORS=1
TARGET := path_tests
SRC := main.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
