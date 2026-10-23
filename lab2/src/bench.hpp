// Benchmark harness following the lab's performance protocol:
//   warm-up (1-2 s) -> N >= 30 independent timed runs, each of `ops` operations
//   -> mean / median / stddev / 95% CI of throughput (MB/s, 1 MB = 10^6 B) and latency (ms/op).
// Synthetic payloads use a fixed-seed xorshift PRNG (allowed: not key/nonce material);
// keys and IVs always come from the OS CSPRNG.
#pragma once
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

#include "aes128.hpp"

using namespace std;

namespace aesp {

struct BenchOptions {
  std::vector<std::size_t> sizes = {1u << 20, 100u << 20};  // payload sizes in bytes
  std::vector<Backend> backends = {Backend::SBoxTable, Backend::TTable};
  std::string ref = "all";     // all | openssl | cryptopp | none
  std::size_t runs = 30;       // independent timed runs per case (>= 30 recommended)
  double warmup_sec = 1.5;     // 1-2 s per case
  double target_sec = 0.25;    // auto ops/run so one run lasts about this long (ignored if ops > 0)
  std::size_t ops = 0;         // fixed ops per run (the lab suggests ~1000); 0 = auto, capped at 1000
  bool do_encrypt = true, do_decrypt = true;
  std::string csv_path;        // optional CSV output
};

void run_bench(const BenchOptions& opt, std::ostream& os);

}  // namespace aesp
