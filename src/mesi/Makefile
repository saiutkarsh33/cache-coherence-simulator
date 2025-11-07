CXX := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -pedantic

all: coherence

coherence: main.cpp ../bus.hpp mesi_sim.hpp mesi_protocol.hpp
	$(CXX) $(CXXFLAGS) -o $@ main.cpp

clean:
	rm -f coherence
