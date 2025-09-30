# Cache Coherence Simulator

## Setup

1. Build and run the cache coherence simulator:

```bash
# Unzip benchmarks
make extract

# Build C++ files
make build

# Run (Part 1 uses only the *_0.data trace file)
# Example: 4 KiB cache, 2-way, 32B blocks (defaults suggested by the spec)
./coherence MESI ./traces/bodytrack_0.data 4096 2 32

# or with explicit filename
./coherence Dragon ./traces/bodytrack_0.data 4096 2 32 --json
```

2. Automated test running with traces:

```bash
# Basic testing
make test

# Extensive testing
make sweep
```
