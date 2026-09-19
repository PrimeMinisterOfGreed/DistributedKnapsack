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

# Clear previous results for a fresh run

for threads in 1 2 4 8 16 32; do
  for cap in 1000 10000 100000 1000000; do
    echo "========================================"
    echo "Capacity: $cap | Threads: $threads"
    echo "========================================"

    export OMP_NUM_THREADS="$threads"

    echo "--- knapsackdp (shared memory) ---"
    python3 distributedknapsack \
      --test knapsackdp \
      --capacity "$cap" \
      --numItems "$NUM_ITEMS" \
      --seed "$SEED" \
      --maxWeight "$MAX_WEIGHT"

    echo ""
    echo "--- knapsackcopa (shared memory) ---"
    python3 distributedknapsack \
      --test knapsackdpdag \
      --capacity "$cap" \
      --numItems "$NUM_ITEMS" \
      --seed "$SEED" \
      --maxWeight "$MAX_WEIGHT"

    echo ""
  done
done
