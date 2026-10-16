#pragma once
#include <cstddef>
#include <vector>

using namespace std;

namespace aesp {
    struct Stats {
        size_t n = 0;
        double mean = 0, median = 0, stddev = 0;
        double ci95_lo = 0, ci95_hi = 0;
        double min = 0, max = 0;
    };

    double t_critical_95(size_t df);
    Stats compute_stats(vector<double> samples);
}