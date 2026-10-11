#!/usr/bin/env python3
"""Ve do thi throughput/latency tu bench.csv (pip install pandas matplotlib).

Usage:
    python plot_bench.py <bench.csv> [--out-dir DIR] [--platform windows|linux]

Default output directory:
    Windows: ../bench_result/windows/
    Linux:   ../bench_result/linux/
"""
import sys
import argparse
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt


def parse_args():
    parser = argparse.ArgumentParser(description="Plot benchmark CSV results")
    parser.add_argument("csv_file", nargs="?", default="bench.csv",
                        help="Input benchmark CSV file")
    parser.add_argument("--out-dir", default=None,
                        help="Output directory for plots (default: ../bench_result/<platform>/)")
    parser.add_argument("--platform", choices=["windows", "linux"], default=None,
                        help="Platform name for default output directory")
    return parser.parse_args()


def determine_out_dir(args, script_dir: Path) -> Path:
    """Determine output directory based on args or platform detection."""
    if args.out_dir:
        return Path(args.out_dir).resolve()

    # Default: ../bench_result/<platform>/
    base = script_dir.parent / "bench_result"
    if args.platform:
        platform = args.platform
    else:
        # Auto-detect from CSV filename or current directory
        platform = "windows" if "windows" in str(args.csv_file).lower() else "linux"

    return (base / platform).resolve()


def main():
    args = parse_args()
    script_dir = Path(__file__).parent

    csv_path = Path(args.csv_file).resolve()
    if not csv_path.exists():
        print(f"[ERROR] CSV file not found: {csv_path}")
        sys.exit(1)

    out_dir = determine_out_dir(args, script_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"[INFO] Reading: {csv_path}")
    print(f"[INFO] Output dir: {out_dir}")

    df = pd.read_csv(csv_path)

    # Expected columns: run, mode, size_bytes, dir (enc/dec),
    #                   thr_mean_MBps, thr_ci95, lat_mean_us, lat_ci95
    # If columns differ, try to adapt
    required_cols = {"mode", "size_bytes", "dir"}
    if not required_cols.issubset(df.columns):
        print(f"[ERROR] CSV missing required columns. Need: {required_cols}")
        print(f"         Found: {set(df.columns)}")
        sys.exit(1)

    for direction in ("enc", "dec"):
        s = df[df.dir == direction]
        if s.empty:
            print(f"[WARN] No data for dir={direction}, skipping")
            continue

        # Throughput plot
        plt.figure(figsize=(10, 6))
        for mode, group in s.groupby("mode"):
            plt.errorbar(
                group.size_bytes, group.thr_mean_MBps,
                yerr=group.thr_ci95 if "thr_ci95" in group.columns else None,
                marker="o", capsize=3, label=mode
            )
        plt.xscale("log", base=2)
        plt.xlabel("Payload (bytes)")
        plt.ylabel("Throughput (MB/s)")
        plt.title(f"AES-256 {direction} throughput (mean, 95% CI)")
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        out_png = out_dir / f"throughput_{direction}.png"
        plt.savefig(out_png, dpi=150, bbox_inches="tight")
        plt.close()
        print(f"[OK] Saved: {out_png}")

        # Latency plot
        plt.figure(figsize=(10, 6))
        for mode, group in s.groupby("mode"):
            plt.errorbar(
                group.size_bytes, group.lat_mean_us,
                yerr=group.lat_ci95 if "lat_ci95" in group.columns else None,
                marker="o", capsize=3, label=mode
            )
        plt.xscale("log", base=2)
        plt.yscale("log")
        plt.xlabel("Payload (bytes)")
        plt.ylabel("Latency (µs/op)")
        plt.title(f"AES-256 {direction} latency")
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        out_png = out_dir / f"latency_{direction}.png"
        plt.savefig(out_png, dpi=150, bbox_inches="tight")
        plt.close()
        print(f"[OK] Saved: {out_png}")

    print(f"\n[DONE] All plots saved to: {out_dir}")


if __name__ == "__main__":
    main()