CXX := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Wextra -pedantic

all: build

# Unzip the benchmarks
extract: tests/benchmark
	./scripts/extract_traces.sh

# Compile C++ files
build: src
	$(CXX) $(CXXFLAGS) -o coherence ./src/main.cpp

# Clean up C++ output files
clean:
	rm -f coherence

# Run benchmarks.
run: coherence
	./scripts/run_benchmarks.sh

# Run tests.
test: coherence
	./scripts/run_tests.sh

# Extensive testing.
sweep: coherence
	./scripts/sweep_part1.sh
