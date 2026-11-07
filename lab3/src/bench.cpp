#include "include/bench.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include <openssl/opensslv.h>

#include "include/hybrid_engine.hpp"
#include "include/key_manager.hpp"
#include "include/json.hpp"
#include "include/rsa_oaep.hpp"
#include "include/utils.hpp"

namespace rsalab {

// ---------------- Thống kê ----------------

double t_critical_95(std::size_t df) {
    static const double table[30] = {12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365, 2.306, 2.262, 2.228,
                                     2.201,  2.179, 2.160, 2.145, 2.131, 2.120, 2.110, 2.101, 2.093, 2.086,
                                     2.080,  2.074, 2.069, 2.064, 2.060, 2.056, 2.052, 2.048, 2.045, 2.042};
    if (df == 0) return 0.0;
    if (df <= 30) return table[df - 1];
    if (df < 40) return 2.042;  // lấy giá trị bảo thủ (cận dưới df)
    if (df < 60) return 2.021;
    if (df < 120) return 2.000;
    return 1.980;
}

Stats compute_stats(std::vector<double> v) {
    Stats s;
    s.n = v.size();
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.min = v.front();
    s.max = v.back();
    s.median = (v.size() % 2) ? v[v.size() / 2] : 0.5 * (v[v.size() / 2 - 1] + v[v.size() / 2]);
    double sum = 0;
    for (double x : v) sum += x;
    s.mean = sum / static_cast<double>(v.size());
    if (v.size() > 1) {
        double acc = 0;
        for (double x : v) acc += (x - s.mean) * (x - s.mean);
        s.stddev = std::sqrt(acc / static_cast<double>(v.size() - 1));
        const double half = t_critical_95(v.size() - 1) * s.stddev / std::sqrt(static_cast<double>(v.size()));
        s.ci_lo = s.mean - half;
        s.ci_hi = s.mean + half;
    } else {
        s.ci_lo = s.ci_hi = s.mean;
    }
    return s;
}

// ---------------- Tiện ích đo ----------------

namespace {

using Clock = std::chrono::steady_clock;

struct Row {
    std::string category, algo, metric, unit;
    int bits = 0;
    std::size_t size = 0;
    int block = 1;
    Stats st;
};

// Dữ liệu tổng hợp với seed cố định (chỉ cho payload, không dùng cho khoá/nonce).
Bytes synthetic_data(std::size_t n) {
    Bytes b(n);
    std::uint64_t x = 0x9E3779B97F4A7C15ull;
    for (std::size_t i = 0; i < n; ++i) {
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        b[i] = static_cast<std::uint8_t>(x & 0xFF);
    }
    return b;
}

// Trả về thời gian mỗi thao tác (ms) cho từng lượt. Mỗi lượt chạy `block` thao tác liên tiếp.
template <class F>
std::vector<double> sample_ms(F&& op, double warmup_sec, int runs, int block) {
    if (warmup_sec > 0) {
        const auto t0 = Clock::now();
        do { op(); } while (std::chrono::duration<double>(Clock::now() - t0).count() < warmup_sec);
    }
    std::vector<double> out;
    out.reserve(static_cast<std::size_t>(runs));
    for (int r = 0; r < runs; ++r) {
        const auto s = Clock::now();
        for (int b = 0; b < block; ++b) op();
        const auto e = Clock::now();
        out.push_back(std::chrono::duration<double, std::milli>(e - s).count() / block);
    }
    return out;
}

std::vector<double> to_throughput_mbps(const std::vector<double>& ms, std::size_t bytes) {
    std::vector<double> out;
    out.reserve(ms.size());
    for (double m : ms) out.push_back((static_cast<double>(bytes) / 1e6) / (m / 1000.0));
    return out;
}

void add_latency(std::vector<Row>& rows, const std::string& cat, const std::string& algo, int bits,
                 std::size_t size, int block, const std::vector<double>& ms) {
    rows.push_back({cat, algo, "latency", "ms/op", bits, size, block, compute_stats(ms)});
}

void add_throughput(std::vector<Row>& rows, const std::string& cat, const std::string& algo, int bits,
                    std::size_t size, int block, const std::vector<double>& ms) {
    rows.push_back({cat, algo, "throughput", "MB/s", bits, size, block, compute_stats(to_throughput_mbps(ms, size))});
}

std::string size_label(std::size_t n) {
    if (n == 0) return "-";
    if (n % (1u << 20) == 0) return std::to_string(n >> 20) + " MiB";
    if (n % 1024 == 0) return std::to_string(n >> 10) + " KiB";
    return std::to_string(n) + " B";
}

void print_row(const Row& r) {
    std::ostringstream os;
    os << std::left << std::setw(8) << r.category << std::setw(20) << r.algo << std::right << std::setw(6)
       << (r.bits ? std::to_string(r.bits) : "-") << std::setw(10) << size_label(r.size) << std::setw(12)
       << r.metric << std::fixed << std::setprecision(3) << std::setw(12) << r.st.mean << std::setw(12)
       << r.st.median << std::setw(12) << r.st.stddev << "  [" << r.st.ci_lo << ", " << r.st.ci_hi << "] " << r.unit;
    std::cout << os.str() << "\n";
}

void print_system_info(const BenchOptions& opt) {
    std::cout << "== System ==\n";
#if defined(_WIN32)
    std::cout << "OS        : Windows\n";
#elif defined(__linux__)
    std::cout << "OS        : Linux\n";
#elif defined(__APPLE__)
    std::cout << "OS        : macOS\n";
#else
    std::cout << "OS        : unknown\n";
#endif
#if defined(_MSC_VER)
    std::cout << "Compiler  : MSVC " << _MSC_VER << "\n";
#elif defined(__clang__)
    std::cout << "Compiler  : Clang " << __clang_version__ << "\n";
#elif defined(__GNUC__)
    std::cout << "Compiler  : GCC " << __VERSION__ << "\n";
#endif
    std::cout << "OpenSSL   : " << OpenSSL_version(OPENSSL_VERSION) << "\n";
    std::cout << "HW threads: " << std::thread::hardware_concurrency()
              << "  (ghi thêm CPU model/RAM/governor vào báo cáo)\n";
    std::cout << "Protocol  : warm-up " << opt.warmup_sec << "s, N=" << opt.runs << " runs/case, RSA block="
              << opt.rsa_block << " ops, hybrid block adaptive (<=1000 ops, ~128 MiB/run)\n"
              << "Throughput: 1 MB = 10^6 bytes. CI 95% = Student t.\n\n";
}

void write_csv(const std::string& path, const std::vector<Row>& rows) {
    std::ostringstream os;
    os << "category,algo,bits,size_bytes,metric,unit,block,n,mean,median,stddev,ci95_lo,ci95_hi,min,max\n";
    os << std::setprecision(10);
    for (const Row& r : rows)
        os << r.category << ',' << r.algo << ',' << r.bits << ',' << r.size << ',' << r.metric << ',' << r.unit
           << ',' << r.block << ',' << r.st.n << ',' << r.st.mean << ',' << r.st.median << ',' << r.st.stddev << ','
           << r.st.ci_lo << ',' << r.st.ci_hi << ',' << r.st.min << ',' << r.st.max << '\n';
    write_file(path, os.str());
}

int hybrid_block_for(std::size_t size) {
    const std::size_t target = std::size_t(128) << 20;
    return static_cast<int>(std::clamp<std::size_t>(target / std::max<std::size_t>(size, 1), 1, 1000));
}

}  // namespace

// ---------------- Chạy benchmark ----------------

int run_bench(const BenchOptions& opt) {
    if (opt.runs < 2) throw UsageError("--runs must be >= 2");
    if (opt.what != "rsa" && opt.what != "hybrid" && opt.what != "all") throw UsageError("--what must be rsa|hybrid|all");
    for (int b : opt.bits)
        if (b != 3072 && b != 4096) throw UsageError("--bits must be 3072, 4096 or both");

    print_system_info(opt);
    std::vector<Row> rows;
    const bool do_rsa = (opt.what == "rsa" || opt.what == "all");
    const bool do_hybrid = (opt.what == "hybrid" || opt.what == "all");

    std::cout << std::left << std::setw(8) << "cat" << std::setw(20) << "algo" << std::right << std::setw(6) << "bits"
              << std::setw(10) << "size" << std::setw(12) << "metric" << std::setw(12) << "mean" << std::setw(12)
              << "median" << std::setw(12) << "stddev" << "  95% CI\n";

    // ---- AES-256-GCM thuần (không phụ thuộc kích thước khoá RSA) ----
    if (do_hybrid) {
        const Bytes key = random_bytes(kAesKeyLen);
        const Bytes iv = random_bytes(kGcmIvLen);
        for (std::size_t size : opt.sizes) {
            const Bytes pt = synthetic_data(size);
            Bytes tag;
            const Bytes ct = aes256gcm_encrypt(key, iv, {}, pt, tag);
            const int block = hybrid_block_for(size);

            auto enc = sample_ms([&] { Bytes t; volatile auto sink = aes256gcm_encrypt(key, iv, {}, pt, t).size(); (void)sink; },
                                 opt.warmup_sec, opt.runs, block);
            auto dec = sample_ms([&] { Bytes o; if (!aes256gcm_decrypt(key, iv, {}, ct, tag, o)) throw CryptoError("bench: GCM self-check failed"); },
                                 opt.warmup_sec, opt.runs, block);
            add_latency(rows, "aes", "AES-256-GCM enc", 0, size, block, enc);
            print_row(rows.back());
            add_throughput(rows, "aes", "AES-256-GCM enc", 0, size, block, enc);
            print_row(rows.back());
            add_latency(rows, "aes", "AES-256-GCM dec", 0, size, block, dec);
            print_row(rows.back());
            add_throughput(rows, "aes", "AES-256-GCM dec", 0, size, block, dec);
            print_row(rows.back());
        }
    }

    for (int bits : opt.bits) {
        const std::string tag = "RSA-" + std::to_string(bits);

        if (do_rsa) {
            // Sinh khoá: không warm-up, mỗi lượt = 1 lần sinh khoá
            auto kg = sample_ms([&] { volatile auto p = generate_rsa_key(bits).get(); (void)p; }, 0.0, opt.keygen_runs, 1);
            add_latency(rows, "rsa", tag + " keygen", bits, 0, 1, kg);
            print_row(rows.back());
        }

        PKeyPtr key = generate_rsa_key(bits);  // khoá dùng cho các phép đo (ngoài vùng đo thời gian)

        if (do_rsa) {
            const Bytes msg = random_bytes(32);  // ví dụ: một khoá AES-256 được bọc
            const Bytes ct = rsa_oaep_encrypt(key.get(), msg);
            auto enc = sample_ms([&] { volatile auto n = rsa_oaep_encrypt(key.get(), msg).size(); (void)n; },
                                 opt.warmup_sec, opt.runs, opt.rsa_block);
            auto dec = sample_ms([&] { volatile auto n = rsa_oaep_decrypt(key.get(), ct).size(); (void)n; },
                                 opt.warmup_sec, opt.runs, opt.rsa_block);
            add_latency(rows, "rsa", tag + " OAEP enc", bits, msg.size(), opt.rsa_block, enc);
            print_row(rows.back());
            add_latency(rows, "rsa", tag + " OAEP dec", bits, msg.size(), opt.rsa_block, dec);
            print_row(rows.back());
        }

        if (do_hybrid) {
            for (std::size_t size : opt.sizes) {
                const Bytes pt = synthetic_data(size);
                const Envelope env = hybrid_encrypt(key.get(), pt);
                const int block = hybrid_block_for(size);

                auto enc = sample_ms([&] { volatile auto n = hybrid_encrypt(key.get(), pt).ciphertext.size(); (void)n; },
                                     opt.warmup_sec, opt.runs, block);
                auto dec = sample_ms([&] { volatile auto n = hybrid_decrypt(key.get(), env).size(); (void)n; },
                                     opt.warmup_sec, opt.runs, block);
                const std::string name = "Hybrid-" + std::to_string(bits);
                add_latency(rows, "hybrid", name + " enc", bits, size, block, enc);
                print_row(rows.back());
                add_throughput(rows, "hybrid", name + " enc", bits, size, block, enc);
                print_row(rows.back());
                add_latency(rows, "hybrid", name + " dec", bits, size, block, dec);
                print_row(rows.back());
                add_throughput(rows, "hybrid", name + " dec", bits, size, block, dec);
                print_row(rows.back());
            }
        }
    }

    if (!opt.csv_path.empty()) {
        write_csv(opt.csv_path, rows);
        std::cout << "\nCSV written to " << opt.csv_path << "\n";
    }
    return 0;
}

}  // namespace rsalab
