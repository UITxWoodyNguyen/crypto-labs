#!/usr/bin/env python3
"""Plot throughput and latency from one or more `aestool bench --csv` files.

Usage: python3 scripts/plot_bench.py bench.csv [other.csv ...]
       [--out-prefix PREFIX]
"""
import argparse
import csv
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter

parser = argparse.ArgumentParser()
parser.add_argument("csv_files", nargs="+", help="benchmark CSV files")
parser.add_argument("--out-prefix", help="output filename prefix")
args = parser.parse_args()

csv_files = args.csv_files
prefix = args.out_prefix
# Keep the previous ``plot_bench.py input.csv output_prefix`` form working.
if prefix is None and len(csv_files) > 1 and not csv_files[-1].lower().endswith(".csv"):
    prefix = csv_files.pop()
if prefix is None:
    prefix = os.path.splitext(csv_files[0])[0]

rows = []
for csv_file in csv_files:
    with open(csv_file, newline="", encoding="utf-8-sig") as stream:
        file_rows = list(csv.DictReader(stream))
    # Keep files distinguishable when several CSVs contain the same backend names.
    source = os.path.splitext(os.path.basename(csv_file))[0]
    for row in file_rows:
        row["_source"] = source
        rows.append(row)
prefix_dir = os.path.dirname(os.path.abspath(prefix))
if prefix_dir:
    os.makedirs(prefix_dir, exist_ok=True)
sizes = sorted({int(r["size_bytes"]) for r in rows})
implementation_sources = {}
for row in rows:
    implementation_sources.setdefault(row["implementation"], {}).setdefault(
        row["_source"], set()
    ).add(int(row["size_bytes"]))

series = []
for implementation in sorted(implementation_sources):
    sources = implementation_sources[implementation]
    sizes_by_source = list(sources.values())
    has_overlapping_sizes = any(
        left.intersection(right)
        for index, left in enumerate(sizes_by_source)
        for right in sizes_by_source[index + 1:]
    )
    if has_overlapping_sizes:
        series.extend(
            (source, implementation, f"{source}: {implementation}")
            for source in sorted(sources)
        )
    else:
        series.append((None, implementation, implementation))
label = lambda n: f"{n >> 20} MiB" if n >= 1 << 20 else f"{n >> 10} KiB"

for direction, direction_label in (("encrypt", "encryption"), ("decrypt", "decryption")):
    for metric, lo, hi, metric_label, unit, fname in [
        ("mean_MBps", "ci95_lo_MBps", "ci95_hi_MBps", "throughput", "MB/s", "throughput"),
        ("mean_ms_per_op", "ci95_lo_ms", "ci95_hi_ms", "latency", "ms/op", "latency"),
    ]:
        fig, ax = plt.subplots(figsize=(11, 6))
        bar_width = 0.8 / max(len(series), 1)
        plotted_values = []
        for k, (source, impl, series_label) in enumerate(series):
            xs, ys, err_low, err_high = [], [], [], []
            for i, s in enumerate(sizes):
                matches = [
                    r for r in rows
                    if (source is None or r["_source"] == source)
                    and r["implementation"] == impl
                    and int(r["size_bytes"]) == s
                    and r["direction"] == direction
                ]
                if matches:
                    value = float(matches[0][metric])
                    low = float(matches[0][lo])
                    high = float(matches[0][hi])
                    xs.append(i - 0.4 + (k + 0.5) * bar_width)
                    ys.append(value)
                    err_low.append(max(value - low, 0))
                    err_high.append(max(high - value, 0))
            if ys:
                ax.bar(
                    xs,
                    ys,
                    width=bar_width,
                    yerr=[err_low, err_high],
                    error_kw={"elinewidth": 1, "capsize": 3},
                    alpha=0.85,
                    edgecolor="black",
                    linewidth=0.5,
                    label=series_label,
                )
                plotted_values.extend(
                    value
                    for value in ys + err_low + err_high
                    if value > 0
                )
        ax.set_xticks(range(len(sizes)))
        ax.set_xticklabels([label(s) for s in sizes])
        if plotted_values:
            smallest = min(plotted_values)
            largest = max(plotted_values)
            # Log scale keeps very small implementations visible when the
            # benchmark spans several orders of magnitude. For close values,
            # linear ticks are easier to read.
            if largest / smallest >= 100:
                ax.set_yscale("log")
                ax.set_ylim(max(smallest * 0.7, largest * 1e-6), largest * 1.25)
            else:
                ax.set_ylim(0, largest * 1.25)
                if largest < 0.01:
                    ax.ticklabel_format(axis="y", style="sci", scilimits=(0, 0))
                else:
                    ax.yaxis.set_major_formatter(ScalarFormatter())
        scale_label = "log scale" if plotted_values and max(plotted_values) / min(plotted_values) >= 100 else "linear scale"
        ax.set_ylabel(f"{unit} ({scale_label})")
        ax.set_title(f"AES-128-CTR {direction_label} {metric_label} (95% CI)")
        ax.set_xlabel("Payload size")
        ax.grid(axis="y", alpha=0.3)
        ax.legend()
        fig.tight_layout()
        output = f"{prefix}_{fname}_{direction}.png"
        fig.savefig(output, dpi=150)
        plt.close(fig)
        print("wrote", output)
