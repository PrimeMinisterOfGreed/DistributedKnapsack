#!/usr/bin/env python3
"""Grafici di confronto KnapsackDP (row-major) vs KnapsackDPDAG, dalle
campagne doc/data/perf/perf_{kpdp,kpdpdag,kpdpbig}.csv (mediana sulle
repliche). Produce PNG in doc/images/rowmajor/.

Figure:
  time_vs_threads    2 pannelli (n=1000, 5000): tempo vs thread, log-log
  speedup_vs_threads S(p) vs thread + bound strutturale S_max(p) del DAG
  dag_classic_ratio  rapporto tempi DAG/classic vs thread (sotto 1: vince DAG)
  layout_dominates   n=5000: column-major vs row-major, seriale e best
  n50_crosscheck     n=50: classica vs DAG (campagne kpdp_perf/kpdpdag_perf)
"""
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

HERE = Path(__file__).resolve().parent
DATA = HERE / "data" / "perf"
IMG = HERE / "images" / "rowmajor"
IMG.mkdir(parents=True, exist_ok=True)

plt.rcParams.update({
    "figure.dpi": 130,
    "axes.grid": True,
    "grid.alpha": 0.35,
    "axes.axisbelow": True,
    "font.size": 9,
})
C_CLASSIC, C_DAG, C_DAGBEST = "#1f77b4", "#ff7f0e", "#d62728"


def median_times(name: str) -> pd.DataFrame:
    df = pd.read_csv(DATA / f"{name}.csv")
    df["time_s"] = pd.to_numeric(df["time_s"], errors="coerce")
    df = df[df["time_s"].notna()]
    if "item_block" not in df.columns:
        df = pd.concat(
            [df, df["tag"].str.extract(r"_ib(?P<item_block>\d+)_cb(?P<cap_block>\d+)")], axis=1)
        df["item_block"] = df["item_block"].astype(float)
        df["cap_block"] = df["cap_block"].astype(float)
    keys = [c for c in df.columns if c not in
            ("tag", "value", "unit", "event", "runtime_ns", "runtime_pct",
             "metric_value", "metric_unit", "repeat", "time_s")]
    if "test" not in df.columns:
        df["test"] = name.replace("perf_kpdp", "knapsackdp") if name == "perf_kpdp" else "knapsackdpdag"
        keys.append("test")
    return df.groupby(keys, as_index=False)["time_s"].median()


big = median_times("perf_kpdpbig")
kpd = median_times("perf_kpdp")
dag50 = median_times("perf_kpdpdag")

classic_big = big[big.test == "knapsackdp"].sort_values(["num_items", "threads"])
dag_big = big[big.test == "knapsackdpdag"]


def dag_best(d: pd.DataFrame) -> pd.DataFrame:
    """Migliore geometria (item_block, cap_block) per ogni (num_items, threads)."""
    idx = d.groupby(["num_items", "threads"])["time_s"].idxmin()
    return d.loc[idx].sort_values(["num_items", "threads"])


dag_big_best = dag_best(dag_big)
THREADS = [1, 4, 8, 16, 32]


def smax(nb: int, nq: int, p: float) -> float:
    """Bound strutturale S_max(p) del solver a livelli (frontiere anti-diagonali)."""
    span_cost = sum(
        int(np.ceil(min(L, nb, nq, nb + nq - 1 - L) / p)) for L in range(1, nb + nq))
    return nb * nq / span_cost


# ---------------------------------------------------------------- fig. 1: tempi
fig, axes = plt.subplots(1, 2, figsize=(10, 3.8), sharey=False)
for ax, n in zip(axes, [1000, 5000]):
    c = classic_big[classic_big.num_items == n]
    d10 = dag_big[(dag_big.num_items == n) & (dag_big.item_block == 10) & (dag_big.cap_block == 10000)]
    db = dag_big_best[dag_big_best.num_items == n]
    ax.plot(c.threads, c.time_s, "o-", color=C_CLASSIC, label="classica")
    ax.plot(d10.threads, d10.time_s, "s--", color=C_DAG, label="DAG (ib=10, cb=10000)")
    ax.plot(db.threads, db.time_s, "D-", color=C_DAGBEST, label="DAG, geometria migliore")
    for _, r in db.iterrows():
        ax.annotate(f"ib{int(r.item_block)}/cb{int(r.cap_block)}", (r.threads, r.time_s),
                    textcoords="offset points", xytext=(4, 6), fontsize=7, color=C_DAGBEST)
    ax.set(xscale="log", yscale="log", xticks=THREADS, xticklabels=THREADS,
           xlabel="thread", ylabel="tempo [s]", title=f"n = {n}, C = 10$^6$")
    ax.legend()
fig.suptitle("Tempo vs thread: classica e DAG sullo stesso codice", y=1.02)
fig.tight_layout()
fig.savefig(IMG / "time_vs_threads.png", bbox_inches="tight")
plt.close(fig)

# ------------------------------------------------------- fig. 2: speedup + S_max
fig, axes = plt.subplots(1, 2, figsize=(10, 3.8))
for ax, n in zip(axes, [1000, 5000]):
    c = classic_big[classic_big.num_items == n].set_index("threads")
    db = dag_big_best[dag_big_best.num_items == n].set_index("threads")
    p = np.array(THREADS)
    ax.plot(p, c.time_s[1] / c.time_s.loc[p], "o-", color=C_CLASSIC, label="classica")
    ax.plot(p, db.time_s[1] / db.time_s.loc[p], "D-", color=C_DAGBEST, label="DAG, geometria migliore")
    nb, nq = n // 10, 101  # geometria ib=10, cb=10000 della curva di riferimento
    ax.plot(p, [smax(nb, nq, x) for x in p], ":", color="#2ca02c",
            label=f"$S_{{max}}(p)$ teorico DAG (ib=10, cb=10$^4$)")
    ax.axhline(1, color="k", lw=0.7)
    ax.set(xscale="log", xticks=p, xticklabels=p, xlabel="thread", ylabel="speedup $S(p)$",
           title=f"n = {n}")
    ax.legend(loc="upper left")
fig.suptitle("Speedup vs thread: la classica satura sulla banda, il DAG resta sotto il suo limite strutturale",
             y=1.02)
fig.tight_layout()
fig.savefig(IMG / "speedup_vs_threads.png", bbox_inches="tight")
plt.close(fig)

# ------------------------------------------------------------- fig. 3: rapporto
fig, ax = plt.subplots(figsize=(6, 4))
for n in [1000, 5000]:
    c = classic_big[classic_big.num_items == n].set_index("threads").time_s
    db = dag_big_best[dag_big_best.num_items == n].set_index("threads").time_s
    ratio = db.loc[c.index] / c
    ax.plot(ratio.index, ratio.values, "o-", label=f"n = {n}")
c50 = kpd[kpd.capacity == 1000000].groupby("threads").time_s.median()
d50 = dag50[dag50.capacity == 1000000].groupby(["threads"]).time_s.min()
ax.plot(c50.index, (d50.loc[c50.index] / c50).values, "o--", color="#7f7f7f",
        label="n = 50 (campagne perf)")
ax.axhline(1.0, color="r", ls="--", lw=1, label="parità")
ax.set(xscale="log", yscale="log", xticks=THREADS, xticklabels=THREADS,
       xlabel="thread", ylabel="tempo DAG / tempo classica",
       title="Con il layout corretto il DAG vince solo a 32 thread")
ax.legend()
fig.tight_layout()
fig.savefig(IMG / "dag_classic_ratio.png", bbox_inches="tight")
plt.close(fig)

# ------------------------------------------------- fig. 4: il layout domina
base = pd.read_csv(DATA / "baseline_columnmajor" / "perf_kpdpbig.csv")
base["time_s"] = pd.to_numeric(base.time_s, errors="coerce")
base_c = base[(base.test == "knapsackdp") & (base.num_items == 5000)].groupby("threads").time_s.median()
labels = ["classica\ncolumn-major\nt=1", "classica\nt=1", "classica\nt=8 (best)",
          "DAG\nt=1", "DAG\nt=32 (best)"]
vals = [base_c[1],
        classic_big[(classic_big.num_items == 5000) & (classic_big.threads == 1)].time_s.iloc[0],
        classic_big[classic_big.num_items == 5000].time_s.min(),
        dag_big[dag_big.num_items == 5000].groupby("threads").time_s.min()[1],
        dag_big[dag_big.num_items == 5000].time_s.min()]
fig, ax = plt.subplots(figsize=(7, 4))
bars = ax.bar(range(len(vals)), vals, color=["#8c564b", C_CLASSIC, C_CLASSIC, C_DAG, C_DAGBEST])
for b, v in zip(bars, vals):
    ax.text(b.get_x() + b.get_width() / 2, v * 1.15, f"{v:.2f} s", ha="center", fontsize=9)
ax.set_xticks(range(len(vals)), labels)
ax.set(yscale="log", ylabel="tempo [s]",
       title="n = 5000, C = 10$^6$: il layout vale ~49x, la scelta dell'algoritmo ~4x")
fig.tight_layout()
fig.savefig(IMG / "layout_dominates.png", bbox_inches="tight")
plt.close(fig)

# ------------------------------------------------------ fig. 5: controllo n=50
fig, ax = plt.subplots(figsize=(6.5, 4))
c50f = kpd[kpd.capacity == 1000000].groupby("threads").time_s.median()
ax.plot(c50f.index, c50f.values, "o-", color=C_CLASSIC, label="classica")
for (ib, cb), g in dag50[dag50.capacity == 1000000].groupby(["item_block", "cap_block"]):
    gg = g.groupby("threads").time_s.median()
    ax.plot(gg.index, gg.values, ".-", alpha=0.35, label=f"DAG ib={int(ib)} cb={int(cb)}")
ax.set(xscale="log", yscale="log", xlabel="thread", ylabel="tempo [s]",
       title="n = 50, C = 10$^6$: già a pochi oggetti la classica è davanti")
ax.legend(fontsize=7, ncol=2)
fig.tight_layout()
fig.savefig(IMG / "n50_crosscheck.png", bbox_inches="tight")
plt.close(fig)

# --------------------------------------------------------------- tabella chat
print("=== Classica vs DAG (row-major), mediana [s]; DAG best = min su (ib, cb) ===\n")
hdr = f"{'n':>5} {'t':>3} | {'classic':>8} | {'DAG best':>8} | {'ratio DAG/C':>10} | best (ib,cb)"
print(hdr)
for n in [1000, 5000]:
    c = classic_big[classic_big.num_items == n].set_index("threads").time_s
    d = dag_big[dag_big.num_items == n]
    for t in THREADS:
        g = d[d.threads == t]
        i = g.time_s.idxmin()
        print(f"{n:>5} {t:>3} | {c[t]:>8.3f} | {g.time_s[i]:>8.3f} | {g.time_s[i]/c[t]:>9.2f}x | "
              f"({int(g.item_block[i])},{int(g.cap_block[i])})")
print("\nns/cella seriali: classic n=5000 = %.2f | DAG n=5000 = %.2f" % (
    c[1] / (5001 * 1_000_000) * 1e9, d[d.threads == 1].time_s.min() / (5001 * 1_000_000) * 1e9))
print("\nFigure scritte in", IMG)
for f in sorted(IMG.glob("*.png")):
    print(" ", f.name)
