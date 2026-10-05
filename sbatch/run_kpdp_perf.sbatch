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
# x capacity {1e6,1e7}.  Cache/TLB/branch events incl. the node's ll_cache
# last-level-cache counters, no perf record.  The target node is ARM
# (armv8_pmuv3 core PMU, arm_cmn mesh, arm_dsu_N L3 clusters, arm_dmc620
# memory controllers); LLC-loads/LLC-load-misses do not exist there.
# Extra core-PMU counters (l2d_cache_refill, l3d_cache_refill, stall_backend,
# mem_access, dtlb_walk, ...) or uncore ones (arm_cmn/hnf_cache_miss/,
# arm_dsu_0/l3d_cache_refill/, arm_dmc620_*/clkdiv2_rdwr/) can be added after
# checking `perf list`; uncore events count PMU instances, not the process,
# and unsupported events make perf fail.
# NOTE: this PC has perf_event_paranoid=4, so run this on a node/account that
# is allowed to access hardware counters (lower the sysctl or run with perms).
#
# Machine-parseable output:
#   perf_kpdp_out/perf_<tag>.csv     one per run, `perf stat -x ,` CSV rows:
#       value,unit,event,runtime_ns,runtime_pct[,metric_value,metric_unit]
#       runtime_ns = time the event's counter group was scheduled on the PMU;
#       runtime_pct = measurement coverage (100% = no multiplexing).
#     unsupported events appear as value "<not supported>".  This node's perf
#     does not emit duration_time/user_time/system_time rows; take elapsed/
#     user/sys time from the matching "time" field in results_*.json.
#   perf_kpdp_out/perf_summary.csv   all runs merged:
#       max_weight,capacity,threads,tag,<7 perf CSV columns above>
#   perf_kpdp_out/results_<tag>.json solver timings (JSON, already parseable).
# LC_ALL=C forces '.' as decimal separator so CSV fields stay unambiguous.

set -euo pipefail

SEED=42
NUM_ITEMS=50
OUT_DIR="perf_kpdp_out"
SUMMARY="$OUT_DIR/perf_summary.csv"
SUMMARY_HEADER="max_weight,capacity,threads,tag,value,unit,event,runtime_ns,runtime_pct,metric_value,metric_unit"
PERF_EVENTS="cycles,instructions,cache-references,cache-misses,branch-misses,L1-dcache-loads,L1-dcache-load-misses,ll_cache_rd,ll_cache_miss_rd,dTLB-load-misses,iTLB-load-misses"

export LC_ALL=C
mkdir -p "$OUT_DIR"
rm -f "$SUMMARY"

echo "perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo n/a)"
if ! perf stat -e "$PERF_EVENTS" true >/dev/null 2>&1; then
  echo "ERROR: perf cannot use the configured PERF_EVENTS. Check perf_event_paranoid" >&2
  echo "       and event availability ('perf list'); aborting before the sweep." >&2
  exit 1
fi

for max_weight in 10 1000; do
  for threads in 1 8 32; do
    for cap in 1000000 10000000; do
      tag="maxw${max_weight}_cap${cap}_t${threads}"
      export OMP_NUM_THREADS="$threads"

      perf stat -x , -e "$PERF_EVENTS" \
        -o "$OUT_DIR/perf_${tag}.csv" \
        python3 distributedknapsack \
          --test knapsackdp \
          --capacity "$cap" \
          --numItems "$NUM_ITEMS" \
          --seed "$SEED" \
          --numThreads "$threads" \
          --maxWeight "$max_weight" \
          --save "$OUT_DIR/results_${tag}.json"

      # Normalize each perf row to 7 fields, prefix run metadata, append;
      # the first fusion writes the CSV header as first line.
      [ -s "$SUMMARY" ] || echo "$SUMMARY_HEADER" > "$SUMMARY"
      awk -F, -v OFS=, -v mw="$max_weight" -v c="$cap" -v p="$threads" -v tag="$tag" \
        '!/^#/ && NF >= 5 {
           mv = (NF >= 6) ? $6 : ""
           mu = (NF >= 7) ? $7 : ""
           print mw, c, p, tag, $1, $2, $3, $4, $5, mv, mu
         }' \
        "$OUT_DIR/perf_${tag}.csv" >> "$SUMMARY"

      echo ""
    done
  done
done

echo "Perf outputs in $OUT_DIR/ (per-run: perf_<tag>.csv, merged: perf_summary.csv)"
echo "Analyze with:"
echo "  import pandas as pd"
echo "  df = pd.read_csv('$OUT_DIR/perf_summary.csv')"
echo "  # e.g. cache-miss counts per (max_weight, capacity) across threads:"
echo "  df[df.event == 'cache-misses'].pivot_table(index=['max_weight', 'capacity'], columns='threads', values='value')"
