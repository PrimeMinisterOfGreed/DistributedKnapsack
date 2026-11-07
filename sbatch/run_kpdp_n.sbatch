#!/bin/bash
#SBATCH --job-name=kpdp_n
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --time=00:30:00
#SBATCH --mem=32G
#SBATCH --output=perf_kpdn.out
#SBATCH --error=perf_kpdn.err

# Speedup-vs-p sweep of the classic KnapsackDP with the number of items as
# the scaling variable -- the exact counterpart of sbatch/run_kpdpdag_n.sbatch
# (same capacity 1e6, same seed, same thread ladder) so the two chapters can
# show comparable speedup-vs-n figures.
#   n {50,250,1000,5000} x threads {1,2,4,8,16,32} x 2 repeats,
#   maxWeight=10, wall time only (no perf counters).
#
# Memory: DP table is 4 B/cell -> (5000+1)*(1e6+1)*4B ~= 20 GB -> --mem=32G.
# Runtime estimate: serial times ~0.64/3.2/23/150 s for n=50/250/1000/5000;
# the thread ladder costs ~2x the serial run per n -> ~12 min total.
#
# Output: perf_kpdn_out/speedup_n.csv with columns
#   n,threads,repeat,time_s

set -euo pipefail

SEED=42
CAPACITY=1000000
MAX_WEIGHT=10
BIN="./build/kpbench"
REPEATS=2
OUT_DIR="perf_kpdn_out"
SUMMARY="$OUT_DIR/speedup_n.csv"

export LC_ALL=C
mkdir -p "$OUT_DIR"
echo "n,threads,repeat,time_s" > "$SUMMARY"

if [ ! -x "$BIN" ]; then
  echo "ERROR: $BIN not found or not executable." >&2
  exit 1
fi

for num_items in 50 250 1000 5000; do
  for threads in 1 2 4 8 16 32; do
    export OMP_NUM_THREADS="$threads"

    for rep in $(seq 1 "$REPEATS"); do
      RUN_OUT=$("$BIN" \
        --test knapsackdp \
        --capacity "$CAPACITY" \
        --numItems "$num_items" \
        --seed "$SEED" \
        --numThreads "$threads" \
        --maxWeight "$MAX_WEIGHT" \
        --save "")

      time_s=$(printf '%s\n' "$RUN_OUT" | sed -n "s/^knapsackdp: \([0-9.]*\)s .*/\1/p")
      if [ -z "$time_s" ]; then
        echo "WARNING: could not parse time (n=$num_items t=$threads rep=$rep)" >&2
      fi
      echo "$num_items,$threads,$rep,$time_s" >> "$SUMMARY"
      echo "n=$num_items t=$threads rep=$rep time=${time_s}s"
    done
  done
done

echo "Done. Summary written to $SUMMARY"
