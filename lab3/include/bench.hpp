/*
    File: bench.hpp
    Usage: Measure performance of RSA-OAEP/AES-GCM/Hybrid according to standard statistical protocol
*/

#pragma once
#include <string>
#include <vector>

using namespace std;

namespace rsalab {

struct Stats {
    size_t n = 0;
    double mean = 0, median = 0, stddev = 0, ci_lo = 0, ci_hi = 0, min = 0, max = 0;
};

// Mean, median, sample standard deviation (n-1), 95% CI using t-Student distribution
Stats compute_stats(vector<double> samples);
double t_critical_95(size_t df);

struct BenchOptions {
    vector<int> bits{3072, 4096};
    vector<size_t> sizes{1024, 1048576, 104857600};  // 1 KiB, 1 MiB, 100 MiB
    int runs = 30;          // N >= 30 independent runs per case
    int keygen_runs = 30;   // number of key generation runs measured
    int rsa_block = 100;    // number of RSA operations per run (RSA-4096 decrypt is quite slow)
    double warmup_sec = 1.0;

    string what = "all";  // rsa | hybrid | all
    string csv_path;      // empty = do not write CSV
};

int run_bench(const BenchOptions& opt);

}   // end namespace rsalab