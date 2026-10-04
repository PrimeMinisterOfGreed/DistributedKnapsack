"""Compare local KnapsackDP speedup against the reference cluster run.

The reference sweep (doc/data/result_kpdp.json, host epito02, sbatch/run_kpdp.sbatch)
and a local subset (default /tmp/opencode/kpdp_local.json, produced with
OMP_NUM_THREADS=...) share the same problem definition (numItems=50, seed=42,
minWeight=1, maxWeight=10, minValue/maxValue defaults).  This script computes
S(p) = T(1)/T(p) for both and reports whether the scaling behaviour matches.

Run with:  ~/pyenv/sci/bin/python doc/kpdp_compare.py
"""

from __future__ import annotations

import json
import os
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

BASE_DIR = Path(__file__).resolve().parent
REF_FILE = BASE_DIR / "data" / "result_kpdp.json"
LOCAL_FILE = Path(os.environ.get("KPDP_LOCAL_FILE", "/tmp/opencode/kpdp_local.json"))
OUT_FILE = BASE_DIR / "images" / "kpdp_speedup_local_vs_ref.png"

FAMILY = 10
CAPS = [100_000, 1_000_000, 10_000_000]


def load(path: Path, family: int) -> pd.DataFrame:
    """Load one benchmark file and derive per-capacity parallel metrics.

    T(c, p)     = mean wall time over repeats
    S(c, p)     = T(c, 1) / T(c, p)          speedup
    E(c, p)     = S(c, p) / p                efficiency
    """
    with path.open() as fh:
        records = json.load(fh)
    df = pd.DataFrame(records)
    if "family" not in df.columns:
        df["family"] = df["max_weight"]
    df = df[df["family"] == family].copy()
    df["capacity"] = df["capacity"].astype(int)
    df["processors"] = df["processors"].astype(int)
    grouped = df.groupby(["capacity", "processors"])["time"].mean().reset_index()
    baseline = (
        grouped[grouped["processors"] == 1].set_index("capacity")["time"].rename("t_base")
    )
    grouped = grouped.merge(baseline, on="capacity")
    grouped["speedup"] = grouped["t_base"] / grouped["time"]
    grouped["efficiency"] = grouped["speedup"] / grouped["processors"]
    return grouped


def main() -> None:
    if not LOCAL_FILE.exists():
        raise SystemExit(f"local results not found: {LOCAL_FILE}")

    ref = load(REF_FILE, FAMILY)
    loc = load(LOCAL_FILE, FAMILY)
    caps = [c for c in CAPS if c in set(ref["capacity"]) & set(loc["capacity"])]

    print(f"reference: {REF_FILE}")
    print(f"local:     {LOCAL_FILE}\n")

    rows = []
    for cap in caps:
        for proc in sorted(set(ref[ref["capacity"] == cap]["processors"])):
            r = ref[(ref["capacity"] == cap) & (ref["processors"] == proc)].iloc[0]
            l = loc[(loc["capacity"] == cap) & (loc["processors"] == proc)]
            if l.empty:
                continue
            l = l.iloc[0]
            rows.append(
                {
                    "capacity": cap,
                    "processors": proc,
                    "ref_time": r["time"],
                    "loc_time": l["time"],
                    "ref_speedup": r["speedup"],
                    "loc_speedup": l["speedup"],
                    "speedup_ratio": l["speedup"] / r["speedup"],
                    "ref_eff": r["efficiency"],
                    "loc_eff": l["efficiency"],
                }
            )
    table = pd.DataFrame(rows)
    pd.set_option("display.width", 180)
    pd.set_option("display.max_columns", 25)
    print("=== Reference vs local (max_weight=10) ===")
    print(table.round(4).to_string(index=False))

    print("\n=== Summary: peak speedup ===")
    for name, df in (("reference", ref), ("local", loc)):
        peaks = df.groupby("capacity")["speedup"].max()
        eff_best = df[df["processors"] == df["processors"].max()].groupby("capacity")[
            "efficiency"
        ].mean()
        print(f"{name}: peak speedup per capacity")
        print(pd.concat({"peak_speedup": peaks, "eff_at_max_p": eff_best}, axis=1).round(4).to_string())

    # For each capacity c and thread count p:
    #   S_ref(c, p) = T_ref(c, 1) / T_ref(c, p)
    #   S_loc(c, p) = T_loc(c, 1) / T_loc(c, p)
    # The ideal reference line is S_ideal(p) = p.
    fig, axes = plt.subplots(1, len(caps), figsize=(5.5 * len(caps), 5), sharey=True)
    if len(caps) == 1:
        axes = [axes]
    procs_ref = sorted(ref["processors"].unique())
    for ax, cap in zip(axes, caps):
        for name, df, style in (("reference (epito02)", ref, "o-"), ("local (Ryzen 5950X)", loc, "s--")):
            curve = df[df["capacity"] == cap].sort_values("processors")
            ax.plot(curve["processors"], curve["speedup"], style, label=name)
        ax.plot(procs_ref, procs_ref, "k:", linewidth=1, label="ideal")
        ax.set_xscale("log", base=2)
        ax.set_xticks(procs_ref)
        ax.set_xticklabels([str(p) for p in procs_ref])
        ax.set_xlabel("processors (threads)")
        ax.set_title(f"capacity={cap:,}")
        ax.grid(True, which="both", alpha=0.3)
        ax.legend(fontsize=8)
    axes[0].set_ylabel("speedup  T(1)/T(p)")
    fig.suptitle("KnapsackDP speedup: local subset vs reference cluster (max_weight=10)")
    fig.tight_layout()
    fig.savefig(OUT_FILE, dpi=150)
    plt.close(fig)
    print(f"\nsaved {OUT_FILE}")


if __name__ == "__main__":
    main()
