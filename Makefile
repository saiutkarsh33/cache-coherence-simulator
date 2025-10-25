CXX := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -pedantic

all: build

# Unzip the benchmarks
extract: benchmarks
	./scripts/extract_traces.sh

# Compile C++ files
build: src
	$(CXX) $(CXXFLAGS) -o coherence ./src/main.cpp

# Clean up C++ output files
clean:
	rm -f coherence

# Basic testing
test: coherence
	./scripts/run_part1.sh

# Extensive testing
sweep: coherence
	./scripts/sweep_part1.sh
