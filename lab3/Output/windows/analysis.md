# Lab 3 analysis — windows / msvc

Hardware: Intel(R) Core(TM) i7-8565U CPU @ 1.80GHz (8 logical CPUs, 15.8 GB RAM), Windows-10-10.0.19045-SP0

- RSA-3072: keygen 644.4 ms, encrypt 0.181 ms, decrypt 4.839 ms -> decrypt/encrypt = 26.8x (public exponent 65537 vs full-size private exponent).
- RSA-4096: keygen 1003.3 ms, encrypt 0.168 ms, decrypt 6.573 ms -> decrypt/encrypt = 39.2x (public exponent 65537 vs full-size private exponent).
- RSA-4096 / RSA-3072 keygen: 1.56x slower.
- RSA-4096 / RSA-3072 OAEP enc: 0.93x slower.
- RSA-4096 / RSA-3072 OAEP dec: 1.36x slower.

- Hybrid-3072 @ 1 KiB: 0.116 ms vs AES-GCM 0.005 ms (x24.61); throughput 8.9 MB/s vs 225.6 MB/s.
- Hybrid-3072 @ 1 MiB: 0.948 ms vs AES-GCM 1.604 ms (x0.59); throughput 1108.8 MB/s vs 666.0 MB/s.
- Hybrid-3072 @ 100 MiB: 92.582 ms vs AES-GCM 91.977 ms (x1.01); throughput 1154.7 MB/s vs 1167.4 MB/s.
- Hybrid-4096 @ 1 KiB: 0.186 ms vs AES-GCM 0.005 ms (x39.36); throughput 5.6 MB/s vs 225.6 MB/s.
- Hybrid-4096 @ 1 MiB: 1.324 ms vs AES-GCM 1.604 ms (x0.83); throughput 796.3 MB/s vs 666.0 MB/s.
- Hybrid-4096 @ 100 MiB: 109.340 ms vs AES-GCM 91.977 ms (x1.19); throughput 963.3 MB/s vs 1167.4 MB/s.

## Tables

### RSA (ms/op)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| RSA-3072 keygen | 3072 | - | 100 | 644.355 | 501.331 | 505.031 | 543.349 | 745.361 | ms/op |
| RSA-3072 OAEP enc | 3072 | 32 B | 100 | 0.181 | 0.172 | 0.028 | 0.175 | 0.186 | ms/op |
| RSA-3072 OAEP dec | 3072 | 32 B | 100 | 4.839 | 5.396 | 1.054 | 4.628 | 5.049 | ms/op |
| RSA-4096 keygen | 4096 | - | 100 | 1003.308 | 797.819 | 719.703 | 859.368 | 1147.249 | ms/op |
| RSA-4096 OAEP enc | 4096 | 32 B | 100 | 0.168 | 0.157 | 0.024 | 0.163 | 0.172 | ms/op |
| RSA-4096 OAEP dec | 4096 | 32 B | 100 | 6.573 | 6.080 | 1.576 | 6.258 | 6.888 | ms/op |

### AES-GCM / Hybrid latency (ms/op)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| AES-256-GCM enc | 0 | 1 KiB | 100 | 0.005 | 0.005 | 0.001 | 0.005 | 0.005 | ms/op |
| AES-256-GCM dec | 0 | 1 KiB | 100 | 0.005 | 0.005 | 0.001 | 0.005 | 0.005 | ms/op |
| AES-256-GCM enc | 0 | 1 MiB | 100 | 1.604 | 1.598 | 0.243 | 1.556 | 1.653 | ms/op |
| AES-256-GCM dec | 0 | 1 MiB | 100 | 1.077 | 1.060 | 0.091 | 1.059 | 1.095 | ms/op |
| AES-256-GCM enc | 0 | 100 MiB | 100 | 91.977 | 89.720 | 16.385 | 88.700 | 95.254 | ms/op |
| AES-256-GCM dec | 0 | 100 MiB | 100 | 160.138 | 115.309 | 72.876 | 145.563 | 174.714 | ms/op |
| Hybrid-3072 enc | 3072 | 1 KiB | 100 | 0.116 | 0.115 | 0.008 | 0.114 | 0.118 | ms/op |
| Hybrid-3072 dec | 3072 | 1 KiB | 100 | 3.687 | 3.370 | 1.269 | 3.433 | 3.941 | ms/op |
| Hybrid-3072 enc | 3072 | 1 MiB | 100 | 0.948 | 0.934 | 0.052 | 0.938 | 0.959 | ms/op |
| Hybrid-3072 dec | 3072 | 1 MiB | 100 | 3.986 | 3.959 | 0.226 | 3.941 | 4.032 | ms/op |
| Hybrid-3072 enc | 3072 | 100 MiB | 100 | 92.582 | 91.099 | 13.154 | 89.951 | 95.213 | ms/op |
| Hybrid-3072 dec | 3072 | 100 MiB | 100 | 69.342 | 66.960 | 8.487 | 67.645 | 71.040 | ms/op |
| Hybrid-4096 enc | 4096 | 1 KiB | 100 | 0.186 | 0.184 | 0.016 | 0.182 | 0.189 | ms/op |
| Hybrid-4096 dec | 4096 | 1 KiB | 100 | 7.974 | 8.094 | 2.120 | 7.550 | 8.398 | ms/op |
| Hybrid-4096 enc | 4096 | 1 MiB | 100 | 1.324 | 1.295 | 0.108 | 1.302 | 1.346 | ms/op |
| Hybrid-4096 dec | 4096 | 1 MiB | 100 | 10.007 | 9.164 | 2.792 | 9.448 | 10.565 | ms/op |
| Hybrid-4096 enc | 4096 | 100 MiB | 100 | 109.340 | 108.363 | 7.547 | 107.831 | 110.850 | ms/op |
| Hybrid-4096 dec | 4096 | 100 MiB | 100 | 131.474 | 122.622 | 27.006 | 126.073 | 136.875 | ms/op |

### AES-GCM / Hybrid throughput (MB/s)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| AES-256-GCM enc | 0 | 1 KiB | 100 | 225.615 | 221.737 | 44.388 | 216.738 | 234.493 | MB/s |
| AES-256-GCM dec | 0 | 1 KiB | 100 | 215.837 | 199.513 | 49.608 | 205.915 | 225.758 | MB/s |
| AES-256-GCM enc | 0 | 1 MiB | 100 | 665.995 | 656.108 | 86.271 | 648.741 | 683.250 | MB/s |
| AES-256-GCM dec | 0 | 1 MiB | 100 | 980.327 | 989.610 | 78.819 | 964.563 | 996.091 | MB/s |
| AES-256-GCM enc | 0 | 100 MiB | 100 | 1167.397 | 1168.718 | 166.402 | 1134.117 | 1200.678 | MB/s |
| AES-256-GCM dec | 0 | 100 MiB | 100 | 768.286 | 909.363 | 267.046 | 714.877 | 821.695 | MB/s |
| Hybrid-3072 enc | 3072 | 1 KiB | 100 | 8.867 | 8.942 | 0.617 | 8.744 | 8.991 | MB/s |
| Hybrid-3072 dec | 3072 | 1 KiB | 100 | 0.306 | 0.304 | 0.090 | 0.288 | 0.324 | MB/s |
| Hybrid-3072 enc | 3072 | 1 MiB | 100 | 1108.819 | 1122.334 | 56.772 | 1097.465 | 1120.174 | MB/s |
| Hybrid-3072 dec | 3072 | 1 MiB | 100 | 263.832 | 264.832 | 14.300 | 260.972 | 266.692 | MB/s |
| Hybrid-3072 enc | 3072 | 100 MiB | 100 | 1154.694 | 1151.026 | 159.653 | 1122.763 | 1186.625 | MB/s |
| Hybrid-3072 dec | 3072 | 100 MiB | 100 | 1532.373 | 1565.981 | 168.680 | 1498.637 | 1566.109 | MB/s |
| Hybrid-4096 enc | 4096 | 1 KiB | 100 | 5.554 | 5.578 | 0.451 | 5.464 | 5.644 | MB/s |
| Hybrid-4096 dec | 4096 | 1 KiB | 100 | 0.138 | 0.127 | 0.038 | 0.130 | 0.145 | MB/s |
| Hybrid-4096 enc | 4096 | 1 MiB | 100 | 796.309 | 809.606 | 54.146 | 785.480 | 807.138 | MB/s |
| Hybrid-4096 dec | 4096 | 1 MiB | 100 | 109.919 | 114.430 | 19.596 | 106.000 | 113.839 | MB/s |
| Hybrid-4096 enc | 4096 | 100 MiB | 100 | 963.308 | 967.650 | 63.319 | 950.644 | 975.972 | MB/s |
| Hybrid-4096 dec | 4096 | 100 MiB | 100 | 824.269 | 855.139 | 134.362 | 797.396 | 851.141 | MB/s |

### Hybrid overhead
| bits | size | hybrid_ms | aes_gcm_ms | overhead_ms | hybrid_over_aes |
|---|---|---|---|---|---|
| 3072 | 1 KiB | 0.116 | 0.005 | 0.111 | 24.611 |
| 3072 | 1 MiB | 0.948 | 1.604 | -0.656 | 0.591 |
| 3072 | 100 MiB | 92.582 | 91.977 | 0.605 | 1.007 |
| 4096 | 1 KiB | 0.186 | 0.005 | 0.181 | 39.364 |
| 4096 | 1 MiB | 1.324 | 1.604 | -0.280 | 0.825 |
| 4096 | 100 MiB | 109.340 | 91.977 | 17.363 | 1.189 |

## Test status
- ctest passed: True
- KAT passed: True
- CLI tests: 30/30 passed
