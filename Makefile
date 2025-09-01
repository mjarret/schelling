CXX := g++
CXXFLAGS := -std=gnu++20 -Ofast -march=native -flto -Wall -Wextra -Wpedantic -pthread
LDFLAGS := -pthread

INCLUDES := -Iinclude

SRC := src/main.cpp
BIN := schelling

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(BIN)

.PHONY: all clean
