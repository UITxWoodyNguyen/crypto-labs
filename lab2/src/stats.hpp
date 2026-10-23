// Descriptive statistics for the benchmark protocol: mean, median, stddev, 95% CI (Student t).
#pragma once
#include <cstddef>
#include <vector>

using namespace std;

namespace aesp {

struct Stats {
  std::size_t n = 0;
  double mean = 0, median = 0, stddev = 0;  // stddev = sample standard deviation (n-1)
  double ci95_lo = 0, ci95_hi = 0;          // mean +/- t_{0.975,n-1} * s / sqrt(n)
  double min = 0, max = 0;
};

double t_critical_95(std::size_t df);  // two-sided 95% Student t critical value
Stats compute_stats(std::vector<double> samples);

}  // namespace aesp
