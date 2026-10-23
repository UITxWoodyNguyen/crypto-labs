// Known Answer Test runner: `aestool --kat vectors.json`.
//
// JSON schema (every section optional, at least one test must exist or the run FAILS):
// {
//   "aes128_block":      [ {"id","source","key","plaintext","ciphertext"} ],   // FIPS-197 / AESAVS
//   "aes128_ctr":        [ {"id","source","key","iv","plaintext","ciphertext"} ],  // SP 800-38A
//   "aes128_ctr_errors": [ {"id","key","iv","length","expect":"counter_overflow"} ]
// }
// All hex strings must be well-formed; a malformed file/vector aborts with Error(InvalidInput).
#pragma once
#include <cstddef>
#include <ostream>
#include <string>

using namespace std;

namespace aesp {

struct KatSummary {
  std::size_t total = 0, passed = 0, failed = 0;
  bool ok() const { return total > 0 && failed == 0; }
};

KatSummary run_kat_file(const std::string& path, bool verbose, std::ostream& os);

}  // namespace aesp
