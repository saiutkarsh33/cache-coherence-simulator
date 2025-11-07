#!/usr/bin/env python3
import json
import os
import sys
import glob

TEST_SINGLE_CORE = True


def load(path):
    with open(path, 'r') as f:
        return json.load(f)


def main():
    folder = sys.argv[1] if len(sys.argv) > 1 else "./tests/out"
    files = sorted(glob.glob(os.path.join(folder, "*.json")))
    if not files:
        print(f"No JSON files in {folder}")
        sys.exit(1)

    failed = 0
    for path in files:
        data = load(path)
        name = os.path.splitext(os.path.basename(path))[0]
        loads = int(data["per_core_loads"][0])
        stores = int(data["per_core_stores"][0])
        hits = int(data["per_core_hits"][0])
        misses = int(data["per_core_misses"][0])
        bus_data_traffic_bytes = int(data["bus_data_traffic_bytes"])
        bus_invalidations_or_updates = int(
            data["bus_invalidations_or_updates"])
        block_size = int(data["config"]["block_size"])
        private_accesses = int(data["private_accesses"][0])
        shared_accesses = int(data["shared_accesses"][0])

        # General invariants
        if (hits + misses != loads + stores):
            failed += 1
            print(
                f"test failed for {name}: expected hits + misses == loads + stores")
        if (bus_data_traffic_bytes % block_size != 0):
            failed += 1
            print(
                f"test failed for {name}: bus_data_traffic_bytes % block_size == 0")

        # Single core specific invariants
        if (TEST_SINGLE_CORE):
            if (private_accesses != loads + stores):
                failed += 1
                print(
                    f"test failed for {name}: private_accesses == loads + stores", )
            if (bus_invalidations_or_updates != 0):
                failed += 1
                print(
                    f"test failed for {name}: bus_invalidations_or_updates == 0")
            if (shared_accesses != 0):
                failed += 1
                print(f"shared_accesses == 0")

    if (failed == 0):
        print(f"All tests passed!")
    else:
        print(f"{failed} tests failed!")


if __name__ == "__main__":
    main()
