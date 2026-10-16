#pragma once
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

#include "aes128.hpp"

namespace aesp {

struct BenchOptions {
    vector<size_t> sizes = {1u << 20, 100u << 20, 1u << 30};    // payload size (1MB, 100MB, 1GB)
    vector<Backend> backends = {Backend::SBoxTable, Backend::TTable};
    string ref = "all";     // all | openssl | cryptopp | none
    size_t runs = 30;
    double warmup_sec = 1.5;
    double target_sec = 0.25;
    size_t ops = 0;         // fixed ops per run (the lab suggests ~1000); 0 = auto, capped at 1000
    bool do_encrypt = true, do_decrypt = true;
    string csv_path;
};

void run_bench(const BenchOptions& opt, ostream& os);

}