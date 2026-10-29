#!/usr/bin/env python3
"""Plot throughput (MB/s, 95% CI error bars) and latency from `aestool bench --csv`.
Usage: python3 scripts/plot_bench.py bench.csv [out_prefix]   (needs matplotlib)"""
import csv
import os
import sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

rows = list(csv.DictReader(open(sys.argv[1])))
prefix = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(sys.argv[1])[0]
prefix_dir = os.path.dirname(os.path.abspath(prefix))
if prefix_dir:
    os.makedirs(prefix_dir, exist_ok=True)
sizes = sorted({int(r["size_bytes"]) for r in rows})
impls = sorted({r["implementation"] for r in rows})
label = lambda n: f"{n >> 20} MiB" if n >= 1 << 20 else f"{n >> 10} KiB"

for metric, lo, hi, title, unit, fname in [
    ("mean_MBps", "ci95_lo_MBps", "ci95_hi_MBps", "AES-128-CTR encrypt throughput", "MB/s", "throughput"),
    ("mean_ms_per_op", "ci95_lo_ms", "ci95_hi_ms", "AES-128-CTR latency per operation", "ms/op", "latency")]:
    fig, ax = plt.subplots(figsize=(9, 5))
    for k, impl in enumerate(impls):
        xs, ys, err = [], [], []
        for i, s in enumerate(sizes):
            m = [r for r in rows if r["implementation"] == impl and int(r["size_bytes"]) == s and r["direction"] == "encrypt"]
            if m:
                v, l, h = float(m[0][metric]), float(m[0][lo]), float(m[0][hi])
                xs.append(i); ys.append(v); err.append(max(h - v, 0))
        if ys:
            ax.errorbar(
                xs,
                ys,
                yerr=err,
                marker=["o", "s", "^", "D", "v", "P"][k % 6],
                linewidth=1.8,
                capsize=3,
                label=impl,
            )
    ax.set_xticks(range(len(sizes))); ax.set_xticklabels([label(s) for s in sizes])
    ax.set_yscale("log"); ax.set_ylabel(unit + " (log)"); ax.set_title(title + " (error bars: 95% CI)")
    ax.legend(); fig.tight_layout(); fig.savefig(f"{prefix}_{fname}.png", dpi=150)
    print("wrote", f"{prefix}_{fname}.png")
