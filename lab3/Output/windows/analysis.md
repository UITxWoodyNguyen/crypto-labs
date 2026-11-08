# Lab 3 analysis — windows / msvc

Hardware: Intel(R) Core(TM) i7-8565U CPU @ 1.80GHz (8 logical CPUs, 15.8 GB RAM), Windows-10-10.0.19045-SP0

- RSA-3072: keygen 174.6 ms, encrypt 0.055 ms, decrypt 2.452 ms -> decrypt/encrypt = 44.2x (public exponent 65537 vs full-size private exponent).
- RSA-4096: keygen 784.6 ms, encrypt 0.116 ms, decrypt 5.377 ms -> decrypt/encrypt = 46.2x (public exponent 65537 vs full-size private exponent).
- RSA-4096 / RSA-3072 keygen: 4.49x slower.
- RSA-4096 / RSA-3072 OAEP enc: 2.10x slower.
- RSA-4096 / RSA-3072 OAEP dec: 2.19x slower.

- Hybrid-3072 @ 1 KiB: 0.088 ms vs AES-GCM 0.001 ms (x62.03); throughput 11.7 MB/s vs 739.0 MB/s.
- Hybrid-3072 @ 1 MiB: 1.002 ms vs AES-GCM 0.513 ms (x1.95); throughput 1047.4 MB/s vs 2047.8 MB/s.
- Hybrid-3072 @ 100 MiB: 91.290 ms vs AES-GCM 47.136 ms (x1.94); throughput 1169.4 MB/s vs 2225.5 MB/s.
- Hybrid-4096 @ 1 KiB: 0.137 ms vs AES-GCM 0.001 ms (x97.15); throughput 7.5 MB/s vs 739.0 MB/s.
- Hybrid-4096 @ 1 MiB: 1.394 ms vs AES-GCM 0.513 ms (x2.72); throughput 780.0 MB/s vs 2047.8 MB/s.
- Hybrid-4096 @ 100 MiB: 76.708 ms vs AES-GCM 47.136 ms (x1.63); throughput 1372.5 MB/s vs 2225.5 MB/s.

## Tables

### RSA (ms/op)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| RSA-3072 keygen | 3072 | - | 100 | 174.584 | 137.869 | 122.653 | 150.054 | 199.115 | ms/op |
| RSA-3072 OAEP enc | 3072 | 32 B | 100 | 0.055 | 0.055 | 0.003 | 0.055 | 0.056 | ms/op |
| RSA-3072 OAEP dec | 3072 | 32 B | 100 | 2.452 | 2.470 | 0.193 | 2.413 | 2.491 | ms/op |
| RSA-4096 keygen | 4096 | - | 100 | 784.618 | 665.171 | 516.651 | 681.288 | 887.948 | ms/op |
| RSA-4096 OAEP enc | 4096 | 32 B | 100 | 0.116 | 0.116 | 0.004 | 0.116 | 0.117 | ms/op |
| RSA-4096 OAEP dec | 4096 | 32 B | 100 | 5.377 | 5.296 | 0.257 | 5.326 | 5.428 | ms/op |

### AES-GCM / Hybrid latency (ms/op)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| AES-256-GCM enc | 0 | 1 KiB | 100 | 0.001 | 0.001 | 0.000 | 0.001 | 0.001 | ms/op |
| AES-256-GCM dec | 0 | 1 KiB | 100 | 0.001 | 0.001 | 0.000 | 0.001 | 0.001 | ms/op |
| AES-256-GCM enc | 0 | 1 MiB | 100 | 0.513 | 0.508 | 0.026 | 0.508 | 0.518 | ms/op |
| AES-256-GCM dec | 0 | 1 MiB | 100 | 0.509 | 0.510 | 0.008 | 0.507 | 0.510 | ms/op |
| AES-256-GCM enc | 0 | 100 MiB | 100 | 47.136 | 47.307 | 0.973 | 46.942 | 47.331 | ms/op |
| AES-256-GCM dec | 0 | 100 MiB | 100 | 47.508 | 47.626 | 0.589 | 47.391 | 47.626 | ms/op |
| Hybrid-3072 enc | 3072 | 1 KiB | 100 | 0.088 | 0.087 | 0.001 | 0.087 | 0.088 | ms/op |
| Hybrid-3072 dec | 3072 | 1 KiB | 100 | 2.698 | 2.723 | 0.795 | 2.539 | 2.857 | ms/op |
| Hybrid-3072 enc | 3072 | 1 MiB | 100 | 1.002 | 0.989 | 0.035 | 0.995 | 1.009 | ms/op |
| Hybrid-3072 dec | 3072 | 1 MiB | 100 | 3.972 | 3.697 | 0.932 | 3.785 | 4.158 | ms/op |
| Hybrid-3072 enc | 3072 | 100 MiB | 100 | 91.290 | 85.110 | 13.951 | 88.500 | 94.080 | ms/op |
| Hybrid-3072 dec | 3072 | 100 MiB | 100 | 85.047 | 83.207 | 5.393 | 83.969 | 86.126 | ms/op |
| Hybrid-4096 enc | 4096 | 1 KiB | 100 | 0.137 | 0.134 | 0.008 | 0.135 | 0.139 | ms/op |
| Hybrid-4096 dec | 4096 | 1 KiB | 100 | 5.455 | 5.284 | 1.438 | 5.167 | 5.742 | ms/op |
| Hybrid-4096 enc | 4096 | 1 MiB | 100 | 1.394 | 1.280 | 0.295 | 1.335 | 1.453 | ms/op |
| Hybrid-4096 dec | 4096 | 1 MiB | 100 | 7.030 | 6.925 | 0.427 | 6.945 | 7.116 | ms/op |
| Hybrid-4096 enc | 4096 | 100 MiB | 100 | 76.708 | 77.431 | 4.909 | 75.726 | 77.690 | ms/op |
| Hybrid-4096 dec | 4096 | 100 MiB | 100 | 86.388 | 85.451 | 4.101 | 85.568 | 87.208 | ms/op |

### AES-GCM / Hybrid throughput (MB/s)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| AES-256-GCM enc | 0 | 1 KiB | 100 | 738.955 | 724.161 | 101.987 | 718.558 | 759.353 | MB/s |
| AES-256-GCM dec | 0 | 1 KiB | 100 | 846.900 | 859.277 | 30.073 | 840.886 | 852.915 | MB/s |
| AES-256-GCM enc | 0 | 1 MiB | 100 | 2047.767 | 2062.984 | 85.975 | 2030.572 | 2064.962 | MB/s |
| AES-256-GCM dec | 0 | 1 MiB | 100 | 2062.220 | 2057.896 | 34.496 | 2055.321 | 2069.120 | MB/s |
| AES-256-GCM enc | 0 | 100 MiB | 100 | 2225.516 | 2216.542 | 46.795 | 2216.157 | 2234.875 | MB/s |
| AES-256-GCM dec | 0 | 100 MiB | 100 | 2207.479 | 2201.670 | 27.907 | 2201.898 | 2213.060 | MB/s |
| Hybrid-3072 enc | 3072 | 1 KiB | 100 | 11.701 | 11.753 | 0.190 | 11.663 | 11.739 | MB/s |
| Hybrid-3072 dec | 3072 | 1 KiB | 100 | 0.405 | 0.376 | 0.099 | 0.385 | 0.425 | MB/s |
| Hybrid-3072 enc | 3072 | 1 MiB | 100 | 1047.402 | 1059.820 | 33.870 | 1040.628 | 1054.175 | MB/s |
| Hybrid-3072 dec | 3072 | 1 MiB | 100 | 271.484 | 283.616 | 33.521 | 264.780 | 278.188 | MB/s |
| Hybrid-3072 enc | 3072 | 100 MiB | 100 | 1169.353 | 1232.024 | 142.275 | 1140.898 | 1197.807 | MB/s |
| Hybrid-3072 dec | 3072 | 100 MiB | 100 | 1237.507 | 1260.204 | 73.117 | 1222.884 | 1252.131 | MB/s |
| Hybrid-4096 enc | 4096 | 1 KiB | 100 | 7.491 | 7.652 | 0.386 | 7.414 | 7.568 | MB/s |
| Hybrid-4096 dec | 4096 | 1 KiB | 100 | 0.197 | 0.194 | 0.041 | 0.189 | 0.206 | MB/s |
| Hybrid-4096 enc | 4096 | 1 MiB | 100 | 780.031 | 819.079 | 135.210 | 752.989 | 807.073 | MB/s |
| Hybrid-4096 dec | 4096 | 1 MiB | 100 | 149.646 | 151.410 | 8.231 | 148.000 | 151.293 | MB/s |
| Hybrid-4096 enc | 4096 | 100 MiB | 100 | 1372.491 | 1354.210 | 87.234 | 1355.045 | 1389.938 | MB/s |
| Hybrid-4096 dec | 4096 | 100 MiB | 100 | 1216.281 | 1227.124 | 53.006 | 1205.679 | 1226.882 | MB/s |

### Hybrid overhead
| bits | size | hybrid_ms | aes_gcm_ms | overhead_ms | hybrid_over_aes |
|---|---|---|---|---|---|
| 3072 | 1 KiB | 0.088 | 0.001 | 0.086 | 62.028 |
| 3072 | 1 MiB | 1.002 | 0.513 | 0.489 | 1.953 |
| 3072 | 100 MiB | 91.290 | 47.136 | 44.154 | 1.937 |
| 4096 | 1 KiB | 0.137 | 0.001 | 0.136 | 97.154 |
| 4096 | 1 MiB | 1.394 | 0.513 | 0.881 | 2.717 |
| 4096 | 100 MiB | 76.708 | 47.136 | 29.572 | 1.627 |

## Test status
- ctest passed: True
- KAT passed: True
- CLI tests: 30/30 passed
