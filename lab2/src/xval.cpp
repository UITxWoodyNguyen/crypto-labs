#include "xval.hpp"

#include <algorithm>
#include <memory>

#include "aes128.hpp"
#include "ctr.hpp"
#include "errors.hpp"
#include "util.hpp"

#ifdef AESP_HAVE_OPENSSL
#include <openssl/evp.h>
#endif
#ifdef AESP_HAVE_CRYPTOPP
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#endif

using namespace std;

namespace aesp {

#ifdef AESP_HAVE_OPENSSL
static void openssl_ctr(const uint8_t* key, const uint8_t* iv, const uint8_t* in, uint8_t* out, std::size_t n) {
  std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (!ctx || EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_ctr(), nullptr, key, iv) != 1)
    throw Error(ExitCode::Failure, "OpenSSL reference init failed");
  std::size_t off = 0;
  while (off < n) {
    const int chunk = static_cast<int>(std::min<std::size_t>(n - off, 1u << 30));
    int outl = 0;
    if (EVP_EncryptUpdate(ctx.get(), out + off, &outl, in + off, chunk) != 1 || outl != chunk)
      throw Error(ExitCode::Failure, "OpenSSL reference update failed");
    off += static_cast<std::size_t>(chunk);
  }
}
#endif

#ifdef AESP_HAVE_CRYPTOPP
static void cryptopp_ctr(const uint8_t* key, const uint8_t* iv, const uint8_t* in, uint8_t* out, std::size_t n) {
  CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption enc(key, 16, iv);
  if (n) enc.ProcessData(out, in, n);
}
#endif

std::vector<RefImpl> available_references(const std::string& which) {
  if (which != "all" && which != "openssl" && which != "cryptopp" && which != "none")
    throw Error(ExitCode::Usage, "unknown reference '" + which + "' (use all|openssl|cryptopp|none)");
  std::vector<RefImpl> refs;
  const bool all = which == "all";
#ifdef AESP_HAVE_OPENSSL
  if (all || which == "openssl") refs.push_back({"openssl", openssl_ctr});
#endif
#ifdef AESP_HAVE_CRYPTOPP
  if (all || which == "cryptopp") refs.push_back({"cryptopp", cryptopp_ctr});
#endif
  if (!all && which != "none" && refs.empty())
    throw Error(ExitCode::Usage, "reference '" + which + "' was not compiled into this build");
  return refs;
}

XvalSummary run_xval(const std::string& which, std::size_t random_cases, std::ostream& os) {
  const std::vector<RefImpl> refs = available_references(which);
  if (refs.empty()) throw Error(ExitCode::Usage, "no reference implementation available in this build (need OpenSSL or Crypto++)");

  static const std::size_t kLens[] = {0, 1, 15, 16, 17, 31, 32, 33, 63, 64, 65, 255, 256, 1000, 4096, 65537};
  std::vector<std::size_t> lens(std::begin(kLens), std::end(kLens));
  for (std::size_t i = 0; i < random_cases; ++i) {
    uint8_t r[4];
    secure_random(r, 4);
    lens.push_back((static_cast<std::size_t>(r[0]) | (static_cast<std::size_t>(r[1]) << 8) | (static_cast<std::size_t>(r[2]) << 16)) % 200000);
  }

  XvalSummary sum;
  for (const RefImpl& ref : refs) {
    std::size_t pass = 0, fail = 0;
    for (std::size_t idx = 0; idx < lens.size(); ++idx) {
      const std::size_t len = lens[idx];
      Bytes key = random_bytes(16), iv = random_bytes(16), data = random_bytes(len);
      if (idx % 3 == 1) {  // force carries: low 8 bytes near 2^64 boundary, carry runs into upper half
        std::fill(iv.begin() + 8, iv.end(), 0xFF);
        iv[15] = static_cast<uint8_t>(0xF0 + (idx % 8));
        iv[7] = 0x00;
      } else if (idx % 3 == 2) {  // carry across the low 32 bits
        iv[12] = iv[13] = iv[14] = 0xFF;
        iv[15] = 0xFE;
      }
      Bytes expect(len);
      ref.ctr(key.data(), iv.data(), data.data(), expect.data(), len);
      bool ok = true;
      for (Backend be : {Backend::SBoxTable, Backend::TTable, Backend::SBoxComputed}) {
        if (be == Backend::SBoxComputed && len > 4096) continue;  // computed S-box is intentionally slow
        Aes128 aes(key.data(), key.size(), be);
        if (ctr_crypt(aes, iv.data(), data) != expect) ok = false;
      }
      ok ? ++pass : ++fail;
      if (!ok) os << "FAIL  vs " << ref.name << "  len=" << len << " iv=" << to_hex(iv) << "\n";
    }
    os << (fail == 0 ? "PASS  " : "FAIL  ") << "pure AES-128-CTR == " << ref.name << "  (" << pass << "/" << (pass + fail)
       << " cases, 3 backends)\n";
    sum.total += pass + fail;
    sum.failed += fail;
  }
  os << "\nSummary: " << sum.total << " cross-validation cases, " << sum.failed << " failed\n";
  return sum;
}

}  // namespace aesp
