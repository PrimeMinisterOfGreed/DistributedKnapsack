#!/usr/bin/env python3
"""Confronto campagne perf: DP classica column-major (baseline) vs row-major.

Legge i CSV di doc/data/perf (campagna row-major corrente) e
doc/data/perf/baseline_columnmajor (prima campagna, column-major) e stampa
mediane, speedup e contatori per kilo-istruzione. Le medie sono mediane sulle
repliche; ogni run ha 11 righe (una per evento perf) con la stessa time_s.
"""
from pathlib import Path

import pandas as pd

HERE = Path(__file__).resolve().parent
DATA = HERE / "data" / "perf"
NEW = DATA
OLD = DATA / "baseline_columnmajor"
OUT_MD = HERE / "port_comparison.md"

ONE_K = 1000.0


def times(csv_path: Path) -> pd.DataFrame:
    """Una riga per run: chiavi di sweep + time_s (le repliche restano distinte)."""
    df = pd.read_csv(csv_path)
    df["time_s"] = pd.to_numeric(df["time_s"], errors="coerce")
    df = df[df["time_s"].notna()]
    if "item_block" not in df.columns and "tag" in df.columns and df["tag"].str.contains("_ib").any():
        # kpdpdag_perf codifica itemBlock/capBlock nel tag: maxw10_cap10000_t1_ib5_cb1000
        tag = df["tag"].str.extract(r"_ib(?P<item_block>\d+)_cb(?P<cap_block>\d+)")
        df = pd.concat([df, tag], axis=1)
    keys = [c for c in df.columns if c not in
            ("tag", "value", "unit", "event", "runtime_ns", "runtime_pct",
             "metric_value", "metric_unit", "repeat", "time_s")]
    return df.groupby(keys, as_index=False)["time_s"].median()


def compare(name: str, keys: list[str]) -> pd.DataFrame:
    old, new = times(OLD / f"{name}.csv"), times(NEW / f"{name}.csv")
    merged = old.merge(new, on=keys, how="inner", suffixes=("_old", "_new"))
    if merged.empty:
        raise SystemExit(f"ERROR: nessuna chiave in comune old/new per {name}: {keys}")
    merged["speedup"] = merged["time_s_old"] / merged["time_s_new"]
    return merged


def counters(name: str) -> pd.DataFrame:
    """Contatori per kilo-istruzione, mediana sulle repliche, ratio new/old."""
    def per_k(csv: Path) -> pd.DataFrame:
        df = pd.read_csv(csv)
        df["value"] = pd.to_numeric(df["value"], errors="coerce")
        wide = df.pivot_table(index=["max_weight", "capacity", "threads", "repeat"],
                              columns="event", values="value", aggfunc="first").reset_index()
        wide["miss_per_kins"] = wide["cache-misses"] / wide["instructions"] * ONE_K
        wide["l1m_per_kins"] = wide["L1-dcache-load-misses"] / wide["instructions"] * ONE_K
        wide["tlbm_per_kins"] = wide["dTLB-load-misses"] / wide["instructions"] * ONE_K
        wide["ipc"] = wide["instructions"] / wide["cycles"]
        return wide.groupby(["max_weight", "capacity", "threads"])[
            ["miss_per_kins", "l1m_per_kins", "tlbm_per_kins", "ipc"]].median().reset_index()
    old, new = per_k(OLD / f"{name}.csv"), per_k(NEW / f"{name}.csv")
    return old.merge(new, on=["max_weight", "capacity", "threads"],
                     suffixes=("_old", "_new"))


def md_table(df: pd.DataFrame, cols: list[str], fmt: dict) -> str:
    head = "| " + " | ".join(cols) + " |\n|" + "---|" * len(cols) + "\n"
    rows = ""
    for _, r in df.iterrows():
        cells = [fmt[c](r[c]) if c in fmt else str(r[c]) for c in cols]
        rows += "| " + " | ".join(cells) + " |\n"
    return head + rows


out = ["# Confronto campagne perf: column-major (baseline) vs row-major\n",
       "Mediane sulle repliche, stesso sweep, stesso nodo ARM (epito).\n"]

# --- 1. DP classica ---------------------------------------------------------
cmp_dp = compare("perf_kpdp", ["max_weight", "capacity", "threads"])
out.append("\n## 1. DP classica (n=50): tempi e speedup\n")
sel = cmp_dp.sort_values(["capacity", "max_weight", "threads"])
out.append(md_table(sel, ["max_weight", "capacity", "threads", "time_s_old", "time_s_new", "speedup"],
                    {"time_s_old": lambda v: f"{v:.4f}", "time_s_new": lambda v: f"{v:.4f}",
                     "speedup": lambda v: f"{v:.1f}x"}))
ct = counters("perf_kpdp")
out.append("\n### Contatori per kilo-istruzione (classica)\n")
out.append(md_table(ct, ["max_weight", "capacity", "threads", "miss_per_kins_old",
                         "miss_per_kins_new", "l1m_per_kins_old", "l1m_per_kins_new",
                         "tlbm_per_kins_old", "tlbm_per_kins_new", "ipc_old", "ipc_new"],
                    {c: (lambda v: f"{v:.2f}") for c in ct.columns[4:]}))

# --- 2. DAG -----------------------------------------------------------------
cmp_dag = compare("perf_kpdpdag", ["max_weight", "capacity", "threads", "item_block", "cap_block"])
out.append("\n## 2. DAG (n=50): speedup per (threads, itemBlock, capBlock), maxWeight=10, C=1e6\n")
sel = cmp_dag[(cmp_dag.max_weight == 10) & (cmp_dag.capacity == 1000000)]
piv = sel.pivot_table(index=["item_block", "cap_block", "threads"], values="speedup").reset_index()
out.append(md_table(piv, ["item_block", "cap_block", "threads", "speedup"],
                    {"speedup": lambda v: f"{v:.2f}x"}))
out.append("\nGeometrie medie e peggiori:\n")
out.append(f"- speedup mediano: {cmp_dag.speedup.median():.2f}x, minimo {cmp_dag.speedup.min():.2f}x, "
           f"massimo {cmp_dag.speedup.max():.2f}x su {len(cmp_dag)} config\n")
# edges: il dedup si vede nel vecchio report? qui confronto solo tempi.

# --- 3. Big / crossover -----------------------------------------------------
cmp_big = compare("perf_kpdpbig", ["test", "num_items", "max_weight", "capacity", "threads",
                                   "item_block", "cap_block"])
out.append("\n## 3. Sweep big (n=1000/5000): speedup per test\n")
for test in cmp_big.test.unique():
    sel = cmp_big[cmp_big.test == test]
    out.append(f"\n**{test}**: speedup mediano {sel.speedup.median():.2f}x "
               f"(min {sel.speedup.min():.2f}x, max {sel.speedup.max():.2f}x)\n")
    sel = sel.sort_values(["num_items", "threads", "item_block", "cap_block"])
    out.append(md_table(sel, ["num_items", "threads", "item_block", "cap_block",
                              "time_s_old", "time_s_new", "speedup"],
                        {"time_s_old": lambda v: f"{v:.2f}", "time_s_new": lambda v: f"{v:.3f}",
                         "speedup": lambda v: f"{v:.2f}x"}))

# --- 4. Crossover classica vs DAG (row-major vs row-major) ------------------
out.append("\n## 4. Classica vs DAG nello stesso codice row-major (tempo, mediana)\n")
classic = cmp_big[cmp_big.test == "knapsackdp"].groupby(["num_items", "threads"])["time_s_new"].median()
dag = cmp_big[(cmp_big.test == "knapsackdpdag") & (cmp_big.item_block == 10) &
              (cmp_big.cap_block == 10000)].groupby(["num_items", "threads"])["time_s_new"].median()
xover = pd.concat({"classic": classic, "dag": dag}, axis=1).dropna()
xover["dag/classic"] = xover.dag / xover.classic
out.append(md_table(xover.reset_index(), ["num_items", "threads", "classic", "dag", "dag/classic"],
                    {"classic": lambda v: f"{v:.3f}", "dag": lambda v: f"{v:.3f}",
                     "dag/classic": lambda v: f"{v:.2f}"}))

OUT_MD.write_text("\n".join(out))
print(f"Scritto {OUT_MD} ({len(out)} sezioni)")
print("\n=== SINTESI ===")
print(f"classica: speedup mediano {cmp_dp.speedup.median():.1f}x "
      f"(seriale C=1e6: {cmp_dp[(cmp_dp.threads==1)&(cmp_dp.capacity==1000000)].iloc[0].time_s_old:.3f}s -> "
      f"{cmp_dp[(cmp_dp.threads==1)&(cmp_dp.capacity==1000000)].iloc[0].time_s_new:.3f}s)")
print(f"DAG: speedup mediano {cmp_dag.speedup.median():.2f}x")
print(f"big: classic {cmp_big[cmp_big.test=='knapsackdp'].speedup.median():.1f}x, "
      f"dag {cmp_big[cmp_big.test=='knapsackdpdag'].speedup.median():.2f}x")
print(f"crossover row-major (n=1000, t=1): dag/classic = "
      f"{xover.loc[(1000, 1), 'dag/classic']:.2f}x" if (1000, 1) in xover.index else "")
