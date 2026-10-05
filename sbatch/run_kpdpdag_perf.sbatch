#!/bin/bash
#SBATCH --job-name=kpdpdag_perf
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --time=02:00:00
#SBATCH --output=perf_kpdpdag.out
#SBATCH --error=perf_kpdpdag.err
#SBATCH --mem=32G

# Cache-miss investigation for KnapsackDPDAG with perf stat.
# Full sweep of run_kpdpdag.sbatch (max_weight x threads x capacity
# x itemBlock x capBlock, skipping capBlock >= capacity, ~1000 runs)
# wrapped in perf stat.  Cache/TLB/branch events incl. the node's ll_cache
# last-level-cache counters, no perf record.  The target node is ARM
# (armv8_pmuv3 core PMU, arm_cmn mesh, arm_dsu_N L3 clusters, arm_dmc620
# memory controllers); LLC-loads/LLC-load-misses do not exist there.
# Extra core-PMU counters (l2d_cache_refill, l3d_cache_refill, stall_backend,
# mem_access, dtlb_walk, ...) or uncore ones (arm_cmn/hnf_cache_miss/,
# arm_dsu_0/l3d_cache_refill/, arm_dmc620_*/clkdiv2_rdwr/) can be added after
# checking `perf list`; uncore events count PMU instances, not the process,
# and unsupported events make perf fail.
# NOTE: run this on a node/account that is allowed to access hardware
# counters (perf_event_paranoid lowered or proper perms).
# capBlock=0 means "auto": (capacity+1)/10 (see knapsackdpdag.cpp); this
# sweep always passes explicit values so the tag is unambiguous.
#
# Machine-parseable output:
#   perf_kpdpdag_out/perf_summary.csv   all runs merged, one row per
#     (run x event), `perf stat -x ,` CSV rows prefixed with run metadata:
#       max_weight,capacity,threads,item_block,cap_block,tag,
#       value,unit,event,runtime_ns,runtime_pct[,metric_value,metric_unit]
#       runtime_ns = time the event's counter group was scheduled on the PMU;
#       runtime_pct = measurement coverage (100% = no multiplexing).
#     unsupported events appear as value "<not supported>".  This node's perf
#     does not emit duration_time/user_time/system_time rows; take elapsed/
#     user/sys time from the matching "time" field in results_*.json.
#   perf_kpdpdag_out/results_kpdpdag_perf.json  solver timings, one JSON
#     entry per run (testengine appends; entries already carry item_block,
#     cap_block, processors, ...).
# Per-run perf files are NOT kept: each run's rows are appended to the
# summary and the temp file removed, so the summary is complete.
# LC_ALL=C forces '.' as decimal separator so CSV fields stay unambiguous.
# Walltime: median run ~0.1s but worst ~40s (cap=1e6, cap_block=10) plus
# ~1s interpreter startup per run -> budget 2h for the full sweep.

set -euo pipefail

SEED=42
NUM_ITEMS=50
OUT_DIR="perf_kpdpdag_out"
SUMMARY="$OUT_DIR/perf_summary.csv"
RESULTS="$OUT_DIR/results_kpdpdag_perf.json"
SUMMARY_HEADER="max_weight,capacity,threads,item_block,cap_block,tag,value,unit,event,runtime_ns,runtime_pct,metric_value,metric_unit"
PERF_EVENTS="cycles,instructions,cache-references,cache-misses,branch-misses,L1-dcache-loads,L1-dcache-load-misses,ll_cache_rd,ll_cache_miss_rd,dTLB-load-misses,iTLB-load-misses"

export LC_ALL=C
mkdir -p "$OUT_DIR"
rm -f "$SUMMARY"
rm -f "$RESULTS"

echo "perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo n/a)"
if ! perf stat -e "$PERF_EVENTS" true >/dev/null 2>&1; then
  echo "ERROR: perf cannot use the configured PERF_EVENTS. Check perf_event_paranoid" >&2
  echo "       and event availability ('perf list'); aborting before the sweep." >&2
  exit 1
fi

for max_weight in 10 100 1000; do
  for threads in 1 2 4 8 16 32; do
    for cap in 10000 100000 1000000; do
      for item_block in 5 10 15 25; do
        for capacity_block in 1000 10000; do
          if [ "$capacity_block" -ge "$cap" ]; then
            continue
          fi
          tag="maxw${max_weight}_cap${cap}_t${threads}_ib${item_block}_cb${capacity_block}"
          export OMP_NUM_THREADS="$threads"

          perf stat -x , -e "$PERF_EVENTS" \
            -o "$OUT_DIR/perf_last.csv" \
            python3 distributedknapsack \
            --test knapsackdpdag \
            --capacity "$cap" \
            --numItems "$NUM_ITEMS" \
            --seed "$SEED" \
            --numThreads "$threads" \
            --maxWeight "$max_weight" \
            --itemBlock "$item_block" \
            --capBlock "$capacity_block" \
            --save "$RESULTS"

          # Normalize each perf row to 7 fields, prefix run metadata, append;
          # the first fusion writes the CSV header as first line.
          [ -s "$SUMMARY" ] || echo "$SUMMARY_HEADER" > "$SUMMARY"
          awk -F, -v OFS=, -v mw="$max_weight" -v c="$cap" -v p="$threads" \
            -v ib="$item_block" -v cb="$capacity_block" -v tag="$tag" \
            '!/^#/ && NF >= 5 {
               mv = (NF >= 6) ? $6 : ""
               mu = (NF >= 7) ? $7 : ""
               print mw, c, p, ib, cb, tag, $1, $2, $3, $4, $5, mv, mu
             }' \
            "$OUT_DIR/perf_last.csv" >>"$SUMMARY"
          rm -f "$OUT_DIR/perf_last.csv"
        done
      done
    done
  done
done

echo "Outputs in $OUT_DIR/ (perf_summary.csv, results_kpdpdag_perf.json)"
echo "Analyze with:"
echo "  import pandas as pd"
echo "  df = pd.read_csv('$OUT_DIR/perf_summary.csv')"
echo "  # e.g. cache-miss counts per (capacity, item_block, cap_block) across threads:"
echo "  df[df.event == 'cache-misses'].pivot_table(index=['capacity', 'item_block', 'cap_block'], columns='threads', values='value')"
