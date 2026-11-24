"""Build figures from the expanded GPU/DAG measurement campaigns.

Only the measured fields are used: dp_ms/time_s for the GPU and wall time
for the DAG. No compute-phase estimates are inferred from these files.
"""
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

BASE_DIR = Path(__file__).resolve().parent
DATA = BASE_DIR / "data" / "perf"
IMAGES = BASE_DIR / "images"
BASE_SEED = 42


def plot_gpu_repetitions() -> None:
    df = pd.read_csv(DATA / "perf_kpdpgpu_stat21.csv")
    stats = df.groupby(["max_weight", "capacity", "threads"], as_index=False).dp_ms.median()
    counts = df.groupby(["max_weight", "capacity", "threads"]).size()
    assert (counts == 21).all()
    colors = {100000: "#1f77b4", 1000000: "#2ca02c", 10000000: "#d62728"}
    threads = sorted(df.threads.unique())
    families = sorted(df.max_weight.unique())
    fig, axes = plt.subplots(2, len(families), figsize=(13.5, 8), sharex=True, squeeze=False)
    if len(families) == 1:
        axes = np.asarray([axes])

    for j, family in enumerate(families):
        top, bottom = axes[0, j], axes[1, j]
        for cap in sorted(df.capacity.unique()):
            s = stats[(stats.max_weight == family) & (stats.capacity == cap)].sort_values("threads")
            times = s.threads.to_numpy()
            med = s.dp_ms.to_numpy()
            label = f"capacity={cap:.0e}"
            for ax, vals in ((top, med), (bottom, med[0] / med)):
                ax.plot(times, vals, "o-", color=colors[cap], label=label, linewidth=1.8, markersize=5)
        for ax in (top, bottom):
            ax.set_xscale("log", base=2)
            ax.set_xticks(threads, [str(t) for t in threads])
            ax.grid(alpha=.3)
            ax.set_xlabel("threads per block")
            ax.legend(fontsize=8)
        top.set_yscale("log", base=2)
        top.set_title(f"max_weight={family}")
        top.set_ylabel("DP loop time, host measured (ms)")
        bottom.set_ylabel("ratio of block-size medians")
        bottom.set_xlabel("threads per block")
    axes[0, 0].set_ylabel("DP loop time (ms, median)")
    fig.tight_layout()
    fig.savefig(IMAGES / "perf_kpdpgpu_block_size.png", dpi=150)
    plt.close(fig)


def plot_gpu_seeds() -> None:
    df = pd.read_csv(DATA / "perf_kpdpgpu_seed9.csv")
    df = df[df.capacity == 10000000].copy()
    seeds = sorted(df.seed.unique())
    assert len(seeds) == 9
    med = df.groupby(["max_weight", "threads", "seed"], as_index=False).dp_ms.median()
    fig, axes = plt.subplots(2, 1, figsize=(12, 9), sharex=True, sharey=False)
    threads = sorted(df.threads.unique())
    xticks = np.arange(len(threads))

    for ax, family in zip(axes, sorted(df.max_weight.unique())):
        sample = med[med.max_weight == family]
        for idx, thread in enumerate(threads):
            for seed in seeds:
                value = sample[(sample.threads == thread) & (sample.seed == seed)].dp_ms.iloc[0]
                is_base = seed == BASE_SEED
                ax.scatter(
                    idx + (seeds.index(seed) - 4) * .04,
                    value,
                    color="#D55E00" if is_base else "#0072B2",
                    edgecolors="black" if is_base else "none",
                    linewidths=.8,
                    s=38 if is_base else 25,
                    alpha=.95 if is_base else .65,
                    zorder=3,
                    label="seed 42" if is_base and idx == 0 else ("other seeds" if not is_base and idx == 0 and seed == seeds[1] else None),
                )
        medline = sample.groupby("threads").dp_ms.median().reindex(threads)
        ax.plot(xticks, medline, color="black", marker="D", linewidth=1.6, markersize=5,
                label="median of seed medians", zorder=4)
        ax.set_xticks(xticks, [str(t) for t in threads])
        ax.set_xlabel("threads per block")
        ax.set_ylabel("DP loop time (ms)")
        ax.set_title(f"max_weight={family}, capacity=1e7")
        ax.grid(axis="y", alpha=.3)
        ax.legend(fontsize=8, ncol=3, loc="upper left")
    fig.suptitle("GPU: nine seeds, five replicas/seed. Circles = per-seed median; line = median across seeds")
    fig.tight_layout()
    fig.savefig(IMAGES / "perf_kpdpgpu_seed9.png", dpi=150)
    plt.close(fig)


def plot_dag_seed_wall() -> None:
    df = pd.read_csv(DATA / "perf_kpdpdag_seed9.csv")
    df = df[df.event == "cycles"].copy()
    assert set(df.num_items) == {5000}
    med = df.groupby(["item_block", "cap_block", "threads", "seed"], as_index=False)["time_s"].median().rename(columns={"time_s": "wall_median_s"})
    geometries = sorted(med[["item_block", "cap_block"]].drop_duplicates().itertuples(index=False, name=None))
    assert len(geometries) == 4
    colors = {(25, 1000): "#1f77b4", (25, 10000): "#2ca02c", (10, 1000): "#d62728", (10, 10000): "#9467bd"}
    threads = sorted(med.threads.unique())
    fig, ax = plt.subplots(figsize=(9, 5.3))
    for ib, cb in geometries:
        s = med[(med.item_block == ib) & (med.cap_block == cb)]
        central = s.groupby("threads").wall_median_s.agg(["median", "min", "max"]).reindex(threads)
        x = central.index.to_numpy()
        ax.plot(x, central["median"], "o-", color=colors[(ib, cb)],
                linewidth=1.8, markersize=5, label=f"item_block={ib}, cap_block={cb}")
        ax.fill_between(x, central["min"], central["max"], color=colors[(ib, cb)], alpha=.16)
    ax.set_xticks(threads, [str(t) for t in threads])
    ax.set_xscale("log", base=2)
    ax.set_yscale("log", base=2)
    ax.set_xlabel("threads")
    ax.set_ylabel("DAG wall time (s)")
    ax.grid(alpha=.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(IMAGES / "perf_kpdpdag_seed9_wall.png", dpi=150)
    plt.close(fig)


def main() -> None:
    plot_gpu_repetitions()
    plot_gpu_seeds()
    plot_dag_seed_wall()
    print("Updated plots in doc/images/")


if __name__ == "__main__":
    main()
