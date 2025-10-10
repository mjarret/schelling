CXX ?= c++
CXXFLAGS := -std=gnu++20 -Wall -Wextra -Wpedantic -Wno-stringop-overflow \
            -I../../include -I../../include/core -I../../include/graphs -I../../include/sim \
            -I../path/src \
            -Ofast -march=native -funroll-loops -flto=auto -fno-omit-frame-pointer -fopenmp

HARDENED ?= 0
CXXFLAGS += -DCORE_HARDENED=$(HARDENED)

TARGET := clique_tests
SRC := main.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
