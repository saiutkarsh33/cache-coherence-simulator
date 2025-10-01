#!/usr/bin/env python3
import json
import os
import sys
import glob


def load(path):
    with open(path, 'r') as f:
        return json.load(f)


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else "./out"
    files = sorted(glob.glob(os.path.join(folder, "*.json")))
    if not files:
        print(f"No JSON files in {folder}")
        sys.exit(1)

    print(f"{'Benchmark':24} {'Loads':>8} {'Stores':>8} {'Hits':>10} {'Misses':>10} {'HitRate':>8} {'ExecCycles':>12} {'BusBytes':>12}")
    print("-"*100)

    for path in files:
        data = load(path)
        name = os.path.splitext(os.path.basename(path))[0]
        loads = int(data["per_core_loads"][0])
        stores = int(data["per_core_stores"][0])
        hits = int(data["per_core_hits"][0])
        misses = int(data["per_core_misses"][0])
        total = hits + misses if (hits + misses) else 1
        hr = hits / total
        execc = int(data["overall_execution_cycles"])
        bus = int(data["bus_data_traffic_bytes"])
        print(
            f"{name:24} {loads:8d} {stores:8d} {hits:10d} {misses:10d} {hr:8.2%} {execc:12d} {bus:12d}")


if __name__ == "__main__":
    main()
