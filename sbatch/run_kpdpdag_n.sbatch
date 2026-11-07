#!/bin/bash
#SBATCH --job-name=kpdpdag_n
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --time=00:30:00
#SBATCH --mem=64G
#SBATCH --output=perf_dagn.out
#SBATCH --error=perf_dagn.err

# Speedup-vs-p sweep of KnapsackDPDAG with the number of items as the
# scaling variable. The n=50 campaign (perf_kpdpdag_speedup.png) saturates
# at ~2.4x; this sweep adds n=250/1000/5000 (same capacity 1e6, same seed)
# so the speedup curves can be plotted per n and the crossover between
# frontier-limited and work-limited regimes becomes visible.
#   n {50,250,1000,5000} x threads {1,2,4,8,16,32}
#     x (itemBlock,capBlock) {(25,1e4),(5,1e4),(25,1e3),(5,1e3)}
#     x 2 repeats, maxWeight=10, wall time only (no perf counters).
#
# cap_block=1e3 exposes frontier widths up to min(nb,1001) (vs 101 at
# cb=1e4), testing whether the ~8.4x plateau of the cb=1e4 sweep is a
# frontier-width limit or a synchronization-per-level limit.
#
# Memory: worst case n=5000, itemBlock=5, capBlock=1e3 -> 1000*1001 tiles
# x 6x1e3 ints (~24 KB each) ~= 24 GB graph + 20 GB DP table -> --mem=64G.
# Runtime estimate: n=5000 serial ~20-30 s per config, ~2x serial summed
# over the thread ladder -> whole sweep well under 30 min.
#
# Output: perf_dagn_out/speedup_n.csv with columns
#   n,threads,item_block,cap_block,repeat,time_s

set -euo pipefail

SEED=42
CAPACITY=1000000
MAX_WEIGHT=10
BIN="./build/kpbench"
REPEATS=2
OUT_DIR="perf_dagn_out"
SUMMARY="$OUT_DIR/speedup_n.csv"

export LC_ALL=C
mkdir -p "$OUT_DIR"
echo "n,threads,item_block,cap_block,repeat,time_s" > "$SUMMARY"

if [ ! -x "$BIN" ]; then
  echo "ERROR: $BIN not found or not executable." >&2
  exit 1
fi

for num_items in 50 250 1000 5000; do
  for threads in 1 2 4 8 16 32; do
    for item_block in 25 5; do
      for cap_block in 10000 1000; do
        export OMP_NUM_THREADS="$threads"

        for rep in $(seq 1 "$REPEATS"); do
          RUN_OUT=$("$BIN" \
            --test knapsackdpdag \
            --capacity "$CAPACITY" \
            --numItems "$num_items" \
            --seed "$SEED" \
            --numThreads "$threads" \
            --maxWeight "$MAX_WEIGHT" \
            --itemBlock "$item_block" \
            --capBlock "$cap_block" \
            --save "")

          time_s=$(printf '%s\n' "$RUN_OUT" | sed -n "s/^knapsackdpdag: \([0-9.]*\)s .*/\1/p")
          if [ -z "$time_s" ]; then
            echo "WARNING: could not parse time (n=$num_items t=$threads ib=$item_block rep=$rep)" >&2
          fi
          echo "$num_items,$threads,$item_block,$cap_block,$rep,$time_s" >> "$SUMMARY"
          echo "n=$num_items t=$threads ib=$item_block cb=$cap_block rep=$rep time=${time_s}s"
        done
      done
    done
  done
done

echo "Done. Summary written to $SUMMARY"
