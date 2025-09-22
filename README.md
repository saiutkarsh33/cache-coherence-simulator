# Cache Coherence Simulator

How to build and run

```
# Unzip benchmarks
cd benchmarks
unzip ./blackscholes_four.zip
unzip ./bodytrack_four.zip
unzip ./fluidanimate_four.zip
cd ..

# Build
make

# Run (Part 1 uses only the *_0.data trace file)
# Example: 4 KiB cache, 2-way, 32B blocks (defaults suggested by the spec)
./coherence MESI ./benchmarks/bodytrack_0.data 4096 2 32

# or with explicit filename
./coherence Dragon ./benchmarks/bodytrack_0.data 4096 2 32 --json
```
