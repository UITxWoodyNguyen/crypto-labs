# Lab 3 analysis — windows / msvc

Hardware: Intel(R) Core(TM) i7-8565U CPU @ 1.80GHz (8 logical CPUs, 15.8 GB RAM), Windows-10-10.0.19045-SP0

- RSA-3072: keygen 238.5 ms, encrypt 0.072 ms, decrypt 3.024 ms -> decrypt/encrypt = 41.8x (public exponent 65537 vs full-size private exponent).
- RSA-4096: keygen 820.1 ms, encrypt 0.147 ms, decrypt 5.418 ms -> decrypt/encrypt = 37.0x (public exponent 65537 vs full-size private exponent).
- RSA-4096 / RSA-3072 keygen: 3.44x slower.
- RSA-4096 / RSA-3072 OAEP enc: 2.02x slower.
- RSA-4096 / RSA-3072 OAEP dec: 1.79x slower.

- Hybrid-3072 @ 1 KiB: 0.105 ms vs AES-GCM 0.002 ms (x61.94); throughput 10.2 MB/s vs 666.4 MB/s.
- Hybrid-3072 @ 1 MiB: 0.779 ms vs AES-GCM 0.626 ms (x1.25); throughput 1346.8 MB/s vs 1684.7 MB/s.
- Hybrid-3072 @ 100 MiB: 86.190 ms vs AES-GCM 81.708 ms (x1.05); throughput 1259.7 MB/s vs 1292.7 MB/s.
- Hybrid-4096 @ 1 KiB: 0.137 ms vs AES-GCM 0.002 ms (x80.75); throughput 7.8 MB/s vs 666.4 MB/s.
- Hybrid-4096 @ 1 MiB: 1.019 ms vs AES-GCM 0.626 ms (x1.63); throughput 1051.0 MB/s vs 1684.7 MB/s.
- Hybrid-4096 @ 100 MiB: 88.719 ms vs AES-GCM 81.708 ms (x1.09); throughput 1215.2 MB/s vs 1292.7 MB/s.

## Tables

### RSA (ms/op)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| RSA-3072 keygen | 3072 | - | 30 | 238.548 | 199.112 | 154.614 | 180.820 | 296.275 | ms/op |
| RSA-3072 OAEP enc | 3072 | 32 B | 30 | 0.072 | 0.071 | 0.009 | 0.069 | 0.076 | ms/op |
| RSA-3072 OAEP dec | 3072 | 32 B | 30 | 3.024 | 2.676 | 1.184 | 2.583 | 3.466 | ms/op |
| RSA-4096 keygen | 4096 | - | 30 | 820.093 | 564.042 | 656.286 | 575.060 | 1065.127 | ms/op |
| RSA-4096 OAEP enc | 4096 | 32 B | 30 | 0.147 | 0.137 | 0.026 | 0.137 | 0.156 | ms/op |
| RSA-4096 OAEP dec | 4096 | 32 B | 30 | 5.418 | 5.075 | 1.095 | 5.009 | 5.827 | ms/op |

### AES-GCM / Hybrid latency (ms/op)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| AES-256-GCM enc | 0 | 1 KiB | 30 | 0.002 | 0.001 | 0.001 | 0.001 | 0.002 | ms/op |
| AES-256-GCM dec | 0 | 1 KiB | 30 | 0.002 | 0.002 | 0.000 | 0.002 | 0.002 | ms/op |
| AES-256-GCM enc | 0 | 1 MiB | 30 | 0.626 | 0.616 | 0.050 | 0.607 | 0.644 | ms/op |
| AES-256-GCM dec | 0 | 1 MiB | 30 | 0.788 | 0.742 | 0.152 | 0.731 | 0.844 | ms/op |
| AES-256-GCM enc | 0 | 100 MiB | 30 | 81.708 | 80.495 | 7.270 | 78.994 | 84.423 | ms/op |
| AES-256-GCM dec | 0 | 100 MiB | 30 | 78.377 | 76.676 | 8.432 | 75.229 | 81.525 | ms/op |
| Hybrid-3072 enc | 3072 | 1 KiB | 30 | 0.105 | 0.098 | 0.028 | 0.095 | 0.116 | ms/op |
| Hybrid-3072 dec | 3072 | 1 KiB | 30 | 2.486 | 2.421 | 0.355 | 2.354 | 2.619 | ms/op |
| Hybrid-3072 enc | 3072 | 1 MiB | 30 | 0.779 | 0.782 | 0.018 | 0.772 | 0.786 | ms/op |
| Hybrid-3072 dec | 3072 | 1 MiB | 30 | 3.055 | 3.009 | 0.183 | 2.987 | 3.124 | ms/op |
| Hybrid-3072 enc | 3072 | 100 MiB | 30 | 86.190 | 80.440 | 20.485 | 78.542 | 93.838 | ms/op |
| Hybrid-3072 dec | 3072 | 100 MiB | 30 | 79.769 | 79.487 | 4.057 | 78.254 | 81.284 | ms/op |
| Hybrid-4096 enc | 4096 | 1 KiB | 30 | 0.137 | 0.124 | 0.038 | 0.123 | 0.151 | ms/op |
| Hybrid-4096 dec | 4096 | 1 KiB | 30 | 5.277 | 5.015 | 0.653 | 5.033 | 5.520 | ms/op |
| Hybrid-4096 enc | 4096 | 1 MiB | 30 | 1.019 | 1.007 | 0.157 | 0.960 | 1.077 | ms/op |
| Hybrid-4096 dec | 4096 | 1 MiB | 30 | 6.176 | 6.150 | 0.817 | 5.870 | 6.481 | ms/op |
| Hybrid-4096 enc | 4096 | 100 MiB | 30 | 88.719 | 85.365 | 17.346 | 82.243 | 95.195 | ms/op |
| Hybrid-4096 dec | 4096 | 100 MiB | 30 | 82.590 | 78.022 | 10.887 | 78.525 | 86.654 | ms/op |

### AES-GCM / Hybrid throughput (MB/s)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| AES-256-GCM enc | 0 | 1 KiB | 30 | 666.406 | 683.215 | 114.575 | 623.628 | 709.184 | MB/s |
| AES-256-GCM dec | 0 | 1 KiB | 30 | 630.722 | 636.738 | 31.361 | 619.012 | 642.431 | MB/s |
| AES-256-GCM enc | 0 | 1 MiB | 30 | 1684.733 | 1701.710 | 114.471 | 1641.994 | 1727.472 | MB/s |
| AES-256-GCM dec | 0 | 1 MiB | 30 | 1363.424 | 1412.668 | 180.083 | 1296.187 | 1430.661 | MB/s |
| AES-256-GCM enc | 0 | 100 MiB | 30 | 1292.687 | 1302.662 | 109.291 | 1251.882 | 1333.492 | MB/s |
| AES-256-GCM dec | 0 | 100 MiB | 30 | 1348.566 | 1367.539 | 104.232 | 1309.650 | 1387.483 | MB/s |
| Hybrid-3072 enc | 3072 | 1 KiB | 30 | 10.156 | 10.452 | 1.677 | 9.530 | 10.782 | MB/s |
| Hybrid-3072 dec | 3072 | 1 KiB | 30 | 0.419 | 0.423 | 0.052 | 0.399 | 0.438 | MB/s |
| Hybrid-3072 enc | 3072 | 1 MiB | 30 | 1346.830 | 1340.274 | 31.068 | 1335.230 | 1358.429 | MB/s |
| Hybrid-3072 dec | 3072 | 1 MiB | 30 | 344.315 | 348.514 | 19.200 | 337.146 | 351.483 | MB/s |
| Hybrid-3072 enc | 3072 | 100 MiB | 30 | 1259.708 | 1303.831 | 194.842 | 1186.961 | 1332.455 | MB/s |
| Hybrid-3072 dec | 3072 | 100 MiB | 30 | 1317.676 | 1319.192 | 64.416 | 1293.625 | 1341.726 | MB/s |
| Hybrid-4096 enc | 4096 | 1 KiB | 30 | 7.845 | 8.245 | 1.474 | 7.294 | 8.395 | MB/s |
| Hybrid-4096 dec | 4096 | 1 KiB | 30 | 0.197 | 0.204 | 0.022 | 0.188 | 0.205 | MB/s |
| Hybrid-4096 enc | 4096 | 1 MiB | 30 | 1051.000 | 1041.180 | 148.665 | 995.494 | 1106.506 | MB/s |
| Hybrid-4096 dec | 4096 | 1 MiB | 30 | 172.301 | 170.506 | 19.981 | 164.841 | 179.761 | MB/s |
| Hybrid-4096 enc | 4096 | 100 MiB | 30 | 1215.156 | 1228.399 | 181.684 | 1147.322 | 1282.991 | MB/s |
| Hybrid-4096 dec | 4096 | 100 MiB | 30 | 1288.561 | 1343.945 | 150.068 | 1232.531 | 1344.591 | MB/s |

### Hybrid overhead
| bits | size | hybrid_ms | aes_gcm_ms | overhead_ms | hybrid_over_aes |
|---|---|---|---|---|---|
| 3072 | 1 KiB | 0.105 | 0.002 | 0.103 | 61.939 |
| 3072 | 1 MiB | 0.779 | 0.626 | 0.153 | 1.245 |
| 3072 | 100 MiB | 86.190 | 81.708 | 4.482 | 1.055 |
| 4096 | 1 KiB | 0.137 | 0.002 | 0.135 | 80.747 |
| 4096 | 1 MiB | 1.019 | 0.626 | 0.393 | 1.628 |
| 4096 | 100 MiB | 88.719 | 81.708 | 7.011 | 1.086 |

## Test status
- ctest passed: True
- KAT passed: True
- CLI tests: 30/30 passed
