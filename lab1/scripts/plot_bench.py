#!/usr/bin/env python3
"""Ve do thi throughput/latency tu bench.csv va SO SANH nhieu nen tang (pip install pandas matplotlib).

1) Ve mot nen tang (nhu truoc):
    python plot_bench.py bench.csv [--platform windows|linux] [--out-dir DIR]
   Mac dinh luu vao ../bench_result/<platform>/

2) So sanh nhieu file CSV (vi du Windows MSVC, Windows GCC, Linux GCC):
    python plot_bench.py --compare win_msvc.csv win_gcc.csv linux_gcc.csv \
        --labels "Windows MSVC" "Windows GCC" "Linux GCC" [--bar-size 1048576] [--out-dir DIR]
   File dau tien la BASELINE cho ty le (ratio) va bang tong ket.
   Mac dinh luu vao ../bench_result/compare/

Cot CSV can co: mode, dir, size_bytes, thr_mean_MBps, lat_mean_us
Cot tuy chon:   thr_ci95, lat_ci95 (khoang tin cay 95%), thr_median_MBps, ...
"""
import argparse
import math
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # khong can man hinh, chay duoc tren server/CI
import matplotlib.pyplot as plt
import pandas as pd

MODE_ORDER = ["ecb", "cbc", "ofb", "cfb", "ctr", "xts", "ccm", "gcm"]
REQUIRED = {"mode", "dir", "size_bytes", "thr_mean_MBps", "lat_mean_us"}
MARKERS = ["o", "s", "^", "D", "v", "P"]
STYLES = ["-", "--", ":", "-."]


# ------------------------------------------------------------------ io
def human_size(n: float) -> str:
    n = int(n)
    if n >= 1 << 20:
        return f"{n >> 20} MB"
    if n >= 1 << 10:
        return f"{n >> 10} KB"
    return f"{n} B"


def load_csv(path: Path, label: str) -> pd.DataFrame:
    if not path.exists():
        sys.exit(f"[ERROR] CSV file not found: {path}")
    df = pd.read_csv(path)
    missing = REQUIRED - set(df.columns)
    if missing:
        sys.exit(f"[ERROR] {path.name}: thieu cot {sorted(missing)}. Co: {sorted(df.columns)}")
    for c in ("thr_ci95", "lat_ci95"):
        if c not in df.columns:
            df[c] = 0.0
    df["platform"] = label
    return df


def ordered_modes(dfs):
    present = set().union(*[set(d["mode"].unique()) for d in dfs])
    return [m for m in MODE_ORDER if m in present] + sorted(present - set(MODE_ORDER))


def set_size_axis(ax, dfs):
    sizes = sorted(set().union(*[set(d["size_bytes"].unique()) for d in dfs]))
    ax.set_xscale("log", base=2)
    ax.set_xticks(sizes)
    ax.set_xticklabels([human_size(x) for x in sizes], rotation=45, ha="right", fontsize=8)
    ax.minorticks_off()
    ax.set_xlim(sizes[0] / 1.4, sizes[-1] * 1.4)


def save(fig, out_dir: Path, name: str):
    fig.tight_layout()
    p = out_dir / name
    fig.savefig(p, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"[OK] Saved: {p}")


# ------------------------------------------------------------------ one platform
def plot_single(df: pd.DataFrame, out_dir: Path):
    for direction in ("enc", "dec"):
        s = df[df["dir"] == direction]
        if s.empty:
            print(f"[WARN] No data for dir={direction}, skipping")
            continue
        for metric, ci, ylabel, name, logy in (
            ("thr_mean_MBps", "thr_ci95", "Throughput (MB/s)", "throughput", False),
            ("lat_mean_us", "lat_ci95", "Latency (µs/op)", "latency", True),
        ):
            fig = plt.figure(figsize=(10, 6))
            for mode, g in s.groupby("mode"):
                g = g.sort_values("size_bytes")
                plt.errorbar(g.size_bytes, g[metric], yerr=g[ci], marker="o", capsize=3, label=mode)
            set_size_axis(plt.gca(), [s])
            if logy:
                plt.yscale("log")
            plt.xlabel("Payload")
            plt.ylabel(ylabel)
            plt.title(f"AES {direction} {name} (mean, 95% CI)")
            plt.legend()
            plt.grid(True, alpha=0.3)
            save(fig, out_dir, f"{name}_{direction}.png")


# ------------------------------------------------------------------ compare
def plot_grid(dfs, labels, direction, metric, ci, ylabel, name, logy, out_dir):
    """Luoi subplot: moi o la mot mode, moi duong la mot nen tang."""
    modes = ordered_modes(dfs)
    cols = 4
    rows = math.ceil(len(modes) / cols)
    fig, axes = plt.subplots(rows, cols, figsize=(4.3 * cols, 3.6 * rows), squeeze=False)
    for ax in axes.flat[len(modes):]:
        ax.axis("off")
    for ax, mode in zip(axes.flat, modes):
        for i, (df, lab) in enumerate(zip(dfs, labels)):
            g = df[(df["mode"] == mode) & (df["dir"] == direction)].sort_values("size_bytes")
            if g.empty:
                continue
            ax.errorbar(g.size_bytes, g[metric], yerr=g[ci], marker=MARKERS[i % len(MARKERS)],
                        linestyle=STYLES[i % len(STYLES)], capsize=2, markersize=4, label=lab)
        set_size_axis(ax, dfs)
        if logy:
            ax.set_yscale("log")
        ax.set_title(mode.upper())
        ax.grid(True, alpha=0.3)
    for ax in axes[-1]:
        ax.set_xlabel("Payload")
    for ax in axes[:, 0]:
        ax.set_ylabel(ylabel)
    h, l = axes.flat[0].get_legend_handles_labels()
    fig.suptitle(f"AES {direction} {name}: platform comparison (mean, 95% CI)", y=1.10, fontsize=13)
    fig.legend(h, l, loc="upper center", bbox_to_anchor=(0.5, 1.055), ncol=len(labels), frameon=False)
    save(fig, out_dir, f"compare_{name}_{direction}.png")


def plot_ratio(dfs, labels, direction, out_dir):
    """Ty le throughput = nen tang X / baseline theo kich thuoc payload (>1: X nhanh hon baseline)."""
    if len(dfs) < 2:
        return
    modes = ordered_modes(dfs)
    n = len(dfs) - 1
    fig, axes = plt.subplots(1, n, figsize=(6.5 * n, 4.6), squeeze=False, sharey=True)
    base = dfs[0][dfs[0]["dir"] == direction]
    for ax, (df, lab) in zip(axes[0], zip(dfs[1:], labels[1:])):
        cur = df[df["dir"] == direction]
        m = base.merge(cur, on=["mode", "size_bytes"], suffixes=("_b", "_c"))
        for mode in modes:
            g = m[m["mode"] == mode].sort_values("size_bytes")
            if g.empty:
                continue
            ax.plot(g.size_bytes, g["thr_mean_MBps_c"] / g["thr_mean_MBps_b"], marker="o", markersize=4, label=mode)
        ax.axhline(1.0, color="gray", linestyle="--", linewidth=1)
        set_size_axis(ax, dfs)
        ax.set_xlabel("Payload")
        ax.set_title(f"{lab} / {labels[0]}")
        ax.grid(True, alpha=0.3)
    axes[0][0].set_ylabel("Throughput ratio (>1: faster than baseline)")
    axes[0][-1].legend(ncol=2, fontsize=8)
    fig.suptitle(f"AES {direction} throughput ratio vs baseline '{labels[0]}'", y=1.03)
    save(fig, out_dir, f"compare_ratio_{direction}.png")


def plot_bars(dfs, labels, size, out_dir):
    """Cot nhom tai mot kich thuoc payload (lay kich thuoc gan nhat co trong du lieu)."""
    modes = ordered_modes(dfs)
    sizes = sorted(set().union(*[set(d["size_bytes"].unique()) for d in dfs]))
    size = min(sizes, key=lambda s: abs(s - size))
    fig, axes = plt.subplots(1, 2, figsize=(13, 4.8), sharey=True)
    w = 0.8 / len(dfs)
    for ax, direction in zip(axes, ("enc", "dec")):
        for i, (df, lab) in enumerate(zip(dfs, labels)):
            vals, errs = [], []
            for mode in modes:
                g = df[(df["mode"] == mode) & (df["dir"] == direction) & (df["size_bytes"] == size)]
                vals.append(g["thr_mean_MBps"].iloc[0] if len(g) else 0)
                errs.append(g["thr_ci95"].iloc[0] if len(g) else 0)
            xs = [k + (i - (len(dfs) - 1) / 2) * w for k in range(len(modes))]
            ax.bar(xs, vals, w, yerr=errs, capsize=2, label=lab)
        ax.set_xticks(range(len(modes)))
        ax.set_xticklabels([m.upper() for m in modes])
        ax.set_title(direction)
        ax.grid(True, axis="y", alpha=0.3)
    axes[0].set_ylabel("Throughput (MB/s)")
    axes[0].legend()
    fig.suptitle(f"Throughput at {human_size(size)} (mean, 95% CI)")
    save(fig, out_dir, f"compare_bars_{human_size(size).replace(' ', '')}.png")


def summary_table(dfs, labels, out_dir):
    """Bang tong ket: moi (mode, dir, size) voi throughput cua tung nen tang, ty le so voi baseline,
    va co khac biet co y nghia khong (khoang tin cay 95% khong chong len nhau)."""
    base = dfs[0].rename(columns={"thr_mean_MBps": "base_MBps", "thr_ci95": "base_ci"})
    base = base[["mode", "dir", "size_bytes", "base_MBps", "base_ci"]]
    rows = []
    for df, lab in zip(dfs[1:], labels[1:]):
        c = df.rename(columns={"thr_mean_MBps": "cmp_MBps", "thr_ci95": "cmp_ci"})
        m = base.merge(c[["mode", "dir", "size_bytes", "cmp_MBps", "cmp_ci"]], on=["mode", "dir", "size_bytes"])
        m["baseline"] = labels[0]
        m["platform"] = lab
        m["ratio"] = m["cmp_MBps"] / m["base_MBps"]
        m["diff_pct"] = (m["ratio"] - 1) * 100
        m["significant"] = (m["cmp_MBps"] - m["base_MBps"]).abs() > (m["base_ci"] + m["cmp_ci"])
        rows.append(m)
    if not rows:
        return
    out = pd.concat(rows)
    out["mode"] = pd.Categorical(out["mode"], ordered_modes(dfs), ordered=True)
    out = out.sort_values(["platform", "dir", "mode", "size_bytes"])
    cols = ["platform", "baseline", "mode", "dir", "size_bytes", "base_MBps", "cmp_MBps", "ratio", "diff_pct", "significant"]
    out = out[cols].round({"base_MBps": 2, "cmp_MBps": 2, "ratio": 3, "diff_pct": 1})
    p = out_dir / "compare_summary.csv"
    out.to_csv(p, index=False)
    print(f"[OK] Saved: {p}")
    print("\n=== Ty le throughput trung binh (geometric mean) so voi baseline "
          f"'{labels[0]}' ===")
    g = out.assign(logr=out["ratio"].apply(math.log)).groupby(["platform", "dir", "mode"], observed=True)["logr"].mean()
    for (plat, d, mode), v in g.items():
        print(f"  {plat:<16} {d}  {mode:<4} x{math.exp(v):.3f}")


def run_compare(args, script_dir: Path):
    paths = [Path(p).resolve() for p in args.compare]
    labels = args.labels or [p.stem for p in paths]
    if len(labels) != len(paths):
        sys.exit("[ERROR] So luong --labels phai bang so file --compare")
    dfs = [load_csv(p, lab) for p, lab in zip(paths, labels)]
    out_dir = Path(args.out_dir).resolve() if args.out_dir else (script_dir.parent / "bench_result" / "compare").resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"[INFO] Comparing: {', '.join(labels)}  (baseline = {labels[0]})")
    print(f"[INFO] Output dir: {out_dir}")
    for d in ("enc", "dec"):
        plot_grid(dfs, labels, d, "thr_mean_MBps", "thr_ci95", "Throughput (MB/s)", "throughput", False, out_dir)
        plot_grid(dfs, labels, d, "lat_mean_us", "lat_ci95", "Latency (µs/op)", "latency", True, out_dir)
        plot_ratio(dfs, labels, d, out_dir)
    plot_bars(dfs, labels, args.bar_size, out_dir)
    summary_table(dfs, labels, out_dir)
    print(f"\n[DONE] All comparison outputs saved to: {out_dir}")


# ------------------------------------------------------------------ main
def parse_args():
    p = argparse.ArgumentParser(description="Plot / compare benchmark CSV results")
    p.add_argument("csv_file", nargs="?", default="bench.csv", help="CSV cho che do mot nen tang")
    p.add_argument("--out-dir", default=None, help="Thu muc luu do thi")
    p.add_argument("--platform", choices=["windows", "linux"], default=None,
                   help="Ten nen tang (chi dung de chon thu muc mac dinh o che do mot nen tang)")
    p.add_argument("--compare", nargs="+", metavar="CSV", help="So sanh 2+ file CSV (file dau la baseline)")
    p.add_argument("--labels", nargs="+", help="Ten hien thi cho tung file --compare")
    p.add_argument("--bar-size", type=int, default=1 << 20, help="Kich thuoc payload cho bieu do cot (mac dinh 1 MiB)")
    return p.parse_args()


def main():
    args = parse_args()
    script_dir = Path(__file__).parent
    if args.compare:
        if len(args.compare) < 2:
            sys.exit("[ERROR] --compare can it nhat 2 file CSV")
        run_compare(args, script_dir)
        return

    csv_path = Path(args.csv_file).resolve()
    df = load_csv(csv_path, args.platform or "run")
    if args.out_dir:
        out_dir = Path(args.out_dir).resolve()
    else:
        plat = args.platform or ("windows" if "windows" in str(csv_path).lower() else "linux")
        out_dir = (script_dir.parent / "bench_result" / plat).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"[INFO] Reading: {csv_path}")
    print(f"[INFO] Output dir: {out_dir}")
    plot_single(df, out_dir)
    print(f"\n[DONE] All plots saved to: {out_dir}")


if __name__ == "__main__":
    main()