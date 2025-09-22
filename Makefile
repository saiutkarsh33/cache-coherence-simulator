CXX := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -pedantic

all: coherence

coherence: coherence.cpp
	$(CXX) $(CXXFLAGS) -o coherence coherence.cpp

clean:
	rm -f coherence
