#include "bench.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <thread>

#include "ctr.hpp"
#include "errors.hpp"
#include "stats.hpp"
#include "util.hpp"
#include "xval.hpp"

using namespace std;

#ifndef AESP_BUILD_TYPE
#define AESP_BUILD_TYPE "unknown"
#endif
#ifndef AESP_BUILD_FLAGS
#define AESP_BUILD_FLAGS ""
#endif

namespace aesp {
namespace {

using Clock = std::chrono::steady_clock;

std::string read_first_match(const char* path, const char* key) {
  std::ifstream f(path);
  std::string line;
  const std::string k(key);
  while (std::getline(f, line))
    if (line.compare(0, k.size(), k) == 0) {
      const auto p = line.find(':');
      if (p != std::string::npos) {
        std::string v = line.substr(p + 1);
        v.erase(0, v.find_first_not_of(" \t"));
        return v;
      }
    }
  return "";
}

void print_hardware(std::ostream& os) {
  std::string cpu, ram;
#if defined(__linux__)
  cpu = read_first_match("/proc/cpuinfo", "model name");
  ram = read_first_match("/proc/meminfo", "MemTotal");
#endif
  if (cpu.empty()) {
    if (const char* e = std::getenv("PROCESSOR_IDENTIFIER")) cpu = e;
  }
  if (cpu.empty()) cpu = "unknown (fill in manually)";
  if (ram.empty()) ram = "unknown (fill in manually)";
#if defined(_WIN32)
  const char* os_name = "Windows";
#elif defined(__APPLE__)
  const char* os_name = "macOS";
#elif defined(__linux__)
  const char* os_name = "Linux";
#else
  const char* os_name = "unknown";
#endif
  std::string compiler;
#if defined(_MSC_VER)
  compiler = "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
  compiler = std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
  compiler = std::string("GCC ") + __VERSION__;
#endif
  os << "# Hardware / build\n"
     << "#   CPU       : " << cpu << "\n"
     << "#   Threads   : " << std::thread::hardware_concurrency() << " logical\n"
     << "#   RAM       : " << ram << "\n"
     << "#   OS        : " << os_name << "\n"
     << "#   Compiler  : " << compiler << "\n"
     << "#   Build     : " << AESP_BUILD_TYPE << "  flags: " << AESP_BUILD_FLAGS << "\n"
     << "#   Reminder  : set CPU governor to 'performance' (Linux) / 'High performance' (Windows) for fair runs\n"
     << "#   MB/s      : 1 MB = 10^6 bytes; single thread; in-place CTR on one buffer\n";
}

using Op = std::function<void(uint8_t*, std::size_t)>;

struct Row {
  std::string impl, dir;
  std::size_t size = 0, ops = 0;
  Stats mbps, ms;
};

double seconds_since(Clock::time_point t0) {
  return std::chrono::duration<double>(Clock::now() - t0).count();
}

volatile uint8_t g_sink = 0;  // keeps the optimiser from discarding the work

Row measure(const std::string& impl, const std::string& dir, const Op& op, uint8_t* buf, std::size_t size,
            const BenchOptions& opt) {
  // Warm-up: stabilises caches, frequency scaling and allocator state; also estimates throughput.
  std::size_t wops = 0;
  const auto w0 = Clock::now();
  do {
    op(buf, size);
    ++wops;
  } while (seconds_since(w0) < opt.warmup_sec);
  const double wsec = seconds_since(w0);
  const double est_ops_per_sec = static_cast<double>(wops) / wsec;

  std::size_t ops = opt.ops;
  if (ops == 0) {
    const double want = est_ops_per_sec * opt.target_sec;
    ops = static_cast<std::size_t>(std::min(1000.0, std::max(1.0, std::floor(want))));
  }

  std::vector<double> mbps, ms;
  for (std::size_t r = 0; r < opt.runs; ++r) {
    const auto t0 = Clock::now();
    for (std::size_t i = 0; i < ops; ++i) op(buf, size);
    const double sec = seconds_since(t0);
    g_sink = static_cast<uint8_t>(g_sink ^ buf[0]);
    const double per_op = sec / static_cast<double>(ops);
    ms.push_back(per_op * 1e3);
    mbps.push_back(static_cast<double>(size) / per_op / 1e6);
  }
  Row row;
  row.impl = impl;
  row.dir = dir;
  row.size = size;
  row.ops = ops;
  row.mbps = compute_stats(mbps);
  row.ms = compute_stats(ms);
  return row;
}

std::string fmt_size(std::size_t n) {
  std::ostringstream s;
  if (n >= (1u << 30) && n % (1u << 30) == 0) s << n / (1u << 30) << " GiB";
  else if (n >= (1u << 20) && n % (1u << 20) == 0) s << n / (1u << 20) << " MiB";
  else if (n >= 1024 && n % 1024 == 0) s << n / 1024 << " KiB";
  else s << n << " B";
  return s.str();
}

}  // namespace

void run_bench(const BenchOptions& opt, std::ostream& os) {
  if (opt.runs < 2) throw Error(ExitCode::Usage, "--runs must be >= 2 (>= 30 recommended)");
  if (opt.sizes.empty() || opt.sizes.size() > 16) throw Error(ExitCode::Usage, "invalid --sizes list");
  if (!opt.do_encrypt && !opt.do_decrypt) throw Error(ExitCode::Usage, "nothing to benchmark (--dir)");
  const std::vector<RefImpl> refs = available_references(opt.ref);

  print_hardware(os);
  os << "# Protocol  : warm-up " << opt.warmup_sec << " s, " << opt.runs << " runs/case, ops/run "
     << (opt.ops ? std::to_string(opt.ops) : "auto(~" + std::to_string(opt.target_sec) + " s)") << ", 95% CI = Student t\n";
  os << "# Note      : CTR decryption is the same operation as encryption; both are timed separately.\n\n";

  os << std::left << std::setw(20) << "implementation" << std::setw(9) << "dir" << std::setw(10) << "size"
     << std::right << std::setw(8) << "ops/run" << std::setw(11) << "mean MB/s" << std::setw(10) << "+/-CI95"
     << std::setw(11) << "median" << std::setw(9) << "stddev" << std::setw(13) << "ms/op(mean)" << "\n";

  std::vector<Row> rows;
  auto emit = [&](const Row& r) {
    os << std::left << std::setw(20) << r.impl << std::setw(9) << r.dir << std::setw(10) << fmt_size(r.size) << std::right
       << std::setw(8) << r.ops << std::fixed << std::setprecision(1) << std::setw(11) << r.mbps.mean << std::setw(10)
       << (r.mbps.ci95_hi - r.mbps.mean) << std::setw(11) << r.mbps.median << std::setw(9) << r.mbps.stddev
       << std::setprecision(4) << std::setw(13) << r.ms.mean << "\n";
    os.flush();
    rows.push_back(r);
  };

  for (std::size_t size : opt.sizes) {
    // Synthetic payload: fixed-seed xorshift64 (NOT used for keys/IVs).
    Bytes buf(size);
    uint64_t x = 0x9E3779B97F4A7C15ull;
    for (std::size_t i = 0; i < size; ++i) {
      x ^= x << 13; x ^= x >> 7; x ^= x << 17;
      buf[i] = static_cast<uint8_t>(x);
    }
    for (const char* dir : {"encrypt", "decrypt"}) {
      if ((dir[0] == 'e' && !opt.do_encrypt) || (dir[0] == 'd' && !opt.do_decrypt)) continue;
      for (Backend be : opt.backends) {
        const Bytes key = random_bytes(16), iv = random_bytes(16);
        auto aes = std::make_shared<Aes128>(key.data(), key.size(), be);
        Bytes ivc = iv;
        Op op = [aes, ivc](uint8_t* b, std::size_t n) {
          CtrStream s(*aes, ivc.data());
          s.process(b, b, n);
        };
        emit(measure(std::string("pure/") + backend_name(be), dir, op, buf.data(), size, opt));
      }
      for (const RefImpl& ref : refs) {
        const Bytes key = random_bytes(16), iv = random_bytes(16);
        auto fn = ref.ctr;
        Op op = [fn, key, iv](uint8_t* b, std::size_t n) { fn(key.data(), iv.data(), b, b, n); };
        emit(measure(ref.name, dir, op, buf.data(), size, opt));
      }
    }
  }

  // Ratio summary vs the first reference (if any): how far the pure code is from a tuned library.
  if (!refs.empty()) {
    os << "\n# Pure / " << refs.front().name << " throughput ratio (encrypt, mean MB/s)\n";
    for (const Row& r : rows) {
      if (r.dir != "encrypt" || r.impl.rfind("pure/", 0) != 0) continue;
      for (const Row& q : rows)
        if (q.dir == "encrypt" && q.size == r.size && q.impl == refs.front().name)
          os << "#   " << std::left << std::setw(20) << r.impl << std::setw(10) << fmt_size(r.size) << std::fixed
             << std::setprecision(3) << r.mbps.mean / q.mbps.mean << "x\n";
    }
  }

  if (!opt.csv_path.empty()) {
    std::ofstream f(to_path(opt.csv_path), std::ios::trunc);
    if (!f) throw Error(ExitCode::Io, "cannot create CSV file '" + opt.csv_path + "'");
    f << "implementation,direction,size_bytes,ops_per_run,runs,mean_MBps,median_MBps,stddev_MBps,ci95_lo_MBps,"
         "ci95_hi_MBps,mean_ms_per_op,median_ms_per_op,stddev_ms_per_op,ci95_lo_ms,ci95_hi_ms\n";
    f << std::setprecision(10);
    for (const Row& r : rows)
      f << r.impl << ',' << r.dir << ',' << r.size << ',' << r.ops << ',' << r.mbps.n << ',' << r.mbps.mean << ','
        << r.mbps.median << ',' << r.mbps.stddev << ',' << r.mbps.ci95_lo << ',' << r.mbps.ci95_hi << ',' << r.ms.mean << ','
        << r.ms.median << ',' << r.ms.stddev << ',' << r.ms.ci95_lo << ',' << r.ms.ci95_hi << '\n';
    if (!f) throw Error(ExitCode::Io, "write error on CSV file");
    os << "\n# CSV written to " << opt.csv_path << "\n";
  }
}

}  // namespace aesp
