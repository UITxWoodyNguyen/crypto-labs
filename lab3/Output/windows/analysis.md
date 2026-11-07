# Lab 3 analysis — windows / msvc

Hardware: Intel(R) Core(TM) i7-8565U CPU @ 1.80GHz (8 logical CPUs, 15.8 GB RAM), Windows-10-10.0.19045-SP0

- RSA-3072: keygen 208.0 ms, encrypt 0.068 ms, decrypt 2.220 ms -> decrypt/encrypt = 32.7x (public exponent 65537 vs full-size private exponent).
- RSA-4096: keygen 569.8 ms, encrypt 0.095 ms, decrypt 4.523 ms -> decrypt/encrypt = 47.5x (public exponent 65537 vs full-size private exponent).
- RSA-4096 / RSA-3072 keygen: 2.74x slower.
- RSA-4096 / RSA-3072 OAEP enc: 1.40x slower.
- RSA-4096 / RSA-3072 OAEP dec: 2.04x slower.

- Hybrid-3072 @ 1 KiB: 0.076 ms vs AES-GCM 0.001 ms (x51.97); throughput 13.5 MB/s vs 705.9 MB/s.
- Hybrid-3072 @ 1 MiB: 0.711 ms vs AES-GCM 0.616 ms (x1.15); throughput 1475.8 MB/s vs 1706.4 MB/s.
- Hybrid-3072 @ 100 MiB: 65.843 ms vs AES-GCM 66.167 ms (x1.00); throughput 1594.8 MB/s vs 1585.7 MB/s.
- Hybrid-4096 @ 1 KiB: 0.108 ms vs AES-GCM 0.001 ms (x74.03); throughput 9.5 MB/s vs 705.9 MB/s.
- Hybrid-4096 @ 1 MiB: 0.807 ms vs AES-GCM 0.616 ms (x1.31); throughput 1305.7 MB/s vs 1706.4 MB/s.
- Hybrid-4096 @ 100 MiB: 66.471 ms vs AES-GCM 66.167 ms (x1.00); throughput 1579.6 MB/s vs 1585.7 MB/s.

## Tables

### RSA (ms/op)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| RSA-3072 keygen | 3072 | - | 100 | 207.952 | 169.715 | 160.855 | 175.781 | 240.123 | ms/op |
| RSA-3072 OAEP enc | 3072 | 32 B | 100 | 0.068 | 0.068 | 0.003 | 0.067 | 0.069 | ms/op |
| RSA-3072 OAEP dec | 3072 | 32 B | 100 | 2.220 | 2.102 | 0.368 | 2.147 | 2.294 | ms/op |
| RSA-4096 keygen | 4096 | - | 100 | 569.790 | 462.598 | 365.953 | 496.600 | 642.981 | ms/op |
| RSA-4096 OAEP enc | 4096 | 32 B | 100 | 0.095 | 0.094 | 0.006 | 0.094 | 0.096 | ms/op |
| RSA-4096 OAEP dec | 4096 | 32 B | 100 | 4.523 | 4.376 | 0.429 | 4.437 | 4.608 | ms/op |

### AES-GCM / Hybrid latency (ms/op)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| AES-256-GCM enc | 0 | 1 KiB | 100 | 0.001 | 0.001 | 0.000 | 0.001 | 0.001 | ms/op |
| AES-256-GCM dec | 0 | 1 KiB | 100 | 0.001 | 0.001 | 0.000 | 0.001 | 0.001 | ms/op |
| AES-256-GCM enc | 0 | 1 MiB | 100 | 0.616 | 0.619 | 0.030 | 0.610 | 0.622 | ms/op |
| AES-256-GCM dec | 0 | 1 MiB | 100 | 0.737 | 0.712 | 0.080 | 0.721 | 0.753 | ms/op |
| AES-256-GCM enc | 0 | 100 MiB | 100 | 66.167 | 66.244 | 1.607 | 65.845 | 66.488 | ms/op |
| AES-256-GCM dec | 0 | 100 MiB | 100 | 67.958 | 66.317 | 5.479 | 66.862 | 69.053 | ms/op |
| Hybrid-3072 enc | 3072 | 1 KiB | 100 | 0.076 | 0.075 | 0.004 | 0.075 | 0.077 | ms/op |
| Hybrid-3072 dec | 3072 | 1 KiB | 100 | 2.070 | 1.985 | 0.188 | 2.032 | 2.108 | ms/op |
| Hybrid-3072 enc | 3072 | 1 MiB | 100 | 0.711 | 0.706 | 0.020 | 0.707 | 0.715 | ms/op |
| Hybrid-3072 dec | 3072 | 1 MiB | 100 | 2.710 | 2.645 | 0.142 | 2.682 | 2.738 | ms/op |
| Hybrid-3072 enc | 3072 | 100 MiB | 100 | 65.843 | 65.738 | 2.593 | 65.324 | 66.361 | ms/op |
| Hybrid-3072 dec | 3072 | 100 MiB | 100 | 70.658 | 67.951 | 7.062 | 69.245 | 72.070 | ms/op |
| Hybrid-4096 enc | 4096 | 1 KiB | 100 | 0.108 | 0.105 | 0.008 | 0.107 | 0.110 | ms/op |
| Hybrid-4096 dec | 4096 | 1 KiB | 100 | 4.573 | 4.497 | 0.240 | 4.525 | 4.621 | ms/op |
| Hybrid-4096 enc | 4096 | 1 MiB | 100 | 0.807 | 0.783 | 0.061 | 0.795 | 0.819 | ms/op |
| Hybrid-4096 dec | 4096 | 1 MiB | 100 | 5.342 | 5.253 | 0.331 | 5.275 | 5.408 | ms/op |
| Hybrid-4096 enc | 4096 | 100 MiB | 100 | 66.471 | 66.684 | 2.536 | 65.964 | 66.979 | ms/op |
| Hybrid-4096 dec | 4096 | 100 MiB | 100 | 71.355 | 71.425 | 3.660 | 70.623 | 72.087 | ms/op |

### AES-GCM / Hybrid throughput (MB/s)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| AES-256-GCM enc | 0 | 1 KiB | 100 | 705.878 | 713.241 | 54.452 | 694.988 | 716.768 | MB/s |
| AES-256-GCM dec | 0 | 1 KiB | 100 | 777.520 | 769.231 | 71.166 | 763.287 | 791.753 | MB/s |
| AES-256-GCM enc | 0 | 1 MiB | 100 | 1706.376 | 1692.625 | 81.596 | 1690.056 | 1722.695 | MB/s |
| AES-256-GCM dec | 0 | 1 MiB | 100 | 1435.595 | 1472.420 | 114.697 | 1412.655 | 1458.534 | MB/s |
| AES-256-GCM enc | 0 | 100 MiB | 100 | 1585.719 | 1582.907 | 40.362 | 1577.647 | 1593.792 | MB/s |
| AES-256-GCM dec | 0 | 100 MiB | 100 | 1550.989 | 1581.155 | 101.244 | 1530.740 | 1571.238 | MB/s |
| Hybrid-3072 enc | 3072 | 1 KiB | 100 | 13.521 | 13.673 | 0.638 | 13.394 | 13.649 | MB/s |
| Hybrid-3072 dec | 3072 | 1 KiB | 100 | 0.498 | 0.516 | 0.037 | 0.491 | 0.505 | MB/s |
| Hybrid-3072 enc | 3072 | 1 MiB | 100 | 1475.844 | 1485.729 | 37.768 | 1468.290 | 1483.398 | MB/s |
| Hybrid-3072 dec | 3072 | 1 MiB | 100 | 387.910 | 396.462 | 18.717 | 384.167 | 391.653 | MB/s |
| Hybrid-3072 enc | 3072 | 100 MiB | 100 | 1594.754 | 1595.090 | 57.329 | 1583.288 | 1606.220 | MB/s |
| Hybrid-3072 dec | 3072 | 100 MiB | 100 | 1496.308 | 1543.126 | 125.211 | 1471.265 | 1521.350 | MB/s |
| Hybrid-4096 enc | 4096 | 1 KiB | 100 | 9.510 | 9.778 | 0.592 | 9.392 | 9.629 | MB/s |
| Hybrid-4096 dec | 4096 | 1 KiB | 100 | 0.224 | 0.228 | 0.011 | 0.222 | 0.227 | MB/s |
| Hybrid-4096 enc | 4096 | 1 MiB | 100 | 1305.671 | 1339.824 | 83.095 | 1289.052 | 1322.290 | MB/s |
| Hybrid-4096 dec | 4096 | 1 MiB | 100 | 196.983 | 199.608 | 11.101 | 194.763 | 199.203 | MB/s |
| Hybrid-4096 enc | 4096 | 100 MiB | 100 | 1579.649 | 1572.463 | 57.586 | 1568.132 | 1591.166 | MB/s |
| Hybrid-4096 dec | 4096 | 100 MiB | 100 | 1472.688 | 1468.072 | 63.298 | 1460.028 | 1485.347 | MB/s |

### Hybrid overhead
| bits | size | hybrid_ms | aes_gcm_ms | overhead_ms | hybrid_over_aes |
|---|---|---|---|---|---|
| 3072 | 1 KiB | 0.076 | 0.001 | 0.074 | 51.967 |
| 3072 | 1 MiB | 0.711 | 0.616 | 0.095 | 1.154 |
| 3072 | 100 MiB | 65.843 | 66.167 | -0.324 | 0.995 |
| 4096 | 1 KiB | 0.108 | 0.001 | 0.107 | 74.032 |
| 4096 | 1 MiB | 0.807 | 0.616 | 0.191 | 1.310 |
| 4096 | 100 MiB | 66.471 | 66.167 | 0.305 | 1.005 |

## Test status
- ctest passed: True
- KAT passed: True
- CLI tests: 30/30 passed
