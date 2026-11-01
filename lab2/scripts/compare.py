#!/usr/bin/env python3
"""Compare benchmark CSV files from Windows and Ubuntu.

Usage:
    python scripts/compare.py \
        Output/windows/bench/bench_windows_1MiB.csv \
        Output/windows/bench/bench_windows_100MiB.csv \
        Output/ubuntu/bench/bench_ubuntu_1MiB.csv \
        Output/ubuntu/bench/bench_ubuntu_100MiB.csv

The output is always written below ``lab2/Output/compare`` unless ``--out-dir``
is supplied. Multiple files belonging to the same platform are combined before
the comparison, which supports one CSV per payload size.
Only the standard library and matplotlib are required.
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
from typing import Iterable, NoReturn

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter


REQUIRED = {
    "implementation",
    "direction",
    "size_bytes",
    "mean_MBps",
    "ci95_lo_MBps",
    "ci95_hi_MBps",
    "mean_ms_per_op",
    "ci95_lo_ms",
    "ci95_hi_ms",
}
ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "Output" / "compare"


def fail(message: str) -> NoReturn:
    raise SystemExit(f"[ERROR] {message}")


def read_rows(path: Path, platform: str) -> list[dict[str, object]]:
    if not path.is_file():
        fail(f"CSV file not found: {path}")
    try:
        with path.open(newline="", encoding="utf-8-sig") as stream:
            reader = csv.DictReader(line for line in stream if not line.startswith("#"))
            if reader.fieldnames is None:
                fail(f"{path.name}: missing CSV header")
            missing = REQUIRED - set(reader.fieldnames)
            if missing:
                fail(f"{path.name}: missing columns {sorted(missing)}")
            rows = []
            for line_number, raw in enumerate(reader, 2):
                try:
                    row: dict[str, object] = {
                        "platform": platform,
                        "implementation": raw["implementation"],
                        "direction": raw["direction"],
                        "size_bytes": int(raw["size_bytes"]),
                    }
                    for column in REQUIRED - {"implementation", "direction", "size_bytes"}:
                        row[column] = float(raw[column])
                except (KeyError, TypeError, ValueError) as exc:
                    fail(f"{path.name}:{line_number}: invalid benchmark row ({exc})")
                rows.append(row)
    except OSError as exc:
        fail(f"cannot read {path}: {exc}")
    if not rows:
        fail(f"{path.name}: no benchmark rows")
    return rows


def platform_from_path(path: Path) -> str:
    name = path.name.lower()
    if "windows" in name:
        return "windows"
    if "ubuntu" in name or "linux" in name:
        return "linux"
    fail(
        f"cannot determine platform from '{path.name}'; "
        "filename must contain windows, ubuntu, or linux"
    )


def combine_platform_files(paths: list[Path]) -> list[tuple[str, list[dict[str, object]]]]:
    grouped: dict[str, list[dict[str, object]]] = {}
    seen: dict[str, set[tuple[str, str, int]]] = {}
    for path in paths:
        platform = platform_from_path(path)
        rows = read_rows(path, platform)
        grouped.setdefault(platform, [])
        seen.setdefault(platform, set())
        for row in rows:
            row_key = key(row)
            if row_key in seen[platform]:
                fail(
                    f"duplicate benchmark row for {platform}: "
                    f"{row_key} (input files must contain distinct payload sizes)"
                )
            seen[platform].add(row_key)
            grouped[platform].append(row)
    if len(grouped) < 2:
        fail("at least one Windows and one Ubuntu/Linux CSV are required")
    return [(platform, grouped[platform]) for platform in sorted(grouped)]


def size_label(size: int) -> str:
    for unit, value in (("GiB", 1 << 30), ("MiB", 1 << 20), ("KiB", 1 << 10)):
        if size >= value and size % value == 0:
            return f"{size // value} {unit}"
    return f"{size} B"


def key(row: dict[str, object]) -> tuple[str, str, int]:
    return (str(row["implementation"]), str(row["direction"]), int(row["size_bytes"]))


def merged_rows(
    datasets: Iterable[tuple[str, Iterable[dict[str, object]]]],
) -> tuple[list[dict[str, object]], list[str]]:
    data = list(datasets)
    labels = [label for label, _ in data]
    indexed = {
        label: {key(row): row for row in dataset}
        for label, dataset in data
    }
    result: list[dict[str, object]] = []
    keys = set().union(*(set(rows) for rows in indexed.values()))
    for benchmark_key in sorted(keys, key=lambda item: (item[1], item[0], item[2])):
        row: dict[str, object] = {
            "implementation": benchmark_key[0],
            "direction": benchmark_key[1],
            "size_bytes": benchmark_key[2],
        }
        for platform in labels:
            source = indexed[platform].get(benchmark_key)
            for metric in ("mean_MBps", "ci95_lo_MBps", "ci95_hi_MBps", "mean_ms_per_op", "ci95_lo_ms", "ci95_hi_ms"):
                row[f"{platform}_{metric}"] = "" if source is None else source[metric]
        if "windows" in indexed and "linux" in indexed:
            win = indexed["windows"].get(benchmark_key)
            lin = indexed["linux"].get(benchmark_key)
            row["linux_vs_windows_throughput"] = (
                float(lin["mean_MBps"]) / float(win["mean_MBps"])
                if win and lin and float(win["mean_MBps"])
                else ""
            )
            row["linux_vs_windows_latency"] = (
                float(lin["mean_ms_per_op"]) / float(win["mean_ms_per_op"])
                if win and lin and float(win["mean_ms_per_op"])
                else ""
            )
        result.append(row)
    return result, labels


def write_comparison(rows: list[dict[str, object]], labels: list[str], output: Path) -> None:
    metrics = ("mean_MBps", "ci95_lo_MBps", "ci95_hi_MBps",
               "mean_ms_per_op", "ci95_lo_ms", "ci95_hi_ms")
    fields = ["implementation", "direction", "size_bytes"]
    fields.extend(f"{label}_{metric}" for label in labels for metric in metrics)
    if "windows" in labels and "linux" in labels:
        fields.extend(["linux_vs_windows_throughput", "linux_vs_windows_latency"])
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})


def make_plot(
    rows: list[dict[str, object]],
    direction: str,
    metric: str,
    output: Path,
    payload_size: int | None = None,
    labels: list[str] | None = None,
) -> None:
    metric_title = "Throughput (MB/s)" if metric == "mean_MBps" else "Latency (ms/op)"
    ci_low = "ci95_lo_MBps" if metric == "mean_MBps" else "ci95_lo_ms"
    ci_high = "ci95_hi_MBps" if metric == "mean_MBps" else "ci95_hi_ms"
    grouped: dict[str, list[dict[str, object]]] = {}
    for row in rows:
        if row["direction"] == direction and (
            payload_size is None or int(row["size_bytes"]) == payload_size
        ):
            grouped.setdefault(str(row["implementation"]), []).append(row)
    if not grouped:
        return

    fig, ax = plt.subplots(figsize=(12, 6))
    implementations = sorted(grouped)
    sizes = sorted({int(row["size_bytes"]) for values in grouped.values() for row in values})
    platforms = labels or ["windows", "linux"]
    series = [
        (implementation, platform)
        for implementation in implementations
        for platform in platforms
    ]
    series = [
        (implementation, platform)
        for implementation, platform in series
        if any(
            row.get(f"{platform}_{metric}", "") != ""
            for row in grouped[implementation]
        )
    ]
    bar_count = len(platforms) if payload_size is not None else len(series)
    bar_width = 0.8 / max(bar_count, 1)

    if payload_size is None:
        categories = sizes
        category_labels = [size_label(size) for size in categories]
        category_index = {size: index for index, size in enumerate(categories)}
        xlabel = "Payload size"
    else:
        categories = implementations
        category_labels = implementations
        category_index = {implementation: index for index, implementation in enumerate(categories)}
        xlabel = "Implementation"

    bar_data = []
    plotted_values = []
    platform_positions = {platform: index for index, platform in enumerate(platforms)}
    for series_index, (implementation, platform) in enumerate(series):
        values = {int(row["size_bytes"]): row for row in grouped[implementation]}
        slot_index = (
            platform_positions[platform]
            if payload_size is not None
            else series_index
        )
        xs = [
            category_index[category] - 0.4 + (slot_index + 0.5) * bar_width
            for category in categories
        ]
        ys = [math.nan] * len(categories)
        err_low = [math.nan] * len(categories)
        err_high = [math.nan] * len(categories)
        for size in sizes:
            row = values.get(size)
            if row is None:
                continue
            raw_value = row[f"{platform}_{metric}"]
            if raw_value == "":
                continue
            value = float(raw_value)
            low = float(row[f"{platform}_{ci_low}"])
            high = float(row[f"{platform}_{ci_high}"])
            category = size if payload_size is None else implementation
            index = category_index[category]
            ys[index] = value
            err_low[index] = max(value - low, 0.0)
            err_high[index] = max(high - value, 0.0)
        present_values = [value for value in ys if not math.isnan(value) and value > 0]
        if present_values:
            bar_data.append((
                implementation, platform, xs, ys, err_low, err_high,
            ))
            plotted_values.extend(
                value
                for values in (ys, err_low, err_high)
                for value in values
                if not math.isnan(value) and value > 0
            )

    use_log_scale = False
    if plotted_values:
        smallest = min(plotted_values)
        largest = max(plotted_values)
        use_log_scale = largest / smallest >= 100
    lower_bound = 0.0
    if use_log_scale:
        lower_bound = max(min(plotted_values) * 0.7, max(plotted_values) * 1e-6)
        ax.set_yscale("log")
        ax.set_ylim(lower_bound, max(plotted_values) * 1.25)
    elif plotted_values:
        ax.set_ylim(0, max(plotted_values) * 1.25)
        if max(plotted_values) < 0.01:
            ax.ticklabel_format(axis="y", style="sci", scilimits=(0, 0))
        else:
            ax.yaxis.set_major_formatter(ScalarFormatter())

    for implementation, platform, xs, ys, err_low, err_high in bar_data:
        heights = (
            [value - lower_bound if not math.isnan(value) else math.nan for value in ys]
            if use_log_scale
            else ys
        )
        ax.bar(
            xs,
            heights,
            width=bar_width,
            bottom=lower_bound if use_log_scale else 0,
            yerr=[err_low, err_high],
            error_kw={"elinewidth": 1, "capsize": 3},
            alpha=0.85,
            edgecolor="black",
            linewidth=0.5,
            label=f"{implementation} / {platform}",
        )

    ax.set_xticks(range(len(categories)), category_labels)
    ax.set_xlim(-0.5, len(categories) - 0.5)
    scale_label = "log scale" if use_log_scale else "linear scale"
    ax.set_ylabel(f"{metric_title} ({scale_label})")
    ax.set_xlabel(xlabel)
    title = f"Benchmark comparison: {direction}"
    if payload_size is not None:
        title += f", {size_label(payload_size)}"
        ax.tick_params(axis="x", labelrotation=25)
    ax.set_title(title)
    ax.grid(axis="y", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output, dpi=150)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare Windows and Ubuntu benchmark CSV files.")
    parser.add_argument(
        "csv_files",
        nargs="+",
        type=Path,
        help="benchmark CSV files; use one or more files per platform",
    )
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT, help="output directory (default: Output/compare)")
    args = parser.parse_args()

    if len(args.csv_files) < 2:
        parser.error("at least two CSV files are required")
    datasets = combine_platform_files(args.csv_files)
    rows, labels = merged_rows(datasets)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    comparison_csv = args.out_dir / "comparison.csv"
    write_comparison(rows, labels, comparison_csv)
    for direction in ("encrypt", "decrypt"):
        make_plot(rows, direction, "mean_MBps", args.out_dir / f"throughput_{direction}.png", labels=labels)
        make_plot(rows, direction, "mean_ms_per_op", args.out_dir / f"latency_{direction}.png", labels=labels)
        for size in sorted({int(row["size_bytes"]) for row in rows}):
            suffix = size_label(size).replace(" ", "_")
            make_plot(
                rows, direction, "mean_MBps",
                args.out_dir / f"throughput_{direction}_{suffix}.png",
                size, labels,
            )
            make_plot(
                rows, direction, "mean_ms_per_op",
                args.out_dir / f"latency_{direction}_{suffix}.png", size, labels,
            )
    print(f"[OK] Wrote {comparison_csv}")
    print(f"[OK] Wrote comparison plots to {args.out_dir}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        fail(str(exc))
