CXX := g++
CXXFLAGS := -Ofast -DNDEBUG -std=gnu++20 -Wall -Wextra -Wpedantic -pthread -march=native \
            -I third_party/indicators/include
LDFLAGS := 

OBJS := main.o

all: schelling

schelling: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

main.o: main.cpp app.hpp cli.hpp config.hpp seeds.hpp plot_manager.hpp progress_manager.hpp move_rule.hpp cs_stop.hpp \
        term_utils.hpp live_plot.hpp experiment.hpp cost_aggregator.hpp checkpoints.hpp \
        geometry_lollipop.hpp \
        bitwords.hpp world_concept.hpp world_grid_packed.hpp world_agent_packed.hpp geometry.hpp \
        metrics.hpp rng.hpp random_fill.hpp sim_random.hpp

clean:
	rm -f $(OBJS) schelling
