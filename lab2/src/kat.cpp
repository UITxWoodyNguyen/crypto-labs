#include "kat.hpp"

#include <algorithm>
#include <array>

#include "aes128.hpp"
#include "ctr.hpp"
#include "errors.hpp"
#include "json.hpp"
#include "util.hpp"

using namespace std;

namespace aesp {
namespace {

constexpr std::array<Backend, 3> kBackends = {Backend::SBoxTable, Backend::SBoxComputed, Backend::TTable};

Bytes need_hex(const Json& v, const char* field, std::size_t len_exact = 0) {
  Bytes b = from_hex(v.str(field));
  if (len_exact && b.size() != len_exact)
    throw Error(ExitCode::InvalidInput, std::string("KAT: field '") + field + "' must be " +
                                            std::to_string(len_exact) + " bytes");
  return b;
}

struct Reporter {
  KatSummary& sum;
  std::ostream& os;
  bool verbose;
  void record(const std::string& name, bool ok, const std::string& detail) {
    ++sum.total;
    ok ? ++sum.passed : ++sum.failed;
    os << (ok ? "PASS  " : "FAIL  ") << name;
    if (!detail.empty() && (!ok || verbose)) os << "  [" << detail << "]";
    os << "\n";
  }
};

void run_block_vectors(const Json& arr, Reporter& rep) {
  for (const Json& v : arr.array) {
    const std::string id = v.str("id");
    const Bytes key = need_hex(v, "key", 16), pt = need_hex(v, "plaintext", 16), ct = need_hex(v, "ciphertext", 16);
    std::string fails;
    for (Backend be : kBackends) {
      Aes128 aes(key.data(), key.size(), be);
      uint8_t out[16];
      aes.encrypt_block(pt.data(), out);
      if (!std::equal(out, out + 16, ct.begin())) fails += std::string(backend_name(be)) + ":encrypt ";
      aes.decrypt_block(ct.data(), out);
      if (!std::equal(out, out + 16, pt.begin())) fails += std::string(backend_name(be)) + ":decrypt ";
    }
    rep.record("aes128_block/" + id, fails.empty(),
               fails.empty() ? "3 backends x (encrypt+decrypt)" : "mismatch in " + fails);
  }
}

void run_ctr_vectors(const Json& arr, Reporter& rep) {
  static const std::size_t kChunks[] = {1, 3, 15, 16, 17, 64};
  for (const Json& v : arr.array) {
    const std::string id = v.str("id");
    const Bytes key = need_hex(v, "key", 16), iv = need_hex(v, "iv", 16);
    const Bytes pt = need_hex(v, "plaintext"), ct = need_hex(v, "ciphertext");
    if (pt.size() != ct.size()) throw Error(ExitCode::InvalidInput, "KAT: plaintext/ciphertext length mismatch in '" + id + "'");
    std::string fails;
    for (Backend be : kBackends) {
      Aes128 aes(key.data(), key.size(), be);
      if (ctr_crypt(aes, iv.data(), pt) != ct) fails += std::string(backend_name(be)) + ":encrypt ";
      if (ctr_crypt(aes, iv.data(), ct) != pt) fails += std::string(backend_name(be)) + ":decrypt ";
      for (std::size_t chunk : kChunks) {  // streaming state must survive arbitrary chunking
        CtrStream s(aes, iv.data());
        Bytes out(pt.size());
        for (std::size_t off = 0; off < pt.size(); off += chunk)
          s.process(pt.data() + off, out.data() + off, std::min(chunk, pt.size() - off));
        if (out != ct) fails += std::string(backend_name(be)) + ":chunk" + std::to_string(chunk) + " ";
      }
    }
    rep.record("aes128_ctr/" + id, fails.empty(),
               fails.empty() ? std::to_string(pt.size()) + " bytes, 3 backends, enc+dec+chunked" : "mismatch in " + fails);
  }
}

void run_error_vectors(const Json& arr, Reporter& rep) {
  for (const Json& v : arr.array) {
    const std::string id = v.str("id");
    const Bytes key = need_hex(v, "key", 16), iv = need_hex(v, "iv", 16);
    const std::size_t len = v.size_value("length");
    if (v.str("expect") != "counter_overflow")
      throw Error(ExitCode::InvalidInput, "KAT: unsupported 'expect' value in '" + id + "'");
    Aes128 aes(key.data(), key.size());
    Bytes buf(len, 0);
    bool got = false;
    try {
      CtrStream s(aes, iv.data());
      s.process(buf.data(), buf.data(), buf.size());
    } catch (const Error& e) {
      got = e.code() == ExitCode::CounterOverflow;
    }
    rep.record("aes128_ctr_errors/" + id, got, got ? "counter overflow rejected (fail closed)" : "overflow NOT detected");
  }
}

}  // namespace

KatSummary run_kat_file(const std::string& path, bool verbose, std::ostream& os) {
  const Bytes raw = read_file(path, 16u << 20);
  const Json root = parse_json(std::string(raw.begin(), raw.end()));
  if (root.type != Json::Type::Object) throw Error(ExitCode::InvalidInput, "KAT file must be a JSON object");

  KatSummary sum;
  Reporter rep{sum, os, verbose};
  auto section = [&](const char* name, void (*fn)(const Json&, Reporter&)) {
    if (const Json* j = root.find(name)) {
      if (j->type != Json::Type::Array) throw Error(ExitCode::InvalidInput, std::string("KAT: '") + name + "' must be an array");
      fn(*j, rep);
    }
  };
  section("aes128_block", run_block_vectors);
  section("aes128_ctr", run_ctr_vectors);
  section("aes128_ctr_errors", run_error_vectors);

  os << "\nSummary: " << sum.total << " tests, " << sum.passed << " passed, " << sum.failed << " failed\n";
  if (sum.total == 0) os << "FAIL: no test vectors found\n";
  return sum;
}

}  // namespace aesp
