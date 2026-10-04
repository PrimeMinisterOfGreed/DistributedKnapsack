#!/bin/bash
#SBATCH --job-name=knapsack_shared
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --time=02:00:00
#SBATCH --output=shared_memory.out
#SBATCH --error=shared_memory.err
#SBATCH --mem=32G

set -euo pipefail

NUM_THREADS=32
SEED=42
NUM_ITEMS=50
MAX_WEIGHT=5000
for max_weight in 10 100 1000; do
  for threads in 1 2 4 8 16 32; do
    for cap in 1000 10000 100000 1000000; do
      for item_block in 5 10 15 25; do
        for capacity_block in 10 100 1000 10000; do
          echo "========================================"
          echo "Capacity: $cap | Threads: $threads | MaxWeight: $max_weight| ItemBlock: $item_block | CapacityBlock: $capacity_block"
          echo "========================================"
          if [ "$capacity_block" -ge "$cap" ]; then
            continue
          fi
          echo "--- knapsackdpdag ---"
          python3 distributedknapsack \
            --test knapsackdpdag \
            --capacity "$cap" \
            --numItems "$NUM_ITEMS" \
            --seed "$SEED" \
            --maxWeight "$max_weight" \
            --itemBlock "$item_block" \
            --numThreads "$threads" \
            --capBlock "$capacity_block"
          echo ""
        done
      done
    done
  done
done
