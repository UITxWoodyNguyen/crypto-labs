// Reference implementations (OpenSSL / Crypto++) used ONLY for cross-validation and benchmark
// comparison. They are never linked into or called from the AES core.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

using namespace std;

namespace aesp {

struct RefImpl {
  std::string name;  // "openssl" | "cryptopp"
  // AES-128-CTR: out = in XOR keystream(key, iv). in == out allowed.
  std::function<void(const uint8_t* key, const uint8_t* iv, const uint8_t* in, uint8_t* out, std::size_t n)> ctr;
};

// which: all | openssl | cryptopp | none. Throws Usage error if a requested reference was not
// compiled in. "all" returns whatever is available (possibly empty).
std::vector<RefImpl> available_references(const std::string& which);

struct XvalSummary {
  std::size_t total = 0, failed = 0;
};

// Compares the pure implementation (all 3 backends) to each selected reference over fixed edge
// lengths + random lengths, random keys/IVs, and IVs that force counter carries.
XvalSummary run_xval(const std::string& which, std::size_t random_cases, std::ostream& os);

}  // namespace aesp
