CXX := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -pedantic

all: coherence

coherence: main.cpp mesi_bus.hpp mesi_cache.hpp mesi_sim.hpp
	$(CXX) $(CXXFLAGS) -o $@ main.cpp

clean:
	rm -f coherence
