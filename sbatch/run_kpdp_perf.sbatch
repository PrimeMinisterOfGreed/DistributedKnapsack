#!/bin/bash
#SBATCH --job-name=kpdp_perf
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --time=00:30:00
#SBATCH --output=perf_kpdp.out
#SBATCH --error=perf_kpdp.err
#SBATCH --mem=32G

# Cache-miss investigation for KnapsackDP with perf stat.
# Small subset of run_kpdp.sbatch: max_weight {10,1000} x threads {1,8,32}
# x capacity {1e6,1e7}.  Generic cache/TLB/branch events, no perf record.
# On AMD Zen nodes the vendor counters l2_cache_misses, l3_cache_misses,
# l2_latency.l2_cycles_waiting_on_fills etc. can be added to PERF_EVENTS
# (check availability with `perf list`; unsupported events make perf fail).
# NOTE: this PC has perf_event_paranoid=4, so run this on a node/account that
# is allowed to access hardware counters (lower the sysctl or run with perms).

set -euo pipefail

SEED=42
NUM_ITEMS=50
OUT_DIR="perf_kpdp_out"
PERF_EVENTS="cycles,instructions,cache-references,cache-misses,branch-misses,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses,dTLB-load-misses,iTLB-load-misses"

mkdir -p "$OUT_DIR"

echo "perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo n/a)"
if ! perf stat -e cycles true >/dev/null 2>&1; then
  echo "WARNING: perf cannot access hardware counters; results will be empty." >&2
fi

for max_weight in 10 1000; do
  for threads in 1 8 32; do
    for cap in 1000000 10000000; do
      tag="maxw${max_weight}_cap${cap}_t${threads}"
      echo "========================================"
      echo "knapsackdp | Capacity: $cap | Threads: $threads | MaxWeight: $max_weight"
      echo "========================================"

      export OMP_NUM_THREADS="$threads"

      perf stat -e "$PERF_EVENTS" \
        -o "$OUT_DIR/perf_${tag}.txt" \
        python3 distributedknapsack \
          --test knapsackdp \
          --capacity "$cap" \
          --numItems "$NUM_ITEMS" \
          --seed "$SEED" \
          --numThreads "$threads" \
          --maxWeight "$max_weight" \
          --save "$OUT_DIR/results_${tag}.json"

      echo ""
    done
  done
done

echo "Perf outputs in $OUT_DIR/"
echo "Aggregate with: grep -H 'cache-misses' $OUT_DIR/perf_*.txt"
