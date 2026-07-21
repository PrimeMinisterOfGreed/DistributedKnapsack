#!/bin/bash
#SBATCH --job-name=knapsack_mpi
#SBATCH --ntasks=32
#SBATCH --cpus-per-task=1
#SBATCH --time=02:00:00
#SBATCH --output=mpi.out
#SBATCH --error=mpi.err

set -euo pipefail


SEED=42
NUM_ITEMS=50
MAX_WEIGHT=5000


for tasks in 4 8 16 32; do
    for cap in 1000 10000 100000 1000000 10000000 100000000; do
        echo "========================================"
        echo "Capacity: $cap | MPI tasks: $tasks"
        echo "========================================"

        echo "--- knapsackcopa_mpi ---"
        srun -n "$tasks" --cpus-per-task=1 python distributedknapsack \
            --test knapsackcopa_mpi \
            --capacity "$cap" \
            --numItems "$NUM_ITEMS" \
            --numThreads 1 \
            --seed "$SEED" \
            --maxWeight "$MAX_WEIGHT" \

        echo ""
        echo "--- knapsackdp_mpi ---"
        srun -n "$tasks" --cpus-per-task=1 python distributedknapsack \
            --test knapsackdp_mpi \
            --capacity "$cap" \
            --numItems "$NUM_ITEMS" \
            --numThreads 1 \
            --seed "$SEED" \
            --maxWeight "$MAX_WEIGHT" \

        echo ""
    done
done

echo "All MPI tests completed"
