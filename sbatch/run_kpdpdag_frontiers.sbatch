#!/bin/bash
#SBATCH --job-name=kpdpdag_fr
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --time=01:00:00
#SBATCH --mem=64G
#SBATCH --output=perf_frontiers.out
#SBATCH --error=perf_frontiers.err

# Frontier-width statistics sweep for KnapsackDPDAG: item_block {5,10,25} x
# cap_block {100,1000,10000} for n {50,5000}, capacity 1e6, seed 42.
# One run per configuration (frontier stats do not depend on the thread
# count: levels are recomputed serially after the solve); 32 threads only
# to keep the n=5000/cap_block=100 solve (~1e7 tiles, ~6e9 DP cells)
# within the wall time.  Memory worst case: ~24 GB of tile block matrices
# + ~20 GB DP table -> --mem=64G.
#
# Output: perf_frontiers_out/frontiers.csv with columns
#   n,item_block,cap_block,tiles,edges,levels,min,median,mean,max

set -euo pipefail

SEED=42
CAPACITY=1000000
BIN="./build/kpbench"
OUT_DIR="perf_frontiers_out"
SUMMARY="$OUT_DIR/frontiers.csv"

export LC_ALL=C
export OMP_NUM_THREADS=32
mkdir -p "$OUT_DIR"
echo "n,item_block,cap_block,tiles,edges,levels,min,median,mean,max" > "$SUMMARY"

if [ ! -x "$BIN" ]; then
  echo "ERROR: $BIN not found or not executable." >&2
  exit 1
fi

for num_items in 50 5000; do
  for item_block in 5 10 25; do
    for cap_block in 100 1000 10000; do
      echo "=== n=$num_items ib=$item_block cb=$cap_block ==="
      RUN_OUT=$("$BIN" \
        --test knapsackdpdag \
        --capacity "$CAPACITY" \
        --numItems "$num_items" \
        --seed "$SEED" \
        --maxWeight 10 \
        --itemBlock "$item_block" \
        --capBlock "$cap_block" \
        --save "")

      stats=$(printf '%s\n' "$RUN_OUT" | grep "DAG stats:" | sed 's/.*tiles=\([0-9]*\) edges=\([0-9]*\) levels=\([0-9]*\)/\1 \2 \3/')
      wave=$(printf '%s\n' "$RUN_OUT" | grep "wavefront widths:" | sed 's/.*min=\([0-9.]*\) median=\([0-9.]*\) mean=\([0-9.]*\) max=\([0-9.]*\)/\1 \2 \3 \4/')
      echo "$num_items,$item_block,$cap_block,$stats,$wave" | tr ' ' ',' >> "$SUMMARY"
      echo "tiles/edges/levels: $stats | wavefront: $wave"
    done
  done
done

echo "Done. Summary written to $SUMMARY"
