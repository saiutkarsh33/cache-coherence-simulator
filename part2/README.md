# Part 2 — MESI (4 cores)

This folder builds a 4-core MESI simulator (`./coherence`) and runs it on PARSEC traces (`*_0..3.data`).

## 0) Prereqs

Make sure you already unzipped the benchmark zips so you have files like:

```
../traces/bodytrack_0.data
../traces/bodytrack_1.data
../traces/bodytrack_2.data
../traces/bodytrack_3.data
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
# from part2/
ln -s ../traces ./traces
```

**Option B — don’t symlink; pass an explicit _0.data path**


## 3) Run (exact commands)

### 3A) If you did the symlink (./traces exists)

```bash
# bodytrack, 4 KiB cache, 2-way, 32-B blocks, JSON output
./coherence MESI bodytrack 4096 2 32 --json

# another example
./coherence MESI blackscholes 4096 2 32
```

### 3B) If you did NOT symlink

```bash
# give explicit _0.data; the program auto-loads _1/_2/_3 from the same folder
./coherence MESI ../traces/bodytrack_0.data 4096 2 32 --json
./coherence MESI ../traces/blackscholes_0.data 4096 2 32
```

## 4) What you should see

* Overall execution cycles (max across the 4 cores)
* Per-core: exec/compute/idle cycles, loads/stores, hits/misses
* Bus data traffic (bytes)
* Invalidation broadcasts (BusUpgr + BusRdX count)

## 5) Quick sanity check (one-liner)

```bash
# Run 3 benchmarks to JSON (adjust paths if needed)
mkdir -p out
./coherence MESI ../traces/bodytrack_0.data      4096 2 32 --json > out/bodytrack.json
./coherence MESI ../traces/blackscholes_0.data   4096 2 32 --json > out/blackscholes.json
./coherence MESI ../traces/fluidanimate_0.data   4096 2 32 --json > out/fluidanimate.json
```

## 6) Common error & fix

**Error:**
`Could not resolve four trace files for base 'bodytrack'.`
**Fix:** Use one of:

* Create the symlink: `ln -s ../traces ./traces` then run `./coherence MESI bodytrack ...`
* Or pass explicit path: `./coherence MESI ../traces/bodytrack_0.data ...`

