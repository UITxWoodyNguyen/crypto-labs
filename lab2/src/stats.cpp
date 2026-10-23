#include "stats.hpp"

#include <algorithm>
#include <cmath>

using namespace std;

namespace aesp {

double t_critical_95(std::size_t df) {
  static const double kT[] = {0,     12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365, 2.306, 2.262, 2.228,
                              2.201, 2.179,  2.160, 2.145, 2.131, 2.120, 2.110, 2.101, 2.093, 2.086, 2.080,
                              2.074, 2.069,  2.064, 2.060, 2.056, 2.052, 2.048, 2.045, 2.042};
  if (df == 0) return 0;
  if (df <= 30) return kT[df];
  return 1.96 + 2.4 / static_cast<double>(df);  // accurate to ~0.001 for df > 30
}

Stats compute_stats(std::vector<double> v) {
  Stats s;
  s.n = v.size();
  if (v.empty()) return s;
  std::sort(v.begin(), v.end());
  s.min = v.front();
  s.max = v.back();
  double sum = 0;
  for (double x : v) sum += x;
  s.mean = sum / static_cast<double>(s.n);
  s.median = (s.n % 2) ? v[s.n / 2] : 0.5 * (v[s.n / 2 - 1] + v[s.n / 2]);
  if (s.n > 1) {
    double ss = 0;
    for (double x : v) ss += (x - s.mean) * (x - s.mean);
    s.stddev = std::sqrt(ss / static_cast<double>(s.n - 1));
    const double half = t_critical_95(s.n - 1) * s.stddev / std::sqrt(static_cast<double>(s.n));
    s.ci95_lo = s.mean - half;
    s.ci95_hi = s.mean + half;
  } else {
    s.ci95_lo = s.ci95_hi = s.mean;
  }
  return s;
}

}  // namespace aesp
