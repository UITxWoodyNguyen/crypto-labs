#!/usr/bin/env python3
"""Ve do thi throughput/latency tu bench.csv (pip install pandas matplotlib)."""
import sys, pandas as pd, matplotlib.pyplot as plt
df = pd.read_csv(sys.argv[1] if len(sys.argv) > 1 else "bench.csv")
for d in ("enc", "dec"):
    s = df[df.dir == d]
    plt.figure(figsize=(8, 5))
    for m, g in s.groupby("mode"):
        plt.errorbar(g.size_bytes, g.thr_mean_MBps, yerr=g.thr_ci95, marker="o", capsize=3, label=m)
    plt.xscale("log", base=2); plt.xlabel("Payload (bytes)"); plt.ylabel("Throughput (MB/s)")
    plt.title(f"AES-256 {d} throughput (mean, 95% CI)"); plt.legend(); plt.grid(True, alpha=.3)
    plt.savefig(f"throughput_{d}.png", dpi=150, bbox_inches="tight")
    plt.figure(figsize=(8, 5))
    for m, g in s.groupby("mode"):
        plt.errorbar(g.size_bytes, g.lat_mean_us, yerr=g.lat_ci95, marker="o", capsize=3, label=m)
    plt.xscale("log", base=2); plt.yscale("log"); plt.xlabel("Payload (bytes)"); plt.ylabel("Latency (us/op)")
    plt.title(f"AES-256 {d} latency"); plt.legend(); plt.grid(True, alpha=.3)
    plt.savefig(f"latency_{d}.png", dpi=150, bbox_inches="tight")
print("Saved throughput_*.png, latency_*.png")
