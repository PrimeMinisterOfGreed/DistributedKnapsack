#!/bin/bash
#SBATCH --job-name=knapsack_gpu
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --gres=gpu:1
#SBATCH --time=02:00:00
#SBATCH --output=gpu.out
#SBATCH --error=gpu.err
#SBATCH --mem=32G

set -euo pipefail

SEED=42
NUM_ITEMS=50
MAX_WEIGHT=5000

# Clear previous results for a fresh run

for threads in 128 256 512; do
  for cap in 1000 10000 100000 1000000 10000000 100000000; do
    echo "========================================"
    echo "Capacity: $cap | GPU threads: $threads"
    echo "========================================"

    export KP_GPU_THREADS="$threads"

    echo "--- knapsackdp_gpu ---"
    python3 distributedknapsack \
      --test knapsackdp_gpu \
      --capacity "$cap" \
      --numItems "$NUM_ITEMS" \
      --seed "$SEED" \
      --maxWeight "$MAX_WEIGHT"

    echo ""
  done
done
