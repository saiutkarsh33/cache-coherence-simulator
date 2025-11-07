# Part 2 — MESI (4 cores)

This folder builds a 4-core MESI simulator (`./coherence`) and runs it on PARSEC traces (`*_0..3.data`).

## 0) Prereqs

Make sure you already unzipped the benchmark zips so you have files like:

```
../../tests/benchmark_traces/bodytrack_0.data
../../tests/benchmark_traces/bodytrack_1.data
../../tests/benchmark_traces/bodytrack_2.data
../../tests/benchmark_traces/bodytrack_3.data
```

If your traces are elsewhere, adjust the paths below.

## 1) Build

```bash
# from part2/
make
# result: ./coherence
```

## 2) Point this folder to your traces (pick ONE option)

**Option A — symlink (then you can use short benchmark names)**

```bash
# from src/mesi/
ln -s ../../tests/benchmark_traces ./tests/benchmark_traces
```

**Option B — don’t symlink; pass an explicit \_0.data path**

## 3) Run (exact commands)

### 3A) If you did the symlink (./tests/benchmark_traces exists)

```bash
# bodytrack, 4 KiB cache, 2-way, 32-B blocks, JSON output
./coherence MESI bodytrack 4096 2 32 --json

# another example
./coherence MESI blackscholes 4096 2 32
```

### 3B) If you did NOT symlink

```bash
# give explicit _0.data; the program auto-loads _1/_2/_3 from the same folder
./coherence MESI ../../tests/benchmark_traces/bodytrack_0.data 4096 2 32 --json
./coherence MESI ../../tests/benchmark_traces/blackscholes_0.data 4096 2 32
```

## 4) What you should see

- Overall execution cycles (max across the 4 cores)
- Per-core: exec/compute/idle cycles, loads/stores, hits/misses
- Bus data traffic (bytes)
- Invalidation broadcasts (BusUpgr + BusRdX count)

## 5) Quick sanity check (one-liner)

```bash
# Run 3 benchmarks to JSON (adjust paths if needed)
mkdir -p out
./coherence MESI ../../tests/benchmark_traces/bodytrack_0.data      4096 2 32 --json > ./tests/out/bodytrack.json
./coherence MESI ../../tests/benchmark_traces/blackscholes_0.data   4096 2 32 --json > ./tests/out/blackscholes.json
./coherence MESI ../../tests/benchmark_traces/fluidanimate_0.data   4096 2 32 --json > ./tests/out/fluidanimate.json
```

## 6) Common error & fix

**Error:**
`Could not resolve four trace files for base 'bodytrack'.`
**Fix:** Use one of:

- Create the symlink: `ln -s ../../tests/benchmark_traces ./tests/benchmark_traces` then run `./coherence MESI bodytrack ...`
- Or pass explicit path: `./coherence MESI ../../tests/benchmark_traces/bodytrack_0.data ...`

## 7) Sample output

blackscholes:

```json
{
  "overall_execution_cycles": 21258071,
  "per_core_execution_cycles": [20565403, 17663440, 18781671, 21258071],
  "per_core_compute_cycles": [10430314, 10383276, 10430338, 10394904],
  "per_core_loads": [1489888, 1485857, 1492629, 1493736],
  "per_core_stores": [1007461, 1004611, 1016428, 1009391],
  "per_core_idle_cycles": [7637740, 4789696, 5842276, 8360040],
  "per_core_hits": [2443083, 2451323, 2464317, 2445171],
  "per_core_misses": [54266, 39145, 44740, 57956],
  "bus_data_traffic_bytes": 6275424,
  "bus_invalidations_or_updates": 47704,
  "protocol": "MESI",
  "config": { "cache_size": 0, "associativity": 0, "block_size": 32 }
}
```

bodytrack:

```json
{
  "overall_execution_cycles": 48283264,
  "per_core_execution_cycles": [48264030, 47714749, 19007651, 48283264],
  "per_core_compute_cycles": [17729254, 17120545, 17556877, 17140113],
  "per_core_loads": [2380720, 2388005, 74523, 2416052],
  "per_core_stores": [889412, 899247, 43175, 908867],
  "per_core_idle_cycles": [27264644, 27306952, 1333076, 27818232],
  "per_core_hits": [3041805, 3057649, 107980, 3090815],
  "per_core_misses": [228327, 229603, 9718, 234104],
  "bus_data_traffic_bytes": 22456064,
  "bus_invalidations_or_updates": 96072,
  "protocol": "MESI",
  "config": { "cache_size": 0, "associativity": 0, "block_size": 32 }
}
```

fluidanimate:

```json
{
  "overall_execution_cycles": 40828212,
  "per_core_execution_cycles": [40131161, 37539783, 40828212, 38267028],
  "per_core_compute_cycles": [11337782, 11290799, 11337671, 11301515],
  "per_core_loads": [1832392, 1821846, 1838008, 1832174],
  "per_core_stores": [744111, 585998, 766181, 579291],
  "per_core_idle_cycles": [26216876, 23841140, 26886352, 24554048],
  "per_core_hits": [2411496, 2257055, 2435922, 2253600],
  "per_core_misses": [165007, 150789, 168267, 157865],
  "bus_data_traffic_bytes": 20541696,
  "bus_invalidations_or_updates": 302382,
  "protocol": "MESI",
  "config": { "cache_size": 0, "associativity": 0, "block_size": 32 }
}
```
