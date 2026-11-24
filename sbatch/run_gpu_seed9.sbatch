#!/bin/bash
#SBATCH --job-name=knapsack_gpu_seed9
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --gres=gpu:1
#SBATCH --time=00:30:00
#SBATCH --output=gpu_seed9.out
#SBATCH --error=gpu_seed9.err
#SBATCH --mem=32G

# Robustness of the existing GPU block-size sweep: same instances and grid,
# with 21 replicates per configuration and forward/reverse configuration order
# per cycle. Data are stored separately from the original campaign.
set -euo pipefail

SEEDS=(42 137 2718 31415 65537 104729 20240617 23001 777)
NUM_ITEMS=50
BIN="./build/kpbench"
TEST="knapsackdp_gpu"
OUT_DIR="gpu_seed9_out"
SUMMARY="$OUT_DIR/gpu_summary.csv"

mkdir -p "$OUT_DIR"
echo "max_weight,capacity,threads,repeat,time_s,dp_ms,elapsed_s,seed" > "$SUMMARY"

configs=()
for max_weight in 10 1000; do
  for threads in 32 64 128 256 512 1024; do
    for cap in 100000 1000000 10000000; do
      configs+=("$max_weight,$cap,$threads")
    done
  done
done

for SEED in "${SEEDS[@]}"; do
  for rep in $(seq 1 5); do
  if ((rep % 2 == 1)); then
    order=(${configs[@]})
  else
    order=()
    for ((i=${#configs[@]} - 1; i >= 0; --i)); do
      order+=("${configs[i]}")
    done
  fi

  for config in "${order[@]}"; do
    IFS=',' read -r max_weight cap threads <<< "$config"
    export KP_GPU_THREADS="$threads"
    run_start=$SECONDS
    RUN_OUT=$("$BIN" \
      --test "$TEST" --capacity "$cap" --numItems "$NUM_ITEMS" \
      --seed "$SEED" --maxWeight "$max_weight" --save "")
    elapsed_s=$((SECONDS - run_start))

    time_s=$(printf '%s\n' "$RUN_OUT" | \
      sed -n "s/^${TEST}: \([0-9.]*\)s .*/\1/p")
    dp_ms=$(printf '%s\n' "$RUN_OUT" | \
      sed -n "s/^GPU DP execution: \([0-9.]*\) ms/\1/p")
    echo "$max_weight,$cap,$threads,$rep,$time_s,$dp_ms,$elapsed_s,$SEED" >> "$SUMMARY"
  done
done
done

echo "BATCH_ELAPSED_SECONDS=$((SECONDS))"