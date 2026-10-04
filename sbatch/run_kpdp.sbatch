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

# Clear previous results for a fresh run
for max_weight in 10 100 1000; do
  for threads in 1 2 4 8 16 32; do
    for cap in 10000 100000 1000000 10000000 10000000; do
      echo "========================================"
      echo "Capacity: $cap | Threads: $threads | MaxWeight: $max_weight"
      echo "========================================"

      export OMP_NUM_THREADS="$threads"

      echo "--- knapsackdp ---"
      python3 distributedknapsack \
        --test knapsackdp \
        --capacity "$cap" \
        --numItems "$NUM_ITEMS" \
        --seed "$SEED" \
        --numThreads "$threads" \
        --maxWeight "$max_weight"

      echo ""
    done
  done
done
