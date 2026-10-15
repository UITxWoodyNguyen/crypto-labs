#!/usr/bin/env python3
"""Ve do thi + so sanh + phan tich benchmark AES (pip install pandas matplotlib).

Duong dan:
  * File CSV vao: tim theo thu muc HIEN TAI truoc, roi theo thu muc goc cua lab (cha cua scripts/).
  * Thu muc ra (tuong doi): luon tinh tu thu muc goc cua lab, chay script o dau cung duoc.

Cau truc mac dinh (goc lab = thu muc chua scripts/ va output/):
    output/windows/benchmarks/*.csv   -> do thi: output/windows/plots/      phan tich: output/windows/mode_cmp/
    output/ubuntu/benchmarks/*.csv    -> do thi: output/ubuntu/plots/       phan tich: output/ubuntu/mode_cmp/
    so sanh Windows vs Ubuntu         -> output/compare/                    phan tich: output/compare/mode_cmp/

1) Chay TAT CA mot lenh (khuyen dung):
    python scripts/plot_bench.py --auto [--prefer gcc] [--bar-size 1048576] [--root output]
   Quet moi CSV day du trong output/<os>/benchmarks/, ve do thi tung OS, phan tich 3 yeu cau trong
   output/<os>/mode_cmp/, roi so sanh cheo Windows vs Ubuntu (moi OS chon file co chuoi --prefer
   trong ten, mac dinh "gcc", de so sanh cung compiler). File chay mot phan (1 mode/1 kich thuoc) bi bo qua.

2) Mot file CSV:
    python scripts/plot_bench.py output/windows/benchmarks/bench_windows_gcc.csv [--analysis] [--no-basic]
   Nen tang tu nhan tu duong dan/ten file (windows | ubuntu), hoac dat --platform windows|ubuntu.

3) So sanh thu cong (file dau la baseline):
    python scripts/plot_bench.py --compare a.csv b.csv --labels "Windows GCC" "Ubuntu GCC" --analysis

Phan tich 3 yeu cau (--analysis; luon bat khi --auto), moi hinh/bang co enc va dec:
   (a) Stream mode (OFB, CFB, CTR) vs block mode (ECB, CBC, XTS)
   (b) AEAD (GCM, CCM) vs non-AEAD (CTR la moc gan nhat, kem trung binh nhan 6 mode non-AEAD)
   (c) Overhead cua tag: chi phi CO DINH (us/op) va THEO BYTE (ns/B), ti le dung luong tag/payload

Tuy chon: --out-dir (do thi), --analysis-dir (thu muc mode_cmp), --no-basic (chi phan tich).
Cot CSV can co: mode, dir, size_bytes, thr_mean_MBps, lat_mean_us (tuy chon: thr_ci95, lat_ci95).
"""
import argparse
import math
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # khong can man hinh, chay duoc tren server/CI
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.patches import Patch

MODE_ORDER = ["ecb", "cbc", "ofb", "cfb", "ctr", "xts", "ccm", "gcm"]
REQUIRED = {"mode", "dir", "size_bytes", "thr_mean_MBps", "lat_mean_us"}
MARKERS = ["o", "s", "^", "D", "v", "P"]
STYLES = ["-", "--", ":", "-."]

ROOT = Path(__file__).resolve().parent.parent  # goc cua lab: chua scripts/, output/, build_*/ ...
OS_NAMES = ("windows", "ubuntu")


def resolve_in(p) -> Path:
    """Duong dan DAU VAO: thu theo thu muc hien tai, roi theo goc lab."""
    p = Path(p)
    if p.is_absolute():
        return p
    for base in (Path.cwd(), ROOT):
        if (base / p).exists():
            return (base / p).resolve()
    return (Path.cwd() / p).resolve()


def resolve_out(p) -> Path:
    """Duong dan DAU RA: tuong doi thi tinh tu goc lab (khong phu thuoc thu muc dang dung)."""
    p = Path(p)
    return p if p.is_absolute() else (ROOT / p).resolve()


def norm_os(x):
    return "ubuntu" if x == "linux" else x


def detect_os(path: Path):
    for s in (path.name.lower(), str(path).lower()):
        if "windows" in s:
            return "windows"
        if "ubuntu" in s or "linux" in s:
            return "ubuntu"
    return None


def short_label(path: Path) -> str:
    return path.stem[len("bench_"):] if path.stem.startswith("bench_") else path.stem


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
    out_dir.mkdir(parents=True, exist_ok=True)
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


def do_compare(dfs, labels, out_dir, mode_dir, args, analysis):
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"[INFO] So sanh: {', '.join(labels)}  (baseline = {labels[0]})")
    print(f"[INFO] Thu muc ra: {out_dir}")
    if not args.no_basic:
        for d in ("enc", "dec"):
            plot_grid(dfs, labels, d, "thr_mean_MBps", "thr_ci95", "Throughput (MB/s)", "throughput", False, out_dir)
            plot_grid(dfs, labels, d, "lat_mean_us", "lat_ci95", "Latency (µs/op)", "latency", True, out_dir)
            plot_ratio(dfs, labels, d, out_dir)
        plot_bars(dfs, labels, args.bar_size, out_dir)
        summary_table(dfs, labels, out_dir)
    if analysis:
        run_analysis(dfs, labels, args.bar_size, mode_dir)


# ------------------------------------------------------------------ analysis: 3 yeu cau so sanh
GROUPS = {"block": ["ecb", "cbc", "xts"], "stream": ["ofb", "cfb", "ctr"], "aead": ["ccm", "gcm"]}
NON_AEAD = ["ecb", "cbc", "ofb", "cfb", "ctr", "xts"]
GROUP_COLORS = {"block": "#1f77b4", "stream": "#2ca02c", "aead": "#d62728"}
MODE_COLORS = {"ctr": "#2ca02c", "gcm": "#d62728", "ccm": "#9467bd"}
HATCHES = ["", "//", "..", "xx"]


def _group(mode):
    for g, ms in GROUPS.items():
        if mode in ms:
            return g
    return "block"


def _wide(df, direction, col="thr_mean_MBps"):
    s = df[df["dir"] == direction]
    return s.pivot_table(index="size_bytes", columns="mode", values=col, aggfunc="mean").sort_index()


def _geomean(w, modes):
    cols = [m for m in modes if m in w.columns]
    if not cols:
        return None
    return np.exp(np.log(w[cols]).mean(axis=1))


def _nearest(dfs, size):
    sizes = sorted(set().union(*[set(d["size_bytes"].unique()) for d in dfs]))
    return min(sizes, key=lambda x: abs(x - size))


def _platform_legend(ax, labels, extra=None):
    handles = list(extra or [])
    if len(labels) > 1:
        handles += [Patch(facecolor="white", edgecolor="black", hatch=HATCHES[i % len(HATCHES)], label=lab)
                    for i, lab in enumerate(labels)]
    ax.legend(handles=handles, fontsize=8)


def analysis_stream_block(dfs, labels, bar_size, out_dir):
    """(a) Stream mode (OFB, CFB, CTR) vs block mode (ECB, CBC, XTS)."""
    rows, size_b = [], _nearest(dfs, bar_size)
    modes_sb = GROUPS["block"] + GROUPS["stream"]
    for d in ("enc", "dec"):
        fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(19, 5.2))
        for i, (df, lab) in enumerate(zip(dfs, labels)):
            w = _wide(df, d)
            gb, gs = _geomean(w, GROUPS["block"]), _geomean(w, GROUPS["stream"])
            if gb is None or gs is None:
                print(f"[WARN] {lab}: thieu mode block/stream, bo qua")
                continue
            ls, mk = STYLES[i % len(STYLES)], MARKERS[i % len(MARKERS)]
            ax1.plot(w.index, gb, color=GROUP_COLORS["block"], linestyle=ls, marker=mk, label=f"block, {lab}")
            ax1.plot(w.index, gs, color=GROUP_COLORS["stream"], linestyle=ls, marker=mk, label=f"stream, {lab}")
            ax2.plot(w.index, gs / gb, linestyle=ls, marker=mk, label=lab)
            for sz in w.index:
                rows.append({"platform": lab, "dir": d, "size_bytes": sz, "block_geomean_MBps": gb[sz],
                             "stream_geomean_MBps": gs[sz], "stream_over_block": gs[sz] / gb[sz]})
        set_size_axis(ax1, dfs); set_size_axis(ax2, dfs)
        ax1.set_ylabel("Throughput, geometric mean of group (MB/s)"); ax1.set_title("Group throughput")
        ax1.legend(fontsize=8); ax1.grid(True, alpha=0.3)
        ax2.axhline(1.0, color="gray", linestyle="--", linewidth=1)
        ax2.set_ylabel("stream / block throughput"); ax2.set_title("Ratio (>1: stream faster)")
        ax2.legend(fontsize=8); ax2.grid(True, alpha=0.3)

        modes = [m for m in modes_sb if any(m in df["mode"].values for df in dfs)]
        wb = 0.8 / len(dfs)
        for i, (df, lab) in enumerate(zip(dfs, labels)):
            s = df[(df["dir"] == d) & (df["size_bytes"] == size_b)].set_index("mode")
            vals = [s["thr_mean_MBps"].get(m, np.nan) for m in modes]
            errs = [s["thr_ci95"].get(m, 0) for m in modes]
            xs = [k + (i - (len(dfs) - 1) / 2) * wb for k in range(len(modes))]
            ax3.bar(xs, vals, wb, yerr=errs, capsize=2, color=[GROUP_COLORS[_group(m)] for m in modes],
                    hatch=HATCHES[i % len(HATCHES)], edgecolor="black", linewidth=0.5)
        ax3.set_xticks(range(len(modes))); ax3.set_xticklabels([m.upper() for m in modes])
        ax3.set_ylabel("Throughput (MB/s)"); ax3.set_title(f"Per mode at {human_size(size_b)} (mean, 95% CI)")
        ax3.grid(True, axis="y", alpha=0.3)
        _platform_legend(ax3, labels, [Patch(color=GROUP_COLORS["block"], label="block"),
                                       Patch(color=GROUP_COLORS["stream"], label="stream")])
        fig.suptitle(f"(a) Stream vs block modes: {d}", y=1.02, fontsize=13)
        save(fig, out_dir, f"analysis_stream_vs_block_{d}.png")
    if rows:
        t = pd.DataFrame(rows)
        t.round(3).to_csv(out_dir / "analysis_stream_vs_block.csv", index=False)
        print(f"[OK] Saved: {out_dir / 'analysis_stream_vs_block.csv'}")
        print(f"\n--- (a) stream/block tai {human_size(size_b)} ---")
        for _, r in t[t["size_bytes"] == size_b].iterrows():
            print(f"  {r['platform']:<16} {r['dir']}  stream/block = x{r['stream_over_block']:.3f}")


def analysis_aead(dfs, labels, bar_size, out_dir):
    """(b) AEAD (GCM, CCM) vs non-AEAD (CTR + trung binh nhan 6 mode non-AEAD)."""
    rows, size_b = [], _nearest(dfs, bar_size)
    for d in ("enc", "dec"):
        fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(19, 5.2))
        for i, (df, lab) in enumerate(zip(dfs, labels)):
            w = _wide(df, d)
            if "ctr" not in w.columns or not ({"gcm", "ccm"} & set(w.columns)):
                print(f"[WARN] {lab}: can CTR va GCM/CCM de so sanh AEAD, bo qua")
                continue
            na = _geomean(w, NON_AEAD)
            ls, mk = STYLES[i % len(STYLES)], MARKERS[i % len(MARKERS)]
            for m in ("ctr", "gcm", "ccm"):
                if m in w.columns:
                    ax1.plot(w.index, w[m], color=MODE_COLORS[m], linestyle=ls, marker=mk, label=f"{m.upper()}, {lab}")
            for m in ("gcm", "ccm"):
                if m in w.columns:
                    ax2.plot(w.index, w[m] / w["ctr"], color=MODE_COLORS[m], linestyle=ls, marker=mk,
                             label=f"{m.upper()} / CTR, {lab}")
            for sz in w.index:
                row = {"platform": lab, "dir": d, "size_bytes": sz, "ctr_MBps": w.loc[sz, "ctr"],
                       "nonaead_geomean_MBps": na[sz]}
                for m in ("gcm", "ccm"):
                    if m in w.columns:
                        row[f"{m}_MBps"] = w.loc[sz, m]
                        row[f"{m}_over_ctr"] = w.loc[sz, m] / w.loc[sz, "ctr"]
                        row[f"{m}_over_nonaead"] = w.loc[sz, m] / na[sz]
                rows.append(row)
        set_size_axis(ax1, dfs); set_size_axis(ax2, dfs)
        ax1.set_ylabel("Throughput (MB/s)"); ax1.set_title("CTR vs GCM vs CCM")
        ax1.legend(fontsize=7); ax1.grid(True, alpha=0.3)
        ax2.axhline(1.0, color="gray", linestyle="--", linewidth=1)
        ax2.set_ylabel("AEAD / CTR throughput"); ax2.set_title("Relative to CTR (<1: authentication costs)")
        ax2.legend(fontsize=7); ax2.grid(True, alpha=0.3)

        modes = [m for m in ("ctr", "gcm", "ccm") if any(m in df["mode"].values for df in dfs)]
        wb = 0.8 / len(dfs)
        for i, (df, lab) in enumerate(zip(dfs, labels)):
            s = df[(df["dir"] == d) & (df["size_bytes"] == size_b)].set_index("mode")
            vals = [s["thr_mean_MBps"].get(m, np.nan) for m in modes]
            errs = [s["thr_ci95"].get(m, 0) for m in modes]
            xs = [k + (i - (len(dfs) - 1) / 2) * wb for k in range(len(modes))]
            ax3.bar(xs, vals, wb, yerr=errs, capsize=2, color=[MODE_COLORS[m] for m in modes],
                    hatch=HATCHES[i % len(HATCHES)], edgecolor="black", linewidth=0.5)
        ax3.set_xticks(range(len(modes))); ax3.set_xticklabels([m.upper() for m in modes])
        ax3.set_ylabel("Throughput (MB/s)"); ax3.set_title(f"At {human_size(size_b)} (mean, 95% CI)")
        ax3.grid(True, axis="y", alpha=0.3)
        _platform_legend(ax3, labels)
        fig.suptitle(f"(b) AEAD vs non-AEAD: {d}", y=1.02, fontsize=13)
        save(fig, out_dir, f"analysis_aead_vs_nonaead_{d}.png")
    if rows:
        t = pd.DataFrame(rows)
        t.round(3).to_csv(out_dir / "analysis_aead_vs_nonaead.csv", index=False)
        print(f"[OK] Saved: {out_dir / 'analysis_aead_vs_nonaead.csv'}")
        print(f"\n--- (b) AEAD tai {human_size(size_b)} ---")
        for _, r in t[t["size_bytes"] == size_b].iterrows():
            parts = [f"{m.upper()}/CTR=x{r[f'{m}_over_ctr']:.3f} (vs non-AEAD x{r[f'{m}_over_nonaead']:.3f})"
                     for m in ("gcm", "ccm") if f"{m}_over_ctr" in r and not pd.isna(r[f"{m}_over_ctr"])]
            print(f"  {r['platform']:<16} {r['dir']}  " + "; ".join(parts))


def _cost_model(df, direction, mode):
    """latency(size) ~ a + b*size. b tu 2 kich thuoc lon nhat (chi phi theo byte), a = ngoai suy ve 1 KB
    (chi phi co dinh moi thao tac: khoi tao, cap phat, tinh/so sanh tag, loc, ...)."""
    s = df[(df["dir"] == direction) & (df["mode"] == mode)].sort_values("size_bytes")
    if len(s) < 3:
        return None
    sz = s["size_bytes"].to_numpy(float)
    lat = s["lat_mean_us"].to_numpy(float)
    b = (lat[-1] - lat[-2]) / (sz[-1] - sz[-2])
    return lat[0] - b * sz[0], b


def analysis_tag_overhead(dfs, labels, bar_size, out_dir):
    """(c) Overhead cua tag (GCM/CCM): chi phi co dinh, chi phi theo byte, dung luong tag."""
    rows = []
    modes = [m for m in ("ctr", "gcm", "ccm") if any(m in df["mode"].values for df in dfs)]
    costs = {}
    for d in ("enc", "dec"):
        fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(19, 5.2))
        for i, (df, lab) in enumerate(zip(dfs, labels)):
            lat = _wide(df, d, "lat_mean_us")
            ls, mk = STYLES[i % len(STYLES)], MARKERS[i % len(MARKERS)]
            if "ctr" in lat.columns:
                for m in ("gcm", "ccm"):
                    if m in lat.columns:
                        ax1.plot(lat.index, lat[m] - lat["ctr"], color=MODE_COLORS[m], linestyle=ls, marker=mk,
                                 label=f"{m.upper()} - CTR, {lab}")
            base = _cost_model(df, d, "ctr")
            for m in modes:
                cm = _cost_model(df, d, m)
                if cm is None:
                    continue
                a, b = cm
                costs[(i, m)] = (a, b * 1000.0)  # us/op, ns/byte
                row = {"platform": lab, "dir": d, "mode": m, "fixed_us_per_op": a, "ns_per_byte": b * 1000.0,
                       "equiv_MBps": (1.0 / b) if b > 0 else np.nan}
                if base is not None and m != "ctr":
                    row["delta_fixed_us_vs_ctr"] = a - base[0]
                    row["delta_ns_per_byte_vs_ctr"] = (b - base[1]) * 1000.0
                    row["per_byte_overhead_pct_vs_ctr"] = (b / base[1] - 1) * 100.0 if base[1] > 0 else np.nan
                rows.append(row)
            # thanh cot cho 2 panel con lai
        set_size_axis(ax1, dfs)
        ax1.axhline(0, color="gray", linestyle="--", linewidth=1)
        ax1.set_ylabel("Extra latency vs CTR (µs/op)"); ax1.set_title("Authentication overhead per operation")
        ax1.legend(fontsize=7); ax1.grid(True, alpha=0.3)
        wb = 0.8 / len(dfs)
        for ax, idx, ylab, title in ((ax2, 0, "Fixed cost (µs/op)", "Fixed cost per operation (extrapolated to 0 B)"),
                                     (ax3, 1, "Per-byte cost (ns/B)", "Per-byte cost (from two largest payloads)")):
            for i, lab in enumerate(labels):
                vals = [costs.get((i, m), (np.nan, np.nan))[idx] for m in modes]
                xs = [k + (i - (len(dfs) - 1) / 2) * wb for k in range(len(modes))]
                ax.bar(xs, vals, wb, color=[MODE_COLORS[m] for m in modes], hatch=HATCHES[i % len(HATCHES)],
                       edgecolor="black", linewidth=0.5)
            ax.set_xticks(range(len(modes))); ax.set_xticklabels([m.upper() for m in modes])
            ax.set_ylabel(ylab); ax.set_title(title); ax.grid(True, axis="y", alpha=0.3)
            _platform_legend(ax, labels)
        fig.suptitle(f"(c) Tag overhead (GCM / CCM vs CTR): {d}", y=1.02, fontsize=13)
        save(fig, out_dir, f"analysis_tag_overhead_{d}.png")

    # dung luong tag so voi payload (ly thuyet, khong phu thuoc nen tang)
    sizes = sorted(set().union(*[set(d["size_bytes"].unique()) for d in dfs]))
    fig = plt.figure(figsize=(7.5, 5))
    srows = []
    for tag in (4, 8, 16):
        pct = [tag / s * 100 for s in sizes]
        plt.plot(sizes, pct, marker="o", label=f"tag {tag} B")
        srows += [{"size_bytes": s, "tag_bytes": tag, "overhead_pct": tag / s * 100} for s in sizes]
    plt.xscale("log", base=2); plt.yscale("log")
    plt.xticks(sizes, [human_size(x) for x in sizes], rotation=45, ha="right", fontsize=8)
    plt.minorticks_off()
    plt.ylabel("Tag size / payload (%)"); plt.xlabel("Payload")
    plt.title("Storage overhead of the authentication tag"); plt.legend(); plt.grid(True, alpha=0.3)
    save(fig, out_dir, "analysis_tag_storage_overhead.png")
    pd.DataFrame(srows).round(4).to_csv(out_dir / "analysis_tag_storage_overhead.csv", index=False)

    if rows:
        t = pd.DataFrame(rows)
        t.round(4).to_csv(out_dir / "analysis_tag_overhead.csv", index=False)
        print(f"[OK] Saved: {out_dir / 'analysis_tag_overhead.csv'}")
        print("\n--- (c) chi phi co dinh va theo byte (so voi CTR) ---")
        for _, r in t[t["mode"].isin(["gcm", "ccm"])].iterrows():
            if "delta_fixed_us_vs_ctr" in r and not pd.isna(r["delta_fixed_us_vs_ctr"]):
                print(f"  {r['platform']:<16} {r['dir']}  {r['mode'].upper():<3} "
                      f"+{r['delta_fixed_us_vs_ctr']:.2f} us/op co dinh, "
                      f"{r['delta_ns_per_byte_vs_ctr']:+.3f} ns/B ({r['per_byte_overhead_pct_vs_ctr']:+.0f}%)")


def run_analysis(dfs, labels, bar_size, mode_dir):
    out = mode_dir
    out.mkdir(parents=True, exist_ok=True)
    print(f"\n[INFO] Phan tich ({', '.join(labels)}) -> {out}")
    analysis_stream_block(dfs, labels, bar_size, out)
    analysis_aead(dfs, labels, bar_size, out)
    analysis_tag_overhead(dfs, labels, bar_size, out)


# ------------------------------------------------------------------ modes
def usable(df):
    """CSV day du (nhieu mode va nhieu kich thuoc), khong phai lan chay 1 mode / 1 kich thuoc."""
    return df["mode"].nunique() >= 3 and df["size_bytes"].nunique() >= 3


def run_compare(args):
    paths = [resolve_in(p) for p in args.compare]
    labels = args.labels or [short_label(p) for p in paths]
    if len(labels) != len(paths):
        sys.exit("[ERROR] So luong --labels phai bang so file --compare")
    dfs = [load_csv(p, lab) for p, lab in zip(paths, labels)]
    out_dir = resolve_out(args.out_dir) if args.out_dir else ROOT / "output" / "compare"
    mode_dir = resolve_out(args.analysis_dir) if args.analysis_dir else out_dir / "mode_cmp"
    do_compare(dfs, labels, out_dir, mode_dir, args, args.analysis)
    print(f"\n[DONE] {out_dir}")


def run_single(args):
    csv_path = resolve_in(args.csv_file)
    os_name = norm_os(args.platform) if args.platform else detect_os(csv_path)
    label = short_label(csv_path)
    df = load_csv(csv_path, label)
    base = ROOT / "output" / (os_name or label)
    if os_name is None:
        print(f"[WARN] Khong nhan ra nen tang tu duong dan, luu vao {base}. Dung --platform windows|ubuntu.")
    plots_dir = resolve_out(args.out_dir) if args.out_dir else base / "plots"
    mode_dir = resolve_out(args.analysis_dir) if args.analysis_dir else base / "mode_cmp"
    print(f"[INFO] Doc: {csv_path}")
    if not args.no_basic:
        print(f"[INFO] Do thi -> {plots_dir}")
        plot_single(df, plots_dir)
    if args.analysis:
        run_analysis([df], [label], args.bar_size, mode_dir)
    print("\n[DONE]")


def run_auto(args):
    root = resolve_out(args.root) if args.root else ROOT / "output"
    per_os = {}
    for os_name in OS_NAMES:
        bdir = root / os_name / "benchmarks"
        if not bdir.is_dir():
            print(f"[WARN] Khong co thu muc {bdir}")
            continue
        items = []
        for p in sorted(bdir.glob("*.csv")):
            label = short_label(p)
            try:
                df = load_csv(p, label)
            except SystemExit:
                print(f"[WARN] Bo qua {p.name}: khong dung dinh dang bench")
                continue
            if not usable(df):
                print(f"[WARN] Bo qua {p.name}: chi 1 phan (can >= 3 mode va >= 3 kich thuoc)")
                continue
            items.append((label, df))
        if items:
            per_os[os_name] = items
            print(f"[INFO] {os_name}: {', '.join(l for l, _ in items)}")
    if not per_os:
        sys.exit(f"[ERROR] Khong co CSV hop le trong {root}/<os>/benchmarks/")

    for os_name, items in per_os.items():
        base = root / os_name
        if not args.no_basic:
            for label, df in items:
                plot_single(df, base / "plots" if len(items) == 1 else base / "plots" / label)
        run_analysis([d for _, d in items], [l for l, _ in items], args.bar_size, base / "mode_cmp")

    if len(per_os) >= 2:
        chosen = []
        for os_name in OS_NAMES:
            if os_name in per_os:
                items = per_os[os_name]
                chosen.append(next((it for it in items if args.prefer.lower() in it[0].lower()), items[0]))
        out_dir = root / "compare"
        do_compare([d for _, d in chosen], [l for l, _ in chosen], out_dir, out_dir / "mode_cmp", args, True)
    else:
        print("[INFO] Chi co 1 he dieu hanh, bo qua so sanh cheo Windows vs Ubuntu")
    print(f"\n[DONE] Ket qua trong {root}")


# ------------------------------------------------------------------ main
def parse_args():
    p = argparse.ArgumentParser(description="Plot / compare / analyze AES benchmark CSVs",
                                formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    p.add_argument("csv_file", nargs="?", default=None, help="CSV cho che do mot file")
    p.add_argument("--auto", action="store_true", help="Quet output/<os>/benchmarks va chay tat ca")
    p.add_argument("--root", default=None, help="Thu muc output (mac dinh <goc lab>/output)")
    p.add_argument("--prefer", default="gcc", help="Chuoi trong ten file de chon CSV so sanh cheo OS (mac dinh gcc)")
    p.add_argument("--compare", nargs="+", metavar="CSV", help="So sanh 2+ file CSV (file dau la baseline)")
    p.add_argument("--labels", nargs="+", help="Ten hien thi cho tung file --compare")
    p.add_argument("--out-dir", default=None, help="Thu muc luu do thi")
    p.add_argument("--analysis-dir", default=None, help="Thu muc luu phan tich (mac dinh <os>/mode_cmp)")
    p.add_argument("--platform", choices=["windows", "ubuntu", "linux"], default=None,
                   help="Nen tang (neu khong tu nhan tu duong dan)")
    p.add_argument("--bar-size", type=int, default=1 << 20, help="Payload cho bieu do cot (mac dinh 1 MiB)")
    p.add_argument("--analysis", action="store_true",
                   help="Them phan tich: stream vs block, AEAD vs non-AEAD, overhead cua tag")
    p.add_argument("--no-basic", action="store_true", help="Bo qua cac do thi co ban (chi chay phan tich)")
    return p.parse_args()


def main():
    args = parse_args()
    if args.auto:
        run_auto(args)
    elif args.compare:
        if len(args.compare) < 2:
            sys.exit("[ERROR] --compare can it nhat 2 file CSV")
        run_compare(args)
    elif args.csv_file:
        run_single(args)
    else:
        sys.exit("[ERROR] Thieu tham so. Dung --auto, hoac <file.csv>, hoac --compare a.csv b.csv (xem --help)")


if __name__ == "__main__":
    main()