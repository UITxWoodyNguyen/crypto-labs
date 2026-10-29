#!/usr/bin/env python3
"""Compare two aestool benchmark CSV files and create reports.

Usage:
    python scripts/compare.py windows.csv linux.csv

The output is always written below ``lab2/Output/compare`` unless ``--out-dir``
is supplied.  The input files are labelled Windows and Linux respectively.
Only the standard library and matplotlib are required.
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
from typing import Iterable, NoReturn

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


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


def size_label(size: int) -> str:
    for unit, value in (("GiB", 1 << 30), ("MiB", 1 << 20), ("KiB", 1 << 10)):
        if size >= value and size % value == 0:
            return f"{size // value} {unit}"
    return f"{size} B"


def key(row: dict[str, object]) -> tuple[str, str, int]:
    return (str(row["implementation"]), str(row["direction"]), int(row["size_bytes"]))


def merged_rows(windows: Iterable[dict[str, object]], linux: Iterable[dict[str, object]]) -> list[dict[str, object]]:
    left = {key(row): row for row in windows}
    right = {key(row): row for row in linux}
    result: list[dict[str, object]] = []
    for benchmark_key in sorted(set(left) | set(right), key=lambda item: (item[1], item[0], item[2])):
        win = left.get(benchmark_key)
        lin = right.get(benchmark_key)
        row: dict[str, object] = {
            "implementation": benchmark_key[0],
            "direction": benchmark_key[1],
            "size_bytes": benchmark_key[2],
        }
        for platform, source in (("windows", win), ("linux", lin)):
            for metric in ("mean_MBps", "ci95_lo_MBps", "ci95_hi_MBps", "mean_ms_per_op", "ci95_lo_ms", "ci95_hi_ms"):
                row[f"{platform}_{metric}"] = "" if source is None else source[metric]
        if win is not None and lin is not None:
            win_speed = float(win["mean_MBps"])
            lin_speed = float(lin["mean_MBps"])
            row["linux_vs_windows_throughput"] = lin_speed / win_speed if win_speed else ""
            win_latency = float(win["mean_ms_per_op"])
            lin_latency = float(lin["mean_ms_per_op"])
            row["linux_vs_windows_latency"] = lin_latency / win_latency if win_latency else ""
        else:
            row["linux_vs_windows_throughput"] = ""
            row["linux_vs_windows_latency"] = ""
        result.append(row)
    return result


def write_comparison(rows: list[dict[str, object]], output: Path) -> None:
    fields = [
        "implementation", "direction", "size_bytes",
        "windows_mean_MBps", "windows_ci95_lo_MBps", "windows_ci95_hi_MBps",
        "linux_mean_MBps", "linux_ci95_lo_MBps", "linux_ci95_hi_MBps",
        "windows_mean_ms_per_op", "windows_ci95_lo_ms", "windows_ci95_hi_ms",
        "linux_mean_ms_per_op", "linux_ci95_lo_ms", "linux_ci95_hi_ms",
        "linux_vs_windows_throughput", "linux_vs_windows_latency",
    ]
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def make_plot(rows: list[dict[str, object]], direction: str, metric: str, output: Path) -> None:
    metric_title = "Throughput (MB/s)" if metric == "mean_MBps" else "Latency (ms/op)"
    ci_low = "ci95_lo_MBps" if metric == "mean_MBps" else "ci95_lo_ms"
    ci_high = "ci95_hi_MBps" if metric == "mean_MBps" else "ci95_hi_ms"
    grouped: dict[str, list[dict[str, object]]] = {}
    for row in rows:
        if row["direction"] == direction:
            grouped.setdefault(str(row["implementation"]), []).append(row)
    if not grouped:
        return

    fig, ax = plt.subplots(figsize=(11, 6))
    sizes = sorted({int(row["size_bytes"]) for values in grouped.values() for row in values})
    x_positions = {size: index for index, size in enumerate(sizes)}
    implementations = sorted(grouped)
    for implementation in implementations:
        values = {int(row["size_bytes"]): row for row in grouped[implementation]}
        for platform, marker, linestyle in (("windows", "o", "-"), ("linux", "s", "--")):
            xs, ys, errors = [], [], []
            for size in sizes:
                row = values.get(size)
                if row is None:
                    continue
                value = row[f"{platform}_{metric}"]
                if value == "":
                    continue
                value = float(value)
                low = float(row[f"{platform}_{ci_low}"])
                high = float(row[f"{platform}_{ci_high}"])
                xs.append(x_positions[size])
                ys.append(value)
                errors.append(max(value - low, high - value, 0.0))
            if ys:
                ax.errorbar(xs, ys, yerr=errors, marker=marker, linestyle=linestyle,
                            linewidth=1.5, capsize=3, label=f"{implementation} / {platform}")
    ax.set_xticks(range(len(sizes)), [size_label(size) for size in sizes])
    ax.set_ylabel(metric_title)
    ax.set_xlabel("Payload size")
    ax.set_title(f"Windows/Linux benchmark comparison: {direction}")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output, dpi=150)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare Windows and Linux aestool benchmark CSV files.")
    parser.add_argument("windows_csv", type=Path, help="Windows benchmark CSV")
    parser.add_argument("linux_csv", type=Path, help="Linux/Ubuntu benchmark CSV")
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT, help="output directory (default: Output/compare)")
    args = parser.parse_args()

    windows = read_rows(args.windows_csv, "windows")
    linux = read_rows(args.linux_csv, "linux")
    rows = merged_rows(windows, linux)
    args.out_dir.mkdir(parents=True, exist_ok=True)
    comparison_csv = args.out_dir / "comparison.csv"
    write_comparison(rows, comparison_csv)
    for direction in ("encrypt", "decrypt"):
        make_plot(rows, direction, "mean_MBps", args.out_dir / f"throughput_{direction}.png")
        make_plot(rows, direction, "mean_ms_per_op", args.out_dir / f"latency_{direction}.png")
    print(f"[OK] Wrote {comparison_csv}")
    print(f"[OK] Wrote comparison plots to {args.out_dir}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as exc:
        fail(str(exc))
