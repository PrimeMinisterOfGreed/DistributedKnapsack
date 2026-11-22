#!/usr/bin/env python3
"""Rigenera le due figure speedup non coperte da kpdp_perf_analysis.py, dai
CSV delle nuove campagne (row-major):

  images/kpdp_speedup_vs_processors.png  classica (phases_kpdp_compute.csv):
                                         speedup della fase compute vs thread
                                         per n in {50, 1000, 5000} e capacity
                                         (maxWeight 10, thread {1,8,32})
  images/perf_kpdpdag_speedup_vs_n.png   DAG, capacity 1e6, geometria migliore
                                         per punto: speedup vs thread per
                                         n in {50, 1000, 5000}
                                         (perf_kpdpdag.csv + perf_kpdpbig.csv;
                                         la vecchia campagna includeva anche
                                         n=250, non presente nei dati nuovi)
"""
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd

BASE = Path(__file__).resolve().parent
DATA = BASE / "data" / "perf"
OUT = BASE / "images"
OUT.mkdir(exist_ok=True)
plt.rcParams.update({"figure.dpi": 150, "axes.grid": True, "grid.alpha": 0.3,
                     "axes.axisbelow": True})


def med_times(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    df["time_s"] = pd.to_numeric(df["time_s"], errors="coerce")
    return df[df.time_s.notna()]


# --- classica: speedup della fase compute -----------------------------------
# I tempi di wall della campagna (perf_kpdp.csv) includono il first touch della
# tabella, che in parallelo rallenta invece di accelerare; lo speedup del kernel
# si legge meglio sulla fase compute (phases_kpdp_compute.csv, stesso protocollo
# e stessa macchina della campagna, maxWeight=10).
phases = pd.read_csv(DATA / "phases_kpdp_compute.csv")
phases = phases[phases.test == "knapsackdp"]
fig, ax = plt.subplots(figsize=(7, 4.5))
for (n, cap), g in phases.groupby(["n", "capacity"]):
    s = g.groupby("threads").compute_s.median()
    speedup = s[1] / s
    ax.plot(speedup.index, speedup.values, "o-",
            label=f"n={n}, C={cap:.0e}")
ax.plot([1, 32], [1, 32], "k--", lw=0.8, label="ideale")
ax.set(xscale="log", xticks=[1, 8, 32], xticklabels=[1, 8, 32],
       xlabel="thread", ylabel="speedup compute $S_c(p) = C(1)/C(p)$")
ax.legend(fontsize=8)
fig.tight_layout()
fig.savefig(OUT / "kpdp_speedup_vs_processors.png")
plt.close(fig)
print("saved kpdp_speedup_vs_processors.png")

# --- DAG speedup vs n: fase compute ------------------------------------------
# Lo speedup di wall della campagna (perf_kpdpdag.csv + perf_kpdpbig.csv) è
# dominato da first touch e teardown; lo speedup del kernel si legge sulla fase
# compute (phases_kpdpdag_compute.csv: stesso protocollo, geometria migliore
# per punto come nel grafico di wall, mediana su 3 repliche).
ph = pd.read_csv(DATA / "phases_kpdpdag_compute.csv")

fig, ax = plt.subplots(figsize=(7, 4.5))
for n, g in ph.groupby("n"):
    per_t = g.groupby("threads").compute_s.median()
    ax.plot(per_t.index, per_t[1] / per_t, "o-", label=f"n = {n}")
ax.plot([1, 32], [1, 32], "k--", lw=0.8, label="ideale")
ax.set(xscale="log", xticks=[1, 2, 4, 8, 16, 32], xticklabels=[1, 2, 4, 8, 16, 32],
       xlabel="thread", ylabel="speedup compute $S_c(p) = C(1)/C(p)$")
ax.legend(fontsize=8)
fig.tight_layout()
fig.savefig(OUT / "perf_kpdpdag_speedup_vs_n.png")
plt.close(fig)
print("saved perf_kpdpdag_speedup_vs_n.png")
