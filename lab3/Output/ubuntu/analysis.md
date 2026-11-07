# Lab 3 analysis — ubuntu / gcc

Hardware: Intel(R) Core(TM) i7-8565U CPU @ 1.80GHz (8 logical CPUs, 14.8 GB RAM), Linux-7.0.0-38-generic-x86_64-with-glibc2.43

- RSA-3072: keygen 187.9 ms, encrypt 0.046 ms, decrypt 1.752 ms -> decrypt/encrypt = 37.7x (public exponent 65537 vs full-size private exponent).
- RSA-4096: keygen 504.8 ms, encrypt 0.077 ms, decrypt 4.295 ms -> decrypt/encrypt = 56.1x (public exponent 65537 vs full-size private exponent).
- RSA-4096 / RSA-3072 keygen: 2.69x slower.
- RSA-4096 / RSA-3072 OAEP enc: 1.65x slower.
- RSA-4096 / RSA-3072 OAEP dec: 2.45x slower.

- Hybrid-3072 @ 1 KiB: 0.046 ms vs AES-GCM 0.001 ms (x40.82); throughput 22.3 MB/s vs 971.2 MB/s.
- Hybrid-3072 @ 1 MiB: 0.354 ms vs AES-GCM 0.268 ms (x1.32); throughput 2961.7 MB/s vs 3917.8 MB/s.
- Hybrid-3072 @ 100 MiB: 81.470 ms vs AES-GCM 75.870 ms (x1.07); throughput 1287.7 MB/s vs 1382.3 MB/s.
- Hybrid-4096 @ 1 KiB: 0.081 ms vs AES-GCM 0.001 ms (x71.57); throughput 12.7 MB/s vs 971.2 MB/s.
- Hybrid-4096 @ 1 MiB: 0.421 ms vs AES-GCM 0.268 ms (x1.57); throughput 2490.5 MB/s vs 3917.8 MB/s.
- Hybrid-4096 @ 100 MiB: 88.547 ms vs AES-GCM 75.870 ms (x1.17); throughput 1185.4 MB/s vs 1382.3 MB/s.

## Tables

### RSA (ms/op)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| RSA-3072 keygen | 3072 | - | 100 | 187.946 | 165.182 | 128.009 | 162.345 | 213.548 | ms/op |
| RSA-3072 OAEP enc | 3072 | 32 B | 100 | 0.046 | 0.043 | 0.010 | 0.045 | 0.048 | ms/op |
| RSA-3072 OAEP dec | 3072 | 32 B | 100 | 1.752 | 1.749 | 0.022 | 1.747 | 1.756 | ms/op |
| RSA-4096 keygen | 4096 | - | 100 | 504.778 | 469.105 | 296.528 | 445.473 | 564.084 | ms/op |
| RSA-4096 OAEP enc | 4096 | 32 B | 100 | 0.077 | 0.076 | 0.001 | 0.076 | 0.077 | ms/op |
| RSA-4096 OAEP dec | 4096 | 32 B | 100 | 4.295 | 4.286 | 0.112 | 4.272 | 4.317 | ms/op |

### AES-GCM / Hybrid latency (ms/op)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| AES-256-GCM enc | 0 | 1 KiB | 100 | 0.001 | 0.001 | 0.000 | 0.001 | 0.001 | ms/op |
| AES-256-GCM dec | 0 | 1 KiB | 100 | 0.001 | 0.001 | 0.000 | 0.001 | 0.001 | ms/op |
| AES-256-GCM enc | 0 | 1 MiB | 100 | 0.268 | 0.267 | 0.005 | 0.267 | 0.269 | ms/op |
| AES-256-GCM dec | 0 | 1 MiB | 100 | 0.264 | 0.264 | 0.004 | 0.264 | 0.265 | ms/op |
| AES-256-GCM enc | 0 | 100 MiB | 100 | 75.870 | 75.959 | 0.918 | 75.686 | 76.053 | ms/op |
| AES-256-GCM dec | 0 | 100 MiB | 100 | 75.029 | 74.840 | 1.361 | 74.757 | 75.301 | ms/op |
| Hybrid-3072 enc | 3072 | 1 KiB | 100 | 0.046 | 0.046 | 0.000 | 0.046 | 0.046 | ms/op |
| Hybrid-3072 dec | 3072 | 1 KiB | 100 | 1.853 | 1.837 | 0.062 | 1.840 | 1.865 | ms/op |
| Hybrid-3072 enc | 3072 | 1 MiB | 100 | 0.354 | 0.353 | 0.003 | 0.353 | 0.355 | ms/op |
| Hybrid-3072 dec | 3072 | 1 MiB | 100 | 2.226 | 2.229 | 0.071 | 2.212 | 2.241 | ms/op |
| Hybrid-3072 enc | 3072 | 100 MiB | 100 | 81.470 | 81.025 | 1.893 | 81.092 | 81.849 | ms/op |
| Hybrid-3072 dec | 3072 | 100 MiB | 100 | 83.483 | 83.217 | 2.071 | 83.069 | 83.898 | ms/op |
| Hybrid-4096 enc | 4096 | 1 KiB | 100 | 0.081 | 0.081 | 0.001 | 0.080 | 0.081 | ms/op |
| Hybrid-4096 dec | 4096 | 1 KiB | 100 | 4.647 | 4.507 | 0.454 | 4.556 | 4.738 | ms/op |
| Hybrid-4096 enc | 4096 | 1 MiB | 100 | 0.421 | 0.421 | 0.013 | 0.419 | 0.424 | ms/op |
| Hybrid-4096 dec | 4096 | 1 MiB | 100 | 4.760 | 4.776 | 0.137 | 4.733 | 4.787 | ms/op |
| Hybrid-4096 enc | 4096 | 100 MiB | 100 | 88.547 | 89.611 | 2.787 | 87.990 | 89.104 | ms/op |
| Hybrid-4096 dec | 4096 | 100 MiB | 100 | 91.344 | 91.378 | 2.503 | 90.844 | 91.845 | ms/op |

### AES-GCM / Hybrid throughput (MB/s)
| algo | bits | size | n | mean | median | stddev | ci95_lo | ci95_hi | unit |
|---|---|---|---|---|---|---|---|---|---|
| AES-256-GCM enc | 0 | 1 KiB | 100 | 971.189 | 1044.966 | 233.909 | 924.407 | 1017.971 | MB/s |
| AES-256-GCM dec | 0 | 1 KiB | 100 | 1035.729 | 1139.456 | 209.740 | 993.781 | 1077.677 | MB/s |
| AES-256-GCM enc | 0 | 1 MiB | 100 | 3917.754 | 3920.760 | 75.567 | 3902.640 | 3932.867 | MB/s |
| AES-256-GCM dec | 0 | 1 MiB | 100 | 3967.454 | 3969.186 | 52.545 | 3956.945 | 3977.963 | MB/s |
| AES-256-GCM enc | 0 | 100 MiB | 100 | 1382.279 | 1380.446 | 16.833 | 1378.913 | 1385.646 | MB/s |
| AES-256-GCM dec | 0 | 100 MiB | 100 | 1398.010 | 1401.090 | 24.831 | 1393.044 | 1402.977 | MB/s |
| Hybrid-3072 enc | 3072 | 1 KiB | 100 | 22.280 | 22.259 | 0.203 | 22.240 | 22.321 | MB/s |
| Hybrid-3072 dec | 3072 | 1 KiB | 100 | 0.553 | 0.557 | 0.017 | 0.550 | 0.557 | MB/s |
| Hybrid-3072 enc | 3072 | 1 MiB | 100 | 2961.723 | 2968.323 | 29.176 | 2955.888 | 2967.559 | MB/s |
| Hybrid-3072 dec | 3072 | 1 MiB | 100 | 471.453 | 470.501 | 14.384 | 468.576 | 474.330 | MB/s |
| Hybrid-3072 enc | 3072 | 100 MiB | 100 | 1287.697 | 1294.136 | 27.663 | 1282.164 | 1293.230 | MB/s |
| Hybrid-3072 dec | 3072 | 100 MiB | 100 | 1256.679 | 1260.044 | 26.531 | 1251.373 | 1261.985 | MB/s |
| Hybrid-4096 enc | 4096 | 1 KiB | 100 | 12.708 | 12.679 | 0.163 | 12.676 | 12.741 | MB/s |
| Hybrid-4096 dec | 4096 | 1 KiB | 100 | 0.222 | 0.227 | 0.020 | 0.218 | 0.226 | MB/s |
| Hybrid-4096 enc | 4096 | 1 MiB | 100 | 2490.505 | 2489.882 | 68.544 | 2476.797 | 2504.214 | MB/s |
| Hybrid-4096 dec | 4096 | 1 MiB | 100 | 220.471 | 219.543 | 6.362 | 219.199 | 221.744 | MB/s |
| Hybrid-4096 enc | 4096 | 100 MiB | 100 | 1185.395 | 1170.149 | 38.292 | 1177.737 | 1193.053 | MB/s |
| Hybrid-4096 dec | 4096 | 100 MiB | 100 | 1148.761 | 1147.518 | 30.418 | 1142.677 | 1154.844 | MB/s |

### Hybrid overhead
| bits | size | hybrid_ms | aes_gcm_ms | overhead_ms | hybrid_over_aes |
|---|---|---|---|---|---|
| 3072 | 1 KiB | 0.046 | 0.001 | 0.045 | 40.817 |
| 3072 | 1 MiB | 0.354 | 0.268 | 0.086 | 1.322 |
| 3072 | 100 MiB | 81.470 | 75.870 | 5.601 | 1.074 |
| 4096 | 1 KiB | 0.081 | 0.001 | 0.079 | 71.566 |
| 4096 | 1 MiB | 0.421 | 0.268 | 0.154 | 1.574 |
| 4096 | 100 MiB | 88.547 | 75.870 | 12.677 | 1.167 |

## Test status
- ctest passed: True
- KAT passed: True
- CLI tests: 30/30 passed
